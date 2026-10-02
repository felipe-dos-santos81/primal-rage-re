# P1: the finisher entries and their +0x0C callbacks (track P, batch 1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the six unported finisher entries (spec O4: `0x1567C 0x15908 0x23BF8 0x23EC0 0x402FC 0x45D14`), the five slot +0x0C callbacks they store (`0x15584 0x1579C 0x23D38 0x23B68 0x401D4`, three of which E2's list cannot see) and `0x401D4`'s unported callee `0x38034`, each verified against the original's bytes by the differential harness with its callees stubbed (E3 §E3.10), every block hit; give the newly seamed callee `0x1A570` its own row; regenerate the E2 table in each commit that ports an E2 target; and fix the P track's batch sequence.

**Architecture:** One C function per original function, appended to `port/src/game/fighter.c` beside the already-ported finisher `0x45C10`, registered in `actors_init` so `0x379C4` (`fn_resolve(DSD(DS_001078E8))`) and `0x3531C` case 7 (`fn_resolve(DSD(slot + 0x0C))`) reach them. Each direct callee carries a `PR_SEAM`/`PR_SEAM_RET` (three new: `0x1A570`, `0x2A17C`, `0x188AC`; `0x38034` is born with one); each function has a binding and a call-list-only mutant in `port/tests/diff_runner.c`, a `Spec` in `tools/diff_verify.py` (`P1_SPECS`), a seeded unit check in `port/tests/test_fight.c`, and its row in the self-check counter.

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and `capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §3 decision 1 ("port all of O3-O6, whether or not a capture reaches them"), §4 track P ("port batches, each function verified by E"), §5.1-§5.3 (the contract, calls out, coverage), §6 ("P: each ported function has a verification row; unhit blocks and non-emulable instructions named; counters and README percentage updated"), §7 (named gaps).

**Derivation record:** `docs/superpowers/plans/2026-10-02-reverse-p1-derivations.md` (§P1.1 the target list today, §P1.2 what E2's list does not hold, §P1.3 the roadmap, §P1.4 the batch choice, the callers' conventions, the EAX masks and the callee declarations, §P1.5-§P1.9 each function from the bytes, §P1.10 decisions and named gaps, §P1.11 results). Recipe: `2026-10-01-reverse-e3-derivations.md` §E3.10.

**What the planner ran (scratch, 2026-10-01/02, `main` at `834b703`, image sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; scratch paths in place of `/tmp`):** a prototype with every change of this plan, on which `make verify` ran every step (14 min 29 s; the 45 oracle lines equal to the base, the gameplay ratchets green on the captures present) with `symbols.h` regenerating byte-identically and the `make audio-render` WAV identical; then a replay: a fresh copy of `834b703`, Tasks 2-5 applied in order with exactly the scripts below, every red and green output and every mutation result quoted below taken from it, and `make verify` plus `make audio-render` on its final state (quoted in Task 6). Every code block below is a file the replay ran, byte for byte.

## Decisions needed from the user

**Decided by the user on 2026-10-02:** all three recommendations accepted — D1 batch 1 is the finisher cluster; D2 the span dispatchers/writers are a named gap backed by the pixel oracles; D3 the already-ported callees get rows in two verification-only batches, C1 (after P3) and C2 (after P6).

1. **D1: batch 1 is the finisher cluster.** **Recommendation: yes** (record §P1.4): it is the one P item with a deviation reached in real play (since U0 the port arms the finisher for characters 0, 3, 4, 6 and then starts `0xC9260[char]` where the raw runs the entry), every callee but three small ones is already seamed, and the entries store +0x0C callbacks, so the batch takes those too (three of them are outside E2's list). Alternatives: (a) the four seamed callees' own rows first (`0x2C3FC` 100 blocks, `0x2AE14` 33 blocks with an indirect callback, `0x2BC30`, `0x3C480`): verification only, no port, raises "rows with callees closed"; (b) the seven unported direct callees (`0x2BDB8 0x2BDE8 0x22404 0x23960 0x3A9D8 0x48170 0x38034`): they unblock eight callers. Cost if wrong: the order of batches changes, not their content; this batch's commits stand either way.
2. **D2: the span code (E2's decision D2).** E2's D2 has P "verify the six dispatchers ... with the writers allow-listed"; but allow-listed means both sides run each writer, and the port has no C function per dispatcher or writer (`sprite.c` blits a span as one routine), so a dispatcher row has nothing to run on the port side (record §P1.10). **Recommendation: make the six dispatchers and the 231 writers a named gap whose evidence is the pixel-exact frame oracles** (every title, attract, front-end, K11 and gameplay frame the captures reach goes through them), and drop the "span" batch. Alternatives: (b) port the six dispatchers as C that dispatch through `fn_resolve` to unregistered writer addresses (compared by address only, E3 decision D3): verification-only code the game never runs; (c) port the 231 writers one by one. Cost if wrong: (a) leaves the span code verified only where a capture draws; (b) adds about 2 400 instructions of C used only by `diffrun`; (c) is the largest batch of the track.
3. **D3: when the callee rows come.** A P row's claim holds "given each stubbed callee behaves as declared" until that callee has its own VERIFIED row (E3 §E3.4); after this batch the counter reads `1/17 rows with callees closed (9 have none)`. **Recommendation: two verification-only batches, C1 after P3 (the callees P1-P3 stub: about 25) and C2 after P6 (the rest, about 20), not a few rows inside each porting batch.** Cost if wrong: if the user wants closure as P goes, every porting batch grows by its new callees' rows (P2: 8, P3: 12, ...), and a large callee (`0x2C3FC`, `0x2AE14`) lands in the first batch that seams it.

## The P-track roadmap

From record §P1.3 (members, sizes and callees there). Each line is one plan and one subagent-driven run; each batch ports only functions whose direct callees are ported, seamed or allowed, or ported earlier in the same batch.

| batch | ports | what |
|---|---|---|
| **P1** (this plan) | 12 (+ 1 callee row) | the six finisher entries, their five +0x0C callbacks, `0x38034` |
| P2 | 19 | move callbacks `0x14EF8..0x3DCEC` and the callbacks they store (`0x2116C 0x211F0 0x212CC 0x22510 0x22588`, with `0x22404`) |
| P3 | 24 | move callbacks `0x475EC..0x489A0` and the callbacks they store (with `0x48170`, and the after-table `0x47E9C`, `0x4844C`) |
| C1 | about 25 rows | verification only: the ported callees P1-P3 stub (`0x2C3FC 0x2BC30 0x2AE14 0x3C480 0x188AC 0x2A17C 0x34D8C 0x18BD4 ...`) |
| P4 | 24 | animation targets: the six leaves and the ones whose callees are all seamed (with `0x15510`, after a tail `jmp`) |
| P5 | 16 | animation targets with 5-11 blocks on seamed callees (with `0x24454`) |
| P6 | 18 | animation targets that need new seams |
| C2 | about 20 rows | verification only: the rest of the stubbed callees |
| P7 | 14 | the five unported direct callees with their eight callers, and the targets outside E2 (`0x23AE0 0x29C78 0x4B03C 0x213F0`) |
| P8 | 16 + triage | `0x10604 0x29CFC 0x1BDF4`, the seven untrusted entries, `0x1D2D0`, the after-table `0x2EE3C 0x37E40 0x3A3FC 0x3A820 0x4AEC4` (reach evidence first), the six `0x2D3FC..0x2D48C` addends (data until shown otherwise), the 15 voice sites still in no body |
| span | 0 | decision D2 |

Total: **8 porting batches, 143 functions**, 2 verification-only batches (about 45 rows), and the span decision. `port_progress.py` counts only Ghidra `FN_` functions (the committed `prage.functions.csv`): none of the 143 is one (each is a non-Ghidra entry by construction, E2 §E2.1), so the raw figure stays `771 1203 64` through P and the README percentage does not move; P's progress shows in the E2 table's ported column and the diff-verify counter instead. The closeout restates the counters.

## Global Constraints

Copied verbatim from their sources:

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- Spec §8: "`make verify` is the gate after every task." User ruling (2026-10-01): the per-task gate is the task's tests, its oracle and `make diff-verify` (plus `make entry-triage` when a target is ported); the full `make verify` runs at the baseline (Task 1), the final task (Task 6) and before the merge. Common brief: the 45 oracle lines (`grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>`) must equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; the `make audio-render` WAV must be byte-identical to `.superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav`; `python3 tools/port_progress.py` prints `771 1203 64` and `731 731 100` (this plan ports no Ghidra function, so the figures do not move: roadmap section).
- AGENTS.md: "**The E2 target list is a gate** ... A commit that ports a target (a new `/* 0xADDR` header or `fn_register`) regenerates the table in the same commit (decision D3) ... A conflict in that file is resolved by regenerating it after the rebase, never by merging lines by hand."
- AGENTS.md: "One C function per original function, header comment `/* 0xADDR — spec section */`." / "Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment styles in `port/src`." / "Code addresses stored in data go through `fn_origin()` / `fn_resolve()`." / "**`port/src/symbols.h` is generated** ... Never hand-edit it; where the generator emits no name, use a local `#define` with the raw address."
- AGENTS.md: "A ported function that a differentially verified function stubs or records ... opens with `PR_SEAM(0xADDR, args...)` (void) or `PR_SEAM_RET(0xADDR, args...)` ... The arguments are the C signature's, in order (a pointer into `mem[]` as its offset)." Record E3 §E3.8: "A seam must be the callee's first statement".
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." / "**Register a test once** — add `X(test_foo)` to `TEST_CASES` in `port/tests/test.h`" / "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests, and never assert an unseeded BSS-zero". "Consolidating must not change an assertion": this plan **extends** the exact-set assertions of `RealFunctionTests` (spec names, mutant names, masks, stub clobbers, the counter line) with the batch's entries and changes no other assertion.
- AGENTS.md: "Every env-gated driver and `--check` run arms `fn_resolve`'s miss log and must record exactly the pinned known-set": registering the twelve adds no miss (none of them is in any pinned set; record §P1.10).
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style: `<area>: <what changed>`. **Never `git add -A`** — stage named files." Trailer: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.

## Review Focus

1. **A row that verifies because the spec hides the difference** (a mask too narrow, a stub EAX the port hard-codes, a field never varied). Pinned by `test_the_eax_mask_is_stated_by_the_spec_per_function` (the masks of record §P1.4, `0xFFFFFFFF` for `0x23BF8`), the `fighter_23bf8@zero` mutant (caught by EAX on case `e1` only), and the per-case stub EAX of `0x1A570` (both values) in Task 3.
2. **A store moved across a call, or a callee called with the raw's other register.** Pinned by `test_each_p1_mutant_is_caught_by_what_it_breaks` (`fighter_23ec0@mutant`, `fighter_402fc@mutant`: memory at a call only; `fighter_1579c@mutant`: the EBX handle `0x2BC30` preserves; `fighter_401d4`'s cases with EDX ≠ ctx[4]).
3. **A seam missing or not first.** Without a seam the port records no call: Task 3, 4, 5's seam checks (`call #0: original 0x1A570(..), port 0x2BC30(..)` and the two like it) and `test_each_stub_declares_the_registers_its_callee_clobbers` (the four new stubs' clobbers re-derived from the bytes).
4. **A registration missing or wrong** (the game would still fall back to `0xC9260`). Pinned by the unit checks' `fn_resolve(addr) == fn` and table-dword assertions (Tasks 2-5 mutation proofs delete a `fn_register` and see them fail).
5. **The E2 table or the oracles drifting.** `make entry-triage` fails before each regeneration (Tasks 2, 3, 5) and passes unchanged in Task 4; Task 1 and Task 6 run the full `make verify` with the 45 oracle lines and the WAV compared.

## Where to run

A worktree off `main`, branch `reverse-p1`:

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse
git worktree add .worktrees/reverse-p1 -b reverse-p1 main
cd .worktrees/reverse-p1
ln -s ../../data data && ln -s ../../.superpowers .superpowers
cp ../../port/tests/ghidra_data.bin ../../port/tests/title_screen_ref.ppm port/tests/
ls -d data .superpowers port/tests/ghidra_data.bin port/tests/title_screen_ref.ppm   # all four must exist
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -1                           # [100%] Built target diffrun (or run_tests)
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_p1_img.bin && shasum /tmp/pr_p1_img.bin
```

The last line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the image differs from the one the record measured: stop. If this plan and its record are not on `main` yet, bring them from the planning branch first. Every command runs from the worktree root. Ledger: `.superpowers/sdd/2026-10-02-reverse-p1-finishers/progress.md`.

**How the code steps are written.** Each change is a `python3 - <<'PY'` script that replaces exact text and asserts the text occurs exactly once (`sub`), so a script either applies cleanly or stops naming the file and the text it could not find. Run each once, from the worktree root. If `main` moved after `834b703` (U9/U10 may land first), an anchor can move: re-apply that `sub` by hand at the same place, never elsewhere; the unit tests' line numbers in the expected output move with `test_fight.c`.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/game/fighter.c`, `fighter.h` | the twelve functions (appended after `0x45D58`'s `fighter_45d58` at the end of the file), their prototypes; the seams of `0x1A570` (`fighter_actor_bit15_clear`) and `0x188AC` (`hit_anchor_set`) |
| `port/src/game/actors.c` | the registrations in `actors_init` (after `0x45C10` and `0x47B04`); the seam of `0x2A17C` (`actor_pset_palette`) |
| `port/tests/diff_runner.c` | the bindings and mutants (`b_*`/`m_*`, `k_bindings`) |
| `tools/diff_verify.py` | `P1_SPECS` and its callee declarations (`BIT15 PALETTE ANCHOR F38034`), appended to `SPECS` |
| `tools/tests/test_diff_verify.py` | `P1_MASKS`, `P1_KINDS`; the exact-set assertions extended; `test_each_p1_mutant_is_caught_by_what_it_breaks`; the counter line |
| `port/tests/test_fight.c`, `port/tests/test.h` | `test_p1_finishers`, `test_p1_callbacks` |
| `docs/superpowers/plans/2026-10-01-reverse-e2-triage.md` | regenerated in Tasks 2, 3, 5 |
| `docs/PROGRESS.md`, the record | Task 6 |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes `main` at the worktree's head; produces the baseline log `/tmp/pr_p1_base.log`.

- [ ] **Step 1: the full gate on the untouched tree.**

```bash
V=/tmp/pr_p1_base; mkdir -p $V
make verify SMK_DUMP=$V/smk TITLE_DUMP=$V/title ATTRACT_DUMP=$V/att FRONTEND_DUMP=$V/fe TITLE_PIN_DIR=$V/pin \
  AUDIO_WAV=$V/a.wav K11_DUMP=$V/k11 GP_DUMP=$V/gp DIFF_IMAGE=$V/diffimg DIFF_TABLE=$V/diff.md E2_IMAGE=$V/e2.bin \
  > /tmp/pr_p1_base.log 2>&1; echo "EXIT=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_p1_base.log \
  | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
grep -E '^diff-verify:|^entry-triage: (targets|voice)' /tmp/pr_p1_base.log
python3 tools/port_progress.py
```

Expected (at `834b703`): `EXIT=0`, `ORACLES-EQUAL`, and

```
diff-verify: 13/13 functions VERIFIED; 17/17 mutants detected; 1 named gaps; 0/5 rows with callees closed (8 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 323 unported, 172 ported; supplement 131 (31 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 46 in unported code, 69 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

If `main` moved, the counter and the triage lines may differ: record the actual lines in the ledger; every later "expected" counter then adds this plan's increments (+4, +3, +3, +3 rows; +4, +4, +3, +3 mutants) to the baseline's figures.

- [ ] **Step 2: the WAV.** `make audio-render AUDIO_WAV=/tmp/pr_p1_base/a.wav >/dev/null 2>&1; cmp /tmp/pr_p1_base/a.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-SAME` prints `WAV-SAME`.

---

### Task 2: the four same-shape finisher entries `0x1567C`, `0x15908`, `0x23EC0`, `0x45D14`

**Files:** modify `tools/diff_verify.py` (after `E3_SPECS`, and the `SPECS` line), `tools/tests/test_diff_verify.py` (after `EXE = ...`; the exact-set assertions; a new test before `test_each_stub_declares_the_registers_its_callee_clobbers`; the counter line), `port/tests/test_fight.c` (append after `test_u6b_231c0`), `port/tests/test.h` (`TEST_CASES`), `port/src/game/fighter.c` (append), `port/src/game/fighter.h` (before `#endif`), `port/src/game/actors.c` (`actors_init`, after `fn_register(0x45C10u, ...)`), `port/tests/diff_runner.c` (before `k_bindings`; four rows after `hit_anim_ctx@mutant`), `docs/superpowers/plans/2026-10-01-reverse-e2-triage.md` (regenerated).

**Interfaces:** produces `int fighter_1567c(u32 slot, u32 rec)`, `int fighter_15908(...)`, `int fighter_23ec0(...)`, `int fighter_45d14(...)` (as `0x379C4` calls `DS_001078E8`: non-zero return); `P1_SEED`, `p1_finisher`, `P1_SPECS` (diff_verify); `P1_MASKS`, `P1_KINDS` (tests); `p1_entry_fn`, `p1_check_finisher`, `p1_check_finishers`, `test_p1_finishers` (test_fight.c). Consumes `actors_anim_begin` (seam `0x2BC30`), `sound_voice` (seam `0x2C3FC`), the E3 specs' `ANIM_BEGIN`, `VOICE`, `E3_SLOT`, `E3_REC`; `z_fseed`, `u6b_run`, `mz_save` fixtures of `test_fight.c`. Record §P1.4, §P1.5.

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


# tools/diff_verify.py: the batch's specs after E3's, and in SPECS
sub("tools/diff_verify.py", '''    Spec("host_1b890", 0x1B890, [Case("g0", {})], mutants=(), gap="in at 0x1B899"),
]
''', '''    Spec("host_1b890", 0x1B890, [Case("g0", {})], mutants=(), gap="in at 0x1B899"),
]

# ---- track P batch 1: the finisher entries and their +0x0C callbacks (record 2026-10-02-reverse-p1) --
# A finisher entry runs as 0x379C4 calls it at 0x379E8: EAX = slot, EDX = rec. Mask 0xFF: the raw sets AL = 1
# over its last callee's EAX and 0x379EE's `test eax,eax` is the only read (record §P1.4). Every field the
# function writes is seeded with a sentinel; +0x42, the one it reads (`or ah,8`), takes 0x21 and 0xFF; the
# voice stub returns AL = 1 and 0 (the function overwrites AL with `mov al,1`).
P1_SEED = {E3_SLOT + 0x52: b"\\x52\\x53\\x54", E3_SLOT + 0x57: b"\\x57", E3_SLOT + 0x0C: le32(0x0C0C0C0C),
           E3_SLOT + 0x14: le32(0x14141414), E3_SLOT + 0x18: le32(0x18181818), E3_SLOT + 0x1C: le32(0x1C1C1C1C)}


def p1_finisher(name, entry, voice):
    calls = (ANIM_BEGIN, VOICE) if voice else (ANIM_BEGIN,)
    return Spec(name, entry, [
        Case("f0", {"eax": E3_SLOT, "edx": E3_REC}, {**P1_SEED, E3_SLOT + 0x42: b"\\x21"}),
        Case("f1", {"eax": E3_SLOT, "edx": E3_REC}, {**P1_SEED, E3_SLOT + 0x42: b"\\xff"},
             {0x2C3FC: 0} if voice else {}),
    ], calls=calls, eax_mask=0xFF)


P1_SPECS = [
    p1_finisher("fighter_1567c", 0x1567C, True),
    p1_finisher("fighter_15908", 0x15908, True),
    p1_finisher("fighter_23ec0", 0x23EC0, True),
    p1_finisher("fighter_45d14", 0x45D14, False),
]
''')
sub("tools/diff_verify.py", "] + E3_SPECS\n", "] + E3_SPECS + P1_SPECS\n")

# tools/tests/test_diff_verify.py: the batch's rows, masks and mutant kinds, extending the exact-set assertions
T = "tools/tests/test_diff_verify.py"
sub(T, '''EXE = os.path.join(os.environ.get("PR_GAME_DIR", os.path.join(ROOT, "data", "game", "C")), "PRAGE.EXE")
''', '''EXE = os.path.join(os.environ.get("PR_GAME_DIR", os.path.join(ROOT, "data", "game", "C")), "PRAGE.EXE")

# Track P batch 1 (record 2026-10-02-reverse-p1): its rows with their EAX masks, and what alone catches
# each of its mutants.
P1_MASKS = {"fighter_1567c": 0xFF, "fighter_15908": 0xFF, "fighter_23ec0": 0xFF, "fighter_45d14": 0xFF}
P1_KINDS = {"fighter_1567c@mutant": {"call #0"}, "fighter_15908@mutant": {"call #1"},
            "fighter_23ec0@mutant": {"call #1 memory"}, "fighter_45d14@mutant": {"call #0"}}
''')
sub(T, '''        self.assertEqual(sorted(self.real), ["anim_10fa8", "anim_3e4e4", "config_codeword_len",''',
    '''        self.assertEqual(sorted(self.real), sorted(["anim_10fa8", "anim_3e4e4", "config_codeword_len",''')
sub(T, '''                                             "host_1b890", "rng_next"])''',
    '''                                             "host_1b890", "rng_next"] + list(P1_MASKS)))''')
sub(T, '''        self.assertEqual(sorted(self.mut), [''', '''        self.assertEqual(sorted(self.mut), sorted([''')
sub(T, '''"hit_anim_start_b@set", "rng_next@mutant"])''',
    '''"hit_anim_start_b@set", "rng_next@mutant"]
            + list(P1_KINDS)))''')
sub(T, '''            "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF})''',
    '''            "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF,
            **P1_MASKS})''')
sub(T, '''    def test_each_stub_declares_the_registers_its_callee_clobbers(self):''',
    '''    def test_each_p1_mutant_is_caught_by_what_it_breaks(self):
        # track P batch 1 (record 2026-10-02-reverse-p1): what alone catches each mutant; every row with a
        # callee has one that only the call list or the memory at a call catches
        for name, want in P1_KINDS.items():
            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
            self.assertEqual(got, want, name)

    def test_each_stub_declares_the_registers_its_callee_clobbers(self):''')
sub(T, '''        self.assertIn("diff-verify: 13/13 functions VERIFIED; 17/17 mutants detected; 1 named gaps; "
                      "0/5 rows with callees closed (8 have none).", out.getvalue())''',
    '''        self.assertIn("diff-verify: 17/17 functions VERIFIED; 21/21 mutants detected; 1 named gaps; "
                      "0/9 rows with callees closed (8 have none).", out.getvalue())''')
print("t2_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p1_img.bin --function fighter_1567c | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected (the binding does not exist yet; the original side already shows the `0x15584` it stores at `0x10A20C`):

```
t2_spec applied
| fighter_1567c | 0x1567C | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified |
  fighter_1567c: f0: port: unknown binding fighter_1567c
  fighter_1567c: f1: port: unknown binding fighter_1567c
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
/* ---- track P batch 1 (record 2026-10-02-reverse-p1-derivations.md) ----------
 * The finisher entries and their +0x0C callbacks. Differential verification
 * (tools/diff_verify.py, P1_SPECS) is the behavioural oracle; these checks pin
 * what it does not see: the registrations and the image references that make
 * 0x379C4 and 0x3531C reach each function, and one seeded run through the
 * registration with sentinels on every store. */

typedef int (*p1_entry_fn)(u32 slot, u32 rec);

/* §P1.5: one finisher entry through its registration, as 0x379C4 calls it
 * (slot, rec): the stream head patched to a plain frame id (0x2BC30 stops on
 * it), sentinels on every slot field it stores, +0x42 = 0x21. */
static void p1_check_finisher(u32 addr, void (*fn)(void), u32 table_dw, u32 stream,
                              u32 st53, u32 cb0c, int or42, u32 voice)
{
    p1_entry_fn e;
    CHECK(fn_resolve(addr) == fn, "the finisher entry is registered");
    CHECK_EQ_INT((int)DSD(table_dw), (int)addr);
    e = (p1_entry_fn)(void *)fn_resolve(addr);
    z_fseed();
    DSW(stream) = 0x12B1u;
    DSB(Z_S0 + 0x54u) = 0x44u;
    DSB(Z_S0 + 0x57u) = 0x33u;
    DSB(Z_S0 + 0x42u) = 0x21u;
    DSD(Z_S0 + 0x0Cu) = 0x0C0C0C0Cu;
    DSD(Z_S0 + 0x14u) = 0x14141414u;
    DSD(Z_S0 + 0x18u) = 0x18181818u;
    DSD(Z_S0 + 0x1Cu) = 0x1C1C1C1Cu;
    sound_voice_log_reset();
    CHECK(e != NULL && e(Z_S0, Z_R0) != 0, "the finisher entry returns non-zero");
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), (int)stream);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), (int)st53);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x0Cu), (int)cb0c);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 0);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x14u), 0);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x18u), 0);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x42u), or42 ? 0x29 : 0x21);
    CHECK_EQ_INT((int)sound_voice_log_count(), voice ? 1 : 0);
    if (voice) CHECK_EQ_INT((int)sound_voice_log_at(0), (int)voice);
    sound_voice_log_reset();
}

static void p1_check_finishers(void)
{
    p1_check_finisher(0x1567Cu, (void (*)(void))fighter_1567c, 0x000BDAF0u, 0x000D32A8u,
                      7u, 0x00015584u, 1, 0xAFu);
    p1_check_finisher(0x15908u, (void (*)(void))fighter_15908, 0x000BDB0Cu, 0x000D3334u,
                      7u, 0x0001579Cu, 1, 0xAFu);
    p1_check_finisher(0x23EC0u, (void (*)(void))fighter_23ec0, 0x000BDB18u, 0x000E1B24u,
                      7u, 0x00023D38u, 0, 0xAAu);
    p1_check_finisher(0x45D14u, (void (*)(void))fighter_45d14, 0x000BDB10u, 0x000EB876u,
                      3u, 0u, 0, 0u);
}

int test_p1_finishers(void)     { return u6b_run(p1_check_finishers); }
'''

ANCHOR = "int test_u6b_231c0(void)        { return u6b_run(check_u6b_231c0); }\n"
sub("port/tests/test_fight.c", ANCHOR, ANCHOR + TEST)
sub("port/tests/test.h", "    X(test_u6b_231c0) \\\n", "    X(test_u6b_231c0) \\\n    X(test_p1_finishers) \\\n")
print("t2_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected:

```
port/tests/test_fight.c:44568:49: error: use of undeclared identifier 'fighter_1567c'
port/tests/test_fight.c:44570:49: error: use of undeclared identifier 'fighter_15908'
port/tests/test_fight.c:44572:49: error: use of undeclared identifier 'fighter_23ec0'
port/tests/test_fight.c:44574:49: error: use of undeclared identifier 'fighter_45d14'
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
/* ---- track P batch 1: the finisher entries and their slot +0x0C callbacks ----
 * Record 2026-10-02-reverse-p1-derivations.md. 0x379C4 calls a finisher entry
 * through DS_001078E8 at 0x379E8 as (EAX = slot, EDX = rec) and tests the whole
 * EAX at 0x379EE; 0x3531C case 7 (0x35431) and 0x38434 (0x384D9) call a slot's
 * +0x0C callback as (EAX = slot, EDX = rec, EBX = side) and discard EAX. */
#define P1_FIN_1567C_STREAM 0x000D32A8u  /* 0x15681 */
#define P1_FIN_15908_STREAM 0x000D3334u  /* 0x1590D */
#define P1_FIN_23EC0_STREAM 0x000E1B24u  /* 0x23EC5 */
#define P1_FIN_45D14_STREAM 0x000EB876u  /* 0x45D19 */

/* 0x1567C — record §P1.5. Character 3's 0xBDAE4 finisher entry (the dword at
 * 0xBDAF0): the record on 0xD32A8 at 3.0 (0x2BC30), the slot 7/9/0 with the
 * +0x0C callback 0x15584, +0x57 = 0, +0x18/+0x1C/+0x14 = 0, +0x42 bit 3, the
 * voice 0xAF. PORT: the raw returns the voice's EAX with AL = 1 (0x156CF);
 * 0x379EE reads only EAX != 0, which AL = 1 settles, so the port returns 1. */
int fighter_1567c(u32 slot, u32 rec)
{
    actors_anim_begin(rec, P1_FIN_1567C_STREAM, 0x40400000u); /* 0x1567F..0x1568B 0x2BC30 */
    DSB(slot + 0x53u) = 7u;                                 /* 0x15690 */
    DSB(slot + 0x52u) = 9u;                                 /* 0x15694 */
    DSB(slot + 0x54u) = 0u;                                 /* 0x15698 */
    DSD(slot + 0x0Cu) = 0x00015584u;                        /* 0x1569C */
    DSB(slot + 0x57u) = 0u;                                 /* 0x156A3 */
    DSD(slot + 0x18u) = 0u;                                 /* 0x156A7 */
    DSD(slot + 0x1Cu) = 0u;                                 /* 0x156B1 */
    DSB(slot + 0x42u) = (u8)(DSB(slot + 0x42u) | 8u);       /* 0x156AE..0x156BB */
    DSD(slot + 0x14u) = 0u;                                 /* 0x156C3 */
    (void)sound_voice(0xAFu);                               /* 0x156BE/0x156CA 0x2C3FC */
    return 1;                                               /* 0x156CF */
}

/* 0x15908 — record §P1.5. Character 3's 0xBDB00 finisher entry (the dword at
 * 0xBDB0C): 0x1567C's shape on the stream 0xD3334 with the +0x0C callback
 * 0x1579C, the voice 0xAF. PORT: AL = 1 at 0x1595B, returned as 1 (0x1567C). */
int fighter_15908(u32 slot, u32 rec)
{
    actors_anim_begin(rec, P1_FIN_15908_STREAM, 0x40400000u); /* 0x1590B..0x15917 0x2BC30 */
    DSB(slot + 0x53u) = 7u;                                 /* 0x1591C */
    DSB(slot + 0x52u) = 9u;                                 /* 0x15920 */
    DSB(slot + 0x54u) = 0u;                                 /* 0x15924 */
    DSD(slot + 0x0Cu) = 0x0001579Cu;                        /* 0x15928 */
    DSB(slot + 0x57u) = 0u;                                 /* 0x1592F */
    DSD(slot + 0x18u) = 0u;                                 /* 0x15933 */
    DSD(slot + 0x1Cu) = 0u;                                 /* 0x1593D */
    DSB(slot + 0x42u) = (u8)(DSB(slot + 0x42u) | 8u);       /* 0x1593A..0x15947 */
    DSD(slot + 0x14u) = 0u;                                 /* 0x1594F */
    (void)sound_voice(0xAFu);                               /* 0x1594A/0x15956 0x2C3FC */
    return 1;                                               /* 0x1595B */
}

/* 0x23EC0 — record §P1.5. Character 6's 0xBDB00 finisher entry (the dword at
 * 0xBDB18): the record on 0xE1B24 at 3.0, the slot 7/9/0 with the +0x0C
 * callback 0x23D38, +0x57/+0x18/+0x1C/+0x14 = 0 (+0x42 untouched), the voice
 * 0xAA. PORT: AL = 1 at 0x23F0A, returned as 1 (0x1567C). */
int fighter_23ec0(u32 slot, u32 rec)
{
    actors_anim_begin(rec, P1_FIN_23EC0_STREAM, 0x40400000u); /* 0x23EC3..0x23ECF 0x2BC30 */
    DSB(slot + 0x53u) = 7u;                                 /* 0x23ED4 */
    DSB(slot + 0x52u) = 9u;                                 /* 0x23ED8 */
    DSB(slot + 0x54u) = 0u;                                 /* 0x23EDC */
    DSD(slot + 0x0Cu) = 0x00023D38u;                        /* 0x23EE0 */
    DSB(slot + 0x57u) = 0u;                                 /* 0x23EE7 */
    DSD(slot + 0x18u) = 0u;                                 /* 0x23EEB */
    DSD(slot + 0x1Cu) = 0u;                                 /* 0x23EF2 */
    DSD(slot + 0x14u) = 0u;                                 /* 0x23EFE */
    (void)sound_voice(0xAAu);                               /* 0x23EF9/0x23F05 0x2C3FC */
    return 1;                                               /* 0x23F0A */
}

/* 0x45D14 — record §P1.5. Character 4's 0xBDB00 finisher entry (the dword at
 * 0xBDB10): the record on 0xEB876 at 3.0, the slot 3/9/0 with no +0x0C
 * callback, +0x57/+0x18/+0x1C/+0x14 = 0; no voice. PORT: the raw returns
 * 0x2BC30's EAX with AL = 1 (0x45D4D), returned as 1 (0x1567C). */
int fighter_45d14(u32 slot, u32 rec)
{
    actors_anim_begin(rec, P1_FIN_45D14_STREAM, 0x40400000u); /* 0x45D17..0x45D23 0x2BC30 */
    DSB(slot + 0x53u) = 3u;                                 /* 0x45D28 */
    DSB(slot + 0x52u) = 9u;                                 /* 0x45D2C */
    DSB(slot + 0x54u) = 0u;                                 /* 0x45D30 */
    DSD(slot + 0x0Cu) = 0u;                                 /* 0x45D34 */
    DSB(slot + 0x57u) = 0u;                                 /* 0x45D3B */
    DSD(slot + 0x18u) = 0u;                                 /* 0x45D3F */
    DSD(slot + 0x1Cu) = 0u;                                 /* 0x45D46 */
    DSD(slot + 0x14u) = 0u;                                 /* 0x45D4F */
    return 1;                                               /* 0x45D4D */
}
'''
BINDINGS = r'''/* Track P batch 1 (record 2026-10-02-reverse-p1 §P1.5): the finisher entries as 0x379C4 calls them
 * at 0x379E8 (EAX = slot, EDX = rec). Mask 0xFF: the raw sets AL = 1 over its last callee's EAX and
 * the only caller tests EAX != 0 (0x379EE), which AL = 1 settles (record §P1.4). */
static void b_1567c(const u32 *r, u32 *eax)            { *eax = (u32)fighter_1567c(r[R_EAX], r[R_EDX]); }
static void b_15908(const u32 *r, u32 *eax)            { *eax = (u32)fighter_15908(r[R_EAX], r[R_EDX]); }
static void b_23ec0(const u32 *r, u32 *eax)            { *eax = (u32)fighter_23ec0(r[R_EAX], r[R_EDX]); }
static void b_45d14(const u32 *r, u32 *eax)            { *eax = (u32)fighter_45d14(r[R_EAX], r[R_EDX]); }
static void m_1567c(const u32 *r, u32 *eax)            /* 0x15908's stream 0xD3334 */
{
    u32 slot = r[R_EAX];
    actors_anim_begin(r[R_EDX], 0x000D3334u, 0x40400000u);
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0x00015584u;
    DSB(slot + 0x57u) = 0u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    DSB(slot + 0x42u) = (u8)(DSB(slot + 0x42u) | 8u);
    DSD(slot + 0x14u) = 0u;
    (void)sound_voice(0xAFu);
    *eax = 1u;
}
static void m_15908(const u32 *r, u32 *eax)            /* the voice 0xAA (0x23EC0's) */
{
    u32 slot = r[R_EAX];
    actors_anim_begin(r[R_EDX], 0x000D3334u, 0x40400000u);
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0x0001579Cu;
    DSB(slot + 0x57u) = 0u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    DSB(slot + 0x42u) = (u8)(DSB(slot + 0x42u) | 8u);
    DSD(slot + 0x14u) = 0u;
    (void)sound_voice(0xAAu);
    *eax = 1u;
}
static void m_23ec0(const u32 *r, u32 *eax)            /* the slot stores after the voice, not before */
{
    u32 slot = r[R_EAX];
    actors_anim_begin(r[R_EDX], 0x000E1B24u, 0x40400000u);
    (void)sound_voice(0xAAu);
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0x00023D38u;
    DSB(slot + 0x57u) = 0u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    DSD(slot + 0x14u) = 0u;
    *eax = 1u;
}
static void m_45d14(const u32 *r, u32 *eax)            /* the frame 2.0, not 3.0 */
{
    u32 slot = r[R_EAX];
    actors_anim_begin(r[R_EDX], 0x000EB876u, 0x40000000u);
    DSB(slot + 0x53u) = 3u;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0u;
    DSB(slot + 0x57u) = 0u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    DSD(slot + 0x14u) = 0u;
    *eax = 1u;
}

'''

sub("port/src/game/fighter.c",
    "    DSB(DS_00104AEA) = (u8)(DSB(DS_00104AEA) | 2u);         /* 0x45D6C..0x45D7B */\n}\n",
    "    DSB(DS_00104AEA) = (u8)(DSB(DS_00104AEA) | 2u);         /* 0x45D6C..0x45D7B */\n}\n" + FIGHTER_C)
sub("port/src/game/fighter.h", "#endif /* PRAGE_GAME_FIGHTER_H */",
    """/* Track P batch 1 (record 2026-10-02-reverse-p1-derivations.md §P1.5): the
 * finisher entries 0x379C4 calls through DS_001078E8 as (slot, rec), testing
 * the whole EAX; registered in actors_init. */
int  fighter_1567c(u32 slot, u32 rec);
int  fighter_15908(u32 slot, u32 rec);
int  fighter_23ec0(u32 slot, u32 rec);
int  fighter_45d14(u32 slot, u32 rec);

#endif /* PRAGE_GAME_FIGHTER_H */""")
sub("port/src/game/actors.c", "    fn_register(0x45C10u, (void (*)(void))fighter_45c10);\n",
    """    fn_register(0x45C10u, (void (*)(void))fighter_45c10);
    /* PORT: record 2026-10-02-reverse-p1 §P1.5. The finisher entries of
     * characters 3, 4 and 6 (the dwords at 0xBDAF0, 0xBDB0C, 0xBDB10 and
     * 0xBDB18; 0x379C4 calls DS_001078E8 as (slot, rec) and tests EAX). */
    fn_register(0x1567Cu, (void (*)(void))fighter_1567c);
    fn_register(0x15908u, (void (*)(void))fighter_15908);
    fn_register(0x23EC0u, (void (*)(void))fighter_23ec0);
    fn_register(0x45D14u, (void (*)(void))fighter_45d14);
""")
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "hit_anim_ctx@mutant",      m_anim_ctx,     0x00000000u },\n',
    """    { "hit_anim_ctx@mutant",      m_anim_ctx,     0x00000000u },
    { "fighter_1567c",            b_1567c,        0x000000FFu },
    { "fighter_15908",            b_15908,        0x000000FFu },
    { "fighter_23ec0",            b_23ec0,        0x000000FFu },
    { "fighter_45d14",            b_45d14,        0x000000FFu },
    { "fighter_1567c@mutant",     m_1567c,        0x000000FFu },
    { "fighter_15908@mutant",     m_15908,        0x000000FFu },
    { "fighter_23ec0@mutant",     m_23ec0,        0x000000FFu },
    { "fighter_45d14@mutant",     m_45d14,        0x000000FFu },
""")
print("t2_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_1567c fighter_15908 fighter_23ec0 fighter_45d14; do
  python3 tools/diff_verify.py --image /tmp/pr_p1_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected: no compiler message, `all checks passed`, and

```
| fighter_1567c | 0x1567C | 2 | 1/1 | VERIFIED | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_1567c@mutant | 0x1567C | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_15908 | 0x15908 | 2 | 1/1 | VERIFIED | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_15908@mutant | 0x15908 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_23ec0 | 0x23EC0 | 2 | 1/1 | VERIFIED | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_23ec0@mutant | 0x23EC0 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_45d14 | 0x45D14 | 2 | 1/1 | VERIFIED | 2BC30 stub unverified |
| fighter_45d14@mutant | 0x45D14 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified |
```

A row that is not `VERIFIED` is a finding, not something to adjust: stop, compare the port's C with the bytes (record §P1.5), and report.

- [ ] **Step 4: the E2 table (decision D3 of E2).** `make entry-triage E2_IMAGE=/tmp/pr_p1_e2.bin 2>&1 | tail -2` first fails:

```
entry-triage: FAIL: docs/superpowers/plans/2026-10-01-reverse-e2-triage.md differs from a fresh run (regenerate it with --out)
make: *** [entry-triage] Error 1
```

then regenerate:

```bash
python3 tools/entry_triage.py --image /tmp/pr_p1_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
  --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
grep -E '^\| (finishers|stubs) ' docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
```

Expected:

```
entry-triage: targets 319 unported, 176 ported; supplement 131 (31 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 43 in unported code, 72 in ported code, 19 nowhere
| finishers | 2 | 7 |
| stubs | 79 |
```

(`git diff --stat` on the table: 9 lines: the four rows' `ported`, the batch and readiness counts, the three voice sites `156CA 15956 23F05`.)

- [ ] **Step 5: the task gate.**

```bash
make diff-verify entry-triage DIFF_IMAGE=/tmp/pr_p1_d.bin DIFF_TABLE=/tmp/pr_p1_d.md E2_IMAGE=/tmp/pr_p1_e2.bin 2>&1 \
  | grep -E '^(Ran|OK|FAILED|diff-verify:|entry-triage: targets)'
```

Expected:

```
Ran 156 tests in 26.071s
OK
diff-verify: 17/17 functions VERIFIED; 21/21 mutants detected; 1 named gaps; 0/9 rows with callees closed (8 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
Ran 44 tests in 8.981s
OK
entry-triage: targets 319 unported, 176 ported; supplement 131 (31 unported, 0 stale); untrusted entries 30
```

(the times vary).

- [ ] **Step 6: every new unit assertion can fail.** Three mutations of the code under test, each restored after its run:

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


mutate("port/src/game/actors.c", "    fn_register(0x1567Cu, (void (*)(void))fighter_1567c);\n", "")
mutate("port/src/game/fighter.c",
       "    (void)sound_voice(0xAAu);                               /* 0x23EF9/0x23F05 0x2C3FC */",
       "    (void)sound_voice(0xAFu);                               /* 0x23EF9/0x23F05 0x2C3FC */")
mutate("port/src/game/fighter.c",
       "    DSB(slot + 0x53u) = 3u;                                 /* 0x45D28 */",
       "    DSB(slot + 0x53u) = 7u;                                 /* 0x45D28 */")
PY
```

Expected (line numbers move with `test_fight.c`):

```
port/src/game/actors.c: 15 FAIL, first: ['test_fight.c:44536: the finisher entry is registered']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:44562: 175 != 170']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:44552: 7 != 3']
```

and `PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1` reads `all checks passed` again. The diff-verify mutants are Step 3's `@mutant` rows; what alone catches each is pinned by `test_each_p1_mutant_is_caught_by_what_it_breaks` (`call #0`, `call #1`, `call #1 memory`, `call #0`).

- [ ] **Step 7: commit.**

```bash
git add port/src/game/fighter.c port/src/game/fighter.h port/src/game/actors.c port/tests/diff_runner.c \
  port/tests/test_fight.c port/tests/test.h tools/diff_verify.py tools/tests/test_diff_verify.py \
  docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git commit -m "fighter: port the finisher entries 0x1567C 0x15908 0x23EC0 0x45D14, differentially verified (track P batch 1)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: the seam and row of `0x1A570`; the finisher entries `0x23BF8` and `0x402FC`

**Files:** modify `tools/diff_verify.py` (helpers before `P1_SPECS = [`, three specs at the end of the list), `tools/tests/test_diff_verify.py` (`P1_MASKS`, `P1_KINDS`, the stub-clobbers dict, the counter line), `port/tests/test_fight.c` (two checks before `p1_check_finishers`, two calls in it), `port/src/game/fighter.c` (append after `fighter_45d14`; the seam as the first statement of `fighter_actor_bit15_clear`), `fighter.h`, `actors.c` (after `0x45D14`'s registration), `diff_runner.c` (before `k_bindings`; seven rows after `fighter_45d14@mutant`), the E2 table.

**Interfaces:** produces `u32 fighter_23bf8(u32 slot, u32 rec)` (the whole EAX: `0x379EE` tests it), `int fighter_402fc(u32 slot, u32 rec)`; `PR_SEAM_RET(0x1A570u, side)` in `int fighter_actor_bit15_clear(u32 side)`; `BIT15 = E.Call(0x1A570, ("eax",))`, `p1_bit15`, `p1_23bf8`; `p1_check_23bf8`, `p1_check_402fc`. Consumes `hit_anim_ctx` (allow `0x339AC`), `hit_anim_start_b` (seam `0x3C4CC`, `HIT_B`), `SLOT_PTRS`, `DS_SLOTS`. Record §P1.4 (the 69 call sites, the mask), §P1.6.

- [ ] **Step 1: specs and expectations first.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


HELPERS = r'''# 0x1A570: AL = 1 when the actor word of the side's record has bit 15 clear (record §P1.4). Its stub EAX is
# the C predicate's 0 or 1 (the AL the 69 callers read).
BIT15 = E.Call(0x1A570, ("eax",))
P1_PSET = 0x10A600            # zero BSS of the image: a pset base for DS_001014EC
DS_PSET_BASE = 0x1014EC
DS_STAGE = 0x104AFC           # DS_00104AFC, the word 0x23BF8 indexes 0xA83C4/0xA83CC by


def p1_bit15(cid, side, idx, word):
    return Case(cid, {"eax": side}, {DS_SLOTS + side * 0x94: le32(E3_REC), E3_REC + 0x56: le32(idx)[:2],
                                     DS_PSET_BASE: le32(P1_PSET), P1_PSET + idx * 0x20: le32(word)[:2]})


def p1_23bf8(cid, stage, al, x, side, extra=None):
    pokes = {**P1_SEED, E3_SLOT + 0x42: bytes([0x21 + side]), DS_STAGE: le32(stage)[:2],
             E3_REC + 0x51: bytes([side]), E3_REC + 0x18: le32(x)}
    pokes.update(extra or {})
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC}, pokes, {} if al is None else {0x1A570: al})


'''
SPECS = r'''    Spec("fighter_actor_bit15_clear", 0x1A570, [
        p1_bit15("b0", 0, 3, 0x7FFF), p1_bit15("b1", 1, 5, 0x8000), p1_bit15("b2", 0, 7, 0xFFFF),
        p1_bit15("b3", 1, 2, 0x0000),
    ], eax_mask=0xFF),
    # 0x23BF8 (record §P1.6). The image's own stage tables: flag bytes 01 00 01 00 00 01 at 0xA83C4,
    # thresholds 0x2600 (stage 0), 0x6000 (2), 0x3100 (5) at 0xA83CC. e1 pokes a flag byte zero for the
    # stage word 0x105 so the early return's EAX is 0x100; a4/a5 pin the signed compares.
    Spec("fighter_23bf8", 0x23BF8, [
        p1_23bf8("e0", 1, None, 0, 0),
        p1_23bf8("e1", 0x105, None, 0, 1, {0xA83C4 + 0x105: b"\x00"}),
        p1_23bf8("a0", 0, 1, 0x2000, 0),
        p1_23bf8("a1", 0, 1, 0x2600, 1),
        p1_23bf8("a2", 2, 0, 0x6001, 0),
        p1_23bf8("a3", 2, 0, 0x6000, 1),
        p1_23bf8("a4", 5, 1, 0xFFFFF000, 0),
        p1_23bf8("a5", 5, 0, 0xFFFFF000, 1),
    ], calls=(BIT15, ANIM_BEGIN), mutants=("@mutant", "@zero")),
    # 0x402FC: 0x339AC runs on both sides (allow, record E3 §E3.6); its side is rec+0x51.
    Spec("fighter_402fc", 0x402FC, [
        Case("z%d" % side, {"eax": E3_SLOT, "edx": E3_REC},
             {**P1_SEED, **SLOT_PTRS, E3_REC + 0x51: bytes([side]), 0x1080A0: b"\xa0\xa0\xa2\xa2"},
             {0x3C4CC: 0x1234} if side else {})
        for side in (0, 1)
    ], allow_calls=(0x339AC,), calls=(HIT_B,), eax_mask=0xFF),
'''

V = "tools/diff_verify.py"
sub(V, "\n\nP1_SPECS = [\n", "\n\n" + HELPERS + "P1_SPECS = [\n")
sub(V, '    p1_finisher("fighter_45d14", 0x45D14, False),\n]\n',
    '    p1_finisher("fighter_45d14", 0x45D14, False),\n' + SPECS + "]\n")
T = "tools/tests/test_diff_verify.py"
sub(T, """P1_MASKS = {"fighter_1567c": 0xFF, "fighter_15908": 0xFF, "fighter_23ec0": 0xFF, "fighter_45d14": 0xFF}""",
    """P1_MASKS = {"fighter_1567c": 0xFF, "fighter_15908": 0xFF, "fighter_23ec0": 0xFF, "fighter_45d14": 0xFF,
            "fighter_actor_bit15_clear": 0xFF, "fighter_23bf8": 0xFFFFFFFF, "fighter_402fc": 0xFF}""")
sub(T, """            "fighter_23ec0@mutant": {"call #1 memory"}, "fighter_45d14@mutant": {"call #0"}}""",
    """            "fighter_23ec0@mutant": {"call #1 memory"}, "fighter_45d14@mutant": {"call #0"},
            "fighter_actor_bit15_clear@mutant": {"eax"}, "fighter_23bf8@mutant": {"call #1"},
            "fighter_23bf8@zero": {"eax"}, "fighter_402fc@mutant": {"call #0 memory"}}""")
sub(T, """                                 0x2AE14: ("ebx", "ecx", "edx")})""",
    """                                 0x2AE14: ("ebx", "ecx", "edx"), 0x1A570: ()})""")
sub(T, """        self.assertIn("diff-verify: 17/17 functions VERIFIED; 21/21 mutants detected; 1 named gaps; "
                      "0/9 rows with callees closed (8 have none).", out.getvalue())""",
    """        self.assertIn("diff-verify: 20/20 functions VERIFIED; 25/25 mutants detected; 1 named gaps; "
                      "1/11 rows with callees closed (9 have none).", out.getvalue())""")
print("t3_spec applied")
PY
for f in fighter_actor_bit15_clear fighter_23bf8 fighter_402fc; do
  python3 tools/diff_verify.py --image /tmp/pr_p1_img.bin --function $f | grep -E '^\| fighter|unknown binding' | head -2; done
```

Expected:

```
t3_spec applied
| fighter_actor_bit15_clear | 0x1A570 | 4 | 1/1 | MISMATCH | - |
  fighter_actor_bit15_clear: b0: port: unknown binding fighter_actor_bit15_clear
| fighter_23bf8 | 0x23BF8 | 8 | 8/8 | MISMATCH | 1A570 stub unverified, 2BC30 stub unverified |
  fighter_23bf8: e0: port: unknown binding fighter_23bf8
| fighter_402fc | 0x402FC | 2 | 1/1 | MISMATCH | 339AC allow unverified, 3C4CC stub unverified |
  fighter_402fc: z0: port: unknown binding fighter_402fc
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


TEST = r'''/* §P1.6: 0x23BF8 through its registration. Stage 1 (flag byte 0) returns 0
 * with nothing stored; stage 0x105 (its flag byte poked 0) returns 0x100;
 * stage 0 (threshold 0x2600) with the side's actor word bit 15 clear and
 * rec+0x18 = 0x2000 takes the far stream 0xE19E6; with bit 15 set and
 * rec+0x18 = 0x2000 the near stream 0xE1A06. */
static void p1_check_23bf8(void)
{
    p1_entry_fn e;
    u32 aw;
    CHECK(fn_resolve(0x23BF8u) == (void (*)(void))fighter_23bf8, "0x23BF8 is registered");
    CHECK_EQ_INT((int)DSD(0x000BDAFCu), 0x00023BF8);
    e = (p1_entry_fn)(void *)fn_resolve(0x23BF8u);
    if (e == NULL) return;
    z_fseed();
    DSW(0x000E19E6u) = 0x12B2u;
    DSW(0x000E1A06u) = 0x12B3u;
    DSB(Z_R0 + 0x51u) = 0u;
    DSD(Z_R0 + 0x18u) = 0x2000u;
    DSD(Z_S0 + 0x0Cu) = 0x0C0C0C0Cu;
    DSW(DS_00104AFC) = 1u;
    CHECK_EQ_INT(e(Z_S0, Z_R0), 0);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x0Cu), 0x0C0C0C0C);
    DSW(DS_00104AFC) = 0x105u;
    DSB(0x000A83C4u + 0x105u) = 0u;
    CHECK_EQ_INT(e(Z_S0, Z_R0), 0x100);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x0Cu), 0x0C0C0C0C);
    DSW(DS_00104AFC) = 0u;
    aw = DSD(DS_001014EC) + (u32)DSW(Z_R0 + 0x56u) * 0x20u;
    DSW(aw) = 0x0F35u;
    CHECK_EQ_INT(e(Z_S0, Z_R0), 1);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000E19E6);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x0Cu), 0x00023B68);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 7);
    DSW(aw) = 0x8F35u;
    DSD(Z_R0 + 0x18u) = 0x2000u;
    CHECK_EQ_INT(e(Z_S0, Z_R0), 1);
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), 0x000E1A06);
}

/* §P1.6: 0x402FC through its registration on side 1: the word 0x1080A2 = 0
 * (0x1080A0 kept), the slot 9/7/2 with the +0x0C callback 0x401D4, +0x57/
 * +0x18/+0x1C = 0, +0x14 kept. */
static void p1_check_402fc(void)
{
    p1_entry_fn e;
    CHECK(fn_resolve(0x402FCu) == (void (*)(void))fighter_402fc, "0x402FC is registered");
    CHECK_EQ_INT((int)DSD(0x000BDB00u), 0x000402FC);
    e = (p1_entry_fn)(void *)fn_resolve(0x402FCu);
    if (e == NULL) return;
    z_fseed();
    DSW(0x000E7C40u) = 0x12B4u;
    DSB(Z_R1 + 0x51u) = 1u;
    DSW(0x001080A0u) = 0xA0A0u;
    DSW(0x001080A2u) = 0xA2A2u;
    DSB(Z_S1 + 0x54u) = 0x44u;
    DSB(Z_S1 + 0x57u) = 0x33u;
    DSD(Z_S1 + 0x0Cu) = 0x0C0C0C0Cu;
    DSD(Z_S1 + 0x14u) = 0x14141414u;
    DSD(Z_S1 + 0x18u) = 0x18181818u;
    DSD(Z_S1 + 0x1Cu) = 0x1C1C1C1Cu;
    CHECK_EQ_INT(e(Z_S1, Z_R1), 1);
    CHECK_EQ_INT((int)DSW(0x001080A0u), 0xA0A0);
    CHECK_EQ_INT((int)DSW(0x001080A2u), 0);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x54u), 2);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x57u), 0);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x0Cu), 0x000401D4);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x14u), 0x14141414);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x18u), 0);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x1Cu), 0);
}

'''

F = "port/tests/test_fight.c"
sub(F, "static void p1_check_finishers(void)\n", TEST + "static void p1_check_finishers(void)\n")
sub(F, """    p1_check_finisher(0x45D14u, (void (*)(void))fighter_45d14, 0x000BDB10u, 0x000EB876u,
                      3u, 0u, 0, 0u);
}""", """    p1_check_finisher(0x45D14u, (void (*)(void))fighter_45d14, 0x000BDB10u, 0x000EB876u,
                      3u, 0u, 0, 0u);
    p1_check_23bf8();
    p1_check_402fc();
}""")
print("t3_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected:

```
port/tests/test_fight.c:44575:51: error: use of undeclared identifier 'fighter_23bf8'
port/tests/test_fight.c:44611:51: error: use of undeclared identifier 'fighter_402fc'
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
#define P1_23BF8_FLAG  0x000A83C4u  /* 0x23C08: [DS_00104AFC] byte, 0 = no finisher here */
#define P1_23BF8_X     0x000A83CCu  /* 0x23C36/0x23C4C: [DS_00104AFC] dword, the x threshold */
#define P1_23BF8_NEAR  0x000E1A06u  /* 0x23C1D: the stream EDX keeps through 0x1A570 */
#define P1_23BF8_FAR   0x000E19E6u  /* 0x23C55 */

/* 0x23BF8 — record §P1.6. Character 6's 0xBDAE4 finisher entry (the dword at
 * 0xBDAFC). With the byte P1_23BF8_FLAG[DS_00104AFC] zero it returns at once,
 * EAX the zero-extended word with AL cleared (`xor al,al` at 0x23C11: 0x379EE
 * tests the whole EAX, so the port returns the same value). Else 0x1A570(the
 * record's side) picks the compare: AL set, the far stream when rec+0x18 is
 * below the threshold (signed `jge` at 0x23C3D); AL clear, when it is above it
 * (`jle` at 0x23C53); otherwise the near stream, the EDX 0x1A570 preserves.
 * The record on that stream at 3.0, the slot 7/9/0 with the +0x0C callback
 * 0x23B68, +0x57/+0x18/+0x1C/+0x14 = 0, +0x42 bit 3. PORT: the raw returns
 * 0x2BC30's EAX with AL = 1 (0x23C98); only EAX != 0 is read, so the port
 * returns 1. */
u32 fighter_23bf8(u32 slot, u32 rec)
{
    u32 w = DSW(DS_00104AFC);                               /* 0x23C00..0x23C02 */
    u32 stream = P1_23BF8_NEAR;
    if (DSB(P1_23BF8_FLAG + w) == 0u) return w & 0xFF00u;   /* 0x23C08..0x23C11 */
    if (fighter_actor_bit15_clear(DSB(rec + 0x51u))) {      /* 0x23C18..0x23C29 0x1A570 */
        if ((s32)DSD(rec + 0x18u) < (s32)DSD(P1_23BF8_X + w * 4u))   /* 0x23C33..0x23C3F */
            stream = P1_23BF8_FAR;                          /* 0x23C55 */
    } else if ((s32)DSD(rec + 0x18u) > (s32)DSD(P1_23BF8_X + w * 4u)) {  /* 0x23C49..0x23C53 */
        stream = P1_23BF8_FAR;                              /* 0x23C55 */
    }
    actors_anim_begin(rec, stream, 0x40400000u);            /* 0x23C5A..0x23C61 0x2BC30 */
    DSB(slot + 0x53u) = 7u;                                 /* 0x23C66 */
    DSB(slot + 0x52u) = 9u;                                 /* 0x23C6A */
    DSB(slot + 0x54u) = 0u;                                 /* 0x23C6E */
    DSD(slot + 0x0Cu) = 0x00023B68u;                        /* 0x23C72 */
    DSB(slot + 0x57u) = 0u;                                 /* 0x23C79 */
    DSD(slot + 0x18u) = 0u;                                 /* 0x23C7D */
    DSD(slot + 0x1Cu) = 0u;                                 /* 0x23C84 */
    DSD(slot + 0x14u) = 0u;                                 /* 0x23C8E */
    DSB(slot + 0x42u) = (u8)(DSB(slot + 0x42u) | 8u);       /* 0x23C8B..0x23C9A */
    return 1u;                                              /* 0x23C98 */
}

#define P1_402FC_STREAM 0x000E7C40u  /* 0x40319 */

/* 0x402FC — record §P1.6. Character 0's 0xBDB00 finisher entry (the dword at
 * 0xBDB00): the context from the record (0x339AC), the word FIGHT_SC_1080A0
 * [its side] = 0, 0x3C4CC(rec, 0xE7C40, 3.0), the slot 9/7/2 with the +0x0C
 * callback 0x401D4, +0x57/+0x18/+0x1C = 0 (+0x14 and +0x42 untouched). PORT:
 * the raw returns 0x3C4CC's EAX with AL = 1 (0x40348), returned as 1. */
int fighter_402fc(u32 slot, u32 rec)
{
    u32 ctx[6];
    hit_anim_ctx(ctx, rec);                                 /* 0x40303..0x40307 0x339AC */
    DSW(0x001080A0u + ctx[0] * 2u) = 0u;                    /* 0x4030C..0x40311 */
    hit_anim_start_b(rec, P1_402FC_STREAM, 0x40400000u);    /* 0x40319..0x40325 0x3C4CC */
    DSB(slot + 0x52u) = 9u;                                 /* 0x4032A */
    DSB(slot + 0x53u) = 7u;                                 /* 0x4032E */
    DSB(slot + 0x54u) = 2u;                                 /* 0x40332 */
    DSB(slot + 0x57u) = 0u;                                 /* 0x40336 */
    DSD(slot + 0x0Cu) = 0x000401D4u;                        /* 0x4033A */
    DSD(slot + 0x18u) = 0u;                                 /* 0x40341 */
    DSD(slot + 0x1Cu) = 0u;                                 /* 0x4034A */
    return 1;                                               /* 0x40348 */
}
'''
BINDINGS = r'''/* Track P batch 1 (record 2026-10-02-reverse-p1 §P1.4/§P1.6): the callee 0x1A570 (EAX = side; mask 0xFF,
 * all 69 direct callers read AL), 0x23BF8 (mask 0xFFFFFFFF: its early return's EAX is a word with AL
 * cleared, which 0x379EE tests whole) and 0x402FC (mask 0xFF, as the other entries). */
static void b_1a570(const u32 *r, u32 *eax)            { *eax = (u32)fighter_actor_bit15_clear(r[R_EAX]); }
static void b_23bf8(const u32 *r, u32 *eax)            { *eax = fighter_23bf8(r[R_EAX], r[R_EDX]); }
static void b_402fc(const u32 *r, u32 *eax)            { *eax = (u32)fighter_402fc(r[R_EAX], r[R_EDX]); }
static void m_1a570(const u32 *r, u32 *eax)            /* tests bit 14, not 15 */
{
    u32 rec = DSD(DS_001077B0 + r[R_EAX] * 0x94u);
    u32 actor = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u;
    *eax = (DSW(actor) & 0x4000u) == 0 ? 1u : 0u;
}
static void m_23bf8(const u32 *r, u32 *eax)            /* the two compares swapped */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    u32 w = DSW(DS_00104AFC), stream = 0x000E1A06u;
    if (DSB(0x000A83C4u + w) == 0u) { *eax = w & 0xFF00u; return; }
    if (fighter_actor_bit15_clear(DSB(rec + 0x51u))) {
        if ((s32)DSD(rec + 0x18u) > (s32)DSD(0x000A83CCu + w * 4u)) stream = 0x000E19E6u;
    } else if ((s32)DSD(rec + 0x18u) < (s32)DSD(0x000A83CCu + w * 4u)) {
        stream = 0x000E19E6u;
    }
    actors_anim_begin(rec, stream, 0x40400000u);
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x54u) = 0u;
    DSD(slot + 0x0Cu) = 0x00023B68u;
    DSB(slot + 0x57u) = 0u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    DSD(slot + 0x14u) = 0u;
    DSB(slot + 0x42u) = (u8)(DSB(slot + 0x42u) | 8u);
    *eax = 1u;
}
static void m_23bf8_zero(const u32 *r, u32 *eax)       /* the early return as 0, not the word with AL cleared */
{
    if (DSB(0x000A83C4u + DSW(DS_00104AFC)) == 0u) { *eax = 0u; return; }
    *eax = fighter_23bf8(r[R_EAX], r[R_EDX]);
}
static void m_402fc(const u32 *r, u32 *eax)            /* the word store after the 0x3C4CC call */
{
    u32 ctx[6], slot = r[R_EAX], rec = r[R_EDX];
    hit_anim_ctx(ctx, rec);
    hit_anim_start_b(rec, 0x000E7C40u, 0x40400000u);
    DSW(0x001080A0u + ctx[0] * 2u) = 0u;
    DSB(slot + 0x52u) = 9u;
    DSB(slot + 0x53u) = 7u;
    DSB(slot + 0x54u) = 2u;
    DSB(slot + 0x57u) = 0u;
    DSD(slot + 0x0Cu) = 0x000401D4u;
    DSD(slot + 0x18u) = 0u;
    DSD(slot + 0x1Cu) = 0u;
    *eax = 1u;
}

'''

sub("port/src/game/fighter.c", "    return 1;                                               /* 0x45D4D */\n}\n",
    "    return 1;                                               /* 0x45D4D */\n}\n" + FIGHTER_C)
sub("port/src/game/fighter.c", "int fighter_actor_bit15_clear(u32 side)\n{\n",
    "int fighter_actor_bit15_clear(u32 side)\n{\n    PR_SEAM_RET(0x1A570u, side);\n")
sub("port/src/game/fighter.h", "int  fighter_45d14(u32 slot, u32 rec);\n",
    "int  fighter_45d14(u32 slot, u32 rec);\nu32  fighter_23bf8(u32 slot, u32 rec);\nint  fighter_402fc(u32 slot, u32 rec);\n")
sub("port/src/game/actors.c", "    fn_register(0x45D14u, (void (*)(void))fighter_45d14);\n",
    """    fn_register(0x45D14u, (void (*)(void))fighter_45d14);
    /* PORT: record 2026-10-02-reverse-p1 §P1.6. Character 6's 0xBDAE4 and
     * character 0's 0xBDB00 entries (the dwords at 0xBDAFC and 0xBDB00). */
    fn_register(0x23BF8u, (void (*)(void))fighter_23bf8);
    fn_register(0x402FCu, (void (*)(void))fighter_402fc);
""")
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_45d14@mutant",     m_45d14,        0x000000FFu },\n',
    """    { "fighter_45d14@mutant",     m_45d14,        0x000000FFu },
    { "fighter_actor_bit15_clear", b_1a570,       0x000000FFu },
    { "fighter_23bf8",            b_23bf8,        0xFFFFFFFFu },
    { "fighter_402fc",            b_402fc,        0x000000FFu },
    { "fighter_actor_bit15_clear@mutant", m_1a570, 0x000000FFu },
    { "fighter_23bf8@mutant",     m_23bf8,        0xFFFFFFFFu },
    { "fighter_23bf8@zero",       m_23bf8_zero,   0xFFFFFFFFu },
    { "fighter_402fc@mutant",     m_402fc,        0x000000FFu },
""")
print("t3_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_actor_bit15_clear fighter_23bf8 fighter_402fc; do
  python3 tools/diff_verify.py --image /tmp/pr_p1_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected: `all checks passed` and

```
| fighter_actor_bit15_clear | 0x1A570 | 4 | 1/1 | VERIFIED | - |
| fighter_actor_bit15_clear@mutant | 0x1A570 | 4 | 1/1 | MISMATCH | - |
| fighter_23bf8 | 0x23BF8 | 8 | 8/8 | VERIFIED | 1A570 stub unverified, 2BC30 stub unverified |
| fighter_23bf8@mutant | 0x23BF8 | 8 | 8/8 | MISMATCH | 1A570 stub unverified, 2BC30 stub unverified |
| fighter_23bf8@zero | 0x23BF8 | 8 | 8/8 | MISMATCH | 1A570 stub unverified, 2BC30 stub unverified |
| fighter_402fc | 0x402FC | 2 | 1/1 | VERIFIED | 339AC allow unverified, 3C4CC stub unverified |
| fighter_402fc@mutant | 0x402FC | 2 | 1/1 | MISMATCH | 339AC allow unverified, 3C4CC stub unverified |
```

(with `--function` the callee column reads `unverified` for every callee: E3 §E3.8; the full run in Step 5 reads `1A570 stub VERIFIED`, `339AC allow VERIFIED, 3C4CC stub VERIFIED`).

- [ ] **Step 4: the E2 table.** `make entry-triage E2_IMAGE=/tmp/pr_p1_e2.bin` fails as in Task 2 Step 4; regenerate with the same command. Expected:

```
entry-triage: targets 317 unported, 178 ported; supplement 131 (31 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 43 in unported code, 72 in ported code, 19 nowhere
| finishers | 0 | 9 |
| stubs | 77 |
```

- [ ] **Step 5: the task gate** (Task 2 Step 5's command). Expected: `Ran 156 tests` `OK` twice and

```
diff-verify: 20/20 functions VERIFIED; 25/25 mutants detected; 1 named gaps; 1/11 rows with callees closed (9 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 317 unported, 178 ported; supplement 131 (31 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 6: mutation proofs** (the unit checks; and the seam: without it the row cannot see the `0x1A570` call).

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


mutate("port/src/game/fighter.c",
       "    if (DSB(P1_23BF8_FLAG + w) == 0u) return w & 0xFF00u;   /* 0x23C08..0x23C11 */",
       "    if (DSB(P1_23BF8_FLAG + w) == 0u) return 0u;            /* 0x23C08..0x23C11 */")
mutate("port/src/game/actors.c", "    fn_register(0x402FCu, (void (*)(void))fighter_402fc);\n", "")
mutate("port/src/game/fighter.c",
       "    DSW(0x001080A0u + ctx[0] * 2u) = 0u;                    /* 0x4030C..0x40311 */",
       "    DSW(0x001080A0u + ctx[1] * 2u) = 0u;                    /* 0x4030C..0x40311 */")
# the seam: without it 0x23BF8's row cannot see the 0x1A570 call (diff-verify, not run_tests)
p = pathlib.Path("port/src/game/fighter.c")
keep = p.read_text()
p.write_text(keep.replace("    PR_SEAM_RET(0x1A570u, side);\n", "", 1))
try:
    subprocess.run(["cmake", "--build", "build"], check=True, capture_output=True)
    out = subprocess.run(["python3", "tools/diff_verify.py", "--image", "/tmp/pr_p1_m3.bin", "--function",
                          "fighter_23bf8"], capture_output=True, text=True).stdout
    print([l.strip() for l in out.splitlines() if "call #0:" in l][:1])
finally:
    p.write_text(keep)
    subprocess.run(["cmake", "--build", "build"], check=True, capture_output=True)
PY
```

Expected:

```
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:44590: 0 != 256']
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:44611: 0x402FC is registered']
port/src/game/fighter.c: 2 FAIL, first: ['test_fight.c:44627: 0 != 41120']
['fighter_23bf8: a0: call #0: original 0x1A570(0x0), port 0x2BC30(0x10A300, 0xE19E6, 0x40400000)']
```

- [ ] **Step 7: commit.**

```bash
git add port/src/game/fighter.c port/src/game/fighter.h port/src/game/actors.c port/tests/diff_runner.c \
  port/tests/test_fight.c tools/diff_verify.py tools/tests/test_diff_verify.py \
  docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git commit -m "fighter: port the finisher entries 0x23BF8 0x402FC; seam and row for 0x1A570 (track P batch 1)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: the +0x0C callbacks after jump tables `0x15584`, `0x1579C`, `0x23D38`; the seam of `0x2A17C`

**Files:** modify `tools/diff_verify.py` (helpers; three specs after `fighter_402fc`'s), `tools/tests/test_diff_verify.py`, `port/tests/test_fight.c` (append after `test_p1_finishers`), `port/tests/test.h`, `port/src/game/fighter.c` (append after `fighter_402fc`), `fighter.h`, `actors.c` (after `0x47B04`'s registration; the seam as the first statement of `actor_pset_palette`), `diff_runner.c` (six rows after `fighter_402fc@mutant`). **The E2 table does not change** (the three are outside E2's lists, record §P1.2); Step 4 checks that.

**Interfaces:** produces `void fighter_15584(u32 slot, u32 rec, u32 side)`, `void fighter_1579c(...)`, `void fighter_23d38(...)` (`fighter_slot_cb`, as `0x3531C` case 7 calls them), `static s32 p1_abs(u32 v)`; `PR_SEAM(0x2A17Cu, rec, word, handle)`; `PALETTE`, `P1_SEED_CB`, `p1_cb`, `p1_23d38`; `p1_cb_fn`, `p1_check_callbacks_a`, `test_p1_callbacks`. Consumes `fighter_ctx_same` (allow `0x33950`), `actor_pset_palette`. Record §P1.7, §P1.8.

- [ ] **Step 1: specs and expectations first.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


HELPERS = r'''# 0x2A17C (actor_pset_palette): EAX = rec, EDX = word, EBX = handle; a plain `ret`; it saves ECX and ESI and
# clobbers EDX (E.callee_clobbers; record §P1.4).
PALETTE = E.Call(0x2A17C, ("eax", "edx", "ebx"), clobbers=("edx",))
# 14 pokes (diffrun takes 16 per case): the two slots' characters (5 and 3), +0x42 and +4 (E3_OUT, the
# record case 3 copies to), sentinels on everything the callbacks store; +0x52..+0x57 as one poke.
P1_SEED_CB = {DS_SLOTS + 0x7A: b"\x05", DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x42: b"\x42",
              DS_SLOTS + 0x94 + 0x42: b"\x24", DS_SLOTS + 4: le32(E3_OUT), DS_SLOTS + 0x94 + 4: le32(E3_OUT),
              E3_OUT + 0x2C: b"\xcc\xcc", 0xF0AFE: b"\xfe", 0x1078FC: b"\xfc",
              E3_REC2 + 0x29: b"\x29", E3_REC2 + 0x34: b"\x34\x34"}


def p1_cb(name, entry, rows, calls, stub_eax=None):
    """A +0x0C callback's cases: (index, the slot's +0x57, side, extra pokes)."""
    return Spec(name, entry, [
        Case("c%d" % i, {"eax": E3_SLOT, "edx": E3_REC, "ebx": side},
             {**SLOT_PTRS, **P1_SEED_CB, E3_SLOT + 0x52: bytes([0x52, 0x53, 0x54, 0x55, 0x56, st]), **extra},
             (stub_eax or {}).get(i, {}))
        for i, st, side, extra in rows
    ], allow_calls=(0x33950,), calls=calls, eax_mask=0)


def p1_23d38(cid, st, x, ox, w28):
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0},
                {E3_SLOT + 0x57: bytes([st]), E3_SLOT + 8: le32(E3_REC2), 0xF0AF0: le32(0x10000),
                 E3_REC + 0x18: le32(x), E3_REC + 0x1C: le32(0x1C1C), E3_REC + 0x28: le32(w28)[:2],
                 E3_REC2 + 0x18: le32(ox), E3_REC2 + 0x1C: le32(0x2C2C), E3_REC2 + 0x29: b"\x29",
                 E3_REC2 + 0x2C: b"\xcc\xcc", E3_REC2 + 0x34: b"\x34\x34", E3_SLOT + 0x52: b"\x52\x53",
                 0xF0AFE: b"\xfe", 0x1078FC: b"\xfc"})


'''
SPECS = r'''    # 0x15584 and 0x1579C (record §P1.8): 0x33950 runs on both sides (allow); with side 0 the other slot is
    # slot 1 (character 3), with side 1 slot 0 (character 5); the own slot's character differs (6, 1) so the
    # mutant's wrong index shows. ctx[3]+4 points at E3_OUT, the record whose word +0x2C case 3 copies to.
    p1_cb("fighter_15584", 0x15584, [(0, 0, 0, {}), (1, 1, 0, {}), (2, 2, 1, {}),
                                      (3, 3, 0, {E3_REC2 + 0x2C: b"\x00\x05"}),
                                      (4, 3, 0, {E3_REC2 + 0x2C: b"\x20\x04"}),
                                      (5, 3, 0, {E3_REC2 + 0x2C: b"\x10\x00"}),
                                      (6, 4, 0, {}), (7, 5, 0, {}), (8, 6, 0, {})],
          calls=(ANIM_BEGIN, VOICE), stub_eax={1: {0x2C3FC: 0}}),
    p1_cb("fighter_1579c", 0x1579C, [(0, 0, 0, {}), (1, 1, 0, {}), (2, 2, 1, {}),
                                      (3, 3, 0, {E3_REC2 + 0x2C: b"\x00\x05"}),
                                      (4, 3, 0, {E3_REC2 + 0x2C: b"\x20\x04", E3_REC2 + 0x28: b"\x00\x40"}),
                                      (5, 3, 1, {E3_REC + 0x28: b"\xff\xbf\x00\x00\x20\x04"}),
                                      (6, 4, 0, {})],
          calls=(ANIM_BEGIN, PALETTE, VOICE)),
    # 0x23D38 (record §P1.8): DS_000F0AF0 = 0x10000; the record at slot+8 is E3_REC2.
    Spec("fighter_23d38", 0x23D38, [
        p1_23d38(cid, st, x, ox, w28) for cid, st, x, ox, w28 in (
            ("g0", 0, 0x12000, 0, 0x4000), ("g1", 0, 0x14000, 0, 0x4000), ("g2", 0, 0xE000, 0, 0),
            ("g3", 0, 0xC000, 0, 0), ("g4", 1, 0xDFFF, 0, 0), ("g5", 1, 0x12000, 0, 0),
            ("g6", 2, 0x5000, 0x6001, 0), ("g7", 2, 0x7000, 0x6000, 0), ("g8", 3, 0x5000, 0x5B01, 0x4000),
            ("g9", 3, 0x5000, 0x4500, 0x4000), ("gA", 3, 0x5000, 0x5000, 0), ("gB", 4, 0, 0, 0),
            ("gC", 5, 0, 0, 0), ("gD", 6, 0, 0, 0))
    ], calls=(ANIM_BEGIN, VOICE), eax_mask=0),
'''

V = "tools/diff_verify.py"
sub(V, "\n\nP1_SPECS = [\n", "\n\n" + HELPERS + "P1_SPECS = [\n")
sub(V, "    ], allow_calls=(0x339AC,), calls=(HIT_B,), eax_mask=0xFF),\n]\n",
    "    ], allow_calls=(0x339AC,), calls=(HIT_B,), eax_mask=0xFF),\n" + SPECS + "]\n")
T = "tools/tests/test_diff_verify.py"
sub(T, """            "fighter_actor_bit15_clear": 0xFF, "fighter_23bf8": 0xFFFFFFFF, "fighter_402fc": 0xFF}""",
    """            "fighter_actor_bit15_clear": 0xFF, "fighter_23bf8": 0xFFFFFFFF, "fighter_402fc": 0xFF,
            "fighter_15584": 0, "fighter_1579c": 0, "fighter_23d38": 0}""")
sub(T, """            "fighter_23bf8@zero": {"eax"}, "fighter_402fc@mutant": {"call #0 memory"}}""",
    """            "fighter_23bf8@zero": {"eax"}, "fighter_402fc@mutant": {"call #0 memory"},
            "fighter_15584@mutant": {"call #1"}, "fighter_1579c@mutant": {"call #1"},
            "fighter_23d38@mutant": {"call #0", "call #1"}}""")
sub(T, """0x2AE14: ("ebx", "ecx", "edx"), 0x1A570: ()})""",
    """0x2AE14: ("ebx", "ecx", "edx"), 0x1A570: (), 0x2A17C: ("edx",)})""")
sub(T, """        self.assertIn("diff-verify: 20/20 functions VERIFIED; 25/25 mutants detected; 1 named gaps; "
                      "1/11 rows with callees closed (9 have none).", out.getvalue())""",
    """        self.assertIn("diff-verify: 23/23 functions VERIFIED; 28/28 mutants detected; 1 named gaps; "
                      "1/14 rows with callees closed (9 have none).", out.getvalue())""")
print("t4_spec applied")
PY
for f in fighter_15584 fighter_1579c fighter_23d38; do
  python3 tools/diff_verify.py --image /tmp/pr_p1_img.bin --function $f | grep -E '^\| fighter|unknown binding' | head -2; done
```

Expected:

```
t4_spec applied
| fighter_15584 | 0x15584 | 9 | 9/9 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified |
  fighter_15584: c0: port: unknown binding fighter_15584
| fighter_1579c | 0x1579C | 7 | 11/11 | MISMATCH | 2A17C stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified |
  fighter_1579c: c0: port: unknown binding fighter_1579c
| fighter_23d38 | 0x23D38 | 14 | 24/24 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified |
  fighter_23d38: g0: port: unknown binding fighter_23d38
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
typedef void (*p1_cb_fn)(u32 slot, u32 rec, u32 side);

/* §P1.8: the three +0x0C callbacks the finisher entries store, through their
 * registrations as 0x3531C case 7 calls them (slot 0, its record, side 0; the
 * other slot is slot 1, whose +4 points at FIGHT_RECS + 0x200). */
static void p1_check_callbacks_a(void)
{
    u32 tgt = FIGHT_RECS + 0x200u;
    p1_cb_fn f;
    CHECK(fn_resolve(0x15584u) == (void (*)(void))fighter_15584, "0x15584 is registered");
    CHECK(fn_resolve(0x1579Cu) == (void (*)(void))fighter_1579c, "0x1579C is registered");
    CHECK(fn_resolve(0x23D38u) == (void (*)(void))fighter_23d38, "0x23D38 is registered");
    CHECK_EQ_INT((int)DSD(0x0001569Fu), 0x00015584);
    CHECK_EQ_INT((int)DSD(0x0001592Bu), 0x0001579C);
    CHECK_EQ_INT((int)DSD(0x00023EE3u), 0x00023D38);

    /* 0x15584 case 3: 0x420 - 0x40 = 0x3E0 is at most 0x400: set 0x400, step
     * +0x57, copy to the other slot's +4 record; then case 5. */
    f = (p1_cb_fn)(void *)fn_resolve(0x15584u);
    if (f == NULL) return;
    z_fseed();
    DSD(Z_S1 + 4u) = tgt;
    DSW(tgt + 0x2Cu) = 0xCCCCu;
    DSW(DSD(Z_S1) + 0x2Cu) = 0x0420u;
    DSB(Z_S0 + 0x57u) = 3u;
    f(Z_S0, DSD(Z_S0), 0u);
    CHECK_EQ_INT((int)DSW(DSD(Z_S1) + 0x2Cu), 0x400);
    CHECK_EQ_INT((int)DSW(tgt + 0x2Cu), 0x400);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 4);
    DSB(Z_S0 + 0x57u) = 5u;
    f(Z_S0, DSD(Z_S0), 0u);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 3);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(DS_000F0AFE), 4);
    CHECK_EQ_INT((int)DSB(DS_001078FC), 1);

    /* 0x1579C case 3 above 0x400: 0x500 - 0x40 = 0x4C0 is copied, +0x57 kept. */
    f = (p1_cb_fn)(void *)fn_resolve(0x1579Cu);
    if (f == NULL) return;
    z_fseed();
    DSD(Z_S1 + 4u) = tgt;
    DSW(tgt + 0x2Cu) = 0xCCCCu;
    DSW(DSD(Z_S1) + 0x2Cu) = 0x0500u;
    DSB(Z_S0 + 0x57u) = 3u;
    f(Z_S0, DSD(Z_S0), 0u);
    CHECK_EQ_INT((int)DSW(tgt + 0x2Cu), 0x4C0);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 3);

    /* 0x23D38 case 0, the record's +0x28 bit 14 clear: rec+0x18 = 0xC000 lies
     * below DS_000F0AF0 - 0x3000 = 0xD000, so it moves to 0x13000. */
    f = (p1_cb_fn)(void *)fn_resolve(0x23D38u);
    if (f == NULL) return;
    z_fseed();
    DSD(DS_000F0AF0) = 0x10000u;
    DSW(Z_R0 + 0x28u) = 0u;
    DSD(Z_R0 + 0x18u) = 0xC000u;
    DSB(Z_S0 + 0x57u) = 0u;
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x18u), 0x13000);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 1);
}

int test_p1_callbacks(void)     { return u6b_run(p1_check_callbacks_a); }
'''

ANCHOR = "int test_p1_finishers(void)     { return u6b_run(p1_check_finishers); }\n"
sub("port/tests/test_fight.c", ANCHOR, ANCHOR + TEST)
sub("port/tests/test.h", "    X(test_p1_finishers) \\\n", "    X(test_p1_finishers) \\\n    X(test_p1_callbacks) \\\n")
print("t4_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected:

```
port/tests/test_fight.c:44664:51: error: use of undeclared identifier 'fighter_15584'
port/tests/test_fight.c:44665:51: error: use of undeclared identifier 'fighter_1579c'
port/tests/test_fight.c:44666:51: error: use of undeclared identifier 'fighter_23d38'
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
#define P1_CB_STREAM_C90F8  0x000C90F8u  /* 0x155B8/0x157D0: [the other's char] dword, a stream */
#define P1_CB_VOICE_C75AA   0x000C75AAu  /* 0x155DD/0x157F5: [the other's char] u16, a voice id */
#define P1_CB_STREAM_9B054  0x0009B054u  /* 0x15608/0x15820: [the other's char] dword, a stream */
#define P1_1579C_STREAM     0x000E8C82u  /* 0x15895 */
#define P1_1579C_PALETTE    0x1187FAE0u  /* 0x1589E: EBX, which 0x2BC30 preserves, 0x2A17C's handle */

/* 0x15584 — record §P1.8. The slot +0x0C callback 0x1567C stores (the
 * immediate at 0x1569F; no Ghidra function, no E2 row: it follows the jump
 * table 0x1556C, not a `ret`). EAX = slot, EDX = rec (not read), EBX = side;
 * the context is 0x33950(side). Dispatched on the slot's +0x57 through the
 * six-entry table 0x1556C (`cmp al,5; ja` at 0x15596): 1 puts the other's
 * record ctx[5] on 0xC90F8[the other's char] at 2.0, sets the other slot's
 * +0x42 bit 2, plays the voice 0xC75AA[char] and steps +0x57; 2 puts ctx[5]
 * on 0x9B054[char] at 1.0; 3 lowers ctx[5]'s word +0x2C by 0x40 and, at
 * 0x400 or below (a signed compare of the zero-extended word), sets it 0x400
 * and steps +0x57; then copies it to the word +0x2C of the record at the other
 * slot's +4; 5 sets the slot's +0x53 = 3, +0x52 = 9, DS_000F0AFE = 4 and
 * DS_001078FC = 1; 0, 4 and above 5 do nothing. */
void fighter_15584(u32 slot, u32 rec, u32 side)
{
    u32 ctx[6];
    (void)rec;
    fighter_ctx_same(ctx, side);                            /* 0x15588..0x1558E 0x33950 */
    switch (DSB(slot + 0x57u)) {                            /* 0x15593..0x155A3 table 0x1556C */
    case 1u:                                                /* 0x155AB */
        actors_anim_begin(ctx[5], DSD(P1_CB_STREAM_C90F8 + (u32)DSB(ctx[3] + 0x7Au) * 4u),
                          0x40000000u);                     /* 0x155AB..0x155C4 0x2BC30 */
        DSB(ctx[3] + 0x42u) = (u8)(DSB(ctx[3] + 0x42u) | 4u); /* 0x155CD */
        (void)sound_voice(DSW(P1_CB_VOICE_C75AA + (u32)DSB(ctx[3] + 0x7Au) * 2u)); /* 0x155D1..0x155EA 0x2C3FC */
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);   /* 0x155EF */
        return;
    case 2u:                                                /* 0x155F7 */
        actors_anim_begin(ctx[5], DSD(P1_CB_STREAM_9B054 + (u32)DSB(ctx[3] + 0x7Au) * 4u),
                          0x3F800000u);                     /* 0x155F7..0x15613 0x2BC30 */
        return;
    case 3u:                                                /* 0x1561D */
        DSW(ctx[5] + 0x2Cu) = (u16)(DSW(ctx[5] + 0x2Cu) - 0x40u);   /* 0x15621 */
        if (DSW(ctx[5] + 0x2Cu) <= 0x400u) {                /* 0x1562A..0x15638 */
            DSW(ctx[5] + 0x2Cu) = 0x400u;                   /* 0x1563E */
            DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);   /* 0x15644 */
        }
        DSW(DSD(ctx[3] + 4u) + 0x2Cu) = DSW(ctx[5] + 0x2Cu);    /* 0x15647..0x15656 */
        return;
    case 5u:                                                /* 0x1565F */
        DSB(slot + 0x53u) = 3u;                             /* 0x15661 */
        DSB(slot + 0x52u) = 9u;                             /* 0x15667 */
        DSB(DS_000F0AFE) = 4u;                              /* 0x1566B */
        DSB(DS_001078FC) = 1u;                              /* 0x15671 */
        return;
    default:                                                /* 0x15677: 0, 4, above 5 */
        return;
    }
}

/* 0x1579C — record §P1.8. The slot +0x0C callback 0x15908 stores (the
 * immediate at 0x1592B; after the jump table 0x1578C, no E2 row). 0x15584's
 * cases 1 and 2 (0x157C3, 0x1580F) through the four-entry table 0x1578C
 * (`cmp al,3; ja` at 0x157AE); case 3 (0x15835) lowers ctx[5]'s word +0x2C by
 * 0x40 and, above 0x400, copies it to the record at the other slot's +4
 * (0x158ED: E2's untrusted entry, a block of this function); at 0x400 or below:
 * ctx[5]'s +0x29 bit 6 cleared and word +0x34 = 0x80 when its word +0x28 has
 * bit 14, else bit 6 set and +0x34 = 0xFF80; ctx[5] on 0xE8C82 at 3.0, then
 * 0x2A17C(ctx[5], 0x18, 0x1187FAE0); word +0x2C = 0x1000, +0x28 bit 7, the
 * voice 0xEA, the slot's +0x53 = 3, +0x52 = 9, DS_000F0AFE = 4, DS_001078FC
 * = 1. Case 0 and above 3 do nothing. */
void fighter_1579c(u32 slot, u32 rec, u32 side)
{
    u32 ctx[6];
    (void)rec;
    fighter_ctx_same(ctx, side);                            /* 0x157A0..0x157A6 0x33950 */
    switch (DSB(slot + 0x57u)) {                            /* 0x157AB..0x157BB table 0x1578C */
    case 1u:                                                /* 0x157C3 */
        actors_anim_begin(ctx[5], DSD(P1_CB_STREAM_C90F8 + (u32)DSB(ctx[3] + 0x7Au) * 4u),
                          0x40000000u);                     /* 0x157C3..0x157DC 0x2BC30 */
        DSB(ctx[3] + 0x42u) = (u8)(DSB(ctx[3] + 0x42u) | 4u); /* 0x157E5 */
        (void)sound_voice(DSW(P1_CB_VOICE_C75AA + (u32)DSB(ctx[3] + 0x7Au) * 2u)); /* 0x157E9..0x15802 0x2C3FC */
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);   /* 0x15807 */
        return;
    case 2u:                                                /* 0x1580F */
        actors_anim_begin(ctx[5], DSD(P1_CB_STREAM_9B054 + (u32)DSB(ctx[3] + 0x7Au) * 4u),
                          0x3F800000u);                     /* 0x1580F..0x1582B 0x2BC30 */
        return;
    case 3u:                                                /* 0x15835 */
        DSW(ctx[5] + 0x2Cu) = (u16)(DSW(ctx[5] + 0x2Cu) - 0x40u);   /* 0x15839 */
        if (DSW(ctx[5] + 0x2Cu) > 0x400u) {                 /* 0x15842..0x15850 */
            DSW(DSD(ctx[3] + 4u) + 0x2Cu) = DSW(ctx[5] + 0x2Cu);    /* 0x158ED..0x158FC */
            return;
        }
        if ((DSW(ctx[5] + 0x28u) & 0x4000u) != 0u) {        /* 0x15856..0x15868 */
            DSB(ctx[5] + 0x29u) = (u8)(DSB(ctx[5] + 0x29u) & 0xBFu);  /* 0x1586E */
            DSW(ctx[5] + 0x34u) = 0x0080u;                  /* 0x15876 */
        } else {
            DSB(ctx[5] + 0x29u) = (u8)(DSB(ctx[5] + 0x29u) | 0x40u);  /* 0x15882 */
            DSW(ctx[5] + 0x34u) = 0xFF80u;                  /* 0x1588A */
        }
        actors_anim_begin(ctx[5], P1_1579C_STREAM, 0x40400000u);  /* 0x15890..0x158A3 0x2BC30 */
        actor_pset_palette(ctx[5], 0x18u, P1_1579C_PALETTE);      /* 0x158A8..0x158B1 0x2A17C */
        DSW(ctx[5] + 0x2Cu) = 0x1000u;                      /* 0x158BA */
        DSB(ctx[5] + 0x28u) = (u8)(DSB(ctx[5] + 0x28u) | 0x80u);  /* 0x158C4 */
        (void)sound_voice(0xEAu);                           /* 0x158C8/0x158CD 0x2C3FC */
        DSB(slot + 0x53u) = 3u;                             /* 0x158D2 */
        DSB(slot + 0x52u) = 9u;                             /* 0x158D6 */
        DSB(DS_000F0AFE) = 4u;                              /* 0x158DA */
        DSB(DS_001078FC) = 1u;                              /* 0x158E1 */
        return;
    default:                                                /* 0x15900: 0 and above 3 */
        return;
    }
}

#define P1_23D38_SNAP   0x000E1BAEu  /* 0x23DCF */
#define P1_23D38_REACH  0x000E1BBCu  /* 0x23DFF */
#define P1_23D38_GRAB   0x000E1BD2u  /* 0x23E74 */
#define P1_23D38_HELD   0x000E1C0Cu  /* 0x23E83 */

/* |v| as the raw's `test; jge; neg` computes it: 0x80000000 stays itself. */
static s32 p1_abs(u32 v)
{
    return (s32)v < 0 ? (s32)(0u - v) : (s32)v;
}

/* 0x23D38 — record §P1.8. The slot +0x0C callback 0x23EC0 stores (the
 * immediate at 0x23EE3; after the jump table 0x23D20, no E2 row). EAX = slot,
 * EDX = rec, EBX (the side) not read. Dispatched on +0x57 through the six-
 * entry table 0x23D20 (`cmp dl,5; ja` at 0x23D41): 0 moves rec+0x18 to
 * DS_000F0AF0 -/+ 0x3000 by the record's word +0x28 bit 14 when it lies beyond
 * that bound (signed) and steps +0x57; 1 within 0x2000 of DS_000F0AF0 puts the
 * record on 0xE1BAE at 3.0 and steps; 2 within 0x1000 of the record at slot+8
 * puts it on 0xE1BBC and steps; 3 within 0xB00 of it moves that record onto
 * this one (x, y, words +0x34 = 0, +0x2C = 0xE00, +0x29 bit 6 from the word
 * +0x28's bit 14), starts both (0xE1BD2, 0xE1C0C at 3.0), steps and plays the
 * voice 0xD6; 5 sets +0x53 = 3, +0x52 = 9, DS_000F0AFE = 4, DS_001078FC = 1;
 * 4 and above 5 do nothing. Distances are |a - b| by `neg` and compared
 * signed (`jg`). */
void fighter_23d38(u32 slot, u32 rec, u32 side)
{
    u32 other;
    (void)side;
    switch (DSB(slot + 0x57u)) {                            /* 0x23D3E..0x23D50 table 0x23D20 */
    case 0u:                                                /* 0x23D58 */
        if ((DSW(rec + 0x28u) & 0x4000u) != 0u) {           /* 0x23D58..0x23D74 */
            if ((s32)(DSD(DS_000F0AF0) + 0x3000u) >= (s32)DSD(rec + 0x18u)) return;  /* 0x23D76 */
            DSD(rec + 0x18u) = DSD(DS_000F0AF0) - 0x3000u;  /* 0x23D7F..0x23D8B */
        } else {
            if ((s32)(DSD(DS_000F0AF0) - 0x3000u) <= (s32)DSD(rec + 0x18u)) return;  /* 0x23D94..0x23DA5 */
            DSD(rec + 0x18u) = DSD(DS_000F0AF0) + 0x3000u;  /* 0x23DAB */
        }
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);   /* 0x23D8E/0x23DAE */
        return;
    case 1u:                                                /* 0x23DB4 */
        if (p1_abs(DSD(DS_000F0AF0) - DSD(rec + 0x18u)) > 0x2000) return;  /* 0x23DB4..0x23DC9 */
        actors_anim_begin(rec, P1_23D38_SNAP, 0x40400000u); /* 0x23DCF..0x23DD9 0x2BC30 */
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);   /* 0x23DDE */
        return;
    case 2u:                                                /* 0x23DE4 */
        other = DSD(slot + 8u);
        if (p1_abs(DSD(rec + 0x18u) - DSD(other + 0x18u)) > 0x1000) return;  /* 0x23DE4..0x23DF9 */
        actors_anim_begin(rec, P1_23D38_REACH, 0x40400000u);  /* 0x23DFF..0x23E09 0x2BC30 */
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);   /* 0x23E0E */
        return;
    case 3u:                                                /* 0x23E14 */
        other = DSD(slot + 8u);
        if (p1_abs(DSD(rec + 0x18u) - DSD(other + 0x18u)) > 0xB00) return;   /* 0x23E14..0x23E29 */
        DSD(DSD(slot + 8u) + 0x18u) = DSD(rec + 0x18u);    /* 0x23E2F..0x23E35 */
        DSD(DSD(slot + 8u) + 0x1Cu) = DSD(rec + 0x1Cu);    /* 0x23E38..0x23E3E */
        DSW(DSD(slot + 8u) + 0x34u) = 0u;                   /* 0x23E41..0x23E44 */
        DSW(DSD(slot + 8u) + 0x2Cu) = 0x0E00u;              /* 0x23E4A..0x23E4D */
        if ((DSW(rec + 0x28u) & 0x4000u) != 0u)             /* 0x23E53..0x23E62 */
            DSB(DSD(slot + 8u) + 0x29u) = (u8)(DSB(DSD(slot + 8u) + 0x29u) | 0x40u);  /* 0x23E64..0x23E67 */
        else
            DSB(DSD(slot + 8u) + 0x29u) = (u8)(DSB(DSD(slot + 8u) + 0x29u) & 0xBFu);  /* 0x23E6D..0x23E70 */
        actors_anim_begin(rec, P1_23D38_GRAB, 0x40400000u); /* 0x23E74..0x23E7E 0x2BC30 */
        actors_anim_begin(DSD(slot + 8u), P1_23D38_HELD, 0x40400000u);  /* 0x23E83..0x23E90 0x2BC30 */
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);   /* 0x23E95 */
        (void)sound_voice(0xD6u);                           /* 0x23E98/0x23E9D 0x2C3FC */
        return;
    case 5u:                                                /* 0x23EA5 */
        DSB(slot + 0x53u) = 3u;                             /* 0x23EA7 */
        DSB(slot + 0x52u) = 9u;                             /* 0x23EAD */
        DSB(DS_000F0AFE) = 4u;                              /* 0x23EB1 */
        DSB(DS_001078FC) = 1u;                              /* 0x23EB7 */
        return;
    default:                                                /* 0x23EBD: 4, above 5 */
        return;
    }
}
'''
BINDINGS = r'''/* Track P batch 1 (record 2026-10-02-reverse-p1 §P1.8): the slot +0x0C callbacks as 0x3531C case 7 calls
 * them at 0x35431 (EAX = slot, EDX = rec, EBX = side). Mask 0: 0x35434 `xor eax,eax` overwrites EAX, and
 * 0x38434's call at 0x384D9 returns it to 0x3856B, which loads EAX at once (`mov eax,esi`). */
static void b_15584(const u32 *r, u32 *eax)            { fighter_15584(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_1579c(const u32 *r, u32 *eax)            { fighter_1579c(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_23d38(const u32 *r, u32 *eax)            { fighter_23d38(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void m_15584(const u32 *r, u32 *eax)            /* case 1's voice by the slot's own character */
{
    u32 ctx[6], slot = r[R_EAX];
    fighter_ctx_same(ctx, r[R_EBX]);
    if (DSB(slot + 0x57u) == 1u) {
        actors_anim_begin(ctx[5], DSD(0x000C90F8u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40000000u);
        DSB(ctx[3] + 0x42u) = (u8)(DSB(ctx[3] + 0x42u) | 4u);
        (void)sound_voice(DSW(0x000C75AAu + (u32)DSB(ctx[2] + 0x7Au) * 2u));
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);
    } else {
        fighter_15584(slot, r[R_EDX], r[R_EBX]);
    }
    *eax = 0u;
}
static void m_1579c(const u32 *r, u32 *eax)            /* case 3's palette handle 0x1F874610 (0x45B50's) */
{
    u32 ctx[6], slot = r[R_EAX];
    fighter_ctx_same(ctx, r[R_EBX]);
    if (DSB(slot + 0x57u) == 3u
            && (u16)(DSW(ctx[5] + 0x2Cu) - 0x40u) <= 0x400u) {
        DSW(ctx[5] + 0x2Cu) = (u16)(DSW(ctx[5] + 0x2Cu) - 0x40u);
        if ((DSW(ctx[5] + 0x28u) & 0x4000u) != 0u) {
            DSB(ctx[5] + 0x29u) = (u8)(DSB(ctx[5] + 0x29u) & 0xBFu);
            DSW(ctx[5] + 0x34u) = 0x0080u;
        } else {
            DSB(ctx[5] + 0x29u) = (u8)(DSB(ctx[5] + 0x29u) | 0x40u);
            DSW(ctx[5] + 0x34u) = 0xFF80u;
        }
        actors_anim_begin(ctx[5], 0x000E8C82u, 0x40400000u);
        actor_pset_palette(ctx[5], 0x18u, 0x1F874610u);
        DSW(ctx[5] + 0x2Cu) = 0x1000u;
        DSB(ctx[5] + 0x28u) = (u8)(DSB(ctx[5] + 0x28u) | 0x80u);
        (void)sound_voice(0xEAu);
        DSB(slot + 0x53u) = 3u;
        DSB(slot + 0x52u) = 9u;
        DSB(DS_000F0AFE) = 4u;
        DSB(DS_001078FC) = 1u;
    } else {
        fighter_1579c(slot, r[R_EDX], r[R_EBX]);
    }
    *eax = 0u;
}
static void m_23d38(const u32 *r, u32 *eax)            /* case 3 starts the held record first */
{
    u32 slot = r[R_EAX], rec = r[R_EDX], o = DSD(slot + 8u);
    u32 dd = DSD(rec + 0x18u) - DSD(o + 0x18u);
    if ((s32)dd < 0) dd = 0u - dd;
    if (DSB(slot + 0x57u) == 3u && (s32)dd <= 0xB00) {
        DSD(o + 0x18u) = DSD(rec + 0x18u);
        DSD(o + 0x1Cu) = DSD(rec + 0x1Cu);
        DSW(o + 0x34u) = 0u;
        DSW(o + 0x2Cu) = 0x0E00u;
        if ((DSW(rec + 0x28u) & 0x4000u) != 0u) DSB(o + 0x29u) = (u8)(DSB(o + 0x29u) | 0x40u);
        else DSB(o + 0x29u) = (u8)(DSB(o + 0x29u) & 0xBFu);
        actors_anim_begin(o, 0x000E1C0Cu, 0x40400000u);
        actors_anim_begin(rec, 0x000E1BD2u, 0x40400000u);
        DSB(slot + 0x57u) = (u8)(DSB(slot + 0x57u) + 1u);
        (void)sound_voice(0xD6u);
    } else {
        fighter_23d38(slot, rec, r[R_EBX]);
    }
    *eax = 0u;
}

'''

sub("port/src/game/fighter.c",
    "    DSD(slot + 0x1Cu) = 0u;                                 /* 0x4034A */\n    return 1;                                               /* 0x40348 */\n}\n",
    "    DSD(slot + 0x1Cu) = 0u;                                 /* 0x4034A */\n    return 1;                                               /* 0x40348 */\n}\n" + FIGHTER_C)
sub("port/src/game/fighter.h", "int  fighter_402fc(u32 slot, u32 rec);\n", """int  fighter_402fc(u32 slot, u32 rec);
/* The slot +0x0C callbacks the finisher entries store (record §P1.8/§P1.9),
 * as 0x3531C case 7 calls them (slot, rec, side); registered in actors_init. */
void fighter_15584(u32 slot, u32 rec, u32 side);
void fighter_1579c(u32 slot, u32 rec, u32 side);
void fighter_23d38(u32 slot, u32 rec, u32 side);
""")
sub("port/src/game/actors.c", "    fn_register(0x47B04u, (void (*)(void))fighter_47b04);\n",
    """    fn_register(0x47B04u, (void (*)(void))fighter_47b04);
    /* PORT: record 2026-10-02-reverse-p1 §P1.8. The slot +0x0C callbacks
     * the finisher entries 0x1567C, 0x15908 and 0x23EC0 store (the dwords
     * at 0x1569F, 0x1592B and 0x23EE3; 0x3531C case 7, (slot, rec, side)). */
    fn_register(0x15584u, (void (*)(void))fighter_15584);
    fn_register(0x1579Cu, (void (*)(void))fighter_1579c);
    fn_register(0x23D38u, (void (*)(void))fighter_23d38);
""")
sub("port/src/game/actors.c", "void actor_pset_palette(u32 rec, u32 word, u32 handle)\n{\n",
    "void actor_pset_palette(u32 rec, u32 word, u32 handle)\n{\n    PR_SEAM(0x2A17Cu, rec, word, handle);\n")
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_402fc@mutant",     m_402fc,        0x000000FFu },\n',
    """    { "fighter_402fc@mutant",     m_402fc,        0x000000FFu },
    { "fighter_15584",            b_15584,        0x00000000u },
    { "fighter_1579c",            b_1579c,        0x00000000u },
    { "fighter_23d38",            b_23d38,        0x00000000u },
    { "fighter_15584@mutant",     m_15584,        0x00000000u },
    { "fighter_1579c@mutant",     m_1579c,        0x00000000u },
    { "fighter_23d38@mutant",     m_23d38,        0x00000000u },
""")
print("t4_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_15584 fighter_1579c fighter_23d38; do
  python3 tools/diff_verify.py --image /tmp/pr_p1_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected: `all checks passed` and

```
| fighter_15584 | 0x15584 | 9 | 9/9 | VERIFIED | 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified |
| fighter_15584@mutant | 0x15584 | 9 | 9/9 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified |
| fighter_1579c | 0x1579C | 7 | 11/11 | VERIFIED | 2A17C stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified |
| fighter_1579c@mutant | 0x1579C | 7 | 11/11 | MISMATCH | 2A17C stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified |
| fighter_23d38 | 0x23D38 | 14 | 24/24 | VERIFIED | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_23d38@mutant | 0x23D38 | 14 | 24/24 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified |
```

- [ ] **Step 4: the task gate; the E2 table stays.** Task 2 Step 5's command. Expected: `OK` twice and

```
diff-verify: 23/23 functions VERIFIED; 28/28 mutants detected; 1 named gaps; 1/14 rows with callees closed (9 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 317 unported, 178 ported; supplement 131 (31 unported, 0 stale); untrusted entries 30
```

with no `FAIL` line from `entry-triage` (the committed table equals a fresh run).

- [ ] **Step 5: mutation proofs.**

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


mutate("port/src/game/fighter.c",
       "        DSW(DSD(ctx[3] + 4u) + 0x2Cu) = DSW(ctx[5] + 0x2Cu);    /* 0x15647..0x15656 */\n", "")
mutate("port/src/game/fighter.c",
       "            DSD(rec + 0x18u) = DSD(DS_000F0AF0) + 0x3000u;  /* 0x23DAB */",
       "            DSD(rec + 0x18u) = DSD(DS_000F0AF0) - 0x3000u;  /* 0x23DAB */")
mutate("port/src/game/fighter.c",
       "        if (DSW(ctx[5] + 0x2Cu) > 0x400u) {                 /* 0x15842..0x15850 */",
       "        if (DSW(ctx[5] + 0x2Cu) > 0x4C0u) {                 /* 0x15842..0x15850 */")
mutate("port/src/game/actors.c", "    fn_register(0x23D38u, (void (*)(void))fighter_23d38);\n", "")
PY
```

Expected:

```
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:44682: 52428 != 1024']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:44713: 53248 != 77824']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:44700: 52428 != 1216']
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:44666: 0x23D38 is registered']
```

and, the seam: delete the line `    PR_SEAM(0x2A17Cu, rec, word, handle);` from `actors.c`, `cmake --build build`, `python3 tools/diff_verify.py --image /tmp/pr_p1_img.bin --function fighter_1579c | grep 'call #1:' | head -1` prints `  fighter_1579c: c4: call #1: original 0x2A17C(0x10A400, 0x18, 0x1187FAE0), port 0x2C3FC(0xEA)`; restore the line (`git diff port/src/game/actors.c` shows it back) and rebuild.

- [ ] **Step 6: commit.**

```bash
git add port/src/game/fighter.c port/src/game/fighter.h port/src/game/actors.c port/tests/diff_runner.c \
  port/tests/test_fight.c port/tests/test.h tools/diff_verify.py tools/tests/test_diff_verify.py
git commit -m "fighter: port the +0x0C callbacks 0x15584 0x1579C 0x23D38 (after jump tables); seam 0x2A17C (track P batch 1)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: `0x38034`, the +0x0C callbacks `0x23B68` and `0x401D4`; the seam of `0x188AC`

**Files:** modify `tools/diff_verify.py` (helpers; three specs after `fighter_23d38`'s), `tools/tests/test_diff_verify.py`, `port/tests/test_fight.c` (a check before `test_p1_callbacks`, which now runs both), `port/src/game/fighter.c` (append after `fighter_23d38`; the seam as the first statement of `hit_anchor_set`), `fighter.h`, `actors.c` (after `0x23D38`'s registration), `diff_runner.c` (six rows after `fighter_23d38@mutant`), the E2 table.

**Interfaces:** produces `void fighter_38034(u32 side)` (opens with `PR_SEAM(0x38034u, side)`), `void fighter_23b68(u32 slot, u32 rec, u32 side)`, `void fighter_401d4(...)`; `PR_SEAM(0x188ACu, side, x, y)`; `ANCHOR`, `F38034`; `p1_check_callbacks_b`, `p1_check_callbacks`. Consumes `actor_spawn` (seam `0x2AE14`, `SPAWN`, a C pointer into `mem[]` reported as its offset), `hit_anchor_set`; the `sc_seed` fixture (a pool for `0x38034`'s spawn). Record §P1.9; the `idiv` named gap §P1.10.

- [ ] **Step 1: specs and expectations first.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


HELPERS = r'''# 0x188AC (hit_anchor_set): EAX = side, EDX = x, EBX = y, a plain `ret`, clobbers EDX; 0x38034: EAX = side,
# a plain `ret`, saves EBX, ECX, EDX (record §P1.4).
ANCHOR = E.Call(0x188AC, ("eax", "edx", "ebx"), clobbers=("edx",))
F38034 = E.Call(0x38034, ("eax",))


'''
SPECS = r'''    # 0x38034 (record §P1.9): side 0 is character 2, side 1 character 4; the spawn stub's EAX is the record
    # whose +0x59 0x38034 sets, a different one per case.
    Spec("fighter_38034", 0x38034, [
        Case("s%d" % side, {"eax": side},
             {DS_SLOTS + side * 0x94: le32(E3_REC), DS_SLOTS + side * 0x94 + 0x7A: bytes([2 + 2 * side]),
              E3_REC + 0x28: le32(w28)[:2], E3_REC + 0x56: b"\x23\x01", sp + 0x59: b"\x59"},
             {0x2AE14: sp})
        for side, w28, sp in ((0, 0x4000, E3_REC2), (1, 0xBFFF, E3_OUT))
    ], calls=(ANIM_BEGIN, SPAWN), eax_mask=0),
    # 0x23B68 (record §P1.9): +0x57 0 and 2 do nothing; 1 with rec+0x1C set only divides; d = 3 and -3 pin
    # the truncating signed division. The slot's +4 record is E3_OUT.
    Spec("fighter_23b68", 0x23B68, [
        Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0},
             {E3_SLOT + 0x52: bytes([0x52, 0x53, 0x54, 0x55, 0x56, st]), E3_SLOT + 4: le32(E3_OUT),
              E3_REC + 0x18: le32(0x12345), E3_REC + 0x1C: le32(y), E3_REC + 0x2C: b"\xcc\xcc",
              E3_REC + 0x30: le32(d << 16), E3_REC + 0x38: b"\x38\x38", E3_OUT + 0x2C: b"\xcc\xcc",
              0xF0AFE: b"\xfe", 0x1078FC: b"\xfc"})
        for cid, st, y, d in (("q0", 0, 0, 3), ("q2", 2, 0, 3), ("q1", 1, 0x77, 3), ("q3", 1, 0, 3),
                              ("q4", 1, 0, -3 & 0xFFFF))
    ], calls=(ANIM_BEGIN, SPAWN, VOICE), eax_mask=0),
    # 0x401D4 (record §P1.9): 0x33950 runs on both sides; EDX (rec) is E3_OUT, not the side's record E3_REC
    # (ctx[4]), so a port that confuses them differs. Side 0's slot has character 1 (threshold 0x1400).
    Spec("fighter_401d4", 0x401D4, [
        Case(cid, {"eax": E3_SLOT, "edx": E3_OUT, "ebx": 0},
             {**SLOT_PTRS, E3_SLOT + 0x52: bytes([0x52, 0x53, 0x54, 0x55, 0x56, st]),
              DS_SLOTS + 0x7A: b"\x01", DS_SLOTS + 0x30: le32(s30), DS_SLOTS + 0x54: b"\x54\x55\x56\x57",
              E3_REC + 0x36: le32(r36)[:2], E3_OUT + 0x18: le32(0x5678), E3_OUT + 0x30: le32(0x00070000),
              E3_OUT + 0x34: le32(o36 << 16 | 0x3434), E3_OUT + 0x44: b"\x44\x44",
              0x1078FD: b"\x01", 0x105B3A: bytes([b3a]), 0x1078FC: b"\xfc"})
        for cid, st, s30, r36, o36, b3a in (
            ("t0", 0, 0, 0, 0x0001, 0), ("t1", 0, 0, 0, 0x8000, 0),
            ("t2", 1, 0x1400, 0x8000, 0, 0), ("t3", 1, 0x13FF, 0x7FFF, 0, 0), ("t4", 1, 0x13FF, 0x8000, 0, 0),
            ("t5", 2, 0, 0, 0, 0), ("t6", 3, 0, 0, 0, 1), ("t7", 3, 0, 0, 0, 2), ("t8", 4, 0, 0, 0, 0))
    ], allow_calls=(0x33950,), calls=(ANCHOR, ANIM_BEGIN, F38034, VOICE, SPAWN), eax_mask=0),
'''

V = "tools/diff_verify.py"
sub(V, "\n\nP1_SPECS = [\n", "\n\n" + HELPERS + "P1_SPECS = [\n")
sub(V, "    ], calls=(ANIM_BEGIN, VOICE), eax_mask=0),\n]\n",
    "    ], calls=(ANIM_BEGIN, VOICE), eax_mask=0),\n" + SPECS + "]\n")
T = "tools/tests/test_diff_verify.py"
sub(T, """            "fighter_15584": 0, "fighter_1579c": 0, "fighter_23d38": 0}""",
    """            "fighter_15584": 0, "fighter_1579c": 0, "fighter_23d38": 0,
            "fighter_38034": 0, "fighter_23b68": 0, "fighter_401d4": 0}""")
sub(T, """            "fighter_23d38@mutant": {"call #0", "call #1"}}""",
    """            "fighter_23d38@mutant": {"call #0", "call #1"},
            "fighter_38034@mutant": {"call #1"}, "fighter_23b68@mutant": {"call #1"},
            "fighter_401d4@mutant": {"call #0", "call #1"}}""")
sub(T, """0x1A570: (), 0x2A17C: ("edx",)})""", """0x1A570: (), 0x2A17C: ("edx",),
                                 0x188AC: ("edx",), 0x38034: ()})""")
sub(T, """        self.assertIn("diff-verify: 23/23 functions VERIFIED; 28/28 mutants detected; 1 named gaps; "
                      "1/14 rows with callees closed (9 have none).", out.getvalue())""",
    """        self.assertIn("diff-verify: 26/26 functions VERIFIED; 31/31 mutants detected; 1 named gaps; "
                      "1/17 rows with callees closed (9 have none).", out.getvalue())""")
print("t5_spec applied")
PY
for f in fighter_38034 fighter_23b68 fighter_401d4; do
  python3 tools/diff_verify.py --image /tmp/pr_p1_img.bin --function $f | grep -E '^\| fighter|unknown binding' | head -2; done
```

Expected:

```
t5_spec applied
| fighter_38034 | 0x38034 | 2 | 3/3 | MISMATCH | 2AE14 stub unverified, 2BC30 stub unverified |
  fighter_38034: s0: port: unknown binding fighter_38034
| fighter_23b68 | 0x23B68 | 5 | 5/5 | MISMATCH | 2AE14 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified |
  fighter_23b68: q0: port: unknown binding fighter_23b68
| fighter_401d4 | 0x401D4 | 9 | 14/14 | MISMATCH | 188AC stub unverified, 2AE14 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 38034 stub unverified |
  fighter_401d4: t0: port: unknown binding fighter_401d4
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


TEST = r'''/* §P1.9: 0x23B68 and 0x401D4 through their registrations, and 0x38034
 * through 0x401D4's case 3 (sc_seed's pool, so 0x38034's spawn runs). */
static void p1_check_callbacks_b(void)
{
    u32 tgt = FIGHT_RECS + 0x200u;
    p1_cb_fn f;
    CHECK(fn_resolve(0x23B68u) == (void (*)(void))fighter_23b68, "0x23B68 is registered");
    CHECK(fn_resolve(0x401D4u) == (void (*)(void))fighter_401d4, "0x401D4 is registered");
    CHECK_EQ_INT((int)DSD(0x00023C75u), 0x00023B68);
    CHECK_EQ_INT((int)DSD(0x0004033Du), 0x000401D4);
    CHECK_EQ_INT((int)(DSD(0x000402A8u) + 0x402ACu), 0x00038034);   /* 0x402A7's rel32 */

    /* 0x23B68 with +0x57 = 1 and rec+0x1C set: 0x400000 / 3 = 0x155555 into
     * both words +0x2C; nothing else (DS_001078FC kept). */
    f = (p1_cb_fn)(void *)fn_resolve(0x23B68u);
    if (f == NULL) return;
    z_fseed();
    DSD(Z_S0 + 4u) = tgt;
    DSW(tgt + 0x2Cu) = 0xCCCCu;
    DSB(Z_S0 + 0x57u) = 1u;
    DSD(Z_R0 + 0x30u) = 0x00030000u;
    DSD(Z_R0 + 0x1Cu) = 0x77u;
    DSB(DS_001078FC) = 0xFCu;
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSW(Z_R0 + 0x2Cu), 0x5555);
    CHECK_EQ_INT((int)DSW(tgt + 0x2Cu), 0x5555);
    CHECK_EQ_INT((int)DSB(DS_001078FC), 0xFC);

    /* 0x401D4 case 0, the word +0x36 negative: 0xFFFF, +0x44 = 0x3C, +0x57 = 1. */
    f = (p1_cb_fn)(void *)fn_resolve(0x401D4u);
    if (f == NULL) return;
    z_fseed();
    DSB(Z_S0 + 0x57u) = 0u;
    DSW(Z_R0 + 0x36u) = 0x8000u;
    DSW(Z_R0 + 0x44u) = 0x4444u;
    f(Z_S0, Z_R0, 0u);
    CHECK_EQ_INT((int)DSW(Z_R0 + 0x36u), 0xFFFF);
    CHECK_EQ_INT((int)DSW(Z_R0 + 0x44u), 0x3C);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 1);

    /* 0x401D4 case 3 with DS_00105B3A = 2 (no spawn of its own): 0x38034 on
     * side 1 (character 4) starts that side's record on 0xBDD00[4] at 1.0 and
     * spawns; the voices 0x46 then 0x6C; the slot 3/9, DS_001078FC = 1. */
    sc_seed(2u, 4u, 0);
    DSW(DSD(0x000BDD00u + 4u * 4u)) = 0x12B5u;
    DSB(DS_001078FD) = 1u;
    DSB(DS_00105B3A) = 2u;
    DSB(DS_001078FC) = 0xFCu;
    DSB(Z_S0 + 0x57u) = 3u;
    DSB(Z_S0 + 0x53u) = 0x55u;
    DSB(Z_S0 + 0x52u) = 0x55u;
    sound_voice_log_reset();
    f(Z_S0, DSD(Z_S0), 0u);
    CHECK_EQ_INT((int)DSD(DSD(Z_S1) + 8u), (int)DSD(0x000BDD00u + 4u * 4u));
    CHECK_EQ_INT((int)DSD(DSD(Z_S1) + 0x24u), 0x3F800000);
    CHECK_EQ_INT((int)sound_voice_log_count(), 2);
    CHECK_EQ_INT((int)sound_voice_log_at(0), 0x46);
    CHECK_EQ_INT((int)sound_voice_log_at(1), 0x6C);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 3);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(DS_001078FC), 1);
    sound_voice_log_reset();
}

'''

F = "port/tests/test_fight.c"
sub(F, "int test_p1_callbacks(void)     { return u6b_run(p1_check_callbacks_a); }\n",
    TEST + """static void p1_check_callbacks(void)
{
    p1_check_callbacks_a();
    p1_check_callbacks_b();
}

int test_p1_callbacks(void)     { return u6b_run(p1_check_callbacks); }
""")
print("t5_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected:

```
port/tests/test_fight.c:44723:51: error: use of undeclared identifier 'fighter_23b68'
port/tests/test_fight.c:44724:51: error: use of undeclared identifier 'fighter_401d4'
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
#define P1_38034_STREAM 0x000BDD00u  /* 0x38055: [char] dword, the record's stream */
#define P1_38034_DESC   0x000BDD1Cu  /* 0x380B0: [char] dword, a spawn descriptor */

/* 0x38034 — record §P1.9. 0x401D4's case 3 call (0x402A7, EAX = the byte
 * DS_001078FD, a side; its only caller). The side's record on
 * 0xBDD00[char] at 1.0 (0x2BC30); then 0x2AE14(0xBDD1C[char], 0, 0, 0,
 * (rec+0x28 bit 14 ? 0x4000 : 0) | the word rec+0x56 | 0x400) and the spawned
 * record's +0x59 = 1. The character byte is read again after 0x2BC30
 * (0x3809E). EAX at return is 0x2AE14's; the caller loads EAX at once (`mov
 * eax,0x46` at 0x402AC), so the port returns nothing. */
void fighter_38034(u32 side)
{
    u32 slot, rec, flag, sp;
    PR_SEAM(0x38034u, side);
    slot = DS_001077B0 + side * 0x94u;
    rec = DSD(slot);                                        /* 0x38037..0x38045 */
    actors_anim_begin(rec, DSD(P1_38034_STREAM + (u32)DSB(slot + 0x7Au) * 4u),
                      0x3F800000u);                         /* 0x3804C..0x38061 0x2BC30 */
    flag = (DSW(rec + 0x28u) & 0x4000u) != 0u ? 0x4000u : 0u;   /* 0x38066..0x38076 */
    sp = actor_spawn((const u32 *)(mem + DSD(P1_38034_DESC + (u32)DSB(slot + 0x7Au) * 4u)),
                     0u, 0u, 0u,
                     flag | ((u32)DSW(rec + 0x56u) | 0x400u));  /* 0x3807B..0x380B7 0x2AE14 */
    DSB(sp + 0x59u) = 1u;                                   /* 0x380BC */
}

#define P1_23B68_STREAM 0x000E87ACu  /* 0x23BA5 */
#define P1_23B68_DESC   0x000A83B0u  /* 0x23BC7 */

/* 0x23B68 — record §P1.9. The slot +0x0C callback 0x23BF8 stores (the
 * immediate at 0x23C75; an E2 supplement entry). EAX = slot, EDX = rec, EBX
 * not read. Only +0x57 == 1 acts (`test al,al; jbe`, `cmp al,1; jne`): the
 * word 0x400000 / (rec+0x30 >> 16) (signed `idiv`) goes to the record's +0x2C
 * and to +0x2C of the record at the slot's +4; then, with rec+0x1C zero, the
 * word rec+0x38 = 0, the record on 0xE87AC at 1.0, 0x2AE14(0xA83B0, rec+0x18,
 * rec+0x30 >> 16, rec+0x1C, 0) (the fields read again after 0x2BC30), the
 * voice 0x5C, the slot's +0x53 = 3, DS_001078FC = 1 (DL, which 0x2C3FC
 * preserves), +0x52 = 9 and DS_000F0AFE = 4. PORT: the raw's `idiv` faults
 * (#DE) on a zero divisor; the port leaves the quotient 0 there (named gap
 * §P1.10; no case divides by zero). */
void fighter_23b68(u32 slot, u32 rec, u32 side)
{
    s32 d, q;
    (void)side;
    if (DSB(slot + 0x57u) != 1u) return;                    /* 0x23B6E..0x23B7B */
    d = (s32)DSD(rec + 0x30u) >> 16;                        /* 0x23B82..0x23B87 */
    q = d != 0 ? (s32)0x400000 / d : 0;                     /* 0x23B7D..0x23B8D */
    DSW(rec + 0x2Cu) = (u16)q;                              /* 0x23B8F */
    DSW(DSD(slot + 4u) + 0x2Cu) = (u16)q;                   /* 0x23B93..0x23B96 */
    if (DSD(rec + 0x1Cu) != 0u) return;                     /* 0x23B9A..0x23B9E */
    DSW(rec + 0x38u) = 0u;                                  /* 0x23BAC */
    actors_anim_begin(rec, P1_23B68_STREAM, 0x3F800000u);   /* 0x23BA0..0x23BB2 0x2BC30 */
    (void)actor_spawn((const u32 *)(mem + P1_23B68_DESC), DSD(rec + 0x18u),
                      (u32)((s32)DSD(rec + 0x30u) >> 16), DSD(rec + 0x1Cu),
                      0u);                                  /* 0x23BB7..0x23BCC 0x2AE14 */
    (void)sound_voice(0x5Cu);                               /* 0x23BD1..0x23BD8 0x2C3FC */
    DSB(slot + 0x53u) = 3u;                                 /* 0x23BDF */
    DSB(DS_001078FC) = 1u;                                  /* 0x23BE3 */
    DSB(slot + 0x52u) = 9u;                                 /* 0x23BE9 */
    DSB(DS_000F0AFE) = 4u;                                  /* 0x23BED */
}

#define P1_401D4_THR    0x000BD882u  /* 0x40234: [char*2] dword >> 16, a signed x bound */
#define P1_401D4_STREAM 0x000E876Au  /* 0x40265 */
#define P1_401D4_DESC   0x000BB0ECu  /* 0x402D1 */

/* 0x401D4 — record §P1.9. The slot +0x0C callback 0x402FC stores (the
 * immediate at 0x4033D; an E2 supplement entry). EAX = slot, EDX = rec, EBX =
 * side; the context is 0x33950(side). On the slot's +0x57: 0 with the
 * record's word +0x36 negative sets it 0xFFFF, +0x44 = 0x3C and +0x57 = 1;
 * 1 with the dword at 0xBD882 + 2 * ctx[2]'s char, shifted right 16 (signed),
 * above ctx[2]'s +0x30 and ctx[4]'s word +0x36 negative: 0x188AC(ctx[0],
 * rec+0x18, 0), ctx[4] on 0xE876A at 3.0, the record's words +0x34/+0x36/
 * +0x44 = 0, ctx[2]'s +0x54 = 0 and +0x57 = 3; 3: 0x38034(DS_001078FD), the
 * voice 0x46, below 2 in DS_00105B3A 0x2AE14(0xBB0EC, rec+0x18, rec+0x30 >>
 * 16, 0, 0), the voice 0x6C, the slot's +0x53 = 3, +0x52 = 9 and
 * DS_001078FC = 1; 2 and above 3 do nothing. */
void fighter_401d4(u32 slot, u32 rec, u32 side)
{
    u32 ctx[6];
    u8 st;
    fighter_ctx_same(ctx, side);                            /* 0x401DB..0x401E1 0x33950 */
    st = DSB(slot + 0x57u);                                 /* 0x401E6 */
    if (st == 0u) {                                         /* 0x401E9..0x401FE */
        if ((s16)DSW(rec + 0x36u) >= 0) return;             /* 0x40204..0x40209 */
        DSW(rec + 0x36u) = 0xFFFFu;                         /* 0x4020F */
        DSW(rec + 0x44u) = 0x003Cu;                         /* 0x40215 */
        DSB(slot + 0x57u) = 1u;                             /* 0x4021B */
        return;
    }
    if (st == 1u) {                                         /* 0x401ED */
        s32 thr = (s32)DSD(P1_401D4_THR + (u32)DSB(ctx[2] + 0x7Au) * 2u) >> 16;  /* 0x40224..0x4023E */
        if (thr <= (s32)DSD(ctx[2] + 0x30u)) return;        /* 0x40241..0x40243 */
        if ((s16)DSW(ctx[4] + 0x36u) >= 0) return;          /* 0x40249..0x40252 */
        hit_anchor_set(ctx[0], DSD(rec + 0x18u), 0u);       /* 0x40258..0x40260 0x188AC */
        actors_anim_begin(ctx[4], P1_401D4_STREAM, 0x40400000u);  /* 0x40265..0x40273 0x2BC30 */
        DSW(rec + 0x34u) = 0u;                              /* 0x40278 */
        DSW(rec + 0x36u) = 0u;                              /* 0x4027E */
        DSW(rec + 0x44u) = 0u;                              /* 0x40284 */
        DSB(ctx[2] + 0x54u) = 0u;                           /* 0x4028A..0x4028E */
        DSB(ctx[2] + 0x57u) = 3u;                           /* 0x40292..0x40296 */
        return;
    }
    if (st != 3u) return;                                   /* 0x401EF..0x401F7 */
    fighter_38034(DSB(DS_001078FD));                        /* 0x402A0..0x402A7 0x38034 */
    (void)sound_voice(0x46u);                               /* 0x402AC/0x402B1 0x2C3FC */
    if (DSB(DS_00105B3A) < 2u)                              /* 0x402B6..0x402C0 */
        (void)actor_spawn((const u32 *)(mem + P1_401D4_DESC), DSD(rec + 0x18u),
                          (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u);  /* 0x402C2..0x402D6 0x2AE14 */
    (void)sound_voice(0x6Cu);                               /* 0x402DB/0x402E0 0x2C3FC */
    DSB(slot + 0x53u) = 3u;                                 /* 0x402E5 */
    DSB(slot + 0x52u) = 9u;                                 /* 0x402EB */
    DSB(DS_001078FC) = 1u;                                  /* 0x402EF */
}
'''
BINDINGS = r'''/* Track P batch 1 (record 2026-10-02-reverse-p1 §P1.9): 0x23B68 and 0x401D4 as the other +0x0C callbacks
 * (mask 0, §P1.8), and 0x38034 (EAX = side; mask 0: its only caller, 0x402A7, loads EAX at once). */
static void b_23b68(const u32 *r, u32 *eax)            { fighter_23b68(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_401d4(const u32 *r, u32 *eax)            { fighter_401d4(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
static void b_38034(const u32 *r, u32 *eax)            { fighter_38034(r[R_EAX]); *eax = 0u; }
static void m_38034(const u32 *r, u32 *eax)            /* the spawn word without bit 10 */
{
    u32 slot = DS_001077B0 + r[R_EAX] * 0x94u, rec = DSD(slot);
    actors_anim_begin(rec, DSD(0x000BDD00u + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
    u32 flag = (DSW(rec + 0x28u) & 0x4000u) != 0u ? 0x4000u : 0u;
    u32 sp = actor_spawn((const u32 *)(mem + DSD(0x000BDD1Cu + (u32)DSB(slot + 0x7Au) * 4u)),
                         0u, 0u, 0u, flag | (u32)DSW(rec + 0x56u));
    DSB(sp + 0x59u) = 1u;
    *eax = 0u;
}
static void m_23b68(const u32 *r, u32 *eax)            /* the spawn's x and y swapped */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    if (DSB(slot + 0x57u) == 1u && DSD(rec + 0x1Cu) == 0u) {
        s32 q = (s32)0x400000 / ((s32)DSD(rec + 0x30u) >> 16);
        DSW(rec + 0x2Cu) = (u16)q;
        DSW(DSD(slot + 4u) + 0x2Cu) = (u16)q;
        DSW(rec + 0x38u) = 0u;
        actors_anim_begin(rec, 0x000E87ACu, 0x3F800000u);
        (void)actor_spawn((const u32 *)(mem + 0x000A83B0u), DSD(rec + 0x1Cu),
                          (u32)((s32)DSD(rec + 0x30u) >> 16), DSD(rec + 0x18u), 0u);
        (void)sound_voice(0x5Cu);
        DSB(slot + 0x53u) = 3u;
        DSB(DS_001078FC) = 1u;
        DSB(slot + 0x52u) = 9u;
        DSB(DS_000F0AFE) = 4u;
    } else {
        fighter_23b68(slot, rec, r[R_EBX]);
    }
    *eax = 0u;
}
static void m_401d4(const u32 *r, u32 *eax)            /* case 3's 0x38034 after the first voice */
{
    u32 slot = r[R_EAX], rec = r[R_EDX];
    if (DSB(slot + 0x57u) == 3u) {
        u32 ctx[6];
        fighter_ctx_same(ctx, r[R_EBX]);
        (void)sound_voice(0x46u);
        fighter_38034(DSB(DS_001078FD));
        if (DSB(DS_00105B3A) < 2u)
            (void)actor_spawn((const u32 *)(mem + 0x000BB0ECu), DSD(rec + 0x18u),
                              (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u);
        (void)sound_voice(0x6Cu);
        DSB(slot + 0x53u) = 3u;
        DSB(slot + 0x52u) = 9u;
        DSB(DS_001078FC) = 1u;
    } else {
        fighter_401d4(slot, rec, r[R_EBX]);
    }
    *eax = 0u;
}

'''

sub("port/src/game/fighter.c",
    "    default:                                                /* 0x23EBD: 4, above 5 */\n        return;\n    }\n}\n",
    "    default:                                                /* 0x23EBD: 4, above 5 */\n        return;\n    }\n}\n" + FIGHTER_C)
sub("port/src/game/fighter.c", "void hit_anchor_set(u32 side, u32 x, u32 y)\n{\n",
    "void hit_anchor_set(u32 side, u32 x, u32 y)\n{\n    PR_SEAM(0x188ACu, side, x, y);\n")
sub("port/src/game/fighter.h", "void fighter_23d38(u32 slot, u32 rec, u32 side);\n", """void fighter_23d38(u32 slot, u32 rec, u32 side);
void fighter_23b68(u32 slot, u32 rec, u32 side);
void fighter_401d4(u32 slot, u32 rec, u32 side);
/* 0x38034 (record §P1.9): the call of 0x401D4's case 3, EAX = a side. */
void fighter_38034(u32 side);
""")
sub("port/src/game/actors.c", "    fn_register(0x23D38u, (void (*)(void))fighter_23d38);\n",
    """    fn_register(0x23D38u, (void (*)(void))fighter_23d38);
    /* PORT: record 2026-10-02-reverse-p1 §P1.9. The +0x0C callbacks the
     * entries 0x23BF8 and 0x402FC store (the dwords at 0x23C75 and 0x4033D;
     * 0x3531C case 7, (slot, rec, side)). */
    fn_register(0x23B68u, (void (*)(void))fighter_23b68);
    fn_register(0x401D4u, (void (*)(void))fighter_401d4);
""")
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", BINDINGS + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_23d38@mutant",     m_23d38,        0x00000000u },\n',
    """    { "fighter_23d38@mutant",     m_23d38,        0x00000000u },
    { "fighter_38034",            b_38034,        0x00000000u },
    { "fighter_23b68",            b_23b68,        0x00000000u },
    { "fighter_401d4",            b_401d4,        0x00000000u },
    { "fighter_38034@mutant",     m_38034,        0x00000000u },
    { "fighter_23b68@mutant",     m_23b68,        0x00000000u },
    { "fighter_401d4@mutant",     m_401d4,        0x00000000u },
""")
print("t5_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_38034 fighter_23b68 fighter_401d4; do
  python3 tools/diff_verify.py --image /tmp/pr_p1_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected: `all checks passed` and

```
| fighter_38034 | 0x38034 | 2 | 3/3 | VERIFIED | 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_38034@mutant | 0x38034 | 2 | 3/3 | MISMATCH | 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_23b68 | 0x23B68 | 5 | 5/5 | VERIFIED | 2AE14 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_23b68@mutant | 0x23B68 | 5 | 5/5 | MISMATCH | 2AE14 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_401d4 | 0x401D4 | 9 | 14/14 | VERIFIED | 188AC stub unverified, 2AE14 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 38034 stub unverified |
| fighter_401d4@mutant | 0x401D4 | 9 | 14/14 | MISMATCH | 188AC stub unverified, 2AE14 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 38034 stub unverified |
```

- [ ] **Step 4: the E2 table.** `make entry-triage E2_IMAGE=/tmp/pr_p1_e2.bin` fails; regenerate (Task 2 Step 4's command). Expected:

```
entry-triage: targets 317 unported, 178 ported; supplement 131 (28 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 40 in unported code, 75 in ported code, 19 nowhere
```

(the supplement rows `23B68`, `38034`, `401D4` and the voice sites `23BD8 402B1 402E0` turn `yes`).

- [ ] **Step 5: the task gate.** Expected: `OK` twice and

```
diff-verify: 26/26 functions VERIFIED; 31/31 mutants detected; 1 named gaps; 1/17 rows with callees closed (9 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 317 unported, 178 ported; supplement 131 (28 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 6: mutation proofs.**

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


mutate("port/src/game/fighter.c",
       "                      0x3F800000u);                         /* 0x3804C..0x38061 0x2BC30 */",
       "                      0x40400000u);                         /* 0x3804C..0x38061 0x2BC30 */")
mutate("port/src/game/fighter.c",
       "    q = d != 0 ? (s32)0x400000 / d : 0;                     /* 0x23B7D..0x23B8D */",
       "    q = d != 0 ? (s32)0x400000 / (d + 1) : 0;               /* 0x23B7D..0x23B8D */")
mutate("port/src/game/fighter.c",
       "        DSW(rec + 0x44u) = 0x003Cu;                         /* 0x40215 */",
       "        DSW(rec + 0x44u) = 0x0030u;                         /* 0x40215 */")
mutate("port/src/game/actors.c", "    fn_register(0x401D4u, (void (*)(void))fighter_401d4);\n", "")
PY
```

Expected:

```
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:44771: 1077936128 != 1065353216']
port/src/game/fighter.c: 2 FAIL, first: ['test_fight.c:44741: 0 != 21845']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:44754: 48 != 60']
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:44724: 0x401D4 is registered']
```

and, the seam: delete `    PR_SEAM(0x188ACu, side, x, y);` from `fighter.c`, rebuild, `python3 tools/diff_verify.py --image /tmp/pr_p1_img.bin --function fighter_401d4 | grep 'call #0:' | head -1` prints `  fighter_401d4: t4: call #0: original 0x188AC(0x0, 0x5678, 0x0), port 0x2BC30(0x10A300, 0xE876A, 0x40400000)`; restore it and rebuild.

- [ ] **Step 7: commit.**

```bash
git add port/src/game/fighter.c port/src/game/fighter.h port/src/game/actors.c port/tests/diff_runner.c \
  port/tests/test_fight.c tools/diff_verify.py tools/tests/test_diff_verify.py \
  docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git commit -m "fighter: port 0x38034 and the +0x0C callbacks 0x23B68 0x401D4; seam 0x188AC (track P batch 1)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: the full gate, PROGRESS, the record

**Files:** modify `docs/PROGRESS.md` (append one paragraph), `docs/superpowers/plans/2026-10-02-reverse-p1-derivations.md` (append the closure to §P1.11). `README.md` is not touched: `port_progress.py` stays `771 1203 64` (the twelve are not Ghidra functions; roadmap section).

**Interfaces:** consumes Tasks 2-5's head.

- [ ] **Step 1: the full gate.**

```bash
V=/tmp/pr_p1_final; mkdir -p $V
make verify SMK_DUMP=$V/smk TITLE_DUMP=$V/title ATTRACT_DUMP=$V/att FRONTEND_DUMP=$V/fe TITLE_PIN_DIR=$V/pin \
  AUDIO_WAV=$V/a.wav K11_DUMP=$V/k11 GP_DUMP=$V/gp DIFF_IMAGE=$V/diffimg DIFF_TABLE=$V/diff.md E2_IMAGE=$V/e2.bin \
  > /tmp/pr_p1_final.log 2>&1; echo "EXIT=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_p1_final.log \
  | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
grep -E '^diff-verify:|^entry-triage: (targets|voice)' /tmp/pr_p1_final.log
make audio-render AUDIO_WAV=$V/a.wav >/dev/null 2>&1; cmp $V/a.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-SAME
python3 tools/port_progress.py
grep -rln 'pr_seam = ' port
```

Expected: `EXIT=0` (the replay took 49 min 46 s on a host shared with other `make verify` runs; the prototype 14 min 29 s), `ORACLES-EQUAL`, `WAV-SAME`, the gameplay ratchets as at the baseline (`gp-idle-loss` N 2064 / 8320, `gp-u5-charsel` 516 / 1513, `gp-u6-moves-b` 1005 / 2262 / 2949, `gp-keys-fight` 11; each skips without its capture), and

```
diff-verify: 26/26 functions VERIFIED; 31/31 mutants detected; 1 named gaps; 1/17 rows with callees closed (9 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 317 unported, 178 ported; supplement 131 (28 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 40 in unported code, 75 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
port/tests/diff_runner.c
port/tests/test_platform.c
```

The last two lines: the seam hook is set only by `diffrun` and inside `test_call_seam` (every seam this plan adds is inert in the game).

- [ ] **Step 2: PROGRESS.** Append this paragraph to `docs/PROGRESS.md` (a blank line before it):

```
**Track P batch 1 (P1): the finisher entries and their +0x0C callbacks (record `2026-10-02-reverse-p1-derivations.md`).** The six finisher entries U0 left unregistered (`0x1567C 0x15908 0x23BF8 0x23EC0 0x402FC 0x45D14`, spec O4) are ported and registered, so `0x379C4` now runs them where it fell back to the `0xC9260[char]` start; with them the five slot +0x0C callbacks they store (`0x15584 0x1579C 0x23D38 0x23B68 0x401D4`) and `0x401D4`'s callee `0x38034`. Three of the callbacks start right after a jump table, a shape E2's list cannot see: a scan finds 27 such functions outside Ghidra, 13 unported (record §P1.2), and the untrusted entry `0x158ED` turns out to be a block of `0x1579C`. Each of the twelve is differentially verified (every block hit; callees stubbed: `0x2BC30`, `0x2C3FC`, `0x3C4CC`, `0x2AE14`, and the newly seamed `0x1A570`, `0x2A17C`, `0x188AC`; `0x1A570` has its own row), with a mutant only the call list or the memory at a call catches; `make diff-verify`: `26/26 functions VERIFIED; 31/31 mutants detected; 1 named gaps; 1/17 rows with callees closed (9 have none)`. E2 table: 317 unported / 178 ported targets, finishers 0 / 9, supplement 28 unported. Named gaps: `0x23B68`'s `idiv` faults on a zero divisor where the port leaves 0; `0x23BF8`'s main-path EAX upper bits; no capture reaches the finisher path, so its screen is claimed only by the differential rows. The P track's batch sequence is in the record (§P1.3): 8 porting batches, 143 functions, two callee-row batches. `port_progress.py` stays `771 1203 64` (none of the twelve is a Ghidra function).
```

- [ ] **Step 3: the record's closure.** Append to `docs/superpowers/plans/2026-10-02-reverse-p1-derivations.md` §P1.11, after its last paragraph, one line with the measured values of Step 1 (the commit hash of Task 5, `EXIT`, `ORACLES-EQUAL`, `WAV-SAME`, the counter line, the two triage lines, the `port_progress.py` lines), in this form:

```
**Closure (Task 6, on `<Task 5's commit>`):** `make verify` EXIT=0 (<minutes> min), the 45 oracle lines equal to `oracle-lines-base.txt`, the `make audio-render` WAV equal to `before-t2.wav`, `diff-verify: 26/26 functions VERIFIED; 31/31 mutants detected; 1 named gaps; 1/17 rows with callees closed (9 have none)`, `entry-triage: targets 317 unported, 178 ported; supplement 131 (28 unported, 0 stale)`, voice `40 / 75 / 19`, `771 1203 64` / `731 731 100`.
```

Any value that differs from the expected ones of Step 1 is a finding: write the measured value, and say in the ledger why it differs.

- [ ] **Step 4: commit.**

```bash
git add docs/PROGRESS.md docs/superpowers/plans/2026-10-02-reverse-p1-derivations.md
git commit -m "docs: track P batch 1 closure (PROGRESS, record)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

## Execution notes

- **Order:** Task 1, then 2, 3, 4, 5 strictly in order (each script's anchors are the previous task's text), then 6.
- **Model tiers:** Task 1 and Task 6 are mechanical (a fast model); Tasks 2-5 apply verified scripts but a reviewer must check each C function against the record's decode (§P1.5-§P1.9) and that every row is `VERIFIED` from the real bytes, not adjusted: a capable model for the implementer of Task 3 (the full-EAX early return, the 1A570 stub values) and Task 5 (the `idiv` gap, `ctx[2]` vs the slot argument), a standard one for Tasks 2 and 4.
- **Gate:** per task, Step 5's (or Task 4's Step 4) `make diff-verify entry-triage` plus `PR_ORACLE_REQUIRED=1 ./build/run_tests`; the full `make verify` at Task 1, Task 6 and before the merge (user ruling). A `MISMATCH`, `PARTIAL` or `NOT_EXERCISABLE` row is a finding to report, never a spec to loosen.
- **Merging with U9/U10:** the shared files are `fighter.c` (appended at its end), `test_fight.c` (appended), `test.h`, `actors.c` (`actors_init`), `diff_runner.c`, `diff_verify.py`, `test_diff_verify.py` and the E2 table. Resolve a conflict in the E2 table by regenerating it; in the others keep both sides' additions.
