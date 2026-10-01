# Gameplay U7 — Two Players (both sides human) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Capture the pinned original with both sides human — LEFT PLAYER ARCADE, P2 joins in the character select with F2, each side moves its cursor and confirms from its own pad, a short fight in which both press keys, cut at the scenario's end — prove from the capture that it is a two-human match, replay it in the port, name the first divergences, and pin a frame + trace ratchet (`make gp-twop-oracle`, in `make verify`, skipping without the capture).

**Architecture:** One new scenario `gp-twop` in `tools/gp_session.py`, captured by U1's `make gp-capture`, replayed by U2's `make gp-replay`, compared by U3's `tools/gp_compare.py`. A new stdlib tool `tools/gp_twop.py` checks, from the raw's command-word writers (record §T.1.6), that every frame from the join to the end has both sides human; the new Makefile target runs that check before the ratchets. No file under `port/src` changes; the port's own miss set for the replay is pinned in `port/tests/test_platform.c`.

**Tech Stack:** Python 3 stdlib (`unittest`), the U1–U4 tools, DOSBox-X 2026.08.31, the port build (CMake, C11), `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track G ("U7 two players"), §6 (G exit criteria), §7; `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md` §2, §3.2 (pad bits), §3.3 (START MENU rows), §3.4 (credits), §4.1–§4.3 (harness, replay, oracle), §5, §7 Q4/Q7.

**Derivation record:** `docs/superpowers/plans/2026-10-01-gameplay-u7-derivations.md` (§T.0–§T.4 and §T.R written while planning; Tasks 1–7 append §T.5–§T.11). It answers the two questions this unit was asked to run now: **what P2's pad bits do in the original** (§T.2.1: each P2 key sets its spec §3.2 bit in the `+0x2D9` byte; F2 and Home share bit 0, the side-1 start mask `0x100` at `0x9ACBC`; held, P2's bits become the high byte of `DS_001088E2`, newly pressed the low byte, `0x4F6DE`) and **which START MENU rows exist** (§T.1.1: seven rows, `0x2D` LEFT PLAYER ARCADE → side 0 only, `0x2E` RIGHT PLAYER ARCADE → side 1 only, `0x28`/`0x29`/`0x2A`/`0x2B`/`0x2C` both sides human from the start; only the two arcade rows spend a credit, 5 → 4).

---

## Decisions needed from the user

**Decided by the user on 2026-10-01:** all three recommendations accepted — (1) storage for one capture of about 105 MB (at most ~165 MB) approved; (2) path (a), LEFT PLAYER ARCADE with P2 joining in the character select; (3) one capture. Task 4 may start.

1. **Storage for one capture (spec §7 Q7).** `data/k11-captures/gp-twop`: about **105 MB** expected, **at most ~165 MB** (record §T.3: 70 s at 70.09 fps, ~3 535 AVI frames after the post-logo start, 46 KB per stored frame as `gp-idle-loss`), plus a ~30 MB port dump in `/tmp` per replay. *Recommendation:* yes. *If no:* stop after Task 3 (the tools and the target, which skips); nothing is pinned. Task 4 does not start without this answer.
2. **Which two-player path.** (a) LEFT PLAYER ARCADE, P2 joins in the character select (`0x11F28(1)` via `0x43B4E`, record §T.1.2) — exercises the join, the no-debit credit rule (§T.1.3), both-confirm select (§T.1.4), the CPU-slot skip (§T.1.5); (b) the mid-fight join (`0x28CC8` → `0x28DA4`, mode `0x17`) after a one-player start; (c) row 6 "Start 2 PLAYER HANDICAP" (mode `0x2C`, both human from the start, no credit, the only path that reads the handicap dwords, `0x394AC`). *Recommendation:* (a); (b) and (c) are listed as not covered (§T.4.3), (c) belongs with U8's modes. *Cost of the wrong answer:* one more capture (~105 MB) and re-pinning (Tasks 4–6 again).
3. **One capture or two.** A second run (`gp-twop-run2`, another ~105 MB) would measure run-to-run determinism under two-human input; §G.19 showed the original deterministic given the same input frames over a whole one-player match. *Recommendation:* one capture, and §T.4.3 names the determinism of the two-human path as not re-measured. *Cost of the wrong answer:* a trace pin `F` above an unmeasured run-to-run difference; the capture's sha256 pin already forces a re-measure if anyone re-captures.

## Global Constraints

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- Spec (gameplay) §5: "Harness values (`HOLD_FRAMES`, the 150-frame gaps, `time_limit`, …) are named as harness values with their source, never presented as game values." U7's: the 60/30-frame gaps, the holds of 5, the 70 s limit, `GP_TWOP_END` if needed (record §T.3).
- `make verify` is THE gate (common brief): the 45 oracle lines (`grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>`) must equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; the `make audio-render` WAV must be byte-identical to `.superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav`; `python3 tools/port_progress.py` prints `771 1203 64` and `731 731 100` (U7 ports no function). Parallel-safe overrides: `T=u7; SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md`.
- Spec (gameplay) §2: "Fixing any divergence U4 finds is also out of scope: U4 pins it and names it; a follow-up unit fixes it." The same for U7: no file under `port/src` changes.
- AGENTS.md: "Captures are git-ignored. `data/` is git-ignored and read-only" — U7 writes only `data/k11-captures/gp-twop/` through `make gp-capture` (`gp_capture.guard_gp`). Gp captures skip in `make verify` when absent, even under `PR_ORACLE_REQUIRED` (spec §4.3).
- AGENTS.md: "Every env-gated driver and `--check` run arms `fn_resolve`'s miss log and must record exactly the pinned known-set … A port that makes a driver reach a new unregistered code pointer fails it: register the target or pin the miss with its evidence."
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." / "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests, and never assert an unseeded BSS-zero." Python tests are `unittest` in `tools/tests/`, importing with `sys.path.insert(0, ROOT/tools)`.
- AGENTS.md (the gameplay oracle): its claim "is narrow the same way …: no content-bearing capture frame from the window start up to N is unexplained and the traced fields agree below F … **the order of the port's frames and that every port frame appears are not claimed**."
- AGENTS.md: "Commit style: `<area>: <what changed>`. **Never `git add -A`**." Trailer (common brief, relaunch): `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. `docs/PROGRESS.md` is append-only.

## Review Focus

1. **A capture that is not a two-human match** (the join missed its frame, a credit was missing, a CPU still drove a side). Pinned by `gp_twop.py check` (Task 2: `test_one_human_fails`, `test_a_cpu_word_fails`, `test_b1f_dropping_after_the_join_fails`, `test_a_side_that_never_pressed_fails`), run on the capture before anything is pinned (Task 4 Step 3) and inside `gp-twop-oracle` before the ratchets (Task 3, proven to fail on a one-player capture).
2. **A press that never reached the game, or reached it on another frame.** Task 4 Step 4 checks each scripted press against the frames the raw predicts (`b1f = 3` at the P2-start press's `I.f + 2`, `cred` 4 → 4, mode `0x1A` at the P2-confirm press's `I.f + 3`) from the `I` and `S` records, and the scenario test pins holds ≥ 2 (`test_the_presses_are_both_sides_pads`).
3. **The replay's miss set pinned too wide or too narrow.** Task 5 pins `k_miss_gp_twop` to exactly the measured `fn-miss` lines, each classified, and proves a dropped row fails the driver (`test_platform.c` count and "unexpected" checks).
4. **A ratchet pinned where it cannot fail.** Task 6 proves `GP_TWOP_MIN_FIRST + 1`, `GP_TWOP_TRACE_MIN_FIRST + 1`, `GP_TWOP_MAX_START − 1`, a wrong sha256 and a damaged port frame each fail.
5. **Reading green as "the two-player game is correct".** Task 7 states the narrow claim with the pinned values and the record's "cannot show" list (§T.4.3), and names each divergence with its owner (U5 select, U6 moves) rather than fixing it.

## Where to run

Branch `gameplay-u7` in `.worktrees/u7`, off `main`. The plan and its record must be on `main` first (the controller commits `docs/superpowers/plans/2026-10-01-gameplay-u7-*.md` from the planning worktree); if they are not, copy them from `.worktrees/reverse-plans/docs/superpowers/plans/`.

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse
git worktree add .worktrees/u7 -b gameplay-u7 main
cd .worktrees/u7
ln -s ../../data data && ln -s ../../.superpowers .superpowers
cp ../../port/tests/ghidra_data.bin ../../port/tests/title_screen_ref.ppm port/tests/
ls -d data .superpowers port/tests/ghidra_data.bin port/tests/title_screen_ref.ppm docs/superpowers/plans/2026-10-01-gameplay-u7-derivations.md
cmake -S port -B build && cmake --build build 2>&1 | tail -2
```

Every command below assumes `cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse/.worktrees/u7` and `S=/tmp/gameplay-u7; mkdir -p $S`. The SDD ledger is `.superpowers/sdd/2026-10-01-gameplay-u7-two-players/progress.md`.

**Baseline (before Task 1):**

```bash
T=u7; make verify SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md > $S/base_verify.txt 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $S/base_verify.txt | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
python3 tools/port_progress.py
```

Expected: `verify-exit=0`, `ORACLES-EQUAL`, `771 1203 64` and `731 731 100`. Record the three lines in the ledger. (Call this sequence **the gate** below; it takes ~15 min.)

## File Structure

| File | Responsibility |
|---|---|
| `tools/gp_session.py` | + the `gp-twop` scenario (one `SCENARIOS` block) |
| `tools/gp_twop.py` (new) | the two-human check and the mode path, over a `poll.log` or a port `trace.txt` |
| `tools/tests/test_gp_twop.py` (new) | the scenario's schedule and the check's tests |
| `Makefile` | + `GP_TWOP_*` pins and `gp-twop-oracle` (with its own `.PHONY` line); one line in `verify` |
| `port/tests/test_platform.c` | + `k_miss_gp_twop` and its use in `test_fn_misslog_driver` |
| `docs/superpowers/plans/2026-10-01-gameplay-u7-derivations.md` | §T.5–§T.11 |
| `docs/PROGRESS.md` | one appended paragraph |

## Shared-file touch points (all additive; for the controller's merge order)

| File | Region | What U7 adds | Conflict with |
|---|---|---|---|
| `tools/gp_session.py` | `SCENARIOS`, right after the `'gp-idle-loss'` entry (before the dict's closing `}`, line 125 on `main` `e9271df`) | one block `'gp-twop': dict(...)` | U5/U6/U8/U11 blocks at the same anchor: keep every block |
| `Makefile` | after `gp-oracle`'s recipe (line 459 on `e9271df`) | the `GP_TWOP_*` variables, `.PHONY: gp-twop-oracle` (its own line, so the shared `.PHONY` list is untouched), the target | other units' `GP_<UNIT>_*` blocks at the same anchor |
| `Makefile` | `verify`, after `@$(MAKE) --no-print-directory gp-oracle` | one line `@$(MAKE) --no-print-directory gp-twop-oracle` (Task 6) | other units' one-liners there: keep all |
| `port/tests/test_platform.c` | before `fnm_gp_scenario` (the new table) and inside `test_fn_misslog_driver` (4 changed lines) | `k_miss_gp_twop`, `twop` flag | U5/U6/U8/U11 scenario tables in the same lines; **and** any unit that registers a function in the gp-twop miss set (U6 porting `0x3A588`, a P batch): the second to merge re-measures Task 5 Step 1 and drops the row, or the driver's count check fails |
| `docs/PROGRESS.md` | end | one paragraph | append-only |

The verify's shared unit-test line (`tools.tests.test_gp_session … test_gp_compare`) is **not** edited: `gp-twop-oracle` runs `tools.tests.test_gp_twop` itself. **No harness extension is needed**: `PAD` already holds the nine `p2.*` names, the injector writes any scan's key-state byte, `port_script` and the port driver carry both kb bytes, and `gp-pads` plus the port preview show P2's bits land (record §T.2.1, §T.4.1).

---

### Task 1: The `gp-twop` scenario

**Files:**
- Modify: `tools/gp_session.py` (`SCENARIOS`: one block after `'gp-idle-loss'`)
- Create: `tools/tests/test_gp_twop.py`
- Modify: record (append §T.5)

**Interfaces:**
- Consumes: `gp_session.Schedule`, `ENTER_WAIT`, `expand`, `PAD` (U1)
- Produces: `SCENARIOS['gp-twop']` (so `make gp-capture scenario=gp-twop` accepts it: `gp_capture.py --scenario` chooses from the `gp-` keys)

- [ ] **Step 1: Write the failing test** — create `tools/tests/test_gp_twop.py`:

```python
# tools/tests/test_gp_twop.py (gameplay U7: the gp-twop scenario and the two-human check)
import os, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs


class TestScenario(unittest.TestCase):
    def test_steps_fire_in_order(self):
        # mode 0x27 at 0x141, 0x2D at 0x26E, 0x10 at 0x293 (gp-idle-loss run 1,
        # record §G.18); mode 6 at 0x5F0 (an example frame)
        s = gs.Schedule(gs.SCENARIOS['gp-twop']['steps'])
        self.assertEqual(s.due_boot(gs.ENTER_WAIT), [(0, ('key', 'enter'))])
        s.on_mode(0x141, 0x27)
        self.assertEqual(s.due(0x141 + 149), [(1, ('key', 'enter'))])
        self.assertEqual(s.due(0x141 + 299), [(2, ('key', 'enter'))])
        s.on_mode(0x26E, 0x2D)
        self.assertEqual(s.due(0x292), [])
        s.on_mode(0x293, 0x10)
        self.assertEqual(s.due(0x293 + 58), [])
        self.assertEqual(s.due(0x293 + 59), [(3, ('pad', ('p2.start',), 5))])
        self.assertEqual(s.due(0x293 + 119), [(4, ('pad', ('p1.right',), 5))])
        self.assertEqual(s.due(0x293 + 149), [(5, ('pad', ('p2.left',), 5))])
        self.assertEqual(s.due(0x293 + 179), [(6, ('pad', ('p1.b0',), 5))])
        self.assertEqual(s.due(0x293 + 209), [(7, ('pad', ('p2.b0',), 5))])
        s.on_mode(0x5F0, 6)
        self.assertEqual(s.due(0x5F0 + 59), [(8, ('pad', ('p1.right', 'p2.left'), 30))])
        self.assertEqual(s.due(0x5F0 + 119), [(9, ('pad', ('p1.b1', 'p2.b2'), 5))])
        self.assertEqual(s.due(0x5F0 + 149), [(10, ('pad', ('p1.b2', 'p2.b1'), 5))])
        self.assertIsNone(s.end_frame)
        self.assertEqual(s.due(0x5F0 + 209), [])
        self.assertEqual((s.end_frame, s.fired, s.total), (0x5F0 + 210, 11, 11))

    def test_the_presses_are_both_sides_pads(self):
        # spec §3.2: P2's start is F2 (scan 0x3C, kb 0x0001), b0 Home; P1's b0 is U
        acts = [st[-1] for st in gs.SCENARIOS['gp-twop']['steps'] if st[-1][0] == 'pad']
        names = [n for a in acts for n in a[1]]
        self.assertEqual(sorted(set(n[:2] for n in names)), ['p1', 'p2'])
        self.assertEqual(gs.expand(acts[0]), [('p2.start', 0x3C, 0x3C00, 5)])
        self.assertTrue(all(a[2] >= 2 for a in acts))       # a 1-frame press never reaches the level


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run it to see it fail**

Run: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_twop 2>&1 | grep -E '^ERROR|Error:|^FAILED'`
Expected: two `ERROR: test_…` lines, `KeyError: 'gp-twop'`, `FAILED (errors=2)`.

- [ ] **Step 3: Add the scenario** — in `tools/gp_session.py`, insert after the `'gp-idle-loss'` entry's closing `    )),` (the line after `('until_mode', 0x03, 0),`) and before the dict's `}`:

```python
    # U7 (plan 2026-10-01-gameplay-u7-two-players.md, record 2026-10-01-gameplay-u7-derivations.md
    # §T.3): LEFT PLAYER ARCADE (b1f = 1, credits 5 -> 4), then P2 joins in the
    # character select (0x11F28(1): a credit and P2's start mask 0x100 newly
    # pressed, 0x43B4E; b1f |= 2, no debit), each side moves its cursor once and
    # confirms (bit 0 of its command word, 0x43CAD; both confirmed ends the
    # select, 0x43BF0), and both press keys in the fight. Harness values: holds
    # of 5 (a press reaches the pad level one iteration late, record §G.7.2, so
    # a 1-frame press never acts), the 60/30-frame gaps, the end 60 frames after
    # the last press, and the 70 s limit (record §T.3).
    'gp-twop': dict(time_limit=70, steps=(
        ('boot', ENTER_WAIT, ('key', 'enter')),       # mode 3 -> 0x27, MAIN MENU on "Start"
        ('after_mode', 0x27, 150, ('key', 'enter')),  # START MENU, cursor on row 0 (spec §3.3)
        ('after', 150, ('key', 'enter')),             # LEFT PLAYER ARCADE: mode 0x2D
        ('after_mode', 0x10, 60, ('pad', ('p2.start',), 5)),   # P2 joins (F2)
        ('after', 60, ('pad', ('p1.right',), 5)),     # P1 cursor 0 -> 1
        ('after', 30, ('pad', ('p2.left',), 5)),      # P2 cursor 5 -> 4
        ('after', 30, ('pad', ('p1.b0',), 5)),        # P1 confirms (U)
        ('after', 30, ('pad', ('p2.b0',), 5)),        # P2 confirms (Home): the select ends
        ('after_mode', 0x06, 60, ('pad', ('p1.right', 'p2.left'), 30)),  # both walk in
        ('after', 60, ('pad', ('p1.b1', 'p2.b2'), 5)),
        ('after', 30, ('pad', ('p1.b2', 'p2.b1'), 5)),
        ('after', 60, ('end',)),
    )),
```

- [ ] **Step 4: Run the tests**

Run: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_twop tools.tests.test_gp_session tools.tests.test_gp_capture tools.tests.test_gp_compare 2>&1 | tail -3` and `python3 tools/gp_capture.py --help | grep -c gp-twop`
Expected: `Ran 66 tests … OK` (U1–U4's 64 plus these 2); `1`.

- [ ] **Step 5: Mutation proof** — change `('pad', ('p2.start',), 5)` to `('pad', ('p2.start',), 1)`; run `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_twop`. Expected: `FAIL: test_steps_fire_in_order` and `FAIL: test_the_presses_are_both_sides_pads`, `FAILED (failures=2)`. Restore; `OK`.

- [ ] **Step 6: Record and commit** — append to the record:

```markdown
## §T.5 The scenario (Task 1)

`SCENARIOS['gp-twop']` as §T.3. `tools/tests/test_gp_twop.py` (TestScenario, 2 tests): before the
block `KeyError: 'gp-twop'` (2 errors); after, the four gp suites `Ran 66 tests … OK`. Mutation
(P2 start hold 5 -> 1): both tests FAIL; restored OK.
```

```bash
git add tools/gp_session.py tools/tests/test_gp_twop.py docs/superpowers/plans/2026-10-01-gameplay-u7-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp-twop scenario: LEFT PLAYER ARCADE, P2 joins in the select, both press (U7 record §T.5)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: `tools/gp_twop.py`, the two-human check

**Files:**
- Create: `tools/gp_twop.py`
- Modify: `tools/tests/test_gp_twop.py` (the import, the fixtures, `TestTwoHuman`)
- Modify: record (append §T.6)

**Interfaces:**
- Consumes: `gp_session.parse`, `snapshots`, `SNAP_FIELDS`, `format_s`
- Produces: `pad_word(new, held, side) -> int`, `classify(r, side) -> 'pad'|'zero'|'entrance'|'other'`, `load(path) -> [str]`, `two_human(lines) -> dict(join, end, frames, counts, pressed, cred_before, cred_join, fail)`, `mode_path(lines) -> [(line, f, mode, b1f, cred, e0, e2)]`; CLI `gp_twop.py check|path --capture DIR | --trace FILE` (exit 1 on a failed check)

- [ ] **Step 1: Write the failing tests** — in `tools/tests/test_gp_twop.py` add `import gp_twop as gt` after `import gp_session as gs`; after the imports add the fixtures:

```python
def _s(f, mode=6, b1f=3, cred=4, new=0, held=0, e0=None, e2=None):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, mode=mode, b1f=b1f, cred=cred, new=new, held=held, t508=1, t50c=2,
                e0=gt.pad_word(new, held, 0) if e0 is None else e0,
                e2=gt.pad_word(new, held, 1) if e2 is None else e2)
    return gs.format_s(0, vals, 0, 0x1E, 0x1E)


def _match():
    """A two-human log: b1f 1 -> 3 at 0x300 (cred stays 4), a fight with both pressing, X at 0x510."""
    L = [_s(f, mode=0x10, b1f=1) for f in range(0x2F0, 0x300)]
    L += [_s(f, mode=0x10) for f in range(0x300, 0x310)]
    L += [_s(0x400, mode=5, e2=0xA000)]                      # character 1's entrance (0x246EE)
    L += [_s(0x500, held=0x10002000)]                         # P1 right held, P2 left held
    L += [_s(0x501, new=0x02000400, held=0x02000400)]         # P1 b1 new, P2 b2 new
    L += [_s(0x502, held=0x10000000, e0=0)]                  # P1 right held, word 0: the 0x24C96 arm
    L += ['X ms=0 f=0510 step=11 end']
    return L
```

and before `if __name__ == '__main__':` the class:

```python
class TestTwoHuman(unittest.TestCase):
    def test_pad_word_is_0x4f644(self):
        # gp-pads f=0x3D6 (record §T.2.1): new = held = 0x24001000 -> e0 2424, e2 1010
        self.assertEqual(gt.pad_word(0x24001000, 0x24001000, 0), 0x2424)
        self.assertEqual(gt.pad_word(0x24001000, 0x24001000, 1), 0x1010)
        self.assertEqual(gt.pad_word(0, 0x24001000, 1), 0x1000)

    def test_a_two_human_match_passes(self):
        r = gt.two_human(_match())
        self.assertEqual(r['fail'], [])
        self.assertEqual((r['join'], r['end'], r['cred_before'], r['cred_join']), (0x300, 0x510, 4, 4))
        self.assertEqual(r['pressed'], [2, 2])
        self.assertEqual(r['counts'][1].get('entrance'), 1)
        self.assertEqual(r['counts'][0].get('zero'), 1)

    def test_a_cpu_word_fails(self):
        L = _match()[:-1] + [_s(0x503, e2=0x2020)] + _match()[-1:]   # gp-idle-loss's first CPU word (f=0x77C)
        r = gt.two_human(L)
        self.assertEqual(len(r['fail']), 1)
        self.assertIn('side 1 command word 2020 at f=503', r['fail'][0])

    def test_records_after_the_end_are_not_checked(self):
        L = _match() + [_s(0x511, b1f=1, e2=0x2020)]           # past X: the capture's idle tail
        self.assertEqual(gt.two_human(L)['fail'], [])

    def test_one_human_fails(self):
        L = [l.replace('b1f=03', 'b1f=01') for l in _match()]
        self.assertEqual(gt.two_human(L)['fail'], ['b1f never reaches 3 (no S record has both sides human)'])

    def test_b1f_dropping_after_the_join_fails(self):
        L = _match()[:-1] + [_s(0x503, b1f=1)] + _match()[-1:]
        self.assertIn('b1f=1 at f=503', gt.two_human(L)['fail'][0])

    def test_a_side_that_never_pressed_fails(self):
        L = _match()[:-4] + [_s(0x500, held=0x10000000)] + _match()[-1:]   # P1 only
        self.assertEqual(gt.two_human(L)['fail'], ['side 1 never pressed a key in the fight (mode 6)'])

    def test_the_entrance_word_is_allowed_in_mode_5_only(self):
        L = _match()[:-1] + [_s(0x503, e2=0xA000)] + _match()[-1:]   # mode 6
        self.assertIn('side 1 command word A000', gt.two_human(L)['fail'][0])

    def test_a_port_trace_reads_as_s_records(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, 'trace.txt')
            with open(p, 'w') as f:
                f.write('\n'.join('T' + l[1:] for l in _match() if l.startswith('S ')) + '\n')
            r = gt.two_human(gt.load(p))
        self.assertEqual((r['join'], r['end'], r['fail']), (0x300, None, []))

    def test_mode_path(self):
        p = gt.mode_path(_match())
        self.assertEqual([(f, m, b1f, cred) for _, f, m, b1f, cred, _, _ in p],
                         [(0x2F0, 0x10, 1, 4), (0x400, 5, 3, 4), (0x500, 6, 3, 4)])
```

- [ ] **Step 2: Run to see it fail**

Run: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_twop 2>&1 | grep -E "Error|FAILED"`
Expected: `ModuleNotFoundError: No module named 'gp_twop'`, `FAILED (errors=1)`.

- [ ] **Step 3: Create `tools/gp_twop.py`**

```python
#!/usr/bin/env python3
"""Gameplay U7 (two players): is a gp capture (or a port replay's trace) a
two-human match? Plan docs/superpowers/plans/2026-10-01-gameplay-u7-two-players.md,
record 2026-10-01-gameplay-u7-derivations.md §T.1-§T.2. Stdlib only.

  gp_twop.py check --capture DIR    exit 0 when DIR/poll.log is a two-human match
  gp_twop.py check --trace FILE     the same over a port trace.txt (T records)
  gp_twop.py path  --capture DIR    the mode path with b1f, cred, e0 and e2

The raw (record §T.1): DS_00104B1F (`b1f`) holds one bit per human side.
0x257A4 stores its argument there (1 for LEFT PLAYER ARCADE, 0x250C4) and the
character select ORs in side + 1 when 0x11F28(side) accepts a start
(0x43B65..0x43B71). With b1f == 3 the versus hook 0x430E8 skips 0x41350
(0x43136 `cmp eax,3; je`), the routine that marks a CPU slot (+0x63 = 1,
0x41385). A human slot's command word (DS_001088E0[side], logged e0/e2) is
written only by 0x4F644 from the pads (0x4F6BD/0x4F6DE), by 0x24C96 (0, while
the slot's +0x41 has bit 0x10) and by 0x246D4 (character 1's entrance in mode
5: 0xA000/0x9000, 0x246EE/0x246F9). The CPU's writers, 0x472CA/0x472FF/0x47325
(0x47208) and 0x3B207..0x3B278 (0x3B134), run only for a slot whose +0x63 is
non-zero (0x472B9, 0x3B170). The check runs up to the scenario's X record (the
capture runs on to its time limit with no input)."""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

ENTRANCE_WORDS = (0xA000, 0x9000)     # 0x246EE / 0x246F9 (mode 5 only)
FIGHT_MODE = 6                        # the fight frame (spec §3.9 item 1)


def pad_word(new, held, side):
    """0x4F644: DS_001088E0 = (new >> 24) | (held >> 16 & 0xFF00) for side 0,
    DS_001088E2 = (new >> 8 & 0xFF) | (held & 0xFF00) for side 1."""
    if side == 0:
        return (new >> 24) & 0xFF | (held >> 16) & 0xFF00
    return (new >> 8) & 0xFF | held & 0xFF00


def classify(r, side):
    """Who wrote this S record's command word for `side`: 'pad', 'zero',
    'entrance' or 'other' (a writer a human slot never runs)."""
    e = r['e2'] if side else r['e0']
    if e == pad_word(r['new'], r['held'], side):
        return 'pad'
    if e == 0:
        return 'zero'
    if r['mode'] == 5 and e in ENTRANCE_WORDS:
        return 'entrance'
    return 'other'


def load(path):
    """poll.log lines, or a port trace.txt with its T records read as S records."""
    with open(path) as f:
        lines = f.read().splitlines()
    if os.path.basename(path) == 'trace.txt':
        lines = ['S' + l[1:] for l in lines if l.startswith('T ')]
    return lines


def two_human(lines):
    """The evidence that a log is a two-human match, up to its X record (all
    records when it has none). Returns a dict: join (the first f with b1f == 3),
    end (the X record's f or None), frames (S records from the join to the end),
    counts ({class: n} per side), pressed (fight-mode S records with a non-zero
    pad word, per side), cred_before/cred_join, and fail (the reasons it is not)."""
    xrec = next((r for r in (gs.parse(l) for l in lines) if r and r['kind'] == 'X'), None)
    end = None if xrec is None else xrec['f']
    snap = gs.snapshots(lines)
    fs = sorted(f for f in snap if end is None or f <= end)
    out = dict(join=None, end=end, frames=0, counts=[{}, {}], pressed=[0, 0],
               cred_before=None, cred_join=None, fail=[])
    join = next((f for f in fs if snap[f]['b1f'] == 3), None)
    if join is None:
        out['fail'].append('b1f never reaches 3 (no S record has both sides human)')
        return out
    out['join'] = join
    out['cred_join'] = snap[join]['cred']
    before = [f for f in fs if f < join]
    if before:
        out['cred_before'] = snap[before[-1]]['cred']
    for f in fs:
        if f < join:
            continue
        r = snap[f]
        out['frames'] += 1
        if r['b1f'] != 3 and not any('b1f' in x for x in out['fail']):
            out['fail'].append('b1f=%X at f=%X after the join at f=%X' % (r['b1f'], f, join))
        for side in (0, 1):
            c = classify(r, side)
            out['counts'][side][c] = out['counts'][side].get(c, 0) + 1
            e = r['e2'] if side else r['e0']
            if c == 'other' and not any('side %d' % side in x for x in out['fail']):
                out['fail'].append('side %d command word %04X at f=%X (mode %X) is not a pad word: a CPU wrote it'
                                   % (side, e, f, r['mode']))
            if r['mode'] == FIGHT_MODE and c == 'pad' and e != 0:
                out['pressed'][side] += 1
    for side in (0, 1):
        if out['pressed'][side] == 0:
            out['fail'].append('side %d never pressed a key in the fight (mode %d)' % (side, FIGHT_MODE))
    return out


def mode_path(lines):
    """[(line number, f, mode, b1f, cred, e0, e2)] at the first S record of each
    mode change (a P record may see the change earlier, without these fields)."""
    out, prev = [], None
    for n, l in enumerate(lines, 1):
        r = gs.parse(l)
        if r and r['kind'] == 'S' and r['mode'] != prev:
            out.append((n, r['f'], r['mode'], r['b1f'], r['cred'], r['e0'], r['e2']))
            prev = r['mode']
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('check', 'path'))
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument('--capture')
    src.add_argument('--trace')
    a = ap.parse_args()
    path = os.path.join(a.capture, 'poll.log') if a.capture else a.trace
    lines = load(path)
    name = os.path.basename(os.path.normpath(a.capture)) if a.capture else path
    if a.cmd == 'path':
        for n, f, m, b1f, cred, e0, e2 in mode_path(lines):
            print('%s:%d f=%X mode=%X b1f=%X cred=%X e0=%04X e2=%04X'
                  % (os.path.basename(path), n, f, m, b1f, cred, e0, e2))
        return 0
    r = two_human(lines)
    if r['join'] is not None:
        print('gp_twop: %s: join f=%X (cred %s -> %X); %d S records from the join to %s; '
              'side 0 %s, side 1 %s; fight presses %d/%d'
              % (name, r['join'], '?' if r['cred_before'] is None else '%X' % r['cred_before'],
                 r['cred_join'], r['frames'], 'the end' if r['end'] is None else 'X f=%X' % r['end'],
                 sorted(r['counts'][0].items()), sorted(r['counts'][1].items()),
                 r['pressed'][0], r['pressed'][1]))
    for x in r['fail']:
        print('gp_twop: %s: FAIL: %s' % (name, x))
    if r['fail']:
        return 1
    print('gp_twop: %s: two-human match: ok' % name)
    return 0


if __name__ == '__main__':
    sys.exit(main())
```

- [ ] **Step 4: Run the tests**

Run: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest -v tools.tests.test_gp_twop 2>&1 | tail -3`
Expected: `Ran 12 tests … OK`.

- [ ] **Step 5: Mutation proofs** (each alone, restore after each; `PYTHONDONTWRITEBYTECODE=1`):

| mutation in `tools/gp_twop.py` | expected |
|---|---|
| `if side == 0:` in `pad_word` → `if side == 1:` | `FAIL: test_pad_word_is_0x4f644`, `test_a_two_human_match_passes`, `test_a_side_that_never_pressed_fails` |
| `if r['b1f'] != 3 and` → `if False and` | `ERROR: test_b1f_dropping_after_the_join_fails` |
| `end = None if xrec is None else xrec['f']` → `end = None` | `FAIL: test_a_two_human_match_passes`, `test_records_after_the_end_are_not_checked` |
| `        if out['pressed'][side] == 0:` → `        if False:` | `FAIL: test_a_side_that_never_pressed_fails` |
| `if r['mode'] == 5 and e in ENTRANCE_WORDS` → `if e in ENTRANCE_WORDS` | `ERROR: test_the_entrance_word_is_allowed_in_mode_5_only` |
| `lines = ['S' + l[1:] for l in lines if l.startswith('T ')]` → `pass` | `FAIL: test_a_port_trace_reads_as_s_records` |
| `if r and r['kind'] == 'S' and r['mode'] != prev` → `if r and r['kind'] == 'S'` | `FAIL: test_mode_path` |

- [ ] **Step 6: The control — the check fails on the one-player captures**

```bash
for c in gp-idle-loss gp-pads gp-idle-loss-run2; do python3 tools/gp_twop.py check --capture data/k11-captures/$c; echo rc=$?; done
python3 tools/gp_twop.py path --capture data/k11-captures/gp-idle-loss | head -6
```

Expected: three `gp_twop: <name>: FAIL: b1f never reaches 3 (no S record has both sides human)` with `rc=1`; the path's first lines `poll.log:4 f=4 mode=3 b1f=0 cred=5 …`, `poll.log:324 f=141 mode=27 b1f=0 cred=5`, `poll.log:633 f=26F mode=1A b1f=1 cred=4`, `poll.log:652 f=281 mode=1B b1f=1 cred=4`, `poll.log:671 f=293 mode=10 b1f=1 cred=4`. (A capture absent on this host: skip it and say so.)

- [ ] **Step 7: Record and commit** — append §T.6 (the 12 tests, each mutation's line, the control's lines) and:

```bash
git add tools/gp_twop.py tools/tests/test_gp_twop.py docs/superpowers/plans/2026-10-01-gameplay-u7-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp_twop: the two-human check from the command-word writers (U7 record §T.6)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: `make gp-twop-oracle` (not yet in `verify`)

**Files:**
- Modify: `Makefile` (one block after `gp-oracle`'s recipe)
- Modify: record (append §T.7)

**Interfaces:**
- Consumes: `make gp-replay` (`GP_OPTIONAL`, `GP_SCRIPT_ARGS`), `tools/gp_compare.py` (`--min-first --trace-min-first --max-start --capture-sha256 --capture-frames`), `tools/gp_twop.py check`
- Produces: `make gp-twop-oracle`; the variables `GP_TWOP_MIN_FIRST`, `GP_TWOP_TRACE_MIN_FIRST`, `GP_TWOP_MAX_START`, `GP_TWOP_CAPTURE_SHA256`, `GP_TWOP_CAPTURE_FRAMES`, `GP_TWOP_END` (empty until Tasks 5–6)

- [ ] **Step 1: The failing check** — `make gp-twop-oracle; echo exit=$?` → ``make: *** No rule to make target `gp-twop-oracle'.  Stop.``, `exit=2`.

- [ ] **Step 2: Add the target** — after the line `		--capture-sha256 "$(GP_IDLE_LOSS_CAPTURE_SHA256)" --capture-frames "$(GP_IDLE_LOSS_CAPTURE_FRAMES)"` (the end of `gp-oracle`'s recipe), insert:

```make

# Gameplay U7 oracle (plan docs/superpowers/plans/2026-10-01-gameplay-u7-two-players.md, record
# docs/superpowers/plans/2026-10-01-gameplay-u7-derivations.md): data/k11-captures/gp-twop, LEFT
# PLAYER ARCADE with P2 joining in the character select and both sides pressing keys in a short
# fight. The tool tests always run; with the capture present it must first be a two-human match
# (tools/gp_twop.py check, record §T.1.6), then the frame and trace ratchets as gp-oracle's. Skips
# without the capture, even under PR_ORACLE_REQUIRED (spec §4.3). An empty pin with the capture
# present FAILS (gp_compare: "not pinned"); U7 Task 6 measures and pins them.
GP_TWOP_MIN_FIRST =
GP_TWOP_TRACE_MIN_FIRST =
GP_TWOP_MAX_START =
GP_TWOP_CAPTURE_SHA256 =
GP_TWOP_CAPTURE_FRAMES =
GP_TWOP_END =
.PHONY: gp-twop-oracle
gp-twop-oracle: build ## Gameplay U7 oracle: two-human check, then frame and trace ratchets on data/k11-captures/gp-twop (skips without it)
	@echo "== gameplay oracle: gp-twop (two humans; frame and trace ratchets; record U7) =="
	$(PYTHON) -m unittest tools.tests.test_gp_twop
	@if [ -d $(K11_CAPTURES)/gp-twop ]; then $(PYTHON) tools/gp_twop.py check --capture $(K11_CAPTURES)/gp-twop; \
		else echo "gp-twop-oracle: no capture at $(K11_CAPTURES)/gp-twop (skipped)"; fi
	@$(MAKE) --no-print-directory gp-replay scenario=gp-twop GP_OPTIONAL=1 GP_SCRIPT_ARGS="$(if $(GP_TWOP_END),--end $(GP_TWOP_END))"
	@$(PYTHON) tools/gp_compare.py --scenario gp-twop --capture $(K11_CAPTURES)/gp-twop \
		--port $(GP_DUMP)/gp-twop --min-first "$(GP_TWOP_MIN_FIRST)" \
		--trace-min-first "$(GP_TWOP_TRACE_MIN_FIRST)" --max-start "$(GP_TWOP_MAX_START)" \
		--capture-sha256 "$(GP_TWOP_CAPTURE_SHA256)" --capture-frames "$(GP_TWOP_CAPTURE_FRAMES)"
```

(Recipe lines start with a TAB.)

- [ ] **Step 3: It skips without the capture, also under `PR_ORACLE_REQUIRED`**

```bash
ls -d data/k11-captures/gp-twop 2>&1 | tail -1          # must say: No such file or directory
make gp-twop-oracle > $S/t3a.txt 2>&1; echo exit=$?; tail -4 $S/t3a.txt
PR_ORACLE_REQUIRED=1 make gp-twop-oracle > $S/t3b.txt 2>&1; echo exit=$?; tail -2 $S/t3b.txt
```

Expected: `exit=0` both times; `Ran 12 tests … OK`, `gp-twop-oracle: no capture at data/k11-captures/gp-twop (skipped)`, `gp-replay: no capture at data/k11-captures/gp-twop`, `gp_compare: no capture at data/k11-captures/gp-twop (skipped)`.

- [ ] **Step 4: It fails on a capture that is not two-human** (a scratch capture root; nothing under `data/` is written):

```bash
rm -rf $S/fakecaps && mkdir -p $S/fakecaps/gp-twop && cp data/k11-captures/gp-idle-loss/poll.log $S/fakecaps/gp-twop/
PR_ORACLE_REQUIRED=1 make gp-twop-oracle K11_CAPTURES=$S/fakecaps GP_DUMP=$S/gpdump > $S/t3c.txt 2>&1; echo exit=$?
grep -E 'gp_twop|Error' $S/t3c.txt
```

Expected: `exit=2`; `gp_twop: gp-twop: FAIL: b1f never reaches 3 (no S record has both sides human)`, `make: *** [gp-twop-oracle] Error 1` (the replay and the ratchets never run). If `gp-idle-loss` is absent on this host, use `gp-pads` (the same FAIL line).

- [ ] **Step 5: Record and commit** — append §T.7 (the three outputs) and:

```bash
git add Makefile docs/superpowers/plans/2026-10-01-gameplay-u7-derivations.md
git commit -m "$(cat <<'EOF'
build: gp-twop-oracle: two-human check then frame and trace ratchets; skips without the capture (U7 record §T.7)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

Then run **the gate** (Where to run). Expected unchanged (the target is not in `verify` yet).

---

### Task 4: Capture `gp-twop` and prove it is a two-human match (needs Decision 1)

**Files:**
- Writes (git-ignored): `data/k11-captures/gp-twop/`
- Modify: record (append §T.8)

**Interfaces:**
- Produces: the capture every later task reads; §T.8's mode path, press table and two-human verdict

- [ ] **Step 1: Capture**

```bash
make gp-capture scenario=gp-twop 2>&1 | tee $S/cap.txt | tail -12
du -sh data/k11-captures/gp-twop; ls data/k11-captures/gp-twop | grep -c raw.gz; cat data/k11-captures/gp-twop/session.txt
shasum -a 256 data/k11-captures/gp-twop/poll.log
```

Expected: every `CHECK … ok` (base, `steps fired 11/11`, end frame reached, mode `0x27` after the Enter, `kb == raw`, frames written, port script v2), `wall` ≈ 70 s, size about 105 MB (≤ ~165 MB, record §T.3). Record `session.txt`, the size, the frame count and the sha256. **If `end frame reached` fails** (mode 6 + 210 frames not reached by 70 s): rerun once with `GP_ARGS="--time-limit 90"` and record both runs and the reason (the host-timed loads, §T.3). **If `steps fired` fails or a check names a pad change "unpinned"** (a press in a snapshot gap, spec §6): stop and report with the `I` lines; do not change the scenario without the controller.

- [ ] **Step 2: The mode path**

```bash
python3 tools/gp_twop.py path --capture data/k11-captures/gp-twop
```

Expected (raw predictions, §T.1, §T.3): `0x27` (b1f 0, cred 5) → `0x1A`/`0x1B` (b1f 1, cred 4: the `0x2D` divert, which has no spin snapshot of its own, as in `gp-idle-loss`) → `0x10` (b1f 1) → `0x1A` with b1f 3 → `0x1B`, `0x11`, `0x17`, `0x1A`, `0x1B`, 5, 6 (b1f 3, cred 4 throughout). Record the lines with their `poll.log:<n>`. Any other path (a CPU loss of the join, mode `0x10` left by the countdown, mode 8 before the end) is a correction: record it with its line; if mode 6 was never reached, stop and report.

- [ ] **Step 3: The two-human check (the evidence step; before anything is pinned)**

```bash
python3 tools/gp_twop.py check --capture data/k11-captures/gp-twop; echo rc=$?
```

Expected: `join f=<J> (cred 4 -> 4); <n> S records from the join to X f=<end>; side 0 [('pad', …)…], side 1 [('pad', …)…]; fight presses <a>/<b>`, `two-human match: ok`, `rc=0`. Only `pad` and `zero` classes are expected (`entrance` only if a cursor ended on class 1, which §T.3 rules out). **If it FAILs, stop**: the capture is not U7's evidence. Record the FAIL line and the `S` records around it, and report to the controller (a `side N … a CPU wrote it` means `+0x63` was set: check `DS_00108173`'s path, §T.1.5).

- [ ] **Step 4: Each press against the raw's prediction**

```bash
cat > $S/presses.py <<'EOF'
import sys; sys.path.insert(0, 'tools')
import gp_session as gs
cap = sys.argv[1]
L = open(cap + '/poll.log').read().splitlines()
snap = gs.snapshots(L)
for n, l in enumerate(L, 1):
    r = gs.parse(l)
    if not (r and r['kind'] == 'I' and 'press' in r):
        continue
    print('poll.log:%d step=%d press=%s f=%X late=%d' % (n, r['step'], r['press'], r['f'], r['late']))
    for d in (1, 2, 3):
        s = snap.get(r['f'] + d)
        print('    S(f+%d)' % d, '-' if s is None else 'f=%X mode=%X b1f=%X cred=%X kb=%04X new=%08X e0=%04X e2=%04X'
              % (s['f'], s['mode'], s['b1f'], s['cred'], s['kb'], s['new'], s['e0'], s['e2']))
EOF
python3 $S/presses.py data/k11-captures/gp-twop
```

(Run while planning on `gp-pads`: e.g. `poll.log:1005 step=18 press=p2.start f=3B6 late=0`, `S(f+1) f=3B7 mode=27 … kb=0001`.) Expected, from the raw and the level lag (spec §3.1: a press written in the spin of `I.f` is in `raw` at `I.f + 1` and in `new` at `I.f + 2`):

| step | press | expected |
|---|---|---|
| 3 | `p2.start` | `S(f+1)` kb `0001`; `S(f+2)` `new` bit 8 set (`00000100`), **`b1f=3`**, `cred=4` (`0x11F28(1)` at `0x43B4E`, no debit `0x2CA93`) |
| 4 | `p1.right` | `S(f+2)` `e0=1010` (new 0x10 + held 0x1000) |
| 5 | `p2.left` | `S(f+2)` `e2=2020` |
| 6 | `p1.b0` | `S(f+2)` `e0=0101`, mode still `0x10` (side 1 not confirmed) |
| 7 | `p2.b0` | `S(f+2)` `e2=0101`; `S(f+3)` **mode `0x1A`** (`0x43BF0`: the next frame's side-0 pass wipes) |
| 8 | `p1.right` + `p2.left` | `S(f+2)` `e0=1010`, `e2=2020`, mode 6 |
| 9, 10 | `p1.b1`+`p2.b2`, `p1.b2`+`p2.b1` | `S(f+2)` `e0`/`e2` low bytes `02`/`04` and `04`/`02` |

A `late=1` press shifts its row by the lateness; record it. A word of 0 where a press is expected in mode 5 or 6 is the raw's `0x24C96` arm (the slot's `+0x41 & 0x10`, record §T.1.6), not a failure: record it with the frame. Any other value is a correction of §T.1 or §T.3: record it with its line (raw wins: re-read the cited address before concluding). Also look at the two fighters in a fight frame (`frame_*.raw.gz` near the end, through `gp_compare.load_capture_frame` → PNG as U4 Task 4 Step 2) and record which characters the two classes (2 and 6, §T.1.4) are; the cursor/class bytes are not logged (§T.4.3).

- [ ] **Step 5: Record and commit** — append §T.8: `session.txt`, the size against the estimate, the frame count, the sha256, the mode path, the check's line, the press table as measured (each with its `poll.log:<n>`), corrections.

```bash
git add docs/superpowers/plans/2026-10-01-gameplay-u7-derivations.md
git commit -m "$(cat <<'EOF'
docs: gp-twop capture: the mode path, the presses and the two-human evidence (U7 record §T.8)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Replay in the port, pin its miss set, triage the first divergences

**Files:**
- Modify: `port/tests/test_platform.c` (before `fnm_gp_scenario`; `test_fn_misslog_driver`)
- Modify (only in the stall/fault branch): `Makefile` (`GP_TWOP_END`)
- Modify: record (append §T.9)

**Interfaces:**
- Consumes: the `PR_GP_DUMP` driver, `fnm_in`, `FNM_N`, `fnm_known` (unchanged)
- Produces: `k_miss_gp_twop[]`; the first unexplained capture frame `j`, the window start, the first differing `f` (for Task 6)

- [ ] **Step 1: The failing driver (measure the miss set)**

```bash
python3 tools/gp_session.py port-script --scenario gp-twop --capture data/k11-captures/gp-twop --out $S/gp-twop.script
head -1 $S/gp-twop.script; grep -c '^bits' $S/gp-twop.script
rm -rf $S/dump && mkdir -p $S/dump
PR_GP_DUMP=$S/dump PR_GP_SCRIPT=$S/gp-twop.script PR_GAME_DIR=data/game/C ./build/run_tests > $S/replay0.txt 2>&1; echo rc=$?
grep -E '^fn-miss|FAIL|all checks|stalled|fault' $S/replay0.txt; tail -3 $S/dump/gp.log
```

Expected: `# gp port script v2: scenario gp-twop`; `rc=1` with `fn-miss PR_GP_DUMP: unexpected …` for each pair outside the base set. The port preview (record §T.4.1, `main` `e9271df`) predicts exactly three: `0x29D60 frontend_mode_1b_step`, `0x5D812 frontend_mode_1b_step`, `0x3A588 fighter_state_3531c`. Record every `fn-miss PR_GP_DUMP 0x… … hits=…` line.

**Branch — the replay stalled or faulted** (`stalled at f=…`, `fault …`, or the end-reached check failed): record the last `T` line of `$S/dump/trace.txt` and the `gp.log` tail; cut the script at the last completed frame `F` (a harness value with that evidence) by setting in the Makefile

```make
# gp-twop replay cut (record §T.9): the port <stalls|faults> at f=<F+1> (<evidence>); the script
# ends at the last completed frame. Remove when the follow-up unit fixes it.
GP_TWOP_END = <F>
```

and redo this step with `--end <F>` on the `port-script` line. A cut replay may record a subset of the pinned set (`fnm_gp_scenario` reads "(cut at"), so pin what the cut run records.

- [ ] **Step 2: Classify each pair** — for each measured pair: if §G.24 of `docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md` classifies it (the three predicted ones are there: the bare `ret`, the runtime stub, the unported state-10 callback owned by U6), cite it; otherwise disassemble it (`$S/dx.py` of record §T.0 over a `diffrun --image-out` image), look it up in the U0 lists (`docs/superpowers/plans/2026-09-30-gameplay-u0-derivations.md` §U0.12 for move-table callbacks), and write one evidence line (what it is, its owner unit). A pair that would be a two-player-logic routine (anything in §T.1's addresses) is a port bug: name it in §T.9, do not fix it here.

- [ ] **Step 3: Pin it** — in `port/tests/test_platform.c`, insert before the comment `/* The scenario named by the first line of PR_GP_SCRIPT` the table whose rows are **exactly the Step 1 lines** (shown with the three the preview predicts; add or remove rows to match the measurement, with one comment line per row):

```c
/* The gp-twop replay's own pairs (gameplay U7, record
 * 2026-10-01-gameplay-u7-derivations.md §T.9: measured on the full replay of
 * data/k11-captures/gp-twop to its X record; each classified there from the
 * raw):
 *   0x29D60 frontend_mode_1b_step: the bare `ret` (record §G.24);
 *   0x5D812 frontend_mode_1b_step: the runtime stub (record §G.24);
 *   0x3A588 fighter_state_3531c: the UNPORTED state-10 callback (§G.24,
 *     owner U6).
 * A scenario with no entry here may miss only the base pair. */
static const fnm_pair k_miss_gp_twop[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
    { 0x3A588u, "fighter_state_3531c" },
};

```

and in `test_fn_misslog_driver` make these four changes:

```c
    int idle_loss = 0, twop = 0, cut = 0;
```
```c
        idle_loss = strncmp(sc, "gp-idle-loss", 12) == 0;
        twop = strcmp(sc, "gp-twop") == 0;
```
```c
               (idle_loss ? (u32)FNM_N(k_miss_gp_idle_loss) : 0u) +
               (twop ? (u32)FNM_N(k_miss_gp_twop) : 0u);
```
```c
        if (!fnm_known(fn_misslog_addr(i), fn_misslog_ctx(i), frontend, idle_loss)
            && !(twop && fnm_in(k_miss_gp_twop, FNM_N(k_miss_gp_twop),
                                fn_misslog_addr(i), fn_misslog_ctx(i)))) {
```

(replacing, in order: `int idle_loss = 0, cut = 0;`; the `idle_loss = strncmp…` line, which stays, plus the new line; the `(idle_loss ? … : 0u);` term; the `if (!fnm_known(…)) {` line.)

- [ ] **Step 4: Run to see it pass, and prove a row matters**

```bash
cmake --build build 2>&1 | grep -E 'error|warning'
PR_GP_DUMP=$S/dump PR_GP_SCRIPT=$S/gp-twop.script PR_GAME_DIR=data/game/C ./build/run_tests > $S/replay1.txt 2>&1; echo rc=$?; tail -1 $S/replay1.txt
```

Expected: no compiler output; `rc=0`, `all checks passed`. Mutation: delete the last row of `k_miss_gp_twop`, rebuild, rerun → `rc=1`, `FAIL …test_platform.c:…: <n> != <n-1>` and `fn-miss PR_GP_DUMP: unexpected 0x… from …`. Restore; `rc=0`. (While planning, on the preview script with the header `scenario gp-twop`: `rc=0`; without the `0x3A588` row `5 != 4` and `unexpected 0x3A588 from fighter_state_3531c`.) Then `PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests | tail -1` → `all checks passed` (the unit suite: the gp-idle-loss and base sets untouched).

- [ ] **Step 5: The port stays two-human**

```bash
python3 tools/gp_twop.py check --trace $S/dump/trace.txt; echo rc=$?
python3 tools/gp_twop.py path --trace $S/dump/trace.txt
```

Expected (the preview did): `two-human match: ok`, `rc=0`, the join at the same `f` as the capture's. A FAIL or another join frame is a port two-player divergence: record it with the first differing `T` line against the capture's `S` line; name it (follow-up) unless it is fully derived here from a §T.1 address in one routine (then still not fixed in U7: `port/src` is out of scope; name it with the fix).

- [ ] **Step 6: The comparison and the triage**

```bash
make gp-report scenario=gp-twop 2>&1 | tee $S/report.txt | grep -E 'gp_compare|FAIL|all checks|window'
```

From `FIRST UNEXPLAINED capture <j> (raw <r>): nearest port <m> …`, triage exactly as U4 Task 4 Step 2 (the `frames.txt` line `<m>`, both frames to PNG with `gp_compare.load_capture_frame`/`load_port_frame` and Pillow into `$S`, look at both; what differs; the owner with its evidence: U4's O1 if it is the character select's idle animation, record §G.20; U5 otherwise in the select; U6 in the fight). From `first difference f=X (…) in <field>`, triage as U4 Task 4 Step 3 (`grep -n " f=$X " data/k11-captures/gp-twop/poll.log`, `grep -n "^T f=$X " /tmp/pr_gp_dump/gp-twop/trace.txt`, the three frames before, the mode at `X`, and whether a §T.9 miss precedes it in `gp.log`). Record the window start (`window from capture <k>`), `j`, `X`, and each named divergence with its owner. Do not fix.

- [ ] **Step 7: Commit**

```bash
git add port/tests/test_platform.c docs/superpowers/plans/2026-10-01-gameplay-u7-derivations.md
git commit -m "$(cat <<'EOF'
test: the gp-twop replay's pinned miss set; first divergences named (U7 record §T.9)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

(Add `Makefile` only if the stall branch set `GP_TWOP_END`.) Then run **the gate**: `make verify` does not run `gp-twop` yet, so it must be unchanged.

---

### Task 6: Pin the ratchets, wire them into `make verify`, prove each fails

**Files:**
- Modify: `Makefile` (the `GP_TWOP_*` values with provenance comments; one line in `verify`)
- Modify: record (append §T.10)

- [ ] **Step 1: The values** (from Task 5 and Task 4; decimal):
  - `GP_TWOP_MIN_FIRST` = `j` (if no capture frame is unexplained: the number of capture frames, the exact pin, as U4);
  - `GP_TWOP_TRACE_MIN_FIRST` = `X` in decimal (if none: the port's last `T` frame + 1); no run-to-run bound (Decision 3; record §G.19);
  - `GP_TWOP_MAX_START` = `k` (the window start; it must be `< j`);
  - `GP_TWOP_CAPTURE_SHA256` = Task 4's `poll.log` sha256; `GP_TWOP_CAPTURE_FRAMES` = Task 4's frame count.

Replace the empty assignments with the values, each preceded by its provenance comment in this form (U4's):

```make
# MIN_FIRST: measured at <commit> (record §T.10): first unexplained capture frame <j> (raw <r>),
# <one-line cause and owner>; raise it when the frame claim improves.
GP_TWOP_MIN_FIRST = <j>
# TRACE_MIN_FIRST: first differing f=<X hex> (decimal <X>) in <field>, <cause and owner>; raise it
# when the trace claim improves.
GP_TWOP_TRACE_MIN_FIRST = <X>
# MAX_START: the window begins at capture frame <k> (raw <r>); it must be < MIN_FIRST.
GP_TWOP_MAX_START = <k>
# The capture the values belong to (record §T.8): poll.log sha256 and frame count of
# data/k11-captures/gp-twop; another capture FAILS until the three values are re-measured.
GP_TWOP_CAPTURE_SHA256 = <sha256>
GP_TWOP_CAPTURE_FRAMES = <n>
```

In `verify`, after `	@$(MAKE) --no-print-directory gp-oracle`, add `	@$(MAKE) --no-print-directory gp-twop-oracle`.

- [ ] **Step 2: Green**

```bash
make gp-twop-oracle > $S/o.txt 2>&1; echo exit=$?; grep -E 'gp_twop|gp_compare' $S/o.txt
```

Expected: `two-human match: ok`, the capture-identity line `matches the pin`, both `ratchet N … ok` lines, the window line, `exit=0`.

- [ ] **Step 3: Each pin can fail**

```bash
make gp-twop-oracle GP_TWOP_MIN_FIRST=$(( <j> + 1 )) > $S/o1.txt 2>&1; echo exit=$?; grep FAIL $S/o1.txt
make gp-twop-oracle GP_TWOP_TRACE_MIN_FIRST=$(( <X> + 1 )) > $S/o2.txt 2>&1; echo exit=$?; grep FAIL $S/o2.txt
make gp-twop-oracle GP_TWOP_MAX_START=$(( <k> - 1 )) > $S/o3.txt 2>&1; echo exit=$?; grep FAIL $S/o3.txt
make gp-twop-oracle GP_TWOP_CAPTURE_SHA256=$(printf '%064d' 0) > $S/o4.txt 2>&1; echo exit=$?; grep FAIL $S/o4.txt
rm -rf $S/dmg; cp -R /tmp/pr_gp_dump/gp-twop $S/dmg
python3 - <<'EOF'
lines = open('/tmp/gameplay-u7/dmg/frames.txt').read().splitlines()
k = len(lines) // 4          # a port frame inside the explained region (below N)
p = '/tmp/gameplay-u7/dmg/frame_%05d.ipx' % k
d = bytearray(open(p, 'rb').read()); d[32000] ^= 0xFF; open(p, 'wb').write(d)
print('damaged', p)
EOF
python3 tools/gp_compare.py --scenario gp-twop --capture data/k11-captures/gp-twop --port $S/dmg --min-first <j> --trace-min-first <X> --max-start <k>; echo rc=$?
```

Expected: `exit=2` with `FAIL: first unexplained <j> < ratchet N <j+1>` (or `N … > end` for the exact pin); `exit=2` with the trace `FAIL`; `exit=2` with `FAIL: window starts at capture <k> … > pinned start <k-1>` (if `k = 0`, skip this one and say so); `exit=2` with `capture: FAIL: poll.log sha256 …`; the damaged dump `FIRST UNEXPLAINED` below `<j>` and `rc=1` (if frame `k` is not exhibited below `j`, pick `k` from a clean capture frame's `nearest port` as U4 did, and record which). Record every output.

- [ ] **Step 4: The gate** — run **the gate**; additionally `grep -E 'gp_twop|gp_compare: gp-twop' $S/…verify log`. Expected: `verify-exit=0`, `ORACLES-EQUAL`, the gp-twop lines as Step 2, `771 1203 64` / `731 731 100`, `cmp /tmp/pr_u7.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav` silent (run `make audio-render AUDIO_WAV=/tmp/pr_u7.wav` first if the verify log did not), `git diff --stat main -- port/src` empty.

- [ ] **Step 5: Commit**

```bash
git add Makefile docs/superpowers/plans/2026-10-01-gameplay-u7-derivations.md
git commit -m "$(cat <<'EOF'
build: pin the gp-twop frame and trace ratchets and run gp-twop-oracle in make verify (U7 record §T.10)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: U7 closure

**Files:**
- Modify: record (append §T.11); `docs/PROGRESS.md` (append one paragraph)

- [ ] **Step 1: Record §T.11** — the narrow claim with the pinned values (U3's wording: no content-bearing capture frame from the window start `k` up to `N` is unexplained, the traced fields agree below `F`, the capture is a two-human match by `gp_twop.py check` from the join to the scenario's end); the answers this unit was asked for (P2's pad bits §T.2.1; the START MENU rows and which side each starts §T.1.1; what a second start does §T.1.2; credits §T.1.3; the handicap `0x64` cannot affect an arcade game §T.1.8); every divergence of Task 5 as a named gap with its evidence and owner (U5 select, U6 moves, or a new one); "Not covered": §T.4.3's list plus every mode of the capture's path past `N`.

- [ ] **Step 2: PROGRESS** — append a paragraph "Gameplay U7: two players (both sides human) …" with: the path, the capture's size and sha256 prefix, the join frame and `cred 4 -> 4`, the three pinned values, the first divergences and owners, the not-covered list in one sentence.

- [ ] **Step 3: Final gate and commit** — run **the gate** (expected as Task 6 Step 4), then:

```bash
git add docs/superpowers/plans/2026-10-01-gameplay-u7-derivations.md docs/PROGRESS.md
git commit -m "$(cat <<'EOF'
docs: gameplay U7 closure: the two-human capture, its ratchets and named gaps (U7 record §T.11)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

## What was run while planning (scratch `…/scratchpad/plans/u7/v2/`)

- The fixed-up image (`diffrun --image-out`, 1 028 304 bytes, equal to the first draft's) and every listing of record §T.1 (capstone 5.0.7), the operand scans of `0x1088E0`/`0x1088E2`/`0x107468`/`0x108170..74`, and Ghidra MCP `get_xrefs_to` (`0x105C04`, `0x108173`, `0x105F30`, `0x10DB0`, `0x10E18`), `get_function_callers 0x2EB80`, `decompile_function 0x2BF08` (read-only).
- §T.2's capture reads (the `gp-pads` P2 presses and chord, the per-side classification of three captures, the sha256s, `gp-idle-loss`'s mode timings and size).
- The preview replay (`run_tests` with `PR_GP_DUMP`, 26 s, 466 frames, 30 MB): as `gp-preview` it fails on the three unknown misses; as `gp-twop` with the Task 5 table `rc=0`; without the `0x3A588` row `5 != 4`.
- In a scratch tree of `main` `e9271df` with Tasks 1–3 and 5's code exactly as written above: `test_gp_twop` before the scenario (`KeyError`, 2 errors), after (`Ran 12 tests … OK`; the four gp suites 76 tests OK), every mutation of Tasks 1, 2 and 5 failing as stated; `gp_twop.py check` on the three captures (`rc=1`) and on the preview trace (`ok`); `make gp-twop-oracle` without the capture (`exit=0`, also under `PR_ORACLE_REQUIRED`) and with a one-player poll.log under `K11_CAPTURES=$S/fakecaps` (`exit=2`, the FAIL line); `PR_ORACLE_REQUIRED=1 run_tests` → `all checks passed`; `make verify` with the target in `verify` and the parallel-safe overrides: every step through the tool tests passed, the 45 oracle lines equal the base, gp-idle-loss's ratchets `203`/`2088` ok, `gp-twop-oracle` ran its 12 tests and skipped; the last step (`git diff --quiet -- port/src/symbols.h`) failed only because the scratch tree is not a git checkout (`git-rc=129`; the regenerated `symbols.h` is byte-identical to `HEAD`'s). `make audio-render` → WAV identical to `before-t2.wav`; `port_progress.py` → `771 1203 64`, `731 731 100`.
- Not run: any DOSBox-X capture (Task 4), and so no `GP_TWOP_*` value; Tasks 4–6's commands that read `data/k11-captures/gp-twop` were exercised only on the stand-ins named above (`presses.py` on `gp-pads`).

## Execution notes

- **Order:** Setup and baseline → Tasks 1, 2, 3 (tools; no capture needed) → **stop for Decisions 1–3** → Task 4 (capture) → 5 → 6 → 7. Tasks 1–3 can be reviewed while the decision is pending.
- **Model tiers:** Tasks 1–3: a standard model (mechanical, code given). Task 4: a strong model (judging the capture against the raw predictions; stop rules). Task 5: a strong model (miss classification and triage, PNG inspection). Task 6: standard (values from Task 5, failure proofs scripted). Task 7: standard.
- **Gate:** the baseline gate before Task 1; after Tasks 3, 5, 6, 7 the full gate (verify exit 0, the 45 oracle lines equal, the WAV identical, `771 1203 64` / `731 731 100`, empty `port/src` diff); after Tasks 1, 2 the tool suites (`Ran 66`/`Ran 76 … OK`).
- **Merge:** see Shared-file touch points; whoever merges after a unit that registers `0x3A588` (U6) or any other function in `k_miss_gp_twop` re-runs Task 5 Steps 1–4 and drops that row.
