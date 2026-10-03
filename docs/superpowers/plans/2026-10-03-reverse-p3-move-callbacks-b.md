# P3: the move callbacks 0x475EC..0x489A0 and the callbacks they store (track P, batch 3) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the 24 functions of track P's batch 3 (record §P3.1: character 2's move callbacks `0x475EC 0x47608 0x47624 0x48964 0x489A0 0x47720 0x47874 0x47FCC 0x48608`, the callbacks they store `0x476FC 0x47648 0x47688 0x47830 0x47798 0x477A8 0x477E8 0x47CB0 0x47D24 0x47E9C 0x48054 0x480B4 0x4844C`, `0x480B4`'s callee `0x48170` and the +0x10 handler `0x4811C` it stores), each verified against the original's bytes with its callees stubbed (E3 §E3.10), every block hit and every store observable; drop `gp-u10-ending`'s `0x475EC` row and show its pins exact; regenerate the E2 table with each port.

**Architecture:** One C function per original function, appended to `port/src/game/fighter.c` after P2's `fighter_22638`, registered in `actors_init` (`port/src/game/actors.c`) so `0x34E2C` (move callbacks), `0x3531C` case 7 (+0x0C) and case 10 (+0x10, through an adapter), `0x19020` (+0x18), `0x193B0` (+0x1C) and the three +0x14 callers reach them. Each new stubbed callee opens with `PR_SEAM`/`PR_SEAM_RET` (`0x35838 0x3B298 0x39FB0 0x3A95C 0x3C190 0x3B714 0x188DC 0x36D98 0x3C148 0x468D8 0x3C16C`, and the member `0x48170`); each function has a binding and mutants in `port/tests/diff_runner.c`, a `Spec` in `tools/diff_verify.py` (`P3_SPECS`), seeded unit checks in `port/tests/test_fight.c`, and its row in the self-check counter. No harness change: P2's `[reg+N]` arguments compare `0x18C14`'s flags, and the two after-table switches are the bounded form `switch_cases` follows.

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and `capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §3 decision 1 ("port all of O3-O6, whether or not a capture reaches them"), §4 track P ("port batches, each function verified by E"), §5.1-§5.3, §6 ("P: each ported function has a verification row; unhit blocks and non-emulable instructions named; counters and README percentage updated"), §7.

**Derivation record:** `docs/superpowers/plans/2026-10-03-reverse-p3-derivations.md` (§P3.1 the 24 members from the raw, §P3.2 callers, masks and the callee declarations, §P3.3-§P3.8 each function from the bytes, §P3.9 the gp interactions, §P3.10 decisions and named gaps, §P3.11 the roadmap after P3, §P3.12 results). Recipe: `2026-10-01-reverse-e3-derivations.md` §E3.10; lessons: `2026-10-02-reverse-p1-derivations.md` §P1.10-§P1.12 and P2's reviews (`.superpowers/sdd/2026-10-02-reverse-p2-move-callbacks/progress.md`).

**What the planner ran (scratch, 2026-10-02/03, on `reverse-p2`'s tip `8d4cdf4` = `main` `b16922d` + P2 with its closure; image sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; scratch paths written as `/tmp/pr_p3_*`, test times as `N.NNN`):** a prototype (each task developed in order and committed in a scratch worktree; the full `make verify` with the parallel-safe overrides ran on `8d4cdf4` before and on the prototype's final state after, quoted in Tasks 1 and 8) and a replay (a fresh `8d4cdf4` worktree, Tasks 2-7 applied in order by one driver with exactly the scripts below; every expected output below is the replay's). Every code block below is a file the replay ran, byte for byte (Task 2 Step 7's script had one comment reworded afterwards and Task 8's docs script runs after the replay's end: both were re-run on copies of the `8d4cdf4` files with the result shown). The replay's final tree is byte-identical to the prototype's, on which Task 8's gate was measured. A store sweep finds every store of the 24 rows observable; every gp scenario's miss set was read before the batch (record §P3.9).

**Re-baseline note.** P2 (`reverse-p2`, tip `8d4cdf4`) merges into `main` before this plan executes. The counters, E2 lines and gp pins below are P2's final values (measured at `8d4cdf4`, Task 1) plus this plan's increments. If the merge, or anything merged after it, moves a value, Task 1 records the measured one in the ledger and every later expected counter adds this plan's increments to it: diff-verify rows +5, +4, +5, +4, +5, +1 (Tasks 2-7); mutants +7, +8, +10, +13, +14, +6; closed rows +2, +1, 0, +1, 0, 0; rows without callees 0, +1, 0, 0, 0, 0; E2 ported targets +5, +1, +1, +1, +1, 0. If the gp-u10-ending set differs from the one Task 2's script asserts, re-measure it (Task 2 Step 7).

## Decisions needed from the user

**None.** The user's decisions D2 (span code: a named gap backed by the pixel oracles; not touched here) and D3 (callee rows in C1, which comes right after P3; the eleven new stubs join it) stand. Three choices this plan makes are derived from the raw and recorded (record §P3.1, §P3.2, §P3.9), not left open:

1. **The member list is the roadmap's 24, no more.** Every code immediate a member stores is a member and nothing else is reached; the five stream targets the members' streams reach (`0x47E04 0x47E30 0x482E4 0x48374`, P6; `0x48254`, P7) are reached by no gp replay, so unlike P2's `0x14FA8 0x14FF8 0x150AC` none is pulled in (P1's I4 and P2's precedent apply only to targets that block a scenario).
2. **The +0x10 handler `0x4811C` registers through an adapter** that supplies the record (`fighter_4811c_case10`), as `fighter_21458_case10` does: the raw reaches it with EDX = the slot's record (`0x35396`), the port's case-10 dispatch passes (slot, side).
3. **gp-u10-ending is the one scenario P3 touches.** Since P2 ported `0x2381C`, its replay reaches `0x475EC` in the final (f=0x1594); Task 2 drops the row and shows every pin exact and unchanged (each + 1 fails). No other set holds a P3 member.

## The P-track roadmap

From record §P1.3 as corrected by P2 §P2.11 and §P3.11. Each line is one plan and one subagent-driven run.

| batch | ports | what |
|---|---|---|
| P1 (merged) | 16 + 1 callee row | the finisher entries, their +0x0C callbacks, `0x38034`, the finisher streams' targets |
| P2 (merges before P3) | 24 | the 13 move callbacks `0x14EF8..0x3DCEC`, the 7 callbacks they store, `0x22404`, `0x14FA8 0x14FF8 0x150AC` |
| **P3** (this plan) | **24** | character 2's 9 move callbacks `0x475EC..0x489A0`, the 13 callbacks they store, `0x48170`, the +0x10 handler `0x4811C` |
| C1 | about 45 rows | verification only: the ported callees P1-P3 stub (P3 adds `0x35838 0x3B298 0x39FB0 0x3A95C 0x3C190 0x3B714 0x3C148 0x468D8 0x36D98 0x188DC 0x3C16C`) |
| P4 | 20 | animation targets A |
| P5 | 13 | animation targets B |
| P6 | 18 | animation targets C (with `0x47E04 0x47E30 0x482E4 0x48374`, P3's streams' targets) |
| C2 | about 20 rows | verification only: the rest of the stubbed callees |
| P7 | 14 | the unported direct callees with their callers, the targets outside E2 (with `0x48254`) |
| P8 | 16 + triage | the rest |
| span | 0 | decision D2 |

None of P3's 24 is a Ghidra `FN_` function: `port_progress.py` stays `771 1203 64` / `731 731 100` and README does not move.

## Global Constraints

Copied verbatim from their sources:

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- Spec §8: "`make verify` is the gate after every task." User ruling (2026-10-01, speed-up): the per-task gate is the task's tests, `make diff-verify`, `make entry-triage` and the gp oracles whose miss sets the task touches; the full `make verify` runs at the baseline (Task 1), the final task (Task 8) and before the merge, with the parallel-safe overrides `T=p3; SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md E2_IMAGE=/tmp/pr_${T}_e2.bin`. The 45 oracle lines (`grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>`) must equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; the `make audio-render` WAV must be byte-identical to `.superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav`; `python3 tools/port_progress.py` prints `771 1203 64` and `731 731 100`. A full `make verify` took 25 min here with two others sharing the host.
- AGENTS.md: "**The E2 target list is a gate** ... A commit that ports a target (a new `/* 0xADDR` header or `fn_register`) regenerates the table in the same commit (decision D3) ... A conflict in that file is resolved by regenerating it after the rebase, never by merging lines by hand."
- AGENTS.md: "One C function per original function, header comment `/* 0xADDR — spec section */`." / "Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment styles in `port/src`." / "Code addresses stored in data go through `fn_origin()` / `fn_resolve()`." / "**`port/src/symbols.h` is generated** ... Never hand-edit it; where the generator emits no name, use a local `#define` with the raw address."
- AGENTS.md: "A ported function that a differentially verified function stubs or records ... opens with `PR_SEAM(0xADDR, args...)` (void) or `PR_SEAM_RET(0xADDR, args...)` ... The arguments are the C signature's, in order (a pointer into `mem[]` as its offset)." Record E3 §E3.8: "A seam must be the callee's first statement".
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." / "**Register a test once** — add `X(test_foo)` to `TEST_CASES` in `port/tests/test.h`" / "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests, and never assert an unseeded BSS-zero". "Consolidating must not change an assertion": this plan **extends** the exact-set assertions of `RealFunctionTests` (rows, masks, mutant names, stub clobbers, the counter line) and changes no other assertion.
- AGENTS.md: "Every env-gated driver and `--check` run arms `fn_resolve`'s miss log and must record exactly the pinned known-set": only `gp-u10-ending`'s set holds a P3 member; Task 2 drops it (record §P3.9).
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style: `<area>: <what changed>`. **Never `git add -A`** — stage named files." Trailer: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Never run `make gp-capture`.
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.

## Review Focus

1. **An input the rows cannot tell apart** (P2's reviews found three). Every row whose original reads one of two lookalikes has a case where they differ and a mutant that only that case catches: EBX against rec+0x51 (`s0`/`s2` of `0x475EC`, `q2` of `0x48964`, `v0`/`v1` of `0x47874`, every `z` case of `0x47830`, `r0`/`r1` of `0x48608`, `e0`/`e1` of `0x47720`); the EAX slot against ctx[2] (`0x47E9C@slot`: its state byte is the EAX slot's); the own slot against the other (`0x47688@pivot`, `0x477E8@mutant`, `0x48170@mutant`/`@reset`, `0x47FCC@mutant`, `0x480B4@char`).
2. **A width or sign read wrong.** Cases each alone catching a mutant: `c2` (`0x476FC@sext`), `h4` (`0x47688@zext`), `b2` (`0x47D24@signed`), `x2` (`0x480B4@signed`), `eD` (`0x47E9C@signed`), `i5` (`0x4811C@signed`), `a3`/`aB`/`aA`/`a2` (`0x4844C@signed`/`@bound`/`@zext`/`@abs`), `k1`/`k2` (`0x47CB0@ge`/`@lo`); AL-only tests of stubbed predicates (`h3`'s `0x3B298` stub EAX `0x100`).
3. **A store no case can observe.** Every field a row writes carries a sentinel that differs from what it writes; the planner's store sweep (record §P3.12) found all 81 store instructions observable; the `@order` mutants (`0x475EC 0x47608 0x47624 0x489A0 0x47874 0x47830 0x477E8 0x47FCC 0x47D24 0x47E9C 0x48608 0x48170 0x4811C 0x4844C`) prove the memory at a call is compared.
4. **The two new slot fields and the registrations.** The +0x14 callback `0x47798` returns its EAX whole (`@eax` caught by `w1` alone) and is registered with the `fighter_slot14_cb` shape; the +0x10 handler registers its case-10 adapter (Task 6's unit check calls the registered function as `fighter_state_3531c` does). Each task's unit check asserts `fn_resolve(addr) == fn`, and its mutation proof deletes a registration. Unit checks that compare with the real `0x18C14` use a fixture state where the mutated flag or branch changes the result (P2 Task 6 I1's lesson; record §P3.10).
5. **gp-u10-ending and the other gp oracles.** Task 2 drops the `0x475EC` row and shows each U10 pin exact (+1 fails); Task 8's full gate shows every other gp ratchet and miss set unchanged; Task 1 re-measures if P2's merge moved anything.

## Where to run

The worktree `.worktrees/reverse-p3` (branch `reverse-p3`). It was cut from `main` `b16922d`, before P2 merged; this plan's two docs are its only commit. **Before Task 1**, once P2 is on `main`, rebase it:

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse/.worktrees/reverse-p3
git rebase main
grep -c '0x475ECu, "hit_reaction_apply"' port/tests/test_platform.c      # 1: P2's closure is in
ls -d data .superpowers port/tests/ghidra_data.bin port/tests/title_screen_ref.ppm   # all four must exist
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -1                           # [100%] Built target ...
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_p3_img.bin && shasum /tmp/pr_p3_img.bin
```

The last line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the image differs from the one the record measured: stop. Every command runs from the worktree root. Ledger: `.superpowers/sdd/2026-10-03-reverse-p3-move-callbacks-b/progress.md`.

**How the code steps are written.** Each change is a `python3 - <<'PY'` script that replaces exact text and asserts the text occurs exactly once (`sub`), so a script either applies cleanly or stops naming the file and the text it could not find. Run each once, from the worktree root, in order. If `main` moved after `8d4cdf4`, an anchor can move: re-apply that `sub` by hand at the same place, never elsewhere; the unit tests' line numbers in the expected output move with `test_fight.c`, and the E2 table is regenerated, never merged.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/game/fighter.c`, `fighter.h` | the 24 functions (appended after `fighter_22638`) and the case-10 adapter, their prototypes; the seams of `0x35838 0x3B298 0x39FB0 0x3A95C 0x3C190 0x3B714 0x188DC 0x36D98 0x3C148 0x468D8 0x3C16C` (and `0x48170`'s own); `fighter_state_35838`, `fighter_command_dispatch`, `fighter_3c190`, `fighter_36d98` made non-static |
| `port/src/game/actors.c` | the registrations in `actors_init` (after P2's `0x22638`) |
| `port/tests/diff_runner.c` | the bindings and mutants (`b_*`/`m_*`, `k_bindings`) |
| `tools/diff_verify.py` | `P3_SPECS` and the new callee declarations |
| `tools/tests/test_diff_verify.py` | `P3_MASKS`, `P3_KINDS`; the exact-set assertions extended; `test_each_p3_mutant_is_caught_by_what_it_breaks`; the stub table; the counter line |
| `port/tests/test_fight.c`, `port/tests/test.h` | `test_p3_simple`, `test_p3_47720`, `test_p3_47874`, `test_p3_47fcc`, `test_p3_48608`, `test_p3_4844c` |
| `port/tests/test_platform.c`, `Makefile`, `AGENTS.md` | gp-u10-ending's set loses `0x475EC`; the `GP_ENDING_*` provenance and the re-measure list say so (Task 2) |
| `docs/superpowers/plans/2026-10-01-reverse-e2-triage.md` | regenerated in Tasks 2-6 (Task 7 leaves it) |
| `docs/PROGRESS.md`, the record, the U9/U10 record | Task 8 |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes `main` with P2 merged at the worktree's head; produces the baseline log `/tmp/pr_p3_base.log`.

- [ ] **Step 1: the full gate on the untouched tree.**

```bash
T=p3; make verify SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att \
  FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 \
  GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md E2_IMAGE=/tmp/pr_${T}_e2.bin \
  > /tmp/pr_p3_base.log 2>&1; echo "EXIT=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_p3_base.log \
  | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
grep -E '^diff-verify:|^entry-triage: (targets|voice)|ratchet N' /tmp/pr_p3_base.log | sed 's/^gp_compare: //'
python3 tools/port_progress.py
```

Expected (measured at `8d4cdf4`, 24 min 48 s): `EXIT=0`, `ORACLES-EQUAL`, and

```
gp-idle-loss: frames: first unexplained 2064, ratchet N 2064 ok
gp-idle-loss: trace: 0 differing through 8319; ratchet N 8320 ok
gp-u5-charsel: frames: first unexplained 516, ratchet N 516 ok
gp-u5-charsel: trace: 0 differing through 1512; ratchet N 1513 ok
gp-u6-moves-b: frames: first unexplained 1005, ratchet N 1005 ok
gp-u6-moves-b: trace: first differing 2262, ratchet N 2262 ok
gp-u6-moves-b: moves: first differing 2949, ratchet N 2949 ok
gp_keys: gp-keys-fight: effects: first not reproduced 11, ratchet N 11 ok
gp-twop: frames: first unexplained 612, ratchet N 612 ok
gp-twop: trace: 0 differing through 1505; ratchet N 1506 ok
gp-twop: moves: 0 differing through 1505; ratchet N 1506 ok
gp-u8-right-arcade: frames: first unexplained 1072, ratchet N 1072 ok
gp-u8-right-arcade: trace: 0 differing through 2273; ratchet N 2274 ok
gp-u8-left-training: frames: first unexplained 1076, ratchet N 1076 ok
gp-u8-left-training: trace: 0 differing through 2337; ratchet N 2338 ok
gp-u8-right-training: frames: first unexplained 1098, ratchet N 1098 ok
gp-u8-right-training: trace: 0 differing through 2401; ratchet N 2402 ok
gp-u8-tug-of-war: frames: first unexplained 1107, ratchet N 1107 ok
gp-u8-tug-of-war: trace: 0 differing through 2465; ratchet N 2466 ok
gp-u8-handicap: frames: first unexplained 1022, ratchet N 1022 ok
gp-u8-handicap: trace: 0 differing through 2273; ratchet N 2274 ok
gp-u8-endurance: frames: first unexplained 278, ratchet N 278 ok
gp-u8-endurance: trace: 0 differing through 1173; ratchet N 1174 ok
gp-u8-attract-start: frames: first unexplained 1087, ratchet N 1087 ok
gp-u8-attract-start: trace: 0 differing through 2017; ratchet N 2018 ok
gp-u9-win: frames: first unexplained 346, ratchet N 346 ok
gp-u9-win: trace: first differing 2150, ratchet N 2150 ok
gp-u9-win: path: 0 not reproduced through 7; ratchet N 8 ok
gp-u9-win: win: first differing 3162, ratchet N 3162 ok
gp-u10-ending: frames: first unexplained 331, ratchet N 331 ok
gp-u10-ending: trace: 0 differing through 9953; ratchet N 9954 ok
gp-u10-ending: path: 0 not reproduced through 29; ratchet N 30 ok
gp-u10-ending: win: 0 differing through 9953; ratchet N 9954 ok
diff-verify: 54/54 functions VERIFIED; 89/89 mutants detected; 1 named gaps; 7/41 rows with callees closed (13 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 297 unported, 198 ported; supplement 131 (22 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 31 in unported code, 84 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

If a value differs (the re-baseline note), record the measured lines in the ledger; every later "expected" counter then adds this plan's increments to them.

- [ ] **Step 2: the WAV.** `make audio-render AUDIO_WAV=/tmp/pr_p3.wav >/dev/null 2>&1; cmp /tmp/pr_p3.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-SAME` prints `WAV-SAME`.

---

### Task 2: the move callbacks with no context: `0x475EC 0x47608 0x47624 0x48964 0x489A0` (seam `0x35838`), and gp-u10-ending's `0x475EC` row

**Files:** modify `tools/diff_verify.py` (after `P2_SPECS`'s last block, and the `SPECS` line), `tools/tests/test_diff_verify.py` (after `P2_KINDS`; the exact-set assertions; a new test before `test_each_stub_declares_the_registers_its_callee_clobbers`; the stub table; the counter line), `port/tests/test_fight.c` (append after `test_p2_0c`), `port/tests/test.h`, `port/src/game/fighter.c` (append after `fighter_22638`; the seam in `fighter_state_35838`), `port/src/game/fighter.h`, `port/src/game/actors.c` (`actors_init`, after `fn_register(0x22638u, ...)`), `port/tests/diff_runner.c`, the E2 table; Step 7: `port/tests/test_platform.c` (`k_miss_gp_u10_ending`), `Makefile` (the `GP_ENDING_*` provenance), `AGENTS.md` (the U10 re-measure list).

**Interfaces:** produces `void fighter_475ec(u32 slot, u32 rec, u32 side)` and the same signature for `fighter_47608`, `fighter_47624`, `fighter_48964`, `fighter_489a0` (as `0x34E2C` calls them: no return value read); `fighter_state_35838(u32 slot, u32 rec, u32 dirbits)` exported with `PR_SEAM(0x35838u, ...)`; `DIRS`, `P3_REC_SEED`, `p3_speed`, `p3_dirs`, `P3_SPECS` (diff_verify); `P3_MASKS`, `P3_KINDS`, `test_each_p3_mutant_is_caught_by_what_it_breaks` (tests); `p3_actor`, `p3_set_bit15`, `test_p3_simple` (test_fight.c). Consumes P2's `p2_cb_fn`, `z_fseed`, `u6b_run`, `Z_S0/Z_S1/Z_R0/Z_R1`; P1's `BIT15`; E3's `ANIM_BEGIN`, `E3_SLOT`, `E3_REC`, `E3_REC2`. Record §P3.2, §P3.3, §P3.9.

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


# tools/diff_verify.py: the batch's specs after P2's, and in SPECS
sub("tools/diff_verify.py", '''       mutants=("@mutant", "@signed", "@byte5d", "@char")),
]
''', '''       mutants=("@mutant", "@signed", "@byte5d", "@char")),
]

# ---- track P batch 3: the move callbacks 0x475EC..0x489A0 and the callbacks they store (record
# 2026-10-03-reverse-p3) --------------------------------------------------------------------------------
# Every member is character 2's: a move callback (the move-table dwords 0xA4004..0xA42AC) or a callback one of them
# stores. A move callback runs as 0x34E2C calls it at 0x35045 (EAX = slot, EDX = rec, EBX = side), mask 0 (record
# 2026-10-02-reverse-p2 §P2.2).
# 0x35838 (fighter_state_35838): EAX = slot, EDX = rec, EBX = the direction bits (`mov edx,ebx` at 0x3583D); a
# plain `ret`; it clobbers EBX and EDX (E.callee_clobbers).
DIRS = E.Call(0x35838, ("eax", "edx", "ebx"), clobbers=("ebx", "edx"))


# 0x475EC/0x47608 (record §P3.3) write the EDX record's +0x43 and word +0x34, the word negated when 0x1A570, whose AL
# they test, returns 1. 0x1A570's argument is EBX = side (`mov eax,ebx`), not rec+0x51: every case has rec+0x51 = 1,
# and s0/s2 run side 0. The stub's AL varies per case.
P3_REC_SEED = {E3_REC + 0x34: b"\\x34\\x34", E3_REC + 0x43: b"\\x43", E3_REC + 0x51: b"\\x01"}


def p3_speed(name, entry, mutants=("@mutant",)):
    return Spec(name, entry, [
        Case("s0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, P3_REC_SEED, {0x1A570: 0}),
        Case("s1", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 1}, P3_REC_SEED, {0x1A570: 1}),
        Case("s2", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, P3_REC_SEED, {0x1A570: 1}),
    ], calls=(BIT15,), eax_mask=0, mutants=mutants)


# 0x48964/0x489A0 (record §P3.3): the slot's +0x41 bit 6 refuses (q0: 0x40 alone; q1/q2 0xBF, every other bit);
# else the bit is set and 0x35838(slot, rec, 0x2000 or 0x1000 by 0x1A570(rec+0x51)'s AL). EBX is not read (`mov
# bl,[ecx+0x41]` overwrites it): q2 runs side 0 with rec+0x51 = 1.
def p3_dirs(name, entry, mutants):
    return Spec(name, entry, [
        Case("q0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x41: b"\\x40", E3_REC + 0x51: b"\\x00"}),
        Case("q1", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x41: b"\\xbf", E3_REC + 0x51: b"\\x00"},
             {0x1A570: 0}),
        Case("q2", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x41: b"\\xbf", E3_REC + 0x51: b"\\x01"},
             {0x1A570: 1}),
    ], calls=(BIT15, DIRS), eax_mask=0, mutants=mutants)


P3_SPECS = [
    p3_speed("fighter_475ec", 0x475EC, ("@mutant", "@side")),
    p3_speed("fighter_47608", 0x47608),
    # 0x47624: 0x2BC30(rec, 0xED79A, 4.0), then the slot 9/8/0 (each seeded otherwise).
    Spec("fighter_47624", 0x47624, [
        Case("n0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x52: b"\\x52\\x53\\x54"}),
        Case("n1", {"eax": E3_SLOT, "edx": E3_REC2, "ebx": 1}, {E3_SLOT + 0x52: b"\\x52\\x53\\x54"}),
    ], calls=(ANIM_BEGIN,), eax_mask=0),
    p3_dirs("fighter_48964", 0x48964, ("@mutant", "@side")),
    p3_dirs("fighter_489a0", 0x489A0, ("@mutant",)),
]
''')
sub("tools/diff_verify.py", "] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS\n",
    "] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS\n")

# tools/tests/test_diff_verify.py: the batch's rows, masks and mutant kinds, extending the exact-set assertions
T = "tools/tests/test_diff_verify.py"
sub(T, '''            "fighter_22638@char": {"byte"}}
''', '''            "fighter_22638@char": {"byte"}}

# Track P batch 3 (record 2026-10-03-reverse-p3): its rows with their EAX masks, and what alone catches each
# of its mutants.
P3_MASKS = {"fighter_475ec": 0, "fighter_47608": 0, "fighter_47624": 0, "fighter_48964": 0, "fighter_489a0": 0}
P3_KINDS = {"fighter_475ec@mutant": {"call #0 memory"}, "fighter_475ec@side": {"call #0"},
            "fighter_47608@mutant": {"call #0 memory"}, "fighter_47624@mutant": {"call #0 memory"},
            "fighter_48964@mutant": {"call #1"}, "fighter_48964@side": {"call #0"},
            "fighter_489a0@mutant": {"call #0 memory"}}
''')
sub(T, '''                                             "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)))''',
    '''                                             "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)
                                            + list(P3_MASKS)))''')
sub(T, '''            + list(P1_KINDS) + list(P2_KINDS)))''', '''            + list(P1_KINDS) + list(P2_KINDS) + list(P3_KINDS)))''')
sub(T, '''            **P1_MASKS, **P2_MASKS})''', '''            **P1_MASKS, **P2_MASKS, **P3_MASKS})''')
sub(T, '''    def test_each_stub_declares_the_registers_its_callee_clobbers(self):''',
    '''    def test_each_p3_mutant_is_caught_by_what_it_breaks(self):
        # track P batch 3 (record 2026-10-03-reverse-p3): what alone catches each mutant; every row with a
        # callee has one that only the call list or the memory at a call catches
        for name, want in P3_KINDS.items():
            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
            self.assertEqual(got, want, name)
        # 0x1A570's argument is EBX (side), not rec+0x51: only the cases where the two differ catch it
        for name, ids in (("fighter_475ec@side", ["s0", "s2"]), ("fighter_48964@side", ["q2"])):
            self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)

    def test_each_stub_declares_the_registers_its_callee_clobbers(self):''')
sub(T, '''                                 0x22404: (), 0x36870: ("esi", "edi", "ebp")})''',
    '''                                 0x22404: (), 0x36870: ("esi", "edi", "ebp"), 0x35838: ("ebx", "edx")})''')
sub(T, '''        self.assertIn("diff-verify: 54/54 functions VERIFIED; 89/89 mutants detected; 1 named gaps; "
                      "7/41 rows with callees closed (13 have none).", out.getvalue())''',
    '''        self.assertIn("diff-verify: 59/59 functions VERIFIED; 96/96 mutants detected; 1 named gaps; "
                      "9/46 rows with callees closed (13 have none).", out.getvalue())''')
print("t2_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p3_img.bin --function fighter_475ec | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected (the binding does not exist yet):

```
t2_spec applied
| fighter_475ec | 0x475EC | 3 | 3/3 | MISMATCH | 1A570 stub unverified |
  fighter_475ec: s0: port: unknown binding fighter_475ec
  fighter_475ec: s1: port: unknown binding fighter_475ec
  fighter_475ec: s2: port: unknown binding fighter_475ec
diff-verify: 0/1 functions VERIFIED; 0 named gaps; 0/1 rows with callees closed (0 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
```

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


TEST = r'''
/* ---- track P batch 3 (record 2026-10-03-reverse-p3-derivations.md) ----------
 * The move callbacks 0x475EC..0x489A0 (character 2's) and the callbacks they
 * store. Differential verification (tools/diff_verify.py, P3_SPECS) is the
 * behavioural oracle; these checks pin what it does not see: the
 * registrations and the image dwords that make 0x34E2C, 0x3531C, 0x19020,
 * 0x193B0 and the +0x14 callers reach each function, and one seeded run
 * through each registration with sentinels on every store. */

/* The side's actor word (0x1A570 returns 1 while its bit 15 is clear). */
static u32 p3_actor(u32 side)
{
    u32 rec = DSD(DS_001077B0 + side * 0x94u);
    return DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u;
}

static void p3_set_bit15(u32 side, int set)
{
    u32 a = p3_actor(side);
    DSW(a) = (u16)(set ? (DSW(a) | 0x8000u) : (DSW(a) & 0x7FFFu));
}

/* §P3.3: the five move callbacks with no context, through their
 * registrations as 0x34E2C calls them (slot 0, its record, side 0). */
static void p3_check_simple(void)
{
    static const u32 addr[5] = { 0x475ECu, 0x47608u, 0x47624u, 0x48964u, 0x489A0u };
    static const u32 dw[5] = { 0x000A4004u, 0x000A40CCu, 0x000A42ACu, 0x000A41F8u, 0x000A420Cu };
    void (*const fn[5])(void) = { (void (*)(void))fighter_475ec, (void (*)(void))fighter_47608,
                                  (void (*)(void))fighter_47624, (void (*)(void))fighter_48964,
                                  (void (*)(void))fighter_489a0 };
    p2_cb_fn f;
    u32 k, set, c;
    for (k = 0; k < 5u; k++) {
        CHECK(fn_resolve(addr[k]) == fn[k], "the move callback is registered");
        CHECK_EQ_INT((int)DSD(dw[k]), (int)addr[k]);
    }

    /* 0x475EC / 0x47608: the record's +0x43 = 0x28 / 0x22 and word +0x34 =
     * 0x258 / 0x2EE, negated while side 0's actor word has bit 15 clear. */
    for (k = 0; k < 2u; k++) {
        f = (p2_cb_fn)(void *)fn_resolve(addr[k]);
        if (f == NULL) return;
        for (set = 0; set < 2u; set++) {
            z_fseed();
            DSB(Z_R0 + 0x43u) = 0x43u;
            DSW(Z_R0 + 0x34u) = 0x3434u;
            p3_set_bit15(0u, (int)set);
            f(Z_S0, Z_R0, 0u);
            CHECK_EQ_INT((int)DSB(Z_R0 + 0x43u), k == 0u ? 0x28 : 0x22);
            CHECK_EQ_INT((int)DSW(Z_R0 + 0x34u), k == 0u ? (set ? 0x0258 : 0xFDA8) : (set ? 0x02EE : 0xFD12));
        }
    }

    /* 0x47624: the record on 0xED79A at 4.0, the slot 9/8/0. */
    f = (p2_cb_fn)(void *)fn_resolve(0x47624u);
    if (f == NULL) return;
    z_fseed();
    DSW(0x000ED79Au) = 0x12B1u;
    DSB(Z_S0 + 0x54u) = 0x44u;
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000ED79A);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x40800000);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 8);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 0);

    /* 0x48964 / 0x489A0: refused while the slot's +0x41 has bit 6; else the
     * bit is set and 0x35838 runs with 0x2000 / 0x1000 while the record's
     * side (rec+0x51 = 0) has its actor bit 15 clear, the other way round
     * when it is set: 0x2000 starts 0xC8A40[char] (+0x43 bit 1), 0x1000
     * 0xC8AB8[char] (+0x43 bit 0) in mode 3 with the record's +0x28 bit 14
     * clear; both end with +0x52 = 0xE and +0x41 bit 7. */
    for (k = 3; k < 5u; k++) {
        f = (p2_cb_fn)(void *)fn_resolve(addr[k]);
        if (f == NULL) return;
        z_fseed();
        DSB(Z_S0 + 0x41u) = 0x40u;
        f(Z_S0, Z_R0, 0u);
        CHECK_EQ_INT((int)DSB(Z_S0 + 0x41u), 0x40);
        CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 0x55);
        for (set = 0; set < 2u; set++) {
            int hi;
            z_fseed();
            c = (u32)DSB(Z_S0 + 0x7Au);
            DSW(DSD(0x000C8A40u + c * 4u)) = 0x12B1u;
            DSW(DSD(0x000C8AB8u + c * 4u)) = 0x12B2u;
            DSB(Z_R0 + 0x51u) = 0u;
            DSW(Z_R0 + 0x28u) = 0u;
            DSB(Z_S0 + 0x41u) = 0u;
            DSB(Z_S0 + 0x43u) = 0u;
            p3_set_bit15(0u, (int)set);
            f(Z_S0, Z_R0, 0u);
            hi = (k == 3u) == (set == 0u);
            CHECK_EQ_INT((int)DSB(Z_S0 + 0x41u), 0xC0);
            CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 0x0E);
            CHECK_EQ_INT((int)DSB(Z_S0 + 0x43u), hi ? 2 : 1);
            CHECK_EQ_INT((int)DSD(Z_R0 + 8u), (int)DSD((hi ? 0x000C8A40u : 0x000C8AB8u) + c * 4u));
        }
    }
}

int test_p3_simple(void)        { return u6b_run(p3_check_simple); }
'''

ANCHOR = "int test_p2_0c(void)            { return u6b_run(p2_check_0c); }\n"
sub("port/tests/test_fight.c", ANCHOR, ANCHOR + TEST)
sub("port/tests/test.h", "    X(test_p2_0c) \\\n", "    X(test_p2_0c) \\\n    X(test_p3_simple) \\\n")
print("t2_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected (line numbers move with `test_fight.c`):

```
t2_test applied
port/tests/test_fight.c:45625:51: error: use of undeclared identifier 'fighter_475ec'
port/tests/test_fight.c:45625:82: error: use of undeclared identifier 'fighter_47608'
port/tests/test_fight.c:45626:51: error: use of undeclared identifier 'fighter_47624'
port/tests/test_fight.c:45626:82: error: use of undeclared identifier 'fighter_48964'
port/tests/test_fight.c:45627:51: error: use of undeclared identifier 'fighter_489a0'
5 errors generated.
```

- [ ] **Step 3: the port, the registrations, the seams, the bindings and the mutants.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


FIGHTER_C = r'''
/* ---- track P batch 3: the move callbacks 0x475EC..0x489A0 and the callbacks
 * they store ------------------------------------------------------------------
 * Record 2026-10-03-reverse-p3-derivations.md. Every member is character 2's:
 * a move callback (its move-table dwords 0xA4004..0xA42AC) or a callback one
 * of them stores. 0x34E2C calls a move callback at 0x35045 as (EAX = slot,
 * EDX = rec, EBX = side); no caller reads the AL it returns (record
 * 2026-10-02-reverse-p2 §P2.2), so the port's callbacks return nothing. */
#define P3_ANIM_47624 0x000ED79Au  /* 0x47629 */

/* 0x475EC — record §P3.3. Character 2's reaction-0x0B callback (the dword at
 * 0xA4004): the EDX record's +0x43 = 0x28 and word +0x34 = 0x258, the word
 * negated when 0x1A570(side) (EBX: `mov eax,ebx`) sets AL. PORT: AL = 1
 * (0x47605) unread (§P2.2). */
void fighter_475ec(u32 slot, u32 rec, u32 side)
{
    (void)slot;
    DSB(rec + 0x43u) = 0x28u;                               /* 0x475EE */
    DSW(rec + 0x34u) = 0x0258u;                             /* 0x475F2 */
    if (fighter_actor_bit15_clear(side) != 0)               /* 0x475EC/0x475F8 0x1A570, 0x475FD */
        DSW(rec + 0x34u) = (u16)(0u - DSW(rec + 0x34u));    /* 0x47601 */
}

/* 0x47608 — record §P3.3. Character 2's reaction-0x15 callback (the dword at
 * 0xA40CC): 0x475EC's shape with +0x43 = 0x22 and the word 0x2EE. PORT: AL
 * unread (§P2.2). */
void fighter_47608(u32 slot, u32 rec, u32 side)
{
    (void)slot;
    DSB(rec + 0x43u) = 0x22u;                               /* 0x4760A */
    DSW(rec + 0x34u) = 0x02EEu;                             /* 0x4760E */
    if (fighter_actor_bit15_clear(side) != 0)               /* 0x47608/0x47614 0x1A570, 0x47619 */
        DSW(rec + 0x34u) = (u16)(0u - DSW(rec + 0x34u));    /* 0x4761D */
}

/* 0x47624 — record §P3.3. Character 2's reaction-0x2D callback (the dword at
 * 0xA42AC): the record on 0xED79A at 4.0 (0x2BC30), then the slot 9/8/0.
 * PORT: AL unread (§P2.2). */
void fighter_47624(u32 slot, u32 rec, u32 side)
{
    (void)side;
    actors_anim_begin(rec, P3_ANIM_47624, 0x40800000u);     /* 0x47625..0x47633 0x2BC30 */
    DSB(slot + 0x52u) = 9u;                                 /* 0x47638 */
    DSB(slot + 0x53u) = 8u;                                 /* 0x4763C */
    DSB(slot + 0x54u) = 0u;                                 /* 0x47642 */
}

/* 0x48964 — record §P3.3. Character 2's reaction-0x24 callback (the dword at
 * 0xA41F8). With the slot's +0x41 bit 6 set nothing happens; else the bit is
 * set and 0x35838(slot, rec, 0x2000 when 0x1A570(rec+0x51) sets AL, else
 * 0x1000). EBX is not read (0x4896D overwrites it). PORT: AL = 0 (0x4899B)
 * or 1 (0x48997) unread (§P2.2). */
void fighter_48964(u32 slot, u32 rec, u32 side)
{
    u32 dir;
    (void)side;
    if ((DSB(slot + 0x41u) & 0x40u) != 0u) return;          /* 0x48967/0x4896B */
    DSB(slot + 0x41u) = (u8)(DSB(slot + 0x41u) | 0x40u);    /* 0x4896D..0x48975 */
    dir = fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0
        ? 0x2000u : 0x1000u;                                /* 0x48973..0x4898B 0x1A570 */
    fighter_state_35838(slot, rec, dir);                    /* 0x48990/0x48992 */
}

/* 0x489A0 — record §P3.3. Character 2's reaction-0x25 callback (the dword at
 * 0xA420C): 0x48964 with the two directions swapped (0x1000 when AL is
 * set). PORT: AL unread (§P2.2). */
void fighter_489a0(u32 slot, u32 rec, u32 side)
{
    u32 dir;
    (void)side;
    if ((DSB(slot + 0x41u) & 0x40u) != 0u) return;          /* 0x489A3/0x489A7 */
    DSB(slot + 0x41u) = (u8)(DSB(slot + 0x41u) | 0x40u);    /* 0x489A9..0x489B1 */
    dir = fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0
        ? 0x1000u : 0x2000u;                                /* 0x489AF..0x489C7 0x1A570 */
    fighter_state_35838(slot, rec, dir);                    /* 0x489CC/0x489CE */
}
'''

BINDINGS = r'''/* Track P batch 3 (record 2026-10-03-reverse-p3 §P3.3): character 2's move callbacks with no context, as 0x34E2C
 * calls them at 0x35045 (EAX = slot, EDX = rec, EBX = side). Mask 0 (record 2026-10-02-reverse-p2 §P2.2). */
static void b_475ec(const u32 *r, u32 *eax)            { fighter_475ec(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_47608(const u32 *r, u32 *eax)            { fighter_47608(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_47624(const u32 *r, u32 *eax)            { fighter_47624(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_48964(const u32 *r, u32 *eax)            { fighter_48964(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_489a0(const u32 *r, u32 *eax)            { fighter_489a0(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_475ec_at(const u32 *r, u32 by_rec, u32 b43, u32 w34, int late)
{
    u32 rec = r[R_EDX];
    int al;
    if (!late) {
        DSB(rec + 0x43u) = (u8)b43;
        DSW(rec + 0x34u) = (u16)w34;
    }
    al = fighter_actor_bit15_clear(by_rec ? (u32)DSB(rec + 0x51u) : r[R_EBX]);
    if (late) {
        DSB(rec + 0x43u) = (u8)b43;
        DSW(rec + 0x34u) = (u16)w34;
    }
    if (al != 0) DSW(rec + 0x34u) = (u16)(0u - DSW(rec + 0x34u));
}
static void m_475ec(const u32 *r, u32 *eax)            /* the two stores after the 0x1A570 call */
{
    m_475ec_at(r, 0u, 0x28u, 0x0258u, 1);
    *eax = 0u;
}
static void m_475ec_side(const u32 *r, u32 *eax)       /* 0x1A570 on rec+0x51, not EBX */
{
    m_475ec_at(r, 1u, 0x28u, 0x0258u, 0);
    *eax = 0u;
}
static void m_47608(const u32 *r, u32 *eax)            /* the two stores after the 0x1A570 call */
{
    m_475ec_at(r, 0u, 0x22u, 0x02EEu, 1);
    *eax = 0u;
}
static void m_47624(const u32 *r, u32 *eax)            /* the slot stores before the 0x2BC30 call */
{
    DSB(r[R_EAX] + 0x52u) = 9u;
    DSB(r[R_EAX] + 0x53u) = 8u;
    DSB(r[R_EAX] + 0x54u) = 0u;
    actors_anim_begin(r[R_EDX], 0x000ED79Au, 0x40800000u);
    *eax = 0u;
}
static void m_48964_at(const u32 *r, u32 when_al, u32 other, int by_side, int late41)
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    int al;
    if ((DSB(slot + 0x41u) & 0x40u) != 0u) return;
    if (!late41) DSB(slot + 0x41u) = (u8)(DSB(slot + 0x41u) | 0x40u);
    al = fighter_actor_bit15_clear(by_side ? r[R_EBX] : (u32)DSB(rec + 0x51u));
    if (late41) DSB(slot + 0x41u) = (u8)(DSB(slot + 0x41u) | 0x40u);
    fighter_state_35838(slot, rec, al != 0 ? when_al : other);
}
static void m_48964(const u32 *r, u32 *eax)            /* the directions swapped */
{
    m_48964_at(r, 0x1000u, 0x2000u, 0, 0);
    *eax = 0u;
}
static void m_48964_side(const u32 *r, u32 *eax)       /* 0x1A570 on EBX, not rec+0x51 */
{
    m_48964_at(r, 0x2000u, 0x1000u, 1, 0);
    *eax = 0u;
}
static void m_489a0(const u32 *r, u32 *eax)            /* +0x41 bit 6 set after the 0x1A570 call */
{
    m_48964_at(r, 0x1000u, 0x2000u, 0, 1);
    *eax = 0u;
}

'''

F = "port/src/game/fighter.c"
sub(F, '''    default:                                                /* 3..7 (0x22930), above 7 (`ja` 0x227A5) */
        return;
    }
}
''', '''    default:                                                /* 3..7 (0x22930), above 7 (`ja` 0x227A5) */
        return;
    }
}
''' + FIGHTER_C)
# 0x35838 gets its seam; the harness's mutants call it, so it loses `static` (declared in fighter.h below)
sub(F, '''static void fighter_state_35838(u32 slot, u32 rec, u32 dirbits)
{
''', '''void fighter_state_35838(u32 slot, u32 rec, u32 dirbits)
{
    PR_SEAM(0x35838u, slot, rec, dirbits);
''')
sub("port/src/game/fighter.h", "void fighter_22638(u32 slot, u32 rec, u32 side);\n",
    """void fighter_22638(u32 slot, u32 rec, u32 side);
/* Track P batch 3 (record 2026-10-03-reverse-p3-derivations.md §P3.3): the
 * move callbacks 0x34E2C calls as (slot, rec, side), registered in
 * actors_init; and the callee 0x35838 they stub (no longer file-local, so
 * the harness's mutants can call it). */
void fighter_state_35838(u32 slot, u32 rec, u32 dirbits);
void fighter_475ec(u32 slot, u32 rec, u32 side);
void fighter_47608(u32 slot, u32 rec, u32 side);
void fighter_47624(u32 slot, u32 rec, u32 side);
void fighter_48964(u32 slot, u32 rec, u32 side);
void fighter_489a0(u32 slot, u32 rec, u32 side);
""")
sub("port/src/game/actors.c", "    fn_register(0x22638u, (void (*)(void))fighter_22638);\n",
    """    fn_register(0x22638u, (void (*)(void))fighter_22638);
    /* PORT: record 2026-10-03-reverse-p3 §P3.3. Character 2's move callbacks
     * with no context (the move-table dwords 0xA4004, 0xA40CC, 0xA42AC,
     * 0xA41F8 and 0xA420C; 0x34E2C at 0x35045, (slot, rec, side)). */
    fn_register(0x475ECu, (void (*)(void))fighter_475ec);
    fn_register(0x47608u, (void (*)(void))fighter_47608);
    fn_register(0x47624u, (void (*)(void))fighter_47624);
    fn_register(0x48964u, (void (*)(void))fighter_48964);
    fn_register(0x489A0u, (void (*)(void))fighter_489a0);
""")
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_22638@char",       m_22638_char,   0x00000000u },\n',
    """    { "fighter_22638@char",       m_22638_char,   0x00000000u },
    { "fighter_475ec",            b_475ec,        0x00000000u },
    { "fighter_47608",            b_47608,        0x00000000u },
    { "fighter_47624",            b_47624,        0x00000000u },
    { "fighter_48964",            b_48964,        0x00000000u },
    { "fighter_489a0",            b_489a0,        0x00000000u },
    { "fighter_475ec@mutant",     m_475ec,        0x00000000u },
    { "fighter_475ec@side",       m_475ec_side,   0x00000000u },
    { "fighter_47608@mutant",     m_47608,        0x00000000u },
    { "fighter_47624@mutant",     m_47624,        0x00000000u },
    { "fighter_48964@mutant",     m_48964,        0x00000000u },
    { "fighter_48964@side",       m_48964_side,   0x00000000u },
    { "fighter_489a0@mutant",     m_489a0,        0x00000000u },
""")
print("t2_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_475ec fighter_47608 fighter_47624 fighter_48964 fighter_489a0; do
  python3 tools/diff_verify.py --image /tmp/pr_p3_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected: no compiler message, then

```
t2_port applied
all checks passed
| fighter_475ec | 0x475EC | 3 | 3/3 | VERIFIED | 1A570 stub unverified |
| fighter_475ec@mutant | 0x475EC | 3 | 3/3 | MISMATCH | 1A570 stub unverified |
| fighter_475ec@side | 0x475EC | 3 | 3/3 | MISMATCH | 1A570 stub unverified |
| fighter_47608 | 0x47608 | 3 | 3/3 | VERIFIED | 1A570 stub unverified |
| fighter_47608@mutant | 0x47608 | 3 | 3/3 | MISMATCH | 1A570 stub unverified |
| fighter_47624 | 0x47624 | 2 | 1/1 | VERIFIED | 2BC30 stub unverified |
| fighter_47624@mutant | 0x47624 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified |
| fighter_48964 | 0x48964 | 3 | 6/6 | VERIFIED | 1A570 stub unverified, 35838 stub unverified |
| fighter_48964@mutant | 0x48964 | 3 | 6/6 | MISMATCH | 1A570 stub unverified, 35838 stub unverified |
| fighter_48964@side | 0x48964 | 3 | 6/6 | MISMATCH | 1A570 stub unverified, 35838 stub unverified |
| fighter_489a0 | 0x489A0 | 3 | 6/6 | VERIFIED | 1A570 stub unverified, 35838 stub unverified |
| fighter_489a0@mutant | 0x489A0 | 3 | 6/6 | MISMATCH | 1A570 stub unverified, 35838 stub unverified |
```

A row that is not `VERIFIED` is a finding, not something to adjust: stop, compare the C with the bytes (the record's section), and report.

- [ ] **Step 4: the E2 table** (`make entry-triage` first, then regenerate):

```bash
make entry-triage E2_IMAGE=/tmp/pr_p3_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/pr_p3_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
  --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
grep -E '^\| (callbacks|finishers|animation-targets|stubs) ' docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git diff --stat docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -1
```

Expected:

```
entry-triage: FAIL: docs/superpowers/plans/2026-10-01-reverse-e2-triage.md differs from a fresh run (regenerate it with --out)
make: *** [entry-triage] Error 1
entry-triage: targets 292 unported, 203 ported; supplement 131 (22 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 31 in unported code, 84 in ported code, 19 nowhere
| callbacks | 4 | 67 |
| finishers | 0 | 9 |
| animation-targets | 54 | 58 |
| stubs | 57 |
 1 file changed, 8 insertions(+), 8 deletions(-)
```

- [ ] **Step 5: the task gate.**

```bash
make diff-verify entry-triage DIFF_IMAGE=/tmp/pr_p3_d.bin DIFF_TABLE=/tmp/pr_p3_d.md E2_IMAGE=/tmp/pr_p3_e2.bin 2>&1 \
  | grep -E '^(Ran|OK|FAILED|diff-verify:|entry-triage: targets)'
```

Expected (the times vary):

```
Ran 161 tests in N.NNNs
OK
diff-verify: 59/59 functions VERIFIED; 96/96 mutants detected; 1 named gaps; 9/46 rows with callees closed (13 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
Ran 44 tests in N.NNNs
OK
entry-triage: targets 292 unported, 203 ported; supplement 131 (22 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 6: every new unit assertion can fail.** Five mutations of the code under test, each restored after its run:

```bash
python3 - <<'PY'
import os
import pathlib
import subprocess


def mutate(path, old, new):
    """Apply one mutation, rebuild, run the suite, print the first FAIL lines, restore the file."""
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


mutate("port/src/game/actors.c", "    fn_register(0x475ECu, (void (*)(void))fighter_475ec);\n", "")
mutate("port/src/game/fighter.c",
       "    DSB(rec + 0x43u) = 0x22u;                               /* 0x4760A */",
       "    DSB(rec + 0x43u) = 0x28u;                               /* 0x4760A */")
mutate("port/src/game/fighter.c",
       "    DSB(slot + 0x53u) = 8u;                                 /* 0x4763C */",
       "    DSB(slot + 0x53u) = 7u;                                 /* 0x4763C */")
mutate("port/src/game/fighter.c",
       "        ? 0x2000u : 0x1000u;                                /* 0x48973..0x4898B 0x1A570 */",
       "        ? 0x1000u : 0x2000u;                                /* 0x48973..0x4898B 0x1A570 */")
mutate("port/src/game/fighter.c",
       "    if ((DSB(slot + 0x41u) & 0x40u) != 0u) return;          /* 0x489A3/0x489A7 */",
       "    if ((DSB(slot + 0x41u) & 0x80u) != 0u) return;          /* 0x489A3/0x489A7 */")
PY
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
```

Expected:

```
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:45631: the move callback is registered']
port/src/game/fighter.c: 2 FAIL, first: ['test_fight.c:45646: 40 != 34']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45661: 7 != 8']
port/src/game/fighter.c: 4 FAIL, first: ['test_fight.c:45693: 1 != 2']
port/src/game/fighter.c: 2 FAIL, first: ['test_fight.c:45676: 192 != 64']
all checks passed
```

- [ ] **Step 7: gp-u10-ending drops `0x475EC` and keeps its pins** (record §P3.9). The block's comment is P2's
wording, so the script matches it whole from its first line to the array's end and asserts the rows it holds first; if
P2's merge left another set (the script's assertion names it), stop: re-measure the set with `make gp-replay
scenario=gp-u10-ending GP_OPTIONAL=1 GP_DUMP=/tmp/pr_p3_gp` and record it, never predict it.

```bash
python3 - <<'PY'
import pathlib
import re

# port/tests/test_platform.c: gp-u10-ending no longer misses 0x475EC (record §P3.9). P2's closing task wrote the
# set this block replaces (its comment's wording is P2's, so the whole block is matched from its first line to
# the array's end; the rows it must hold are asserted first).
p = pathlib.Path("port/tests/test_platform.c")
s = p.read_text()
m = re.findall(r"/\* gp-u10-ending \(plan gameplay-u9-u10.*?static const fnm_pair k_miss_gp_u10_ending\[\] = \{\n.*?\n\};\n",
               s, re.S)
assert len(m) == 1, "%d gp-u10-ending blocks" % len(m)
rows = re.findall(r"\{ (0x[0-9A-F]+)u, \"(\w+)\" \}", m[0])
assert rows == [("0x29D60", "frontend_mode_1b_step"), ("0x5D812", "frontend_mode_1b_step"),
                ("0x37DD4", "anim_indirect"), ("0x29C78", "anim_indirect"), ("0x3DA50", "anim_indirect"),
                ("0x475EC", "hit_reaction_apply")], rows
p.write_text(s.replace(m[0], '''/* gp-u10-ending (plan gameplay-u9-u10, record 2026-10-02-gameplay-u9-u10-derivations.md
 * §W.14), measured on its full replay to its X record (f = 0x26E1): the two wipe hooks of
 * §G.24 (0x29D60, a bare `ret`, from f = 0x286; 0x5D812, the runtime stub, from f = 0x3FE),
 * the death-animation stream's targets 0x37DD4 (E2 anim-target row, dword 0xD2BCE) at
 * f = 0x14FB in mode 0xC and 0x29C78 (outside E2, record reverse-p1 §P1.2, dword 0xD2BDA)
 * at f = 0x14FE in mode 0xD, and 0x3DA50 (E2 anim-target row, dword 0xD4BEC) at f = 0x155D
 * in mode 0xF. CHAOS's reaction-0x25 callback 0x2381C (track P batch 2) and character 2's
 * reaction-0x0B callback 0x475EC (the dword 0xA4004, from f = 0x1594 in mode 0xF; track P
 * batch 3, record 2026-10-03-reverse-p3 §P3.9) are ported, so neither is a miss. */
static const fnm_pair k_miss_gp_u10_ending[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
    { 0x37DD4u, "anim_indirect" },
    { 0x29C78u, "anim_indirect" },
    { 0x3DA50u, "anim_indirect" },
};
'''))
print("t2_u10 applied")

# Makefile: the GP_ENDING_* provenance names P3's re-measure (the pins do not move: the oracle run below)
p = pathlib.Path("Makefile")
s = p.read_text()
old = '''# Re-measure: a P batch that ports 0x37DD4 (P6), 0x29C78 (P7), 0x3DA50 (P5) or 0x475EC (P3) drops
# its row and re-measures the U10 set; TRACE/WIN are at the replay's end, so they cannot rise.
'''
new = '''# Re-measured by track P batch 3 (record 2026-10-03-reverse-p3 §P3.9) once 0x475EC is ported: the
# set loses that row alone and every pin above is unchanged (each + 1 fails).
# Re-measure: a P batch that ports 0x37DD4 (P6), 0x29C78 (P7) or 0x3DA50 (P5) drops its row and
# re-measures the U10 set; TRACE/WIN are at the replay's end, so they cannot rise.
'''
assert s.count(old) == 1, "the GP_ENDING re-measure sentence"
p.write_text(s.replace(old, new))
print("t2_u10 Makefile applied")

# AGENTS.md: the U10 re-measure list loses 0x475EC
p = pathlib.Path("AGENTS.md")
s = p.read_text()
old = "  `0x475EC` P3, `0x3DA50` P5, `0x37DD4` P6, `0x29C78` P7; record §W.16).\n"
new = ("  `0x3DA50` P5, `0x37DD4` P6, `0x29C78` P7; record §W.16; track P batch 3 ported `0x475EC`, record\n"
       "  2026-10-03-reverse-p3 §P3.9).\n")
assert s.count(old) == 1, "the AGENTS U10 re-measure list"
p.write_text(s.replace(old, new))
print("t2_u10 AGENTS applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'
make gp-ending-oracle GP_DUMP=/tmp/pr_p3_gp GP_WIN_KEEP=1 2>&1 | grep -E '^(Ran|OK|FAILED)|ratchet N|FAIL|fn-miss PR_GP_DUMP distinct'
SC=gp-u10-ending; CAP=data/k11-captures/$SC; SHA=a88de48ad39e90df1e3d3329fbd5aa876c69a08484fa22c66db8deebd3feff38
python3 tools/gp_compare.py --scenario $SC --capture $CAP --port /tmp/pr_p3_gp/$SC --min-first 332 --trace-min-first 9954 \
  --max-start 83 --capture-sha256 $SHA --capture-frames 5704 2>&1 | grep -E 'FAIL'
python3 tools/gp_compare.py --scenario $SC --capture $CAP --port /tmp/pr_p3_gp/$SC --min-first 331 --trace-min-first 9955 \
  --max-start 83 --capture-sha256 $SHA --capture-frames 5704 2>&1 | grep -E 'FAIL'
python3 tools/gp_win.py path --scenario $SC --capture $CAP --port /tmp/pr_p3_gp/$SC --min-milestones 31 --win-min-first 9954 \
  --capture-sha256 $SHA 2>&1 | grep -E 'FAIL'
python3 tools/gp_win.py path --scenario $SC --capture $CAP --port /tmp/pr_p3_gp/$SC --min-milestones 30 --win-min-first 9955 \
  --capture-sha256 $SHA 2>&1 | grep -E 'FAIL'
rm -rf /tmp/pr_p3_gp/$SC
```

Expected: the oracle passes on P2's pins with the set less `0x475EC` (`distinct=7`: the base pair and five rows), and
each pin + 1 fails, so every pin is exact and none rises:

```
t2_u10 applied
t2_u10 Makefile applied
t2_u10 AGENTS applied
Ran 18 tests in N.NNNs
OK
fn-miss PR_GP_DUMP distinct=7 dropped=0
gp_compare: gp-u10-ending: frames: first unexplained 331, ratchet N 331 ok
gp_compare: gp-u10-ending: trace: 0 differing through 9953; ratchet N 9954 ok
gp_compare: gp-u10-ending: path: 0 not reproduced through 29; ratchet N 30 ok
gp_compare: gp-u10-ending: win: 0 differing through 9953; ratchet N 9954 ok
gp_compare: gp-u10-ending: frames: FAIL: first unexplained 331 < ratchet N 332
gp_compare: gp-u10-ending: trace: FAIL: N 9955 > end 9954: N is unreachable
gp_compare: gp-u10-ending: path: FAIL: N 31 > end 30: N is unreachable
gp_compare: gp-u10-ending: win: FAIL: N 9955 > end 9954: N is unreachable
```

A `distinct` above 7 means the replay now reaches something new (a P3 member's stream target, or a changed opponent
order): stop, read the new pair's first frame (record §P3.9's lldb method) and report; do not pin it unclassified.

- [ ] **Step 8: commit.**

```bash
git add tools/diff_verify.py tools/tests/test_diff_verify.py port/tests/test_fight.c port/tests/test.h \
  port/src/game/fighter.c port/src/game/fighter.h port/src/game/actors.c port/tests/diff_runner.c \
  docs/superpowers/plans/2026-10-01-reverse-e2-triage.md \
  port/tests/test_platform.c Makefile AGENTS.md
git commit -m "fighter: port the move callbacks 0x475EC 0x47608 0x47624 0x48964 0x489A0, seam 0x35838; gp-u10-ending drops its 0x475EC row, pins unchanged; E2 table regenerated (track P batch 3)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: `0x47720` and the callbacks it stores, `0x476FC 0x47648 0x47688` (seams `0x3B298 0x39FB0 0x3A95C`)

**Files:** as Task 2 Steps 1-6 (the anchors move to Task 2's last lines).

**Interfaces:** produces `void fighter_47720(u32 slot, u32 rec, u32 side)`, `void fighter_476fc(u32 slot, u32 rec, u32 side)`, `u32 fighter_47648(u32 side)` (EAX tested whole: mask `0xFFFFFFFF`), `void fighter_47688(u32 side)`; `int fighter_command_dispatch(u32 side, u32 edx_arg)` exported with `PR_SEAM_RET(0x3B298u, ...)`; the seams `PR_SEAM(0x39FB0u, slot)`, `PR_SEAM(0x3A95Cu, side, b)`; `DISPATCH`, `PIVOT`, `STANCE`, `p3_47720`, `p3_476fc`, `p3_hook0`, `p3_47688`; `test_p3_47720`. Consumes Task 2's `P3_SPECS`, `P3_MASKS`, `P3_KINDS`; P2's `CHECKS`, `POSE`, `TIMER`, `p2_hook_fn`, `p2_side_fn`, `fighter_18c14`, `fighter_39834`; E3's `HIT_B` and the allowed `0x339AC` (`hit_anim_ctx`). Record §P3.4.

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


sub("tools/diff_verify.py", '''    p3_dirs("fighter_489a0", 0x489A0, ("@mutant",)),
]
''', '''    p3_dirs("fighter_489a0", 0x489A0, ("@mutant",)),
]

# The callees 0x47688 stubs (record §P3.4), args from their bytes, clobbers from E.callee_clobbers: 0x3B298 EAX =
# side, EDX = a byte (`mov ecx,edx; ...; mov edx,eax`; it returns AL); 0x39FB0 EAX = slot (pushes EBX/ECX/EDX);
# 0x3A95C EAX = side, EDX = a byte. All plain `ret`.
DISPATCH = E.Call(0x3B298, ("eax", "edx"), clobbers=("edx", "edi", "ebp"))
PIVOT = E.Call(0x39FB0, ("eax",))
STANCE = E.Call(0x3A95C, ("eax", "edx"), clobbers=("edx",))


# 0x47720 (record §P3.4) builds its context from the EDX record (0x339AC: ctx[0] = rec+0x51, allowed) and arms that
# side's slot: the EAX slot and EBX are not read (EBX = the other side in every case, EDX = E3_OUT, whose +0x51 names
# the side), and the started record is ctx[4], the slot's own (SLOT_PTRS), not EDX's.
def p3_47720(cid, side):
    own = DS_SLOTS + side * 0x94
    return Case(cid, {"eax": E3_SLOT, "edx": E3_OUT, "ebx": 1 - side},
                {**SLOT_PTRS, E3_OUT + 0x51: bytes([side]), own + 0x0C: le32(0x0C0C0C0C), own + 0x18: le32(0x18181818),
                 own + 0x1C: le32(0x1C1C1C1C), own + 0x52: b"\\x52\\x53"})


# 0x476FC (the +0x0C callback 0x47720 stores; 0x3531C case 7, (slot, rec, side), EAX unread): the EDX record's
# +0x63, zero-extended (`and edx,0xff`), against 5 (`jl`): c0 4 (nothing), c1 5, c2 0x80 (a signed byte would
# refuse). The slot's own +0x63 is seeded 0 so a read of it differs.
def p3_476fc(cid, b63):
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0},
                {E3_SLOT + 0x0C: le32(0x0C0C0C0C), E3_SLOT + 0x18: le32(0x18181818), E3_SLOT + 0x1C: le32(0x1C1C1C1C),
                 E3_SLOT + 0x63: b"\\x00", E3_REC + 0x63: bytes([b63])})


# The +0x18 hooks 0x47648 and 0x477A8 (identical bodies; 0x19020, fn(side), EAX tested whole): flags 1 and 8 = 0,
# 0 = 1, and EBX = ECX = 0 for 0x18C14's two box tables (`xor ecx,ecx` before 0x33950 and `xor ebx,ebx` before
# 0x18BD4, which both keep them). The stub's EAX is returned.
def p3_hook0(cid, side, stub):
    return Case(cid, {"eax": side}, SLOT_PTRS, {0x18C14: stub})


# 0x47688 (the +0x1C callback 0x47720 stores; 0x193B0, fn(side)): the slots' bytes +0x52..+0x5F carry different
# sentinels (slot 0 0x52.., slot 1 0xD2..: the own +0x5F is the byte 0x3B298 and 0x39834 take), the other slot's
# +0x54 selects 0x39FB0 (2) or 0x3A95C; the signed word 0xBEDD8 (10 in the image, `sar 0x10` of the dword
# 0xBEDD6) is poked negative in h4, where a zero-extended read differs.
def p3_47688(cid, side, al, o54=None, timer=None):
    pokes = {**SLOT_PTRS, DS_SLOTS + 0x52: bytes(range(0x52, 0x60)), DS_SLOTS + 0x94 + 0x52: bytes(range(0xD2, 0xE0))}
    if o54 is not None:
        pokes[DS_SLOTS + (1 - side) * 0x94 + 0x54] = bytes([o54])
    if timer is not None:
        pokes[0xBEDD8] = le32(timer)[:2]
    return Case(cid, {"eax": side}, pokes, {0x3B298: al})


P3_SPECS += [
    Spec("fighter_47720", 0x47720, [p3_47720("e0", 0), p3_47720("e1", 1)],
         allow_calls=(0x339AC,), calls=(HIT_B,), eax_mask=0, mutants=("@mutant", "@ebx")),
    Spec("fighter_476fc", 0x476FC, [p3_476fc("c0", 4), p3_476fc("c1", 5), p3_476fc("c2", 0x80)],
         eax_mask=0, mutants=("@mutant", "@sext")),
    Spec("fighter_47648", 0x47648, [p3_hook0("k0", 0, 0), p3_hook0("k1", 1, 0x12345678)],
         allow_calls=(0x33950, 0x18BD4), calls=(CHECKS,), mutants=("@mutant",)),
    Spec("fighter_47688", 0x47688, [
        p3_47688("h0", 0, 1), p3_47688("h1", 0, 0, o54=2), p3_47688("h2", 1, 0), p3_47688("h3", 1, 0x100, o54=2),
        p3_47688("h4", 0, 0, timer=0xFFF0),
    ], allow_calls=(0x33950,), calls=(DISPATCH, POSE, PIVOT, STANCE, TIMER), eax_mask=0,
       mutants=("@mutant", "@pivot", "@zext")),
]
''')
T = "tools/tests/test_diff_verify.py"
sub(T, '''P3_MASKS = {"fighter_475ec": 0, "fighter_47608": 0, "fighter_47624": 0, "fighter_48964": 0, "fighter_489a0": 0}''',
    '''P3_MASKS = {"fighter_475ec": 0, "fighter_47608": 0, "fighter_47624": 0, "fighter_48964": 0, "fighter_489a0": 0,
            "fighter_47720": 0, "fighter_476fc": 0, "fighter_47648": 0xFFFFFFFF, "fighter_47688": 0}''')
sub(T, '''            "fighter_489a0@mutant": {"call #0 memory"}}''',
    '''            "fighter_489a0@mutant": {"call #0 memory"},
            "fighter_47720@mutant": {"call #0"}, "fighter_47720@ebx": {"byte", "call #0"},
            "fighter_476fc@mutant": {"byte"}, "fighter_476fc@sext": {"byte"},
            "fighter_47648@mutant": {"call #0"},
            "fighter_47688@mutant": {"call #2"}, "fighter_47688@pivot": {"call #2"},
            "fighter_47688@zext": {"call #3"}}''')
sub(T, '''        for name, ids in (("fighter_475ec@side", ["s0", "s2"]), ("fighter_48964@side", ["q2"])):''',
    '''        for name, ids in (("fighter_475ec@side", ["s0", "s2"]), ("fighter_48964@side", ["q2"]),
                          # 0x476FC's byte is zero-extended (c2's 0x80 alone), 0x47688's timer word signed (h4
                          # alone), its 0x39FB0 slot the other one (h1/h3, the cases that reach it)
                          ("fighter_476fc@sext", ["c2"]), ("fighter_47688@zext", ["h4"]),
                          ("fighter_47688@pivot", ["h1", "h3"])):''')
sub(T, '''0x36870: ("esi", "edi", "ebp"), 0x35838: ("ebx", "edx")})''',
    '''0x36870: ("esi", "edi", "ebp"), 0x35838: ("ebx", "edx"),
                                 0x3B298: ("edx", "edi", "ebp"), 0x39FB0: (), 0x3A95C: ("edx",)})''')
sub(T, '''        self.assertIn("diff-verify: 59/59 functions VERIFIED; 96/96 mutants detected; 1 named gaps; "
                      "9/46 rows with callees closed (13 have none).", out.getvalue())''',
    '''        self.assertIn("diff-verify: 63/63 functions VERIFIED; 104/104 mutants detected; 1 named gaps; "
                      "10/49 rows with callees closed (14 have none).", out.getvalue())''')
print("t3_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p3_img.bin --function fighter_47720 | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected (the binding does not exist yet):

```
t3_spec applied
| fighter_47720 | 0x47720 | 2 | 1/1 | MISMATCH | 339AC allow unverified, 3C4CC stub unverified |
  fighter_47720: e0: port: unknown binding fighter_47720
  fighter_47720: e1: port: unknown binding fighter_47720
diff-verify: 0/1 functions VERIFIED; 0 named gaps; 0/1 rows with callees closed (0 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
```

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


TEST = r'''
/* §P3.4: 0x47720 and the three callbacks it stores, through their
 * registrations. The image dwords are evidence lines (the bytes that make
 * 0x34E2C, 0x3531C, 0x19020 and 0x193B0 reach each address), not port
 * behaviour. */
static void p3_check_47720(void)
{
    p2_cb_fn f;
    p2_hook_fn h;
    p2_side_fn g;
    u8 fl[16];
    u32 k, r;
    CHECK(fn_resolve(0x47720u) == (void (*)(void))fighter_47720, "0x47720 is registered");
    CHECK(fn_resolve(0x476FCu) == (void (*)(void))fighter_476fc, "0x476FC is registered");
    CHECK(fn_resolve(0x47648u) == (void (*)(void))fighter_47648, "0x47648 is registered");
    CHECK(fn_resolve(0x47688u) == (void (*)(void))fighter_47688, "0x47688 is registered");
    CHECK_EQ_INT((int)DSD(0x000A41A8u), 0x00047720);
    CHECK_EQ_INT((int)DSD(0x00047754u), 0x000476FC);
    CHECK_EQ_INT((int)DSD(0x0004775Fu), 0x00047648);
    CHECK_EQ_INT((int)DSD(0x0004776Au), 0x00047688);
    CHECK_EQ_INT((int)DSD(0x0004770Du), 0x00047648);
    CHECK_EQ_INT((int)DSD(0x00047714u), 0x00047688);

    /* 0x47720 with side 0's record (rec+0x51 = 0) while EBX names side 1 and
     * EAX no slot: slot 0 is armed (9/7 and the three callbacks) and its
     * record started on 0xECE1C at 2.0; slot 1 keeps its sentinels. */
    f = (p2_cb_fn)(void *)fn_resolve(0x47720u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSW(0x000ECE1Cu) = 0x12B1u;
    for (k = 0; k < 2u; k++) {
        u32 s = k == 0u ? Z_S0 : Z_S1;
        DSD(s + 0x0Cu) = 0x0C0C0C0Cu;
        DSD(s + 0x18u) = 0x18181818u;
        DSD(s + 0x1Cu) = 0x1C1C1C1Cu;
    }
    f(0x0A0A0A0Au, Z_R0, 1u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000ECE1C);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x0Cu), 0x000476FC);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x18u), 0x00047648);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x1Cu), 0x00047688);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x0Cu), 0x0C0C0C0C);

    /* 0x476FC: the record's +0x63 = 4 keeps the slot; 0x80 (128, at least 5)
     * re-arms +0x18/+0x1C and clears +0x0C. */
    f = (p2_cb_fn)(void *)fn_resolve(0x476FCu);
    if (f == NULL) return;
    for (k = 0; k < 2u; k++) {
        z_fseed();
        DSB(Z_R0 + 0x63u) = k == 0u ? 4u : 0x80u;
        DSD(Z_S0 + 0x0Cu) = 0x0C0C0C0Cu;
        DSD(Z_S0 + 0x18u) = 0x18181818u;
        DSD(Z_S0 + 0x1Cu) = 0x1C1C1C1Cu;
        f(Z_S0, Z_R0, 0u);
        CHECK_EQ_INT((int)DSD(Z_S0 + 0x0Cu), k == 0u ? 0x0C0C0C0C : 0);
        CHECK_EQ_INT((int)DSD(Z_S0 + 0x18u), k == 0u ? 0x18181818 : 0x00047648);
        CHECK_EQ_INT((int)DSD(Z_S0 + 0x1Cu), k == 0u ? 0x1C1C1C1C : 0x00047688);
    }

    /* 0x47648: 0x18C14(side, flags 0 = 1, 1/8 = 0, the rest 2, 0, 0) on the
     * same seeded state. */
    h = (p2_hook_fn)(void *)fn_resolve(0x47648u);
    if (h == NULL) return;
    z_fseed();
    r = h(1u);
    z_fseed();
    for (k = 0; k < 16u; k++) fl[k] = 2u;
    fl[0] = 1u;
    fl[1] = fl[8] = 0u;
    CHECK_EQ_INT((int)r, fighter_18c14(1u, fl, 0u, 0u));

    /* 0x47688 on side 0 (the fixture's 0x3B298(1, slot 0's +0x5F) returns
     * 0): slot 1's +0x54 = 0x66 takes 0x3A95C(1, 0xF), which leaves slot 1 in
     * 0x10/0xA/0 with +0x10 = 0; with +0x54 = 2, 0x39FB0(slot 1) arms the
     * pose instead (+0x10 = 0x39CC8, +0x54 kept); both end with 0x39A10(the
     * other record, the word 0xBEDD8 = 10): slot 1's +0x74 = 10. */
    g = (p2_side_fn)(void *)fn_resolve(0x47688u);
    if (g == NULL) return;
    for (k = 0; k < 2u; k++) {
        z_fseed();
        if (k == 1u) DSB(Z_S1 + 0x54u) = 2u;
        DSW(Z_S1 + 0x74u) = 0x7777u;
        DSD(Z_S1 + 0x10u) = 0x10101010u;
        g(0u);
        CHECK_EQ_INT((int)DSB(Z_S1 + 0x53u), 0x0A);
        CHECK_EQ_INT((int)DSB(Z_S1 + 0x54u), k == 0u ? 0 : 2);
        CHECK_EQ_INT((int)DSD(Z_S1 + 0x10u), k == 0u ? 0 : 0x00039CC8);
        CHECK_EQ_INT((int)DSW(Z_S1 + 0x74u), 10);
    }
}

int test_p3_47720(void)         { return u6b_run(p3_check_47720); }
'''

ANCHOR = "int test_p3_simple(void)        { return u6b_run(p3_check_simple); }\n"
sub("port/tests/test_fight.c", ANCHOR, ANCHOR + TEST)
sub("port/tests/test.h", "    X(test_p3_simple) \\\n", "    X(test_p3_simple) \\\n    X(test_p3_47720) \\\n")
print("t3_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected (line numbers move with `test_fight.c`):

```
t3_test applied
port/tests/test_fight.c:45712:51: error: use of undeclared identifier 'fighter_47720'
port/tests/test_fight.c:45713:51: error: use of undeclared identifier 'fighter_476fc'
port/tests/test_fight.c:45714:51: error: use of undeclared identifier 'fighter_47648'
port/tests/test_fight.c:45715:51: error: use of undeclared identifier 'fighter_47688'
4 errors generated.
```

- [ ] **Step 3: the port, the registrations, the seams, the bindings and the mutants.**

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
# 0x3B298 gets its seam; the harness's mutants call it, so it loses `static` (declared in fighter.h below)
sub(F, '''static int fighter_command_dispatch(u32 side, u32 edx_arg)
{
''', '''int fighter_command_dispatch(u32 side, u32 edx_arg)
{
    PR_SEAM_RET(0x3B298u, side, edx_arg);
''')
sub(F, '''void fighter_39fb0(u32 slot)
{
''', '''void fighter_39fb0(u32 slot)
{
    PR_SEAM(0x39FB0u, slot);
''')
sub(F, '''void fighter_3a95c(u32 side, u32 b)
{
''', '''void fighter_3a95c(u32 side, u32 b)
{
    PR_SEAM(0x3A95Cu, side, b);
''')

FIGHTER_C = r'''
#define P3_ANIM_47720 0x000ECE1Cu  /* 0x4772A */
#define P3_BEDD8      0x000BEDD8u  /* 0x476E4: the signed word 0x47688 gives 0x39A10 (the dword 0xBEDD6's high half) */

/* 0x47720 — record §P3.4. Character 2's reaction-0x20 callback (the dword at
 * 0xA41A8). The context is 0x339AC(the EDX record) (ctx[0] = its +0x51; the
 * EAX slot and EBX are not read). The slot's own record (ctx[4]) on 0xECE1C
 * at 2.0 (0x3C4CC); that side's slot (ctx[2]) 9/7 with the +0x0C callback
 * 0x476FC (0x3531C case 7), the +0x18 hook 0x47648 (0x19020) and the +0x1C
 * callback 0x47688 (0x193B0's 0x19505). PORT: AL = 1 (0x4776E) unread
 * (§P2.2). */
void fighter_47720(u32 slot, u32 rec, u32 side)
{
    u32 ctx[6];
    (void)slot;
    (void)side;
    hit_anim_ctx(ctx, rec);                                 /* 0x47723/0x47725 0x339AC */
    hit_anim_start_b(ctx[4], P3_ANIM_47720, 0x40000000u);   /* 0x4772A..0x47738 0x3C4CC */
    DSB(ctx[2] + 0x52u) = 9u;                               /* 0x47741 */
    DSB(ctx[2] + 0x53u) = 7u;                               /* 0x47749 */
    DSD(ctx[2] + 0x0Cu) = 0x000476FCu;                      /* 0x47751 */
    DSD(ctx[2] + 0x18u) = 0x00047648u;                      /* 0x4775C */
    DSD(ctx[2] + 0x1Cu) = 0x00047688u;                      /* 0x47767 */
}

/* 0x476FC — record §P3.4. The slot +0x0C callback 0x47720 stores (the dword
 * at 0x47754; 0x3531C case 7, (slot, rec, side), EAX unread). With the EDX
 * record's +0x63 (zero-extended, `and edx,0xff`) at least 5 (`jl`), the EAX
 * slot's +0x18 = 0x47648, +0x1C = 0x47688 and +0x0C = 0. */
void fighter_476fc(u32 slot, u32 rec, u32 side)
{
    (void)side;
    if ((s32)(u32)DSB(rec + 0x63u) < 5) return;             /* 0x476FC..0x47708 */
    DSD(slot + 0x18u) = 0x00047648u;                        /* 0x4770A */
    DSD(slot + 0x1Cu) = 0x00047688u;                        /* 0x47711 */
    DSD(slot + 0x0Cu) = 0u;                                 /* 0x47718 */
}

/* 0x47648 — record §P3.4. The slot +0x18 hook 0x47720 and 0x476FC store (the
 * dwords at 0x4775F and 0x4770D; 0x19020's call at 0x1903F, fn(side), the
 * whole EAX tested). The context is 0x33950(side); the flags from 0x18BD4
 * with 1 and 8 = 0 and 0 = 1; it returns 0x18C14(side, the flags, 0, 0)'s
 * result: EBX and ECX are zeroed before 0x18BD4 and 0x33950, which keep
 * them. */
u32 fighter_47648(u32 side)
{
    u32 ctx[6];
    u8 flags[16];
    fighter_ctx_same(ctx, side);                            /* 0x4764E..0x47654 0x33950 */
    fighter_18bd4(flags);                                   /* 0x47659..0x4765F 0x18BD4 */
    flags[1] = 0;                                           /* 0x47668 */
    flags[8] = 0;                                           /* 0x4766C */
    flags[0] = 1u;                                          /* 0x47666/0x47674 */
    return (u32)fighter_18c14(ctx[0], flags, 0u, 0u);       /* 0x4765D/0x47652, 0x47678/0x4767B 0x18C14 */
}

/* 0x47688 — record §P3.4. The slot +0x1C callback 0x47720 and 0x476FC store
 * (the dwords at 0x4776A and 0x47714; 0x193B0's 0x19505, fn(side), EAX
 * unread). The context is 0x33950(side). 0x3B298(the other side, the own
 * slot's +0x5F); when its AL is set nothing else. Else 0x39834(the other
 * side, the own +0x5F); 0x39FB0(the other slot) when that slot's +0x54 is
 * 2, else 0x3A95C(the other side, 0xF); then 0x39A10(the other record, the
 * signed word 0xBEDD8). */
void fighter_47688(u32 side)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);                            /* 0x4768C..0x47690 0x33950 */
    if ((u8)fighter_command_dispatch(ctx[1], (u32)DSB(ctx[2] + 0x5Fu)) != 0u)   /* 0x47695..0x476A6 0x3B298 */
        return;                                             /* 0x476AB/0x476AD */
    fighter_39834(ctx[1], (s32)(u32)DSB(ctx[2] + 0x5Fu));   /* 0x476AF..0x476C0 */
    if (DSB(ctx[3] + 0x54u) == 2u)                          /* 0x476C5..0x476CD */
        fighter_39fb0(ctx[3]);                              /* 0x476CF */
    else
        fighter_3a95c(ctx[1], 0x0Fu);                       /* 0x476D6..0x476DF */
    fighter_39a10(ctx[5], (u32)(s32)(s16)DSW(P3_BEDD8));    /* 0x476E4..0x476F1 */
}
'''

BINDINGS = r'''/* §P3.4: 0x47720 (0x34E2C at 0x35045) and the +0x0C callback 0x476FC (0x3531C case 7), mask 0; the +0x18 hook
 * 0x47648 (0x19020 at 0x1903F, the whole EAX tested); the +0x1C callback 0x47688 (0x193B0 at 0x19505, mask 0). */
static void b_47720(const u32 *r, u32 *eax)            { fighter_47720(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_476fc(const u32 *r, u32 *eax)            { fighter_476fc(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_47648(const u32 *r, u32 *eax)            { *eax = fighter_47648(r[R_EAX]); }
static void b_47688(const u32 *r, u32 *eax)            { fighter_47688(r[R_EAX]); *eax = 0u; }
static void m_47720_at(const u32 *r, int by_edx, int by_ebx)
{
    u32 ctx[6];
    if (by_ebx) fighter_ctx_same(ctx, r[R_EBX]);
    else hit_anim_ctx(ctx, r[R_EDX]);
    hit_anim_start_b(by_edx ? r[R_EDX] : ctx[4], 0x000ECE1Cu, 0x40000000u);
    DSB(ctx[2] + 0x52u) = 9u;
    DSB(ctx[2] + 0x53u) = 7u;
    DSD(ctx[2] + 0x0Cu) = 0x000476FCu;
    DSD(ctx[2] + 0x18u) = 0x00047648u;
    DSD(ctx[2] + 0x1Cu) = 0x00047688u;
}
static void m_47720(const u32 *r, u32 *eax)            /* the EDX record started, not the slot's own */
{
    m_47720_at(r, 1, 0);
    *eax = 0u;
}
static void m_47720_ebx(const u32 *r, u32 *eax)        /* the context by EBX, not the record's +0x51 */
{
    m_47720_at(r, 0, 1);
    *eax = 0u;
}
static void m_476fc_at(const u32 *r, int sext, s32 bound)
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    s32 b = sext ? (s32)(s8)DSB(rec + 0x63u) : (s32)(u32)DSB(rec + 0x63u);
    if (b < bound) return;
    DSD(slot + 0x18u) = 0x00047648u;
    DSD(slot + 0x1Cu) = 0x00047688u;
    DSD(slot + 0x0Cu) = 0u;
}
static void m_476fc(const u32 *r, u32 *eax)            /* the bound 6 */
{
    m_476fc_at(r, 0, 6);
    *eax = 0u;
}
static void m_476fc_sext(const u32 *r, u32 *eax)       /* +0x63 read as a signed byte */
{
    m_476fc_at(r, 1, 5);
    *eax = 0u;
}
static void m_47648(const u32 *r, u32 *eax)            /* flag 0 left at 2 */
{
    u32 ctx[6], k;
    u8 f[16];
    fighter_ctx_same(ctx, r[R_EAX]);
    for (k = 0; k < 16u; k++) f[k] = 2u;
    f[1] = 0;
    f[8] = 0;
    *eax = (u32)fighter_18c14(ctx[0], f, 0u, 0u);
}
static void m_47688_at(u32 side, u32 stance, int own_pivot, int zext)
{
    u32 ctx[6];
    u32 w;
    fighter_ctx_same(ctx, side);
    if ((u8)fighter_command_dispatch(ctx[1], (u32)DSB(ctx[2] + 0x5Fu)) != 0u) return;
    fighter_39834(ctx[1], (s32)(u32)DSB(ctx[2] + 0x5Fu));
    if (DSB(ctx[3] + 0x54u) == 2u) fighter_39fb0(own_pivot ? ctx[2] : ctx[3]);
    else fighter_3a95c(ctx[1], stance);
    w = DSW(0x000BEDD8u);
    fighter_39a10(ctx[5], zext ? w : (u32)(s32)(s16)w);
}
static void m_47688(const u32 *r, u32 *eax)            /* 0x3A95C with 0xE */
{
    m_47688_at(r[R_EAX], 0x0Eu, 0, 0);
    *eax = 0u;
}
static void m_47688_pivot(const u32 *r, u32 *eax)      /* 0x39FB0 on the own slot */
{
    m_47688_at(r[R_EAX], 0x0Fu, 1, 0);
    *eax = 0u;
}
static void m_47688_zext(const u32 *r, u32 *eax)       /* the timer word zero-extended */
{
    m_47688_at(r[R_EAX], 0x0Fu, 0, 1);
    *eax = 0u;
}

'''

sub(F, '''    fighter_state_35838(slot, rec, dir);                    /* 0x489CC/0x489CE */
}
''', '''    fighter_state_35838(slot, rec, dir);                    /* 0x489CC/0x489CE */
}
''' + FIGHTER_C)
sub("port/src/game/fighter.h", "void fighter_489a0(u32 slot, u32 rec, u32 side);\n",
    """void fighter_489a0(u32 slot, u32 rec, u32 side);
/* §P3.4: 0x47720 and its +0x0C callback (slot, rec, side), +0x18 hook
 * fn(side) and +0x1C callback fn(side); and the callee 0x3B298 (no longer
 * file-local, so the harness's mutants can call it). */
int fighter_command_dispatch(u32 side, u32 edx_arg);
void fighter_47720(u32 slot, u32 rec, u32 side);
void fighter_476fc(u32 slot, u32 rec, u32 side);
u32  fighter_47648(u32 side);
void fighter_47688(u32 side);
""")
sub("port/src/game/actors.c", "    fn_register(0x489A0u, (void (*)(void))fighter_489a0);\n",
    """    fn_register(0x489A0u, (void (*)(void))fighter_489a0);
    /* PORT: record 2026-10-03-reverse-p3 §P3.4. Character 2's reaction-0x20
     * callback 0x47720 (the dword at 0xA41A8) and the three callbacks it
     * stores (+0x0C 0x476FC, 0x3531C case 7, (slot, rec, side); +0x18 0x47648,
     * 0x19020, fn(side) with EAX returned; +0x1C 0x47688, 0x193B0's 0x19505,
     * fn(side)); 0x476FC stores the last two again. */
    fn_register(0x47720u, (void (*)(void))fighter_47720);
    fn_register(0x476FCu, (void (*)(void))fighter_476fc);
    fn_register(0x47648u, (void (*)(void))fighter_47648);
    fn_register(0x47688u, (void (*)(void))fighter_47688);
""")
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_489a0@mutant",     m_489a0,        0x00000000u },\n',
    """    { "fighter_489a0@mutant",     m_489a0,        0x00000000u },
    { "fighter_47720",            b_47720,        0x00000000u },
    { "fighter_476fc",            b_476fc,        0x00000000u },
    { "fighter_47648",            b_47648,        0xFFFFFFFFu },
    { "fighter_47688",            b_47688,        0x00000000u },
    { "fighter_47720@mutant",     m_47720,        0x00000000u },
    { "fighter_47720@ebx",        m_47720_ebx,    0x00000000u },
    { "fighter_476fc@mutant",     m_476fc,        0x00000000u },
    { "fighter_476fc@sext",       m_476fc_sext,   0x00000000u },
    { "fighter_47648@mutant",     m_47648,        0xFFFFFFFFu },
    { "fighter_47688@mutant",     m_47688,        0x00000000u },
    { "fighter_47688@pivot",      m_47688_pivot,  0x00000000u },
    { "fighter_47688@zext",       m_47688_zext,   0x00000000u },
""")
print("t3_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_47720 fighter_476fc fighter_47648 fighter_47688; do
  python3 tools/diff_verify.py --image /tmp/pr_p3_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected: no compiler message, then

```
t3_port applied
all checks passed
| fighter_47720 | 0x47720 | 2 | 1/1 | VERIFIED | 339AC allow unverified, 3C4CC stub unverified |
| fighter_47720@mutant | 0x47720 | 2 | 1/1 | MISMATCH | 339AC allow unverified, 3C4CC stub unverified |
| fighter_47720@ebx | 0x47720 | 2 | 1/1 | MISMATCH | 339AC allow unverified, 3C4CC stub unverified |
| fighter_476fc | 0x476FC | 3 | 3/3 | VERIFIED | - |
| fighter_476fc@mutant | 0x476FC | 3 | 3/3 | MISMATCH | - |
| fighter_476fc@sext | 0x476FC | 3 | 3/3 | MISMATCH | - |
| fighter_47648 | 0x47648 | 2 | 1/1 | VERIFIED | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_47648@mutant | 0x47648 | 2 | 1/1 | MISMATCH | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_47688 | 0x47688 | 5 | 6/6 | VERIFIED | 33950 allow unverified, 39834 stub unverified, 39A10 stub unverified, 39FB0 stub unverified, 3A95C stub unverified, 3B298 stub unverified |
| fighter_47688@mutant | 0x47688 | 5 | 6/6 | MISMATCH | 33950 allow unverified, 39834 stub unverified, 39A10 stub unverified, 39FB0 stub unverified, 3A95C stub unverified, 3B298 stub unverified |
| fighter_47688@pivot | 0x47688 | 5 | 6/6 | MISMATCH | 33950 allow unverified, 39834 stub unverified, 39A10 stub unverified, 39FB0 stub unverified, 3A95C stub unverified, 3B298 stub unverified |
| fighter_47688@zext | 0x47688 | 5 | 6/6 | MISMATCH | 33950 allow unverified, 39834 stub unverified, 39A10 stub unverified, 39FB0 stub unverified, 3A95C stub unverified, 3B298 stub unverified |
```

A row that is not `VERIFIED` is a finding, not something to adjust: stop, compare the C with the bytes (the record's section), and report.

- [ ] **Step 4: the E2 table** (`make entry-triage` first, then regenerate):

```bash
make entry-triage E2_IMAGE=/tmp/pr_p3_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/pr_p3_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
  --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
grep -E '^\| (callbacks|finishers|animation-targets|stubs) ' docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git diff --stat docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -1
```

Expected:

```
entry-triage: FAIL: docs/superpowers/plans/2026-10-01-reverse-e2-triage.md differs from a fresh run (regenerate it with --out)
make: *** [entry-triage] Error 1
entry-triage: targets 291 unported, 204 ported; supplement 131 (19 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 31 in unported code, 84 in ported code, 19 nowhere
| callbacks | 3 | 68 |
| finishers | 0 | 9 |
| animation-targets | 54 | 58 |
| stubs | 56 |
 1 file changed, 6 insertions(+), 6 deletions(-)
```

- [ ] **Step 5: the task gate.**

```bash
make diff-verify entry-triage DIFF_IMAGE=/tmp/pr_p3_d.bin DIFF_TABLE=/tmp/pr_p3_d.md E2_IMAGE=/tmp/pr_p3_e2.bin 2>&1 \
  | grep -E '^(Ran|OK|FAILED|diff-verify:|entry-triage: targets)'
```

Expected (the times vary):

```
Ran 161 tests in N.NNNs
OK
diff-verify: 63/63 functions VERIFIED; 104/104 mutants detected; 1 named gaps; 10/49 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
Ran 44 tests in N.NNNs
OK
entry-triage: targets 291 unported, 204 ported; supplement 131 (19 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 6: every new unit assertion can fail.** Six mutations:

```bash
python3 - <<'PY'
import os
import pathlib
import subprocess


def mutate(path, old, new):
    """Apply one mutation, rebuild, run the suite, print the first FAIL lines, restore the file."""
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



mutate("port/src/game/actors.c", "    fn_register(0x47688u, (void (*)(void))fighter_47688);\n", "")
mutate("port/src/game/fighter.c",
       "    DSD(ctx[2] + 0x0Cu) = 0x000476FCu;                      /* 0x47751 */",
       "    DSD(ctx[2] + 0x0Cu) = 0x00047648u;                      /* 0x47751 */")
mutate("port/src/game/fighter.c",
       "    hit_anim_ctx(ctx, rec);                                 /* 0x47723/0x47725 0x339AC */",
       "    fighter_ctx_same(ctx, side);                            /* 0x47723/0x47725 0x339AC */")
mutate("port/src/game/fighter.c",
       "    if ((s32)(u32)DSB(rec + 0x63u) < 5) return;             /* 0x476FC..0x47708 */",
       "    if ((s32)(s8)DSB(rec + 0x63u) < 5) return;              /* 0x476FC..0x47708 */")
mutate("port/src/game/fighter.c",
       "    flags[0] = 1u;                                          /* 0x47666/0x47674 */",
       "    flags[0] = 2u;                                          /* 0x47666/0x47674 */")
mutate("port/src/game/fighter.c",
       "    if (DSB(ctx[3] + 0x54u) == 2u)                          /* 0x476C5..0x476CD */",
       "    if (DSB(ctx[2] + 0x54u) == 2u)                          /* 0x476C5..0x476CD */")
PY
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
```

Expected:

```
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:45715: 0x47688 is registered']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45742: 292424 != 292604']
port/src/game/fighter.c: 8 FAIL, first: ['test_fight.c:45738: 11259375 != 970268']
port/src/game/fighter.c: 3 FAIL, first: ['test_fight.c:45758: 202116108 != 0']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45773: 0 != 1']
port/src/game/fighter.c: 2 FAIL, first: ['test_fight.c:45789: 0 != 2']
all checks passed
```

- [ ] **Step 7: commit.**

```bash
git add tools/diff_verify.py tools/tests/test_diff_verify.py port/tests/test_fight.c port/tests/test.h \
  port/src/game/fighter.c port/src/game/fighter.h port/src/game/actors.c port/tests/diff_runner.c \
  docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git commit -m "fighter: port 0x47720 and its callbacks 0x476FC 0x47648 0x47688, seams 0x3B298 0x39FB0 0x3A95C; E2 table regenerated (track P batch 3)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: `0x47874` and the callbacks it stores, `0x47830 0x47798 0x477A8 0x477E8` (seams `0x3C190 0x3B714`; a +0x14 callback)

**Files:** as Task 3.

**Interfaces:** produces `void fighter_47874(u32 slot, u32 rec, u32 side)`, `void fighter_47830(u32 slot, u32 rec, u32 side)`, `u32 fighter_47798(u32 slot)` (the +0x14 shape `fighter_slot14_cb`: EAX = EDX = the slot, EAX tested whole), `u32 fighter_477a8(u32 side)`, `void fighter_477e8(u32 side)`; `fighter_3c190(u32 side, u32 v)` exported with `PR_SEAM(0x3C190u, ...)`; `PR_SEAM(0x3B714u, ...)` in `fighter_reaction`; `SPEED`, `REACT`, `P3_SLOT_CBS`, `p3_47874`, `p3_47830`, `p3_477e8`; `test_p3_47874`. Consumes Task 3's `p3_hook0`, Task 2's `p3_set_bit15`. Record §P3.5.

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


sub("tools/diff_verify.py", '''    ], allow_calls=(0x33950,), calls=(DISPATCH, POSE, PIVOT, STANCE, TIMER), eax_mask=0,
       mutants=("@mutant", "@pivot", "@zext")),
]
''', '''    ], allow_calls=(0x33950,), calls=(DISPATCH, POSE, PIVOT, STANCE, TIMER), eax_mask=0,
       mutants=("@mutant", "@pivot", "@zext")),
]

# The callees the 0x47874 family stubs (record §P3.5): 0x3C190 EAX = side, EDX = the speed (`mov ebx,eax` before
# 0x1A570, EDX read after); 0x3B714 EAX = the other slot, EDX = the own slot (`mov esi,eax; mov ebp,edx`). Both
# plain `ret`, both clobber EDX.
SPEED = E.Call(0x3C190, ("eax", "edx"), clobbers=("edx",))
REACT = E.Call(0x3B714, ("eax", "edx"), clobbers=("edx",))
P3_SLOT_CBS = {E3_SLOT + 0x0C: b"".join(le32(v * 0x01010101) for v in (0x0C, 0x10, 0x14, 0x18, 0x1C)),
               E3_SLOT + 0x52: b"\\x52\\x53\\x54"}


# 0x47874 (record §P3.5): the EDX record on 0xED974 at 2.0, the EAX slot 9/7/0 with four callbacks (+0x0C 0x47830,
# +0x18 0x477A8, +0x1C 0x477E8, +0x14 0x47798), then 0x3C190(rec+0x51, 0x80) and the voice 0x4B. EBX is not read:
# every case has EBX = 1 - rec+0x51.
def p3_47874(cid, r51, voice_al=1):
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": 1 - r51}, {**P3_SLOT_CBS, E3_REC + 0x51: bytes([r51])},
                {0x2C3FC: voice_al})


# 0x47830 (the +0x0C callback; 0x3531C case 7): the command word DS_001088E0[rec+0x51] (the whole byte index); with
# both bits 0x100 and 0x800 set (`xor dl,dl; and dh,9; cmp edx,0x900`) nothing, else the record on 0xED9A4 at 2.0
# and the slot's +0x0C/+0x14 = 0. EBX is 1 - rec+0x51, the other word a sentinel that takes the other branch.
def p3_47830(cid, r51, own, other):
    words = [own, other] if r51 == 0 else [other, own]
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": 1 - r51},
                {**P3_SLOT_CBS, E3_REC + 0x51: bytes([r51]), 0x1088E0: le32(words[0])[:2] + le32(words[1])[:2]})


# 0x477E8 (the +0x1C callback; 0x193B0, fn(side)): 0x3B714(the other slot, the own slot), the own record on 0xED9A4
# at 2.0, the own slot's +0x0C/+0x14 = 0 (both slots' +0x0C..+0x1F seeded).
def p3_477e8(cid, side):
    return Case(cid, {"eax": side},
                {**SLOT_PTRS, DS_SLOTS + 0x0C: b"".join(le32(v * 0x01010101) for v in (0x0C, 0x10, 0x14, 0x18, 0x1C)),
                 DS_SLOTS + 0x94 + 0x0C: b"".join(le32(v * 0x01010101) for v in (0x8C, 0x90, 0x94, 0x98, 0x9C))})


P3_SPECS += [
    Spec("fighter_47874", 0x47874, [p3_47874("v0", 0), p3_47874("v1", 1, 0)],
         calls=(HIT_B, SPEED, VOICE), eax_mask=0, mutants=("@mutant", "@side")),
    Spec("fighter_47830", 0x47830, [
        p3_47830("z0", 0, 0x0900, 0), p3_47830("z1", 0, 0x0100, 0x0900), p3_47830("z2", 1, 0x0800, 0x0900),
        p3_47830("z3", 1, 0xF6FF, 0x0900), p3_47830("z4", 0, 0xFFFF, 0),
    ], calls=(ANIM_BEGIN,), eax_mask=0, mutants=("@mutant", "@side", "@order")),
    # 0x47798 (the +0x14 callback; 0x1952F/0x3514C/0x350B8, fn(slot) with EAX = EDX = the slot, the whole EAX
    # tested): the voice 0x4C, then EAX = 1 (`mov eax,1` over the voice's EAX: w1's stub returns 0).
    Spec("fighter_47798", 0x47798, [
        Case("w0", {"eax": E3_SLOT, "edx": E3_SLOT}),
        Case("w1", {"eax": E3_SLOT, "edx": E3_SLOT}, {}, {0x2C3FC: 0}),
    ], calls=(VOICE,), mutants=("@mutant", "@eax")),
    Spec("fighter_477a8", 0x477A8, [p3_hook0("k0", 0, 0), p3_hook0("k1", 1, 0x12345678)],
         allow_calls=(0x33950, 0x18BD4), calls=(CHECKS,), mutants=("@mutant",)),
    Spec("fighter_477e8", 0x477E8, [p3_477e8("y0", 0), p3_477e8("y1", 1)],
         allow_calls=(0x33950,), calls=(REACT, ANIM_BEGIN), eax_mask=0, mutants=("@mutant", "@order")),
]
''')
T = "tools/tests/test_diff_verify.py"
sub(T, '''            "fighter_47720": 0, "fighter_476fc": 0, "fighter_47648": 0xFFFFFFFF, "fighter_47688": 0}''',
    '''            "fighter_47720": 0, "fighter_476fc": 0, "fighter_47648": 0xFFFFFFFF, "fighter_47688": 0,
            "fighter_47874": 0, "fighter_47830": 0, "fighter_47798": 0xFFFFFFFF, "fighter_477a8": 0xFFFFFFFF,
            "fighter_477e8": 0}''')
sub(T, '''            "fighter_47688@zext": {"call #3"}}''',
    '''            "fighter_47688@zext": {"call #3"},
            "fighter_47874@mutant": {"call #1 memory"}, "fighter_47874@side": {"call #1"},
            "fighter_47830@mutant": {"byte", "call #0"}, "fighter_47830@side": {"byte", "call #0"},
            "fighter_47830@order": {"call #0 memory"},
            "fighter_47798@mutant": {"call #0"}, "fighter_47798@eax": {"eax"},
            "fighter_477a8@mutant": {"call #0"},
            "fighter_477e8@mutant": {"call #0"}, "fighter_477e8@order": {"call #1 memory"}}''')
sub(T, '''                          ("fighter_47688@pivot", ["h1", "h3"])):''',
    '''                          ("fighter_47688@pivot", ["h1", "h3"]),
                          # 0x47830 skips only with both bits 0x100 and 0x800 (z1/z2 have one), indexes by
                          # rec+0x51 (every case: the other word takes the other branch), and 0x47798's 1 replaces
                          # the voice's EAX (w1's stub returns 0)
                          ("fighter_47830@mutant", ["z1", "z2"]),
                          ("fighter_47830@side", ["z0", "z1", "z2", "z3", "z4"]),
                          ("fighter_47798@eax", ["w1"])):''')
sub(T, '''                                 0x3B298: ("edx", "edi", "ebp"), 0x39FB0: (), 0x3A95C: ("edx",)})''',
    '''                                 0x3B298: ("edx", "edi", "ebp"), 0x39FB0: (), 0x3A95C: ("edx",),
                                 0x3C190: ("edx",), 0x3B714: ("edx",)})''')
sub(T, '''        self.assertIn("diff-verify: 63/63 functions VERIFIED; 104/104 mutants detected; 1 named gaps; "
                      "10/49 rows with callees closed (14 have none).", out.getvalue())''',
    '''        self.assertIn("diff-verify: 68/68 functions VERIFIED; 114/114 mutants detected; 1 named gaps; "
                      "10/54 rows with callees closed (14 have none).", out.getvalue())''')
print("t4_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p3_img.bin --function fighter_47874 | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected (the binding does not exist yet):

```
t4_spec applied
| fighter_47874 | 0x47874 | 2 | 1/1 | MISMATCH | 2C3FC stub unverified, 3C190 stub unverified, 3C4CC stub unverified |
  fighter_47874: v0: port: unknown binding fighter_47874
  fighter_47874: v1: port: unknown binding fighter_47874
diff-verify: 0/1 functions VERIFIED; 0 named gaps; 0/1 rows with callees closed (0 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
```

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


TEST = r'''
/* §P3.5: 0x47874 and the four callbacks it stores, through their
 * registrations (slot 0, its record, side 0). The image dwords are
 * evidence lines, not port behaviour. */
static void p3_check_47874(void)
{
    p2_cb_fn f;
    p2_hook_fn h;
    p2_side_fn g;
    u8 fl[16];
    u32 k, r;
    CHECK(fn_resolve(0x47874u) == (void (*)(void))fighter_47874, "0x47874 is registered");
    CHECK(fn_resolve(0x47830u) == (void (*)(void))fighter_47830, "0x47830 is registered");
    CHECK(fn_resolve(0x47798u) == (void (*)(void))fighter_47798, "0x47798 is registered");
    CHECK(fn_resolve(0x477A8u) == (void (*)(void))fighter_477a8, "0x477A8 is registered");
    CHECK(fn_resolve(0x477E8u) == (void (*)(void))fighter_477e8, "0x477E8 is registered");
    CHECK_EQ_INT((int)DSD(0x000A41BCu), 0x00047874);
    CHECK_EQ_INT((int)DSD(0x0004789Au), 0x00047830);
    CHECK_EQ_INT((int)DSD(0x000478A1u), 0x000477A8);
    CHECK_EQ_INT((int)DSD(0x000478A8u), 0x000477E8);
    CHECK_EQ_INT((int)DSD(0x000478AFu), 0x00047798);

    /* 0x47874: the record on 0xED974 at 2.0, the slot 9/7/0 with the four
     * callbacks, 0x3C190(0, 0x80) (side 0's actor bit 15 clear: the record's
     * word +0x34 = -0x80), the voice 0x4B. */
    f = (p2_cb_fn)(void *)fn_resolve(0x47874u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    p3_set_bit15(0u, 0);
    DSW(0x000ED974u) = 0x12B1u;
    DSW(Z_R0 + 0x34u) = 0x3434u;
    DSB(Z_S0 + 0x54u) = 0x44u;
    DSD(Z_S0 + 0x0Cu) = 0x0C0C0C0Cu;
    DSD(Z_S0 + 0x14u) = 0x14141414u;
    DSD(Z_S0 + 0x18u) = 0x18181818u;
    DSD(Z_S0 + 0x1Cu) = 0x1C1C1C1Cu;
    sound_voice_log_reset();
    f(Z_S0, Z_R0, 1u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000ED974);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x0Cu), 0x00047830);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x14u), 0x00047798);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x18u), 0x000477A8);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x1Cu), 0x000477E8);
    CHECK_EQ_INT((int)DSW(Z_R0 + 0x34u), 0xFF80);
    CHECK_EQ_INT((int)sound_voice_log_count(), 1);
    CHECK_EQ_INT((int)sound_voice_log_at(0), 0x4B);
    sound_voice_log_reset();

    /* 0x47830: side 0's command word 0x0900 keeps the slot; 0x0100 starts the
     * record on 0xED9A4 at 2.0 and clears +0x0C/+0x14. */
    f = (p2_cb_fn)(void *)fn_resolve(0x47830u);
    if (f == NULL) return;
    for (k = 0; k < 2u; k++) {
        z_fseed();
        DSB(Z_R0 + 0x51u) = 0u;
        DSW(DS_001088E0) = k == 0u ? 0x0900u : 0x0100u;
        DSW(0x000ED9A4u) = 0x12B1u;
        DSD(Z_R0 + 8u) = 0x08080808u;
        DSD(Z_S0 + 0x0Cu) = 0x0C0C0C0Cu;
        DSD(Z_S0 + 0x14u) = 0x14141414u;
        f(Z_S0, Z_R0, 0u);
        CHECK_EQ_INT((int)DSD(Z_R0 + 8u), k == 0u ? 0x08080808 : 0x000ED9A4);
        CHECK_EQ_INT((int)DSD(Z_S0 + 0x0Cu), k == 0u ? 0x0C0C0C0C : 0);
        CHECK_EQ_INT((int)DSD(Z_S0 + 0x14u), k == 0u ? 0x14141414 : 0);
    }

    /* 0x47798, as the +0x14 callers call it (the slot): the voice 0x4C, then
     * 1 (the caller then zeroes the field). */
    h = (p2_hook_fn)(void *)fn_resolve(0x47798u);
    if (h == NULL) return;
    sound_voice_log_reset();
    CHECK_EQ_INT((int)h(Z_S0), 1);
    CHECK_EQ_INT((int)sound_voice_log_count(), 1);
    CHECK_EQ_INT((int)sound_voice_log_at(0), 0x4C);
    sound_voice_log_reset();

    /* 0x477A8: 0x18C14(side, flags 0 = 1, 1/8 = 0, the rest 2, 0, 0) on the
     * same seeded state. */
    h = (p2_hook_fn)(void *)fn_resolve(0x477A8u);
    if (h == NULL) return;
    z_fseed();
    r = h(1u);
    z_fseed();
    for (k = 0; k < 16u; k++) fl[k] = 2u;
    fl[0] = 1u;
    fl[1] = fl[8] = 0u;
    CHECK_EQ_INT((int)r, fighter_18c14(1u, fl, 0u, 0u));

    /* 0x477E8 on side 0: after 0x3B714(slot 1, slot 0), the own record on
     * 0xED9A4 at 2.0 and slot 0's +0x0C/+0x14 = 0. */
    g = (p2_side_fn)(void *)fn_resolve(0x477E8u);
    if (g == NULL) return;
    z_fseed();
    DSW(0x000ED9A4u) = 0x12B1u;
    DSD(Z_S0 + 0x0Cu) = 0x0C0C0C0Cu;
    DSD(Z_S0 + 0x14u) = 0x14141414u;
    DSD(Z_S1 + 0x0Cu) = 0x8C8C8C8Cu;
    g(0u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000ED9A4);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x0Cu), 0);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x14u), 0);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x0Cu), (int)0x8C8C8C8Cu);
}

int test_p3_47874(void)         { return u6b_run(p3_check_47874); }
'''

ANCHOR = "int test_p3_47720(void)         { return u6b_run(p3_check_47720); }\n"
sub("port/tests/test_fight.c", ANCHOR, ANCHOR + TEST)
sub("port/tests/test.h", "    X(test_p3_47720) \\\n", "    X(test_p3_47720) \\\n    X(test_p3_47874) \\\n")
print("t4_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected (line numbers move with `test_fight.c`):

```
t4_test applied
port/tests/test_fight.c:45807:51: error: use of undeclared identifier 'fighter_47874'
port/tests/test_fight.c:45808:51: error: use of undeclared identifier 'fighter_47830'
port/tests/test_fight.c:45809:51: error: use of undeclared identifier 'fighter_47798'
port/tests/test_fight.c:45810:51: error: use of undeclared identifier 'fighter_477a8'
port/tests/test_fight.c:45811:51: error: use of undeclared identifier 'fighter_477e8'
5 errors generated.
```

- [ ] **Step 3: the port, the registrations, the seams, the bindings and the mutants.**

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
# 0x3C190 and 0x3B714 get their seams; the harness's mutants call 0x3C190, so it loses `static`
sub(F, '''static void fighter_3c190(u32 side, u32 v)
{
''', '''void fighter_3c190(u32 side, u32 v)
{
    PR_SEAM(0x3C190u, side, v);
''')
sub(F, '''void fighter_reaction(u32 param_1, u32 param_2)
{
''', '''void fighter_reaction(u32 param_1, u32 param_2)
{
    PR_SEAM(0x3B714u, param_1, param_2);
''')

FIGHTER_C = r'''
#define P3_ANIM_47874 0x000ED974u  /* 0x4787A */
#define P3_ANIM_ED9A4 0x000ED9A4u  /* 0x47808 (0x477E8), 0x47855 (0x47830) */

/* 0x47874 — record §P3.5. Character 2's reaction-0x21 callback (the dword at
 * 0xA41BC): the EDX record on 0xED974 at 2.0 (0x3C4CC); the EAX slot 9/7/0
 * with the +0x0C callback 0x47830 (0x3531C case 7), the +0x18 hook 0x477A8
 * (0x19020), the +0x1C callback 0x477E8 (0x193B0's 0x19505) and the +0x14
 * callback 0x47798 (0x1952F/0x3514C/0x350B8); then 0x3C190(rec+0x51, 0x80)
 * and the voice 0x4B. EBX is not read. PORT: AL = 1 (0x478CC) unread
 * (§P2.2). */
void fighter_47874(u32 slot, u32 rec, u32 side)
{
    (void)side;
    hit_anim_start_b(rec, P3_ANIM_47874, 0x40000000u);      /* 0x4787A..0x47886 0x3C4CC */
    DSB(slot + 0x53u) = 7u;                                 /* 0x4788B */
    DSB(slot + 0x54u) = 0u;                                 /* 0x4788F */
    DSB(slot + 0x52u) = 9u;                                 /* 0x47893 */
    DSD(slot + 0x0Cu) = 0x00047830u;                        /* 0x47897 */
    DSD(slot + 0x18u) = 0x000477A8u;                        /* 0x4789E */
    DSD(slot + 0x1Cu) = 0x000477E8u;                        /* 0x478A5 */
    DSD(slot + 0x14u) = 0x00047798u;                        /* 0x478AC */
    fighter_3c190((u32)DSB(rec + 0x51u), 0x80u);            /* 0x478B3..0x478BD 0x3C190 */
    (void)sound_voice(0x4Bu);                               /* 0x478C2/0x478C7 0x2C3FC */
}

/* 0x47830 — record §P3.5. The slot +0x0C callback 0x47874 stores (the dword at
 * 0x4789A; 0x3531C case 7, (slot, rec, side), EAX unread). With the command
 * word DS_001088E0[rec+0x51] (the whole byte index) holding both bits 0x100
 * and 0x800 nothing; else the EDX record on 0xED9A4 at 2.0 (0x2BC30) and the
 * EAX slot's +0x0C = +0x14 = 0. */
void fighter_47830(u32 slot, u32 rec, u32 side)
{
    (void)side;
    if ((DSW(DS_001088E0 + (u32)DSB(rec + 0x51u) * 2u) & 0x0900u) == 0x0900u)   /* 0x47835..0x47853 */
        return;
    actors_anim_begin(rec, P3_ANIM_ED9A4, 0x40000000u);     /* 0x47855..0x4785F 0x2BC30 */
    DSD(slot + 0x0Cu) = 0u;                                 /* 0x47864 */
    DSD(slot + 0x14u) = 0u;                                 /* 0x4786B */
}

/* 0x47798 — record §P3.5. The slot +0x14 callback 0x47874 stores (the dword at
 * 0x478AF), as 0x1952F, 0x350D0 (0x3514C) and 0x35050 (0x350B8) call it
 * (EAX = EDX = the slot; a non-zero EAX zeroes the field): the voice 0x4C,
 * then 1 (`mov eax,1` over the voice's EAX). */
u32 fighter_47798(u32 slot)
{
    (void)slot;
    (void)sound_voice(0x4Cu);                               /* 0x47798/0x4779D 0x2C3FC */
    return 1u;                                              /* 0x477A2 */
}

/* 0x477A8 — record §P3.5. The slot +0x18 hook 0x47874 stores (the dword at
 * 0x478A1; 0x19020, fn(side), the whole EAX tested): 0x47648's bytes at
 * another address. The flags 1 and 8 = 0 and 0 = 1; 0x18C14(side, the
 * flags, 0, 0)'s result. */
u32 fighter_477a8(u32 side)
{
    u32 ctx[6];
    u8 flags[16];
    fighter_ctx_same(ctx, side);                            /* 0x477AE..0x477B4 0x33950 */
    fighter_18bd4(flags);                                   /* 0x477B9..0x477BF 0x18BD4 */
    flags[1] = 0;                                           /* 0x477C8 */
    flags[8] = 0;                                           /* 0x477CC */
    flags[0] = 1u;                                          /* 0x477C6/0x477D4 */
    return (u32)fighter_18c14(ctx[0], flags, 0u, 0u);       /* 0x477BD/0x477B2, 0x477D8/0x477DB 0x18C14 */
}

/* 0x477E8 — record §P3.5. The slot +0x1C callback 0x47874 stores (the dword at
 * 0x478A8; 0x193B0's 0x19505, fn(side), EAX unread). The context is
 * 0x33950(side). 0x3B714(the other slot, the own slot); the own record on
 * 0xED9A4 at 2.0 (0x2BC30); the own slot's +0x0C = +0x14 = 0 (EBX = ctx[2],
 * loaded before the call, which keeps it). */
void fighter_477e8(u32 side)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);                            /* 0x477ED..0x477F1 0x33950 */
    fighter_reaction(ctx[3], ctx[2]);                       /* 0x477F6..0x477FE 0x3B714 */
    actors_anim_begin(ctx[4], P3_ANIM_ED9A4, 0x40000000u);  /* 0x47803..0x47815 0x2BC30 */
    DSD(ctx[2] + 0x0Cu) = 0u;                               /* 0x4781A */
    DSD(ctx[2] + 0x14u) = 0u;                               /* 0x47821 */
}
'''

BINDINGS = r'''/* §P3.5: 0x47874 (0x34E2C at 0x35045) and its +0x0C callback 0x47830 (0x3531C case 7), mask 0; the +0x14
 * callback 0x47798 (fn(slot), EAX = EDX = the slot; the callers test the whole EAX); the +0x18 hook 0x477A8 (the
 * whole EAX); the +0x1C callback 0x477E8 (mask 0). */
static void b_47874(const u32 *r, u32 *eax)            { fighter_47874(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_47830(const u32 *r, u32 *eax)            { fighter_47830(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_47798(const u32 *r, u32 *eax)            { *eax = fighter_47798(r[R_EAX]); }
static void b_477a8(const u32 *r, u32 *eax)            { *eax = fighter_477a8(r[R_EAX]); }
static void b_477e8(const u32 *r, u32 *eax)            { fighter_477e8(r[R_EAX]); *eax = 0u; }
static void m_47874_at(const u32 *r, int late, int by_ebx)
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    hit_anim_start_b(rec, 0x000ED974u, 0x40000000u);
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x54u) = 0u;
    DSB(slot + 0x52u) = 9u;
    if (!late) {
        DSD(slot + 0x0Cu) = 0x00047830u;
        DSD(slot + 0x18u) = 0x000477A8u;
        DSD(slot + 0x1Cu) = 0x000477E8u;
        DSD(slot + 0x14u) = 0x00047798u;
    }
    fighter_3c190(by_ebx ? r[R_EBX] : (u32)DSB(rec + 0x51u), 0x80u);
    if (late) {
        DSD(slot + 0x0Cu) = 0x00047830u;
        DSD(slot + 0x18u) = 0x000477A8u;
        DSD(slot + 0x1Cu) = 0x000477E8u;
        DSD(slot + 0x14u) = 0x00047798u;
    }
    (void)sound_voice(0x4Bu);
}
static void m_47874(const u32 *r, u32 *eax)            /* the four callbacks stored after 0x3C190 */
{
    m_47874_at(r, 1, 0);
    *eax = 0u;
}
static void m_47874_side(const u32 *r, u32 *eax)       /* 0x3C190 on EBX, not rec+0x51 */
{
    m_47874_at(r, 0, 1);
    *eax = 0u;
}
static void m_47830_at(const u32 *r, int any, int by_ebx, int early)
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u32 w = DSW(0x001088E0u + (by_ebx ? r[R_EBX] : (u32)DSB(rec + 0x51u)) * 2u) & 0x0900u;
    if (any ? w != 0u : w == 0x0900u) return;
    if (early) {
        DSD(slot + 0x0Cu) = 0u;
        DSD(slot + 0x14u) = 0u;
    }
    actors_anim_begin(rec, 0x000ED9A4u, 0x40000000u);
    DSD(slot + 0x0Cu) = 0u;
    DSD(slot + 0x14u) = 0u;
}
static void m_47830(const u32 *r, u32 *eax)            /* either bit skips */
{
    m_47830_at(r, 1, 0, 0);
    *eax = 0u;
}
static void m_47830_side(const u32 *r, u32 *eax)       /* the command word by EBX, not rec+0x51 */
{
    m_47830_at(r, 0, 1, 0);
    *eax = 0u;
}
static void m_47830_order(const u32 *r, u32 *eax)      /* +0x0C/+0x14 zeroed before the 0x2BC30 call */
{
    m_47830_at(r, 0, 0, 1);
    *eax = 0u;
}
static void m_47798(const u32 *r, u32 *eax)            /* the voice 0x4B */
{
    (void)r;
    (void)sound_voice(0x4Bu);
    *eax = 1u;
}
static void m_47798_eax(const u32 *r, u32 *eax)        /* the voice's EAX returned */
{
    (void)r;
    *eax = sound_voice(0x4Cu);
}
static void m_477a8(const u32 *r, u32 *eax)            /* flag 8 left at 2 */
{
    u32 ctx[6], k;
    u8 f[16];
    fighter_ctx_same(ctx, r[R_EAX]);
    for (k = 0; k < 16u; k++) f[k] = 2u;
    f[1] = 0;
    f[0] = 1u;
    *eax = (u32)fighter_18c14(ctx[0], f, 0u, 0u);
}
static void m_477e8_at(u32 side, int swapped, int early)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);
    if (swapped) fighter_reaction(ctx[2], ctx[3]);
    else fighter_reaction(ctx[3], ctx[2]);
    if (early) {
        DSD(ctx[2] + 0x0Cu) = 0u;
        DSD(ctx[2] + 0x14u) = 0u;
    }
    actors_anim_begin(ctx[4], 0x000ED9A4u, 0x40000000u);
    DSD(ctx[2] + 0x0Cu) = 0u;
    DSD(ctx[2] + 0x14u) = 0u;
}
static void m_477e8(const u32 *r, u32 *eax)            /* 0x3B714's two slots swapped */
{
    m_477e8_at(r[R_EAX], 1, 0);
    *eax = 0u;
}
static void m_477e8_order(const u32 *r, u32 *eax)      /* +0x0C/+0x14 zeroed before the 0x2BC30 call */
{
    m_477e8_at(r[R_EAX], 0, 1);
    *eax = 0u;
}

'''

sub(F, '''    fighter_39a10(ctx[5], (u32)(s32)(s16)DSW(P3_BEDD8));    /* 0x476E4..0x476F1 */
}
''', '''    fighter_39a10(ctx[5], (u32)(s32)(s16)DSW(P3_BEDD8));    /* 0x476E4..0x476F1 */
}
''' + FIGHTER_C)
sub("port/src/game/fighter.h", "void fighter_47688(u32 side);\n",
    """void fighter_47688(u32 side);
/* §P3.5: 0x47874 and its +0x0C callback (slot, rec, side), +0x14 callback
 * fn(slot), +0x18 hook fn(side) and +0x1C callback fn(side); and the callee
 * 0x3C190 (no longer file-local, so the harness's mutants can call it). */
void fighter_3c190(u32 side, u32 v);
void fighter_47874(u32 slot, u32 rec, u32 side);
void fighter_47830(u32 slot, u32 rec, u32 side);
u32  fighter_47798(u32 slot);
u32  fighter_477a8(u32 side);
void fighter_477e8(u32 side);
""")
sub("port/src/game/actors.c", "    fn_register(0x47688u, (void (*)(void))fighter_47688);\n",
    """    fn_register(0x47688u, (void (*)(void))fighter_47688);
    /* PORT: record 2026-10-03-reverse-p3 §P3.5. Character 2's reaction-0x21
     * callback 0x47874 (the dword at 0xA41BC) and the four callbacks it
     * stores (+0x0C 0x47830, 0x3531C case 7, (slot, rec, side); +0x14 0x47798,
     * fn(slot) with EAX returned; +0x18 0x477A8, 0x19020, fn(side) with EAX
     * returned; +0x1C 0x477E8, 0x193B0's 0x19505, fn(side)). */
    fn_register(0x47874u, (void (*)(void))fighter_47874);
    fn_register(0x47830u, (void (*)(void))fighter_47830);
    fn_register(0x47798u, (void (*)(void))fighter_47798);
    fn_register(0x477A8u, (void (*)(void))fighter_477a8);
    fn_register(0x477E8u, (void (*)(void))fighter_477e8);
""")
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_47688@zext",       m_47688_zext,   0x00000000u },\n',
    """    { "fighter_47688@zext",       m_47688_zext,   0x00000000u },
    { "fighter_47874",            b_47874,        0x00000000u },
    { "fighter_47830",            b_47830,        0x00000000u },
    { "fighter_47798",            b_47798,        0xFFFFFFFFu },
    { "fighter_477a8",            b_477a8,        0xFFFFFFFFu },
    { "fighter_477e8",            b_477e8,        0x00000000u },
    { "fighter_47874@mutant",     m_47874,        0x00000000u },
    { "fighter_47874@side",       m_47874_side,   0x00000000u },
    { "fighter_47830@mutant",     m_47830,        0x00000000u },
    { "fighter_47830@side",       m_47830_side,   0x00000000u },
    { "fighter_47830@order",      m_47830_order,  0x00000000u },
    { "fighter_47798@mutant",     m_47798,        0xFFFFFFFFu },
    { "fighter_47798@eax",        m_47798_eax,    0xFFFFFFFFu },
    { "fighter_477a8@mutant",     m_477a8,        0xFFFFFFFFu },
    { "fighter_477e8@mutant",     m_477e8,        0x00000000u },
    { "fighter_477e8@order",      m_477e8_order,  0x00000000u },
""")
print("t4_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_47874 fighter_47830 fighter_47798 fighter_477a8 fighter_477e8; do
  python3 tools/diff_verify.py --image /tmp/pr_p3_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected: no compiler message, then

```
t4_port applied
all checks passed
| fighter_47874 | 0x47874 | 2 | 1/1 | VERIFIED | 2C3FC stub unverified, 3C190 stub unverified, 3C4CC stub unverified |
| fighter_47874@mutant | 0x47874 | 2 | 1/1 | MISMATCH | 2C3FC stub unverified, 3C190 stub unverified, 3C4CC stub unverified |
| fighter_47874@side | 0x47874 | 2 | 1/1 | MISMATCH | 2C3FC stub unverified, 3C190 stub unverified, 3C4CC stub unverified |
| fighter_47830 | 0x47830 | 5 | 3/3 | VERIFIED | 2BC30 stub unverified |
| fighter_47830@mutant | 0x47830 | 5 | 3/3 | MISMATCH | 2BC30 stub unverified |
| fighter_47830@side | 0x47830 | 5 | 3/3 | MISMATCH | 2BC30 stub unverified |
| fighter_47830@order | 0x47830 | 5 | 3/3 | MISMATCH | 2BC30 stub unverified |
| fighter_47798 | 0x47798 | 2 | 1/1 | VERIFIED | 2C3FC stub unverified |
| fighter_47798@mutant | 0x47798 | 2 | 1/1 | MISMATCH | 2C3FC stub unverified |
| fighter_47798@eax | 0x47798 | 2 | 1/1 | MISMATCH | 2C3FC stub unverified |
| fighter_477a8 | 0x477A8 | 2 | 1/1 | VERIFIED | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_477a8@mutant | 0x477A8 | 2 | 1/1 | MISMATCH | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_477e8 | 0x477E8 | 2 | 1/1 | VERIFIED | 2BC30 stub unverified, 33950 allow unverified, 3B714 stub unverified |
| fighter_477e8@mutant | 0x477E8 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 33950 allow unverified, 3B714 stub unverified |
| fighter_477e8@order | 0x477E8 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 33950 allow unverified, 3B714 stub unverified |
```

A row that is not `VERIFIED` is a finding, not something to adjust: stop, compare the C with the bytes (the record's section), and report.

- [ ] **Step 4: the E2 table** (`make entry-triage` first, then regenerate):

```bash
make entry-triage E2_IMAGE=/tmp/pr_p3_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/pr_p3_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
  --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
grep -E '^\| (callbacks|finishers|animation-targets|stubs) ' docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git diff --stat docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -1
```

Expected:

```
entry-triage: FAIL: docs/superpowers/plans/2026-10-01-reverse-e2-triage.md differs from a fresh run (regenerate it with --out)
make: *** [entry-triage] Error 1
entry-triage: targets 290 unported, 205 ported; supplement 131 (15 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 29 in unported code, 86 in ported code, 19 nowhere
| callbacks | 2 | 69 |
| finishers | 0 | 9 |
| animation-targets | 54 | 58 |
| stubs | 55 |
 1 file changed, 9 insertions(+), 9 deletions(-)
```

- [ ] **Step 5: the task gate.**

```bash
make diff-verify entry-triage DIFF_IMAGE=/tmp/pr_p3_d.bin DIFF_TABLE=/tmp/pr_p3_d.md E2_IMAGE=/tmp/pr_p3_e2.bin 2>&1 \
  | grep -E '^(Ran|OK|FAILED|diff-verify:|entry-triage: targets)'
```

Expected (the times vary):

```
Ran 161 tests in N.NNNs
OK
diff-verify: 68/68 functions VERIFIED; 114/114 mutants detected; 1 named gaps; 10/54 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
Ran 44 tests in N.NNNs
OK
entry-triage: targets 290 unported, 205 ported; supplement 131 (15 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 6: every new unit assertion can fail.** Six mutations:

```bash
python3 - <<'PY'
import os
import pathlib
import subprocess


def mutate(path, old, new):
    """Apply one mutation, rebuild, run the suite, print the first FAIL lines, restore the file."""
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



mutate("port/src/game/actors.c", "    fn_register(0x47798u, (void (*)(void))fighter_47798);\n", "")
mutate("port/src/game/fighter.c",
       "    DSD(slot + 0x14u) = 0x00047798u;                        /* 0x478AC */",
       "    DSD(slot + 0x14u) = 0u;                                 /* 0x478AC */")
mutate("port/src/game/fighter.c",
       "    if ((DSW(DS_001088E0 + (u32)DSB(rec + 0x51u) * 2u) & 0x0900u) == 0x0900u)   /* 0x47835..0x47853 */",
       "    if ((DSW(DS_001088E0 + (u32)DSB(rec + 0x51u) * 2u) & 0x0900u) != 0u)        /* 0x47835..0x47853 */")
mutate("port/src/game/fighter.c",
       "    return 1u;                                              /* 0x477A2 */",
       "    return 0u;                                              /* 0x477A2 */")
mutate("port/src/game/fighter.c",
       "    flags[0] = 1u;                                          /* 0x477C6/0x477D4 */",
       "    flags[0] = 2u;                                          /* 0x477C6/0x477D4 */")
mutate("port/src/game/fighter.c",
       "    DSD(ctx[2] + 0x14u) = 0u;                               /* 0x47821 */",
       "    DSD(ctx[3] + 0x14u) = 0u;                               /* 0x47821 */")
PY
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
```

Expected:

```
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:45809: 0x47798 is registered']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45841: 0 != 292760']
port/src/game/fighter.c: 3 FAIL, first: ['test_fight.c:45862: 134744072 != 973220']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45872: 0 != 1']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45887: 0 != 1']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45902: 336860180 != 0']
all checks passed
```

- [ ] **Step 7: commit.**

```bash
git add tools/diff_verify.py tools/tests/test_diff_verify.py port/tests/test_fight.c port/tests/test.h \
  port/src/game/fighter.c port/src/game/fighter.h port/src/game/actors.c port/tests/diff_runner.c \
  docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git commit -m "fighter: port 0x47874 and its callbacks 0x47830 0x47798 0x477A8 0x477E8, seams 0x3C190 0x3B714; E2 table regenerated (track P batch 3)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: `0x47FCC` and the callbacks it stores, `0x47CB0 0x47D24 0x47E9C` (x87; `0x47E9C` after its own table)

**Files:** as Task 3 (no new seam).

**Interfaces:** produces `void fighter_47fcc(u32 slot, u32 rec, u32 side)`, `u32 fighter_47cb0(u32 side)`, `void fighter_47d24(u32 side)`, `void fighter_47e9c(u32 slot, u32 rec, u32 side)`; `P3_108370_SEED`, `p3_47d24`, `p3_47e9c`; `test_p3_47fcc`. Consumes P2's `p2_ctx_case`, `p2_2116c`, `f32`, `FLASH`, `FACING`, `HOLD`, `PLACE`, the static `p2_f32`/`p2_set_f32`/`p2_f64` and the macros `P2_STREAMS_21374` (`0xC8950`), `P2_STREAMS_C90F8` of `fighter.c`. Record §P3.6 (the x87 named limit and its exhaustive check).

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


sub("tools/diff_verify.py", '''         allow_calls=(0x33950,), calls=(REACT, ANIM_BEGIN), eax_mask=0, mutants=("@mutant", "@order")),
]
''', '''         allow_calls=(0x33950,), calls=(REACT, ANIM_BEGIN), eax_mask=0, mutants=("@mutant", "@order")),
]

# 0x47FCC (record §P3.6) reads EBX alone (`mov edx,ebx`; the context 0x33950(side)): the side's dword 0x108370 = 0,
# then the own record on 0xC8950[the own slot's character] at 2.0 and the own slot armed (P2's p2_ctx_case seeds:
# characters 5 and 3, sentinels on +0x0C..+0x1F, +0x41/+0x42, +0x52..+0x57); both sides' dwords 0x108370 seeded.
P3_108370_SEED = {0x108370: le32(0x70707070) + le32(0x74747474)}


# 0x47D24 (the +0x1C callback 0x47FCC stores; 0x193B0, fn(side)): the side's float 0x108378 is 0x2BC30's frame
# (pushed as a dword), then 3.0; the side's byte 0x108394 = 0; the signed word 0xC947E[the other slot's character]
# (`sar 0x10` of the dword 0xC947C + 2c) is 0x3C208's distance, poked negative for character 3 in b2. Both slots'
# bytes +0x42..+0x5F carry different sentinels (the own +0x5F is 0x39834's byte; +0x42 takes bit 2).
def p3_47d24(cid, side, dist=None):
    pokes = {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\\x05", DS_SLOTS + 0x94 + 0x7A: b"\\x03",
             DS_SLOTS + 0x42: bytes(range(0x42, 0x60)), DS_SLOTS + 0x94 + 0x42: bytes(range(0xC2, 0xE0)),
             0x108378: f32(1.5) + f32(2.5), 0x108394: b"\\x94\\x95"}
    if dist is not None:
        pokes[0xC947E + 2 * 3] = le32(dist)[:2]
    return Case(cid, {"eax": side}, pokes)


# 0x47E9C (the +0x0C callback 0x47FCC stores; 0x3531C case 7, EAX unread). Its state byte is the EAX slot's +0x57
# (`mov ecx,eax` at 0x47EA0, `mov al,[ecx+0x57]` at 0x47EC9: E3_SLOT here, `st`), its stores go to ctx[2] (the
# side's slot, +0x57 seeded 0x57). Each frame the side's dword 0x108370 + 1 (signed, above 0x3C sets the byte
# 0x108394); 0: the slot's word +0x88 (`sar 0x10` of the dword +0x86) above 3 sets +0x57 = 1; 1: the own record on
# 0xEDA40 at 2.0, +0x57 = 2, +0x8A = 0; 3: the side's float 0x108378 takes -0.1 on the command's bit 0, else +0.1
# on bit 1, then below 1.1 (the double 0x80C74) becomes 1.1f, above 5.0f becomes 5.0f; 2 and above 3 nothing.
def p3_47e9c(cid, side, st, cnt=0x10, w88=0, cmd=0, fl=2.0):
    own = DS_SLOTS + side * 0x94
    cnts, words, fls = [0x70707070, 0x74747474], [0x5A5A, 0xA5A5], [f32(1.5), f32(2.5)]
    cnts[side], words[side], fls[side] = cnt, cmd, f32(fl)
    return Case(cid, {"eax": E3_SLOT, "edx": E3_OUT, "ebx": side},
                {**SLOT_PTRS, E3_SLOT + 0x57: bytes([st]), own + 0x57: b"\\x57", own + 0x86: b"\\x86\\x86" + le32(w88)[:2],
                 own + 0x8A: b"\\x8a", 0x108370: le32(cnts[0]) + le32(cnts[1]), 0x108378: fls[0] + fls[1],
                 0x108394: b"\\x94\\x95", 0x1088E0: le32(words[0])[:2] + le32(words[1])[:2]})


P3_SPECS += [
    Spec("fighter_47fcc", 0x47FCC, [p2_ctx_case("s0", 0, P3_108370_SEED), p2_ctx_case("s1", 1, P3_108370_SEED)],
         allow_calls=(0x33950,), calls=(HIT_B,), eax_mask=0, mutants=("@mutant", "@order")),
    # 0x47CB0 (the +0x18 hook; 0x19020, fn(side), the whole EAX): flags 1, 8, 4, 0xD, 0xE, 7 = 0 and 5 = 1; the own
    # slot's signed word +0x88 in 1..3 (`jg`/`jge` against immediates) calls 0x18C14(side, the flags, 0xC946A,
    # 0xC9474), else 1. k4: the word -1 (signed: below 1).
    Spec("fighter_47cb0", 0x47CB0, [
        p2_2116c("k0", 0, 4), p2_2116c("k1", 0, 3, stub=0), p2_2116c("k2", 1, 1, stub=0x12345678),
        p2_2116c("k3", 1, 0), p2_2116c("k4", 0, 0xFFFF),
    ], allow_calls=(0x33950, 0x18BD4), calls=(CHECKS,), mutants=("@mutant", "@ge", "@lo")),
    Spec("fighter_47d24", 0x47D24, [p3_47d24("b0", 0), p3_47d24("b1", 1), p3_47d24("b2", 0, 0xF000)],
         allow_calls=(0x33950,), calls=(FLASH, ANIM_BEGIN, HIT_A, PLACE, FACING, POSE, HOLD, TIMER, VOICE),
         eax_mask=0, mutants=("@mutant", "@order", "@signed", "@frame")),
    # e0..eF: the count's bound (0x3B + 1 stays, 0x3C + 1 sets; 0x7FFFFFFF + 1 is negative), each state, the word
    # +0x88 (3, 4, -1), the float's two steps (bit 0 first, then bit 1; 0xFFFC has neither), and both clamps.
    Spec("fighter_47e9c", 0x47E9C, [
        p3_47e9c("e0", 0, 0, cnt=0x3B, w88=3),
        p3_47e9c("e1", 0, 0, cnt=0x3C, w88=4),
        p3_47e9c("e2", 1, 1, cnt=0),
        p3_47e9c("e3", 0, 2),
        p3_47e9c("e4", 0, 4),
        p3_47e9c("e5", 0, 3, cmd=1, fl=2.0),
        p3_47e9c("e6", 1, 3, cmd=2, fl=2.0),
        p3_47e9c("e7", 0, 3, cmd=3, fl=2.0),
        p3_47e9c("e8", 0, 3, cmd=0xFFFC, fl=2.0),
        p3_47e9c("e9", 0, 3, cmd=1, fl=1.15),
        p3_47e9c("eA", 1, 3, cmd=2, fl=4.95),
        p3_47e9c("eB", 0, 3, fl=1.0),
        p3_47e9c("eC", 0, 3, fl=6.0),
        p3_47e9c("eD", 0, 2, cnt=0x7FFFFFFF),
        p3_47e9c("eE", 1, 0, w88=0xFFFF),
        p3_47e9c("eF", 0, 3, cmd=0x0101, fl=3.0),
    ], allow_calls=(0x33950,), calls=(ANIM_BEGIN,), eax_mask=0, mutants=("@mutant", "@slot", "@signed", "@order")),
]
''')
T = "tools/tests/test_diff_verify.py"
sub(T, '''            "fighter_477e8": 0}''',
    '''            "fighter_477e8": 0, "fighter_47fcc": 0, "fighter_47cb0": 0xFFFFFFFF, "fighter_47d24": 0,
            "fighter_47e9c": 0}''')
sub(T, '''            "fighter_477e8@mutant": {"call #0"}, "fighter_477e8@order": {"call #1 memory"}}''',
    '''            "fighter_477e8@mutant": {"call #0"}, "fighter_477e8@order": {"call #1 memory"},
            "fighter_47fcc@mutant": {"call #0"}, "fighter_47fcc@order": {"call #0 memory"},
            "fighter_47cb0@mutant": {"call #0"}, "fighter_47cb0@ge": {"eax", "call #0"},
            "fighter_47cb0@lo": {"eax", "call #0"},
            "fighter_47d24@mutant": {"call #2"}, "fighter_47d24@order": {"call #6 memory"},
            "fighter_47d24@signed": {"call #3"}, "fighter_47d24@frame": {"call #1"},
            "fighter_47e9c@mutant": {"byte"}, "fighter_47e9c@slot": {"byte", "call #0"},
            "fighter_47e9c@signed": {"byte"}, "fighter_47e9c@order": {"call #0 memory"}}''')
sub(T, '''                          ("fighter_47798@eax", ["w1"])):''',
    '''                          ("fighter_47798@eax", ["w1"]),
                          # 0x47CB0's bounds (k1's 3 alone tells `jg`, k2's 1 alone `jge`), 0x47D24's distance
                          # signed (b2 alone), 0x47E9C's count signed (eD alone)
                          ("fighter_47cb0@ge", ["k1"]), ("fighter_47cb0@lo", ["k2"]),
                          ("fighter_47d24@signed", ["b2"]), ("fighter_47e9c@signed", ["eD"])):''')
sub(T, '''        self.assertIn("diff-verify: 68/68 functions VERIFIED; 114/114 mutants detected; 1 named gaps; "
                      "10/54 rows with callees closed (14 have none).", out.getvalue())''',
    '''        self.assertIn("diff-verify: 72/72 functions VERIFIED; 127/127 mutants detected; 1 named gaps; "
                      "11/58 rows with callees closed (14 have none).", out.getvalue())''')
print("t5_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p3_img.bin --function fighter_47fcc | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected (the binding does not exist yet):

```
t5_spec applied
| fighter_47fcc | 0x47FCC | 2 | 1/1 | MISMATCH | 33950 allow unverified, 3C4CC stub unverified |
  fighter_47fcc: s0: port: unknown binding fighter_47fcc
  fighter_47fcc: s1: port: unknown binding fighter_47fcc
diff-verify: 0/1 functions VERIFIED; 0 named gaps; 0/1 rows with callees closed (0 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
```

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


TEST = r'''
/* §P3.6: 0x47FCC and the three callbacks it stores, through their
 * registrations (side 0: ctx[2] = slot 0, ctx[3] = slot 1). The image dwords
 * are evidence lines, not port behaviour. */
static void p3_check_47fcc(void)
{
    p2_cb_fn f;
    p2_hook_fn h;
    p2_side_fn g;
    u8 fl[16];
    u32 k, r, c;
    union { float f; u32 u; } v;
    CHECK(fn_resolve(0x47FCCu) == (void (*)(void))fighter_47fcc, "0x47FCC is registered");
    CHECK(fn_resolve(0x47CB0u) == (void (*)(void))fighter_47cb0, "0x47CB0 is registered");
    CHECK(fn_resolve(0x47D24u) == (void (*)(void))fighter_47d24, "0x47D24 is registered");
    CHECK(fn_resolve(0x47E9Cu) == (void (*)(void))fighter_47e9c, "0x47E9C is registered");
    CHECK_EQ_INT((int)DSD(0x000A41E4u), 0x00047FCC);
    CHECK_EQ_INT((int)DSD(0x00048024u), 0x00047E9C);
    CHECK_EQ_INT((int)DSD(0x0004802Fu), 0x00047CB0);
    CHECK_EQ_INT((int)DSD(0x0004803Au), 0x00047D24);
    /* 0x47E9C starts right after its own jump table 0x47E8C (4 dwords) */
    CHECK_EQ_INT((int)DSD(0x00047E8Cu), 0x00047EE1);
    CHECK_EQ_INT((int)DSD(0x00047E98u), 0x00047F2F);

    /* 0x47FCC on side 0: the side's dword 0x108370 = 0, its record on
     * 0xC8950[slot 0's character] at 2.0, slot 0 7/9/0 armed, +0x57 = 0,
     * +0x41 bit 7. */
    f = (p2_cb_fn)(void *)fn_resolve(0x47FCCu);
    if (f == NULL) return;
    z_fseed();
    c = (u32)DSB(Z_S0 + 0x7Au);
    DSW(DSD(0x000C8950u + c * 4u)) = 0x12B1u;
    DSD(0x00108370u) = 0x70707070u;
    DSB(Z_S0 + 0x41u) = 0x01u;
    DSB(Z_S0 + 0x54u) = 0x44u;
    DSB(Z_S0 + 0x57u) = 0x77u;
    f(0x0A0A0A0Au, 0x0B0B0B0Bu, 0u);
    CHECK_EQ_INT((int)DSD(0x00108370u), 0);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), (int)DSD(0x000C8950u + c * 4u));
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x0Cu), 0x00047E9C);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x18u), 0x00047CB0);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x1Cu), 0x00047D24);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 0);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x41u), 0x81);

    /* 0x47CB0 on side 0: the word +0x88 = 4 returns 1; 2 returns
     * 0x18C14(0, flags 1/4/7/8/0xD/0xE = 0, 5 = 1, the rest 2, 0xC946A,
     * 0xC9474) on the same seeded state. */
    h = (p2_hook_fn)(void *)fn_resolve(0x47CB0u);
    if (h == NULL) return;
    z_fseed();
    DSW(Z_S0 + 0x88u) = 4u;
    CHECK_EQ_INT((int)h(0u), 1);
    z_fseed();
    DSW(Z_S0 + 0x88u) = 2u;
    r = h(0u);
    z_fseed();
    DSW(Z_S0 + 0x88u) = 2u;
    for (k = 0; k < 16u; k++) fl[k] = 2u;
    fl[1] = fl[4] = fl[7] = fl[8] = fl[0xD] = fl[0xE] = 0u;
    fl[5] = 1u;
    CHECK_EQ_INT((int)r, fighter_18c14(0u, fl, 0x000C946Au, 0x000C9474u));

    /* 0x47D24 on side 0: slot 0's +0x42 bit 2 and +0x57 = 3, the side's
     * float 0x108378 (1.5) is the own record's frame on 0xED9D0, then 3.0;
     * the side's byte 0x108394 = 0; the voice 0x66 last. */
    g = (p2_side_fn)(void *)fn_resolve(0x47D24u);
    if (g == NULL) return;
    z_fseed();
    DSW(0x000ED9D0u) = 0x12B1u;
    DSW(DSD(0x000C90F8u + (u32)DSB(Z_S1 + 0x7Au) * 4u)) = 0x12B2u;
    v.f = 1.5f;
    DSD(0x00108378u) = v.u;
    DSB(0x00108394u) = 0x94u;
    DSB(Z_S0 + 0x42u) = 0u;
    DSB(Z_S0 + 0x57u) = 0x77u;
    sound_voice_log_reset();
    g(0u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000ED9D0);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x3FC00000);
    CHECK_EQ_INT((int)DSD(0x00108378u), 0x40400000);
    CHECK_EQ_INT((int)DSB(0x00108394u), 0);
    CHECK_EQ_INT((int)(DSB(Z_S0 + 0x42u) & 4u), 4);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 3);
    CHECK(sound_voice_log_count() >= 1u, "a voice plays");
    CHECK_EQ_INT((int)sound_voice_log_at(sound_voice_log_count() - 1u), 0x66);
    sound_voice_log_reset();

    /* 0x47E9C as 0x3531C case 7 calls it (slot 0, its record, side 0): state
     * 1 starts the record on 0xEDA40 at 2.0, +0x57 = 2, +0x8A = 0, and the
     * count 0x3C + 1 sets the byte 0x108394; state 3 with the command's bit 0
     * takes the float 2.0 to (float)(2.0 - 0.1), and 1.15 to 1.1f. */
    f = (p2_cb_fn)(void *)fn_resolve(0x47E9Cu);
    if (f == NULL) return;
    z_fseed();
    DSW(0x000EDA40u) = 0x12B1u;
    DSB(Z_S0 + 0x57u) = 1u;
    DSB(Z_S0 + 0x8Au) = 0x8Au;
    DSD(0x00108370u) = 0x3Cu;
    DSB(0x00108394u) = 0x94u;
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000EDA40);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x8Au), 0);
    CHECK_EQ_INT((int)DSD(0x00108370u), 0x3D);
    CHECK_EQ_INT((int)DSB(0x00108394u), 1);
    for (k = 0; k < 2u; k++) {
        z_fseed();
        DSB(Z_S0 + 0x57u) = 3u;
        DSW(DS_001088E0) = 1u;
        v.f = k == 0u ? 2.0f : 1.15f;
        DSD(0x00108378u) = v.u;
        f(Z_S0, Z_R0, 0u);
        v.f = (float)(2.0 + -0.1);
        CHECK_EQ_INT((int)DSD(0x00108378u), k == 0u ? (int)v.u : 0x3F8CCCCD);
        CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 3);
    }
}

int test_p3_47fcc(void)         { return u6b_run(p3_check_47fcc); }
'''

ANCHOR = "int test_p3_47874(void)         { return u6b_run(p3_check_47874); }\n"
sub("port/tests/test_fight.c", ANCHOR, ANCHOR + TEST)
sub("port/tests/test.h", "    X(test_p3_47874) \\\n", "    X(test_p3_47874) \\\n    X(test_p3_47fcc) \\\n")
print("t5_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected (line numbers move with `test_fight.c`):

```
t5_test applied
port/tests/test_fight.c:45919:51: error: use of undeclared identifier 'fighter_47fcc'
port/tests/test_fight.c:45920:51: error: use of undeclared identifier 'fighter_47cb0'
port/tests/test_fight.c:45921:51: error: use of undeclared identifier 'fighter_47d24'
port/tests/test_fight.c:45922:51: error: use of undeclared identifier 'fighter_47e9c'
4 errors generated.
```

- [ ] **Step 3: the port, the registrations, the seams, the bindings and the mutants.**

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
FIGHTER_C = r'''
#define P3_108370      0x00108370u  /* 0x47FDD/0x47EAE: a dword per side, 0x47E9C's frame count */
#define P3_108378      0x00108378u  /* 0x47D49/0x47DC3/0x47F4B: a float per side, 0x47D24's frame */
#define P3_108394      0x00108394u  /* 0x47DD0/0x47EC2: a byte per side */
#define P3_BOX_C946A   0x000C946Au  /* 0x47D0B (EBX) */
#define P3_BOX_C9474   0x000C9474u  /* 0x47D06 (ECX) */
#define P3_ANIM_47D24  0x000ED9D0u  /* 0x47D44 */
#define P3_DIST_C947E  0x000C947Eu  /* 0x47D83: [char] the high half of the dword at 0xC947C + 2c */
#define P3_ANIM_47E9C  0x000EDA40u  /* 0x47F04 */
#define P3_D_80C6C     0x00080C6Cu  /* 0x47F51: the double -0.1 */
#define P3_D_80C64     0x00080C64u  /* 0x47F75: the double 0.1 */
#define P3_D_80C74     0x00080C74u  /* 0x47F8F: the double 1.1 */
#define P3_F_80C7C     0x00080C7Cu  /* 0x47FAF: the float 5.0 */

/* 0x47FCC — record §P3.6. Character 2's reaction-0x23 callback (the dword at
 * 0xA41E4). EBX = side (`mov edx,ebx` at 0x47FCF; the EAX slot and EDX record
 * are not read); the context is 0x33950(side). The side's dword 0x108370 = 0;
 * the own record (ctx[4]) on 0xC8950[the own slot's character] at 2.0
 * (0x3C4CC); the own slot (ctx[2]) 7/9/0 with the +0x0C callback 0x47E9C
 * (0x3531C case 7), the +0x18 hook 0x47CB0 (0x19020) and the +0x1C callback
 * 0x47D24 (0x193B0's 0x19505), +0x57 = 0, +0x41 bit 7. PORT: AL = 1
 * (0x4804E) unread (§P2.2). */
void fighter_47fcc(u32 slot, u32 rec, u32 side)
{
    u32 ctx[6];
    (void)slot;
    (void)rec;
    fighter_ctx_same(ctx, side);                            /* 0x47FCF..0x47FD3 0x33950 */
    DSD(P3_108370 + ctx[0] * 4u) = 0u;                      /* 0x47FD8..0x47FDD */
    hit_anim_start_b(ctx[4], DSD(P2_STREAMS_21374 + (u32)DSB(ctx[2] + 0x7Au) * 4u),
                     0x40000000u);                          /* 0x47FE4..0x48000 0x3C4CC */
    DSB(ctx[2] + 0x53u) = 7u;                               /* 0x48009 */
    DSB(ctx[2] + 0x52u) = 9u;                               /* 0x48011 */
    DSB(ctx[2] + 0x54u) = 0u;                               /* 0x48019 */
    DSD(ctx[2] + 0x0Cu) = 0x00047E9Cu;                      /* 0x48021 */
    DSD(ctx[2] + 0x18u) = 0x00047CB0u;                      /* 0x4802C */
    DSD(ctx[2] + 0x1Cu) = 0x00047D24u;                      /* 0x48037 */
    DSB(ctx[2] + 0x57u) = 0u;                               /* 0x48042 */
    DSB(ctx[2] + 0x41u) = (u8)(DSB(ctx[2] + 0x41u) | 0x80u);   /* 0x4804A */
}

/* 0x47CB0 — record §P3.6. The slot +0x18 hook 0x47FCC stores (the dword at
 * 0x4802F; 0x19020, fn(side), the whole EAX tested). The flags 1, 8, 4, 0xD,
 * 0xE and 7 = 0, 5 = 1. With the own slot's signed word +0x88 (`sar 0x10` of
 * the dword +0x86) above 3 or below 1 it returns 1; else 0x18C14(side, the
 * flags, 0xC946A, 0xC9474)'s result. */
u32 fighter_47cb0(u32 side)
{
    u32 ctx[6];
    u8 flags[16];
    s32 w;
    fighter_ctx_same(ctx, side);                            /* 0x47CB6..0x47CBA 0x33950 */
    fighter_18bd4(flags);                                   /* 0x47CBF/0x47CC3 0x18BD4 */
    flags[1] = 0;                                           /* 0x47CCC */
    flags[8] = 0;                                           /* 0x47CD0 */
    flags[4] = 0;                                           /* 0x47CD4 */
    flags[0xD] = 0;                                         /* 0x47CD8 */
    flags[5] = 1u;                                          /* 0x47CCA/0x47CDC */
    flags[0xE] = 0;                                         /* 0x47CE4 */
    flags[7] = 0;                                           /* 0x47CE8 */
    w = (s32)DSD(ctx[2] + 0x86u) >> 16;                     /* 0x47CE0..0x47CF2 */
    if (w > 3 || w < 1)                                     /* 0x47CF5..0x47CFD */
        return 1u;                                          /* 0x47CFF */
    return (u32)fighter_18c14(ctx[0], flags, P3_BOX_C946A, P3_BOX_C9474);   /* 0x47D06..0x47D17 0x18C14 */
}

/* 0x47D24 — record §P3.6. The slot +0x1C callback 0x47FCC stores (the dword at
 * 0x4803A; 0x193B0's 0x19505, fn(side), EAX unread). The context is
 * 0x33950(side). 0x34D8C(side); the own slot's +0x42 bit 2; the own record on
 * 0xED9D0 at the side's float 0x108378 (pushed as a dword, 0x2BC30); the
 * other record on 0xC90F8[the other slot's character] at 3.0 (0x3C480);
 * 0x3C208(side, the signed word 0xC947E[that character]); 0x18AF8;
 * 0x39834(the other side, the own slot's +0x5F); 0x3C358(side); the own
 * slot's +0x57 = 3, the side's float = 3.0 and byte 0x108394 = 0;
 * 0x39A10(each record, 0x309); the voice 0x66. */
void fighter_47d24(u32 side)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);                            /* 0x47D28..0x47D2C 0x33950 */
    hit_flash_pair(ctx[0]);                                 /* 0x47D31/0x47D34 0x34D8C */
    DSB(ctx[2] + 0x42u) = (u8)(DSB(ctx[2] + 0x42u) | 4u);   /* 0x47D39/0x47D3D */
    actors_anim_begin(ctx[4], P3_ANIM_47D24, DSD(P3_108378 + ctx[0] * 4u));   /* 0x47D41..0x47D54 0x2BC30 */
    hit_anim_start_a(ctx[5], DSD(P2_STREAMS_C90F8 + (u32)DSB(ctx[3] + 0x7Au) * 4u),
                     0x40400000u);                          /* 0x47D59..0x47D72 0x3C480 */
    fighter_3c208(ctx[0], (s32)(s16)DSW(P3_DIST_C947E + (u32)DSB(ctx[3] + 0x7Au) * 2u));   /* 0x47D77..0x47D90 */
    fighter_18af8();                                        /* 0x47D95 */
    fighter_39834(ctx[1], (s32)(u32)DSB(ctx[2] + 0x5Fu));   /* 0x47D9A..0x47DAB */
    fighter_3c358(ctx[0]);                                  /* 0x47DB0/0x47DB3 */
    DSB(ctx[2] + 0x57u) = 3u;                               /* 0x47DB8/0x47DBC */
    DSD(P3_108378 + ctx[0] * 4u) = 0x40400000u;             /* 0x47DC0/0x47DC3 */
    DSB(P3_108394 + ctx[0]) = 0u;                           /* 0x47DCE/0x47DD0 */
    fighter_39a10(ctx[4], 0x309u);                          /* 0x47DD6..0x47DDF */
    fighter_39a10(ctx[5], 0x309u);                          /* 0x47DE4..0x47DED */
    (void)sound_voice(0x66u);                               /* 0x47DF2/0x47DF7 0x2C3FC */
}

/* 0x47E9C — record §P3.6. The slot +0x0C callback 0x47FCC stores (the dword at
 * 0x48024; 0x3531C case 7, EAX unread); its bytes start right after its own
 * jump table 0x47E8C (4 dwords). EBX = side, the context 0x33950(side); the
 * state byte is the EAX slot's +0x57 (`mov ecx,eax`, 0x47EC9), the stores go
 * to ctx[2]. Each frame the side's dword 0x108370 + 1, above 0x3C (signed)
 * setting the side's byte 0x108394 = 1. Then by the state: 0 sets the own
 * slot's +0x57 = 1 once its signed word +0x88 exceeds 3; 1 starts the own
 * record on 0xEDA40 at 2.0 (+0x57 = 2, +0x8A = 0); 3 steps the side's float
 * 0x108378 by -0.1 on the command's bit 0, else +0.1 on bit 1, then clamps it
 * to 1.1f below the double 1.1 and to 5.0f above 5.0f; 2 and above 3
 * nothing. PORT: the raw adds in x87 extended precision (0x47F4B..0x47F7D);
 * the port adds in double and rounds once more to the float: for every
 * float of [1.0, 6.0] the two stored floats are equal (record §P3.6, an
 * exhaustive check). */
void fighter_47e9c(u32 slot, u32 rec, u32 side)
{
    u32 ctx[6];
    s32 n;
    (void)rec;
    fighter_ctx_same(ctx, side);                            /* 0x47EA0..0x47EA6 0x33950 */
    n = (s32)(DSD(P3_108370 + ctx[0] * 4u) + 1u);           /* 0x47EAB..0x47EB5 */
    DSD(P3_108370 + ctx[0] * 4u) = (u32)n;                  /* 0x47EB6 */
    if (n > 0x3C) DSB(P3_108394 + ctx[0]) = 1u;             /* 0x47EBD..0x47EC2 */
    switch (DSB(slot + 0x57u)) {                            /* 0x47EC9..0x47ED9 table 0x47E8C */
    case 0u:
        if ((s32)DSD(ctx[2] + 0x86u) >> 16 <= 3) return;    /* 0x47EE1..0x47EF1 */
        DSB(ctx[2] + 0x57u) = 1u;                           /* 0x47EF7/0x47EFB */
        return;
    case 1u:
        actors_anim_begin(ctx[4], P3_ANIM_47E9C, 0x40000000u);  /* 0x47F04..0x47F12 0x2BC30 */
        DSB(ctx[2] + 0x57u) = 2u;                           /* 0x47F17/0x47F1B */
        DSB(ctx[2] + 0x8Au) = 0u;                           /* 0x47F1F/0x47F23 */
        return;
    case 3u: {
        u32 a = P3_108378 + ctx[0] * 4u;
        u32 cmd = (u32)DSW(DS_001088E0 + side * 2u);        /* 0x47F2F/0x47F31, 0x47F59 */
        if ((cmd & 1u) != 0u)                               /* 0x47F38..0x47F49 */
            p2_set_f32(a, (float)((double)p2_f32(a) + p2_f64(P3_D_80C6C)));   /* 0x47F4B/0x47F51, 0x47F7D */
        else if ((cmd & 2u) != 0u)                          /* 0x47F60..0x47F6B */
            p2_set_f32(a, (float)((double)p2_f32(a) + p2_f64(P3_D_80C64)));   /* 0x47F6D..0x47F7D */
        if (!((double)p2_f32(a) >= p2_f64(P3_D_80C74))) {   /* 0x47F83..0x47F98 `jae` */
            DSD(a) = 0x3F8CCCCDu;                           /* 0x47F9A: 1.1f */
            return;
        }
        if (p2_f32(a) > p2_f32(P3_F_80C7C))                 /* 0x47FA9..0x47FB8 `jbe` */
            DSD(a) = 0x40A00000u;                           /* 0x47FBA: 5.0f */
        return;
    }
    default:                                                /* 2 (0x47FC4), above 3 (`ja` 0x47ECE) */
        return;
    }
}
'''

BINDINGS = r'''/* §P3.6: 0x47FCC (0x34E2C at 0x35045) and its +0x0C callback 0x47E9C (0x3531C case 7), mask 0; the +0x18 hook
 * 0x47CB0 (the whole EAX); the +0x1C callback 0x47D24 (mask 0). */
static void b_47fcc(const u32 *r, u32 *eax)            { fighter_47fcc(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_47cb0(const u32 *r, u32 *eax)            { *eax = fighter_47cb0(r[R_EAX]); }
static void b_47d24(const u32 *r, u32 *eax)            { fighter_47d24(r[R_EAX]); *eax = 0u; }
static void b_47e9c(const u32 *r, u32 *eax)            { fighter_47e9c(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_47fcc_at(u32 side, int other_char, int late)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);
    if (!late) DSD(0x00108370u + ctx[0] * 4u) = 0u;
    hit_anim_start_b(ctx[4], DSD(0x000C8950u + (u32)DSB(ctx[other_char ? 3 : 2] + 0x7Au) * 4u), 0x40000000u);
    if (late) DSD(0x00108370u + ctx[0] * 4u) = 0u;
    DSB(ctx[2] + 0x53u) = 7u;
    DSB(ctx[2] + 0x52u) = 9u;
    DSB(ctx[2] + 0x54u) = 0u;
    DSD(ctx[2] + 0x0Cu) = 0x00047E9Cu;
    DSD(ctx[2] + 0x18u) = 0x00047CB0u;
    DSD(ctx[2] + 0x1Cu) = 0x00047D24u;
    DSB(ctx[2] + 0x57u) = 0u;
    DSB(ctx[2] + 0x41u) = (u8)(DSB(ctx[2] + 0x41u) | 0x80u);
}
static void m_47fcc(const u32 *r, u32 *eax)            /* the stream by the other slot's character */
{
    m_47fcc_at(r[R_EBX], 1, 0);
    *eax = 0u;
}
static void m_47fcc_order(const u32 *r, u32 *eax)      /* the dword 0x108370 zeroed after the 0x3C4CC call */
{
    m_47fcc_at(r[R_EBX], 0, 1);
    *eax = 0u;
}
static u32 m_47cb0_at(u32 side, int mode)
{
    u32 ctx[6], k;
    u8 f[16];
    s32 w;
    fighter_ctx_same(ctx, side);
    for (k = 0; k < 16u; k++) f[k] = 2u;
    f[1] = f[8] = f[4] = f[0xE] = f[7] = 0u;
    f[0xD] = mode == 1 ? 2u : 0u;
    f[5] = 1u;
    w = (s32)DSD(ctx[2] + 0x86u) >> 16;
    if (mode == 2 ? (w >= 3 || w < 1) : mode == 3 ? (w > 3 || w <= 1) : (w > 3 || w < 1)) return 1u;
    return (u32)fighter_18c14(ctx[0], f, 0x000C946Au, 0x000C9474u);
}
static void m_47cb0(const u32 *r, u32 *eax)            /* flag 0xD left at 2 */
{
    *eax = m_47cb0_at(r[R_EAX], 1);
}
static void m_47cb0_ge(const u32 *r, u32 *eax)         /* `>= 3` for `jg` */
{
    *eax = m_47cb0_at(r[R_EAX], 2);
}
static void m_47cb0_lo(const u32 *r, u32 *eax)         /* `<= 1` for the `jge` */
{
    *eax = m_47cb0_at(r[R_EAX], 3);
}
static void m_47d24_at(u32 side, int own_char, int early, int zext, int fixed_frame)
{
    u32 ctx[6], w;
    fighter_ctx_same(ctx, side);
    hit_flash_pair(ctx[0]);
    DSB(ctx[2] + 0x42u) = (u8)(DSB(ctx[2] + 0x42u) | 4u);
    actors_anim_begin(ctx[4], 0x000ED9D0u, fixed_frame ? 0x40400000u : DSD(0x00108378u + ctx[0] * 4u));
    hit_anim_start_a(ctx[5], DSD(0x000C90F8u + (u32)DSB(ctx[own_char ? 2 : 3] + 0x7Au) * 4u), 0x40400000u);
    w = DSW(0x000C947Eu + (u32)DSB(ctx[3] + 0x7Au) * 2u);
    fighter_3c208(ctx[0], zext ? (s32)w : (s32)(s16)w);
    fighter_18af8();
    fighter_39834(ctx[1], (s32)(u32)DSB(ctx[2] + 0x5Fu));
    if (early) {
        DSB(ctx[2] + 0x57u) = 3u;
        DSD(0x00108378u + ctx[0] * 4u) = 0x40400000u;
        DSB(0x00108394u + ctx[0]) = 0u;
    }
    fighter_3c358(ctx[0]);
    DSB(ctx[2] + 0x57u) = 3u;
    DSD(0x00108378u + ctx[0] * 4u) = 0x40400000u;
    DSB(0x00108394u + ctx[0]) = 0u;
    fighter_39a10(ctx[4], 0x309u);
    fighter_39a10(ctx[5], 0x309u);
    (void)sound_voice(0x66u);
}
static void m_47d24(const u32 *r, u32 *eax)            /* the other record's stream by the own character */
{
    m_47d24_at(r[R_EAX], 1, 0, 0, 0);
    *eax = 0u;
}
static void m_47d24_order(const u32 *r, u32 *eax)      /* +0x57, the float and the byte before 0x3C358 */
{
    m_47d24_at(r[R_EAX], 0, 1, 0, 0);
    *eax = 0u;
}
static void m_47d24_signed(const u32 *r, u32 *eax)     /* the distance word zero-extended */
{
    m_47d24_at(r[R_EAX], 0, 0, 1, 0);
    *eax = 0u;
}
static void m_47d24_frame(const u32 *r, u32 *eax)      /* the frame 3.0, not the side's float */
{
    m_47d24_at(r[R_EAX], 0, 0, 0, 1);
    *eax = 0u;
}
static void m_47e9c_at(const u32 *r, int up, int by_ctx, int uns, int early)
{
    u32 ctx[6], side = r[R_EBX], n;
    fighter_ctx_same(ctx, side);
    n = DSD(0x00108370u + ctx[0] * 4u) + 1u;
    DSD(0x00108370u + ctx[0] * 4u) = n;
    if (uns ? n > 0x3Cu : (s32)n > 0x3C) DSB(0x00108394u + ctx[0]) = 1u;
    switch (DSB((by_ctx ? ctx[2] : r[R_EAX]) + 0x57u)) {
    case 0u:
        if ((s32)DSD(ctx[2] + 0x86u) >> 16 <= 3) return;
        DSB(ctx[2] + 0x57u) = 1u;
        return;
    case 1u:
        if (early) DSB(ctx[2] + 0x57u) = 2u;
        actors_anim_begin(ctx[4], 0x000EDA40u, 0x40000000u);
        DSB(ctx[2] + 0x57u) = 2u;
        DSB(ctx[2] + 0x8Au) = 0u;
        return;
    case 3u: {
        u32 a = 0x00108378u + ctx[0] * 4u, cmd = (u32)DSW(0x001088E0u + side * 2u);
        union { float f; u32 u; } v;
        double d;
        v.u = DSD(a);
        if ((cmd & 1u) != 0u) {
            memcpy(&d, mem + (up ? 0x00080C64u : 0x00080C6Cu), sizeof d);
            v.f = (float)((double)v.f + d);
            DSD(a) = v.u;
        } else if ((cmd & 2u) != 0u) {
            memcpy(&d, mem + 0x00080C64u, sizeof d);
            v.f = (float)((double)v.f + d);
            DSD(a) = v.u;
        }
        memcpy(&d, mem + 0x00080C74u, sizeof d);
        if (!((double)v.f >= d)) { DSD(a) = 0x3F8CCCCDu; return; }
        if (v.f > 5.0f) DSD(a) = 0x40A00000u;
        return;
    }
    default:
        return;
    }
}
static void m_47e9c(const u32 *r, u32 *eax)            /* +0.1 on the command's bit 0 */
{
    m_47e9c_at(r, 1, 0, 0, 0);
    *eax = 0u;
}
static void m_47e9c_slot(const u32 *r, u32 *eax)       /* the state read from ctx[2], not the EAX slot */
{
    m_47e9c_at(r, 0, 1, 0, 0);
    *eax = 0u;
}
static void m_47e9c_signed(const u32 *r, u32 *eax)     /* the count compared unsigned */
{
    m_47e9c_at(r, 0, 0, 1, 0);
    *eax = 0u;
}
static void m_47e9c_order(const u32 *r, u32 *eax)      /* state 1's +0x57 = 2 before the 0x2BC30 call */
{
    m_47e9c_at(r, 0, 0, 0, 1);
    *eax = 0u;
}

'''

sub(F, '''    DSD(ctx[2] + 0x14u) = 0u;                               /* 0x47821 */
}
''', '''    DSD(ctx[2] + 0x14u) = 0u;                               /* 0x47821 */
}
''' + FIGHTER_C)
sub("port/src/game/fighter.h", "void fighter_477e8(u32 side);\n",
    """void fighter_477e8(u32 side);
/* §P3.6: 0x47FCC and its +0x0C callback (slot, rec, side), +0x18 hook
 * fn(side) and +0x1C callback fn(side). */
void fighter_47fcc(u32 slot, u32 rec, u32 side);
u32  fighter_47cb0(u32 side);
void fighter_47d24(u32 side);
void fighter_47e9c(u32 slot, u32 rec, u32 side);
""")
sub("port/src/game/actors.c", "    fn_register(0x477E8u, (void (*)(void))fighter_477e8);\n",
    """    fn_register(0x477E8u, (void (*)(void))fighter_477e8);
    /* PORT: record 2026-10-03-reverse-p3 §P3.6. Character 2's reaction-0x23
     * callback 0x47FCC (the dword at 0xA41E4) and the three callbacks it
     * stores (+0x0C 0x47E9C, after its own jump table 0x47E8C, 0x3531C case 7,
     * (slot, rec, side); +0x18 0x47CB0, 0x19020, fn(side) with EAX returned;
     * +0x1C 0x47D24, 0x193B0's 0x19505, fn(side)). */
    fn_register(0x47FCCu, (void (*)(void))fighter_47fcc);
    fn_register(0x47CB0u, (void (*)(void))fighter_47cb0);
    fn_register(0x47D24u, (void (*)(void))fighter_47d24);
    fn_register(0x47E9Cu, (void (*)(void))fighter_47e9c);
""")
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_477e8@order",      m_477e8_order,  0x00000000u },\n',
    """    { "fighter_477e8@order",      m_477e8_order,  0x00000000u },
    { "fighter_47fcc",            b_47fcc,        0x00000000u },
    { "fighter_47cb0",            b_47cb0,        0xFFFFFFFFu },
    { "fighter_47d24",            b_47d24,        0x00000000u },
    { "fighter_47e9c",            b_47e9c,        0x00000000u },
    { "fighter_47fcc@mutant",     m_47fcc,        0x00000000u },
    { "fighter_47fcc@order",      m_47fcc_order,  0x00000000u },
    { "fighter_47cb0@mutant",     m_47cb0,        0xFFFFFFFFu },
    { "fighter_47cb0@ge",         m_47cb0_ge,     0xFFFFFFFFu },
    { "fighter_47cb0@lo",         m_47cb0_lo,     0xFFFFFFFFu },
    { "fighter_47d24@mutant",     m_47d24,        0x00000000u },
    { "fighter_47d24@order",      m_47d24_order,  0x00000000u },
    { "fighter_47d24@signed",     m_47d24_signed, 0x00000000u },
    { "fighter_47d24@frame",      m_47d24_frame,  0x00000000u },
    { "fighter_47e9c@mutant",     m_47e9c,        0x00000000u },
    { "fighter_47e9c@slot",       m_47e9c_slot,   0x00000000u },
    { "fighter_47e9c@signed",     m_47e9c_signed, 0x00000000u },
    { "fighter_47e9c@order",      m_47e9c_order,  0x00000000u },
""")
print("t5_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_47fcc fighter_47cb0 fighter_47d24 fighter_47e9c; do
  python3 tools/diff_verify.py --image /tmp/pr_p3_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected: no compiler message, then

```
t5_port applied
all checks passed
| fighter_47fcc | 0x47FCC | 2 | 1/1 | VERIFIED | 33950 allow unverified, 3C4CC stub unverified |
| fighter_47fcc@mutant | 0x47FCC | 2 | 1/1 | MISMATCH | 33950 allow unverified, 3C4CC stub unverified |
| fighter_47fcc@order | 0x47FCC | 2 | 1/1 | MISMATCH | 33950 allow unverified, 3C4CC stub unverified |
| fighter_47cb0 | 0x47CB0 | 5 | 5/5 | VERIFIED | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_47cb0@mutant | 0x47CB0 | 5 | 5/5 | MISMATCH | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_47cb0@ge | 0x47CB0 | 5 | 5/5 | MISMATCH | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_47cb0@lo | 0x47CB0 | 5 | 5/5 | MISMATCH | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_47d24 | 0x47D24 | 3 | 1/1 | VERIFIED | 18AF8 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 34D8C stub unverified, 39834 stub unverified, 39A10 stub unverified, 3C208 stub unverified, 3C358 stub unverified, 3C480 stub unverified |
| fighter_47d24@mutant | 0x47D24 | 3 | 1/1 | MISMATCH | 18AF8 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 34D8C stub unverified, 39834 stub unverified, 39A10 stub unverified, 3C208 stub unverified, 3C358 stub unverified, 3C480 stub unverified |
| fighter_47d24@order | 0x47D24 | 3 | 1/1 | MISMATCH | 18AF8 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 34D8C stub unverified, 39834 stub unverified, 39A10 stub unverified, 3C208 stub unverified, 3C358 stub unverified, 3C480 stub unverified |
| fighter_47d24@signed | 0x47D24 | 3 | 1/1 | MISMATCH | 18AF8 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 34D8C stub unverified, 39834 stub unverified, 39A10 stub unverified, 3C208 stub unverified, 3C358 stub unverified, 3C480 stub unverified |
| fighter_47d24@frame | 0x47D24 | 3 | 1/1 | MISMATCH | 18AF8 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 34D8C stub unverified, 39834 stub unverified, 39A10 stub unverified, 3C208 stub unverified, 3C358 stub unverified, 3C480 stub unverified |
| fighter_47e9c | 0x47E9C | 16 | 17/17 | VERIFIED | 2BC30 stub unverified, 33950 allow unverified |
| fighter_47e9c@mutant | 0x47E9C | 16 | 17/17 | MISMATCH | 2BC30 stub unverified, 33950 allow unverified |
| fighter_47e9c@slot | 0x47E9C | 16 | 17/17 | MISMATCH | 2BC30 stub unverified, 33950 allow unverified |
| fighter_47e9c@signed | 0x47E9C | 16 | 17/17 | MISMATCH | 2BC30 stub unverified, 33950 allow unverified |
| fighter_47e9c@order | 0x47E9C | 16 | 17/17 | MISMATCH | 2BC30 stub unverified, 33950 allow unverified |
```

A row that is not `VERIFIED` is a finding, not something to adjust: stop, compare the C with the bytes (the record's section), and report.

- [ ] **Step 4: the E2 table** (`make entry-triage` first, then regenerate):

```bash
make entry-triage E2_IMAGE=/tmp/pr_p3_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/pr_p3_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
  --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
grep -E '^\| (callbacks|finishers|animation-targets|stubs) ' docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git diff --stat docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -1
```

Expected:

```
entry-triage: FAIL: docs/superpowers/plans/2026-10-01-reverse-e2-triage.md differs from a fresh run (regenerate it with --out)
make: *** [entry-triage] Error 1
entry-triage: targets 289 unported, 206 ported; supplement 131 (13 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 28 in unported code, 87 in ported code, 19 nowhere
| callbacks | 1 | 70 |
| finishers | 0 | 9 |
| animation-targets | 54 | 58 |
| stubs | 54 |
 1 file changed, 6 insertions(+), 6 deletions(-)
```

- [ ] **Step 5: the task gate.**

```bash
make diff-verify entry-triage DIFF_IMAGE=/tmp/pr_p3_d.bin DIFF_TABLE=/tmp/pr_p3_d.md E2_IMAGE=/tmp/pr_p3_e2.bin 2>&1 \
  | grep -E '^(Ran|OK|FAILED|diff-verify:|entry-triage: targets)'
```

Expected (the times vary):

```
Ran 161 tests in N.NNNs
OK
diff-verify: 72/72 functions VERIFIED; 127/127 mutants detected; 1 named gaps; 11/58 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
Ran 44 tests in N.NNNs
OK
entry-triage: targets 289 unported, 206 ported; supplement 131 (13 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 6: every new unit assertion can fail.** Six mutations:

```bash
python3 - <<'PY'
import os
import pathlib
import subprocess


def mutate(path, old, new):
    """Apply one mutation, rebuild, run the suite, print the first FAIL lines, restore the file."""
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



mutate("port/src/game/actors.c", "    fn_register(0x47E9Cu, (void (*)(void))fighter_47e9c);\n", "")
mutate("port/src/game/fighter.c",
       "    DSD(P3_108370 + ctx[0] * 4u) = 0u;                      /* 0x47FD8..0x47FDD */",
       "    DSD(P3_108370 + ctx[1] * 4u) = 0u;                      /* 0x47FD8..0x47FDD */")
mutate("port/src/game/fighter.c",
       "    flags[5] = 1u;                                          /* 0x47CCA/0x47CDC */",
       "    flags[5] = 0u;                                          /* 0x47CCA/0x47CDC */")
mutate("port/src/game/fighter.c",
       "    DSB(P3_108394 + ctx[0]) = 0u;                           /* 0x47DCE/0x47DD0 */",
       "    DSB(P3_108394 + ctx[1]) = 0u;                           /* 0x47DCE/0x47DD0 */")
mutate("port/src/game/fighter.c",
       "            DSD(a) = 0x3F8CCCCDu;                           /* 0x47F9A: 1.1f */",
       "            DSD(a) = 0x3F800000u;                           /* 0x47F9A: 1.1f */")
mutate("port/src/game/fighter.c",
       "    if (n > 0x3C) DSB(P3_108394 + ctx[0]) = 1u;             /* 0x47EBD..0x47EC2 */",
       "    if (n > 0x3D) DSB(P3_108394 + ctx[0]) = 1u;             /* 0x47EBD..0x47EC2 */")
PY
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
```

Expected:

```
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:45922: 0x47E9C is registered']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45944: 1886417008 != 0']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45972: 1 != 0']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45992: 148 != 0']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46025: 1065353216 != 1066192077']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46016: 148 != 1']
all checks passed
```

- [ ] **Step 7: commit.**

```bash
git add tools/diff_verify.py tools/tests/test_diff_verify.py port/tests/test_fight.c port/tests/test.h \
  port/src/game/fighter.c port/src/game/fighter.h port/src/game/actors.c port/tests/diff_runner.c \
  docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git commit -m "fighter: port 0x47FCC and its callbacks 0x47CB0 0x47D24 0x47E9C; E2 table regenerated (track P batch 3)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: `0x48608`, its callbacks `0x48054 0x480B4`, `0x480B4`'s callee `0x48170` and the +0x10 handler `0x4811C` (seams `0x188DC 0x36D98 0x3C148 0x468D8 0x48170`)

**Files:** as Task 3.

**Interfaces:** produces `void fighter_48608(u32 slot, u32 rec, u32 side)`, `u32 fighter_48054(u32 side)`, `void fighter_480b4(u32 side)`, `void fighter_48170(u32 side)` (seamed: `0x480B4`'s row stubs it), `void fighter_4811c(u32 slot, u32 rec, u32 side)` and its case-10 adapter `void fighter_4811c_case10(u32 slot, u32 side)` (the registered function: `fighter_state_3531c` case 10 passes (slot, side)); `fighter_36d98(u32 slot)` exported; the seams of `0x188DC 0x36D98 0x3C148 0x468D8 0x48170`; `ARM170`, `CLEAR34`, `PRED`, `RESET`, `ANCHORX`, `p3_48608`, `p3_48054`, `p3_480b4`, `p3_48170`, `p3_4811c`; `test_p3_48608`. Consumes Task 4's `SPEED`, `P3_SLOT_CBS`, `fighter_3c190`; Task 3's `DISPATCH`; P2's `ANIM54`. Record §P3.7.

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


sub("tools/diff_verify.py", '''    ], allow_calls=(0x33950,), calls=(ANIM_BEGIN,), eax_mask=0, mutants=("@mutant", "@slot", "@signed", "@order")),
]
''', '''    ], allow_calls=(0x33950,), calls=(ANIM_BEGIN,), eax_mask=0, mutants=("@mutant", "@slot", "@signed", "@order")),
]

# The callees the 0x48608 family stubs (record §P3.7), args from their bytes, clobbers from E.callee_clobbers:
# 0x48170 EAX = side (it saves every register it writes); 0x3C148 EAX = side; 0x468D8 EAX = side (it returns
# AL); 0x36D98 EAX = slot; 0x188DC EAX = side, EDX = x (clobbers EDX). All plain `ret`.
ARM170 = E.Call(0x48170, ("eax",))
CLEAR34 = E.Call(0x3C148, ("eax",))
PRED = E.Call(0x468D8, ("eax",))
RESET = E.Call(0x36D98, ("eax",))
ANCHORX = E.Call(0x188DC, ("eax", "edx"), clobbers=("edx",))


# 0x48608 (record §P3.7): the EDX record on 0xED834 at 2.0, 0x3C190(rec+0x51, 0x78), the record's +0x42 = 0x1E, the
# EAX slot 9/7/0 with +0x57 = 0 and three callbacks (+0x0C 0x4844C, +0x18 0x48054, +0x1C 0x480B4), and the word
# 0x10838C[rec+0x51] = 0 (EBX = rec+0x51, loaded before both calls, which keep it). EBX at entry is not read
# (1 - rec+0x51 in every case).
def p3_48608(cid, r51):
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": 1 - r51},
                {**P3_SLOT_CBS, E3_SLOT + 0x55: b"\\x55\\x56\\x57", E3_REC + 0x42: b"\\x42", E3_REC + 0x51: bytes([r51]),
                 0x10838C: b"\\x8c\\x8c\\x8e\\x8e"})


# 0x48054 (the +0x18 hook; 0x19020, fn(side), the whole EAX): flags 1, 8, 4, 0xD = 0, 5 and 9 = 1; 0x18C14(side,
# the flags, EBX = 0xC9492, ECX = 0xC949C: both loaded before 0x33950/0x18BD4, which keep them); with the own slot's
# +0x57 non-zero the result is replaced by 1.
def p3_48054(cid, side, st, stub):
    return Case(cid, {"eax": side}, {**SLOT_PTRS, DS_SLOTS + side * 0x94 + 0x57: bytes([st])}, {0x18C14: stub})


# 0x480B4 (the +0x1C callback; 0x193B0, fn(side)): 0x3B298(the other side, the own +0x5F) (its AL unread),
# 0x39A10(each record, 0x309), 0x48170(side), 0x3C208(the other side, the signed word 0xC94A6[the other slot's
# character], `sar 0x10` of the dword 0xC94A4 + 2c), poked negative for character 3 in x2.
def p3_480b4(cid, side, al, dist=None):
    pokes = {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\\x05", DS_SLOTS + 0x94 + 0x7A: b"\\x03",
             DS_SLOTS + 0x52: bytes(range(0x52, 0x60)), DS_SLOTS + 0x94 + 0x52: bytes(range(0xD2, 0xE0))}
    if dist is not None:
        pokes[0xC94A6 + 2 * 3] = le32(dist)[:2]
    return Case(cid, {"eax": side}, pokes, {0x3B298: al})


# 0x48170 (called by 0x480B4 at 0x480F6, EAX = side; 0x480FB reloads EAX): the side's word 0x108388 = 0, 0x3C148 on
# both sides, the own record on 0xED850 at 3.0 (0x3C480), the own slot's +0x57 = 2, 0x468D8(the other side) and on
# its AL 0x36D98(the other slot), the other record on 0xC90F8[its character] at 3.0, 0x188DC(the other side, the
# other slot's +0x2C read before that call), then the other slot 0x10/0xA/0 with the +0x10 handler 0x4811C (0x3531C
# case 10), +0x58 = 0, and the side's byte 0x108392 = (the other slot's +0x43 & 0x30) != 0. Both slots' +0x10..+0x13,
# +0x2C and +0x43 differ, the per-side words and bytes carry sentinels.
def p3_48170(cid, side, al, o43):
    oth = DS_SLOTS + (1 - side) * 0x94
    return Case(cid, {"eax": side},
                {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\\x05", DS_SLOTS + 0x94 + 0x7A: b"\\x03",
                 DS_SLOTS + 0x10: le32(0x10101010), DS_SLOTS + 0x94 + 0x10: le32(0x90909090),
                 DS_SLOTS + 0x2C: le32(0x2C2C2C2C), DS_SLOTS + 0x94 + 0x2C: le32(0xACACACAC),
                 DS_SLOTS + 0x43: b"\\x43", DS_SLOTS + 0x94 + 0x43: b"\\xc3", oth + 0x43: bytes([o43]),
                 DS_SLOTS + 0x52: bytes(range(0x52, 0x59)), DS_SLOTS + 0x94 + 0x52: bytes(range(0xD2, 0xD9)),
                 0x108388: b"\\x88\\x88\\x8a\\x8a", 0x108392: b"\\x92\\x93"}, {0x468D8: al})


# 0x4811C (the +0x10 handler 0x48170 stores; 0x3531C case 10 at 0x354E2: EAX = slot, EDX = the slot's record loaded
# at 0x35396, EBX = side; the case's `ret` leaves EAX unread): by the slot's +0x58: 0 nothing; 1 the word
# 0x108380[side] = 0 and +0x58 = 2; 2 the word + 1 and, above 0xF (signed: `sar 0x10` of the dword 0x10837E + 2 *
# side), +0x54 = 0 and 0x36870(rec); above 2 nothing. i5: the word 0x7FFF + 1 is negative.
def p3_4811c(cid, side, st, w):
    words = [0x8080, 0x8282]
    words[side] = w
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": side},
                {E3_SLOT + 0x54: b"\\x54", E3_SLOT + 0x58: bytes([st]),
                 0x108380: le32(words[0])[:2] + le32(words[1])[:2]})


P3_SPECS += [
    Spec("fighter_48608", 0x48608, [p3_48608("r0", 0), p3_48608("r1", 1)],
         calls=(HIT_B, SPEED), eax_mask=0, mutants=("@mutant", "@order", "@side")),
    Spec("fighter_48054", 0x48054, [p3_48054("n0", 0, 0, 0), p3_48054("n1", 1, 0, 0x12345678),
                                    p3_48054("n2", 0, 3, 0)],
         allow_calls=(0x33950, 0x18BD4), calls=(CHECKS,), mutants=("@mutant", "@eax")),
    Spec("fighter_480b4", 0x480B4, [p3_480b4("x0", 0, 0), p3_480b4("x1", 1, 1), p3_480b4("x2", 0, 0, 0xF000)],
         allow_calls=(0x33950,), calls=(DISPATCH, TIMER, ARM170, PLACE), eax_mask=0,
         mutants=("@mutant", "@signed", "@char")),
    Spec("fighter_48170", 0x48170, [p3_48170("g0", 0, 0, 0x10), p3_48170("g1", 1, 1, 0x20), p3_48170("g2", 0, 0, 0xCF)],
         allow_calls=(0x33950,), calls=(CLEAR34, HIT_A, PRED, RESET, ANCHORX), eax_mask=0,
         mutants=("@mutant", "@order", "@reset")),
    Spec("fighter_4811c", 0x4811C, [
        p3_4811c("i0", 0, 0, 0x0F), p3_4811c("i1", 1, 1, 0x0F), p3_4811c("i2", 0, 2, 0x0F), p3_4811c("i3", 1, 2, 0x0E),
        p3_4811c("i4", 0, 3, 0x0F), p3_4811c("i5", 0, 2, 0x7FFF),
    ], calls=(ANIM54,), eax_mask=0, mutants=("@mutant", "@signed", "@order")),
]
''')
T = "tools/tests/test_diff_verify.py"
sub(T, '''            "fighter_47e9c": 0}''',
    '''            "fighter_47e9c": 0, "fighter_48608": 0, "fighter_48054": 0xFFFFFFFF, "fighter_480b4": 0,
            "fighter_48170": 0, "fighter_4811c": 0}''')
sub(T, '''            "fighter_47e9c@signed": {"byte"}, "fighter_47e9c@order": {"call #0 memory"}}''',
    '''            "fighter_47e9c@signed": {"byte"}, "fighter_47e9c@order": {"call #0 memory"},
            "fighter_48608@mutant": {"call #1"}, "fighter_48608@order": {"call #1 memory"},
            "fighter_48608@side": {"byte"},
            "fighter_48054@mutant": {"call #0"}, "fighter_48054@eax": {"eax"},
            "fighter_480b4@mutant": {"call #4"}, "fighter_480b4@signed": {"call #4"},
            "fighter_480b4@char": {"call #4"},
            "fighter_48170@mutant": {"call #5", "call #6"},
            "fighter_48170@order": {"call #3 memory", "call #4 memory"},
            "fighter_48170@reset": {"call #4"},
            "fighter_4811c@mutant": {"call #0"}, "fighter_4811c@signed": {"byte", "call #0"},
            "fighter_4811c@order": {"call #0 memory"}}''')
sub(T, '''                          ("fighter_47d24@signed", ["b2"]), ("fighter_47e9c@signed", ["eD"])):''',
    '''                          ("fighter_47d24@signed", ["b2"]), ("fighter_47e9c@signed", ["eD"]),
                          # 0x48054's 1 when +0x57 is set (n2 alone), 0x480B4's distance signed (x2 alone),
                          # 0x48170's 0x36D98 on the other slot (g1, the case whose 0x468D8 AL is set), 0x4811C's
                          # word signed (i5 alone)
                          ("fighter_48054@eax", ["n2"]), ("fighter_480b4@signed", ["x2"]),
                          ("fighter_48170@reset", ["g1"]), ("fighter_4811c@signed", ["i5"])):''')
sub(T, '''                                 0x3C190: ("edx",), 0x3B714: ("edx",)})''',
    '''                                 0x3C190: ("edx",), 0x3B714: ("edx",), 0x48170: (), 0x3C148: (), 0x468D8: (),
                                 0x36D98: (), 0x188DC: ("edx",)})''')
sub(T, '''        self.assertIn("diff-verify: 72/72 functions VERIFIED; 127/127 mutants detected; 1 named gaps; "
                      "11/58 rows with callees closed (14 have none).", out.getvalue())''',
    '''        self.assertIn("diff-verify: 77/77 functions VERIFIED; 141/141 mutants detected; 1 named gaps; "
                      "11/63 rows with callees closed (14 have none).", out.getvalue())''')
print("t6_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p3_img.bin --function fighter_48608 | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected (the binding does not exist yet):

```
t6_spec applied
| fighter_48608 | 0x48608 | 2 | 1/1 | MISMATCH | 3C190 stub unverified, 3C4CC stub unverified |
  fighter_48608: r0: port: unknown binding fighter_48608
  fighter_48608: r1: port: unknown binding fighter_48608
diff-verify: 0/1 functions VERIFIED; 0 named gaps; 0/1 rows with callees closed (0 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
```

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


TEST = r'''
/* §P3.7: 0x48608, its +0x18 hook and +0x1C callback, the latter's callee
 * 0x48170 and the +0x10 handler 0x4811C that one stores, through their
 * registrations (side 0: ctx[2] = slot 0, ctx[3] = slot 1). The image dwords
 * are evidence lines, not port behaviour. */
static void p3_check_48608(void)
{
    p2_cb_fn f;
    p2_hook_fn h;
    p2_side_fn g;
    void (*c10)(u32 slot, u32 side);
    u8 fl[16];
    u32 k, r;
    CHECK(fn_resolve(0x48608u) == (void (*)(void))fighter_48608, "0x48608 is registered");
    CHECK(fn_resolve(0x48054u) == (void (*)(void))fighter_48054, "0x48054 is registered");
    CHECK(fn_resolve(0x480B4u) == (void (*)(void))fighter_480b4, "0x480B4 is registered");
    CHECK(fn_resolve(0x4811Cu) == (void (*)(void))fighter_4811c_case10, "0x4811C is registered");
    CHECK_EQ_INT((int)DSD(0x000A4234u), 0x00048608);
    CHECK_EQ_INT((int)DSD(0x00048649u), 0x0004844C);
    CHECK_EQ_INT((int)DSD(0x00048658u), 0x00048054);
    CHECK_EQ_INT((int)DSD(0x00048661u), 0x000480B4);
    CHECK_EQ_INT((int)DSD(0x00048234u), 0x0004811C);
    CHECK_EQ_INT((int)(DSD(0x000480F7u) + 0x480FBu), 0x00048170);   /* 0x480F6's rel32 */

    /* 0x48608: the record on 0xED834 at 2.0, 0x3C190(0, 0x78) (side 0's
     * actor bit 15 clear: word +0x34 = -0x78), +0x42 = 0x1E, the slot 9/7/0,
     * +0x57 = 0 and three callbacks, the side's word 0x10838C = 0. */
    f = (p2_cb_fn)(void *)fn_resolve(0x48608u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    p3_set_bit15(0u, 0);
    DSW(0x000ED834u) = 0x12B1u;
    DSB(Z_R0 + 0x42u) = 0x42u;
    DSW(Z_R0 + 0x34u) = 0x3434u;
    DSB(Z_S0 + 0x54u) = 0x44u;
    DSB(Z_S0 + 0x57u) = 0x77u;
    DSD(Z_S0 + 0x0Cu) = 0x0C0C0C0Cu;
    DSD(Z_S0 + 0x18u) = 0x18181818u;
    DSD(Z_S0 + 0x1Cu) = 0x1C1C1C1Cu;
    DSW(0x0010838Cu) = 0x8C8Cu;
    DSW(0x0010838Eu) = 0x8E8Eu;
    f(Z_S0, Z_R0, 1u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000ED834);
    CHECK_EQ_INT((int)DSW(Z_R0 + 0x34u), 0xFF88);
    CHECK_EQ_INT((int)DSB(Z_R0 + 0x42u), 0x1E);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 0);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x0Cu), 0x0004844C);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x18u), 0x00048054);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x1Cu), 0x000480B4);
    CHECK_EQ_INT((int)DSW(0x0010838Cu), 0);
    CHECK_EQ_INT((int)DSW(0x0010838Eu), 0x8E8E);

    /* 0x48054 on side 0 (the slot's word +0x88 = 2): with +0x57 = 0 it
     * returns 0x18C14(0, flags 1/4/8/0xD = 0, 5/9 = 1, the rest 2, 0xC9492,
     * 0xC949C) on the same seeded state, here 0; with +0x57 = 3, 1. */
    h = (p2_hook_fn)(void *)fn_resolve(0x48054u);
    if (h == NULL) return;
    z_fseed();
    DSB(Z_S0 + 0x57u) = 0u;
    DSW(Z_S0 + 0x88u) = 2u;
    r = h(0u);
    z_fseed();
    DSB(Z_S0 + 0x57u) = 0u;
    DSW(Z_S0 + 0x88u) = 2u;
    for (k = 0; k < 16u; k++) fl[k] = 2u;
    fl[1] = fl[4] = fl[8] = fl[0xD] = 0u;
    fl[5] = fl[9] = 1u;
    CHECK_EQ_INT((int)r, fighter_18c14(0u, fl, 0x000C9492u, 0x000C949Cu));
    CHECK_EQ_INT((int)r, 0);
    z_fseed();
    DSB(Z_S0 + 0x57u) = 3u;
    DSW(Z_S0 + 0x88u) = 2u;
    CHECK_EQ_INT((int)h(0u), 1);

    /* 0x480B4 on side 0, through 0x48170(0): slot 1 0x10/0xA/0 with the +0x10
     * handler 0x4811C and +0x58 = 0, slot 0's +0x57 = 2, the side's word
     * 0x108388 = 0 and byte 0x108392 = 0 (0x3B298(1, ...) has cleared slot
     * 1's +0x43 by then: 0x48170 alone is checked below). */
    g = (p2_side_fn)(void *)fn_resolve(0x480B4u);
    if (g == NULL) return;
    z_fseed();
    DSW(0x000ED850u) = 0x12B1u;
    DSW(DSD(0x000C90F8u + (u32)DSB(Z_S1 + 0x7Au) * 4u)) = 0x12B2u;
    DSB(Z_S0 + 0x57u) = 0x77u;
    DSB(Z_S1 + 0x43u) = 0x10u;
    DSB(Z_S1 + 0x58u) = 0x58u;
    DSD(Z_S1 + 0x10u) = 0x10101010u;
    DSW(0x00108388u) = 0x8888u;
    DSB(0x00108392u) = 0x92u;
    DSB(0x00108393u) = 0x93u;
    g(0u);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x52u), 0x10);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x10u), 0x0004811C);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x58u), 0);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSW(0x00108388u), 0);
    CHECK_EQ_INT((int)DSB(0x00108392u), 0);
    CHECK_EQ_INT((int)DSB(0x00108393u), 0x93);
    /* 0x48170(0) alone: the side's byte 0x108392 is slot 1's +0x43 & 0x30
     * tested (0x10: 1; 0xCF: 0). */
    for (k = 0; k < 2u; k++) {
        z_fseed();
        DSW(0x000ED850u) = 0x12B1u;
        DSW(DSD(0x000C90F8u + (u32)DSB(Z_S1 + 0x7Au) * 4u)) = 0x12B2u;
        DSB(Z_S1 + 0x43u) = k == 0u ? 0x10u : 0xCFu;
        DSB(0x00108392u) = 0x92u;
        fighter_48170(0u);
        CHECK_EQ_INT((int)DSB(0x00108392u), k == 0u ? 1 : 0);
    }

    /* 0x4811C as 0x3531C case 10 calls it (slot 1, side 1): +0x58 = 1 zeroes
     * the side's word 0x108380 and steps to 2; in 2 the word 0xF + 1 ends the
     * hold (+0x54 = 0), 0xE + 1 does not. */
    c10 = (void (*)(u32, u32))(void *)fn_resolve(0x4811Cu);
    if (c10 == NULL) return;
    z_fseed();
    DSB(Z_S1 + 0x58u) = 1u;
    DSW(0x00108382u) = 0x8282u;
    c10(Z_S1, 1u);
    CHECK_EQ_INT((int)DSW(0x00108382u), 0);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x58u), 2);
    for (k = 0; k < 2u; k++) {
        z_fseed();
        DSB(Z_S1 + 0x58u) = 2u;
        DSB(Z_S1 + 0x54u) = 0x44u;
        DSW(0x00108382u) = k == 0u ? 0x0Fu : 0x0Eu;
        c10(Z_S1, 1u);
        CHECK_EQ_INT((int)DSW(0x00108382u), k == 0u ? 0x10 : 0x0F);
        CHECK_EQ_INT((int)DSB(Z_S1 + 0x54u), k == 0u ? 0 : 0x44);
    }
}

int test_p3_48608(void)         { return u6b_run(p3_check_48608); }
'''

ANCHOR = "int test_p3_47fcc(void)         { return u6b_run(p3_check_47fcc); }\n"
sub("port/tests/test_fight.c", ANCHOR, ANCHOR + TEST)
sub("port/tests/test.h", "    X(test_p3_47fcc) \\\n", "    X(test_p3_47fcc) \\\n    X(test_p3_48608) \\\n")
print("t6_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected (line numbers move with `test_fight.c`):

```
t6_test applied
port/tests/test_fight.c:46044:51: error: use of undeclared identifier 'fighter_48608'
port/tests/test_fight.c:46045:51: error: use of undeclared identifier 'fighter_48054'
port/tests/test_fight.c:46046:51: error: use of undeclared identifier 'fighter_480b4'
port/tests/test_fight.c:46047:51: error: use of undeclared identifier 'fighter_4811c_case10'
port/tests/test_fight.c:46143:9: error: call to undeclared function 'fighter_48170'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]
5 errors generated.
```

- [ ] **Step 3: the port, the registrations, the seams, the bindings and the mutants.**

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
# five callees get their seams; the harness's mutants call 0x36D98, so it loses `static` (declared in fighter.h)
sub(F, '''void hit_anchor_x(u32 side, u32 x)
{
''', '''void hit_anchor_x(u32 side, u32 x)
{
    PR_SEAM(0x188DCu, side, x);
''')
sub(F, '''static void fighter_36d98(u32 slot)
{
''', '''void fighter_36d98(u32 slot)
{
    PR_SEAM(0x36D98u, slot);
''')
sub(F, '''void fighter_3c148(u32 side)
{
''', '''void fighter_3c148(u32 side)
{
    PR_SEAM(0x3C148u, side);
''')
sub(F, '''int ai_pred_468d8(u32 side)
{
''', '''int ai_pred_468d8(u32 side)
{
    PR_SEAM_RET(0x468D8u, side);
''')

FIGHTER_C = r'''
#define P3_ANIM_48608  0x000ED834u  /* 0x48615 */
#define P3_10838C      0x0010838Cu  /* 0x4864D/0x4845E: a word per side, 0x4844C's frame count */
#define P3_BOX_C9492   0x000C9492u  /* 0x4806C (EBX) */
#define P3_BOX_C949C   0x000C949Cu  /* 0x4805E (ECX) */
#define P3_DIST_C94A6  0x000C94A6u  /* 0x48104: [char] the high half of the dword at 0xC94A4 + 2c */
#define P3_108388      0x00108388u  /* 0x4818E/0x48465: a word per side, 0x4844C's voice count */
#define P3_ANIM_48170  0x000ED850u  /* 0x481D2 */
#define P3_108392      0x00108392u  /* 0x48242: a byte per side */
#define P3_108380      0x00108380u  /* 0x4813D/0x4814B: a word per side, 0x4811C's count */

/* 0x48608 — record §P3.7. Character 2's reaction-0x27 callback (the dword at
 * 0xA4234). EBX = the EDX record's +0x51 (zero-extended, 0x4860E/0x48612;
 * the entry EBX is not read). The record on 0xED834 at 2.0 (0x3C4CC);
 * 0x3C190(rec+0x51, 0x78); the record's +0x42 = 0x1E; the EAX slot 9/7/0
 * with +0x57 = 0, the +0x0C callback 0x4844C (0x3531C case 7), the +0x18
 * hook 0x48054 (0x19020) and the +0x1C callback 0x480B4 (0x193B0's 0x19505);
 * the word 0x10838C[rec+0x51] = 0. PORT: AL = 1 (0x4865C) unread (§P2.2). */
void fighter_48608(u32 slot, u32 rec, u32 side)
{
    u32 s = (u32)DSB(rec + 0x51u);                          /* 0x4860E/0x48612 */
    (void)side;
    hit_anim_start_b(rec, P3_ANIM_48608, 0x40000000u);      /* 0x48610..0x4861F 0x3C4CC */
    fighter_3c190(s, 0x78u);                                /* 0x48624..0x4862B 0x3C190 */
    DSB(rec + 0x42u) = 0x1Eu;                               /* 0x48630 */
    DSB(slot + 0x52u) = 9u;                                 /* 0x48634 */
    DSB(slot + 0x53u) = 7u;                                 /* 0x48638 */
    DSB(slot + 0x54u) = 0u;                                 /* 0x4863C */
    DSB(slot + 0x57u) = 0u;                                 /* 0x48640 */
    DSD(slot + 0x0Cu) = 0x0004844Cu;                        /* 0x48646 */
    DSW(P3_10838C + s * 2u) = 0u;                           /* 0x48644/0x4864D */
    DSD(slot + 0x18u) = 0x00048054u;                        /* 0x48655 */
    DSD(slot + 0x1Cu) = 0x000480B4u;                        /* 0x4865E */
}

/* 0x48054 — record §P3.7. The slot +0x18 hook 0x48608 stores (the dword at
 * 0x48658; 0x19020, fn(side), the whole EAX tested). The flags 1, 8, 4 and 0xD
 * = 0, 5 and 9 = 1; 0x18C14(side, the flags, 0xC9492, 0xC949C) (EBX/ECX
 * loaded before 0x18BD4/0x33950, which keep them); with the own slot's +0x57
 * non-zero it returns 1, else 0x18C14's result. */
u32 fighter_48054(u32 side)
{
    u32 ctx[6];
    u8 flags[16];
    u32 r;
    fighter_ctx_same(ctx, side);                            /* 0x4805A..0x48063 0x33950 */
    fighter_18bd4(flags);                                   /* 0x48068..0x48071 0x18BD4 */
    flags[1] = 0;                                           /* 0x4807A */
    flags[8] = 0;                                           /* 0x4807E */
    flags[4] = 0;                                           /* 0x48082 */
    flags[0xD] = 0;                                         /* 0x48086 */
    flags[5] = 1u;                                          /* 0x48078/0x4808E */
    flags[9] = 1u;                                          /* 0x48092 */
    r = (u32)fighter_18c14(ctx[0], flags, P3_BOX_C9492, P3_BOX_C949C);   /* 0x4805E/0x4806C, 0x48096/0x48099 0x18C14 */
    if (DSB(ctx[2] + 0x57u) != 0u) r = 1u;                  /* 0x4809E..0x480A8 */
    return r;
}

/* 0x48170 — record §P3.7. Called by 0x480B4 (0x480F6) with EAX = side (it
 * saves every register it writes; 0x480FB reloads EAX). The context is
 * 0x33950(side); the own slot is EDI, the other EBX (0x481A4..0x481D7). The
 * side's word 0x108388 = 0; 0x3C148(side), 0x3C148(the other side); the own
 * record on 0xED850 at 3.0 (0x3C480); the own slot's +0x57 = 2;
 * 0x468D8(the other side) and, on its AL, 0x36D98(the other slot); the other
 * record on 0xC90F8[the other slot's character] at 3.0 (0x3C480);
 * 0x188DC(the other side, the other slot's +0x2C, read before that call);
 * the other slot 0x10/0xA/0 with the +0x10 handler 0x4811C (0x3531C case 10)
 * and +0x58 = 0; the side's byte 0x108392 = (the other slot's +0x43 & 0x30)
 * != 0. */
void fighter_48170(u32 side)
{
    u32 ctx[6], x, other, own_s, oth_s;
    PR_SEAM(0x48170u, side);
    fighter_ctx_same(ctx, side);                            /* 0x48178..0x48183 0x33950 */
    other = 1u - side;                                      /* 0x4817E/0x48188 */
    own_s = DS_001077B0 + side * 0x94u;                     /* 0x481A4..0x481BA (EDI) */
    oth_s = DS_001077B0 + other * 0x94u;                    /* 0x481BC..0x481D7 (EBX) */
    DSW(P3_108388 + side * 2u) = 0u;                        /* 0x4818A..0x4818E */
    fighter_3c148(side);                                    /* 0x4818C/0x48196 */
    fighter_3c148(other);                                   /* 0x4819B/0x4819F */
    hit_anim_start_a(DSD(own_s), P3_ANIM_48170, 0x40400000u);   /* 0x481D2..0x481E0 0x3C480 */
    DSB(own_s + 0x57u) = 2u;                                /* 0x481E7 */
    if ((u8)ai_pred_468d8(other) != 0u)                     /* 0x481E5/0x481EB..0x481F2 0x468D8 */
        fighter_36d98(oth_s);                               /* 0x481F4/0x481F6 */
    x = DSD(ctx[3] + 0x2Cu);                                /* 0x48205/0x48212 */
    hit_anim_start_a(DSD(oth_s), DSD(P2_STREAMS_C90F8 + (u32)DSB(oth_s + 0x7Au) * 4u),
                     0x40400000u);                          /* 0x481FB..0x48215 0x3C480 */
    hit_anchor_x(ctx[1], x);                                /* 0x4821A..0x48220 0x188DC */
    DSB(oth_s + 0x52u) = 0x10u;                             /* 0x48225 */
    DSB(oth_s + 0x53u) = 0x0Au;                             /* 0x48229 */
    DSB(oth_s + 0x54u) = 0u;                                /* 0x4822D */
    DSD(oth_s + 0x10u) = 0x0004811Cu;                       /* 0x48231 */
    DSB(oth_s + 0x58u) = 0u;                                /* 0x4823B */
    DSB(P3_108392 + side) = (DSB(oth_s + 0x43u) & 0x30u) != 0u ? 1u : 0u;   /* 0x48238..0x48242 */
}

/* 0x480B4 — record §P3.7. The slot +0x1C callback 0x48608 stores (the dword at
 * 0x48661; 0x193B0's 0x19505, fn(side), EAX unread). The context is
 * 0x33950(side). 0x3B298(the other side, the own slot's +0x5F) (its AL
 * unread); 0x39A10(each record, 0x309); 0x48170(side); 0x3C208(the other
 * side, the signed word 0xC94A6[the other slot's character]). */
void fighter_480b4(u32 side)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);                            /* 0x480B8..0x480BC 0x33950 */
    (void)fighter_command_dispatch(ctx[1], (u32)DSB(ctx[2] + 0x5Fu));   /* 0x480C1..0x480D2 0x3B298 */
    fighter_39a10(ctx[4], 0x309u);                          /* 0x480D7..0x480E0 */
    fighter_39a10(ctx[5], 0x309u);                          /* 0x480E5..0x480EE */
    fighter_48170(ctx[0]);                                  /* 0x480F3/0x480F6 */
    fighter_3c208(ctx[1], (s32)(s16)DSW(P3_DIST_C94A6 + (u32)DSB(ctx[3] + 0x7Au) * 2u));   /* 0x480FB..0x48112 */
}

/* 0x4811C — record §P3.7. The slot +0x10 handler 0x48170 stores in the other
 * slot (the dword at 0x48234). 0x3531C case 10 (0x354E2) calls it with EAX =
 * slot, EDX = the slot's record (loaded at 0x35396) and EBX = side; the
 * case's `ret` leaves EAX unread. By the slot's +0x58: 0 nothing; 1 the
 * side's word 0x108380 = 0 and +0x58 = 2; 2 that word + 1 and, once it
 * exceeds 0xF (signed: `sar 0x10` of the dword 0x10837E + 2 * side), +0x54 =
 * 0 and 0x36870(rec); above 2 nothing. */
void fighter_4811c(u32 slot, u32 rec, u32 side)
{
    u8 st = DSB(slot + 0x58u);                              /* 0x48122 */
    if (st < 1u) return;                                    /* 0x48125/0x48128 */
    if (st == 1u) {                                         /* 0x48131 `jbe` */
        DSW(P3_108380 + side * 2u) = 0u;                    /* 0x4812A, 0x4813B/0x4813D */
        DSB(slot + 0x58u) = 2u;                             /* 0x48144 */
        return;
    }
    if (st != 2u) return;                                   /* 0x48133..0x4813A */
    DSW(P3_108380 + side * 2u) = (u16)(DSW(P3_108380 + side * 2u) + 1u);   /* 0x4814B */
    if ((s32)(s16)DSW(P3_108380 + side * 2u) <= 0x0F) return;   /* 0x48152..0x4815E */
    DSB(slot + 0x54u) = 0u;                                 /* 0x48162 */
    fighter_36870(rec);                                     /* 0x48160/0x48166 */
}

/* PORT: the case-10 call 0x354E2 in the port's fighter_state_3531c passes
 * only (slot, side); the raw reaches 0x4811C there with EDX = the slot's
 * record (`mov edx,[ecx]` at 0x35396), as fighter_21458_case10 supplies it. */
void fighter_4811c_case10(u32 slot, u32 side)
{
    fighter_4811c(slot, DSD(slot), side);                   /* 0x35396, 0x354E2 */
}
'''

BINDINGS = r'''/* §P3.7: 0x48608 (0x34E2C at 0x35045), mask 0; the +0x18 hook 0x48054 (the whole EAX); the +0x1C callback 0x480B4
 * and its callee 0x48170 (EAX = side; 0x480FB reloads EAX), mask 0; the +0x10 handler 0x4811C as 0x3531C case 10
 * calls it (EAX = slot, EDX = rec, EBX = side), mask 0. */
static void b_48608(const u32 *r, u32 *eax)            { fighter_48608(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_48054(const u32 *r, u32 *eax)            { *eax = fighter_48054(r[R_EAX]); }
static void b_480b4(const u32 *r, u32 *eax)            { fighter_480b4(r[R_EAX]); *eax = 0u; }
static void b_48170(const u32 *r, u32 *eax)            { fighter_48170(r[R_EAX]); *eax = 0u; }
static void b_4811c(const u32 *r, u32 *eax)            { fighter_4811c(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_48608_at(const u32 *r, u32 speed, int late, int by_ebx)
{
    u32 slot = r[R_EAX], rec = r[R_EDX], s = (u32)DSB(rec + 0x51u);
    hit_anim_start_b(rec, 0x000ED834u, 0x40000000u);
    if (!late) fighter_3c190(s, speed);
    DSB(rec + 0x42u) = 0x1Eu;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x54u) = 0u;
    DSB(slot + 0x57u) = 0u;
    DSD(slot + 0x0Cu) = 0x0004844Cu;
    DSW(0x0010838Cu + (by_ebx ? r[R_EBX] : s) * 2u) = 0u;
    DSD(slot + 0x18u) = 0x00048054u;
    DSD(slot + 0x1Cu) = 0x000480B4u;
    if (late) fighter_3c190(s, speed);
}
static void m_48608(const u32 *r, u32 *eax)            /* 0x3C190 with 0x80 (0x47874's) */
{
    m_48608_at(r, 0x80u, 0, 0);
    *eax = 0u;
}
static void m_48608_order(const u32 *r, u32 *eax)      /* the stores before 0x3C190 */
{
    m_48608_at(r, 0x78u, 1, 0);
    *eax = 0u;
}
static void m_48608_side(const u32 *r, u32 *eax)       /* the word 0x10838C by EBX, not rec+0x51 */
{
    m_48608_at(r, 0x78u, 0, 1);
    *eax = 0u;
}
static u32 m_48054_at(u32 side, int mode)
{
    u32 ctx[6], k, v;
    u8 f[16];
    fighter_ctx_same(ctx, side);
    for (k = 0; k < 16u; k++) f[k] = 2u;
    f[1] = f[8] = f[4] = f[0xD] = 0u;
    f[5] = 1u;
    f[9] = mode == 1 ? 2u : 1u;
    v = (u32)fighter_18c14(ctx[0], f, 0x000C9492u, 0x000C949Cu);
    if (mode != 2 && DSB(ctx[2] + 0x57u) != 0u) v = 1u;
    return v;
}
static void m_48054(const u32 *r, u32 *eax)            /* flag 9 left at 2 */
{
    *eax = m_48054_at(r[R_EAX], 1);
}
static void m_48054_eax(const u32 *r, u32 *eax)        /* 0x18C14's result even with +0x57 set */
{
    *eax = m_48054_at(r[R_EAX], 2);
}
static void m_480b4_at(u32 side, int own_side, int zext, int own_char)
{
    u32 ctx[6], w;
    fighter_ctx_same(ctx, side);
    (void)fighter_command_dispatch(ctx[1], (u32)DSB(ctx[2] + 0x5Fu));
    fighter_39a10(ctx[4], 0x309u);
    fighter_39a10(ctx[5], 0x309u);
    fighter_48170(ctx[0]);
    w = DSW(0x000C94A6u + (u32)DSB(ctx[own_char ? 2 : 3] + 0x7Au) * 2u);
    fighter_3c208(ctx[own_side ? 0 : 1], zext ? (s32)w : (s32)(s16)w);
}
static void m_480b4(const u32 *r, u32 *eax)            /* 0x3C208 on the own side */
{
    m_480b4_at(r[R_EAX], 1, 0, 0);
    *eax = 0u;
}
static void m_480b4_signed(const u32 *r, u32 *eax)     /* the distance word zero-extended */
{
    m_480b4_at(r[R_EAX], 0, 1, 0);
    *eax = 0u;
}
static void m_480b4_char(const u32 *r, u32 *eax)       /* the distance by the own slot's character */
{
    m_480b4_at(r[R_EAX], 0, 0, 1);
    *eax = 0u;
}
static void m_48170_at(u32 side, int own_x, int late57, int own_reset)
{
    u32 ctx[6], x, other = 1u - side;
    u32 own_s = 0x001077B0u + side * 0x94u, oth_s = 0x001077B0u + other * 0x94u;
    fighter_ctx_same(ctx, side);
    DSW(0x00108388u + side * 2u) = 0u;
    fighter_3c148(side);
    fighter_3c148(other);
    hit_anim_start_a(DSD(own_s), 0x000ED850u, 0x40400000u);
    if (!late57) DSB(own_s + 0x57u) = 2u;
    if ((u8)ai_pred_468d8(other) != 0u) fighter_36d98(own_reset ? own_s : oth_s);
    if (late57) DSB(own_s + 0x57u) = 2u;
    x = DSD(ctx[own_x ? 2 : 3] + 0x2Cu);
    hit_anim_start_a(DSD(oth_s), DSD(0x000C90F8u + (u32)DSB(oth_s + 0x7Au) * 4u), 0x40400000u);
    hit_anchor_x(ctx[1], x);
    DSB(oth_s + 0x52u) = 0x10u;
    DSB(oth_s + 0x53u) = 0x0Au;
    DSB(oth_s + 0x54u) = 0u;
    DSD(oth_s + 0x10u) = 0x0004811Cu;
    DSB(oth_s + 0x58u) = 0u;
    DSB(0x00108392u + side) = (DSB(oth_s + 0x43u) & 0x30u) != 0u ? 1u : 0u;
}
static void m_48170(const u32 *r, u32 *eax)            /* 0x188DC with the own slot's +0x2C */
{
    m_48170_at(r[R_EAX], 1, 0, 0);
    *eax = 0u;
}
static void m_48170_order(const u32 *r, u32 *eax)      /* the own +0x57 = 2 after 0x468D8 */
{
    m_48170_at(r[R_EAX], 0, 1, 0);
    *eax = 0u;
}
static void m_48170_reset(const u32 *r, u32 *eax)      /* 0x36D98 on the own slot */
{
    m_48170_at(r[R_EAX], 0, 0, 1);
    *eax = 0u;
}
static void m_4811c_at(const u32 *r, int on_slot, int uns, int late54)
{
    u32 slot = r[R_EAX], a = 0x00108380u + r[R_EBX] * 2u;
    u8 st = DSB(slot + 0x58u);
    if (st < 1u) return;
    if (st == 1u) {
        DSW(a) = 0u;
        DSB(slot + 0x58u) = 2u;
        return;
    }
    if (st != 2u) return;
    DSW(a) = (u16)(DSW(a) + 1u);
    if (uns ? DSW(a) <= 0x0Fu : (s32)(s16)DSW(a) <= 0x0F) return;
    if (!late54) DSB(slot + 0x54u) = 0u;
    fighter_36870(on_slot ? slot : r[R_EDX]);
    if (late54) DSB(slot + 0x54u) = 0u;
}
static void m_4811c(const u32 *r, u32 *eax)            /* 0x36870 on the slot, not the record */
{
    m_4811c_at(r, 1, 0, 0);
    *eax = 0u;
}
static void m_4811c_signed(const u32 *r, u32 *eax)     /* the count compared unsigned */
{
    m_4811c_at(r, 0, 1, 0);
    *eax = 0u;
}
static void m_4811c_order(const u32 *r, u32 *eax)      /* +0x54 = 0 after the 0x36870 call */
{
    m_4811c_at(r, 0, 0, 1);
    *eax = 0u;
}

'''

sub(F, '''    default:                                                /* 2 (0x47FC4), above 3 (`ja` 0x47ECE) */
        return;
    }
}
''', '''    default:                                                /* 2 (0x47FC4), above 3 (`ja` 0x47ECE) */
        return;
    }
}
''' + FIGHTER_C)
sub("port/src/game/fighter.h", "void fighter_47e9c(u32 slot, u32 rec, u32 side);\n",
    """void fighter_47e9c(u32 slot, u32 rec, u32 side);
/* §P3.7: 0x48608 (slot, rec, side), its +0x18 hook fn(side) and +0x1C
 * callback fn(side), the +0x1C callback's callee 0x48170(side), and the
 * +0x10 handler 0x4811C (slot, rec, side) with its case-10 adapter; and the
 * callee 0x36D98 (no longer file-local, so the harness's mutants can call
 * it). */
void fighter_36d98(u32 slot);
void fighter_48608(u32 slot, u32 rec, u32 side);
u32  fighter_48054(u32 side);
void fighter_48170(u32 side);
void fighter_480b4(u32 side);
void fighter_4811c(u32 slot, u32 rec, u32 side);
void fighter_4811c_case10(u32 slot, u32 side);
""")
sub("port/src/game/actors.c", "    fn_register(0x47E9Cu, (void (*)(void))fighter_47e9c);\n",
    """    fn_register(0x47E9Cu, (void (*)(void))fighter_47e9c);
    /* PORT: record 2026-10-03-reverse-p3 §P3.7. Character 2's reaction-0x27
     * callback 0x48608 (the dword at 0xA4234), its +0x18 hook 0x48054
     * (0x19020, fn(side) with EAX returned) and +0x1C callback 0x480B4
     * (0x193B0's 0x19505, fn(side)), and the +0x10 handler 0x4811C that
     * 0x480B4's callee 0x48170 stores in the other slot (0x3531C case 10,
     * (slot, side): the adapter supplies the record). */
    fn_register(0x48608u, (void (*)(void))fighter_48608);
    fn_register(0x48054u, (void (*)(void))fighter_48054);
    fn_register(0x480B4u, (void (*)(void))fighter_480b4);
    fn_register(0x4811Cu, (void (*)(void))fighter_4811c_case10);
""")
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_47e9c@order",      m_47e9c_order,  0x00000000u },\n',
    """    { "fighter_47e9c@order",      m_47e9c_order,  0x00000000u },
    { "fighter_48608",            b_48608,        0x00000000u },
    { "fighter_48054",            b_48054,        0xFFFFFFFFu },
    { "fighter_480b4",            b_480b4,        0x00000000u },
    { "fighter_48170",            b_48170,        0x00000000u },
    { "fighter_4811c",            b_4811c,        0x00000000u },
    { "fighter_48608@mutant",     m_48608,        0x00000000u },
    { "fighter_48608@order",      m_48608_order,  0x00000000u },
    { "fighter_48608@side",       m_48608_side,   0x00000000u },
    { "fighter_48054@mutant",     m_48054,        0xFFFFFFFFu },
    { "fighter_48054@eax",        m_48054_eax,    0xFFFFFFFFu },
    { "fighter_480b4@mutant",     m_480b4,        0x00000000u },
    { "fighter_480b4@signed",     m_480b4_signed, 0x00000000u },
    { "fighter_480b4@char",       m_480b4_char,   0x00000000u },
    { "fighter_48170@mutant",     m_48170,        0x00000000u },
    { "fighter_48170@order",      m_48170_order,  0x00000000u },
    { "fighter_48170@reset",      m_48170_reset,  0x00000000u },
    { "fighter_4811c@mutant",     m_4811c,        0x00000000u },
    { "fighter_4811c@signed",     m_4811c_signed, 0x00000000u },
    { "fighter_4811c@order",      m_4811c_order,  0x00000000u },
""")
print("t6_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_48608 fighter_48054 fighter_480b4 fighter_48170 fighter_4811c; do
  python3 tools/diff_verify.py --image /tmp/pr_p3_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected: no compiler message, then

```
t6_port applied
all checks passed
| fighter_48608 | 0x48608 | 2 | 1/1 | VERIFIED | 3C190 stub unverified, 3C4CC stub unverified |
| fighter_48608@mutant | 0x48608 | 2 | 1/1 | MISMATCH | 3C190 stub unverified, 3C4CC stub unverified |
| fighter_48608@order | 0x48608 | 2 | 1/1 | MISMATCH | 3C190 stub unverified, 3C4CC stub unverified |
| fighter_48608@side | 0x48608 | 2 | 1/1 | MISMATCH | 3C190 stub unverified, 3C4CC stub unverified |
| fighter_48054 | 0x48054 | 3 | 3/3 | VERIFIED | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_48054@mutant | 0x48054 | 3 | 3/3 | MISMATCH | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_48054@eax | 0x48054 | 3 | 3/3 | MISMATCH | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_480b4 | 0x480B4 | 3 | 1/1 | VERIFIED | 33950 allow unverified, 39A10 stub unverified, 3B298 stub unverified, 3C208 stub unverified, 48170 stub unverified |
| fighter_480b4@mutant | 0x480B4 | 3 | 1/1 | MISMATCH | 33950 allow unverified, 39A10 stub unverified, 3B298 stub unverified, 3C208 stub unverified, 48170 stub unverified |
| fighter_480b4@signed | 0x480B4 | 3 | 1/1 | MISMATCH | 33950 allow unverified, 39A10 stub unverified, 3B298 stub unverified, 3C208 stub unverified, 48170 stub unverified |
| fighter_480b4@char | 0x480B4 | 3 | 1/1 | MISMATCH | 33950 allow unverified, 39A10 stub unverified, 3B298 stub unverified, 3C208 stub unverified, 48170 stub unverified |
| fighter_48170 | 0x48170 | 3 | 3/3 | VERIFIED | 188DC stub unverified, 33950 allow unverified, 36D98 stub unverified, 3C148 stub unverified, 3C480 stub unverified, 468D8 stub unverified |
| fighter_48170@mutant | 0x48170 | 3 | 3/3 | MISMATCH | 188DC stub unverified, 33950 allow unverified, 36D98 stub unverified, 3C148 stub unverified, 3C480 stub unverified, 468D8 stub unverified |
| fighter_48170@order | 0x48170 | 3 | 3/3 | MISMATCH | 188DC stub unverified, 33950 allow unverified, 36D98 stub unverified, 3C148 stub unverified, 3C480 stub unverified, 468D8 stub unverified |
| fighter_48170@reset | 0x48170 | 3 | 3/3 | MISMATCH | 188DC stub unverified, 33950 allow unverified, 36D98 stub unverified, 3C148 stub unverified, 3C480 stub unverified, 468D8 stub unverified |
| fighter_4811c | 0x4811C | 6 | 8/8 | VERIFIED | 36870 stub unverified |
| fighter_4811c@mutant | 0x4811C | 6 | 8/8 | MISMATCH | 36870 stub unverified |
| fighter_4811c@signed | 0x4811C | 6 | 8/8 | MISMATCH | 36870 stub unverified |
| fighter_4811c@order | 0x4811C | 6 | 8/8 | MISMATCH | 36870 stub unverified |
```

A row that is not `VERIFIED` is a finding, not something to adjust: stop, compare the C with the bytes (the record's section), and report.

- [ ] **Step 4: the E2 table** (`make entry-triage` first, then regenerate):

```bash
make entry-triage E2_IMAGE=/tmp/pr_p3_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/pr_p3_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
  --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
grep -E '^\| (callbacks|finishers|animation-targets|stubs) ' docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git diff --stat docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -1
```

Expected:

```
entry-triage: FAIL: docs/superpowers/plans/2026-10-01-reverse-e2-triage.md differs from a fresh run (regenerate it with --out)
make: *** [entry-triage] Error 1
entry-triage: targets 288 unported, 207 ported; supplement 131 (9 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 28 in unported code, 87 in ported code, 19 nowhere
| callbacks | 0 | 71 |
| finishers | 0 | 9 |
| animation-targets | 54 | 58 |
| stubs | 53 |
 1 file changed, 7 insertions(+), 7 deletions(-)
```

- [ ] **Step 5: the task gate.**

```bash
make diff-verify entry-triage DIFF_IMAGE=/tmp/pr_p3_d.bin DIFF_TABLE=/tmp/pr_p3_d.md E2_IMAGE=/tmp/pr_p3_e2.bin 2>&1 \
  | grep -E '^(Ran|OK|FAILED|diff-verify:|entry-triage: targets)'
```

Expected (the times vary):

```
Ran 161 tests in N.NNNs
OK
diff-verify: 77/77 functions VERIFIED; 141/141 mutants detected; 1 named gaps; 11/63 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
Ran 44 tests in N.NNNs
OK
entry-triage: targets 288 unported, 207 ported; supplement 131 (9 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 6: every new unit assertion can fail.** Six mutations:

```bash
python3 - <<'PY'
import os
import pathlib
import subprocess


def mutate(path, old, new):
    """Apply one mutation, rebuild, run the suite, print the first FAIL lines, restore the file."""
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



mutate("port/src/game/actors.c", "    fn_register(0x4811Cu, (void (*)(void))fighter_4811c_case10);\n", "")
mutate("port/src/game/fighter.c",
       "    DSW(P3_10838C + s * 2u) = 0u;                           /* 0x48644/0x4864D */",
       "    DSW(P3_10838C + 2u) = 0u;                               /* 0x48644/0x4864D */")
mutate("port/src/game/fighter.c",
       "    if (DSB(ctx[2] + 0x57u) != 0u) r = 1u;                  /* 0x4809E..0x480A8 */\n", "")
mutate("port/src/game/fighter.c",
       "    DSD(oth_s + 0x10u) = 0x0004811Cu;                       /* 0x48231 */",
       "    DSD(oth_s + 0x10u) = 0u;                                /* 0x48231 */")
mutate("port/src/game/fighter.c",
       "    DSB(P3_108392 + side) = (DSB(oth_s + 0x43u) & 0x30u) != 0u ? 1u : 0u;   /* 0x48238..0x48242 */",
       "    DSB(P3_108392 + side) = (DSB(oth_s + 0x43u) & 0x20u) != 0u ? 1u : 0u;   /* 0x48238..0x48242 */")
mutate("port/src/game/fighter.c",
       "    if ((s32)(s16)DSW(P3_108380 + side * 2u) <= 0x0F) return;   /* 0x48152..0x4815E */",
       "    if ((s32)(s16)DSW(P3_108380 + side * 2u) <= 0x10) return;   /* 0x48152..0x4815E */")
PY
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
```

Expected:

```
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:46047: 0x4811C is registered']
port/src/game/fighter.c: 2 FAIL, first: ['test_fight.c:46084: 35980 != 0']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46107: 0 != 1']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46129: 0 != 295196']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46144: 0 != 1']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46165: 68 != 0']
all checks passed
```

- [ ] **Step 7: commit.**

```bash
git add tools/diff_verify.py tools/tests/test_diff_verify.py port/tests/test_fight.c port/tests/test.h \
  port/src/game/fighter.c port/src/game/fighter.h port/src/game/actors.c port/tests/diff_runner.c \
  docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git commit -m "fighter: port 0x48608 0x48054 0x480B4 0x48170 and the +0x10 handler 0x4811C (case-10 adapter), seams 0x188DC 0x36D98 0x3C148 0x468D8 0x48170; E2 table regenerated (track P batch 3)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: `0x4844C`, `0x48608`'s +0x0C callback after its own table (seam `0x3C16C`)

**Files:** as Task 3 (the E2 table does not change: `0x4844C` is outside E2's universe; Step 4 shows it).

**Interfaces:** produces `void fighter_4844c(u32 slot, u32 rec, u32 side)`; `PR_SEAM(0x3C16Cu, side)`; `CLEAR36`, `p3_4844c`; `test_p3_4844c`. Consumes Task 6's `CLEAR34`, `ANCHORX`, `P3_10838C`, `P3_108388`; P1's `ANCHOR` (`0x188AC`, `hit_anchor_set`). Record §P3.8.

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


sub("tools/diff_verify.py", '''    ], calls=(ANIM54,), eax_mask=0, mutants=("@mutant", "@signed", "@order")),
]
''', '''    ], calls=(ANIM54,), eax_mask=0, mutants=("@mutant", "@signed", "@order")),
]

# 0x3C16C (fighter_3c16c): EAX = side; it saves EDX, the one register it writes (record §P3.8).
CLEAR36 = E.Call(0x3C16C, ("eax",))


# 0x4844C (the +0x0C callback 0x48608 stores; 0x3531C case 7, EAX unread; record §P3.8). EBX = side, the context
# 0x33950(side). Each frame the side's words 0x10838C and 0x108388 + 1. By the own slot's +0x57 (table 0x48438):
# 0: the own record's word +0x34 in absolute value above 0x15E clears its +0x42; once the count 0x10838C (signed)
# exceeds 0x1E, +0x54 = 0 and 0x36870(the own record); 2: the count 0x108388 against the five (key, voice) words of
# 0xC94CE (the other slot's +0x43 & 0x30) or 0xC94BA, a voice on the equal key; 3: the own slot's word +0x74 = 0, the
# record's +0x28 bit 5, the side's word 0x108384 = the slot's word +0x2C; with the signed word 0xBD884[the own
# character] above the slot's dword +0x30 (signed): +0x54 = 0, 0x3C148(side), 0x3C16C(side), 0x188AC(side, the
# record's +0x18, 0), the record on 0xC8B58[the own character] at 3.0, 0x188DC(side, that word 0x108384, signed) and
# +0x57 = 4; 1, 4 and above nothing. The other side's words carry sentinels; the slot's +0x2C/+0x30 and the record's
# +0x18/+0x28/+0x34/+0x42 are seeded per case.
def p3_4844c(cid, side, st, w34=0x100, cnt=(0x10, 0x10), o43=0, x2c=0x2C2C2C2C, x30=0x30303030, stub=None):
    own, oth = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    rec = E3_REC if side == 0 else E3_REC2
    w84, w88, w8c = [0x8484, 0x8686], [0x8888, 0x8A8A], [0x8C8C, 0x8E8E]
    w88[side], w8c[side] = cnt
    return Case(cid, {"eax": E3_SLOT, "edx": E3_OUT, "ebx": side},
                {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\\x05", DS_SLOTS + 0x94 + 0x7A: b"\\x03",
                 own + 0x54: bytes([0x54, 0x55, 0x56, st]), own + 0x74: b"\\x74\\x74", own + 0x2C: le32(x2c) + le32(x30),
                 rec + 0x18: le32(0x18181818), rec + 0x28: b"\\x08", rec + 0x34: le32(w34)[:2], rec + 0x42: b"\\x42",
                 oth + 0x43: bytes([o43]), 0x108384: b"".join(le32(v)[:2] for v in w84 + w88 + w8c)},
                {} if stub is None else stub)


P3_SPECS += [
    # a0..aD: state 0's two bounds (|+0x34| 0x100/0x15F/-0x15F/-0x15E; the count 0x1D/0x1E/0x7FFF + 1), state 2's
    # two tables and their keys (2, 25, 56; 6 matches none), state 3's bound (0x1800 against 0x1800, 0x17FF and -1)
    # with a negative word +0x2C (aA), and the states that do nothing.
    Spec("fighter_4844c", 0x4844C, [
        p3_4844c("a0", 0, 0, w34=0x100, cnt=(0x10, 0x1D)),
        p3_4844c("a1", 0, 0, w34=0x15F, cnt=(0x10, 0x1E)),
        p3_4844c("a2", 1, 0, w34=0xFEA1, cnt=(0x10, 0x10)),
        p3_4844c("a3", 0, 0, w34=0xFEA2, cnt=(0x10, 0x7FFF)),
        p3_4844c("a4", 0, 1),
        p3_4844c("a5", 0, 2, cnt=(1, 0), o43=0x10),
        p3_4844c("a6", 1, 2, cnt=(0x18, 0), o43=0x00),
        p3_4844c("a7", 0, 2, cnt=(0x37, 0), o43=0x20, stub={0x2C3FC: 0}),
        p3_4844c("a8", 0, 2, cnt=(5, 0), o43=0x10),
        p3_4844c("a9", 0, 3, x30=0x1800),
        p3_4844c("aA", 0, 3, x2c=0x1234F000, x30=0x17FF),
        p3_4844c("aB", 1, 3, x30=0xFFFFFFFF),
        p3_4844c("aC", 0, 4),
        p3_4844c("aD", 1, 5),
    ], allow_calls=(0x33950,), calls=(ANIM54, VOICE, CLEAR34, CLEAR36, ANCHOR, ANIM_BEGIN, ANCHORX), eax_mask=0,
       mutants=("@mutant", "@signed", "@abs", "@order", "@bound", "@zext")),
]
''')
T = "tools/tests/test_diff_verify.py"
sub(T, '''            "fighter_48170": 0, "fighter_4811c": 0}''',
    '''            "fighter_48170": 0, "fighter_4811c": 0, "fighter_4844c": 0}''')
sub(T, '''            "fighter_4811c@order": {"call #0 memory"}}''',
    '''            "fighter_4811c@order": {"call #0 memory"},
            "fighter_4844c@mutant": {"call #0"}, "fighter_4844c@signed": {"byte", "call #0"},
            "fighter_4844c@abs": {"byte"}, "fighter_4844c@order": {"call #0 memory"},
            "fighter_4844c@bound": {"byte", "call #0", "call #1", "call #2", "call #3", "call #4"},
            "fighter_4844c@zext": {"call #4"}}''')
sub(T, '''                          ("fighter_48170@reset", ["g1"]), ("fighter_4811c@signed", ["i5"])):''',
    '''                          ("fighter_48170@reset", ["g1"]), ("fighter_4811c@signed", ["i5"]),
                          # 0x4844C: the count signed (a3 alone), |+0x34| (a2's -0x15F alone), the bound signed
                          # (aB's -1 alone), 0x188DC's word signed (aA's 0xF000 alone)
                          ("fighter_4844c@signed", ["a3"]), ("fighter_4844c@abs", ["a2"]),
                          ("fighter_4844c@bound", ["aB"]), ("fighter_4844c@zext", ["aA"])):''')
sub(T, '''                                 0x36D98: (), 0x188DC: ("edx",)})''',
    '''                                 0x36D98: (), 0x188DC: ("edx",), 0x3C16C: ()})''')
sub(T, '''        self.assertIn("diff-verify: 77/77 functions VERIFIED; 141/141 mutants detected; 1 named gaps; "
                      "11/63 rows with callees closed (14 have none).", out.getvalue())''',
    '''        self.assertIn("diff-verify: 78/78 functions VERIFIED; 147/147 mutants detected; 1 named gaps; "
                      "11/64 rows with callees closed (14 have none).", out.getvalue())''')
print("t7_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p3_img.bin --function fighter_4844c | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected (the binding does not exist yet):

```
t7_spec applied
| fighter_4844c | 0x4844C | 14 | 23/23 | MISMATCH | 188AC stub unverified, 188DC stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 36870 stub unverified, 3C148 stub unverified, 3C16C stub unverified |
  fighter_4844c: a0: port: unknown binding fighter_4844c
  fighter_4844c: a1: port: unknown binding fighter_4844c
  fighter_4844c: a2: port: unknown binding fighter_4844c
  fighter_4844c: a3: port: unknown binding fighter_4844c
  fighter_4844c: a4: port: unknown binding fighter_4844c
  fighter_4844c: a5: port: unknown binding fighter_4844c
  fighter_4844c: a6: port: unknown binding fighter_4844c
  fighter_4844c: a7: port: unknown binding fighter_4844c
  fighter_4844c: a8: port: unknown binding fighter_4844c
  fighter_4844c: a9: port: unknown binding fighter_4844c
  fighter_4844c: aA: port: unknown binding fighter_4844c
  fighter_4844c: aB: port: unknown binding fighter_4844c
  fighter_4844c: aC: port: unknown binding fighter_4844c
  fighter_4844c: aD: port: unknown binding fighter_4844c
diff-verify: 0/1 functions VERIFIED; 0 named gaps; 0/1 rows with callees closed (0 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
```

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


TEST = r'''
/* §P3.8: 0x4844C through its registration as 0x3531C case 7 calls it (slot
 * 0, its record, side 0; slot 1 character 2's other side). The image dwords
 * are evidence lines, not port behaviour. */
static void p3_check_4844c(void)
{
    p2_cb_fn f;
    u32 k, c;
    CHECK(fn_resolve(0x4844Cu) == (void (*)(void))fighter_4844c, "0x4844C is registered");
    /* 0x4844C starts right after its own jump table 0x48438 (5 dwords) */
    CHECK_EQ_INT((int)DSD(0x00048438u), 0x0004849A);
    CHECK_EQ_INT((int)DSD(0x00048448u), 0x00048601);
    f = (p2_cb_fn)(void *)fn_resolve(0x4844Cu);
    if (f == NULL) return;

    /* state 0: the record's word +0x34 = -0x200 clears its +0x42; the count
     * 0x10838C 0x1D + 1 keeps the hold, 0x1E + 1 ends it (+0x54 = 0); both
     * side-0 counts step, side 1's do not. */
    for (k = 0; k < 2u; k++) {
        z_fseed();
        DSB(Z_S0 + 0x57u) = 0u;
        DSB(Z_S0 + 0x54u) = 0x44u;
        DSW(Z_R0 + 0x34u) = 0xFE00u;
        DSB(Z_R0 + 0x42u) = 0x42u;
        DSW(0x0010838Cu) = k == 0u ? 0x1Du : 0x1Eu;
        DSW(0x0010838Eu) = 0x8E8Eu;
        DSW(0x00108388u) = 0x8888u;
        f(Z_S0, Z_R0, 0u);
        CHECK_EQ_INT((int)DSB(Z_R0 + 0x42u), 0);
        CHECK_EQ_INT((int)DSW(0x0010838Cu), k == 0u ? 0x1E : 0x1F);
        CHECK_EQ_INT((int)DSW(0x0010838Eu), 0x8E8E);
        CHECK_EQ_INT((int)DSW(0x00108388u), 0x8889);
        CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), k == 0u ? 0x44 : 0);
    }

    /* state 2: the count 0x108388 1 + 1 = 2 plays 0x78 (both tables' key 2);
     * 0x18 + 1 = 25 plays 0x70 with slot 1's +0x43 & 0x30 set (0xC94CE),
     * 0x68 without (0xC94BA). */
    for (k = 0; k < 3u; k++) {
        z_fseed();
        DSB(Z_S0 + 0x57u) = 2u;
        DSB(Z_S1 + 0x43u) = k == 2u ? 0x00u : 0x10u;
        DSW(0x00108388u) = k == 0u ? 1u : 0x18u;
        sound_voice_log_reset();
        f(Z_S0, Z_R0, 0u);
        CHECK_EQ_INT((int)sound_voice_log_count(), 1);
        CHECK_EQ_INT((int)sound_voice_log_at(0), k == 0u ? 0x78 : k == 1u ? 0x70 : 0x68);
    }
    sound_voice_log_reset();

    /* state 3: +0x74 = 0, the record's +0x28 bit 5, the side's word 0x108384 =
     * the slot's word +0x2C (0xF000); with +0x30 below 0xBD884[the character]
     * the move ends: the record on 0xC8B58[the character] at 3.0, 0x188DC puts
     * the signed word back in the slot's +0x2C (0xFFFFF000), +0x57 = 4. */
    z_fseed();
    c = (u32)DSB(Z_S0 + 0x7Au);
    DSW(DSD(0x000C8B58u + c * 4u)) = 0x12B1u;
    DSB(Z_S0 + 0x57u) = 3u;
    DSW(Z_S0 + 0x74u) = 0x7474u;
    DSD(Z_S0 + 0x2Cu) = 0x1234F000u;
    DSD(Z_S0 + 0x30u) = (u32)((s32)(s16)DSW(0x000BD884u + c * 2u) - 1);
    DSB(Z_R0 + 0x28u) = 0x08u;
    DSW(0x00108384u) = 0x8484u;
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSW(Z_S0 + 0x74u), 0);
    CHECK_EQ_INT((int)(DSB(Z_R0 + 0x28u) & 0x20u), 0x20);
    CHECK_EQ_INT((int)DSW(0x00108384u), 0xF000);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), (int)DSD(0x000C8B58u + c * 4u));
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x2Cu), (int)0xFFFFF000u);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 4);
}

int test_p3_4844c(void)         { return u6b_run(p3_check_4844c); }
'''

ANCHOR = "int test_p3_48608(void)         { return u6b_run(p3_check_48608); }\n"
sub("port/tests/test_fight.c", ANCHOR, ANCHOR + TEST)
sub("port/tests/test.h", "    X(test_p3_48608) \\\n", "    X(test_p3_48608) \\\n    X(test_p3_4844c) \\\n")
print("t7_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected (line numbers move with `test_fight.c`):

```
t7_test applied
port/tests/test_fight.c:46178:51: error: use of undeclared identifier 'fighter_4844c'
1 error generated.
```

- [ ] **Step 3: the port, the registrations, the seams, the bindings and the mutants.**

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
sub(F, '''void fighter_3c16c(u32 side)
{
''', '''void fighter_3c16c(u32 side)
{
    PR_SEAM(0x3C16Cu, side);
''')

FIGHTER_C = r'''
#define P3_108384        0x00108384u  /* 0x48577/0x485E7: a word per side */
#define P3_BD884         0x000BD884u  /* 0x4858B: [char] the high half of the dword at 0xBD882 + 2c */
#define P3_VOICES_C94BA  0x000C94BAu  /* 0x48530: five (key, voice) word pairs */
#define P3_VOICES_C94CE  0x000C94CEu  /* 0x48500: five (key, voice) word pairs */
#define P3_STREAMS_C8B58 0x000C8B58u  /* 0x485D4: [char] */

/* 0x4844C — record §P3.8. The slot +0x0C callback 0x48608 stores (the dword at
 * 0x48649; 0x3531C case 7, EAX unread); its bytes start right after its own
 * jump table 0x48438 (5 dwords). EBX = side, the context 0x33950(side); the
 * EAX slot and EDX record are not read. Each frame the side's words 0x10838C
 * and 0x108388 + 1. By the own slot's +0x57: 0 clears the own record's +0x42
 * while its word +0x34 exceeds 0x15E in absolute value and, once the count
 * 0x10838C (signed) exceeds 0x1E, ends the hold (+0x54 = 0, 0x36870 on the
 * record); 2 plays the voice of the five (key, voice) pairs at 0xC94CE (the
 * other slot's +0x43 & 0x30) or 0xC94BA whose key equals the count 0x108388;
 * 3 clears the slot's word +0x74, sets the record's +0x28 bit 5, copies the
 * slot's word +0x2C to the side's word 0x108384 and, with the signed word
 * 0xBD884[the own character] above the slot's dword +0x30, ends the move:
 * +0x54 = 0, 0x3C148(side), 0x3C16C(side), 0x188AC(side, the record's +0x18,
 * 0), the record on 0xC8B58[the own character] at 3.0, 0x188DC(side, that
 * signed word 0x108384), +0x57 = 4; 1, 4 and above nothing. */
void fighter_4844c(u32 slot, u32 rec, u32 side)
{
    u32 ctx[6];
    (void)slot;
    (void)rec;
    fighter_ctx_same(ctx, side);                            /* 0x48450..0x48454 0x33950 */
    DSW(P3_10838C + ctx[0] * 2u) = (u16)(DSW(P3_10838C + ctx[0] * 2u) + 1u);   /* 0x4845E/0x4846C/0x4846E */
    DSW(P3_108388 + ctx[0] * 2u) = (u16)(DSW(P3_108388 + ctx[0] * 2u) + 1u);   /* 0x48465/0x4846D/0x48479 */
    switch (DSB(ctx[2] + 0x57u)) {                          /* 0x48475..0x48492 table 0x48438 */
    case 0u: {
        s32 v = (s32)(s16)DSW(ctx[4] + 0x34u);              /* 0x4849A..0x484B2 */
        if (v < 0) v = -v;                                  /* 0x484AB */
        if (v > 0x15E) DSB(ctx[4] + 0x42u) = 0u;            /* 0x484B5..0x484C0 */
        if ((s32)(s16)DSW(P3_10838C + ctx[0] * 2u) <= 0x1E) return;   /* 0x484C4..0x484D4 */
        DSB(ctx[2] + 0x54u) = 0u;                           /* 0x484DA/0x484DE */
        fighter_36870(ctx[4]);                              /* 0x484E2/0x484E6 */
        return;
    }
    case 2u: {
        u32 t = (DSB(ctx[3] + 0x43u) & 0x30u) != 0u ? P3_VOICES_C94CE : P3_VOICES_C94BA;   /* 0x484F0..0x484F8 */
        s32 n = (s32)(s16)DSW(P3_108388 + ctx[0] * 2u);     /* 0x484FA/0x48505, 0x4852A/0x48535 */
        u32 e;
        for (e = t; e != t + 0x14u; e += 4u)                /* 0x48508..0x48528, 0x48538..0x48558 */
            if ((s32)(s16)DSW(e) == n)                      /* 0x4850B..0x48510 */
                (void)sound_voice((u32)DSW(e + 2u));        /* 0x48512..0x48518 0x2C3FC */
        return;
    }
    case 3u:
        DSW(ctx[2] + 0x74u) = 0u;                           /* 0x4855A/0x4855E */
        DSB(ctx[4] + 0x28u) = (u8)(DSB(ctx[4] + 0x28u) | 0x20u);   /* 0x48564/0x48568 */
        DSW(P3_108384 + ctx[0] * 2u) = DSW(ctx[2] + 0x2Cu); /* 0x4856C..0x48577 */
        if ((s32)(s16)DSW(P3_BD884 + (u32)DSB(ctx[2] + 0x7Au) * 2u)
                <= (s32)DSD(ctx[2] + 0x30u)) return;        /* 0x4857F..0x4859C `jle` */
        DSB(ctx[2] + 0x54u) = 0u;                           /* 0x4859E */
        fighter_3c148(ctx[0]);                              /* 0x485A2/0x485A5 */
        fighter_3c16c(ctx[0]);                              /* 0x485AA/0x485AD */
        hit_anchor_set(ctx[0], DSD(ctx[4] + 0x18u), 0u);    /* 0x485B2..0x485BE 0x188AC */
        actors_anim_begin(ctx[4], DSD(P3_STREAMS_C8B58 + (u32)DSB(ctx[2] + 0x7Au) * 4u),
                          0x40400000u);                     /* 0x485C3..0x485DF 0x2BC30 */
        hit_anchor_x(ctx[0], (u32)(s32)(s16)DSW(P3_108384 + ctx[0] * 2u));   /* 0x485E4..0x485F4 0x188DC */
        DSB(ctx[2] + 0x57u) = 4u;                           /* 0x485F9/0x485FD */
        return;
    default:                                                /* 1, 4 (0x48601), above 4 (`ja` 0x48486) */
        return;
    }
}
'''

BINDINGS = r'''/* §P3.8: the +0x0C callback 0x4844C (0x3531C case 7), mask 0. */
static void b_4844c(const u32 *r, u32 *eax)            { fighter_4844c(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_4844c_at(u32 side, int swap, int uns, int noabs, int late54, int ubound, int zext)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);
    DSW(0x0010838Cu + ctx[0] * 2u) = (u16)(DSW(0x0010838Cu + ctx[0] * 2u) + 1u);
    DSW(0x00108388u + ctx[0] * 2u) = (u16)(DSW(0x00108388u + ctx[0] * 2u) + 1u);
    switch (DSB(ctx[2] + 0x57u)) {
    case 0u: {
        s32 v = (s32)(s16)DSW(ctx[4] + 0x34u);
        u16 c = DSW(0x0010838Cu + ctx[0] * 2u);
        if (!noabs && v < 0) v = -v;
        if (v > 0x15E) DSB(ctx[4] + 0x42u) = 0u;
        if (uns ? c <= 0x1Eu : (s32)(s16)c <= 0x1E) return;
        DSB(ctx[2] + 0x54u) = 0u;
        fighter_36870(ctx[4]);
        return;
    }
    case 2u: {
        int hit = (DSB(ctx[3] + 0x43u) & 0x30u) != 0u;
        u32 t = (hit != swap) ? 0x000C94CEu : 0x000C94BAu, e;
        s32 n = (s32)(s16)DSW(0x00108388u + ctx[0] * 2u);
        for (e = t; e != t + 0x14u; e += 4u)
            if ((s32)(s16)DSW(e) == n) (void)sound_voice((u32)DSW(e + 2u));
        return;
    }
    case 3u: {
        s32 w;
        u16 x;
        DSW(ctx[2] + 0x74u) = 0u;
        DSB(ctx[4] + 0x28u) = (u8)(DSB(ctx[4] + 0x28u) | 0x20u);
        DSW(0x00108384u + ctx[0] * 2u) = DSW(ctx[2] + 0x2Cu);
        w = (s32)(s16)DSW(0x000BD884u + (u32)DSB(ctx[2] + 0x7Au) * 2u);
        if (ubound ? (u32)w <= DSD(ctx[2] + 0x30u) : w <= (s32)DSD(ctx[2] + 0x30u)) return;
        if (!late54) DSB(ctx[2] + 0x54u) = 0u;
        fighter_3c148(ctx[0]);
        if (late54) DSB(ctx[2] + 0x54u) = 0u;
        fighter_3c16c(ctx[0]);
        hit_anchor_set(ctx[0], DSD(ctx[4] + 0x18u), 0u);
        actors_anim_begin(ctx[4], DSD(0x000C8B58u + (u32)DSB(ctx[2] + 0x7Au) * 4u), 0x40400000u);
        x = DSW(0x00108384u + ctx[0] * 2u);
        hit_anchor_x(ctx[0], zext ? (u32)x : (u32)(s32)(s16)x);
        DSB(ctx[2] + 0x57u) = 4u;
        return;
    }
    default:
        return;
    }
}
static void m_4844c(const u32 *r, u32 *eax)            /* the two voice tables swapped */
{
    m_4844c_at(r[R_EBX], 1, 0, 0, 0, 0, 0);
    *eax = 0u;
}
static void m_4844c_signed(const u32 *r, u32 *eax)     /* the count 0x10838C compared unsigned */
{
    m_4844c_at(r[R_EBX], 0, 1, 0, 0, 0, 0);
    *eax = 0u;
}
static void m_4844c_abs(const u32 *r, u32 *eax)        /* +0x34 compared without its absolute value */
{
    m_4844c_at(r[R_EBX], 0, 0, 1, 0, 0, 0);
    *eax = 0u;
}
static void m_4844c_order(const u32 *r, u32 *eax)      /* +0x54 = 0 after 0x3C148 */
{
    m_4844c_at(r[R_EBX], 0, 0, 0, 1, 0, 0);
    *eax = 0u;
}
static void m_4844c_bound(const u32 *r, u32 *eax)      /* the slot's +0x30 compared unsigned */
{
    m_4844c_at(r[R_EBX], 0, 0, 0, 0, 1, 0);
    *eax = 0u;
}
static void m_4844c_zext(const u32 *r, u32 *eax)       /* 0x188DC's word zero-extended */
{
    m_4844c_at(r[R_EBX], 0, 0, 0, 0, 0, 1);
    *eax = 0u;
}

'''

sub(F, '''void fighter_4811c_case10(u32 slot, u32 side)
{
    fighter_4811c(slot, DSD(slot), side);                   /* 0x35396, 0x354E2 */
}
''', '''void fighter_4811c_case10(u32 slot, u32 side)
{
    fighter_4811c(slot, DSD(slot), side);                   /* 0x35396, 0x354E2 */
}
''' + FIGHTER_C)
sub("port/src/game/fighter.h", "void fighter_4811c_case10(u32 slot, u32 side);\n",
    """void fighter_4811c_case10(u32 slot, u32 side);
/* §P3.8: 0x48608's +0x0C callback (slot, rec, side). */
void fighter_4844c(u32 slot, u32 rec, u32 side);
""")
sub("port/src/game/actors.c", "    fn_register(0x4811Cu, (void (*)(void))fighter_4811c_case10);\n",
    """    fn_register(0x4811Cu, (void (*)(void))fighter_4811c_case10);
    /* PORT: record 2026-10-03-reverse-p3 §P3.8. 0x48608's +0x0C callback
     * 0x4844C (after its own jump table 0x48438; 0x3531C case 7, (slot, rec,
     * side)). */
    fn_register(0x4844Cu, (void (*)(void))fighter_4844c);
""")
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_4811c@order",      m_4811c_order,  0x00000000u },\n',
    """    { "fighter_4811c@order",      m_4811c_order,  0x00000000u },
    { "fighter_4844c",            b_4844c,        0x00000000u },
    { "fighter_4844c@mutant",     m_4844c,        0x00000000u },
    { "fighter_4844c@signed",     m_4844c_signed, 0x00000000u },
    { "fighter_4844c@abs",        m_4844c_abs,    0x00000000u },
    { "fighter_4844c@order",      m_4844c_order,  0x00000000u },
    { "fighter_4844c@bound",      m_4844c_bound,  0x00000000u },
    { "fighter_4844c@zext",       m_4844c_zext,   0x00000000u },
""")
print("t7_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_4844c; do
  python3 tools/diff_verify.py --image /tmp/pr_p3_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected: no compiler message, then

```
t7_port applied
all checks passed
| fighter_4844c | 0x4844C | 14 | 23/23 | VERIFIED | 188AC stub unverified, 188DC stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 36870 stub unverified, 3C148 stub unverified, 3C16C stub unverified |
| fighter_4844c@mutant | 0x4844C | 14 | 23/23 | MISMATCH | 188AC stub unverified, 188DC stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 36870 stub unverified, 3C148 stub unverified, 3C16C stub unverified |
| fighter_4844c@signed | 0x4844C | 14 | 23/23 | MISMATCH | 188AC stub unverified, 188DC stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 36870 stub unverified, 3C148 stub unverified, 3C16C stub unverified |
| fighter_4844c@abs | 0x4844C | 14 | 23/23 | MISMATCH | 188AC stub unverified, 188DC stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 36870 stub unverified, 3C148 stub unverified, 3C16C stub unverified |
| fighter_4844c@order | 0x4844C | 14 | 23/23 | MISMATCH | 188AC stub unverified, 188DC stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 36870 stub unverified, 3C148 stub unverified, 3C16C stub unverified |
| fighter_4844c@bound | 0x4844C | 14 | 23/23 | MISMATCH | 188AC stub unverified, 188DC stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 36870 stub unverified, 3C148 stub unverified, 3C16C stub unverified |
| fighter_4844c@zext | 0x4844C | 14 | 23/23 | MISMATCH | 188AC stub unverified, 188DC stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 36870 stub unverified, 3C148 stub unverified, 3C16C stub unverified |
```

A row that is not `VERIFIED` is a finding, not something to adjust: stop, compare the C with the bytes (the record's section), and report.

- [ ] **Step 4: the E2 table** (`make entry-triage` first, then regenerate):

```bash
make entry-triage E2_IMAGE=/tmp/pr_p3_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/pr_p3_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
  --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
grep -E '^\| (callbacks|finishers|animation-targets|stubs) ' docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git diff --stat docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -1
```

Expected: the table does not change (`0x4844C` is outside E2's universe, like P2's `0x22638`): `make entry-triage` passes, the regeneration writes the same bytes and `git diff --stat` prints nothing; the file is still staged in Step 7 so the commit records the gate as run.

```
entry-triage: targets 288 unported, 207 ported; supplement 131 (9 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 28 in unported code, 87 in ported code, 19 nowhere
entry-triage: targets 288 unported, 207 ported; supplement 131 (9 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 28 in unported code, 87 in ported code, 19 nowhere
| callbacks | 0 | 71 |
| finishers | 0 | 9 |
| animation-targets | 54 | 58 |
| stubs | 53 |
```

- [ ] **Step 5: the task gate.**

```bash
make diff-verify entry-triage DIFF_IMAGE=/tmp/pr_p3_d.bin DIFF_TABLE=/tmp/pr_p3_d.md E2_IMAGE=/tmp/pr_p3_e2.bin 2>&1 \
  | grep -E '^(Ran|OK|FAILED|diff-verify:|entry-triage: targets)'
```

Expected (the times vary):

```
Ran 161 tests in N.NNNs
OK
diff-verify: 78/78 functions VERIFIED; 147/147 mutants detected; 1 named gaps; 11/64 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
Ran 44 tests in N.NNNs
OK
entry-triage: targets 288 unported, 207 ported; supplement 131 (9 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 6: every new unit assertion can fail.** Five mutations:

```bash
python3 - <<'PY'
import os
import pathlib
import subprocess


def mutate(path, old, new):
    """Apply one mutation, rebuild, run the suite, print the first FAIL lines, restore the file."""
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



mutate("port/src/game/actors.c", "    fn_register(0x4844Cu, (void (*)(void))fighter_4844c);\n", "")
mutate("port/src/game/fighter.c",
       "        u32 t = (DSB(ctx[3] + 0x43u) & 0x30u) != 0u ? P3_VOICES_C94CE : P3_VOICES_C94BA;   /* 0x484F0..0x484F8 */",
       "        u32 t = (DSB(ctx[3] + 0x43u) & 0x30u) != 0u ? P3_VOICES_C94BA : P3_VOICES_C94CE;   /* 0x484F0..0x484F8 */")
mutate("port/src/game/fighter.c",
       "        if (v > 0x15E) DSB(ctx[4] + 0x42u) = 0u;            /* 0x484B5..0x484C0 */",
       "        if (v > 0x200) DSB(ctx[4] + 0x42u) = 0u;            /* 0x484B5..0x484C0 */")
mutate("port/src/game/fighter.c",
       "        if ((s32)(s16)DSW(P3_10838C + ctx[0] * 2u) <= 0x1E) return;   /* 0x484C4..0x484D4 */",
       "        if ((s32)(s16)DSW(P3_10838C + ctx[0] * 2u) <= 0x1F) return;   /* 0x484C4..0x484D4 */")
mutate("port/src/game/fighter.c",
       "        hit_anchor_x(ctx[0], (u32)(s32)(s16)DSW(P3_108384 + ctx[0] * 2u));   /* 0x485E4..0x485F4 0x188DC */",
       "        hit_anchor_x(ctx[0], (u32)DSW(P3_108384 + ctx[0] * 2u));   /* 0x485E4..0x485F4 0x188DC */")
PY
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
```

Expected:

```
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:46178: 0x4844C is registered']
port/src/game/fighter.c: 2 FAIL, first: ['test_fight.c:46216: 104 != 112']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46198: 66 != 0']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46202: 68 != 0']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46238: 61440 != -4096']
all checks passed
```

- [ ] **Step 7: commit.**

```bash
git add tools/diff_verify.py tools/tests/test_diff_verify.py port/tests/test_fight.c port/tests/test.h \
  port/src/game/fighter.c port/src/game/fighter.h port/src/game/actors.c port/tests/diff_runner.c \
  docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git commit -m "fighter: port 0x4844C (after its own table 0x48438), seam 0x3C16C (track P batch 3)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: closure: the full gate, the docs, the record

**Files:** `docs/PROGRESS.md`, `docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md` (§W.17), `docs/superpowers/plans/2026-10-03-reverse-p3-derivations.md` (§P3.12). **Interfaces:** consumes Tasks 2-7.

- [ ] **Step 1: the full gate** (Task 1 Step 1's commands with the log `/tmp/pr_p3_final.log`) and the WAV (Task 1 Step 2).

Expected (the prototype's final state, measured): `EXIT=0`, `ORACLES-EQUAL`, `WAV-SAME`, and

```
gp-idle-loss: frames: first unexplained 2064, ratchet N 2064 ok
gp-idle-loss: trace: 0 differing through 8319; ratchet N 8320 ok
gp-u5-charsel: frames: first unexplained 516, ratchet N 516 ok
gp-u5-charsel: trace: 0 differing through 1512; ratchet N 1513 ok
gp-u6-moves-b: frames: first unexplained 1005, ratchet N 1005 ok
gp-u6-moves-b: trace: first differing 2262, ratchet N 2262 ok
gp-u6-moves-b: moves: first differing 2949, ratchet N 2949 ok
gp_keys: gp-keys-fight: effects: first not reproduced 11, ratchet N 11 ok
gp-twop: frames: first unexplained 612, ratchet N 612 ok
gp-twop: trace: 0 differing through 1505; ratchet N 1506 ok
gp-twop: moves: 0 differing through 1505; ratchet N 1506 ok
gp-u8-right-arcade: frames: first unexplained 1072, ratchet N 1072 ok
gp-u8-right-arcade: trace: 0 differing through 2273; ratchet N 2274 ok
gp-u8-left-training: frames: first unexplained 1076, ratchet N 1076 ok
gp-u8-left-training: trace: 0 differing through 2337; ratchet N 2338 ok
gp-u8-right-training: frames: first unexplained 1098, ratchet N 1098 ok
gp-u8-right-training: trace: 0 differing through 2401; ratchet N 2402 ok
gp-u8-tug-of-war: frames: first unexplained 1107, ratchet N 1107 ok
gp-u8-tug-of-war: trace: 0 differing through 2465; ratchet N 2466 ok
gp-u8-handicap: frames: first unexplained 1022, ratchet N 1022 ok
gp-u8-handicap: trace: 0 differing through 2273; ratchet N 2274 ok
gp-u8-endurance: frames: first unexplained 278, ratchet N 278 ok
gp-u8-endurance: trace: 0 differing through 1173; ratchet N 1174 ok
gp-u8-attract-start: frames: first unexplained 1087, ratchet N 1087 ok
gp-u8-attract-start: trace: 0 differing through 2017; ratchet N 2018 ok
gp-u9-win: frames: first unexplained 346, ratchet N 346 ok
gp-u9-win: trace: first differing 2150, ratchet N 2150 ok
gp-u9-win: path: 0 not reproduced through 7; ratchet N 8 ok
gp-u9-win: win: first differing 3162, ratchet N 3162 ok
gp-u10-ending: frames: first unexplained 331, ratchet N 331 ok
gp-u10-ending: trace: 0 differing through 9953; ratchet N 9954 ok
gp-u10-ending: path: 0 not reproduced through 29; ratchet N 30 ok
gp-u10-ending: win: 0 differing through 9953; ratchet N 9954 ok
diff-verify: 78/78 functions VERIFIED; 147/147 mutants detected; 1 named gaps; 11/64 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 288 unported, 207 ported; supplement 131 (9 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 28 in unported code, 87 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

Every gp ratchet line equals Task 1's; the miss sets of every gp driver pass (no `fn-miss ... unexpected` line: `grep -c 'unexpected' /tmp/pr_p3_final.log` prints 0).

- [ ] **Step 2: the docs.**

```bash
python3 - <<'PY'
import pathlib

PROGRESS = '''

**Track P batch 3: the move callbacks `0x475EC..0x489A0` and the callbacks they store (plan `2026-10-03-reverse-p3-move-callbacks-b.md`, record `2026-10-03-reverse-p3-derivations.md`).** Twenty-four functions, all character 2's, ported from the raw and differentially verified, every block hit and every store observable (record §P3.1: the roadmap's list, closed under the callbacks they store): the move callbacks `0x475EC 0x47608 0x47624 0x48964 0x489A0 0x47720 0x47874 0x47FCC 0x48608`, the +0x0C callbacks `0x476FC 0x47830 0x47E9C 0x4844C` (the last two right after their own jump tables, bounded switches the harness follows), the +0x18 hooks `0x47648 0x477A8 0x47CB0 0x48054`, the +0x1C callbacks `0x47688 0x477E8 0x47D24 0x480B4`, the +0x14 callback `0x47798`, `0x480B4`'s callee `0x48170` and the +0x10 handler `0x4811C` it stores in the other slot (registered through a case-10 adapter, as `0x21458`). Eleven callees gained seams (`0x35838 0x3B298 0x39FB0 0x3A95C 0x3C190 0x3B714 0x3C148 0x468D8 0x36D98 0x188DC 0x3C16C`) and `0x48170` its own. `gp-u10-ending` drops its `0x475EC` row (reached at f=0x1594 in the final since P2 ported `0x2381C`); every U10 pin stays exact (frames 331, trace and WIN 9954, milestones 30); no other gp miss set held a P3 member, and every other oracle line is unchanged. `make diff-verify`: `78/78 functions VERIFIED; 147/147 mutants detected; 1 named gaps; 11/64 rows with callees closed (14 have none)`. E2 table: 288 unported / 207 ported targets, callbacks 0 / 71, supplement 9 unported, voice 28 / 87 / 19. Named gaps: the stream targets of P3's streams (`0x47E04 0x47E30 0x482E4 0x48374` P6, `0x48254` P7, reached by no capture); `0x47E9C`'s x87 sums are computed in double (the stored float equals the x87's for every float of [1.0, 6.0], an exhaustive check); the eleven new stubs get their rows in C1. `port_progress.py` stays `771 1203 64` (none of the 24 is a Ghidra function) and README is untouched.
'''
p = pathlib.Path("docs/PROGRESS.md")
s = p.read_text()
p.write_text(s.rstrip("\n") + PROGRESS)

W17 = '''

## §W.17 Track P batch 3 drops `0x475EC` (plan reverse-p3 Task 2)

With `0x475EC` ported (record `2026-10-03-reverse-p3-derivations.md` §P3.9), the `gp-u10-ending` replay records §W.16's
set less that row (`distinct=7`): no other target appears. `make gp-ending-oracle` holds every pin of §W.16 (frames
331, trace 9954, milestones 30, WIN 9954), each + 1 failing; MAX_START stays 83. The re-measure list is now
`0x3DA50` (P5), `0x37DD4` (P6), `0x29C78` (P7).
'''
p = pathlib.Path("docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md")
s = p.read_text()
p.write_text(s.rstrip("\n") + W17)
print("t8_docs applied")
PY
```

- [ ] **Step 3: the record's §P3.12.** Append the implementation's closure paragraph: the commit range, each task's counter and E2 lines as measured (§P3.12's table is the replay's), the final gate's lines from Step 1 and the time it took, and any deviation from this plan's expected output (a deviation is a finding: record the raw fact and the address).

- [ ] **Step 4: commit.**

```bash
git add docs/PROGRESS.md docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md \
  docs/superpowers/plans/2026-10-03-reverse-p3-derivations.md
git commit -m "docs: P3 closure (the move callbacks 0x475EC..0x489A0): PROGRESS, U9/U10 record §W.17, record §P3.12

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

Then the whole-branch review and, before the merge, the full gate again (Step 1).

## Self-review (the planner's)

- **Spec coverage.** Every member of §P3.1 has a task, a C function, a registration, a row, mutants (at least one caught only by a call or the memory at one, where the row has callees), a unit check and its mutation proof; every new stub has a seam and a declared `clobbers` re-derived by `test_each_stub_declares_the_registers_its_callee_clobbers`; the E2 table is regenerated in the commit of each port (Task 7's leaves it unchanged, measured); the one gp set holding a member is re-measured (Task 2 Step 7) and the rest are shown unchanged (Task 8). Span code (D2) and the callee rows (D3) are outside the plan by decision.
- **Placeholders.** None: every step carries its script and its measured output.
- **Names.** `P3_SPECS`, `P3_MASKS`, `P3_KINDS`, `P3_SLOT_CBS`, `p3_hook0`, `DISPATCH`, `SPEED`, `CHECKS`, `ANCHOR`, `CLEAR34`, `ANCHORX` and the C names are defined in the task that first uses them and used with the same spelling after.
