# Gameplay U9 (the win path) and U10 (the endings) under pokes — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Capture the pinned original winning a match (U9) and reaching an ending (U10) inside a bounded capture by writing a few bytes of game memory at chosen frames ("pokes"). Replay the same pokes in the port at the same frames, prove from each capture that it took the raw-derived path, and pin frame, trace, milestone and win-field ratchets (`make gp-win-oracle`, `make gp-ending-oracle`, both in `make verify`, both skipping without their capture).

**Architecture:** A backward-compatible harness extension. `tools/gp_session.py` gains a step keyed on the k-th entry into a mode (`after_entry`) and a `poke` action, logged as `W` records and emitted in the port script as `poke` lines. `tools/gp_capture.py` writes the bytes in the master loop's spin and refuses a write that raced the iteration. The `PR_GP_DUMP` driver applies `poke` lines and its `T` line carries the win fields. A new checker `tools/gp_win.py` holds the raw-derived milestones per scenario, as evidence on the capture and as a ratchet on the port. Two scenarios, `gp-u9-win` and `gp-u10-ending`, are each captured once. No file under `port/src` changes.

**Why one combined plan:** U9 and U10 share Tasks 0–7 (the poke step, the capture side, the driver, the milestone checker and the Makefile targets). They differ only in a scenario, a capture and its pins (Tasks 8–9 against 10–11). The spec orders U9 before U10, and this plan keeps that order.

**Tech Stack:** Python 3 stdlib (`unittest`), the U1–U4/U7 gp tools, DOSBox-X 2026.08.31, the port build (CMake, C11), `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 G ("then U9 win path under pokes, U10 endings"), §6 ("U9, U10 ratcheted under pokes"), §7 ("Wins and endings without pokes" stays a named gap); `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md` §3.1 (the spin), §4.1–§4.3, §5, §7 Q7.

**Derivation record:** `docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md`. §W.0–§W.9 were written while planning on main `b5beff9`, and the tasks append §W.10 onward. The record covers the defaults the path depends on (§W.1), the win fields (§W.2), round and match wins (§W.3), the tour and WORLD DOMINATION (§W.4), the final and why its death-done byte must be poked (§W.5), the per-character ending (§W.6), the port preview with timelines, size estimates and predicted misses (§W.7), the harness design and its runs (§W.8), and the claims and the remaining named gaps (§W.9).

---

## Decisions needed from the user

**Decided by the user on 2026-10-02:** all three recommendations accepted — D1 two captures (gp-u9-win ~70-130 MB, gp-u10-ending ~200-360 MB) approved; D2 only SAURON's ending is captured, the other six are a named gap; D3 the poke mechanism (P2's damage byte, the seven land marks, the final's death-done byte; raced writes refused; the port writes the same bytes at the same frame).

1. **D1 — Storage and run time for two captures (spec §7 Q7; gates Tasks 8 and 10).** `data/k11-captures/gp-u9-win` is about **70–130 MB** (≈78 s of play). `data/k11-captures/gp-u10-ending` is about **200–360 MB** (≈185 s). Record §W.7 derives both from the measured 44–49 KB per stored frame and the 0.36–0.65 distinct ratio of the existing captures. Each port replay writes a dump in `/tmp` while it is compared: 129 MB and 302 MB, measured on the planner's preview. `make verify` grows by about 4–5 min (replays of 59 s and ~170 s, plus their comparisons). *Recommendation:* yes. *If no:* stop after Task 7. The tools and the targets land and skip, and nothing is pinned. The G exit criterion "U9, U10 ratcheted under pokes" stays unmet.
2. **D2 — Which endings (gates Task 10).** Mode `0x1F`'s content is indexed by the winner's character (record §W.6: seven titles, from "THE FEAST OF SAURON" to "CHAOS' REDEMPTION"). The code path is the same for all seven; only the table entries differ. **(a) One ending: character 0, SAURON.** Cursor 0 is confirmed at once and no walk is needed. The other six endings are a named gap with the §W.6 evidence. **(b) All seven.** That adds six captures (~200–360 MB each, ≈1.2–2.2 GB in all), six more replays in `make verify` (+~18 min), and a follow-up plan written after Task 11. That plan would add a cursor walk per character and parametrize the milestones; it is not planned here. *Recommendation:* (a). *Cost of the wrong answer:* (a) leaves six characters' ending data unverified, and (b) costs disk and verify time.
3. **D3 — The poke mechanism and what it stands in for (gates Task 1).** Bytes are written through DOSBox-X's memory file during the spin. A re-read refuses a write that raced the iteration (`race=1`), and the port writes the same bytes before the same iteration. Three pokes replace game events:
   - P2's damage byte set to `0x78` replaces the hits that would KO P2 (§W.3).
   - The seven land marks replace six earlier won matches (§W.4).
   - `DS_00104B0C = 1` replaces the death animation that ends each final fight (§W.5). Without it mode `0xD` never ends: the planner's run stayed in mode `0xD` from `f = 0x1830` to its limit.

   Each replaced event becomes a named gap (§W.9). *Recommendation:* accept. *Alternative:* real hits. That is unbounded: the CPU fights back, the moves ratchet already diverges at `f = 0x8D6`, and the final needs seven real fatal hits. *Cost of declining:* U9 only as a long, uncertain capture; U10 not reachable.

## Global Constraints

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- Spec (gameplay) §5: "Harness values (`HOLD_FRAMES`, the 150-frame gaps, `time_limit`, …) are named as harness values with their source, never presented as game values." This plan's harness values are the 60-frame wait into mode `0x10`, the hold 6, the 10 frames into each fight mode before a poke, the 60 frames after match 2 begins, and the time limits 110 s and 240 s (record §W.7).
- `make verify` is THE gate. The 45 oracle lines (`grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>`) must equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`. The `make audio-render` WAV must be byte-identical to `.superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav`. `python3 tools/port_progress.py` and the `diff-verify:` summary line must print the baseline's values (this plan ports no function). Speed-up ruling (user, 2026-10-01): the full gate runs at the baseline (Task 0), at Task 12 and before merge. After Tasks 4, 7, 9 and 11 **the task gate** runs instead (Where to run). Parallel-safe overrides: `T=u910; SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md`.
- Spec (gameplay) §2: a unit pins and names a divergence; a follow-up unit fixes it. No file under `port/src` changes. A divergence caused by an unported animation target (record §W.7: `0x400E0`, `0x21044`, `0x21084`, `0x3DA50`) belongs to the P track.
- AGENTS.md: "Captures are git-ignored. `data/` is git-ignored and read-only." This plan writes only `data/k11-captures/gp-u9-win/` and `data/k11-captures/gp-u10-ending/`, through `make gp-capture` (`gp_capture.guard_gp`). Gp captures skip in `make verify` when absent, even under `PR_ORACLE_REQUIRED` (spec §4.3). Never re-capture over a pinned capture.
- AGENTS.md: "`make gp-capture` fails (`check=FAIL unscripted input`) on any key word or pad bit the harness did not inject, so nobody may type into the DOSBox-X window while it runs."
- AGENTS.md: "Every env-gated driver and `--check` run arms `fn_resolve`'s miss log and must record exactly the pinned known-set." On main the gp sets are the name-keyed table `k_gp_sets` in `port/tests/test_platform.c`. A new scenario adds a `k_gp_sets` entry and **never a new `fnm_known` parameter** (controller note, main `b5beff9`).
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." / "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests, and never assert an unseeded BSS-zero." Python tests are `unittest` in `tools/tests/` and import with `sys.path.insert(0, ROOT/tools)`.
- AGENTS.md (the gameplay oracles): the claims are narrow. "**the order of the port's frames and that every port frame appears are not claimed**"; where the port's script ends before the capture, the first unexplained frame is how far the port got.
- AGENTS.md: "Commit style: `<area>: <what changed>`. **Never `git add -A`**." Trailer: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. `docs/PROGRESS.md` is append-only.

## Review Focus

1. **A poke that lands in the wrong iteration, or is replayed one frame off.** Pinned by `TestPoke.test_a_tick_between_the_snapshot_and_the_write_is_a_race` (Task 3), `TestPokeScript.test_a_poke_is_replayed_at_the_next_frame` (Task 2, with the `W.f + 1 → W.f` mutation), the capture CHECK `pokes written N/N, 0 raced` (Tasks 8, 10) and the C test's frame boundary (`test_gp_poke_script`: nothing at the iteration raising the counter to 11, both pokes at the one raising it to 12; Task 4).
2. **Entry counting off by one, so a poke lands in the wrong round or on the wrong final opponent.** Pinned by `TestWinPokes.test_after_entry_keys_on_the_kth_s_entry` (Task 1) and by the evidence rows on the capture (round 2 `b1e = 2`; the final's `b21 = k − 1` at the k-th mode-`0xD` entry; Tasks 6, 8, 10).
3. **A capture that did not take the path:** P1 lost, the game set the death-done byte itself, or the name entry blocked. Pinned by `gp_win.py check` (Task 6 tests `test_a_wrong_character_fails`, `test_a_cpu_round_win_fails`, `test_an_unmarked_land_fails`, `test_a_death_done_poke_must_find_the_byte_clear`). It runs on each capture before anything is pinned (Tasks 8, 10) and first in the oracle. The contingencies are spelled out in Tasks 8 and 10.
4. **A ratchet pinned where it cannot fail, or for another capture.** Tasks 9 and 11 prove that each pin + 1 (window start − 1), a wrong sha256 and a damaged port frame each fail.
5. **Breaking an existing replay or over-reading green.** The `T` line test (`test_the_port_t_line_writes_every_snap_field_in_order`, extended in Task 4), the five existing gp oracles' `ok` lines in Task 4's gate, and the narrow claim stated in the Makefile comment and in record §W.9: nothing the pokes replace is claimed.

## Where to run

Branch `gameplay-u9-u10` in `.worktrees/u9u10`, off `main` **after U8 has merged** (the spec order: U5–U8 and U11, then U9→U10). The plan and its record must be on `main` first; if they are not, copy both from the planning worktree `.worktrees/plans-p-u9/docs/superpowers/plans/2026-10-02-gameplay-u9-u10-*.md`.

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse
git worktree add .worktrees/u9u10 -b gameplay-u9-u10 main
cd .worktrees/u9u10
ln -s ../../data data && ln -s ../../.superpowers .superpowers
cp ../../port/tests/ghidra_data.bin ../../port/tests/title_screen_ref.ppm port/tests/
ls -d data .superpowers port/tests/ghidra_data.bin port/tests/title_screen_ref.ppm docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md
cmake -S port -B build && cmake --build build 2>&1 | tail -2
```

Every command below assumes `cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse/.worktrees/u9u10` and `S=/tmp/gameplay-u9u10; mkdir -p $S`. The SDD ledger is `.superpowers/sdd/2026-10-02-gameplay-u9-u10-win-and-endings/progress.md`.

**The gate** (Task 0, Task 12, before merge; ~20 min):

```bash
T=u910; make verify SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md > $S/<name>_verify.txt 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $S/<name>_verify.txt | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
cmp /tmp/pr_u910.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-EQUAL
python3 tools/port_progress.py
grep -E '^diff-verify: [0-9]+/[0-9]+ functions' $S/<name>_verify.txt
grep -E '^gp_compare: .*: (frames|trace|moves): .*ratchet N' $S/<name>_verify.txt
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_session tools.tests.test_gp_capture tools.tests.test_gp_compare tools.tests.test_gp_twop 2>&1 | grep '^Ran'
git diff --stat main -- port/src
```

Pass: `verify-exit=0`, `ORACLES-EQUAL`, `WAV-EQUAL`, the counters and the `diff-verify:` line unchanged from Task 0, every existing gp ratchet line `ok`, `git diff --stat main -- port/src` empty. Task 0 records the values; on main `b5beff9` they are `771 1203 64` / `731 731 100`, and the four gp suites `Ran 105` (session 30, capture 30, compare 32, twop 13). U8 adds tests, so call the merged base's count **B** and read it at Task 0.

**The task gate** (after Tasks 4, 7, 9, 11):
- the task's tests (`PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_session tools.tests.test_gp_capture tools.tests.test_gp_compare tools.tests.test_gp_twop tools.tests.test_gp_win` → `OK`, and `PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests | tail -1` → `all checks passed`);
- its oracle (`make gp-win-oracle gp-ending-oracle GP_DUMP=/tmp/pr_u910_gp`);
- after Tasks 4, 9 and 11, which change the shared driver or the miss table, also `make gp-oracle gp-charsel-oracle gp-moves-oracle gp-keys-oracle gp-twop-oracle GP_DUMP=/tmp/pr_u910_gp` with every ratchet line `ok`, as in Task 0's log (plus U8's `gp-modes-oracle` if it is merged);
- `make diff-verify DIFF_IMAGE=/tmp/pr_u910_diffimg DIFF_TABLE=/tmp/pr_u910_diff.md` with its summary line unchanged;
- `git diff --stat main -- port/src` empty.

## File Structure

| File | Responsibility |
|---|---|
| `tools/gp_session.py` | `W` record format/parse; `WIN_EXTRA`, `KO_P2`, `DEATH_DONE`, `lands_marked`; `after_entry` and `Schedule.on_snap`; `port_script`'s `poke` lines; the two scenarios and `STOP_AT_END` |
| `tools/gp_capture.py` | `apply_poke`, `expected_pokes`, `poke_check`; the `Poller` calls `on_snap` and applies pokes; `run_checks` adds the poke CHECK |
| `tools/gp_win.py` (new) | the milestones per scenario, `check` (evidence) and `path` (ratchets) |
| `tools/tests/test_gp_session.py`, `test_gp_capture.py` | + `TestWinPokes`, `TestPokeScript`, `TestPoke`; the `T` line test includes `WIN_EXTRA` |
| `tools/tests/test_gp_win.py` (new) | the checker's tests |
| `port/tests/test_game.c` | the driver's `poke` line (parse, apply, count CHECK), the `T` line's win fields, `test_gp_poke_script` |
| `port/tests/test.h` | `X(test_gp_poke_script)` |
| `port/tests/test_platform.c` | two `k_gp_sets` entries and their tables (measured) |
| `Makefile` | `GP_WIN_*`, `GP_ENDING_*`, `gp-win-oracle`, `gp-ending-oracle`, `gp-win-one`, two `verify` lines |
| `docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md` | §W.10 onward |
| `docs/PROGRESS.md`, `AGENTS.md` | one appended paragraph; two command lines and one oracle sentence |

## Shared-file touch points (all additive; for the controller's merge order)

| File | Region | What this plan adds | Conflict with |
|---|---|---|---|
| `tools/gp_session.py` | module docstring (after the `E` line); `_DEC`/`_TEXT`; before `def parse`; before `def expand` (after the last `SCENARIOS[...]` block); `Schedule` (`__init__`, `_target`, docstring, a new `on_snap` before `due_boot`); `STOP_AT_END`; `port_script` (before `xrec = next(...)` and after the `bits` `ev +=` line) | `format_w`, `WIN_EXTRA`…, `after_entry`, `on_snap`, `poke` lines, two scenarios | U8's `_u8_menu`/scenario blocks before `def expand` (keep both), U8's `pad_arm` and header edits in `port_script` (different lines) |
| `tools/gp_capture.py` | before `def accept`; `run_checks`'s list (after `input_check(lines)`); `Poller.run` (after `self.sched.on_mode(f, v2['mode'])` in the S branch) | three functions, one CHECK, two calls | U8's arm label in `run_checks` (another list element) |
| `port/tests/test_game.c` | `GpStep`; after `static jmp_buf gp_end_jb;`; `gp_parse` (counter reset, `char hex[40]`, one `else if`); `gp_apply` (one branch); `gp_trace_line` (format and arguments appended); `test_gp_replay` (one CHECK); before `/* ---- named-gaps B` (the unit test); `#include <ctype.h>` | the poke line, the win fields, `test_gp_poke_script` | U8's pad-arm parse (`arm pad` line) in `gp_parse`: keep both |
| `tools/tests/test_gp_session.py` | `test_the_port_t_line_writes_every_snap_field_in_order` (three lines); two classes before `if __name__` | `WIN_EXTRA` in the `T` line test | any unit appending `T` fields: the sum of all extras |
| `port/tests/test_platform.c` | before `typedef struct { … } gp_set;` (two tables); `k_gp_sets` (two rows) | `gp-u9-win`, `gp-u10-ending` | U8's rows: keep all. A P batch that ports `0x400E0`, `0x21044`, `0x21084` or `0x3DA50` must drop the row from these sets (the driver's count is exact). |
| `Makefile` | before `gp-report:`; `verify` after `@$(MAKE) --no-print-directory gp-twop-oracle` | the block; two lines | U8's `gp-modes-oracle` line at the same place: keep both |
| `docs/PROGRESS.md`, `AGENTS.md` | end / the command list and the gameplay-oracle paragraph | append | append-only |

---

## Task 0: Baseline

**Files:** none (ledger only).

- [ ] **Step 1:** Run **the gate** with `<name>` = `base`. Record in the ledger the commit (`git rev-parse HEAD`), `verify-exit`, `ORACLES-EQUAL`, `WAV-EQUAL`, the two `port_progress.py` lines, the `diff-verify:` line, every `gp_compare: … ratchet N` line, and `Ran B`.
- [ ] **Step 2:** Record the shared anchors this plan edits, each of which must print exactly one line:

```bash
grep -n "^_DEC = \|^_TEXT = \|^def parse\|^def expand\|^STOP_AT_END\|    xrec = next((r for r in recs if r\['kind'\] == 'X'), None)" tools/gp_session.py
grep -n "^def accept\|            input_check(lines)\|                        self.sched.on_mode(f, v2\['mode'\])" tools/gp_capture.py
grep -n "typedef struct { char op; u32 f, a, b; } GpStep;\|^static jmp_buf gp_end_jb;\|    gp_n = gp_nkeys = 0u;\|static const gp_set k_gp_sets\[\]\|^gp-report: build" port/tests/test_game.c port/tests/test_platform.c Makefile
grep -n "0x400E0\|0x21044\|0x21084\|0x3DA50" port/src -r | head
```

The last command lists any of the four predicted-miss animation targets that a P batch has already ported. Note those in the ledger; Tasks 9 and 11 measure the sets anyway.
- [ ] **Step 3:** No commit (nothing changed).

---

## Task 1: `after_entry`, the `W` record and the win constants (`gp_session`)

**Files:** Modify `tools/gp_session.py` (docstring, `_DEC`/`_TEXT`, before `def parse`, before `def expand`, `Schedule`). Modify `tools/tests/test_gp_session.py` (a new class before `if __name__ == '__main__':`).

**Interfaces — produces:** `gs.format_w(ms, f, step, addr, was, now, late, race) -> str`; `parse` keeps `was`/`now` as text and `len`/`race` as decimal; `gs.WIN_EXTRA`, `gs.WIN_FIELDS`, `gs.KO_P2`, `gs.DEATH_DONE`, `gs.lands_marked(side, char) -> bytes`; the step `('after_entry', mode, k, n, action)`; `Schedule.on_snap(f, mode)`, `Schedule.entry_frame[(mode, k)]`.

- [ ] **Step 1: Write the failing tests.** Insert before the final `if __name__ == '__main__':` of `tools/tests/test_gp_session.py`:

```python
class TestWinPokes(unittest.TestCase):
    """Plan gameplay-u9-u10 (record 2026-10-02-gameplay-u9-u10-derivations.md §W.8)."""
    STEPS = (('boot', 25.0, ('key', 'enter')),
             ('after_entry', 0x06, 2, 10, ('poke', ((0x10789E, b'\x78'),))),
             ('after_entry', 0x06, 3, 60, ('end',)))

    def test_after_entry_keys_on_the_kth_s_entry(self):
        s = gs.Schedule(self.STEPS)
        s.on_snap(0x50, 0x03)
        s.on_snap(0x60, 0x06)                     # before the boot step: not counted
        s.due_boot(25.0)
        for f, mode in ((0x100, 0x27), (0x200, 0x06), (0x210, 0x06), (0x300, 0x08), (0x400, 0x06)):
            s.on_snap(f, mode)
        self.assertEqual(s.entry_frame, {(0x27, 1): 0x100, (0x06, 1): 0x200, (0x08, 1): 0x300,
                                         (0x06, 2): 0x400})
        self.assertEqual(s.due(0x400 + 8), [])
        self.assertEqual(s.due(0x400 + 9), [(1, ('poke', ((0x10789E, b'\x78'),)))])
        s.on_snap(0x500, 0x09)
        s.on_snap(0x600, 0x06)
        self.assertFalse(s.ended(0x600 + 59))
        s.due(0x600 + 59)
        self.assertEqual(s.end_frame, 0x63C)
        self.assertEqual((s.fired, s.total), (2, 2))

    def test_w_record_round_trip(self):
        w = gs.format_w(12, 0x48F, 5, 0x108106, b'\x00' * 7, b'\x80' * 7, 0, 0)
        self.assertEqual(w, 'W ms=12 f=048F step=5 addr=00108106 len=7 was=00000000000000 '
                            'now=80808080808080 late=0 race=0')
        r = gs.parse(w)
        self.assertEqual((r['kind'], r['f'], r['addr'], r['len'], r['was'], r['now'], r['race']),
                         ('W', 0x48F, 0x108106, 7, '00000000000000', '80808080808080', 0))
        self.assertEqual(gs.lands_marked(0, 0), b'\x80' * 7)
        self.assertEqual(gs.lands_marked(1, 3), b'\xC3' * 7)
        self.assertEqual(gs.WIN_FIELDS[:4], ('afc', 'ad4', 'w2', 'w3'))
```

- [ ] **Step 2: Run it to see it fail.** `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_session 2>&1 | tail -3` → `FAILED (errors=2)`, with `AttributeError: 'Schedule' object has no attribute 'on_snap'` and `module 'gp_session' has no attribute 'format_w'`.
- [ ] **Step 3: Implement.** Make each edit (old text → new text) in `tools/gp_session.py`.

(a) At the end of the module docstring, replace
`  E ms=<int> reason=<exit|time-limit|end> rc=<int>   end: stopped at the script's end (STOP_AT_END)"""`
with:

```
  E ms=<int> reason=<exit|time-limit|end> rc=<int>   end: stopped at the script's end (STOP_AT_END)
  W ms=<int> f=<hex4> step=<n> addr=<hex8> len=<n> was=<hex> now=<hex> late=<0|1> race=<0|1>
        a memory poke written in the spin of f, read by iteration f + 1 (plan U9/U10)"""
```

(b) Replace `_DEC = ('ms', 'rc', 'step', 'late', 'ring')` with `_DEC = ('ms', 'rc', 'step', 'late', 'ring', 'len', 'race')`, and `_TEXT = ('reason', 'press', 'release')` with `_TEXT = ('reason', 'press', 'release', 'was', 'now')`.

(c) Immediately before `def parse(line):`:

```python
def format_w(ms, f, step, addr, was, now, late, race):
    """The W record of one poke write (plan U9/U10): `was` the bytes before,
    `now` the bytes written, hex."""
    return ('W ms=%d f=%04X step=%d addr=%08X len=%d was=%s now=%s late=%d race=%d'
            % (ms, f, step, addr, len(now), bytes(was).hex().upper(), bytes(now).hex().upper(), late, race))


```

(d) Immediately before `def expand(action):`:

```python
# Plan gameplay-u9-u10 (record 2026-10-02-gameplay-u9-u10-derivations.md §W.2-§W.6): the
# win path and the ending, reached under memory pokes (spec 2026-09-30-reverse-completion-design
# §4 G, §7 "Wins and endings without pokes"). A ('poke', ((addr, bytes), ...)) action writes
# the bytes at the linear address in the spin of F - 1 (the iteration F reads them), logged as
# W records; the port replays them as `poke` lines at F. WIN_EXTRA: the S fields both scenarios
# add (record §W.2): the stage word (0x25848), the match result (0x27BA4), the round wins
# (0x27C48), the round index, the final's KO count (0x274FC), the final flag (0x25C88), the
# death-done byte (0x37FF2), the lands-held bytes and the seven land marks (0x41C28, 0x286BC),
# slot 0's score (+0x3C) and its world-domination count (+0x82, 0x416C2).
WIN_EXTRA = (('afc', 0x104AFC, 2), ('ad4', 0x104AD4, 4), ('w2', 0x104AF2, 1), ('w3', 0x104AF3, 1),
             ('b1e', 0x104B1E, 1), ('b21', 0x104B21, 1), ('b14', 0x104B14, 1), ('b0c', 0x104B0C, 1),
             ('t104', 0x108104, 2), ('m106', 0x108106, 4), ('m10a', 0x10810A, 4),
             ('sc0', 0x1077EC, 4), ('c82', 0x107832, 1))
WIN_FIELDS = tuple(n for n, _, _ in WIN_EXTRA)
# P2's +0x5A damage byte (DS_0010789E) at 0x78: the KO the round-end checks test
# (0x27FA8 `cmp 0x78`, 0x272DC's DS_00104B12 side), record §W.3.
KO_P2 = ('poke', ((0x10789E, b'\x78'),))
# The byte 0x37EA0 sets at 0x37FF2 when a KO'd fighter's death animation ends; mode
# 0xD (0x274FC) waits for it, and a poked KO never starts that animation (record §W.5).
DEATH_DONE = ('poke', ((0x104B0C, b'\x01'),))


def lands_marked(side, char):
    """The seven land marks DS_00108106..0x10810C as 0x286BC writes a won land:
    0x80 | side << 6 | the winner's character (record §W.4)."""
    return bytes([0x80 | side << 6 | char]) * 7


```

(e) In `class Schedule`, replace the docstring's first two lines
```
    """Steps: ('boot', s, act) | ('after_mode', mode, n, act) | ('after', n, act)
    | ('until_mode', mode, n); an ('end',) action ends the scenario at its frame.
```
with
```
    """Steps: ('boot', s, act) | ('after_mode', mode, n, act) | ('after', n, act)
    | ('after_entry', mode, k, n, act) | ('until_mode', mode, n); an ('end',) action
    ends the scenario at its frame. after_entry: F = the frame of the k-th entry
    into `mode` (on_snap) + n.
```
In `__init__`, after `        self.mode_first = {}` add:
```python
        self.snap_mode = None
        self.entry_count = {}
        self.entry_frame = {}       # (mode, k) -> the first S frame of the k-th entry
```
In `_target`, before its final `        return None`, add:
```python
        if st[0] == 'after_entry':
            f0 = self.entry_frame.get((st[1], st[2]))
            return None if f0 is None else f0 + st[3]
```
Before `    def due_boot(self, now_s):` add:
```python
    def on_snap(self, f, mode):
        """An accepted S record's mode (gp_capture.Poller, after on_mode): the k-th
        entry into a mode is the k-th S record whose mode differs from the previous
        S record's, counted, like mode_first, once the boot step fired. Only S records
        count: a P record can catch a mode word mid-iteration (plan U9/U10)."""
        prev, self.snap_mode = self.snap_mode, mode
        if prev is None or prev == mode or (self.i == 0 and self.steps and self.steps[0][0] == 'boot'):
            return
        k = self.entry_count.get(mode, 0) + 1
        self.entry_count[mode] = k
        self.entry_frame[(mode, k)] = f

```

- [ ] **Step 4: Run to pass.** The same command → `OK` (`Ran B_session + 2`; 32 on `b5beff9`).
- [ ] **Step 5: Mutation proof.** In `on_snap`, change `if prev is None or prev == mode or (` to `if prev is None or (`. Run → `FAIL: test_after_entry_keys_on_the_kth_s_entry` (planner-verified). Restore → `OK`.
- [ ] **Step 6: Commit.**

```bash
git add tools/gp_session.py tools/tests/test_gp_session.py
git commit -m "tools: gp_session after_entry step, W poke record and the win fields (plan U9/U10, record §W.8)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

## Task 2: `poke` lines in the port script

**Files:** Modify `tools/gp_session.py` (`port_script`, two places) and `tools/tests/test_gp_session.py` (a new class before `if __name__`).

**Interfaces:** consumes `W` records; produces `poke <W.f + 1> <addr hex8> <now hex>` lines (event order: keys, bits, pokes within a frame). A raced or unpinned `W` record raises `gs.ScriptError`.

- [ ] **Step 1: Write the failing tests** (they use the module's existing `_log` helper):

```python
class TestPokeScript(unittest.TestCase):
    def setUp(self):
        gs.SCENARIOS['_t'] = dict(time_limit=1, steps=())

    def tearDown(self):
        gs.SCENARIOS.pop('_t', None)

    W = 'W ms=4 f=0128 step=3 addr=0010789E len=1 was=00 now=78 late=0 race=0'

    def test_a_poke_is_replayed_at_the_next_frame(self):
        text = gs.port_script('_t', _log(extra_after=[self.W]))
        self.assertIn('poke 297 0010789E 78', text.splitlines())        # 0x128 + 1
        self.assertEqual(text.splitlines()[-1], 'end 304')

    def test_a_raced_poke_is_refused(self):
        with self.assertRaisesRegex(gs.ScriptError, 'raced'):
            gs.port_script('_t', _log(extra_after=[self.W.replace('race=0', 'race=1')]))

    def test_a_poke_without_its_snapshot_is_refused(self):
        with self.assertRaisesRegex(gs.ScriptError, 'unpinned'):
            gs.port_script('_t', _log(extra_after=[self.W.replace('f=0128', 'f=0140')]))

    def test_end_cuts_the_pokes(self):
        text = gs.port_script('_t', _log(extra_after=[self.W]), end=0x128)
        self.assertNotIn('poke', text)
```

- [ ] **Step 2: Run to fail.** `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_session 2>&1 | tail -3` → three failures (no `poke` line, no `ScriptError`); `test_end_cuts_the_pokes` passes.
- [ ] **Step 3: Implement.** In `port_script`, replace `    xrec = next((r for r in recs if r['kind'] == 'X'), None)` with:

```python
    pokes = []
    for k, w in enumerate(r for r in recs if r['kind'] == 'W'):
        if w['race']:
            raise ScriptError('poke %d (%08X) at f=%X raced the iteration' % (k, w['addr'], w['f']))
        if w['f'] not in snap:
            raise ScriptError('poke %d (%08X) at f=%X unpinned (no S record at f=%X)'
                              % (k, w['addr'], w['f'], w['f']))
        pokes.append((w['f'] + 1, w['addr'], w['now']))
    xrec = next((r for r in recs if r['kind'] == 'X'), None)
```

and after the line `    ev += [(f, 1, 0, 'bits %d %04X' % (f, kb)) for f, kb in bits if f <= last]` add:

```python
    ev += [(f, 2, i, 'poke %d %08X %s' % (f, a, d)) for i, (f, a, d) in enumerate(pokes) if f <= last]
```

- [ ] **Step 4: Run to pass** → `OK` (`B_session + 6`; 36 on `b5beff9`).
- [ ] **Step 5: Mutation proof.** Replace `pokes.append((w['f'] + 1,` with `pokes.append((w['f'],` → `FAIL: test_a_poke_is_replayed_at_the_next_frame` and `FAIL: test_end_cuts_the_pokes` (planner-verified). Restore.
- [ ] **Step 6: Commit** `tools/gp_session.py tools/tests/test_gp_session.py`, message `tools: gp_session port script replays W pokes as poke lines at W.f + 1 (plan U9/U10)` plus the trailer.

---

## Task 3: The capture side (`gp_capture`)

**Files:** Modify `tools/gp_capture.py` (before `def accept`, `run_checks`, `Poller.run`) and `tools/tests/test_gp_capture.py` (a new class before `if __name__`).

**Interfaces — produces:** `gc.apply_poke(mm, base, log, step, f, writes, late, ms, ref, reread) -> race`; `gc.expected_pokes(sched)`; `gc.poke_check(lines, sched) -> (label, ok)`, appended to `run_checks`. `Poller` calls `sched.on_snap(f, mode)` on every accepted `S` record and applies a `poke` action in the spin.

- [ ] **Step 1: Write the failing tests** (they use the module's `_mem`, `_put` and `BASE`):

```python
class TestPoke(unittest.TestCase):
    """Plan gameplay-u9-u10 (record 2026-10-02-gameplay-u9-u10-derivations.md §W.8)."""

    def _ref(self, m):
        _put(m, 0x0EF6DC, 2, 0x48F)
        _put(m, 0x101508, 4, 7)
        _put(m, 0x10150C, 4, 8)
        return gc.read_snap(m, BASE)

    def test_apply_poke_writes_at_the_data_base_and_logs(self):
        m, log = _mem(), io.StringIO()
        ref = self._ref(m)
        _put(m, 0x10789E, 1, 0x11)
        writes = ((0x108106, b'\x80' * 7), (0x10789E, b'\x78'))
        race = gc.apply_poke(m, BASE, log, 5, 0x48F, writes, 0, 12, ref, lambda: gc.read_snap(m, BASE))
        self.assertEqual(race, 0)
        o = BASE + 0x10789E - gs.DATA_BASE_VA
        self.assertEqual(m[o], 0x78)
        o = BASE + 0x108106 - gs.DATA_BASE_VA
        self.assertEqual(bytes(m[o:o + 8]), b'\x80' * 7 + b'\x00')
        self.assertEqual(log.getvalue().splitlines(), [
            gs.format_w(12, 0x48F, 5, 0x108106, b'\x00' * 7, b'\x80' * 7, 0, 0),
            gs.format_w(12, 0x48F, 5, 0x10789E, b'\x11', b'\x78', 0, 0)])

    def test_a_tick_between_the_snapshot_and_the_write_is_a_race(self):
        m, log = _mem(), io.StringIO()
        ref = self._ref(m)
        moved = lambda: dict(gc.read_snap(m, BASE), t508=8)
        self.assertEqual(gc.apply_poke(m, BASE, log, 5, 0x48F, ((0x10789E, b'\x78'),), 1, 12, ref, moved), 1)
        self.assertIn('late=1 race=1', log.getvalue())

    def test_the_poke_check(self):
        s = gs.Schedule((('after', 1, ('poke', ((0x108106, b'\x80' * 7), (0x10789E, b'\x78')))),))
        s.prev_frame = 0x100
        s.due(0x100)
        ok = gs.format_w(0, 0x100, 0, 0x108106, b'\x00' * 7, b'\x80' * 7, 0, 0)
        ok2 = gs.format_w(0, 0x100, 0, 0x10789E, b'\x00', b'\x78', 0, 0)
        self.assertEqual(gc.poke_check([ok, ok2], s), ('pokes written 2/2, 0 raced', True))
        self.assertEqual(gc.poke_check([ok], s), ('pokes written 1/2, 0 raced', False))
        raced = ok2.replace('race=0', 'race=1')
        self.assertEqual(gc.poke_check([ok, raced], s), ('pokes written 2/2, 1 raced', False))
        self.assertEqual(gc.poke_check([], gs.Schedule(())), ('pokes written 0/0, 0 raced', True))
```

- [ ] **Step 2: Run to fail.** `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_capture 2>&1 | tail -3` → `FAILED (errors=3)` (`module 'gp_capture' has no attribute 'apply_poke'` / `'poke_check'`).
- [ ] **Step 3: Implement.** Before `def accept(v, v2, v3):`:

```python
def apply_poke(mm, base, log, step, f, writes, late, ms, ref, reread):
    """A ('poke', writes) action in the spin of f (plan U9/U10): each (addr, data)
    is written at the linear address's place in the memory file (base + addr -
    DATA_BASE_VA, as read_snap) and logged as a W record. `ref` is the accepted S
    snapshot; reread() re-reads it after the writes: when f or the tick DS_00101508
    moved, the iteration may have begun before the bytes landed, so the record
    says race=1 and gp_session.port_script refuses it. Returns the race flag."""
    olds = []
    for addr, data in writes:
        o = base + addr - gs.DATA_BASE_VA
        olds.append(bytes(mm[o:o + len(data)]))
        mm[o:o + len(data)] = data
    race = int(not consistent(ref, reread()))
    for (addr, data), was in zip(writes, olds):
        log.write(gs.format_w(ms, f, step, addr, was, data, late, race) + '\n')
    return race


def expected_pokes(sched):
    """The poke writes of the steps that fired (Schedule.frame_of)."""
    return sum(len(sched.steps[i][-1][1]) for i in sched.frame_of
               if sched.steps[i][-1][0] == 'poke')


def poke_check(lines, sched):
    """The CHECK (label, ok): one W record per poke write of the fired steps, none raced."""
    ws = [r for r in (gs.parse(l) for l in lines) if r and r['kind'] == 'W']
    raced = sum(1 for w in ws if w['race'])
    want = expected_pokes(sched)
    return ('pokes written %d/%d, %d raced' % (len(ws), want, raced), len(ws) == want and raced == 0)


```

In `run_checks`, replace
```
            ('port script v2%s' % why, script_ok),
            input_check(lines)]
```
with
```
            ('port script v2%s' % why, script_ok),
            input_check(lines),
            poke_check(lines, sched)]
```
In `Poller.run`, replace
```
                        self.sched.on_mode(f, v2['mode'])
                        inj.release_due(f, ms)
                        for step, act, late in fire(self.sched, f):
                            for name, scan, word, hold in gs.expand(act):
```
with
```
                        self.sched.on_mode(f, v2['mode'])
                        self.sched.on_snap(f, v2['mode'])
                        inj.release_due(f, ms)
                        for step, act, late in fire(self.sched, f):
                            if act[0] == 'poke':
                                apply_poke(mm, base, log, step, f, act[1], late, ms, v2,
                                           lambda: read_snap(mm, base))
                            for name, scan, word, hold in gs.expand(act):
```
(`gs.expand` returns `[]` for a poke, so no key is pressed.)

- [ ] **Step 4: Run to pass:** `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_session tools.tests.test_gp_capture 2>&1 | tail -1` → `OK` (`B_capture + 3`; 33 on `b5beff9`).
- [ ] **Step 5: Mutation proof.** In `apply_poke`, replace `race = int(not consistent(ref, reread()))` with `race = 0` → `FAIL: test_a_tick_between_the_snapshot_and_the_write_is_a_race`. Restore.
- [ ] **Step 6: Commit** `tools/gp_capture.py tools/tests/test_gp_capture.py`, message `tools: gp_capture writes pokes in the spin, refuses a raced write, CHECKs the count (plan U9/U10)` plus the trailer.

---

## Task 4: The port driver's `poke` line and the win fields in the `T` line

**Files:** Modify `port/tests/test_game.c` (includes; `GpStep`; after `static jmp_buf gp_end_jb;`; `gp_parse`; `gp_apply`; `gp_trace_line`; `test_gp_replay`; a new test before `/* ---- named-gaps B: the 0x65431 soft restart (record B) ------------------ */`). Modify `port/tests/test.h` (`TEST_CASES`). Modify `tools/tests/test_gp_session.py` (`test_the_port_t_line_writes_every_snap_field_in_order`, three lines).

**Interfaces:** consumes `poke <f> <addr hex8> <hex>` (Task 2). Produces `gp_npokes`, `gp_pokes_applied`, `CHECK_EQ_INT(gp_pokes_applied, gp_npokes)` in the driver, the `T` line suffix `afc=%04X ad4=%08X w2 w3 b1e b21 b14 b0c=%02X t104=%04X m106=%08X m10a=%08X sc0=%08X c82=%02X` (the order of `gs.WIN_EXTRA`), and `int test_gp_poke_script(void)`.

- [ ] **Step 1: Write the failing tests.**
  (a) In `test_the_port_t_line_writes_every_snap_field_in_order` (`tools/tests/test_gp_session.py`), replace
  `        fields = gs.SNAP_FIELDS + gs.KEYS_EXTRA` with `        fields = gs.SNAP_FIELDS + gs.KEYS_EXTRA + gs.WIN_EXTRA     # + plan U9/U10's (record §W.2)`.
  Replace `            if n in gs.MOVE_FIELDS or (n, addr, size) in gs.KEYS_EXTRA:` with `            if n in gs.MOVE_FIELDS or (n, addr, size) in gs.KEYS_EXTRA + gs.WIN_EXTRA:`.
  Replace `            if (n, addr, size) in gs.KEYS_EXTRA:` with `            if (n, addr, size) in gs.KEYS_EXTRA + gs.WIN_EXTRA:`.
  (If U8 or another unit has appended `T` fields first, keep its tuple too: the line is the sum of every unit's extras, in the `T` line's order.)
  (b) In `port/tests/test.h`, after `    X(test_virtual_clock) \` add `    X(test_gp_poke_script) \`.
  (c) In `port/tests/test_game.c`, before `/* ---- named-gaps B: the 0x65431 soft restart (record B) ------------------ */`, add:

```c
/* Plan U9/U10 (record 2026-10-02-gameplay-u9-u10-derivations.md §W.8): the gp
 * driver's `poke` line. gp_parse takes it (an address in the data object, 1..16
 * bytes) and gp_apply writes it before the iteration that raises the frame
 * counter to its frame. No game_init(): parse and apply only. */
static int gp_poke_parses(const char *path, const char *body)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) return -1;
    fputs("# gp port script v2: scenario t\nenter_frame 10\nenter_state 0000\nkey 10 1C 0D\n", f);
    fputs(body, f);
    fputs("end 13\n", f);
    fclose(f);
    const int ok = gp_parse(path);
    free(gp_step);
    gp_step = NULL;
    return ok;
}

int test_gp_poke_script(void)
{
    const int before = g_failures;
    char path[] = "/tmp/pr_gp_poke_XXXXXX";
    const int fd = mkstemp(path);
    CHECK(fd >= 0, "a temp script opens");
    if (fd < 0) return g_failures - before;
    FILE *f = fdopen(fd, "w");
    fputs("# gp port script v2: scenario t\nenter_frame 10\nenter_state 0000\nkey 10 1C 0D\n"
          "poke 12 0010789E 78\npoke 12 00108106 81828384858687\nend 13\n", f);
    fclose(f);
    CHECK(gp_parse(path), "a script with poke lines parses");
    CHECK_EQ_INT((int)gp_npokes, 2);
    CHECK_EQ_INT((int)gp_n, 4);
    CHECK_EQ_INT(gp_step[1].op, 'p');
    CHECK_EQ_INT((int)gp_step[1].a, 0x10789E);
    CHECK_EQ_INT((int)gp_step[1].b, 1);
    CHECK_EQ_INT((int)gp_step[2].b, 7);
    CHECK_EQ_INT((int)gp_step[2].d[6], 0x87);
    /* Sentinels the pokes overwrite, and one past the second poke they must not;
     * the bytes are restored at the end (later cases share mem[]). */
    u8 keep[9];
    keep[8] = DSB(0x10789Eu);
    for (u32 i = 0; i < 8u; i++) keep[i] = DSB(0x108106u + i);
    DSB(0x10789Eu) = 0x11u;
    for (u32 i = 0; i < 8u; i++) DSB(0x108106u + i) = 0x22u;
    gp_log = tmpfile();
    CHECK(gp_log != NULL, "a temp log opens");
    if (gp_log == NULL) { free(gp_step); gp_step = NULL; remove(path); return g_failures - before; }
    gp_next = 1u;                                /* the Enter is behind */
    gp_missed = 0u;
    gp_apply(10u);                               /* the iteration raising the counter to 11: none due */
    CHECK_EQ_INT((int)DSB(0x10789Eu), 0x11);
    CHECK_EQ_INT((int)gp_pokes_applied, 0);
    gp_apply(11u);                               /* the iteration raising it to 12 */
    CHECK_EQ_INT((int)DSB(0x10789Eu), 0x78);
    CHECK_EQ_INT((int)DSB(0x108106u), 0x81);
    CHECK_EQ_INT((int)DSB(0x10810Cu), 0x87);
    CHECK_EQ_INT((int)DSB(0x10810Du), 0x22);
    CHECK_EQ_INT((int)gp_pokes_applied, 2);
    CHECK_EQ_INT((int)gp_missed, 0);
    CHECK_EQ_INT((int)gp_next, 3);               /* stops at `end` */
    DSB(0x10789Eu) = keep[8];
    for (u32 i = 0; i < 8u; i++) DSB(0x108106u + i) = keep[i];
    fclose(gp_log);
    gp_log = NULL;
    free(gp_step);
    gp_step = NULL;
    /* Refused: below the data object, past its end, an odd digit count, 17 bytes, a non-hex digit. */
    CHECK_EQ_INT(gp_poke_parses(path, "poke 12 0007FFFF 78\n"), 0);
    CHECK_EQ_INT(gp_poke_parses(path, "poke 12 0010B0CF 7878\n"), 0);
    CHECK_EQ_INT(gp_poke_parses(path, "poke 12 0010789E 787\n"), 0);
    CHECK_EQ_INT(gp_poke_parses(path, "poke 12 00108106 0102030405060708090A0B0C0D0E0F1011\n"), 0);
    CHECK_EQ_INT(gp_poke_parses(path, "poke 12 0010789E zz\n"), 0);
    CHECK_EQ_INT(gp_poke_parses(path, "poke 12 0010B0CF 78\n"), 1);   /* the last byte of the object */
    remove(path);
    return g_failures - before;
}

```

- [ ] **Step 2: Run to fail.** `cmake --build build 2>&1 | grep -E "error" | head -3` → compile errors (`use of undeclared identifier 'gp_npokes'`, `no member named 'd'`). `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_session 2>&1 | grep '^FAIL'` → `FAIL: test_the_port_t_line_writes_every_snap_field_in_order` (the `T` line has 13 fewer fields).
- [ ] **Step 3: Implement** in `port/tests/test_game.c`:
  - After `#include <string.h>` add `#include <ctype.h>`.
  - Replace `typedef struct { char op; u32 f, a, b; } GpStep;` with `typedef struct { char op; u32 f, a, b; u8 d[16]; } GpStep;   /* 'p': a = address, b = length, d = bytes */`.
  - After `static jmp_buf gp_end_jb;` add:

```c
static u32 gp_npokes, gp_pokes_applied;   /* plan U9/U10 (record §W.8): the script's `poke` lines */

/* A `poke` line's hex bytes: an even count of hex digits, 1..16 bytes. */
static int gp_poke_bytes(const char *hex, u8 *out, unsigned *len)
{
    size_t n = strlen(hex);
    if (n == 0u || n % 2u != 0u || n / 2u > 16u) return 0;
    for (size_t i = 0; i < n; i += 2u) {
        unsigned v;
        if (!isxdigit((unsigned char)hex[i]) || !isxdigit((unsigned char)hex[i + 1u])
            || sscanf(hex + i, "%2x", &v) != 1) return 0;
        out[i / 2u] = (u8)v;
    }
    *len = (unsigned)(n / 2u);
    return 1;
}
```

  - In `gp_parse`, after `    gp_n = gp_nkeys = 0u;` add `    gp_npokes = gp_pokes_applied = 0u;`. After `        unsigned a = 0u, b = 0u, c = 0u;` add `        char hex[40];`. After `        else if (sscanf(line, "bits %u %x", &a, &b) == 2) s->op = 'b';` add:

```c
        /* plan U9/U10: `poke <f> <addr hex8> <bytes hex>`, inside the data object
         * 0x80000..0x10B0CF (AGENTS.md). */
        else if (sscanf(line, "poke %u %x %39s", &a, &b, hex) == 3 && gp_poke_bytes(hex, s->d, &c)
                 && b >= 0x80000u && b + c <= 0x10B0D0u) { s->op = 'p'; gp_npokes++; }
```

  (The existing `s->f = a; s->a = b; s->b = c;` then stores the frame, the address and the length.)
  - In `gp_apply`, after the `'b'` branch's `fprintf(gp_log, "bits f=%u kb=%04X\n", s->f, s->a);`, replace `        } else {` (the one before `break;  /* 'e' is checked after the iteration */`) with:

```c
        } else if (s->op == 'p') {
            memcpy(mem + s->a, s->d, s->b);          /* the capture's W record, written in the spin of f - 1 */
            fprintf(gp_log, "poke f=%u addr=%08X len=%u\n", s->f, s->a, s->b);
            gp_pokes_applied++;
        } else {
```

  - In `gp_trace_line`, replace the format's last line `            "lat=%08X spz=%02X mpz=%02X\n",` with:

```c
            "lat=%08X spz=%02X mpz=%02X "
            "afc=%04X ad4=%08X w2=%02X w3=%02X b1e=%02X b21=%02X b14=%02X b0c=%02X t104=%04X "
            "m106=%08X m10a=%08X sc0=%08X c82=%02X\n",
```

  and its last argument line `            (unsigned)DSD(DS_00105F30), (unsigned)DSB(DS_001028DB), (unsigned)DSB(DS_001028DA));` with:

```c
            (unsigned)DSD(DS_00105F30), (unsigned)DSB(DS_001028DB), (unsigned)DSB(DS_001028DA),
            /* plan U9/U10 (record §W.2): gp_session.WIN_EXTRA's names. */
            (unsigned)DSW(DS_00104AFC), (unsigned)DSD(DS_00104AD4), (unsigned)DSB(DS_00104AF2),
            (unsigned)DSB(DS_00104AF3), (unsigned)DSB(DS_00104B1E), (unsigned)DSB(DS_00104B21),
            (unsigned)DSB(DS_00104B14), (unsigned)DSB(DS_00104B0C), (unsigned)DSW(DS_00108104),
            (unsigned)DSD(DS_00108106), (unsigned)DSD(DS_0010810A), (unsigned)DSD(DS_001077EC),
            (unsigned)DSB(DS_00107832));
```

  - In `test_gp_replay`, after `    CHECK_EQ_INT((int)gp_keys_sent, (int)gp_nkeys);` add `    CHECK_EQ_INT((int)gp_pokes_applied, (int)gp_npokes);   /* plan U9/U10: every poke written */`.

- [ ] **Step 4: Run to pass.** `cmake --build build 2>&1 | grep -E "error|warning:"` prints nothing. `PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests | tail -1` → `all checks passed`. `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_session tools.tests.test_gp_capture 2>&1 | tail -1` → `OK`.
- [ ] **Step 5: Mutation proofs** (planner-verified on `b5beff9`; rebuild and run `./build/run_tests` each time, then restore):
  - `memcpy(mem + s->a, s->d, s->b);` → `memcpy(mem + s->a, s->d, s->b - 1u);` gives `test_game.c:…: 17 != 120` and `34 != 135` (`FAILURES: 2`).
  - Delete ` && b + c <= 0x10B0D0u` gives `…: 1 != 0` (`FAILURES: 1`).
  - In the `T` line, swap the `DSB(DS_00104B21)` and `DSB(DS_00104B14)` arguments: `test_the_port_t_line_writes_every_snap_field_in_order` fails on `b21`.
- [ ] **Step 6: Task gate** (Where to run). The five existing gp oracles replay with this driver and their `ok` lines must equal Task 0's. The planner ran `gp-twop` on the edited driver: `ratchet N 612 ok`, `1506 ok`, `1506 ok`.
- [ ] **Step 7: Commit** `port/tests/test_game.c port/tests/test.h tools/tests/test_gp_session.py`, message `test: the gp driver applies poke lines; the T line carries the win fields (plan U9/U10, record §W.8)` plus the trailer.

---

## Task 5: The two scenarios

**Files:** Modify `tools/gp_session.py` (after `lands_marked`, before `def expand`; `STOP_AT_END`) and `tools/tests/test_gp_session.py` (`TestWinPokes`, one method).

**Interfaces:** produces `gs.WIN_MENU`, `SCENARIOS['gp-u9-win']`, `SCENARIOS['gp-u10-ending']` (`extra=WIN_EXTRA`), both in `STOP_AT_END`. `gp_capture.py --scenario` accepts both, because its choices are the `gp-` keys.

- [ ] **Step 1: Failing test.** Add to `class TestWinPokes`:

```python
    def test_the_scenarios(self):
        u9 = gs.SCENARIOS['gp-u9-win']['steps']
        self.assertEqual([st[-1] for st in u9 if st[-1][0] == 'poke'], [gs.KO_P2, gs.KO_P2])
        u10 = gs.SCENARIOS['gp-u10-ending']['steps']
        pokes = [st for st in u10 if isinstance(st[-1], tuple) and st[-1][0] == 'poke']
        self.assertEqual(len(pokes), 2 + 14)
        self.assertEqual(pokes[0][-1][1], ((0x108106, b'\x80' * 7), (0x10789E, b'\x78')))
        self.assertEqual([(st[1], st[2]) for st in pokes[2:]],
                         [(m, k) for k in range(1, 8) for m in (0x0C, 0x0D)])
        self.assertEqual(u10[-1], ('until_mode', 0x03, 0))
        for name in ('gp-u9-win', 'gp-u10-ending'):
            self.assertIn(name, gs.STOP_AT_END)
            self.assertEqual(gs.SCENARIOS[name]['extra'], gs.WIN_EXTRA)
```

  Run `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_session 2>&1 | grep KeyError` → `KeyError: 'gp-u9-win'`.
- [ ] **Step 2: Implement.** After `lands_marked`'s body (before `def expand`):

```python
# The menu path of gp-idle-loss (spec §4.4) and the immediate confirm of the
# character-select cursor 0 (character 0, SAURON; p1.start = e0 bit 0, 0x43CAD, as
# gp-u5-charsel's confirm, record gameplay-u5 §C5.4). Harness values: the 60-frame
# wait into mode 0x10 and the hold 6 (gp-u5-charsel's), the 10 frames into each
# fight mode before a poke (the round is live: 0x27FA8 runs every mode-6 frame).
WIN_MENU = (
    ('boot', ENTER_WAIT, ('key', 'enter')),           # mode 3 -> 0x27, MAIN MENU on "Start"
    ('after_mode', 0x27, 150, ('key', 'enter')),      # START MENU, cursor on row 0 (spec §3.3)
    ('after', 150, ('key', 'enter')),                 # LEFT PLAYER ARCADE: mode 0x2D
    ('after_mode', 0x10, 60, ('pad', ('p1.start',), 6)),   # confirm cursor 0: character 0
)
# U9 (record §W.6): two poked KOs win match 1 (0x27BA4: 2 wins of 3), the conquered-lands
# screen (mode 0x12) and the next opponent's wipe, to 60 frames into match 2's round 1.
# 110 s: 1.4x the port-predicted end at 78 s (record §W.7), a harness value.
SCENARIOS['gp-u9-win'] = dict(time_limit=110, extra=WIN_EXTRA, steps=WIN_MENU + (
    ('after_entry', 0x06, 1, 10, KO_P2),              # round 1: P2 KO'd, mode 8
    ('after_entry', 0x06, 2, 10, KO_P2),              # round 2: P1 wins the match, mode 9
    ('after_entry', 0x06, 3, 60, ('end',)),           # match 2, round 1
))
# U10 (record §W.6): round 1 also marks all seven lands P1's, so the won match is the
# seventh land (0x41C28 state 5: DS_00108104[0] == 7): WORLD DOMINATION, the health
# bonus (modes 0x23/0x22/0x24), the final (mode 0xC: seven opponents, each KO'd by a poke
# and replaced in mode 0xD once DEATH_DONE is poked), mode 0xF, the ending (mode 0x1F),
# the high-score entry (mode 0x1E) and back to mode 3. 240 s: 1.3x the port-predicted
# end at 185 s (record §W.7), a harness value.
SCENARIOS['gp-u10-ending'] = dict(time_limit=240, extra=WIN_EXTRA, steps=WIN_MENU + (
    ('after_entry', 0x06, 1, 10, ('poke', ((0x108106, lands_marked(0, 0)), (0x10789E, b'\x78')))),
    ('after_entry', 0x06, 2, 10, KO_P2),
) + tuple(st for k in range(1, 8) for st in (
    ('after_entry', 0x0C, k, 10, KO_P2),              # the final's k-th opponent KO'd: mode 0xD
    ('after_entry', 0x0D, k, 10, DEATH_DONE),         # replaced (k < 7) or mode 0xF (k = 7)
)) + (
    ('until_mode', 0x03, 0),                          # back in mode 3 after the high-score entry
))
```

  Add `'gp-u9-win', 'gp-u10-ending'` to the `STOP_AT_END` set: on `b5beff9` it becomes `STOP_AT_END = frozenset({'gp-u6-moves', 'gp-u6-moves-b', 'gp-twop', 'gp-u9-win', 'gp-u10-ending'})`. Keep any names U8 added. Append to the comment above it: `gp-u9-win and gp-u10-ending (plan U9/U10) stop too: nothing past their end is compared.`
- [ ] **Step 3: Run to pass.** `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_session tools.tests.test_gp_capture 2>&1 | tail -1` → `OK`. `python3 tools/gp_capture.py --help | grep -c -- 'gp-u10-ending'` → at least `1`.
- [ ] **Step 4: Mutation proof.** In gp-u10-ending, change `range(1, 8)` to `range(1, 7)` → `FAIL: test_the_scenarios`. Restore.
- [ ] **Step 5: Commit** `tools/gp_session.py tools/tests/test_gp_session.py`, message `tools: gp-u9-win and gp-u10-ending scenarios (plan U9/U10, record §W.6)` plus the trailer.

---

## Task 6: `tools/gp_win.py`, the milestones and the evidence

**Files:** Create `tools/gp_win.py` and `tools/tests/test_gp_win.py`.

**Interfaces — produces:** `gw.MILESTONES[name] = ((label, mode, k, predicate), …)` (8 rows for gp-u9-win, 30 for gp-u10-ending); `gw.evidence(name, lines, out) -> 0|1`; `gw.reproduced(name, cap_lines, port_lines) -> (n, rows)`; `gw.death_done_set(lines, snaps)`; the CLI `check`/`path`. Consumes `gp_compare.ratchet` and `gp_compare.trace_claim` (read-only).

- [ ] **Step 1: Write the failing tests:** `tools/tests/test_gp_win.py`:

```python
# tools/tests/test_gp_win.py (gameplay U9/U10: the win path and the ending under pokes)
import io, os, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs
import gp_win as gw

FIELDS = gs.SNAP_FIELDS + gs.WIN_EXTRA


def _rec(kind, f, mode, kw):
    vals = {n: 0 for n, _, _ in FIELDS}
    vals.update(f=f, mode=mode, t508=1, t50c=2, **kw)
    s = gs.format_s(0, vals, 0, 0x1E, 0x1E, FIELDS)
    return s if kind == 'S' else 'T ' + s[len('S ms=0 '):].rsplit(' kb=', 1)[0]


def _marks(**at):
    """m106/m10a from {land index: byte}."""
    b = bytearray(8)
    for i, v in at.items():
        b[int(i[1:])] = v
    return dict(m106=int.from_bytes(b[:4], 'little'), m10a=int.from_bytes(b[4:], 'little'))


# The U9 path (frames from the port preview of record §W.7): (first f, last f, mode, fields).
U9 = [(0x100, 0x140, 0x03, {}), (0x141, 0x292, 0x27, {}), (0x293, 0x2F5, 0x10, {}),
      (0x2F6, 0x485, 0x17, {}),
      (0x486, 0x48F, 0x06, dict(b1e=1, afc=5)),
      (0x490, 0x853, 0x08, dict(b1e=1, afc=5, s1_5a=0x78, w2=1, ad4=gw.NO_RESULT)),
      (0x854, 0x85D, 0x06, dict(b1e=2, afc=5, w2=1, ad4=gw.NO_RESULT)),
      (0x85E, 0xAB5, 0x09, dict(b1e=2, afc=5, s1_5a=0x78, w2=2, ad4=0)),
      (0xAB6, 0xBA6, 0x17, dict(afc=5, w2=2, **_marks(l5=0x80))),
      (0xBA7, 0xC13, 0x12, dict(afc=5, w2=2, **_marks(l5=0x80))),
      (0xC14, 0xD77, 0x12, dict(afc=5, w2=2, t104=1, **_marks(l5=0x80))),
      (0xD78, 0xDB4, 0x06, dict(b1e=1, afc=3, t104=1, ad4=gw.NO_RESULT, **_marks(l5=0x80)))]


def _log(path, kind='S', drop=lambda f: False, change=None):
    out = []
    for f0, f1, mode, kw in path:
        for f in range(f0, f1 + 1):
            if drop(f):
                continue
            k = dict(kw)
            if change:
                k = change(f, mode, k)
            out.append(_rec(kind, f, mode, k))
    return out


class TestEvidence(unittest.TestCase):
    def test_the_u9_path_passes(self):
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u9-win', _log(U9), lambda s: out.write(s + '\n')), 0)
        self.assertIn('8/8 milestones ok', out.getvalue())

    def test_a_wrong_character_fails(self):
        L = _log(U9, change=lambda f, m, k: dict(k, c0=1) if f >= 0x486 else k)
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u9-win', L, lambda s: out.write(s + '\n')), 1)
        self.assertIn('FAIL: round 1: P1 is character 0', out.getvalue())

    def test_a_cpu_round_win_fails(self):
        L = _log(U9, change=lambda f, m, k: dict(k, w2=0, w3=1) if m == 0x08 else k)
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u9-win', L, lambda s: out.write(s + '\n')), 1)
        self.assertIn('FAIL: round 1 KO', out.getvalue())

    def test_an_unmarked_land_fails(self):
        L = _log(U9, change=lambda f, m, k: dict(k, m10a=0) if m == 0x12 else k)
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u9-win', L, lambda s: out.write(s + '\n')), 1)
        self.assertIn('FAIL: the conquered-lands screen', out.getvalue())

    def test_no_win_fields_fails(self):
        L = [l.split(' afc=')[0] + ' kb=0000 head=001E tail=001E' for l in _log(U9)]
        out = io.StringIO()
        self.assertEqual(gw.evidence('gp-u9-win', L, lambda s: out.write(s + '\n')), 1)
        self.assertIn('records no WIN fields', out.getvalue())

    def test_a_death_done_poke_must_find_the_byte_clear(self):
        L = _log([(0x10, 0x20, 0x0D, {})])
        ok = gs.format_w(0, 0x15, 9, 0x104B0C, b'\x00', b'\x01', 0, 0)
        self.assertEqual(gw.death_done_set(L + [ok], gs.snapshots(L)), [])
        bad = gs.format_w(0, 0x16, 9, 0x104B0C, b'\x01', b'\x01', 0, 0)
        self.assertEqual(gw.death_done_set(L + [bad], gs.snapshots(L)), [0x16])
        L2 = _log([(0x10, 0x20, 0x0D, dict(b0c=1))])
        self.assertEqual(gw.death_done_set(L2 + [ok], gs.snapshots(L2)), [0x15])


class TestPath(unittest.TestCase):
    def test_the_same_path_reproduces_every_milestone(self):
        n, rows = gw.reproduced('gp-u9-win', _log(U9, drop=lambda f: f % 3 == 0), _log(U9, 'T'))
        self.assertEqual((n, len(rows)), (8, 8))

    def test_a_late_mode_counts_up_to_it(self):
        port = _log([(f0 + (2 if m == 0x12 else 0), f1, m, k) for f0, f1, m, k in U9], 'T')
        n, rows = gw.reproduced('gp-u9-win', _log(U9), port)
        self.assertEqual(n, 5)                    # the conquered-lands screen is the 6th row
        self.assertEqual((rows[5][1], rows[5][2]), (0xBA7, 0xBA9))

    def test_rows_cover_both_scenarios_in_order(self):
        self.assertEqual(sorted(gw.MILESTONES), ['gp-u10-ending', 'gp-u9-win'])
        for name, rows in gw.MILESTONES.items():
            for label, mode, k, pred in rows:
                self.assertTrue(0 < mode < 0x40 and k >= 1, (name, label))
        u10 = [(m, k) for _, m, k, _ in gw.MILESTONES['gp-u10-ending']]
        self.assertEqual(u10.count((0x0D, 7)), 1)
        self.assertLess(u10.index((0x0D, 1)), u10.index((0x0C, 2)))
        self.assertLess(u10.index((0x0C, 7)), u10.index((0x0D, 7)))
        self.assertEqual(u10[-1], (0x03, 1))


if __name__ == '__main__':
    unittest.main()
```

  Run `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_win 2>&1 | tail -2` → `ModuleNotFoundError: No module named 'gp_win'`.
- [ ] **Step 2: Implement:** create `tools/gp_win.py` with exactly this content:

```python
#!/usr/bin/env python3
"""Gameplay U9/U10 (plan docs/superpowers/plans/2026-10-02-gameplay-u9-u10-win-and-endings.md,
record docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md §W.9): the win path
and the ending, reached under memory pokes (gp_session's ('poke', ...) steps).

  check  evidence: the capture's S records show the scenario's raw-derived path, every
         MILESTONES row in order (each found in the k-th entry into its mode, from the
         first frame in mode 0x27 on), and every DEATH_DONE poke found the death-done
         byte still 0 (the game had not ended a death animation itself). Exit 1 on the
         first failure.
  path   the port's T records (trace.txt) reach the same milestones at the same frame,
         judged over the frames the capture snapshotted (as gp_compare's trace claim);
         the leading count reproduced must be >= --min-milestones N (a ratchet). With
         --win-min-first F, also gp_compare's trace claim over gp_session.WIN_FIELDS
         ('win'): the first differing f must be >= F.

Narrow, like the other gp oracles: a milestone is judged on the WIN fields, mode and the
slot bytes it names, nothing else; the frames are gp_compare's claim. Stdlib only.
Usage: gp_win.py check --scenario S --capture DIR [--capture-sha256 H]
       gp_win.py path --scenario S --capture DIR --port DIR --min-milestones N
                      [--win-min-first F] [--capture-sha256 H]"""
import argparse
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_compare as gc
import gp_session as gs

NO_RESULT = 0xFFFFFFFF          # DS_00104AD4 = -1: 0x27BA4's undecided match (0x25A0E, 0x27C0C)


def lands(r):
    """The seven land marks DS_00108106..0x10810C from the m106/m10a fields."""
    return list(r['m106'].to_bytes(4, 'little') + r['m10a'].to_bytes(4, 'little'))[:7]


def land(r, i):
    return lands(r)[i] if 0 <= i < 7 else None


def _all(**want):
    return lambda r: all(r[n] == v for n, v in want.items())


# One row per milestone: (label, mode, k, predicate). The frame of a row is the first record
# of the k-th entry into `mode` whose predicate holds. Raw sources: record §W.3-§W.6.
_START = (
    ('character select (mode 0x10)', 0x10, 1, None),
    ('round 1: P1 is character 0 (cursor 0 confirmed, 0x43CAD)', 0x06, 1, _all(c0=0, b1e=1)),
)
_MATCH1 = (
    ('round 1 KO: 0x27C48 counts P1, 0x27BA4 leaves the match open, 0x27FA8 -> mode 8',
     0x08, 1, _all(s1_5a=0x78, w2=1, w3=0, ad4=NO_RESULT)),
    ('round 2 (mode 6, round index 2)', 0x06, 2, _all(b1e=2, w2=1)),
    ('round 2 KO: P1 wins the match (0x27BA4 result 0) -> mode 9', 0x09, 1,
     _all(s1_5a=0x78, w2=2, ad4=0)),
    ('the conquered-lands screen (mode 0x12, 0x4142C), the won land marked by 0x286BC '
     '(0x80 | side 0 | character)', 0x12, 1, lambda r: land(r, r['afc']) == 0x80 | r['c0']),
)
MILESTONES = {
    'gp-u9-win': _START + _MATCH1 + (
        ('0x41C28 state 3 counts one land for P1', 0x12, 1, lambda r: r['t104'] & 0xFF == 1),
        ('match 2, round 1: a new land and opponent (0x25848, 0x41350)', 0x06, 3,
         lambda r: r['b1e'] == 1 and r['w2'] == 0 and land(r, r['afc']) == 0),
    ),
    'gp-u10-ending': _START + (
        ('round 1 KO with the seven lands poked P1\'s', 0x08, 1,
         lambda r: r['s1_5a'] == 0x78 and r['w2'] == 1 and r['ad4'] == NO_RESULT
         and lands(r) == [0x80 | r['c0']] * 7),
    ) + _MATCH1[1:] + (
        ('0x41C28 state 3 counts the seventh land', 0x12, 1, lambda r: r['t104'] & 0xFF == 7),
        ('WORLD DOMINATION: 0x4160C clears the marks, +0x82 = 1', 0x12, 1,
         lambda r: r['c82'] == 1 and lands(r) == [0] * 7),
        ('YOU MUST REPLENISH YOUR HEALTH: mode 0x23 (0x417C4, 0x26978)', 0x23, 1, None),
        ('the health bonus (mode 0x22)', 0x22, 1, None),
        ('mode 0x24 (0x26F58)', 0x24, 1, None),
        ('the final: mode 0xC, stage 7, flag DS_00104B14 (0x26F58, 0x25C88)', 0x0C, 1,
         _all(afc=7, b14=1, b21=0)),
    ) + tuple(row for k in range(1, 8) for row in (
        ('final opponent %d KO\'d -> mode 0xD (0x272DC)' % k, 0x0D, k, _all(s1_5a=0x78, b21=k - 1, b0c=0)),
        ('final opponent %d replaced (0x274FC, count %d)' % (k, k), 0x0C, k + 1, _all(b21=k)),
    )[:2 if k < 7 else 1]) + (
        ('YOU ARE MASTER OF THE NEW URTH: mode 0xF, count 7 (0x274FC)', 0x0F, 1, _all(b21=7)),
        ('the ending (mode 0x1F, 0x208F8)', 0x1F, 1, None),
        ('the ending, second part (mode 0x1F, state 3)', 0x1F, 2, None),
        ('the high-score entry (mode 0x1E)', 0x1E, 1, None),
        ('back in mode 3', 0x03, 1, None),
    ),
}


def entries(recs, start):
    """{(mode, k): [records of the k-th entry into mode]} over the records (by f) from
    `start` on; an entry begins at a record whose mode differs from the previous one's."""
    out, count, prev, cur = {}, {}, None, None
    for f in sorted(recs):
        if f < start:
            continue
        r = recs[f]
        if r['mode'] != prev:
            k = count.get(r['mode'], 0) + 1
            count[r['mode']] = k
            cur = out.setdefault((r['mode'], k), [])
            prev = r['mode']
        cur.append(r)
    return out


def start_frame(recs):
    return next((f for f in sorted(recs) if recs[f]['mode'] == 0x27), None)


def milestone_frames(name, recs, start):
    """[f or None] per MILESTONES[name] row."""
    ent = entries(recs, start)
    out = []
    for _, mode, k, pred in MILESTONES[name]:
        rows = ent.get((mode, k), [])
        out.append(next((r['f'] for r in rows if pred is None or pred(r)), None))
    return out


def snaps_from(lines):
    return gs.snapshots(lines)


def trace_from(lines):
    out = {}
    for l in lines:
        r = gs.parse(l)
        if r and r['kind'] == 'T':
            out.setdefault(r['f'], r)
    return out


def death_done_set(lines, snaps):
    """The frames of the DEATH_DONE pokes (W records at DS_00104B0C) that found the byte
    already set, in the W record or in the S record of the spin they were written in:
    there the game ended a death animation itself and the poke stood in for nothing."""
    addr = gs.DEATH_DONE[1][0][0]
    out = []
    for r in (gs.parse(l) for l in lines):
        if r and r['kind'] == 'W' and r['addr'] == addr:
            s = snaps.get(r['f'])
            if s is None or s['b0c'] != 0 or r['was'] != '00':
                out.append(r['f'])
    return out


def evidence(name, lines, out=print):
    """The check. Returns 0 ok, 1 FAIL."""
    snaps = snaps_from(lines)
    if not snaps or any(n not in next(iter(snaps.values())) for n in gs.WIN_FIELDS):
        out('gp_win: %s: evidence: FAIL: the capture records no WIN fields' % name)
        return 1
    start = start_frame(snaps)
    if start is None:
        out('gp_win: %s: evidence: FAIL: mode 0x27 never observed' % name)
        return 1
    prev = start
    for (label, mode, k, _), f in zip(MILESTONES[name], milestone_frames(name, snaps, start)):
        if f is None or f < prev:
            out('gp_win: %s: evidence: FAIL: %s (mode 0x%X entry %d)%s'
                % (name, label, mode, k, '' if f is None else ' at f=%X, before the previous milestone' % f))
            return 1
        out('gp_win: %s: evidence: %s at f=%X ok' % (name, label, f))
        prev = f
    bad = death_done_set(lines, snaps)
    if bad:
        out('gp_win: %s: evidence: FAIL: the death-done poke at f=%X found the byte set' % (name, bad[0]))
        return 1
    out('gp_win: %s: evidence: %d/%d milestones ok' % (name, len(MILESTONES[name]), len(MILESTONES[name])))
    return 0


def reproduced(name, cap_lines, port_lines):
    """(count, rows): the leading milestones the port reaches at the capture's frame, over
    the frames the capture snapshotted; rows = [(label, capture f, port f)]."""
    snaps, port = snaps_from(cap_lines), trace_from(port_lines)
    start = start_frame(snaps)
    shared = {f: port[f] for f in port if f in snaps}
    cf = milestone_frames(name, snaps, start)
    pf = milestone_frames(name, shared, start)
    rows = [(row[0], c, p) for row, c, p in zip(MILESTONES[name], cf, pf)]
    n = 0
    for _, c, p in rows:
        if c is None or c != p:
            break
        n += 1
    return n, rows


def sha_ok(name, cap_dir, sha, out):
    if sha is None:
        return True
    with open(os.path.join(cap_dir, 'poll.log'), 'rb') as f:
        got = hashlib.sha256(f.read()).hexdigest()
    if sha == '' or got != sha:
        out('gp_win: %s: capture: FAIL: poll.log sha256 %s != the pinned %s: re-measure, then re-pin'
            % (name, got, sha or '(unpinned)'))
        return False
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('check', 'path'))
    ap.add_argument('--scenario', required=True, choices=sorted(MILESTONES))
    ap.add_argument('--capture', required=True)
    ap.add_argument('--port')
    ap.add_argument('--min-milestones')
    ap.add_argument('--win-min-first')
    ap.add_argument('--capture-sha256')
    a = ap.parse_args()
    name = a.scenario
    if not os.path.isfile(os.path.join(a.capture, 'poll.log')):
        print('gp_win: %s: no capture at %s (skipped)' % (name, a.capture))
        return 0
    if not sha_ok(name, a.capture, a.capture_sha256, print):
        return 1
    with open(os.path.join(a.capture, 'poll.log')) as f:
        cl = f.read().splitlines()
    if a.cmd == 'check':
        return evidence(name, cl)
    if a.port is None or not os.path.isfile(os.path.join(a.port, 'trace.txt')):
        print('gp_win: %s: path: FAIL: no port trace at %s' % (name, a.port))
        return 1
    with open(os.path.join(a.port, 'trace.txt')) as f:
        pl = f.read().splitlines()
    n, rows = reproduced(name, cl, pl)
    for label, c, p in rows:
        print('gp_win: %s: path: %-70s capture %s port %s' % (name, label, '-' if c is None else '%X' % c,
                                                             '-' if p is None else '%X' % p))
    rc = gc.ratchet(name, 'path', None if n == len(rows) else n, len(rows),
                    None if a.min_milestones in (None, '') else int(a.min_milestones), print, 'not reproduced')
    if a.win_min_first is not None:
        rc2, _ = gc.trace_claim(name, cl, pl, None if a.win_min_first == '' else int(a.win_min_first),
                                print, False, gs.WIN_FIELDS, 'win')
        rc = rc or rc2
    return rc


if __name__ == '__main__':
    sys.exit(main())
```

- [ ] **Step 3: Run to pass.** `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_win 2>&1 | tail -2` → `Ran 9 tests … OK`.
- [ ] **Step 4: Mutation proofs** (planner-verified): in `_MATCH1`, change `_all(s1_5a=0x78, w2=1, w3=0, ad4=NO_RESULT)` to `w2=2` → 4 failures (`test_the_u9_path_passes`, `test_an_unmarked_land_fails`, both `TestPath` tests); in `reproduced`, `if c is None or c != p:` → `if c is None:` → `FAIL: test_a_late_mode_counts_up_to_it`. Restore.
- [ ] **Step 5: Commit** `tools/gp_win.py tools/tests/test_gp_win.py`, message `tools: gp_win, the win-path and ending milestones as evidence and ratchet (plan U9/U10, record §W.9)` plus the trailer.

---

## Task 7: The Makefile targets and the port preview

**Files:** Modify `Makefile` (a block before `gp-report: build ##`; two `verify` lines after `@$(MAKE) --no-print-directory gp-twop-oracle`). Append §W.10 to the record.

**Interfaces:** `make gp-win-oracle`, `make gp-ending-oracle`, `make gp-win-one scenario=<sc> GP_WIN_ID=WIN|ENDING`; variables `GP_WIN_*` / `GP_ENDING_*` (`MIN_FIRST TRACE_MIN_FIRST MAX_START MILESTONES WIN_MIN_FIRST CAPTURE_SHA256 CAPTURE_FRAMES`), all empty until Tasks 9 and 11.

- [ ] **Step 1: Add the block** before the line `gp-report: build ## Report-only gameplay comparison …`:

```make
# Gameplay U9/U10 oracles (plan docs/superpowers/plans/2026-10-02-gameplay-u9-u10-win-and-endings.md,
# record docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md): the win path
# (data/k11-captures/gp-u9-win) and the ending (data/k11-captures/gp-u10-ending), each reached
# under memory pokes (gp_session's ('poke', ...) steps; the port replays the capture's W records
# as `poke` lines at the same frames). The tool tests always run; with a capture present it must
# first show the raw-derived path (tools/gp_win.py check, record §W.9), then the frame and trace
# ratchets (gp_compare), the milestone ratchet (the leading milestones the port reaches at the
# capture's frame) and the win-fields trace ratchet (gp_session.WIN_FIELDS). Skips without the
# capture, even under PR_ORACLE_REQUIRED (spec §4.3); with it, an empty pin FAILS. The claims are
# narrow like gp-oracle's: what a poke replaced (the hits that would have KO'd P2, the death
# animation that would have set DS_00104B0C) is not claimed (record §W.9).
# The values below are pinned by the plan's Tasks 9 and 11 from the measured report lines
# (record §W.12/§W.14); raise each when it improves.
GP_WIN_MIN_FIRST =
GP_WIN_TRACE_MIN_FIRST =
GP_WIN_MAX_START =
GP_WIN_MILESTONES =
GP_WIN_WIN_MIN_FIRST =
GP_WIN_CAPTURE_SHA256 =
GP_WIN_CAPTURE_FRAMES =
GP_ENDING_MIN_FIRST =
GP_ENDING_TRACE_MIN_FIRST =
GP_ENDING_MAX_START =
GP_ENDING_MILESTONES =
GP_ENDING_WIN_MIN_FIRST =
GP_ENDING_CAPTURE_SHA256 =
GP_ENDING_CAPTURE_FRAMES =
.PHONY: gp-win-oracle gp-ending-oracle gp-win-one
gp-win-oracle: build ## Gameplay U9 oracle: win-path evidence, frame/trace/milestone/win ratchets on data/k11-captures/gp-u9-win (skips without it)
	@echo "== gameplay oracle: gp-u9-win (the win path under pokes; plan U9/U10) =="
	$(PYTHON) -m unittest tools.tests.test_gp_win
	@$(MAKE) --no-print-directory gp-win-one scenario=gp-u9-win GP_WIN_ID=WIN
gp-ending-oracle: build ## Gameplay U10 oracle: ending evidence, frame/trace/milestone/win ratchets on data/k11-captures/gp-u10-ending (skips without it)
	@echo "== gameplay oracle: gp-u10-ending (the ending under pokes; plan U9/U10) =="
	$(PYTHON) -m unittest tools.tests.test_gp_win
	@$(MAKE) --no-print-directory gp-win-one scenario=gp-u10-ending GP_WIN_ID=ENDING
gp-win-one: build
	@if [ ! -d $(K11_CAPTURES)/$(scenario) ]; then echo "gp-win-one: no capture at $(K11_CAPTURES)/$(scenario) (skipped)"; else \
		$(PYTHON) tools/gp_win.py check --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) \
			--capture-sha256 "$(GP_$(GP_WIN_ID)_CAPTURE_SHA256)" && \
		$(MAKE) --no-print-directory gp-replay scenario=$(scenario) GP_OPTIONAL=1 && \
		$(PYTHON) tools/gp_compare.py --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) \
			--port $(GP_DUMP)/$(scenario) --min-first "$(GP_$(GP_WIN_ID)_MIN_FIRST)" \
			--trace-min-first "$(GP_$(GP_WIN_ID)_TRACE_MIN_FIRST)" --max-start "$(GP_$(GP_WIN_ID)_MAX_START)" \
			--capture-sha256 "$(GP_$(GP_WIN_ID)_CAPTURE_SHA256)" --capture-frames "$(GP_$(GP_WIN_ID)_CAPTURE_FRAMES)" && \
		$(PYTHON) tools/gp_win.py path --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) \
			--port $(GP_DUMP)/$(scenario) --min-milestones "$(GP_$(GP_WIN_ID)_MILESTONES)" \
			--win-min-first "$(GP_$(GP_WIN_ID)_WIN_MIN_FIRST)" --capture-sha256 "$(GP_$(GP_WIN_ID)_CAPTURE_SHA256)"; fi

```

  In `verify`, after `	@$(MAKE) --no-print-directory gp-twop-oracle` add the two lines `	@$(MAKE) --no-print-directory gp-win-oracle` and `	@$(MAKE) --no-print-directory gp-ending-oracle` (keep U8's line).
- [ ] **Step 2: Run (skip path).** `make gp-win-oracle gp-ending-oracle GP_DUMP=/tmp/pr_u910_gp 2>&1 | grep -E "^Ran|^OK|skipped"` → twice `Ran 9 tests`/`OK` and both `gp-win-one: no capture at data/k11-captures/gp-u9-win (skipped)` and `… gp-u10-ending (skipped)` (planner-verified).
- [ ] **Step 3: Prove an unpinned present capture fails** (scratch `K11_CAPTURES`, never `data/`). This builds a stand-in `poll.log` from the preview of Step 4, so run Step 4's U9 replay first, then:

```bash
mkdir -p $S/caps/gp-u9-win && python3 -c "
T=open('$S/prev/gp-u9-win-preview/trace.txt').read().splitlines()
open('$S/caps/gp-u9-win/poll.log','w').write('\n'.join('S ms=0 '+l[2:]+' kb=0000 head=001E tail=001E' for l in T if l.startswith('T '))+'\n')"
make gp-win-one scenario=gp-u9-win GP_WIN_ID=WIN K11_CAPTURES=$S/caps 2>&1 | grep -E 'FAIL|Error'
```

  Expected: `gp_win: gp-u9-win: capture: FAIL: poll.log sha256 … != the pinned (unpinned): re-measure, then re-pin` and `make: *** [gp-win-one] Error 1` (planner-verified). Delete `$S/caps` afterwards.
- [ ] **Step 4: The port preview** (record §W.7; validates both scenarios in the port before any capture). It uses two hand-written scripts at the frames the planner's harness found:

```bash
mkdir -p $S/prev
printf '# gp port script v2: scenario gp-u9-win-preview\nenter_frame 321\nenter_state 0000\nkey 321 1C 0D\nkey 474 1C 0D\nkey 622 1C 0D\nbits 719 0100\nbits 725 0000\npoke 1168 0010789E 78\npoke 2142 0010789E 78\nend 3508\n' > $S/prev/u9.script
python3 - > $S/prev/u10.script <<'EOF'
print("# gp port script v2: scenario gp-u10-ending-preview")
print("enter_frame 321\nenter_state 0000\nkey 321 1C 0D\nkey 474 1C 0D\nkey 622 1C 0D\nbits 719 0100\nbits 725 0000")
print("poke 1168 00108106 80808080808080\npoke 1168 0010789E 78\npoke 2142 0010789E 78")
for i, f in enumerate(range(0x14C0, 0x1543, 10)):
    print("poke %d %s" % (f, "0010789E 78" if i % 2 == 0 else "00104B0C 01"))
print("end %d" % 0x26DF)
EOF
PR_GP_DUMP=$S/prev/gp-u9-win-preview PR_GP_SCRIPT=$S/prev/u9.script PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep -E "restart|fn-miss|test_platform" | tee $S/prev/u9.txt
PR_GP_DUMP=$S/prev/gp-u10-ending-preview PR_GP_SCRIPT=$S/prev/u10.script PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep -E "restart|fn-miss|test_platform" | tee $S/prev/u10.txt
for n in u9-win u10-ending; do python3 -c "
import sys; sys.path.insert(0,'tools'); import gp_win as gw
T=open('$S/prev/gp-$n-preview/trace.txt').read().splitlines()
sys.exit(gw.evidence('gp-$n', ['S ms=0 '+l[2:]+' kb=0000 head=001E tail=001E' for l in T if l.startswith('T ')]))" | tail -1; done
```

  Expected (planner, `b5beff9`; ~1 min and ~3 min). Each replay prints `test_gp_replay: 0 restart(s) landed`. The only failures are the miss-set check: `fn-miss PR_GP_DUMP: unexpected …` for `0x29D60`/`0x5D812 frontend_mode_1b_step` and `0x400E0`, `0x21044`, `0x21084` `anim_indirect`, plus `0x3DA50` for U10, with the count lines `7 != 2` and `8 != 2`. Then `gp_win: gp-u9-win: evidence: 8/8 milestones ok` and `gp_win: gp-u10-ending: evidence: 30/30 milestones ok`. Any other driver failure, or fewer milestones, stops the plan: report it before any capture. A pair a P batch has ported is absent (Task 0 Step 2's list). Delete `$S/prev/gp-*-preview` afterwards (431 MB).
- [ ] **Step 5: Record** §W.10 in the derivation record: the Step 2–4 outputs, verbatim lines.
- [ ] **Step 6: Task gate** (Where to run).
- [ ] **Step 7: Commit** `Makefile docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md`, message `build: gp-win-oracle and gp-ending-oracle (skip without their captures); the port preview (plan U9/U10, record §W.10)` plus the trailer.

---

## Task 8 (D1): Capture `gp-u9-win`

**Files:** writes `data/k11-captures/gp-u9-win/` (through the tool only). Appends §W.11 to the record.

- [ ] **Step 1:** Make sure nobody types into the DOSBox-X window. Then run:

```bash
make gp-capture scenario=gp-u9-win TITLE_PIN_DIR=/tmp/pr_u910_pin 2>&1 | tee $S/u9_capture.txt | grep -E 'CHECK|wrote|snapshots'
```

  Expected: every `gp_capture: CHECK …: ok`, including `steps fired 6/6`, `pokes written 2/2, 0 raced`, `no unscripted input` and `stopped at the end (SIGTERM at f=…, rc=0)`, then `wrote N frames to data/k11-captures/gp-u9-win`. The wall time is ~80 s (record §W.7: end ≈ 78 s).
- [ ] **Step 2: The evidence.** `python3 tools/gp_win.py check --scenario gp-u9-win --capture data/k11-captures/gp-u9-win | tee $S/u9_evidence.txt | tail -1` → `gp_win: gp-u9-win: evidence: 8/8 milestones ok`. Also run `python3 tools/gp_capture.py check-input data/k11-captures/gp-u9-win` → `check=ok no unscripted input`.
- [ ] **Step 3: Contingencies** (decide by the first failing line, then record the decision):
  - A `pokes … raced` or `port script v2 (…raced…)` CHECK: the capture went to `gp-u9-win.failed`. Re-run Step 1 once. A second race stops the task; report it with the two `W` lines.
  - A failing `gp_win` row: do not pin. Record the row and the `S` records around its expected frame (`grep -E '^(S|W|P)' data/k11-captures/gp-u9-win/poll.log | awk '$3 >= "f=<F-5>" && $3 <= "f=<F+20>"'`) and report to the controller. This is a raw-vs-plan conflict, and the raw wins: the scenario or the milestone changes in a follow-up, and the capture is redone only after that change.
- [ ] **Step 4: Record** §W.11: `session.txt` (all of it), the CHECK lines, the evidence lines, `du -sh data/k11-captures/gp-u9-win`, `ls data/k11-captures/gp-u9-win | grep -c raw.gz`, `shasum -a 256 data/k11-captures/gp-u9-win/poll.log`, the `W` lines (`grep '^W' …/poll.log`) and the first `S` record of each mode entry the milestones name. Compare the frames with the preview (§W.7) and say where they differ.
- [ ] **Step 5: Commit** the record (`docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md`), message `docs: gp-u9-win capture, evidence 8/8 (plan U9/U10, record §W.11)` plus the trailer. The capture itself is git-ignored.

---

## Task 9: The U9 replay — the miss set and the pins

**Files:** Modify `port/tests/test_platform.c` (a table before `typedef struct { const char *name; int prefix; const fnm_pair *rows; size_t n; } gp_set;`, one row in `k_gp_sets`), `Makefile` (the seven `GP_WIN_*` values and their provenance comment). Append §W.12 to the record.

- [ ] **Step 1: Measure the misses.** `make gp-replay scenario=gp-u9-win GP_DUMP=$S/gp GP_OPTIONAL=1 2>&1 | grep -E "fn-miss|test_platform|checks passed|FAILURES" | tee $S/u9_miss.txt`. Expected on `b5beff9` (§W.7): `unexpected 0x29D60 from frontend_mode_1b_step`, `0x5D812 from frontend_mode_1b_step`, `0x400E0 from anim_indirect`, `0x21044 from anim_indirect`, `0x21084 from anim_indirect`, and the count `7 != 2`. The measured lines win. For each pair, record its first frame with `grep -n 'fn-miss\|miss' $S/gp/gp-u9-win/gp.log` and its classification: §G.24 for the two frontend pairs, the E2 row (`2026-10-01-reverse-e2-triage.md`) for each animation target.
- [ ] **Step 2: Pin the miss set.** Before the `gp_set` typedef add (one row per measured pair, in the printed order; this is the planner's prediction):

```c
/* gp-u9-win (plan gameplay-u9-u10, record 2026-10-02-gameplay-u9-u10-derivations.md
 * §W.12), measured on its full replay: the two wipe hooks of §G.24 (0x29D60, a bare
 * `ret`; 0x5D812, the runtime stub) and the E2 animation targets the CPU's CHAOS
 * reaches after the poked KOs (unported P-track rows, reverse-e2-triage.md). */
static const fnm_pair k_miss_gp_u9_win[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
    { 0x400E0u, "anim_indirect" },
    { 0x21044u, "anim_indirect" },
    { 0x21084u, "anim_indirect" },
};
```

  and add the row `    { "gp-u9-win", 0, k_miss_gp_u9_win, FNM_N(k_miss_gp_u9_win) },` at the end of `k_gp_sets`. Rebuild and rerun Step 1's command → `all checks passed`. Mutation: delete the `0x400E0u` row and rerun → `fn-miss PR_GP_DUMP: unexpected 0x400E0 from anim_indirect` and the count line `7 != 6` (recorded 7, pinned 2 + 4). Restore.
- [ ] **Step 3: Measure the claims:**

```bash
python3 tools/gp_compare.py --report --scenario gp-u9-win --capture data/k11-captures/gp-u9-win --port $S/gp/gp-u9-win | tee $S/u9_report.txt | grep -E 'window|FIRST UNEXPLAINED|first difference|differing through|coverage'
python3 tools/gp_win.py path --scenario gp-u9-win --capture data/k11-captures/gp-u9-win --port $S/gp/gp-u9-win --min-milestones 0 --win-min-first 0 | tee $S/u9_path.txt | grep -E 'path:|win:'
```

  Read the values:
  - `MIN_FIRST` = the `FIRST UNEXPLAINED capture` index. The port's script ends at the `X` frame, so the first tail frame after it is "how far the port got", like gp-twop's 612.
  - `MAX_START` = the `window from capture` index.
  - `TRACE_MIN_FIRST` = the first differing f in decimal, or `end + 1` when `0 differing through end`.
  - `MILESTONES` = the count reproduced: 8 when the line says `0 not reproduced through 7`, otherwise the index `gp_compare` prints as `first not reproduced`.
  - `WIN_MIN_FIRST` = the `win:` first difference in decimal, or `end + 1`.
  - `CAPTURE_SHA256` = `shasum -a 256 data/k11-captures/gp-u9-win/poll.log`.
  - `CAPTURE_FRAMES` = `ls data/k11-captures/gp-u9-win | grep -c raw.gz`.

  For each first difference, name the cause from the evidence: the field, the frame, the mode, and the nearest `fn-miss` frame of Step 1. If it is at or after the first animation-target miss (preview f = `0x5A2`, mode 8), it is that unported target's (P track). Otherwise it is a new divergence: name it with its evidence, as U4 did. Do not fix it.
- [ ] **Step 4: Pin.** Set the seven `GP_WIN_*` values in the Makefile. Above them, add a provenance comment quoting the report lines verbatim, in the style of `GP_TWOP_*`'s. Run `make gp-win-oracle GP_DUMP=/tmp/pr_u910_gp` → every line `ok`, exit 0.
- [ ] **Step 5: Prove each pin can fail** (one at a time on the command line; each must exit non-zero with the quoted line):
  - `make gp-win-oracle GP_WIN_MIN_FIRST=<N+1>` → `frames: FAIL: first unexplained N < ratchet N N+1`, or `N … is unreachable`;
  - `GP_WIN_TRACE_MIN_FIRST=<F+1>` → `trace: FAIL`;
  - `GP_WIN_MAX_START=<start-1>` → `frames: FAIL: … window`;
  - `GP_WIN_MILESTONES=<M+1>` → `path: FAIL`;
  - `GP_WIN_WIN_MIN_FIRST=<W+1>` → `win: FAIL`;
  - `GP_WIN_CAPTURE_SHA256=0000` → `capture: FAIL`;
  - one byte of `/tmp/pr_u910_gp/gp-u9-win/frame_00010.ipx` flipped (`printf '\xff' | dd of=… bs=1 seek=100 conv=notrunc`) → a frame `FAIL`. Re-run the replay afterwards.

  Record each FAIL line.
- [ ] **Step 6: Record** §W.12: the miss set and its classification, the report lines, the pins, the FAIL lines and the named divergences.
- [ ] **Step 7: Task gate** (Where to run, including the five existing gp oracles: the driver's miss table changed).
- [ ] **Step 8: Commit** `port/tests/test_platform.c Makefile docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md`, message `test: pin gp-u9-win's miss set and the U9 ratchets (plan U9/U10, record §W.12)` plus the trailer.

---

## Task 10 (D1, D2): Capture `gp-u10-ending`

**Files:** writes `data/k11-captures/gp-u10-ending/`. Appends §W.13. Only for D2 = (a); for (b), finish this task for character 0, then write the follow-up plan named in D2.

- [ ] **Step 1:** `make gp-capture scenario=gp-u10-ending TITLE_PIN_DIR=/tmp/pr_u910_pin 2>&1 | tee $S/u10_capture.txt | grep -E 'CHECK|wrote|snapshots'`. Expected: every CHECK `ok`, including `steps fired 20/20`, `pokes written 17/17, 0 raced` (the marks step writes two), `no unscripted input` and `stopped at the end`. The wall time is ~190 s.
- [ ] **Step 2:** `python3 tools/gp_win.py check --scenario gp-u10-ending --capture data/k11-captures/gp-u10-ending | tee $S/u10_evidence.txt | tail -1` → `gp_win: gp-u10-ending: evidence: 30/30 milestones ok`. Then `check-input` as in Task 8.
- [ ] **Step 3: Contingencies:**
  - Races: as in Task 8.
  - **`FAIL: the death-done poke at f=… found the byte set`.** The original ended the death animation itself, which contradicts §W.5's run. Record the `S` records of that mode-`0xD` entry. Remove the `('after_entry', 0x0D, k, 10, DEATH_DONE)` steps from `SCENARIOS['gp-u10-ending']`: the `tuple(...)` keeps only the `0x0C` rows, so `test_the_scenarios` expects `2 + 7` pokes and the `(0x0C, k)` list. Drop the `b0c=0` from the KO rows in `gp_win.MILESTONES`. Rerun Tasks 5–6's tests, commit (`tools: gp-u10-ending without the death-done poke: the game sets it (record §W.13)`), and recapture (the capture is not pinned yet).
  - **`end frame reached` FAIL with the last `S` record in mode `0x1E` and `f` frozen** (the name entry blocks the loop, spec §7 Q5). Replace `('until_mode', 0x03, 0),` with `('after_entry', 0x1E, 1, 60, ('end',)),`, and the test's `u10[-1]` expectation with `('after_entry', 0x1E, 1, 60, ('end',))`. Delete the milestone row `('back in mode 3', 0x03, 1, None),` and the test's `self.assertEqual(u10[-1], (0x03, 1))` (it becomes `(0x1E, 1)`). Record the evidence (the last `S`/`P` records). Rerun the tests, commit, recapture.
  - Any other failing row: as in Task 8.
- [ ] **Step 4: Record** §W.13 as in Task 8 Step 4, plus the ending's frames: mode `0x1F` entries 1/2 and the `last_frame.png` path.
- [ ] **Step 5: Commit** the record, message `docs: gp-u10-ending capture, evidence 30/30 (plan U9/U10, record §W.13)` plus the trailer.

---

## Task 11: The U10 replay — the miss set and the pins

**Files:** `port/tests/test_platform.c` (`k_miss_gp_u10_ending`, one `k_gp_sets` row), `Makefile` (`GP_ENDING_*`), record §W.14.

- [ ] **Step 1:** Do as Task 9 Step 1 with `scenario=gp-u10-ending` (~3 min). The prediction is Task 9's five pairs plus `0x3DA50 from anim_indirect` (§W.7: f = `0x14F7`, mode `0xC`, VERTIGO), count `8 != 2`.
- [ ] **Step 2:** Add after `k_miss_gp_u9_win`:

```c
/* gp-u10-ending (plan gameplay-u9-u10, record 2026-10-02-gameplay-u9-u10-derivations.md
 * §W.14), measured on its full replay: gp-u9-win's pairs (the same match 1) and the
 * animation target 0x3DA50 a final opponent reaches (an unported E2 row). */
static const fnm_pair k_miss_gp_u10_ending[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
    { 0x400E0u, "anim_indirect" },
    { 0x21044u, "anim_indirect" },
    { 0x21084u, "anim_indirect" },
    { 0x3DA50u, "anim_indirect" },
};
```

  and the row `    { "gp-u10-ending", 0, k_miss_gp_u10_ending, FNM_N(k_miss_gp_u10_ending) },`. The measured pairs win over this prediction. Mutation: delete the `0x3DA50u` row → `unexpected 0x3DA50 from anim_indirect`, `8 != 7`. Restore.
- [ ] **Step 3–5:** Do as Task 9 Steps 3–5 with `gp-u10-ending`, `GP_ENDING_*`, `make gp-ending-oracle`. The milestone count is out of 30. Name the first divergence's owner: the P track for an animation-target frame; for a frame in modes `0x22`/`0x23`/`0x24`/`0x1F`/`0x1E` before any miss, a new named gap with its evidence.
- [ ] **Step 6:** Record §W.14. **Step 7:** Task gate. **Step 8:** Commit `port/tests/test_platform.c Makefile docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md`, message `test: pin gp-u10-ending's miss set and the U10 ratchets (plan U9/U10, record §W.14)` plus the trailer.

---

## Task 12: Docs and the final gate

**Files:** `docs/PROGRESS.md` (append), `AGENTS.md` (two command lines after `make gp-keys-oracle …`, and one sentence at the end of the gameplay-oracle paragraph), record §W.15.

- [ ] **Step 1:** AGENTS.md, after the `make gp-keys-oracle` command line:

```
make gp-win-oracle         # U9: the win path under pokes, data/k11-captures/gp-u9-win: evidence (tools/gp_win.py check), frame/trace/milestone/win ratchets (GP_WIN_* in the Makefile); in make verify; skips without the capture
make gp-ending-oracle      # U10: the ending under pokes, data/k11-captures/gp-u10-ending (GP_ENDING_*); in make verify; skips without the capture
```

  and at the end of the paragraph that begins "The gameplay oracle (`make gp-oracle`, …)":

```
  `make gp-win-oracle`/`gp-ending-oracle` (plan 2026-10-02-gameplay-u9-u10, record §W.9) reach the
  win and the ending under memory pokes (a `poke` step writes bytes in the spin; the port replays
  the capture's `W` records as `poke` lines at the same frame). Their claim is as narrow, and what
  a poke replaced is not claimed: the KO by hits, the six earlier won matches, the final's death
  animations. Only character 0's ending is captured.
```

- [ ] **Step 2:** `docs/PROGRESS.md`: append one paragraph with the two captures (sizes, frames, sha prefixes), the pins, the miss sets, the named divergences and the named gaps of §W.9.
- [ ] **Step 3:** Run **the gate** with `<name>` = `final`. Pass as defined, and also `gp_compare: gp-u9-win: …ok`, `gp-u10-ending … ok`, the two `gp_win … path` and `win` lines `ok`, `port_progress.py` unchanged (no function ported). Record §W.15 with the gate lines.
- [ ] **Step 4:** Commit `AGENTS.md docs/PROGRESS.md docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md`, message `docs: U9/U10 win path and ending under pokes (plan, record §W.15)` plus the trailer.

---

## Execution notes

- **Order:** 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 (no decision needed beyond D3) → [D1] 8 → 9 → [D1, D2] 10 → 11 → 12. D3 gates Task 1, because all harness work assumes the poke mechanism. U8 must be merged first. Its pad-arm edits touch `port_script`, `run_checks` and `gp_parse` on other lines, and its `k_gp_sets` rows sit beside this plan's (touch points).
- **Model tiers:** Tasks 1–3, 5–7: sonnet (mechanical, code given). Task 4: sonnet, with a reviewer who checks the `T` line test and the gp oracles' ok lines. Tasks 8–11: opus (judgement on contingencies, divergence naming and pins). Task 12: sonnet.
- **Gates:** the full gate at Task 0, Task 12 and before merge; the task gate after Tasks 4, 7, 9 and 11 (the speed-up ruling). Never re-capture over a pinned capture: after Task 9 or 11 pins, a new capture of the same name needs the controller's approval and a re-measure of every pin.
- **Interaction with the P track:** porting `0x400E0`, `0x21044`, `0x21084` or `0x3DA50` changes these scenarios' miss sets. The driver's count is exact, so the porting commit must re-measure (Task 9/11 Step 1) and drop the row. Its ratchets may then rise ("improved: raise N").
- **What was run while planning** (record §W.0, §W.7, §W.8):
  - the fixed-up image and capstone listings of every cited decision point;
  - the decoded strings and per-character tables;
  - the default config at the character select (`0x00142095` → difficulty 9, `DS_00105B3A = 0`);
  - the planner harness on the idle, U9 and U10 paths, including the run that stayed in mode `0xD` without the death-done poke;
  - the scratch tree of `b5beff9` with every code block of Tasks 1–7: the suites `Ran 124 … OK`, `run_tests` `all checks passed`, every listed mutation failing as stated, both preview replays (driver checks pass; the misses as predicted; evidence 8/8 and 30/30), the gp-twop oracle unchanged on the edited driver, and the Makefile targets skipping without captures and failing an unpinned stand-in.

  Not run: any DOSBox-X capture (Tasks 8, 10).
