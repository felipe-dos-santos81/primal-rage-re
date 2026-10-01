# Gameplay U6b — The Scripted-Moves Capture and the Callbacks It Reaches — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Capture the pinned original while P1 (Sauron, the pick time-out's character) performs twelve raw-derived move attempts against the CPU in round 1 (two each of normal A, normal B and the specials `0x20`, `0x24`, `0x2D`, `0x3D`), prove from the capture which attempts the original performed, replay the capture in the port, port from the raw every unregistered callback the replay reaches (the record predicts `0x3C048`, `0x3D1EC`, `0x3F0A8` and the three callbacks it stores, `0x3F0F0`/`0x3F130`/`0x2BEF4`, `0x3A820`, `0x231C0`), and pin frame, trace and moves ratchets in `make verify`.

**Architecture:** Five snapshot bytes are appended to the capture's `S` records and the port's `T` lines (the last reaction per side, the characters, slot 0's block bits); `tools/gp_compare.py` gains a third claim, `moves`, over those bytes with its own ratchet. A new `tools/gp_moves.py` decodes the keyboard command table from the fixed-up image and turns each move's phases into frame-keyed pad presses; its output is the literal `gp-u6-moves` scenario in `tools/gp_session.py`, and its `check` subcommand reads the capture to say which attempts were performed. The callbacks are ported one task each, gated on the replay's miss log.

**Tech Stack:** Python 3 (stdlib; unittest), C (the port), DOSBox-X 2026.08.31 (`make gp-capture`), the U1–U4 gameplay tools, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track G (U6 moves; O3), §3 decisions 1–3, §6; `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md` §2, §3.1 (the loop, the debounce), §3.2 (the pad bitmap), §4.1–§4.3, §5.

**Derivation record:** `docs/superpowers/plans/2026-10-01-gameplay-u6-derivations.md` §U6.10–§U6.20 (U6b). Execution appends §U6.22 (U6b execution).

**Depends on:** U6a (`2026-10-01-gameplay-u6a-idle-loss-divergences.md`) merged: its four ports make round 1 a fight the CPU does not stall, the `0x3A820` check reuses U6a's `pose_3a588_seed`, and the record's dry runs were measured on the U6a tree.

## Decisions needed from the user

**Decided by the user on 2026-10-01:** all three recommendations accepted — (1) the capture (about 105 MB, plus about 113 MB of /tmp and about 55 s per `make verify`) approved; (2) the five snapshot bytes are appended to every capture snapshot and the port trace line, and that task merges early; (3) every reached callback the record decodes is ported, including the CPU's `0x231C0`; anything else is pinned for track P.

1. **The capture and its cost (gates Task 6).** One DOSBox-X capture `data/k11-captures/gp-u6-moves` of about **105 MB** (2136 frames to `f = 0xCAF`, measured from the `gp-idle-loss` capture's sizes, record §U6.12), and in every `make verify` a replay of about 1742 port frames (**~113 MB** of `/tmp`, ~55 s) plus the comparison. *Recommendation:* accept. *Cost of the wrong answer:* without it U6b stops after Task 5 (the tools, the scenario and the dry run are delivered; no callback can be claimed as capture-reached, and Tasks 8–13 move to track P).
2. **The snapshot format (Task 1).** Five bytes appended to `SNAP_FIELDS` and to the port's `T` line change the `poll.log`/`trace.txt` format for every capture made after the merge (older captures lack the fields and every tool tolerates that, Task 1's tests). U5, U7, U8 and U11 run in parallel: whoever merges second resolves a one-line textual conflict in `SNAP_FIELDS`. *Recommendation:* accept and merge U6b Task 1 early. *Cost of the wrong answer:* without the bytes the capture cannot name the reaction a move produced (the check falls back to `e0`, which proves the input, not the move).
3. **Porting scope (Tasks 8–14).** Port every unregistered callback the replay reaches, P1's or the CPU's (the record's dry run reached the CPU's `0x231C0`), and the cascade they open, but only the functions the record decodes (§U6.13–§U6.18); any other reached target is pinned in the replay's set with its evidence and handed to track P. *Recommendation:* yes. *Cost of the wrong answer:* porting further targets without a record decode would ship unverified code; pinning more would leave capture-reached gameplay unported.

---

## Global Constraints

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." "**On any plan-vs-raw conflict the raw wins.**"
- Common brief: "Harness values (hold frames, gaps, time limits) are named harness values with their source." Here: the 100-frame gap, the 10-frame offset after mode 6, the 4-frame step, the 130 s limit (§U6.12); `REPRESS = 2` is derived (spec §3.1), not a harness value.
- AGENTS.md: "One C function per original function, header comment `/* 0xADDR — spec section */`. Mark deliberate deviations `/* PORT: ... */` … No other comment styles in `port/src`." "Code addresses stored in data go through `fn_origin()` / `fn_resolve()`." "`port/src/symbols.h` is generated … where the generator emits no name, use a local `#define` with the raw address."
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." "**Assertions must be able to fail.**" Python tests are `unittest` in `tools/tests/` with `sys.path.insert(0, ROOT/tools)`.
- AGENTS.md: "Every env-gated driver … must record exactly the pinned known-set … A port that makes a driver reach a new unregistered code pointer fails it: register the target or pin the miss with its evidence."
- AGENTS.md (gameplay oracle): the claim is narrow; "It cannot detect a port that under-renders, and **the order of the port's frames and that every port frame appears are not claimed**". Record §G.24 item 5: "Never re-capture to 'refresh' a pin: capture to a new scenario name, measure it, and pin its own `poll.log` hash."
- Common brief: the 45 oracle lines equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; the WAV is byte-identical to `before-t2.wav`; **`port_progress.py` must still print `771 1203 64` and `731 731 100`** (none of the U6b functions is in `prage.functions.csv`, record §U6.19 item 8). Gp captures skip in `make verify` when absent (spec §4.3).
- AGENTS.md: "`data/` is git-ignored and read-only" except `data/k11-captures/` through `make gp-capture`. "Never `git add -A`." Commit style `<area>: <what changed>`; trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Python mutation proofs run with `PYTHONDONTWRITEBYTECODE=1` (a same-size edit restored within the same second leaves a stale `.pyc`: measured while planning).

## Review Focus

1. **A capture that did not perform the moves** (wrong character, wrong facing, an attempt eaten by the CPU's attack). Task 6's `gp_moves.py check` must name the performed attempts from `r0` and `c0` before anything is pinned; a move never performed is a named gap, not a re-capture.
2. **A port whose untraced effects differ from the raw** (voice ids, hold floats, callback pointers). Each port task's seeded tests and the listed mutations (all must fail except the one equivalent mutant of §U6.15, which is named).
3. **The pinned miss set drifting.** Each port task states the miss line that must disappear; Task 14 pins anything left with its evidence; the replay ends `all checks passed` before Task 15 pins the ratchets.
4. **A shared-tool change that moves an existing oracle.** Task 1 and Task 2 compare the `gp-idle-loss` oracle output before and after (identical), and Task 15 runs the full gate (45 lines, WAV, K11, gp-pads, gp-idle-loss).
5. **A ratchet that cannot fail or claims more than the capture.** Task 15 pins measured values only and shows each fails at N+1 (and the start at its pin minus 1); the record states the narrow claim (frames before N, traced fields before F, move fields before M).

## Where to run

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse
git worktree add .worktrees/gameplay-u6b -b gameplay-u6b main      # main with U6a merged
cd .worktrees/gameplay-u6b
ln -s /Users/felipe.dos.santos/code/mine/primal-rage-reverse/data data
ln -s /Users/felipe.dos.santos/code/mine/primal-rage-reverse/.superpowers .superpowers
cp ../../port/tests/ghidra_data.bin ../../port/tests/title_screen_ref.ppm port/tests/
make build
export S=/tmp/pr_u6b; mkdir -p $S
export V="SMK_DUMP=/tmp/pr_u6b_smk TITLE_DUMP=/tmp/pr_u6b_title ATTRACT_DUMP=/tmp/pr_u6b_att FRONTEND_DUMP=/tmp/pr_u6b_fe TITLE_PIN_DIR=/tmp/pr_u6b_pin AUDIO_WAV=/tmp/pr_u6b.wav K11_DUMP=/tmp/pr_u6b_k11 GP_DUMP=/tmp/pr_u6b_gp DIFF_IMAGE=/tmp/pr_u6b_diffimg DIFF_TABLE=/tmp/pr_u6b_diff.md"
build/diffrun --exe data/game/C/PRAGE.EXE --image-out $S/image.bin && shasum -a 256 $S/image.bin
```

Expected sha256 `0cfd6f481182f050d5897871068bb9f8fa4410108a56cba0b17be7d513943dec` (record §U6.0); a different image means a different `PRAGE.EXE`: stop. All commands run from `.worktrees/gameplay-u6b`.

---

### Task 0: Baseline (U6a merged)

**Files:** none.

- [ ] **Step 1:** `make gp-oracle $V > $S/t0_oracle.txt 2>&1; echo "rc=$?"; grep -E 'ratchet N|fn-miss' $S/t0_oracle.txt`
Expected: `rc=0`, `distinct=4`, `first unexplained 203, ratchet N 203 ok`, `first differing 2161, ratchet N 2161 ok` (U6a's pins). Keep `grep '^gp_compare' $S/t0_oracle.txt > $S/t0_gp_lines.txt`.
- [ ] **Step 2:** `make verify $V > $S/t0_verify.txt 2>&1; echo "verify-exit=$?"`, the 45-line `diff` against `oracle-lines-base.txt` (`ORACLES-EQUAL`), `make audio-render AUDIO_WAV=/tmp/pr_u6b.wav` and `cmp` with `before-t2.wav` (`WAV-EQUAL`), `python3 tools/port_progress.py` (`771 1203 64`, `731 731 100`).

---

### Task 1: Five snapshot bytes in `poll.log` and the port's `T` line

Record §U6.11. Decision 2.

**Files:**
- Modify: `tools/gp_session.py` (`SNAP_FIELDS`: append after `('ent', 0x10741C, 4),`; a new `MOVE_FIELDS` line after `TRACE_FIELDS`)
- Modify: `port/tests/test_game.c` (`gp_trace_line`)
- Modify: `tools/tests/test_gp_session.py` (`TestTables`: three tests after `test_snap_fields_cover_the_spin_predicate_and_trace`)

**Interfaces:** Produces `gs.SNAP_FIELDS[-5:]` = `r0 r1 c0 c1 s0_43`, `gs.MOVE_FIELDS`; the `T` line gains the same five fields in the same order. Consumes nothing new.

- [ ] **Step 1: Write the failing tests** (in `TestTables`, after `test_snap_fields_cover_the_spin_predicate_and_trace`):

```python
    def test_the_u6_fields_are_appended(self):
        # plan gameplay-u6b, record gameplay-u6 §U6.11: after every U1 field, so older
        # poll.log lines are a prefix of the new format
        self.assertEqual(gs.SNAP_FIELDS[-5:], (('r0', 0x1088A8, 1), ('r1', 0x1088A9, 1),
                                                ('c0', 0x10782A, 1), ('c1', 0x1078BE, 1),
                                                ('s0_43', 0x1077F3, 1)))
        self.assertEqual(gs.MOVE_FIELDS, ('c0', 'c1', 'r0', 'r1', 's0_43'))
        self.assertFalse(set(gs.MOVE_FIELDS) & set(gs.TRACE_FIELDS))

    def test_the_port_t_line_writes_every_snap_field_in_order(self):
        # port/tests/test_game.c gp_trace_line: the capture's S names and order (spec §4.2)
        import re
        src = open(os.path.join(ROOT, 'port', 'tests', 'test_game.c')).read()
        body = src[src.index('static void gp_trace_line(void)'):]
        fmt = ''.join(re.findall(r'"([^"]*)"', body[:body.index(');')]))
        self.assertTrue(fmt.startswith('T '), fmt)
        self.assertEqual([p.split('=')[0] for p in fmt[2:].split()], [n for n, _, _ in gs.SNAP_FIELDS])

    def test_an_s_line_without_the_u6_fields_still_parses(self):
        old = [(n, d, s) for n, d, s in gs.SNAP_FIELDS if n not in gs.MOVE_FIELDS]
        body = ' '.join('%s=%0*X' % (n, 2 * s, 7) for n, _, s in old)
        snaps = gs.snapshots(['S ms=1 %s kb=0000 head=001E tail=001E' % body])
        self.assertEqual(snaps[7]['rng'], 7)
        self.assertNotIn('r0', snaps[7])

```

- [ ] **Step 2: Run to verify they fail**: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_session 2>&1 | tail -3` ⇒ `FAILED (failures=1, errors=1)`: `test_the_u6_fields_are_appended` (`Tuples differ`) and `test_an_s_line_without_the_u6_fields_still_parses` (`AttributeError: module 'gp_session' has no attribute 'MOVE_FIELDS'`). `test_the_port_t_line_writes_every_snap_field_in_order` passes now (both sides still agree); it fails when only one side changes (Step 5).

- [ ] **Step 3: Implement.** In `SNAP_FIELDS`, after `    ('ent', 0x10741C, 4),` add:

```python
    # Plan gameplay-u6b, record gameplay-u6 §U6.11 (appended, so older poll.log
    # lines simply lack them): the last reaction each side applied (0x34E2C's
    # 0x34EF6 store to DS_001088A8 + side), the slots' characters (+0x7A) and
    # slot 0's +0x43 (0x1A6AC's block bits 0x20/0x10).
    ('r0', 0x1088A8, 1), ('r1', 0x1088A9, 1), ('c0', 0x10782A, 1), ('c1', 0x1078BE, 1),
    ('s0_43', 0x1077F3, 1),
```

After the `TRACE_FIELDS = …` line add:

```python
MOVE_FIELDS = ('c0', 'c1', 'r0', 'r1', 's0_43')     # gp_compare's moves claim (record gameplay-u6 §U6.11)
```

Apply the `T`-line change (`git apply`):

```diff
--- a/port/tests/test_game.c
+++ b/port/tests/test_game.c
@@ -12256,7 +12256,8 @@
     fprintf(gp_trace,
             "T f=%04X mode=%04X st=%04X tick=%08X t508=%08X t50c=%08X raw=%08X pad=%08X new=%08X held=%08X "
             "e0=%04X e2=%04X rng=%08X cred=%08X fp=%02X b1d=%02X b1f=%02X b25=%02X w10d=%02X cnt=%02X "
-            "s0_52=%02X s0_54=%02X s0_5a=%02X s1_52=%02X s1_54=%02X s1_5a=%02X ent=%08X\n",
+            "s0_52=%02X s0_54=%02X s0_5a=%02X s1_52=%02X s1_54=%02X s1_5a=%02X ent=%08X "
+            "r0=%02X r1=%02X c0=%02X c1=%02X s0_43=%02X\n",
             (unsigned)DSW(DS_000EF6DC), (unsigned)DSW(DS_00104B00), (unsigned)DSW(DS_000F0A64),
             (unsigned)DSD(DS_00101500), (unsigned)DSD(DS_00101508), (unsigned)DSD(DS_0010150C),
             (unsigned)DSD(DS_000E1C30), (unsigned)DSD(DS_000E1C34), (unsigned)DSD(DS_001088E4),
@@ -12266,7 +12267,10 @@
             (unsigned)DSB(GP_DS_0010810D), (unsigned)DSB(DS_00108110),
             (unsigned)DSB(DS_00107802), (unsigned)DSB(DS_00107804), (unsigned)DSB(DS_0010780A),
             (unsigned)DSB(DS_00107896), (unsigned)DSB(DS_00107898), (unsigned)DSB(DS_0010789E),
-            (unsigned)DSD(DS_0010741C));
+            (unsigned)DSD(DS_0010741C),
+            (unsigned)DSB(DS_001088A8), (unsigned)DSB(DS_001088A8 + 1u),
+            (unsigned)DSB(DS_0010782A), (unsigned)DSB(DS_001078BE),
+            (unsigned)DSB(DS_001077B0 + 0x43u));
     gp_trace_lines++;
 }
 
```

- [ ] **Step 4: Run**: `cmake --build build && PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_session tools.tests.test_gp_capture tools.tests.test_gp_compare 2>&1 | tail -1` ⇒ `OK`; `PR_GAME_DIR=data/game/C ./build/run_tests | tail -1` ⇒ `all checks passed`.

- [ ] **Step 5: Mutation proofs**: `('s0_43', 0x1077F3, 1)` → `0x1077F2` ⇒ `FAIL: test_the_u6_fields_are_appended`; `cp port/tests/test_game.c $S/tg.c && git checkout port/tests/test_game.c`, run the tests ⇒ `FAIL: test_the_port_t_line_writes_every_snap_field_in_order`; `cp $S/tg.c port/tests/test_game.c`.

- [ ] **Step 6: The existing oracles see no change**: `make gp-oracle $V 2>&1 | grep '^gp_compare' | diff - $S/t0_gp_lines.txt && echo GP-EQUAL`; `make gp-replay scenario=gp-pads $V | tail -2` ⇒ `distinct=2`, `all checks passed`; `grep -c ' r0=' /tmp/pr_u6b_gp/gp-idle-loss/trace.txt` ⇒ the `T` line count (7 999).

- [ ] **Step 7: Commit**: `git add tools/gp_session.py tools/tests/test_gp_session.py port/tests/test_game.c`; `tools: five move bytes in the gp snapshot and the port trace (record gameplay-u6 §U6.11)`.

---

### Task 2: The `moves` claim in `gp_compare`

Record §U6.11.

**Files:** Modify `tools/gp_compare.py` (`trace_claim`, `main`); `tools/tests/test_gp_compare.py` (one test in `TestTrace` before `test_nothing_compared_fails`, one in `TestCli` before `test_an_absent_capture_skips`).

**Interfaces:** Produces `trace_claim(name, cap_lines, port_lines, min_first, out=print, report=False, fields=None, what='trace')` (the old call form unchanged) and `gp_compare.py … --moves-min-first N`.

- [ ] **Step 1: Write the failing tests.** In `TestTrace` before `def test_nothing_compared_fails`:

```python
    def test_the_moves_claim_compares_the_move_fields_only(self):
        # record gameplay-u6 §U6.11: rng differs at f=2 (the trace's business), r0 at f=5
        port = [_t(f, rng=f, r0=0xFF if f < 5 else 0x20) for f in range(10)]
        cap = [_S(f, rng=f if f != 2 else 9, r0=0xFF if f < 6 else 0x20) for f in range(10)]
        out = []
        rc, first = gc.trace_claim('t', cap, port, 5, out.append, False, gs.MOVE_FIELDS, 'moves')
        self.assertEqual((rc, first), (0, 5), out)
        self.assertTrue(any('moves: first difference f=5 (5) in r0' in l for l in out), out)
        self.assertFalse(any('tick' in l or 'normalised' in l for l in out), out)
        rc, _ = gc.trace_claim('t', cap, port, 6, out.append, False, gs.MOVE_FIELDS, 'moves')
        self.assertEqual(rc, 1)
        self.assertTrue(any('moves: FAIL: first differing 5 < ratchet N 6' in l for l in out), out)
        rc, first = gc.trace_claim('t', cap, port, 2, out.append)
        self.assertEqual((rc, first), (0, 2))                 # the trace claim is unchanged

```

In `TestCli` before `def test_an_absent_capture_skips`:

```python
    def test_the_moves_claim_runs_when_asked_or_reported_with_its_fields(self):
        self.dump()                                   # a poll.log with the U6 fields (SNAP_FIELDS)
        rc, out = self.cli('--report', '--scenario', 'gp-x', '--capture', self.cap, '--port', self.port)
        self.assertIn('moves: 3 frames compared', out)
        with open(os.path.join(self.cap, 'poll.log'), 'w') as f:      # an older capture
            f.write(''.join(' '.join(p for p in _S(k, t508=k - 1).split()
                                     if p.split('=')[0] not in gs.MOVE_FIELDS) + '\n' for k in range(10, 13)))
        rc, out = self.cli('--report', '--scenario', 'gp-x', '--capture', self.cap, '--port', self.port)
        self.assertEqual(rc, 0)
        self.assertNotIn('moves:', out)

```

- [ ] **Step 2: Run to verify they fail**: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_compare 2>&1 | tail -1` ⇒ `FAILED` (`TypeError: trace_claim() takes … positional arguments` and `'moves: 3 frames compared' not found`).

- [ ] **Step 3: Implement** (`git apply`):

```diff
--- a/tools/gp_compare.py
+++ b/tools/gp_compare.py
@@ -301,7 +301,12 @@
     return {n: tuple(v) for n, v in out.items()}
 
 
-def trace_claim(name, cap_lines, port_lines, min_first, out=print, report=False):
+def trace_claim(name, cap_lines, port_lines, min_first, out=print, report=False, fields=None, what='trace'):
+    """The trace claim over `fields` (gp_session.TRACE_FIELDS by default). With
+    what='moves' (gp_session.MOVE_FIELDS, record gameplay-u6 §U6.11) the same
+    comparison is labelled 'moves' and the tick and normalised lines are left to
+    the trace claim."""
+    fields = gs.TRACE_FIELDS if fields is None else fields
     cap = gs.snapshots(cap_lines)
     port = {}
     for l in port_lines:
@@ -318,19 +323,34 @@
         compared += 1
         if tick_first is None and cap[f]['tick'] != port[f]['tick']:
             tick_first = f
-        for n in gs.TRACE_FIELDS:
+        for n in fields:
             if cap[f][n] != port[f][n]:
                 first, field = f, n
                 break
         if first is not None:
             break
-    out('gp_compare: %s: trace: %d frames compared%s (f %s..), %d without a capture snapshot; first tick '
-        'difference %s (reported, not ratcheted)'
-        % (name, compared, '' if first is None else ' up to the first difference', ('%X' % fs[0]) if fs else '-', skipped,
-           'none' if tick_first is None else 'f=%X' % tick_first))
+    if what != 'trace':
+        out('gp_compare: %s: %s: %d frames compared%s (f %s..) over %s, %d without a capture snapshot'
+            % (name, what, compared, '' if first is None else ' up to the first difference',
+               ('%X' % fs[0]) if fs else '-', ' '.join(fields), skipped))
+    else:
+        out('gp_compare: %s: trace: %d frames compared%s (f %s..), %d without a capture snapshot; first tick '
+            'difference %s (reported, not ratcheted)'
+            % (name, compared, '' if first is None else ' up to the first difference', ('%X' % fs[0]) if fs else '-', skipped,
+               'none' if tick_first is None else 'f=%X' % tick_first))
     if first is not None:
-        out('gp_compare: %s: trace: first difference f=%X (%d) in %s: capture %X, port %X'
-            % (name, first, first, field, cap[first][field], port[first][field]))
+        out('gp_compare: %s: %s: first difference f=%X (%d) in %s: capture %X, port %X'
+            % (name, what, first, first, field, cap[first][field], port[first][field]))
+    if what != 'trace':
+        if compared == 0:
+            out('gp_compare: %s: %s: FAIL: no port T record has a capture snapshot (nothing compared)' % (name, what))
+            return 1, None
+        end = (fs[-1] + 1) if fs else 0
+        if report:
+            if first is None:
+                out('gp_compare: %s: %s: 0 differing through %d' % (name, what, end - 1))
+            return 0, first
+        return ratchet(name, what, first, end, min_first, out, 'differing'), first
     base = None
     for l in cap_lines:
         r = gs.parse(l)
@@ -394,6 +414,9 @@
                     help="the pinned sha256 of the capture's poll.log; with it a different capture fails")
     ap.add_argument('--capture-frames', default=None,
                     help="the pinned number of frame_*.raw.gz in the capture (given with --capture-sha256)")
+    ap.add_argument('--moves-min-first', default=None,
+                    help='the moves claim over gp_session.MOVE_FIELDS (record gameplay-u6 §U6.11); '
+                         'report mode runs it whenever the capture records those fields')
     ap.add_argument('--report', action='store_true')
     a = ap.parse_args()
     name = a.scenario
@@ -429,7 +452,13 @@
     with open(os.path.join(a.port, 'trace.txt')) as f:
         pl = f.read().splitlines()
     rc2, _ = trace_claim(name, cl, pl, n_trace, print, a.report)
-    return 0 if a.report else (1 if rc1 or rc2 else 0)
+    rc3 = 0
+    snaps = gs.snapshots(cl)
+    has_moves = bool(snaps) and all(n in next(iter(snaps.values())) for n in gs.MOVE_FIELDS)
+    if a.moves_min_first is not None or (a.report and has_moves):
+        rc3, _ = trace_claim(name, cl, pl, None if a.report else _int_or_none(a.moves_min_first),
+                             print, a.report, gs.MOVE_FIELDS, 'moves')
+    return 0 if a.report else (1 if rc1 or rc2 or rc3 else 0)
 
 
 if __name__ == '__main__':
```

- [ ] **Step 4: Run**: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_session tools.tests.test_gp_capture tools.tests.test_gp_compare 2>&1 | tail -1` ⇒ `OK`.

- [ ] **Step 5: Mutation proofs** (`PYTHONDONTWRITEBYTECODE=1`): `        for n in fields:` → `        for n in gs.TRACE_FIELDS:` ⇒ `FAIL: test_the_moves_claim_compares_the_move_fields_only`; the `if a.moves_min_first is not None or (a.report and has_moves):` line → `if True:` ⇒ `ERROR: test_the_moves_claim_runs_when_asked_or_reported_with_its_fields`.

- [ ] **Step 6: The idle-loss oracle is unchanged**: `make gp-oracle $V 2>&1 | grep '^gp_compare' | diff - $S/t0_gp_lines.txt && echo GP-EQUAL` (no `moves:` line: that capture has no move bytes).

- [ ] **Step 7: Commit**: `tools/gp_compare.py tools/tests/test_gp_compare.py`; `tools: gp_compare moves claim over the move bytes (record gameplay-u6 §U6.11)`.

---

### Task 3: `tools/gp_moves.py`

Record §U6.10, §U6.12.

**Files:** Create `tools/gp_moves.py`, `tools/tests/test_gp_moves.py`; Modify `Makefile` (the `verify` tool-test line: append ` tools.tests.test_gp_moves` after `tools.tests.test_gp_compare`).

**Interfaces:** Produces `Image`, `decode(img, char)`, `absolute(mask, facing)`, `tokens(word)`, `presses(phases, facing, step)`, `back(facing)`, `plan(table, moves, facing, gap, step)`, `scenario_steps(attempts)`, `kb_levels(events, base)`, `merge_script(text, bits)`, `expected(table, item)`, `check(snaps, f0, attempts, table, gap)`, `first_press(lines)`; CLI `list | steps | dry | check`. Consumes `gp_session.PAD`, `port_script`, `snapshots`, `parse`, `format_s`.

- [ ] **Step 1: Write the failing tests**: create `tools/tests/test_gp_moves.py`:

```python
# tools/tests/test_gp_moves.py (plan gameplay-u6b, record gameplay-u6 §U6.10-§U6.12)
import os, struct, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_moves as gm
import gp_session as gs

# Character 0's entry 0x00 as the image holds it (record §U6.10): reaction 0x20,
# d 1, e 0, the callback 0x3D17C, five phases.
PH20 = [(0x80000, 0x4000, 0, 3), (0x80000, 0x4000, 0, 3), (0x20000, 0x4000, 0xC000, 15),
        (0x10500, 0x4A00, 0xC000, 15), (0, 0, 0, 10)]
DESC = 0x0D0000


def synthetic():
    """An image from 0x10000 holding one keyboard entry (character 0, i 0) and
    its CPU-table reaction/stance bytes and move-table callback."""
    data = bytearray(0x0D1000 - gm.IMAGE_BASE)
    def d(a, v): struct.pack_into('<I', data, a - gm.IMAGE_BASE, v)
    def w(a, v): struct.pack_into('<H', data, a - gm.IMAGE_BASE, v)
    d(gm.KB_TABLE, DESC)
    w(gm.CPU_TABLE + 4, 0x20)
    data[gm.CPU_TABLE + 6 - gm.IMAGE_BASE] = 1
    d(gm.MOVE_TABLE + 0x20 * 20, 0x3D17C)
    for k, (m0, m1, m2, c) in enumerate(PH20):
        a = DESC + k * gm.PHASE
        d(a, m0); d(a + 4, m1); d(a + 8, m2); w(a + 0xC, c)
    return gm.Image(bytes(data))


def snap(f, **kw):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, **kw)
    return gs.parse(gs.format_s(0, vals, 0, 0, 0))


class TestDecode(unittest.TestCase):
    def test_decode_reads_the_keyboard_table_and_the_cpu_reaction(self):
        t = gm.decode(synthetic(), 0)
        self.assertEqual(sorted(t), [0])
        r, d, e, cb, ph = t[0]
        self.assertEqual((r, d, e, cb), (0x20, 1, 0, 0x3D17C))
        self.assertEqual(ph, PH20)               # stops at the first m0 == 0

    def test_absolute_folds_the_facing_bits(self):
        self.assertEqual(gm.absolute(0x80000, 0), 0x2000)    # held back = held left
        self.assertEqual(gm.absolute(0x80000, 1), 0x1000)
        self.assertEqual(gm.absolute(0x10500, 0), 0x0510)    # new forward = new right
        self.assertEqual(gm.absolute(0x10500, 1), 0x0520)
        self.assertEqual(gm.absolute(0x40000, 0), 0x0020)
        self.assertEqual(gm.absolute(0x20000, 0), 0x1000)

    def test_tokens_split_held_and_new(self):
        self.assertEqual(gm.tokens(0x0510), (['p1.b0', 'p1.b2'], ['p1.right']))
        self.assertEqual(gm.tokens(0x0303), (['p1.b0', 'p1.b1'], ['p1.b0', 'p1.b1']))


class TestPresses(unittest.TestCase):
    def test_the_0x20_motion(self):
        # buttons held from 0; back, forward, forward re-pressed after a 2-frame release
        self.assertEqual(gm.presses(PH20, 0, 4),
                         [(0, 'p1.b0', 12), (0, 'p1.b2', 12), (0, 'p1.left', 4),
                          (4, 'p1.right', 2), (8, 'p1.right', 4)])

    def test_a_repress_needs_the_debounce_gap(self):
        with self.assertRaises(ValueError):
            gm.presses(PH20, 0, 2)

    def test_a_chord_is_one_phase(self):
        self.assertEqual(gm.presses([(0x3, 0xC00, 0, 3), (0, 0, 0, 5)], 0, 4),
                         [(0, 'p1.b0', 4), (0, 'p1.b1', 4)])

    def test_plan_and_steps(self):
        t = {0: (0x20, 1, 0, 0x3D17C, PH20), 0x1A: (0x10, 1, 1, 0, [(0x3, 0xC00, 0, 3), (0, 0, 0, 5)])}
        att = gm.plan(t, [0x1A, 0], 0, 100, 4)
        self.assertEqual([(o, i) for o, i, _ in att], [(0, 0x1A), (100, 0)])
        first, steps = gm.scenario_steps(att)
        self.assertEqual(first, 0)
        self.assertEqual(steps, [('after', 0, ('pad', ('p1.b0', 'p1.b1'), 4)),
                                 ('after', 100, ('pad', ('p1.left',), 4)),
                                 ('after', 0, ('pad', ('p1.b0', 'p1.b2'), 12)),
                                 ('after', 4, ('pad', ('p1.right',), 2)),
                                 ('after', 4, ('pad', ('p1.right',), 4))])

    def test_kb_levels(self):
        self.assertEqual(gm.kb_levels([(0, 'p1.b0', 4), (2, 'p1.left', 4)], 100),
                         [(100, 0x0100), (102, 0x2100), (104, 0x2000), (106, 0)])


class TestScript(unittest.TestCase):
    TEXT = ('# gp port script v2: scenario x (cut at 50)\nenter_frame 10\nenter_state 0000\n'
            'key 10 1C 0D\nbits 30 0100\nend 50\n')

    def test_merge_orders_by_frame_keys_first(self):
        out = gm.merge_script(self.TEXT, [(10, 0x2000), (20, 0)])
        self.assertEqual(out.splitlines()[3:], ['key 10 1C 0D', 'bits 10 2000', 'bits 20 0000',
                                                'bits 30 0100', 'end 50'])

    def test_merge_refuses_bits_past_the_end(self):
        with self.assertRaises(ValueError):
            gm.merge_script(self.TEXT, [(51, 0)])


class TestCheck(unittest.TestCase):
    T = {0: (0x20, 1, 0, 0x3D17C, PH20), 0x1A: (0x10, 1, 1, 0, [(0x3, 0xC00, 0, 3), (0, 0, 0, 5)])}

    def test_performed_not_shown_and_no_snapshot(self):
        att = gm.plan(self.T, [0x1A, 0, 0x1A], 0, 10, 4)
        snaps = {f: snap(f, r0=0xFF) for f in range(100, 120)}
        snaps[103] = snap(103, r0=0x12)          # a 0x10 variant (0x3CBC4)
        snaps[104] = snap(104, r0=0x12)
        res = gm.check(snaps, 100, att, self.T, 10)
        self.assertEqual([v for _, _, v, _ in res], ['performed', 'not shown', 'no-snapshot'])
        self.assertEqual(res[0][3], 103)

    def test_a_reaction_already_shown_is_not_a_new_attempt(self):
        att = gm.plan(self.T, [0], 0, 10, 4)
        snaps = {f: snap(f, r0=0x20) for f in range(99, 110)}
        self.assertEqual(gm.check(snaps, 100, att, self.T, 10)[0][2], 'not shown')

    def test_a_block_is_the_slot_bits(self):
        att = gm.plan(self.T, [gm.BLOCK], 0, 10, 4)
        self.assertEqual(att[0][2], [(0, 'p1.left', 6)])
        snaps = {f: snap(f, s0_43=0x80) for f in range(100, 110)}
        self.assertEqual(gm.check(snaps, 100, att, self.T, 10)[0][2], 'not shown')
        snaps[105] = snap(105, s0_43=0xA0)
        self.assertEqual(gm.check(snaps, 100, att, self.T, 10)[0][2:], ('performed', 105))


class TestFirstPress(unittest.TestCase):
    def test_the_first_p1_press_names_the_attempt_frame(self):
        lines = ['I ms=1 f=0140 step=0 press=enter scan=1C lin=00010070 old=9C bios=1C0D ring=1 late=0',
                 'I ms=2 f=07FE step=3 press=p1.b0 scan=16 lin=0001006A old=96 bios=1675 ring=1 late=0',
                 'I ms=3 f=0862 step=4 press=p1.left scan=2C lin=00010080 old=AC bios=2C7A ring=1 late=0']
        self.assertEqual(gm.first_press(lines), 0x7FF)
        self.assertIsNone(gm.first_press(lines[:1]))


def real_image():
    """The fixed-up image dumped by build/diffrun, or None (skip)."""
    diffrun = os.path.join(ROOT, 'build', 'diffrun')
    exe = os.path.join(ROOT, 'data', 'game', 'C', 'PRAGE.EXE')
    if not (os.path.exists(diffrun) and os.path.exists(exe)):
        return None
    fd, path = tempfile.mkstemp(suffix='.bin')
    os.close(fd)
    try:
        subprocess.run([diffrun, '--exe', exe, '--image-out', path], check=True, capture_output=True)
        return gm.Image.load(path)
    finally:
        os.remove(path)


class TestRealImage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.img = real_image()

    def setUp(self):
        if self.img is None:
            self.skipTest('no build/diffrun or data/game/C/PRAGE.EXE')

    def test_character_0_entries(self):
        t = gm.decode(self.img, 0)
        self.assertEqual({i: t[i][0] for i in (0, 1, 6, 0x15, 0x1A, 0x1B)},
                         {0: 0x20, 1: 0x24, 6: 0x2D, 0x15: 0x3D, 0x1A: 0x10, 0x1B: 0x11})
        self.assertEqual(t[0][4], PH20)
        self.assertEqual({i: t[i][3] for i in (0, 1, 6, 0x15)},
                         {0: 0x3D17C, 1: 0x3F0A8, 6: 0x3D1EC, 0x15: 0x3C048})


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run to verify it fails**: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_moves 2>&1 | tail -2` ⇒ `ModuleNotFoundError: No module named 'gp_moves'`.

- [ ] **Step 3: Implement**: create `tools/gp_moves.py`:

```python
#!/usr/bin/env python3
"""Gameplay moves (plan 2026-10-01-gameplay-u6b-moves-capture, record
2026-10-01-gameplay-u6-derivations §U6.10-§U6.12): decode a character's keyboard
command table from the fixed-up image, turn a move's phases into frame-keyed
pad presses, and check from a poll.log which attempts the original performed.

The tables (raw, record §U6.10): 0x3C600 reads the descriptor of (side, i) from
0xC6B9C when the side's slot +0x63 is 0 and its device word
[DS_00101514]+0x2D4/+0x2D6 is 0, 4 or 6 (the jump table 0x3C5E4: a keyboard
player), and from 0xC619C when +0x63 != 0 (the CPU). Both are 7 characters x
0x20 entries x 8 bytes {u32 desc; u16 reaction; u8 d; u8 e}; the reaction
(0x3CEAD/0x3CEEC) and the stance bytes (0x3CD10/0x3CD29) are read from
0xC619C for both. A descriptor is up to 8 phases of 0x14 bytes: m0 (+0, the
bits to collect), m1 (+4), m2 (+8, reset) and the countdown word +0xC; 0x3C88C
advances a phase once the command word 0x1088E0 has covered m0 and arms the
entry when the next phase's m0 is 0. The command word (0x4F644): low byte = the
P1 pad bits newly pressed this frame, high byte = the level (a new bit is in
both). Bits 0x10000/0x20000/0x40000/0x80000 are facing-relative (0x3C6E8).

Usage:
  gp_moves.py list  --image IMG --char C
  gp_moves.py steps --image IMG --char C --facing F --moves I,I,... --gap G --step S
  gp_moves.py dry   --image IMG --char C --facing F --moves I,I,... --gap G --step S
                    --capture DIR --start F0 --end E --out PATH
  gp_moves.py check --image IMG --char C --facing F --moves I,I,... --gap G --step S
                    --capture DIR"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

IMAGE_BASE = 0x10000        # build/diffrun --image-out dumps mem[] from 0x10000 (record E1 §E.1)
KB_TABLE = 0x0C6B9C         # 0x3C66E: the keyboard player's descriptors (sel 0, 4, 6)
CPU_TABLE = 0x0C619C        # 0x3C699: the CPU's descriptors; +4 reaction, +6 d, +7 e for both
MOVE_TABLE = 0x0A3528       # 0x3AFC4: 20-byte rows (char * 64 + reaction), the callback dword at +0
ENTRIES, ENTRY = 0x20, 8
PHASE, MAX_PHASES = 0x14, 8
NAMES = ('p1.b0', 'p1.b1', 'p1.b2', 'p1.b3', 'p1.right', 'p1.left', 'p1.down', 'p1.up')
# 0x500C4 keeps a bit that changed this iteration at its old level (spec §3.1,
# record gameplay-ground-truth §G.7), so a release shorter than 2 frames never
# reaches the level 0x4F644 reads: a re-press is released 2 frames first.
REPRESS = 2
BLOCK = 'block'             # a move-list item: hold back for one gap (0x1AB5C's block test)
# 0x3CE58: an entry whose reaction is 0x10 or 0x11 applies 0x3CBC4's or 0x3CC58's variant.
VARIANTS = {0x10: (0x10, 0x12, 0x14, 0x16), 0x11: (0x11, 0x13, 0x15, 0x17)}


class Image:
    def __init__(self, data, base=IMAGE_BASE):
        self.data, self.base = data, base

    @classmethod
    def load(cls, path):
        with open(path, 'rb') as f:
            return cls(f.read())

    def d(self, a):
        return struct.unpack_from('<I', self.data, a - self.base)[0]

    def w(self, a):
        return struct.unpack_from('<H', self.data, a - self.base)[0]

    def b(self, a):
        return self.data[a - self.base]


def decode(img, char):
    """{i: (reaction, d, e, callback, [(m0, m1, m2, count), ...])} for the
    keyboard table's non-empty entries of `char`."""
    out = {}
    for i in range(ENTRIES):
        e = (char * ENTRIES + i) * ENTRY
        desc = img.d(KB_TABLE + e)
        if not desc:
            continue
        reaction = img.w(CPU_TABLE + e + 4)
        phases = []
        for k in range(MAX_PHASES):
            a = desc + k * PHASE
            ph = (img.d(a), img.d(a + 4), img.d(a + 8), img.w(a + 0xC))
            phases.append(ph)
            if ph[0] == 0:
                break
        cb = img.d(MOVE_TABLE + (char * 64 + reaction) * 20) if reaction < 64 else 0
        out[i] = (reaction, img.b(CPU_TABLE + e + 6), img.b(CPU_TABLE + e + 7), cb, phases)
    return out


def absolute(mask, facing):
    """0x3C6E8: the facing-relative bits folded into the command word."""
    a, b, c, d = (0x20, 0x2000, 0x10, 0x1000) if facing else (0x10, 0x1000, 0x20, 0x2000)
    out = mask & 0xFFFF
    for bit, v in ((0x10000, a), (0x40000, c), (0x20000, b), (0x80000, d)):
        if mask & bit:
            out |= v
    return out


def tokens(word):
    """A command word -> (held names, new names)."""
    return ([NAMES[k] for k in range(8) if word & (0x100 << k)],
            [NAMES[k] for k in range(8) if word & (1 << k)])


def presses(phases, facing, step):
    """A move's phases -> sorted [(offset, name, hold)]. The buttons the last
    phase asks for as held are pressed at offset 0 and held to the end: with a
    b0/b1 and a b2/b3 bit held, 0x351DB..0x35201 skips the up-jump 0x3BDDC, so
    a motion through up stays a motion. The phases with m0 != 0 then run `step`
    frames apart (phase 1 is skipped when it repeats phase 0: 0x3C88C leaves it
    on the first frame m0 is no longer whole). At each phase every other name
    the phase does not ask for is released, a name it asks for as new is
    (re)pressed there (released REPRESS frames first when it was held), and a name
    it asks for as held is held from there; all are released one step after
    the last phase."""
    seq = [p[0] for p in phases if p[0]]
    if len(seq) > 1 and seq[1] == seq[0]:
        seq = seq[:1] + seq[2:]
    keep = set(n for n in tokens(absolute(seq[-1], facing))[0] if n in NAMES[:4]) if seq else set()
    since, ev, t = {}, [], 0
    for m0 in seq:
        held, new = tokens(absolute(m0, facing))
        need = set(held) | set(new) | keep
        for n in sorted(since):
            if n not in need or n in new:
                end = t - REPRESS if n in new else t
                if end <= since[n]:
                    raise ValueError('%s re-pressed with no gap at offset %d' % (n, t))
                ev.append((since[n], n, end - since[n]))
                del since[n]
        for n in NAMES:
            if n in need and n not in since:
                since[n] = t
        t += step
    for n, s0 in since.items():
        ev.append((s0, n, t - s0))
    return sorted(ev)


def back(facing):
    """The held-back name: 0x80000 folded for this facing."""
    return tokens(absolute(0x80000, facing))[0][0]


def plan(table, moves, facing, gap, step):
    """[(attempt offset, item, [(offset, name, hold)])]: one attempt per item,
    `gap` frames apart (a harness value); an item is a table index or BLOCK
    (hold back for gap - step frames)."""
    out = []
    for k, item in enumerate(moves):
        t = k * gap
        if item == BLOCK:
            ev = [(t, back(facing), gap - step)]
        else:
            ev = [(t + o, n, h) for o, n, h in presses(table[item][4], facing, step)]
        out.append((t, item, ev))
    return out


def scenario_steps(attempts):
    """The attempts -> (first offset, [('after', delta, ('pad', names, hold))]):
    the presses grouped by (frame, hold); the first step's delta is 0 and the
    caller anchors it (an 'after_mode' step)."""
    groups = {}
    for _, _, ev in attempts:
        for f, n, h in ev:
            groups.setdefault((f, h), []).append(n)
    keys = sorted(groups)
    steps, prev = [], keys[0][0]
    for f, h in keys:
        steps.append(('after', f - prev, ('pad', tuple(sorted(groups[(f, h)])), h)))
        prev = f
    return keys[0][0], steps


def kb_levels(events, base):
    """[(frame, name, hold)] -> [(F, kb)]: the kb level iteration F samples (a
    press made for F, gp_session.Schedule), at every frame the level changes."""
    frames = sorted({f for f, _, _ in events} | {f + h for f, _, h in events})
    out = []
    for f in frames:
        kb = 0
        for s, n, h in events:
            if s <= f < s + h:
                kb |= gs.PAD[n][2]
        out.append((base + f, kb))
    return out


def merge_script(text, bits):
    """A port script v2 (gp_session.port_script) plus `bits F KB` lines, in the
    generator's order (by frame; keys before bits at one frame)."""
    lines = text.splitlines()
    head = [l for l in lines if l.startswith(('#', 'enter_'))]
    end = [l for l in lines if l.startswith('end ')]
    last = int(end[0].split()[1])
    ev = []
    for k, l in enumerate(lines):
        p = l.split()
        if p and p[0] in ('key', 'bits'):
            ev.append((int(p[1]), 0 if p[0] == 'key' else 1, k, l))
    for k, (f, kb) in enumerate(bits):
        if f > last:
            raise ValueError('bits at %d past the script end %d' % (f, last))
        ev.append((f, 1, len(lines) + k, 'bits %d %04X' % (f, kb)))
    return '\n'.join(head + [l for _, _, _, l in sorted(ev)] + end) + '\n'


def expected(table, item):
    """The r0 values an attempt counts as performed (BLOCK: None, see check)."""
    if item == BLOCK:
        return None
    r = table[item][0]
    return VARIANTS.get(r, (r,))


def check(snaps, f0, attempts, table, gap):
    """Per attempt: (offset, item, verdict, f). A move is performed when, in
    [F, F + gap), some S record shows r0 in its expected set and the record
    before it does not; a block when s0_43 & 0x30 (0x1A6AC's 0x20/0x10) is set.
    F = f0 + offset; 'no-snapshot' when no S record lies in the window."""
    out = []
    for t, item, _ in attempts:
        F = f0 + t
        fs = [f for f in range(F, F + gap) if f in snaps]
        hit = None
        for f in fs:
            s = snaps[f]
            if item == BLOCK:
                if s['s0_43'] & 0x30:
                    hit = f
                    break
            else:
                want = expected(table, item)
                prev = snaps.get(f - 1)
                if s['r0'] in want and prev is not None and prev['r0'] not in want:
                    hit = f
                    break
        verdict = 'no-snapshot' if not fs else ('performed' if hit is not None else 'not shown')
        out.append((t, item, verdict, hit))
    return out


def first_press(lines):
    """F of the scenario's first pad press: its I record's f is the spin
    snapshot of F - 1 (gp_session.Schedule)."""
    for r in (gs.parse(l) for l in lines):
        if r and r['kind'] == 'I' and str(r.get('press', '')).startswith('p1.'):
            return r['f'] + 1
    return None


def _moves(s):
    return [BLOCK if m == BLOCK else int(m, 16) for m in s.split(',')]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('list', 'steps', 'dry', 'check'))
    ap.add_argument('--image', required=True)
    ap.add_argument('--char', type=int, required=True)
    ap.add_argument('--facing', type=int, default=0)
    ap.add_argument('--moves')
    ap.add_argument('--gap', type=int)
    ap.add_argument('--step', type=int)
    ap.add_argument('--capture')
    ap.add_argument('--start', type=lambda s: int(s, 0))
    ap.add_argument('--end', type=lambda s: int(s, 0))
    ap.add_argument('--out')
    a = ap.parse_args()
    table = decode(Image.load(a.image), a.char)
    if a.cmd == 'list':
        for i, (r, d, e, cb, ph) in sorted(table.items()):
            print('i=%02X reaction=%02X d=%d e=%d callback=%05X phases=%s'
                  % (i, r, d, e, cb, ' '.join('%05X/%05X/%05X/%d' % p for p in ph)))
        return 0
    attempts = plan(table, _moves(a.moves), a.facing, a.gap, a.step)
    if a.cmd == 'steps':
        first, steps = scenario_steps(attempts)
        print('first offset %d' % first)
        for st in steps:
            print('        %r,' % (st,))
        return 0
    if a.cmd == 'dry':
        with open(os.path.join(a.capture, 'poll.log')) as f:
            text = gs.port_script('gp-u6-moves-dry', f.read().splitlines(), end=a.end)
        ev = [e for _, _, evs in attempts for e in evs]
        with open(a.out, 'w') as f:
            f.write(merge_script(text, kb_levels(ev, a.start)))
        print('gp_moves: dry: wrote %s (%d attempts from f=%d)' % (a.out, len(attempts), a.start))
        return 0
    with open(os.path.join(a.capture, 'poll.log')) as f:
        lines = f.read().splitlines()
    F1 = first_press(lines)
    if F1 is None:
        print('gp_moves: check: no p1 press in %s' % a.capture)
        return 1
    f0 = F1 - scenario_steps(attempts)[0]
    n = 0
    for t, item, verdict, hit in check(gs.snapshots(lines), f0, attempts, table, a.gap):
        n += verdict == 'performed'
        print('gp_moves: check: attempt at f=%X %s: %s%s'
              % (f0 + t, 'block' if item == BLOCK else 'i=%02X' % item, verdict,
                 '' if hit is None else ' (f=%X)' % hit))
    print('gp_moves: check: %d of %d attempts performed' % (n, len(attempts)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
```

- [ ] **Step 4: Run**: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_moves -v 2>&1 | tail -3` ⇒ `Ran 15 tests … OK` (`TestRealImage` runs: `build/diffrun` and `data/game/C/PRAGE.EXE` exist). `python3 tools/gp_moves.py list --image $S/image.bin --char 0 | head -2` ⇒ `i=00 reaction=20 d=1 e=0 callback=3D17C phases=80000/04000/00000/3 …` and `i=01 reaction=24 d=1 e=0 callback=3F0A8 …`.

- [ ] **Step 5: Mutation proofs** (each alone, `PYTHONDONTWRITEBYTECODE=1`, restored): `REPRESS = 2` → `1` ⇒ `FAIL: test_a_repress_needs_the_debounce_gap`; swap the two tuples in `absolute` ⇒ `FAIL: test_absolute_folds_the_facing_bits`; `and prev is not None and prev['r0'] not in want` deleted ⇒ `FAIL: test_a_reaction_already_shown_is_not_a_new_attempt`; `| keep` deleted from `need = …` ⇒ `FAIL: test_the_0x20_motion`; `KB_TABLE = 0x0C6B9C` → `0x0C619C` ⇒ `FAIL: test_character_0_entries`; `return r['f'] + 1` → `return r['f']` ⇒ `FAIL: test_the_first_p1_press_names_the_attempt_frame`.

- [ ] **Step 6: Makefile**: in the `verify` recipe line `PR_ORACLE_REQUIRED=1 $(PYTHON) -m unittest tools.tests.test_k11_fields … tools.tests.test_gp_compare`, append ` tools.tests.test_gp_moves`.

- [ ] **Step 7: Commit**: `git add tools/gp_moves.py tools/tests/test_gp_moves.py Makefile`; `tools: gp_moves decodes the keyboard move table into pad presses (record gameplay-u6 §U6.10)`.

---

### Task 4: The `gp-u6-moves` scenario and the replay's pinned set for it

Record §U6.12.

**Files:** Modify `tools/gp_session.py` (after the line `SCENARIOS['gp-idle-loss-run2'] = dict(SCENARIOS['gp-idle-loss'])   # the determinism run (spec §7 Q1)`); `tools/tests/test_gp_session.py` (a class `TestU6Moves` before `if __name__`); `tools/tests/test_gp_moves.py` (one method at the end of `TestRealImage`); `port/tests/test_platform.c` (`k_miss_gp_u6_moves`, `fnm_known`, `test_fn_misslog_driver`).

**Interfaces:** Produces `SCENARIOS['gp-u6-moves']`, `U6_MOVES_STEPS`; `static const fnm_pair k_miss_gp_u6_moves[]`; `fnm_known(addr, ctx, frontend, idle_loss, moves)`.

- [ ] **Step 1: Write the failing tests.** Before `if __name__ == '__main__':` in `test_gp_session.py`:

```python
class TestU6Moves(unittest.TestCase):
    def test_the_moves_fire_from_round_1(self):
        # plan gameplay-u6b: the idle-loss menu path, then 34 press groups from 10 frames
        # after mode 6 begins, the end 1200 frames after the first (record §U6.12)
        s = gs.Schedule(gs.SCENARIOS['gp-u6-moves']['steps'])
        self.assertEqual(s.due_boot(gs.ENTER_WAIT), [(0, ('key', 'enter'))])
        s.on_mode(0x141, 0x27)
        self.assertEqual(s.due(0x141 + 149), [(1, ('key', 'enter'))])
        self.assertEqual(s.due(0x141 + 299), [(2, ('key', 'enter'))])
        for f, m in ((0x26E, 0x2D), (0x293, 0x10), (0x77A, 5)):
            s.on_mode(f, m)
            self.assertEqual(s.due(f), [])
        s.on_mode(0x7F5, 6)
        self.assertEqual(s.due(0x7F5 + 8), [])
        self.assertEqual(s.due(0x7FF - 1), [(3, ('pad', ('p1.b0', 'p1.b1'), 4))])
        fired = 4
        for f in range(0x7FF, 0xCAF + 1):
            fired += len(s.due(f))
        self.assertEqual((fired, s.total, s.end_frame), (37, 37, 0x7FF + 1200))

```

At the end of `TestRealImage` in `test_gp_moves.py`:

```python
    def test_the_scenario_steps_are_the_tool_s(self):
        t = gm.decode(self.img, 0)
        att = gm.plan(t, [0x1A, 0, 1, 0x1B, 6, 0x15] * 2, 0, 100, 4)
        first, steps = gm.scenario_steps(att)
        sc = gs.SCENARIOS['gp-u6-moves']['steps']
        self.assertEqual(first, 0)
        self.assertEqual(sc[3], ('after_mode', 0x06, 10, steps[0][2]))
        self.assertEqual(list(sc[4:-1]), steps[1:])
        self.assertEqual(sc[-1], ('after', 100 - (att[-1][2][-1][0] - att[-1][0]), ('end',)))
```

- [ ] **Step 2: Run to verify they fail**: `KeyError: 'gp-u6-moves'` in both.

- [ ] **Step 3: Implement the scenario.** After `SCENARIOS['gp-idle-loss-run2'] = …`:

```python

# Plan gameplay-u6b (record gameplay-u6 §U6.12): P1 (Sauron, character 0, the
# pick time-out's) performs twelve attempts against the CPU in round 1, 100
# frames apart from 10 frames after mode 6 begins (harness values): the
# keyboard-table entries 0x1A, 0x00, 0x01, 0x1B, 0x06, 0x15 (reactions 0x10,
# 0x20, 0x24, 0x11, 0x2D, 0x3D) twice, each through tools/gp_moves.py presses
# (facing 0, a 4-frame step). `gp_moves.py steps --char 0 --facing 0 --moves
# 1A,00,01,1B,06,15,1A,00,01,1B,06,15 --gap 100 --step 4` prints these steps
# (tools/tests/test_gp_moves.py regenerates them from the image).
U6_MOVES_STEPS = (
    ('after', 100, ('pad', ('p1.left',), 4)),
    ('after', 0, ('pad', ('p1.b0', 'p1.b2'), 12)),
    ('after', 4, ('pad', ('p1.right',), 2)),
    ('after', 4, ('pad', ('p1.right',), 4)),
    ('after', 92, ('pad', ('p1.down',), 4)),
    ('after', 0, ('pad', ('p1.b0', 'p1.b2'), 12)),
    ('after', 4, ('pad', ('p1.up',), 4)),
    ('after', 4, ('pad', ('p1.left',), 4)),
    ('after', 92, ('pad', ('p1.b2', 'p1.b3'), 4)),
    ('after', 100, ('pad', ('p1.down',), 2)),
    ('after', 0, ('pad', ('p1.b0', 'p1.b2'), 12)),
    ('after', 4, ('pad', ('p1.down',), 4)),
    ('after', 4, ('pad', ('p1.up',), 4)),
    ('after', 92, ('pad', ('p1.left',), 4)),
    ('after', 4, ('pad', ('p1.down',), 4)),
    ('after', 4, ('pad', ('p1.up',), 4)),
    ('after', 92, ('pad', ('p1.b0', 'p1.b1'), 4)),
    ('after', 100, ('pad', ('p1.left',), 4)),
    ('after', 0, ('pad', ('p1.b0', 'p1.b2'), 12)),
    ('after', 4, ('pad', ('p1.right',), 2)),
    ('after', 4, ('pad', ('p1.right',), 4)),
    ('after', 92, ('pad', ('p1.down',), 4)),
    ('after', 0, ('pad', ('p1.b0', 'p1.b2'), 12)),
    ('after', 4, ('pad', ('p1.up',), 4)),
    ('after', 4, ('pad', ('p1.left',), 4)),
    ('after', 92, ('pad', ('p1.b2', 'p1.b3'), 4)),
    ('after', 100, ('pad', ('p1.down',), 2)),
    ('after', 0, ('pad', ('p1.b0', 'p1.b2'), 12)),
    ('after', 4, ('pad', ('p1.down',), 4)),
    ('after', 4, ('pad', ('p1.up',), 4)),
    ('after', 92, ('pad', ('p1.left',), 4)),
    ('after', 4, ('pad', ('p1.down',), 4)),
    ('after', 4, ('pad', ('p1.up',), 4)),
)
SCENARIOS['gp-u6-moves'] = dict(time_limit=130, steps=(
    ('boot', ENTER_WAIT, ('key', 'enter')),       # mode 3 -> 0x27
    ('after_mode', 0x27, 150, ('key', 'enter')),  # START MENU
    ('after', 150, ('key', 'enter')),             # LEFT PLAYER ARCADE: mode 0x2D
    ('after_mode', 0x06, 10, ('pad', ('p1.b0', 'p1.b1'), 4)),   # attempt 1 (offset 0)
) + U6_MOVES_STEPS + (('after', 92, ('end',)),))  # 100 frames after the last attempt began
```

The steps are `gp_moves.py steps --image $S/image.bin --char 0 --facing 0 --moves 1A,00,01,1B,06,15,1A,00,01,1B,06,15 --gap 100 --step 4` (the first printed step becomes the `after_mode` anchor; `test_the_scenario_steps_are_the_tool_s` keeps them equal).

- [ ] **Step 4: The pinned set for the scenario** (the moves scenario passes the same wipes as `gp-idle-loss`; without these rows the dry run of Task 5 fails with `unexpected 0x29D60 from frontend_mode_1b_step`, measured). In `port/tests/test_platform.c`:

1. Replace

```c
/* The scenario named by the first line of PR_GP_SCRIPT
```

   with

```c
/* gp-u6-moves (plan gameplay-u6b, record gameplay-u6 §U6.12) passes the same
 * MAIN MENU, START MENU and character-select wipes as gp-idle-loss, so it
 * records the same two harmless pairs (record §G.24): the bare `ret` 0x29D60
 * and the runtime stub 0x5D812, both from frontend_mode_1b_step. */
static const fnm_pair k_miss_gp_u6_moves[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};

/* The scenario named by the first line of PR_GP_SCRIPT
```

2. Replace

```c
static int fnm_known(u32 addr, const char *ctx, int frontend, int idle_loss)
{
    if (fnm_in(k_miss_known, FNM_N(k_miss_known), addr, ctx)) return 1;
    if (frontend && fnm_in(k_miss_frontend, FNM_N(k_miss_frontend), addr, ctx)) return 1;
    return idle_loss && fnm_in(k_miss_gp_idle_loss, FNM_N(k_miss_gp_idle_loss), addr, ctx);
}
```

   with

```c
static int fnm_known(u32 addr, const char *ctx, int frontend, int idle_loss, int moves)
{
    if (fnm_in(k_miss_known, FNM_N(k_miss_known), addr, ctx)) return 1;
    if (frontend && fnm_in(k_miss_frontend, FNM_N(k_miss_frontend), addr, ctx)) return 1;
    if (moves && fnm_in(k_miss_gp_u6_moves, FNM_N(k_miss_gp_u6_moves), addr, ctx)) return 1;
    return idle_loss && fnm_in(k_miss_gp_idle_loss, FNM_N(k_miss_gp_idle_loss), addr, ctx);
}
```

3. Replace

```c
    int idle_loss = 0, cut = 0;
    if (strcmp(env, "PR_GP_DUMP") == 0) {
        char sc[64];
        fnm_gp_scenario(sc, sizeof sc, &cut);
        idle_loss = strncmp(sc, "gp-idle-loss", 12) == 0;
    }
    u32 want = (u32)FNM_N(k_miss_known) +
               (frontend ? (u32)FNM_N(k_miss_frontend) : 0u) +
               (idle_loss ? (u32)FNM_N(k_miss_gp_idle_loss) : 0u);
```

   with

```c
    int idle_loss = 0, moves = 0, cut = 0;
    if (strcmp(env, "PR_GP_DUMP") == 0) {
        char sc[64];
        fnm_gp_scenario(sc, sizeof sc, &cut);
        idle_loss = strncmp(sc, "gp-idle-loss", 12) == 0;
        moves = strncmp(sc, "gp-u6-moves", 11) == 0;
    }
    u32 want = (u32)FNM_N(k_miss_known) +
               (frontend ? (u32)FNM_N(k_miss_frontend) : 0u) +
               (idle_loss ? (u32)FNM_N(k_miss_gp_idle_loss) : 0u) +
               (moves ? (u32)FNM_N(k_miss_gp_u6_moves) : 0u);
```

4. Replace

```c
        if (!fnm_known(fn_misslog_addr(i), fn_misslog_ctx(i), frontend, idle_loss)) {
```

   with

```c
        if (!fnm_known(fn_misslog_addr(i), fn_misslog_ctx(i), frontend, idle_loss, moves)) {
```

5. Replace

```c
            if (!fnm_known((u32)addr, ctx, 0, 0)) {
```

   with

```c
            if (!fnm_known((u32)addr, ctx, 0, 0, 0)) {
```


- [ ] **Step 5: Run**: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_session tools.tests.test_gp_moves 2>&1 | tail -1` ⇒ `OK`; `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests | tail -1` ⇒ `all checks passed`.

- [ ] **Step 6: Mutation proofs**: `('after_mode', 0x06, 10, …)` → `11` ⇒ `FAIL: test_the_moves_fire_from_round_1`; delete the line `    ('after', 4, ('pad', ('p1.up',), 4)),` that ends `U6_MOVES_STEPS` ⇒ `FAIL: test_the_scenario_steps_are_the_tool_s` and `test_the_moves_fire_from_round_1`.

- [ ] **Step 7: Commit**: `tools/gp_session.py tools/tests/test_gp_session.py tools/tests/test_gp_moves.py port/tests/test_platform.c`; `tools: the gp-u6-moves scenario and its replay pinned set (record gameplay-u6 §U6.12)`.

---

### Task 5: The port dry run (measurement)

Record §U6.12. Proves in the port, before the capture is spent, that the presses reach the intended reactions, and predicts the misses.

**Files:** none committed (outputs in `$S`, recorded in §U6.22).

- [ ] **Step 1: Write and replay the dry script**

```bash
M=1A,00,01,1B,06,15,1A,00,01,1B,06,15
python3 tools/gp_moves.py dry --image $S/image.bin --char 0 --facing 0 --moves $M --gap 100 --step 4 \
    --capture data/k11-captures/gp-idle-loss --start 0x7FF --end 0xD40 --out $S/dry.script
rm -rf $S/dry && PR_GP_DUMP=$S/dry PR_GP_SCRIPT=$S/dry.script PR_GAME_DIR=data/game/C ./build/run_tests > $S/dry.log 2>&1
grep fn-miss $S/dry.log
mkdir -p $S/drycap      # the port's T lines read as S records, with the first press's I record (f = 0x7FE)
(echo 'I ms=0 f=07FE step=3 press=p1.b0 scan=16 lin=0 old=96 bios=1675 ring=1 late=0'; sed 's/^T /S ms=0 /' $S/dry/trace.txt) > $S/drycap/poll.log
python3 tools/gp_moves.py check --image $S/image.bin --char 0 --facing 0 --moves $M --gap 100 --step 4 --capture $S/drycap
grep 'f=07F5 ' $S/dry/trace.txt | grep -o 'c0=.. c1=..'
```

Expected (record §U6.12, measured on this tree: U6a plus Tasks 1–4): the misses `0x3F0A8 hit_reaction_apply hits=1`, `0x3D1EC hit_reaction_apply hits=2`, `0x3C048 hit_reaction_apply hits=2` besides the four harmless pairs; `c0=00 c1=01`; `7 of 12 attempts performed` (`i=1A` at `f=800`, `i=01` at `8D0`, `i=1B` at `92D`, `i=06` at `998`, `i=15` at `9FC`, `i=06` at `BF0`, `i=15` at `C54`). `i=00` (reaction `0x20`) is not shown in either window on this tree (P1 is in hit-stun); with the U6b ports it is performed at `f=AC4` (record §U6.12), so its presses are proven. The driver reports `FAIL` for the three unregistered pairs: expected here, they are what Tasks 8–10 port.

- [ ] **Step 2: Record** the outputs in `$S/t5.txt` for §U6.22. If the dry run shows fewer than the five distinct moves above (`0x10 0x24 0x11 0x2D 0x3D`), or other misses, stop: the decode, the facing or the debounce reading differs from the record (raw wins: re-derive before capturing).

---

### Task 6: The capture (behind Decision 1)

**Files:** none committed (`data/k11-captures/gp-u6-moves`, git-ignored, written only by `make gp-capture`).

- [ ] **Step 1: Capture**: `make gp-capture scenario=gp-u6-moves TITLE_PIN_DIR=/tmp/pr_u6b_pin > $S/t6_capture.txt 2>&1; echo "rc=$?"; cat data/k11-captures/gp-u6-moves/session.txt; du -sh data/k11-captures/gp-u6-moves`
Expected: `rc=0`; `check=ok` on `base`, `steps fired 37/37`, `end frame reached`, `mode 0x27 after the Enter`, `snapshots kb == raw`, `frames written N/N`, `port script v2`; about 105 MB. Any `check` line not `ok`: stop and record it (a re-capture goes to a new scenario name, never over this one).

- [ ] **Step 2: The path and the character**:

```bash
python3 - <<'EOF'
import sys; sys.path.insert(0, 'tools'); import gp_session as gs
prev = None
for l in open('data/k11-captures/gp-u6-moves/poll.log'):
    r = gs.parse(l)
    if r and r['kind'] == 'S' and r['mode'] != prev:
        print('%X mode=%X c0=%X c1=%X s0_5a=%X s1_5a=%X' % (r['f'], r['mode'], r['c0'], r['c1'], r['s0_5a'], r['s1_5a'])); prev = r['mode']
EOF
grep -c 'late=1' data/k11-captures/gp-u6-moves/poll.log
```

Expected: the `gp-idle-loss` path to mode 6 (`0x27`, `0x2D`, `0x1A/0x1B`, `0x10`, the pick time-out at `f & 0x3F == 0`, `0x1A/0x1B/0x11/0x17`, `0x1A/0x1B`, 5, 6) and `c0=0` at mode 6. If `c0` is not 0 the move list is not P1's: stop, record, ask (the decode is per character). Late presses are reported, not fatal (each shifts that press by its lateness, visible in the check).

- [ ] **Step 3: Which attempts the original performed**: `python3 tools/gp_moves.py check --image $S/image.bin --char 0 --facing 0 --moves 1A,00,01,1B,06,15,1A,00,01,1B,06,15 --gap 100 --step 4 --capture data/k11-captures/gp-u6-moves | tee $S/t6_check.txt`
Record each attempt's verdict. A distinct move with no `performed` attempt is a named gap (its evidence: the `r0`/`s0_52`/`e0` lines of its two windows); the capture stands.

- [ ] **Step 4: The identity**: `shasum -a 256 data/k11-captures/gp-u6-moves/poll.log; ls data/k11-captures/gp-u6-moves/frame_*.raw.gz | wc -l` — the values Task 15 pins.

---

### Task 7: The replay (measurement)

**Files:** none committed.

- [ ] **Step 1**:

```bash
python3 tools/gp_session.py port-script --scenario gp-u6-moves --capture data/k11-captures/gp-u6-moves --out $S/moves.script
rm -rf /tmp/pr_u6b_gp/gp-u6-moves && PR_GP_DUMP=/tmp/pr_u6b_gp/gp-u6-moves PR_GP_SCRIPT=$S/moves.script PR_GAME_DIR=data/game/C ./build/run_tests > $S/t7_replay.txt 2>&1
grep fn-miss $S/t7_replay.txt
python3 tools/gp_compare.py --report --scenario gp-u6-moves --capture data/k11-captures/gp-u6-moves --port /tmp/pr_u6b_gp/gp-u6-moves | tee $S/t7_report.txt
```

Expected: no stall, no fault; the miss lines (besides the four harmless pairs) name which of Tasks 8–13 run. The record predicts `0x3F0A8`, `0x3D1EC`, `0x3C048` (P1's moves) and possibly `0x231C0` (the CPU's character-1 reaction `0x27`), then the cascades of Tasks 11–12. The report's three claims (`frames:`, `trace:`, `moves:`) are recorded, not pinned.

---

### Tasks 8–13: The callbacks the replay reaches

Each task runs **only if** its address appears in the replay's miss log (Task 7, or the re-run at the end of an earlier port task); otherwise it is skipped and the function stays for track P with "not reached by gp-u6-moves" as its evidence. Common steps for every one of them:

- **Test frame (once).** If `static int u6b_run(` is not yet in `port/tests/test_fight.c`, append first:

```c

/* ---- gameplay-u6 §U6.13-§U6.16: the moves capture's callbacks ------------
 * The move-table callbacks the gp-u6-moves scenario reaches (record
 * gameplay-u6 §U6.12): the T-rex's 0x24/0x25 entry 0x3F0A8 and the three
 * callbacks it stores, its 0x2D entry 0x3D1EC, every character's 0x3D entry
 * 0x3C048 and character 1's 0x27 entry 0x231C0. Each check runs inside one
 * mz_save/mz_restore and seeds sentinels that differ from every
 * post-condition. */

/* The U6b tests' shared frame (record gameplay-u6 §U6.13-§U6.18): each runs its
 * checks between mz_save and mz_restore. */
static int u6b_run(void (*checks)(void))
{
    int before = g_failures;
    u8 sv_slots[0x160], sv_cf0[0x30], sv_ab0[0x50];
    if (!mz_save()) { CHECK(0, "the gameplay-u6b snapshot allocates"); return 1; }
    tf_snap(sv_slots, 0x001077A0u, 0x160u);
    tf_snap(sv_cf0, 0x00107CF0u, 0x30u);
    tf_snap(sv_ab0, 0x00100AB0u, 0x50u);
    checks();
    tf_put(sv_slots, 0x001077A0u, 0x160u);
    tf_put(sv_cf0, 0x00107CF0u, 0x30u);
    tf_put(sv_ab0, 0x00100AB0u, 0x50u);
    mz_restore();
    return g_failures - before;
}

```

- **Registration of the test**: in `port/tests/test.h` add `    X(test_u6b_<name>) \` after the last `X(test_u6…` line.
- **Replay after the port**: Task 7's `PR_GP_DUMP` commands again (`$S/t<N>_replay.txt`); the task's address must be gone from the miss log; a newly listed address goes to its task (11, 12, 13) or to Task 14.
- **Commit**: the task's files; `game: port <0xADDR> (<what>) reached by gp-u6-moves (record gameplay-u6 §U6.<n>)`.

### Task 8: `0x3C048` (every character's reaction `0x3D`, the down-up jump)

Record §U6.15. **Files:** `port/tests/test_fight.c`, `port/tests/test.h`, `port/src/game/fighter.c` (before `/* 0x3E3A8. The T-rex's reaction-0x2A callback`), `fighter.h` (after `u32 fighter_3e484(u32 side);`), `actors.c`.

- [ ] **Step 1: Failing test** (append):

```c

/* §U6.15: 0x3C048 is 0x3BF70 with the slot's +0x4E = 0 on success (both flip
 * states), nothing on a rejection; seven table dwords. */
static void check_u6b_3c048(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 st = FIGHT_RECS + 0x3A00u, c;
    for (c = 0; c < 7u; c++)
        CHECK_EQ_INT((int)DSD(0x000A3528u + (c * 64u + 0x3Du) * 20u), 0x0003C048);
    CHECK(fn_resolve(0x3C048u) != NULL, "0x3C048 is registered");
    ra_seed(s0, s1, r0, r1, st);
    CHECK_EQ_INT(fighter_3c048(s0, r0, 0u), 1);
    CHECK_EQ_INT((int)DSW(s0 + 0x4Eu), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 3);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0xFF);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x43u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x42u), 0);
    CHECK_EQ_INT((int)DSW(s1 + 0x4Eu), 0x1235);
    ra_seed(s0, s1, r0, r1, st);
    DSB(r0 + 0x29u) = 0x40u;
    CHECK_EQ_INT(fighter_3c048(s0, r0, 0u), 1);
    CHECK_EQ_INT((int)DSW(s0 + 0x4Eu), 0);
    ra_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x40u) = 0x85u;
    CHECK_EQ_INT(fighter_3c048(s0, r0, 0u), 0);
    CHECK_EQ_INT((int)DSW(s0 + 0x4Eu), 0x1234);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0xAAAA);
    ra_seed(s0, s1, r0, r1, st);
    CHECK_EQ_INT(fighter_3c048(s1, r1, 1u), 1);
    CHECK_EQ_INT((int)DSW(s1 + 0x4Eu), 0);
    CHECK_EQ_INT((int)DSW(s0 + 0x4Eu), 0x1234);
}

int test_u6b_3c048(void)        { return u6b_run(check_u6b_3c048); }
```

- [ ] **Step 2**: `cmake --build build` ⇒ an error naming `fighter_3c048`.
- [ ] **Step 3: Implement.** `fighter.c`:

```c
/* 0x3C048 — record gameplay-u6 §U6.15. Every character's reaction-0x3D
 * callback (the seven dwords 0xA3528 + (c*64 + 0x3D)*20). EAX = slot, EDX =
 * rec, EBX = side: 0x3BF70, and on success the side's slot +0x4E = 0 and its
 * record's +0x34 word, +0x43 and +0x42 bytes = 0. Returns 0x3BF70's AL (0x34E2C
 * does not read it). */
int fighter_3c048(u32 slot, u32 rec, u32 side)
{
    u32 ctx[6];
    int r;
    fighter_ctx_same(ctx, side);                        /* 0x3C051..0x3C055 0x33950 */
    r = fighter_3bf70(slot, rec, side);                 /* 0x3C05A..0x3C05E */
    if (r != 0) {                                       /* 0x3C065 */
        u32 own = DSD(DS_001077B0 + ctx[0] * 0x94u);    /* 0x3C073..0x3C084 */
        DSW(ctx[2] + 0x4Eu) = 0;                        /* 0x3C069/0x3C06D */
        DSW(own + 0x34u) = 0;                           /* 0x3C08B */
        DSB(own + 0x43u) = 0;                           /* 0x3C091 */
        DSB(own + 0x42u) = 0;                           /* 0x3C095 */
    }
    return r;                                           /* 0x3C099 */
}

```

`fighter.h` after `u32 fighter_3e484(u32 side);`:

```c
/* 0x3C048 (record gameplay-u6 §U6.15): every character's reaction-0x3D
 * callback; returns 0x3BF70's AL. */
int fighter_3c048(u32 slot, u32 rec, u32 side);
```

`actors.c`: after `static void reaction_cb_3C0A4(u32 slot, u32 rec, u32 side);` add `static void reaction_cb_3C048(u32 slot, u32 rec, u32 side);`; after `    fn_register(0x3C0A4u, (void (*)(void))reaction_cb_3C0A4);` add

```c
    /* PORT: record gameplay-u6 §U6.15. Every character's reaction-0x3D
     * callback 0x3C048 (0x34E2C, (slot, rec, side), AL unread). */
    fn_register(0x3C048u, (void (*)(void))reaction_cb_3C048);
```

after the definition of `reaction_cb_3C0A4` add

```c

/* 0x3C048 — the reaction-callback shape. PORT: the same 0x35045 call, whose
 * AL is ignored; this wrapper drops fighter_3c048's result. */
static void reaction_cb_3C048(u32 slot, u32 rec, u32 side)
{
    (void)fighter_3c048(slot, rec, side);
}
```

- [ ] **Step 4**: `run_tests` ⇒ `all checks passed`.
- [ ] **Step 5: Mutations**: `DSW(ctx[2] + 0x4Eu) = 0;` → `(void)0;` ⇒ `65535 != 0`; the `fn_register(0x3C048u, …)` line → `(void)reaction_cb_3C048;` ⇒ `0x3C048 is registered`. **Named equivalent mutant**: deleting the three `own` writes survives (record §U6.15); record it.
- [ ] **Step 6**: replay; `0x3C048` gone. Commit.

### Task 9: `0x3D1EC` (the T-rex's reaction `0x2D`)

Record §U6.14. Files as Task 8.

- [ ] **Step 1: Failing test** (append):

```c

/* §U6.14: 0x3D1EC: the record on the stream the dword 0xC8CC0 holds, at 5.0,
 * then state 9/8/1. The dword is pointed at a crafted head. */
static void check_u6b_3d1ec(void)
{
    u32 st = FIGHT_RECS + 0x3A00u;
    CHECK_EQ_INT((int)DSD(0x000A38ACu), 0x0003D1EC);
    CHECK(fn_resolve(0x3D1ECu) == (void (*)(void))fighter_3d1ec, "0x3D1EC is registered");
    sc_seed(0u, 1u, 0);
    DSW(st) = 0x1564u;
    DSD(0x000C8CC0u) = st;
    DSB(Z_S0 + 0x52u) = 0x0Cu;
    DSB(Z_S0 + 0x53u) = 0x33u;
    DSB(Z_S0 + 0x54u) = 0x44u;
    fighter_3d1ec(Z_S0, Z_R0, 0u);
    sc_stream(Z_R0, st, 0x40A00000u, 0x1564u);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 8);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 1);
}

int test_u6b_3d1ec(void)        { return u6b_run(check_u6b_3d1ec); }
```

- [ ] **Step 2**: build error naming `fighter_3d1ec`.
- [ ] **Step 3: Implement.** `fighter.c` before `/* 0x3E3A8. The T-rex's reaction-0x2A callback`:

```c
/* PORT: a data-object address symbols.h does not name. */
#define FIGHT_C8CC0      0x000C8CC0u  /* 0x3D1F1: the reaction-0x2D stream dword */

/* 0x3D1EC — record gameplay-u6 §U6.14. The T-rex's reaction-0x2D callback
 * (*(u32*)0xA38AC, the (char 0, 0x2D) entry, its only reference). EAX = slot,
 * EDX = rec: the stream the dword 0xC8CC0 holds at hold 5.0 through 0x3C4CC,
 * then state 9/8/1; AL = 1 (unread). */
void fighter_3d1ec(u32 slot, u32 rec, u32 side)
{
    (void)side;
    hit_anim_start_b(rec, DSD(FIGHT_C8CC0), 0x40A00000u);   /* 0x3D1EF..0x3D1FC 0x3C4CC */
    DSB(slot + 0x52u) = 9u;                             /* 0x3D201 */
    DSB(slot + 0x53u) = 8u;                             /* 0x3D205 */
    DSB(slot + 0x54u) = 1u;                             /* 0x3D20B */
}

```

`fighter.h` after `u32 fighter_3e484(u32 side);`:

```c
/* 0x3D1EC (record gameplay-u6 §U6.14): the T-rex's reaction-0x2D callback. */
void fighter_3d1ec(u32 slot, u32 rec, u32 side);
```

`actors.c` after `    fn_register(0x3C0A4u, (void (*)(void))reaction_cb_3C0A4);`:

```c
    /* PORT: record gameplay-u6 §U6.14. The T-rex's reaction-0x2D callback
     * 0x3D1EC (the dword at 0xA38AC; 0x34E2C, (slot, rec, side)). */
    fn_register(0x3D1ECu, (void (*)(void))fighter_3d1ec);
```

- [ ] **Step 4**: `all checks passed`.
- [ ] **Step 5: Mutations**: hold `0x40A00000u` → `0x40400000u` ⇒ `1077936128 != 1084227584`; `DSB(slot + 0x54u) = 1u;` → `0u` ⇒ `0 != 1`.
- [ ] **Step 6**: replay; `0x3D1EC` gone. Commit.

### Task 10: `0x3F0A8` and the three callbacks it stores

Record §U6.13. Files as Task 8. The three callbacks (`0x3F054` +0x0C, `0x3EFE0` +0x18, `0x3F020` +0x1C) are stored by `0x3F0A8` at `0x3F0D2/0x3F0DD/0x3F0E6` and reached as soon as its move runs, so they are ported with it.

- [ ] **Step 1: Failing test** (append):

```c

/* §U6.13: 0x3F0A8 on side 0 (the stream head patched to the plain id 0x1562):
 * the record on 0xE7B78 at 3.0, the voice 0x8C, state 9/7/0, +0x57 = 0 and
 * the three callbacks; the other slot untouched. */
static void check_u6b_3f0a8(void)
{
    CHECK_EQ_INT((int)DSD(0x000A37F8u), 0x0003F0A8);
    CHECK_EQ_INT((int)DSD(0x000A380Cu), 0x0003F0A8);
    CHECK(fn_resolve(0x3F0A8u) == (void (*)(void))fighter_3f0a8, "0x3F0A8 is registered");
    CHECK(fn_resolve(0x3F054u) == (void (*)(void))fighter_3f054, "0x3F054 is registered");
    CHECK(fn_resolve(0x3EFE0u) == (void (*)(void))fighter_3efe0, "0x3EFE0 is registered");
    CHECK(fn_resolve(0x3F020u) == (void (*)(void))fighter_3f020, "0x3F020 is registered");
    sc_seed(0u, 1u, 0);
    DSW(0x000E7B78u) = 0x1562u;
    DSB(Z_S0 + 0x52u) = 0x0Cu;
    DSB(Z_S0 + 0x53u) = 0x33u;
    DSB(Z_S0 + 0x54u) = 0x44u;
    DSD(Z_S0 + 0x0Cu) = 0x0C0C0C0Cu;
    DSB(Z_S0 + 0x57u) = 0x57u;
    DSD(Z_S0 + 0x18u) = 0x18181818u;
    DSD(Z_S0 + 0x1Cu) = 0x1C1C1C1Cu;
    DSB(Z_S1 + 0x52u) = 0x0Du;
    sound_voice_log_reset();
    fighter_3f0a8(Z_S0, Z_R0, 0u);
    sc_stream(Z_R0, 0x000E7B78u, 0x40400000u, 0x1562u);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x0Cu), 0x0003F054);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 0);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x18u), 0x0003EFE0);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x1Cu), 0x0003F020);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x52u), 0x0D);
    CHECK_EQ_INT((int)sound_voice_log_count(), 1);
    CHECK_EQ_INT((int)sound_voice_log_at(0), 0x8C);
    sound_voice_log_reset();
}

/* §U6.13: 0x3F054 counts the word 0x1080A4[side] while +0x57 == 1 and, past
 * 10, starts the side's record on 0xE7BBE at 2.0 and sets +0x57 = 2. */
static void check_u6b_3f054(void)
{
    sc_seed(0u, 1u, 0);
    DSW(0x000E7BBEu) = 0x1563u;
    DSD(Z_R0 + 0x08u) = 0x00ABCDEFu;
    DSB(Z_S0 + 0x57u) = 1u;
    DSW(0x001080A4u) = 9u;
    DSW(0x001080A6u) = 0x7777u;
    fighter_3f054(0x11111111u, 0x22222222u, 0u);    /* EAX/EDX are not read */
    CHECK_EQ_INT((int)DSW(0x001080A4u), 10);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 1);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x08u), 0x00ABCDEF);  /* 10 is not past 10 */
    fighter_3f054(0x11111111u, 0x22222222u, 0u);
    CHECK_EQ_INT((int)DSW(0x001080A4u), 11);
    sc_stream(Z_R0, 0x000E7BBEu, 0x40000000u, 0x1563u);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSW(0x001080A6u), 0x7777);      /* side 1's word */
    fighter_3f054(0x11111111u, 0x22222222u, 0u);      /* +0x57 = 2: nothing */
    CHECK_EQ_INT((int)DSW(0x001080A4u), 11);
    DSB(Z_S0 + 0x57u) = 0u;                           /* +0x57 = 0: nothing */
    fighter_3f054(0x11111111u, 0x22222222u, 0u);
    CHECK_EQ_INT((int)DSW(0x001080A4u), 11);
    /* A negative word is signed (0x3F07F `sar eax,0x10`): 0xFFF0 + 1 is -15. */
    DSB(Z_S1 + 0x57u) = 1u;
    DSW(0x001080A6u) = 0xFFF0u;
    DSD(Z_R1 + 0x08u) = 0x00FEDCBAu;
    fighter_3f054(0u, 0u, 1u);
    CHECK_EQ_INT((int)DSW(0x001080A6u), 0xFFF1);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x57u), 1);
    CHECK_EQ_INT((int)DSD(Z_R1 + 0x08u), 0x00FEDCBA);
}

/* §U6.13: 0x3F020 clears the word 0x1080A4[side] and sets the side's slot
 * +0x57 = 1 after 0x3B714; the other side's word stays. */
static void check_u6b_3f020(void)
{
    sc_seed(0u, 1u, 0);
    DSW(0x001080A4u) = 0x5151u;
    DSW(0x001080A6u) = 0x6161u;
    DSB(Z_S1 + 0x57u) = 0x57u;
    DSB(Z_S0 + 0x57u) = 0x75u;
    fighter_3f020(1u);
    CHECK_EQ_INT((int)DSW(0x001080A6u), 0);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x57u), 1);
    CHECK_EQ_INT((int)DSW(0x001080A4u), 0x5151);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 0x75);
}

/* §U6.13: 0x3EFE0 is 0x3E484's body: the same seeds give the same results
 * (check_guarded_walk's block N, both sides). */
static void check_u6b_3efe0(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    sh_seed(s0, s1, r0, r1);
    DSB(s1 + 0x54u) = 2u;
    DSB(s1 + 0x62u) = 1u;
    DSB(s1 + 0x53u) = 0x0Au;
    DSD(DS_00100AF8) = 0;
    CHECK_EQ_INT((int)fighter_3efe0(0u), 1);
    DSD(DS_00100AF8) = 6u;
    CHECK_EQ_INT((int)fighter_3efe0(0u), 0);
    DSW(s1 + 0x76u) = 2u;
    CHECK_EQ_INT((int)fighter_3efe0(0u), 1);
    DSW(s1 + 0x76u) = 1u;
    CHECK_EQ_INT((int)fighter_3efe0(0u), 0);
    DSW(s1 + 0x74u) = 1u;
    CHECK_EQ_INT((int)fighter_3efe0(0u), 1);
    DSW(s1 + 0x74u) = 0;
    DSB(s1 + 0x42u) = 8u;
    CHECK_EQ_INT((int)fighter_3efe0(0u), 1);
    DSB(s1 + 0x42u) = 0;
    DSD(DS_00100AFC) = 0;
    CHECK_EQ_INT((int)fighter_3efe0(1u), 1);
    DSD(DS_00100AFC) = 9u;
    DSW(s0 + 0x74u) = 1u;
    CHECK_EQ_INT((int)fighter_3efe0(1u), 1);
    DSW(s0 + 0x74u) = 0;
    CHECK_EQ_INT((int)fighter_3efe0(1u), 0);
}

static void u6b_3f0a8_family(void)
{
    check_u6b_3f0a8();
    check_u6b_3f054();
    check_u6b_3f020();
    check_u6b_3efe0();
}

int test_u6b_3f0a8(void)        { return u6b_run(u6b_3f0a8_family); }
```

- [ ] **Step 2**: build error naming `fighter_3f0a8`.
- [ ] **Step 3: Implement.** `fighter.c` before `/* 0x3E3A8. The T-rex's reaction-0x2A callback`:

```c
/* PORT: data-object addresses symbols.h does not name. */
#define FIGHT_ANIM_3F0A8 0x000E7B78u  /* 0x3F0AD: the reaction-0x24 stream */
#define FIGHT_ANIM_3F054 0x000E7BBEu  /* 0x3F087: the +0x57 == 1 stream */
#define FIGHT_1080A4     0x001080A4u  /* 0x3F03F/0x3F070: a word per side */

/* 0x3F0A8 — record gameplay-u6 §U6.13. The T-rex's reaction-0x24 and 0x25
 * callback (*(u32*)0xA37F8 and 0xA380C, the (char 0, 0x24/0x25) entries of
 * 0x34E2C's 0xA3528 table, its only references). EAX = slot, EDX = rec; EBX is saved and reused as the slot.
 * The 0xE7B78 stream at hold 3.0 through 0x3C4CC, the voice 0x8C, state 9/7/0,
 * +0x57 = 0 and the callbacks +0x0C 0x3F054 (0x3531C case 7), +0x18 0x3EFE0
 * (0x19020's hook) and +0x1C 0x3F020 (0x193B0's 0x19505 call); AL = 1, which
 * 0x34E2C does not read (0x35049). */
void fighter_3f0a8(u32 slot, u32 rec, u32 side)
{
    (void)side;
    hit_anim_start_b(rec, FIGHT_ANIM_3F0A8, 0x40400000u);   /* 0x3F0AB..0x3F0B7 0x3C4CC */
    (void)sound_voice(0x8Cu);                           /* 0x3F0BC/0x3F0C1 0x2C3FC */
    DSB(slot + 0x52u) = 9u;                             /* 0x3F0C6 */
    DSB(slot + 0x53u) = 7u;                             /* 0x3F0CA */
    DSB(slot + 0x54u) = 0;                              /* 0x3F0CE */
    DSD(slot + 0x0Cu) = 0x0003F054u;                    /* 0x3F0D2 */
    DSB(slot + 0x57u) = 0;                              /* 0x3F0D9 */
    DSD(slot + 0x18u) = 0x0003EFE0u;                    /* 0x3F0DD */
    DSD(slot + 0x1Cu) = 0x0003F020u;                    /* 0x3F0E6 */
}

/* 0x3EFE0 — record gameplay-u6 §U6.13. The slot +0x18 hook 0x3F0A8 stores,
 * called by 0x19020 as fn(side) with EAX returned: 0x3E484's body (0x18C14
 * with flag 0 = 1, flags 1 and 8 = 0, EBX = ECX = 0). */
u32 fighter_3efe0(u32 side)
{
    u32 ctx[6];
    u8 flags[16];
    fighter_ctx_same(ctx, side);                            /* 0x3EFEC 0x33950 */
    fighter_18bd4(flags);                                   /* 0x3EFF7 0x18BD4 */
    flags[1] = 0;                                           /* 0x3F000 */
    flags[8] = 0;                                           /* 0x3F004 */
    flags[0] = 1u;                                          /* 0x3F00C */
    return (u32)fighter_18c14(ctx[0], flags, 0u, 0u);       /* 0x3F013 0x18C14 */
}

/* 0x3F020 — record gameplay-u6 §U6.13. The slot +0x1C callback 0x3F0A8
 * stores; 0x193B0 calls it at 0x19505 as fn(side). 0x3B714(ctx[3], ctx[2])
 * as 0x3E4C4, then the word 0x1080A4[side] = 0 and the side's slot +0x57 = 1. */
void fighter_3f020(u32 side)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);                        /* 0x3F028 0x33950 */
    fighter_reaction(ctx[3], ctx[2]);                   /* 0x3F02D..0x3F035 0x3B714 */
    DSW(FIGHT_1080A4 + ctx[0] * 2u) = 0;                /* 0x3F03A..0x3F03F */
    DSB(ctx[2] + 0x57u) = 1u;                           /* 0x3F047/0x3F04B */
}

/* 0x3F054 — record gameplay-u6 §U6.13. The per-frame +0x0C callback 0x3F0A8
 * stores (0x3531C case 7: EAX = slot, EDX = rec, EBX = side; only EBX is
 * read). With the side's slot +0x57 == 1 it counts the word 0x1080A4[side]
 * up and, past 10 (signed), starts the side's record on 0xE7BBE at hold 2.0
 * through 0x2BC30 and sets +0x57 = 2; any other +0x57 returns. */
void fighter_3f054(u32 slot, u32 rec, u32 side)
{
    u32 ctx[6];
    (void)slot;
    (void)rec;
    fighter_ctx_same(ctx, side);                        /* 0x3F05B 0x33950 */
    if (DSB(ctx[2] + 0x57u) != 1u) return;              /* 0x3F064..0x3F06B */
    DSW(FIGHT_1080A4 + ctx[0] * 2u) =
        (u16)(DSW(FIGHT_1080A4 + ctx[0] * 2u) + 1u);    /* 0x3F070 */
    if ((s32)(s16)DSW(FIGHT_1080A4 + ctx[0] * 2u) <= 10) return;   /* 0x3F078..0x3F085 */
    actors_anim_begin(ctx[4], FIGHT_ANIM_3F054, 0x40000000u);       /* 0x3F087..0x3F095 0x2BC30 */
    DSB(ctx[2] + 0x57u) = 2u;                           /* 0x3F09A/0x3F09E */
}

```

`fighter.h` after `u32 fighter_3e484(u32 side);`:

```c
/* 0x3F0A8 and its three callbacks (record gameplay-u6 §U6.13): the T-rex's
 * reaction-0x24 callback (slot, rec, side), the +0x18 hook 0x3EFE0 (fn(side),
 * EAX returned), the +0x1C callback 0x3F020 (fn(side)) and the per-frame
 * +0x0C callback 0x3F054 (slot, rec, side). */
void fighter_3f0a8(u32 slot, u32 rec, u32 side);
u32  fighter_3efe0(u32 side);
void fighter_3f020(u32 side);
void fighter_3f054(u32 slot, u32 rec, u32 side);
```

`actors.c` after `    fn_register(0x3C0A4u, (void (*)(void))reaction_cb_3C0A4);`:

```c
    /* PORT: record gameplay-u6 §U6.13. The T-rex's reaction-0x24/0x25 callback
     * 0x3F0A8 (the dwords 0xA37F8/0xA380C) with the +0x0C/+0x18/+0x1C callbacks
     * it stores (0x3F0D2/0x3F0DD/0x3F0E6). */
    fn_register(0x3F0A8u, (void (*)(void))fighter_3f0a8);
    fn_register(0x3F054u, (void (*)(void))fighter_3f054);
    fn_register(0x3EFE0u, (void (*)(void))fighter_3efe0);
    fn_register(0x3F020u, (void (*)(void))fighter_3f020);
```

- [ ] **Step 4**: `all checks passed`.
- [ ] **Step 5: Mutations**: voice `0x8Cu` → `0x8Du` ⇒ `141 != 140`; `+0x18 = 0x0003EFE0u` → `0x0003E484u` ⇒ `255108 != 258016`; `<= 10` → `<= 9` ⇒ `2 != 1`; `(s32)(s16)DSW(…)` → `(s32)DSW(…)` ⇒ `2 != 1`; the `0x1080A4` clear in `fighter_3f020` → `(void)0;` ⇒ `24929 != 0`; `flags[0] = 1u;` → `0u` in `fighter_3efe0` ⇒ `0 != 1`; the `fn_register(0x3F0A8u, …)` line → `(void)fighter_3f0a8;` ⇒ `0x3F0A8 is registered`.
- [ ] **Step 6**: replay; `0x3F0A8` gone. The record predicts `0x3F0F0`/`0x3F130` (`anim_indirect`) next: Task 11.

### Task 11: `0x3F0F0`, `0x3F130` and `0x2BEF4` (the `0xD100` targets in `0x3F0A8`'s streams)

Record §U6.17. Files as Task 8 (and `actors.c`'s prototype block).

- [ ] **Step 1: Failing test** (append):

```c

/* The one active pool record that `before` (n entries) does not list; 0 for
 * none or several. */
static u32 u6b_new_record(const u32 *before, u32 n)
{
    u32 r, hit = 0u, k, found = 0u;
    for (r = actor_list_head(); r != 0; r = actor_next(r)) {
        for (k = 0; k < n && before[k] != r; k++) {}
        if (k == n) { hit = r; found++; }
    }
    return found == 1u ? hit : 0u;
}

static u32 u6b_list(u32 *out, u32 cap)
{
    u32 r, n = 0;
    for (r = actor_list_head(); r != 0 && n < cap; r = actor_next(r)) out[n++] = r;
    return n;
}

/* §U6.17: 0x3F0F0 and 0x3F130, the 0xD100 targets of 0x3F0A8's two streams:
 * no owner slot, nothing; with one, the emitter 0xBB290 is spawned with the
 * slot in +0x14 and +0x2B bit 0 (0x2BEF4); 0x3F130 also sets +0x50 = 2. */
static void check_u6b_3f0f0(void)
{
    static u32 before[0x80];
    u32 n, e, rec = Z_R0;
    CHECK_EQ_INT((int)DSD(0x000E7B8Cu), 0x0003F0F0);
    CHECK_EQ_INT((int)DSW(0x000E7B8Au), 0xD100);
    CHECK_EQ_INT((int)DSD(0x000E7BC6u), 0x0003F130);
    CHECK_EQ_INT((int)DSW(0x000E7BC4u), 0xD100);
    CHECK(fn_resolve(0x3F0F0u) != NULL, "0x3F0F0 is registered");
    CHECK(fn_resolve(0x3F130u) != NULL, "0x3F130 is registered");
    sc_seed(0u, 1u, 0);
    DSD(rec + 0x14u) = 0u;
    n = u6b_list(before, 0x80u);
    fighter_3f0f0(rec);
    fighter_3f130(rec);
    CHECK_EQ_INT((int)u6b_list(before, 0x80u), (int)n);   /* no owner: no spawn */
    DSD(rec + 0x14u) = Z_S0;
    n = u6b_list(before, 0x80u);
    fighter_3f0f0(rec);
    e = u6b_new_record(before, n);
    CHECK(e != 0u, "0x3F0F0 spawns one record");
    if (e != 0u) {
        CHECK_EQ_INT((int)DSD(e + 0x14u), (int)Z_S0);
        CHECK_EQ_INT((int)(DSB(e + 0x2Bu) & 1u), 1);
        CHECK(DSB(e + 0x50u) != 2u, "0x3F0F0 leaves +0x50");
    }
    n = u6b_list(before, 0x80u);
    fighter_3f130(rec);
    e = u6b_new_record(before, n);
    CHECK(e != 0u, "0x3F130 spawns one record");
    if (e != 0u) {
        CHECK_EQ_INT((int)DSD(e + 0x14u), (int)Z_S0);
        CHECK_EQ_INT((int)(DSB(e + 0x2Bu) & 1u), 1);
        CHECK_EQ_INT((int)DSB(e + 0x50u), 2);
    }
}

int test_u6b_3f0f0(void)        { return u6b_run(check_u6b_3f0f0); }
```

- [ ] **Step 2**: build error naming `fighter_3f0f0`.
- [ ] **Step 3: Implement.** `fighter.c` before `/* 0x3E3A8. The T-rex's reaction-0x2A callback`:

```c
#define FIGHT_DESC_3F0F0 0x000BB290u  /* 0x3F115/0x3F155: the emitter descriptor */

/* 0x2BEF4 — record gameplay-u6 §U6.17. `or byte [eax+0x2B],1; ret`: sets bit 0
 * of the record's +0x2B. Its two callers are 0x3F0F0 and 0x3F130 (0x3F125,
 * 0x3F169); Ghidra has no function here. */
static void fighter_2bef4(u32 rec)
{
    DSB(rec + 0x2Bu) |= 1u;                             /* 0x2BEF4 */
}

/* 0x3F0F0 — record gameplay-u6 §U6.17. The animation-opcode 0x11 target in the
 * reaction-0x24 stream (the dword at 0xE7B8C after the 0xD100 word at 0xE7B8A,
 * its only reference). EAX = rec; EBX, ECX, EDX and ESI are saved and set
 * before any read. With the record's +0x14 (its slot) set, it spawns the
 * emitter 0xBB290 (a2 = EDX = 8, a3 = ECX = 0, a4 = EBX = -0x5A, a5 = the
 * record's +0x56 | 0x400), gives it the slot in +0x14 and sets its +0x2B bit 0
 * through 0x2BEF4. */
void fighter_3f0f0(u32 rec)
{
    u32 slot = DSD(rec + 0x14u);                        /* 0x3F0F6 */
    u32 e;
    if (slot == 0u) return;                             /* 0x3F0FA */
    e = actor_spawn((const u32 *)(mem + FIGHT_DESC_3F0F0), 8u, 0u, 0xFFFFFFA6u,
                    (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x3F0FC..0x3F11A 0x2AE14 */
    DSD(e + 0x14u) = slot;                              /* 0x3F11F/0x3F122 */
    fighter_2bef4(e);                                   /* 0x3F125 0x2BEF4 */
}

/* 0x3F130 — record gameplay-u6 §U6.17. The same target in the 0xE7BBE stream
 * (the dword at 0xE7BC6 after its 0xD100 word, its only reference): 0x3F0F0's
 * body with the emitter's +0x50 = 2 before +0x14. */
void fighter_3f130(u32 rec)
{
    u32 slot = DSD(rec + 0x14u);                        /* 0x3F136 */
    u32 e;
    if (slot == 0u) return;                             /* 0x3F13A */
    e = actor_spawn((const u32 *)(mem + FIGHT_DESC_3F0F0), 8u, 0u, 0xFFFFFFA6u,
                    (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x3F13C..0x3F15A 0x2AE14 */
    DSB(e + 0x50u) = 2u;                                /* 0x3F162 */
    DSD(e + 0x14u) = slot;                              /* 0x3F15F/0x3F166 */
    fighter_2bef4(e);                                   /* 0x3F169 0x2BEF4 */
}

```

`fighter.h` after `u32 fighter_3e484(u32 side);`:

```c
/* 0x3F0F0 and 0x3F130 (record gameplay-u6 §U6.17): the 0xD100 targets of the
 * 0xE7B78 and 0xE7BBE streams (EAX = rec): the emitter 0xBB290 as the slot's
 * child, +0x2B bit 0 (0x3F130 also +0x50 = 2). */
void fighter_3f0f0(u32 rec);
void fighter_3f130(u32 rec);
```

`actors.c`: after `static void reaction_cb_3C0A4(u32 slot, u32 rec, u32 side);` add the two lines `static void anim_code_3F0F0(u32 rec, u32 arg);` and `static void anim_code_3F130(u32 rec, u32 arg);`; after `    fn_register(0x3C0A4u, (void (*)(void))reaction_cb_3C0A4);` add

```c
    /* PORT: record gameplay-u6 §U6.17. The 0xD100 targets of 0x3F0A8's two
     * streams (the dwords 0xE7B8C and 0xE7BC6). */
    fn_register(0x3F0F0u, (void (*)(void))anim_code_3F0F0);
    fn_register(0x3F130u, (void (*)(void))anim_code_3F130);
```

and after `reaction_cb_3C0A4`'s definition

```c

/* 0x3F0F0 and 0x3F130 — the animation-opcode target shape. PORT: anim_indirect
 * calls every code pointer as (rec, arg); the raw reads EAX = rec only (EDX is
 * pushed, then set to 8 at 0x3F10D/0x3F14D before any read), so these
 * wrappers drop the operand (record gameplay-u6 §U6.17). */
static void anim_code_3F0F0(u32 rec, u32 arg)
{
    (void)arg;
    fighter_3f0f0(rec);
}

static void anim_code_3F130(u32 rec, u32 arg)
{
    (void)arg;
    fighter_3f130(rec);
}
```

- [ ] **Step 4**: `all checks passed`.
- [ ] **Step 5: Mutations**: `|= 1u` → `|= 2u` in `fighter_2bef4` ⇒ `0 != 1`; `DSB(e + 0x50u) = 2u;` → `(void)0;` ⇒ `0 != 2`; the `slot == 0u` return in `fighter_3f0f0` → `(void)0;` ⇒ `4 != 3`; the `fn_register(0x3F130u, …)` line → `(void)anim_code_3F130;` ⇒ `0x3F130 is registered`.
- [ ] **Step 6**: replay; both gone. Commit.

### Task 12: `0x3A820` (the `0x3A8E8` pose family's case-10 handler)

Record §U6.18. Files: `test_fight.c`, `test.h`, `fighter.c` (before `/* PORT: 0xC9030 (the 0x3A650 family's`), `fighter.h` (after `void fighter_pose_3a588(u32 slot, u32 side);`), `actors.c` (after `    fn_register(0x3A588u, (void (*)(void))fighter_pose_3a588);`).

- [ ] **Step 1: Failing test** (append; it uses U6a's `pose_3a588_seed`):

```c

/* §U6.18: 0x3A820, the 0x3A8E8 family's handler: 0x3A588's checks with the
 * 0xC9058 table (character 3: 0xD26F0, first word 0x17E8), B/A at
 * 0x107CFC/0x107CF8 and +0x90 = 4; 0x3A588's B/A words armed as traps. */
static void check_u6b_3a820(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    CHECK_EQ_INT((int)DSD(0x0003A921u), 0x0003A820);
    CHECK(fn_resolve(0x3A820u) == (void (*)(void))fighter_pose_3a820, "0x3A820 is registered");
    pose_3a588_seed(s0, s1, r0, r1);
    DSW(DS_00107CF8) = 0; DSW(DS_00107CF8 + 2u) = 0;
    DSW(DS_00107CFC) = 0; DSW(DS_00107CFC + 2u) = 0;
    DSW(0x00107D00u) = 3; DSW(0x00107D0Cu) = 0x4321;     /* 0x3A588's pair: a trap */
    DSB(s0 + 0x58u) = 0;
    DSB(s0 + 0x90u) = 0x55;
    fighter_pose_3a820(s0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 0x55);
    pose_3a588_seed(s0, s1, r0, r1);
    DSW(DS_00107CF8) = 0; DSW(DS_00107CFC) = 0;
    DSW(0x00107D00u) = 3; DSW(0x00107D0Cu) = 0x4321;
    fighter_pose_3a820(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000D26F0);      /* 0xC9058[3] */
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS), 0x17E8);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 4);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);       /* B = 0: no snap */
    pose_3a588_seed(s0, s1, r0, r1);
    DSW(DS_00107CFC) = 3; DSW(DS_00107CF8) = 0x2468;
    fighter_pose_3a820(s0, 0u);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x2468);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x2468 - 0x1000);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 4);
    pose_3a588_seed(s0, s1, r0, r1);
    DSB(s0 + 0x90u) = 4;                               /* 0x3A810: 1..4 skip the snap */
    DSW(DS_00107CFC) = 3; DSW(DS_00107CF8) = 0x2468;
    fighter_pose_3a820(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);
    pose_3a588_seed(s0, s1, r0, r1);
    DSW(DS_00107CFC + 2u) = 3; DSW(DS_00107CF8 + 2u) = 0x6543;
    DSW(DS_00107CFC) = 0;
    fighter_pose_3a820(s1, 1u);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 0x6543);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), 0x6543 - 0x2000);
    CHECK_EQ_INT((int)DSB(s1 + 0x90u), 4);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)0xDEADBEEFu);
}


int test_u6b_3a820(void)        { return u6b_run(check_u6b_3a820); }
```

- [ ] **Step 2**: build error naming `fighter_pose_3a820`.
- [ ] **Step 3: Implement.** `fighter.c`:

```c
/* PORT: 0xC9058 (the 0x3A8E8 family's per-character animation-stream table,
 * read at 0x3A862) has no symbols.h name. */
#define FIGHT_ANIM_3A820  0x000C9058u

/* 0x3A820 — record gameplay-u6 §U6.18. The 0x3A8E8 pose family's per-frame
 * handler 0x3531C case 10 calls through slot+0x10 (0x3A8E8 stores it at
 * 0x3A91E, the dword at 0x3A921 its only reference). 0x3A588's body with the
 * 0xC9058 stream table, the 0x3A8E8 setter's globs (B = 0x107CFC + side*2,
 * A = 0x107CF8 + side*2, the high words of the dwords at 0x107CFA/0x107CF6)
 * and +0x90 = 4 at the end; the jump table 0x3A810 sends +0x90 - 1 in 0..3 to
 * 0x3A8D6, past the snap. EAX = slot (dead), EBX = side. */
void fighter_pose_3a820(u32 slot, u32 side)
{
    u32 ctx[6];
    u8 phase;
    (void)slot;
    fighter_ctx_swap(ctx, side);                            /* 0x3A823..0x3A827 0x33A10 */
    phase = DSB(ctx[3] + 0x58u);                            /* 0x3A82C/0x3A830 */
    if (phase == 0u) {                                      /* 0x3A835/0x3A83D */
        DSB(ctx[3] + 0x58u) = 1u;                           /* 0x3A849 */
        return;
    }
    if (phase != 1u) return;                                /* 0x3A837/0x3A839 */
    actors_anim_begin(ctx[5],                                /* 0x3A855..0x3A86D 0x2BC30 */
                      DSD(FIGHT_ANIM_3A820
                          + (u32)DSB(ctx[3] + 0x7Au) * 4u),
                      0x40400000u);
    hit_anchor_set(ctx[1], DSD(ctx[5] + 0x18u), 0u);        /* 0x3A872..0x3A87F 0x188AC */
    DSB(ctx[3] + 0x58u) = 2u;                               /* 0x3A888 */
    {
        s32 a = (s32)(s16)DSW(DS_00107CF8 + ctx[1] * 2u);   /* 0x3A890/0x3A8A3 */
        s32 b = (s32)(s16)DSW(DS_00107CFC + ctx[1] * 2u);   /* 0x3A897/0x3A8A0 */
        if (b != 0 && b != 5) {                             /* 0x3A8A6..0x3A8AD */
            if ((u8)(DSB(ctx[3] + 0x90u) - 1u) > 3u)        /* 0x3A8B3..0x3A8BD */
                hit_anchor_x(ctx[1], (u32)a);               /* 0x3A8CC..0x3A8D1 0x188DC */
        }
    }
    DSB(ctx[3] + 0x90u) = 4u;                               /* 0x3A8DA */
}

```

`fighter.h`:

```c
/* 0x3A820 (record gameplay-u6 §U6.18): the 0x3A8E8 family's handler, 0x3A588's
 * body with 0xC9058, B/A at 0x107CFC/0x107CF8 and +0x90 = 4. */
void fighter_pose_3a820(u32 slot, u32 side);
```

`actors.c`:

```c
    /* PORT: record gameplay-u6 §U6.18. The fourth sibling 0x3A820, which the
     * 0x3A8E8 setter stores at 0x3A91E (the dword at 0x3A921). */
    fn_register(0x3A820u, (void (*)(void))fighter_pose_3a820);
```

- [ ] **Step 4**: `all checks passed`.
- [ ] **Step 5: Mutations**: `+0x90 = 4u` → `2u` ⇒ `2 != 4`; `FIGHT_ANIM_3A820 0x000C9058u` → `0x000C9030u` ⇒ `861896 != 861936`; A read `DS_00107CF8` → `DS_00107CFC` ⇒ `3 != 9320`; the `fn_register(0x3A820u, …)` line → `(void)fighter_pose_3a820;` ⇒ `0x3A820 is registered`.
- [ ] **Step 6**: replay; gone. Commit.

### Task 13: `0x231C0` (character 1's reaction `0x27`, the CPU's move)

Record §U6.16. Files: `test_fight.c`, `test.h`, `fighter.c` (before `/* 0x23178 — record §43-C.`), `fighter.h` (after `int fighter_230f0(u32 slot, u32 rec, u32 side);`), `actors.c`.

- [ ] **Step 1: Failing test** (append):

```c

/* §U6.16: 0x231C0 on side 1: state 9/7/0 and +0x0C = 0, the record on 0xE48EE
 * at 3.0, the voice 0x7C once; +0x57/+0x18/+0x1C untouched. */
static void check_u6b_231c0(void)
{
    CHECK_EQ_INT((int)DSD(0x000A3D34u), 0x000231C0);
    CHECK(fn_resolve(0x231C0u) != NULL, "0x231C0 is registered");
    sc_seed(1u, 1u, 0);
    DSW(0x000E48EEu) = 0x1565u;
    DSB(Z_S1 + 0x52u) = 0x0Cu;
    DSB(Z_S1 + 0x53u) = 0x33u;
    DSB(Z_S1 + 0x54u) = 0x44u;
    DSD(Z_S1 + 0x0Cu) = 0x0C0C0C0Cu;
    DSB(Z_S1 + 0x57u) = 0x57u;
    DSD(Z_S1 + 0x18u) = 0x18181818u;
    DSD(Z_S1 + 0x1Cu) = 0x1C1C1C1Cu;
    sound_voice_log_reset();
    CHECK_EQ_INT(fighter_231c0(Z_S1, Z_R1, 1u), 1);
    sc_stream(Z_R1, 0x000E48EEu, 0x40400000u, 0x1565u);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x0Cu), 0);
    CHECK_EQ_INT((int)DSB(Z_S1 + 0x57u), 0x57);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x18u), 0x18181818);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x1Cu), 0x1C1C1C1C);
    CHECK_EQ_INT((int)sound_voice_log_count(), 1);
    CHECK_EQ_INT((int)sound_voice_log_at(0), 0x7C);
    sound_voice_log_reset();
}

int test_u6b_231c0(void)        { return u6b_run(check_u6b_231c0); }
```

- [ ] **Step 2**: build error naming `fighter_231c0`.
- [ ] **Step 3: Implement.** `fighter.c`:

```c
/* PORT: a data-object address symbols.h does not name. */
#define FIGHT_ANIM_231C0 0x000E48EEu  /* 0x231DF: the reaction-0x27 stream */

/* 0x231C0 — record gameplay-u6 §U6.16. Character 1's reaction-0x27 callback
 * (*(u32*)0xA3D34, its only reference), same registers and dead context as
 * 0x23130: state 9/7/0 and +0x0C = 0 first (so 0x3C4CC sees +0x52 = 9), the
 * 0xE48EE stream at 3.0, the voice 0x7C; AL = 1 (unread). */
int fighter_231c0(u32 slot, u32 rec, u32 side)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);                        /* 0x231C9..0x231CD 0x33950 */
    DSB(slot + 0x52u) = 9u;                             /* 0x231D2 */
    DSB(slot + 0x53u) = 7u;                             /* 0x231DB */
    DSB(slot + 0x54u) = 0;                              /* 0x231E4 */
    DSD(slot + 0x0Cu) = 0;                              /* 0x231EA */
    hit_anim_start_b(rec, FIGHT_ANIM_231C0, 0x40400000u);   /* 0x231D6..0x231F1 0x3C4CC */
    (void)sound_voice(0x7Cu);                           /* 0x231F6/0x231FB 0x2C3FC */
    return 1;                                           /* 0x23200 */
}

```

`fighter.h`:

```c
/* 0x231C0 (record gameplay-u6 §U6.16): character 1's reaction-0x27 callback. */
int fighter_231c0(u32 slot, u32 rec, u32 side);
```

`actors.c`: after `static void reaction_cb_3C0A4(u32 slot, u32 rec, u32 side);` add `static void reaction_cb_231C0(u32 slot, u32 rec, u32 side);`; after `    fn_register(0x23130u, (void (*)(void))reaction_cb_23130);` add

```c
    /* PORT: record gameplay-u6 §U6.16. Character 1's reaction-0x27 callback. */
    fn_register(0x231C0u, (void (*)(void))reaction_cb_231C0);
```

after `reaction_cb_3C0A4`'s definition

```c

/* 0x231C0 — the reaction-callback shape. PORT: the same 0x35045 call, whose
 * AL is ignored; this wrapper drops fighter_231c0's result. */
static void reaction_cb_231C0(u32 slot, u32 rec, u32 side)
{
    (void)fighter_231c0(slot, rec, side);
}
```

- [ ] **Step 4**: `all checks passed`.
- [ ] **Step 5: Mutations**: voice `0x7Cu` → `0x7Bu` ⇒ `123 != 124`; the `fn_register(0x231C0u, …)` line → `(void)reaction_cb_231C0;` ⇒ `0x231C0 is registered`.
- [ ] **Step 6**: replay; gone. Commit.

(All of Tasks 8–13 were applied in this order and also with Tasks 8, 9, 11 skipped to a U6a tree while planning: every step built and ended `all checks passed`.)

---

### Task 14: Pin what is left (only if the replay still lists an unregistered pair)

**Files:** `port/tests/test_platform.c` (`k_miss_gp_u6_moves` rows and its comment); record §U6.22.

- [ ] **Step 1**: for each remaining pair, classify it from the raw like record §G.24's table (disassemble the target from `$S/image.bin`: what it is, its references, its first `f` with a throw-away `fprintf` in `fn_resolve_from`, reverted), and name it for track P in §U6.22.
- [ ] **Step 2**: add one row per pair to `k_miss_gp_u6_moves` (`    { 0xADDRu, "<caller>" },`) and one comment line per row above the array (`*   0xADDR <caller>: <what it is>, f = 0x…;`). Replay ⇒ `all checks passed`. Mutation: remove the row ⇒ `unexpected 0xADDR from <caller>`. Commit (`tests: pin the gp-u6-moves misses handed to track P (record gameplay-u6 §U6.22)`). Skip the task when the replay already ends `all checks passed`.

---

### Task 15: Pin the three ratchets and the identity; `make verify`

**Files:** `Makefile` (variables and the `gp-moves-oracle` target before `gp-report:`; the `.PHONY` list; one `verify` line after `@$(MAKE) --no-print-directory gp-oracle`).

- [ ] **Step 1: Measure** on the final head: `make gp-report scenario=gp-u6-moves $V | tee $S/t15_report.txt` ⇒ `window from capture W`, `FIRST UNEXPLAINED capture N` (or `0 unexplained through E`: then N = E + 1, the exact pin), `trace: first difference f=… (F)`, `moves: first difference f=… (M)` (or `0 differing through E2`: M = E2 + 1). These are the pins (measured, never chosen); the expected shape from §U6.8: N stops at the character select (U5's divergence), F at or after the round-1 start.

- [ ] **Step 2: Edit the Makefile.** Add `\` and a continuation line `        gp-moves-oracle` to the `.PHONY` list after `gp-report diff-verify`; before `gp-report: build ##` add (with the measured numbers and their provenance lines in the comment):

```make
# Plan gameplay-u6b (record gameplay-u6 §U6.22): the gp-u6-moves capture (P1 Sauron's twelve
# scripted attempts in round 1, record §U6.12) against its port replay. Three ratchets, each the
# measured first unexplained/differing item (raise it when it improves): the frames, the trace
# (gp_session.TRACE_FIELDS) and the moves claim (gp_session.MOVE_FIELDS: c0 c1 r0 r1 s0_43,
# record §U6.11); the window start; the capture identity (a re-capture fails: re-measure, then
# re-pin). Skips without data/k11-captures/gp-u6-moves, like gp-oracle.
GP_MOVES_MIN_FIRST = <N from Step 1>
GP_MOVES_TRACE_MIN_FIRST = <F from Step 1>
GP_MOVES_MOVES_MIN_FIRST = <M from Step 1>
GP_MOVES_MAX_START = <W from Step 1>
GP_MOVES_CAPTURE_SHA256 = <Task 6 Step 4>
GP_MOVES_CAPTURE_FRAMES = <Task 6 Step 4>
gp-moves-oracle: build ## Gameplay oracle: gp-u6-moves frame, trace and moves ratchets (skips without data/k11-captures/gp-u6-moves)
	@echo "== gameplay oracle: gp-u6-moves (frame, trace and moves ratchets) =="
	@$(MAKE) --no-print-directory gp-replay scenario=gp-u6-moves GP_OPTIONAL=1
	@$(PYTHON) tools/gp_compare.py --scenario gp-u6-moves --capture $(K11_CAPTURES)/gp-u6-moves \
		--port $(GP_DUMP)/gp-u6-moves --min-first "$(GP_MOVES_MIN_FIRST)" \
		--trace-min-first "$(GP_MOVES_TRACE_MIN_FIRST)" --max-start "$(GP_MOVES_MAX_START)" \
		--moves-min-first "$(GP_MOVES_MOVES_MIN_FIRST)" \
		--capture-sha256 "$(GP_MOVES_CAPTURE_SHA256)" --capture-frames "$(GP_MOVES_CAPTURE_FRAMES)"

```

The `<…>` are the six numbers measured in Step 1 and Task 6 Step 4 (the target and the comment ran while planning with empty pins: absent capture ⇒ `gp-replay: no capture …`, `gp_compare: no capture … (skipped)`, `rc=0`). In the `verify` recipe after `	@$(MAKE) --no-print-directory gp-oracle` add:

```make
	@echo "== gameplay oracle: gp-u6-moves (skips without its capture; record gameplay-u6 §U6.22) =="
	@$(MAKE) --no-print-directory gp-moves-oracle
```

- [ ] **Step 3: Green and each pin can fail**:

```bash
make gp-moves-oracle $V; echo "rc=$?"
make gp-moves-oracle $V GP_MOVES_MIN_FIRST=$((N+1)) 2>&1 | grep FAIL
make gp-moves-oracle $V GP_MOVES_TRACE_MIN_FIRST=$((F+1)) 2>&1 | grep FAIL
make gp-moves-oracle $V GP_MOVES_MOVES_MIN_FIRST=$((M+1)) 2>&1 | grep FAIL
make gp-moves-oracle $V GP_MOVES_MAX_START=$((W-1)) 2>&1 | grep FAIL
make gp-moves-oracle $V GP_MOVES_CAPTURE_SHA256=00ff 2>&1 | grep FAIL
```

Expected: `rc=0` with the three `ratchet N … ok` lines and `matches the pin`; then one `FAIL` line each (`frames: FAIL: first unexplained N < ratchet N N+1`, `trace: FAIL: …`, `moves: FAIL: first differing M < ratchet N M+1`, `window starts at capture W … > pinned start W-1`, `poll.log sha256 … != the pinned 00ff`).

- [ ] **Step 4: The full gate**: `make verify $V > $S/t15_verify.txt 2>&1; echo "verify-exit=$?"`; `ORACLES-EQUAL` (the 45 lines), `WAV-EQUAL`, the `k11_compare` lines equal Task 0's, the gp-idle-loss lines equal `$S/t0_gp_lines.txt`, the gp-moves-oracle lines of Step 3, `python3 tools/port_progress.py` ⇒ `771 1203 64` and `731 731 100`.
- [ ] **Step 5: Commit**: `git add Makefile`; `build: pin the gp-u6-moves frame, trace and moves ratchets in make verify (record gameplay-u6 §U6.22)`.

---

### Task 16: Closure

**Files:** record §U6.22; `docs/PROGRESS.md` (append); `AGENTS.md` (the `## Commands` block: one line `make gp-moves-oracle      # gameplay oracle: gp-u6-moves frame, trace and moves ratchets (N values in the Makefile); in make verify; skips without the capture`; in the gameplay-oracle paragraph one sentence: "`make gp-moves-oracle` does the same for `data/k11-captures/gp-u6-moves` and adds a third ratchet, `moves`, over the snapshot's move bytes (`c0 c1 r0 r1 s0_43`, record gameplay-u6 §U6.11); its claim is as narrow.").

- [ ] **Step 1: Record §U6.22 "U6b execution"**: the capture facts (size, session checks, path, `c0/c1`), the check table of Task 6 Step 3, the replay's miss log before and after each port task, which of Tasks 8–14 ran, the three pins with their measured lines, the failure proofs, every mutation's first `FAIL` line, the named gaps (record §U6.19 items 4–7 plus any not-performed move and any Task 14 pair), and the narrow claim: "no content-bearing capture frame of `gp-u6-moves` before N is unexplained; `TRACE_FIELDS` agree below F and `MOVE_FIELDS` below M; nothing is claimed about frames or state after those, nor about the order of the port's frames."
- [ ] **Step 2: PROGRESS** paragraph "Gameplay U6b: the scripted-moves capture" (the moves performed, the functions ported, the pins, the counters unchanged).
- [ ] **Step 3: Gate and commit**: `make verify $V` ⇒ `verify-exit=0`; `git add docs/superpowers/plans/2026-10-01-gameplay-u6-derivations.md docs/PROGRESS.md AGENTS.md`; `docs: gameplay U6b closure: the scripted-moves capture, its ratchets and named gaps (record gameplay-u6 §U6.22)`.

---

## Shared-file touch points (additive; for the controller's merge order)

| file | region | what |
|---|---|---|
| `tools/gp_session.py` | `SNAP_FIELDS` tail; after `TRACE_FIELDS`; after `SCENARIOS['gp-idle-loss-run2']` | five fields; `MOVE_FIELDS`; `U6_MOVES_STEPS` and `SCENARIOS['gp-u6-moves']` |
| `port/tests/test_game.c` | `gp_trace_line` | five fields appended to the `T` line (another unit appending fields must keep the order equal to `SNAP_FIELDS`: Task 1's test enforces it) |
| `tools/gp_compare.py` | `trace_claim` signature (backward compatible), `main` | the `moves` claim, `--moves-min-first` |
| `port/tests/test_platform.c` | before `fnm_gp_scenario`; `fnm_known` (one parameter); `test_fn_misslog_driver` | `k_miss_gp_u6_moves`; units adding their own scenario sets add a parameter the same way |
| `Makefile` | `.PHONY`, before `gp-report:`, `verify` | `GP_MOVES_*`, `gp-moves-oracle`, one verify line, `tools.tests.test_gp_moves` |
| `port/src/game/{fighter.c,fighter.h,actors.c}`, `port/tests/test_fight.c`, `port/tests/test.h` | next to `0x3E3A8`, `0x3A588`, `0x23178`; end of `test_fight.c` | up to eleven functions and six tests |
| `docs/PROGRESS.md`, `AGENTS.md` | end; Commands, gameplay-oracle paragraph | one paragraph; one line; one sentence |

## Execution notes

- **Order:** 0 → 1 → 2 → 3 → 4 → 5 → (Decision 1) 6 → 7 → 8–13 as the miss log names them (8, 9, 10 before 11; 12 and 13 whenever listed) → 14 → 15 → 16. Never re-capture `gp-u6-moves` over itself (record §G.24 item 5).
- **Model tier:** Tasks 1–4 and 8–13 (given code, measured checks): mid tier (Sonnet). Tasks 5–7, 14 and 15 (measurement, judgement on the check table, classification, pins) and all reviews: strong tier (Opus).
- **Gate:** each task's tests as written; the replay after every port task; `make verify $V` in Tasks 0, 15 and 16 (about 13 minutes plus about 1 minute for the new replay).
