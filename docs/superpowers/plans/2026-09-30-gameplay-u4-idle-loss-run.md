# Gameplay U4 — The First Scripted Run (idle loss) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Capture the pinned original from mode 3 through START MENU → LEFT PLAYER ARCADE → character select → a two-round match with P1 idle → the CPU's win → the challenge offer → game over → mode 3, twice; answer the determinism question; replay it in the port; triage and name the first frame and trace divergences; pin both gameplay ratchets in `make verify`.

**Architecture:** One scenario, `gp-idle-loss` (spec §4.4), in `tools/gp_session.py`, captured with U1's `make gp-capture`, replayed with U2's `make gp-replay`, compared with U3's `tools/gp_compare.py`. This unit writes no engine code: every divergence is pinned by the ratchet and named in the record for a follow-up unit.

**Tech Stack:** the U1–U3 tools, DOSBox-X 2026.08.31, the port build, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md` §3.4–§3.5 (the expected path), §4.4 (this unit), §6, §7 Q1/Q6/Q7.

**Derivation record:** `docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md`, sections `§G.17..§G.23`.

**Where to run.** Main checkout, branch `gameplay-ground-truth`, after U1–U3. `S=/tmp/gameplay-u4`. Every command assumes `cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse`.

**Before Task 2: spec §7 Q7 needs the user's answer** (two captures of roughly 0.3–0.6 GB each after gzip under the git-ignored `data/k11-captures/`). Without it, stop after Task 1.

---

## Global Constraints

- Spec §5: "Raw wins; never a fitted constant; a value that cannot be pinned is a named gap with its evidence. Harness values (`HOLD_FRAMES`, the 150-frame gaps, `time_limit`, …) are named as harness values with their source, never presented as game values."
- Spec §5: "`make verify` is the gate after every task; the oracle lines equal the U1 Task 0 baseline; the enforced front-end oracle, the demo-fight and attract cycle-2 ratchets and the K11 oracles stay green."
- Spec §2: "Fixing any divergence U4 finds is also out of scope: U4 pins it and names it; a follow-up unit fixes it." No file under `port/src` changes in U4.
- Spec §4.4 acceptance: "the capture reproduces §3.5's mode path (or the record says where and why it differs); both ratchets are in `make verify`; the oracle lines of the baseline are unchanged."
- AGENTS.md (the demo-fight precedent): a ratchet value is "a measured value … raise it when the window grows"; the Makefile comment gives its provenance (the commit and the measured line).
- AGENTS.md: "Captures are git-ignored. `data/` is git-ignored and read-only" — U4 writes only `data/k11-captures/gp-idle-loss*/` through `make gp-capture`.
- AGENTS.md: "Commit style: `<area>: <what changed>`. **Never `git add -A`**." Trailer `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **A capture that did something else.** The CPU might not win, the pick might not time out, or a key might land in the wrong menu row. Task 2 compares the capture's mode path with spec §3.5 row by row before anything is pinned.
2. **Pinning a nondeterministic frame.** If two runs of the original differ, a ratchet above the first run-to-run difference claims more than the original itself repeats. Task 3's `trace-diff` bounds the trace N; Task 5 checks it.
3. **A ratchet pinned where it cannot fail.** Task 5 proves both N values fail when raised by one and when a port frame below N is damaged.
4. **A replay that stalls or faults mid-run** turns `make verify` red for a reason unrelated to the claim. Task 4's branch truncates the script at a recorded frame (`--end`), pinned with its evidence, rather than dropping the oracle.
5. **Reading green as correct.** The record states the narrow claim (U3 Task 5) and lists every mode of spec §3.5 past N as not covered.

---

### Task 1: The scenario and the script truncation

**Files:**
- Modify: `tools/gp_session.py` (`SCENARIOS`; `port_script(..., end=None)`; the CLI's `--end`)
- Modify: `tools/tests/test_gp_session.py` (append `TestIdleLoss`)
- Modify: `Makefile` (`gp-replay` passes `$(GP_SCRIPT_ARGS)` to `port-script`)
- Modify: record §G.17

**Interfaces:**
- Consumes: `Schedule`, `port_script`, `_log` (the U1 test fixture in `test_gp_session.py`)
- Produces: `SCENARIOS['gp-idle-loss']`, `SCENARIOS['gp-idle-loss-run2']` (the same steps, a second capture directory); `port_script(name, lines, end=None)`; `gp_session.py port-script … [--end F]`; `make gp-replay … GP_SCRIPT_ARGS="--end F"`

- [ ] **Step 1: Write the failing tests**

```python
class TestIdleLoss(unittest.TestCase):
    def test_steps_fire_on_the_probe_path(self):
        # spec §3.5: mode 0x27 first at 0x120, back in mode 3 at 0x22BA
        s = gs.Schedule(gs.SCENARIOS['gp-idle-loss']['steps'])
        self.assertEqual(s.due_boot(gs.ENTER_WAIT), [(0, ('key', 'enter'))])
        s.on_mode(0x120, 0x27)
        self.assertEqual(s.due(0x120 + 149), [(1, ('key', 'enter'))])
        self.assertEqual(s.due(0x120 + 299), [(2, ('key', 'enter'))])
        for f, m in ((0x247, 0x2D), (0x26C, 0x10), (0x7B5, 6), (0x1F84, 0x13), (0x2200, 0x1E)):
            s.on_mode(f, m)
        self.assertIsNone(s.end_frame)
        s.on_mode(0x22BA, 0x03)
        self.assertEqual((s.end_frame, s.fired, s.total), (0x22BA, 3, 3))
        self.assertEqual(gs.SCENARIOS['gp-idle-loss-run2']['steps'], gs.SCENARIOS['gp-idle-loss']['steps'])

    def test_end_truncates_the_script(self):
        gs.SCENARIOS['_t'] = dict(time_limit=1, steps=())
        text = gs.port_script('_t', _log(), end=295)
        lines = [l for l in text.splitlines() if not l.startswith('#')]
        self.assertEqual(lines, ['enter_frame 288', 'enter_state 0000',
                                 'key 288 1C 0D', 'key 294 1F 73', 'bits 294 8000', 'end 295'])
        with self.assertRaises(gs.ScriptError):
            gs.port_script('_t', _log(), end=400)        # past the capture's X record
```

- [ ] **Step 2: Run to verify it fails**

Run: `python3 -m unittest tools.tests.test_gp_session -v`
Expected: `KeyError: 'gp-idle-loss'` and `TypeError: port_script() got an unexpected keyword argument 'end'`.

- [ ] **Step 3: Implement**

In `SCENARIOS` add (spec §4.4; the 150-frame gaps are the probe's 2.5 s, the 200 s limit covers its 169.9 s — harness values):

```python
    'gp-idle-loss': dict(time_limit=200, steps=(
        ('boot', ENTER_WAIT, ('key', 'enter')),       # mode 3 -> 0x27, MAIN MENU on "Start"
        ('after_mode', 0x27, 150, ('key', 'enter')),  # START MENU, cursor on row 0 (spec §3.3)
        ('after', 150, ('key', 'enter')),             # LEFT PLAYER ARCADE: mode 0x2D
        ('until_mode', 0x03, 0),                      # back in mode 3 after game over
    )),
```

and after the dict: `SCENARIOS['gp-idle-loss-run2'] = dict(SCENARIOS['gp-idle-loss'])`.

In `port_script`, change the signature to `def port_script(name, lines, end=None):`, rename the local `X` record from `end` to `xrec`, and replace everything from the `X` lookup to the `return` with:

```python
    xrec = next((r for r in recs if r['kind'] == 'X'), None)
    if xrec is None:
        raise ScriptError('the scenario end (X record) was not reached')
    last = xrec['f']
    if end is not None:
        if end > last:
            raise ScriptError('--end %d is past the capture end f=%X' % (end, last))
        last = end
    out = ['# gp port script v2: scenario %s%s' % (name, '' if end is None else ' (cut at %d)' % end),
           'enter_frame %d' % p27['f'],
           'enter_state %04X' % p27['st']]
    ev = [(c, 0, 'key %d %02X %02X' % (c, s, a)) for c, s, a in keys if c <= last]
    ev += [(f, 1, 'bits %d %04X' % (f, kb)) for f, kb in bits if f <= last]
    out += [t for _, _, t in sorted(ev)]
    out.append('end %d' % last)
    return '\n'.join(out) + '\n'
```

In `main`, add `ap.add_argument('--end', type=int)` and pass `end=a.end` to `port_script`. In the Makefile's `gp-replay`, change the `port-script` command's tail to `--out $(GP_DUMP)/$(scenario).script $(GP_SCRIPT_ARGS) && \`.

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_gp_session -v`
Expected: all tests `OK` (U1's 17 plus these 2).

- [ ] **Step 5: Mutation proof**

Drop the `if c <= last` filter on keys: expected `FAIL: test_end_truncates_the_script`. Restore; `OK`. Record in §G.17.

- [ ] **Step 6: Commit**

```bash
git add tools/gp_session.py tools/tests/test_gp_session.py Makefile docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp-idle-loss scenario and port-script --end truncation (record §G.17)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Capture run 1 and the mode path (investigation)

**Files:**
- Modify: record §G.18
- Writes (git-ignored): `data/k11-captures/gp-idle-loss/`

**Interfaces:**
- Produces: the capture every later task uses; §G.18's mode-path table

- [ ] **Step 1: Capture**

```bash
S=/tmp/gameplay-u4; mkdir -p $S
make gp-capture scenario=gp-idle-loss 2>&1 | tee $S/cap1.txt | tail -12
du -sh data/k11-captures/gp-idle-loss; cat data/k11-captures/gp-idle-loss/session.txt
```

Expected: every `CHECK … ok`, `steps fired 3/3`, the snapshot line (spec §3.7 predicts roughly 0.4 % of frames missed), `wall` under 200 s. Record `session.txt` and the size (Q7). If `end frame reached` fails (mode 3 not seen by 200 s), rerun once with `GP_ARGS="--time-limit 260"` and record both.

- [ ] **Step 2: The mode path**

```bash
python3 - <<'EOF'
import sys; sys.path.insert(0, 'tools')
import gp_session as gs
L = open('data/k11-captures/gp-idle-loss/poll.log').read().splitlines()
prev = None
for n, l in enumerate(L, 1):
    r = gs.parse(l)
    if r and r['kind'] in ('S', 'P') and r['mode'] != prev:
        extra = ' cred=%X s0_5a=%X s1_5a=%X e2=%X' % (r['cred'], r['s0_5a'], r['s1_5a'], r['e2']) if r['kind'] == 'S' else ''
        print('poll.log:%d %s f=%X mode=%X%s' % (n, r['kind'], r['f'], r['mode'], extra))
        prev = r['mode']
EOF
```

Expected: the sequence of spec §3.5 — `0x27`, `0x2D`, `0x1A`, `0x1B`, `0x10`, `0x1A`, `0x1B`, `0x11`, `0x17`, `0x1A`, `0x1B`, 5, 6, 8, `0x16`, 5, 6, 7, 9, `0x17`, `0x15`, `0x13`, `0x1E`, (`0x14`), `0x17`, 3 — with credits 5 → 4 at `0x2D`, P1 `+0x5A` reaching `0x78` in round 1, P2's `+0x5A` staying 0. The frames differ from the probe's by the Enter's timing (the probe's first Enter was at `f = 0x11D`); the deltas between modes should match the probe's (for example 916 frames in mode `0x10`, 3 249 in the second mode 6). Record the table with its `poll.log:<n>` citations beside spec §3.5's, and each delta. Any mode that differs (a different pick, a CPU loss, a continue on mode `0xE`) is a correction of spec §3.5: record it with its line; if the scenario no longer ends in mode 3, stop and report.

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
docs: gp-idle-loss capture run 1: the observed mode path (record §G.18)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Capture run 2 and the determinism answer (spec §7 Q1)

**Files:**
- Modify: record §G.19
- Writes (git-ignored): `data/k11-captures/gp-idle-loss-run2/`

- [ ] **Step 1: Capture and diff**

```bash
S=/tmp/gameplay-u4
make gp-capture scenario=gp-idle-loss-run2 2>&1 | tee $S/cap2.txt | tail -8
python3 tools/gp_session.py trace-diff --a data/k11-captures/gp-idle-loss --b data/k11-captures/gp-idle-loss-run2
```

The two runs' Enter frames differ (the boot is wall-timed), so compare them relative to their Enters: the trace diff keys by absolute `f`, which is only meaningful when the two first-`0x27` frames are equal. Check that first:

```bash
python3 - <<'EOF'
import sys; sys.path.insert(0, 'tools')
import gp_session as gs
for d in ('gp-idle-loss', 'gp-idle-loss-run2'):
    L = open('data/k11-captures/%s/poll.log' % d).read().splitlines()
    print(d, gs.port_script(d, L).splitlines()[1:4])
EOF
```

- [ ] **Step 2: Decide from the output**

- **Same `enter_frame` and same key frames:** the `trace-diff` line is the answer. `first difference none` → the original is deterministic over the run under this input (record "Q1: deterministic over f `<enter>..<end>`"). A first difference `f=X (field)` → record it, the mode at `X` in both runs, and the `tick` line; the trace ratchet can claim at most `f < X` (Task 5).
- **Different `enter_frame` or key frames** (the boot's wall-timed Enter landed on another frame): the runs are not the same input, so `trace-diff` does not answer Q1. Record both scripts' key lines, then compare the runs from each run's `enter_frame` on by writing both logs' `S` records with `f` rebased to `f − enter_frame` into `$S/a.log` and `$S/b.log` (a scratch script: `r['f'] -= enter`, then `gs.format_s`) and run `trace_diff` on those. Record the rebased answer and say that the rebasing is what makes it comparable. If the rebased runs also differ before mode `0x2D`, rerun run 2 once (the stimulus landed differently) and record both.

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
docs: gp-idle-loss run 2 and the determinism answer (record §G.19, spec Q1)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Replay in the port and triage the first divergences

**Files:**
- Modify: record §G.20
- Modify (only in the stall/fault branch): `Makefile` (`GP_IDLE_LOSS_END`)

- [ ] **Step 1: Replay and report**

```bash
S=/tmp/gameplay-u4
make gp-report scenario=gp-idle-loss 2>&1 | tee $S/report1.txt | grep -E 'gp_compare|FAIL|all checks|stalled|fault'
tail -3 /tmp/pr_gp_dump/gp-idle-loss/gp.log
du -sh /tmp/pr_gp_dump/gp-idle-loss
```

Expected: the driver's `all checks passed` (the Enter arm, every key, the end reached), then the two claims' lines. Record them verbatim.

**Branch — the replay stalled or faulted** (`FAIL … stalled at f=…`, `fault …`, or `the gp script ran to its end frame` failed): record the last `T` line of `trace.txt` and the `gp.log` tail. Cut the script at the last frame the port completed before the stall (`F = the last T line's f`), a harness value with that evidence:

```make
# gp-idle-loss replay cut (record §G.20): the port <stalls|faults> at f=<F+1> (<evidence>); the
# script ends at the last completed frame. Remove when the follow-up unit fixes it.
GP_IDLE_LOSS_END = <F>
```

and in `gp-oracle` pass it: `@$(MAKE) --no-print-directory gp-replay scenario=gp-idle-loss GP_SCRIPT_ARGS="$(if $(GP_IDLE_LOSS_END),--end $(GP_IDLE_LOSS_END))"`; rerun the report with `GP_SCRIPT_ARGS="--end <F>"` and continue from its lines.

- [ ] **Step 2: Triage the first unexplained capture frame**

From the `FIRST UNEXPLAINED capture <j> (raw <r>): nearest port <m>, rows …, x … (… px)` line:

```bash
M=<m>; J=<j>
sed -n "$((M+1))p" /tmp/pr_gp_dump/gp-idle-loss/frames.txt
python3 - <<EOF
import sys; sys.path.insert(0, 'tools')
import gp_compare as gc
from PIL import Image
c = gc.load_capture_frame('data/k11-captures/gp-idle-loss/frame_%05d.raw.gz' % $J)
p = gc.load_port_frame('/tmp/pr_gp_dump/gp-idle-loss/frame_%05d.ipx' % $M)
Image.frombytes('RGB', (320, 200), c).save('/tmp/gameplay-u4/cap_$J.png')
Image.frombytes('RGB', (320, 200), p).save('/tmp/gameplay-u4/port_$M.png')
EOF
```

Look at both PNGs. Record in §G.20: the capture frame, the port frame's `f` and mode (from `frames.txt`), what differs (the box and what it shows), and the owning subsystem with its evidence (a raw address, a trace field, or a named gap the ledger already lists). Do not fix it.

- [ ] **Step 3: Triage the first trace difference**

From `first difference f=X (…) in <field>: capture …, port …`:

```bash
X=<f hex>
grep -n " f=$X " data/k11-captures/gp-idle-loss/poll.log | head -2
grep -n "^T f=$X " /tmp/pr_gp_dump/gp-idle-loss/trace.txt
```

Record both lines, the three frames before, the mode at `X`, and whether Task 3's run-to-run difference comes first. The `tick` line: record the first tick difference and whether any `TRACE_FIELDS` difference follows it within 60 frames (the evidence for spec §7 Q6).

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md Makefile
git commit -m "$(cat <<'EOF'
docs: gp-idle-loss port replay: the first frame and trace divergences named (record §G.20)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

(Stage `Makefile` only if the branch changed it.)

---

### Task 5: Pin the ratchets and prove they fail

**Files:**
- Modify: `Makefile` (`GP_IDLE_LOSS_MIN_FIRST`, `GP_IDLE_LOSS_TRACE_MIN_FIRST` with provenance comments)
- Modify: record §G.21

- [ ] **Step 1: The values**

- Frame N = the first unexplained capture frame `j` of Task 4 (if none: N = the number of capture frames, the exact pin, as the demo-fight rule).
- Trace F = the first differing `f` of Task 4 (decimal), but not above Task 3's first run-to-run difference: `F = min(port difference, run-to-run difference)`; if neither exists, F = the port's last `T` frame + 1.

Write them with their provenance:

```make
# Measured at <commit> (record §G.21): first unexplained capture frame <j> (raw <r>), <one-line
# cause>; raise it when the frame claim improves (gp_compare prints "improved: raise N").
GP_IDLE_LOSS_MIN_FIRST = <j>
# Measured at <commit> (record §G.21): first differing f=<X hex> (<field>) <port|run-to-run>;
# raise it when the trace claim improves.
GP_IDLE_LOSS_TRACE_MIN_FIRST = <F>
```

- [ ] **Step 2: Green**

```bash
S=/tmp/gameplay-u4
make gp-oracle > $S/o.txt 2>&1; echo "exit=$?"; grep gp_compare $S/o.txt
```

Expected: both `ratchet N … ok` lines, `exit=0`.

- [ ] **Step 3: Each ratchet can fail**

```bash
S=/tmp/gameplay-u4
make gp-oracle GP_IDLE_LOSS_MIN_FIRST=$(( <j> + 1 )) > $S/o1.txt 2>&1; echo "exit=$?"; grep FAIL $S/o1.txt
make gp-oracle GP_IDLE_LOSS_TRACE_MIN_FIRST=$(( <F> + 1 )) > $S/o2.txt 2>&1; echo "exit=$?"; grep FAIL $S/o2.txt
rm -rf $S/dmg; cp -R /tmp/pr_gp_dump/gp-idle-loss $S/dmg
python3 - <<'EOF'
import re
lines = open('/tmp/gameplay-u4/dmg/frames.txt').read().splitlines()
k = len(lines) // 4          # a port frame well inside the explained region (below N)
p = '/tmp/gameplay-u4/dmg/frame_%05d.ipx' % k
d = bytearray(open(p, 'rb').read()); d[32000] ^= 0xFF; open(p, 'wb').write(d)
print('damaged', p)
EOF
python3 tools/gp_compare.py --scenario gp-idle-loss --capture data/k11-captures/gp-idle-loss --port $S/dmg --min-first <j> --trace-min-first <F>; echo "rc=$?"
```

Expected: `FAIL: first unexplained <j> < ratchet N <j+1>` (or `N … > end` when N was the exact pin) and `exit=2` from make; the same for the trace; the damaged dump gives a `FIRST UNEXPLAINED` below `<j>` and `rc=1`. If the damaged frame is not exhibited by any capture frame below `<j>` (the port frame index `k` may fall in a stretch the capture skips), pick `k` from the `nearest port` of a clean capture frame instead and record which. Record all outputs in §G.21.

- [ ] **Step 4: The full gate**

```bash
make verify > $S/t5_verify.txt 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|k11_compare)' $S/t5_verify.txt | diff - /tmp/gameplay-u1/or_base.txt && echo ORACLES-EQUAL
grep gp_compare $S/t5_verify.txt
git diff --stat 934992a -- port/src
```

Expected: `verify-exit=0`, `ORACLES-EQUAL`, the two ratchet lines, an empty `port/src` diff.

- [ ] **Step 5: Commit**

```bash
git add Makefile docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
build: pin the gp-idle-loss frame and trace ratchets in make verify (record §G.21)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: U4 closure

**Files:**
- Modify: record §G.22 (closure) and §G.23 (named gaps); `docs/PROGRESS.md`; `AGENTS.md` (the N values' pointer, one line)

- [ ] **Step 1: Record**

§G.22: the path table (capture against port, per mode: the capture's first `f`, the port's first `f`, first unexplained frame and first trace difference marked), the answers to spec §7 Q1 (Task 3), Q6 (Task 4 Step 3), Q7 (the sizes), and the narrow claim (U3 Task 5's text with the pinned values). §G.23: every divergence named in Task 4 as a named gap with its evidence and the unit expected to own it (U5 character select, U6 moves, U9/U10 later, or a new one), plus "Not covered": each mode of spec §3.5 past the frame N.

- [ ] **Step 2: PROGRESS and AGENTS**

Append a `docs/PROGRESS.md` paragraph "Gameplay U4: the first scripted run (idle loss) …" with the two N values, the determinism answer and the first divergences. In `AGENTS.md`'s gameplay-oracle sentence (U3 Task 5) add "The current N values and their provenance are in the Makefile."

- [ ] **Step 3: Final gate and commit**

```bash
make verify > /tmp/gameplay-u4/t6_verify.txt 2>&1; echo "verify-exit=$?"
git add docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md docs/PROGRESS.md AGENTS.md
git commit -m "$(cat <<'EOF'
docs: gameplay U4 closure: the idle-loss run, its ratchets and named gaps (record §G.22-§G.23)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review

- Spec §4.4 coverage: the scenario (Task 1), two captures and the determinism answer (Tasks 2–3), the replay and triage (Task 4), the pinned ratchets with failure proofs (Task 5), the record and PROGRESS (Task 6). The acceptance line "the capture reproduces §3.5's mode path (or the record says where and why it differs)" is Task 2 Step 2.
- Names: `SCENARIOS['gp-idle-loss']`, `SCENARIOS['gp-idle-loss-run2']`, `port_script(name, lines, end=None)`, `--end`, `GP_SCRIPT_ARGS`, `GP_IDLE_LOSS_END`, `GP_IDLE_LOSS_MIN_FIRST`, `GP_IDLE_LOSS_TRACE_MIN_FIRST` — consistent with U1–U3 (`make gp-capture`, `gp-replay`, `gp-report`, `gp-oracle`).
- Task 1 Step 3 renames `port_script`'s local `X` record to `xrec` so the new parameter can be `end`, the name the test uses.
</content>
</invoke>
