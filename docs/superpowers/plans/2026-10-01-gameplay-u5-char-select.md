# Gameplay U5 — Character-Select Walk and Divergence 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** (1) Diagnose and fix divergence 1 (spec O1: the character-select idle animation "turns ~96 frames late") behind an explicit evidence gate, and raise the `gp-idle-loss` frame ratchet to the measured new first unexplained frame. (2) Capture the pinned original through START MENU → LEFT PLAYER ARCADE → character select, where a frame-keyed script walks P1's cursor over all seven cells and picks cell 1 before the pick time-out, through the versus/countdown modes and the round start to the first frame of mode 6; replay it in the port; pin a new frame and trace ratchet in `make verify`.

**Architecture:** Divergence 1 is one operand in the hand-ported idle-animation tick `0x37A58` (`port/src/game/actors.c`): the raw's top wrap compares the frame byte `rec+0x52` (`0x37B03 mov edx,[ebx+0x4f]; sar edx,0x18`), the port compares `rec+0x4F`; Task 1 re-runs the three discriminating measurements as a gate, Task 2 fixes it with a failing-first unit test. The walk is one new scenario, `gp-u5-charsel`, in `tools/gp_session.py`, captured with U1's `make gp-capture`, replayed with U2's `make gp-replay`, compared with U3's `tools/gp_compare.py` through a new `gp-charsel-oracle` target that mirrors `gp-oracle`.

**Tech Stack:** C (the port), Python 3 stdlib (`tools/gp_*.py`, `unittest`), `unicorn` 2.1.4 + `capstone` 5.0.7 (`tools/diff_emu.py`, already required by `make diff-verify`), DOSBox-X 2026.08.31 (the capture), `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §2 (O1), §4 track G ("O1 … fixed in the owning unit … or restated as a named gap with evidence"), §6 (G exit), §8; `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md` §3.1 (the loop), §3.2 (the pad bitmap), §3.3 (START MENU), §3.6 (key latency, Q3), §4.1 (the scenario language), §4.3 (the ratchets), §5.

**Derivation record:** `docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md` — §C5.1-§C5.7 are the planner's raw evidence (every number there was produced by a command that was run); the executor appends §C5.10-§C5.19, one per task. Cited below as "record §C5.n"; the shared U1-U4 record `2026-09-30-gameplay-ground-truth-derivations.md` is cited as "record §G.n" and is **not** edited by this unit.

---

## Decisions needed from the user

**Decided by the user on 2026-10-01:** all recommendations accepted — (1) storage for the capture of about 55 MB (at most 170 MB) approved; (2) U5 merges before U6, and whichever merges second re-measures the shared idle-loss pins and miss set rather than resolving numbers by hand; (3) the capture ends at the first frame of mode 6.

1. **Storage for the `gp-u5-charsel` capture** (spec §7 Q7). Expected ≈ 55 MB gzipped, bound 170 MB (record §C5.7: U4's own frames over the same 60 s of DOSBox are 53.5 MB; the bound assumes every AVI frame distinct), in the git-ignored `data/k11-captures/gp-u5-charsel/`, plus ≈ 23 MB of port dump under `/tmp` per `make verify`. **Recommendation: yes** — it is a tenth of the 0.3–0.6 GB that Q7's ruling (record §G.18) already accepted for `gp-idle-loss`. **Task 6 (the capture) and everything after it wait for this answer.** Cost of the wrong answer: none for Tasks 0–5 (the fix and the tooling stand alone); a "no" leaves the walk unratcheted (a named gap).
2. **U5 re-pins `gp-idle-loss`** (Task 2 removes the `0x3640C anim_indirect` row from `k_miss_gp_idle_loss`, Task 3 raises `GP_IDLE_LOSS_MIN_FIRST` 203 → the measured value, 787 in the planner's run). U6 (moves) owns divergence 2 and will touch the same set (`0x23208`, `0x3A588`) and the trace N. **Recommendation: merge U5 before U6**, and have whichever branch merges second re-run `make gp-oracle` and re-measure (never hand-merge the numbers). Cost of the wrong answer: a red `make verify` after the second merge until the values are re-measured; no wrong value can be shipped because the miss-set count and the ratchet both fail.
3. **Where the walk's capture ends.** **Recommendation: the first frame of mode 6** (`('until_mode', 0x06, 0)`): the round's own divergences (`0x3A588`, `0x23208`, record §G.24) belong to U6 and would otherwise become U5's first unexplained frame. Cost of the wrong answer: running on into round 1 pins a fight divergence in U5's ratchet and adds ≈ 45 KB per capture frame.

## Global Constraints

- AGENTS.md (evidence discipline): "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- AGENTS.md: "The byte-exact oracle lines are the regression gate and must not move; run `make verify` after any change that can affect rendering, timing or RNG." The gate for every task: `make verify` exit 0; the 45 lines of `grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>` equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; `make audio-render` byte-identical (`cmp`) to `.superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav`; the `k11_compare:` lines equal Task 0's; `python3 tools/port_progress.py` prints `771 1203 64` and `731 731 100` (U5 adds no `FN_` function: `0x37A58` has no symbol, record §C5.3). The only oracle values U5 changes are the `gp-idle-loss` frame ratchet (raised, Task 3) and its miss set (one row, Task 2).
- AGENTS.md (gp oracle): "a ratchet value is a measured value … raise N when it improves"; "Never re-capture to 'refresh' a pin: capture to a new scenario name, measure it, and pin its own `poll.log` hash" (record §G.24 item 5).
- AGENTS.md (porting): "One C function per original function, header comment `/* 0xADDR — spec section */`. Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment styles in `port/src`." / "`port/src/symbols.h` is generated … Never hand-edit it."
- AGENTS.md (tests): "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." / "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests, and never assert an unseeded BSS-zero." / "`game_init()` may run only once per process." Python tests: `unittest` in `tools/tests/`, imported with `sys.path.insert(0, ROOT/tools)`.
- AGENTS.md: "Every env-gated driver … arms `fn_resolve`'s miss log and must record exactly the pinned known-set … A port that makes a driver reach a new unregistered code pointer fails it: register the target or pin the miss with its evidence."
- AGENTS.md: "Captures are git-ignored. `data/` is git-ignored and read-only" — U5 writes only `data/k11-captures/gp-u5-charsel/` through `make gp-capture`. Never `pkill`.
- Spec gameplay §5: "Harness values (`HOLD_FRAMES`, the 150-frame gaps, `time_limit`, …) are named as harness values with their source, never presented as game values."
- AGENTS.md: "Commit style: `<area>: <what changed>`. **Never `git add -A`**." Trailer: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Append to `docs/PROGRESS.md`, never rewrite it.

## Review Focus

1. **A fix accepted on weaker evidence than the gate.** Task 1's gate requires all three measurements (raw listing, the original's bytes under E1, the port trace) to match record §C5.3 verbatim; Task 3 then requires the `gp-idle-loss` frame ratchet to move past the character select. Checked by Task 1 Step 5 (the gate table) and Task 3 Step 3 (`gp-oracle` fails at the old operand).
2. **A new assertion that cannot fail.** Tick C seeds `rec+0x52 = 0x1D` and expects 0; Tick D seeds `rec+0x4F = 0x20` and expects 4. Task 2 Step 2 shows both failing on the unfixed operand (`30 != 0`, `0 != 4`).
3. **A key script that does not do what it claims.** `TestCharsel` (Task 4) models `0x43B24`'s stick on the scenario and fails under three named mutations; Task 5 replays the same walk in the port (cursor `0 0 1 2 3 6 5 4 0 1`, confirm on 1); Task 6 Step 2 checks the capture's own `e0` edges.
4. **A miss set or a ratchet pinned where it cannot fail.** Task 5 shows the replay failing without the `gp-u5-charsel` set; Task 8 shows `gp-charsel-oracle` failing at N+1, F+1, a wrong `poll.log` hash, and skipping cleanly without the capture.
5. **A merge that silently breaks another unit's pins.** Every shared-file edit is listed under "Shared-file touch points"; Task 9 re-runs the full gate and records the `gp-idle-loss` values it leaves for U6.

## Where to run

A worktree `.worktrees/gameplay-u5`, branch `gameplay-u5` off `main`. One-time setup (Task 0 Step 1):

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse
git worktree add .worktrees/gameplay-u5 -b gameplay-u5 main
cd .worktrees/gameplay-u5
ln -s /Users/felipe.dos.santos/code/mine/primal-rage-reverse/data data
ln -s /Users/felipe.dos.santos/code/mine/primal-rage-reverse/.superpowers .superpowers
cp ../../port/tests/ghidra_data.bin ../../port/tests/title_screen_ref.ppm port/tests/
```

Every command below runs from the worktree root, and every command block starts with `S=/tmp/gameplay-u5` (each shell is fresh). Every `make` that dumps or renders goes through a wrapper that appends the parallel-safe overrides (a wrapper, not a `$VO` string: the host shell is zsh, which does not word-split an unquoted variable), created once in Task 0 Step 1:

```bash
S=/tmp/gameplay-u5; mkdir -p $S
cat > $S/mkv <<'EOF'
#!/bin/sh
# U5: make with the parallel-safe overrides (the 2026-10-01 planning brief's list, u5 names).
exec make "$@" SMK_DUMP=/tmp/pr_u5_smk TITLE_DUMP=/tmp/pr_u5_title ATTRACT_DUMP=/tmp/pr_u5_att FRONTEND_DUMP=/tmp/pr_u5_fe TITLE_PIN_DIR=/tmp/pr_u5_pin AUDIO_WAV=/tmp/pr_u5.wav K11_DUMP=/tmp/pr_u5_k11 GP_DUMP=/tmp/pr_u5_gp DIFF_IMAGE=/tmp/pr_u5_diffimg DIFF_TABLE=/tmp/pr_u5_diff.md
EOF
chmod +x $S/mkv
```

(`$S/mkv -n gp-oracle GP_IDLE_LOSS_MIN_FIRST=788` was checked by the planner: `--port /tmp/pr_u5_gp/gp-idle-loss`, `min-first "788"`.)

The gate check after a verify run (used by every task; `$L` is the log):

```bash
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $L > $S/oracles.txt
diff $S/oracles.txt .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
grep -E '^k11_compare:' $L | diff - $S/k11_base.txt && echo K11-EQUAL
$S/mkv audio-render >/dev/null && cmp /tmp/pr_u5.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-IDENTICAL
python3 tools/port_progress.py
```

## Shared-file touch points

All additive except the two marked **(changes a shared value)**; the controller sequences them (Decision 2).

| file | region | what |
|---|---|---|
| `port/src/game/actors.c` | `anim_code_37A58` (lines 1358–1395 at `e9271df`), line 1387 and the header comment | the `0x37B03` operand `0x4f → 0x52` (Task 2) |
| `port/tests/test_game.c` | `check_idle_tick_37a58` (4705–4759), appended after line 4758 | Tick C and Tick D (Task 2) |
| `port/tests/test_platform.c` | the `k_miss_gp_idle_loss` comment and table (107–127) | **(changes a shared value)** remove the `0x3640C anim_indirect` row and its comment line (Task 2) |
| `port/tests/test_platform.c` | after `k_miss_gp_idle_loss`; `fnm_known` (163–168) gains a `charsel` parameter; `test_fn_misslog_driver` (171–193); the `--check` call (269) | new `k_miss_gp_charsel[]` (Task 5). U7/U8 adding their own scenario sets edit the same three spots: merge by keeping every flag and every table |
| `Makefile` | the `GP_IDLE_LOSS_MIN_FIRST` comment and value (426–432) | **(changes a shared value)** 203 → the measured value (Task 3) |
| `Makefile` | new block before `gp-report:` (line 460); `.PHONY` (line 56) gains `gp-charsel-oracle`; `verify` gains one line after `@$(MAKE) --no-print-directory gp-oracle` (line 511) | `GP_CHARSEL_*` and `gp-charsel-oracle` (Task 8) |
| `tools/gp_session.py` | after `SCENARIOS['gp-idle-loss-run2']` (line 126) | `CHARSEL_WALK`, `SCENARIOS['gp-u5-charsel']` (Task 4) |
| `tools/tests/test_gp_session.py` | appended at the end (after line 250) | `class TestCharsel` (Task 4) |
| `docs/PROGRESS.md` | appended | one paragraph (Task 9) |

---

### Task 0: Baseline

**Files:**
- Modify: `docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md` (the record; it reaches `main` with this plan — if `git ls-files docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md` prints nothing, copy it from the planning branch `reverse-plans` with `git show reverse-plans:docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md > docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md`), new section `§C5.10 Baseline`

**Interfaces:**
- Produces: `$S/base_verify.txt`, `$S/k11_base.txt`, the baseline `gp-oracle` lines

- [ ] **Step 1: Setup** — the "Where to run" commands, including the `$S/mkv` wrapper; `git log --oneline -1` (record the base commit).

- [ ] **Step 2: Baseline gate**

```bash
S=/tmp/gameplay-u5; $S/mkv verify > $S/base_verify.txt 2>&1; echo "verify-exit=$?" >> $S/base_verify.txt
grep -E '^k11_compare:' $S/base_verify.txt > $S/k11_base.txt
grep -E 'gp_compare: gp-idle-loss: (frames: first unexplained|trace: first differing)|fn-miss PR_GP_DUMP .*distinct' $S/base_verify.txt
```

Expected: `verify-exit=0`; `first unexplained 203, ratchet N 203 ok`; `first differing 2088, ratchet N 2088 ok`; `fn-miss PR_GP_DUMP distinct=2` (gp-pads) and `distinct=7` (gp-idle-loss). Then run the gate check (`L=$S/base_verify.txt`; the `k11` diff is trivially equal here): `ORACLES-EQUAL`, `WAV-IDENTICAL`, `771 1203 64` / `731 731 100`. If any differs, stop: the base is not the one this plan was measured on.

- [ ] **Step 3: Record and commit** — write §C5.10 (base commit, the lines above).

```bash
git add docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md
git commit -m "$(cat <<'EOF'
docs: gameplay U5 record and baseline (record §C5.10)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 1: Divergence 1 — the discriminating measurements and the gate

No source change is committed. Three measurements, each predicted verbatim by record §C5.3; the gate decides between Task 2 (fix) and Task 2-alt (named gap).

**Files:**
- Modify: record, `§C5.11 The gate`
- Scratch only: `$S/img.bin`, `$S/orig_idle_tick.py`, `$S/instr_at.py`, `$S/at_summary.py`, `$S/t1/` (a throw-away clone)

**Interfaces:**
- Consumes: `tools/diff_emu.py` (`Image.load`, `run_original`), `build/diffrun --image-out`, the `PR_GP_DUMP` driver
- Produces: the gate decision in §C5.11

- [ ] **Step 1: The raw** (evidence (i))

```bash
S=/tmp/gameplay-u5; cmake -S port -B build >/dev/null && cmake --build build -j8 | tail -1
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out $S/img.bin
python3 - <<'EOF'
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
I = open('/tmp/gameplay-u5/img.bin', 'rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_32)
for i in md.disasm(I[0x37A9B - 0x10000:0x37B24 - 0x10000], 0x37A9B):
    print('%06X  %-16s %s %s' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
EOF
```

Expected (among the lines): `037B03  8b534f           mov edx, dword ptr [ebx + 0x4f]`, `037B08  c1fa18           sar edx, 0x18`, `037B0E  39c2             cmp edx, eax`, `037B12  c6435200         mov byte ptr [ebx + 0x52], 0` — and `port/src/game/actors.c:1387` reads `if ((s32)(s8)DSB(rec + 0x4fu) >= (s32)DSB(rec + 0x4du))`.

- [ ] **Step 2: The original's bytes on the discriminating input** (evidence (ii))

```bash
cat > $S/orig_idle_tick.py <<'EOF'
# Runs the ORIGINAL bytes of 0x37A58 (the idle-animation tick) under tools/diff_emu.py on the
# fixed-up image (build/diffrun --image-out). Read-only; prints rec+0x52 after one tick.
import sys
sys.path.insert(0, sys.argv[1] + "/tools")
import diff_emu as de
img = de.Image.load(sys.argv[2])
REC = 0x10A000
def tick(frame, d, kids=0, count=5, n4d=0x1E, mode=3):
    b = bytearray(14)                       # rec+0x4B .. rec+0x58
    b[1], b[2], b[4], b[7], b[13] = count, n4d, kids, frame, d
    r = de.run_original(img, 0x37A58, regs={"eax": REC, "edx": 0},
                        pokes={REC + 0x4B: bytes(b), 0x104B00: mode.to_bytes(2, "little")},
                        allow_calls=(0x5D7DC,))
    return r.outcome, r.writes.get(REC + 0x52, frame)
for name, args in [("top 0x1D +1, 0x4F=0", (0x1D, 1)),
                   ("kids 3 +1, 0x4F=0x20", (3, 1, 0x20)),
                   ("mid 0x10 +1, 0x4F=0", (0x10, 1)),
                   ("bottom 0 -1", (0, 0xFF))]:
    out, v = tick(*args)
    print("%-24s outcome=%s rec+0x52 -> 0x%02X" % (name, out, v))
EOF
python3 $S/orig_idle_tick.py . $S/img.bin
```

Expected, verbatim:

```
top 0x1D +1, 0x4F=0      outcome=ok rec+0x52 -> 0x00
kids 3 +1, 0x4F=0x20     outcome=ok rec+0x52 -> 0x04
mid 0x10 +1, 0x4F=0      outcome=ok rec+0x52 -> 0x11
bottom 0 -1              outcome=ok rec+0x52 -> 0x1D
```

(The port's values for "top"/"kids", `0x1E`/`0`, are shown by Task 2 Step 2's failing assertions.) Without `unicorn`/`capstone` (`pip install -r tools/requirements-diff.txt`) this step cannot run: stop and report — the gate needs it.

- [ ] **Step 3: The port's character-select fighter, current against fixed** (evidence (iii), the frame)

```bash
cat > $S/instr_at.py <<'EOF'
# Throw-away instrumentation for U5 Task 1 (never committed). Run from the root of a SCRATCH
# clone: prints P1's character-select fighter record each mode-0x10 frame (AT lines on stderr
# when PR_AT is set) and makes 0x37A58's wrap compare read rec+0x52 when PR_U5_FIX is set.
import re
p = 'port/src/game/flow.c'
s = open(p).read()
a = "    actors_update();                                   /* 0x2A31C */\n"
assert s.count(a) == 1
s = s.replace(a, a + '''    if (getenv("PR_AT") && DSW(DS_00104B00) == 0x10u && DSD(DS_0010813C) != 0u) {
        u32 r = DSD(DS_0010813C);
        fprintf(stderr, "AT f=%04X id=%04X v52=%02X v58=%02X c4c=%02X\\n", DSW(DS_000EF6DC),
            DSW(DSD(DS_001014EC) + DSW(r + 0x56) * 0x20u), DSB(r + 0x52), DSB(r + 0x58), DSB(r + 0x4c));
    }
''')
if '#include <stdlib.h>' not in s:
    s = '#include <stdlib.h>\n' + s
open(p, 'w').write(s)
p = 'port/src/game/actors.c'
s = open(p).read()
a = "    if ((s32)(s8)DSB(rec + 0x4fu) >= (s32)DSB(rec + 0x4du))\n"
assert s.count(a) == 1, 'run on the unfixed source'
s = s.replace(a, "    if ((s32)(s8)DSB(rec + (getenv(\"PR_U5_FIX\") ? 0x52u : 0x4fu)) >= (s32)DSB(rec + 0x4du))\n")
if '#include <stdlib.h>' not in s:
    s = '#include <stdlib.h>\n' + s
open(p, 'w').write(s)
print('instrumented')
EOF
cat > $S/at_summary.py <<'EOF'
# Summarise an AT trace (instr_at.py): the first frame rec+0x52 leaves 0..0x1D, the max, the
# direction flips (rec+0x58), and the first frame the two traces differ.
import sys
def load(p):
    out = []
    for l in open(p):
        d = dict(x.split('=') for x in l.split()[1:])
        out.append((int(d['f'], 16), int(d['v52'], 16), d['v58'], d['id']))
    return out
a, b = load(sys.argv[1]), load(sys.argv[2])
for name, t in (('current', a), ('fixed', b)):
    out = [(f, v) for f, v, _, _ in t if v > 0x1D]
    flips = [(hex(f), d) for (f, _, d, _), (_, _, d0, _) in zip(t[1:], t) if d != d0]
    print('%s: %d frames, max v52 %X, first out of 0..1D %s, flips %s'
          % (name, len(t), max(v for _, v, _, _ in t), hex(out[0][0]) + ' id ' + [i for f, v, _, i in t if f == out[0][0]][0] if out else 'none', flips))
d = next(((x, y) for x, y in zip(a, b) if x != y), None)
print('first difference:', d and 'f=%X current v52=%X fixed v52=%X' % (d[0][0], d[0][1], d[1][1]))
EOF
M=/Users/felipe.dos.santos/code/mine/primal-rage-reverse
rm -rf $S/t1 && git clone -q --local . $S/t1 && (cd $S/t1 && ln -s $M/data data \
  && cp $M/port/tests/ghidra_data.bin $M/port/tests/title_screen_ref.ppm port/tests/ \
  && python3 $S/instr_at.py && cmake -S port -B build >/dev/null && cmake --build build -j8 | tail -1)
printf '# gp port script v2: scenario gp-idle-loss (cut at 1120)\nenter_frame 321\nenter_state 0000\nkey 321 1C 0D\nkey 474 1C 0D\nkey 622 1C 0D\nend 1120\n' > $S/t1.script
(cd $S/t1 && PR_AT=1 PR_GP_DUMP=$S/t1_cur PR_GP_SCRIPT=$S/t1.script PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep '^AT' > $S/t1_cur.txt)
(cd $S/t1 && PR_U5_FIX=1 PR_AT=1 PR_GP_DUMP=$S/t1_fix PR_GP_SCRIPT=$S/t1.script PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep '^AT' > $S/t1_fix.txt)
python3 $S/at_summary.py $S/t1_cur.txt $S/t1_fix.txt
```

(The script's three keys are `gp-idle-loss`'s consumption frames, record §G.19 run 1; the cut at 1120 = `0x460` covers the character select through the second turn.) Expected, verbatim:

```
current: 462 frames, max v52 3B, first out of 0..1D 0x340 id AD78, flips [('0x33d', '01'), ('0x39a', 'FF'), ('0x454', '01')]
fixed: 462 frames, max v52 1D, first out of 0..1D none, flips [('0x33d', '01'), ('0x39a', 'FF'), ('0x454', '01')]
first difference: f=340 current v52=1E fixed v52=0
```

`$S/t1` is scratch; it is never committed and is deleted in Task 9.

- [ ] **Step 4: The candidates** — copy record §C5.1's table into §C5.11 with this run's outputs beside each prediction (C1–C4 refuted by the equal flips and the value outside `0..0x1D`; C5 confirmed by the first difference at `f = 0x340`, the frame of U4's first unexplained capture frame 203, record §G.20).

- [ ] **Step 5: The gate** — write in §C5.11:

| evidence | required | observed |
|---|---|---|
| (i) raw | `0x37B03 mov edx,[ebx+0x4f]` + `0x37B08 sar edx,0x18` | Step 1 |
| (ii) original's bytes | `top … -> 0x00`, `kids … -> 0x04` | Step 2 |
| (iii) port frame | `first difference: f=340 current v52=1E fixed v52=0` | Step 3 |

All three as expected → **go to Task 2** (Task 3 adds the capture-level evidence). Any one different → **go to Task 2-alt** and skip Tasks 2–3.

- [ ] **Step 6: Commit**

```bash
git add docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md
git commit -m "$(cat <<'EOF'
docs: divergence 1 isolated to the 0x37B03 operand of 0x37A58 (record §C5.11)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Fix the idle-animation wrap (`0x37B03`)

**Files:**
- Modify: `port/src/game/actors.c` (`anim_code_37A58`, header comment 1358–1366 and line 1387)
- Modify: `port/tests/test_game.c` (`check_idle_tick_37a58`, after line 4758)
- Modify: `port/tests/test_platform.c` (lines 107–127: the `k_miss_gp_idle_loss` comment and table)
- Modify: record `§C5.12`

**Interfaces:**
- Consumes: `tf_anim_alloc_record`, `actor_sync`, `ANIM_SCRATCH` (already used by `check_idle_tick_37a58`)
- Produces: no new symbol; `anim_code_37A58` matches the raw's top wrap

- [ ] **Step 1: The failing assertions.** In `port/tests/test_game.c`, inside `check_idle_tick_37a58`, after `    CHECK_EQ_INT((int)DSB(rec + 0x52), draw != 0u ? 6 : 4);` and before the closing `}`, insert:

```c

    /* Tick C: counting up past the last frame wraps to 0. 0x37B03 loads the
     * dword at rec+0x4F and 0x37B08 `sar edx,0x18` keeps its top byte, which is
     * rec+0x52 (0x4F + 3), the frame just stored at 0x37AAB; 0x37B0E/0x37B10
     * zero it when that signed byte is >= rec+0x4D (record gameplay-u5 §C5.2).
     * rec+0x4F (the child count) is 0 here, so a port that compares rec+0x4F
     * keeps 0x1E, a sprite id past the character's 0x1E idle frames. */
    DSB(rec + 0x4c) = 5;
    DSB(rec + 0x52) = 0x1d;
    DSB(rec + 0x58) = 1;
    DSB(rec + 0x4f) = 0;
    DSD(rec + 8) = ANIM_SCRATCH;
    DSD(rec + 0x20) = 0;
    DSD(rec + 0x24) = 0x3f800000u;
    DSW(rec + 0x28) = 0;
    DSW(rec + 0x2a) = 0;
    actor_sync(rec);
    CHECK_EQ_INT((int)DSB(rec + 0x52), 0);           /* 0x1E >= 0x1E: wrapped */

    /* Tick D: the child count takes no part. rec+0x4F = 0x20 (>= rec+0x4D)
     * and rec+0x52 = 3 counting up: the frame is 4, not reset. */
    DSB(rec + 0x4c) = 5;
    DSB(rec + 0x52) = 3;
    DSB(rec + 0x58) = 1;
    DSB(rec + 0x4f) = 0x20;
    DSD(rec + 8) = ANIM_SCRATCH;
    DSD(rec + 0x20) = 0;
    DSD(rec + 0x24) = 0x3f800000u;
    DSW(rec + 0x28) = 0;
    DSW(rec + 0x2a) = 0;
    actor_sync(rec);
    CHECK_EQ_INT((int)DSB(rec + 0x52), 4);
    DSB(rec + 0x4f) = 0;
```

(The seeds differ from the post-conditions: Tick C seeds `0x1D`, expects 0, a written zero; Tick D seeds `rec+0x4F = 0x20`.)

- [ ] **Step 2: Run to see it fail**

Run: `cmake --build build -j8 | tail -1 && PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep -E 'FAIL|FAILURES|passed'`
Expected: `FAIL …/port/tests/test_game.c:4776: 30 != 0`, `FAIL …/port/tests/test_game.c:4790: 0 != 4`, `FAILURES: 2` (line numbers as measured on `e9271df` plus this insertion).

- [ ] **Step 3: The fix.** In `port/src/game/actors.c` replace

```c
    if ((s32)(s8)DSB(rec + 0x4fu) >= (s32)DSB(rec + 0x4du))
```

with

```c
    if ((s32)(s8)DSB(rec + 0x52u) >= (s32)DSB(rec + 0x4du)) /* 0x37B03: [rec+0x4F] >> 24 */
```

and in the header comment of `anim_code_37A58` replace its last line

```c
 * skips). The dispatcher passed EAX=rec; the arg is ignored. */
```

with

```c
 * skips). The dispatcher passed EAX=rec; the arg is ignored. The top wrap
 * reads the frame itself: 0x37B03 loads the dword at rec+0x4F and 0x37B08
 * `sar edx,0x18` keeps its top byte, rec+0x52 (record gameplay-u5 §C5.2). */
```

- [ ] **Step 4: Run to see it pass**

Run: `cmake --build build -j8 | tail -1 && PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -1`
Expected: `all checks passed`.

- [ ] **Step 5: The gp-idle-loss miss set** (record §C5.3: the replay no longer reaches `0x3640C`)

Run: `$S/mkv gp-replay scenario=gp-idle-loss 2>&1 | grep -E 'fn-miss|FAIL|passed'`
Expected: `fn-miss PR_GP_DUMP distinct=6 dropped=0`, `FAIL …test_platform.c:185: 6 != 7` (line 185 at `e9271df`: `else CHECK_EQ_INT(fn_misslog_count(), want);`).
Then in `port/tests/test_platform.c` delete the table row `    { 0x3640Cu, "anim_indirect" },` and replace the comment lines

```c
 * (measured on the merged base, the full replay to f = 0x207F) adds five
 * pairs, each classified from the raw in §G.24:
```

with

```c
 * (measured on the merged base, the full replay to f = 0x207F) adds four
 * pairs, each classified from the raw in §G.24:
```

and the line ` *   0x3640C anim_indirect: an UNPORTED animation-opcode target, f = 0x173A.` with

```c
 *   (U4 measured a fifth, 0x3640C anim_indirect, an UNPORTED animation-opcode
 *   target at f = 0x173A, past the trace divergence; since the 0x37B03 fix the
 *   replay no longer reaches it, record gameplay-u5 §C5.3/§C5.12.)
```

Re-run the same command. Expected: `distinct=6`, `all checks passed`.

- [ ] **Step 6: Gate**

```bash
L=$S/t2_verify.txt; $S/mkv verify > $L 2>&1; echo "verify-exit=$?" >> $L; tail -1 $L
grep -E 'gp_compare: gp-idle-loss: (frames: (FIRST UNEXPLAINED|first unexplained)|trace: first differing)' $L
```

Expected: `verify-exit=0`; `FIRST UNEXPLAINED capture 787 (raw 3899): nearest port 575, rows 147..199, x 0..319 (11357 px)`; `first unexplained 787, ratchet N 203 ok (improved: raise N)`; `first differing 2088, ratchet N 2088 ok`. Then the gate check (`ORACLES-EQUAL`, `K11-EQUAL`, `WAV-IDENTICAL`, `771 1203 64`, `731 731 100`). If the frame number differs from 787, the run's value is the one recorded and pinned in Task 3 (the raw wins over this plan).

- [ ] **Step 7: Record and commit** — §C5.12: the outputs of Steps 2, 4, 5, 6.

```bash
git add port/src/game/actors.c port/tests/test_game.c port/tests/test_platform.c docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md
git commit -m "$(cat <<'EOF'
game: 0x37A58 wraps on the frame byte rec+0x52 (0x37B03), divergence 1 (record §C5.12)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2-alt: Divergence 1 as a named gap (only if Task 1's gate fails)

**Files:**
- Modify: `Makefile` (the comment above `GP_IDLE_LOSS_MIN_FIRST`, lines 426–431; the value stays the measured 203)
- Modify: record `§C5.12`

- [ ] **Step 1:** In §C5.12 record which of (i)–(iii) failed, with its output beside the expected text, and which candidates (record §C5.1) remain open with their predictions.
- [ ] **Step 2:** In the Makefile replace the three comment lines

```make
# isolated (no raw address; the fn_resolve miss log holds nothing in the character select
# but the bare `ret` 0x29D60, record §G.24); raise it when the frame claim improves
# (gp_compare prints "improved: raise N").
```

with

```make
# isolated: divergence 1 stays a named gap (record 2026-10-01-gameplay-u5 §C5.12: the evidence
# (i)-(iii) and the open candidates); raise it when the frame claim improves
# (gp_compare prints "improved: raise N").
```

Keep `GP_IDLE_LOSS_MIN_FIRST = 203`.
- [ ] **Step 3:** `$S/mkv verify` → exit 0 and the gate check; then `$S/mkv gp-oracle GP_IDLE_LOSS_MIN_FIRST=204` → `FAIL: first unexplained 203 < ratchet N 204` (the pin can fail).
- [ ] **Step 4:** Commit `git add Makefile docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md`, message `docs: divergence 1 restated as a named gap with its evidence (record §C5.12)` and the trailer. Skip Task 3; continue at Task 4.

---

### Task 3: Raise the gp-idle-loss frame ratchet

**Files:**
- Modify: `Makefile` (lines 426–432)
- Modify: record `§C5.13`

**Interfaces:**
- Consumes: Task 2 Step 6's `FIRST UNEXPLAINED capture J (raw R)` and its nearest port frame's `frames.txt` line
- Produces: `GP_IDLE_LOSS_MIN_FIRST = J`

- [ ] **Step 1: The nearest port frame's f** — `sed -n "$((575 + 1))p" /tmp/pr_u5_gp/gp-idle-loss/frames.txt` (use Task 2's nearest port index). Expected: `00575 f=0823 tick=00000881 mode=0006`.

- [ ] **Step 2: Pin.** Replace lines 426–432 (the `# MIN_FIRST:` comment through `GP_IDLE_LOSS_MIN_FIRST = 203`) with (the numbers are Task 2 Step 6's; 787/3899/575/0x823 are the planner's run):

```make
# MIN_FIRST: first unexplained capture frame 787 (raw 3899), round 1 (mode 6): its nearest port
# frame 575 is f=0x823, one frame before the unregistered move callback 0x23208 at f=0x824
# (divergence 2, record §G.24, U6's). Raised from 203 by U5 (record 2026-10-01-gameplay-u5
# §C5.12/§C5.13): divergence 1, the character-select idle animation turning at f=0x340, was the
# 0x37A58 top wrap comparing rec+0x4F where the raw's 0x37B03/0x37B08 compares the frame byte
# rec+0x52; with it fixed the claim covers the character select, the time-out, 0x11, 0x17 and the
# round start. Raise it when the frame claim improves (gp_compare prints "improved: raise N").
GP_IDLE_LOSS_MIN_FIRST = 787
```

- [ ] **Step 3: It holds, and it fails.**

```bash
$S/mkv gp-oracle 2>&1 | grep -E 'frames: first unexplained|trace: first differing'
$S/mkv gp-oracle GP_IDLE_LOSS_MIN_FIRST=788 2>&1 | grep -E 'FAIL'; echo "exit=$?"
```

Expected: `first unexplained 787, ratchet N 787 ok`, `first differing 2088, ratchet N 2088 ok`; then `frames: FAIL: first unexplained 787 < ratchet N 788`. The old operand must fail it too. The replay driver stops `make gp-oracle` before the comparison when its miss set fails, so run the two halves by hand:

```bash
sed -i '' 's/DSB(rec + 0x52u) >= (s32)DSB(rec + 0x4du)) \/\* 0x37B03/DSB(rec + 0x4fu) >= (s32)DSB(rec + 0x4du)) \/* 0x37B03/' port/src/game/actors.c
git diff --stat port/src    # one line changed
$S/mkv gp-replay scenario=gp-idle-loss 2>&1 | grep -E 'distinct|FAIL'
pin() { awk -v n="$1" '$1 == n {print $3}' Makefile; }
python3 tools/gp_compare.py --scenario gp-idle-loss --capture data/k11-captures/gp-idle-loss --port /tmp/pr_u5_gp/gp-idle-loss \
  --min-first "$(pin GP_IDLE_LOSS_MIN_FIRST)" --trace-min-first "$(pin GP_IDLE_LOSS_TRACE_MIN_FIRST)" \
  --max-start "$(pin GP_IDLE_LOSS_MAX_START)" --capture-sha256 "$(pin GP_IDLE_LOSS_CAPTURE_SHA256)" \
  --capture-frames "$(pin GP_IDLE_LOSS_CAPTURE_FRAMES)" | grep -E 'FAIL|ok'
git checkout port/src/game/actors.c && git diff --stat port/src   # empty: the fix is committed (Task 2)
```

Expected: `distinct=7` and `FAIL …test_platform.c:…: 7 != 6` plus `the driver's miss log holds only its pinned known-set` (`0x3640C` is reached again: the old operand is what made the replay reach it); then `frames: FAIL: first unexplained 203 < ratchet N 787` and `trace: first differing 2088, ratchet N 2088 ok`. (The planner ran the same with the old operand selected by an environment switch in a scratch build: `distinct=7`, `7 != 6`, `FAIL: first unexplained 203 < ratchet N 787`, trace `ok`.) Rebuild (`cmake --build build -j8`) before the gate.

- [ ] **Step 4: Gate** — `$S/mkv verify` → exit 0 and the gate check.

- [ ] **Step 5: Record and commit** — §C5.13: Steps 1–4's lines.

```bash
git add Makefile docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md
git commit -m "$(cat <<'EOF'
build: gp-idle-loss frame ratchet raised to the measured first unexplained frame (record §C5.13)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: The `gp-u5-charsel` scenario

**Files:**
- Modify: `tools/gp_session.py` (after line 126, `SCENARIOS['gp-idle-loss-run2'] = …`)
- Modify: `tools/tests/test_gp_session.py` (append `class TestCharsel`)
- Modify: record `§C5.14`

**Interfaces:**
- Consumes: `gs.SCENARIOS`, `gs.PAD`, `gs.Schedule`, `gs.ENTER_WAIT`
- Produces: `gs.CHARSEL_WALK` (tuple of pad names), `gs.SCENARIOS['gp-u5-charsel']`; `make gp-capture scenario=gp-u5-charsel` accepts it (`gp_capture.py` `choices` is every `gp-` key)

- [ ] **Step 1: The failing tests.** Append to `tools/tests/test_gp_session.py`:

```python


class TestCharsel(unittest.TestCase):
    """gp-u5-charsel (record 2026-10-01-gameplay-u5 §C5.4): the walk, modelled on the raw."""

    @staticmethod
    def stick(c, bit):
        # 0x43B24's stick (0x43BB5..0x43C99) on the signed cursor byte DS_00108166[side]
        if bit == 0x10 and c < 6:
            return c + 1
        if bit == 0x20 and c > 0:
            return c - 1
        if bit == 0x40:
            c = c + 4 if c < 4 else c
            return 6 if c >= 7 else c
        if bit == 0x80 and c >= 4:
            return c - 4
        return c

    def walk(self):
        steps = gs.SCENARIOS['gp-u5-charsel']['steps']
        i = next(k for k, st in enumerate(steps) if st[0] == 'after_mode' and st[1] == 0x10)
        return steps[i:]

    def test_the_walk_visits_every_cell_and_confirms_on_1(self):
        c, seen = 0, [0]                       # 0xC8880: P1 starts on 0 (0x4370D, DS_00108173 == 0)
        for st in self.walk():
            if st[0] == 'until_mode':
                break
            (name,), hold = st[-1][1], st[-1][2]
            self.assertTrue(2 <= hold < 0x1F, name)
            bit = gs.PAD[name][2] >> 8
            if bit & 0xF0:
                c = self.stick(c, bit & 0xF0)
                seen.append(c)
            else:
                self.assertEqual(bit & 1, 1, name)          # 0x43CAD: e0 bit 0 confirms
        self.assertEqual(seen, [0, 0, 1, 2, 3, 6, 5, 4, 0, 1])
        self.assertEqual(sorted(set(seen)), list(range(7)))

    def test_presses_are_separate_edges_before_the_time_out(self):
        w = self.walk()
        gaps = [st[2] if st[0] == 'after_mode' else st[1] for st in w if st[0] != 'until_mode']
        holds = [st[-1][2] for st in w if st[0] != 'until_mode']
        for g, h in zip(gaps[1:], holds):
            self.assertGreaterEqual(g, h + 2)      # a released bit falls before the next press
        self.assertLess(sum(gaps), 14 * 64)        # the earliest pick time-out
        self.assertEqual(w[-1], ('until_mode', 0x06, 0))

    def test_schedule_fires_the_walk(self):
        s = gs.Schedule(gs.SCENARIOS['gp-u5-charsel']['steps'])
        s.due_boot(gs.ENTER_WAIT)
        s.on_mode(0x141, 0x27)
        s.due(0x141 + 149); s.due(0x141 + 299)
        s.on_mode(0x26E, 0x2D); s.on_mode(0x293, 0x10)
        fired = []
        for f in range(0x293, 0x293 + 500):
            fired += [(f + 1, a) for _, a in s.due(f)]
        self.assertEqual([f for f, _ in fired], [0x293 + 60 + 40 * k for k in range(10)])
        self.assertEqual(fired[-1][1], ('pad', ('p1.start',), 6))
        s.on_mode(0x439, 0x1A)
        self.assertIsNone(s.end_frame)
        s.on_mode(0x5EE, 0x06)
        self.assertEqual((s.end_frame, s.fired, s.total), (0x5EE, 13, 13))
```

(The frames `0x141`, `0x26E`, `0x293` are `gp-idle-loss` run 1's, record §G.18; `0x439`/`0x5EE` the port's prediction, record §C5.6.)

- [ ] **Step 2: Run to see it fail**

Run: `python3 -m unittest tools.tests.test_gp_session 2>&1 | tail -4`
Expected: `KeyError: 'gp-u5-charsel'` in the three `TestCharsel` tests, `FAILED (errors=3)`.

- [ ] **Step 3: The scenario.** In `tools/gp_session.py`, after the `SCENARIOS['gp-idle-loss-run2'] = …` line, add:

```python
# U5 (record 2026-10-01-gameplay-u5 §C5.4): LEFT PLAYER ARCADE as gp-idle-loss, then
# P1 walks the character-select cursor DS_00108166[0] (0x43B24: e0 bits 0x10 right
# while < 6, 0x20 left while > 0, 0x40 down +4 then clamped to 6, 0x80 up -4 while >= 4)
# over all seven cells, 0 (left: the clamp, a no-op) 1 2 3 6 5 4 0 1, confirms on 1 with
# p1.start (e0 bit 0, 0x43CAD) and runs to the first frame of mode 6 (the round).
# Harness values: the hold 6 (>= 2 for the level, 0x500C4; < 0x1F, the 0xC000C000 repeat
# first period 0x1E that the mode 0x27 menu arms at 0x251C6), the 60-frame wait into mode
# 0x10 and the 40-frame gaps (the confirm lands 420 frames in, before the earliest
# pick time-out 14 * 64 = 896 frames in: countdown 0xF, 0x437C6, stepped at f & 0x3F == 0),
# and the 60 s limit (the mode-6 frame is expected near 47 s, §C5.7).
CHARSEL_WALK = ('p1.left', 'p1.right', 'p1.right', 'p1.right', 'p1.down',
                'p1.left', 'p1.left', 'p1.up', 'p1.right')
SCENARIOS['gp-u5-charsel'] = dict(time_limit=60, steps=(
    ('boot', ENTER_WAIT, ('key', 'enter')),           # mode 3 -> 0x27, MAIN MENU on "Start"
    ('after_mode', 0x27, 150, ('key', 'enter')),      # START MENU, cursor on row 0 (spec §3.3)
    ('after', 150, ('key', 'enter')),                 # LEFT PLAYER ARCADE: mode 0x2D
    ('after_mode', 0x10, 60, ('pad', (CHARSEL_WALK[0],), 6)),
) + tuple(('after', 40, ('pad', (n,), 6)) for n in CHARSEL_WALK[1:]) + (
    ('after', 40, ('pad', ('p1.start',), 6)),         # confirm cursor 1
    ('until_mode', 0x06, 0),                          # the round's first frame
))
```

- [ ] **Step 4: Run to see it pass**

Run: `python3 -m unittest tools.tests.test_gp_session 2>&1 | tail -2`
Expected: `Ran 25 tests`, `OK`.

- [ ] **Step 5: Mutation proofs** (each applied with `sed -i ''` to `tools/gp_session.py`, run, then restored with `git checkout tools/gp_session.py` and the Step 3 edit re-applied — or keep a copy `cp tools/gp_session.py $S/gs_keep.py` and restore from it):
  - `'p1.right', 'p1.down',` → `'p1.right', 'p1.up',`: `FAIL: test_the_walk_visits_every_cell_and_confirms_on_1`.
  - `('pad', (n,), 6)) for n in CHARSEL_WALK` → `('pad', (n,), 1)) for n in CHARSEL_WALK` (a one-frame press never reaches the level): `FAIL: test_the_walk_visits_every_cell_and_confirms_on_1`.
  - `(('after', 40, ('pad', (n,), 6))` → `(('after', 7, ('pad', (n,), 6))`: `FAIL: test_presses_are_separate_edges_before_the_time_out` and `FAIL: test_schedule_fires_the_walk`.
  Restored: `Ran 25 tests … OK`.

- [ ] **Step 6: Record and commit** — §C5.14 (the outputs of Steps 2, 4, 5).

```bash
git add tools/gp_session.py tools/tests/test_gp_session.py docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp-u5-charsel scenario, the character-select walk (record §C5.14)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: The replay driver's miss set for `gp-u5-charsel`, proven on the port's own walk

**Files:**
- Modify: `port/tests/test_platform.c` (after `k_miss_gp_idle_loss`; `fnm_known`; `test_fn_misslog_driver`; the `--check` call)
- Modify: record `§C5.15`
- Scratch: `$S/sim/`

**Interfaces:**
- Consumes: the `PR_GP_DUMP` driver (`PR_GP_SCRIPT`), `fnm_gp_scenario`
- Produces: `static const fnm_pair k_miss_gp_charsel[]`; `fnm_known(u32 addr, const char *ctx, int frontend, int idle_loss, int charsel)`

- [ ] **Step 1: The port's walk** — a v2 script with the scenario's pad frames on `gp-idle-loss`'s Enter frames (mode `0x10` at `0x293`), each pad's BIOS word consumed 2 frames after its press (the capture's real frames replace these in Task 7):

```bash
mkdir -p $S/sim && python3 - <<'EOF'
PAD = {'p1.up': (0x1F, 0x73, 0x8000), 'p1.down': (0x2D, 0x78, 0x4000), 'p1.left': (0x2C, 0x7A, 0x2000),
       'p1.right': (0x2E, 0x63, 0x1000), 'p1.start': (0x3B, 0x00, 0x0100)}
moves = ['p1.left','p1.right','p1.right','p1.right','p1.down','p1.left','p1.left','p1.up','p1.right','p1.start']
f0 = 0x293; HOLD = 6; GAP = 40; FIRST = 60
ev = [(321,'key 321 1C 0D'),(474,'key 474 1C 0D'),(622,'key 622 1C 0D')]
for k, m in enumerate(moves):
    F = f0 + FIRST + GAP*k; sc, a, bit = PAD[m]
    ev += [(F,'bits %d %04X' % (F, bit)), (F+HOLD, 'bits %d 0000' % (F+HOLD)), (F+2, 'key %d %02X %02X' % (F+2, sc, a))]
lines = ['# gp port script v2: scenario gp-u5-charsel', 'enter_frame 321', 'enter_state 0000'] + [t for _, t in sorted(ev)] + ['end 1518']
open('/tmp/gameplay-u5/sim/sim6.script','w').write('\n'.join(lines)+'\n')
EOF
rm -rf $S/sim/dump6; PR_GP_DUMP=$S/sim/dump6 PR_GP_SCRIPT=$S/sim/sim6.script PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep -E 'fn-miss|FAIL|passed'
```

(`end 1518` = `0x5EE`, the port's first mode-6 frame, record §C5.6.) Expected: `fn-miss PR_GP_DUMP 0x29D60 frontend_mode_1b_step hits=1`, `fn-miss PR_GP_DUMP 0x5D812 frontend_mode_1b_step hits=1`, `distinct=4 dropped=0`, `FAIL …test_platform.c:…: 4 != 2`, two `unexpected` lines.

- [ ] **Step 2: The set.** In `port/tests/test_platform.c`, after the `k_miss_gp_idle_loss[]` table's `};`, add:

```c

/* gp-u5-charsel (record gameplay-u5 §C5.6/§C5.15): the character-select walk,
 * cut at the first frame of mode 6, adds the two hooks of the wipes it passes,
 * each classified in §G.24: 0x29D60, a bare `ret` (the wipe's end into mode
 * 0x10), and 0x5D812, the runtime stub (the wipe into mode 5). */
static const fnm_pair k_miss_gp_charsel[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};
```

Replace `fnm_known` with:

```c
static int fnm_known(u32 addr, const char *ctx, int frontend, int idle_loss, int charsel)
{
    if (fnm_in(k_miss_known, FNM_N(k_miss_known), addr, ctx)) return 1;
    if (frontend && fnm_in(k_miss_frontend, FNM_N(k_miss_frontend), addr, ctx)) return 1;
    if (charsel && fnm_in(k_miss_gp_charsel, FNM_N(k_miss_gp_charsel), addr, ctx)) return 1;
    return idle_loss && fnm_in(k_miss_gp_idle_loss, FNM_N(k_miss_gp_idle_loss), addr, ctx);
}
```

In `test_fn_misslog_driver` replace

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
    int idle_loss = 0, charsel = 0, cut = 0;
    if (strcmp(env, "PR_GP_DUMP") == 0) {
        char sc[64];
        fnm_gp_scenario(sc, sizeof sc, &cut);
        idle_loss = strncmp(sc, "gp-idle-loss", 12) == 0;
        charsel = strcmp(sc, "gp-u5-charsel") == 0;
    }
    u32 want = (u32)FNM_N(k_miss_known) +
               (frontend ? (u32)FNM_N(k_miss_frontend) : 0u) +
               (idle_loss ? (u32)FNM_N(k_miss_gp_idle_loss) : 0u) +
               (charsel ? (u32)FNM_N(k_miss_gp_charsel) : 0u);
```

its loop's `fnm_known(fn_misslog_addr(i), fn_misslog_ctx(i), frontend, idle_loss)` with `fnm_known(fn_misslog_addr(i), fn_misslog_ctx(i), frontend, idle_loss, charsel)`, and in the `--check` log reader `fnm_known((u32)addr, ctx, 0, 0)` with `fnm_known((u32)addr, ctx, 0, 0, 0)`.

- [ ] **Step 3: Run to see it pass** — rebuild, re-run Step 1's replay command. Expected: `distinct=4 dropped=0`, `all checks passed`.

- [ ] **Step 4: Mutation proof** — `sed 's/scenario gp-u5-charsel/scenario gp-u5-sim/' $S/sim/sim6.script > $S/sim/simx.script`, replay it (`PR_GP_SCRIPT=$S/sim/simx.script`, `PR_GP_DUMP=$S/sim/dumpx`): `FAIL …: 4 != 2` and `unexpected 0x29D60 …`, `unexpected 0x5D812 from frontend_mode_1b_step`. Then delete the `0x29D60` row from `k_miss_gp_charsel` and replay `sim6.script`: `FAIL …: 4 != 3`, `unexpected 0x29D60 from frontend_mode_1b_step`; restore the row.

- [ ] **Step 5: The port's walk, for the record** — `python3 - <<'EOF'` over `$S/sim/dump6/trace.txt` printing each mode change (`f`, `mode`): expected `0x10 0x293`, `0x1A 0x439`, `0x1B 0x44B`, `0x11 0x45D`, `0x17 0x45E`, `0x1A 0x54F`, `0x1B 0x561`, `5 0x573`, `6 0x5EE` (record §C5.6):

```bash
python3 - <<'EOF'
prev = None
for l in open('/tmp/gameplay-u5/sim/dump6/trace.txt'):
    p = dict(x.split('=') for x in l.split()[1:] if '=' in x)
    if p.get('mode') != prev:
        print('f=%s mode=%s e0=%s' % (p['f'], p['mode'], p.get('e0'))); prev = p.get('mode')
EOF
```

- [ ] **Step 6: Gate** — `$S/mkv verify` → exit 0 (gp-pads `distinct=2`, gp-idle-loss `distinct=6`, both `all checks passed`) and the gate check.

- [ ] **Step 7: Record and commit** — §C5.15.

```bash
git add port/tests/test_platform.c docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md
git commit -m "$(cat <<'EOF'
tests: the gp replay driver pins gp-u5-charsel's miss set (record §C5.15)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: The capture (behind Decision 1)

**Files:**
- Writes (git-ignored): `data/k11-captures/gp-u5-charsel/`
- Modify: record `§C5.16`

**Interfaces:**
- Consumes: `make gp-capture`, `SCENARIOS['gp-u5-charsel']`
- Produces: the capture every later task reads; its mode path and `e0` edges

- [ ] **Step 0:** Log Decision 1's answer in §C5.16 (as §G.18 logged Q7). Without a "yes", stop here and report.

- [ ] **Step 1: Capture**

```bash
$S/mkv gp-capture scenario=gp-u5-charsel 2>&1 | tee $S/cap.txt | tail -14
du -sh data/k11-captures/gp-u5-charsel; cat data/k11-captures/gp-u5-charsel/session.txt
```

Expected: every `CHECK … ok`, `steps fired 13/13`, `end frame reached`, `port script v2` ok, `wall` ≈ 60 s (the time limit), size near 55 MB (record §C5.7; anything above 170 MB is a finding to record). If `end frame reached` fails, rerun once with `GP_ARGS="--time-limit 75"` and record both runs (a failing run lands in `gp-u5-charsel.failed`). If the port-script CHECK fails on an unpinned key consumption (a snapshot gap at a press, spec §6), record it and stop: the scenario's frames must move, which is a plan change.

- [ ] **Step 2: The mode path and the walk as the original saw it**

```bash
cat > $S/modepath.py <<'EOF'
# U5 Task 6 Step 2: the capture's mode path and P1's e0 edges in mode 0x10 (record §C5.16).
import sys
sys.path.insert(0, 'tools')
import gp_session as gs
L = open(sys.argv[1] + '/poll.log').read().splitlines()
prev, edges = None, []
for n, l in enumerate(L, 1):
    r = gs.parse(l)
    if not r or r['kind'] not in ('S', 'P'):
        continue
    if r['mode'] != prev:
        extra = ' cred=%X e0=%04X' % (r['cred'], r['e0']) if r['kind'] == 'S' else ''
        print('poll.log:%d %s f=%X mode=%X%s' % (n, r['kind'], r['f'], r['mode'], extra))
        prev = r['mode']
    if r['kind'] == 'S' and r['mode'] == 0x10 and r['e0'] & 0xFF:
        edges.append((r['f'], r['e0'] & 0xFF, n))
print('e0 edges in mode 0x10:', ' '.join('f=%X:%02X(poll.log:%d)' % e for e in edges))
print('sticks:', [e for _, e, _ in edges])
EOF
python3 $S/modepath.py data/k11-captures/gp-u5-charsel
```

Expected: modes `3, 0x27, 0x2D, 0x1A, 0x1B, 0x10, 0x1A, 0x1B, 0x11, 0x17, 0x1A, 0x1B, 5, 6` (credits 5 → 4 at `0x2D`); `sticks: [32, 16, 16, 16, 64, 32, 32, 128, 16, 1]` (left, right ×3, down, left ×2, up, right, confirm), each edge one frame after its press's `I` record; the pick, not the time-out, ends mode `0x10`: the first `0x1A` after `0x10` comes one frame after the confirm edge (the port's prediction: edge `0x438`, `0x1A` at `0x439`), about 420 frames into mode `0x10`, below the 896-frame earliest time-out. Any deviation is a correction of the plan: record it with its `poll.log:<n>`. A missing snapshot at an edge is "not observed", not "absent" (spec §3.7).

- [ ] **Step 3: Record and commit** — §C5.16: `session.txt`, the CHECK lines, `du -sh`, the `poll.log` sha256 (`shasum -a 256 data/k11-captures/gp-u5-charsel/poll.log`), the frame count, Step 2's output.

```bash
git add docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md
git commit -m "$(cat <<'EOF'
docs: gp-u5-charsel capture: the mode path and the walk's edges (record §C5.16)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: The port replay and the first divergences

**Files:**
- Modify: record `§C5.17`
- Possibly modify: `port/tests/test_platform.c` (`k_miss_gp_charsel`), only as Step 2 says

**Interfaces:**
- Consumes: `make gp-report scenario=gp-u5-charsel`
- Produces: `$S/report.txt` (Task 8 reads it)

- [ ] **Step 1: Replay and report**

```bash
$S/mkv gp-report scenario=gp-u5-charsel 2>&1 | tee $S/report.txt | grep -E 'fn-miss|FAIL|passed|gp_compare'
cat /tmp/pr_u5_gp/gp-u5-charsel.script | head -40; du -sh /tmp/pr_u5_gp/gp-u5-charsel
```

Expected: the script carries 13 `key` lines and the pad `bits` lines; the driver prints `distinct=4` and `all checks passed`; the frame claim prints its window and a first unexplained frame; the trace claim prints `0 differing through …` or a first difference.

- [ ] **Step 2: The miss set against the prediction.** If the driver failed on the set, the replay reached a pair Task 5 did not predict: classify it from the raw like §G.24 (address, caller, `f`, what it is), add it to `k_miss_gp_charsel` with that classification in the comment, rebuild, re-run Step 1, and record it as a correction of §C5.6. Never add a pair without its classification.

- [ ] **Step 3: Triage** (each in §C5.17 with its evidence, the way §G.20 did):
  - The frame claim. If the first unexplained capture frame lies after the port's last frame (the capture runs on into round 1 until the 60 s limit), it is how far the port got, not a defect (spec §4.3): record the nearest port frame's `frames.txt` line (its `f` should be the script's `end`, mode 6). If it lies inside the walk, find the edge it follows (Task 6 Step 2's `f` list) and describe the difference box (`gp_compare` prints rows/x). Either way, render the capture frame and the nearest port frame to PNG through `gp_compare`'s own loaders and read them:

```bash
cat > $S/pngs.py <<'EOF'
# U5 Task 7 Step 3: the first unexplained capture frame and its nearest port frame as PNGs,
# through gp_compare's own loaders. argv: report capture_dir port_dir out_dir
import re, sys
sys.path.insert(0, 'tools')
import gp_compare as gc
from PIL import Image
rep, cap, port, out = sys.argv[1:5]
m = re.search(r'FIRST UNEXPLAINED capture (\d+) \(raw \d+\): nearest port (\d+)', open(rep).read())
if not m:
    print('no unexplained frame in the report')
else:
    j, p = int(m.group(1)), int(m.group(2))
    Image.frombytes('RGB', (320, 200), gc.load_capture_frame('%s/frame_%05d.raw.gz' % (cap, j))).save('%s/cap_%d.png' % (out, j))
    Image.frombytes('RGB', (320, 200), gc.load_port_frame('%s/frame_%05d.ipx' % (port, p))).save('%s/port_%d.png' % (out, p))
    print('wrote %s/cap_%d.png %s/port_%d.png' % (out, j, out, p))
EOF
python3 $S/pngs.py $S/report.txt data/k11-captures/gp-u5-charsel /tmp/pr_u5_gp/gp-u5-charsel $S
```

    (The planner ran it on `gp-idle-loss` after the fix: `cap_787.png` shows round 1 with the HUD names SAURON and BLIZZARD and the CPU's Blizzard standing, `port_575.png` the same scene with Blizzard down — divergence 2, U6's.)
  - The trace claim: the first differing `f` and field, or `0 differing through …`.
  - For each divergence: port divergence / harness / host-timed / unregistered callback (the miss log) — as §G.20; a cause not isolated is a named gap with its evidence and the owner. Nothing here is fixed in U5 unless it is the 0x37A58 class of error with the same three-part evidence as Task 1 (then it is a new task the controller approves).

- [ ] **Step 4: Record and commit** — §C5.17 (Steps 1–3; the dump size).

```bash
git add docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md
git commit -m "$(cat <<'EOF'
docs: gp-u5-charsel port replay and its first divergences (record §C5.17)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

(Add `port/tests/test_platform.c` to the `git add` only if Step 2 changed it.)

---

### Task 8: The `gp-charsel-oracle` ratchets in `make verify`

**Files:**
- Modify: `Makefile` (new block before `gp-report:`; `.PHONY` line 56; `verify` after the `gp-oracle` line)
- Modify: record `§C5.18`

**Interfaces:**
- Consumes: `$S/report.txt`, the capture
- Produces: `GP_CHARSEL_MIN_FIRST`, `GP_CHARSEL_TRACE_MIN_FIRST`, `GP_CHARSEL_MAX_START`, `GP_CHARSEL_CAPTURE_SHA256`, `GP_CHARSEL_CAPTURE_FRAMES`; target `gp-charsel-oracle`

- [ ] **Step 1: The five values, mechanically** (measured in Task 7, pinned here):

```bash
cat > $S/pins.sh <<'EOF'
# U5 Task 8 Step 1: the five pins from Task 7's report ($1) and the capture ($2). Prints Makefile lines.
R=$1; C=$2
N=$(sed -n 's/.*frames: FIRST UNEXPLAINED capture \([0-9][0-9]*\) .*/\1/p' "$R" | head -1)
if [ -z "$N" ]; then
  E=$(sed -n 's/.*frames: 0 unexplained through \([0-9][0-9]*\).*/\1/p' "$R" | head -1)
  [ -n "$E" ] && N=$((E + 1))
fi
F=$(sed -n 's/.*trace: first difference f=[0-9A-F]* (\([0-9][0-9]*\)).*/\1/p' "$R" | head -1)
if [ -z "$F" ]; then
  E=$(sed -n 's/.*trace: 0 differing through \([0-9][0-9]*\).*/\1/p' "$R" | head -1)
  F=$((E + 1))
fi
W=$(sed -n 's/.*frames: window from capture \([0-9][0-9]*\) .*/\1/p' "$R" | head -1)
H=$(shasum -a 256 "$C/poll.log" | cut -d' ' -f1)
K=$(ls "$C" | grep -c '^frame_[0-9]*\.raw\.gz$')
[ -n "$N" ] && [ -n "$F" ] && [ -n "$W" ] || { echo "pins: a value is missing (N=$N F=$F W=$W)"; exit 1; }
echo "GP_CHARSEL_MIN_FIRST = $N"
echo "GP_CHARSEL_TRACE_MIN_FIRST = $F"
echo "GP_CHARSEL_MAX_START = $W"
echo "GP_CHARSEL_CAPTURE_SHA256 = $H"
echo "GP_CHARSEL_CAPTURE_FRAMES = $K"
EOF
sh $S/pins.sh $S/report.txt data/k11-captures/gp-u5-charsel | tee $S/pins.txt
```

(Checked by the planner on `gp-idle-loss`'s verify lines: it printed `787 / 2088 / 90 / 773e2647…8c8447 / 8173`, the Makefile's own values. N must be greater than W; if not, stop: the window claims nothing.)

- [ ] **Step 2: The target**, inserted mechanically from `$S/pins.txt` (the block goes before `gp-report:`, the target joins `.PHONY`, and `verify` gains one line after `gp-oracle`):

```bash
cat > $S/add_target.py <<'EOF'
# U5 Task 8 Step 2: insert the gp-charsel-oracle block (values from pins.txt, argv[1]) into the
# Makefile, add the target to .PHONY and one line to verify. Run from the worktree root.
import sys
pins = [l.strip() for l in open(sys.argv[1]) if l.startswith('GP_CHARSEL_')]
names = ['GP_CHARSEL_MIN_FIRST', 'GP_CHARSEL_TRACE_MIN_FIRST', 'GP_CHARSEL_MAX_START',
         'GP_CHARSEL_CAPTURE_SHA256', 'GP_CHARSEL_CAPTURE_FRAMES']
assert [p.split(' = ')[0] for p in pins] == names and all(p.split(' = ')[1] for p in pins), pins
block = '''# U5 character-select walk (record 2026-10-01-gameplay-u5 §C5.16-§C5.18): the gp-u5-charsel capture
# against its port replay, both ratchets, enforced like gp-oracle (skips without the capture,
# fails on a present capture with another poll.log). Values measured in U5 Task 7 (report lines in
# §C5.17), pinned by U5 Task 8 Step 1 (pins.sh); raise N/F when the claims improve.
''' + '\n'.join(pins) + '''
gp-charsel-oracle: build ## Gameplay oracle: gp-u5-charsel frame and trace ratchets (skips without data/k11-captures/gp-u5-charsel)
\t@echo "== gameplay oracle: gp-u5-charsel (frame and trace ratchets) =="
\t@$(MAKE) --no-print-directory gp-replay scenario=gp-u5-charsel GP_OPTIONAL=1
\t@$(PYTHON) tools/gp_compare.py --scenario gp-u5-charsel --capture $(K11_CAPTURES)/gp-u5-charsel \\
\t\t--port $(GP_DUMP)/gp-u5-charsel --min-first "$(GP_CHARSEL_MIN_FIRST)" \\
\t\t--trace-min-first "$(GP_CHARSEL_TRACE_MIN_FIRST)" --max-start "$(GP_CHARSEL_MAX_START)" \\
\t\t--capture-sha256 "$(GP_CHARSEL_CAPTURE_SHA256)" --capture-frames "$(GP_CHARSEL_CAPTURE_FRAMES)"

'''
s = open('Makefile').read()
for old, new in (("gp-report: build ## Report-only gameplay comparison", block + "gp-report: build ## Report-only gameplay comparison"),
                 ("gp-capture gp-replay gp-oracle gp-report diff-verify", "gp-capture gp-replay gp-oracle gp-report diff-verify gp-charsel-oracle"),
                 ("\t@$(MAKE) --no-print-directory gp-oracle\n", "\t@$(MAKE) --no-print-directory gp-oracle\n\t@$(MAKE) --no-print-directory gp-charsel-oracle\n")):
    assert s.count(old) == 1, old
    s = s.replace(old, new)
open('Makefile', 'w').write(s)
print('Makefile: gp-charsel-oracle added')
EOF
python3 $S/add_target.py $S/pins.txt && git diff --stat Makefile && make help | grep gp-charsel-oracle
```

Expected: `Makefile: gp-charsel-oracle added`, `1 file changed, 19 insertions(+), 1 deletion(-)` (the 17-line block, the `verify` line, the `.PHONY` line changed; measured by the planner), the `make help` line. Then add one provenance line under the block's comment by hand, citing Task 7's two report lines (the first unexplained frame's raw index and nearest port `f`; the trace line) — words only, no number that is not in §C5.17. (The planner ran `add_target.py` on a clean Makefile with `pins.sh`'s gp-idle-loss values as stand-ins, and `make gp-charsel-oracle` on a checkout without the capture: `gp-replay: no capture at data/k11-captures/gp-u5-charsel`, `gp_compare: no capture at data/k11-captures/gp-u5-charsel (skipped)`, exit 0.)

- [ ] **Step 3: Green, and each pin can fail**

```bash
$S/mkv gp-charsel-oracle 2>&1 | grep gp_compare
N=$(awk '/^GP_CHARSEL_MIN_FIRST/{print $3}' Makefile); F=$(awk '/^GP_CHARSEL_TRACE_MIN_FIRST/{print $3}' Makefile)
$S/mkv gp-charsel-oracle GP_CHARSEL_MIN_FIRST=$((N + 1)) 2>&1 | grep FAIL
$S/mkv gp-charsel-oracle GP_CHARSEL_TRACE_MIN_FIRST=$((F + 1)) 2>&1 | grep FAIL
$S/mkv gp-charsel-oracle GP_CHARSEL_CAPTURE_SHA256=00ff 2>&1 | grep FAIL
$S/mkv gp-charsel-oracle K11_CAPTURES=/tmp/gameplay-u5/no-captures 2>&1 | grep -E 'skipped|no capture'; echo "exit=$?"
```

Expected: `capture: … matches the pin`, `frames: … ratchet N <N> ok`, `trace: … ratchet N <F> ok`; then `frames: FAIL: first unexplained <N> < ratchet N <N+1>` (or, in the exact-pin case, `FAIL: N … > end …: N is unreachable`); the trace's `FAIL` likewise; `capture: FAIL: poll.log sha256 … != the pinned 00ff`; the skip lines with exit 0. Then damaged port frames below N (the frame claim really reads the frames, as U4 §G.21 showed for its pin):

```bash
cat > $S/damage.py <<'EOF'
# U5 Task 8 Step 3: a copy of a port dump with the port frames [lo, hi] damaged
# (byte 32000 ^= 0xFF in each). argv: src_dump dst_dump lo hi
import os, shutil, sys
src, dst, lo, hi = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
shutil.rmtree(dst, ignore_errors=True)
shutil.copytree(src, dst)
for k in range(lo, hi + 1):
    p = os.path.join(dst, 'frame_%05d.ipx' % k)
    b = bytearray(open(p, 'rb').read())
    b[32000] ^= 0xFF
    open(p, 'wb').write(bytes(b))
print('damaged port frames %d..%d in %s' % (lo, hi, dst))
EOF
P=$(sed -n 's/.*FIRST UNEXPLAINED capture [0-9]* (raw [0-9]*): nearest port \([0-9]*\),.*/\1/p' $S/report.txt | head -1)
[ -n "$P" ] || P=$(($(ls /tmp/pr_u5_gp/gp-u5-charsel | grep -c '\.ipx$') - 1))
python3 $S/damage.py /tmp/pr_u5_gp/gp-u5-charsel $S/dmg $((P / 2)) $P
pin() { awk -v n="$1" '$1 == n {print $3}' Makefile; }
python3 tools/gp_compare.py --scenario gp-u5-charsel --capture data/k11-captures/gp-u5-charsel --port $S/dmg \
  --min-first "$(pin GP_CHARSEL_MIN_FIRST)" --trace-min-first "$(pin GP_CHARSEL_TRACE_MIN_FIRST)" \
  --max-start "$(pin GP_CHARSEL_MAX_START)" --capture-sha256 "$(pin GP_CHARSEL_CAPTURE_SHA256)" \
  --capture-frames "$(pin GP_CHARSEL_CAPTURE_FRAMES)" | grep -E 'FAIL|ok'; rm -rf $S/dmg
```

Expected: `frames: FAIL: first unexplained J < ratchet N …` with `J` below the pin. (The planner ran the same on `gp-idle-loss` after the fix, ports 287..575 damaged: `FAIL: first unexplained 457 < ratchet N 787`, the trace still `ok`.)

- [ ] **Step 4: Gate** — `$S/mkv verify` → exit 0, the gate check, and the verify log shows the `gp-u5-charsel` section with both `ok` lines.

- [ ] **Step 5: Record and commit** — §C5.18 (pins.txt, Step 3's lines).

```bash
git add Makefile docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md
git commit -m "$(cat <<'EOF'
build: gp-charsel-oracle, the character-select walk's frame and trace ratchets (record §C5.18)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 9: Closure

**Files:**
- Modify: `docs/PROGRESS.md` (append one paragraph)
- Modify: record `§C5.19` (closure, named gaps, corrections)

- [ ] **Step 1: The narrow claims, stated** (§C5.19): `gp-charsel-oracle` proves that no content-bearing capture frame of `gp-u5-charsel` from the window start up to N is unexplained and that `mode st raw pad e0 e2 rng cred s0_5a s1_5a` agree below F; it does not trace the cursor or class bytes (record §C5.5), does not claim the port renders everything, and says nothing past the script's end (mode 6's first frame). `gp-oracle` now claims the whole `gp-idle-loss` character select.

- [ ] **Step 2: Named gaps and coverage** (§C5.19), from record §C5.5 and Task 7: the second player's slot (prompt only; U7), the team pass `DS_00104B1D == 3` (U8), the button variant `DS_00105B34 != 0`, the `DS_00108173 != 0` path (no writer found by the address scan), the audit count `0x2E934` (port-deferred), the character names (inferred from the audit string order, not claimed), voices (not compared), `0x3640C` (still unported; no longer reached by the gp-idle-loss replay), and every divergence Task 7 left open with its owner. Corrections of the plan by the raw or the capture, numbered, each with its address or `poll.log:<n>`. The brief's "countdown modes 0x15–0x17" is corrected by the raw path: before the round the one-player path runs `0x11, 0x17` (record §C5.6, §G.18), `0x15` follows the match.

- [ ] **Step 3: PROGRESS** — the measured values come from the Makefile and the capture, so the paragraph is generated, then appended:

```bash
N3=$(awk '/^GP_IDLE_LOSS_MIN_FIRST/{print $3}' Makefile)
F3=$(awk '/^GP_IDLE_LOSS_TRACE_MIN_FIRST/{print $3}' Makefile)
NC=$(awk '/^GP_CHARSEL_MIN_FIRST/{print $3}' Makefile); FC=$(awk '/^GP_CHARSEL_TRACE_MIN_FIRST/{print $3}' Makefile)
SZ=$(du -sh data/k11-captures/gp-u5-charsel | awk '{print $1}')
cat >> docs/PROGRESS.md <<EOF

**Gameplay U5: the character-select walk and divergence 1** (branch \`gameplay-u5\`; record \`docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md\`). Divergence 1 (spec O1) was one operand: the fighters' idle-animation tick \`0x37A58\` (not a Ghidra function; reached as an animation opcode-\`0x10\` target) wraps its frame with \`0x37B03 mov edx,[ebx+0x4f]; 0x37B08 sar edx,0x18\`, i.e. the byte \`rec+0x52\`, and the port compared \`rec+0x4F\`; counting up past \`0x1D\` the original wraps to 0 while the port drew sprites past the 30 idle frames until the next turn, which is U4's "the animation turns ~96 frames late" at \`f = 0x340\`. Shown by the raw, by the original's bytes under the E1 emulator (\`0x1D + 1 -> 0\`) and by a frame trace of the port; fixed with failing-first unit assertions. \`GP_IDLE_LOSS_MIN_FIRST\` rose from 203 to ${N3}, the trace ratchet stays ${F3}, and the gp-idle-loss replay no longer reaches \`0x3640C\` (its miss set lost that row). New scenario \`gp-u5-charsel\`: LEFT PLAYER ARCADE, then P1 walks the cursor over all seven cells (\`0 1 2 3 6 5 4 0 1\`; \`0x43B24\`'s stick: right/left by one within \`0..6\`, down +4 clamped to 6, up -4) and confirms cell 1 (class 2) before the pick time-out, through \`0x11\`, \`0x17\` and the round start to mode 6's first frame; captured (${SZ}), replayed, and ratcheted by \`make gp-charsel-oracle\` in \`make verify\` (\`GP_CHARSEL_MIN_FIRST = ${NC}\`, \`GP_CHARSEL_TRACE_MIN_FIRST = ${FC}\`, the capture's \`poll.log\` sha256 pinned). The cursor and class bytes are not traced fields; P2's slot shows only its prompt (U7), the team pass is U8's; the divergences and named gaps are in record §C5.17/§C5.19. \`make verify\` exits 0 with the 45 oracle lines, the K11 lines and the WAV unchanged; counters \`771 1203 64\` / \`731 731 100\`.
EOF
tail -c 400 docs/PROGRESS.md
```

(If Task 2-alt was taken, write the paragraph by hand instead, stating divergence 1 as a named gap; the `N3` sentence does not apply.)

- [ ] **Step 4: Final gate** — `$S/mkv verify` → exit 0; the gate check; `git status --short` shows only committed work; `rm -rf $S/t1` (the throw-away instrumented clone).

- [ ] **Step 5: Commit**

```bash
git add docs/PROGRESS.md docs/superpowers/plans/2026-10-01-gameplay-u5-derivations.md
git commit -m "$(cat <<'EOF'
docs: gameplay U5 closure, named gaps and PROGRESS (record §C5.19)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

## What the planner ran (so the plan's code is not unverified)

On scratch clones of `e9271df` (record §C5 header lists the paths):

- The fix (Task 2 Steps 1, 3, 5 minus the comment wording; Task 3's value) with `make verify` and overrides: `verify-exit=0`, `ORACLES-EQUAL` (45 lines), `WAV-IDENTICAL`, `gp-idle-loss` `first unexplained 787, ratchet N 787 ok`, `first differing 2088, ratchet N 2088 ok`, `distinct=6`, `771 1203 64` / `731 731 100`, tool tests `Ran 104`, `Ran 71`.
- The Tick C/D assertions failing on the unfixed operand (`30 != 0`, `0 != 4`) and passing on the fixed one.
- The full gp-idle-loss replay with the old operand against the edited set and N = 787 (Task 3 Step 3's converse): `distinct=7`, `7 != 6`, `frames: FAIL: first unexplained 203 < ratchet N 787`, trace `ok`; the revert `sed` of Task 3 Step 3 on a copy of the fixed `actors.c` (one line changed).
- `orig_idle_tick.py`, `instr_at.py` + `at_summary.py` on a fresh clone of `e9271df` (outputs in record §C5.3, identical to Task 1's expected text).
- `TestCharsel` and the `gp-u5-charsel` scenario: `Ran 25 tests … OK`; before the scenario `KeyError` ×3; the three mutations fail as listed.
- The Task 5 generator, replay, set and both mutations (the `gp-u5-sim` header: `4 != 2`; the `0x29D60` row deleted: `4 != 3`, `unexpected 0x29D60 from frontend_mode_1b_step`); the replay to `end 1518` with the set: `all checks passed`; the cursor sequence and mode path of record §C5.6 (with throw-away `fprintf`s, not part of the plan).
- The Task 8 block with dummy values on a checkout without the capture (skip lines, exit 0, `make help` line) and `pins.sh` on `gp-idle-loss`'s verify lines (it reproduced the Makefile's pins) and on synthetic report lines for both exact-pin branches.
- `modepath.py` on `gp-idle-loss` (the U4 mode path; no edges, as P1 never pressed).
- `pngs.py` and `damage.py` on `gp-idle-loss` after the fix (`cap_787.png`/`port_575.png`; `FAIL: first unexplained 457 < ratchet N 787`); `add_target.py` on a clean Makefile (`19 insertions(+), 1 deletion(-)`); the Task 9 PROGRESS heredoc into a scratch file (the values expand: `rose from 203 to 787, the trace ratchet stays 2088`).
- Not run (needs the capture): Task 6, Task 7 on `gp-u5-charsel`, Task 8 Steps 1–4 on `gp-u5-charsel`. The Task 2-alt branch was not taken.

## Execution notes

- **Order:** 0 → 1 → (2 → 3 | 2-alt) → 4 → 5 → [Decision 1] → 6 → 7 → 8 → 9. Tasks 4–5 do not depend on 2–3 and may run before them if the gate is delayed, but the ratchet values of Task 7 must be measured on the fixed port (after Task 2), or the walk's frames inherit divergence 1.
- **Model tiers:** Task 1 and Task 7 (evidence and triage) — the strongest available model; Tasks 2, 3, 5, 8 — a standard implementer model (exact code given); Tasks 0, 4, 6, 9 — a fast model is sufficient, with the reviewer checking every quoted output against the logs.
- **The gate after every task:** `$S/mkv verify` exit 0 and the gate check (45 oracle lines, K11 lines, WAV, counters). Expect about 13 minutes per verify on this host (U4 §G.24 item 7: 761 s; the walk adds a ~25 s replay and a short compare).
- **Merge note for the controller:** U5 changes two shared values (the `gp-idle-loss` miss set and `GP_IDLE_LOSS_MIN_FIRST`, Decision 2) and adds a `charsel` flag to `fnm_known` that U7/U8 may extend in the same lines; after any merge that touches either, re-run `make gp-oracle` and `make gp-charsel-oracle` and re-measure rather than resolving numbers by hand.
