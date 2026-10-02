# P2: the move callbacks 0x14EF8..0x3DCEC and the callbacks they store (track P, batch 2) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the 24 functions of track P's batch 2 (record §P2.1: the 13 unported move callbacks `0x14EF8 0x14F50 0x15478 0x21114 0x21374 0x22938 0x22A00 0x237D0 0x2381C 0x3D10C 0x3DADC 0x3DB34 0x3DCEC`, the callbacks they store `0x2116C 0x22510 0x211F0 0x22588 0x212CC 0x22638 0x229FC` with `0x22588`'s callee `0x22404`, and the stream targets the U8 replay reaches `0x14FA8 0x14FF8 0x150AC`), each verified against the original's bytes with its callees stubbed (E3 §E3.10), every block hit and every store observable; drop `gp-u8-right-arcade`'s two miss rows and raise its pins to what the replay now reaches; regenerate the E2 table with each port.

**Architecture:** One C function per original function, appended to `port/src/game/fighter.c` after P1's `fighter_3f174`, registered in `actors_init` (`port/src/game/actors.c`) so `0x34E2C` (move callbacks), `0x3531C` case 7 (+0x0C), `0x19020` (+0x18), `0x193B0` (+0x1C) and `anim_indirect` (the `0xD000` targets) reach them. Each new stubbed callee opens with `PR_SEAM`/`PR_SEAM_RET` (`0x34D8C 0x18C14 0x18AF8 0x39834 0x39A10 0x3C208 0x3C358 0x22404 0x36870`); each function has a binding and mutants in `port/tests/diff_runner.c`, a `Spec` in `tools/diff_verify.py` (`P2_SPECS`), seeded unit checks in `port/tests/test_fight.c`, and its row in the self-check counter. Three small harness changes are forced by the raw: `[reg+N]` call arguments (`0x18C14`'s flags live on the caller's stack), `PR_SEAM0` (`0x18AF8` takes no argument), and `0x22638`'s own jump table in `RESOLVED_JUMPS`; one infrastructure fix: `fn_register` skips a repeated identical pair (the table overflowed).

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and `capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §3 decision 1 ("port all of O3-O6, whether or not a capture reaches them"), §4 track P ("port batches, each function verified by E"), §5.1-§5.3, §6 ("P: each ported function has a verification row; unhit blocks and non-emulable instructions named; counters and README percentage updated"), §7.

**Derivation record:** `docs/superpowers/plans/2026-10-02-reverse-p2-derivations.md` (§P2.1 the 24 members from the raw, §P2.2 callers, masks and callee declarations, §P2.3-§P2.9 each function from the bytes, §P2.10 decisions and named gaps, §P2.11 the roadmap after P2, §P2.12 the U8 and U9/U10 interactions, §P2.13 results). Recipe: `2026-10-01-reverse-e3-derivations.md` §E3.10; lessons: `2026-10-02-reverse-p1-derivations.md` §P1.10-§P1.12.

**What the planner ran (scratch, 2026-10-02, `main` at `1085402`, image sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; scratch paths in place of `/tmp`):** a prototype (each task developed in order and committed in a scratch git repository; the full `make verify` with the parallel-safe overrides ran on its final state, quoted in Task 9) and a replay (a fresh copy of `1085402`, Tasks 2-8 applied in order by one driver with exactly the scripts below; every red and green output and every mutation result quoted below is the replay's, with the scratch paths written as `/tmp/pr_p2_*` and test times as `N.NNN`). Every code block below is a file the replay ran, byte for byte. A store sweep (scratch: every non-stack store instruction of each new row is the last writer of a byte whose value then differs from the case's start, at the end or at a recorded call) finds every store of the 24 rows observable (the x87 loads and compares of `0x22638` aside, which read memory).

## Decisions needed from the user

**None.** The user's decisions D2 (span code: a named gap backed by the pixel oracles; not touched here) and D3 (callee rows in C1, after P3; the nine new stubs join that batch) stand. Two choices this plan makes are derived from the raw and recorded as corrections (record §P2.1, §P2.7, §P2.10), not left open:

1. **The member list is 24, not 19.** `0x22938` and `0x22A00` store two +0x0C callbacks the roadmap missed (`0x22638`, after its own jump table; `0x229FC`, the `ret` ending `0x229E8`), and the U8 right-arcade replay, once `0x14EF8`/`0x14F50` exist, misses the `0xD000` targets of their streams, `0x14FA8 0x14FF8 0x150AC` (P4/P5 in the roadmap). P1's final review moved the finisher streams' targets into P1 for the same reason (§P1.12, I4); the alternative, pinning `0x14FA8`/`0x14FF8` as RA misses, leaves the callbacks running without the held record that guards them.
2. **`0x18C14`'s flags are compared by value.** E3 §E3.10 item 6 would allow-mode a callee given a stack buffer, but `0x18C14`'s tree is 263 functions with indirect calls into the runtime; the harness instead reads `[edx]..[edx+12]` on the original side and the seam passes the 16 bytes as four dwords (record §P2.7). Without it the two +0x18 hooks' flags (their whole work) would be unobservable, the defect class of P1's final review.

## The P-track roadmap

From record §P1.3 as corrected by §P2.11. Each line is one plan and one subagent-driven run.

| batch | ports | what |
|---|---|---|
| P1 (merged) | 16 + 1 callee row | the finisher entries, their +0x0C callbacks, `0x38034`, the finisher streams' targets |
| **P2** (this plan) | **24** | the 13 move callbacks, the 7 callbacks they store, `0x22404`, and `0x14FA8 0x14FF8 0x150AC` (from P4/P5) |
| P3 | 24 | move callbacks `0x475EC..0x489A0` and the callbacks they store |
| C1 | about 34 rows | verification only: the ported callees P1-P3 stub (P2 adds `0x34D8C 0x18C14 0x18AF8 0x39834 0x39A10 0x3C208 0x3C358 0x36870`) |
| P4 | 20 | animation targets A (less `0x14FA8`) |
| P5 | 13 | animation targets B (less `0x14FF8 0x150AC`) |
| P6 | 18 | animation targets C |
| C2 | about 20 rows | verification only: the rest of the stubbed callees |
| P7 | 14 | the unported direct callees with their callers, the targets outside E2 |
| P8 | 16 + triage | the rest |
| span | 0 | decision D2 |

Total **145 functions** (143 + `0x22638` + `0x229FC`, both outside E2's list). None is a Ghidra `FN_` function: `port_progress.py` stays `771 1203 64` / `731 731 100` and README does not move.

## Global Constraints

Copied verbatim from their sources:

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- Spec §8: "`make verify` is the gate after every task." User ruling (2026-10-01, speed-up): the per-task gate is the task's tests, `make diff-verify`, `make entry-triage` and the gp oracles whose miss sets the task touches; the full `make verify` runs at the baseline (Task 1), the final task (Task 9) and before the merge, with the parallel-safe overrides `T=p2; SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md E2_IMAGE=/tmp/pr_${T}_e2.bin`. The 45 oracle lines (`grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>`) must equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; the `make audio-render` WAV must be byte-identical to `.superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav`; `python3 tools/port_progress.py` prints `771 1203 64` and `731 731 100`. A full `make verify` takes 15 min alone and 1-2 h when another verify shares the host.
- AGENTS.md: "**The E2 target list is a gate** ... A commit that ports a target (a new `/* 0xADDR` header or `fn_register`) regenerates the table in the same commit (decision D3) ... A conflict in that file is resolved by regenerating it after the rebase, never by merging lines by hand."
- AGENTS.md: "One C function per original function, header comment `/* 0xADDR — spec section */`." / "Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment styles in `port/src`." / "Code addresses stored in data go through `fn_origin()` / `fn_resolve()`." / "**`port/src/symbols.h` is generated** ... Never hand-edit it; where the generator emits no name, use a local `#define` with the raw address."
- AGENTS.md: "A ported function that a differentially verified function stubs or records ... opens with `PR_SEAM(0xADDR, args...)` (void) or `PR_SEAM_RET(0xADDR, args...)` ... The arguments are the C signature's, in order (a pointer into `mem[]` as its offset)." Record E3 §E3.8: "A seam must be the callee's first statement". This plan adds `PR_SEAM0(0xADDR)` (no argument) and the by-value form of a stack buffer (record §P2.7).
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." / "**Register a test once** — add `X(test_foo)` to `TEST_CASES` in `port/tests/test.h`" / "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests, and never assert an unseeded BSS-zero". "Consolidating must not change an assertion": this plan **extends** the exact-set assertions of `RealFunctionTests` (rows, masks, mutant names, stub clobbers, resolved tables, the counter line) and changes no other assertion.
- AGENTS.md: "Every env-gated driver and `--check` run arms `fn_resolve`'s miss log and must record exactly the pinned known-set": only `gp-u8-right-arcade`'s set held P2 members; Task 3 drops them (record §P2.12).
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style: `<area>: <what changed>`. **Never `git add -A`** — stage named files." Trailer: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.

## Review Focus

1. **A store no case can observe** (P1's final review found three). Every new row's seeds put a sentinel on each field the function writes, different from what it writes (`P2_SEED`, `p2_ctx_case`, `p2_1c`, `p2_22638`); the planner's store sweep found none unobservable; the `@order` mutants (`0x22938`, `0x211F0`, `0x22588`) and `0x22404@mutant` prove the memory at a call is compared.
2. **A guard or bound read with the wrong width or sign.** Pinned by cases each alone catching a mutant: `g0` (`0x237D0@guard`, a byte test of the dword slot+8), `k4` (`0x2116C@unsigned`), `j1` (`0x22510@ge`), `a2` (`0x22404@signed`), `m2` (`0x212CC@signed`), `pE`/`pF` (`0x22638@signed`, `@byte5d`), `u1` (`0x3DCEC@mutant`, a hard-coded stream).
3. **The flags 0x2116C/0x22510 hand 0x18C14** (their only work). Compared by value through `[edx]..[edx+12]` (Task 6); `fighter_2116c@mutant` and `fighter_22510@mutant` (one flag wrong) are caught by `call #0` alone; `DerefArgTests` and its two mutations pin the harness side.
4. **A registration missing, or the table overflowing.** Each task's unit check asserts `fn_resolve(addr) == fn` and the image dword that holds the address, and its mutation proof deletes a `fn_register`; Task 6's `test_fn_register_repeats` pins the repeated-pair skip (without it the run aborts).
5. **The U8 right-arcade pins and the other gp oracles.** Task 3 measures RA from red (the old miss set) to green and pins N = 1072, F = 2274 as exact (one more fails); Task 9's `make verify` shows every other gp ratchet unchanged.

## Where to run

The worktree `.worktrees/reverse-p2` (branch `reverse-p2` off `main` `1085402`), already set up:

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse/.worktrees/reverse-p2
ls -d data .superpowers port/tests/ghidra_data.bin port/tests/title_screen_ref.ppm   # all four must exist
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -1                           # [100%] Built target ...
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_p2_img.bin && shasum /tmp/pr_p2_img.bin
```

The last line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the image differs from the one the record measured: stop. Every command runs from the worktree root. Ledger: `.superpowers/sdd/2026-10-02-reverse-p2-move-callbacks/progress.md`.

**How the code steps are written.** Each change is a `python3 - <<'PY'` script that replaces exact text and asserts the text occurs exactly once (`sub`), so a script either applies cleanly or stops naming the file and the text it could not find. Run each once, from the worktree root, in order. If `main` moved after `1085402` (U9/U10 may land first), an anchor can move: re-apply that `sub` by hand at the same place, never elsewhere; the unit tests' line numbers in the expected output move with `test_fight.c`, and the E2 table is regenerated, never merged.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/game/fighter.c`, `fighter.h` | the 24 functions (appended after `fighter_3f174`), their prototypes; the seams of `0x34D8C 0x18C14 0x18AF8 0x39834 0x39A10 0x3C208 0x3C358 0x36870` (and `0x22404`'s own); `hit_anim_start_a`, `fighter_18af8`, `fighter_39834` made non-static |
| `port/src/game/actors.c` | the registrations in `actors_init` (after P1's `0x3F174`), three `anim_code_` wrappers |
| `port/src/mem.h`, `port/src/mem.c` | `PR_SEAM0`; `fn_register` skips a repeated identical pair |
| `port/tests/diff_runner.c` | the bindings and mutants (`b_*`/`m_*`, `k_bindings`) |
| `tools/diff_emu.py` | `deref_arg` (`[reg+N]` call arguments); `RESOLVED_JUMPS[0x227BC]` |
| `tools/diff_verify.py` | `P2_SPECS` and the new callee declarations; the coverage scans follow `RESOLVED_JUMPS` |
| `tools/tests/test_diff_verify.py` | `P2_MASKS`, `P2_KINDS`; the exact-set assertions extended; `test_each_p2_mutant_is_caught_by_what_it_breaks`; `DerefArgTests`; the counter line |
| `port/tests/test_fight.c`, `port/tests/test_platform.c`, `port/tests/test.h` | `test_p2_guarded`, `test_p2_reactions_3`, `test_p2_unconditional`, `test_p2_arming`, `test_p2_hooks`, `test_p2_1c`, `test_p2_0c`; `test_fn_register_repeats`; the RA miss set |
| `Makefile` | `GP_MODES_RA_MIN_FIRST` / `GP_MODES_RA_TRACE_MIN_FIRST` and their provenance |
| `docs/superpowers/plans/2026-10-01-reverse-e2-triage.md` | regenerated in Tasks 2-8 |
| `AGENTS.md`, `docs/PROGRESS.md`, the record | Task 9 |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes `main` at the worktree's head; produces the baseline log `/tmp/pr_p2_base.log`.

- [ ] **Step 1: the full gate on the untouched tree.**

```bash
T=p2; make verify SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att \
  FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 \
  GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md E2_IMAGE=/tmp/pr_${T}_e2.bin \
  > /tmp/pr_p2_base.log 2>&1; echo "EXIT=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_p2_base.log \
  | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
grep -E '^diff-verify:|^entry-triage: (targets|voice)|ratchet N' /tmp/pr_p2_base.log | sed 's/^gp_compare: //'
python3 tools/port_progress.py
```

Expected (at `1085402`): `EXIT=0`, `ORACLES-EQUAL`, and

```
diff-verify: 30/30 functions VERIFIED; 47/47 mutants detected; 1 named gaps; 1/18 rows with callees closed (12 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 313 unported, 182 ported; supplement 131 (28 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 40 in unported code, 75 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

with every gp ratchet line `ok` at its Makefile pin (`gp-u8-right-arcade`: `first unexplained 726, ratchet N 726 ok` and `... ratchet N 1978 ok`). If `main` moved, record the actual lines in the ledger; every later "expected" counter then adds this plan's increments (rows +7, +5, +3, +2, +2, +3, +2; mutants +8, +5, +4, +3, +5, +6, +6) to the baseline's figures.

- [ ] **Step 2: the WAV.** `make audio-render AUDIO_WAV=/tmp/pr_p2.wav >/dev/null 2>&1; cmp /tmp/pr_p2.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-SAME` prints `WAV-SAME`.

---

### Task 2: the guard-shaped move callbacks `0x237D0 0x2381C 0x3DADC 0x3DB34 0x3D10C 0x22A00` and `0x229FC`

**Files:** modify `tools/diff_verify.py` (after `P1_ANIM_SPECS`, and the `SPECS` line), `tools/tests/test_diff_verify.py` (after `P1_KINDS`; the exact-set assertions; a new test before `test_each_stub_declares_the_registers_its_callee_clobbers`; the counter line), `port/tests/test_fight.c` (append after `test_p1_anim_targets`), `port/tests/test.h`, `port/src/game/fighter.c` (append after `fighter_3f174`), `port/src/game/fighter.h`, `port/src/game/actors.c` (`actors_init`, after `fn_register(0x3F174u, ...)`), `port/tests/diff_runner.c`, the E2 table.

**Interfaces:** produces `void fighter_237d0(u32 slot, u32 rec, u32 side)` and the same signature for `fighter_2381c`, `fighter_3dadc`, `fighter_3db34`, `fighter_3d10c`, `fighter_22a00`, `fighter_229fc` (as `0x34E2C`/`0x3531C` call them: no return value read); `P2_SEED`, `p2_guarded`, `P2_SPECS` (diff_verify); `P2_MASKS`, `P2_KINDS` (tests); `p2_cb_fn`, `p2_check_guarded` (test_fight.c). Consumes `hit_anim_start_b` (seam `0x3C4CC`), `sound_voice` (seam `0x2C3FC`), E3's `HIT_B`, `VOICE`, `E3_SLOT`, `E3_REC`; `z_fseed`, `u6b_run`, `Z_S0/Z_S1/Z_R0/Z_R1` of `test_fight.c`. Record §P2.2, §P2.3.

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


# tools/diff_verify.py: the batch's specs after P1's, and in SPECS
sub("tools/diff_verify.py", '''        for i, rec in enumerate((E3_REC, E3_REC2))
    ], eax_mask=0),
]
''', '''        for i, rec in enumerate((E3_REC, E3_REC2))
    ], eax_mask=0),
]

# ---- track P batch 2: the move callbacks 0x14EF8..0x3DCEC and the callbacks they store (record
# 2026-10-02-reverse-p2) --------------------------------------------------------------------------------
# A move callback runs as 0x34E2C calls it at 0x35045: EAX = slot, EDX = rec, EBX = side. Mask 0: 0x34E2C
# returns the callback's EAX to 0x352CD (whose 0x350D0 returns to 0x3531C, whose only caller 0x35803 loads
# `mov eax,ebx`) and to 0x3CF2E (`mov al,1` at 0x3CF33; 0x3CE58's two callers read AL alone, `mov dl,al`
# at 0x3CF84/0x3D03A): no caller reads it (record §P2.2). Every slot field the function writes carries a
# sentinel that differs from what it writes; +0x5F (copied to +0x64) takes 0x22 and 0x80.
P2_SEED = {E3_SLOT + 0x0C: le32(0x0C0C0C0C), E3_SLOT + 0x18: le32(0x18181818), E3_SLOT + 0x1C: le32(0x1C1C1C1C),
           E3_SLOT + 0x52: b"\\x52\\x53\\x54\\x55\\x56\\x57", E3_SLOT + 0x5F: b"\\x22", E3_SLOT + 0x64: b"\\x64"}


def p2_guarded(name, entry, voice, mutants=("@mutant",), extra=None):
    """A guard-shaped move callback (record §P2.3): `cmp dword [slot+8],0; je` else AL = 0 and nothing
    written. g0: slot+8 = 0x01000000 (non-zero in its high byte only, so a byte test runs the body); g1:
    the body, +0x5F 0x22; g2: the body, +0x5F 0x80, side 1, the voice stub's AL = 0 (`mov al,1` overwrites
    it). `extra(i)` adds pokes per case."""
    ex = extra or (lambda i: {})
    return Spec(name, entry, [
        Case("g0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P2_SEED, E3_SLOT + 8: le32(0x01000000), **ex(0)}),
        Case("g1", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P2_SEED, E3_SLOT + 8: le32(0), **ex(1)}),
        Case("g2", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 1},
             {**P2_SEED, E3_SLOT + 8: le32(0), E3_SLOT + 0x5F: b"\\x80", **ex(2)}, {0x2C3FC: 0} if voice else {}),
    ], calls=(HIT_B, VOICE) if voice else (HIT_B,), eax_mask=0, mutants=mutants)


# 0x3D10C writes the word DS_001080AC[rec+0x51] (0x3D170): rec+0x51 is 0 in g0/g1 and 1 in g2, both words
# seeded with sentinels.
def p2_3d10c_extra(i):
    return {E3_REC + 0x51: bytes([1 if i == 2 else 0]), 0x1080AC: b"\\xac\\xac\\xae\\xae"}


P2_SPECS = [
    p2_guarded("fighter_237d0", 0x237D0, True, ("@mutant", "@guard")),
    p2_guarded("fighter_2381c", 0x2381C, True),
    p2_guarded("fighter_3dadc", 0x3DADC, True),
    p2_guarded("fighter_3db34", 0x3DB34, True),
    p2_guarded("fighter_3d10c", 0x3D10C, True, extra=p2_3d10c_extra),
    p2_guarded("fighter_22a00", 0x22A00, False),
    # 0x229FC is the `ret` that ends 0x229E8 (0x229FC: c3), the +0x0C callback 0x22A00 stores: nothing at all.
    Spec("fighter_229fc", 0x229FC, [
        Case("r0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x57: b"\\x57"}),
    ], eax_mask=0),
]
''')
sub("tools/diff_verify.py", "] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS\n",
    "] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS\n")

# tools/tests/test_diff_verify.py: the batch's rows, masks and mutant kinds, extending the exact-set assertions
T = "tools/tests/test_diff_verify.py"
sub(T, '''            "fighter_23868@mutant": {"call #0"}, "fighter_3f174@mutant": {"byte"}}
''', '''            "fighter_23868@mutant": {"call #0"}, "fighter_3f174@mutant": {"byte"}}

# Track P batch 2 (record 2026-10-02-reverse-p2): its rows with their EAX masks, and what alone catches each
# of its mutants.
P2_MASKS = {"fighter_237d0": 0, "fighter_2381c": 0, "fighter_3dadc": 0, "fighter_3db34": 0, "fighter_3d10c": 0,
            "fighter_22a00": 0, "fighter_229fc": 0}
P2_KINDS = {"fighter_237d0@mutant": {"call #0"}, "fighter_237d0@guard": {"byte", "call #0", "call #1"},
            "fighter_2381c@mutant": {"call #1"}, "fighter_3dadc@mutant": {"call #1 memory"},
            "fighter_3db34@mutant": {"call #0"}, "fighter_3d10c@mutant": {"call #0", "call #1"},
            "fighter_22a00@mutant": {"call #0 memory"}, "fighter_229fc@mutant": {"byte"}}
''')
sub(T, '''                                             "host_1b890", "rng_next"] + list(P1_MASKS)))''',
    '''                                             "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)))''')
sub(T, '''            + list(P1_KINDS)))''', '''            + list(P1_KINDS) + list(P2_KINDS)))''')
sub(T, '''            **P1_MASKS})''', '''            **P1_MASKS, **P2_MASKS})''')
sub(T, '''    def test_each_stub_declares_the_registers_its_callee_clobbers(self):''',
    '''    def test_each_p2_mutant_is_caught_by_what_it_breaks(self):
        # track P batch 2 (record 2026-10-02-reverse-p2): what alone catches each mutant; every row with a
        # callee has one that only the call list or the memory at a call catches
        for name, want in P2_KINDS.items():
            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
            self.assertEqual(got, want, name)
        # the guard's high byte (case g0, slot+8 = 0x01000000) is the only case that tells a dword test from
        # a byte test
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_237d0@guard"].problems}), ["g0"])

    def test_each_stub_declares_the_registers_its_callee_clobbers(self):''')
sub(T, '''        self.assertIn("diff-verify: 30/30 functions VERIFIED; 47/47 mutants detected; 1 named gaps; "
                      "1/18 rows with callees closed (12 have none).", out.getvalue())''',
    '''        self.assertIn("diff-verify: 37/37 functions VERIFIED; 55/55 mutants detected; 1 named gaps; "
                      "2/24 rows with callees closed (13 have none).", out.getvalue())''')
print("t2_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p2_img.bin --function fighter_237d0 | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected (the binding does not exist yet):

```
t2_spec applied
| fighter_237d0 | 0x237D0 | 3 | 3/3 | MISMATCH | 2C3FC stub unverified, 3C4CC stub unverified |
  fighter_237d0: g0: port: unknown binding fighter_237d0
  fighter_237d0: g1: port: unknown binding fighter_237d0
  fighter_237d0: g2: port: unknown binding fighter_237d0
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
/* ---- track P batch 2 (record 2026-10-02-reverse-p2-derivations.md) ----------
 * The move callbacks 0x14EF8..0x3DCEC and the callbacks they store.
 * Differential verification (tools/diff_verify.py, P2_SPECS) is the
 * behavioural oracle; these checks pin what it does not see: the
 * registrations and the image dwords that make 0x34E2C, 0x3531C, 0x19020 and
 * 0x193B0 reach each function, and one seeded run through each registration
 * with sentinels on every store. */

typedef void (*p2_cb_fn)(u32 slot, u32 rec, u32 side);

/* §P2.3: one guard-shaped move callback through its registration, as 0x34E2C
 * calls it (slot 0, its record, side 0): with slot+8 = 0x01000000 nothing is
 * written; with it clear the stream head is patched to a plain frame id
 * (0x2BC30 stops on it) and every slot field it stores carries a sentinel.
 * arm57: 0x22A00's shape (+0x57 = 0, +0x5F kept); clears18: +0x18/+0x1C = 0. */
static void p2_check_guarded(u32 addr, void (*fn)(void), u32 table_dw, u32 stream, u32 frame,
                             u32 st52, u32 st53, u32 cb0c, int arm57, int clears18, u32 voice)
{
    p2_cb_fn f;
    CHECK(fn_resolve(addr) == fn, "the move callback is registered");
    CHECK_EQ_INT((int)DSD(table_dw), (int)addr);
    f = (p2_cb_fn)(void *)fn_resolve(addr);
    if (f == NULL) return;
    z_fseed();
    DSW(stream) = 0x12B1u;
    DSD(Z_S0 + 8u) = 0x01000000u;
    DSB(Z_S0 + 0x53u) = 0x33u;
    DSB(Z_S0 + 0x5Fu) = 0x22u;
    sound_voice_log_reset();
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 0x33);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x5Fu), 0x22);
    CHECK_EQ_INT((int)sound_voice_log_count(), 0);
    z_fseed();
    DSW(stream) = 0x12B1u;
    DSD(Z_S0 + 8u) = 0u;
    DSB(Z_S0 + 0x54u) = 0x44u;
    DSB(Z_S0 + 0x57u) = 0x77u;
    DSB(Z_S0 + 0x5Fu) = 0x22u;
    DSB(Z_S0 + 0x64u) = 0x64u;
    DSD(Z_S0 + 0x0Cu) = 0x0C0C0C0Cu;
    DSD(Z_S0 + 0x18u) = 0x18181818u;
    DSD(Z_S0 + 0x1Cu) = 0x1C1C1C1Cu;
    sound_voice_log_reset();
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), (int)stream);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), (int)frame);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), (int)st52);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), (int)st53);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x0Cu), (int)cb0c);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x64u), 0x22);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), arm57 ? 0 : 0x77);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x5Fu), arm57 ? 0x22 : 0xFF);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x18u), clears18 ? 0 : 0x18181818);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x1Cu), clears18 ? 0 : 0x1C1C1C1C);
    CHECK_EQ_INT((int)sound_voice_log_count(), voice ? 1 : 0);
    if (voice) CHECK_EQ_INT((int)sound_voice_log_at(0), (int)voice);
    sound_voice_log_reset();
}

static void p2_check_guarded_all(void)
{
    p2_cb_fn f;
    p2_check_guarded(0x237D0u, (void (*)(void))fighter_237d0, 0x000A5620u, 0x000E14D8u, 0x40400000u,
                     0x0Bu, 6u, 0u, 0, 0, 0xAAu);
    p2_check_guarded(0x2381Cu, (void (*)(void))fighter_2381c, 0x000A560Cu, 0x000E1506u, 0x40400000u,
                     0x0Bu, 6u, 0u, 0, 0, 0xAAu);
    p2_check_guarded(0x3DADCu, (void (*)(void))fighter_3dadc, 0x000A50F8u, 0x000D4AB2u, 0x40400000u,
                     0x0Bu, 6u, 0u, 0, 1, 0xB8u);
    p2_check_guarded(0x3DB34u, (void (*)(void))fighter_3db34, 0x000A510Cu, 0x000D4AFAu, 0x40400000u,
                     0x0Bu, 6u, 0u, 0, 1, 0xB8u);
    p2_check_guarded(0x3D10Cu, (void (*)(void))fighter_3d10c, 0x000A37BCu, 0x000E84C8u, 0x40400000u,
                     0x0Bu, 6u, 0u, 0, 1, 0x91u);
    p2_check_guarded(0x22A00u, (void (*)(void))fighter_22a00, 0x000A55A8u, 0x000E1534u, 0x40400000u,
                     9u, 7u, 0x000229FCu, 1, 0, 0u);
    /* 0x3D10C's word DS_001080AC[rec+0x51] (0x3D170): side 1's word only. */
    z_fseed();
    DSW(0x000E84C8u) = 0x12B1u;
    DSD(Z_S1 + 8u) = 0u;
    DSW(0x001080ACu) = 0xACACu;
    DSW(0x001080AEu) = 0xAEAEu;
    fighter_3d10c(Z_S1, Z_R1, 1u);
    CHECK_EQ_INT((int)DSW(0x001080ACu), 0xACAC);
    CHECK_EQ_INT((int)DSW(0x001080AEu), 0x0080);
    /* 0x229FC, the +0x0C callback 0x22A00 stores (the dword at 0x22A2D), is
     * the `ret` that ends 0x229E8: 0x3531C case 7 reaches it and nothing
     * changes. */
    CHECK_EQ_INT((int)DSD(0x00022A2Du), 0x000229FC);
    CHECK_EQ_INT((int)DSB(0x000229FCu), 0xC3);
    CHECK(fn_resolve(0x229FCu) == (void (*)(void))fighter_229fc, "0x229FC is registered");
    f = (p2_cb_fn)(void *)fn_resolve(0x229FCu);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_S0 + 0x57u) = 0x57u;
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 0x57);
}

int test_p2_guarded(void)       { return u6b_run(p2_check_guarded_all); }
'''

ANCHOR = "int test_p1_anim_targets(void)  { return u6b_run(p1_check_anim_targets); }\n"
sub("port/tests/test_fight.c", ANCHOR, ANCHOR + TEST)
sub("port/tests/test.h", "    X(test_p1_anim_targets) \\\n", "    X(test_p1_anim_targets) \\\n    X(test_p2_guarded) \\\n")
print("t2_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected:

```
t2_test applied
port/tests/test_fight.c:45059:48: error: use of undeclared identifier 'fighter_237d0'
port/tests/test_fight.c:45061:48: error: use of undeclared identifier 'fighter_2381c'
port/tests/test_fight.c:45063:48: error: use of undeclared identifier 'fighter_3dadc'
port/tests/test_fight.c:45065:48: error: use of undeclared identifier 'fighter_3db34'
port/tests/test_fight.c:45067:48: error: use of undeclared identifier 'fighter_3d10c'
port/tests/test_fight.c:45069:48: error: use of undeclared identifier 'fighter_22a00'
port/tests/test_fight.c:45077:5: error: call to undeclared function 'fighter_3d10c'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]
port/tests/test_fight.c:45085:51: error: use of undeclared identifier 'fighter_229fc'
8 errors generated.
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


FIGHTER_C = r'''
/* ---- track P batch 2: the move callbacks and the callbacks they store -------
 * Record 2026-10-02-reverse-p2-derivations.md. 0x34E2C calls a move callback
 * (the +0 dword of a 0xA3528 move-table entry) at 0x35045 as (EAX = slot,
 * EDX = rec, EBX = side), and 0x3531C case 7 (0x35431) a slot +0x0C callback
 * with the same registers; no caller reads the EAX either returns (record
 * §P2.2), so the port's callbacks return nothing. */
#define P2_ANIM_237D0 0x000E14D8u  /* 0x237DF */
#define P2_ANIM_2381C 0x000E1506u  /* 0x2382B */
#define P2_ANIM_3DADC 0x000D4AB2u  /* 0x3DAEB */
#define P2_ANIM_3DB34 0x000D4AFAu  /* 0x3DB43 */
#define P2_ANIM_3D10C 0x000E84C8u  /* 0x3D128 */
#define P2_ANIM_22A00 0x000E1534u  /* 0x22A0F */
#define P2_1080AC     0x001080ACu  /* 0x3D170: a word per side (0x3D17C's too) */

/* 0x237D0 — record §P2.3. Character 6's reaction-0x26 callback (the dword at
 * 0xA5620). With the slot's +8 clear: the record on 0xE14D8 at 3.0 (0x3C4CC),
 * the slot 0xB/6/0, +0x0C = 0, +0x64 = +0x5F, +0x5F = 0xFF, the voice 0xAA.
 * PORT: the raw returns AL = 0 (0x237DB) or 1 (0x23815); unread (§P2.2). */
void fighter_237d0(u32 slot, u32 rec, u32 side)
{
    u8 r5f;
    (void)side;
    if (DSD(slot + 8u) != 0u) return;                       /* 0x237D5/0x237D9 */
    hit_anim_start_b(rec, P2_ANIM_237D0, 0x40400000u);      /* 0x237DF..0x237E9 0x3C4CC */
    DSB(slot + 0x52u) = 0x0Bu;                              /* 0x237EE */
    DSB(slot + 0x53u) = 6u;                                 /* 0x237F2 */
    DSB(slot + 0x54u) = 0u;                                 /* 0x237F6 */
    r5f = DSB(slot + 0x5Fu);                                /* 0x237FA */
    DSD(slot + 0x0Cu) = 0u;                                 /* 0x237FD */
    DSB(slot + 0x64u) = r5f;                                /* 0x23804 */
    DSB(slot + 0x5Fu) = 0xFFu;                              /* 0x2380C */
    (void)sound_voice(0xAAu);                               /* 0x23807/0x23810 0x2C3FC */
}

/* 0x2381C — record §P2.3. Character 6's reaction-0x25 callback (the dword at
 * 0xA560C): 0x237D0's shape on the stream 0xE1506. PORT: AL unread (§P2.2). */
void fighter_2381c(u32 slot, u32 rec, u32 side)
{
    u8 r5f;
    (void)side;
    if (DSD(slot + 8u) != 0u) return;                       /* 0x23821/0x23825 */
    hit_anim_start_b(rec, P2_ANIM_2381C, 0x40400000u);      /* 0x2382B..0x23835 0x3C4CC */
    DSB(slot + 0x52u) = 0x0Bu;                              /* 0x2383A */
    DSB(slot + 0x53u) = 6u;                                 /* 0x2383E */
    DSB(slot + 0x54u) = 0u;                                 /* 0x23842 */
    r5f = DSB(slot + 0x5Fu);                                /* 0x23846 */
    DSD(slot + 0x0Cu) = 0u;                                 /* 0x23849 */
    DSB(slot + 0x64u) = r5f;                                /* 0x23850 */
    DSB(slot + 0x5Fu) = 0xFFu;                              /* 0x23858 */
    (void)sound_voice(0xAAu);                               /* 0x23853/0x2385C 0x2C3FC */
}

/* 0x3DADC — record §P2.3. Character 5's reaction-0x24 callback (the dword at
 * 0xA50F8): the record on 0xD4AB2 at 3.0, the slot 0xB/6/0, +0x0C = +0x18 =
 * +0x1C = 0, +0x64 = +0x5F, +0x5F = 0xFF, the voice 0xB8. PORT: AL unread. */
void fighter_3dadc(u32 slot, u32 rec, u32 side)
{
    u8 r5f;
    (void)side;
    if (DSD(slot + 8u) != 0u) return;                       /* 0x3DAE1/0x3DAE5 */
    hit_anim_start_b(rec, P2_ANIM_3DADC, 0x40400000u);      /* 0x3DAEB..0x3DAF5 0x3C4CC */
    DSB(slot + 0x52u) = 0x0Bu;                              /* 0x3DAFA */
    DSB(slot + 0x53u) = 6u;                                 /* 0x3DAFE */
    DSB(slot + 0x54u) = 0u;                                 /* 0x3DB02 */
    DSD(slot + 0x0Cu) = 0u;                                 /* 0x3DB06 */
    DSD(slot + 0x18u) = 0u;                                 /* 0x3DB0D */
    r5f = DSB(slot + 0x5Fu);                                /* 0x3DB14 */
    DSD(slot + 0x1Cu) = 0u;                                 /* 0x3DB17 */
    DSB(slot + 0x64u) = r5f;                                /* 0x3DB1E */
    DSB(slot + 0x5Fu) = 0xFFu;                              /* 0x3DB26 */
    (void)sound_voice(0xB8u);                               /* 0x3DB21/0x3DB2A 0x2C3FC */
}

/* 0x3DB34 — record §P2.3. Character 5's reaction-0x25 callback (the dword at
 * 0xA510C): 0x3DADC's shape on the stream 0xD4AFA. PORT: AL unread. */
void fighter_3db34(u32 slot, u32 rec, u32 side)
{
    u8 r5f;
    (void)side;
    if (DSD(slot + 8u) != 0u) return;                       /* 0x3DB39/0x3DB3D */
    hit_anim_start_b(rec, P2_ANIM_3DB34, 0x40400000u);      /* 0x3DB43..0x3DB4D 0x3C4CC */
    DSB(slot + 0x52u) = 0x0Bu;                              /* 0x3DB52 */
    DSB(slot + 0x53u) = 6u;                                 /* 0x3DB56 */
    DSB(slot + 0x54u) = 0u;                                 /* 0x3DB5A */
    DSD(slot + 0x0Cu) = 0u;                                 /* 0x3DB5E */
    DSD(slot + 0x18u) = 0u;                                 /* 0x3DB65 */
    r5f = DSB(slot + 0x5Fu);                                /* 0x3DB6C */
    DSD(slot + 0x1Cu) = 0u;                                 /* 0x3DB6F */
    DSB(slot + 0x64u) = r5f;                                /* 0x3DB76 */
    DSB(slot + 0x5Fu) = 0xFFu;                              /* 0x3DB7E */
    (void)sound_voice(0xB8u);                               /* 0x3DB79/0x3DB82 0x2C3FC */
}

/* 0x3D10C — record §P2.3. Character 0's reaction-0x21 callback (the dword at
 * 0xA37BC); 0x3D17C's shape with the word 0x80. ESI = rec+0x51 (movzx,
 * 0x3D113, before the guard). With the slot's +8 clear: the voice 0x91, then
 * the record on 0xE84C8 at 3.0 (EDX loaded at 0x3D128, before the voice, which
 * preserves it), the slot 0xB/6/0, +0x0C = +0x18 = +0x1C = 0, +0x64 = +0x5F,
 * +0x5F = 0xFF and DS_001080AC[side] = 0x80. PORT: AL unread (§P2.2). */
void fighter_3d10c(u32 slot, u32 rec, u32 side)
{
    u32 i = (u32)DSB(rec + 0x51u);                          /* 0x3D113 movzx */
    u8 r5f;
    (void)side;
    if (DSD(slot + 8u) != 0u) return;                       /* 0x3D117/0x3D11B */
    (void)sound_voice(0x91u);                               /* 0x3D123/0x3D12D 0x2C3FC */
    hit_anim_start_b(rec, P2_ANIM_3D10C, 0x40400000u);      /* 0x3D132..0x3D139 0x3C4CC */
    DSB(slot + 0x52u) = 0x0Bu;                              /* 0x3D13E */
    DSB(slot + 0x53u) = 6u;                                 /* 0x3D142 */
    DSB(slot + 0x54u) = 0u;                                 /* 0x3D146 */
    DSD(slot + 0x0Cu) = 0u;                                 /* 0x3D14A */
    DSD(slot + 0x18u) = 0u;                                 /* 0x3D151 */
    DSD(slot + 0x1Cu) = 0u;                                 /* 0x3D158 */
    r5f = DSB(slot + 0x5Fu);                                /* 0x3D15F */
    DSB(slot + 0x5Fu) = 0xFFu;                              /* 0x3D162 */
    DSB(slot + 0x64u) = r5f;                                /* 0x3D16B */
    DSW(P2_1080AC + i * 2u) = 0x0080u;                      /* 0x3D166/0x3D170 */
}

/* 0x22A00 — record §P2.3. Character 6's reaction-0x20 callback (the dword at
 * 0xA55A8): with the slot's +8 clear, the record on 0xE1534 at 3.0, the slot
 * 9/7/0 with the +0x0C callback 0x229FC (0x3531C case 7), +0x57 = 0, +0x64 =
 * +0x5F; no voice. PORT: AL unread (§P2.2). */
void fighter_22a00(u32 slot, u32 rec, u32 side)
{
    (void)side;
    if (DSD(slot + 8u) != 0u) return;                       /* 0x22A05/0x22A09 */
    hit_anim_start_b(rec, P2_ANIM_22A00, 0x40400000u);      /* 0x22A0F..0x22A19 0x3C4CC */
    DSB(slot + 0x52u) = 9u;                                 /* 0x22A1E */
    DSB(slot + 0x53u) = 7u;                                 /* 0x22A22 */
    DSB(slot + 0x54u) = 0u;                                 /* 0x22A26 */
    DSD(slot + 0x0Cu) = 0x000229FCu;                        /* 0x22A2A */
    DSB(slot + 0x57u) = 0u;                                 /* 0x22A34 */
    DSB(slot + 0x64u) = DSB(slot + 0x5Fu);                  /* 0x22A31/0x22A38 */
}

/* 0x229FC — record §P2.3. The `ret` (c3) that ends 0x229E8, which 0x22A00
 * stores as the slot's +0x0C callback (the dword at 0x22A2D): 0x3531C case 7
 * calls it every frame and it does nothing. */
void fighter_229fc(u32 slot, u32 rec, u32 side)
{
    (void)slot;
    (void)rec;
    (void)side;
}
'''

BINDINGS = r'''/* Track P batch 2 (record 2026-10-02-reverse-p2 §P2.2): the move callbacks as 0x34E2C calls them at
 * 0x35045, and the slot +0x0C callbacks as 0x3531C case 7 calls them at 0x35431 (EAX = slot, EDX = rec,
 * EBX = side). Mask 0: no caller reads the EAX they return. */
static void b_237d0(const u32 *r, u32 *eax)            { fighter_237d0(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_2381c(const u32 *r, u32 *eax)            { fighter_2381c(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_3dadc(const u32 *r, u32 *eax)            { fighter_3dadc(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_3db34(const u32 *r, u32 *eax)            { fighter_3db34(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_3d10c(const u32 *r, u32 *eax)            { fighter_3d10c(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_22a00(const u32 *r, u32 *eax)            { fighter_22a00(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_229fc(const u32 *r, u32 *eax)            { fighter_229fc(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_237d0(const u32 *r, u32 *eax)            /* 0x2381C's stream 0xE1506 */
{
    u32 slot = r[R_EAX];
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    hit_anim_start_b(r[R_EDX], 0x000E1506u, 0x40400000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSD(slot + 0x0Cu) = 0u;
    DSB(slot + 0x64u) = r5f;
    DSB(slot + 0x5Fu) = 0xFFu;
    (void)sound_voice(0xAAu);
}
static void m_237d0_guard(const u32 *r, u32 *eax)      /* the guard tests the low byte of +8 */
{
    u32 slot = r[R_EAX];
    u8 r5f;
    *eax = 0u;
    if (DSB(slot + 8u) != 0u) return;
    hit_anim_start_b(r[R_EDX], 0x000E14D8u, 0x40400000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSD(slot + 0x0Cu) = 0u;
    DSB(slot + 0x64u) = r5f;
    DSB(slot + 0x5Fu) = 0xFFu;
    (void)sound_voice(0xAAu);
}
static void m_2381c(const u32 *r, u32 *eax)            /* the voice 0xB8 (character 5's) */
{
    u32 slot = r[R_EAX];
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    hit_anim_start_b(r[R_EDX], 0x000E1506u, 0x40400000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSD(slot + 0x0Cu) = 0u;
    DSB(slot + 0x64u) = r5f;
    DSB(slot + 0x5Fu) = 0xFFu;
    (void)sound_voice(0xB8u);
}
static void m_3dadc(const u32 *r, u32 *eax)            /* +0x64/+0x5F stored after the voice */
{
    u32 slot = r[R_EAX];
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    hit_anim_start_b(r[R_EDX], 0x000D4AB2u, 0x40400000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0u;
    DSD(slot + 0x18u) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSD(slot + 0x1Cu) = 0u;
    (void)sound_voice(0xB8u);
    DSB(slot + 0x64u) = r5f;
    DSB(slot + 0x5Fu) = 0xFFu;
}
static void m_3db34(const u32 *r, u32 *eax)            /* the frame 2.0, not 3.0 */
{
    u32 slot = r[R_EAX];
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    hit_anim_start_b(r[R_EDX], 0x000D4AFAu, 0x40000000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0u;
    DSD(slot + 0x18u) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSD(slot + 0x1Cu) = 0u;
    DSB(slot + 0x64u) = r5f;
    DSB(slot + 0x5Fu) = 0xFFu;
    (void)sound_voice(0xB8u);
}
static void m_3d10c(const u32 *r, u32 *eax)            /* the record started before the voice */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u32 i = (u32)DSB(rec + 0x51u);
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    hit_anim_start_b(rec, 0x000E84C8u, 0x40400000u);
    (void)sound_voice(0x91u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSB(slot + 0x5Fu) = 0xFFu;
    DSB(slot + 0x64u) = r5f;
    DSW(0x001080ACu + i * 2u) = 0x0080u;
}
static void m_22a00(const u32 *r, u32 *eax)            /* the slot stores before the 0x3C4CC call */
{
    u32 slot = r[R_EAX];
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0x000229FCu;
    DSB(slot + 0x57u) = 0u;
    DSB(slot + 0x64u) = DSB(slot + 0x5Fu);
    hit_anim_start_b(r[R_EDX], 0x000E1534u, 0x40400000u);
}
static void m_229fc(const u32 *r, u32 *eax)            /* steps the slot's +0x57 */
{
    DSB(r[R_EAX] + 0x57u) = (u8)(DSB(r[R_EAX] + 0x57u) + 1u);
    *eax = 0u;
}

'''

sub("port/src/game/fighter.c",
    "    DSW(rec + 0x44u) = 0x0020u;                             /* 0x3F17A */\n}\n",
    "    DSW(rec + 0x44u) = 0x0020u;                             /* 0x3F17A */\n}\n" + FIGHTER_C)
sub("port/src/game/fighter.h", "void fighter_3f174(u32 rec);\n\n#endif /* PRAGE_GAME_FIGHTER_H */",
    """void fighter_3f174(u32 rec);
/* Track P batch 2 (record 2026-10-02-reverse-p2-derivations.md §P2.3): the
 * guard-shaped move callbacks 0x34E2C calls as (slot, rec, side), and the
 * +0x0C callback 0x22A00 stores (0x3531C case 7, the same registers);
 * registered in actors_init. */
void fighter_237d0(u32 slot, u32 rec, u32 side);
void fighter_2381c(u32 slot, u32 rec, u32 side);
void fighter_3dadc(u32 slot, u32 rec, u32 side);
void fighter_3db34(u32 slot, u32 rec, u32 side);
void fighter_3d10c(u32 slot, u32 rec, u32 side);
void fighter_22a00(u32 slot, u32 rec, u32 side);
void fighter_229fc(u32 slot, u32 rec, u32 side);

#endif /* PRAGE_GAME_FIGHTER_H */""")
sub("port/src/game/actors.c", "    fn_register(0x3F174u, (void (*)(void))anim_code_3F174);\n",
    """    fn_register(0x3F174u, (void (*)(void))anim_code_3F174);
    /* PORT: record 2026-10-02-reverse-p2 §P2.3. The guard-shaped move
     * callbacks (the move-table dwords 0xA5620, 0xA560C, 0xA50F8, 0xA510C,
     * 0xA37BC and 0xA55A8; 0x34E2C at 0x35045, (slot, rec, side)) and the
     * +0x0C callback 0x22A00 stores, 0x229FC (the dword at 0x22A2D, the `ret`
     * that ends 0x229E8; 0x3531C case 7, the same registers). */
    fn_register(0x237D0u, (void (*)(void))fighter_237d0);
    fn_register(0x2381Cu, (void (*)(void))fighter_2381c);
    fn_register(0x3DADCu, (void (*)(void))fighter_3dadc);
    fn_register(0x3DB34u, (void (*)(void))fighter_3db34);
    fn_register(0x3D10Cu, (void (*)(void))fighter_3d10c);
    fn_register(0x22A00u, (void (*)(void))fighter_22a00);
    fn_register(0x229FCu, (void (*)(void))fighter_229fc);
""")
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_3f174@mutant",     m_3f174,        0x00000000u },\n',
    """    { "fighter_3f174@mutant",     m_3f174,        0x00000000u },
    { "fighter_237d0",            b_237d0,        0x00000000u },
    { "fighter_2381c",            b_2381c,        0x00000000u },
    { "fighter_3dadc",            b_3dadc,        0x00000000u },
    { "fighter_3db34",            b_3db34,        0x00000000u },
    { "fighter_3d10c",            b_3d10c,        0x00000000u },
    { "fighter_22a00",            b_22a00,        0x00000000u },
    { "fighter_229fc",            b_229fc,        0x00000000u },
    { "fighter_237d0@mutant",     m_237d0,        0x00000000u },
    { "fighter_237d0@guard",      m_237d0_guard,  0x00000000u },
    { "fighter_2381c@mutant",     m_2381c,        0x00000000u },
    { "fighter_3dadc@mutant",     m_3dadc,        0x00000000u },
    { "fighter_3db34@mutant",     m_3db34,        0x00000000u },
    { "fighter_3d10c@mutant",     m_3d10c,        0x00000000u },
    { "fighter_22a00@mutant",     m_22a00,        0x00000000u },
    { "fighter_229fc@mutant",     m_229fc,        0x00000000u },
""")
print("t2_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_237d0 fighter_2381c fighter_3dadc fighter_3db34 fighter_3d10c fighter_22a00 fighter_229fc; do
  python3 tools/diff_verify.py --image /tmp/pr_p2_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected: no compiler message, then

```
t2_port applied
all checks passed
| fighter_237d0 | 0x237D0 | 3 | 3/3 | VERIFIED | 2C3FC stub unverified, 3C4CC stub unverified |
| fighter_237d0@mutant | 0x237D0 | 3 | 3/3 | MISMATCH | 2C3FC stub unverified, 3C4CC stub unverified |
| fighter_237d0@guard | 0x237D0 | 3 | 3/3 | MISMATCH | 2C3FC stub unverified, 3C4CC stub unverified |
| fighter_2381c | 0x2381C | 3 | 3/3 | VERIFIED | 2C3FC stub unverified, 3C4CC stub unverified |
| fighter_2381c@mutant | 0x2381C | 3 | 3/3 | MISMATCH | 2C3FC stub unverified, 3C4CC stub unverified |
| fighter_3dadc | 0x3DADC | 3 | 3/3 | VERIFIED | 2C3FC stub unverified, 3C4CC stub unverified |
| fighter_3dadc@mutant | 0x3DADC | 3 | 3/3 | MISMATCH | 2C3FC stub unverified, 3C4CC stub unverified |
| fighter_3db34 | 0x3DB34 | 3 | 3/3 | VERIFIED | 2C3FC stub unverified, 3C4CC stub unverified |
| fighter_3db34@mutant | 0x3DB34 | 3 | 3/3 | MISMATCH | 2C3FC stub unverified, 3C4CC stub unverified |
| fighter_3d10c | 0x3D10C | 3 | 3/3 | VERIFIED | 2C3FC stub unverified, 3C4CC stub unverified |
| fighter_3d10c@mutant | 0x3D10C | 3 | 3/3 | MISMATCH | 2C3FC stub unverified, 3C4CC stub unverified |
| fighter_22a00 | 0x22A00 | 3 | 3/3 | VERIFIED | 3C4CC stub unverified |
| fighter_22a00@mutant | 0x22A00 | 3 | 3/3 | MISMATCH | 3C4CC stub unverified |
| fighter_229fc | 0x229FC | 1 | 1/1 | VERIFIED | - |
| fighter_229fc@mutant | 0x229FC | 1 | 1/1 | MISMATCH | - |
```

A row that is not `VERIFIED` is a finding, not something to adjust: stop, compare the C with the bytes (record §P2.3), and report.

- [ ] **Step 4: the E2 table.** `make entry-triage` first fails, then the table is regenerated:

```bash
make entry-triage E2_IMAGE=/tmp/pr_p2_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/pr_p2_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
  --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
grep -E '^\| (callbacks|finishers|animation-targets|stubs) ' docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git diff --stat docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -1
```

Expected:

```
entry-triage: FAIL: docs/superpowers/plans/2026-10-01-reverse-e2-triage.md differs from a fresh run (regenerate it with --out)
make: *** [entry-triage] Error 1
entry-triage: targets 307 unported, 188 ported; supplement 131 (28 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 35 in unported code, 80 in ported code, 19 nowhere
| callbacks | 16 | 55 |
| finishers | 0 | 9 |
| animation-targets | 57 | 55 |
| stubs | 70 |
 1 file changed, 13 insertions(+), 13 deletions(-)
```

- [ ] **Step 5: the task gate.**

```bash
make diff-verify entry-triage DIFF_IMAGE=/tmp/pr_p2_d.bin DIFF_TABLE=/tmp/pr_p2_d.md E2_IMAGE=/tmp/pr_p2_e2.bin 2>&1 \
  | grep -E '^(Ran|OK|FAILED|diff-verify:|entry-triage: targets)'
```

Expected (the times vary):

```
Ran 157 tests in N.NNNs
OK
diff-verify: 37/37 functions VERIFIED; 55/55 mutants detected; 1 named gaps; 2/24 rows with callees closed (13 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
Ran 44 tests in N.NNNs
OK
entry-triage: targets 307 unported, 188 ported; supplement 131 (28 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 6: every new unit assertion can fail.** Four mutations of the code under test, each restored after its run:

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


mutate("port/src/game/actors.c", "    fn_register(0x237D0u, (void (*)(void))fighter_237d0);\n", "")
mutate("port/src/game/actors.c", "    fn_register(0x229FCu, (void (*)(void))fighter_229fc);\n", "")
mutate("port/src/game/fighter.c",
       "    DSW(P2_1080AC + i * 2u) = 0x0080u;                      /* 0x3D166/0x3D170 */",
       "    DSW(P2_1080AC + i * 2u) = 0x0100u;                      /* 0x3D166/0x3D170 */")
mutate("port/src/game/fighter.c",
       "    DSD(slot + 0x0Cu) = 0x000229FCu;                        /* 0x22A2A */",
       "    DSD(slot + 0x0Cu) = 0u;                                 /* 0x22A2A */")
PY
```

Expected (line numbers move with `test_fight.c`), then `all checks passed` again:

```
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:45014: the move callback is registered']
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:45085: 0x229FC is registered']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45079: 256 != 128']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45045: 0 != 141820']
all checks passed
```

- [ ] **Step 7: commit.**

```bash
git add tools/diff_verify.py tools/tests/test_diff_verify.py port/tests/test_fight.c port/tests/test.h \
  port/src/game/fighter.c port/src/game/fighter.h port/src/game/actors.c port/tests/diff_runner.c \
  docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git commit -m "fighter: port the guard-shaped move callbacks 0x237D0 0x2381C 0x3DADC 0x3DB34 0x3D10C 0x22A00 and 0x229FC with rows and mutants; E2 table regenerated (track P batch 2)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: character 3's reactions `0x14EF8`/`0x14F50`, their streams' targets `0x14FA8 0x14FF8 0x150AC`, and U8's right-arcade

**Files:** as Task 2, plus `port/tests/test_platform.c` (`k_miss_gp_u8_right_arcade`) and `Makefile` (`GP_MODES_RA_*`).

**Interfaces:** produces `void fighter_14ef8(u32 slot, u32 rec, u32 side)`, `fighter_14f50` (same), `void fighter_14fa8(u32 rec)`, `void fighter_14ff8(u32 rec)`, `void fighter_150ac(u32 rec)` (registered through `anim_code_14FA8/14FF8/150AC`, which drop the operand); `p2_14fa8`, `p2_held`, `P2_HELD_ROWS` (diff_verify); `p2_run_reaction`, `test_p2_reactions_3` (test_fight.c). Consumes Task 2's `p2_guarded`, `p2_check_guarded`; `actor_spawn` (seam `0x2AE14`, E3's `SPAWN`), `fighter_actor_bit15_clear` (seam `0x1A570`, P1's `BIT15`); `sh_seed`, `c4r_pool` of `test_fight.c`. Record §P2.4, §P2.12.

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


sub("tools/diff_verify.py", '''    Spec("fighter_229fc", 0x229FC, [
        Case("r0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x57: b"\\x57"}),
    ], eax_mask=0),
]
''', '''    Spec("fighter_229fc", 0x229FC, [
        Case("r0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x57: b"\\x57"}),
    ], eax_mask=0),
    # character 3's reactions 0x20 and 0x21: the two callbacks gp-u8-right-arcade reaches (record §P2.4)
    p2_guarded("fighter_14ef8", 0x14EF8, True),
    p2_guarded("fighter_14f50", 0x14F50, True),
]


# The 0xD000 targets (opcode 0x10, mode 0x4000) of the streams 0x14EF8 and 0x14F50 start (record §P2.4): the
# animation dispatcher calls them at 0x2B56D with EAX = rec (`mov eax,esi` 0x2B56B; none of the three reads EDX
# or ECX) and overwrites EAX after (`xor ecx,ecx; mov eax,ecx` 0x2B573/0x2B575): mask 0. The SPAWN stub's EAX
# is the spawned record, a different one per case, seeded with sentinels on every field written there.
def p2_14fa8(cid, side, sp):
    return Case(cid, {"eax": E3_REC, "edx": 0x1234, "ecx": 0x5678},
                {E3_REC + 0x4B: b"\\x4b", E3_REC + 0x51: bytes([side]), E3_REC + 0x56: b"\\x23\\x01",
                 sp + 0x2E: b"\\xfe\\xff", sp + 0x4E: b"\\x4e", sp + 0x56: b"\\x07", sp + 0x59: b"\\x59",
                 sp + 0x60: b"\\x60"}, {0x2AE14: sp})


def p2_held(cid, owner, side, al, w28, x, z, w30, sp):
    """0x14FF8/0x150AC: owner = rec+0x14; side = rec+0x51 (0x1A570's argument and the +0x2E/+0x4E arm); al =
    the 0x1A570 stub's AL; w28 = the word rec+0x28 (bit 14: the spawn flag); x, z = rec+0x18/+0x1C; w30 =
    rec+0x30 (`sar 0x10`: the y argument)."""
    return Case(cid, {"eax": E3_REC, "edx": 0x1234, "ecx": 0x5678},
                {E3_REC + 0x14: le32(owner), E3_REC + 0x18: le32(x) + le32(z), E3_REC + 0x28: le32(w28)[:2],
                 E3_REC + 0x30: le32(w30), E3_REC + 0x51: bytes([side]), E3_SLOT + 8: le32(0x08080808),
                 sp + 0x14: le32(0x14141414), sp + 0x2E: b"\\xfe\\xff", sp + 0x34: b"\\x34\\x34",
                 sp + 0x4E: b"\\x4e", sp + 0x59: b"\\x59"},
                {0x2AE14: sp, 0x1A570: al})


P2_HELD_ROWS = (("h0", 0, 0, 0, 0, 0x5000, 0x30000, 0x70000, E3_OUT),
                ("h1", E3_SLOT, 0, 0, 0, 0x5000, 0x30000, 0x70000, E3_OUT),
                ("h2", E3_SLOT, 1, 1, 0x4000, 0xFFFFF000, 0xFFFD8000, 0xFFFD0000, E3_REC2),
                ("h3", E3_SLOT, 1, 0, 0xBFFF, 0x6000, 0x50000, 0x30000, E3_OUT),
                ("h4", E3_SLOT, 0, 1, 0x4000, 0x7000, 0x10000, 0x20000, E3_REC2))
P2_SPECS += [
    Spec("fighter_14fa8", 0x14FA8, [p2_14fa8("f0", 0, E3_OUT), p2_14fa8("f1", 1, E3_REC2)],
         calls=(SPAWN,), eax_mask=0),
    Spec("fighter_14ff8", 0x14FF8, [p2_held(*r) for r in P2_HELD_ROWS], calls=(BIT15, SPAWN), eax_mask=0),
    Spec("fighter_150ac", 0x150AC, [p2_held(*r) for r in P2_HELD_ROWS], calls=(BIT15, SPAWN), eax_mask=0),
]
''')
T = "tools/tests/test_diff_verify.py"
sub(T, '''            "fighter_22a00": 0, "fighter_229fc": 0}''',
    '''            "fighter_22a00": 0, "fighter_229fc": 0, "fighter_14ef8": 0, "fighter_14f50": 0,
            "fighter_14fa8": 0, "fighter_14ff8": 0, "fighter_150ac": 0}''')
sub(T, '''            "fighter_22a00@mutant": {"call #0 memory"}, "fighter_229fc@mutant": {"byte"}}''',
    '''            "fighter_22a00@mutant": {"call #0 memory"}, "fighter_229fc@mutant": {"byte"},
            "fighter_14ef8@mutant": {"call #0"}, "fighter_14f50@mutant": {"call #0"},
            "fighter_14fa8@mutant": {"call #0"}, "fighter_14ff8@mutant": {"call #1"},
            "fighter_150ac@mutant": {"call #1"}}''')
sub(T, '''        self.assertIn("diff-verify: 37/37 functions VERIFIED; 55/55 mutants detected; 1 named gaps; "
                      "2/24 rows with callees closed (13 have none).", out.getvalue())''',
    '''        self.assertIn("diff-verify: 42/42 functions VERIFIED; 60/60 mutants detected; 1 named gaps; "
                      "2/29 rows with callees closed (13 have none).", out.getvalue())''')
print("t3_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p2_img.bin --function fighter_14ff8 | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected:

```
t3_spec applied
| fighter_14ff8 | 0x14FF8 | 5 | 9/9 | MISMATCH | 1A570 stub unverified, 2AE14 stub unverified |
  fighter_14ff8: h0: port: unknown binding fighter_14ff8
  fighter_14ff8: h1: port: unknown binding fighter_14ff8
  fighter_14ff8: h2: port: unknown binding fighter_14ff8
  fighter_14ff8: h3: port: unknown binding fighter_14ff8
  fighter_14ff8: h4: port: unknown binding fighter_14ff8
diff-verify: 0/1 functions VERIFIED; 0 named gaps; 0/1 rows with callees closed (0 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
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


sub("port/tests/test_fight.c", '''    p2_check_guarded(0x22A00u, (void (*)(void))fighter_22a00, 0x000A55A8u, 0x000E1534u, 0x40400000u,
                     9u, 7u, 0x000229FCu, 1, 0, 0u);
''', '''    p2_check_guarded(0x22A00u, (void (*)(void))fighter_22a00, 0x000A55A8u, 0x000E1534u, 0x40400000u,
                     9u, 7u, 0x000229FCu, 1, 0, 0u);
    p2_check_guarded(0x14EF8u, (void (*)(void))fighter_14ef8, 0x000A46A8u, 0x000D2E26u, 0x40000000u,
                     0x0Bu, 6u, 0u, 0, 1, 0xB2u);
    p2_check_guarded(0x14F50u, (void (*)(void))fighter_14f50, 0x000A46BCu, 0x000D2E56u, 0x40000000u,
                     0x0Bu, 6u, 0u, 0, 1, 0xB2u);
''')

TEST = r'''
typedef void (*p2_anim_fn)(u32 rec, u32 arg);

/* §P2.4: character 3's reactions 0x20/0x21 on their real streams. The
 * 0xD000 words (opcode 0x10, mode 0x4000) at 0xD2E2C/0xD2E32 (0xD2E26) and
 * 0xD2E5C/0xD2E62 (0xD2E56) name 0x14FA8 then 0x14FF8, and 0x14FA8 then
 * 0x150AC; slot 0 (character 3)
 * runs the callback through its registration, then its record syncs each
 * frame (actor_sync walks the stream and dispatches the targets) until the
 * held record lands in the slot's +8 (rec+0x4B carries the sentinel 0xEE).
 * Returns that frame, or -1. */
static int p2_run_reaction(u32 addr)
{
    p2_cb_fn cb = (p2_cb_fn)(void *)fn_resolve(addr);
    int f;
    if (cb == NULL) return -2;
    sh_seed(Z_S0, Z_S1, Z_R0, Z_R1);
    c4r_pool();
    DSB(Z_S0 + 0x7Au) = 3u;
    DSB(Z_S1 + 0x7Au) = 2u;
    DSD(DS_001077A8) = Z_S0;
    DSD(DS_001077A8 + 4u) = Z_S1;
    DSD(Z_R0 + 0x14u) = Z_S0;
    DSD(Z_R1 + 0x14u) = Z_S1;
    DSD(Z_S0 + 8u) = 0u;
    DSB(Z_R0 + 0x4Bu) = 0xEEu;
    cb(Z_S0, Z_R0, 0u);
    for (f = 0; f < 400; f++) {
        actor_sync(Z_R0);
        if (DSD(Z_S0 + 8u) != 0u) return f;
    }
    return -1;
}

static void p2_check_stream_targets(void)
{
    static const u32 dw[4] = { 0x000D2E2Eu, 0x000D2E34u, 0x000D2E5Eu, 0x000D2E64u };
    static const u32 fn[4] = { 0x00014FA8u, 0x00014FF8u, 0x00014FA8u, 0x000150ACu };
    u32 k, held;
    int f;
    for (k = 0; k < 4u; k++) {
        CHECK_EQ_INT((int)DSW(dw[k] - 2u), 0xD000);
        CHECK_EQ_INT((int)DSD(dw[k]), (int)fn[k]);
        CHECK(fn_resolve(fn[k]) != NULL, "the stream target is registered");
    }
    /* 0x14EF8 (0xD2E26): 0x14FA8 spawns its record (rec+0x4B takes the
     * spawned record's index), then 0x14FF8 puts the held record in the
     * slot's +8, with the word +0x34 = -0x14A when 0x1A570(0) is set, else
     * 0x14A. */
    f = p2_run_reaction(0x14EF8u);
    CHECK(f >= 0, "0x14EF8's stream puts a held record in the slot's +8");
    held = DSD(Z_S0 + 8u);
    if (held == 0u) return;
    CHECK_EQ_INT((int)DSD(held + 0x14u), (int)Z_S0);
    CHECK_EQ_INT((int)DSB(held + 0x59u), 2);
    CHECK_EQ_INT((int)DSW(held + 0x34u), fighter_actor_bit15_clear(0u) ? 0xFEB6 : 0x014A);
    CHECK(DSB(Z_R0 + 0x4Bu) != 0xEEu, "0x14FA8 stored the spawned record's index");
    /* the held record now blocks the callback (the guard at 0x14EFD) */
    DSB(Z_S0 + 0x53u) = 0x33u;
    fighter_14ef8(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 0x33);
    /* 0x14F50 (0xD2E56): 0x14FA8 again, then 0x150AC's word -0x226 or 0x226. */
    f = p2_run_reaction(0x14F50u);
    CHECK(f >= 0, "0x14F50's stream puts a held record in the slot's +8");
    held = DSD(Z_S0 + 8u);
    if (held == 0u) return;
    CHECK_EQ_INT((int)DSW(held + 0x34u), fighter_actor_bit15_clear(0u) ? 0xFDDA : 0x0226);
    CHECK(DSB(Z_R0 + 0x4Bu) != 0xEEu, "0x14FA8 stored the spawned record's index");
}

static void p2_check_reactions_3(void)
{
    p2_check_stream_targets();
}

int test_p2_reactions_3(void)   { return u6b_run(p2_check_reactions_3); }
'''
ANCHOR = "int test_p2_guarded(void)       { return u6b_run(p2_check_guarded_all); }\n"
sub("port/tests/test_fight.c", ANCHOR, ANCHOR + TEST)
sub("port/tests/test.h", "    X(test_p2_guarded) \\\n", "    X(test_p2_guarded) \\\n    X(test_p2_reactions_3) \\\n")
print("t3_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected:

```
t3_test applied
port/tests/test_fight.c:45071:48: error: use of undeclared identifier 'fighter_14ef8'
port/tests/test_fight.c:45073:48: error: use of undeclared identifier 'fighter_14f50'
port/tests/test_fight.c:45158:5: error: call to undeclared function 'fighter_14ef8'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]
3 errors generated.
```

- [ ] **Step 3: the port.**

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
#define P2_ANIM_14EF8 0x000D2E26u  /* 0x14F07 */
#define P2_ANIM_14F50 0x000D2E56u  /* 0x14F5F */
#define P2_DESC_14FA8 0x000BB36Cu  /* 0x14FC1 */
#define P2_DESC_HELD  0x000BB380u  /* 0x15069 (0x14FF8), 0x1511D (0x150AC) */

/* 0x14EF8 — record §P2.4. Character 3's reaction-0x20 callback (the dword at
 * 0xA46A8): with the slot's +8 clear, the record on 0xD2E26 at 2.0 (0x3C4CC),
 * the slot 0xB/6/0, +0x0C = +0x18 = +0x1C = 0, +0x64 = +0x5F, +0x5F = 0xFF,
 * the voice 0xB2. PORT: AL unread (§P2.2). */
void fighter_14ef8(u32 slot, u32 rec, u32 side)
{
    u8 r5f;
    (void)side;
    if (DSD(slot + 8u) != 0u) return;                       /* 0x14EFD/0x14F01 */
    hit_anim_start_b(rec, P2_ANIM_14EF8, 0x40000000u);      /* 0x14F07..0x14F11 0x3C4CC */
    DSB(slot + 0x52u) = 0x0Bu;                              /* 0x14F16 */
    DSB(slot + 0x53u) = 6u;                                 /* 0x14F1A */
    DSB(slot + 0x54u) = 0u;                                 /* 0x14F1E */
    DSD(slot + 0x0Cu) = 0u;                                 /* 0x14F22 */
    DSD(slot + 0x18u) = 0u;                                 /* 0x14F29 */
    r5f = DSB(slot + 0x5Fu);                                /* 0x14F30 */
    DSD(slot + 0x1Cu) = 0u;                                 /* 0x14F33 */
    DSB(slot + 0x64u) = r5f;                                /* 0x14F3A */
    DSB(slot + 0x5Fu) = 0xFFu;                              /* 0x14F42 */
    (void)sound_voice(0xB2u);                               /* 0x14F3D/0x14F46 0x2C3FC */
}

/* 0x14F50 — record §P2.4. Character 3's reaction-0x21 callback (the dword at
 * 0xA46BC): 0x14EF8's shape on the stream 0xD2E56. PORT: AL unread. */
void fighter_14f50(u32 slot, u32 rec, u32 side)
{
    u8 r5f;
    (void)side;
    if (DSD(slot + 8u) != 0u) return;                       /* 0x14F55/0x14F59 */
    hit_anim_start_b(rec, P2_ANIM_14F50, 0x40000000u);      /* 0x14F5F..0x14F69 0x3C4CC */
    DSB(slot + 0x52u) = 0x0Bu;                              /* 0x14F6E */
    DSB(slot + 0x53u) = 6u;                                 /* 0x14F72 */
    DSB(slot + 0x54u) = 0u;                                 /* 0x14F76 */
    DSD(slot + 0x0Cu) = 0u;                                 /* 0x14F7A */
    DSD(slot + 0x18u) = 0u;                                 /* 0x14F81 */
    r5f = DSB(slot + 0x5Fu);                                /* 0x14F88 */
    DSD(slot + 0x1Cu) = 0u;                                 /* 0x14F8B */
    DSB(slot + 0x64u) = r5f;                                /* 0x14F92 */
    DSB(slot + 0x5Fu) = 0xFFu;                              /* 0x14F9A */
    (void)sound_voice(0xB2u);                               /* 0x14F95/0x14F9E 0x2C3FC */
}

/* 0x14FA8 — record §P2.4. The 0xD000 target (opcode 0x10, mode 0x4000) at
 * the dwords 0xD2E2E (0x14EF8's stream 0xD2E26) and 0xD2E5E (0x14F50's
 * 0xD2E56). EAX = rec (EDX is pushed and
 * zeroed at 0x14FBF before any read). 0x2AE14(0xBB36C, 0, 0, 0, rec's word
 * +0x56 | 0x400); the spawned record's +0x59 = 2; for side 1 (rec+0x51) its
 * +0x4E = 1 and word +0x2E += 4; rec+0x4B = its byte +0x56; its +0x60 = 1. */
void fighter_14fa8(u32 rec)
{
    u32 e = actor_spawn((const u32 *)(mem + P2_DESC_14FA8), 0u, 0u, 0u,
                        (u32)(DSW(rec + 0x56u) | 0x400u)); /* 0x14FAE..0x14FC6 0x2AE14 */
    DSB(e + 0x59u) = 2u;                                    /* 0x14FCB */
    if (DSB(rec + 0x51u) != 0u) {                           /* 0x14FCF/0x14FD4 */
        DSB(e + 0x4Eu) = 1u;                                /* 0x14FDC */
        DSW(e + 0x2Eu) = (u16)(DSW(e + 0x2Eu) + 4u);        /* 0x14FD8..0x14FE3 */
    }
    DSB(rec + 0x4Bu) = DSB(e + 0x56u);                      /* 0x14FE7/0x14FEA */
    DSB(e + 0x60u) = 1u;                                    /* 0x14FED */
}

/* 0x14FF8 — record §P2.4. The 0xD000 target at the dword 0xD2E34 in 0xD2E26.
 * EAX = rec; with its owner slot rec+0x14 (ESI): by 0x1A570(rec+0x51) the
 * word w = -0x14A and the x offset -0x1200 (AL set) or 0x14A and 0x1200;
 * 0x2AE14(0xBB380, rec+0x18 + offset, rec+0x30 >> 16, rec+0x1C + 0x1600,
 * rec's word +0x28 bit 14 ? 0x4000 : 0) into the slot's +8, its +0x59 = 2,
 * word +0x34 = w, +0x14 = the slot; for side 1 its word +0x2E += 4 and +0x4E
 * = 1 (the slot's +8 re-read at each store, 0x1507A..0x15098). */
void fighter_14ff8(u32 rec)
{
    u32 slot = DSD(rec + 0x14u);                            /* 0x15003 */
    u32 w, dx, flag, e;
    if (slot == 0u) return;                                 /* 0x15006/0x15008 */
    if (fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0) {   /* 0x1500E..0x1501A 0x1A570 */
        w = 0xFFFFFEB6u;                                    /* 0x1501C/0x15026 */
        dx = 0xFFFFEE00u;                                   /* 0x15021 */
    } else {
        w = 0x0000014Au;                                    /* 0x1502B */
        dx = 0x00001200u;                                   /* 0x15032 */
    }
    flag = (DSW(rec + 0x28u) & 0x4000u) != 0u ? 0x4000u : 0u;   /* 0x15037..0x1504C */
    e = actor_spawn((const u32 *)(mem + P2_DESC_HELD), DSD(rec + 0x18u) + (u32)(s32)(s16)dx,
                    (u32)((s32)DSD(rec + 0x30u) >> 16),
                    DSD(rec + 0x1Cu) + 0x1600u, flag);      /* 0x15051..0x1506E 0x2AE14 */
    DSD(slot + 8u) = e;                                     /* 0x15073 */
    DSB(e + 0x59u) = 2u;                                    /* 0x15076 */
    DSW(DSD(slot + 8u) + 0x34u) = (u16)w;                   /* 0x1507A..0x15080 */
    DSD(DSD(slot + 8u) + 0x14u) = slot;                     /* 0x15084/0x15087 */
    if (DSB(rec + 0x51u) == 0u) return;                     /* 0x1508A/0x1508E */
    DSW(DSD(slot + 8u) + 0x2Eu) = (u16)(DSW(DSD(slot + 8u) + 0x2Eu) + 4u);   /* 0x15090/0x15093 */
    DSB(DSD(slot + 8u) + 0x4Eu) = 1u;                       /* 0x15098/0x1509B */
}

/* 0x150AC — record §P2.4. The 0xD000 target at the dword 0xD2E64 in 0x14F50's
 * stream 0xD2E56: 0x14FF8's shape with the word 0x226 (-0x226 when 0x1A570's
 * AL is set, 0x150D0..0x150E6). */
void fighter_150ac(u32 rec)
{
    u32 slot = DSD(rec + 0x14u);                            /* 0x150B7 */
    u32 w, dx, flag, e;
    if (slot == 0u) return;                                 /* 0x150BA/0x150BC */
    if (fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0) {   /* 0x150C2..0x150CE 0x1A570 */
        w = 0xFFFFFDDAu;                                    /* 0x150D0/0x150DA */
        dx = 0xFFFFEE00u;                                   /* 0x150D5 */
    } else {
        w = 0x00000226u;                                    /* 0x150DF */
        dx = 0x00001200u;                                   /* 0x150E6 */
    }
    flag = (DSW(rec + 0x28u) & 0x4000u) != 0u ? 0x4000u : 0u;   /* 0x150EB..0x15100 */
    e = actor_spawn((const u32 *)(mem + P2_DESC_HELD), DSD(rec + 0x18u) + (u32)(s32)(s16)dx,
                    (u32)((s32)DSD(rec + 0x30u) >> 16),
                    DSD(rec + 0x1Cu) + 0x1600u, flag);      /* 0x15105..0x15122 0x2AE14 */
    DSD(slot + 8u) = e;                                     /* 0x15127 */
    DSB(e + 0x59u) = 2u;                                    /* 0x1512A */
    DSW(DSD(slot + 8u) + 0x34u) = (u16)w;                   /* 0x1512E..0x15134 */
    DSD(DSD(slot + 8u) + 0x14u) = slot;                     /* 0x15138/0x1513B */
    if (DSB(rec + 0x51u) == 0u) return;                     /* 0x1513E/0x15142 */
    DSW(DSD(slot + 8u) + 0x2Eu) = (u16)(DSW(DSD(slot + 8u) + 0x2Eu) + 4u);   /* 0x15144/0x15147 */
    DSB(DSD(slot + 8u) + 0x4Eu) = 1u;                       /* 0x1514C/0x1514F */
}
'''
BINDINGS = r'''/* §P2.4: the 0xD000 targets as the animation dispatcher calls them at 0x2B56D (EAX = rec), mask 0 (0x2B575
 * overwrites EAX). */
static void b_14ef8(const u32 *r, u32 *eax)            { fighter_14ef8(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_14f50(const u32 *r, u32 *eax)            { fighter_14f50(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_14fa8(const u32 *r, u32 *eax)            { fighter_14fa8(r[R_EAX]); *eax = 0u; }
static void b_14ff8(const u32 *r, u32 *eax)            { fighter_14ff8(r[R_EAX]); *eax = 0u; }
static void b_150ac(const u32 *r, u32 *eax)            { fighter_150ac(r[R_EAX]); *eax = 0u; }
static void m_14ef8(const u32 *r, u32 *eax)            /* the frame 3.0, not 2.0 */
{
    u32 slot = r[R_EAX];
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    hit_anim_start_b(r[R_EDX], 0x000D2E26u, 0x40400000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0u;
    DSD(slot + 0x18u) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSD(slot + 0x1Cu) = 0u;
    DSB(slot + 0x64u) = r5f;
    DSB(slot + 0x5Fu) = 0xFFu;
    (void)sound_voice(0xB2u);
}
static void m_14f50(const u32 *r, u32 *eax)            /* 0x14EF8's stream 0xD2E26 */
{
    u32 slot = r[R_EAX];
    u8 r5f;
    *eax = 0u;
    if (DSD(slot + 8u) != 0u) return;
    hit_anim_start_b(r[R_EDX], 0x000D2E26u, 0x40000000u);
    DSB(slot + 0x52u) = 0x0Bu;
    DSB(slot + 0x53u) = 6u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0u;
    DSD(slot + 0x18u) = 0u;
    r5f = DSB(slot + 0x5Fu);
    DSD(slot + 0x1Cu) = 0u;
    DSB(slot + 0x64u) = r5f;
    DSB(slot + 0x5Fu) = 0xFFu;
    (void)sound_voice(0xB2u);
}
static void m_14fa8(const u32 *r, u32 *eax)            /* 0x14FF8's descriptor 0xBB380 */
{
    u32 rec = r[R_EAX];
    u32 e = actor_spawn((const u32 *)(mem + 0x000BB380u), 0u, 0u, 0u, (u32)(DSW(rec + 0x56u) | 0x400u));
    DSB(e + 0x59u) = 2u;
    if (DSB(rec + 0x51u) != 0u) {
        DSB(e + 0x4Eu) = 1u;
        DSW(e + 0x2Eu) = (u16)(DSW(e + 0x2Eu) + 4u);
    }
    DSB(rec + 0x4Bu) = DSB(e + 0x56u);
    DSB(e + 0x60u) = 1u;
    *eax = 0u;
}
static void m_held(u32 rec, u32 w1, u32 w0, int same_dx)
{
    u32 slot = DSD(rec + 0x14u), w, dx, flag, e;
    if (slot == 0u) return;
    if (fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0) { w = w1; dx = same_dx ? 0x1200u : 0xFFFFEE00u; }
    else { w = w0; dx = 0x1200u; }
    flag = (DSW(rec + 0x28u) & 0x4000u) != 0u ? 0x4000u : 0u;
    e = actor_spawn((const u32 *)(mem + 0x000BB380u), DSD(rec + 0x18u) + (u32)(s32)(s16)dx,
                    (u32)((s32)DSD(rec + 0x30u) >> 16), DSD(rec + 0x1Cu) + (same_dx ? 0x1600u : 0x1200u), flag);
    DSD(slot + 8u) = e;
    DSB(e + 0x59u) = 2u;
    DSW(DSD(slot + 8u) + 0x34u) = (u16)w;
    DSD(DSD(slot + 8u) + 0x14u) = slot;
    if (DSB(rec + 0x51u) == 0u) return;
    DSW(DSD(slot + 8u) + 0x2Eu) = (u16)(DSW(DSD(slot + 8u) + 0x2Eu) + 4u);
    DSB(DSD(slot + 8u) + 0x4Eu) = 1u;
}
static void m_14ff8(const u32 *r, u32 *eax)            /* the x offset +0x1200 on both arms */
{
    m_held(r[R_EAX], 0xFFFFFEB6u, 0x14Au, 1);
    *eax = 0u;
}
static void m_150ac(const u32 *r, u32 *eax)            /* the z offset 0x1200, not 0x1600 */
{
    m_held(r[R_EAX], 0xFFFFFDDAu, 0x226u, 0);
    *eax = 0u;
}

'''
sub("port/src/game/fighter.c", '''void fighter_229fc(u32 slot, u32 rec, u32 side)
{
    (void)slot;
    (void)rec;
    (void)side;
}
''', '''void fighter_229fc(u32 slot, u32 rec, u32 side)
{
    (void)slot;
    (void)rec;
    (void)side;
}
''' + FIGHTER_C)
sub("port/src/game/fighter.h", "void fighter_229fc(u32 slot, u32 rec, u32 side);\n",
    """void fighter_229fc(u32 slot, u32 rec, u32 side);
/* §P2.4: character 3's reactions 0x20 and 0x21, and the 0xD000 targets of the
 * streams they start (EAX = rec; registered through anim_code wrappers). */
void fighter_14ef8(u32 slot, u32 rec, u32 side);
void fighter_14f50(u32 slot, u32 rec, u32 side);
void fighter_14fa8(u32 rec);
void fighter_14ff8(u32 rec);
void fighter_150ac(u32 rec);
""")
sub("port/src/game/actors.c", "static void reaction_cb_3C048(u32 slot, u32 rec, u32 side);\n",
    """static void reaction_cb_3C048(u32 slot, u32 rec, u32 side);
static void anim_code_14FA8(u32 rec, u32 arg);
static void anim_code_14FF8(u32 rec, u32 arg);
static void anim_code_150AC(u32 rec, u32 arg);
""")
sub("port/src/game/actors.c", "    fn_register(0x229FCu, (void (*)(void))fighter_229fc);\n",
    """    fn_register(0x229FCu, (void (*)(void))fighter_229fc);
    /* PORT: record 2026-10-02-reverse-p2 §P2.4. Character 3's reaction-0x20
     * and 0x21 callbacks (the move-table dwords 0xA46A8 and 0xA46BC; 0x34E2C
     * at 0x35045, (slot, rec, side)), which gp-u8-right-arcade reaches, and
     * the 0xD000 targets (opcode 0x10, mode 0x4000) of their streams: 0x14FA8
     * (the dwords 0xD2E2E and 0xD2E5E), 0x14FF8 (0xD2E34 in 0xD2E26) and
     * 0x150AC (0xD2E64 in 0xD2E56). */
    fn_register(0x14EF8u, (void (*)(void))fighter_14ef8);
    fn_register(0x14F50u, (void (*)(void))fighter_14f50);
    fn_register(0x14FA8u, (void (*)(void))anim_code_14FA8);
    fn_register(0x14FF8u, (void (*)(void))anim_code_14FF8);
    fn_register(0x150ACu, (void (*)(void))anim_code_150AC);
""")
sub("port/src/game/actors.c", '''/* 0x23868 — the animation-opcode target shape: the raw reads both EAX = rec
''', '''/* 0x14FA8, 0x14FF8 and 0x150AC — the animation-opcode target shape. PORT:
 * anim_indirect calls every code pointer as (rec, arg); the raw reads EAX =
 * rec only (0x14FA8 pushes EDX and zeroes it at 0x14FBF, 0x14FF8/0x150AC load
 * it at 0x15021/0x15032 and 0x150D5/0x150E6 before any read), so these
 * wrappers drop the operand (record 2026-10-02-reverse-p2 §P2.4). */
static void anim_code_14FA8(u32 rec, u32 arg)
{
    (void)arg;
    fighter_14fa8(rec);
}

static void anim_code_14FF8(u32 rec, u32 arg)
{
    (void)arg;
    fighter_14ff8(rec);
}

static void anim_code_150AC(u32 rec, u32 arg)
{
    (void)arg;
    fighter_150ac(rec);
}

/* 0x23868 — the animation-opcode target shape: the raw reads both EAX = rec
''')
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_229fc@mutant",     m_229fc,        0x00000000u },\n',
    """    { "fighter_229fc@mutant",     m_229fc,        0x00000000u },
    { "fighter_14ef8",            b_14ef8,        0x00000000u },
    { "fighter_14f50",            b_14f50,        0x00000000u },
    { "fighter_14fa8",            b_14fa8,        0x00000000u },
    { "fighter_14ff8",            b_14ff8,        0x00000000u },
    { "fighter_150ac",            b_150ac,        0x00000000u },
    { "fighter_14ef8@mutant",     m_14ef8,        0x00000000u },
    { "fighter_14f50@mutant",     m_14f50,        0x00000000u },
    { "fighter_14fa8@mutant",     m_14fa8,        0x00000000u },
    { "fighter_14ff8@mutant",     m_14ff8,        0x00000000u },
    { "fighter_150ac@mutant",     m_150ac,        0x00000000u },
""")
print("t3_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_14ef8 fighter_14f50 fighter_14fa8 fighter_14ff8 fighter_150ac; do
  python3 tools/diff_verify.py --image /tmp/pr_p2_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected:

```
t3_port applied
all checks passed
| fighter_14ef8 | 0x14EF8 | 3 | 3/3 | VERIFIED | 2C3FC stub unverified, 3C4CC stub unverified |
| fighter_14ef8@mutant | 0x14EF8 | 3 | 3/3 | MISMATCH | 2C3FC stub unverified, 3C4CC stub unverified |
| fighter_14f50 | 0x14F50 | 3 | 3/3 | VERIFIED | 2C3FC stub unverified, 3C4CC stub unverified |
| fighter_14f50@mutant | 0x14F50 | 3 | 3/3 | MISMATCH | 2C3FC stub unverified, 3C4CC stub unverified |
| fighter_14fa8 | 0x14FA8 | 2 | 3/3 | VERIFIED | 2AE14 stub unverified |
| fighter_14fa8@mutant | 0x14FA8 | 2 | 3/3 | MISMATCH | 2AE14 stub unverified |
| fighter_14ff8 | 0x14FF8 | 5 | 9/9 | VERIFIED | 1A570 stub unverified, 2AE14 stub unverified |
| fighter_14ff8@mutant | 0x14FF8 | 5 | 9/9 | MISMATCH | 1A570 stub unverified, 2AE14 stub unverified |
| fighter_150ac | 0x150AC | 5 | 9/9 | VERIFIED | 1A570 stub unverified, 2AE14 stub unverified |
| fighter_150ac@mutant | 0x150AC | 5 | 9/9 | MISMATCH | 1A570 stub unverified, 2AE14 stub unverified |
```

- [ ] **Step 4: the E2 table** (the same three commands as Task 2 Step 4). Expected:

```
entry-triage: FAIL: docs/superpowers/plans/2026-10-01-reverse-e2-triage.md differs from a fresh run (regenerate it with --out)
make: *** [entry-triage] Error 1
entry-triage: targets 302 unported, 193 ported; supplement 131 (28 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 33 in unported code, 82 in ported code, 19 nowhere
| callbacks | 14 | 57 |
| finishers | 0 | 9 |
| animation-targets | 54 | 58 |
| stubs | 65 |
 1 file changed, 10 insertions(+), 10 deletions(-)
```

- [ ] **Step 5: U8's right-arcade (the mandatory interaction, record §P2.4).** First the replay with the old miss set, which must now fail (the two pinned misses are gone):

```bash
make gp-modes-one GP_MODES_ID=RA scenario=gp-u8-right-arcade GP_DUMP=/tmp/pr_p2_gp 2>&1 \
  | grep -E '^(fn-miss|FAIL|gp_compare: .*(FIRST|ratchet)|make: \*\*\*)' | sed 's#/[^ ]*/port/#port/#'
```

Expected:

```
fn-miss PR_GP_DUMP 0x5D812 actor_spawn hits=4542
fn-miss PR_GP_DUMP 0x5D812 set_dead hits=4199
fn-miss PR_GP_DUMP 0x29D60 frontend_mode_1b_step hits=1
fn-miss PR_GP_DUMP 0x5D812 frontend_mode_1b_step hits=1
fn-miss PR_GP_DUMP distinct=4 dropped=0
FAIL port/tests/test_platform.c:328: 4 != 6
FAILURES: 1
make: *** [gp-modes-one] Error 2
```

Then drop the two rows and replay, keeping the dump:

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


# gp-u8-right-arcade no longer misses 0x14EF8/0x14F50 (record §P2.4): its set is the two wipe hooks
sub("port/tests/test_platform.c", ''' * gp-u8-right-arcade (START MENU row 1, b1f = 2), to its X record (f = 0x8E1):
 *   0x29D60 frontend_mode_1b_step: the bare `ret` (record §G.24);
 *   0x5D812 frontend_mode_1b_step: the runtime stub (record §G.24);
 *   0x14EF8 and 0x14F50 hit_reaction_apply: unported move callbacks (the
 *   dwords 0xA46A8 and 0xA46BC, character 3's reactions 0x20/0x21, called
 *   through [0x105BD4] at 0x2B56D; record reverse-e2 triage), owner track P
 *   (batch P2); not registered here. */
static const fnm_pair k_miss_gp_u8_right_arcade[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
    { 0x14EF8u, "hit_reaction_apply" },
    { 0x14F50u, "hit_reaction_apply" },
};''', ''' * gp-u8-right-arcade (START MENU row 1, b1f = 2), to its X record (f = 0x8E1):
 *   0x29D60 frontend_mode_1b_step: the bare `ret` (record §G.24);
 *   0x5D812 frontend_mode_1b_step: the runtime stub (record §G.24).
 * The move callbacks it reaches, 0x14EF8 and 0x14F50 (the dwords 0xA46A8 and
 * 0xA46BC, character 3's reactions 0x20/0x21, called by hit_reaction_apply at
 * 0x35045), and their streams' 0xD000 targets 0x14FA8, 0x14FF8 and 0x150AC
 * (anim_indirect) are ported by track P batch 2 (record
 * 2026-10-02-reverse-p2 §P2.4), so none of them is a miss. */
static const fnm_pair k_miss_gp_u8_right_arcade[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};''')
print("t3_u8 applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'
make gp-modes-one GP_MODES_ID=RA scenario=gp-u8-right-arcade GP_DUMP=/tmp/pr_p2_gp GP_MODES_KEEP=1 2>&1 \
  | grep -E '^(fn-miss|FAIL|all checks|gp_compare)'
```

Expected (the old pins still pass; both say "improved"):

```
t3_u8 applied
fn-miss PR_GP_DUMP 0x5D812 actor_spawn hits=4542
fn-miss PR_GP_DUMP 0x5D812 set_dead hits=4199
fn-miss PR_GP_DUMP 0x29D60 frontend_mode_1b_step hits=1
fn-miss PR_GP_DUMP 0x5D812 frontend_mode_1b_step hits=1
fn-miss PR_GP_DUMP distinct=4 dropped=0
all checks passed
gp_compare: gp-u8-right-arcade: capture: poll.log sha256 b72dbaa7..27906f, 1140 frames: matches the pin
gp_compare: gp-u8-right-arcade: frames: window from capture 88 (raw 1745); 975 classified: 624 clean, 350 splice, 0 transition, 1 unexplained, 10 all-black
gp_compare: gp-u8-right-arcade: frames: FIRST UNEXPLAINED capture 1072 (raw 4116): nearest port 824, rows 0..191, x 7..319 (5108 px)
gp_compare: gp-u8-right-arcade: frames: coverage (reported, not ratcheted): 12 non-black port frame(s) up to port 825 not exhibited by any classified capture frame: [10, 32, 111, 250, 251, 409, 450, 451, 452, 454, 455, 494]
gp_compare: gp-u8-right-arcade: frames: first unexplained 1072, ratchet N 726 ok (improved: raise N)
gp_compare: gp-u8-right-arcade: trace: 1949 frames compared (f 13E..), 7 without a capture snapshot; first tick difference f=13F (reported, not ratcheted)
gp_compare: gp-u8-right-arcade: trace: normalised (reported, not ratcheted): ent 0 of 1949 differ; t508 736 of 1949 differ (first f=600)
gp_compare: gp-u8-right-arcade: trace: 0 differing through 2273; ratchet N 1978 ok (every item is explained: N = 2274 is the exact pin)
```

Show that N is how far the port got (capture 1071 equals the port's last frame 825, f=0x8E1, and 1072 does not) and that both new pins are exact (one more fails):

```bash
python3 - <<'PY'
import sys; sys.path.insert(0, 'tools')
import gp_compare as G
cap = lambda i: G.load_capture_frame('data/k11-captures/gp-u8-right-arcade/frame_%05d.raw.gz' % i)
port = lambda i: G.load_port_frame('/tmp/pr_p2_gp/gp-u8-right-arcade/frame_%05d.ipx' % i)
last = port(825)
for c in (1071, 1072):
    f = cap(c)
    print(c, sum(1 for k in range(0, len(f), 3) if f[k:k + 3] != last[k:k + 3]))
PY
python3 tools/gp_compare.py --scenario gp-u8-right-arcade --capture data/k11-captures/gp-u8-right-arcade \
  --port /tmp/pr_p2_gp/gp-u8-right-arcade --min-first 1073 --trace-min-first 2275 --max-start 88 2>&1 | grep -E 'FAIL'
rm -rf /tmp/pr_p2_gp/gp-u8-right-arcade
```

Expected (pixels differing from port frame 825; then the two failures):

```
1071 0
1072 5045
gp_compare: gp-u8-right-arcade: frames: FAIL: first unexplained 1072 < ratchet N 1073
gp_compare: gp-u8-right-arcade: trace: FAIL: N 2275 > end 2274: N is unreachable
```

Pin N = 1072 and F = 2274 with their provenance and replay:

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


sub("Makefile", '''# gp-u8-right-arcade (record §U8.16): measured at bc51fd0 (+ its miss set) on the capture below.
# MIN_FIRST: first unexplained capture frame 726 (raw 3767), nearest port 526 (f=0x7B6): side 0
# takes character 3's reaction 0x20 at f=0x7B7 (r0=20) and the original runs its move callback
# 0x14EF8 (s0_52 = 0x0B, 0x14F16), which the port does not have (fn-miss 0x14EF8, unported, owner
# track P batch P2); TRACE_MIN_FIRST: first differing f=0x7BA (1978) in e0 (capture 0000, port
# 1010), the same cause; MAX_START: the window starts at capture frame 88 (raw 1745). The port's
# script runs to X (f=0x8E1). Raise N/F when they improve.
GP_MODES_RA_MIN_FIRST = 726
GP_MODES_RA_TRACE_MIN_FIRST = 1978
''', '''# gp-u8-right-arcade (record §U8.16; re-measured by track P batch 2, record 2026-10-02-reverse-p2
# §P2.4, once 0x14EF8/0x14F50 and their streams' 0xD000 targets 0x14FA8/0x14FF8/0x150AC are ported) on
# the capture below. MIN_FIRST: first unexplained capture frame 1072 (raw 4116): capture 1071 equals
# port 825 (f=0x8E1, the port's last frame, 0 px), so 1072 is the next game frame, which the port never
# ran (its script ends at the capture's X record): how far the port got, not a divergence (gp_compare's
# row-hash "nearest port 824"). TRACE_MIN_FIRST: "0 differing through 2273", end + 1 = 2274, the exact
# pin. Before P2 (U8, at bc51fd0) both stopped at the reaction 0x20 of f=0x7B7 (N 726, F 1978).
# MAX_START: the window starts at capture frame 88 (raw 1745). Raise N/F when they improve.
GP_MODES_RA_MIN_FIRST = 1072
GP_MODES_RA_TRACE_MIN_FIRST = 2274
''')
print("t3_pin applied")
PY
make gp-modes-one GP_MODES_ID=RA scenario=gp-u8-right-arcade GP_DUMP=/tmp/pr_p2_gp 2>&1 | grep -E 'ratchet'
```

Expected:

```
t3_pin applied
gp_compare: gp-u8-right-arcade: frames: coverage (reported, not ratcheted): 12 non-black port frame(s) up to port 825 not exhibited by any classified capture frame: [10, 32, 111, 250, 251, 409, 450, 451, 452, 454, 455, 494]
gp_compare: gp-u8-right-arcade: frames: first unexplained 1072, ratchet N 1072 ok
gp_compare: gp-u8-right-arcade: trace: 1949 frames compared (f 13E..), 7 without a capture snapshot; first tick difference f=13F (reported, not ratcheted)
gp_compare: gp-u8-right-arcade: trace: normalised (reported, not ratcheted): ent 0 of 1949 differ; t508 736 of 1949 differ (first f=600)
gp_compare: gp-u8-right-arcade: trace: 0 differing through 2273; ratchet N 2274 ok
```

The other gp scenarios' miss sets (`gp-idle-loss`, `gp-u5-charsel`, `gp-u6-moves`, `gp-keys-fight`, `gp-twop`, the six other `gp-u8-*`) hold only `0x29D60`/`0x5D812`: no P2 member, so their pins do not move (record §P2.12; Task 9's `make verify` shows them).

- [ ] **Step 6: the task gate** (Task 2 Step 5's command). Expected:

```
Ran 157 tests in N.NNNs
OK
diff-verify: 42/42 functions VERIFIED; 60/60 mutants detected; 1 named gaps; 2/29 rows with callees closed (13 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
Ran 44 tests in N.NNNs
OK
entry-triage: targets 302 unported, 193 ported; supplement 131 (28 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 7: every new unit assertion can fail.**

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


mutate("port/src/game/actors.c", "    fn_register(0x14FF8u, (void (*)(void))anim_code_14FF8);\n", "")
mutate("port/src/game/actors.c", "    fn_register(0x14FA8u, (void (*)(void))anim_code_14FA8);\n", "")
mutate("port/src/game/fighter.c",
       "        w = 0xFFFFFEB6u;                                    /* 0x1501C/0x15026 */",
       "        w = 0xFFFFFEB7u;                                    /* 0x1501C/0x15026 */")
mutate("port/src/game/fighter.c",
       "    if (DSD(slot + 8u) != 0u) return;                       /* 0x14EFD/0x14F01 */\n", "")
PY
```

Expected, then `all checks passed`:

```
port/src/game/actors.c: 2 FAIL, first: ['test_fight.c:45142: the stream target is registered']
port/src/game/actors.c: 4 FAIL, first: ['test_fight.c:45142: the stream target is registered']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45154: 65207 != 65206']
port/src/game/fighter.c: 4 FAIL, first: ['test_fight.c:45025: 6 != 51']
all checks passed
```

- [ ] **Step 8: commit.**

```bash
git add tools/diff_verify.py tools/tests/test_diff_verify.py port/tests/test_fight.c port/tests/test.h \
  port/src/game/fighter.c port/src/game/fighter.h port/src/game/actors.c port/tests/diff_runner.c \
  port/tests/test_platform.c Makefile docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git commit -m "fighter: port character 3's reactions 0x14EF8/0x14F50 and their streams' 0xD000 targets 0x14FA8 0x14FF8 0x150AC; gp-u8-right-arcade drops its two misses, N 726 -> 1072, F 1978 -> 2274 (track P batch 2)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: the unconditional move callbacks `0x15478 0x3DCEC 0x21114`

**Files:** as Task 2. **Interfaces:** produces `fighter_15478`, `fighter_3dcec`, `fighter_21114` (`(u32 slot, u32 rec, u32 side)`); `P2_SEED_UNC`, `p2_21114`. Consumes `HIT_B`, `DS_SLOTS`. Record §P2.5.

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


sub("tools/diff_verify.py", '''    Spec("fighter_150ac", 0x150AC, [p2_held(*r) for r in P2_HELD_ROWS], calls=(BIT15, SPAWN), eax_mask=0),
]
''', '''    Spec("fighter_150ac", 0x150AC, [p2_held(*r) for r in P2_HELD_ROWS], calls=(BIT15, SPAWN), eax_mask=0),
]

# The unconditional move callbacks (record §P2.5): no guard, the record started, the slot 9/8/0 or 9/8/1.
# 0x3DCEC reads its stream from the dword 0xC8CD4 (0xD40F2 in the image): u1 pokes another value there, so a
# port that hard-codes the image's value differs. 0x21114 indexes DS_001077A8 by rec+0x51 (0x2111C) and
# switches on that slot's +0x7A: 1 and 6 start a stream (`jbe`/`je`), 0, 2..5 and above 6 do not (`jb`, the
# `jmp` at 0x21140); w0 a zero pointer (nothing stored); w5 side 2 reads 0x1077B0 (a `xor edx,edx; mov
# dl,..` index: no `& 1`).
P2_SEED_UNC = {E3_SLOT + 0x52: b"\\x52\\x53\\x54"}


def p2_21114(cid, side, ptrs, char):
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0},
                {**P2_SEED_UNC, E3_REC + 0x51: bytes([side]), DS_SLOTS - 8: b"".join(le32(v) for v in ptrs),
                 E3_REC2 + 0x7A: bytes([char])})


P2_SPECS += [
    Spec("fighter_15478", 0x15478, [
        Case("u0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, P2_SEED_UNC),
        Case("u1", {"eax": E3_SLOT, "edx": E3_REC2, "ebx": 1}, {E3_SLOT + 0x52: b"\\x09\\x09\\x09"}),
    ], calls=(HIT_B,), eax_mask=0),
    Spec("fighter_3dcec", 0x3DCEC, [
        Case("u0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, P2_SEED_UNC),
        Case("u1", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 1}, {**P2_SEED_UNC, 0xC8CD4: le32(0x000E1234)}),
    ], calls=(HIT_B,), eax_mask=0),
    Spec("fighter_21114", 0x21114, [
        p2_21114("w0", 0, (0, E3_REC2, 0, 0), 1),
        p2_21114("w1", 0, (E3_REC2, 0, 0, 0), 1),
        p2_21114("w2", 1, (0, E3_REC2, 0, 0), 6),
        p2_21114("w3", 1, (0, E3_REC2, 0, 0), 0),
        p2_21114("w4", 0, (E3_REC2, 0, 0, 0), 2),
        p2_21114("w5", 2, (0, 0, E3_REC2, 0), 7),
        p2_21114("w6", 0, (E3_REC2, 0, 0, 0), 5),
    ], calls=(HIT_B,), eax_mask=0, mutants=("@mutant", "@side")),
]
''')
T = "tools/tests/test_diff_verify.py"
sub(T, '''            "fighter_14fa8": 0, "fighter_14ff8": 0, "fighter_150ac": 0}''',
    '''            "fighter_14fa8": 0, "fighter_14ff8": 0, "fighter_150ac": 0,
            "fighter_15478": 0, "fighter_3dcec": 0, "fighter_21114": 0}''')
sub(T, '''            "fighter_150ac@mutant": {"call #1"}}''',
    '''            "fighter_150ac@mutant": {"call #1"},
            "fighter_15478@mutant": {"call #0 memory"}, "fighter_3dcec@mutant": {"call #0"},
            "fighter_21114@mutant": {"call #0"}, "fighter_21114@side": {"byte", "call #0"}}''')
sub(T, '''        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_237d0@guard"].problems}), ["g0"])
''', '''        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_237d0@guard"].problems}), ["g0"])
        # 0x3DCEC's stream dword: only u1 pokes it; 0x21114's index by rec+0x51, not its other side: every case
        # (each has one pointer, on its own side, so the other index reads 0 or a pointer)
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_3dcec@mutant"].problems}), ["u1"])
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_21114@side"].problems}),
                         ["w0", "w1", "w2", "w3", "w4", "w5", "w6"])
''')
sub(T, '''        self.assertIn("diff-verify: 42/42 functions VERIFIED; 60/60 mutants detected; 1 named gaps; "
                      "2/29 rows with callees closed (13 have none).", out.getvalue())''',
    '''        self.assertIn("diff-verify: 45/45 functions VERIFIED; 64/64 mutants detected; 1 named gaps; "
                      "5/32 rows with callees closed (13 have none).", out.getvalue())''')
print("t4_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p2_img.bin --function fighter_21114 | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected:

```
t4_spec applied
| fighter_21114 | 0x21114 | 7 | 10/10 | MISMATCH | 3C4CC stub unverified |
  fighter_21114: w0: port: unknown binding fighter_21114
  fighter_21114: w1: port: unknown binding fighter_21114
  fighter_21114: w2: port: unknown binding fighter_21114
  fighter_21114: w3: port: unknown binding fighter_21114
  fighter_21114: w4: port: unknown binding fighter_21114
  fighter_21114: w5: port: unknown binding fighter_21114
  fighter_21114: w6: port: unknown binding fighter_21114
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
/* §P2.5: the unconditional move callbacks through their registrations (slot
 * 0, its record, side 0; the stream heads patched to a plain frame id). */
static void p2_check_unconditional(void)
{
    p2_cb_fn f;
    CHECK(fn_resolve(0x15478u) == (void (*)(void))fighter_15478, "0x15478 is registered");
    CHECK(fn_resolve(0x3DCECu) == (void (*)(void))fighter_3dcec, "0x3DCEC is registered");
    CHECK(fn_resolve(0x21114u) == (void (*)(void))fighter_21114, "0x21114 is registered");
    CHECK_EQ_INT((int)DSD(0x000A47ACu), 0x00015478);
    CHECK_EQ_INT((int)DSD(0x000A51ACu), 0x0003DCEC);
    CHECK_EQ_INT((int)DSD(0x000A3DACu), 0x00021114);
    CHECK_EQ_INT((int)DSD(0x000A56ACu), 0x00021114);
    CHECK_EQ_INT((int)DSD(0x000C8CD4u), 0x000D40F2);

    /* 0x15478: the record on 0xD2DD2 at 4.0, the slot 9/8/0. */
    f = (p2_cb_fn)(void *)fn_resolve(0x15478u);
    if (f == NULL) return;
    z_fseed();
    DSW(0x000D2DD2u) = 0x12B1u;
    DSB(Z_S0 + 0x54u) = 0x44u;
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000D2DD2);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x40800000);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 8);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 0);

    /* 0x3DCEC: the stream the dword 0xC8CD4 holds, at 4.0, the slot 9/8/1. */
    f = (p2_cb_fn)(void *)fn_resolve(0x3DCECu);
    if (f == NULL) return;
    z_fseed();
    DSW(0x000D40F2u) = 0x12B1u;
    DSB(Z_S0 + 0x54u) = 0x44u;
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000D40F2);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x40800000);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 8);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 1);

    /* 0x21114: rec+0x51 = 0 selects DS_001077A8[0] = slot 0 (character 1
     * here): the record on 0xE481C at 5.0; with slot 1 (character 2) no
     * stream, but the slot stores; with a zero pointer nothing. */
    f = (p2_cb_fn)(void *)fn_resolve(0x21114u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_S0 + 0x7Au) = 1u;
    DSB(Z_S1 + 0x7Au) = 2u;
    DSW(0x000E481Cu) = 0x12B1u;
    DSB(Z_R0 + 0x51u) = 0u;
    DSB(Z_S0 + 0x54u) = 0x44u;
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000E481C);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x40A00000);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 8);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 1);
    z_fseed();
    DSB(Z_S1 + 0x7Au) = 2u;
    DSB(Z_R0 + 0x51u) = 1u;
    DSD(Z_R0 + 8u) = 0x08080808u;
    DSB(Z_S0 + 0x54u) = 0x44u;
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x08080808);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 1);
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSD(DS_001077A8) = 0u;
    DSB(Z_S0 + 0x54u) = 0x44u;
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 0x44);
}

int test_p2_unconditional(void) { return u6b_run(p2_check_unconditional); }
'''
ANCHOR = "int test_p2_reactions_3(void)   { return u6b_run(p2_check_reactions_3); }\n"
sub("port/tests/test_fight.c", ANCHOR, ANCHOR + TEST)
sub("port/tests/test.h", "    X(test_p2_reactions_3) \\\n", "    X(test_p2_reactions_3) \\\n    X(test_p2_unconditional) \\\n")
print("t4_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected:

```
t4_test applied
port/tests/test_fight.c:45181:51: error: use of undeclared identifier 'fighter_15478'
port/tests/test_fight.c:45182:51: error: use of undeclared identifier 'fighter_3dcec'
port/tests/test_fight.c:45183:51: error: use of undeclared identifier 'fighter_21114'
3 errors generated.
```

- [ ] **Step 3: the port.**

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
#define P2_ANIM_15478 0x000D2DD2u  /* 0x1547D */
#define P2_C8CD4      0x000C8CD4u  /* 0x3DCF1: the dword holding 0x3DCEC's stream */
#define P2_ANIM_21114_C1 0x000E481Cu  /* 0x21142: character 1 */
#define P2_ANIM_21114_C6 0x000E1702u  /* 0x21149: character 6 */

/* 0x15478 — record §P2.5. Character 3's reaction-0x2D callback (the dword at
 * 0xA47AC): the record on 0xD2DD2 at 4.0 (0x3C4CC), the slot 9/8/0. PORT:
 * the raw returns AL = 1 (0x15494); unread (§P2.2). */
void fighter_15478(u32 slot, u32 rec, u32 side)
{
    (void)side;
    hit_anim_start_b(rec, P2_ANIM_15478, 0x40800000u);      /* 0x1547D..0x15487 0x3C4CC */
    DSB(slot + 0x52u) = 9u;                                 /* 0x1548C */
    DSB(slot + 0x53u) = 8u;                                 /* 0x15490 */
    DSB(slot + 0x54u) = 0u;                                 /* 0x15496 */
}

/* 0x3DCEC — record §P2.5. Character 5's reaction-0x2D callback (the dword at
 * 0xA51AC): the record on the stream the dword 0xC8CD4 holds (0xD40F2 in the
 * image), read at the call, at 4.0; the slot 9/8/1. PORT: AL unread. */
void fighter_3dcec(u32 slot, u32 rec, u32 side)
{
    (void)side;
    hit_anim_start_b(rec, DSD(P2_C8CD4), 0x40800000u);      /* 0x3DCF1..0x3DCFC 0x3C4CC */
    DSB(slot + 0x52u) = 9u;                                 /* 0x3DD01 */
    DSB(slot + 0x53u) = 8u;                                 /* 0x3DD05 */
    DSB(slot + 0x54u) = 1u;                                 /* 0x3DD0B */
}

/* 0x21114 — record §P2.5. Characters 1's and 6's reaction-0x2D callback (the
 * dwords at 0xA3DAC and 0xA56AC). p = DS_001077A8[rec+0x51] (the byte, not
 * masked); none: AL = 0, nothing written. By p's +0x7A: 1 starts the record
 * on 0xE481C, 6 on 0xE1702, at 5.0 (0x3C4CC); any other character starts
 * nothing. Then the slot 9/8/1. PORT: AL unread (§P2.2). */
void fighter_21114(u32 slot, u32 rec, u32 side)
{
    u32 p = DSD(DS_001077A8 + (u32)DSB(rec + 0x51u) * 4u);  /* 0x2111A..0x21122 */
    u8 c;
    (void)side;
    if (p == 0u) return;                                    /* 0x21128/0x2112A */
    c = DSB(p + 0x7Au);                                     /* 0x21131 */
    if (c == 1u)                                            /* 0x21134..0x21139 */
        hit_anim_start_b(rec, P2_ANIM_21114_C1, 0x40A00000u);   /* 0x21142, 0x2114E/0x21153 0x3C4CC */
    else if (c == 6u)                                       /* 0x2113B/0x2113E */
        hit_anim_start_b(rec, P2_ANIM_21114_C6, 0x40A00000u);   /* 0x21149, 0x2114E/0x21153 0x3C4CC */
    DSB(slot + 0x52u) = 9u;                                 /* 0x21158 */
    DSB(slot + 0x53u) = 8u;                                 /* 0x2115C */
    DSB(slot + 0x54u) = 1u;                                 /* 0x21162 */
}
'''
BINDINGS = r'''/* §P2.5: the unconditional move callbacks (0x34E2C at 0x35045), mask 0. */
static void b_15478(const u32 *r, u32 *eax)            { fighter_15478(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_3dcec(const u32 *r, u32 *eax)            { fighter_3dcec(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_21114(const u32 *r, u32 *eax)            { fighter_21114(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_15478(const u32 *r, u32 *eax)            /* the slot stores before the 0x3C4CC call */
{
    u32 slot = r[R_EAX];
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x53u) = 8u;
    DSB(slot + 0x54u) = 0u;
    hit_anim_start_b(r[R_EDX], 0x000D2DD2u, 0x40800000u);
    *eax = 0u;
}
static void m_3dcec(const u32 *r, u32 *eax)            /* the image's stream 0xD40F2 hard-coded */
{
    u32 slot = r[R_EAX];
    hit_anim_start_b(r[R_EDX], 0x000D40F2u, 0x40800000u);
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x53u) = 8u;
    DSB(slot + 0x54u) = 1u;
    *eax = 0u;
}
static void m_21114_at(const u32 *r, u32 *eax, u32 idx, u32 s1, u32 s6)
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u32 p = DSD(DS_001077A8 + idx * 4u);
    *eax = 0u;
    if (p == 0u) return;
    if (DSB(p + 0x7Au) == 1u) hit_anim_start_b(rec, s1, 0x40A00000u);
    else if (DSB(p + 0x7Au) == 6u) hit_anim_start_b(rec, s6, 0x40A00000u);
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x53u) = 8u;
    DSB(slot + 0x54u) = 1u;
}
static void m_21114(const u32 *r, u32 *eax)            /* the two characters' streams swapped */
{
    m_21114_at(r, eax, (u32)DSB(r[R_EDX] + 0x51u), 0x000E1702u, 0x000E481Cu);
}
static void m_21114_side(const u32 *r, u32 *eax)       /* the other side's pointer (rec+0x51 ^ 1) */
{
    m_21114_at(r, eax, (u32)DSB(r[R_EDX] + 0x51u) ^ 1u, 0x000E481Cu, 0x000E1702u);
}

'''
sub("port/src/game/fighter.c", '''    DSB(DSD(slot + 8u) + 0x4Eu) = 1u;                       /* 0x1514C/0x1514F */
}
''', '''    DSB(DSD(slot + 8u) + 0x4Eu) = 1u;                       /* 0x1514C/0x1514F */
}
''' + FIGHTER_C)
sub("port/src/game/fighter.h", "void fighter_150ac(u32 rec);\n",
    """void fighter_150ac(u32 rec);
/* §P2.5: the unconditional move callbacks (slot, rec, side). */
void fighter_15478(u32 slot, u32 rec, u32 side);
void fighter_3dcec(u32 slot, u32 rec, u32 side);
void fighter_21114(u32 slot, u32 rec, u32 side);
""")
sub("port/src/game/actors.c", "    fn_register(0x150ACu, (void (*)(void))anim_code_150AC);\n",
    """    fn_register(0x150ACu, (void (*)(void))anim_code_150AC);
    /* PORT: record 2026-10-02-reverse-p2 §P2.5. The unconditional move
     * callbacks: character 3's reaction 0x2D (the dword 0xA47AC), character
     * 5's (0xA51AC) and characters 1's and 6's (0xA3DAC, 0xA56AC); 0x34E2C at
     * 0x35045, (slot, rec, side). */
    fn_register(0x15478u, (void (*)(void))fighter_15478);
    fn_register(0x3DCECu, (void (*)(void))fighter_3dcec);
    fn_register(0x21114u, (void (*)(void))fighter_21114);
""")
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_150ac@mutant",     m_150ac,        0x00000000u },\n',
    """    { "fighter_150ac@mutant",     m_150ac,        0x00000000u },
    { "fighter_15478",            b_15478,        0x00000000u },
    { "fighter_3dcec",            b_3dcec,        0x00000000u },
    { "fighter_21114",            b_21114,        0x00000000u },
    { "fighter_15478@mutant",     m_15478,        0x00000000u },
    { "fighter_3dcec@mutant",     m_3dcec,        0x00000000u },
    { "fighter_21114@mutant",     m_21114,        0x00000000u },
    { "fighter_21114@side",       m_21114_side,   0x00000000u },
""")
print("t4_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_15478 fighter_3dcec fighter_21114; do
  python3 tools/diff_verify.py --image /tmp/pr_p2_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected:

```
t4_port applied
all checks passed
| fighter_15478 | 0x15478 | 2 | 1/1 | VERIFIED | 3C4CC stub unverified |
| fighter_15478@mutant | 0x15478 | 2 | 1/1 | MISMATCH | 3C4CC stub unverified |
| fighter_3dcec | 0x3DCEC | 2 | 1/1 | VERIFIED | 3C4CC stub unverified |
| fighter_3dcec@mutant | 0x3DCEC | 2 | 1/1 | MISMATCH | 3C4CC stub unverified |
| fighter_21114 | 0x21114 | 7 | 10/10 | VERIFIED | 3C4CC stub unverified |
| fighter_21114@mutant | 0x21114 | 7 | 10/10 | MISMATCH | 3C4CC stub unverified |
| fighter_21114@side | 0x21114 | 7 | 10/10 | MISMATCH | 3C4CC stub unverified |
```

- [ ] **Step 4: the E2 table** (Task 2 Step 4's commands). Expected:

```
entry-triage: FAIL: docs/superpowers/plans/2026-10-01-reverse-e2-triage.md differs from a fresh run (regenerate it with --out)
make: *** [entry-triage] Error 1
entry-triage: targets 299 unported, 196 ported; supplement 131 (28 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 33 in unported code, 82 in ported code, 19 nowhere
| callbacks | 11 | 60 |
| finishers | 0 | 9 |
| animation-targets | 54 | 58 |
| stubs | 62 |
 1 file changed, 5 insertions(+), 5 deletions(-)
```

- [ ] **Step 5: the task gate.** Expected:

```
Ran 157 tests in N.NNNs
OK
diff-verify: 45/45 functions VERIFIED; 64/64 mutants detected; 1 named gaps; 5/32 rows with callees closed (13 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
Ran 44 tests in N.NNNs
OK
entry-triage: targets 299 unported, 196 ported; supplement 131 (28 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 6: every new unit assertion can fail.**

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


mutate("port/src/game/actors.c", "    fn_register(0x21114u, (void (*)(void))fighter_21114);\n", "")
mutate("port/src/game/fighter.c",
       "    hit_anim_start_b(rec, DSD(P2_C8CD4), 0x40800000u);      /* 0x3DCF1..0x3DCFC 0x3C4CC */",
       "    hit_anim_start_b(rec, DSD(P2_C8CD4), 0x40400000u);      /* 0x3DCF1..0x3DCFC 0x3C4CC */")
mutate("port/src/game/fighter.c",
       "    if (p == 0u) return;                                    /* 0x21128/0x2112A */\n", "    if (p == 0u) p = DS_001077B0;\n")
PY
```

Expected, then `all checks passed`:

```
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:45183: 0x21114 is registered']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45211: 1077936128 != 1082130432']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45246: 1 != 68']
all checks passed
```

- [ ] **Step 7: commit** (the files of Task 2 Step 7), message `fighter: port the unconditional move callbacks 0x15478 0x3DCEC 0x21114 with rows and mutants; E2 table regenerated (track P batch 2)` and the trailer.

---

### Task 5: the callbacks that arm the slot, `0x21374` and `0x22938` (seam `0x34D8C`)

**Files:** as Task 2. **Interfaces:** produces `fighter_21374`, `fighter_22938` (`(u32 slot, u32 rec, u32 side)`; both read EBX = side alone); `FLASH`, `p2_ctx_case`, `P2_22938_SEED`; the seam `PR_SEAM(0x34D8Cu, side)` in `hit_flash_pair`. Consumes `fighter_ctx_same` (allow-mode `0x33950`), `SLOT_PTRS`. Record §P2.6 (note its correction: `0x21374` starts ctx[4], the own record).

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


sub("tools/diff_verify.py", '''    ], calls=(HIT_B,), eax_mask=0, mutants=("@mutant", "@side")),
]
''', '''    ], calls=(HIT_B,), eax_mask=0, mutants=("@mutant", "@side")),
]

# 0x34D8C (hit_flash_pair): EAX = side, a plain `ret`; it pushes EBX and EDX and pops both (record §P2.6).
FLASH = E.Call(0x34D8C, ("eax",))


# The context-built move callbacks (record §P2.6) read EBX (side) alone: 0x33950(side) runs on both sides
# (allow) and they store into ctx[2], the side's own slot DS_SLOTS + side * 0x94, not the EAX slot. Both slots'
# records are SLOT_PTRS; the two slots' characters differ (5 and 3), so a port that reads the other slot's
# character differs; every field written carries a sentinel (+0x0C..+0x1F, +0x41/+0x42, +0x52..+0x57).
def p2_ctx_case(cid, side, extra=None):
    own = DS_SLOTS + side * 0x94
    return Case(cid, {"eax": E3_SLOT, "edx": E3_OUT, "ebx": side},
                {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\\x05", DS_SLOTS + 0x94 + 0x7A: b"\\x03",
                 own + 0x0C: b"".join(le32(v * 0x01010101) for v in (0x0C, 0x10, 0x14, 0x18, 0x1C)),
                 own + 0x41: b"\\x41\\x42", own + 0x52: b"\\x52\\x53\\x54\\x55\\x56\\x57", **(extra or {})})


# 0x22938: the other slot's (ctx[3]) +0x42 bit 4 refuses (t0, 0x10 alone; t1/t2 0xEF, every other bit); the
# per-side words 0x104754/0x104758 and the floats 0x104738 (both sides' in one poke each) carry sentinels.
P2_22938_SEED = {0x104754: b"\\x54\\x47\\x56\\x47\\x58\\x47\\x5a\\x47", 0x104738: b"\\x38\\x47\\x00\\x00\\x3c\\x47\\x00\\x00"}
P2_SPECS += [
    Spec("fighter_21374", 0x21374, [p2_ctx_case("s0", 0), p2_ctx_case("s1", 1)],
         allow_calls=(0x33950,), calls=(HIT_B,), eax_mask=0),
    Spec("fighter_22938", 0x22938, [
        p2_ctx_case("t0", 0, {**P2_22938_SEED, DS_SLOTS + 0x94 + 0x42: b"\\x10"}),
        p2_ctx_case("t1", 0, {**P2_22938_SEED, DS_SLOTS + 0x94 + 0x42: b"\\xef"}),
        p2_ctx_case("t2", 1, {**P2_22938_SEED, DS_SLOTS + 0x42: b"\\xef"}),
    ], allow_calls=(0x33950,), calls=(HIT_B, FLASH), eax_mask=0, mutants=("@mutant", "@order")),
]
''')
T = "tools/tests/test_diff_verify.py"
sub(T, '''            "fighter_15478": 0, "fighter_3dcec": 0, "fighter_21114": 0}''',
    '''            "fighter_15478": 0, "fighter_3dcec": 0, "fighter_21114": 0,
            "fighter_21374": 0, "fighter_22938": 0}''')
sub(T, '''            "fighter_21114@mutant": {"call #0"}, "fighter_21114@side": {"byte", "call #0"}}''',
    '''            "fighter_21114@mutant": {"call #0"}, "fighter_21114@side": {"byte", "call #0"},
            "fighter_21374@mutant": {"call #0"}, "fighter_22938@mutant": {"call #1"},
            "fighter_22938@order": {"call #0 memory", "call #1 memory"}}''')
sub(T, '''                                 0x188AC: ("edx",), 0x38034: ()})''',
    '''                                 0x188AC: ("edx",), 0x38034: (), 0x34D8C: ()})''')
sub(T, '''        self.assertIn("diff-verify: 45/45 functions VERIFIED; 64/64 mutants detected; 1 named gaps; "
                      "5/32 rows with callees closed (13 have none).", out.getvalue())''',
    '''        self.assertIn("diff-verify: 47/47 functions VERIFIED; 67/67 mutants detected; 1 named gaps; "
                      "6/34 rows with callees closed (13 have none).", out.getvalue())''')
print("t5_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p2_img.bin --function fighter_22938 | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected:

```
t5_spec applied
| fighter_22938 | 0x22938 | 3 | 3/3 | MISMATCH | 33950 allow unverified, 34D8C stub unverified, 3C4CC stub unverified |
  fighter_22938: t0: port: unknown binding fighter_22938
  fighter_22938: t1: port: unknown binding fighter_22938
  fighter_22938: t2: port: unknown binding fighter_22938
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
/* §P2.6: the two move callbacks that build 0x33950's context and arm the
 * side's own slot with three callbacks, through their registrations (side 0:
 * ctx[2] = slot 0; the EAX slot and EDX record are not read). */
static void p2_check_arming(void)
{
    p2_cb_fn f;
    u32 i;
    static const u32 imm[6] = { 0x000213C0u, 0x000213CBu, 0x000213D6u, 0x00022963u, 0x0002296Eu, 0x00022979u };
    static const u32 cb[6] = { 0x000212CCu, 0x0002116Cu, 0x000211F0u, 0x00022638u, 0x00022510u, 0x00022588u };
    CHECK(fn_resolve(0x21374u) == (void (*)(void))fighter_21374, "0x21374 is registered");
    CHECK(fn_resolve(0x22938u) == (void (*)(void))fighter_22938, "0x22938 is registered");
    CHECK_EQ_INT((int)DSD(0x000A55BCu), 0x00021374);
    CHECK_EQ_INT((int)DSD(0x000A3CD0u), 0x00022938);
    for (i = 0; i < 6u; i++) CHECK_EQ_INT((int)DSD(imm[i]), (int)cb[i]);

    /* 0x21374 on side 0 (slot 0 character 2): its own record on 0xC8950[2]
     * at 2.0, slot 0 7/9/0 with +0x0C/+0x18/+0x1C = 0x212CC/0x2116C/0x211F0,
     * +0x57 = 0 and +0x41 bit 7. */
    f = (p2_cb_fn)(void *)fn_resolve(0x21374u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_S0 + 0x7Au) = 2u;
    DSB(Z_S1 + 0x7Au) = 4u;
    DSW(DSD(0x000C8950u + 2u * 4u)) = 0x12B1u;
    DSB(Z_S0 + 0x41u) = 0x01u;
    DSB(Z_S0 + 0x54u) = 0x44u;
    DSB(Z_S0 + 0x57u) = 0x77u;
    f(0x0A0A0A0Au, 0x0B0B0B0Bu, 0u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), (int)DSD(0x000C8950u + 2u * 4u));
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x0Cu), 0x000212CC);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x18u), 0x0002116C);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x1Cu), 0x000211F0);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 0);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x41u), 0x81);

    /* 0x22938 on side 0: refused while slot 1's +0x42 has bit 4; else slot 0
     * armed with 0x22638/0x22510/0x22588, 9/7/0 and +0x42 bit 2, the side's
     * words 0x104758/0x104754 = 0 and its float 0x104738 = 3.0, the record on
     * 0xE4DB4 at 3.0 and 0x34D8C(1) (DS_001078FA = 2: slot 1's record +0x59
     * = 1, slot 0's = 0xFF). */
    f = (p2_cb_fn)(void *)fn_resolve(0x22938u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_S1 + 0x42u) = 0x10u;
    DSB(Z_S0 + 0x53u) = 0x33u;
    f(0x0A0A0A0Au, 0x0B0B0B0Bu, 0u);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 0x33);
    z_fseed();
    DSB(Z_S1 + 0x42u) = 0xEFu;
    DSB(Z_S0 + 0x42u) = 0x00u;
    DSB(Z_S0 + 0x54u) = 0x44u;
    DSB(Z_S0 + 0x57u) = 0x77u;
    DSW(0x000E4DB4u) = 0x12B1u;
    DSW(0x00104754u) = 0x5454u;
    DSW(0x00104758u) = 0x5858u;
    DSD(0x00104738u) = 0x38383838u;
    DSB(DS_001078FA) = 2u;
    DSB(Z_R0 + 0x59u) = 0x59u;
    DSB(Z_R1 + 0x59u) = 0x59u;
    f(0x0A0A0A0Au, 0x0B0B0B0Bu, 0u);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x0Cu), 0x00022638);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x18u), 0x00022510);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x1Cu), 0x00022588);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 0);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x42u), 4);
    CHECK_EQ_INT((int)DSW(0x00104754u), 0);
    CHECK_EQ_INT((int)DSW(0x00104758u), 0);
    CHECK_EQ_INT((int)DSD(0x00104738u), 0x40400000);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000E4DB4);
    CHECK_EQ_INT((int)DSB(Z_R1 + 0x59u), 1);
    CHECK_EQ_INT((int)DSB(Z_R0 + 0x59u), 0xFF);
}

int test_p2_arming(void)        { return u6b_run(p2_check_arming); }
'''
ANCHOR = "int test_p2_unconditional(void) { return u6b_run(p2_check_unconditional); }\n"
sub("port/tests/test_fight.c", ANCHOR, ANCHOR + TEST)
sub("port/tests/test.h", "    X(test_p2_unconditional) \\\n", "    X(test_p2_unconditional) \\\n    X(test_p2_arming) \\\n")
print("t5_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected:

```
t5_test applied
port/tests/test_fight.c:45260:51: error: use of undeclared identifier 'fighter_21374'
port/tests/test_fight.c:45261:51: error: use of undeclared identifier 'fighter_22938'
2 errors generated.
```

- [ ] **Step 3: the port and the seam.**

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
#define P2_STREAMS_21374 0x000C8950u  /* 0x21391: [char] the stream 0x21374 starts */
#define P2_ANIM_22938    0x000E4DB4u  /* 0x229A2 */
#define P2_104738        0x00104738u  /* 0x229D8: a float per side (0x22638's scale) */
#define P2_104754        0x00104754u  /* 0x229D0: a word per side (0x22638's latched command) */
#define P2_104758        0x00104758u  /* 0x2299A: a word per side (0x22638's frame count) */

/* 0x21374 — record §P2.6. Character 6's reaction-0x21 callback (the dword at
 * 0xA55BC). EBX = side (`mov edx,ebx` at 0x21377 overwrites the EDX record;
 * the EAX slot is not read); the context is 0x33950(side). The side's own
 * record (ctx[4]: `[esp+0x14]` after the push at 0x21384) on 0xC8950[its
 * slot's character] at 2.0 (0x3C4CC); the own slot (ctx[2]) 7/9/0 with the +0x0C callback 0x212CC (0x3531C case 7), the
 * +0x18 hook 0x2116C (0x19020) and the +0x1C callback 0x211F0 (0x193B0's
 * 0x19505), +0x57 = 0, +0x41 bit 7. PORT: AL = 1 (0x213EA) unread (§P2.2). */
void fighter_21374(u32 slot, u32 rec, u32 side)
{
    u32 ctx[6];
    (void)slot;
    (void)rec;
    fighter_ctx_same(ctx, side);                            /* 0x21377..0x2137B 0x33950 */
    hit_anim_start_b(ctx[4], DSD(P2_STREAMS_21374 + (u32)DSB(ctx[2] + 0x7Au) * 4u),
                     0x40000000u);                          /* 0x21380..0x2139C 0x3C4CC */
    DSB(ctx[2] + 0x53u) = 7u;                               /* 0x213A5 */
    DSB(ctx[2] + 0x52u) = 9u;                               /* 0x213AD */
    DSB(ctx[2] + 0x54u) = 0u;                               /* 0x213B5 */
    DSD(ctx[2] + 0x0Cu) = 0x000212CCu;                      /* 0x213BD */
    DSD(ctx[2] + 0x18u) = 0x0002116Cu;                      /* 0x213C8 */
    DSD(ctx[2] + 0x1Cu) = 0x000211F0u;                      /* 0x213D3 */
    DSB(ctx[2] + 0x57u) = 0u;                               /* 0x213DE */
    DSB(ctx[2] + 0x41u) = (u8)(DSB(ctx[2] + 0x41u) | 0x80u);   /* 0x213E6 */
}

/* 0x22938 — record §P2.6. Character 1's reaction-0x22 callback (the dword at
 * 0xA3CD0). EBX = side; the context is 0x33950(side). The other slot's +0x42
 * bit 4 refuses: AL = 0, nothing written. Else the own slot: +0x57 = 0, the
 * +0x0C callback 0x22638, the +0x18 hook 0x22510, the +0x1C callback 0x22588,
 * 9/0/7 (+0x52/+0x54/+0x53); the side's word 0x104758 = 0; the own record on
 * 0xE4DB4 at 3.0 (0x3C4CC); 0x34D8C(the other side); the own slot's +0x42
 * bit 2; the side's word 0x104754 = 0 and float 0x104738 = 3.0. PORT: AL
 * unread (§P2.2). */
void fighter_22938(u32 slot, u32 rec, u32 side)
{
    u32 ctx[6];
    (void)slot;
    (void)rec;
    fighter_ctx_same(ctx, side);                            /* 0x2293B..0x2293F 0x33950 */
    if ((DSB(ctx[3] + 0x42u) & 0x10u) != 0u) return;        /* 0x22944..0x2294C */
    DSB(ctx[2] + 0x57u) = 0u;                               /* 0x22958 */
    DSD(ctx[2] + 0x0Cu) = 0x00022638u;                      /* 0x22960 */
    DSD(ctx[2] + 0x18u) = 0x00022510u;                      /* 0x2296B */
    DSD(ctx[2] + 0x1Cu) = 0x00022588u;                      /* 0x22976 */
    DSB(ctx[2] + 0x52u) = 9u;                               /* 0x22981 */
    DSB(ctx[2] + 0x54u) = 0u;                               /* 0x22989 */
    DSB(ctx[2] + 0x53u) = 7u;                               /* 0x22991 */
    DSW(P2_104758 + ctx[0] * 2u) = 0u;                      /* 0x22995..0x2299A */
    hit_anim_start_b(ctx[4], P2_ANIM_22938, 0x40400000u);   /* 0x229A2..0x229B0 0x3C4CC */
    hit_flash_pair(ctx[1]);                                 /* 0x229B5/0x229B9 0x34D8C */
    DSB(ctx[2] + 0x42u) = (u8)(DSB(ctx[2] + 0x42u) | 4u);   /* 0x229C2 */
    DSW(P2_104754 + ctx[0] * 2u) = 0u;                      /* 0x229C6..0x229D0 */
    DSD(P2_104738 + ctx[0] * 4u) = 0x40400000u;             /* 0x229CB/0x229D8 */
}
'''
BINDINGS = r'''/* §P2.6: the context-built move callbacks (0x34E2C at 0x35045), mask 0. */
static void b_21374(const u32 *r, u32 *eax)            { fighter_21374(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_22938(const u32 *r, u32 *eax)            { fighter_22938(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_21374(const u32 *r, u32 *eax)            /* the stream by the other slot's character */
{
    u32 ctx[6];
    fighter_ctx_same(ctx, r[R_EBX]);
    hit_anim_start_b(ctx[4], DSD(0x000C8950u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40000000u);
    DSB(ctx[2] + 0x53u) = 7u;
    DSB(ctx[2] + 0x52u) = 9u;
    DSB(ctx[2] + 0x54u) = 0u;
    DSD(ctx[2] + 0x0Cu) = 0x000212CCu;
    DSD(ctx[2] + 0x18u) = 0x0002116Cu;
    DSD(ctx[2] + 0x1Cu) = 0x000211F0u;
    DSB(ctx[2] + 0x57u) = 0u;
    DSB(ctx[2] + 0x41u) = (u8)(DSB(ctx[2] + 0x41u) | 0x80u);
    *eax = 0u;
}
static void m_22938_at(const u32 *r, u32 *eax, int own_flash, int late_first)
{
    u32 ctx[6];
    *eax = 0u;
    fighter_ctx_same(ctx, r[R_EBX]);
    if ((DSB(ctx[3] + 0x42u) & 0x10u) != 0u) return;
    DSB(ctx[2] + 0x57u) = 0u;
    DSD(ctx[2] + 0x0Cu) = 0x00022638u;
    DSD(ctx[2] + 0x18u) = 0x00022510u;
    DSD(ctx[2] + 0x1Cu) = 0x00022588u;
    DSB(ctx[2] + 0x52u) = 9u;
    DSB(ctx[2] + 0x54u) = 0u;
    DSB(ctx[2] + 0x53u) = 7u;
    DSW(0x00104758u + ctx[0] * 2u) = 0u;
    if (late_first) {
        DSW(0x00104754u + ctx[0] * 2u) = 0u;
        DSD(0x00104738u + ctx[0] * 4u) = 0x40400000u;
    }
    hit_anim_start_b(ctx[4], 0x000E4DB4u, 0x40400000u);
    hit_flash_pair(own_flash ? ctx[0] : ctx[1]);
    DSB(ctx[2] + 0x42u) = (u8)(DSB(ctx[2] + 0x42u) | 4u);
    DSW(0x00104754u + ctx[0] * 2u) = 0u;
    DSD(0x00104738u + ctx[0] * 4u) = 0x40400000u;
}
static void m_22938(const u32 *r, u32 *eax)            /* 0x34D8C on the own side */
{
    m_22938_at(r, eax, 1, 0);
}
static void m_22938_order(const u32 *r, u32 *eax)      /* the word 0x104754 and the float before the calls */
{
    m_22938_at(r, eax, 0, 1);
}

'''
sub("port/src/game/fighter.c", '''    DSB(slot + 0x54u) = 1u;                                 /* 0x21162 */
}
''', '''    DSB(slot + 0x54u) = 1u;                                 /* 0x21162 */
}
''' + FIGHTER_C)
sub("port/src/game/fighter.c", '''void hit_flash_pair(u32 side)
{
''', '''void hit_flash_pair(u32 side)
{
    PR_SEAM(0x34D8Cu, side);
''')
sub("port/src/game/fighter.h", "void fighter_21114(u32 slot, u32 rec, u32 side);\n",
    """void fighter_21114(u32 slot, u32 rec, u32 side);
/* §P2.6: the move callbacks that arm the side's own slot (slot, rec, side). */
void fighter_21374(u32 slot, u32 rec, u32 side);
void fighter_22938(u32 slot, u32 rec, u32 side);
""")
sub("port/src/game/actors.c", "    fn_register(0x21114u, (void (*)(void))fighter_21114);\n",
    """    fn_register(0x21114u, (void (*)(void))fighter_21114);
    /* PORT: record 2026-10-02-reverse-p2 §P2.6. Character 6's reaction-0x21
     * and character 1's reaction-0x22 callbacks (the dwords 0xA55BC and
     * 0xA3CD0; 0x34E2C at 0x35045, (slot, rec, side)), which arm the side's
     * slot with the callbacks registered below. */
    fn_register(0x21374u, (void (*)(void))fighter_21374);
    fn_register(0x22938u, (void (*)(void))fighter_22938);
""")
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_21114@side",       m_21114_side,   0x00000000u },\n',
    """    { "fighter_21114@side",       m_21114_side,   0x00000000u },
    { "fighter_21374",            b_21374,        0x00000000u },
    { "fighter_22938",            b_22938,        0x00000000u },
    { "fighter_21374@mutant",     m_21374,        0x00000000u },
    { "fighter_22938@mutant",     m_22938,        0x00000000u },
    { "fighter_22938@order",      m_22938_order,  0x00000000u },
""")
print("t5_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_21374 fighter_22938; do
  python3 tools/diff_verify.py --image /tmp/pr_p2_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected:

```
t5_port applied
all checks passed
| fighter_21374 | 0x21374 | 2 | 1/1 | VERIFIED | 33950 allow unverified, 3C4CC stub unverified |
| fighter_21374@mutant | 0x21374 | 2 | 1/1 | MISMATCH | 33950 allow unverified, 3C4CC stub unverified |
| fighter_22938 | 0x22938 | 3 | 3/3 | VERIFIED | 33950 allow unverified, 34D8C stub unverified, 3C4CC stub unverified |
| fighter_22938@mutant | 0x22938 | 3 | 3/3 | MISMATCH | 33950 allow unverified, 34D8C stub unverified, 3C4CC stub unverified |
| fighter_22938@order | 0x22938 | 3 | 3/3 | MISMATCH | 33950 allow unverified, 34D8C stub unverified, 3C4CC stub unverified |
```

- [ ] **Step 4: the E2 table.** Expected:

```
entry-triage: FAIL: docs/superpowers/plans/2026-10-01-reverse-e2-triage.md differs from a fresh run (regenerate it with --out)
make: *** [entry-triage] Error 1
entry-triage: targets 297 unported, 198 ported; supplement 131 (28 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 33 in unported code, 82 in ported code, 19 nowhere
| callbacks | 9 | 62 |
| finishers | 0 | 9 |
| animation-targets | 54 | 58 |
| stubs | 60 |
 1 file changed, 4 insertions(+), 4 deletions(-)
```

- [ ] **Step 5: the task gate.** Expected:

```
Ran 157 tests in N.NNNs
OK
diff-verify: 47/47 functions VERIFIED; 67/67 mutants detected; 1 named gaps; 6/34 rows with callees closed (13 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
Ran 44 tests in N.NNNs
OK
entry-triage: targets 297 unported, 198 ported; supplement 131 (28 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 6: every new unit assertion can fail.**

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


mutate("port/src/game/actors.c", "    fn_register(0x22938u, (void (*)(void))fighter_22938);\n", "")
mutate("port/src/game/fighter.c",
       "    hit_flash_pair(ctx[1]);                                 /* 0x229B5/0x229B9 0x34D8C */",
       "    hit_flash_pair(ctx[0]);                                 /* 0x229B5/0x229B9 0x34D8C */")
mutate("port/src/game/fighter.c",
       "    DSB(ctx[2] + 0x41u) = (u8)(DSB(ctx[2] + 0x41u) | 0x80u);   /* 0x213E6 */",
       "    DSB(ctx[2] + 0x41u) = (u8)(DSB(ctx[2] + 0x41u) | 0x40u);   /* 0x213E6 */")
PY
```

Expected, then `all checks passed`:

```
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:45261: 0x22938 is registered']
port/src/game/fighter.c: 2 FAIL, first: ['test_fight.c:45327: 255 != 1']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45288: 65 != 129']
all checks passed
```

- [ ] **Step 7: commit** (Task 2's files), message `fighter: port the slot-arming move callbacks 0x21374 and 0x22938, seam 0x34D8C; E2 table regenerated (track P batch 2)` and the trailer.

---

### Task 6: the +0x18 hooks `0x2116C` and `0x22510` (seam `0x18C14` by value; the registration table)

**Files:** as Task 2, plus `tools/diff_emu.py`, `port/src/mem.c`, `port/tests/test_platform.c`.

**Interfaces:** produces `u32 fighter_2116c(u32 side)`, `u32 fighter_22510(u32 side)` (as `0x19020` calls them; EAX tested whole: binding mask `0xFFFFFFFF`); `diff_emu.deref_arg(a)` and `E.Call` arguments `[reg]`/`[reg+N]`; `CHECKS`, `p2_2116c`, `p2_22510`; `P2_FLAGS_DW(f, k)` and `PR_SEAM_RET(0x18C14u, side, 4 flag dwords, box_a, box_b)` in `fighter_18c14`; `fn_register` idempotent for a repeated pair; `test_fn_register_repeats`. Consumes `fighter_18bd4` (allow-mode `0x18BD4`), `fighter_18c14`, `fighter_19020`. Record §P2.7.

- [ ] **Step 0: the harness reads a stack buffer by value.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


# tools/diff_emu.py: a Call argument may name the dword at a register plus an offset, `[reg+N]` (record
# 2026-10-02-reverse-p2 §P2.7): a callee handed a pointer to the caller's stack (0x18C14's 16 flag bytes)
# has no mem[] offset to compare, so the bytes it points at are compared instead.
E = "tools/diff_emu.py"
sub(E, '''REGS = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")
''', '''REGS = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")


def deref_arg(a):
    """(reg, offset) for a Call argument `[reg]` or `[reg+N]` (N decimal): the dword at that address when
    the callee is reached, for a pointer argument with no mem[] offset to compare (a caller's stack buffer;
    record 2026-10-02-reverse-p2 §P2.7). None for any other argument."""
    if not (a.startswith("[") and a.endswith("]")):
        return None
    reg, _, off = a[1:-1].partition("+")
    if reg not in REGS or (off and not off.isdigit()):
        return None
    return reg, int(off or "0")
''')
sub(E, '''        bad = [a for a in self.args if a not in REGS + STACK_ARGS]''',
    '''        bad = [a for a in self.args if a not in REGS + STACK_ARGS and deref_arg(a) is None]''')
sub(E, '''    def arg(uc, esp, a):
        if a in STACK_ARGS:
            return int.from_bytes(uc.mem_read(esp + 4 + 4 * STACK_ARGS.index(a), 4), "little")
        return uc.reg_read(names[a])
''', '''    def arg(uc, esp, a):
        if a in STACK_ARGS:
            return int.from_bytes(uc.mem_read(esp + 4 + 4 * STACK_ARGS.index(a), 4), "little")
        d = deref_arg(a)
        if d is not None:
            return int.from_bytes(uc.mem_read((uc.reg_read(names[d[0]]) + d[1]) & 0xFFFFFFFF, 4), "little")
        return uc.reg_read(names[a])
''')

T = "tools/tests/test_diff_verify.py"
sub(T, '''class CallParseTests(unittest.TestCase):''', '''# 10000: mov edx,0x80010; call 0x10020; ret    10020: ret    80010: 11 22 33 44 55 66 77 88
DEREF = program({0x10000: "BA10000800" "E816000000" "C3", 0x10020: "C3", 0x80010: "1122334455667788"})


@needs_unicorn
class DerefArgTests(unittest.TestCase):
    """A Call argument `[reg+N]` is the dword at reg + N when the callee is reached (record
    2026-10-02-reverse-p2 §P2.7: 0x18C14's flag bytes live on its caller's stack)."""

    def test_a_deref_argument_reads_the_dword_the_register_points_at(self):
        r = E.run_original(DEREF, 0x10000, calls=(E.Call(0x10020, ("edx", "[edx]", "[edx+4]")),))
        self.assertEqual(r.outcome, "ok")
        self.assertEqual(r.calls, [(0x10020, (0x80010, 0x44332211, 0x88776655))])

    def test_a_malformed_deref_argument_is_refused(self):
        for a in ("[esp]", "[edx+x]", "[edx-4]", "edx+4", "[edx"):
            with self.assertRaises(ValueError, msg=a):
                E.Call(0x10020, ("eax", a))


class CallParseTests(unittest.TestCase):''')
print("t6_harness applied")
PY
python3 -m unittest tools.tests.test_diff_verify.DerefArgTests 2>&1 | tail -1
```

Expected:

```
t6_harness applied
OK
```

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


sub("tools/diff_verify.py", '''    ], allow_calls=(0x33950,), calls=(HIT_B, FLASH), eax_mask=0, mutants=("@mutant", "@order")),
]
''', '''    ], allow_calls=(0x33950,), calls=(HIT_B, FLASH), eax_mask=0, mutants=("@mutant", "@order")),
]

# 0x18C14 (fighter_18c14): EAX = side, EDX = the 16 flag bytes (a buffer on the caller's stack: compared by value,
# the four dwords at EDX, record §P2.7), EBX/ECX = the two box tables; a plain `ret`; it clobbers EBX, EDX and
# EBP (E.callee_clobbers). 0x18BD4 fills the flags (16 bytes of 2 at EAX, a leaf) and runs on both sides.
CHECKS = E.Call(0x18C14, ("eax", "[edx]", "[edx+4]", "[edx+8]", "[edx+12]", "ebx", "ecx"),
                clobbers=("ebx", "edx", "ebp"))


# The slot +0x18 hooks (record §P2.7) run as 0x19020 calls them at 0x1903F: EAX = side; 0x19048 tests the whole
# EAX (`test eax,eax`): mask 0xFFFFFFFF, and the 0x18C14 stub's EAX (returned as is) varies per case. The side's
# slot (ctx[2]) is DS_SLOTS + side * 0x94; 0x2116C bounds its word +0x88 by the words 0xA81AC (1) and 0xA81AE
# (3), signed (`jl`/`jle`); k4 pokes the bounds to -16 and 16 with the word -1, where an unsigned compare
# returns 1 without the call.
def p2_2116c(cid, side, w88, extra=None, stub=None):
    return Case(cid, {"eax": side}, {**SLOT_PTRS, DS_SLOTS + side * 0x94 + 0x88: le32(w88)[:2], **(extra or {})},
                {0x18C14: stub} if stub is not None else {})


# 0x22510 bounds the side's word 0x104758 (the high half of the dword at 0x104756 + 2 * side, `sar 0x10`) by
# 0xD..0x14 (`jg`/`jge`); both sides' words in one poke.
def p2_22510(cid, side, w, stub=None):
    words = [0x5858, 0x5A5A]
    words[side] = w
    return Case(cid, {"eax": side}, {**SLOT_PTRS, 0x104758: le32(words[0])[:2] + le32(words[1])[:2]},
                {0x18C14: stub} if stub is not None else {})


P2_SPECS += [
    Spec("fighter_2116c", 0x2116C, [
        p2_2116c("k0", 0, 4), p2_2116c("k1", 0, 3, stub=0), p2_2116c("k2", 1, 1, stub=0x12345678),
        p2_2116c("k3", 1, 0), p2_2116c("k4", 0, 0xFFFF, {0xA81AC: b"\\xf0\\xff\\x10\\x00"}, stub=7),
    ], allow_calls=(0x33950, 0x18BD4), calls=(CHECKS,), mutants=("@mutant", "@unsigned", "@eax")),
    Spec("fighter_22510", 0x22510, [
        p2_22510("j0", 0, 0x15), p2_22510("j1", 0, 0x14, 0), p2_22510("j2", 1, 0xD, 0x9ABCDEF0),
        p2_22510("j3", 1, 0xC), p2_22510("j4", 0, 0xFFFF),
    ], allow_calls=(0x33950, 0x18BD4), calls=(CHECKS,), mutants=("@mutant", "@ge")),
]
''')
T = "tools/tests/test_diff_verify.py"
sub(T, '''            "fighter_21374": 0, "fighter_22938": 0}''',
    '''            "fighter_21374": 0, "fighter_22938": 0, "fighter_2116c": 0xFFFFFFFF, "fighter_22510": 0xFFFFFFFF}''')
sub(T, '''            "fighter_22938@order": {"call #0 memory", "call #1 memory"}}''',
    '''            "fighter_22938@order": {"call #0 memory", "call #1 memory"},
            "fighter_2116c@mutant": {"call #0"}, "fighter_2116c@unsigned": {"eax", "call #0"},
            "fighter_2116c@eax": {"eax"}, "fighter_22510@mutant": {"call #0"},
            "fighter_22510@ge": {"eax", "call #0"}}''')
sub(T, '''                         ["w0", "w1", "w2", "w3", "w4", "w5", "w6"])
''', '''                         ["w0", "w1", "w2", "w3", "w4", "w5", "w6"])
        # the hooks' bounds: only k4's -16..16 tells the signed compares from unsigned ones; only j1's word
        # 0x14 tells `jg` from `jge`; the stub's EAX is returned (k1/k2/k4 differ when the port returns 1)
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_2116c@unsigned"].problems}), ["k4"])
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_22510@ge"].problems}), ["j1"])
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_2116c@eax"].problems}),
                         ["k1", "k2", "k4"])
''')
sub(T, '''                                 0x188AC: ("edx",), 0x38034: (), 0x34D8C: ()})''',
    '''                                 0x188AC: ("edx",), 0x38034: (), 0x34D8C: (),
                                 0x18C14: ("ebx", "edx", "ebp")})''')
sub(T, '''        self.assertIn("diff-verify: 47/47 functions VERIFIED; 67/67 mutants detected; 1 named gaps; "
                      "6/34 rows with callees closed (13 have none).", out.getvalue())''',
    '''        self.assertIn("diff-verify: 49/49 functions VERIFIED; 72/72 mutants detected; 1 named gaps; "
                      "6/36 rows with callees closed (13 have none).", out.getvalue())''')
print("t6_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p2_img.bin --function fighter_2116c | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected:

```
t6_spec applied
| fighter_2116c | 0x2116C | 5 | 5/5 | MISMATCH | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
  fighter_2116c: k0: port: unknown binding fighter_2116c
  fighter_2116c: k1: port: unknown binding fighter_2116c
  fighter_2116c: k2: port: unknown binding fighter_2116c
  fighter_2116c: k3: port: unknown binding fighter_2116c
  fighter_2116c: k4: port: unknown binding fighter_2116c
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
typedef u32 (*p2_hook_fn)(u32 side);

/* §P2.7: the slot +0x18 hooks 0x21374 and 0x22938 store, through 0x19020
 * (DS_00100AF8[side] = 1 when the hook returns 0, else 0): out of their
 * bounds they return 1; inside, they return 0x18C14's result on their flags
 * (the same call made here on the expected flags, on the same seeded state). */
static void p2_check_hooks(void)
{
    p2_hook_fn h;
    u8 fl[16];
    u32 r, k;
    CHECK(fn_resolve(0x2116Cu) == (void (*)(void))fighter_2116c, "0x2116C is registered");
    CHECK(fn_resolve(0x22510u) == (void (*)(void))fighter_22510, "0x22510 is registered");

    /* 0x2116C on side 0: the slot's word +0x88 = 4 lies above 0xA81AE's 3. */
    z_fseed();
    DSD(Z_S0 + 0x18u) = 0x0002116Cu;
    DSW(Z_S0 + 0x88u) = 4u;
    DSD(DS_00100AF8) = 0xAAAAAAAAu;
    fighter_19020(0u);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0);
    /* inside 1..3: 0x18C14(0, flags 1/4/7/8/0xD/0xE = 0, 5 = 1, the rest 2,
     * 0xA81BE, 0xA81C8) */
    h = (p2_hook_fn)(void *)fn_resolve(0x2116Cu);
    if (h == NULL) return;
    z_fseed();
    DSW(Z_S0 + 0x88u) = 2u;
    r = h(0u);
    z_fseed();
    DSW(Z_S0 + 0x88u) = 2u;
    for (k = 0; k < 16u; k++) fl[k] = 2u;
    fl[1] = fl[4] = fl[7] = fl[8] = fl[0xD] = fl[0xE] = 0u;
    fl[5] = 1u;
    CHECK_EQ_INT((int)r, fighter_18c14(0u, fl, 0x000A81BEu, 0x000A81C8u));
    z_fseed();
    DSW(Z_S0 + 0x88u) = 0u;
    CHECK_EQ_INT((int)h(0u), 1);

    /* 0x22510 on side 1: the side's word 0x104758 + 2 = 0x15 lies above 0x14;
     * 0x10 inside: 0x18C14(1, flags 1/4/7/8/0xD/0xE = 0, 5/9 = 1, 0xA82C4,
     * 0xA82CE). */
    h = (p2_hook_fn)(void *)fn_resolve(0x22510u);
    if (h == NULL) return;
    z_fseed();
    DSW(0x0010475Au) = 0x15u;
    CHECK_EQ_INT((int)h(1u), 1);
    z_fseed();
    DSW(0x0010475Au) = 0x10u;
    r = h(1u);
    z_fseed();
    DSW(0x0010475Au) = 0x10u;
    for (k = 0; k < 16u; k++) fl[k] = 2u;
    fl[1] = fl[4] = fl[7] = fl[8] = fl[0xD] = fl[0xE] = 0u;
    fl[5] = fl[9] = 1u;
    CHECK_EQ_INT((int)r, fighter_18c14(1u, fl, 0x000A82C4u, 0x000A82CEu));
}

int test_p2_hooks(void)         { return u6b_run(p2_check_hooks); }
'''
ANCHOR = "int test_p2_arming(void)        { return u6b_run(p2_check_arming); }\n"
sub("port/tests/test_fight.c", ANCHOR, ANCHOR + TEST)
sub("port/tests/test.h", "    X(test_p2_arming) \\\n", "    X(test_p2_arming) \\\n    X(test_p2_hooks) \\\n")
print("t6_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected:

```
t6_test applied
port/tests/test_fight.c:45344:51: error: use of undeclared identifier 'fighter_2116c'
port/tests/test_fight.c:45345:51: error: use of undeclared identifier 'fighter_22510'
2 errors generated.
```

- [ ] **Step 3: the port and the seam.** The suite then aborts: the registration table overflows (record §P2.7).

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
#define P2_A81AC         0x000A81ACu  /* 0x211BC: 0x2116C's low bound word */
#define P2_A81AE         0x000A81AEu  /* 0x211A8: its high bound word */
#define P2_BOX_2116C_A   0x000A81BEu  /* 0x211D8 (EBX) */
#define P2_BOX_2116C_B   0x000A81C8u  /* 0x211D3 (ECX) */
#define P2_BOX_22510_A   0x000A82C4u  /* 0x2256F (EBX) */
#define P2_BOX_22510_B   0x000A82CEu  /* 0x2256A (ECX) */

/* 0x2116C — record §P2.7. The slot +0x18 hook 0x21374 stores (the dword at
 * 0x213CB; 0x19020's call at 0x1903F, fn(side), the whole EAX tested). The
 * context is 0x33950(side); the flags from 0x18BD4 with 1, 8, 4, 0xE, 7 and
 * 0xD = 0 and 5 = 1. With the side's slot word +0x88 above the word 0xA81AE
 * or below the word 0xA81AC (signed) it returns 1; else 0x18C14(side, the
 * flags, 0xA81BE, 0xA81C8)'s result. */
u32 fighter_2116c(u32 side)
{
    u32 ctx[6];
    u8 flags[16];
    fighter_ctx_same(ctx, side);                            /* 0x21172..0x21176 0x33950 */
    fighter_18bd4(flags);                                   /* 0x2117B/0x2117F 0x18BD4 */
    flags[1] = 0;                                           /* 0x21188 */
    flags[8] = 0;                                           /* 0x2118C */
    flags[4] = 0;                                           /* 0x21190 */
    flags[0xE] = 0;                                         /* 0x21194 */
    flags[7] = 0;                                           /* 0x21198 */
    flags[5] = 1u;                                          /* 0x2119C */
    flags[0xD] = 0;                                         /* 0x211A4 */
    if ((s16)DSW(P2_A81AE) < (s16)DSW(ctx[2] + 0x88u))     /* 0x211A8..0x211B6 */
        return 1u;                                          /* 0x211CC */
    if ((s16)DSW(P2_A81AC) > (s16)DSW(ctx[2] + 0x88u))     /* 0x211B8..0x211CA */
        return 1u;                                          /* 0x211CC */
    return (u32)fighter_18c14(ctx[0], flags, P2_BOX_2116C_A, P2_BOX_2116C_B);   /* 0x211D3..0x211E4 0x18C14 */
}

/* 0x22510 — record §P2.7. The slot +0x18 hook 0x22938 stores (the dword at
 * 0x2296E; 0x19020, fn(side), the whole EAX tested). The flags 1, 8, 4, 0xE,
 * 7 and 0xD = 0, 5 and 9 = 1. With the side's word 0x104758 (the high half
 * of the dword at 0x104756 + 2 * side, `sar 0x10`) above 0x14 or below 0xD
 * it returns 1; else 0x18C14(side, the flags, 0xA82C4, 0xA82CE)'s result. */
u32 fighter_22510(u32 side)
{
    u32 ctx[6];
    u8 flags[16];
    s32 w;
    fighter_ctx_same(ctx, side);                            /* 0x22516..0x2251A 0x33950 */
    fighter_18bd4(flags);                                   /* 0x2251F/0x22523 0x18BD4 */
    flags[1] = 0;                                           /* 0x2252C */
    flags[8] = 0;                                           /* 0x22530 */
    flags[5] = 1u;                                          /* 0x22534 */
    flags[9] = 1u;                                          /* 0x22538 */
    flags[4] = 0;                                           /* 0x2253F */
    flags[0xE] = 0;                                         /* 0x22543 */
    flags[7] = 0;                                           /* 0x2254E */
    flags[0xD] = 0;                                         /* 0x22555 */
    w = (s32)(s16)DSW(P2_104758 + ctx[0] * 2u);             /* 0x22547/0x22552 */
    if (w > 0x14 || w < 0x0D)                               /* 0x22559..0x22561 */
        return 1u;                                          /* 0x22563 */
    return (u32)fighter_18c14(ctx[0], flags, P2_BOX_22510_A, P2_BOX_22510_B);   /* 0x2256A..0x2257B 0x18C14 */
}
'''
BINDINGS = r'''/* §P2.7: the slot +0x18 hooks as 0x19020 calls them at 0x1903F (EAX = side); 0x19048 tests the whole EAX. */
static void b_2116c(const u32 *r, u32 *eax)            { *eax = fighter_2116c(r[R_EAX]); }
static void b_22510(const u32 *r, u32 *eax)            { *eax = fighter_22510(r[R_EAX]); }
static u32 m_hook(u32 side, u32 lo_box, u32 hi_box, int mode)
{
    u32 ctx[6];
    u8 f[16];
    u32 k;
    fighter_ctx_same(ctx, side);
    for (k = 0; k < 16u; k++) f[k] = 2u;
    f[1] = f[8] = f[4] = f[0xE] = f[7] = 0u;
    f[5] = 1u;
    f[0xD] = (mode == 1) ? 2u : 0u;
    if (lo_box == 0x000A81BEu) {
        if (mode == 2) {
            if (DSW(0x000A81AEu) < DSW(ctx[2] + 0x88u) || DSW(0x000A81ACu) > DSW(ctx[2] + 0x88u)) return 1u;
        } else if ((s16)DSW(0x000A81AEu) < (s16)DSW(ctx[2] + 0x88u)
                   || (s16)DSW(0x000A81ACu) > (s16)DSW(ctx[2] + 0x88u)) {
            return 1u;
        }
        k = (u32)fighter_18c14(ctx[0], f, lo_box, hi_box);
        return mode == 3 ? 1u : k;
    }
    f[9] = (mode == 1) ? 0u : 1u;
    f[0xD] = 0u;
    {
        s32 w = (s32)(s16)DSW(0x00104758u + ctx[0] * 2u);
        if ((mode == 4 ? w >= 0x14 : w > 0x14) || w < 0x0D) return 1u;
    }
    return (u32)fighter_18c14(ctx[0], f, lo_box, hi_box);
}
static void m_2116c(const u32 *r, u32 *eax)            /* flag 0xD left at 2 */
{
    *eax = m_hook(r[R_EAX], 0x000A81BEu, 0x000A81C8u, 1);
}
static void m_2116c_unsigned(const u32 *r, u32 *eax)   /* the bounds compared unsigned */
{
    *eax = m_hook(r[R_EAX], 0x000A81BEu, 0x000A81C8u, 2);
}
static void m_2116c_eax(const u32 *r, u32 *eax)        /* 1 instead of 0x18C14's result */
{
    *eax = m_hook(r[R_EAX], 0x000A81BEu, 0x000A81C8u, 3);
}
static void m_22510(const u32 *r, u32 *eax)            /* flag 9 = 0, not 1 */
{
    *eax = m_hook(r[R_EAX], 0x000A82C4u, 0x000A82CEu, 1);
}
static void m_22510_ge(const u32 *r, u32 *eax)         /* `>= 0x14` for `jg` */
{
    *eax = m_hook(r[R_EAX], 0x000A82C4u, 0x000A82CEu, 4);
}

'''
F = "port/src/game/fighter.c"
sub(F, '''    DSD(P2_104738 + ctx[0] * 4u) = 0x40400000u;             /* 0x229CB/0x229D8 */
}
''', '''    DSD(P2_104738 + ctx[0] * 4u) = 0x40400000u;             /* 0x229CB/0x229D8 */
}
''' + FIGHTER_C)
sub(F, '''int fighter_18c14(u32 side, u8 flags[16], u32 box_a, u32 box_b)
{
''', '''/* PORT: the harness seam passes the 16 flag bytes by value, four
 * little-endian dwords (record 2026-10-02-reverse-p2 §P2.7): the raw's EDX is
 * a buffer on its caller's stack, which has no mem[] offset to compare. */
#define P2_FLAGS_DW(f, k) ((u32)(f)[k] | (u32)(f)[(k) + 1] << 8 | (u32)(f)[(k) + 2] << 16 \\
                           | (u32)(f)[(k) + 3] << 24)

int fighter_18c14(u32 side, u8 flags[16], u32 box_a, u32 box_b)
{
    PR_SEAM_RET(0x18C14u, side, P2_FLAGS_DW(flags, 0), P2_FLAGS_DW(flags, 4), P2_FLAGS_DW(flags, 8),
                P2_FLAGS_DW(flags, 12), box_a, box_b);
''')
sub("port/src/game/fighter.h", "void fighter_22938(u32 slot, u32 rec, u32 side);\n",
    """void fighter_22938(u32 slot, u32 rec, u32 side);
/* §P2.7: the slot +0x18 hooks they store, fn(side) as 0x19020 calls them. */
u32  fighter_2116c(u32 side);
u32  fighter_22510(u32 side);
""")
sub("port/src/game/actors.c", "    fn_register(0x22938u, (void (*)(void))fighter_22938);\n",
    """    fn_register(0x22938u, (void (*)(void))fighter_22938);
    /* PORT: record 2026-10-02-reverse-p2 §P2.7. The slot +0x18 hooks 0x21374
     * and 0x22938 store (the dwords at 0x213CB and 0x2296E; 0x19020 at
     * 0x1903F, fn(side) with EAX returned). */
    fn_register(0x2116Cu, (void (*)(void))fighter_2116c);
    fn_register(0x22510u, (void (*)(void))fighter_22510);
""")
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_22938@order",      m_22938_order,  0x00000000u },\n',
    """    { "fighter_22938@order",      m_22938_order,  0x00000000u },
    { "fighter_2116c",            b_2116c,        0xFFFFFFFFu },
    { "fighter_22510",            b_22510,        0xFFFFFFFFu },
    { "fighter_2116c@mutant",     m_2116c,        0xFFFFFFFFu },
    { "fighter_2116c@unsigned",   m_2116c_unsigned, 0xFFFFFFFFu },
    { "fighter_2116c@eax",        m_2116c_eax,    0xFFFFFFFFu },
    { "fighter_22510@mutant",     m_22510,        0xFFFFFFFFu },
    { "fighter_22510@ge",         m_22510_ge,     0xFFFFFFFFu },
""")
print("t6_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'
PR_ORACLE_REQUIRED=1 ./build/run_tests > /tmp/pr_p2_rt.log 2>&1; echo "exit $?"; grep -E 'table full' /tmp/pr_p2_rt.log
```

Expected:

```
t6_port applied
exit 134
fn_register: table full (limit 1300), cannot register original address 0xF1A10 (fn 0x...)
```

- [ ] **Step 3b: a repeated identical registration is not appended.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


# port/src/mem.c: a repeated identical registration is not appended (record §P2.7). run_tests calls
# actors_init four times (test_game.c 2900 and 10064, test_fight.c 11055, test_platform.c 3871) and each
# appended every pair again: 1307 entries for 351 distinct addresses once this task's two hooks join, past
# FN_TABLE_MAX = 1300.
sub("port/src/mem.c", '''void fn_register(u32 orig_addr, void (*fn)(void))
{
''', '''void fn_register(u32 orig_addr, void (*fn)(void))
{
    /* PORT: record 2026-10-02-reverse-p2 §P2.7. A pair already in the table
     * is not added again: every actors_init() registers its handlers again
     * (run_tests calls it four times), and fn_resolve returns the first entry
     * for an address, so a repeated identical pair is never the one returned. */
    for (u32 i = 0; i < fn_table_len; i++)
        if (fn_table[i].addr == orig_addr && fn_table[i].fn == fn) return;
''')
sub("port/tests/test_platform.c", "int test_call_seam(void)\n", '''/* Record 2026-10-02-reverse-p2 §P2.7: registering one (address, function)
 * pair more times than the table holds (FN_TABLE_MAX = 1300) adds it once;
 * before the fix the 1301st call aborted the run. A second function for the
 * same address is still appended after it, and fn_resolve keeps the first. */
static void fnreg_probe_a(void) {}
static void fnreg_probe_b(void) {}

int test_fn_register_repeats(void)
{
    int before = g_failures;
    for (int i = 0; i < 1301; i++) fn_register(0xF00F8u, fnreg_probe_a);
    CHECK(fn_resolve(0xF00F8u) == fnreg_probe_a, "the repeated pair resolves");
    fn_register(0xF00F8u, fnreg_probe_b);
    CHECK(fn_resolve(0xF00F8u) == fnreg_probe_a, "the first pair for an address wins");
    return g_failures - before;
}

int test_call_seam(void)
''')
sub("port/tests/test.h", "    X(test_call_seam)   \\\n", "    X(test_fn_register_repeats) \\\n    X(test_call_seam)   \\\n")
print("t6_table applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_2116c fighter_22510; do
  python3 tools/diff_verify.py --image /tmp/pr_p2_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected:

```
t6_table applied
all checks passed
| fighter_2116c | 0x2116C | 5 | 5/5 | VERIFIED | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_2116c@mutant | 0x2116C | 5 | 5/5 | MISMATCH | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_2116c@unsigned | 0x2116C | 5 | 5/5 | MISMATCH | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_2116c@eax | 0x2116C | 5 | 5/5 | MISMATCH | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_22510 | 0x22510 | 5 | 5/5 | VERIFIED | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_22510@mutant | 0x22510 | 5 | 5/5 | MISMATCH | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_22510@ge | 0x22510 | 5 | 5/5 | MISMATCH | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
```

- [ ] **Step 4: the E2 table.** Expected (the two hooks are supplement entries: only the supplement count moves):

```
entry-triage: FAIL: docs/superpowers/plans/2026-10-01-reverse-e2-triage.md differs from a fresh run (regenerate it with --out)
make: *** [entry-triage] Error 1
entry-triage: targets 297 unported, 198 ported; supplement 131 (26 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 33 in unported code, 82 in ported code, 19 nowhere
| callbacks | 9 | 62 |
| finishers | 0 | 9 |
| animation-targets | 54 | 58 |
| stubs | 60 |
 1 file changed, 2 insertions(+), 2 deletions(-)
```

- [ ] **Step 5: the task gate.** Expected:

```
Ran 159 tests in N.NNNs
OK
diff-verify: 49/49 functions VERIFIED; 72/72 mutants detected; 1 named gaps; 6/36 rows with callees closed (13 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
Ran 44 tests in N.NNNs
OK
entry-triage: targets 297 unported, 198 ported; supplement 131 (26 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 6: every new assertion can fail** (the C suite, and the harness's `DerefArgTests`):

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


def mutate_py(path, old, new, test):
    """Apply one mutation to a tool, run one unittest, print its last line, restore the file."""
    p = pathlib.Path(path)
    keep = p.read_text()
    assert keep.count(old) == 1, (path, old[:60])
    p.write_text(keep.replace(old, new))
    try:
        out = subprocess.run(["python3", "-m", "unittest", test], capture_output=True, text=True).stderr
        print("%s: %s" % (path, out.strip().splitlines()[-1]))
    finally:
        p.write_text(keep)


def mutate_rc(path, old, new):
    """As mutate, for a mutation that aborts the run: print the exit status and the abort message."""
    p = pathlib.Path(path)
    keep = p.read_text()
    assert keep.count(old) == 1, (path, old[:60])
    p.write_text(keep.replace(old, new))
    try:
        subprocess.run(["cmake", "--build", "build"], check=True, capture_output=True)
        r = subprocess.run(["./build/run_tests"], capture_output=True, text=True,
                           env={**os.environ, "PR_ORACLE_REQUIRED": "1"})
        msg = [l for l in r.stderr.splitlines() if "table full" in l]
        print("%s: exit %d, %s" % (path, r.returncode, msg[:1]))
    finally:
        p.write_text(keep)
        subprocess.run(["cmake", "--build", "build"], check=True, capture_output=True)


mutate_rc("port/src/mem.c",
          "        if (fn_table[i].addr == orig_addr && fn_table[i].fn == fn) return;\n", "")
mutate("port/src/game/fighter.c",
       "    flags[5] = 1u;                                          /* 0x2119C */",
       "    flags[5] = 0u;                                          /* 0x2119C */")
mutate("port/src/game/fighter.c",
       "    flags[9] = 1u;                                          /* 0x22538 */",
       "    flags[9] = 2u;                                          /* 0x22538 */")
mutate_py("tools/diff_emu.py", "uc.reg_read(names[d[0]]) + d[1]", "uc.reg_read(names[d[0]])",
          "tools.tests.test_diff_verify.DerefArgTests")
mutate_py("tools/diff_emu.py", "    if reg not in REGS or (off and not off.isdigit()):",
          "    if reg not in REGS + ('esp',) or (off and not off.isdigit()):",
          "tools.tests.test_diff_verify.DerefArgTests")
PY
```

Expected, then `all checks passed`:

```
port/src/mem.c: exit -6, ['fn_register: table full (limit 1300), cannot register original address 0xF00F8 (fn 0x...)']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45366: 1 != 0']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45387: 0 != 1']
tools/diff_emu.py: FAILED (failures=1)
tools/diff_emu.py: FAILED (failures=1)
all checks passed
```

- [ ] **Step 7: commit** (Task 2's files plus `tools/diff_emu.py port/src/mem.c port/tests/test_platform.c`), message `fighter: port the +0x18 hooks 0x2116C and 0x22510; 0x18C14's flags compared by value ([reg+N] call arguments); fn_register skips a repeated pair; E2 table regenerated (track P batch 2)` and the trailer.

---

### Task 7: the +0x1C callbacks `0x211F0`, `0x22588` and their callee `0x22404` (five new seams)

**Files:** as Task 2, plus `port/src/mem.h`. **Interfaces:** produces `void fighter_22404(u32 side)` (seamed: `0x22588`'s row stubs it), `void fighter_211f0(u32 side)`, `void fighter_22588(u32 side)`; `PR_SEAM0(addr)`; the seams of `0x18AF8` (`PR_SEAM0`), `0x39834`, `0x39A10`, `0x3C208`, `0x3C358`; `hit_anim_start_a`, `fighter_18af8`, `fighter_39834` declared in `fighter.h`; `FACING POSE TIMER PLACE HOLD ARM404`, `p2_1c`. Record §P2.8.

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


sub("tools/diff_verify.py", '''    ], allow_calls=(0x33950, 0x18BD4), calls=(CHECKS,), mutants=("@mutant", "@ge")),
]
''', '''    ], allow_calls=(0x33950, 0x18BD4), calls=(CHECKS,), mutants=("@mutant", "@ge")),
]

# The callees the slot +0x1C callbacks stub (record §P2.8), args from their bytes, clobbers from
# E.callee_clobbers: 0x18AF8 takes nothing (`xor eax,eax; call 0x18b04`); 0x39834 EAX = side, EDX = a byte;
# 0x39A10 EAX = rec, EDX = the word (`movsx ebx,dx`); 0x3C208 EAX = side, EDX = the distance; 0x3C358 EAX = side
# (it pushes EDX and loads it from EAX before any read); 0x22404 EAX = side. All plain `ret`.
FACING = E.Call(0x18AF8, (), clobbers=("ebx", "ecx", "edx"))
POSE = E.Call(0x39834, ("eax", "edx"), clobbers=("edx", "ebp"))
TIMER = E.Call(0x39A10, ("eax", "edx"), clobbers=("edx",))
PLACE = E.Call(0x3C208, ("eax", "edx"), clobbers=("edx",))
HOLD = E.Call(0x3C358, ("eax",))
ARM404 = E.Call(0x22404, ("eax",))


# The slot +0x1C callbacks (record §P2.8) run as 0x193B0 calls them at 0x19505: EAX = side; 0x19508 loads
# `mov eax,[esp+8]`: mask 0. Both slots' bytes +0x52..+0x5F carry sentinels (+0x5F = 0x5F is 0x211F0's byte
# for 0x39834); the characters 5 (slot 0) and 3 (slot 1) differ. `dist` pokes the word 0xA82D8 + 2 * 3 (the
# 0xA82D6 dword's high half for character 3, `sar 0x10`) negative, where a zero-extended read differs.
def p2_1c(cid, side, dist=None):
    pokes = {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\\x05", DS_SLOTS + 0x94 + 0x7A: b"\\x03",
             DS_SLOTS + 0x52: bytes(range(0x52, 0x60)), DS_SLOTS + 0x94 + 0x52: bytes(range(0x52, 0x60)),
             DS_SLOTS + 0x10: le32(0x10101010), DS_SLOTS + 0x94 + 0x10: le32(0x10101010)}
    if dist is not None:
        pokes[0xA82D8 + 2 * 3] = le32(dist)[:2]
    return Case(cid, {"eax": side}, pokes)


P2_SPECS += [
    Spec("fighter_22404", 0x22404, [p2_1c("a0", 0), p2_1c("a1", 1), p2_1c("a2", 0, 0xF000)],
         allow_calls=(0x33950,), calls=(ANIM_BEGIN, HIT_A, PLACE), eax_mask=0, mutants=("@mutant", "@signed")),
    Spec("fighter_211f0", 0x211F0, [p2_1c("b0", 0), p2_1c("b1", 1)],
         allow_calls=(0x33950,), calls=(FLASH, HIT_B, HIT_A, FACING, PLACE, POSE, HOLD, TIMER, VOICE), eax_mask=0,
         mutants=("@mutant", "@order")),
    Spec("fighter_22588", 0x22588, [p2_1c("d0", 0), p2_1c("d1", 1), p2_1c("d2", 0, 0xF000)],
         allow_calls=(0x33950,), calls=(FACING, FLASH, ARM404, HOLD, PLACE, TIMER, VOICE), eax_mask=0,
         mutants=("@mutant", "@order")),
]
''')
T = "tools/tests/test_diff_verify.py"
sub(T, '''"fighter_2116c": 0xFFFFFFFF, "fighter_22510": 0xFFFFFFFF}''',
    '''"fighter_2116c": 0xFFFFFFFF, "fighter_22510": 0xFFFFFFFF,
            "fighter_22404": 0, "fighter_211f0": 0, "fighter_22588": 0}''')
sub(T, '''            "fighter_22510@ge": {"eax", "call #0"}}''',
    '''            "fighter_22510@ge": {"eax", "call #0"},
            "fighter_22404@mutant": {"call #1 memory"}, "fighter_22404@signed": {"call #2"},
            "fighter_211f0@mutant": {"call #2"}, "fighter_211f0@order": {"call #9 memory"},
            "fighter_22588@mutant": {"call #1"}, "fighter_22588@order": {"call #4 memory"}}''')
sub(T, '''        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_2116c@eax"].problems}),
                         ["k1", "k2", "k4"])
''', '''        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_2116c@eax"].problems}),
                         ["k1", "k2", "k4"])
        # 0x22404's distance is a signed word: only a2's negative entry tells it from a zero-extended one
        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_22404@signed"].problems}), ["a2"])
''')
sub(T, '''                                 0x18C14: ("ebx", "edx", "ebp")})''',
    '''                                 0x18C14: ("ebx", "edx", "ebp"), 0x18AF8: ("ebx", "ecx", "edx"),
                                 0x39834: ("edx", "ebp"), 0x39A10: ("edx",), 0x3C208: ("edx",), 0x3C358: (),
                                 0x22404: ()})''')
sub(T, '''        self.assertIn("diff-verify: 49/49 functions VERIFIED; 72/72 mutants detected; 1 named gaps; "
                      "6/36 rows with callees closed (13 have none).", out.getvalue())''',
    '''        self.assertIn("diff-verify: 52/52 functions VERIFIED; 78/78 mutants detected; 1 named gaps; "
                      "6/39 rows with callees closed (13 have none).", out.getvalue())''')
print("t7_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p2_img.bin --function fighter_211f0 | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected:

```
t7_spec applied
| fighter_211f0 | 0x211F0 | 2 | 1/1 | MISMATCH | 18AF8 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 34D8C stub unverified, 39834 stub unverified, 39A10 stub unverified, 3C208 stub unverified, 3C358 stub unverified, 3C480 stub unverified, 3C4CC stub unverified |
  fighter_211f0: b0: port: unknown binding fighter_211f0
  fighter_211f0: b1: port: unknown binding fighter_211f0
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
typedef void (*p2_side_fn)(u32 side);

/* §P2.8: the slot +0x1C callbacks 0x21374 and 0x22938 store, as 0x193B0 calls
 * them (fn(side)), and 0x22588's callee 0x22404 (side 0: ctx[2] = slot 0,
 * ctx[3] = slot 1, character 2). */
static void p2_check_1c(void)
{
    p2_side_fn f;
    CHECK(fn_resolve(0x211F0u) == (void (*)(void))fighter_211f0, "0x211F0 is registered");
    CHECK(fn_resolve(0x22588u) == (void (*)(void))fighter_22588, "0x22588 is registered");
    CHECK_EQ_INT((int)(DSD(0x000225A7u) + 0x225ABu), 0x00022404);   /* 0x225A6's rel32 */

    /* 0x22404: the own record on 0xE4DEA at 2.0, the own slot's +0x57 = 2,
     * the other slot 0xA/9/0 with +0x10 = 0. */
    z_fseed();
    DSB(Z_S1 + 0x7Au) = 2u;
    DSW(0x000E4DEAu) = 0x12B1u;
    DSW(DSD(0x000C90F8u + 2u * 4u)) = 0x12B2u;
    DSB(Z_S0 + 0x57u) = 0x77u;
    DSB(Z_S1 + 0x54u) = 0x44u;
    DSD(Z_S1 + 0x10u) = 0x10101010u;
    fighter_22404(0u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000E4DEA);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x10u), 0);

    /* 0x211F0: the own slot's +0x57 = 2, the other's +0x53 = 0xF, the voice
     * 0xC75AA[2] (0xA6) last. */
    f = (p2_side_fn)(void *)fn_resolve(0x211F0u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_S1 + 0x7Au) = 2u;
    DSW(0x000E1672u) = 0x12B1u;
    DSW(DSD(0x000C90F8u + 2u * 4u)) = 0x12B2u;
    DSB(Z_S0 + 0x57u) = 0x77u;
    sound_voice_log_reset();
    f(0u);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x53u), 0x0F);
    CHECK(sound_voice_log_count() >= 1u, "a voice plays");
    CHECK_EQ_INT((int)sound_voice_log_at(sound_voice_log_count() - 1u), 0xA6);
    sound_voice_log_reset();

    /* 0x22588: through 0x22404 (the other slot 0xA/9/0), then the other
     * slot's +0x5D = 0x44 and the voice 0xA6. */
    f = (p2_side_fn)(void *)fn_resolve(0x22588u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_S1 + 0x7Au) = 2u;
    DSW(0x000E4DEAu) = 0x12B1u;
    DSW(DSD(0x000C90F8u + 2u * 4u)) = 0x12B2u;
    DSB(Z_S1 + 0x5Du) = 0x5Du;
    sound_voice_log_reset();
    f(0u);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x5Du), 0x44);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 2);
    CHECK(sound_voice_log_count() >= 1u, "a voice plays");
    CHECK_EQ_INT((int)sound_voice_log_at(sound_voice_log_count() - 1u), 0xA6);
    sound_voice_log_reset();
}

int test_p2_1c(void)            { return u6b_run(p2_check_1c); }
'''
ANCHOR = "int test_p2_hooks(void)         { return u6b_run(p2_check_hooks); }\n"
sub("port/tests/test_fight.c", ANCHOR, ANCHOR + TEST)
sub("port/tests/test.h", "    X(test_p2_hooks) \\\n", "    X(test_p2_hooks) \\\n    X(test_p2_1c) \\\n")
print("t7_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected:

```
t7_test applied
port/tests/test_fight.c:45400:51: error: use of undeclared identifier 'fighter_211f0'
port/tests/test_fight.c:45401:51: error: use of undeclared identifier 'fighter_22588'
port/tests/test_fight.c:45413:5: error: call to undeclared function 'fighter_22404'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]
3 errors generated.
```

- [ ] **Step 3: the port and the seams.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


# port/src/mem.h: the seam of a callee with no arguments (0x18AF8, record §P2.8): an empty initializer list
# is not C11, so PR_SEAM cannot take zero arguments.
sub("port/src/mem.h", '''#define PR_SEAM_RET(addr, ...)                                              \\
''', '''#define PR_SEAM0(addr)                                                      \\
    do {                                                                    \\
        if (pr_seam) {                                                      \\
            u32 seam_e_;                                                    \\
            if (pr_seam((addr), 0u, (const u32 *)0, &seam_e_)) return;      \\
        }                                                                   \\
    } while (0)
#define PR_SEAM_RET(addr, ...)                                              \\
''')

F = "port/src/game/fighter.c"
# three callees the mutants of port/tests/diff_runner.c call lose `static` (declared in fighter.h below)
p = pathlib.Path(F)
t = p.read_text()
old = "static void hit_anim_start_a(u32 rec, u32 stream, u32 frame_bits); /* 0x3C480 */\n"
assert t.count(old) == 2
p.write_text(t.replace(old, ""))
sub(F, "static void fighter_39834(u32 side, s32 b);              /* 0x39834 */\n", "")
sub(F, '''static void hit_anim_start_a(u32 rec, u32 stream, u32 frame_bits)
{
''', '''void hit_anim_start_a(u32 rec, u32 stream, u32 frame_bits)
{
''')
sub(F, '''static void fighter_18af8(void)
{
''', '''void fighter_18af8(void)
{
    PR_SEAM0(0x18AF8u);
''')
sub(F, '''static void fighter_39834(u32 side, s32 b)
{
''', '''void fighter_39834(u32 side, s32 b)
{
    PR_SEAM(0x39834u, side, (u32)b);
''')
sub(F, '''void fighter_39a10(u32 rec, u32 value)
{
''', '''void fighter_39a10(u32 rec, u32 value)
{
    PR_SEAM(0x39A10u, rec, value);
''')
sub(F, '''void fighter_3c208(u32 side, s32 dist)
{
''', '''void fighter_3c208(u32 side, s32 dist)
{
    PR_SEAM(0x3C208u, side, (u32)dist);
''')
sub(F, '''void fighter_3c358(u32 side)
{
''', '''void fighter_3c358(u32 side)
{
    PR_SEAM(0x3C358u, side);
''')

FIGHTER_C = r'''
#define P2_ANIM_211F0    0x000E1672u  /* 0x21200: EDX, kept through 0x34D8C */
#define P2_STREAMS_C90F8 0x000C90F8u  /* 0x21229/0x2243D: [char] the other record's stream */
#define P2_DIST_A81B0    0x000A81B0u  /* 0x2124C: [char] a word, zero-extended */
#define P2_DIST_A82D8    0x000A82D8u  /* 0x22459/0x225C7: [char] the high half of the dword at 0xA82D6 + 2c */
#define P2_VOICE_C75AA   0x000C75AAu  /* 0x212A2/0x22601: [char] a voice word */
#define P2_ANIM_22404    0x000E4DEAu  /* 0x22411 */

/* 0x22404 — record §P2.8. Called by 0x22588 (0x225A6) and 0x224EC (0x224FC)
 * with EAX = side; both overwrite EAX after it. The context is 0x33950(side).
 * The own record on 0xE4DEA at 2.0 (0x2BC30), the own slot's +0x57 = 2, the
 * other record on 0xC90F8[the other slot's character] at 2.0 (0x3C480),
 * 0x3C208(side, the signed word 0xA82D8[that character]), the other slot
 * 0xA/9/0 (+0x53/+0x52/+0x54) with +0x10 = 0. */
void fighter_22404(u32 side)
{
    u32 ctx[6];
    PR_SEAM(0x22404u, side);
    fighter_ctx_same(ctx, side);                            /* 0x22408..0x2240C 0x33950 */
    actors_anim_begin(ctx[4], P2_ANIM_22404, 0x40000000u);  /* 0x22411..0x2241F 0x2BC30 */
    DSB(ctx[2] + 0x57u) = 2u;                               /* 0x22428 */
    hit_anim_start_a(ctx[5], DSD(P2_STREAMS_C90F8 + (u32)DSB(ctx[3] + 0x7Au) * 4u),
                     0x40000000u);                          /* 0x2242C..0x22448 0x3C480 */
    fighter_3c208(ctx[0], (s32)(s16)DSW(P2_DIST_A82D8 + (u32)DSB(ctx[3] + 0x7Au) * 2u));   /* 0x2244D..0x22466 */
    DSB(ctx[3] + 0x53u) = 0x0Au;                            /* 0x2246F */
    DSB(ctx[3] + 0x52u) = 9u;                               /* 0x22477 */
    DSB(ctx[3] + 0x54u) = 0u;                               /* 0x2247F */
    DSD(ctx[3] + 0x10u) = 0u;                               /* 0x22487 */
}

/* 0x211F0 — record §P2.8. The slot +0x1C callback 0x21374 stores (the dword
 * at 0x213D6; 0x193B0's 0x19505, fn(side), EAX unread). The context is
 * 0x33950(side). 0x34D8C(side); the own record on 0xE1672 at 2.0 (0x3C4CC;
 * EDX loaded at 0x21200, before 0x34D8C, which keeps it); the other record on
 * 0xC90F8[the other slot's character] at 2.0 (0x3C480); 0x18AF8; 0x3C208(side,
 * the word 0xA81B0[that character], zero-extended); 0x39834(the other side,
 * the own slot's +0x5F); 0x3C358(side); 0x39A10(each record, 0x29A; EDX =
 * 0x29A at 0x21275 survives 0x3C358); the voice 0xC75AA[that character]; the
 * own slot's +0x57 = 2 and the other's +0x53 = 0xF. The character is re-read
 * at each use (0x21221, 0x21242, 0x2129A). */
void fighter_211f0(u32 side)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);                            /* 0x211F4..0x211F8 0x33950 */
    hit_flash_pair(ctx[0]);                                 /* 0x211FD..0x21205 0x34D8C */
    hit_anim_start_b(ctx[4], P2_ANIM_211F0, 0x40000000u);   /* 0x2120A..0x21213 0x3C4CC */
    hit_anim_start_a(ctx[5], DSD(P2_STREAMS_C90F8 + (u32)DSB(ctx[3] + 0x7Au) * 4u),
                     0x40000000u);                          /* 0x21218..0x21234 0x3C480 */
    fighter_18af8();                                        /* 0x21239 */
    fighter_3c208(ctx[0], (s32)(u32)DSW(P2_DIST_A81B0 + (u32)DSB(ctx[3] + 0x7Au) * 2u));  /* 0x2123E..0x21257 */
    fighter_39834(ctx[1], (s32)(u32)DSB(ctx[2] + 0x5Fu));   /* 0x2125C..0x2126D */
    fighter_3c358(ctx[0]);                                  /* 0x21272..0x2127A */
    fighter_39a10(ctx[4], 0x29Au);                          /* 0x2127F/0x21283 */
    fighter_39a10(ctx[5], 0x29Au);                          /* 0x21288..0x21291 */
    (void)sound_voice((u32)DSW(P2_VOICE_C75AA + (u32)DSB(ctx[3] + 0x7Au) * 2u));   /* 0x21296..0x212AF 0x2C3FC */
    DSB(ctx[2] + 0x57u) = 2u;                               /* 0x212B8 */
    DSB(ctx[3] + 0x53u) = 0x0Fu;                            /* 0x212C0 */
}

/* 0x22588 — record §P2.8. The slot +0x1C callback 0x22938 stores (the dword
 * at 0x22979; 0x193B0's 0x19505, fn(side)). The context is 0x33950(side).
 * 0x18AF8; 0x34D8C(the other side); 0x22404(side); 0x3C358(side); the other
 * slot's +0x5D = 0x44; 0x3C208(side, the signed word 0xA82D8[the other slot's
 * character]); 0x39A10(each record, 0x29A); the voice 0xC75AA[that character]. */
void fighter_22588(u32 side)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);                            /* 0x2258C..0x22590 0x33950 */
    fighter_18af8();                                        /* 0x22595 */
    hit_flash_pair(ctx[1]);                                 /* 0x2259A/0x2259E 0x34D8C */
    fighter_22404(ctx[0]);                                  /* 0x225A3/0x225A6 */
    fighter_3c358(ctx[0]);                                  /* 0x225AB/0x225AE */
    DSB(ctx[3] + 0x5Du) = 0x44u;                            /* 0x225B7 */
    fighter_3c208(ctx[0], (s32)(s16)DSW(P2_DIST_A82D8 + (u32)DSB(ctx[3] + 0x7Au) * 2u));   /* 0x225BB..0x225D4 */
    fighter_39a10(ctx[4], 0x29Au);                          /* 0x225D9..0x225E2 */
    fighter_39a10(ctx[5], 0x29Au);                          /* 0x225E7..0x225F0 */
    (void)sound_voice((u32)DSW(P2_VOICE_C75AA + (u32)DSB(ctx[3] + 0x7Au) * 2u));   /* 0x225F5..0x2260E 0x2C3FC */
}
'''
sub(F, '''    return (u32)fighter_18c14(ctx[0], flags, P2_BOX_22510_A, P2_BOX_22510_B);   /* 0x2256A..0x2257B 0x18C14 */
}
''', '''    return (u32)fighter_18c14(ctx[0], flags, P2_BOX_22510_A, P2_BOX_22510_B);   /* 0x2256A..0x2257B 0x18C14 */
}
''' + FIGHTER_C)
sub("port/src/game/fighter.h", "u32  fighter_22510(u32 side);\n",
    """u32  fighter_22510(u32 side);
/* §P2.8: the slot +0x1C callbacks they store, fn(side) as 0x193B0 calls them,
 * and 0x22588's callee 0x22404 (side); and three callees they stub, no
 * longer file-local so the harness's mutants can call them: 0x3C480, 0x18AF8
 * and 0x39834. */
void hit_anim_start_a(u32 rec, u32 stream, u32 frame_bits);
void fighter_18af8(void);
void fighter_39834(u32 side, s32 b);
void fighter_22404(u32 side);
void fighter_211f0(u32 side);
void fighter_22588(u32 side);
""")
sub("port/src/game/actors.c", "    fn_register(0x22510u, (void (*)(void))fighter_22510);\n",
    """    fn_register(0x22510u, (void (*)(void))fighter_22510);
    /* PORT: record 2026-10-02-reverse-p2 §P2.8. The slot +0x1C callbacks
     * 0x21374 and 0x22938 store (the dwords at 0x213D6 and 0x22979; 0x193B0's
     * 0x19505, fn(side)). */
    fn_register(0x211F0u, (void (*)(void))fighter_211f0);
    fn_register(0x22588u, (void (*)(void))fighter_22588);
""")

BINDINGS = r'''/* §P2.8: the slot +0x1C callbacks as 0x193B0 calls them at 0x19505 (EAX = side; 0x19508 reloads EAX), and
 * 0x22404 as 0x22588 (0x225A6) calls it (0x225AB reloads EAX): mask 0. */
static void b_22404(const u32 *r, u32 *eax)            { fighter_22404(r[R_EAX]); *eax = 0u; }
static void b_211f0(const u32 *r, u32 *eax)            { fighter_211f0(r[R_EAX]); *eax = 0u; }
static void b_22588(const u32 *r, u32 *eax)            { fighter_22588(r[R_EAX]); *eax = 0u; }
static void m_22404_at(u32 side, int late57, int zext)
{
    u32 ctx[6];
    u32 w;
    fighter_ctx_same(ctx, side);
    actors_anim_begin(ctx[4], 0x000E4DEAu, 0x40000000u);
    if (!late57) DSB(ctx[2] + 0x57u) = 2u;
    hit_anim_start_a(ctx[5], DSD(0x000C90F8u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40000000u);
    if (late57) DSB(ctx[2] + 0x57u) = 2u;
    w = DSW(0x000A82D8u + (u32)DSB(ctx[3] + 0x7Au) * 2u);
    fighter_3c208(ctx[0], zext ? (s32)w : (s32)(s16)w);
    DSB(ctx[3] + 0x53u) = 0x0Au;
    DSB(ctx[3] + 0x52u) = 9u;
    DSB(ctx[3] + 0x54u) = 0u;
    DSD(ctx[3] + 0x10u) = 0u;
}
static void m_22404(const u32 *r, u32 *eax)            /* +0x57 = 2 after the 0x3C480 call */
{
    m_22404_at(r[R_EAX], 1, 0);
    *eax = 0u;
}
static void m_22404_signed(const u32 *r, u32 *eax)     /* the distance word zero-extended */
{
    m_22404_at(r[R_EAX], 0, 1);
    *eax = 0u;
}
static void m_211f0_at(u32 side, int own_char, int early57)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);
    hit_flash_pair(ctx[0]);
    hit_anim_start_b(ctx[4], 0x000E1672u, 0x40000000u);
    hit_anim_start_a(ctx[5], DSD(0x000C90F8u + (u32)DSB(ctx[own_char ? 2 : 3] + 0x7Au) * 4u), 0x40000000u);
    fighter_18af8();
    fighter_3c208(ctx[0], (s32)(u32)DSW(0x000A81B0u + (u32)DSB(ctx[3] + 0x7Au) * 2u));
    fighter_39834(ctx[1], (s32)(u32)DSB(ctx[2] + 0x5Fu));
    fighter_3c358(ctx[0]);
    fighter_39a10(ctx[4], 0x29Au);
    fighter_39a10(ctx[5], 0x29Au);
    if (early57) DSB(ctx[2] + 0x57u) = 2u;
    (void)sound_voice((u32)DSW(0x000C75AAu + (u32)DSB(ctx[3] + 0x7Au) * 2u));
    DSB(ctx[2] + 0x57u) = 2u;
    DSB(ctx[3] + 0x53u) = 0x0Fu;
}
static void m_211f0(const u32 *r, u32 *eax)            /* the other record's stream by the own character */
{
    m_211f0_at(r[R_EAX], 1, 0);
    *eax = 0u;
}
static void m_211f0_order(const u32 *r, u32 *eax)      /* +0x57 = 2 before the voice */
{
    m_211f0_at(r[R_EAX], 0, 1);
    *eax = 0u;
}
static void m_22588_at(u32 side, int own_flash, int late5d)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);
    fighter_18af8();
    hit_flash_pair(own_flash ? ctx[0] : ctx[1]);
    fighter_22404(ctx[0]);
    fighter_3c358(ctx[0]);
    if (!late5d) DSB(ctx[3] + 0x5Du) = 0x44u;
    fighter_3c208(ctx[0], (s32)(s16)DSW(0x000A82D8u + (u32)DSB(ctx[3] + 0x7Au) * 2u));
    if (late5d) DSB(ctx[3] + 0x5Du) = 0x44u;
    fighter_39a10(ctx[4], 0x29Au);
    fighter_39a10(ctx[5], 0x29Au);
    (void)sound_voice((u32)DSW(0x000C75AAu + (u32)DSB(ctx[3] + 0x7Au) * 2u));
}
static void m_22588(const u32 *r, u32 *eax)            /* 0x34D8C on the own side */
{
    m_22588_at(r[R_EAX], 1, 0);
    *eax = 0u;
}
static void m_22588_order(const u32 *r, u32 *eax)      /* +0x5D after the 0x3C208 call */
{
    m_22588_at(r[R_EAX], 0, 1);
    *eax = 0u;
}

'''
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_22510@ge",         m_22510_ge,     0xFFFFFFFFu },\n',
    """    { "fighter_22510@ge",         m_22510_ge,     0xFFFFFFFFu },
    { "fighter_22404",            b_22404,        0x00000000u },
    { "fighter_211f0",            b_211f0,        0x00000000u },
    { "fighter_22588",            b_22588,        0x00000000u },
    { "fighter_22404@mutant",     m_22404,        0x00000000u },
    { "fighter_22404@signed",     m_22404_signed, 0x00000000u },
    { "fighter_211f0@mutant",     m_211f0,        0x00000000u },
    { "fighter_211f0@order",      m_211f0_order,  0x00000000u },
    { "fighter_22588@mutant",     m_22588,        0x00000000u },
    { "fighter_22588@order",      m_22588_order,  0x00000000u },
""")
print("t7_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_22404 fighter_211f0 fighter_22588; do
  python3 tools/diff_verify.py --image /tmp/pr_p2_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected:

```
t7_port applied
all checks passed
| fighter_22404 | 0x22404 | 3 | 1/1 | VERIFIED | 2BC30 stub unverified, 33950 allow unverified, 3C208 stub unverified, 3C480 stub unverified |
| fighter_22404@mutant | 0x22404 | 3 | 1/1 | MISMATCH | 2BC30 stub unverified, 33950 allow unverified, 3C208 stub unverified, 3C480 stub unverified |
| fighter_22404@signed | 0x22404 | 3 | 1/1 | MISMATCH | 2BC30 stub unverified, 33950 allow unverified, 3C208 stub unverified, 3C480 stub unverified |
| fighter_211f0 | 0x211F0 | 2 | 1/1 | VERIFIED | 18AF8 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 34D8C stub unverified, 39834 stub unverified, 39A10 stub unverified, 3C208 stub unverified, 3C358 stub unverified, 3C480 stub unverified, 3C4CC stub unverified |
| fighter_211f0@mutant | 0x211F0 | 2 | 1/1 | MISMATCH | 18AF8 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 34D8C stub unverified, 39834 stub unverified, 39A10 stub unverified, 3C208 stub unverified, 3C358 stub unverified, 3C480 stub unverified, 3C4CC stub unverified |
| fighter_211f0@order | 0x211F0 | 2 | 1/1 | MISMATCH | 18AF8 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 34D8C stub unverified, 39834 stub unverified, 39A10 stub unverified, 3C208 stub unverified, 3C358 stub unverified, 3C480 stub unverified, 3C4CC stub unverified |
| fighter_22588 | 0x22588 | 3 | 1/1 | VERIFIED | 18AF8 stub unverified, 22404 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 34D8C stub unverified, 39A10 stub unverified, 3C208 stub unverified, 3C358 stub unverified |
| fighter_22588@mutant | 0x22588 | 3 | 1/1 | MISMATCH | 18AF8 stub unverified, 22404 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 34D8C stub unverified, 39A10 stub unverified, 3C208 stub unverified, 3C358 stub unverified |
| fighter_22588@order | 0x22588 | 3 | 1/1 | MISMATCH | 18AF8 stub unverified, 22404 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 34D8C stub unverified, 39A10 stub unverified, 3C208 stub unverified, 3C358 stub unverified |
```

- [ ] **Step 4: the E2 table.** Expected:

```
entry-triage: FAIL: docs/superpowers/plans/2026-10-01-reverse-e2-triage.md differs from a fresh run (regenerate it with --out)
make: *** [entry-triage] Error 1
entry-triage: targets 297 unported, 198 ported; supplement 131 (23 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 31 in unported code, 84 in ported code, 19 nowhere
| callbacks | 9 | 62 |
| finishers | 0 | 9 |
| animation-targets | 54 | 58 |
| stubs | 60 |
 1 file changed, 5 insertions(+), 5 deletions(-)
```

- [ ] **Step 5: the task gate.** Expected:

```
Ran 159 tests in N.NNNs
OK
diff-verify: 52/52 functions VERIFIED; 78/78 mutants detected; 1 named gaps; 6/39 rows with callees closed (13 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
Ran 44 tests in N.NNNs
OK
entry-triage: targets 297 unported, 198 ported; supplement 131 (23 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 6: every new unit assertion can fail.**

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


mutate("port/src/game/actors.c", "    fn_register(0x22588u, (void (*)(void))fighter_22588);\n", "")
mutate("port/src/game/fighter.c",
       "    DSD(ctx[3] + 0x10u) = 0u;                               /* 0x22487 */",
       "    DSD(ctx[2] + 0x10u) = 0u;                               /* 0x22487 */")
mutate("port/src/game/fighter.c",
       "    DSB(ctx[3] + 0x53u) = 0x0Fu;                            /* 0x212C0 */",
       "    DSB(ctx[2] + 0x53u) = 0x0Fu;                            /* 0x212C0 */")
PY
```

Expected, then `all checks passed`:

```
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:45401: 0x22588 is registered']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45420: 269488144 != 0']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45434: 10 != 15']
all checks passed
```

- [ ] **Step 7: commit** (Task 2's files plus `port/src/mem.h`), message `fighter: port the +0x1C callbacks 0x211F0 0x22588 and 0x22404, seams 0x18AF8 (PR_SEAM0) 0x39834 0x39A10 0x3C208 0x3C358; E2 table regenerated (track P batch 2)` and the trailer.

---

### Task 8: the +0x0C callbacks `0x212CC` and `0x22638` (seam `0x36870`; `0x22638`'s jump table)

**Files:** as Task 2, plus `tools/diff_emu.py`. **Interfaces:** produces `fighter_212cc`, `fighter_22638` (`(u32 slot, u32 rec, u32 side)`); `RESOLVED_JUMPS[0x227BC] = (0x227A3, 0x22618, 8)`; `verify_spec`/`verify_gap` scan with `resolved=E.RESOLVED_JUMPS`; `ANIM54`, `p2_212cc`, `p2_22638`, `f32`; the seam `PR_SEAM(0x36870u, rec)`. Record §P2.9 (the x87 named limit).

- [ ] **Step 1: the specs.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new, count=1):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == count, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


# tools/diff_emu.py: 0x22638's own jump table (record §P2.9): `cmp al,7; ja; and eax,0xff; lea edx,[eax*4]; mov
# eax,[esp]; add eax,eax; jmp cs:[edx+0x22618]` is pre-scaled into a base, the form switch_cases does not bound.
sub("tools/diff_emu.py", '''    0x2A056: (0x2A04B, 0x29F1C, 6),
}
''', '''    0x2A056: (0x2A04B, 0x29F1C, 6),
    # 0x22638 (record 2026-10-02-reverse-p2 §P2.9): cmp al,7; ja; and eax,0xff; lea edx,[eax*4]; mov eax,[esp];
    # add eax,eax; jmp cs:[edx+0x22618]
    0x227BC: (0x227A3, 0x22618, 8),
}
''')
# tools/diff_verify.py: a row's coverage follows the hand-resolved tables too (its own body's, record §P2.9)
sub("tools/diff_verify.py",
    "    info = E.static_scan(image, spec.entry, stop=[k.addr for k in spec.calls], switches=True)\n",
    "    info = E.static_scan(image, spec.entry, stop=[k.addr for k in spec.calls], switches=True,\n"
    "                         resolved=E.RESOLVED_JUMPS)\n", count=2)

sub("tools/diff_verify.py", '''         allow_calls=(0x33950,), calls=(FACING, FLASH, ARM404, HOLD, PLACE, TIMER, VOICE), eax_mask=0,
         mutants=("@mutant", "@order")),
]
''', '''         allow_calls=(0x33950,), calls=(FACING, FLASH, ARM404, HOLD, PLACE, TIMER, VOICE), eax_mask=0,
         mutants=("@mutant", "@order")),
]

# 0x36870 (fighter_36870): EAX = rec, a plain `ret`; it clobbers ESI, EDI and EBP (E.callee_clobbers).
ANIM54 = E.Call(0x36870, ("eax",), clobbers=("esi", "edi", "ebp"))


# The slot +0x0C callbacks 0x21374 and 0x22938 store (record §P2.9) run as 0x3531C case 7 calls them (0x35431:
# EAX = slot, EDX = rec, EBX = side; `xor eax,eax` at 0x35434): mask 0. Both read EBX alone for the context.
# 0x212CC: the own slot's +0x57 (state), word +0x88 (against 0xA81AE's 3, signed) and +0x8A; in state 1 the
# pointer DS_001077A8[rec+0x51] (EDX's record, E3_OUT here: not ctx[4]) and its +0x7A (1, 6 or another).
def p2_212cc(cid, st, w88=0x0100, ridx=0, ptrs=(E3_REC2, 0), char=1):
    return Case(cid, {"eax": E3_SLOT, "edx": E3_OUT, "ebx": 0},
                {**SLOT_PTRS, DS_SLOTS - 8: b"".join(le32(v) for v in ptrs), E3_REC2 + 0x7A: bytes([char]),
                 E3_OUT + 0x51: bytes([ridx]), DS_SLOTS + 0x52: bytes([0x52, 0x53, 0x54, 0x55, 0x56, st]),
                 DS_SLOTS + 0x88: le32(w88)[:2] + b"\\x8a"})


def f32(x):
    import struct
    return struct.pack("<f", x)


# 0x22638: cmd = the two sides' command words DS_001088E0 (own, other); lat/cnt = the side's words 0x104754 and
# 0x104758 (the other side's carry sentinels); fl = the side's float 0x104738; o53/o5d/o63 = the other slot's
# +0x53/+0x5D/+0x63 (its character 3: the words 0xA82EC[3] = 4 and 0xA8300[3] = 0x78); st = the own +0x57.
def p2_22638(cid, side, st, cmd=(0, 0), lat=0, cnt=0x200, fl=2.0, o53=0x0A, o5d=7, o63=0, stub=None):
    own, oth = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    words = [cmd[0], cmd[1]] if side == 0 else [cmd[1], cmd[0]]
    lw, cw = [0x5454, 0x5656], [0x5858, 0x5A5A]
    lw[side], cw[side] = lat, cnt
    fls = [f32(1.5), f32(2.5)]
    fls[side] = f32(fl)
    oth_bytes = bytearray(range(0x53, 0x64))
    oth_bytes[0] = o53
    oth_bytes[0x5D - 0x53] = o5d
    oth_bytes[0x63 - 0x53] = o63
    return Case(cid, {"eax": E3_SLOT, "edx": E3_OUT, "ebx": side},
                {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\\x05", DS_SLOTS + 0x94 + 0x7A: b"\\x03",
                 0x1088E0: le32(words[0])[:2] + le32(words[1])[:2],
                 0x104754: b"".join(le32(v)[:2] for v in lw + cw), 0x104738: fls[0] + fls[1],
                 own + 0x52: bytes([0x52, 0x53, 0x54, 0x55, 0x56, st]), own + 0x8A: b"\\x8a",
                 oth + 0x53: bytes(oth_bytes)}, {} if stub is None else stub)


P2_SPECS += [
    # m2: the word +0x88 = -1 against 3: signed `jge` returns; unsigned it would step +0x57. m3/m4/m5: state 1
    # with the pointer's character 1, 6 and 2; m6 a zero pointer; m9 rec+0x51 = 1 reads DS_001077AC.
    Spec("fighter_212cc", 0x212CC, [
        p2_212cc("m0", 0, 0x0004), p2_212cc("m1", 0, 0x0003), p2_212cc("m2", 0, 0xFFFF),
        p2_212cc("m3", 1, char=1), p2_212cc("m4", 1, char=6), p2_212cc("m5", 1, char=2),
        p2_212cc("m6", 1, ptrs=(0, E3_REC2)), p2_212cc("m7", 2), p2_212cc("m8", 0xFF),
        p2_212cc("m9", 1, ridx=1, ptrs=(0, E3_REC2), char=6),
    ], allow_calls=(0x33950,), calls=(HIT_B,), eax_mask=0, mutants=("@mutant", "@signed", "@side")),
    # p0..p15 (record §P2.9): the frame count's step and the two bounds, the +0x5D drain (the other side's stick
    # or its +0x63) and floor, the float's -0.7/+0.1 with the 1.0 and 3.0 clamps, the latch, and each state.
    # p14: the count 0x8000 + 1 is negative (signed: 0x78 > it, +0x5D floored to 1); p15: +0x5D = 0x80 is 128
    # against 4 (a signed byte would read -128 and zero it).
    Spec("fighter_22638", 0x22638, [
        p2_22638("p0", 0, 0, cnt=0x14, o5d=0, fl=2.0),
        p2_22638("p1", 0, 0, cmd=(1, 0x10), cnt=0x13, o5d=9, fl=1.5),
        p2_22638("p2", 0, 1, cmd=(0x0C, 0), o5d=3, o63=1, fl=2.95),
        p2_22638("p3", 0, 2, o5d=0),
        p2_22638("p4", 0, 2, o53=0x09),
        p2_22638("p5", 0, 2, cmd=(1, 0), fl=2.0),
        p2_22638("p6", 0, 2, cmd=(4, 0)),
        p2_22638("p7", 0, 2, cmd=(2, 0)),
        p2_22638("p8", 0, 2, cmd=(8, 0)),
        p2_22638("p9", 0, 2),
        p2_22638("pA", 0, 2, lat=6),
        p2_22638("pB", 0, 3),
        p2_22638("pC", 0, 8),
        p2_22638("pD", 1, 2, cmd=(1, 0), fl=1.2, stub={0x2C3FC: 0}),
        p2_22638("pE", 0, 0, cnt=0x8000, o5d=0),
        p2_22638("pF", 1, 3, cmd=(0, 0x20), o5d=0x80),
    ], allow_calls=(0x33950,), calls=(ANIM_BEGIN, ANIM54, VOICE), eax_mask=0,
       mutants=("@mutant", "@signed", "@byte5d")),
]
''')

T = "tools/tests/test_diff_verify.py"
sub(T, '''            "fighter_22404": 0, "fighter_211f0": 0, "fighter_22588": 0}''',
    '''            "fighter_22404": 0, "fighter_211f0": 0, "fighter_22588": 0,
            "fighter_212cc": 0, "fighter_22638": 0}''')
sub(T, '''            "fighter_22588@mutant": {"call #1"}, "fighter_22588@order": {"call #4 memory"}}''',
    '''            "fighter_22588@mutant": {"call #1"}, "fighter_22588@order": {"call #4 memory"},
            "fighter_212cc@mutant": {"call #0"}, "fighter_212cc@signed": {"byte"},
            "fighter_212cc@side": {"byte", "call #0"}, "fighter_22638@mutant": {"call #1"},
            "fighter_22638@signed": {"byte"}, "fighter_22638@byte5d": {"byte"}}''')
sub(T, '''        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_22404@signed"].problems}), ["a2"])
''', '''        self.assertEqual(sorted({p.split(":")[0] for p in self.mut["fighter_22404@signed"].problems}), ["a2"])
        # 0x212CC: only m2's word -1 tells the signed bound; 0x22638: only pE's count 0x8001 and pF's +0x5D 0x80
        for name, ids in (("fighter_212cc@signed", ["m2"]), ("fighter_22638@signed", ["pE"]),
                          ("fighter_22638@byte5d", ["pF"])):
            self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
''')
sub(T, '''                                 0x22404: ()})''',
    '''                                 0x22404: (), 0x36870: ("esi", "edi", "ebp")})''')
sub(T, '''        0x2A056: ["cmp dx, 5", "ja 0x2a05e", "xor ebx, ebx", "mov bx, dx", "jmp dword ptr cs:[ebx*4 + 0x29f1c]"],
    }''', '''        0x2A056: ["cmp dx, 5", "ja 0x2a05e", "xor ebx, ebx", "mov bx, dx", "jmp dword ptr cs:[ebx*4 + 0x29f1c]"],
        0x227BC: ["cmp al, 7", "ja 0x22930", "and eax, 0xff", "lea edx, [eax*4]", "mov eax, dword ptr [esp]",
                  "add eax, eax", "jmp dword ptr cs:[edx + 0x22618]"],
    }''')
sub(T, '''        self.assertIn("diff-verify: 52/52 functions VERIFIED; 78/78 mutants detected; 1 named gaps; "
                      "6/39 rows with callees closed (13 have none).", out.getvalue())''',
    '''        self.assertIn("diff-verify: 54/54 functions VERIFIED; 84/84 mutants detected; 1 named gaps; "
                      "7/41 rows with callees closed (13 have none).", out.getvalue())''')
print("t8_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p2_img.bin --function fighter_22638 | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected (36 blocks: the resolved table is followed):

```
t8_spec applied
| fighter_22638 | 0x22638 | 16 | 36/36 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 36870 stub unverified |
  fighter_22638: p0: port: unknown binding fighter_22638
  fighter_22638: p1: port: unknown binding fighter_22638
  fighter_22638: p2: port: unknown binding fighter_22638
  fighter_22638: p3: port: unknown binding fighter_22638
  fighter_22638: p4: port: unknown binding fighter_22638
  fighter_22638: p5: port: unknown binding fighter_22638
  fighter_22638: p6: port: unknown binding fighter_22638
  fighter_22638: p7: port: unknown binding fighter_22638
  fighter_22638: p8: port: unknown binding fighter_22638
  fighter_22638: p9: port: unknown binding fighter_22638
  fighter_22638: pA: port: unknown binding fighter_22638
  fighter_22638: pB: port: unknown binding fighter_22638
  fighter_22638: pC: port: unknown binding fighter_22638
  fighter_22638: pD: port: unknown binding fighter_22638
  fighter_22638: pE: port: unknown binding fighter_22638
  fighter_22638: pF: port: unknown binding fighter_22638
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
/* §P2.9: the slot +0x0C callbacks 0x21374 and 0x22938 store, through their
 * registrations as 0x3531C case 7 calls them (slot 0, its record, side 0;
 * slot 1 character 3). */
static void p2_check_0c(void)
{
    p2_cb_fn f;
    union { float f; u32 u; } v;
    CHECK(fn_resolve(0x212CCu) == (void (*)(void))fighter_212cc, "0x212CC is registered");
    CHECK(fn_resolve(0x22638u) == (void (*)(void))fighter_22638, "0x22638 is registered");
    /* 0x22638 starts right after its own jump table (0x22618, 8 dwords) */
    CHECK_EQ_INT((int)DSD(0x00022618u), 0x000227C3);
    CHECK_EQ_INT((int)DSD(0x00022634u), 0x00022930);

    /* 0x212CC state 0: the word +0x88 = 4 lies above 0xA81AE's 3: +0x57 = 1;
     * state 1 with DS_001077A8[rec+0x51] = slot 0 (character 6): the record on
     * 0xE16E6 at 3.0, +0x52 = 9, +0x57 = 2, +0x8A = 0. */
    f = (p2_cb_fn)(void *)fn_resolve(0x212CCu);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_S0 + 0x57u) = 0u;
    DSW(Z_S0 + 0x88u) = 4u;
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 1);
    z_fseed();
    DSB(Z_S0 + 0x7Au) = 6u;
    DSB(Z_R0 + 0x51u) = 0u;
    DSW(0x000E16E6u) = 0x12B1u;
    DSB(Z_S0 + 0x57u) = 1u;
    DSB(Z_S0 + 0x8Au) = 0x8Au;
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000E16E6);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x8Au), 0);

    /* 0x22638 state 2, the other slot in state 0xA with +0x5D = 7, the own
     * command word bit 0: the float 2.0 - 0.7 is stored (1.3f), the record
     * on 0xE4E08 at that hold, +0x57 = 4, the voice 0x7D; the frame count
     * steps. Then with the float 1.5: 0.8 is clamped to 1.0. */
    f = (p2_cb_fn)(void *)fn_resolve(0x22638u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_S1 + 0x7Au) = 3u;
    DSB(Z_S1 + 0x53u) = 0x0Au;
    DSB(Z_S1 + 0x5Du) = 7u;
    DSB(Z_S1 + 0x63u) = 0u;
    DSW(DS_001088E0) = 1u;
    DSW(DS_001088E0 + 2u) = 0u;
    DSW(0x00104758u) = 0x0200u;
    v.f = 2.0f;
    DSD(0x00104738u) = v.u;
    DSW(0x000E4E08u) = 0x12B1u;
    DSB(Z_S0 + 0x57u) = 2u;
    sound_voice_log_reset();
    f(Z_S0, Z_R0, 0u);
    v.f = (float)(2.0 + -0.7);
    CHECK_EQ_INT((int)DSD(0x00104738u), (int)v.u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000E4E08);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), (int)v.u);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 4);
    CHECK_EQ_INT((int)DSW(0x00104758u), 0x0201);
    CHECK_EQ_INT((int)sound_voice_log_count(), 1);
    CHECK_EQ_INT((int)sound_voice_log_at(0), 0x7D);
    sound_voice_log_reset();
    v.f = 1.5f;
    DSD(0x00104738u) = v.u;
    DSB(Z_S0 + 0x57u) = 3u;
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSD(0x00104738u), 0x3F800000);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 3);
}

int test_p2_0c(void)            { return u6b_run(p2_check_0c); }
'''
ANCHOR = "int test_p2_1c(void)            { return u6b_run(p2_check_1c); }\n"
sub("port/tests/test_fight.c", ANCHOR, ANCHOR + TEST)
sub("port/tests/test.h", "    X(test_p2_1c) \\\n", "    X(test_p2_1c) \\\n    X(test_p2_0c) \\\n")
print("t8_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected:

```
t8_test applied
port/tests/test_fight.c:45466:51: error: use of undeclared identifier 'fighter_212cc'
port/tests/test_fight.c:45467:51: error: use of undeclared identifier 'fighter_22638'
2 errors generated.
```

- [ ] **Step 3: the port and the seam.**

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
sub(F, '''void fighter_36870(u32 rec)
{
''', '''void fighter_36870(u32 rec)
{
    PR_SEAM(0x36870u, rec);
''')

FIGHTER_C = r'''
#define P2_ANIM_212CC_C1 0x000E4A18u  /* 0x21337: character 1 */
#define P2_ANIM_212CC_C6 0x000E16E6u  /* 0x2133E: character 6 */
#define P2_A82EC         0x000A82ECu  /* 0x22690/0x226A6: [char] a signed word (the dword 0xA82EA's high half) and its low byte */
#define P2_A8300         0x000A8300u  /* 0x226D3: [char] the frame count below which +0x5D stays at least 1 */
#define P2_D_8098C       0x0008098Cu  /* 0x22725: the double -0.7 */
#define P2_D_80980       0x00080980u  /* 0x22752: the double 0.1 */
#define P2_F_80988       0x00080988u  /* 0x22760: the float 3.0 */
#define P2_ANIM_22638_1  0x000E4DCEu  /* 0x227E2: state 1 */
#define P2_ANIM_22638_4  0x000E4E08u  /* 0x22863: state 2, command bit 0 */
#define P2_ANIM_22638_5  0x000E4E34u  /* 0x2289E: state 2, latched bit 2 */
#define P2_ANIM_22638_6  0x000E4E4Au  /* 0x228CF: state 2, latched bit 1 */
#define P2_ANIM_22638_7  0x000E4E72u  /* 0x2290B: state 2, latched bit 3 */

/* 0x212CC — record §P2.9. The slot +0x0C callback 0x21374 stores (the dword
 * at 0x213C0; 0x3531C case 7, (slot, rec, side), EAX unread). EBX = side, the
 * context 0x33950(side); ECX = the EDX record (0x212D0). By the own slot's
 * +0x57: 0 with the word 0xA81AE below its word +0x88 (signed `jge`) steps it
 * to 1; 1 takes p = DS_001077A8[rec+0x51] (none: nothing) and, for p's +0x7A
 * 1 or 6, the own record on 0xE4A18 or 0xE16E6 at 3.0 (0x3C4CC), then the own
 * slot's +0x52 = 9, +0x57 = 2, +0x8A = 0; above 1 nothing. */
void fighter_212cc(u32 slot, u32 rec, u32 side)
{
    u32 ctx[6];
    u8 st;
    (void)slot;
    fighter_ctx_same(ctx, side);                            /* 0x212D0..0x212D6 0x33950 */
    st = DSB(ctx[2] + 0x57u);                               /* 0x212DB/0x212DF */
    if (st == 0u) {                                         /* 0x212E2/0x212E4, 0x212ED */
        if ((s16)DSW(P2_A81AE) >= (s16)DSW(ctx[2] + 0x88u)) return;   /* 0x212F5..0x21307 */
        DSB(ctx[2] + 0x57u) = 1u;                           /* 0x2130D */
        return;
    }
    if (st != 1u) return;                                   /* 0x212E6 */
    {
        u32 p = DSD(DS_001077A8 + (u32)DSB(rec + 0x51u) * 4u);   /* 0x21316..0x2131E */
        u8 c;
        if (p == 0u) return;                                /* 0x21324/0x21326 */
        c = DSB(p + 0x7Au);                                 /* 0x21328 */
        if (c == 1u)                                        /* 0x2132B..0x2132F */
            hit_anim_start_b(ctx[4], P2_ANIM_212CC_C1, 0x40400000u);   /* 0x21337, 0x21343..0x2134C 0x3C4CC */
        else if (c == 6u)                                   /* 0x21331/0x21333 */
            hit_anim_start_b(ctx[4], P2_ANIM_212CC_C6, 0x40400000u);   /* 0x2133E, 0x21343..0x2134C 0x3C4CC */
    }
    DSB(ctx[2] + 0x52u) = 9u;                               /* 0x21355 */
    DSB(ctx[2] + 0x57u) = 2u;                               /* 0x2135D */
    DSB(ctx[2] + 0x8Au) = 0u;                               /* 0x21365 */
}

static float p2_f32(u32 a)
{
    union { float f; u32 u; } v;
    v.u = DSD(a);
    return v.f;
}

static void p2_set_f32(u32 a, float f)
{
    union { float f; u32 u; } v;
    v.f = f;
    DSD(a) = v.u;
}

static double p2_f64(u32 a)
{
    double d;
    memcpy(&d, mem + a, sizeof d);
    return d;
}

/* 0x22638 — record §P2.9. The slot +0x0C callback 0x22938 stores (the dword
 * at 0x22963; 0x3531C case 7, EAX unread); its bytes start right after its
 * own jump table 0x22618. EBX = side, the context 0x33950(side); the EAX slot
 * and EDX record are not read. Each frame: the side's count 0x104758 + 1; when
 * the other side's stick (DS_001088E0 & 0xF0) or the other slot's +0x63 is
 * set, the other slot's +0x5D drains by the byte 0xA82EC[its character] (to 0
 * when at most that word); while 0xA8300[that character] exceeds the count,
 * +0x5D stays at least 1; the side's float 0x104738 takes -0.7 down to 1.0 on
 * the command's bit 0, else +0.1 up to 3.0; a command with bits 1..3 is
 * latched in 0x104754. Then by the own slot's +0x57 (table 0x22618): 0 waits
 * for the count above 0x14 (+0x57 = 1); 1 starts the own record on 0xE4DCE at
 * 3.0 (+0x57 = 3, +0x8A = 0); 2 ends (0x36870 on the other record when +0x5D
 * is 0, +0x57 = 1 when the other slot is not in state 0xA) or starts the own
 * record on 0xE4E08 at the float (bit 0; +0x57 = 4, voice 0x7D), 0xE4E34
 * (latched bit 2; 5), 0xE4E4A (bit 1; 6) or 0xE4E72 (bit 3; 7) at 3.0 with
 * the voice 0x78; 3..7 nothing. PORT: the raw adds in x87 extended precision
 * and compares the +0.1 sum before rounding (0x22750..0x22769); the port adds
 * in double: for the floats this code keeps (1.0..3.0) the stored float and
 * the comparison are the same (record §P2.9). */
void fighter_22638(u32 slot, u32 rec, u32 side)
{
    u32 ctx[6];
    u32 c, cmd;
    (void)slot;
    (void)rec;
    fighter_ctx_same(ctx, side);                            /* 0x2263C..0x22640 0x33950 */
    {
        u32 t = (u32)DSW(DS_001088E0 + ctx[1] * 2u) & 0xF0u;   /* 0x22645..0x22662 */
        DSW(P2_104758 + ctx[0] * 2u) = (u16)(DSW(P2_104758 + ctx[0] * 2u) + 1u);   /* 0x22653..0x22668 */
        if (t != 0u || DSB(ctx[3] + 0x63u) != 0u) {         /* 0x22670..0x2267C */
            c = (u32)DSB(ctx[3] + 0x7Au);                   /* 0x2267E..0x2268A */
            if ((s32)(u32)DSB(ctx[3] + 0x5Du) <= (s32)(s16)DSW(P2_A82EC + c * 2u))   /* 0x2268C..0x226A4 */
                DSB(ctx[3] + 0x5Du) = 0u;                   /* 0x226B5 */
            else
                DSB(ctx[3] + 0x5Du) = (u8)(DSB(ctx[3] + 0x5Du) - DSB(P2_A82EC + c * 2u));   /* 0x226A6..0x226B0 */
        }
    }
    c = (u32)DSB(ctx[3] + 0x7Au);                           /* 0x226BD..0x226C9 */
    if ((s16)DSW(P2_A8300 + c * 2u) > (s16)DSW(P2_104758 + ctx[0] * 2u)) {   /* 0x226D3..0x226E2 */
        u8 b = DSB(ctx[3] + 0x5Du);                         /* 0x226E4..0x226EB */
        DSB(ctx[3] + 0x5Du) = (u8)(b >= 1u ? b : 1u);       /* 0x226F0..0x226FE */
    }
    cmd = (u32)DSW(DS_001088E0 + ctx[0] * 2u);              /* 0x22701/0x22704 */
    if ((cmd & 1u) != 0u) {                                 /* 0x2270C..0x2271D */
        float f = (float)((double)p2_f32(P2_104738 + ctx[0] * 4u) + p2_f64(P2_D_8098C));   /* 0x2271F..0x2272B */
        p2_set_f32(P2_104738 + ctx[0] * 4u, f);
        if (f < 1.0f)                                       /* 0x22731..0x2273C */
            p2_set_f32(P2_104738 + ctx[0] * 4u, 1.0f);      /* 0x2273E */
    } else {
        double s = (double)p2_f32(P2_104738 + ctx[0] * 4u) + p2_f64(P2_D_80980);   /* 0x2274A..0x22758 */
        p2_set_f32(P2_104738 + ctx[0] * 4u, (float)s);      /* 0x2275A */
        if (s > (double)p2_f32(P2_F_80988))                 /* 0x22760..0x22769 */
            p2_set_f32(P2_104738 + ctx[0] * 4u, 3.0f);      /* 0x2276B */
    }
    if ((DSW(DS_001088E0 + ctx[0] * 2u) & 0x0Eu) != 0u)     /* 0x22775..0x2278C */
        DSW(P2_104754 + ctx[0] * 2u) = DSW(DS_001088E0 + ctx[0] * 2u);   /* 0x2278E/0x22795 */
    switch (DSB(ctx[2] + 0x57u)) {                          /* 0x2279C..0x227BC table 0x22618 */
    case 0:
        if ((s16)DSW(P2_104758 + ctx[0] * 2u) <= 0x14) return;   /* 0x227C3..0x227CF */
        DSB(ctx[2] + 0x57u) = 1u;                           /* 0x227D9 */
        return;
    case 1:
        actors_anim_begin(ctx[4], P2_ANIM_22638_1, 0x40400000u);   /* 0x227E2..0x227F0 0x2BC30 */
        DSB(ctx[2] + 0x57u) = 3u;                           /* 0x227F9 */
        DSB(ctx[2] + 0x8Au) = 0u;                           /* 0x22801 */
        return;
    case 2:
        if (DSB(ctx[3] + 0x5Du) < 1u) {                     /* 0x2280D..0x2281D */
            fighter_36870(ctx[5]);                          /* 0x2281F/0x22823 */
            DSB(ctx[2] + 0x57u) = 1u;                       /* 0x2282C */
            return;
        }
        if (DSB(ctx[3] + 0x53u) != 0x0Au) {                 /* 0x22835..0x2283D */
            DSB(ctx[2] + 0x57u) = 1u;                       /* 0x22843 */
            return;
        }
        if ((DSW(DS_001088E0 + ctx[0] * 2u) & 1u) != 0u) {  /* 0x2284C..0x2285E */
            actors_anim_begin(ctx[4], P2_ANIM_22638_4, DSD(P2_104738 + ctx[0] * 4u));   /* 0x22860..0x22873 0x2BC30 */
            DSB(ctx[2] + 0x57u) = 4u;                       /* 0x2287C */
            (void)sound_voice(0x7Du);                       /* 0x22880, 0x2292B 0x2C3FC */
            return;
        }
        if ((DSW(P2_104754 + ctx[0] * 2u) & 4u) != 0u) {    /* 0x2288A..0x2289C */
            actors_anim_begin(ctx[4], P2_ANIM_22638_5, 0x40400000u);   /* 0x2289E..0x228AC 0x2BC30 */
            DSB(ctx[2] + 0x57u) = 5u;                       /* 0x228B5 */
            (void)sound_voice(0x78u);                       /* 0x22926/0x2292B 0x2C3FC */
            return;
        }
        if ((DSW(P2_104754 + ctx[0] * 2u) & 2u) != 0u) {    /* 0x228BB..0x228CD */
            actors_anim_begin(ctx[4], P2_ANIM_22638_6, 0x40400000u);   /* 0x228CF..0x228DD 0x2BC30 */
            DSB(ctx[2] + 0x57u) = 6u;                       /* 0x228E6 */
            (void)sound_voice(0x78u);                       /* 0x228EA/0x228EF 0x2C3FC */
            return;
        }
        if ((DSW(P2_104754 + ctx[0] * 2u) & 8u) != 0u) {    /* 0x228F9..0x22909 */
            actors_anim_begin(ctx[4], P2_ANIM_22638_7, 0x40400000u);   /* 0x2290B..0x22919 0x2BC30 */
            DSB(ctx[2] + 0x57u) = 7u;                       /* 0x22922 */
            (void)sound_voice(0x78u);                       /* 0x22926/0x2292B 0x2C3FC */
        }
        return;
    default:                                                /* 3..7 (0x22930), above 7 (`ja` 0x227A5) */
        return;
    }
}
'''
sub(F, '''    (void)sound_voice((u32)DSW(P2_VOICE_C75AA + (u32)DSB(ctx[3] + 0x7Au) * 2u));   /* 0x225F5..0x2260E 0x2C3FC */
}
''', '''    (void)sound_voice((u32)DSW(P2_VOICE_C75AA + (u32)DSB(ctx[3] + 0x7Au) * 2u));   /* 0x225F5..0x2260E 0x2C3FC */
}
''' + FIGHTER_C)
sub("port/src/game/fighter.h", "void fighter_22588(u32 side);\n",
    """void fighter_22588(u32 side);
/* §P2.9: the slot +0x0C callbacks they store (slot, rec, side). */
void fighter_212cc(u32 slot, u32 rec, u32 side);
void fighter_22638(u32 slot, u32 rec, u32 side);
""")
sub("port/src/game/actors.c", "    fn_register(0x22588u, (void (*)(void))fighter_22588);\n",
    """    fn_register(0x22588u, (void (*)(void))fighter_22588);
    /* PORT: record 2026-10-02-reverse-p2 §P2.9. The slot +0x0C callbacks
     * 0x21374 and 0x22938 store (the dwords at 0x213C0 and 0x22963; 0x3531C
     * case 7, (slot, rec, side)). */
    fn_register(0x212CCu, (void (*)(void))fighter_212cc);
    fn_register(0x22638u, (void (*)(void))fighter_22638);
""")

BINDINGS = r'''/* §P2.9: the slot +0x0C callbacks as 0x3531C case 7 calls them (0x35431), mask 0. */
static void b_212cc(const u32 *r, u32 *eax)            { fighter_212cc(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_22638(const u32 *r, u32 *eax)            { fighter_22638(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_212cc_at(const u32 *r, int mode)
{
    u32 ctx[6];
    u8 st;
    fighter_ctx_same(ctx, r[R_EBX]);
    st = DSB(ctx[2] + 0x57u);
    if (st == 0u) {
        if (mode == 1 ? DSW(0x000A81AEu) >= DSW(ctx[2] + 0x88u)
                      : (s16)DSW(0x000A81AEu) >= (s16)DSW(ctx[2] + 0x88u)) return;
        DSB(ctx[2] + 0x57u) = 1u;
        return;
    }
    if (st != 1u) return;
    {
        u32 p = DSD(DS_001077A8 + (mode == 2 ? ctx[0] : (u32)DSB(r[R_EDX] + 0x51u)) * 4u);
        u8 c;
        if (p == 0u) return;
        c = DSB(p + 0x7Au);
        if (c == 1u) hit_anim_start_b(ctx[4], mode == 3 ? 0x000E16E6u : 0x000E4A18u, 0x40400000u);
        else if (c == 6u) hit_anim_start_b(ctx[4], mode == 3 ? 0x000E4A18u : 0x000E16E6u, 0x40400000u);
    }
    DSB(ctx[2] + 0x52u) = 9u;
    DSB(ctx[2] + 0x57u) = 2u;
    DSB(ctx[2] + 0x8Au) = 0u;
}
static void m_212cc(const u32 *r, u32 *eax)            /* the two characters' streams swapped */
{
    m_212cc_at(r, 3);
    *eax = 0u;
}
static void m_212cc_signed(const u32 *r, u32 *eax)     /* the state-0 bound compared unsigned */
{
    m_212cc_at(r, 1);
    *eax = 0u;
}
static void m_212cc_side(const u32 *r, u32 *eax)       /* the pointer by the context's side, not rec+0x51 */
{
    m_212cc_at(r, 2);
    *eax = 0u;
}
static void m_22638(const u32 *r, u32 *eax)            /* the voice 0x78 for the bit-0 start too */
{
    u32 ctx[6], c, cmd, a = DS_001088E0;
    union { float f; u32 u; } v;
    double d;
    fighter_ctx_same(ctx, r[R_EBX]);
    {
        u32 t = (u32)DSW(a + ctx[1] * 2u) & 0xF0u;
        DSW(0x00104758u + ctx[0] * 2u) = (u16)(DSW(0x00104758u + ctx[0] * 2u) + 1u);
        if (t != 0u || DSB(ctx[3] + 0x63u) != 0u) {
            c = (u32)DSB(ctx[3] + 0x7Au);
            if ((s32)(u32)DSB(ctx[3] + 0x5Du) <= (s32)(s16)DSW(0x000A82ECu + c * 2u))
                DSB(ctx[3] + 0x5Du) = 0u;
            else
                DSB(ctx[3] + 0x5Du) = (u8)(DSB(ctx[3] + 0x5Du) - DSB(0x000A82ECu + c * 2u));
        }
    }
    c = (u32)DSB(ctx[3] + 0x7Au);
    if ((s16)DSW(0x000A8300u + c * 2u) > (s16)DSW(0x00104758u + ctx[0] * 2u)) {
        u8 b = DSB(ctx[3] + 0x5Du);
        DSB(ctx[3] + 0x5Du) = (u8)(b >= 1u ? b : 1u);
    }
    cmd = (u32)DSW(a + ctx[0] * 2u);
    v.u = DSD(0x00104738u + ctx[0] * 4u);
    if ((cmd & 1u) != 0u) {
        memcpy(&d, mem + 0x0008098Cu, sizeof d);
        v.f = (float)((double)v.f + d);
        if (v.f < 1.0f) v.f = 1.0f;
        DSD(0x00104738u + ctx[0] * 4u) = v.u;
    } else {
        double sum;
        memcpy(&d, mem + 0x00080980u, sizeof d);
        sum = (double)v.f + d;
        v.f = (float)sum;
        if (sum > 3.0) v.f = 3.0f;
        DSD(0x00104738u + ctx[0] * 4u) = v.u;
    }
    if ((cmd & 0x0Eu) != 0u) DSW(0x00104754u + ctx[0] * 2u) = (u16)cmd;
    *eax = 0u;
    switch (DSB(ctx[2] + 0x57u)) {
    case 0:
        if ((s16)DSW(0x00104758u + ctx[0] * 2u) > 0x14) DSB(ctx[2] + 0x57u) = 1u;
        return;
    case 1:
        actors_anim_begin(ctx[4], 0x000E4DCEu, 0x40400000u);
        DSB(ctx[2] + 0x57u) = 3u;
        DSB(ctx[2] + 0x8Au) = 0u;
        return;
    case 2:
        if (DSB(ctx[3] + 0x5Du) < 1u) { fighter_36870(ctx[5]); DSB(ctx[2] + 0x57u) = 1u; return; }
        if (DSB(ctx[3] + 0x53u) != 0x0Au) { DSB(ctx[2] + 0x57u) = 1u; return; }
        if ((cmd & 1u) != 0u) {
            actors_anim_begin(ctx[4], 0x000E4E08u, DSD(0x00104738u + ctx[0] * 4u));
            DSB(ctx[2] + 0x57u) = 4u;
            (void)sound_voice(0x78u);
        } else if ((DSW(0x00104754u + ctx[0] * 2u) & 4u) != 0u) {
            actors_anim_begin(ctx[4], 0x000E4E34u, 0x40400000u);
            DSB(ctx[2] + 0x57u) = 5u;
            (void)sound_voice(0x78u);
        } else if ((DSW(0x00104754u + ctx[0] * 2u) & 2u) != 0u) {
            actors_anim_begin(ctx[4], 0x000E4E4Au, 0x40400000u);
            DSB(ctx[2] + 0x57u) = 6u;
            (void)sound_voice(0x78u);
        } else if ((DSW(0x00104754u + ctx[0] * 2u) & 8u) != 0u) {
            actors_anim_begin(ctx[4], 0x000E4E72u, 0x40400000u);
            DSB(ctx[2] + 0x57u) = 7u;
            (void)sound_voice(0x78u);
        }
        return;
    default:
        return;
    }
}
/* 0x22638's two boundary mutants run the port, then redo the one store their bug changes. */
static void m_22638_signed(const u32 *r, u32 *eax)     /* the +0x5D floor's count compared unsigned */
{
    u32 ctx[6], c;
    u8 b5d;
    int floor_s, floor_u;
    fighter_ctx_same(ctx, r[R_EBX]);
    fighter_22638(r[R_EAX], r[R_EDX], r[R_EBX]);
    c = (u32)DSB(ctx[3] + 0x7Au);
    floor_s = (s16)DSW(0x000A8300u + c * 2u) > (s16)DSW(0x00104758u + ctx[0] * 2u);
    floor_u = DSW(0x000A8300u + c * 2u) > DSW(0x00104758u + ctx[0] * 2u);
    b5d = DSB(ctx[3] + 0x5Du);
    if (floor_s && !floor_u && b5d == 1u) DSB(ctx[3] + 0x5Du) = 0u;
    *eax = 0u;
}
static void m_22638_byte5d(const u32 *r, u32 *eax)     /* the drain compares +0x5D as a signed byte */
{
    u32 ctx[6], c;
    s8 b;
    int drain;
    fighter_ctx_same(ctx, r[R_EBX]);
    c = (u32)DSB(ctx[3] + 0x7Au);
    b = (s8)DSB(ctx[3] + 0x5Du);
    drain = (DSW(DS_001088E0 + ctx[1] * 2u) & 0xF0u) != 0u || DSB(ctx[3] + 0x63u) != 0u;
    fighter_22638(r[R_EAX], r[R_EDX], r[R_EBX]);
    if (drain && b < 0 && (s32)b <= (s32)(s16)DSW(0x000A82ECu + c * 2u)) DSB(ctx[3] + 0x5Du) = 0u;
    *eax = 0u;
}

'''
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_22588@order",      m_22588_order,  0x00000000u },\n',
    """    { "fighter_22588@order",      m_22588_order,  0x00000000u },
    { "fighter_212cc",            b_212cc,        0x00000000u },
    { "fighter_22638",            b_22638,        0x00000000u },
    { "fighter_212cc@mutant",     m_212cc,        0x00000000u },
    { "fighter_212cc@signed",     m_212cc_signed, 0x00000000u },
    { "fighter_212cc@side",       m_212cc_side,   0x00000000u },
    { "fighter_22638@mutant",     m_22638,        0x00000000u },
    { "fighter_22638@signed",     m_22638_signed, 0x00000000u },
    { "fighter_22638@byte5d",     m_22638_byte5d, 0x00000000u },
""")
print("t8_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_212cc fighter_22638; do
  python3 tools/diff_verify.py --image /tmp/pr_p2_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected:

```
t8_port applied
all checks passed
| fighter_212cc | 0x212CC | 10 | 16/16 | VERIFIED | 33950 allow unverified, 3C4CC stub unverified |
| fighter_212cc@mutant | 0x212CC | 10 | 16/16 | MISMATCH | 33950 allow unverified, 3C4CC stub unverified |
| fighter_212cc@signed | 0x212CC | 10 | 16/16 | MISMATCH | 33950 allow unverified, 3C4CC stub unverified |
| fighter_212cc@side | 0x212CC | 10 | 16/16 | MISMATCH | 33950 allow unverified, 3C4CC stub unverified |
| fighter_22638 | 0x22638 | 16 | 36/36 | VERIFIED | 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 36870 stub unverified |
| fighter_22638@mutant | 0x22638 | 16 | 36/36 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 36870 stub unverified |
| fighter_22638@signed | 0x22638 | 16 | 36/36 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 36870 stub unverified |
| fighter_22638@byte5d | 0x22638 | 16 | 36/36 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 36870 stub unverified |
```

- [ ] **Step 4: the E2 table.** Expected (`0x212CC` is a supplement entry; `0x22638` is outside E2's list):

```
entry-triage: FAIL: docs/superpowers/plans/2026-10-01-reverse-e2-triage.md differs from a fresh run (regenerate it with --out)
make: *** [entry-triage] Error 1
entry-triage: targets 297 unported, 198 ported; supplement 131 (22 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 31 in unported code, 84 in ported code, 19 nowhere
| callbacks | 9 | 62 |
| finishers | 0 | 9 |
| animation-targets | 54 | 58 |
| stubs | 60 |
 1 file changed, 1 insertion(+), 1 deletion(-)
```

- [ ] **Step 5: the task gate.** Expected:

```
Ran 159 tests in N.NNNs
OK
diff-verify: 54/54 functions VERIFIED; 84/84 mutants detected; 1 named gaps; 7/41 rows with callees closed (13 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
Ran 44 tests in N.NNNs
OK
entry-triage: targets 297 unported, 198 ported; supplement 131 (22 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 6: every new assertion can fail.**

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


def mutate_py(path, old, new, test):
    """Apply one mutation to a tool, run one unittest, print its last line, restore the file."""
    p = pathlib.Path(path)
    keep = p.read_text()
    assert keep.count(old) == 1, (path, old[:60])
    p.write_text(keep.replace(old, new))
    try:
        out = subprocess.run(["python3", "-m", "unittest", test], capture_output=True, text=True).stderr
        print("%s: %s" % (path, out.strip().splitlines()[-1]))
    finally:
        p.write_text(keep)



mutate("port/src/game/actors.c", "    fn_register(0x22638u, (void (*)(void))fighter_22638);\n", "")
mutate("port/src/game/fighter.c",
       "        if (f < 1.0f)                                       /* 0x22731..0x2273C */",
       "        if (f < 0.5f)                                       /* 0x22731..0x2273C */")
mutate("port/src/game/fighter.c",
       "        DSB(ctx[2] + 0x57u) = 1u;                           /* 0x2130D */",
       "        DSB(ctx[2] + 0x57u) = 2u;                           /* 0x2130D */")
mutate_py("tools/diff_emu.py", "    0x227BC: (0x227A3, 0x22618, 8),\n", "",
          "tools.tests.test_diff_verify.RealFunctionTests.test_each_resolved_jump_table_matches_the_bytes")
PY
```

Expected, then `all checks passed`:

```
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:45467: 0x22638 is registered']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45527: 1061997773 != 1065353216']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:45481: 2 != 1']
tools/diff_emu.py: FAILED (failures=1)
all checks passed
```

- [ ] **Step 7: commit** (Task 2's files plus `tools/diff_emu.py`), message `fighter: port the +0x0C callbacks 0x212CC and 0x22638 (its jump table resolved), seam 0x36870; E2 table regenerated (track P batch 2)` and the trailer.

---

### Task 9: closure: the full gate, the docs, the record

**Files:** `AGENTS.md`, `docs/PROGRESS.md`, `docs/superpowers/plans/2026-10-02-reverse-p2-derivations.md` (§P2.13). **Interfaces:** consumes Tasks 2-8.

- [ ] **Step 1: the full gate** (Task 1 Step 1's commands with the log `/tmp/pr_p2_final.log`) and the WAV (Task 1 Step 2).

Expected: `EXIT=0`, `ORACLES-EQUAL`, `WAV-SAME`, and

```
diff-verify: 54/54 functions VERIFIED; 84/84 mutants detected; 1 named gaps; 7/41 rows with callees closed (13 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 297 unported, 198 ported; supplement 131 (22 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 31 in unported code, 84 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
gp-idle-loss: frames: first unexplained 2064, ratchet N 2064 ok
gp-idle-loss: trace: 0 differing through 8319; ratchet N 8320 ok
gp-u5-charsel: frames: first unexplained 516, ratchet N 516 ok
gp-u5-charsel: trace: 0 differing through 1512; ratchet N 1513 ok
gp-u6-moves-b: frames: first unexplained 1005, ratchet N 1005 ok
gp-u6-moves-b: trace: first differing 2262, ratchet N 2262 ok
gp-u6-moves-b: moves: first differing 2949, ratchet N 2949 ok
gp-keys-fight: effects: first not reproduced 11, ratchet N 11 ok
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
```

- [ ] **Step 2: the docs.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


# AGENTS.md: the two seam forms P2 added, and the RA sentence its port made stale
sub("AGENTS.md", '''  The arguments are the C signature's, in order (a pointer into `mem[]` as its offset); the `E.Call`
  names the original's registers and stack slots in the same order.''',
    '''  The arguments are the C signature's, in order (a pointer into `mem[]` as its offset); the `E.Call`
  names the original's registers and stack slots in the same order. A callee with no argument opens with
  `PR_SEAM0(0xADDR)`; a buffer on the caller's stack that the callee cannot run on both sides is passed by
  value (its bytes as little-endian dwords, the `E.Call` naming `[reg]`, `[reg+N]`; record
  `2026-10-02-reverse-p2-derivations.md` §P2.7).''')
sub("AGENTS.md", '''N and F are the end of the port's replay in six of the seven (RA diverges at f=0x7B7 on the unported `0x14EF8`/`0x14F50`, whose port drops its two miss rows and re-measures its N and F); nothing past them is covered''',
    '''N and F are the end of the port's replay in all seven (RA reached its end once track P batch 2 ported `0x14EF8`/`0x14F50` and their streams' targets: N 726 -> 1072, F 1978 -> 2274, record 2026-10-02-reverse-p2 §P2.4); nothing past them is covered''')

PROGRESS = '''

**Track P batch 2: the move callbacks `0x14EF8..0x3DCEC` and the callbacks they store (plan `2026-10-02-reverse-p2-move-callbacks.md`, record `2026-10-02-reverse-p2-derivations.md`).** Twenty-four functions ported from the raw and differentially verified, every block hit and every store observable (record §P2.1: the roadmap's 19, the two +0x0C callbacks `0x22938`/`0x22A00` store that no list had, `0x22638` after its own jump table and `0x229FC`, the `ret` ending `0x229E8`, and the `0xD000` targets `0x14FA8 0x14FF8 0x150AC` of `0x14EF8`/`0x14F50`'s streams, moved from P4/P5 because the U8 replay reaches them): the guard-shaped callbacks `0x237D0 0x2381C 0x3DADC 0x3DB34 0x3D10C 0x22A00 0x14EF8 0x14F50`, the unconditional `0x15478 0x3DCEC 0x21114`, the slot-arming `0x21374 0x22938`, their +0x18 hooks `0x2116C 0x22510`, +0x1C callbacks `0x211F0 0x22588` (with `0x22404`) and +0x0C callbacks `0x212CC 0x22638`. Nine callees gained seams (`0x34D8C 0x18C14 0x18AF8 0x39834 0x39A10 0x3C208 0x3C358 0x22404 0x36870`); the harness compares `0x18C14`'s stack flags by value (`[reg+N]` call arguments, record §P2.7), `PR_SEAM0` seams a callee with no argument, `0x22638`'s table is in `RESOLVED_JUMPS`, and `fn_register` skips a repeated identical pair (the four `actors_init` runs of `run_tests` had filled the 1 300-entry table). `gp-u8-right-arcade` drops its two misses and now replays to its X record: N 726 -> 1072 (capture 1071 equals the port's last frame), F 1978 -> 2274 (no traced difference); no other gp miss set held a P2 member, and every other oracle line is unchanged. `make diff-verify`: `54/54 functions VERIFIED; 84/84 mutants detected; 1 named gaps; 7/41 rows with callees closed (13 have none)`. E2 table: 297 unported / 198 ported targets, callbacks 9 / 62, animation targets 54 / 58, supplement 22 unported, voice 31 / 84 / 19. Named gaps: the other stream targets of P2's streams (`0x213F0 0x213F4 0x22338 0x22494 0x224EC 0x3DB8C 0x3DC3C 0x22A40 0x229E8`, owners P4-P7, reached by no capture); `0x22638`'s x87 sums are computed in double (equal for the floats 1.0..3.0 the code keeps); the nine new stubs get their rows in C1. `port_progress.py` stays `771 1203 64` (none of the 24 is a Ghidra function) and README is untouched.
'''
p = pathlib.Path("docs/PROGRESS.md")
s = p.read_text()
p.write_text(s.rstrip("\n") + PROGRESS)
print("t9_docs applied")
PY
```

- [ ] **Step 3: the record's §P2.13.** Append the implementation's closure paragraph: the commit range, each task's counter and E2 lines as measured (the planner's replay measured the table below), the final gate's lines from Step 1 and the time it took, and any deviation from this plan's expected output (a deviation is a finding: record the raw fact and the address).

| after | diff-verify | entry-triage |
|---|---|---|
| `1085402` | `30/30 ...; 47/47 ...; 1/18 ... (12 have none)` | `313 / 182`; supplement 28 unported; voice `40 / 75 / 19` |
| Task 2 | `37/37 ...; 55/55 ...; 2/24 ... (13 have none)` | `307 / 188`; supplement 28 unported; voice `35 / 80 / 19` |
| Task 3 | `42/42 ...; 60/60 ...; 2/29 ... (13 have none)` | `302 / 193`; supplement 28 unported; voice `33 / 82 / 19` |
| Task 4 | `45/45 ...; 64/64 ...; 5/32 ... (13 have none)` | `299 / 196`; supplement 28 unported; voice `33 / 82 / 19` |
| Task 5 | `47/47 ...; 67/67 ...; 6/34 ... (13 have none)` | `297 / 198`; supplement 28 unported; voice `33 / 82 / 19` |
| Task 6 | `49/49 ...; 72/72 ...; 6/36 ... (13 have none)` | `297 / 198`; supplement 26 unported; voice `33 / 82 / 19` |
| Task 7 | `52/52 ...; 78/78 ...; 6/39 ... (13 have none)` | `297 / 198`; supplement 23 unported; voice `31 / 84 / 19` |
| Task 8 | `54/54 ...; 84/84 ...; 7/41 ... (13 have none)` | `297 / 198`; supplement 22 unported; voice `31 / 84 / 19` |
- [ ] **Step 4: commit.**

```bash
git add AGENTS.md docs/PROGRESS.md docs/superpowers/plans/2026-10-02-reverse-p2-derivations.md
git commit -m "docs: P2 closure (the move callbacks): AGENTS seam forms and the RA pins, PROGRESS, record §P2.13

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

Then the whole-branch review and, before the merge, the full gate again (Step 1).
