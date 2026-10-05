# P8: the remaining entries and the triage (track P, batch 8) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Resolve track P's last batch: port the two live entries of the 16 members (`0x3A820`, the 0x3A8E8 pose family's +0x10 handler; `0x29CFC`, the `jmp 0x13DF0` effects-clear tail) with differential rows, unit checks and registrations; and decide, with raw-byte evidence, every remaining member — the 7 untrusted entries (all blocks of already-ported functions), the 4 dead after-table ends, the two data immediates (`0x1D2D0`, `0x2D3FC..0x2D48C`), the 15 voice sites with no body, and the host-owned/deferred pair (`0x1BDF4`, `0x10604`) — recording the verdicts and writing them into the E2 table's resolutions section.

**Architecture:** One C function per original function. `fighter_pose_3a820` is `fighter_pose_3a43c`'s twin (the 0xC9058 stream table, the 0x107CF8/0x107CFC B/A words, +0x90 = 4) appended to `port/src/game/fighter.c` after `fighter_pose_3a588`, declared in `fighter.h`, registered in `actors_init` (`actors.c`). `effects_29cfc` is the one-call wrapper of the already-ported `effects_clear` (0x13DF0) in `effects.c`; `effects_clear` gains its `PR_SEAM0(0x13DF0u)` first statement so the new row is stubbed on both sides, and `game_mode_13_step`'s case 2 (flow.c) calls the wrapper. Each new function has a binding and mutants in `port/tests/diff_runner.c`, a `Spec` in `tools/diff_verify.py` (`P8_SPECS`), seeded unit checks in `port/tests/test_fight.c`/`test_game.c`, and its row in the self-check counter. The triage verdicts are documentation: a new `Track P batch 8 resolutions` section in `tools/entry_triage.py`'s render (regenerated table) and `tools/port_classification.txt` rows for `0x1BDF4`/`0x10604`. No gp miss set holds a P8 address, so no gp pin moves.

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and `capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §3 decision 1 ("port all of O3-O6, whether or not a capture reaches them"), §4 track P ("port batches, each function verified by E"), §5.1-§5.3, §6 ("P: each ported function has a verification row; unhit blocks and non-emulable instructions named; counters and README percentage updated"), §7.

**Derivation record:** `docs/superpowers/plans/2026-10-04-reverse-p8-derivations.md` (§P8.1 the 16 members and the triage from the raw, §P8.2 `0x3A820`, §P8.3 `0x29CFC`, §P8.4 the triage verdicts, §P8.5 the 15 voice sites, §P8.6 the host-owned/deferred resolutions, §P8.7 decisions and named gaps, §P8.8 results, §P8.9 the roadmap after P8). Recipe: `2026-10-01-reverse-e3-derivations.md` §E3.10; lessons: the P-track review checklist (`.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`) and the P3/P7 records' fix rounds.

**What the planner ran (this worktree, 2026-10-04, on `main` `4bf2209`; image sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`):** the prototype is Tasks 2-5 applied in order in this worktree; every expected output below is the output the planner measured on that prototype. The full `make verify` with the parallel-safe overrides ran **once on the prototype's final state** (`EXIT=0`; 45 oracle lines equal to `oracle-lines-base.txt`; the `make audio-render` WAV identical to `before-t2.wav`; `symbols.h` byte-identical; `192/192 functions VERIFIED; 552/552 mutants detected; 1 named gaps; 69/158 rows with callees closed (34 have none)`; `targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30`; every gp ratchet at its pin, quoted in Task 6). The base counters were re-measured on the reverted tree (Task 1). Every address and instruction is capstone 5.0.7 over that image (fixups applied); `unicorn` 2.1.4 runs the original side.

**Re-baseline note.** The base is `main` `4bf2209` (C1+C2+P4+P5+P6+P7 merged); no parallel batch is expected to merge first (C2b does not exist yet). The counters, E2 lines and gp pins below are the measured `4bf2209` values (Task 1) plus this plan's increments: diff-verify rows +1 (+1), mutants +6, closed rows +1, rows without callees 0, E2 ported targets +1. If a later merge moves a value, Task 1 records the measured one in the ledger and every later expected counter adds these increments to it. No gp set holds `0x29CFC` or `0x3A820` (`k_gp_sets`' rows were read: none), so no gp re-measure is planned; if a merge changes that, re-measure the affected scenario as P3 §P3.9 does.

## Decisions needed from the user

**None.** Every verdict is derived from the raw and recorded §P8.4-§P8.6:

1. **The 16 members' verdicts** (record §P8.1): two live ports (`3A820`, `29CFC`); seven interior blocks of ported functions (`19AD4 19DD5 26163 26226 34962 45444 49078`); four dead after-table ends with no reference anywhere (`2EE3C 37E40 3A3FC 4AEC4`, not ported); one data struct (`1D2D0`); six data addends (`2D3FC 2D414 2D444 2D45C 2D474 2D48C`).
2. **`0x1BDF4` is host-owned** (record §P8.6): its two callees are already host-owned (`0x1BBAC` record §50-C, `0x2D62C` record §K9.5), the port models the ISR tick in `config.c`/`host.c`, and a differential row would need seamed host-side stubs whose real bodies are host infrastructure. `tools/port_classification.txt` gains its row; it is not ported.
3. **`0x10604` is deferred** (record §P8.6): the 250 Hz AIL timer callback registered by the deferred movie-audio starter `0x10610` (record §50-E, `2026-09-24-demo-pose-derivations.md`: "the callback drives 0x102B8"); `tools/port_classification.txt` gains its row; it is not ported.
4. **The 15 voice sites with no body** (record §P8.5): 8 ride with ported bodies; the other 7 sit in bodies with no reference anywhere in the image (dead code): named gaps, not ported.

## The P-track roadmap

From record `2026-10-02-reverse-p1-derivations.md` §P1.3 as corrected by P2 §P2.11, P3 §P3.11 and P7 §P7.11. P8 is the last porting batch.

| batch | ports | what |
|---|---|---|
| P1-P7 (merged) | 16+24+24+21+15+18+14 | the finishers, callbacks, animation targets, the unported callees and outside-E2 targets |
| **P8** (this plan) | **2 + triage** | `0x3A820`, `0x29CFC`; the triage of the 16 members, the `2D3FC..2D48C` addends and the 15 voice sites |
| C2 / C2b | rows, no port | the remaining stubbed callees (C2 landed; a C2b re-review may touch `tools/diff_verify.py` later) |
| span | 0 | decision D2 |

Neither `0x3A820` nor `0x29CFC` is a Ghidra `FN_` function in the committed export: `python3 tools/port_progress.py` stays `771 1203 64` / `731 731 100` and README does not move (measured on the prototype).

## Global Constraints

Copied verbatim from their sources:

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- Spec §8: "`make verify` is the gate after every task." User ruling (2026-10-01, speed-up): the per-task gate is the task's tests, `make diff-verify`, `make entry-triage` and the gp oracles whose miss sets the task touches; the full `make verify` runs at the baseline (Task 1), the final task (Task 6) and before the merge, with the parallel-safe overrides `T=p8; SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md E2_IMAGE=/tmp/pr_${T}_e2.bin`. The 45 oracle lines (`grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>`) must equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; the `make audio-render AUDIO_WAV=/tmp/pr_p8.wav` WAV must be byte-identical to `.superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav`; `python3 tools/port_progress.py` prints `771 1203 64` and `731 731 100`. The planner's full gate on the prototype took about 35 minutes.
- AGENTS.md: "**The E2 target list is a gate** ... A commit that ports a target (a new `/* 0xADDR` header or `fn_register`) regenerates the table in the same commit (decision D3) ... A conflict in that file is resolved by regenerating it after the rebase, never by merging lines by hand."
- AGENTS.md: "One C function per original function, header comment `/* 0xADDR — spec section */`." / "Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment styles in `port/src`." / "Code addresses stored in data go through `fn_origin()` / `fn_resolve()`." / "**`port/src/symbols.h` is generated** ... Never hand-edit it; where the generator emits no name, use a local `#define` with the raw address."
- AGENTS.md: "A ported function that a differentially verified function stubs or records ... opens with `PR_SEAM(0xADDR, args...)` (void) or `PR_SEAM_RET(0xADDR, args...)` ... The arguments are the C signature's, in order (a pointer into `mem[]` as its offset)." Record E3 §E3.8: "A seam must be the callee's first statement". The hook returns 1 (stub) only for an address in the case's call set (`mem.h`), so the seam added to `effects_clear` (Task 3) is inert for every existing row (none declares `0x13DF0`).
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." / "**Register a test once** — add `X(test_foo)` to `TEST_CASES` in `port/tests/test.h`" / "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests, and never assert an unseeded BSS-zero". "Consolidating must not change an assertion": this plan **extends** the exact-set assertions of `RealFunctionTests` (rows, masks, mutant names, the stub clobbers table, the counter line) and changes no other assertion.
- AGENTS.md: "Every env-gated driver and `--check` run arms `fn_resolve`'s miss log and must record exactly the pinned known-set": no gp set holds a P8 member (Task 1 verifies), so no miss set or gp pin moves.
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style: `<area>: <what changed>`. **Never `git add -A`** — stage named files." No `Co-Authored-By` trailer (this is not a Claude session). Never run `make gp-capture`.
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.

## Review Focus

1. **The `0x3A820` row's lookalikes.** EAX is the dead slot and the side comes from EBX (`mov edx,ebx` at 0x3A823); the row's cases seed EDX with `0xDEAD` and differ EBX, and the `@side` mutant (side from EDX) is caught. The stream table 0xC9058 must not be 0xC8FE0 (`@stream`), the B/A globs must be the side-indexed words at 0x107CF8/0x107CFC and not swapped (`@globs`), the end store is +0x90 = 4 (`@end`), and the stores go after the 0x2BC30 call (`@order` via the call-memory comparison).
2. **The `0x29CFC` seam.** `effects_clear` gains `PR_SEAM0(0x13DF0u)` as its first statement; the new row declares `E.Call(0x13DF0)` stub, so both sides stop at the callee. The stub-table assertion gains `0x13DF0: ()` (`E.callee_clobbers` measured ()). The seam is inert in every existing row because no spec's call set holds 0x13DF0 (verified: the self-check's exact-set assertions still pass).
3. **Every store observable, every neighbour seeded.** The `0x3A820` row and the `test_p8_3a820` seeds carry sentinels for +0x58/+0x90, the record's +0x18/+0x1C, the +0x52/+0x24 anim writes, the pset word, and the other slot/record. `test_p8_29cfc` seeds the count bytes 0x5A and the pool head both built-empty and zero (the guard), so the clear and the guard each have a failing seed.
4. **The triage verdicts are evidence, not prose.** §P8.4/§P8.5 name the instruction site (or the zero-occurrence scan) for every member; the E2 table's new resolutions section repeats them (regenerated by the tool, never by hand).
5. **No gp oracle moves.** Task 1 reads `k_gp_sets`: no set holds `0x29CFC`/`0x3A820`; the full gate (Task 6) shows every ratchet at its pin unchanged.

## Where to run

The worktree `.worktrees/reverse-p8` (branch `reverse-p8`), cut from `main` `4bf2209`. Before Task 1:

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse/.worktrees/reverse-p8
ls -d data .superpowers port/tests/ghidra_data.bin port/tests/title_screen_ref.ppm build/diffrun
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_p8_img.bin && shasum /tmp/pr_p8_img.bin
```

The last line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the image differs from the one the record measured: stop. **A fresh linked worktree lacks the two git-ignored fixtures `port/tests/ghidra_data.bin` and `port/tests/title_screen_ref.ppm`** (they live only in the main checkout); copy them in (they are `PR_ORACLE_REQUIRED`-only fixtures, git-ignored, never committed) or the test pass fails with `PR_ORACLE_REQUIRED=1 but the Ghidra oracle is missing`. Every command runs from the worktree root. Ledger: `.superpowers/sdd/2026-10-04-reverse-p8-remaining-entries/progress.md`.

**How the code steps are written.** Each change is a `python3 - <<'PY'` script that replaces exact text and asserts the text occurs exactly once (`sub`), so a script either applies cleanly or stops naming the file and the text it could not find. Run each once, from the worktree root, in order. If `main` moved after `4bf2209`, an anchor can move: re-apply that `sub` by hand at the same place, never elsewhere; the unit tests' line numbers in the expected output move with `test_fight.c`, and the E2 table is regenerated, never merged.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/game/fighter.c`, `fighter.h` | `fighter_pose_3a820` (after `fighter_pose_3a588`) and its prototype |
| `port/src/game/effects.c`, `effects.h` | `effects_29cfc` (after `effects_clear`) and its prototype; `effects_clear`'s seam |
| `port/src/game/flow.c` | `game_mode_13_step`'s case 2 calls `effects_29cfc` |
| `port/src/game/actors.c` | the registrations of `0x3A820` and `0x29CFC` in `actors_init` |
| `port/tests/diff_runner.c` | the bindings and mutants (`b_29cfc`/`m_29cfc`, `b_3a820`/`m_3a820_*`) and their `k_bindings` rows |
| `tools/diff_verify.py` | `P8_SPECS` and the `SPECS` concat |
| `tools/tests/test_diff_verify.py` | `P8_MASKS`, `P8_KINDS`, the exact-set assertions extended, the stub table, the counter line |
| `port/tests/test_fight.c`, `port/tests/test_game.c`, `port/tests/test.h` | `test_p8_3a820`, `test_p8_29cfc` and their registration |
| `tools/entry_triage.py`, `docs/superpowers/plans/2026-10-01-reverse-e2-triage.md` | the resolutions section and the regenerated table (Tasks 3-4) |
| `tools/port_classification.txt`, `docs/PROGRESS.md`, the record, `AGENTS.md` | Task 5 and Task 6 |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes `main` `4bf2209` at the worktree's head; produces the baseline log `/tmp/pr_p8_base.log`.

- [ ] **Step 1: the full gate on the untouched tree.**

```bash
T=p8; make verify SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att \
  FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 \
  GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md E2_IMAGE=/tmp/pr_${T}_e2.bin \
  > /tmp/pr_p8_base.log 2>&1; echo "EXIT=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_p8_base.log \
  | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
grep -E '^diff-verify:|^entry-triage: (targets|voice)|ratchet N' /tmp/pr_p8_base.log | sed 's/^gp_compare: //;s/^gp_keys: //'
python3 tools/port_progress.py
```

Expected (the planner measured the counters on the reverted tree and the prototype's full gate; the base gate's oracle/ratchet lines equal the prototype's, quoted in Task 6): `EXIT=0`, `ORACLES-EQUAL`, and

```
diff-verify: 190/190 functions VERIFIED; 546/546 mutants detected; 1 named gaps; 68/156 rows with callees closed (34 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 234 unported, 261 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

and every gp ratchet line at the prototype's pins (Task 6's list). If a value differs (the re-baseline note), record the measured lines in the ledger; every later "expected" counter then adds this plan's increments to them.

- [ ] **Step 2: the WAV.** `make audio-render AUDIO_WAV=/tmp/pr_p8.wav >/dev/null 2>&1; cmp /tmp/pr_p8.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-SAME` prints `WAV-SAME`.

- [ ] **Step 3: no gp set holds a P8 address.**

```bash
python3 - <<'PY'
import sys
sys.path.insert(0, "port/tests")
import re
s = open("port/tests/test_platform.c").read()
for a in ("0x29CFC", "0x3A820"):
    print(a, "in test_platform.c:", a.lower() in s.lower())
PY
```

Expected: both print `False` (read `k_gp_sets`; no set holds a P8 member).

---

### Task 2: `0x3A820` (the 0x3A8E8 pose family's handler)

**Files:** modify `port/src/game/fighter.c` (after `fighter_pose_3a588`), `port/src/game/fighter.h` (after `fighter_pose_3a588`'s declaration), `port/src/game/actors.c` (`actors_init`, after the `0x3A588` registration), `port/tests/diff_runner.c` (after `b_29c78`; the `k_bindings` rows after `fighter_29c78@rec`), `tools/diff_verify.py` (a `P8_SPECS` block before `SPECS = [`; the `SPECS` concat), `tools/tests/test_diff_verify.py` (after `P7_KINDS`; the exact-set lists; the counter line), `port/tests/test_fight.c` (append at the end), `port/tests/test.h` (`TEST_CASES`).

**Interfaces:** produces `void fighter_pose_3a820(u32 slot, u32 side)`; `b_3a820`, `m_3a820_side/stream/globs/end/order` (diff_runner); `P8_MASKS`, `P8_KINDS` (tests); `p8_pose_seed`, `p8_check_3a820`, `test_p8_3a820` (test_fight.c). Consumes `E3_SLOT`, `E3_REC`, `E3_REC2`, `DS_SLOTS`, `SLOT_PTRS`, `ANIM_BEGIN`, `ANCHOR`, `ANCHORX` and `u6b_run`/`pose_chain_setup`/`FIGHT_RECS`/`FIGHT_ACTORS`/`fn_resolve`/`fn_register`. Record §P8.2.

- [ ] **Step 1: the port and its registration.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


# fighter.c: the twin after fighter_pose_3a588
sub("port/src/game/fighter.c", '''    DSB(ctx[3] + 0x90u) = 2u;                               /* 0x3A642 */
}

/* ---- the 0x39F40 knockback pose's handler 0x39CC8 ---------------------- */''', '''    DSB(ctx[3] + 0x90u) = 2u;                               /* 0x3A642 */
}

/* PORT: 0xC9058 (the 0x3A8E8 family's per-character animation-stream table,
 * read at 0x3A862) has no symbols.h name. */
#define FIGHT_ANIM_3A820  0x000C9058u

/* 0x3A820 — record §P8.2. The 0x3A8E8 pose family's per-frame handler
 * 0x3531C case 10 calls through slot+0x10 (0x3A8E8 stores it at 0x3A91E, the
 * dword at 0x3A921 its only reference). The body is 0x3A43C's with the
 * 0xC9058 stream table, the 0x3A8E8 setter's globs (B = 0x107CFC + side*2,
 * A = 0x107CF8 + side*2) and +0x90 = 4 at the end. Phase 0 sets +0x58 = 1;
 * phase 1 starts the self record's 0xC9058[char] stream at 3.0, re-anchors
 * the self record (x kept, y = 0), sets +0x58 = 2 and +0x90 = 4, and — when
 * B[side] is neither 0 nor 5 and (u8)(+0x90 - 1) > 3 — snaps the self x to
 * A[side] (the jump table at 0x3A810 sends 1..4 to 0x3A8D6, past the snap;
 * every entry is 0x3A8D6). Phases above 1 return. The raw takes EAX = slot,
 * EBX = side; the ctx swap overwrites EAX, so only the side is read.
 * 0x2BC30 returns with RET 4 (0x2BCEF), popping the 0x3A855 push, so from
 * 0x3A872 on the ESP offsets name ctx[1] (the side) and ctx[5] (rec_self). */
void fighter_pose_3a820(u32 slot, u32 side)
{
    u32 ctx[6];
    u8 phase;
    (void)slot;
    fighter_ctx_swap(ctx, side);                            /* 0x3A820/0x3A827 0x33A10 */
    phase = DSB(ctx[3] + 0x58u);                            /* 0x3A82C/0x3A830 */
    if (phase == 0u) {                                      /* 0x3A833/0x3A83D */
        DSB(ctx[3] + 0x58u) = 1u;                           /* 0x3A849 */
        return;
    }
    if (phase != 1u) return;                                /* 0x3A837/0x3A839 */
    actors_anim_begin(ctx[5],                                /* 0x3A862/0x3A86D */
                      DSD(FIGHT_ANIM_3A820
                          + (u32)DSB(ctx[3] + 0x7Au) * 4u),
                      0x40400000u);
    hit_anchor_set(ctx[1], DSD(ctx[5] + 0x18u), 0u);        /* 0x3A872..0x3A87F */
    DSB(ctx[3] + 0x58u) = 2u;                               /* 0x3A888 */
    {
        s32 b = (s32)(s16)DSW(DS_00107CFC + ctx[1] * 2u);   /* 0x3A890/0x3A8A0 */
        s32 a = (s32)(s16)DSW(DS_00107CF8 + ctx[1] * 2u);   /* 0x3A897/0x3A8A3 */
        if (b != 0 && b != 5) {                             /* 0x3A8A6/0x3A8AA */
            if ((u8)(DSB(ctx[3] + 0x90u) - 1u) > 3u)        /* 0x3A8B3..0x3A8BD */
                hit_anchor_x(ctx[1], (u32)a);               /* 0x3A8CC/0x3A8D1 */
        }
    }
    DSB(ctx[3] + 0x90u) = 4u;                               /* 0x3A8DA */
}

/* ---- the 0x39F40 knockback pose's handler 0x39CC8 ---------------------- */''')

# fighter.h: the prototype
sub("port/src/game/fighter.h", '''void fighter_pose_3a588(u32 slot, u32 side);
''', '''void fighter_pose_3a588(u32 slot, u32 side);

/* 0x3A820 (record §P8.2). The 0x3A8E8 pose family's per-frame handler:
 * 0x3A43C's body with the 0xC9058[char] stream, the 0x107CF8/0x107CFC B/A
 * words and +0x90 = 4. 0x3531C case 10 resolves it from slot+0x10;
 * registered in actors_init. EAX = slot (dead), EBX = side. */
void fighter_pose_3a820(u32 slot, u32 side);
''')

# actors.c: the registration
sub("port/src/game/actors.c", '''    fn_register(0x3A588u, (void (*)(void))fighter_pose_3a588);
''', '''    fn_register(0x3A588u, (void (*)(void))fighter_pose_3a588);
    /* PORT: record §P8.2. Their sibling 0x3A820, which the 0x3A8E8 setter
     * stores in slot+0x10 at 0x3A91E (the dword at 0x3A921 its only
     * reference). */
    fn_register(0x3A820u, (void (*)(void))fighter_pose_3a820);
''')
print("applied")
PY
cmake --build build 2>&1 | tail -1
```

Expected: `[100%] Built target ...` (or `run_tests`).

- [ ] **Step 2: the binding, the mutants and their `k_bindings` rows.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


sub("port/tests/diff_runner.c", '''static void b_29c78(const u32 *r, u32 *eax)            { b_anim(0x29C78u, r, eax); }
''', '''static void b_29c78(const u32 *r, u32 *eax)            { b_anim(0x29C78u, r, eax); }
static void b_3a820(const u32 *r, u32 *eax)            { fighter_pose_3a820(r[R_EAX], r[R_EBX]); *eax = 0u; }
static void m_3a820_side(const u32 *r, u32 *eax)       /* the side from EDX, not EBX */
{
    fighter_pose_3a820(r[R_EAX], r[R_EDX]);
    *eax = 0u;
}
static void m_3a820_stream(const u32 *r, u32 *eax)     /* the 0xC8FE0 stream table */
{
    u32 ctx[6];
    u8 phase;
    fighter_ctx_swap(ctx, r[R_EBX]);
    phase = DSB(ctx[3] + 0x58u);
    if (phase == 0u) { DSB(ctx[3] + 0x58u) = 1u; *eax = 0u; return; }
    if (phase != 1u) { *eax = 0u; return; }
    actors_anim_begin(ctx[5], DSD(0x000C8FE0u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40400000u);
    hit_anchor_set(ctx[1], DSD(ctx[5] + 0x18u), 0u);
    DSB(ctx[3] + 0x58u) = 2u;
    {
        s32 b = (s32)(s16)DSW(DS_00107CFC + ctx[1] * 2u);
        s32 a = (s32)(s16)DSW(DS_00107CF8 + ctx[1] * 2u);
        if (b != 0 && b != 5) {
            if ((u8)(DSB(ctx[3] + 0x90u) - 1u) > 3u)
                hit_anchor_x(ctx[1], (u32)a);
        }
    }
    DSB(ctx[3] + 0x90u) = 4u;
    *eax = 0u;
}
static void m_3a820_globs(const u32 *r, u32 *eax)      /* the B/A globs swapped */
{
    u32 ctx[6];
    u8 phase;
    fighter_ctx_swap(ctx, r[R_EBX]);
    phase = DSB(ctx[3] + 0x58u);
    if (phase == 0u) { DSB(ctx[3] + 0x58u) = 1u; *eax = 0u; return; }
    if (phase != 1u) { *eax = 0u; return; }
    actors_anim_begin(ctx[5], DSD(0x000C9058u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40400000u);
    hit_anchor_set(ctx[1], DSD(ctx[5] + 0x18u), 0u);
    DSB(ctx[3] + 0x58u) = 2u;
    {
        s32 b = (s32)(s16)DSW(DS_00107CF8 + ctx[1] * 2u);
        s32 a = (s32)(s16)DSW(DS_00107CFC + ctx[1] * 2u);
        if (b != 0 && b != 5) {
            if ((u8)(DSB(ctx[3] + 0x90u) - 1u) > 3u)
                hit_anchor_x(ctx[1], (u32)a);
        }
    }
    DSB(ctx[3] + 0x90u) = 4u;
    *eax = 0u;
}
static void m_3a820_end(const u32 *r, u32 *eax)        /* +0x90 = 1 at the end */
{
    u32 ctx[6];
    u8 phase;
    fighter_ctx_swap(ctx, r[R_EBX]);
    phase = DSB(ctx[3] + 0x58u);
    if (phase == 0u) { DSB(ctx[3] + 0x58u) = 1u; *eax = 0u; return; }
    if (phase != 1u) { *eax = 0u; return; }
    actors_anim_begin(ctx[5], DSD(0x000C9058u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40400000u);
    hit_anchor_set(ctx[1], DSD(ctx[5] + 0x18u), 0u);
    DSB(ctx[3] + 0x58u) = 2u;
    {
        s32 b = (s32)(s16)DSW(DS_00107CFC + ctx[1] * 2u);
        s32 a = (s32)(s16)DSW(DS_00107CF8 + ctx[1] * 2u);
        if (b != 0 && b != 5) {
            if ((u8)(DSB(ctx[3] + 0x90u) - 1u) > 3u)
                hit_anchor_x(ctx[1], (u32)a);
        }
    }
    DSB(ctx[3] + 0x90u) = 1u;
    *eax = 0u;
}
static void m_3a820_order(const u32 *r, u32 *eax)      /* +0x58 = 2 before the anim call */
{
    u32 ctx[6];
    u8 phase;
    fighter_ctx_swap(ctx, r[R_EBX]);
    phase = DSB(ctx[3] + 0x58u);
    if (phase == 0u) { DSB(ctx[3] + 0x58u) = 1u; *eax = 0u; return; }
    if (phase != 1u) { *eax = 0u; return; }
    DSB(ctx[3] + 0x58u) = 2u;
    actors_anim_begin(ctx[5], DSD(0x000C9058u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40400000u);
    hit_anchor_set(ctx[1], DSD(ctx[5] + 0x18u), 0u);
    {
        s32 b = (s32)(s16)DSW(DS_00107CFC + ctx[1] * 2u);
        s32 a = (s32)(s16)DSW(DS_00107CF8 + ctx[1] * 2u);
        if (b != 0 && b != 5) {
            if ((u8)(DSB(ctx[3] + 0x90u) - 1u) > 3u)
                hit_anchor_x(ctx[1], (u32)a);
        }
    }
    DSB(ctx[3] + 0x90u) = 4u;
    *eax = 0u;
}
''')

sub("port/tests/diff_runner.c", '''    { "fighter_29c78",            b_29c78,        0x00000000u },
    { "fighter_29c78@pal",        m_29c78_pal,    0x00000000u },
    { "fighter_29c78@rec",        m_29c78_rec,    0x00000000u },''', '''    { "fighter_29c78",            b_29c78,        0x00000000u },
    { "fighter_29c78@pal",        m_29c78_pal,    0x00000000u },
    { "fighter_29c78@rec",        m_29c78_rec,    0x00000000u },
    { "fighter_pose_3a820",       b_3a820,        0x00000000u },
    { "fighter_pose_3a820@side",  m_3a820_side,   0x00000000u },
    { "fighter_pose_3a820@stream", m_3a820_stream, 0x00000000u },
    { "fighter_pose_3a820@globs", m_3a820_globs,  0x00000000u },
    { "fighter_pose_3a820@end",   m_3a820_end,    0x00000000u },
    { "fighter_pose_3a820@order", m_3a820_order,  0x00000000u },''')
print("applied")
PY
```

- [ ] **Step 3: the spec.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


sub("tools/diff_verify.py", '''    ], calls=(ANIM_BEGIN, E.Call(0x38154, ("eax",))), eax_mask=0xFF,
       mutants=("@mutant", "@anim", "@mode", "@side")),
]

SPECS = [''', '''    ], calls=(ANIM_BEGIN, E.Call(0x38154, ("eax",))), eax_mask=0xFF,
       mutants=("@mutant", "@anim", "@mode", "@side")),
]

# ---- track P batch 8: the remaining entries and the triage (record 2026-10-04-reverse-p8) ---------

# 0x29CFC (record §P8.3): a one-instruction tail alias, `jmp 0x13DF0` (effects_clear). The callee
# is stubbed on both sides through its seam; the port's effects_29cfc records the arrival. Mask 0
# (the caller, 0x424E8's case 2, overwrites EAX at once).
P8_CLEAR = E.Call(0x13DF0)
P8_SPECS = [
    Spec("effects_29cfc", 0x29CFC, [
        Case("c0", {}),
    ], calls=(P8_CLEAR,), eax_mask=0, mutants=("@mutant",)),
    # 0x3A820 (record §P8.2): the 0x3A8E8 pose family's per-frame handler. EAX = slot (dead),
    # EBX = side (`mov edx,ebx` at 0x3A823); the ctx swap (0x33A10, allow) builds ctx[3] = the
    # own slot and ctx[5] = its record. Phase 0 arms +0x58; phase 1 starts the 0xC9058[char]
    # stream at 3.0 (0x40400000), re-anchors the self record and, when B[side] (0x107CFC+side*2)
    # is neither 0 nor 5 and (u8)(+0x90 - 1) > 3, snaps x to A[side] (0x107CF8+side*2); +0x90 =
    # 4 at the end. EDX is a scratch seed the raw never reads (@side catches a port that does).
    Spec("fighter_pose_3a820", 0x3A820, [
        Case("p0", {"eax": E3_SLOT, "edx": 0xDEAD, "ebx": 0},
             {**SLOT_PTRS, E3_REC + 0x51: b"\\x00", E3_REC2 + 0x51: b"\\x01",
              DS_SLOTS + 0x58: b"\\x00", DS_SLOTS + 0x59: b"\\x59", DS_SLOTS + 0x7A: b"\\x7a",
              DS_SLOTS + 0x7B: b"\\x7b", DS_SLOTS + 0x90: b"\\x90", DS_SLOTS + 0x91: b"\\x91",
              DS_SLOTS + 0x94 + 0x58: b"\\x58", DS_SLOTS + 0x94 + 0x7A: b"\\x6a",
              DS_SLOTS + 0x94 + 0x90: b"\\x70", 0x00107CF8: le32(0x43214321),
              0x00107CFC: le32(0x00030003)}),
        Case("p2", {"eax": E3_SLOT, "edx": 0xDEAD, "ebx": 1},
             {**SLOT_PTRS, E3_REC + 0x51: b"\\x00", E3_REC2 + 0x51: b"\\x01",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              DS_SLOTS + 0x58: b"\\x58", DS_SLOTS + 0x7A: b"\\x7a", DS_SLOTS + 0x90: b"\\x90",
              DS_SLOTS + 0x94 + 0x58: b"\\x02", DS_SLOTS + 0x94 + 0x59: b"\\x59",
              DS_SLOTS + 0x94 + 0x7A: b"\\x6a", DS_SLOTS + 0x94 + 0x90: b"\\x70",
              0x00107CF8: le32(0x43214321), 0x00107CFC: le32(0x00030003)}),
        Case("s0", {"eax": E3_SLOT, "edx": 0xDEAD, "ebx": 0},
             {**SLOT_PTRS, E3_REC + 0x51: b"\\x00", E3_REC2 + 0x51: b"\\x01",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              DS_SLOTS + 0x58: b"\\x01", DS_SLOTS + 0x7A: b"\\x00", DS_SLOTS + 0x90: b"\\x00",
              DS_SLOTS + 0x94 + 0x58: b"\\x58", DS_SLOTS + 0x94 + 0x7A: b"\\x6a",
              DS_SLOTS + 0x94 + 0x90: b"\\x70", 0x00107CF8: le32(0x00004321),
              0x00107CFC: le32(0x00000003)}),
        Case("s1", {"eax": E3_SLOT, "edx": 0xDEAD, "ebx": 1},
             {**SLOT_PTRS, E3_REC + 0x51: b"\\x00", E3_REC2 + 0x51: b"\\x01",
              E3_REC2 + 0x18: le32(0x28282828), E3_REC2 + 0x1C: le32(0x2C2C2C2C),
              DS_SLOTS + 0x58: b"\\x58", DS_SLOTS + 0x7A: b"\\x7a", DS_SLOTS + 0x90: b"\\x90",
              DS_SLOTS + 0x94 + 0x58: b"\\x01", DS_SLOTS + 0x94 + 0x7A: b"\\x01",
              DS_SLOTS + 0x94 + 0x90: b"\\x04", DS_SLOTS + 0x94 + 0x91: b"\\x91",
              0x00107CFA: le32(0x00004322), 0x00107CFE: le32(0x00000003)}),
        Case("q0", {"eax": E3_SLOT, "edx": 0xDEAD, "ebx": 0},
             {**SLOT_PTRS, E3_REC + 0x51: b"\\x00", E3_REC2 + 0x51: b"\\x01",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              DS_SLOTS + 0x58: b"\\x01", DS_SLOTS + 0x7A: b"\\x00", DS_SLOTS + 0x90: b"\\x00",
              0x00107CF8: le32(0x00004321), 0x00107CFC: le32(0x00000000)}),
        Case("q5", {"eax": E3_SLOT, "edx": 0xDEAD, "ebx": 0},
             {**SLOT_PTRS, E3_REC + 0x51: b"\\x00", E3_REC2 + 0x51: b"\\x01",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              DS_SLOTS + 0x58: b"\\x01", DS_SLOTS + 0x7A: b"\\x00", DS_SLOTS + 0x90: b"\\x00",
              0x00107CF8: le32(0x00004321), 0x00107CFC: le32(0x00000005)}),
    ], allow_calls=(0x33A10,), calls=(ANIM_BEGIN, ANCHOR, ANCHORX), eax_mask=0,
       mutants=("@side", "@stream", "@globs", "@end", "@order")),
]

SPECS = [''')

sub("tools/diff_verify.py",
    "] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + P7_SPECS",
    "] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + P7_SPECS + P8_SPECS")
print("applied")
PY
```

- [ ] **Step 4: the expectations (masks, kinds, the counter, the stub table).** The `P8_KINDS` sets are the planner's measured ones; the `test_each_p8_mutant_is_caught_by_what_it_breaks` test pins them.

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


sub("tools/tests/test_diff_verify.py", '''    "fighter_4b03c@voice": {"call #0"},
}''', '''    "fighter_4b03c@voice": {"call #0"},
}

# Track P batch 8 (record 2026-10-04-reverse-p8): the 0x3A820 row with its EAX mask, and what alone
# catches each of its mutants (Task 3 adds the effects_29cfc entries).
P8_MASKS = {"fighter_pose_3a820": 0}
P8_KINDS = {
    "fighter_pose_3a820@side": {"byte", "call #0", "call #1", "call #2"},
    "fighter_pose_3a820@stream": {"call #0"},
    "fighter_pose_3a820@globs": {"call #2"},
    "fighter_pose_3a820@end": {"byte"},
    "fighter_pose_3a820@order": {"call #0 memory", "call #1 memory"},
}''')
sub("tools/tests/test_diff_verify.py",
    '+ list(P6_MASKS) + list(C2_MASKS) + list(P7_MASKS)))',
    '+ list(P6_MASKS) + list(C2_MASKS) + list(P7_MASKS) + list(P8_MASKS)))')
sub("tools/tests/test_diff_verify.py",
    '+ list(P6_KINDS) + list(C2_KINDS) + list(P7_KINDS)))',
    '+ list(P6_KINDS) + list(C2_KINDS) + list(P7_KINDS) + list(P8_KINDS)))')
sub("tools/tests/test_diff_verify.py",
    '''    def test_each_p7_mutant_is_caught_by_what_it_breaks(self):
        # track P batch 7 (record 2026-10-04-reverse-p7): what alone catches each mutant
        for name, want in P7_KINDS.items():
            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
            self.assertEqual(got, want, name)
''', '''    def test_each_p7_mutant_is_caught_by_what_it_breaks(self):
        # track P batch 7 (record 2026-10-04-reverse-p7): what alone catches each mutant
        for name, want in P7_KINDS.items():
            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
            self.assertEqual(got, want, name)

    def test_each_p8_mutant_is_caught_by_what_it_breaks(self):
        # track P batch 8 (record 2026-10-04-reverse-p8): what alone catches each mutant
        for name, want in P8_KINDS.items():
            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
            self.assertEqual(got, want, name)
''')
sub("tools/tests/test_diff_verify.py", '''            "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF,
            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS,
            **P7_MASKS})''', '''            "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF,
            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS,
            **P7_MASKS, **P8_MASKS})''')
print("applied")
PY
```

(Task 3 adds the `0x13DF0: ()` stub-table row and the `192/552/69/158` counter line; run this step's scripts after Task 3's if applying by hand out of order.)

- [ ] **Step 5: the unit check.** Append `p8_pose_seed`/`p8_check_3a820`/`test_p8_3a820` at the end of `test_fight.c` and register it.

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


block = 'int test_p7_4b03c(void)         { return u6b_run(p7_check_4b03c); }\n' + '''
/* ---- §P8.2: the 0x3A8E8 family's pose handler 0x3A820 ------------------- */

/* pose_handler_seed with the 0x3A8E8 family's fields: char 0 (0xC9058[0] =
 * 0xE7398), the B/A globs at 0x107CF8/0x107CFC (the setter's latch) and the
 * +0x90 = 4 target. Every seeded value differs from its post-condition. */
static void p8_pose_seed(u32 s0, u32 s1, u32 r0, u32 r1)
{
    pose_chain_setup(s0, s1, r0, r1);

    DSB(s0 + 0x58u) = 1;                     /* phase 1 */
    DSB(s0 + 0x7Au) = 0;                     /* char 0: the 0xC9058 table */
    DSB(s0 + 0x90u) = 0;                     /* (u8)(0 - 1) > 3: the snap arm */
    DSD(s0 + 0x2Cu) = 0x1234;
    DSW(r0 + 0x56u) = 0;                     /* the pset index */
    DSD(r0 + 8u) = 0xDEADBEEFu;              /* the stream sentinel */
    DSB(r0 + 0x52u) = 0x7F;                  /* the animation variable */
    DSD(r0 + 0x24u) = 0xDEADBEEFu;           /* the frame-hold sentinel */
    DSD(r0 + 0x18u) = 0x5678;                /* the snap's x sentinel */
    DSW(FIGHT_ACTORS) = 0xFFFFu;             /* the pset id sentinel */
    DSD(r0 + 0x1Cu) = 0xDEADBEEFu;           /* hit_anchor_set's y sentinel */
    DSD(r1 + 0x1Cu) = 0xDEADBEEFu;           /* the other record: untouched */
    DSB(s1 + 0x52u) = 0x07;
    DSW(0x00107CF8u) = 0;                    /* A[0] */
    DSW(0x00107CFCu) = 0;                    /* B[0] = 0: the gate closed */
}

static void p8_check_3a820(void)
{
    u32 s0 = DS_001077B0;
    u32 s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS;
    u32 r1 = FIGHT_RECS + 0x100u;
    u16 sv_78f6 = DSW(DS_001078F6);

    /* phase 0 arms +0x58 (the seed's 0 differs from the post-condition 1). */
    p8_pose_seed(s0, s1, r0, r1);
    DSB(s0 + 0x58u) = 0;
    fighter_pose_3a820(s0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 1);

    /* any +0x58 above 1 returns before touching anything (seeded sentinels). */
    p8_pose_seed(s0, s1, r0, r1);
    DSB(s0 + 0x58u) = 3;
    DSD(r0 + 8u) = 0xCAFEF00Du;
    DSB(s0 + 0x90u) = 0x55;
    fighter_pose_3a820(s0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 3);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)0xCAFEF00Du);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 0x55);

    /* phase 1 starts the char-0 stream, re-anchors the self record, sets
     * +0x58 = 2 and +0x90 = 4; B[0] = 0 closes the snap. */
    p8_pose_seed(s0, s1, r0, r1);
    fighter_pose_3a820(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000E7398);    /* 0xC9058[0] */
    CHECK_EQ_INT((int)DSD(r0 + 0x20u), 0x40400000); /* 3.0f */
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSB(r0 + 0x52u), 0);
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS), 0x1099);   /* the stream's first id */
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 4);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0);          /* hit_anchor_set */
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);     /* B[0] = 0: no snap */

    /* B[0] = 3, A[0] = 0x4321 and +0x90 = 0 open the snap; DS_001077A8[0] = 0
     * and DS_00100AF0[0] = s0+0x20 make the record-x path's calls inert, as in
     * the 0x3A43C check. */
    p8_pose_seed(s0, s1, r0, r1);
    DSD(DS_001077A8) = 0;
    DSD(DS_00100AF0) = DSD(s0 + 0x20u);
    DSD(DS_00100AB0) = 0x1000;
    DSW(0x00107CFCu) = 3;
    DSW(0x00107CF8u) = 0x4321;
    fighter_pose_3a820(s0, 0u);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x4321);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x4321 - 0x1000);

    /* +0x90 in 1..4 is the table arm (all entries 0x3A8D6): no snap. */
    p8_pose_seed(s0, s1, r0, r1);
    DSB(s0 + 0x90u) = 4;
    DSW(0x00107CFCu) = 3;
    DSW(0x00107CF8u) = 0x4321;
    fighter_pose_3a820(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);

    /* B[0] = 5 closes the snap. */
    p8_pose_seed(s0, s1, r0, r1);
    DSW(0x00107CFCu) = 5;
    DSW(0x00107CF8u) = 0x4321;
    fighter_pose_3a820(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);

    /* the B/A words are the self side's: B[1] = 3 with A[1] = 0x4321 leaves
     * the gate closed (B[0] = 0). */
    p8_pose_seed(s0, s1, r0, r1);
    DSW(0x00107CFEu) = 3;
    DSW(0x00107CFAu) = 0x4321;
    fighter_pose_3a820(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);

    /* side 1: the own record r1 and the side-1 globs 0x107CFA/0x107CFE. */
    p8_pose_seed(s0, s1, r0, r1);
    DSB(s1 + 0x58u) = 1;                    /* the side-1 phase */
    DSB(s1 + 0x7Au) = 1;
    DSW(0x00107CFEu) = 3;
    DSW(0x00107CFAu) = 0x4321;
    fighter_pose_3a820(s1, 1u);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x000E401E);    /* 0xC9058[1] */
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 2);
    CHECK_EQ_INT((int)DSB(s1 + 0x90u), 4);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 1);          /* the other slot untouched */

    /* the wiring: 0x3531C case 10 resolves slot+0x10 and calls it with the
     * raw's (EAX = slot, EBX = side). */
    CHECK(fn_resolve(0x3A820u) == (void (*)(void))fighter_pose_3a820,
          "actors_init registered 0x3A820 as fighter_pose_3a820");
    if (fn_resolve(0x3A820u) == NULL)
        fn_register(0x3A820u, (void (*)(void))fighter_pose_3a820);
    p8_pose_seed(s0, s1, r0, r1);
    DSB(s0 + 0x53u) = 0x0A;
    DSD(s0 + 0x10u) = 0x0003A820u;
    DSW(DS_001078F6) = 0;
    fighter_state_3531c(0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000E7398);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 2);

    DSW(DS_001078F6) = sv_78f6;
}

int test_p8_3a820(void)         { return u6b_run(p8_check_3a820); }
'''
sub("port/tests/test_fight.c", 'int test_p7_4b03c(void)         { return u6b_run(p7_check_4b03c); }\n', block)
sub("port/tests/test.h", '''    X(test_p7_4b03c) \\
    X(test_virtual_clock) \\''', '''    X(test_p7_4b03c) \\
    X(test_p8_3a820) \\
    X(test_virtual_clock) \\''')
print("applied")
PY
cmake --build build 2>&1 | tail -1
```

- [ ] **Step 6: the row, alone and with its mutants.**

```bash
python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE --image /tmp/pr_p8_diffimg.bin --function fighter_pose_3a820
```

Expected:

```
| function | original | cases | blocks hit/total | verdict | callees (each by its own check) |
|---|---|---|---|---|---|
| fighter_pose_3a820 | 0x3A820 | 6 | 12/12 | VERIFIED | 188AC stub unverified, 188DC stub unverified, 2BC30 stub unverified, 33A10 allow unverified |
diff-verify: 1/1 functions VERIFIED; 0 named gaps; 0/1 rows with callees closed (0 have none). ...
```

(`--function` runs the row alone, so its callees' rows read unverified; the self-check in Task 6 reports them VERIFIED.) Then `PR_ORACLE_REQUIRED=1 ./build/run_tests` prints `all checks passed` with `test_p8_3a820` included.

- [ ] **Step 7: commit.** Stage the named files and commit `fighter: 0x3A820 (the 0x3A8E8 pose family's handler) with its differential row`.

---

### Task 3: `0x29CFC` (the effects-clear tail) and the `effects_clear` seam

**Files:** modify `port/src/game/effects.c` (the seam in `effects_clear`; `effects_29cfc` after it), `port/src/game/effects.h` (after `effects_clear`'s declaration), `port/src/game/flow.c` (`game_mode_13_step` case 2), `port/src/game/actors.c` (`actors_init`, after the `0x29B74` registration), `port/tests/diff_runner.c` (after the Task 2 mutants; the `k_bindings` rows), `tools/tests/test_diff_verify.py` (the stub table and the counter line), `port/tests/test_game.c` (after `test_effects`), `port/tests/test.h`, the E2 table.

**Interfaces:** produces `void effects_29cfc(void)`; `b_29cfc`/`m_29cfc`; `effects_clear` opens with `PR_SEAM0(0x13DF0u)`; `E.Call(0x13DF0)` is the P8 row's stub. Consumes `DS_000FCCE0`, `DS_0009AF3C`, `DS_0009AF3D`, `effects_clear`. Record §P8.3.

- [ ] **Step 1: the seam, the wrapper, the registration and the call site.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


sub("port/src/game/effects.c", '''/* 0x13DF0. */
void effects_clear(void)
{
    /* PORT: the original assumes the pool was built;''', '''/* 0x13DF0. */
void effects_clear(void)
{
    PR_SEAM0(0x13DF0u);
    /* PORT: the original assumes the pool was built;''')
sub("port/src/game/effects.c", '''    DSB(DS_0009AF3D) = 0;
    DSB(DS_0009AF3C) = 0;
}

/* DS_0009AF3D. */''', '''    DSB(DS_0009AF3D) = 0;
    DSB(DS_0009AF3C) = 0;
}

/* 0x29CFC — record §P8.3. A one-instruction tail alias: `jmp 0x13DF0`
 * (effects_clear; the dword at 0x29CFC's only reference is the `call` at
 * 0x4255D in 0x424E8, game_mode_13_step's case 2). Ported as the wrapper so
 * the original's call graph and fn_resolve(0x29CFC) hold. */
void effects_29cfc(void)
{
    effects_clear();
}

/* DS_0009AF3D. */''')
sub("port/src/game/effects.h", '''void effects_clear(void);
''', '''void effects_clear(void);
/* 0x29CFC (record §P8.3): the `jmp 0x13DF0` tail alias game_mode_13_step's
 * case 2 calls. */
void effects_29cfc(void);
''')
sub("port/src/game/flow.c",
    "        effects_clear();                                /* 0x4255D 0x29CFC -> 0x13DF0 */",
    "        effects_29cfc();                                /* 0x4255D 0x29CFC -> 0x13DF0 */")
sub("port/src/game/actors.c", '''    fn_register(0x29B74u, frontend_darken_all);
''', '''    fn_register(0x29B74u, frontend_darken_all);
    /* PORT: record §P8.3. The effects-clear tail alias 0x29CFC (`jmp
     * 0x13DF0`), which game_mode_13_step's case 2 calls at 0x4255D (the
     * dword at 0x29CFC's only reference). */
    fn_register(0x29CFCu, (void (*)(void))effects_29cfc);
''')
print("applied")
PY
cmake --build build 2>&1 | tail -1
```

- [ ] **Step 2: the binding, the mutant and the expectations.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


sub("port/tests/diff_runner.c", '''static void b_29c78(const u32 *r, u32 *eax)            { b_anim(0x29C78u, r, eax); }
''', '''static void b_29c78(const u32 *r, u32 *eax)            { b_anim(0x29C78u, r, eax); }
static void b_29cfc(const u32 *r, u32 *eax)            { (void)r; effects_29cfc(); *eax = 0u; }
static void m_29cfc(const u32 *r, u32 *eax)            /* the clear skipped */
{
    (void)r;
    *eax = 0u;
}
''')
sub("tools/tests/test_diff_verify.py",
    '''# Track P batch 8 (record 2026-10-04-reverse-p8): the 0x3A820 row with its EAX mask, and what alone
# catches each of its mutants (Task 3 adds the effects_29cfc entries).
P8_MASKS = {"fighter_pose_3a820": 0}
P8_KINDS = {
    "fighter_pose_3a820@side": {"byte", "call #0", "call #1", "call #2"},''',
    '''# Track P batch 8 (record 2026-10-04-reverse-p8): its two rows with their EAX masks, and what alone
# catches each of its mutants.
P8_MASKS = {"fighter_pose_3a820": 0, "effects_29cfc": 0}
P8_KINDS = {
    "effects_29cfc@mutant": {"call #0"},
    "fighter_pose_3a820@side": {"byte", "call #0", "call #1", "call #2"},''')
sub("port/tests/diff_runner.c", '''    { "fighter_29c78@rec",        m_29c78_rec,    0x00000000u },''',
    '''    { "fighter_29c78@rec",        m_29c78_rec,    0x00000000u },
    { "effects_29cfc",            b_29cfc,        0x00000000u },
    { "effects_29cfc@mutant",     m_29cfc,        0x00000000u },''')
sub("tools/tests/test_diff_verify.py",
    '                                 0x18540: (), 0x18350: ("edx",), 0x18788: (), 0x1881C: (),',
    '                                 0x13DF0: (), 0x18540: (), 0x18350: ("edx",), 0x18788: (), 0x1881C: (),')
sub("tools/tests/test_diff_verify.py", '''        self.assertIn("diff-verify: 190/190 functions VERIFIED; 546/546 mutants detected; 1 named gaps; "
                      "68/156 rows with callees closed (34 have none).", out.getvalue())''',
'''        self.assertIn("diff-verify: 192/192 functions VERIFIED; 552/552 mutants detected; 1 named gaps; "
                      "69/158 rows with callees closed (34 have none).", out.getvalue())''')
print("applied")
PY
```

- [ ] **Step 3: the unit check.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


block = '''    CHECK_EQ_INT((int)hiscore_rank_probe(999999u, 3u), (int)0xFFFFFFFFu);

    memcpy(mem + DS_00105D88, sreg, sizeof sreg);
}

/* §P8.3: 0x29CFC is the `jmp 0x13DF0` tail alias effects_clear (the dword at
 * 0x29CFC's only reference is game_mode_13_step's 0x4255D call). On a built,
 * empty pool the clear sets and clears the guard bytes; on the unbuilt pool
 * effects_clear's guard returns and the sentinels survive. Both seeds differ
 * from the post-conditions. */
int test_p8_29cfc(void)
{
    int before = g_failures;
    u32 sv_head = DSD(DS_000FCCE0);
    u32 sv_head4 = DSD(DS_000FCCE4);
    u8 sv_3c = DSB(DS_0009AF3C);
    u8 sv_3d = DSB(DS_0009AF3D);

    DSD(DS_000FCCE0) = DS_000FCCE0;          /* a built, empty list */
    DSD(DS_000FCCE4) = DS_000FCCE0;
    DSB(DS_0009AF3C) = 0x5Au;
    DSB(DS_0009AF3D) = 0x5Au;
    effects_29cfc();
    CHECK_EQ_INT((int)DSB(DS_0009AF3D), 0);
    CHECK_EQ_INT((int)DSB(DS_0009AF3C), 0);

    /* The pool head zeroed: effects_clear's guard returns before touching the
     * count bytes. A clear that ran anyway would zero the 0x5A sentinels. */
    DSD(DS_000FCCE0) = 0;
    DSB(DS_0009AF3C) = 0x5Au;
    DSB(DS_0009AF3D) = 0x5Au;
    effects_29cfc();
    CHECK_EQ_INT((int)DSB(DS_0009AF3C), 0x5A);
    CHECK_EQ_INT((int)DSB(DS_0009AF3D), 0x5A);

    CHECK(fn_resolve(0x29CFCu) == (void (*)(void))effects_29cfc,
          "actors_init registered 0x29CFC as effects_29cfc");
    if (fn_resolve(0x29CFCu) == NULL)
        fn_register(0x29CFCu, (void (*)(void))effects_29cfc);

    DSD(DS_000FCCE0) = sv_head;
    DSD(DS_000FCCE4) = sv_head4;
    DSB(DS_0009AF3C) = sv_3c;
    DSB(DS_0009AF3D) = sv_3d;
    return g_failures - before;
}
'''
sub("port/tests/test_game.c",
    '''    CHECK_EQ_INT((int)hiscore_rank_probe(999999u, 3u), (int)0xFFFFFFFFu);

    memcpy(mem + DS_00105D88, sreg, sizeof sreg);
}
''', block)
sub("port/tests/test.h", '''    X(test_p8_3a820) \\
    X(test_virtual_clock) \\''', '''    X(test_p8_3a820) \\
    X(test_p8_29cfc) \\
    X(test_virtual_clock) \\''')
print("applied")
PY
cmake --build build 2>&1 | tail -1
```

- [ ] **Step 4: the row and the tests.**

```bash
python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE --image /tmp/pr_p8_diffimg.bin --function effects_29cfc
```

Expected:

```
| function | original | cases | blocks hit/total | verdict | callees (each by its own check) |
|---|---|---|---|---|---|
| effects_29cfc | 0x29CFC | 1 | 1/1 | VERIFIED | 13DF0 stub unverified |
diff-verify: 1/1 functions VERIFIED; 0 named gaps; 0/1 rows with callees closed (0 have none). ...
```

Then `PR_ORACLE_REQUIRED=1 ./build/run_tests` prints `all checks passed`.

- [ ] **Step 5: the self-check and the E2 table (0x29CFC is a counted target).**

```bash
python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE --image /tmp/pr_p8_diffimg.bin --self-check | tail -1
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_p8_e2.bin
python3 tools/entry_triage.py --image /tmp/pr_p8_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
python3 tools/entry_triage.py --image /tmp/pr_p8_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt --check docs/superpowers/plans/2026-10-01-reverse-e2-triage.md && echo E2-CHECK-OK
python3 -m unittest tools.tests.test_entry_triage
```

Expected self-check: `diff-verify: 192/192 functions VERIFIED; 552/552 mutants detected; 1 named gaps; 69/158 rows with callees closed (34 have none). ...`; E2: `targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30` and `voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere`; the E2 suite `Ran 44 tests ... OK`. The table diff versus the committed one is exactly three lines: the `| other | 3 | 54 |` batch row becomes `| other | 2 | 55 |`, the `| allow-list | 1 |` readiness row disappears, and the `| 29CFC | ... | no | other |` row becomes `yes`. (Task 4 changes the tool's header sentence and adds the resolutions section and regenerates again.)

- [ ] **Step 6: commit.** `effects: 0x29CFC (the effects-clear tail), its seam and differential row; E2 table regenerated`.

---

### Task 4: the triage (the 14 non-port verdicts) and the E2 resolutions section

**Files:** modify `tools/entry_triage.py` (the header sentence; the resolutions section at the end of `render_table`), `docs/superpowers/plans/2026-10-04-reverse-p8-derivations.md` (the record's §P8.4/§P8.5, written in Task 6 or now), the E2 table (regenerated).

**Interfaces:** the new section and the regenerated table are the triage's table form; the record carries the evidence. Verdicts (record §P8.4): the 7 untrusted entries are blocks of ported functions (`19AD4` in `0x19B90`; `19DD5` in `0x19D34`; `26163`/`26226` in `0x260BC`/`0x26194`; `34962` in the `0x348xx` health sync; `45444` in `0x452E4`; `49078` in `0x48F98`); the 4 after-table ends (`2EE3C 37E40 3A3FC 4AEC4`) have no rel32 and no dword anywhere in the image and are dead code, not ported; `1D2D0` and the six `2D3FC..2D48C` addends are data. §P8.5: of the 15 voice sites with no body, 8 sit in ported bodies and 7 in unreferenced dead bodies.

- [ ] **Step 1: the tool's resolutions section.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


sub("tools/entry_triage.py", '''           "Outside this list, a named gap: the 8 unexamined immediates of record §E2.11 (functions after "
           "inline data, reached only by an immediate in Ghidra code; track P takes them as follow-up input).",
''', '''           "Outside this list, the §E2.11 immediates and the voice sites that have no body are resolved by",
           "track P batch 8 (record `2026-10-04-reverse-p8-derivations.md` §P8.4-§P8.5 and the 'Track P",
           "batch 8 resolutions' section below).",
''')
sub("tools/entry_triage.py", '''    for v in voice:
        out.append("| %05X | %s | %s | %s |" % (v["site"], "%05X" % v["entry"] if v["entry"] else "-",
                                               v["kind"], "yes" if v["ported"] else "no"))
    return "\\n".join(out) + "\\n"''', '''    for v in voice:
        out.append("| %05X | %s | %s | %s |" % (v["site"], "%05X" % v["entry"] if v["entry"] else "-",
                                               v["kind"], "yes" if v["ported"] else "no"))
    out += ["", "## Track P batch 8 resolutions (record `2026-10-04-reverse-p8-derivations.md` §P8.4-§P8.5)", "",
            "The §E2.11 immediates, the untrusted entries and the voice sites with no body are resolved by",
            "track P's last batch. The verdicts below are documentation: the table's classes and counts stay",
            "the tool's mechanical ones (a block of a ported function is not a port at that address).", "",
            "| candidate | verdict | evidence (record §P8.4/§P8.5) |",
            "|---|---|---|",
            "| 3A820 | ported | the 0x3A8E8 pose family's +0x10 handler (stored at 0x3A91E); row `fighter_pose_3a820` |",
            "| 1D2D0 | data | a struct of strings/pointers at 0x1D2C0..0x1D2EF stored to 0x10740C by 0x2F9CC (0x2FA02), read at +4 (0x2CACC) and +0xC (0x2F98C); the bytes there are not code |",
            "| 2D3FC | data | an embedded word-table base: `add ebx,0x2d3fc` at 0x2DB6F/0x2DBE3/0x2DCD4 (stride 8) |",
            "| 2D414 | data | `add ebx,0x2d414` at 0x2E94E (stride 0x10) |",
            "| 2D444 | data | the base at 0x2D519/0x2E0DD/0x2E19A/0x2E050 |",
            "| 2D45C | data | `mov ecx,0x2d45c` at 0x2DF98 |",
            "| 2D474 | data | `mov edx,0x2d474` at 0x2DEA5 |",
            "| 2D48C | data | `mov [esp+8],0x2d48c` at 0x2D4F7 |",
            "| 19AD4 | block of 19B90 | `jbe 0x19ad4` at 0x19C09 in the ported debris walker |",
            "| 19DD5 | block of 19D34 | the `ja` default arm at 0x19D37 of the ported service-menu getter |",
            "| 26163 26226 | blocks of 260BC 26194 | the state-2 arms (`je` at 0x260CC / 0x261A4) of the ported bonus-card steps |",
            "| 34962 | block of the 348xx health sync | the char > 6 / char 1 shared tail (`ja` at 0x3486D/0x348CB; table 0x3479C) |",
            "| 45444 | block of 452E4 | the `ja` default arm at 0x452FC of the ported mode handler |",
            "| 49078 | block of 48F98 | the phase-1 arm (`jbe` at 0x48FB7) of the ported actor_type_2d_update |",
            "| 2EE3C 37E40 3A3FC 4AEC4 | dead code | after-table entries with no rel32 and no dword anywhere in the image (and no Ghidra function); not ported |",
            "| 228EF 2292B 39EDA 45F8C 48518 48548 4B0B4 4B0BE | voiced by ported bodies | the site sits in a ported function's body (record §P8.5) |",
            "| 11A3D 11C38 1599B 295FD 3D730 41880 475D9 | dead code (named gap) | the site's body (0x11A30, 0x11BF8, 0x15960, 0x295C0, 0x3D6E0, 0x41878, 0x475C0) has no reference anywhere; not ported |",
            ""]
    return "\\n".join(out) + "\\n"''')
print("applied")
PY
```

- [ ] **Step 2: regenerate the table and run the E2 suite.**

```bash
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_p8_e2.bin
python3 tools/entry_triage.py --image /tmp/pr_p8_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
python3 tools/entry_triage.py --image /tmp/pr_p8_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt --check docs/superpowers/plans/2026-10-01-reverse-e2-triage.md && echo E2-CHECK-OK
python3 -m unittest tools.tests.test_entry_triage
grep -n 'Track P batch 8 resolutions' docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
```

Expected: `E2-CHECK-OK`; `Ran 44 tests ... OK`; the section header present. The regenerated table's diff versus Task 3's: the header paragraph and the new section only (the counts are unchanged) — plus, if Task 3 has not run yet, the `other`/`29CFC` rows move here instead; run this after Task 3 as the plan orders.

- [ ] **Step 3: commit.** `docs: P8 triage resolutions in the E2 table (the 14 verdicts and the voice sites)`.

---

### Task 5: the host-owned/deferred resolutions (`0x1BDF4`, `0x10604`)

**Files:** modify `tools/port_classification.txt` (two rows).

**Interfaces:** `1BDF4 host-owned record-§K9.5 ...` and `10604 deferred record-§50-E ...`; the README/`port_progress.py` figures do not move (neither address is a Ghidra `FN_` in the committed export — measured: `771 1203 64` / `731 731 100` before and after). Record §P8.6.

- [ ] **Step 1: the rows.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


sub("tools/port_classification.txt",
    "10570 deferred record-§50-E\n10610 deferred record-§50-E\n",
    "10570 deferred record-§50-E\n10604 deferred record-§50-E (the 250 Hz callback 0x10610 registers)\n10610 deferred record-§50-E\n")
sub("tools/port_classification.txt",
    "1BBAC host-owned record-§50-C\n1C308 host-owned record-§50-D\n",
    "1BBAC host-owned record-§50-C\n1BDF4 host-owned record-§K9.5 (tick model config.c; callees 1BBAC §50-C and 2D62C §K9.5 host-owned)\n1C308 host-owned record-§50-D\n")
print("applied")
PY
python3 tools/port_progress.py
```

Expected: `771 1203 64` and `731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)` (the two rows are not `FN_` addresses, so the skip set's size is unchanged).

- [ ] **Step 2: commit.** `docs: P8 host-owned/deferred resolutions (1BDF4, 10604)`.

---

### Task 6: closure — docs, the record's results, the full gate

**Files:** modify `docs/PROGRESS.md` (an appended P8 paragraph), `docs/superpowers/plans/2026-10-04-reverse-p8-derivations.md` (the whole record, its §P8.8 results filled with the measurements below), `AGENTS.md` (only if a pinned list moves — none does: no gp pin, no counter sentence), the Makefile (no `GP_*` provenance changes; verify with `git diff -- Makefile` empty).

**Interfaces:** consumes every earlier task's measurement; produces the closure commit.

- [ ] **Step 1: the record's results.** Fill the record's §P8.8 with the per-commit counters measured here (the tables in the record are seeded from the planner's prototype and corrected by the implementation's Task reports, as P7 §P7.10 does) and append the PROGRESS paragraph: the two ports, the seam, the triage verdicts (the 14 + voice sites + host-owned pair), the counters, and the named gaps.

- [ ] **Step 2: the full gate.**

```bash
T=p8; make verify SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att \
  FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 \
  GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md E2_IMAGE=/tmp/pr_${T}_e2.bin \
  > /tmp/pr_p8_final.log 2>&1; echo "EXIT=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_p8_final.log \
  | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
grep -E '^diff-verify:|^entry-triage: (targets|voice)|ratchet N|all checks passed' /tmp/pr_p8_final.log | sed 's/^gp_compare: //;s/^gp_keys: //'
make audio-render AUDIO_WAV=/tmp/pr_p8.wav >/dev/null 2>&1
cmp /tmp/pr_p8.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-SAME
python3 tools/port_progress.py
```

Expected (the planner's prototype full-gate outputs, `EXIT=0`, ~35 min): `ORACLES-EQUAL`; `WAV-SAME`; `771 1203 64` / `731 731 100`; `all checks passed` for every run; `symbols.h` regenerated byte-identically; the Python diff-verify suite `Ran 103 tests ... OK` and the E2 suite 44; and

```
diff-verify: 192/192 functions VERIFIED; 552/552 mutants detected; 1 named gaps; 69/158 rows with callees closed (34 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
```

with every gp ratchet at its pin (all unchanged from Task 1's base, measured):

```
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
gp-u9-win: frames: first unexplained 346, ratchet N 346 ok
gp-u9-win: trace: 0 differing through 3502; ratchet N 3503 ok
gp-u9-win: path: 0 not reproduced through 7; ratchet N 8 ok
gp-u9-win: win: 0 differing through 3502; ratchet N 3503 ok
gp-u10-ending: frames: first unexplained 331, ratchet N 331 ok
gp-u10-ending: trace: 0 differing through 9953; ratchet N 9954 ok
gp-u10-ending: path: 0 not reproduced through 29; ratchet N 30 ok
gp-u10-ending: win: 0 differing through 9953; ratchet N 9954 ok
```

- [ ] **Step 3: the record's closure table.** Correct every counter in the record to the commit that produced it (the per-task reports), note any raw-over-plan correction with its address, and leave the named limits of §P8.7 standing.

- [ ] **Step 4: commit.** `docs: P8 closure (track P complete): 2 ports, the triage resolutions, counters`.

