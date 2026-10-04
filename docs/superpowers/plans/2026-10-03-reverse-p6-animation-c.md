# P6: animation targets C, 18 functions (track P, batch 6) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the 18 animation targets of track P's batch 6 (`0x22338 0x22494 0x22A40 0x23F10 0x2400C 0x241F4 0x24338 0x24220 0x2BDA0 0x37DD4 0x3E160 0x40148 0x40170 0x45C98 0x47E04 0x47E30 0x482E4 0x48374`), seam the twelve callees they stub (`0x39280 0x39F40 0x13244 0x2A148 0x2BCF4 0x1890C 0x5D7DC 0x29BC8 0x37D18 0x13C70 0x3AA54 0x29C08`), each verified against the original's bytes with its callees stubbed (E3 §E3.10), every block hit and every store observable; drop `gp-u10-ending`'s `0x37DD4` row and show its pins exact; regenerate the E2 table with each port.

**Architecture:** One C function per original function. The 17 animation targets are appended to `port/src/game/fighter.c` (except `0x2BDA0`, whose body is the `anim_code_2BDA0` wrapper in `actors.c`), registered in `actors_init` (`port/src/game/actors.c`) as `anim_indirect`'s `(rec, operand)` code pointers; `0x24220` is appended with the case-10 adapter `fighter_24220_case10(slot, side)` and registered the same way. Each new stubbed callee opens with `PR_SEAM`/`PR_SEAM_RET` (`0x39280 0x39F40 0x13244 0x2A148 0x2BCF4 0x1890C 0x5D7DC 0x29BC8 0x37D18 0x13C70 0x3AA54 0x29C08`); each function has a binding and mutants in `port/tests/diff_runner.c`, a `Spec` in `tools/diff_verify.py` (`P6_SPECS`), seeded unit checks in `port/tests/test_fight.c`, and its row in the self-check counter. The harness needs no change: the ctx offsets after `0x2BC30`'s `ret 4` and the 16-poke/64-byte diffrun limits are baked into the specs.

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and `capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §3 decision 1 ("port all of O3-O6, whether or not a capture reaches them"), §4 track P ("port batches, each function verified by E"), §5.1-§5.3, §6 ("P: each ported function has a verification row; unhit blocks and non-emulable instructions named; counters and README percentage updated"), §7.

**Derivation record:** `docs/superpowers/plans/2026-10-03-reverse-p6-derivations.md` (§P6.1 the 18 members from the raw, §P6.2 callers, registers, masks and the callee declarations, §P6.3-§P6.10 each task from the bytes, §P6.11 the gp interactions, §P6.11 decisions and named gaps, §P6.12 results). Recipe: `2026-10-01-reverse-e3-derivations.md` §E3.10; lessons: the P3 record §P3.10 and its reviews.

**What the planner ran (2026-10-04, on `f5b5556`):** the full `make verify` on the untouched tree (the baseline, quoted in Task 1) and a prototype of Tasks 2-9 applied in order in this worktree. The final prototype state was measured: `python3 tools/diff_verify.py --image /tmp/p6_img.bin --self-check` printed `96/96 functions VERIFIED; 236/236 mutants detected; 1 named gaps; 12/82 rows with callees closed (14 have none)`; `make diff-verify entry-triage` ran 163 Python tests OK; `PR_ORACLE_REQUIRED=1 ./build/run_tests` printed `all checks passed`; `make entry-triage` regenerated the table to `targets 271 unported, 224 ported; supplement 131 (8 unported, 0 stale)` / `animation-targets 37/75` / `voice 15/100/19`; `make gp-ending-oracle` passed with `distinct=6` and every pin exact (331/9954/30/9954). Task 2's counter (`83/83 ...; 173/173 ...; 12/69 ...`) and E2 line were measured directly; Tasks 3-9's counter lines are the counter code's arithmetic on the final SPECS (each is one task's increment), and every task's rows are VERIFIED with every mutant detected (the final `P6_KINDS` pins each mutant's catching case set). The full `make verify` was not re-run on the prototype (the baseline took 25 min on a shared host); Task 10 runs it.

**Re-baseline note.** P4+P5 (and C1, if it merges first) execute before this plan. The counters, E2 lines and gp pins below are the measured `f5b5556` values plus this plan's increments. If a merge moves a value, Task 1 records the measured one in the ledger and every later expected counter adds this plan's increments to it: diff-verify rows +5, +3, +2, +2, +2, +1, +2, +1 (Tasks 2-9); mutants +15, +13, +7, +9, +11, +3, +12, +8; closed rows +1 then 0; rows with callees +5, +3, +2, +2, +2, +1, +2, +1; E2 ported targets +5, +1, +1, +1, +1, +1, +2, +6 (the Task 8 E2 line is the final one; Tasks 8-9's E2 deltas are +2 and 0). If the gp-u10-ending set differs from the one Task 7's script asserts, re-measure it (Task 7 Step 5), never predict it.

## Decisions needed from the user

**None.** The member list is the roadmap's (§P6.1); `0x24220` follows the case-10 adapter precedent; `0x2BDA0`'s rng runs real because it has its own row; the twelve seams follow AGENTS.md. User decisions D2 (span code, a named gap) and D3 (callee rows in C2) stand.

## The P-track roadmap

From record §P1.3 as corrected by P2 §P2.11, P3 §P3.11 and P6 §P6.13. Each line is one plan and one subagent-driven run.

| batch | ports | what |
|---|---|---|
| P1-P3 (merged) | 64 + 2 callee rows | the finisher/move callbacks and their callbacks |
| C1 (parallel) | about 45 rows | verification only: the ported callees P1-P3 stub |
| P4+P5 (parallel) | 33 | animation targets A+B |
| **P6** (this plan) | **18** | animation targets C (with `0x47E04 0x47E30 0x482E4 0x48374`) |
| C2 | about 20 rows | verification only: the rest of the stubbed callees (P6 adds twelve) |
| P7 | 14 | the unported direct callees with their callers (with `0x48254`) |
| P8 | 16 + triage | the rest |
| span | 0 | decision D2 |

None of P6's 18 is a Ghidra `FN_` function: `port_progress.py` stays `771 1203 64` / `731 731 100` and README does not move.

## Global Constraints

Copied verbatim from their sources:

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- Spec §8: "`make verify` is the gate after every task." User ruling (2026-10-01, speed-up): the per-task gate is the task's tests, `python3 tools/diff_verify.py --self-check` (or `make diff-verify` once Task 10 has updated the Python expectations), `make entry-triage` with the regenerated table and the gp oracles whose miss sets the task touches; the full `make verify` runs at the baseline (Task 1), the final task (Task 10) and before the merge, with the parallel-safe overrides `T=p6; SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md E2_IMAGE=/tmp/pr_${T}_e2.bin`. The 45 oracle lines (`grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>`) must equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; the `make audio-render` WAV must be byte-identical to `.superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav`; `python3 tools/port_progress.py` prints `771 1203 64` and `731 731 100`. A full `make verify` took 25 min here.
- AGENTS.md: "**The E2 target list is a gate** ... A commit that ports a target (a new `/* 0xADDR` header or `fn_register`) regenerates the table in the same commit (decision D3) ... A conflict in that file is resolved by regenerating it after the rebase, never by merging lines by hand."
- AGENTS.md: "One C function per original function, header comment `/* 0xADDR — spec section */`." / "Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment styles in `port/src`." / "Code addresses stored in data go through `fn_origin()` / `fn_resolve()`." / "**`port/src/symbols.h` is generated** ... Never hand-edit it; where the generator emits no name, use a local `#define` with the raw address."
- AGENTS.md: "A ported function that a differentially verified function stubs or records ... opens with `PR_SEAM(0xADDR, args...)` (void) or `PR_SEAM_RET(0xADDR, args...)` ... The arguments are the C signature's, in order (a pointer into `mem[]` as its offset)." Record E3 §E3.8: "A seam must be the callee's first statement".
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." / "**Register a test once** — add `X(test_foo)` to `TEST_CASES` in `port/tests/test.h`" / "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests, and never assert an unseeded BSS-zero".
- AGENTS.md: "Every env-gated driver and `--check` run arms `fn_resolve`'s miss log and must record exactly the pinned known-set": only `gp-u10-ending`'s set holds a P6 member; Task 7 drops it.
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style: `<area>: <what changed>`. **Never `git add -A`** — stage named files." No `Co-Authored-By` trailer (this is not a Claude session). Never run `make gp-capture`.
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.

## Review Focus

1. **The ctx offsets after a `ret 4` callee.** `0x47E30`'s `0x3AA54` argument is ctx[3], not ctx[2] (`[esp+0xC]` after `0x2BC30`'s `ret 4`); the planner's first prototype had ctx[2] and the diff row was MISMATCH. Every `[esp+N]` read after a `ret 4` call in the batch was re-derived from the bytes (§P6.4).
2. **The effective word tables.** `0xA83FA`, `0xA83EC` and `0xBD884` are the dword-load `sar 16` halves (`0xA83F8 + 2c`, `0xA83EA + 2c`, `0xBD882 + 2c`), not the raw table addresses; the pokes and the C both use the effective bases.
3. **The diffrun poke limits.** At most 16 pokes and 64 bytes each; the records are composed as buffers (`_p6_fill`) with 0xA5 in the gaps, and a mutant must not read an unseeded pointer (`@plus4`'s slot+8 is seeded to a record so it cannot crash).
4. **Inputs that separate alternatives.** The argument rec vs `orec` and the slot's record's +0x51 vs the argument's (`0x45C98`), the own vs the other side (`@side` of every ctx function), the own vs the other slot (`@pose`, `@pivot`), the branch bytes of both sides (`@flag`, `@byte`, `@side`), the range mask (`anim_2bda0@mask`), the palette word 4 for any non-zero side (`0x37DD4` s2).
5. **gp-u10-ending.** Task 7 drops the `0x37DD4` row and shows each pin exact (+1 fails: 332, 9955, 31, 9955); no other gp miss set holds a P6 member (checked from `k_gp_sets`).

## Where to run

The worktree `.worktrees/reverse-p6` (branch `reverse-p6`). It was cut from `main` `f5b5556`; this plan's two docs are its first commit. **Before Task 1**, rebase it once P4+P5 (and C1) are on `main`:

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse/.worktrees/reverse-p6
git rebase main
grep -c 'P45_SPECS\|C1_SPECS' tools/diff_verify.py   # 0 or the merged batches: keep both sides' specs
ls -d data .superpowers port/tests/ghidra_data.bin port/tests/title_screen_ref.ppm   # all four must exist
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -1
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/p6_img.bin && shasum /tmp/p6_img.bin
```

The last line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`. Every command runs from the worktree root. Ledger: `.superpowers/sdd/2026-10-03-reverse-p6-animation-c/progress.md`.

**How the code steps are written.** Each change is a `python3 - <<'PY'` script that replaces exact text and asserts the text occurs exactly once (`sub`), so a script either applies cleanly or stops naming the file and the text it could not find. Run each once, from the worktree root, in order. If a merge moved an anchor, re-apply that `sub` by hand at the same place, never elsewhere; the E2 table is regenerated, never merged. The `sub` helper is:

```python
def sub(path, old, new):
    import pathlib
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:90])
    p.write_text(s.replace(old, new))
```

**Task code blocks.** Each task's `sub` steps insert the exact final text; the anchors are the previous task's last line. The unit-test blocks append after the previous task's `test_p6_*` function; the bindings/mutants are inserted before `static const binding_t k_bindings[] = {`; the spec blocks extend `P6_SPECS` (Task 2 creates it with the constants and the helper functions); the registrations follow the previous task's last `fn_register`.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/game/fighter.c`, `fighter.h` | the 18 functions (17 + the case-10 adapter), their prototypes, the seams of `0x39280 0x39F40 0x13244 0x2A148 0x2BCF4 0x1890C 0x29BC8 0x37D18 0x13C70 0x3AA54 0x29C08` |
| `port/src/game/actors.c` | the `anim_code_*` wrappers, the `0x2BDA0` body, the forward declarations and the `actors_init` registrations; the seam of `0x2BCF4`/`0x2A148` |
| `port/src/game/rng.c`, `effects.c` | the seams of `0x5D7DC` and `0x13C70` |
| `port/tests/diff_runner.c` | the bindings and mutants (`b_*`/`m_*`, `k_bindings`) |
| `tools/diff_verify.py` | the `P6_*` `E.Call`s, `P6_PTRS`, `P6_SLOT_SEED`, `_p6_fill`, `P6_SPECS` |
| `tools/tests/test_diff_verify.py` | `P6_MASKS`, `P6_KINDS`, the exact-set assertions, `test_each_p6_mutant_is_caught_by_what_it_breaks`, the stub table, the counter line (Task 10) |
| `port/tests/test_fight.c`, `port/tests/test.h` | the `test_p6_*` unit checks |
| `port/tests/test_platform.c`, `Makefile`, `AGENTS.md` | gp-u10-ending's set loses `0x37DD4`; the `GP_ENDING_*` provenance and the re-measure list say so (Task 7) |
| `docs/superpowers/plans/2026-10-01-reverse-e2-triage.md` | regenerated in Tasks 2-9 |
| `docs/PROGRESS.md`, the record | Task 10 |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes `main` with P4+P5 merged at the worktree's head; produces the baseline log `/tmp/pr_p6_base.log`.

- [ ] **Step 1: the full gate on the untouched tree.**

```bash
T=p6; make verify SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att \
  FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 \
  GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md E2_IMAGE=/tmp/pr_${T}_e2.bin \
  > /tmp/pr_p6_base.log 2>&1; echo "EXIT=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_p6_base.log \
  | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
grep -E '^diff-verify:|^entry-triage: (targets|voice)|ratchet N' /tmp/pr_p6_base.log | sed 's/^gp_compare: //'
python3 tools/port_progress.py
```

Expected (measured at `f5b5556`): `EXIT=0`, `ORACLES-EQUAL`, and the baseline lines of record §P6.12:
`78/78 functions VERIFIED; 158/158 mutants detected; 1 named gaps; 11/64 rows with callees closed (14 have none)`;
`entry-triage: targets 288 unported, 207 ported; supplement 131 (9 unported, 0 stale); untrusted entries 30`;
`entry-triage: voice sites outside Ghidra 134: 28 in unported code, 87 in ported code, 19 nowhere`;
`771 1203 64` / `731 731 100`; and every gp ratchet at its pin (gp-idle-loss 2064/8320, gp-u5-charsel 516/1513, gp-u6-moves-b 1005/2262/moves 2949, gp-keys-fight 11, gp-twop 612/1506/1506, the seven gp-u8-* 1072/2274, 1076/2338, 1098/2402, 1107/2466, 1022/2274, 278/1174, 1087/2018, gp-u9-win 346/2150/path 8/win 3162, gp-u10-ending 331/9954/path 30/win 9954). If P4+P5's merge moved a value, record the measured line in the ledger; every later expected counter adds this plan's increments to it.

- [ ] **Step 2: the WAV.** `make audio-render AUDIO_WAV=/tmp/pr_p6.wav >/dev/null 2>&1; cmp /tmp/pr_p6.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-SAME` prints `WAV-SAME`.

---


### Task 2: the leaves: `0x2BDA0 0x241F4 0x47E04 0x40148 0x40170` (seams `0x5D7DC 0x37D18`)

**Files:** modify `port/src/game/rng.c`, `port/src/game/fighter.c`, `port/src/game/fighter.h`, `port/src/game/actors.c`, `tools/diff_verify.py`, `port/tests/diff_runner.c`, `port/tests/test_fight.c`, `port/tests/test.h`, the E2 table.

**Interfaces:** see the record §P6.1; the function signatures are in the code below.

- [ ] **Step 1: the seams.**

**`0x5D7DC` (rng.c).** The callee runs real in the `anim_2bda0` row, so it needs its seam:
`u32 rng_next(u32 range)` opens with `PR_SEAM_RET(0x5D7DCu, range);`.
**`0x37D18` (fighter.c).** `void fighter_37d18(u32 slot, u32 rec)` opens with
`PR_SEAM(0x37D18u, slot, rec);`; declare it in `fighter.h` (the harness's mutants call it).

- [ ] **Step 2: the functions (append to the end of `port/src/game/fighter.c` after `fighter_4844c`).**

```c
/* 0x241F4 — record §P6.4. The D100 target at the dword 0xE505E. ctx; stance
 * 0xA on the other side; voice 0x66. */
void fighter_241f4(u32 rec)
{
    u32 ctx[6];
    hit_anim_ctx(ctx, rec);                                 /* 0x241FC */
    fighter_3a95c(ctx[1], 0x0Au);                           /* 0x24201..0x2420A */
    (void)sound_voice(0x66u);                               /* 0x2420F/0x24214 */
}


/* 0x47E04 — record §P6.4. The D000 target at the dword 0xED9FC. ctx; voice
 * 0x66; stance 0xF on the other side. */
void fighter_47e04(u32 rec)
{
    u32 ctx[6];
    hit_anim_ctx(ctx, rec);                                 /* 0x47E0C */
    (void)sound_voice(0x66u);                               /* 0x47E11/0x47E1B */
    fighter_3a95c(ctx[1], 0x0Fu);                           /* 0x47E16/0x47E20/0x47E24 */
}


/* 0x40148 — record §P6.8. The D100 target at the dword 0xE8716. With the other
 * side's slot set (rec+0x51 ^ 1): 0x37D18(that slot, its record); then 0x104AE9
 * bit 2 clear. */
void fighter_40148(u32 rec)
{
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);   /* 0x40149..0x40153 */
    if (other == 0u) return;                                /* 0x4015C */
    fighter_37d18(other, DSD(other));                       /* 0x4015E/0x40160 */
    DSB(DS_00104AE9) &= 0xFBu;                              /* 0x40165 */
}


/* 0x40170 — record §P6.8. The D100 target at the dword 0xE86EE. With the other
 * side's slot set: the record's +0x28 bit 14 clears (else sets) the other
 * record's +0x29 bit 6; 0x34D8C(rec+0x51); 0x3C208(rec+0x51, word
 * 0xC759C[other char]). */
void fighter_40170(u32 rec)
{
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);   /* 0x40174..0x4017E */
    u32 orec;
    if (other == 0u) return;                                /* 0x40187 */
    orec = DSD(other);                                      /* 0x40199/0x401A1 */
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u)                 /* 0x40189..0x40197 */
        DSB(orec + 0x29u) &= 0xBFu;                         /* 0x4019B */
    else
        DSB(orec + 0x29u) |= 0x40u;                         /* 0x401A3 */
    (void)hit_flash_pair((u32)DSB(rec + 0x51u));             /* 0x401A7..0x401AC */
    (void)fighter_3c208((u32)DSB(rec + 0x51u),
                        (u32)(u16)DSW(P6_WORD_C759C
                                      + (u32)DSB(other + 0x7Au) * 2u));   /* 0x401B3..0x401CC */
}

```

- [ ] **Step 3: the prototypes, the wrappers, the forward declarations and the registrations.**

`port/src/game/fighter.h` (after the previous batch's prototypes):
```c
void fighter_241f4(u32 rec);
void fighter_47e04(u32 rec);
void fighter_40148(u32 rec);
void fighter_40170(u32 rec);
```

`port/src/game/actors.c` (after `anim_code_37B54`; each wrapper drops the operand):
```c
/* 0x2BDA0 — record §P6.3. The D100 target at the dword 0xE8B90: EAX = rec,
 * EDX = the operand (rng_next's range); the record's +0x53 = 1 when
 * rng_next((u16)arg) returns 0. */
static void anim_code_2BDA0(u32 rec, u32 arg)
{
    DSB(rec + 0x53u) = (u8)(rng_next(arg & 0xFFFFu) == 0u ? 1u : 0u);   /* 0x2BDA5/0x2BDA8, 0x2BDAF/0x2BDB2 */
}


/* 0x241F4 — the D100 target at the dword 0xE505E. anim_indirect passes EAX =
 * rec; the operand is not read. */
static void anim_code_241F4(u32 rec, u32 arg)
{
    (void)arg;
    fighter_241f4(rec);
}


/* 0x47E04 — the D000 target at the dword 0xED9FC (opcode 0x10). */
static void anim_code_47E04(u32 rec, u32 arg)
{
    (void)arg;
    fighter_47e04(rec);
}


/* 0x40148 — the D100 target at the dword 0xE8716. */
static void anim_code_40148(u32 rec, u32 arg)
{
    (void)arg;
    fighter_40148(rec);
}


/* 0x40170 — the D100 target at the dword 0xE86EE. */
static void anim_code_40170(u32 rec, u32 arg)
{
    (void)arg;
    fighter_40170(rec);
}

```

`port/src/game/actors.c` forward declarations (after `static void anim_code_45D58(u32 rec, u32 arg);`):
```c
static void anim_code_2BDA0(u32 rec, u32 arg);
static void anim_code_241F4(u32 rec, u32 arg);
static void anim_code_47E04(u32 rec, u32 arg);
static void anim_code_40148(u32 rec, u32 arg);
static void anim_code_40170(u32 rec, u32 arg);
```

`port/src/game/actors.c` registrations (after the previous task's last `fn_register`):
```c
    fn_register(0x2BDA0u, (void (*)(void))anim_code_2BDA0);
    fn_register(0x241F4u, (void (*)(void))anim_code_241F4);
    fn_register(0x47E04u, (void (*)(void))anim_code_47E04);
    fn_register(0x40148u, (void (*)(void))anim_code_40148);
    fn_register(0x40170u, (void (*)(void))anim_code_40170);
```

- [ ] **Step 4: the spec.**

`tools/diff_verify.py`: insert the P6 constants/helpers and `P6_SPECS` after the last `P3_SPECS += [...]` block, and change the `SPECS` list to `... + P3_SPECS + P6_SPECS`:

```python
# ---- track P batch 6: animation targets C (record 2026-10-03-reverse-p6) -----
# Every member is an animation target (EAX = rec, EDX = the stream opcode's operand; only 0x2BDA0
# reads the operand) except 0x24220, the slot +0x10 handler 0x24338 stores (EAX = slot, EDX = its
# record, EBX = side). The callees the batch stubs, args in the port's C order and clobbers from
# E.callee_clobbers (record §P6.2); 0x39F40 is the only `ret 4`.
P6_39280 = E.Call(0x39280, ("eax",))
P6_39F40 = E.Call(0x39F40, ("eax", "edx", "ebx", "ecx", "s0"), pop=4, clobbers=("ebx", "edx"))
P6_13244 = E.Call(0x13244, ())
P6_2A148 = E.Call(0x2A148, ("eax", "edx"), clobbers=("edx",))
P6_2BCF4 = E.Call(0x2BCF4, ("eax", "edx"), clobbers=("edx",))
P6_1890C = E.Call(0x1890C, ("eax", "edx"), clobbers=("edx",))
P6_RNG   = E.Call(0x5D7DC, ("eax",), mode="real")
P6_29BC8 = E.Call(0x29BC8, ("eax", "ebx", "edx"), clobbers=("ebx", "edx"))
P6_37D18 = E.Call(0x37D18, ("eax", "edx"), clobbers=("edx",))
P6_13C70 = E.Call(0x13C70, ("eax", "edx", "ebx"), clobbers=("ebx", "edx"))
P6_3AA54 = E.Call(0x3AA54, ("eax",))
P6_29C08 = E.Call(0x29C08, ("eax", "edx"), clobbers=("edx",))

# The slot-pointer table 0x1077A8 the functions that do not build the ctx read (it normally holds the
# slot array addresses; record §P6.2).
P6_PTRS = {DS_SLOTS - 8: le32(DS_SLOTS) + le32(DS_SLOTS + 0x94)}


P6_SPECS = [
    Spec("anim_2bda0", 0x2BDA0, [
        Case("r0", {"eax": E3_REC, "edx": 1}, {DS_RNG: le32(0), E3_REC + 0x53: b"\x53"}),
        Case("r1", {"eax": E3_REC, "edx": 0xFFFF}, {DS_RNG: le32(0), E3_REC + 0x53: b"\x53"}),
        Case("r2", {"eax": E3_REC, "edx": 0x12340001}, {DS_RNG: le32(0), E3_REC + 0x53: b"\x53"}),
        Case("r3", {"eax": E3_REC, "edx": 0}, {DS_RNG: le32(0x12345678), E3_REC + 0x53: b"\x53"}),
    ], calls=
    Spec("fighter_241f4", 0x241F4, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01"}),
    ], allow_calls=(0x339AC,), calls=
    Spec("fighter_47e04", 0x47E04, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01"}),
    ], allow_calls=(0x339AC,), calls=
    Spec("fighter_40148", 0x40148, [
        Case("o0", {"eax": E3_REC, "edx": 0x55}, {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0),
                                                   E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                   DS_104AE9: b"\xff\x5a\x5b"}),
        Case("o1", {"eax": E3_REC, "edx": 0x55}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                   E3_REC2 + 0x51: b"\x01", DS_104AE9: b"\xff\x5a\x5b"}),
        Case("o2", {"eax": E3_REC2, "edx": 0x55}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                    E3_REC2 + 0x51: b"\x01", DS_104AE9: b"\xff\x5a\x5b"}),
    ], calls=
    Spec("fighter_40170", 0x40170, [
        Case("p0", {"eax": E3_REC, "edx": 0x55}, {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0),
                                                   E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                   E3_REC + 0x28: b"\x00\x40"}),
        Case("p1", {"eax": E3_REC, "edx": 0x55}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                   E3_REC2 + 0x51: b"\x01", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                   E3_REC + 0x28: b"\x00\x40", E3_REC2 + 0x29: b"\x29",
                                                   0xC759C + 6: b"\x34\x12", 0xC759C + 8: b"\x78\x56"}),
        Case("p2", {"eax": E3_REC, "edx": 0x55}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                   E3_REC2 + 0x51: b"\x01", DS_SLOTS + 0x94 + 0x7A: b"\x04",
                                                   E3_REC + 0x28: b"\x00\x00", E3_REC2 + 0x29: b"\x00",
                                                   0xC759C + 8: b"\x78\x56"}),
        Case("p3", {"eax": E3_REC2, "edx": 0x55}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                    E3_REC2 + 0x51: b"\x01", DS_SLOTS + 0x7A: b"\x02",
                                                    E3_REC2 + 0x28: b"\x00\x40", E3_REC + 0x29: b"\x29",
                                                    0xC759C + 4: b"\xCD\xAB"}),
    ], calls=
]
```

- [ ] **Step 5: the bindings and mutants.** Insert before `static const binding_t k_bindings[] = {` in `port/tests/diff_runner.c`:

```c
static void b_2bda0(const u32 *r, u32 *eax)            { b_anim(0x2BDA0u, r, eax); }
static void b_22494(const u32 *r, u32 *eax)            { b_anim(0x22494u, r, eax); }
static void b_22a40(const u32 *r, u32 *eax)            { b_anim(0x22A40u, r, eax); }
static void b_23f10(const u32 *r, u32 *eax)            { b_anim(0x23F10u, r, eax); }
static void b_37dd4(const u32 *r, u32 *eax)            { b_anim(0x37DD4u, r, eax); }
static void b_22338(const u32 *r, u32 *eax)            { b_anim(0x22338u, r, eax); }
static void b_24220(const u32 *r, u32 *eax)            { fighter_24220(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_24220(const u32 *r, u32 *eax)            /* the countdown bound 1, not 0 */
{
    u32 slot = r[R_EAX], rec = r[R_EDX], side = r[R_EBX];
    u8 st = DSB(slot + 0x58u);
    if (st == 0u) {
        if ((s32)DSD(rec + 0x1Cu) >= 0x6400) {
            u32 ch = (u32)DSB(slot + 0x7Au);
            actors_anim_begin(rec, DSD(0x000A8510u + ch * 4u), 0x40000000u);
            hit_anchor_x(side, DSD(slot + 0x2Cu));
            DSW(rec + 0x2Cu) = 0x0200u;
            DSW(rec + 0x36u) = 0u;
            DSW(rec + 0x44u) = 1u;
            DSW(rec + 0x32u) = DSW(0x000A852Cu + (u32)DSW(DS_00104AFC) * 2u);
            DSB(slot + 0x41u) |= 0x20u;
            DSB(slot + 0x58u) = 1u;
            DSW(0x00104768u) = 0x0078u;
        }
        DSW(DSD(slot + 4u) + 0x2Cu) -= 0x80u;
        *eax = 0u;
        return;
    }
    if (st == 1u) {
        u16 t = (u16)(DSW(0x00104768u) - 1u);
        DSW(0x00104768u) = t;
        if ((s16)t <= 1) {
            (void)sound_voice(0x5Au);
            DSB(slot + 0x58u) = 2u;
        }
    }
    if (st == 1u || st == 2u) {
        if (DSD(rec + 0x1Cu) == 0u) {
            (void)actor_spawn((const u32 *)(mem + 0x000A853Cu), DSD(rec + 0x18u),
                              (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u);
            actors_anim_begin(rec, 0x000E87ACu, 0x3F800000u);
            (void)sound_voice(0x5Bu);
            DSB(DS_001078FC) = 1u;
            DSB(slot + 0x53u) = 3u;
            DSB(slot + 0x52u) = 9u;
            DSB(DS_000F0AFE) = 4u;
        }
        *eax = 0u;
        return;
    }
    *eax = 0u;
}
static void b_241f4(const u32 *r, u32 *eax)            { b_anim(0x241F4u, r, eax); }
static void b_47e04(const u32 *r, u32 *eax)            { b_anim(0x47E04u, r, eax); }
static void b_40148(const u32 *r, u32 *eax)            { b_anim(0x40148u, r, eax); }
static void b_40170(const u32 *r, u32 *eax)            { b_anim(0x40170u, r, eax); }
static void m_2bda0(const u32 *r, u32 *eax)            /* the range not masked before the rng call */
{
    u32 rec = r[R_EAX];
    DSB(rec + 0x53u) = (u8)(rng_next(r[R_EDX]) == 0u ? 1u : 0u);
    *eax = 0u;
}
static void b_47e04(const u32 *r, u32 *eax)            { b_anim(0x47E04u, r, eax); }
static void b_40148(const u32 *r, u32 *eax)            { b_anim(0x40148u, r, eax); }
static void b_40170(const u32 *r, u32 *eax)            { b_anim(0x40170u, r, eax); }
static void m_2bda0(const u32 *r, u32 *eax)            /* the range not masked before the rng call */
{
    u32 rec = r[R_EAX];
    DSB(rec + 0x53u) = (u8)(rng_next(r[R_EDX]) == 0u ? 1u : 0u);
    *eax = 0u;
}
static void b_40148(const u32 *r, u32 *eax)            { b_anim(0x40148u, r, eax); }
static void b_40170(const u32 *r, u32 *eax)            { b_anim(0x40170u, r, eax); }
static void m_2bda0(const u32 *r, u32 *eax)            /* the range not masked before the rng call */
{
    u32 rec = r[R_EAX];
    DSB(rec + 0x53u) = (u8)(rng_next(r[R_EDX]) == 0u ? 1u : 0u);
    *eax = 0u;
}
static void b_40170(const u32 *r, u32 *eax)            { b_anim(0x40170u, r, eax); }
static void m_2bda0(const u32 *r, u32 *eax)            /* the range not masked before the rng call */
{
    u32 rec = r[R_EAX];
    DSB(rec + 0x53u) = (u8)(rng_next(r[R_EDX]) == 0u ? 1u : 0u);
    *eax = 0u;
}
static void m_2bda0(const u32 *r, u32 *eax)            /* the range not masked before the rng call */
{
    u32 rec = r[R_EAX];
    DSB(rec + 0x53u) = (u8)(rng_next(r[R_EDX]) == 0u ? 1u : 0u);
    *eax = 0u;
}
static void m_2bda0_inv(const u32 *r, u32 *eax)        /* the test inverted */
{
    u32 rec = r[R_EAX];
    DSB(rec + 0x53u) = (u8)(rng_next(r[R_EDX] & 0xFFFFu) != 0u ? 1u : 0u);
    *eax = 0u;
}
static void m_241f4(const u32 *r, u32 *eax)            /* the stance value 0xB */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    fighter_3a95c(ctx[1], 0x0Bu);
    (void)sound_voice(0x66u);
    *eax = 0u;
}
static void m_241f4_side(const u32 *r, u32 *eax)       /* the stance side ctx[0], not ctx[1] */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    fighter_3a95c(ctx[0], 0x0Au);
    (void)sound_voice(0x66u);
    *eax = 0u;
}
static void m_241f4_voice(const u32 *r, u32 *eax)      /* the voice 0x67 */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    fighter_3a95c(ctx[1], 0x0Au);
    (void)sound_voice(0x67u);
    *eax = 0u;
}
static void m_47e04(const u32 *r, u32 *eax)            /* the stance value 0xE */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    (void)sound_voice(0x66u);
    fighter_3a95c(ctx[1], 0x0Eu);
    *eax = 0u;
}
static void m_47e04_side(const u32 *r, u32 *eax)       /* the stance side ctx[0], not ctx[1] */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    (void)sound_voice(0x66u);
    fighter_3a95c(ctx[0], 0x0Fu);
    *eax = 0u;
}
static void m_47e04_order(const u32 *r, u32 *eax)      /* stance before the voice */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    fighter_3a95c(ctx[1], 0x0Fu);
    (void)sound_voice(0x66u);
    *eax = 0u;
}
static void m_40148(const u32 *r, u32 *eax)            /* the other slot without the ^ 1 */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((u32)DSB(rec + 0x51u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    fighter_37d18(other, DSD(other));
    DSB(DS_00104AE9) &= 0xFBu;
    *eax = 0u;
}
static void m_40148_side(const u32 *r, u32 *eax)       /* 0x37D18 on the record's own side */
{
    u32 rec = r[R_EAX];
    u32 own = DSD(DS_001077A8 + ((u32)DSB(rec + 0x51u) & 0xFFu) * 4u);
    if (own == 0u) { *eax = 0u; return; }
    fighter_37d18(own, DSD(own));
    DSB(DS_00104AE9) &= 0xFBu;
    *eax = 0u;
}
static void m_40148_bit(const u32 *r, u32 *eax)        /* the wrong bit cleared */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    fighter_37d18(other, DSD(other));
    DSB(DS_00104AE9) &= 0xF7u;
    *eax = 0u;
}
static void m_40170(const u32 *r, u32 *eax)            /* the other record's +0x29 always set */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    u32 orec;
    if (other == 0u) { *eax = 0u; return; }
    orec = DSD(other);
    DSB(orec + 0x29u) |= 0x40u;
    (void)hit_flash_pair((u32)DSB(rec + 0x51u));
    (void)fighter_3c208((u32)DSB(rec + 0x51u),
                        (u32)(u16)DSW(0x000C759Cu + (u32)DSB(other + 0x7Au) * 2u));
    *eax = 0u;
}
static void m_40170_side(const u32 *r, u32 *eax)       /* flash/place on the other side */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    u32 orec, side;
    if (other == 0u) { *eax = 0u; return; }
    orec = DSD(other);
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u)
        DSB(orec + 0x29u) &= 0xBFu;
    else
        DSB(orec + 0x29u) |= 0x40u;
    side = (u32)DSB(rec + 0x51u) ^ 1u;
    (void)hit_flash_pair(side);
    (void)fighter_3c208(side, (u32)(u16)DSW(0x000C759Cu + (u32)DSB(other + 0x7Au) * 2u));
    *eax = 0u;
}
static void m_40170_set(const u32 *r, u32 *eax)        /* the clear as a set */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    u32 orec;
    if (other == 0u) { *eax = 0u; return; }
    orec = DSD(other);
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u)
        DSB(orec + 0x29u) |= 0x40u;
    else
        DSB(orec + 0x29u) |= 0x40u;
    (void)hit_flash_pair((u32)DSB(rec + 0x51u));
    (void)fighter_3c208((u32)DSB(rec + 0x51u),
                        (u32)(u16)DSW(0x000C759Cu + (u32)DSB(other + 0x7Au) * 2u));
    *eax = 0u;
}
static void m_40170_char(const u32 *r, u32 *eax)       /* the own slot's char, not the other's */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    u32 orec;
    if (other == 0u) { *eax = 0u; return; }
    orec = DSD(other);
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u)
        DSB(orec + 0x29u) &= 0xBFu;
    else
        DSB(orec + 0x29u) |= 0x40u;
    (void)hit_flash_pair((u32)DSB(rec + 0x51u));
    (void)fighter_3c208((u32)DSB(rec + 0x51u),
                        (u32)(u16)DSW(0x000C759Cu
                                      + (u32)DSB(DS_001077B0
                                                 + (u32)DSB(rec + 0x51u) * 0x94u + 0x7Au) * 2u));
    *eax = 0u;
}
```

and the rows into `k_bindings`:
```c
    { "anim_2bda0",                    0x00000000u },
    { "fighter_241f4",                 0x00000000u },
    { "fighter_47e04",                 0x00000000u },
    { "fighter_40148",                 0x00000000u },
    { "fighter_40170",                 0x00000000u },
    { "anim_2bda0@mutant",             0x00000000u },
    { "anim_2bda0@mask",               0x00000000u },
    { "fighter_241f4@mutant",          0x00000000u },
    { "fighter_241f4@side",            0x00000000u },
    { "fighter_241f4@voice",           0x00000000u },
    { "fighter_47e04@mutant",          0x00000000u },
    { "fighter_47e04@side",            0x00000000u },
    { "fighter_47e04@order",           0x00000000u },
    { "fighter_40148@mutant",          0x00000000u },
    { "fighter_40148@side",            0x00000000u },
    { "fighter_40148@bit",             0x00000000u },
    { "fighter_40170@mutant",          0x00000000u },
    { "fighter_40170@side",            0x00000000u },
    { "fighter_40170@set",             0x00000000u },
    { "fighter_40170@char",            0x00000000u },
```

- [ ] **Step 6: the unit checks.** Append to `port/tests/test_fight.c` and register in `test.h`:

```c
/* ---- track P batch 6 (record 2026-10-03-reverse-p6-derivations.md) ----------
 * The animation targets C. Differential verification (tools/diff_verify.py,
 * P6_SPECS) is the behavioural oracle; these checks pin what it does not see:
 * the registrations and the image dwords that make anim_indirect reach each
 * function, and one seeded run through each registration with sentinels on
 * every store. */
static void p6_check_simple(void)
{
    static const u32 addr[5] = { 0x2BDA0u, 0x241F4u, 0x47E04u, 0x40148u, 0x40170u };
    static const u32 dw[5] = { 0x000E8B90u, 0x000E505Eu, 0x000ED9FCu, 0x000E8716u, 0x000E86EEu };
    p1_anim_fn f;
    u32 k;

    for (k = 0; k < 5u; k++) {
        CHECK(fn_resolve(addr[k]) != NULL, "the animation target is registered");
        CHECK_EQ_INT((int)DSD(dw[k]), (int)addr[k]);
    }

    /* 0x2BDA0: rng_next(1) with the seed 0 is 0 (the +0x53 = 1); rng_next(0xFFFF)
     * is 0x38CD (0). The call's range is the masked operand: 0x12340001 -> 1. */
    f = (p1_anim_fn)(void *)fn_resolve(0x2BDA0u);
    if (f == NULL) return;
    z_fseed();
    DSD(DS_000EF6D8) = 0;
    DSB(Z_R0 + 0x53u) = 0x53u;
    f(Z_R0, 1u);
    CHECK_EQ_INT((int)DSB(Z_R0 + 0x53u), 1);
    z_fseed();
    DSD(DS_000EF6D8) = 0;
    DSB(Z_R0 + 0x53u) = 0x53u;
    f(Z_R0, 0xFFFFu);
    CHECK_EQ_INT((int)DSB(Z_R0 + 0x53u), 0);
    z_fseed();
    DSD(DS_000EF6D8) = 0;
    DSB(Z_R0 + 0x53u) = 0x53u;
    f(Z_R0, 0x12340001u);
    CHECK_EQ_INT((int)DSB(Z_R0 + 0x53u), 1);

    /* 0x241F4 with side 0's record: stance(ctx[1] = side 1, 0xA) writes slot 1's
     * +0x52/+0x53/+0x54/+0x10 and +0x7E = 0xBECF8 + 0xA; voice 0x66 is a no-op
     * here (the SDL path), so the slot 1 state is the observation. */
    f = (p1_anim_fn)(void *)fn_resolve(0x241F4u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSB(Z_S1 + 0x53u) = 0x33u;
    DSB(Z_S1 + 0x7Eu) = 0x77u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x7Eu), (int)(u8)(DSB(DS_000BECF8) + 0x0Au));
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 0x55);

    /* 0x47E04: voice first then stance(ctx[1] = side 1, 0xF). */
    f = (p1_anim_fn)(void *)fn_resolve(0x47E04u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSB(Z_S1 + 0x7Eu) = 0x77u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x7Eu), (int)(u8)(DSB(DS_000BECF8) + 0x0Fu));

    /* 0x40148: with the other slot (Z_S1) set, 0x37D18(Z_S1, Z_R1) sets its
     * +0x52..+0x54 and clears 0x104AE9 bit 2; without it nothing. */
    f = (p1_anim_fn)(void *)fn_resolve(0x40148u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSB(DS_00104AE9) = 0xFFu;
    DSB(Z_S1 + 0x52u) = 0x55u;
    DSD(DS_001077A8 + 4u) = 0u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x52u), 0x55);
    CHECK_EQ_INT((int)DSB(DS_00104AE9), 0xFF);
    DSD(DS_001077A8 + 4u) = Z_S1;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(DS_00104AE9), 0xFB);

    /* 0x40170: with the other slot set and the record's +0x28 bit 14, the other
     * record's +0x29 bit 6 clears (else sets) and 0x3C208 runs on rec+0x51 with
     * word 0xC759C[other char]. */
    f = (p1_anim_fn)(void *)fn_resolve(0x40170u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSB(Z_S1 + 0x7Au) = 3u;
    DSW(0x000C759Cu + 6u) = 0x1234u;
    DSW(DS_00104B00) = 0x22u;      /* the facing flag 0x18B16's early return keeps the store */
    DSW(Z_R0 + 0x28u) = 0x4000u;
    DSB(Z_R1 + 0x29u) = 0x69u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(Z_R1 + 0x29u), 0x29);
    DSW(Z_R0 + 0x28u) = 0x0000u;
    DSB(Z_R1 + 0x29u) = 0x00u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(Z_R1 + 0x29u), 0x40);
}
```

- [ ] **Step 7: the task gate.**

```bash
cmake --build build 2>&1 | grep -E 'error|warning'
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/diff_verify.py --image /tmp/p6_img.bin --self-check 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/p6_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/p6_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
```

Expected: no compiler message, `all checks passed`, and
```
diff-verify: 83/83 functions VERIFIED; 173/173 mutants detected; 1 named gaps; 12/69 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 283 unported, 212 ported; supplement 131 (9 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 26 in unported code, 89 in ported code, 19 nowhere
```

- [ ] **Step 8: the E2 table and the commit.**

Regenerate the E2 table (`make entry-triage` first fails against the committed one, then `entry_triage.py --out` writes it) and commit the named files with `<area>: <what changed>`; no `Co-Authored-By` trailer.

---


### Task 3: `0x22494 0x2400C 0x482E4` (seam `0x2BCF4`)

**Files:** modify `port/src/game/actors.c`, `port/src/game/fighter.c`, `port/src/game/fighter.h`, `tools/diff_verify.py`, `port/tests/diff_runner.c`, `port/tests/test_fight.c`, `port/tests/test.h`, the E2 table.

**Interfaces:** see the record §P6.2; the function signatures are in the code below.

- [ ] **Step 1: the seams.**

**`0x2BCF4` (actors.c).** `void actors_anim_seek(u32 rec, u32 stream)` opens with
`PR_SEAM(0x2BCF4u, rec, stream);`.

- [ ] **Step 2: the functions (append to the end of `port/src/game/fighter.c` after `fighter_40170`).**

```c
/* 0x22494 — record §P6.4. The D500 target at the dword 0xE4E1A. ctx; the own
 * record on 0xE4E1E at the side's 0x104738 frame; stance 0xA on the other
 * side; pose the own slot's +0x5F; voice 0x66. */
void fighter_22494(u32 rec)
{
    u32 ctx[6];
    hit_anim_ctx(ctx, rec);                                 /* 0x2249C */
    actors_anim_begin(ctx[4], P6_ANIM_22494,
                      DSD(P6_FRAME_104738 + ctx[0] * 4u));  /* 0x224A1..0x224B4 */
    fighter_3a95c(ctx[1], 0x0Au);                           /* 0x224B9..0x224C2 */
    fighter_39834(ctx[1], (u32)DSB(ctx[2] + 0x5Fu));        /* 0x224C7..0x224D8 */
    (void)sound_voice(0x66u);                               /* 0x224DD/0x224E2 */
}


/* 0x2400C — record §P6.5. The D100 target at the dword 0xE4FF4. The other
 * slot's record: its +0x1C -= word 0xA83F8[other char]; the record on
 * 0xA8408[other char] at 3.0; 0x104748's record +0x29 bit 3; 0x2BCF4(that
 * record, 0x741); voice 0x67. */
void fighter_2400c(u32 rec)
{
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);   /* 0x2400F..0x24019 */
    u32 orec, ch;
    if (other == 0u) return;                                /* 0x24022 */
    ch = (u32)DSB(other + 0x7Au);                           /* 0x24026 */
    orec = DSD(other);                                      /* 0x24029 */
    DSD(orec + 0x1Cu) -= (u32)(s32)(s16)DSW(P6_WORD_A83FA + ch * 2u);   /* 0x2402B..0x2403A */
    actors_anim_begin(orec, DSD(P6_STREAMS_A8408 + ch * 4u), 0x40400000u);  /* 0x2403D..0x24050 */
    DSB(DSD(P6_REC_104748) + 0x29u) |= 8u;                  /* 0x24055/0x2405A */
    actors_anim_seek(DSD(P6_REC_104748), 0x741u);           /* 0x2405E/0x24063 */
    (void)sound_voice(0x67u);                               /* 0x24068/0x2406D */
}


/* 0x482E4 — record §P6.10. The D500 target at the dword 0xED8A8. The record on
 * 0xED8BC at 3.0; when 0x108392[own side] is non-zero the other record on
 * 0xC8F40[other char] at 5.0, else stance 0xF and pose the own slot's +0x5F
 * on the other side. */
void fighter_482e4(u32 rec)
{
    u32 own = (u32)DSB(rec + 0x51u);                        /* 0x482F0 */
    u32 other_side = 1u - own;                              /* 0x482F4 */
    u32 oslot = DS_001077B0 + other_side * 0x94u;           /* 0x48304..0x48311 */
    actors_anim_begin(rec, P6_ANIM_482E4, 0x40400000u);     /* 0x4830C..0x4831A */
    if (DSB(P6_108392 + own) != 0u) {                     /* 0x4831F/0x48326 */
        u32 ch = (u32)DSB(oslot + 0x7Au);                   /* 0x4832A */
        actors_anim_begin(DSD(oslot), DSD(P6_STREAMS_C8F40 + ch * 4u),
                          0x40A00000u);                     /* 0x4832D..0x4833B */
    } else {
        fighter_3a95c(other_side, 0x0Fu);                   /* 0x48342..0x48349 */
        fighter_39834(other_side, (u32)DSB(DS_0010780F + own * 0x94u));  /* 0x4834E..0x48367 */
    }
}

```

- [ ] **Step 3: the prototypes, the wrappers, the forward declarations and the registrations.**

`port/src/game/fighter.h` (after the previous batch's prototypes):
```c
void fighter_22494(u32 rec);
void fighter_2400c(u32 rec);
void fighter_482e4(u32 rec);
```

`port/src/game/actors.c` (after `anim_code_37B54`; each wrapper drops the operand):
```c
/* 0x22494 — the D500 target at the dword 0xE4E1A. */
static void anim_code_22494(u32 rec, u32 arg)
{
    (void)arg;
    fighter_22494(rec);
}


/* 0x2400C — the D100 target at the dword 0xE4FF4. */
static void anim_code_2400C(u32 rec, u32 arg)
{
    (void)arg;
    fighter_2400c(rec);
}


/* 0x482E4 — the D500 target at the dword 0xED8A8. */
static void anim_code_482E4(u32 rec, u32 arg)
{
    (void)arg;
    fighter_482e4(rec);
}

```

`port/src/game/actors.c` forward declarations (after `static void anim_code_45D58(u32 rec, u32 arg);`):
```c
static void anim_code_22494(u32 rec, u32 arg);
static void anim_code_2400C(u32 rec, u32 arg);
static void anim_code_482E4(u32 rec, u32 arg);
```

`port/src/game/actors.c` registrations (after the previous task's last `fn_register`):
```c
    fn_register(0x22494u, (void (*)(void))anim_code_22494);
    fn_register(0x2400Cu, (void (*)(void))anim_code_2400C);
    fn_register(0x482E4u, (void (*)(void))anim_code_482E4);
```

- [ ] **Step 4: the spec.**

`tools/diff_verify.py`: append to `P6_SPECS` (before its closing `]`):
```python
    Spec("fighter_22494", 0x22494, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_104738: le32(0x40400000), DS_104738 + 4: le32(0x40A00000),
                                                     DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      DS_104738: le32(0x40400000), DS_104738 + 4: le32(0x40A00000),
                                                      DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f"}),
    ], allow_calls=(0x339AC,), calls=
    Spec("fighter_2400c", 0x2400C, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     E3_REC2 + 0x1C: le32(0x1000),
                                                     0xA83FA + 6: b"\xf0\xff", 0xA83FA + 4: b"\x11\x11",
                                                     0xA8408 + 12: le32(0x000ED111), 0xA8408 + 8: le32(0x000ED222),
                                                     DS_104748: le32(E3_OUT), E3_OUT + 0x29: b"\x29"}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x04",
                                                     E3_REC2 + 0x1C: le32(0x1000),
                                                     0xA83FA + 8: b"\x34\x12", 0xA83FA + 6: b"\x11\x11",
                                                     0xA8408 + 16: le32(0x000ED333), 0xA8408 + 12: le32(0x000ED222),
                                                     DS_104748: le32(E3_OUT), E3_OUT + 0x29: b"\x29"}),
        Case("s2", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                      DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                      E3_REC + 0x1C: le32(0x2000),
                                                      0xA83FA + 6: b"\xf0\xff", 0xA83FA + 4: b"\x11\x11",
                                                      0xA8408 + 12: le32(0x000ED111), 0xA8408 + 8: le32(0x000ED222),
                                                      DS_104748: le32(E3_OUT), E3_OUT + 0x29: b"\x29"}),
    ], calls=
    Spec("fighter_482e4", 0x482E4, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     0x108392: b"\x00\x5a",
                                                     DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
                                                     DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x7A: b"\x02",
                                                     0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     0x108392: b"\x01\x00",
                                                     DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
                                                     DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x7A: b"\x02",
                                                     0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
        Case("s2", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      0x108392: b"\x00\x00",
                                                      DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
                                                      DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x7A: b"\x02",
                                                      0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
        Case("s3", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      0x108392: b"\x00\x01",
                                                      DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
                                                      DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x7A: b"\x02",
                                                      0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
    ], calls=
```

- [ ] **Step 5: the bindings and mutants.** Insert before `static const binding_t k_bindings[] = {` in `port/tests/diff_runner.c`:

```c
static void b_22494(const u32 *r, u32 *eax)            { b_anim(0x22494u, r, eax); }
static void b_22a40(const u32 *r, u32 *eax)            { b_anim(0x22A40u, r, eax); }
static void b_23f10(const u32 *r, u32 *eax)            { b_anim(0x23F10u, r, eax); }
static void b_37dd4(const u32 *r, u32 *eax)            { b_anim(0x37DD4u, r, eax); }
static void b_22338(const u32 *r, u32 *eax)            { b_anim(0x22338u, r, eax); }
static void b_24220(const u32 *r, u32 *eax)            { fighter_24220(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_24220(const u32 *r, u32 *eax)            /* the countdown bound 1, not 0 */
{
    u32 slot = r[R_EAX], rec = r[R_EDX], side = r[R_EBX];
    u8 st = DSB(slot + 0x58u);
    if (st == 0u) {
        if ((s32)DSD(rec + 0x1Cu) >= 0x6400) {
            u32 ch = (u32)DSB(slot + 0x7Au);
            actors_anim_begin(rec, DSD(0x000A8510u + ch * 4u), 0x40000000u);
            hit_anchor_x(side, DSD(slot + 0x2Cu));
            DSW(rec + 0x2Cu) = 0x0200u;
            DSW(rec + 0x36u) = 0u;
            DSW(rec + 0x44u) = 1u;
            DSW(rec + 0x32u) = DSW(0x000A852Cu + (u32)DSW(DS_00104AFC) * 2u);
            DSB(slot + 0x41u) |= 0x20u;
            DSB(slot + 0x58u) = 1u;
            DSW(0x00104768u) = 0x0078u;
        }
        DSW(DSD(slot + 4u) + 0x2Cu) -= 0x80u;
        *eax = 0u;
        return;
    }
    if (st == 1u) {
        u16 t = (u16)(DSW(0x00104768u) - 1u);
        DSW(0x00104768u) = t;
        if ((s16)t <= 1) {
            (void)sound_voice(0x5Au);
            DSB(slot + 0x58u) = 2u;
        }
    }
    if (st == 1u || st == 2u) {
        if (DSD(rec + 0x1Cu) == 0u) {
            (void)actor_spawn((const u32 *)(mem + 0x000A853Cu), DSD(rec + 0x18u),
                              (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u);
            actors_anim_begin(rec, 0x000E87ACu, 0x3F800000u);
            (void)sound_voice(0x5Bu);
            DSB(DS_001078FC) = 1u;
            DSB(slot + 0x53u) = 3u;
            DSB(slot + 0x52u) = 9u;
            DSB(DS_000F0AFE) = 4u;
        }
        *eax = 0u;
        return;
    }
    *eax = 0u;
}
static void b_2400c(const u32 *r, u32 *eax)            { b_anim(0x2400Cu, r, eax); }
static void b_482e4(const u32 *r, u32 *eax)            { b_anim(0x482E4u, r, eax); }
static void m_22494(const u32 *r, u32 *eax)            /* the anim frame from the other side */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    actors_anim_begin(ctx[4], 0x000E4E1Eu, DSD(0x00104738u + (1u - ctx[0]) * 4u));
    fighter_3a95c(ctx[1], 0x0Au);
    fighter_39834(ctx[1], (u32)DSB(ctx[2] + 0x5Fu));
    (void)sound_voice(0x66u);
    *eax = 0u;
}
static void b_482e4(const u32 *r, u32 *eax)            { b_anim(0x482E4u, r, eax); }
static void m_22494(const u32 *r, u32 *eax)            /* the anim frame from the other side */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    actors_anim_begin(ctx[4], 0x000E4E1Eu, DSD(0x00104738u + (1u - ctx[0]) * 4u));
    fighter_3a95c(ctx[1], 0x0Au);
    fighter_39834(ctx[1], (u32)DSB(ctx[2] + 0x5Fu));
    (void)sound_voice(0x66u);
    *eax = 0u;
}
static void m_22494(const u32 *r, u32 *eax)            /* the anim frame from the other side */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    actors_anim_begin(ctx[4], 0x000E4E1Eu, DSD(0x00104738u + (1u - ctx[0]) * 4u));
    fighter_3a95c(ctx[1], 0x0Au);
    fighter_39834(ctx[1], (u32)DSB(ctx[2] + 0x5Fu));
    (void)sound_voice(0x66u);
    *eax = 0u;
}
static void m_22494_side(const u32 *r, u32 *eax)       /* the stance/pose on the own side */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    actors_anim_begin(ctx[4], 0x000E4E1Eu, DSD(0x00104738u + ctx[0] * 4u));
    fighter_3a95c(ctx[0], 0x0Au);
    fighter_39834(ctx[0], (u32)DSB(ctx[2] + 0x5Fu));
    (void)sound_voice(0x66u);
    *eax = 0u;
}
static void m_22494_pose(const u32 *r, u32 *eax)       /* the other slot's +0x5F */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    actors_anim_begin(ctx[4], 0x000E4E1Eu, DSD(0x00104738u + ctx[0] * 4u));
    fighter_3a95c(ctx[1], 0x0Au);
    fighter_39834(ctx[1], (u32)DSB(ctx[3] + 0x5Fu));
    (void)sound_voice(0x66u);
    *eax = 0u;
}
static void m_2400c(const u32 *r, u32 *eax)            /* the word 0xA83EA, not 0xA83F8 */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    u32 orec, ch;
    if (other == 0u) { *eax = 0u; return; }
    ch = (u32)DSB(other + 0x7Au);
    orec = DSD(other);
    DSD(orec + 0x1Cu) -= (u32)(s32)(s16)DSW(0x000A83EAu + ch * 2u);
    actors_anim_begin(orec, DSD(0x000A8408u + ch * 4u), 0x40400000u);
    DSB(DSD(0x00104748u) + 0x29u) |= 8u;
    actors_anim_seek(DSD(0x00104748u), 0x741u);
    (void)sound_voice(0x67u);
    *eax = 0u;
}
static void m_2400c_char(const u32 *r, u32 *eax)       /* the own slot's char */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    u32 orec, ch;
    if (other == 0u) { *eax = 0u; return; }
    ch = (u32)DSB(DS_001077B0 + ((u32)DSB(rec + 0x51u)) * 0x94u + 0x7Au);
    orec = DSD(other);
    DSD(orec + 0x1Cu) -= (u32)(s32)(s16)DSW(0x000A83F8u + ch * 2u);
    actors_anim_begin(orec, DSD(0x000A8408u + ch * 4u), 0x40400000u);
    DSB(DSD(0x00104748u) + 0x29u) |= 8u;
    actors_anim_seek(DSD(0x00104748u), 0x741u);
    (void)sound_voice(0x67u);
    *eax = 0u;
}
static void m_2400c_seek(const u32 *r, u32 *eax)       /* the seek 0x740 */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    u32 orec, ch;
    if (other == 0u) { *eax = 0u; return; }
    ch = (u32)DSB(other + 0x7Au);
    orec = DSD(other);
    DSD(orec + 0x1Cu) -= (u32)(s32)(s16)DSW(0x000A83F8u + ch * 2u);
    actors_anim_begin(orec, DSD(0x000A8408u + ch * 4u), 0x40400000u);
    DSB(DSD(0x00104748u) + 0x29u) |= 8u;
    actors_anim_seek(DSD(0x00104748u), 0x740u);
    (void)sound_voice(0x67u);
    *eax = 0u;
}
static void m_2400c_other(const u32 *r, u32 *eax)      /* the own slot's record */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    u32 orec, ch;
    if (other == 0u) { *eax = 0u; return; }
    ch = (u32)DSB(other + 0x7Au);
    orec = DSD(other);
    DSD(orec + 0x1Cu) -= (u32)(s32)(s16)DSW(0x000A83F8u + ch * 2u);
    actors_anim_begin(DSD(DS_001077B0 + ((u32)DSB(rec + 0x51u)) * 0x94u),
                      DSD(0x000A8408u + ch * 4u), 0x40400000u);
    DSB(DSD(0x00104748u) + 0x29u) |= 8u;
    actors_anim_seek(DSD(0x00104748u), 0x741u);
    (void)sound_voice(0x67u);
    *eax = 0u;
}
static void m_482e4(const u32 *r, u32 *eax)            /* the byte read from the other side */
{
    u32 rec = r[R_EAX];
    u32 own = (u32)DSB(rec + 0x51u);
    u32 other_side = 1u - own;
    u32 oslot = DS_001077B0 + other_side * 0x94u;
    actors_anim_begin(rec, 0x000ED8BCu, 0x40400000u);
    if (DSB(0x00108392u + other_side) != 0u) {
        u32 ch = (u32)DSB(oslot + 0x7Au);
        actors_anim_begin(DSD(oslot), DSD(0x000C8F40u + ch * 4u), 0x40A00000u);
    } else {
        fighter_3a95c(other_side, 0x0Fu);
        fighter_39834(other_side, (u32)DSB(0x0010780Fu + own * 0x94u));
    }
    *eax = 0u;
}
static void m_482e4_side(const u32 *r, u32 *eax)       /* stance/pose on the own side */
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
        fighter_3a95c(own, 0x0Fu);
        fighter_39834(own, (u32)DSB(0x0010780Fu + own * 0x94u));
    }
    *eax = 0u;
}
static void m_482e4_pose(const u32 *r, u32 *eax)       /* the other slot's +0x5F */
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
        fighter_3a95c(other_side, 0x0Fu);
        fighter_39834(other_side, (u32)DSB(oslot + 0x5Fu));
    }
    *eax = 0u;
}
```

and the rows into `k_bindings`:
```c
    { "fighter_22494",                 0x00000000u },
    { "fighter_2400c",                 0x00000000u },
    { "fighter_482e4",                 0x00000000u },
    { "fighter_22494@mutant",          0x00000000u },
    { "fighter_22494@side",            0x00000000u },
    { "fighter_22494@frame",           0x00000000u },
    { "fighter_22494@pose",            0x00000000u },
    { "fighter_2400c@mutant",          0x00000000u },
    { "fighter_2400c@word",            0x00000000u },
    { "fighter_2400c@char",            0x00000000u },
    { "fighter_2400c@seek",            0x00000000u },
    { "fighter_2400c@other",           0x00000000u },
    { "fighter_482e4@mutant",          0x00000000u },
    { "fighter_482e4@side",            0x00000000u },
    { "fighter_482e4@byte",            0x00000000u },
    { "fighter_482e4@pose",            0x00000000u },
```

- [ ] **Step 6: the unit checks.** Append to `port/tests/test_fight.c` and register in `test.h`:

```c
/* §P6.4/§P6.5/§P6.10: 0x22494, 0x2400C and 0x482E4 through their registrations. */
static void p6_check_22494(void)
{
    p1_anim_fn f;
    CHECK(fn_resolve(0x22494u) != NULL, "0x22494 is registered");
    CHECK(fn_resolve(0x2400Cu) != NULL, "0x2400C is registered");
    CHECK(fn_resolve(0x482E4u) != NULL, "0x482E4 is registered");
    CHECK_EQ_INT((int)DSD(0x000E4E1Au), 0x00022494);
    CHECK_EQ_INT((int)DSD(0x000E4FF4u), 0x0002400C);
    CHECK_EQ_INT((int)DSD(0x000ED8A8u), 0x000482E4);

    /* 0x22494 with side 0's record: the frame 0x104738[0] and the own slot's
     * +0x5F; stance(ctx[1] = 1, 0xA). */
    f = (p1_anim_fn)(void *)fn_resolve(0x22494u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSD(0x00104738u) = 0x40400000u;
    DSD(0x0010473Cu) = 0x40A00000u;
    DSB(Z_S0 + 0x5Fu) = 0x5Fu;
    DSB(Z_S1 + 0x5Fu) = 0x9Fu;
    DSB(Z_S1 + 0x7Eu) = 0x77u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x7Eu), (int)(u8)(DSB(DS_000BECF8) + 0x0Au));
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x5Fu), 0x5F);

    /* 0x2400C with the other slot set: the record on 0xA8408[3] (poked), its
     * +0x1C -= 0xA83F8[3] (-16), 0x104748's record +0x29 bit 3. */
    f = (p1_anim_fn)(void *)fn_resolve(0x2400Cu);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSB(Z_S1 + 0x7Au) = 3u;
    DSW(0x000A83FAu + 6u) = 0xFFF0u;
    DSD(Z_R1 + 0x1Cu) = 0x1000u;
    DSD(0x00104748u) = Z_R1 + 0x800u;
    DSB(Z_R1 + 0x829u) = 0x29u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSD(Z_R1 + 0x1Cu), 0x1010);
    CHECK_EQ_INT((int)DSB(Z_R1 + 0x829u), 0x29);

    /* 0x482E4: the record on 0xED8BC at 3.0; the byte 0x108392[side] 0 takes
     * stance(ctx[1] = 1, 0xF) and the pose of the own slot's +0x5F; 1 takes the
     * other record on 0xC8F40[3] (poked) at 5.0. */
    f = (p1_anim_fn)(void *)fn_resolve(0x482E4u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSB(Z_S1 + 0x5Fu) = 0x9Fu;
    DSB(0x00108392u) = 0u;
    DSB(0x00108393u) = 0x5Au;
    DSB(Z_S1 + 0x7Eu) = 0x77u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x7Eu), (int)(u8)(DSB(DS_000BECF8) + 0x0Fu));
    DSB(0x00108392u) = 1u;
    DSB(Z_S1 + 0x7Au) = 3u;
    DSD(Z_R1 + 8u) = 0x9999u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSD(Z_R1 + 8u), (int)DSD(0x000C8F40u + 12u));
}
```

- [ ] **Step 7: the task gate.**

```bash
cmake --build build 2>&1 | grep -E 'error|warning'
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/diff_verify.py --image /tmp/p6_img.bin --self-check 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/p6_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/p6_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
```

Expected: no compiler message, `all checks passed`, and
```
diff-verify: 86/86 functions VERIFIED; 186/186 mutants detected; 1 named gaps; 12/72 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 280 unported, 215 ported; supplement 131 (9 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 8: the E2 table and the commit.**

Regenerate the E2 table (`make entry-triage` first fails against the committed one, then `entry_triage.py --out` writes it) and commit the named files with `<area>: <what changed>`; no `Co-Authored-By` trailer.

---


### Task 4: `0x22A40 0x47E30` (seams `0x13244 0x3AA54`)

**Files:** modify `port/src/game/fighter.c`, `port/src/game/fighter.h`, `port/src/game/actors.c`, `tools/diff_verify.py`, `port/tests/diff_runner.c`, `port/tests/test_fight.c`, `port/tests/test.h`, the E2 table.

**Interfaces:** see the record §P6.3; the function signatures are in the code below.

- [ ] **Step 1: the seams.**

**`0x3AA54` (fighter.c).** Remove `static` from `fighter_3aa54`, open it with
`PR_SEAM_RET(0x3AA54u, slot);` and declare `u32 fighter_3aa54(u32 slot);` in `fighter.h`.
**`0x13244` (fighter.c).** Remove `static` from `fighter_13244`, open it with `PR_SEAM0(0x13244u);`
and declare `void fighter_13244(void);` in `fighter.h`.

- [ ] **Step 2: the functions (append to the end of `port/src/game/fighter.c` after `fighter_482e4`).**

```c
/* 0x22A40 — record §P6.5. The D100 target at the dword 0xE154A. With the
 * record's +0x14 slot set: spawn 0xBB31C at the record's x/y/+0x1C with a5 =
 * its +0x28 bit 14; the child into the slot's +8 and the side's 0x104730, its
 * +0x14 = the slot, its +0x59 = 2; the slot's +8 = 0; voice 0xA8; 0x13244. */
void fighter_22a40(u32 rec)
{
    u32 slot = DSD(rec + 0x14u);                            /* 0x22A47 */
    u32 child;
    if (slot == 0u) return;                                 /* 0x22A4A/0x22A4C */
    child = actor_spawn((const u32 *)(mem + P6_DESC_22A40),
                        DSD(rec + 0x18u),                   /* 0x22A6E */
                        (u32)((s32)DSD(rec + 0x30u) >> 16), /* 0x22A68/0x22A72 */
                        DSD(rec + 0x1Cu),                   /* 0x22A6B */
                        (u32)(DSW(rec + 0x28u) & 0x4000u)); /* 0x22A4E..0x22A71 */
    DSD(slot + 8u) = child;                                 /* 0x22A7F */
    DSB(child + 0x59u) = 2u;                                /* 0x22A82 */
    DSD(child + 0x14u) = slot;                              /* 0x22A8B */
    DSD(P6_SLOT_104730 + (u32)DSB(rec + 0x51u) * 4u) = child;   /* 0x22A8E..0x22A94 */
    DSD(slot + 8u) = 0u;                                    /* 0x22AA0 */
    (void)sound_voice(0xA8u);                               /* 0x22A9B/0x22AA7 */
    fighter_13244();                                        /* 0x22AAC */
}


/* 0x47E30 — record §P6.9. The D500 target at the dword 0xEDA26. ctx; the own
 * record on 0xEDA2A (0x108394[side] non-zero, then 0x3AA54(the other slot)) or
 * 0xED9FA, both at the side's 0x108378 frame. */
void fighter_47e30(u32 rec)
{
    u32 ctx[6];
    hit_anim_ctx(ctx, rec);                                 /* 0x47E39 */
    if (DSB(P6_108394 + ctx[0]) != 0u) {                    /* 0x47E44/0x47E4D */
        actors_anim_begin(ctx[4], P6_ANIM_47E30_A,
                          DSD(P6_FRAME_108378 + ctx[0] * 4u));   /* 0x47E51..0x47E60 */
        (void)fighter_3aa54(ctx[3]);                        /* 0x47E65/0x47E69 ([esp+0xC] after RET 4) */
    } else {
        actors_anim_begin(ctx[4], P6_ANIM_47E30_B,
                          DSD(P6_FRAME_108378 + ctx[0] * 4u));   /* 0x47E70..0x47E7F */
    }
}

```

- [ ] **Step 3: the prototypes, the wrappers, the forward declarations and the registrations.**

`port/src/game/fighter.h` (after the previous batch's prototypes):
```c
void fighter_22a40(u32 rec);
void fighter_47e30(u32 rec);
```

`port/src/game/actors.c` (after `anim_code_37B54`; each wrapper drops the operand):
```c
/* 0x22A40 — the D100 target at the dword 0xE154A. */
static void anim_code_22A40(u32 rec, u32 arg)
{
    (void)arg;
    fighter_22a40(rec);
}


/* 0x47E30 — the D500 target at the dword 0xEDA26. */
static void anim_code_47E30(u32 rec, u32 arg)
{
    (void)arg;
    fighter_47e30(rec);
}

```

`port/src/game/actors.c` forward declarations (after `static void anim_code_45D58(u32 rec, u32 arg);`):
```c
static void anim_code_22A40(u32 rec, u32 arg);
static void anim_code_47E30(u32 rec, u32 arg);
```

`port/src/game/actors.c` registrations (after the previous task's last `fn_register`):
```c
    fn_register(0x22A40u, (void (*)(void))anim_code_22A40);
    fn_register(0x47E30u, (void (*)(void))anim_code_47E30);
```

- [ ] **Step 4: the spec.**

`tools/diff_verify.py`: append to `P6_SPECS` (before its closing `]`):
```python
    Spec("fighter_22a40", 0x22A40, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x14: le32(0), E3_REC + 0x51: b"\x00",
                                                     E3_REC + 0x18: le32(0x1111), E3_REC + 0x1C: le32(0x2222),
                                                     E3_REC + 0x28: b"\x00\x00", E3_REC + 0x30: le32(0x33330000)}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x51: b"\x00",
                                                     E3_REC + 0x18: le32(0x1111), E3_REC + 0x1C: le32(0x2222),
                                                     E3_REC + 0x28: b"\x00\x40", E3_REC + 0x30: le32(0x33330000),
                                                     E3_SLOT + 8: le32(0x8888), P6_CHILD: b"\x99" * 0x20},
             {0x2AE14: P6_CHILD}),
        Case("s2", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x51: b"\x01",
                                                     E3_REC + 0x18: le32(0x1111), E3_REC + 0x1C: le32(0x2222),
                                                     E3_REC + 0x28: b"\x00\x00", E3_REC + 0x30: le32(0x33330000),
                                                     E3_SLOT + 8: le32(0x8888), P6_CHILD: b"\x99" * 0x20},
             {0x2AE14: P6_CHILD}),
        Case("s3", {"eax": E3_REC2, "edx": 0x1234}, {E3_REC2 + 0x14: le32(E3_SLOT), E3_REC2 + 0x51: b"\x01",
                                                      E3_REC2 + 0x18: le32(0x1111), E3_REC2 + 0x1C: le32(0x2222),
                                                      E3_REC2 + 0x28: b"\x00\x40", E3_REC2 + 0x30: le32(0x33330000),
                                                      E3_SLOT + 8: le32(0x8888), P6_CHILD: b"\x99" * 0x20},
             {0x2AE14: P6_CHILD}),
    ], calls=
    Spec("fighter_47e30", 0x47E30, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_108394: b"\x00\x5a",
                                                     DS_108378: le32(0x40400000), DS_108378 + 4: le32(0x40A00000)}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_108394: b"\x01\x00",
                                                     DS_108378: le32(0x40400000), DS_108378 + 4: le32(0x40A00000)}),
        Case("s2", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      DS_108394: b"\x00\x01",
                                                      DS_108378: le32(0x40400000), DS_108378 + 4: le32(0x40A00000)}),
    ], allow_calls=(0x339AC,), calls=
```

- [ ] **Step 5: the bindings and mutants.** Insert before `static const binding_t k_bindings[] = {` in `port/tests/diff_runner.c`:

```c
static void b_22a40(const u32 *r, u32 *eax)            { b_anim(0x22A40u, r, eax); }
static void b_23f10(const u32 *r, u32 *eax)            { b_anim(0x23F10u, r, eax); }
static void b_37dd4(const u32 *r, u32 *eax)            { b_anim(0x37DD4u, r, eax); }
static void b_22338(const u32 *r, u32 *eax)            { b_anim(0x22338u, r, eax); }
static void b_24220(const u32 *r, u32 *eax)            { fighter_24220(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_24220(const u32 *r, u32 *eax)            /* the countdown bound 1, not 0 */
{
    u32 slot = r[R_EAX], rec = r[R_EDX], side = r[R_EBX];
    u8 st = DSB(slot + 0x58u);
    if (st == 0u) {
        if ((s32)DSD(rec + 0x1Cu) >= 0x6400) {
            u32 ch = (u32)DSB(slot + 0x7Au);
            actors_anim_begin(rec, DSD(0x000A8510u + ch * 4u), 0x40000000u);
            hit_anchor_x(side, DSD(slot + 0x2Cu));
            DSW(rec + 0x2Cu) = 0x0200u;
            DSW(rec + 0x36u) = 0u;
            DSW(rec + 0x44u) = 1u;
            DSW(rec + 0x32u) = DSW(0x000A852Cu + (u32)DSW(DS_00104AFC) * 2u);
            DSB(slot + 0x41u) |= 0x20u;
            DSB(slot + 0x58u) = 1u;
            DSW(0x00104768u) = 0x0078u;
        }
        DSW(DSD(slot + 4u) + 0x2Cu) -= 0x80u;
        *eax = 0u;
        return;
    }
    if (st == 1u) {
        u16 t = (u16)(DSW(0x00104768u) - 1u);
        DSW(0x00104768u) = t;
        if ((s16)t <= 1) {
            (void)sound_voice(0x5Au);
            DSB(slot + 0x58u) = 2u;
        }
    }
    if (st == 1u || st == 2u) {
        if (DSD(rec + 0x1Cu) == 0u) {
            (void)actor_spawn((const u32 *)(mem + 0x000A853Cu), DSD(rec + 0x18u),
                              (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u);
            actors_anim_begin(rec, 0x000E87ACu, 0x3F800000u);
            (void)sound_voice(0x5Bu);
            DSB(DS_001078FC) = 1u;
            DSB(slot + 0x53u) = 3u;
            DSB(slot + 0x52u) = 9u;
            DSB(DS_000F0AFE) = 4u;
        }
        *eax = 0u;
        return;
    }
    *eax = 0u;
}
static void b_47e30(const u32 *r, u32 *eax)            { b_anim(0x47E30u, r, eax); }
static void m_22a40(const u32 *r, u32 *eax)            /* the a5 mask 0x40, not 0x4000 */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u32 child;
    if (slot == 0u) { *eax = 0u; return; }
    child = actor_spawn((const u32 *)(mem + 0x000BB31Cu), DSD(rec + 0x18u),
                        (u32)((s32)DSD(rec + 0x30u) >> 16), DSD(rec + 0x1Cu),
                        (u32)(DSW(rec + 0x28u) & 0x0040u));
    DSD(slot + 8u) = child;
    DSB(child + 0x59u) = 2u;
    DSD(child + 0x14u) = slot;
    DSD(0x00104730u + (u32)DSB(rec + 0x51u) * 4u) = child;
    DSD(slot + 8u) = 0u;
    (void)sound_voice(0xA8u);
    fighter_13244();
    *eax = 0u;
}
static void m_22a40(const u32 *r, u32 *eax)            /* the a5 mask 0x40, not 0x4000 */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u32 child;
    if (slot == 0u) { *eax = 0u; return; }
    child = actor_spawn((const u32 *)(mem + 0x000BB31Cu), DSD(rec + 0x18u),
                        (u32)((s32)DSD(rec + 0x30u) >> 16), DSD(rec + 0x1Cu),
                        (u32)(DSW(rec + 0x28u) & 0x0040u));
    DSD(slot + 8u) = child;
    DSB(child + 0x59u) = 2u;
    DSD(child + 0x14u) = slot;
    DSD(0x00104730u + (u32)DSB(rec + 0x51u) * 4u) = child;
    DSD(slot + 8u) = 0u;
    (void)sound_voice(0xA8u);
    fighter_13244();
    *eax = 0u;
}
static void m_22a40_side(const u32 *r, u32 *eax)       /* the 0x104730 index by the other side */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u32 child;
    if (slot == 0u) { *eax = 0u; return; }
    child = actor_spawn((const u32 *)(mem + 0x000BB31Cu), DSD(rec + 0x18u),
                        (u32)((s32)DSD(rec + 0x30u) >> 16), DSD(rec + 0x1Cu),
                        (u32)(DSW(rec + 0x28u) & 0x4000u));
    DSD(slot + 8u) = child;
    DSB(child + 0x59u) = 2u;
    DSD(child + 0x14u) = slot;
    DSD(0x00104730u + ((u32)DSB(rec + 0x51u) ^ 1u) * 4u) = child;
    DSD(slot + 8u) = 0u;
    (void)sound_voice(0xA8u);
    fighter_13244();
    *eax = 0u;
}
static void m_22a40_order(const u32 *r, u32 *eax)      /* the slot+8 clear after the voice */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u32 child;
    if (slot == 0u) { *eax = 0u; return; }
    child = actor_spawn((const u32 *)(mem + 0x000BB31Cu), DSD(rec + 0x18u),
                        (u32)((s32)DSD(rec + 0x30u) >> 16), DSD(rec + 0x1Cu),
                        (u32)(DSW(rec + 0x28u) & 0x4000u));
    DSD(slot + 8u) = child;
    DSB(child + 0x59u) = 2u;
    DSD(child + 0x14u) = slot;
    DSD(0x00104730u + (u32)DSB(rec + 0x51u) * 4u) = child;
    (void)sound_voice(0xA8u);
    DSD(slot + 8u) = 0u;
    fighter_13244();
    *eax = 0u;
}
static void m_47e30(const u32 *r, u32 *eax)            /* the branch byte from the other side */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    if (DSB(0x00108394u + (1u - ctx[0])) != 0u) {
        actors_anim_begin(ctx[4], 0x000EDA2Au, DSD(0x00108378u + ctx[0] * 4u));
        (void)fighter_3aa54(ctx[2]);
    } else {
        actors_anim_begin(ctx[4], 0x000ED9FAu, DSD(0x00108378u + ctx[0] * 4u));
    }
    *eax = 0u;
}
static void m_47e30_side(const u32 *r, u32 *eax)       /* 0x3AA54 on the own slot */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    if (DSB(0x00108394u + ctx[0]) != 0u) {
        actors_anim_begin(ctx[4], 0x000EDA2Au, DSD(0x00108378u + ctx[0] * 4u));
        (void)fighter_3aa54(ctx[2]);
    } else {
        actors_anim_begin(ctx[4], 0x000ED9FAu, DSD(0x00108378u + ctx[0] * 4u));
    }
    *eax = 0u;
}
```

and the rows into `k_bindings`:
```c
    { "fighter_22a40",                 0x00000000u },
    { "fighter_47e30",                 0x00000000u },
    { "fighter_22a40@mutant",          0x00000000u },
    { "fighter_22a40@side",            0x00000000u },
    { "fighter_22a40@a5",              0x00000000u },
    { "fighter_22a40@order",           0x00000000u },
    { "fighter_47e30@mutant",          0x00000000u },
    { "fighter_47e30@flag",            0x00000000u },
    { "fighter_47e30@side",            0x00000000u },
```

- [ ] **Step 6: the unit checks.** Append to `port/tests/test_fight.c` and register in `test.h`:

```c
/* §P6.5/§P6.9: 0x22A40 (registration and evidence only; its spawn needs the
 * actor pool) and 0x47E30 through its registration. */
static void p6_check_47e30(void)
{
    p1_anim_fn f;
    CHECK(fn_resolve(0x22A40u) != NULL, "0x22A40 is registered");
    CHECK(fn_resolve(0x47E30u) != NULL, "0x47E30 is registered");
    CHECK_EQ_INT((int)DSD(0x000E154Au), 0x00022A40);
    CHECK_EQ_INT((int)DSD(0x000EDA26u), 0x00047E30);

    /* 0x47E30: the byte 0x108394[side] 0 starts the own record on 0xED9FA; 1
     * on 0xEDA2A and then 0x3AA54(the own slot) sets its +0x52 = 0x11. */
    f = (p1_anim_fn)(void *)fn_resolve(0x47E30u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSB(0x00108394u) = 0u;
    DSB(0x00108395u) = 0x5Au;
    f(Z_R0, 0u);                                   /* the else branch: 0xED9FA (row-covered) */
    DSB(0x00108394u) = 1u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x52u), 0x11);    /* the if branch ran 0x3AA54 on ctx[3] */
}
```

- [ ] **Step 7: the task gate.**

```bash
cmake --build build 2>&1 | grep -E 'error|warning'
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/diff_verify.py --image /tmp/p6_img.bin --self-check 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/p6_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/p6_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
```

Expected: no compiler message, `all checks passed`, and
```
diff-verify: 88/88 functions VERIFIED; 193/193 mutants detected; 1 named gaps; 12/74 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 278 unported, 217 ported; supplement 131 (9 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 8: the E2 table and the commit.**

Regenerate the E2 table (`make entry-triage` first fails against the committed one, then `entry_triage.py --out` writes it) and commit the named files with `<area>: <what changed>`; no `Co-Authored-By` trailer.

---


### Task 5: `0x24338 0x3E160` (seams `0x1890C 0x29C08`)

**Files:** modify `port/src/game/fighter.c`, `port/src/game/fighter.h`, `port/src/game/actors.c`, `tools/diff_verify.py`, `port/tests/diff_runner.c`, `port/tests/test_fight.c`, `port/tests/test.h`, the E2 table.

**Interfaces:** see the record §P6.4; the function signatures are in the code below.

- [ ] **Step 1: the seams.**

**`0x1890C` (fighter.c).** `void hit_anchor_y(u32 side, u32 y)` opens with
`PR_SEAM(0x1890Cu, side, y);`.
**`0x29C08` (fighter.c).** Remove `static` from `fighter_29c08`, open it with
`PR_SEAM_RET(0x29C08u, side, ch);` and declare `u32 fighter_29c08(u32 side, u32 ch);` in `fighter.h`.

- [ ] **Step 2: the functions (append to the end of `port/src/game/fighter.c` after `fighter_47e30`).**

```c
/* 0x24338 — record §P6.6. The D100 target at the dword 0xE50B8. ctx; the other
 * record on 0xBED60[other char] at 4.0; 0x1890C(the other side, word
 * 0xBD884[other char]); the other slot +0x52/0x53/0x54 = 0x10/0xA/2, its +0x10
 * = 0x24220 and +0x58 = 0; the other record's +0x36 = 0x600; 0xF0AFE = 0 and
 * 0xF0AFF = the record's +0x51; voice 0x73; voice 0xBE008[other char]. */
void fighter_24338(u32 rec)
{
    u32 ctx[6];
    u32 ch;
    hit_anim_ctx(ctx, rec);                                 /* 0x24343 */
    ch = (u32)DSB(ctx[3] + 0x7Au);                          /* 0x2434E */
    actors_anim_begin(ctx[5], DSD(P6_STREAMS_BED60 + ch * 4u), 0x40800000u);  /* 0x24351..0x24361 */
    ch = (u32)DSB(ctx[3] + 0x7Au);                          /* 0x2436D */
    hit_anchor_y(ctx[1], (u32)(s32)(s16)DSW(P6_WORD_BD884 + ch * 2u));   /* 0x24372..0x24380 */
    DSB(ctx[3] + 0x52u) = 0x10u;                            /* 0x24389 */
    DSB(ctx[3] + 0x53u) = 0x0Au;                            /* 0x24391 */
    DSB(ctx[3] + 0x54u) = 2u;                               /* 0x24399 */
    DSD(ctx[3] + 0x10u) = 0x00024220u;                      /* 0x243A1 */
    DSB(ctx[3] + 0x58u) = 0u;                               /* 0x243AC */
    DSW(ctx[5] + 0x36u) = 0x0600u;                          /* 0x243B4 */
    DSB(DS_000F0AFE) = 0u;                                  /* 0x243BA/0x243BC */
    DSB(DS_000F0AFF) = DSB(rec + 0x51u);                    /* 0x243C2/0x243C5 */
    (void)sound_voice(0x73u);                               /* 0x243CA/0x243CF */
    ch = (u32)DSB(ctx[3] + 0x7Au);                          /* 0x243DB */
    (void)sound_voice((u32)(u16)DSW(P6_VOICE_BE008 + ch * 2u));   /* 0x243E0..0x243ED */
}


/* 0x3E160 — record §P6.7. The D100 target at the dword 0xE86E0. 0x29C08
 * (rec+0x51, the side's char) into the side's 0xC7614 descriptor's +0x10, then
 * spawn that descriptor at 0/0/0 with a5 = the record's +0x56 | 0x400; the
 * record's +0x4B = the child's +0x56 and the child's +0x60 = 1. */
void fighter_3e160(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x3E168 */
    u32 pal = fighter_29c08(side, (u32)DSB(DS_0010782A + side * 0x94u));  /* 0x3E17D/0x3E186 */
    u32 child;
    DSD(DSD(P6_DESC_C7614 + side * 4u) + 0x10u) = pal;      /* 0x3E18B..0x3E197 */
    child = actor_spawn((const u32 *)(mem + DSD(P6_DESC_C7614 + side * 4u)),
                        0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));  /* 0x3E19A..0x3E1B9 */
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);                  /* 0x3E1BE/0x3E1C1 */
    DSB(child + 0x60u) = 1u;                                /* 0x3E1C4 */
}

```

- [ ] **Step 3: the prototypes, the wrappers, the forward declarations and the registrations.**

`port/src/game/fighter.h` (after the previous batch's prototypes):
```c
void fighter_24338(u32 rec);
void fighter_3e160(u32 rec);
```

`port/src/game/actors.c` (after `anim_code_37B54`; each wrapper drops the operand):
```c
/* 0x24338 — the D100 target at the dword 0xE50B8. */
static void anim_code_24338(u32 rec, u32 arg)
{
    (void)arg;
    fighter_24338(rec);
}


/* 0x3E160 — the D100 target at the dword 0xE86E0. */
static void anim_code_3E160(u32 rec, u32 arg)
{
    (void)arg;
    fighter_3e160(rec);
}

```

`port/src/game/actors.c` forward declarations (after `static void anim_code_45D58(u32 rec, u32 arg);`):
```c
static void anim_code_24338(u32 rec, u32 arg);
static void anim_code_3E160(u32 rec, u32 arg);
```

`port/src/game/actors.c` registrations (after the previous task's last `fn_register`):
```c
    fn_register(0x24338u, (void (*)(void))anim_code_24338);
    fn_register(0x3E160u, (void (*)(void))anim_code_3E160);
```

- [ ] **Step 4: the spec.**

`tools/diff_verify.py`: append to `P6_SPECS` (before its closing `]`):
```python
    Spec("fighter_24338", 0x24338, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     0xBED60 + 8: le32(0x000ED222) + le32(0x000ED111),
                                                     0xBD884 + 4: b"\x11\x11\x00\x20",
                                                     0xBE008 + 4: b"\x66\x00\x77\x00",
                                                     E3_REC2 + 0x10: _p6_fill(0x40, {0x00: le32(0x10101010), 0x19: b"\x29", 0x26: b"\x36\x36"}),
                                                     E3_REC2 + 0x50: b"\xa5\xa5\xa5\xa5\xa5\xa5\xa5\xa5\x58",
                                                     0xF0AFE: b"\xfe\xff"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x04",
                                                      0xBED60 + 8: le32(0x000ED222) + le32(0x000ED333),
                                                      0xBD884 + 6: b"\x11\x11\x34\x12",
                                                      0xBE008 + 6: b"\x66\x00\x55\x00",
                                                      E3_REC + 0x10: _p6_fill(0x40, {0x00: le32(0x10101010), 0x19: b"\x29", 0x26: b"\x36\x36"}),
                                                      E3_REC + 0x50: b"\xa5\xa5\xa5\xa5\xa5\xa5\xa5\xa5\x58",
                                                      0xF0AFE: b"\xfe\xff"}),
    ], allow_calls=(0x339AC,), calls=
    Spec("fighter_3e160", 0x3E160, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x51: b"\x00", DS_SLOTS + 0x7A: b"\x02",
                                                     E3_REC + 0x56: b"\x00\x11",
                                                     P6_CHILD + 0x56: b"\x77", P6_CHILD + 0x60: b"\x00",
                                                     0xBB420 + 0x10: le32(0x10101010),
                                                     0xBB420 + 0x14: le32(0x14141414)},
             {0x2AE14: P6_CHILD}),
        Case("s1", {"eax": E3_REC2, "edx": 0x1234}, {E3_REC2 + 0x51: b"\x01", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                      E3_REC2 + 0x56: b"\x00\x22",
                                                      P6_CHILD + 0x56: b"\x77", P6_CHILD + 0x60: b"\x00",
                                                      0xBB434 + 0x10: le32(0x20202020),
                                                      0xBB434 + 0x14: le32(0x24242424)},
             {0x2AE14: P6_CHILD}),
    ], calls=
```

- [ ] **Step 5: the bindings and mutants.** Insert before `static const binding_t k_bindings[] = {` in `port/tests/diff_runner.c`:

```c
static void b_24338(const u32 *r, u32 *eax)            { b_anim(0x24338u, r, eax); }
static void b_45c98(const u32 *r, u32 *eax)            { b_anim(0x45C98u, r, eax); }
static void m_23f10(const u32 *r, u32 *eax)            /* the word 0xA83F8, not 0xA83EA */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    u32 orec, ch, child, a5;
    if (other == 0u) { *eax = 0u; return; }
    DSB(other + 0x41u) |= 0x20u;
    ch = (u32)DSB(other + 0x7Au);
    orec = DSD(other);
    DSD(orec + 0x1Cu) -= (u32)(s32)(s16)DSW(0x000A83FAu + ch * 2u);
    actor_pset_flag_5f(orec, 0u);
    actors_anim_begin(orec, DSD(0x000A8408u + ch * 4u), 0x40400000u);
    child = actor_spawn((const u32 *)(mem + 0x000A84CCu), DSD(orec + 0x18u),
                        (u32)((s32)DSD(orec + 0x30u) >> 16), 0u, 0u);
    DSB(child + 0x59u) = 2u;
    DSD(0x00104748u) = child;
    a5 = (u32)(u16)(DSW(child + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0u, 0u, 0xFu, a5);
    a5 = (u32)(u16)(DSW(DSD(0x00104748u) + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0x14u, 0u, 0xFu, a5);
    a5 = (u32)(u16)(DSW(DSD(0x00104748u) + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0xFFFFFFECu, 0u, 0xFu, a5);
    (void)sound_voice(0x64u);
    *eax = 0u;
}
static void b_3e160(const u32 *r, u32 *eax)            { b_anim(0x3E160u, r, eax); }
static void m_24338(const u32 *r, u32 *eax)            /* the stream table by the own char */
{
    u32 ctx[6];
    u32 ch;
    hit_anim_ctx(ctx, r[R_EAX]);
    ch = (u32)DSB(DS_001077B0 + ((u32)DSB(r[R_EAX] + 0x51u)) * 0x94u + 0x7Au);
    actors_anim_begin(ctx[5], DSD(0x000BED60u + ch * 4u), 0x40800000u);
    ch = (u32)DSB(ctx[3] + 0x7Au);
    hit_anchor_y(ctx[1], (u32)(s32)(s16)DSW(0x000BD884u + ch * 2u));
    DSB(ctx[3] + 0x52u) = 0x10u;
    DSB(ctx[3] + 0x53u) = 0x0Au;
    DSB(ctx[3] + 0x54u) = 2u;
    DSD(ctx[3] + 0x10u) = 0x00024220u;
    DSB(ctx[3] + 0x58u) = 0u;
    DSW(ctx[5] + 0x36u) = 0x0600u;
    DSB(DS_000F0AFE) = 0u;
    DSB(DS_000F0AFF) = DSB(r[R_EAX] + 0x51u);
    (void)sound_voice(0x73u);
    (void)sound_voice((u32)(u16)DSW(0x000BE008u + ch * 2u));
    *eax = 0u;
}
static void m_24338(const u32 *r, u32 *eax)            /* the stream table by the own char */
{
    u32 ctx[6];
    u32 ch;
    hit_anim_ctx(ctx, r[R_EAX]);
    ch = (u32)DSB(DS_001077B0 + ((u32)DSB(r[R_EAX] + 0x51u)) * 0x94u + 0x7Au);
    actors_anim_begin(ctx[5], DSD(0x000BED60u + ch * 4u), 0x40800000u);
    ch = (u32)DSB(ctx[3] + 0x7Au);
    hit_anchor_y(ctx[1], (u32)(s32)(s16)DSW(0x000BD884u + ch * 2u));
    DSB(ctx[3] + 0x52u) = 0x10u;
    DSB(ctx[3] + 0x53u) = 0x0Au;
    DSB(ctx[3] + 0x54u) = 2u;
    DSD(ctx[3] + 0x10u) = 0x00024220u;
    DSB(ctx[3] + 0x58u) = 0u;
    DSW(ctx[5] + 0x36u) = 0x0600u;
    DSB(DS_000F0AFE) = 0u;
    DSB(DS_000F0AFF) = DSB(r[R_EAX] + 0x51u);
    (void)sound_voice(0x73u);
    (void)sound_voice((u32)(u16)DSW(0x000BE008u + ch * 2u));
    *eax = 0u;
}
static void m_24338_other(const u32 *r, u32 *eax)      /* the anim on the own record */
{
    u32 ctx[6];
    u32 ch;
    hit_anim_ctx(ctx, r[R_EAX]);
    ch = (u32)DSB(ctx[3] + 0x7Au);
    actors_anim_begin(ctx[4], DSD(0x000BED60u + ch * 4u), 0x40800000u);
    hit_anchor_y(ctx[1], (u32)(s32)(s16)DSW(0x000BD884u + ch * 2u));
    DSB(ctx[3] + 0x52u) = 0x10u;
    DSB(ctx[3] + 0x53u) = 0x0Au;
    DSB(ctx[3] + 0x54u) = 2u;
    DSD(ctx[3] + 0x10u) = 0x00024220u;
    DSB(ctx[3] + 0x58u) = 0u;
    DSW(ctx[5] + 0x36u) = 0x0600u;
    DSB(DS_000F0AFE) = 0u;
    DSB(DS_000F0AFF) = DSB(r[R_EAX] + 0x51u);
    (void)sound_voice(0x73u);
    (void)sound_voice((u32)(u16)DSW(0x000BE008u + ch * 2u));
    *eax = 0u;
}
static void m_24338_slot(const u32 *r, u32 *eax)       /* the +0x10 handler in the own slot */
{
    u32 ctx[6];
    u32 ch;
    hit_anim_ctx(ctx, r[R_EAX]);
    ch = (u32)DSB(ctx[3] + 0x7Au);
    actors_anim_begin(ctx[5], DSD(0x000BED60u + ch * 4u), 0x40800000u);
    hit_anchor_y(ctx[1], (u32)(s32)(s16)DSW(0x000BD884u + ch * 2u));
    DSB(ctx[3] + 0x52u) = 0x10u;
    DSB(ctx[3] + 0x53u) = 0x0Au;
    DSB(ctx[3] + 0x54u) = 2u;
    DSD(ctx[2] + 0x10u) = 0x00024220u;
    DSB(ctx[3] + 0x58u) = 0u;
    DSW(ctx[5] + 0x36u) = 0x0600u;
    DSB(DS_000F0AFE) = 0u;
    DSB(DS_000F0AFF) = DSB(r[R_EAX] + 0x51u);
    (void)sound_voice(0x73u);
    (void)sound_voice((u32)(u16)DSW(0x000BE008u + ch * 2u));
    *eax = 0u;
}
static void m_24338_af(const u32 *r, u32 *eax)         /* 0xF0AFF from the other side */
{
    u32 ctx[6];
    u32 ch;
    hit_anim_ctx(ctx, r[R_EAX]);
    ch = (u32)DSB(ctx[3] + 0x7Au);
    actors_anim_begin(ctx[5], DSD(0x000BED60u + ch * 4u), 0x40800000u);
    hit_anchor_y(ctx[1], (u32)(s32)(s16)DSW(0x000BD884u + ch * 2u));
    DSB(ctx[3] + 0x52u) = 0x10u;
    DSB(ctx[3] + 0x53u) = 0x0Au;
    DSB(ctx[3] + 0x54u) = 2u;
    DSD(ctx[3] + 0x10u) = 0x00024220u;
    DSB(ctx[3] + 0x58u) = 0u;
    DSW(ctx[5] + 0x36u) = 0x0600u;
    DSB(DS_000F0AFE) = 0u;
    DSB(DS_000F0AFF) = (u8)(DSB(r[R_EAX] + 0x51u) ^ 1u);
    (void)sound_voice(0x73u);
    (void)sound_voice((u32)(u16)DSW(0x000BE008u + ch * 2u));
    *eax = 0u;
}
static void m_24338_voice(const u32 *r, u32 *eax)      /* the second voice from the own char */
{
    u32 ctx[6];
    u32 ch;
    hit_anim_ctx(ctx, r[R_EAX]);
    ch = (u32)DSB(ctx[3] + 0x7Au);
    actors_anim_begin(ctx[5], DSD(0x000BED60u + ch * 4u), 0x40800000u);
    hit_anchor_y(ctx[1], (u32)(s32)(s16)DSW(0x000BD884u + ch * 2u));
    DSB(ctx[3] + 0x52u) = 0x10u;
    DSB(ctx[3] + 0x53u) = 0x0Au;
    DSB(ctx[3] + 0x54u) = 2u;
    DSD(ctx[3] + 0x10u) = 0x00024220u;
    DSB(ctx[3] + 0x58u) = 0u;
    DSW(ctx[5] + 0x36u) = 0x0600u;
    DSB(DS_000F0AFE) = 0u;
    DSB(DS_000F0AFF) = DSB(r[R_EAX] + 0x51u);
    (void)sound_voice(0x73u);
    (void)sound_voice((u32)(u16)DSW(0x000BE008u
                                    + (u32)DSB(DS_001077B0
                                               + ((u32)DSB(r[R_EAX] + 0x51u)) * 0x94u + 0x7Au) * 2u));
    *eax = 0u;
}
static void m_3e160(const u32 *r, u32 *eax)            /* the descriptor of the other side */
{
    u32 rec = r[R_EAX];
    u32 side = (u32)DSB(rec + 0x51u);
    u32 pal = fighter_29c08(side, (u32)DSB(DS_0010782A + side * 0x94u));
    u32 child;
    DSD(DSD(0x000C7614u + (side ^ 1u) * 4u) + 0x10u) = pal;
    child = actor_spawn((const u32 *)(mem + DSD(0x000C7614u + side * 4u)), 0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    DSB(child + 0x60u) = 1u;
    *eax = 0u;
}
static void m_3e160_side(const u32 *r, u32 *eax)       /* 0x29C08 on the other side */
{
    u32 rec = r[R_EAX];
    u32 side = (u32)DSB(rec + 0x51u);
    u32 pal = fighter_29c08(side ^ 1u, (u32)DSB(DS_0010782A + side * 0x94u));
    u32 child;
    DSD(DSD(0x000C7614u + side * 4u) + 0x10u) = pal;
    child = actor_spawn((const u32 *)(mem + DSD(0x000C7614u + side * 4u)), 0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    DSB(child + 0x60u) = 1u;
    *eax = 0u;
}
static void m_3e160_a5(const u32 *r, u32 *eax)         /* a5 without the 0x400 */
{
    u32 rec = r[R_EAX];
    u32 side = (u32)DSB(rec + 0x51u);
    u32 pal = fighter_29c08(side, (u32)DSB(DS_0010782A + side * 0x94u));
    u32 child;
    DSD(DSD(0x000C7614u + side * 4u) + 0x10u) = pal;
    child = actor_spawn((const u32 *)(mem + DSD(0x000C7614u + side * 4u)), 0u, 0u, 0u,
                        (u32)(u16)DSW(rec + 0x56u));
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    DSB(child + 0x60u) = 1u;
    *eax = 0u;
}
static void m_3e160_child(const u32 *r, u32 *eax)      /* the +0x4B from the record's own +0x56 */
{
    u32 rec = r[R_EAX];
    u32 side = (u32)DSB(rec + 0x51u);
    u32 pal = fighter_29c08(side, (u32)DSB(DS_0010782A + side * 0x94u));
    u32 child;
    DSD(DSD(0x000C7614u + side * 4u) + 0x10u) = pal;
    child = actor_spawn((const u32 *)(mem + DSD(0x000C7614u + side * 4u)), 0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(rec + 0x4Bu) = DSB(rec + 0x56u);
    DSB(child + 0x60u) = 1u;
    *eax = 0u;
}
```

and the rows into `k_bindings`:
```c
    { "fighter_24338",                 0x00000000u },
    { "fighter_3e160",                 0x00000000u },
    { "fighter_24338@mutant",          0x00000000u },
    { "fighter_24338@other",           0x00000000u },
    { "fighter_24338@slot",            0x00000000u },
    { "fighter_24338@af",              0x00000000u },
    { "fighter_24338@voice",           0x00000000u },
    { "fighter_3e160@mutant",          0x00000000u },
    { "fighter_3e160@side",            0x00000000u },
    { "fighter_3e160@a5",              0x00000000u },
    { "fighter_3e160@child",           0x00000000u },
```

- [ ] **Step 6: the unit checks.** Append to `port/tests/test_fight.c` and register in `test.h`:

```c
/* §P6.6/§P6.7: 0x24338 and 0x3E160 (registration and evidence only; 0x3E160's
 * spawn needs the actor pool) through their registrations. */
static void p6_check_24338(void)
{
    p1_anim_fn f;
    CHECK(fn_resolve(0x24338u) != NULL, "0x24338 is registered");
    CHECK(fn_resolve(0x3E160u) != NULL, "0x3E160 is registered");
    CHECK_EQ_INT((int)DSD(0x000E50B8u), 0x00024338);
    CHECK_EQ_INT((int)DSD(0x000E86E0u), 0x0003E160);

    /* 0x24338 with side 0's record: the other slot's +0x52/0x53/0x54/+0x10/
     * +0x58, the other record's +0x36, 0xF0AFE/0xF0AFF. */
    f = (p1_anim_fn)(void *)fn_resolve(0x24338u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSB(Z_S1 + 0x7Au) = 3u;
    DSB(Z_S1 + 0x52u) = 0x55u;
    DSB(Z_S1 + 0x58u) = 0x58u;
    DSD(Z_S1 + 0x10u) = 0x10101010u;
    DSW(Z_R1 + 0x36u) = 0x3636u;
    DSB(DS_000F0AFE) = 0xFEu;
    DSB(DS_000F0AFF) = 0xFFu;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x52u), 0x10);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x54u), 2);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x10u), 0x00024220);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x58u), 0);
    CHECK_EQ_INT((int)DSW(Z_R1 + 0x36u), 0x0600);
    CHECK_EQ_INT((int)DSB(DS_000F0AFE), 0);
    CHECK_EQ_INT((int)DSB(DS_000F0AFF), 0);
}
```

- [ ] **Step 7: the task gate.**

```bash
cmake --build build 2>&1 | grep -E 'error|warning'
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/diff_verify.py --image /tmp/p6_img.bin --self-check 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/p6_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/p6_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
```

Expected: no compiler message, `all checks passed`, and
```
diff-verify: 90/90 functions VERIFIED; 202/202 mutants detected; 1 named gaps; 12/76 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 276 unported, 219 ported; supplement 131 (9 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 8: the E2 table and the commit.**

Regenerate the E2 table (`make entry-triage` first fails against the committed one, then `entry_triage.py --out` writes it) and commit the named files with `<area>: <what changed>`; no `Co-Authored-By` trailer.

---


### Task 6: `0x23F10 0x45C98` (seams `0x2A148 0x13C70`)

**Files:** modify `port/src/game/actors.c`, `port/src/game/effects.c`, `port/src/game/fighter.c`, `port/src/game/fighter.h`, `tools/diff_verify.py`, `port/tests/diff_runner.c`, `port/tests/test_fight.c`, `port/tests/test.h`, the E2 table.

**Interfaces:** see the record §P6.5; the function signatures are in the code below.

- [ ] **Step 1: the seams.**

**`0x2A148` (actors.c).** `void actor_pset_flag_5f(u32 rec, u8 flag)` opens with
`PR_SEAM(0x2A148u, rec, flag);`.
**`0x13C70` (effects.c).** `u32 effects_spawn(u32 source_rec, u32 byte_arg, u32 handle)` opens with
`PR_SEAM_RET(0x13C70u, source_rec, byte_arg, handle);`.

- [ ] **Step 2: the functions (append to the end of `port/src/game/fighter.c` after `fighter_3e160`).**

```c
/* 0x23F10 — record §P6.5. The D100 target at the dword 0xE4FE0. The other
 * slot: its +0x41 bit 5; its record's +0x1C -= word 0xA83EC[other char];
 * 0x2A148(that record, 0); the record on 0xA8408[other char] at 3.0; a 0xA84CC
 * child at the record's x/y; its +0x59 = 2 into 0x104748; three more 0xA84CC
 * children at x 0/+0x14/-0x14 with a4 = 0xF and a5 = the first child's +0x56 |
 * 0x400; voice 0x64. */
void fighter_23f10(u32 rec)
{
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);  /* 0x23F13..0x23F1D */
    u32 orec, ch, child, a5;
    if (other == 0u) return;                                /* 0x23F26 */
    DSB(other + 0x41u) |= 0x20u;                            /* 0x23F2C..0x23F37 */
    ch = (u32)DSB(other + 0x7Au);                           /* 0x23F34 */
    orec = DSD(other);                                      /* 0x23F41 */
    DSD(orec + 0x1Cu) -= (u32)(s32)(s16)DSW(P6_WORD_A83EC + ch * 2u);   /* 0x23F3A..0x23F46 */
    actor_pset_flag_5f(orec, 0u);                           /* 0x23F49..0x23F4D */
    actors_anim_begin(orec, DSD(P6_STREAMS_A8408 + ch * 4u), 0x40400000u);  /* 0x23F52..0x23F65 */
    child = actor_spawn((const u32 *)(mem + P6_DESC_A84CC),
                        DSD(orec + 0x18u),
                        (u32)((s32)DSD(orec + 0x30u) >> 16),
                        0u, 0u);                            /* 0x23F6A..0x23F7E */
    DSB(child + 0x59u) = 2u;                                /* 0x23F83 */
    DSD(P6_REC_104748) = child;                             /* 0x23F87 */
    a5 = (u32)(u16)(DSW(child + 0x56u) | 0x0400u);          /* 0x23F8C..0x23F98 */
    (void)actor_spawn((const u32 *)(mem + P6_DESC_A84CC), 0u, 0u, 0xFu, a5);  /* 0x23F9D..0x23FA7 */
    a5 = (u32)(u16)(DSW(DSD(P6_REC_104748) + 0x56u) | 0x0400u);   /* 0x23FAC..0x23FBD */
    (void)actor_spawn((const u32 *)(mem + P6_DESC_A84CC), 0x14u, 0u, 0xFu, a5);  /* 0x23FC2..0x23FCF */
    a5 = (u32)(u16)(DSW(DSD(P6_REC_104748) + 0x56u) | 0x0400u);   /* 0x23FD4..0x23FE5 */
    (void)actor_spawn((const u32 *)(mem + P6_DESC_A84CC), 0xFFFFFFECu, 0u, 0xFu, a5);  /* 0x23FEA..0x23FF7 */
    (void)sound_voice(0x64u);                               /* 0x23FFC/0x24001 */
}


/* 0x45C98 — record §P6.9. The D100 target at the dword 0xEB80A. With the
 * record's +0x14 slot and the other side's slot: the other record on
 * 0xC90F8[other char] at 2.0; 0x13C70(pset[other rec+0x56].+0x18, 4,
 * 0x105FC30); voice 0xC75AA[other char]. */
void fighter_45c98(u32 rec)
{
    u32 slot = DSD(rec + 0x14u);                            /* 0x45C9B */
    u32 side, other, orec, ch, pset;
    if (slot == 0u) return;                                 /* 0x45CA0 */
    side = (u32)DSB(DSD(slot) + 0x51u);                     /* 0x45CA2..0x45CA7 */
    other = DSD(DS_001077A8 + ((side ^ 1u) & 0xFFu) * 4u);  /* 0x45CAE */
    if (other == 0u) return;                                /* 0x45CB7 */
    ch = (u32)DSB(other + 0x7Au);                           /* 0x45CBB */
    orec = DSD(other);                                      /* 0x45CBE */
    actors_anim_begin(orec, DSD(P6_STREAMS_C90F8 + ch * 4u), 0x40000000u);  /* 0x45CC0..0x45CCC */
    pset = DSD(DS_001014EC) + (u32)DSW(orec + 0x56u) * 0x20u;
    (void)effects_spawn(DSD(pset + 0x18u), 4u, P6_FX_HANDLE);   /* 0x45CD1..0x45CF3 */
    (void)sound_voice((u32)(u16)DSW(P6_VOICE_C75AA + ch * 2u));   /* 0x45CF8..0x45D0A */
}

```

- [ ] **Step 3: the prototypes, the wrappers, the forward declarations and the registrations.**

`port/src/game/fighter.h` (after the previous batch's prototypes):
```c
void fighter_23f10(u32 rec);
void fighter_45c98(u32 rec);
```

`port/src/game/actors.c` (after `anim_code_37B54`; each wrapper drops the operand):
```c
/* 0x23F10 — the D100 target at the dword 0xE4FE0. */
static void anim_code_23F10(u32 rec, u32 arg)
{
    (void)arg;
    fighter_23f10(rec);
}


/* 0x45C98 — the D100 target at the dword 0xEB80A. */
static void anim_code_45C98(u32 rec, u32 arg)
{
    (void)arg;
    fighter_45c98(rec);
}

```

`port/src/game/actors.c` forward declarations (after `static void anim_code_45D58(u32 rec, u32 arg);`):
```c
static void anim_code_23F10(u32 rec, u32 arg);
static void anim_code_45C98(u32 rec, u32 arg);
```

`port/src/game/actors.c` registrations (after the previous task's last `fn_register`):
```c
    fn_register(0x23F10u, (void (*)(void))anim_code_23F10);
    fn_register(0x45C98u, (void (*)(void))anim_code_45C98);
```

- [ ] **Step 4: the spec.**

`tools/diff_verify.py`: append to `P6_SPECS` (before its closing `]`):
```python
    Spec("fighter_23f10", 0x23F10, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0),
                                                     E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x7A: b"\x02"}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS,
                                                     E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x7A: b"\x02",
                                                     E3_REC2 + 0x18: _p6_fill(0x40, {0x09: b"\x41", 0x04: le32(0x1000),
                                                                                    0x00: le32(0x1111),
                                                                                    0x18: le32(0x22220000),
                                                                                    0x3E: b"\x00\x11"}),
                                                     0xA83EC + 4: b"\x11\x11\xf0\xff",
                                                     0xA8408 + 8: le32(0x000ED222) + le32(0x000ED111),
                                                     P6_CHILD: _p6_fill(0x40, {}, 0x99),
                                                     P6_CHILD + 0x40: _p6_fill(0x20, {0x16: b"\x00\x22"}, 0x99),
                                                     0x104748: le32(0xDEADBEEF)},
             {0x2AE14: P6_CHILD}),
        Case("s2", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS,
                                                      E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                      DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x7A: b"\x02",
                                                      E3_REC + 0x18: _p6_fill(0x40, {0x09: b"\x41", 0x04: le32(0x1000),
                                                                                     0x00: le32(0x1111),
                                                                                     0x18: le32(0x22220000),
                                                                                     0x3E: b"\x00\x11"}),
                                                      0xA83EC + 4: b"\x34\x12\x11\x11",
                                                      0xA8408 + 8: le32(0x000ED222) + le32(0x000ED333),
                                                      P6_CHILD: _p6_fill(0x40, {}, 0x99),
                                                     P6_CHILD + 0x40: _p6_fill(0x20, {0x16: b"\x00\x22"}, 0x99),
                                                      0x104748: le32(0xDEADBEEF)},
             {0x2AE14: P6_CHILD}),
    ], calls=
    Spec("fighter_45c98", 0x45C98, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x14: le32(0), E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01"}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {DS_SLOTS: le32(E3_REC), DS_SLOTS + 0x94: le32(E3_REC2),
                                                     DS_SLOTS - 8: le32(DS_SLOTS) + le32(0),
                                                     E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x51: b"\x01",
                                                     E3_SLOT: le32(E3_REC), E3_REC + 0x56: b"\x02\x00",
                                                     DS_PSET_BASE: le32(P6_PSET),
                                                     P6_PSET + 2 * 0x20 + 0x18: le32(0x0010A800)}),
        Case("s2", {"eax": E3_REC, "edx": 0x1234}, {DS_SLOTS: le32(E3_REC2), DS_SLOTS + 0x94: le32(E3_REC),
                                                     DS_SLOTS - 8: le32(DS_SLOTS) + le32(DS_SLOTS + 0x94),
                                                     E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x51: b"\x00",
                                                     E3_SLOT: le32(E3_OUT),
                                                     E3_OUT + 0x51: b"\x01\xa5\xa5\xa5\xa5\x02\x00",
                                                     DS_SLOTS + 0x7A: b"\x02", E3_REC2 + 0x56: b"\x02\x00",
                                                     0xC90F8 + 8: le32(0x000ED111) + le32(0x000ED222),
                                                     0xC75AA + 4: b"\x66\x00\x77\x00",
                                                     DS_PSET_BASE: le32(P6_PSET),
                                                     P6_PSET + 2 * 0x20 + 0x18: le32(0x0010A800)}),
        Case("s3", {"eax": E3_REC2, "edx": 0x1234}, {DS_SLOTS: le32(E3_REC2), DS_SLOTS + 0x94: le32(E3_REC),
                                                      DS_SLOTS - 8: le32(DS_SLOTS) + le32(DS_SLOTS + 0x94),
                                                      E3_REC2 + 0x14: le32(E3_SLOT), E3_REC2 + 0x51: b"\x01",
                                                      E3_SLOT: le32(E3_OUT),
                                                      E3_OUT + 0x51: b"\x00\xa5\xa5\xa5\xa5\x02\x00",
                                                      DS_SLOTS + 0x94 + 0x7A: b"\x03", E3_REC + 0x56: b"\x02\x00",
                                                      0xC90F8 + 8: le32(0x000ED222) + le32(0x000ED333),
                                                      0xC75AA + 4: b"\x66\x00\x77\x00",
                                                      DS_PSET_BASE: le32(P6_PSET),
                                                      P6_PSET + 2 * 0x20 + 0x18: le32(0x0010A800)}),
    ], calls=
```

- [ ] **Step 5: the bindings and mutants.** Insert before `static const binding_t k_bindings[] = {` in `port/tests/diff_runner.c`:

```c
static void b_23f10(const u32 *r, u32 *eax)            { b_anim(0x23F10u, r, eax); }
static void b_37dd4(const u32 *r, u32 *eax)            { b_anim(0x37DD4u, r, eax); }
static void b_22338(const u32 *r, u32 *eax)            { b_anim(0x22338u, r, eax); }
static void b_24220(const u32 *r, u32 *eax)            { fighter_24220(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_24220(const u32 *r, u32 *eax)            /* the countdown bound 1, not 0 */
{
    u32 slot = r[R_EAX], rec = r[R_EDX], side = r[R_EBX];
    u8 st = DSB(slot + 0x58u);
    if (st == 0u) {
        if ((s32)DSD(rec + 0x1Cu) >= 0x6400) {
            u32 ch = (u32)DSB(slot + 0x7Au);
            actors_anim_begin(rec, DSD(0x000A8510u + ch * 4u), 0x40000000u);
            hit_anchor_x(side, DSD(slot + 0x2Cu));
            DSW(rec + 0x2Cu) = 0x0200u;
            DSW(rec + 0x36u) = 0u;
            DSW(rec + 0x44u) = 1u;
            DSW(rec + 0x32u) = DSW(0x000A852Cu + (u32)DSW(DS_00104AFC) * 2u);
            DSB(slot + 0x41u) |= 0x20u;
            DSB(slot + 0x58u) = 1u;
            DSW(0x00104768u) = 0x0078u;
        }
        DSW(DSD(slot + 4u) + 0x2Cu) -= 0x80u;
        *eax = 0u;
        return;
    }
    if (st == 1u) {
        u16 t = (u16)(DSW(0x00104768u) - 1u);
        DSW(0x00104768u) = t;
        if ((s16)t <= 1) {
            (void)sound_voice(0x5Au);
            DSB(slot + 0x58u) = 2u;
        }
    }
    if (st == 1u || st == 2u) {
        if (DSD(rec + 0x1Cu) == 0u) {
            (void)actor_spawn((const u32 *)(mem + 0x000A853Cu), DSD(rec + 0x18u),
                              (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u);
            actors_anim_begin(rec, 0x000E87ACu, 0x3F800000u);
            (void)sound_voice(0x5Bu);
            DSB(DS_001078FC) = 1u;
            DSB(slot + 0x53u) = 3u;
            DSB(slot + 0x52u) = 9u;
            DSB(DS_000F0AFE) = 4u;
        }
        *eax = 0u;
        return;
    }
    *eax = 0u;
}
static void b_45c98(const u32 *r, u32 *eax)            { b_anim(0x45C98u, r, eax); }
static void m_23f10(const u32 *r, u32 *eax)            /* the word 0xA83F8, not 0xA83EA */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    u32 orec, ch, child, a5;
    if (other == 0u) { *eax = 0u; return; }
    DSB(other + 0x41u) |= 0x20u;
    ch = (u32)DSB(other + 0x7Au);
    orec = DSD(other);
    DSD(orec + 0x1Cu) -= (u32)(s32)(s16)DSW(0x000A83FAu + ch * 2u);
    actor_pset_flag_5f(orec, 0u);
    actors_anim_begin(orec, DSD(0x000A8408u + ch * 4u), 0x40400000u);
    child = actor_spawn((const u32 *)(mem + 0x000A84CCu), DSD(orec + 0x18u),
                        (u32)((s32)DSD(orec + 0x30u) >> 16), 0u, 0u);
    DSB(child + 0x59u) = 2u;
    DSD(0x00104748u) = child;
    a5 = (u32)(u16)(DSW(child + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0u, 0u, 0xFu, a5);
    a5 = (u32)(u16)(DSW(DSD(0x00104748u) + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0x14u, 0u, 0xFu, a5);
    a5 = (u32)(u16)(DSW(DSD(0x00104748u) + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0xFFFFFFECu, 0u, 0xFu, a5);
    (void)sound_voice(0x64u);
    *eax = 0u;
}
static void m_23f10(const u32 *r, u32 *eax)            /* the word 0xA83F8, not 0xA83EA */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    u32 orec, ch, child, a5;
    if (other == 0u) { *eax = 0u; return; }
    DSB(other + 0x41u) |= 0x20u;
    ch = (u32)DSB(other + 0x7Au);
    orec = DSD(other);
    DSD(orec + 0x1Cu) -= (u32)(s32)(s16)DSW(0x000A83FAu + ch * 2u);
    actor_pset_flag_5f(orec, 0u);
    actors_anim_begin(orec, DSD(0x000A8408u + ch * 4u), 0x40400000u);
    child = actor_spawn((const u32 *)(mem + 0x000A84CCu), DSD(orec + 0x18u),
                        (u32)((s32)DSD(orec + 0x30u) >> 16), 0u, 0u);
    DSB(child + 0x59u) = 2u;
    DSD(0x00104748u) = child;
    a5 = (u32)(u16)(DSW(child + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0u, 0u, 0xFu, a5);
    a5 = (u32)(u16)(DSW(DSD(0x00104748u) + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0x14u, 0u, 0xFu, a5);
    a5 = (u32)(u16)(DSW(DSD(0x00104748u) + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0xFFFFFFECu, 0u, 0xFu, a5);
    (void)sound_voice(0x64u);
    *eax = 0u;
}
static void m_23f10_flag(const u32 *r, u32 *eax)       /* the pset flag 1 */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    u32 orec, ch, child, a5;
    if (other == 0u) { *eax = 0u; return; }
    DSB(other + 0x41u) |= 0x20u;
    ch = (u32)DSB(other + 0x7Au);
    orec = DSD(other);
    DSD(orec + 0x1Cu) -= (u32)(s32)(s16)DSW(0x000A83ECu + ch * 2u);
    actor_pset_flag_5f(orec, 1u);
    actors_anim_begin(orec, DSD(0x000A8408u + ch * 4u), 0x40400000u);
    child = actor_spawn((const u32 *)(mem + 0x000A84CCu), DSD(orec + 0x18u),
                        (u32)((s32)DSD(orec + 0x30u) >> 16), 0u, 0u);
    DSB(child + 0x59u) = 2u;
    DSD(0x00104748u) = child;
    a5 = (u32)(u16)(DSW(child + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0u, 0u, 0xFu, a5);
    a5 = (u32)(u16)(DSW(DSD(0x00104748u) + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0x14u, 0u, 0xFu, a5);
    a5 = (u32)(u16)(DSW(DSD(0x00104748u) + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0xFFFFFFECu, 0u, 0xFu, a5);
    (void)sound_voice(0x64u);
    *eax = 0u;
}
static void m_23f10_x14(const u32 *r, u32 *eax)        /* the second child at x 0x15 */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    u32 orec, ch, child, a5;
    if (other == 0u) { *eax = 0u; return; }
    DSB(other + 0x41u) |= 0x20u;
    ch = (u32)DSB(other + 0x7Au);
    orec = DSD(other);
    DSD(orec + 0x1Cu) -= (u32)(s32)(s16)DSW(0x000A83ECu + ch * 2u);
    actor_pset_flag_5f(orec, 0u);
    actors_anim_begin(orec, DSD(0x000A8408u + ch * 4u), 0x40400000u);
    child = actor_spawn((const u32 *)(mem + 0x000A84CCu), DSD(orec + 0x18u),
                        (u32)((s32)DSD(orec + 0x30u) >> 16), 0u, 0u);
    DSB(child + 0x59u) = 2u;
    DSD(0x00104748u) = child;
    a5 = (u32)(u16)(DSW(child + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0u, 0u, 0xFu, a5);
    a5 = (u32)(u16)(DSW(DSD(0x00104748u) + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0x15u, 0u, 0xFu, a5);
    a5 = (u32)(u16)(DSW(DSD(0x00104748u) + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0xFFFFFFECu, 0u, 0xFu, a5);
    (void)sound_voice(0x64u);
    *eax = 0u;
}
static void m_23f10_a5(const u32 *r, u32 *eax)         /* a5 without the 0x400 */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    u32 orec, ch, child, a5;
    if (other == 0u) { *eax = 0u; return; }
    DSB(other + 0x41u) |= 0x20u;
    ch = (u32)DSB(other + 0x7Au);
    orec = DSD(other);
    DSD(orec + 0x1Cu) -= (u32)(s32)(s16)DSW(0x000A83ECu + ch * 2u);
    actor_pset_flag_5f(orec, 0u);
    actors_anim_begin(orec, DSD(0x000A8408u + ch * 4u), 0x40400000u);
    child = actor_spawn((const u32 *)(mem + 0x000A84CCu), DSD(orec + 0x18u),
                        (u32)((s32)DSD(orec + 0x30u) >> 16), 0u, 0u);
    DSB(child + 0x59u) = 2u;
    DSD(0x00104748u) = child;
    a5 = (u32)(u16)DSW(child + 0x56u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0u, 0u, 0xFu, a5);
    a5 = (u32)(u16)DSW(DSD(0x00104748u) + 0x56u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0x14u, 0u, 0xFu, a5);
    a5 = (u32)(u16)DSW(DSD(0x00104748u) + 0x56u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0xFFFFFFECu, 0u, 0xFu, a5);
    (void)sound_voice(0x64u);
    *eax = 0u;
}
static void m_23f10_other41(const u32 *r, u32 *eax)    /* the own slot's +0x41 */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u) ^ 1u) & 0xFFu) * 4u);
    u32 orec, ch, child, a5;
    if (other == 0u) { *eax = 0u; return; }
    DSB(DS_001077B0 + ((u32)DSB(rec + 0x51u)) * 0x94u + 0x41u) |= 0x20u;
    ch = (u32)DSB(other + 0x7Au);
    orec = DSD(other);
    DSD(orec + 0x1Cu) -= (u32)(s32)(s16)DSW(0x000A83ECu + ch * 2u);
    actor_pset_flag_5f(orec, 0u);
    actors_anim_begin(orec, DSD(0x000A8408u + ch * 4u), 0x40400000u);
    child = actor_spawn((const u32 *)(mem + 0x000A84CCu), DSD(orec + 0x18u),
                        (u32)((s32)DSD(orec + 0x30u) >> 16), 0u, 0u);
    DSB(child + 0x59u) = 2u;
    DSD(0x00104748u) = child;
    a5 = (u32)(u16)(DSW(child + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0u, 0u, 0xFu, a5);
    a5 = (u32)(u16)(DSW(DSD(0x00104748u) + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0x14u, 0u, 0xFu, a5);
    a5 = (u32)(u16)(DSW(DSD(0x00104748u) + 0x56u) | 0x0400u);
    (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), 0xFFFFFFECu, 0u, 0xFu, a5);
    (void)sound_voice(0x64u);
    *eax = 0u;
}
static void m_45c98(const u32 *r, u32 *eax)            /* the own record's stream table */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u32 side, other, orec, ch, pset;
    if (slot == 0u) { *eax = 0u; return; }
    side = (u32)DSB(DSD(slot) + 0x51u);
    other = DSD(DS_001077A8 + ((side ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    ch = (u32)DSB(other + 0x7Au);
    orec = DSD(other);
    actors_anim_begin(rec, DSD(0x000C90F8u + ch * 4u), 0x40000000u);
    pset = DSD(DS_001014EC) + (u32)DSW(orec + 0x56u) * 0x20u;
    (void)effects_spawn(DSD(pset + 0x18u), 4u, 0x00105FC30u);
    (void)sound_voice((u32)(u16)DSW(0x000C75AAu + ch * 2u));
    *eax = 0u;
}
static void m_45c98_fx(const u32 *r, u32 *eax)         /* the effects byte 5 */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u32 side, other, orec, ch, pset;
    if (slot == 0u) { *eax = 0u; return; }
    side = (u32)DSB(DSD(slot) + 0x51u);
    other = DSD(DS_001077A8 + ((side ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    ch = (u32)DSB(other + 0x7Au);
    orec = DSD(other);
    actors_anim_begin(orec, DSD(0x000C90F8u + ch * 4u), 0x40000000u);
    pset = DSD(DS_001014EC) + (u32)DSW(orec + 0x56u) * 0x20u;
    (void)effects_spawn(DSD(pset + 0x18u), 5u, 0x00105FC30u);
    (void)sound_voice((u32)(u16)DSW(0x000C75AAu + ch * 2u));
    *eax = 0u;
}
static void m_45c98_side(const u32 *r, u32 *eax)       /* the side from the argument, not the slot's record */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u32 side, other, orec, ch, pset;
    if (slot == 0u) { *eax = 0u; return; }
    side = (u32)DSB(rec + 0x51u);
    other = DSD(DS_001077A8 + ((side ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    ch = (u32)DSB(other + 0x7Au);
    orec = DSD(other);
    actors_anim_begin(orec, DSD(0x000C90F8u + ch * 4u), 0x40000000u);
    pset = DSD(DS_001014EC) + (u32)DSW(orec + 0x56u) * 0x20u;
    (void)effects_spawn(DSD(pset + 0x18u), 4u, 0x00105FC30u);
    (void)sound_voice((u32)(u16)DSW(0x000C75AAu + ch * 2u));
    *eax = 0u;
}
static void m_45c98_voice(const u32 *r, u32 *eax)      /* the voice from the own char */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u32 side, other, orec, ch, pset;
    if (slot == 0u) { *eax = 0u; return; }
    side = (u32)DSB(DSD(slot) + 0x51u);
    other = DSD(DS_001077A8 + ((side ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    ch = (u32)DSB(other + 0x7Au);
    orec = DSD(other);
    actors_anim_begin(orec, DSD(0x000C90F8u + ch * 4u), 0x40000000u);
    pset = DSD(DS_001014EC) + (u32)DSW(orec + 0x56u) * 0x20u;
    (void)effects_spawn(DSD(pset + 0x18u), 4u, 0x00105FC30u);
    (void)sound_voice((u32)(u16)DSW(0x000C75AAu
                                    + (u32)DSB(DS_001077B0 + side * 0x94u + 0x7Au) * 2u));
    *eax = 0u;
}
```

and the rows into `k_bindings`:
```c
    { "fighter_23f10",                 0x00000000u },
    { "fighter_45c98",                 0x00000000u },
    { "fighter_23f10@mutant",          0x00000000u },
    { "fighter_23f10@word",            0x00000000u },
    { "fighter_23f10@flag",            0x00000000u },
    { "fighter_23f10@x14",             0x00000000u },
    { "fighter_23f10@a5",              0x00000000u },
    { "fighter_23f10@other41",         0x00000000u },
    { "fighter_45c98@mutant",          0x00000000u },
    { "fighter_45c98@stream",          0x00000000u },
    { "fighter_45c98@fx",              0x00000000u },
    { "fighter_45c98@side",            0x00000000u },
    { "fighter_45c98@voice",           0x00000000u },
```

- [ ] **Step 6: the unit checks.** Append to `port/tests/test_fight.c` and register in `test.h`:

```c
/* §P6.5/§P6.9: 0x23F10 and 0x45C98 (registration and evidence only; both spawn
 * and would need the actor pool) through their registrations. */
static void p6_check_23f10(void)
{
    CHECK(fn_resolve(0x23F10u) != NULL, "0x23F10 is registered");
    CHECK(fn_resolve(0x45C98u) != NULL, "0x45C98 is registered");
    CHECK_EQ_INT((int)DSD(0x000E4FE0u), 0x00023F10);
    CHECK_EQ_INT((int)DSD(0x000EB80Au), 0x00045C98);
}
```

- [ ] **Step 7: the task gate.**

```bash
cmake --build build 2>&1 | grep -E 'error|warning'
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/diff_verify.py --image /tmp/p6_img.bin --self-check 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/p6_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/p6_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
```

Expected: no compiler message, `all checks passed`, and
```
diff-verify: 92/92 functions VERIFIED; 213/213 mutants detected; 1 named gaps; 12/78 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 274 unported, 221 ported; supplement 131 (8 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 8: the E2 table and the commit.**

Regenerate the E2 table (`make entry-triage` first fails against the committed one, then `entry_triage.py --out` writes it) and commit the named files with `<area>: <what changed>`; no `Co-Authored-By` trailer.

---


### Task 7: `0x37DD4` (seam `0x29BC8`) and gp-u10-ending

**Files:** modify `port/src/game/fighter.c`, `port/src/game/fighter.h`, `port/src/game/actors.c`, `tools/diff_verify.py`, `port/tests/diff_runner.c`, `port/tests/test_fight.c`, `port/tests/test.h`, `port/tests/test_platform.c`, `Makefile`, `AGENTS.md`, the E2 table.

**Interfaces:** see the record §P6.6; the function signatures are in the code below.

- [ ] **Step 1: the seams.**

**`0x29BC8` (fighter.c).** `void fighter_29bc8(u32 side, u32 rec, u32 ch)` opens with
`PR_SEAM(0x29BC8u, side, rec, ch);`.

- [ ] **Step 2: the functions (append to the end of `port/src/game/fighter.c` after `fighter_45c98`).**

```c
/* 0x37DD4 — record §P6.7. The D100 target at the dword 0xD2BCE. The record's
 * pset palette word 4 when its +0x51 is non-zero else 0 (0x2A17C, handle 0),
 * then 0x29BC8(rec+0x51, rec, the side's char 0x10782A + side*0x94). */
void fighter_37dd4(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x37DDA/0x37DF8 */
    actor_pset_palette(rec, DSB(rec + 0x51u) != 0u ? 4u : 0u, 0u);  /* 0x37DDD..0x37DF3 */
    fighter_29bc8(side, rec, (u32)DSB(DS_0010782A + side * 0x94u));  /* 0x37E10..0x37E19 */
}

```

- [ ] **Step 3: the prototypes, the wrappers, the forward declarations and the registrations.**

`port/src/game/fighter.h` (after the previous batch's prototypes):
```c
void fighter_37dd4(u32 rec);
```

`port/src/game/actors.c` (after `anim_code_37B54`; each wrapper drops the operand):
```c
/* 0x37DD4 — the D100 target at the dword 0xD2BCE. */
static void anim_code_37DD4(u32 rec, u32 arg)
{
    (void)arg;
    fighter_37dd4(rec);
}

```

`port/src/game/actors.c` forward declarations (after `static void anim_code_45D58(u32 rec, u32 arg);`):
```c
static void anim_code_37DD4(u32 rec, u32 arg);
```

`port/src/game/actors.c` registrations (after the previous task's last `fn_register`):
```c
    fn_register(0x37DD4u, (void (*)(void))anim_code_37DD4);
```

- [ ] **Step 4: the spec.**

`tools/diff_verify.py`: append to `P6_SPECS` (before its closing `]`):
```python
    Spec("fighter_37dd4", 0x37DD4, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x51: b"\x00", DS_SLOTS + 0x7A: b"\x03",
                                                     DS_PSET_BASE: le32(P6_PSET)}),
        Case("s1", {"eax": E3_REC2, "edx": 0x1234}, {E3_REC2 + 0x51: b"\x01", DS_SLOTS + 0x94 + 0x7A: b"\x05",
                                                      DS_PSET_BASE: le32(P6_PSET)}),
        Case("s2", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x51: b"\x02", DS_SLOTS + 0x7A: b"\x03",
                                                     DS_PSET_BASE: le32(P6_PSET)}),
    ], calls=
```

- [ ] **Step 5: the bindings and mutants.** Insert before `static const binding_t k_bindings[] = {` in `port/tests/diff_runner.c`:

```c
static void b_37dd4(const u32 *r, u32 *eax)            { b_anim(0x37DD4u, r, eax); }
static void b_22338(const u32 *r, u32 *eax)            { b_anim(0x22338u, r, eax); }
static void b_24220(const u32 *r, u32 *eax)            { fighter_24220(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_24220(const u32 *r, u32 *eax)            /* the countdown bound 1, not 0 */
{
    u32 slot = r[R_EAX], rec = r[R_EDX], side = r[R_EBX];
    u8 st = DSB(slot + 0x58u);
    if (st == 0u) {
        if ((s32)DSD(rec + 0x1Cu) >= 0x6400) {
            u32 ch = (u32)DSB(slot + 0x7Au);
            actors_anim_begin(rec, DSD(0x000A8510u + ch * 4u), 0x40000000u);
            hit_anchor_x(side, DSD(slot + 0x2Cu));
            DSW(rec + 0x2Cu) = 0x0200u;
            DSW(rec + 0x36u) = 0u;
            DSW(rec + 0x44u) = 1u;
            DSW(rec + 0x32u) = DSW(0x000A852Cu + (u32)DSW(DS_00104AFC) * 2u);
            DSB(slot + 0x41u) |= 0x20u;
            DSB(slot + 0x58u) = 1u;
            DSW(0x00104768u) = 0x0078u;
        }
        DSW(DSD(slot + 4u) + 0x2Cu) -= 0x80u;
        *eax = 0u;
        return;
    }
    if (st == 1u) {
        u16 t = (u16)(DSW(0x00104768u) - 1u);
        DSW(0x00104768u) = t;
        if ((s16)t <= 1) {
            (void)sound_voice(0x5Au);
            DSB(slot + 0x58u) = 2u;
        }
    }
    if (st == 1u || st == 2u) {
        if (DSD(rec + 0x1Cu) == 0u) {
            (void)actor_spawn((const u32 *)(mem + 0x000A853Cu), DSD(rec + 0x18u),
                              (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u);
            actors_anim_begin(rec, 0x000E87ACu, 0x3F800000u);
            (void)sound_voice(0x5Bu);
            DSB(DS_001078FC) = 1u;
            DSB(slot + 0x53u) = 3u;
            DSB(slot + 0x52u) = 9u;
            DSB(DS_000F0AFE) = 4u;
        }
        *eax = 0u;
        return;
    }
    *eax = 0u;
}
static void m_37dd4(const u32 *r, u32 *eax)            /* the palette word always 4 */
{
    u32 rec = r[R_EAX];
    u32 side = (u32)DSB(rec + 0x51u);
    actor_pset_palette(rec, 4u, 0u);
    fighter_29bc8(side, rec, (u32)DSB(DS_0010782A + side * 0x94u));
    *eax = 0u;
}
static void m_37dd4_word(const u32 *r, u32 *eax)       /* the palette word when the side is 1 */
{
    u32 rec = r[R_EAX];
    u32 side = (u32)DSB(rec + 0x51u);
    actor_pset_palette(rec, side == 1u ? 4u : 0u, 0u);
    fighter_29bc8(side, rec, (u32)DSB(DS_0010782A + side * 0x94u));
    *eax = 0u;
}
static void m_37dd4_side(const u32 *r, u32 *eax)       /* the char of the other side */
{
    u32 rec = r[R_EAX];
    u32 side = (u32)DSB(rec + 0x51u);
    actor_pset_palette(rec, DSB(rec + 0x51u) != 0u ? 4u : 0u, 0u);
    fighter_29bc8(side, rec, (u32)DSB(DS_0010782A + (side ^ 1u) * 0x94u));
    *eax = 0u;
}
```

and the rows into `k_bindings`:
```c
    { "fighter_37dd4",                 0x00000000u },
    { "fighter_37dd4@mutant",          0x00000000u },
    { "fighter_37dd4@word",            0x00000000u },
    { "fighter_37dd4@side",            0x00000000u },
```

- [ ] **Step 6: the unit checks.** Append to `port/tests/test_fight.c` and register in `test.h`:

```c
/* §P6.7: 0x37DD4 through its registration: the palette word and the 0x29BC8
 * character. The real 0x2A17C runs (the fixture pset), so the pset's +2 word
 * shows the palette word. */
static void p6_check_37dd4(void)
{
    p1_anim_fn f;
    u32 pset;
    CHECK(fn_resolve(0x37DD4u) != NULL, "0x37DD4 is registered");
    CHECK_EQ_INT((int)DSD(0x000D2BCEu), 0x00037DD4);

    f = (p1_anim_fn)(void *)fn_resolve(0x37DD4u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSB(Z_S0 + 0x7Au) = 3u;
    pset = DSD(DS_001014EC) + (u32)DSW(Z_R0 + 0x56u) * 0x20u;
    DSW(pset + 2u) = 0x1234u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSW(pset + 2u), 0x1234);
    DSB(Z_R0 + 0x51u) = 1u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSW(pset + 2u), 0x1234);
}
```

- [ ] **Step 7: the task gate.**

```bash
cmake --build build 2>&1 | grep -E 'error|warning'
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/diff_verify.py --image /tmp/p6_img.bin --self-check 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/p6_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/p6_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
```

Expected: no compiler message, `all checks passed`, and
```
diff-verify: 93/93 functions VERIFIED; 216/216 mutants detected; 1 named gaps; 12/79 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 273 unported, 222 ported; supplement 131 (8 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 8: gp-u10-ending drops the `0x37DD4` row.** The block's comment is P2's wording, so the
script matches it whole from its first line to the array's end and asserts the rows it holds first; if the
merge left another set (the script's assertion names it), stop: re-measure the set with `make gp-replay
scenario=gp-u10-ending GP_OPTIONAL=1 GP_DUMP=/tmp/p6_gp` and record it, never predict it.

```bash
python3 - <<'PYEOF'
import pathlib
import re

p = pathlib.Path("port/tests/test_platform.c")
s = p.read_text()
m = re.findall(r"/\* gp-u10-ending \(plan gameplay-u9-u10.*?static const fnm_pair k_miss_gp_u10_ending\[\] = \{\n.*?\n\};\n",
               s, re.S)
assert len(m) == 1, "%d gp-u10-ending blocks" % len(m)
rows = re.findall(r"\{ (0x[0-9A-F]+)u, \"(\w+)\" \}", m[0])
assert rows == [("0x29D60", "frontend_mode_1b_step"), ("0x5D812", "frontend_mode_1b_step"),
                ("0x37DD4", "anim_indirect"), ("0x29C78", "anim_indirect"), ("0x3DA50", "anim_indirect")], rows
p.write_text(s.replace(m[0], open("/tmp/p6_u10_block.txt").read()))
print("t7_u10 applied")

p = pathlib.Path("Makefile")
s = p.read_text()
old = """# Re-measure: a P batch that ports 0x37DD4 (P6), 0x29C78 (P7) or 0x3DA50 (P5) drops its row and
# re-measures the U10 set; TRACE/WIN are at the replay's end, so they cannot rise.
"""
assert s.count(old) == 1, "the GP_ENDING re-measure sentence"
p.write_text(s.replace(old, """# Re-measured by track P batch 6 (record 2026-10-03-reverse-p6 §P6.12) once 0x37DD4 is
# ported: the set loses that row alone and every pin above is unchanged (each + 1 fails).
# Re-measure: a P batch that ports 0x29C78 (P7) or 0x3DA50 (P5) drops its row and
# re-measures the U10 set; TRACE/WIN are at the replay's end, so they cannot rise.
"""))
print("t7_u10 Makefile applied")

p = pathlib.Path("AGENTS.md")
s = p.read_text()
old = "  `0x3DA50` P5, `0x37DD4` P6, `0x29C78` P7; record §W.16; track P batch 3 ported `0x475EC`, record\n  2026-10-03-reverse-p3 §P3.9).\n"
new = ("  `0x3DA50` P5, `0x29C78` P7; record §W.16; track P batches 3 and 6 ported `0x475EC` and\n"
       "  `0x37DD4`, records 2026-10-03-reverse-p3 §P3.9 and 2026-10-03-reverse-p6 §P6.12).\n")
assert s.count(old) == 1, "the AGENTS U10 re-measure list"
p.write_text(s.replace(old, new))
print("t7_u10 AGENTS applied")
PYEOF
cmake --build build 2>&1 | grep -E 'error|warning'
make gp-ending-oracle GP_DUMP=/tmp/p6_gp GP_WIN_KEEP=1 2>&1 | grep -E '^(Ran|OK|FAILED)|ratchet N|FAIL|fn-miss PR_GP_DUMP distinct'
```

`/tmp/p6_u10_block.txt` is the block below. Expected: `fn-miss PR_GP_DUMP distinct=6 dropped=0` and every
pin at its P3 value (the measured lines above); a `distinct` above 6 means the replay now reaches something
new: stop, read the new pair's first frame and report; do not pin it unclassified.

The block to write to `/tmp/p6_u10_block.txt`:
```c
/* gp-u10-ending (plan gameplay-u9-u10, record 2026-10-02-gameplay-u9-u10-derivations.md
 * §W.14), measured on its full replay to its X record (f = 0x26E1): the two wipe hooks of
 * §G.24 (0x29D60, a bare `ret`, from f = 0x286; 0x5D812, the runtime stub, from f = 0x3FE),
 * the death-animation stream's target 0x29C78 (outside E2, record reverse-p1 §P1.2, dword
 * 0xD2BDA) at f = 0x14FE in mode 0xD and 0x3DA50 (E2 anim-target row, dword 0xD4BEC) at
 * f = 0x155D in mode 0xF. The stream target 0x37DD4 (E2 anim-target row, dword 0xD2BCE,
 * f = 0x14FB in mode 0xC; track P batch 6, record 2026-10-03-reverse-p6 §P6.7), CHAOS's
 * reaction-0x25 callback 0x2381C (track P batch 2) and character 2's reaction-0x0B callback
 * 0x475EC (the dword 0xA4004, from f = 0x1594 in mode 0xF; track P batch 3, record
 * 2026-10-03-reverse-p3 §P3.9) are ported, so none is a miss. */
static const fnm_pair k_miss_gp_u10_ending[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
    { 0x29C78u, "anim_indirect" },
    { 0x3DA50u, "anim_indirect" },
};
```

(The U10 script's `p.write_text(s.replace(m[0], open("/tmp/p6_u10_block.txt").read()))` reads the block
above from the file the step writes; write the block to `/tmp/p6_u10_block.txt` first, then run the
script.)

and the Makefile / AGENTS.md provenance:
```
# Re-measured by track P batch 6 (record 2026-10-03-reverse-p6 §P6.12) once 0x37DD4 is
# ported: the set loses that row alone and every pin above is unchanged (each + 1 fails).
# Re-measure: a P batch that ports 0x29C78 (P7) or 0x3DA50 (P5) drops its row and
# re-measures the U10 set; TRACE/WIN are at the replay's end, so they cannot rise.
```
```
  `0x3DA50` P5, `0x29C78` P7; record §W.16; track P batches 3 and 6 ported `0x475EC` and
  `0x37DD4`, records 2026-10-03-reverse-p3 §P3.9 and 2026-10-03-reverse-p6 §P6.12).
```

```bash
cmake --build build 2>&1 | grep -E 'error|warning'
make gp-ending-oracle GP_DUMP=/tmp/p6_gp GP_WIN_KEEP=1 2>&1 | grep -E '^(Ran|OK|FAILED)|ratchet N|fn-miss PR_GP_DUMP distinct'
```

Expected: `fn-miss PR_GP_DUMP distinct=6 dropped=0`, frames 331 ok, trace 9954 ok, path 30 ok, win 9954 ok; every pin +1 fails (332, 9955, 31, 9955).

- [ ] **Step 9: the E2 table and the commit.**

Regenerate the E2 table (`make entry-triage` first fails against the committed one, then `entry_triage.py --out` writes it) and commit the named files with `<area>: <what changed>`; no `Co-Authored-By` trailer.

---


### Task 8: `0x22338 0x48374` (seams `0x39280 0x39F40`)

**Files:** modify `port/src/game/fighter.c`, `port/src/game/fighter.h`, `port/src/game/actors.c`, `tools/diff_verify.py`, `port/tests/diff_runner.c`, `port/tests/test_fight.c`, `port/tests/test.h`, the E2 table.

**Interfaces:** see the record §P6.7; the function signatures are in the code below.

- [ ] **Step 1: the seams.**

**`0x39280` (fighter.c).** `void fighter_state_39280(u32 side)` opens with
`PR_SEAM(0x39280u, side);`.
**`0x39F40` (fighter.c).** Remove `static` from `fighter_pose_start`, open it with
`PR_SEAM(0x39F40u, side, edx, ebx, ecx, frame);` and declare
`void fighter_pose_start(u32 side, u32 edx, u32 ebx, u32 ecx, u32 frame);` in `fighter.h`.

- [ ] **Step 2: the functions (append to the end of `port/src/game/fighter.c` after `fighter_37dd4`).**

```c
/* 0x22338 — record §P6.4. The D500 target at the dword 0xE4E46. ctx; the own
 * record on 0xE4EAC at 3.0; pose 0x10 on the other side; voice 0x6B; 0x39280
 * (the other side); the other slot's +0x74 = 0; by the own slot's +0x57: 6
 * takes the 0x39F40 pose (-300, 0x8C, 0xF, 0xD), 7 takes (-70, 0x118, 0x13,
 * 0x1E), every other state (-100, 0x78, 0xF, 0x10); then voice
 * 0xBE008[other char] and the own +0x57 = 3. */
void fighter_22338(u32 rec)
{
    u32 ctx[6];
    u8 st;
    hit_anim_ctx(ctx, rec);                                 /* 0x22342 */
    actors_anim_begin(ctx[4], P6_ANIM_22338, 0x40400000u);  /* 0x22347..0x22355 */
    fighter_39834(ctx[1], 0x10u);                           /* 0x2235A/0x22363 */
    (void)sound_voice(0x6Bu);                               /* 0x22368/0x2236D */
    fighter_state_39280(ctx[1]);                            /* 0x22372/0x22376 */
    DSW(ctx[3] + 0x74u) = 0u;                               /* 0x2237B/0x2237F */
    st = DSB(ctx[2] + 0x57u);                               /* 0x22389 */
    if (st == 6u)                                           /* 0x2238E `jbe` */
        fighter_pose_start(ctx[1], 0xFFFFFED4u, 0x8Cu, 0x0Fu, 0x0Du);
    else if (st == 7u)                                      /* 0x22392/0x22394 */
        fighter_pose_start(ctx[1], 0xFFFFFFBAu, 0x118u, 0x13u, 0x1Eu);
    else                                                    /* 0x22396 */
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x78u, 0x0Fu, 0x10u);
    (void)sound_voice((u32)(u16)DSW(P6_VOICE_BE008
                                    + (u32)DSB(ctx[3] + 0x7Au) * 2u));   /* 0x223D6..0x223EF */
    DSB(ctx[2] + 0x57u) = 3u;                               /* 0x223F4/0x223F8 */
}


/* 0x48374 — record §P6.10. The D500 target at the dword 0xED8EA. ctx; the
 * side's speed -0x200; the own record's +0x36 = 0x2EE and +0x44 = 0x28; the own
 * slot's +0x57 = 3 and +0x54 = 2; the record on 0xED8FE at 4.0; when
 * 0x108392[side] is non-zero the other record on 0xC8F40[other char] at 5.0
 * and the other slot's +0x58 = 1, else pose the own slot's +0x5F and the
 * 0x39F40 pose (-100, 0x64, 0xF, 0x14). */
void fighter_48374(u32 rec)
{
    u32 ctx[6];
    hit_anim_ctx(ctx, rec);                                 /* 0x48380 */
    fighter_3c190(ctx[0], 0xFFFFFF38u);                     /* 0x48385..0x4838D */
    DSW(ctx[4] + 0x36u) = 0x02EEu;                          /* 0x48392/0x48396 */
    DSW(ctx[4] + 0x44u) = 0x0028u;                          /* 0x4839C/0x483A0 */
    DSB(ctx[2] + 0x57u) = 3u;                               /* 0x483A6/0x483AA */
    DSB(ctx[2] + 0x54u) = 2u;                               /* 0x483AE/0x483B2 */
    actors_anim_begin(rec, P6_ANIM_48374, 0x40400000u);     /* 0x483BD/0x483C2 */
    if (DSB(P6_108392 + ctx[0]) != 0u) {                  /* 0x483CA/0x483D1 */
        u32 ch = (u32)DSB(ctx[3] + 0x7Au);                  /* 0x483DC/0x483DF */
        actors_anim_begin(ctx[5], DSD(P6_STREAMS_C8F40 + ch * 4u),
                          0x40A00000u);                     /* 0x483E4..0x483EF */
        DSB(ctx[3] + 0x58u) = 1u;                           /* 0x483F8 */
    } else {
        fighter_39834(ctx[1], (u32)DSB(ctx[2] + 0x5Fu));    /* 0x483FE..0x48419 */
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x64u, 0x0Fu, 0x14u);   /* 0x4841E..0x48429 */
    }
}

```

- [ ] **Step 3: the prototypes, the wrappers, the forward declarations and the registrations.**

`port/src/game/fighter.h` (after the previous batch's prototypes):
```c
void fighter_22338(u32 rec);
void fighter_48374(u32 rec);
```

`port/src/game/actors.c` (after `anim_code_37B54`; each wrapper drops the operand):
```c
/* 0x22338 — the D500 target at the dword 0xE4E46. */
static void anim_code_22338(u32 rec, u32 arg)
{
    (void)arg;
    fighter_22338(rec);
}


/* 0x48374 — the D500 target at the dword 0xED8EA. */
static void anim_code_48374(u32 rec, u32 arg)
{
    (void)arg;
    fighter_48374(rec);
}

```

`port/src/game/actors.c` forward declarations (after `static void anim_code_45D58(u32 rec, u32 arg);`):
```c
static void anim_code_22338(u32 rec, u32 arg);
static void anim_code_48374(u32 rec, u32 arg);
```

`port/src/game/actors.c` registrations (after the previous task's last `fn_register`):
```c
    fn_register(0x22338u, (void (*)(void))anim_code_22338);
    fn_register(0x48374u, (void (*)(void))anim_code_48374);
```

- [ ] **Step 4: the spec.**

`tools/diff_verify.py`: append to `P6_SPECS` (before its closing `]`):
```python
    Spec("fighter_22338", 0x22338, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     DS_SLOTS + 0x57: b"\x00", DS_SLOTS + 0x74: b"\x74\x74",
                                                     DS_SLOTS + 0x94 + 0x74: b"\x84\x84",
                                                     0xBE008 + 6: b"\x77\x00", 0xBE008 + 4: b"\x66\x00"}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     DS_SLOTS + 0x57: b"\x06", DS_SLOTS + 0x74: b"\x74\x74",
                                                     DS_SLOTS + 0x94 + 0x74: b"\x84\x84",
                                                     0xBE008 + 6: b"\x77\x00", 0xBE008 + 4: b"\x66\x00"}),
        Case("s2", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     DS_SLOTS + 0x57: b"\x07", DS_SLOTS + 0x74: b"\x74\x74",
                                                     DS_SLOTS + 0x94 + 0x74: b"\x84\x84",
                                                     0xBE008 + 6: b"\x77\x00", 0xBE008 + 4: b"\x66\x00"}),
        Case("s3", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     DS_SLOTS + 0x57: b"\x08", DS_SLOTS + 0x74: b"\x74\x74",
                                                     DS_SLOTS + 0x94 + 0x74: b"\x84\x84",
                                                     0xBE008 + 6: b"\x77\x00", 0xBE008 + 4: b"\x66\x00"}),
        Case("s4", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                      DS_SLOTS + 0x94 + 0x57: b"\x06",
                                                      DS_SLOTS + 0x74: b"\x74\x74",
                                                      DS_SLOTS + 0x94 + 0x74: b"\x84\x84",
                                                      0xBE008 + 6: b"\x77\x00", 0xBE008 + 4: b"\x66\x00"}),
    ], allow_calls=(0x339AC,), calls=
    Spec("fighter_48374", 0x48374, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     0x108392: b"\x00\x5a",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x57: b"\x57",
                                                     DS_SLOTS + 0x54: b"\x54", DS_SLOTS + 0x58: b"\x58",
                                                     E3_REC + 0x36: b"\x36\x36", E3_REC + 0x44: b"\x44\x44",
                                                     DS_SLOTS + 0x94 + 0x58: b"\x98",
                                                     0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     0x108392: b"\x01\x00",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x57: b"\x57",
                                                     DS_SLOTS + 0x54: b"\x54", DS_SLOTS + 0x58: b"\x58",
                                                     E3_REC + 0x36: b"\x36\x36", E3_REC + 0x44: b"\x44\x44",
                                                     DS_SLOTS + 0x94 + 0x58: b"\x98",
                                                     0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
        Case("s2", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      0x108392: b"\x00\x00",
                                                      DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                      DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x57: b"\x57",
                                                      DS_SLOTS + 0x54: b"\x54", DS_SLOTS + 0x58: b"\x58",
                                                      E3_REC2 + 0x36: b"\x36\x36", E3_REC2 + 0x44: b"\x44\x44",
                                                      DS_SLOTS + 0x94 + 0x58: b"\x98",
                                                      0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
        Case("s3", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      0x108392: b"\x00\x01",
                                                      DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                      DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x57: b"\x57",
                                                      DS_SLOTS + 0x54: b"\x54", DS_SLOTS + 0x58: b"\x58",
                                                      E3_REC2 + 0x36: b"\x36\x36", E3_REC2 + 0x44: b"\x44\x44",
                                                      DS_SLOTS + 0x94 + 0x58: b"\x98",
                                                      0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
    ], allow_calls=(0x339AC,), calls=
```

- [ ] **Step 5: the bindings and mutants.** Insert before `static const binding_t k_bindings[] = {` in `port/tests/diff_runner.c`:

```c
static void b_22338(const u32 *r, u32 *eax)            { b_anim(0x22338u, r, eax); }
static void b_24220(const u32 *r, u32 *eax)            { fighter_24220(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_24220(const u32 *r, u32 *eax)            /* the countdown bound 1, not 0 */
{
    u32 slot = r[R_EAX], rec = r[R_EDX], side = r[R_EBX];
    u8 st = DSB(slot + 0x58u);
    if (st == 0u) {
        if ((s32)DSD(rec + 0x1Cu) >= 0x6400) {
            u32 ch = (u32)DSB(slot + 0x7Au);
            actors_anim_begin(rec, DSD(0x000A8510u + ch * 4u), 0x40000000u);
            hit_anchor_x(side, DSD(slot + 0x2Cu));
            DSW(rec + 0x2Cu) = 0x0200u;
            DSW(rec + 0x36u) = 0u;
            DSW(rec + 0x44u) = 1u;
            DSW(rec + 0x32u) = DSW(0x000A852Cu + (u32)DSW(DS_00104AFC) * 2u);
            DSB(slot + 0x41u) |= 0x20u;
            DSB(slot + 0x58u) = 1u;
            DSW(0x00104768u) = 0x0078u;
        }
        DSW(DSD(slot + 4u) + 0x2Cu) -= 0x80u;
        *eax = 0u;
        return;
    }
    if (st == 1u) {
        u16 t = (u16)(DSW(0x00104768u) - 1u);
        DSW(0x00104768u) = t;
        if ((s16)t <= 1) {
            (void)sound_voice(0x5Au);
            DSB(slot + 0x58u) = 2u;
        }
    }
    if (st == 1u || st == 2u) {
        if (DSD(rec + 0x1Cu) == 0u) {
            (void)actor_spawn((const u32 *)(mem + 0x000A853Cu), DSD(rec + 0x18u),
                              (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u);
            actors_anim_begin(rec, 0x000E87ACu, 0x3F800000u);
            (void)sound_voice(0x5Bu);
            DSB(DS_001078FC) = 1u;
            DSB(slot + 0x53u) = 3u;
            DSB(slot + 0x52u) = 9u;
            DSB(DS_000F0AFE) = 4u;
        }
        *eax = 0u;
        return;
    }
    *eax = 0u;
}
static void b_48374(const u32 *r, u32 *eax)            { b_anim(0x48374u, r, eax); }
static void m_22338(const u32 *r, u32 *eax)            /* the state-6 pose args of the default */
{
    u32 ctx[6];
    u8 st;
    hit_anim_ctx(ctx, r[R_EAX]);
    actors_anim_begin(ctx[4], 0x000E4EACu, 0x40400000u);
    fighter_39834(ctx[1], 0x10u);
    (void)sound_voice(0x6Bu);
    fighter_state_39280(ctx[1]);
    DSW(ctx[3] + 0x74u) = 0u;
    st = DSB(ctx[2] + 0x57u);
    if (st == 6u)
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x78u, 0x0Fu, 0x10u);
    else if (st == 7u)
        fighter_pose_start(ctx[1], 0xFFFFFFBAu, 0x118u, 0x13u, 0x1Eu);
    else
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x78u, 0x0Fu, 0x10u);
    (void)sound_voice((u32)(u16)DSW(0x000BE008u + (u32)DSB(ctx[3] + 0x7Au) * 2u));
    DSB(ctx[2] + 0x57u) = 3u;
    *eax = 0u;
}
static void m_22338(const u32 *r, u32 *eax)            /* the state-6 pose args of the default */
{
    u32 ctx[6];
    u8 st;
    hit_anim_ctx(ctx, r[R_EAX]);
    actors_anim_begin(ctx[4], 0x000E4EACu, 0x40400000u);
    fighter_39834(ctx[1], 0x10u);
    (void)sound_voice(0x6Bu);
    fighter_state_39280(ctx[1]);
    DSW(ctx[3] + 0x74u) = 0u;
    st = DSB(ctx[2] + 0x57u);
    if (st == 6u)
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x78u, 0x0Fu, 0x10u);
    else if (st == 7u)
        fighter_pose_start(ctx[1], 0xFFFFFFBAu, 0x118u, 0x13u, 0x1Eu);
    else
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x78u, 0x0Fu, 0x10u);
    (void)sound_voice((u32)(u16)DSW(0x000BE008u + (u32)DSB(ctx[3] + 0x7Au) * 2u));
    DSB(ctx[2] + 0x57u) = 3u;
    *eax = 0u;
}
static void m_22338_state(const u32 *r, u32 *eax)      /* 6 also takes the default pose */
{
    u32 ctx[6];
    u8 st;
    hit_anim_ctx(ctx, r[R_EAX]);
    actors_anim_begin(ctx[4], 0x000E4EACu, 0x40400000u);
    fighter_39834(ctx[1], 0x10u);
    (void)sound_voice(0x6Bu);
    fighter_state_39280(ctx[1]);
    DSW(ctx[3] + 0x74u) = 0u;
    st = DSB(ctx[2] + 0x57u);
    if (st == 7u)
        fighter_pose_start(ctx[1], 0xFFFFFFBAu, 0x118u, 0x13u, 0x1Eu);
    else
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x78u, 0x0Fu, 0x10u);
    (void)sound_voice((u32)(u16)DSW(0x000BE008u + (u32)DSB(ctx[3] + 0x7Au) * 2u));
    DSB(ctx[2] + 0x57u) = 3u;
    *eax = 0u;
}
static void m_22338_args(const u32 *r, u32 *eax)       /* the state-7 frame 0x1D */
{
    u32 ctx[6];
    u8 st;
    hit_anim_ctx(ctx, r[R_EAX]);
    actors_anim_begin(ctx[4], 0x000E4EACu, 0x40400000u);
    fighter_39834(ctx[1], 0x10u);
    (void)sound_voice(0x6Bu);
    fighter_state_39280(ctx[1]);
    DSW(ctx[3] + 0x74u) = 0u;
    st = DSB(ctx[2] + 0x57u);
    if (st == 6u)
        fighter_pose_start(ctx[1], 0xFFFFFED4u, 0x8Cu, 0x0Fu, 0x0Du);
    else if (st == 7u)
        fighter_pose_start(ctx[1], 0xFFFFFFBAu, 0x118u, 0x13u, 0x1Du);
    else
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x78u, 0x0Fu, 0x10u);
    (void)sound_voice((u32)(u16)DSW(0x000BE008u + (u32)DSB(ctx[3] + 0x7Au) * 2u));
    DSB(ctx[2] + 0x57u) = 3u;
    *eax = 0u;
}
static void m_22338_side(const u32 *r, u32 *eax)       /* 0x39280 on the own side */
{
    u32 ctx[6];
    u8 st;
    hit_anim_ctx(ctx, r[R_EAX]);
    actors_anim_begin(ctx[4], 0x000E4EACu, 0x40400000u);
    fighter_39834(ctx[1], 0x10u);
    (void)sound_voice(0x6Bu);
    fighter_state_39280(ctx[0]);
    DSW(ctx[3] + 0x74u) = 0u;
    st = DSB(ctx[2] + 0x57u);
    if (st == 6u)
        fighter_pose_start(ctx[1], 0xFFFFFED4u, 0x8Cu, 0x0Fu, 0x0Du);
    else if (st == 7u)
        fighter_pose_start(ctx[1], 0xFFFFFFBAu, 0x118u, 0x13u, 0x1Eu);
    else
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x78u, 0x0Fu, 0x10u);
    (void)sound_voice((u32)(u16)DSW(0x000BE008u + (u32)DSB(ctx[3] + 0x7Au) * 2u));
    DSB(ctx[2] + 0x57u) = 3u;
    *eax = 0u;
}
static void m_22338_word(const u32 *r, u32 *eax)       /* the second voice from the own char */
{
    u32 ctx[6];
    u8 st;
    hit_anim_ctx(ctx, r[R_EAX]);
    actors_anim_begin(ctx[4], 0x000E4EACu, 0x40400000u);
    fighter_39834(ctx[1], 0x10u);
    (void)sound_voice(0x6Bu);
    fighter_state_39280(ctx[1]);
    DSW(ctx[3] + 0x74u) = 0u;
    st = DSB(ctx[2] + 0x57u);
    if (st == 6u)
        fighter_pose_start(ctx[1], 0xFFFFFED4u, 0x8Cu, 0x0Fu, 0x0Du);
    else if (st == 7u)
        fighter_pose_start(ctx[1], 0xFFFFFFBAu, 0x118u, 0x13u, 0x1Eu);
    else
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x78u, 0x0Fu, 0x10u);
    (void)sound_voice((u32)(u16)DSW(0x000BE008u + (u32)DSB(ctx[2] + 0x7Au) * 2u));
    DSB(ctx[2] + 0x57u) = 3u;
    *eax = 0u;
}
static void m_22338_state3(const u32 *r, u32 *eax)     /* the final +0x57 = 2 */
{
    u32 ctx[6];
    u8 st;
    hit_anim_ctx(ctx, r[R_EAX]);
    actors_anim_begin(ctx[4], 0x000E4EACu, 0x40400000u);
    fighter_39834(ctx[1], 0x10u);
    (void)sound_voice(0x6Bu);
    fighter_state_39280(ctx[1]);
    DSW(ctx[3] + 0x74u) = 0u;
    st = DSB(ctx[2] + 0x57u);
    if (st == 6u)
        fighter_pose_start(ctx[1], 0xFFFFFED4u, 0x8Cu, 0x0Fu, 0x0Du);
    else if (st == 7u)
        fighter_pose_start(ctx[1], 0xFFFFFFBAu, 0x118u, 0x13u, 0x1Eu);
    else
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x78u, 0x0Fu, 0x10u);
    (void)sound_voice((u32)(u16)DSW(0x000BE008u + (u32)DSB(ctx[3] + 0x7Au) * 2u));
    DSB(ctx[2] + 0x57u) = 2u;
    *eax = 0u;
}
static void m_48374(const u32 *r, u32 *eax)            /* the speed -0x1FF */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    fighter_3c190(ctx[0], 0xFFFFFE01u);
    DSW(ctx[4] + 0x36u) = 0x02EEu;
    DSW(ctx[4] + 0x44u) = 0x0028u;
    DSB(ctx[2] + 0x57u) = 3u;
    DSB(ctx[2] + 0x54u) = 2u;
    actors_anim_begin(r[R_EAX], 0x000ED8FEu, 0x40400000u);
    if (DSB(0x00108392u + ctx[0]) != 0u) {
        u32 ch = (u32)DSB(ctx[3] + 0x7Au);
        actors_anim_begin(ctx[5], DSD(0x000C8F40u + ch * 4u), 0x40A00000u);
        DSB(ctx[3] + 0x58u) = 1u;
    } else {
        fighter_39834(ctx[1], (u32)DSB(ctx[2] + 0x5Fu));
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x64u, 0x0Fu, 0x14u);
    }
    *eax = 0u;
}
static void m_48374_stream(const u32 *r, u32 *eax)     /* the first stream 0xED8BC */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    fighter_3c190(ctx[0], 0xFFFFFF38u);
    DSW(ctx[4] + 0x36u) = 0x02EEu;
    DSW(ctx[4] + 0x44u) = 0x0028u;
    DSB(ctx[2] + 0x57u) = 3u;
    DSB(ctx[2] + 0x54u) = 2u;
    actors_anim_begin(r[R_EAX], 0x000ED8BCu, 0x40400000u);
    if (DSB(0x00108392u + ctx[0]) != 0u) {
        u32 ch = (u32)DSB(ctx[3] + 0x7Au);
        actors_anim_begin(ctx[5], DSD(0x000C8F40u + ch * 4u), 0x40A00000u);
        DSB(ctx[3] + 0x58u) = 1u;
    } else {
        fighter_39834(ctx[1], (u32)DSB(ctx[2] + 0x5Fu));
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x64u, 0x0Fu, 0x14u);
    }
    *eax = 0u;
}
static void m_48374_frame(const u32 *r, u32 *eax)      /* the second frame 4.0 */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    fighter_3c190(ctx[0], 0xFFFFFF38u);
    DSW(ctx[4] + 0x36u) = 0x02EEu;
    DSW(ctx[4] + 0x44u) = 0x0028u;
    DSB(ctx[2] + 0x57u) = 3u;
    DSB(ctx[2] + 0x54u) = 2u;
    actors_anim_begin(r[R_EAX], 0x000ED8FEu, 0x40400000u);
    if (DSB(0x00108392u + ctx[0]) != 0u) {
        u32 ch = (u32)DSB(ctx[3] + 0x7Au);
        actors_anim_begin(ctx[5], DSD(0x000C8F40u + ch * 4u), 0x40400000u);
        DSB(ctx[3] + 0x58u) = 1u;
    } else {
        fighter_39834(ctx[1], (u32)DSB(ctx[2] + 0x5Fu));
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x64u, 0x0Fu, 0x14u);
    }
    *eax = 0u;
}
static void m_48374_side(const u32 *r, u32 *eax)       /* the branch byte from the other side */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    fighter_3c190(ctx[0], 0xFFFFFF38u);
    DSW(ctx[4] + 0x36u) = 0x02EEu;
    DSW(ctx[4] + 0x44u) = 0x0028u;
    DSB(ctx[2] + 0x57u) = 3u;
    DSB(ctx[2] + 0x54u) = 2u;
    actors_anim_begin(r[R_EAX], 0x000ED8FEu, 0x40400000u);
    if (DSB(0x00108392u + (1u - ctx[0])) != 0u) {
        u32 ch = (u32)DSB(ctx[3] + 0x7Au);
        actors_anim_begin(ctx[5], DSD(0x000C8F40u + ch * 4u), 0x40A00000u);
        DSB(ctx[3] + 0x58u) = 1u;
    } else {
        fighter_39834(ctx[1], (u32)DSB(ctx[2] + 0x5Fu));
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x64u, 0x0Fu, 0x14u);
    }
    *eax = 0u;
}
static void m_48374_pose(const u32 *r, u32 *eax)       /* the pose frame 0x15 */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    fighter_3c190(ctx[0], 0xFFFFFF38u);
    DSW(ctx[4] + 0x36u) = 0x02EEu;
    DSW(ctx[4] + 0x44u) = 0x0028u;
    DSB(ctx[2] + 0x57u) = 3u;
    DSB(ctx[2] + 0x54u) = 2u;
    actors_anim_begin(r[R_EAX], 0x000ED8FEu, 0x40400000u);
    if (DSB(0x00108392u + ctx[0]) != 0u) {
        u32 ch = (u32)DSB(ctx[3] + 0x7Au);
        actors_anim_begin(ctx[5], DSD(0x000C8F40u + ch * 4u), 0x40A00000u);
        DSB(ctx[3] + 0x58u) = 1u;
    } else {
        fighter_39834(ctx[1], (u32)DSB(ctx[2] + 0x5Fu));
        fighter_pose_start(ctx[1], 0xFFFFFF9Cu, 0x64u, 0x0Fu, 0x15u);
    }
    *eax = 0u;
}
```

and the rows into `k_bindings`:
```c
    { "fighter_22338",                 0x00000000u },
    { "fighter_48374",                 0x00000000u },
    { "fighter_22338@mutant",          0x00000000u },
    { "fighter_22338@state",           0x00000000u },
    { "fighter_22338@args",            0x00000000u },
    { "fighter_22338@side",            0x00000000u },
    { "fighter_22338@word",            0x00000000u },
    { "fighter_22338@state3",          0x00000000u },
    { "fighter_48374@mutant",          0x00000000u },
    { "fighter_48374@speed",           0x00000000u },
    { "fighter_48374@stream",          0x00000000u },
    { "fighter_48374@frame",           0x00000000u },
    { "fighter_48374@side",            0x00000000u },
    { "fighter_48374@pose",            0x00000000u },
```

- [ ] **Step 6: the unit checks.** Append to `port/tests/test_fight.c` and register in `test.h`:

```c
/* §P6.4/§P6.10: 0x22338 and 0x48374 through their registrations, with the real
 * 0x39280/0x3A95C/0x39834/0x39F40 running on the fixture (their own rows cover
 * the arguments). */
static void p6_check_22338(void)
{
    p1_anim_fn f;
    CHECK(fn_resolve(0x22338u) != NULL, "0x22338 is registered");
    CHECK(fn_resolve(0x48374u) != NULL, "0x48374 is registered");
    CHECK_EQ_INT((int)DSD(0x000E4E46u), 0x00022338);
    CHECK_EQ_INT((int)DSD(0x000ED8EAu), 0x00048374);

    /* 0x22338 with side 0's record: the own slot's +0x57 = 6 selects the
     * 0x39F40 pose, which writes the side's pose words 0x107A60..0x107A7C. */
    f = (p1_anim_fn)(void *)fn_resolve(0x22338u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSB(Z_S0 + 0x57u) = 6u;
    DSB(Z_S0 + 0x74u) = 0x74u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSD(0x00107A68u + 4u), (int)0xFFFFFED4u);
    CHECK_EQ_INT((int)DSD(0x00107A78u + 4u), 0x8C);
    CHECK_EQ_INT((int)DSD(0x00107A60u + 4u), 0x0F);
    CHECK_EQ_INT((int)DSD(0x00107A70u + 4u), 0x0D);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 3);
    CHECK_EQ_INT((int)DSW(Z_S1 + 0x74u), 0);

    /* 0x48374 with side 0's record and the byte 0x108392[0] = 0: the speed, the
     * own record's +0x36/+0x44, the own slot's +0x57/+0x54 and the 0x39F40 pose. */
    f = (p1_anim_fn)(void *)fn_resolve(0x48374u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSB(0x00108392u) = 0u;
    DSB(0x00108393u) = 0x5Au;
    DSW(Z_R0 + 0x36u) = 0x3636u;
    DSW(Z_R0 + 0x44u) = 0x4444u;
    DSB(Z_S0 + 0x57u) = 0x57u;
    DSB(Z_S0 + 0x54u) = 0x54u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSW(Z_R0 + 0x36u), 0x02EE);
    CHECK_EQ_INT((int)DSW(Z_R0 + 0x44u), 0x0028);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 3);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 2);
    CHECK_EQ_INT((int)DSD(0x00107A68u + 4u), (int)0xFFFFFF9Cu);
    CHECK_EQ_INT((int)DSD(0x00107A70u + 4u), 0x14);
}
```

- [ ] **Step 7: the task gate.**

```bash
cmake --build build 2>&1 | grep -E 'error|warning'
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/diff_verify.py --image /tmp/p6_img.bin --self-check 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/p6_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/p6_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
```

Expected: no compiler message, `all checks passed`, and
```
diff-verify: 95/95 functions VERIFIED; 228/228 mutants detected; 1 named gaps; 12/81 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 271 unported, 224 ported; supplement 131 (8 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 8: the E2 table and the commit.**

Regenerate the E2 table (`make entry-triage` first fails against the committed one, then `entry_triage.py --out` writes it) and commit the named files with `<area>: <what changed>`; no `Co-Authored-By` trailer.

---


### Task 9: `0x24220`, the slot +0x10 handler

**Files:** modify `port/src/game/fighter.c`, `port/src/game/fighter.h`, `port/src/game/actors.c`, `tools/diff_verify.py`, `port/tests/diff_runner.c`, `port/tests/test_fight.c`, `port/tests/test.h`, the E2 table.

**Interfaces:** see the record §P6.8; the function signatures are in the code below.

- [ ] **Step 1: the seams.**

No seam: `0x24220` has no unseamed callee. It registers through the case-10 adapter in the function block below.

- [ ] **Step 2: the functions (append to the end of `port/src/game/fighter.c` after `fighter_48374`).**

```c
/* 0x24220 — record §P6.6. The slot +0x10 handler 0x24338 stores in the other
 * slot (the dword at 0x243A4). 0x3531C case 10 (0x354E2) calls it with EAX =
 * slot, EDX = the slot's record (0x35396), EBX = side. By the slot's +0x58: 0,
 * with the record's +0x1C >= 0x6400, the record on 0xA8510[char] at 2.0,
 * 0x188DC(side, slot+0x2C), the record's +0x2C = 0x200, +0x36 = 0, +0x44 = 1,
 * +0x32 = word 0xA852C[stage], the slot's +0x41 bit 5, +0x58 = 1 and 0x104768
 * = 0x78; 1 counts 0x104768 down and at zero plays voice 0x5A and steps to 2;
 * 2, once the record's +0x1C is 0, spawns 0xA853C at the record's x/y, the
 * record on 0xE87AC at 1.0, voice 0x5B, 0x1078FC = 1, +0x53 = 3, +0x52 = 9
 * and 0xF0AFE = 4; above 2 nothing. Every state-0 path ends with the slot+4
 * record's word +0x2C -= 0x80. */
void fighter_24220(u32 slot, u32 rec, u32 side)
{
    u8 st = DSB(slot + 0x58u);                              /* 0x24227 */
    if (st == 0u) {                                         /* 0x2422C/0x24240 */
        if ((s32)DSD(rec + 0x1Cu) >= 0x6400) {              /* 0x24248/0x2424F */
            u32 ch = (u32)DSB(slot + 0x7Au);                /* 0x24253 */
            actors_anim_begin(rec, DSD(P6_STREAMS_A8510 + ch * 4u), 0x40000000u);  /* 0x2425D..0x24267 */
            hit_anchor_x(side, DSD(slot + 0x2Cu));          /* 0x24264/0x2426C/0x2426E/0x24270 */
            DSW(rec + 0x2Cu) = 0x0200u;                     /* 0x24286 */
            DSW(rec + 0x36u) = 0u;                          /* 0x2428C */
            DSW(rec + 0x44u) = 1u;                          /* 0x24292 */
            DSW(rec + 0x32u) = DSW(P6_WORD_A852C
                                   + (u32)DSW(DS_00104AFC) * 2u);  /* 0x24277..0x24298 */
            DSB(slot + 0x41u) |= 0x20u;                     /* 0x2429C */
            DSB(slot + 0x58u) = 1u;                         /* 0x242B1 */
            DSW(P6_WORD_104768) = 0x0078u;                  /* 0x242AA */
        }
        DSW(DSD(slot + 4u) + 0x2Cu) -= 0x80u;               /* 0x242B4/0x242B7 */
        return;
    }
    if (st == 1u) {                                         /* 0x2422E/0x242C1 */
        u16 t = (u16)(DSW(P6_WORD_104768) - 1u);            /* 0x242C1/0x242C8 */
        DSW(P6_WORD_104768) = t;                            /* 0x242C9 */
        if ((s16)t <= 0) {                                  /* 0x242D0/0x242D3 `jg` */
            (void)sound_voice(0x5Au);                       /* 0x242D5/0x242DA */
            DSB(slot + 0x58u) = 2u;                         /* 0x242DF */
        }
    }
    if (st == 1u || st == 2u) {                             /* 0x24234/0x24236, fallthrough */
        if (DSD(rec + 0x1Cu) == 0u) {                       /* 0x242E2..0x242E7 */
            (void)actor_spawn((const u32 *)(mem + P6_DESC_A853C),
                              DSD(rec + 0x18u),
                              (u32)((s32)DSD(rec + 0x30u) >> 16),
                              0u, 0u);                      /* 0x242E9..0x242FA */
            actors_anim_begin(rec, P6_ANIM_24220, 0x3F800000u);   /* 0x242FF..0x2430B */
            (void)sound_voice(0x5Bu);                       /* 0x24310/0x24317 */
            DSB(DS_001078FC) = 1u;                          /* 0x2431C (DL = 1) */
            DSB(slot + 0x53u) = 3u;                         /* 0x24322 */
            DSB(slot + 0x52u) = 9u;                         /* 0x24328 */
            DSB(DS_000F0AFE) = 4u;                          /* 0x2432C */
        }
        return;                                             /* 0x24332 */
    }
}

```

- [ ] **Step 3: the prototypes and the registration (append to `port/src/game/fighter.h`; register in `actors_init`).**

`port/src/game/fighter.h` (after the previous batch's prototypes):
```c
void fighter_24220(u32 slot, u32 rec, u32 side);
```

`port/src/game/actors.c` registrations (after the previous task's last `fn_register`):
```c
    fn_register(0x24220u, (void (*)(void))fighter_24220_case10);
```

- [ ] **Step 4: the spec.**

`tools/diff_verify.py`: append to `P6_SPECS` (before its closing `]`):
```python
    Spec("fighter_24220", 0x24220, [
        Case("m0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P6_SLOT_SEED, E3_REC + 0x1C: le32(0x63FF)}),
        Case("m1", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P6_SLOT_SEED, E3_REC + 0x1C: le32(0x6400)}),
        Case("m2", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P6_SLOT_SEED, 0x104768: b"\x02\x00", E3_REC + 0x1C: le32(0),
                     **P6_SLOT_SEED_EXTRA, P6_CHILD: b"\x99" * 0x20}, {0x2AE14: P6_CHILD}),
        Case("m3", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P6_SLOT_SEED, 0x104768: b"\x01\x00", E3_REC + 0x1C: le32(5),
                     **P6_SLOT_SEED_EXTRA, P6_CHILD: b"\x99" * 0x20}, {0x2AE14: P6_CHILD}),
        Case("m4", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P6_SLOT_SEED, E3_SLOT + 0x58: b"\x02", E3_REC + 0x1C: le32(0),
                     P6_CHILD: b"\x99" * 0x20}, {0x2AE14: P6_CHILD}),
        Case("m5", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P6_SLOT_SEED, E3_SLOT + 0x58: b"\x03", E3_REC + 0x1C: le32(0)}),
        Case("m6", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 1}, {**P6_SLOT_SEED, E3_REC + 0x1C: le32(0x6400)}),
    ], calls=
```

- [ ] **Step 5: the bindings and mutants.** Insert before `static const binding_t k_bindings[] = {` in `port/tests/diff_runner.c`:

```c
static void b_24220(const u32 *r, u32 *eax)            { fighter_24220(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_24220(const u32 *r, u32 *eax)            /* the countdown bound 1, not 0 */
{
    u32 slot = r[R_EAX], rec = r[R_EDX], side = r[R_EBX];
    u8 st = DSB(slot + 0x58u);
    if (st == 0u) {
        if ((s32)DSD(rec + 0x1Cu) >= 0x6400) {
            u32 ch = (u32)DSB(slot + 0x7Au);
            actors_anim_begin(rec, DSD(0x000A8510u + ch * 4u), 0x40000000u);
            hit_anchor_x(side, DSD(slot + 0x2Cu));
            DSW(rec + 0x2Cu) = 0x0200u;
            DSW(rec + 0x36u) = 0u;
            DSW(rec + 0x44u) = 1u;
            DSW(rec + 0x32u) = DSW(0x000A852Cu + (u32)DSW(DS_00104AFC) * 2u);
            DSB(slot + 0x41u) |= 0x20u;
            DSB(slot + 0x58u) = 1u;
            DSW(0x00104768u) = 0x0078u;
        }
        DSW(DSD(slot + 4u) + 0x2Cu) -= 0x80u;
        *eax = 0u;
        return;
    }
    if (st == 1u) {
        u16 t = (u16)(DSW(0x00104768u) - 1u);
        DSW(0x00104768u) = t;
        if ((s16)t <= 1) {
            (void)sound_voice(0x5Au);
            DSB(slot + 0x58u) = 2u;
        }
    }
    if (st == 1u || st == 2u) {
        if (DSD(rec + 0x1Cu) == 0u) {
            (void)actor_spawn((const u32 *)(mem + 0x000A853Cu), DSD(rec + 0x18u),
                              (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u);
            actors_anim_begin(rec, 0x000E87ACu, 0x3F800000u);
            (void)sound_voice(0x5Bu);
            DSB(DS_001078FC) = 1u;
            DSB(slot + 0x53u) = 3u;
            DSB(slot + 0x52u) = 9u;
            DSB(DS_000F0AFE) = 4u;
        }
        *eax = 0u;
        return;
    }
    *eax = 0u;
}
static void m_24220(const u32 *r, u32 *eax)            /* the countdown bound 1, not 0 */
{
    u32 slot = r[R_EAX], rec = r[R_EDX], side = r[R_EBX];
    u8 st = DSB(slot + 0x58u);
    if (st == 0u) {
        if ((s32)DSD(rec + 0x1Cu) >= 0x6400) {
            u32 ch = (u32)DSB(slot + 0x7Au);
            actors_anim_begin(rec, DSD(0x000A8510u + ch * 4u), 0x40000000u);
            hit_anchor_x(side, DSD(slot + 0x2Cu));
            DSW(rec + 0x2Cu) = 0x0200u;
            DSW(rec + 0x36u) = 0u;
            DSW(rec + 0x44u) = 1u;
            DSW(rec + 0x32u) = DSW(0x000A852Cu + (u32)DSW(DS_00104AFC) * 2u);
            DSB(slot + 0x41u) |= 0x20u;
            DSB(slot + 0x58u) = 1u;
            DSW(0x00104768u) = 0x0078u;
        }
        DSW(DSD(slot + 4u) + 0x2Cu) -= 0x80u;
        *eax = 0u;
        return;
    }
    if (st == 1u) {
        u16 t = (u16)(DSW(0x00104768u) - 1u);
        DSW(0x00104768u) = t;
        if ((s16)t <= 1) {
            (void)sound_voice(0x5Au);
            DSB(slot + 0x58u) = 2u;
        }
    }
    if (st == 1u || st == 2u) {
        if (DSD(rec + 0x1Cu) == 0u) {
            (void)actor_spawn((const u32 *)(mem + 0x000A853Cu), DSD(rec + 0x18u),
                              (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u);
            actors_anim_begin(rec, 0x000E87ACu, 0x3F800000u);
            (void)sound_voice(0x5Bu);
            DSB(DS_001078FC) = 1u;
            DSB(slot + 0x53u) = 3u;
            DSB(slot + 0x52u) = 9u;
            DSB(DS_000F0AFE) = 4u;
        }
        *eax = 0u;
        return;
    }
    *eax = 0u;
}
static void m_24220_st(const u32 *r, u32 *eax)         /* state 1 handled as state 2 */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u8 st = DSB(slot + 0x58u);
    if (st == 0u) {
        if ((s32)DSD(rec + 0x1Cu) >= 0x6400) {
            u32 ch = (u32)DSB(slot + 0x7Au);
            actors_anim_begin(rec, DSD(0x000A8510u + ch * 4u), 0x40000000u);
            hit_anchor_x(r[R_EBX], DSD(slot + 0x2Cu));
            DSW(rec + 0x2Cu) = 0x0200u;
            DSW(rec + 0x36u) = 0u;
            DSW(rec + 0x44u) = 1u;
            DSW(rec + 0x32u) = DSW(0x000A852Cu + (u32)DSW(DS_00104AFC) * 2u);
            DSB(slot + 0x41u) |= 0x20u;
            DSB(slot + 0x58u) = 1u;
            DSW(0x00104768u) = 0x0078u;
        }
        DSW(DSD(slot + 4u) + 0x2Cu) -= 0x80u;
        *eax = 0u;
        return;
    }
    if (st == 1u || st == 2u) {
        if (DSD(rec + 0x1Cu) == 0u) {
            (void)actor_spawn((const u32 *)(mem + 0x000A853Cu), DSD(rec + 0x18u),
                              (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u);
            actors_anim_begin(rec, 0x000E87ACu, 0x3F800000u);
            (void)sound_voice(0x5Bu);
            DSB(DS_001078FC) = 1u;
            DSB(slot + 0x53u) = 3u;
            DSB(slot + 0x52u) = 9u;
            DSB(DS_000F0AFE) = 4u;
        }
        *eax = 0u;
        return;
    }
    *eax = 0u;
}
static void m_24220_bound(const u32 *r, u32 *eax)      /* the bound 0x63FF */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u8 st = DSB(slot + 0x58u);
    if (st == 0u) {
        if ((s32)DSD(rec + 0x1Cu) >= 0x63FF) {
            u32 ch = (u32)DSB(slot + 0x7Au);
            actors_anim_begin(rec, DSD(0x000A8510u + ch * 4u), 0x40000000u);
            hit_anchor_x(r[R_EBX], DSD(slot + 0x2Cu));
            DSW(rec + 0x2Cu) = 0x0200u;
            DSW(rec + 0x36u) = 0u;
            DSW(rec + 0x44u) = 1u;
            DSW(rec + 0x32u) = DSW(0x000A852Cu + (u32)DSW(DS_00104AFC) * 2u);
            DSB(slot + 0x41u) |= 0x20u;
            DSB(slot + 0x58u) = 1u;
            DSW(0x00104768u) = 0x0078u;
        }
        DSW(DSD(slot + 4u) + 0x2Cu) -= 0x80u;
        *eax = 0u;
        return;
    }
    *eax = 0u;
}
static void m_24220_anchor(const u32 *r, u32 *eax)     /* the anchor x from the record's +0x2C */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u8 st = DSB(slot + 0x58u);
    if (st == 0u) {
        if ((s32)DSD(rec + 0x1Cu) >= 0x6400) {
            u32 ch = (u32)DSB(slot + 0x7Au);
            actors_anim_begin(rec, DSD(0x000A8510u + ch * 4u), 0x40000000u);
            hit_anchor_x(r[R_EBX], DSD(rec + 0x2Cu));
            DSW(rec + 0x2Cu) = 0x0200u;
            DSW(rec + 0x36u) = 0u;
            DSW(rec + 0x44u) = 1u;
            DSW(rec + 0x32u) = DSW(0x000A852Cu + (u32)DSW(DS_00104AFC) * 2u);
            DSB(slot + 0x41u) |= 0x20u;
            DSB(slot + 0x58u) = 1u;
            DSW(0x00104768u) = 0x0078u;
        }
        DSW(DSD(slot + 4u) + 0x2Cu) -= 0x80u;
        *eax = 0u;
        return;
    }
    *eax = 0u;
}
static void m_24220_desc(const u32 *r, u32 *eax)       /* the spawn descriptor 0xA84CC */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u8 st = DSB(slot + 0x58u);
    if (st == 1u || st == 2u) {
        if (DSD(rec + 0x1Cu) == 0u) {
            (void)actor_spawn((const u32 *)(mem + 0x000A84CCu), DSD(rec + 0x18u),
                              (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u);
            actors_anim_begin(rec, 0x000E87ACu, 0x3F800000u);
            (void)sound_voice(0x5Bu);
            DSB(DS_001078FC) = 1u;
            DSB(slot + 0x53u) = 3u;
            DSB(slot + 0x52u) = 9u;
            DSB(DS_000F0AFE) = 4u;
        }
        *eax = 0u;
        return;
    }
    *eax = 0u;
}
static void m_24220_voice(const u32 *r, u32 *eax)      /* the state-2 voice 0x5C */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u8 st = DSB(slot + 0x58u);
    if (st == 1u || st == 2u) {
        if (DSD(rec + 0x1Cu) == 0u) {
            (void)actor_spawn((const u32 *)(mem + 0x000A853Cu), DSD(rec + 0x18u),
                              (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u);
            actors_anim_begin(rec, 0x000E87ACu, 0x3F800000u);
            (void)sound_voice(0x5Cu);
            DSB(DS_001078FC) = 1u;
            DSB(slot + 0x53u) = 3u;
            DSB(slot + 0x52u) = 9u;
            DSB(DS_000F0AFE) = 4u;
        }
        *eax = 0u;
        return;
    }
    *eax = 0u;
}
static void m_24220_plus4(const u32 *r, u32 *eax)      /* the slot+8 record's +0x2C */
{
    u32 slot = r[R_EAX];
    u8 st = DSB(slot + 0x58u);
    if (st == 0u) {
        DSW(DSD(slot + 8u) + 0x2Cu) -= 0x80u;
        *eax = 0u;
        return;
    }
    *eax = 0u;
}
static void m_24220_decr(const u32 *r, u32 *eax)       /* the decrement 0x100 */
{
    u32 slot = r[R_EAX];
    u8 st = DSB(slot + 0x58u);
    if (st == 0u) {
        DSW(DSD(slot + 4u) + 0x2Cu) -= 0x100u;
        *eax = 0u;
        return;
    }
    *eax = 0u;
}
```

and the rows into `k_bindings`:
```c
    { "fighter_24220",                 0x00000000u },
    { "fighter_24220@mutant",          0x00000000u },
    { "fighter_24220@st",              0x00000000u },
    { "fighter_24220@bound",           0x00000000u },
    { "fighter_24220@anchor",          0x00000000u },
    { "fighter_24220@desc",            0x00000000u },
    { "fighter_24220@voice",           0x00000000u },
    { "fighter_24220@plus4",           0x00000000u },
    { "fighter_24220@decr",            0x00000000u },
```

- [ ] **Step 6: the unit checks.** Append to `port/tests/test_fight.c` and register in `test.h`:

```c
/* §P6.6: 0x24220, the slot +0x10 handler, through its case-10 adapter: the
 * state byte walks and the spawn/anim/voice block on the fixture. */
static void p6_check_24220(void)
{
    void (*g)(u32, u32);
    u32 slot = Z_S0, rec = Z_R0;
    CHECK(fn_resolve(0x24220u) == (void (*)(void))fighter_24220_case10, "0x24220 is registered");
    CHECK_EQ_INT((int)DSD(0x000243A4u), 0x00024220);

    g = (void (*)(u32, u32))(void *)fn_resolve(0x24220u);
    if (g == NULL) return;

    /* State 0 with the record's +0x1C below 0x6400: only the slot+4 record's
     * +0x2C word decrements. */
    z_fseed();
    DSD(slot + 4u) = Z_R1;
    DSW(Z_R1 + 0x2Cu) = 0x2C2Cu;
    DSB(slot + 0x58u) = 0u;
    DSD(rec + 0x1Cu) = 0x63FFu;
    g(slot, 0u);
    CHECK_EQ_INT((int)DSW(Z_R1 + 0x2Cu), 0x2BAC);

    /* State 0 at/above 0x6400: the block runs, then the same decrement. */
    z_fseed();
    DSD(slot + 4u) = Z_R1;
    DSW(Z_R1 + 0x2Cu) = 0x2C2Cu;
    DSB(slot + 0x58u) = 0u;
    DSB(slot + 0x7Au) = 3u;
    DSD(rec + 0x1Cu) = 0x6400u;
    DSW(rec + 0x36u) = 0x3636u;
    DSW(rec + 0x44u) = 0x4444u;
    DSW(rec + 0x32u) = 0x3232u;
    DSB(slot + 0x41u) = 0x41u;
    DSW(DS_00104AFC) = 2u;
    DSW(0x000A852Cu + 4u) = 0x8899u;
    g(slot, 0u);
    CHECK_EQ_INT((int)DSW(rec + 0x2Cu), 0x0200);
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x44u), 1);
    CHECK_EQ_INT((int)DSW(rec + 0x32u), 0x8899);
    CHECK_EQ_INT((int)DSB(slot + 0x41u), 0x61);
    CHECK_EQ_INT((int)DSB(slot + 0x58u), 1);
    CHECK_EQ_INT((int)DSW(0x00104768u), 0x78);
    CHECK_EQ_INT((int)DSW(Z_R1 + 0x2Cu), 0x2BAC);

    /* State 1 with the countdown 1: the voice 0x5A and +0x58 = 2, then the
     * state-2 block (the record's +0x1C = 0) sets +0x52/+0x53 and 0xF0AFE. */
    z_fseed();
    DSD(slot + 4u) = Z_R1;
    DSW(Z_R1 + 0x2Cu) = 0x2C2Cu;
    DSB(slot + 0x58u) = 1u;
    DSW(0x00104768u) = 1u;
    DSD(rec + 0x1Cu) = 0u;
    DSB(slot + 0x52u) = 0x52u;
    DSB(slot + 0x53u) = 0x53u;
    DSB(DS_000F0AFE) = 0xFEu;
    DSB(DS_001078FC) = 0xFCu;
    g(slot, 0u);
    CHECK_EQ_INT((int)DSW(0x00104768u), 0);
    CHECK_EQ_INT((int)DSB(slot + 0x58u), 2);
    CHECK_EQ_INT((int)DSB(slot + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(slot + 0x53u), 3);
    CHECK_EQ_INT((int)DSB(DS_000F0AFE), 4);
    CHECK_EQ_INT((int)DSB(DS_001078FC), 1);
}
```

- [ ] **Step 7: the task gate.**

```bash
cmake --build build 2>&1 | grep -E 'error|warning'
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/diff_verify.py --image /tmp/p6_img.bin --self-check 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/p6_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/p6_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
```

Expected: no compiler message, `all checks passed`, and
```
diff-verify: 96/96 functions VERIFIED; 236/236 mutants detected; 1 named gaps; 12/82 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 271 unported, 224 ported; supplement 131 (8 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 8: the E2 table and the commit.**

Regenerate the E2 table (`make entry-triage` first fails against the committed one, then `entry_triage.py --out` writes it) and commit the named files with `<area>: <what changed>`; no `Co-Authored-By` trailer.

---

### Task 10: the harness expectations, the full gate and the docs (no code)

**Files:** modify `tools/tests/test_diff_verify.py`, `docs/PROGRESS.md`, the record (`docs/superpowers/plans/2026-10-03-reverse-p6-derivations.md` §P6.12).

- [ ] **Step 1: the Python expectations.** Add `P6_MASKS` (all 18 zero) and `P6_KINDS`, the new
`test_each_p6_mutant_is_caught_by_what_it_breaks` method, extend the exact-set assertions, the stub
clobber table and the counter assertion. The `P6_MASKS`/`P6_KINDS` block and the method are below; the
three assertion edits are:

```python
# 1. test_every_mutant_is_reported_as_a_mismatch: the names and the merged masks
s.replace('                                             "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)\n'
          '                                            + list(P3_MASKS)))',
          '                                             "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)\n'
          '                                            + list(P3_MASKS) + list(P6_MASKS)))')
s.replace('            **P1_MASKS, **P2_MASKS, **P3_MASKS})',
          '            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P6_MASKS})')
s.replace('            + list(P1_KINDS) + list(P2_KINDS) + list(P3_KINDS)))',
          '            + list(P1_KINDS) + list(P2_KINDS) + list(P3_KINDS) + list(P6_KINDS)))')
# 2. test_each_stub_declares_the_registers_its_callee_clobbers: the twelve new stubs
s.replace('0x36D98: (), 0x188DC: ("edx",), 0x3C16C: ()})',
          '0x36D98: (), 0x188DC: ("edx",), 0x3C16C: (),\n'
          '                                 0x39280: (), 0x39F40: ("ebx", "edx"), 0x13244: (), 0x2A148: ("edx",),\n'
          '                                 0x2BCF4: ("edx",), 0x1890C: ("edx",), 0x37D18: ("edx",),\n'
          '                                 0x13C70: ("ebx", "edx"), 0x3AA54: (), 0x29C08: ("edx",),\n'
          '                                 0x29BC8: ("ebx", "edx")})')
# 3. test_the_self_check_counts_functions_mutants_gaps_and_closed_rows: the counter
s.replace('        self.assertIn("diff-verify: 78/78 functions VERIFIED; 158/158 mutants detected; 1 named gaps; "\n'
          '                      "11/64 rows with callees closed (14 have none).", out.getvalue())',
          '        self.assertIn("diff-verify: 96/96 functions VERIFIED; 236/236 mutants detected; 1 named gaps; "\n'
          '                      "12/82 rows with callees closed (14 have none).", out.getvalue())')
```

`P6_MASKS`, `P6_KINDS` and the method (insert the method next to `test_each_p3_mutant_is_caught_by_what_it_breaks`):

```python
P6_MASKS = {"anim_2bda0": 0, "fighter_22338": 0, "fighter_22494": 0, "fighter_22a40": 0, "fighter_23f10": 0, "fighter_2400c": 0, "fighter_241f4": 0, "fighter_24220": 0, "fighter_24338": 0, "fighter_37dd4": 0, "fighter_3e160": 0, "fighter_40148": 0, "fighter_40170": 0, "fighter_45c98": 0, "fighter_47e04": 0, "fighter_47e30": 0, "fighter_482e4": 0, "fighter_48374": 0}
P6_KINDS = {
            "anim_2bda0@mask": {"call #0"},
            "anim_2bda0@mutant": {"byte"},
            "fighter_22338@args": {"call #4"},
            "fighter_22338@mutant": {"call #4"},
            "fighter_22338@side": {"call #3"},
            "fighter_22338@state": {"call #4"},
            "fighter_22338@state3": {"byte"},
            "fighter_22338@word": {"call #5"},
            "fighter_22494@frame": {"call #0"},
            "fighter_22494@mutant": {"call #0"},
            "fighter_22494@pose": {"call #2"},
            "fighter_22494@side": {"call #1", "call #2"},
            "fighter_22a40@a5": {"call #0"},
            "fighter_22a40@mutant": {"call #0"},
            "fighter_22a40@order": {"call #1 memory"},
            "fighter_22a40@side": {"byte", "call #1 memory", "call #2 memory"},
            "fighter_23f10@a5": {"call #3", "call #4", "call #5"},
            "fighter_23f10@flag": {"call #0"},
            "fighter_23f10@mutant": {"byte", "call #0 memory", "call #1 memory", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory", "call #6 memory"},
            "fighter_23f10@other41": {"byte", "call #0 memory", "call #1 memory", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory", "call #6 memory"},
            "fighter_23f10@word": {"byte", "call #0 memory", "call #1 memory", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory", "call #6 memory"},
            "fighter_23f10@x14": {"call #4"},
            "fighter_2400c@char": {"byte", "call #0", "call #0 memory", "call #1 memory", "call #2 memory"},
            "fighter_2400c@mutant": {"byte", "call #0 memory", "call #1 memory", "call #2 memory"},
            "fighter_2400c@other": {"byte", "call #0", "call #0 memory", "call #1 memory", "call #2 memory"},
            "fighter_2400c@seek": {"byte", "call #0 memory", "call #1", "call #1 memory", "call #2 memory"},
            "fighter_2400c@word": {"byte", "call #0 memory", "call #1 memory", "call #2 memory"},
            "fighter_241f4@mutant": {"call #0"},
            "fighter_241f4@side": {"call #0"},
            "fighter_241f4@voice": {"call #1"},
            "fighter_24220@anchor": {"byte", "call #0", "call #1", "call #2"},
            "fighter_24220@bound": {"byte", "call #0", "call #1", "call #2"},
            "fighter_24220@decr": {"byte", "call #0", "call #1", "call #2"},
            "fighter_24220@desc": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory", "call #2 memory"},
            "fighter_24220@mutant": {"byte", "call #0", "call #1", "call #1 memory", "call #2", "call #2 memory", "call #3"},
            "fighter_24220@plus4": {"byte", "call #0", "call #1", "call #2"},
            "fighter_24220@st": {"byte", "call #0", "call #0 memory", "call #1 memory", "call #2 memory"},
            "fighter_24220@voice": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory", "call #2", "call #2 memory"},
            "fighter_24338@af": {"byte", "call #2 memory", "call #3 memory"},
            "fighter_24338@mutant": {"call #0"},
            "fighter_24338@other": {"call #0"},
            "fighter_24338@slot": {"byte", "call #2 memory", "call #3 memory"},
            "fighter_24338@voice": {"call #3"},
            "fighter_37dd4@mutant": {"call #0"},
            "fighter_37dd4@side": {"call #1"},
            "fighter_37dd4@word": {"call #0"},
            "fighter_3e160@a5": {"call #1"},
            "fighter_3e160@child": {"byte"},
            "fighter_3e160@mutant": {"byte", "call #1 memory"},
            "fighter_3e160@side": {"call #0"},
            "fighter_40148@bit": {"byte"},
            "fighter_40148@mutant": {"byte", "call #0"},
            "fighter_40148@side": {"byte", "call #0"},
            "fighter_40170@char": {"call #1"},
            "fighter_40170@mutant": {"byte", "call #0 memory", "call #1 memory"},
            "fighter_40170@set": {"byte", "call #0 memory", "call #1 memory"},
            "fighter_40170@side": {"call #0", "call #1"},
            "fighter_45c98@fx": {"call #1"},
            "fighter_45c98@mutant": {"call #0"},
            "fighter_45c98@side": {"call #0", "call #1", "call #2"},
            "fighter_45c98@stream": {"call #0"},
            "fighter_45c98@voice": {"call #2"},
            "fighter_47e04@mutant": {"call #1"},
            "fighter_47e04@order": {"call #0", "call #1"},
            "fighter_47e04@side": {"call #1"},
            "fighter_47e30@flag": {"call #0", "call #1"},
            "fighter_47e30@mutant": {"call #0", "call #1"},
            "fighter_47e30@side": {"call #1"},
            "fighter_482e4@byte": {"call #1", "call #2"},
            "fighter_482e4@mutant": {"call #1", "call #2"},
            "fighter_482e4@pose": {"call #2"},
            "fighter_482e4@side": {"call #1", "call #2"},
            "fighter_48374@frame": {"call #2"},
            "fighter_48374@mutant": {"call #0"},
            "fighter_48374@pose": {"call #3"},
            "fighter_48374@side": {"byte", "call #2", "call #3"},
            "fighter_48374@speed": {"call #0"},
            "fighter_48374@stream": {"call #1"},}
```

```python
    def test_each_p6_mutant_is_caught_by_what_it_breaks(self):
        # track P batch 6 (record 2026-10-03-reverse-p6): what alone catches each mutant
        for name, want in P6_KINDS.items():
            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
            self.assertEqual(got, want, name)
```


- [ ] **Step 2: the full gate.**

```bash
T=p6; make verify SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att \
  FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 \
  GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md E2_IMAGE=/tmp/pr_${T}_e2.bin \
  > /tmp/pr_p6_final.log 2>&1; echo "EXIT=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_p6_final.log \
  | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
make audio-render AUDIO_WAV=/tmp/pr_p6.wav >/dev/null 2>&1; cmp /tmp/pr_p6.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-SAME
grep -E '^diff-verify:|^entry-triage: (targets|voice)|ratchet N' /tmp/pr_p6_final.log | sed 's/^gp_compare: //'
python3 tools/port_progress.py
```

Expected: `EXIT=0`, `ORACLES-EQUAL`, `WAV-SAME`, every gp ratchet at its pin, `diff-verify: 96/96 functions VERIFIED; 236/236 mutants detected; 1 named gaps; 12/82 rows with callees closed (14 have none)`, `entry-triage: targets 271 unported, 224 ported; supplement 131 (8 unported, 0 stale); untrusted entries 30`, `entry-triage: voice sites outside Ghidra 134: 15 in unported code, 100 in ported code, 19 nowhere`, `771 1203 64` / `731 731 100`. The 18 rows and P6's 78 mutants are the record §P6.12 tables. The full gate takes about 25 min.

- [ ] **Step 3: the docs.** Append the P6 paragraph to `docs/PROGRESS.md`; fill the record's §P6.12 with the measured final gate and any deviation (each a finding). Commit the docs with `docs: P6 closure (animation targets C) ...`.

---

## Self-review (the planner's)

- **The raw wins:** the plan's `0x47E30` ctx[3], the effective word bases and the 16-poke compositions are corrections the prototype's gates forced; each is in the record with its address.
- **No fitted constants:** every value in the specs is an image address/byte, a measured clobber, or a fixture seed; the case sets pin both alternatives of every branch (`P6_KINDS`).
- **Every store observable:** the rows compare every changed byte and the memory at every recorded call; the unit checks seed the stores they observe and arm `0x104B00 = 0x22` where the real PLACE chain would overwrite `0x29` (record §P6.3).
- **The E2 gate:** every task regenerates the table in the same commit (decision D3).
- **The gp claim is narrow:** only `0x37DD4` leaves a miss set; the other scenarios' sets were read from `k_gp_sets` and hold no P6 member.
