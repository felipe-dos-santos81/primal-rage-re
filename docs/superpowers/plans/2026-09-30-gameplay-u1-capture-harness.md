# Gameplay U1 — Capture Harness v2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A DOSBox-X capture harness for play: frame-keyed key and pad injection (P1/P2, chords, holds), a per-frame consistent snapshot log of the frame counter, the pad words, the command words, the RNG and the match state, and a frame-keyed port script (v2) the port driver of U2 replays.

**Architecture:** Two new tools beside the K11 ones, which stay byte-unchanged. `tools/gp_session.py` is pure (stdlib): the constants, the pad table, the scenario scheduler, the `poll.log` v2 format, the port-script v2 generator and the trace diff. `tools/gp_capture.py` runs the pinned original in DOSBox-X with `DX-CAPTURE`, and a poller thread on the memory file takes one snapshot per frame while the master loop spins (`DS_0010150C − 1 == DS_00101508`, spec §3.1), fires the scheduled injections from that state, and writes `poll.log`; after the run it streams the distinct AVI frames to gzip files.

**Tech Stack:** Python 3 stdlib (`unittest`, `gzip`, `mmap`, `threading`), `capstone` for Task 1's raw reads only, DOSBox-X 2026.08.31 (`/opt/homebrew/bin/dosbox-x`), `ffmpeg`/`ffprobe`.

**Spec:** `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md` (§3 facts, §4.1 this unit, §5 rules, §7 open questions).

**Derivation record (created in Task 0, shared by U1–U4):** `docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md`, sections `§G.0..` in task order.

**Where to run.** In the main checkout `/Users/felipe.dos.santos/code/mine/primal-rage-reverse` on branch `gameplay-ground-truth` (a worktree has no `data/`). Scratch: `S=/tmp/gameplay-u1`. Every command assumes `cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse`.

**Before Task 2: spec §7 Q4 needs the user's answer** (pad presses queue the key's config BIOS word once per press, no typematic repeat). If the user declines, Task 2's `PAD` table keeps the words but `pad` actions pass `bios=False`; record the decision in §G.2.

---

## Global Constraints

- Spec §5: "Raw wins; never a fitted constant; a value that cannot be pinned is a named gap with its evidence. Harness values (`HOLD_FRAMES`, the 150-frame gaps, `time_limit`, the `[p − 2, p + 64)` search order, `GP_LOOP_SLACK`) are named as harness values with their source, never presented as game values."
- Spec §5: "`make verify` is the gate after every task; the oracle lines equal the U1 Task 0 baseline; the enforced front-end oracle, the demo-fight and attract cycle-2 ratchets and the K11 oracles stay green."
- Spec §5: "Tests: Python `unittest` under `tools/tests` (stdlib), C only `CHECK`/`CHECK_EQ_INT`; every assertion can fail and each new test is shown failing under a named mutation."
- Spec §5: "Writes under `data/` only to `data/k11-captures/gp-*`; scratch under `/tmp`. Never `pkill`; stage named files; commit trailer `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`."
- Spec §4.1: "New files, so the K11 tools and their oracle stay byte-unchanged." Do not edit `tools/k11_*.py`, `tools/title_capture.py`, `tools/smk_capture.py`.
- AGENTS.md: "Raw-file disassembly … those bytes are pre-fixup … use Ghidra (fixups applied) for any data address". In capstone listings a data displacement shows as VA − `0x80000` (`[0x6f6dc]` is `DS_000EF6DC`).
- AGENTS.md: "Captures are git-ignored. `data/` is git-ignored and read-only". `gp_capture.py` writes only below `data/k11-captures/` (`k11_capture.guard_out`), and only `gp-` directories.
- AGENTS.md: "Commit style: `<area>: <what changed>`. **Never `git add -A`**". Only commit what the task names.

## Review Focus

1. **A snapshot taken mid-iteration.** A torn read (the ISR tick lands between two field reads) would log an `f` with the previous iteration's `raw`. Task 5's `test_consistent_rejects_a_moving_counter` pins the double-read rule; Task 8 cross-checks one pad press by `raw` in `S(F)` and not in `S(F − 1)`.
2. **The off-by-one between the injection and the frame that sees it.** An action for frame `F` must be written during the spin of `F − 1`. Task 3's `test_after_fires_at_the_spin_before_its_frame` and Task 8's measured press → `raw` frames pin it.
3. **A key or pad change the generator cannot pin, emitted anyway.** A missing `S(f − 1)` must fail the script, not guess. Task 4's `test_unpinned_key_is_an_error` and `test_bits_need_the_previous_snapshot`.
4. **Writing outside `data/k11-captures/gp-*`** or holding every frame in memory. Task 6's `test_guard_requires_gp_prefix` and `test_frames_stream_to_gzip`.
5. **The K11 oracle moving.** No K11 file changes; Task 7 diffs the oracle lines against the Task 0 baseline.

---

### Task 0: Baseline and the derivation record

**Files:**
- Create: `docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md`
- Create (git-ignored ledger): `.superpowers/sdd/2026-09-30-gameplay-u1-capture-harness/progress.md`

**Interfaces:**
- Consumes: branch `gameplay-ground-truth` with the spec commit on top of `934992a`
- Produces: `$S/or_base.txt` (the oracle lines every later task diffs against), record §G.0

- [ ] **Step 1: Clean tree on the branch**

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse
git status --porcelain        # expected: no output; otherwise stop and ask
git switch gameplay-ground-truth
git log --oneline -3          # expected: the plans commit, the spec commit, 934992a
mkdir -p /tmp/gameplay-u1
```

- [ ] **Step 2: Baseline gate**

```bash
S=/tmp/gameplay-u1
cmake -S port -B build && cmake --build build 2>&1 | tail -3
make verify > "$S/t0_verify.txt" 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|k11_compare)' "$S/t0_verify.txt" > "$S/or_base.txt"
wc -l < "$S/or_base.txt"; shasum -a 256 "$S/or_base.txt"
python3 tools/port_progress.py
```

Expected: `verify-exit=0`; `port_progress.py` prints `771 1203 64` and `731 731 100 …` (AGENTS.md). If either differs, stop.

- [ ] **Step 3: Fixed inputs**

```bash
shasum -a 256 data/game/C/PRAGE.EXE
python3 -c "import os;p='data/game/C/CMOS';d=open(p,'rb').read() if os.path.exists(p) else b'';print(len(d),sum(1 for b in d if b))"
make title-pin && shasum -a 256 /tmp/pr_title_pin/PRAGE.EXE
dosbox-x -version 2>&1 | grep -m1 'DOSBox-X version'
```

Expected: `eecba701576d36d1a217a271acd90e9aa4473121db8d51e8c8085c194ce0e91b`; `2040 0` (or `0 0` when absent); pinned `8120f1bd1df389ed94cb329c030f95d9e38caad193bbd9840717af557161a68d`; `DOSBox-X version 2026.08.31 SDL2 …`.

- [ ] **Step 4: Write the record skeleton**

```markdown
# Gameplay ground truth (U1–U4): derivation record

Spec: `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md`.
Plans: `docs/superpowers/plans/2026-09-30-gameplay-u{1,2,3,4}-*.md`. Every
claim cites a raw address, a `poll.log` line
(`data/k11-captures/<scenario>/poll.log:<n>`) or a capture frame
(`<scenario> frame <i> (raw <r>)`). On any conflict the raw wins; corrections
are recorded here with their address.

## §G.0 Baseline (U1 Task 0)
<HEAD, verify-exit, or_base.txt line count and sha256, port_progress lines,
the PRAGE.EXE / CMOS / pinned sha256s, the DOSBox-X version — verbatim>

## §G.1 Raw facts (U1 Task 1)
## §G.2 gp_session: constants, pads, log format (U1 Task 2)
## §G.3 The scheduler (U1 Task 3)
## §G.4 Port script v2 and trace diff (U1 Task 4)
## §G.5 gp_capture (U1 Tasks 5–6)
## §G.6 Make targets (U1 Task 7)
## §G.7 The gp-pads capture (U1 Task 8)
## §G.8 U1 closure (U1 Task 9)
```

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
docs: gameplay ground-truth derivation record and baseline (record §G.0)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 1: Raw facts (no game run)

**Files:**
- Modify: `docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md` (§G.1)
- Create (scratch): `$S/dx.py`

**Interfaces:**
- Produces: §G.1.1–§G.1.5, the addresses Tasks 2–6 cite (the spin predicate, the pad bits, the START MENU rows, the credits)

- [ ] **Step 1: The scratch disassembler**

```python
#!/usr/bin/env python3
"""Raw-file disassembly of PRAGE.EXE (pre-fixup operands: a data displacement
shows as VA - 0x80000). Usage: dx.py VA_HEX [LEN_HEX]"""
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
d = open('data/game/C/PRAGE.EXE', 'rb').read()
va = int(sys.argv[1], 16)
n = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x40
for i in Cs(CS_ARCH_X86, CS_MODE_32).disasm(d[va + 0x52E54:va + 0x52E54 + n], va):
    print('%08X  %-22s %s %s' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
```

- [ ] **Step 2: §G.1.1 the master loop and the counter**

```bash
python3 $S/dx.py 255CC 120
python3 $S/dx.py 24C5C 90
python3 $S/dx.py 1BDF4 3C
```

Expected (verified by the planner): `000255EE e8d1aa0200 call 0x500c4`, `0002560B e84cf6ffff call 0x24c5c`, `000256C0 ff050c150800 inc dword ptr [0x8150c]`, `000256C6..000256DB` the spin (`dec eax; cmp eax, dword ptr [0x81508]; jne 0x256dd; … call 0x5d7dc; jmp 0x256c6`); `00024C6E e8d1a90200 call 0x4f644` after `cmp eax, 0x27; je`; `00024CCD 668b3ddcf60600 mov di, word ptr [0x6f6dc]`, `00024CD4 47 inc edi`, `00024CDB 66893ddcf60600 mov word ptr [0x6f6dc], di`; the ISR `0001BE0E inc edx`, `0001BE10 mov dword ptr [0x81508], edx`, `0001BE16 mov dword ptr [0x81500], ebx`, `0001BE1C call 0x1bbac`. Record spec §3.1's consequence in these addresses: `0x500C4` samples before `0x24CDB`; the spin state is the consistent point.

- [ ] **Step 3: §G.1.2 the pad bits**

```bash
awk 'NR>=7310 && NR<=7420' port/decomp/prage.c | grep -n "0x2d\|0x2e\|0x28f\|0x290\|bVar1 = \|bVar1 |"
awk 'NR>=7486 && NR<=7603' port/decomp/prage.c | grep -n "2d8\|2d9\|FUN_0001b"
python3 -c "
import struct
d=open('data/game/C/PRAGE.EXE','rb').read(); o=0xA2C62+0x46E54
w=struct.unpack('<20H',d[o:o+0x28]); print([hex(x) for x in w])"
```

Expected: `0x1B610` tests `+0x2de..+0x2e1` into `0x80 0x40 0x20 0x10`; `0x1B730` `+0x2e2..+0x2e5` into `1 2 4 8`; `0x1B850` `+0x28f`; `0x1B6A0`/`0x1B7C0`/`0x1B870` the same with `+0x2e6..+0x2ed` and `+0x290`; `FUN_0001bbac` stores `+0x2d8` and `+0x2d9`. The config words: `['0x0', '0x1f73', '0x2d78', '0x2c7a', '0x2e63', '0x1675', '0x1769', '0x316e', '0x326d', '0x0', '0x4800', '0x5000', '0x4b00', '0x4d00', '0x4700', '0x4900', '0x4f00', '0x5100', …]`. Record spec §3.2's table verbatim with these addresses, and its status: raw-derived; the walk's `kb=0040` Down (record named-gaps-a §A.4) is the only capture evidence so far; Task 8 adds the rest.

- [ ] **Step 4: §G.1.3 START MENU and §G.1.4 credits**

```bash
rg -n "0x2CBC4|0x2CBDC|0x2CBF4|0x2CC0C|0x2CC24|0x2CC3C|0x2CC54" port/src/game/svcmenu.c | head -14
sed -n 60,72p docs/superpowers/plans/2026-09-29-k11-service-menu-derivations.md
python3 $S/dx.py 25071 5D
python3 $S/dx.py 2C304 18
```

Expected: the seven setters in the spec §3.3 order; the table rows `BCCDC … BCD3C`; `0x25071` ends `call 0x2ca7c` (`0x250BA`) and `call 0x257a4` (`0x250C4`) with `eax = 1`; `0x2C304` is `mov eax,0x29; call 0x2d974; and eax,0xf0000; sar eax,0x10; inc eax; mov [0x85c00],eax`. Record spec §3.3 and §3.4.

- [ ] **Step 5: §G.1.5 the planner's probe**

Copy spec §3.5–§3.7 into §G.1.5 under the heading "planner's probe (not reproduced; U4 Task 2 re-measures it with the harness)", citing the kept log `.superpowers/sdd/2026-09-30-gameplay-scope/planner-probe/probe.log` (check its sha256 `37785064bbe9b1c8dec31ff532f555e4bec1f86401d7417a28e1380f42258e2c` and record it). Do not rerun the probe.

- [ ] **Step 6: Commit**

```bash
git add docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
docs: gameplay raw facts: master loop order, pad bits, START MENU, credits (record §G.1)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: `tools/gp_session.py`: constants, pad table, the v2 log format

**Files:**
- Create: `tools/gp_session.py`
- Create: `tools/tests/test_gp_session.py`

**Interfaces:**
- Produces: `DATA_BASE_VA`, `KB_PTR_DS`, `KEYTAB_OFF`, `ENTER_WAIT`, `HOLD_FRAMES`, `BDA_HEAD`, `BDA_TAIL`, `SNAP_FIELDS` (tuple of `(name, ds, size)`), `TRACE_FIELDS`, `KEYS` (`name -> (scan, bios_word)`), `PAD` (`name -> (scan, bios_word, kb_bit)`), `raw_to_kb(raw) -> int`, `format_s(ms, vals, kb, head, tail) -> str`, `parse(line) -> dict | None`

- [ ] **Step 1: Write the failing tests**

```python
# tools/tests/test_gp_session.py
import os, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs


class TestTables(unittest.TestCase):
    def test_pad_bits_follow_the_isr_sampler(self):
        # spec §3.2: P1 byte is kb >> 8, P2 byte kb & 0xFF; up..right 0x80..0x10, b0..b3 1..8
        want = {'up': 0x80, 'down': 0x40, 'left': 0x20, 'right': 0x10,
                'b0': 0x01, 'b1': 0x02, 'b2': 0x04, 'b3': 0x08, 'start': 0x01}
        for n, bit in want.items():
            self.assertEqual(gs.PAD['p1.' + n][2], bit << 8, n)
            self.assertEqual(gs.PAD['p2.' + n][2], bit, n)

    def test_pad_scans_are_the_config_table_and_f1_f2(self):
        self.assertEqual([gs.PAD['p1.' + n][0] for n in ('up', 'down', 'left', 'right', 'b0', 'b1', 'b2', 'b3')],
                         [0x1F, 0x2D, 0x2C, 0x2E, 0x16, 0x17, 0x31, 0x32])
        self.assertEqual([gs.PAD['p2.' + n][0] for n in ('up', 'down', 'left', 'right', 'b0', 'b1', 'b2', 'b3')],
                         [0x48, 0x50, 0x4B, 0x4D, 0x47, 0x49, 0x4F, 0x51])
        self.assertEqual((gs.PAD['p1.start'][0], gs.PAD['p2.start'][0]), (0x3B, 0x3C))

    def test_raw_to_kb(self):
        # 0x500C4: raw = (+0x2D8 << 24) | (+0x2D9 << 8)
        self.assertEqual(gs.raw_to_kb(0x81004000), 0x8140)
        self.assertEqual(gs.raw_to_kb(0x00000100), 0x0001)
        self.assertEqual(gs.raw_to_kb(0), 0)

    def test_snap_fields_cover_the_spin_predicate_and_trace(self):
        names = [n for n, _, _ in gs.SNAP_FIELDS]
        for n in ('f', 't508', 't50c', 'raw', 'pad', 'e0', 'e2', 'rng') + gs.TRACE_FIELDS:
            self.assertIn(n, names)
        self.assertEqual(dict((n, (d, s)) for n, d, s in gs.SNAP_FIELDS)['f'], (0x0EF6DC, 2))


class TestFormat(unittest.TestCase):
    def test_s_record_round_trip(self):
        vals = {n: (i * 0x1111) & ((1 << (8 * s)) - 1) for i, (n, _, s) in enumerate(gs.SNAP_FIELDS)}
        line = gs.format_s(1234, vals, 0x4000, 0x1E, 0x20)
        rec = gs.parse(line)
        self.assertEqual(rec['kind'], 'S')
        self.assertEqual(rec['ms'], 1234)
        for n in vals:
            self.assertEqual(rec[n], vals[n], n)
        self.assertEqual((rec['kb'], rec['head'], rec['tail']), (0x4000, 0x1E, 0x20))

    def test_parse_keeps_names_and_decimals(self):
        rec = gs.parse('I ms=10 f=011D step=2 press=p1.up scan=1F lin=00010093 old=FF bios=1F73 ring=1 late=0')
        self.assertEqual((rec['press'], rec['step'], rec['f'], rec['late']), ('p1.up', 2, 0x11D, 0))
        rec = gs.parse('E ms=5 reason=time-limit rc=0')
        self.assertEqual((rec['reason'], rec['rc']), ('time-limit', 0))
        self.assertIsNone(gs.parse('   '))


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run to verify it fails**

Run: `python3 -m unittest tools.tests.test_gp_session -v`
Expected: `ModuleNotFoundError: No module named 'gp_session'`.

- [ ] **Step 3: Write `tools/gp_session.py` (this task's part)**

```python
#!/usr/bin/env python3
"""Gameplay ground-truth sessions (spec 2026-09-30-gameplay-ground-truth-design.md
§4.1, record §G.2..§G.4): the constants, the pad table, the frame-keyed
scenario scheduler, the poll.log v2 format, the port-script v2 generator and
the trace diff. Stdlib only; tools/gp_capture.py is the DOSBox-X side.

poll.log v2, one record per line:
  B ms=<int> base=<hex8> ptr=<hex8>
  S ms=<int> f=<hex4> <SNAP_FIELDS>=<hex> ... kb=<hex4> head=<hex4> tail=<hex4>
        a consistent end-of-iteration snapshot (the master loop's spin state)
  P ms=<int> f=<hex4> mode=<hex4> st=<hex4> tick=<hex8>   read outside a spin state
  H ms=<int> f=<hex4> head=<hex4>                         the BIOS head moved
  I ms=<int> f=<hex4> step=<n> press=<name> scan=<hex2> lin=<hex8> old=<hex2>
        bios=<hex4|-> ring=<0|1|-> late=<0|1>
  I ms=<int> f=<hex4> step=<n> release=<name> lin=<hex8>
  X ms=<int> f=<hex4> step=<n> end
  E ms=<int> reason=<exit|time-limit> rc=<int>"""
import argparse
import os
import sys

DATA_BASE_VA = 0x80000
KB_PTR_DS = 0x101514        # DS_00101514 -> the key block (+0x254 key state, +0x2D8/+0x2D9 bitmap)
KEYTAB_OFF = 0x254          # IRQ1 key-state table, bit 7 = released (demo-pose §50-C.3)
ENTER_WAIT = 25.0           # s, harness: the K11 boot wait (k11_session.ENTER_WAIT)
HOLD_FRAMES = 3             # harness: AUTOTYPE's 3-tick press (record named-gaps-a §A.9)
BDA_HEAD, BDA_TAIL = 0x41A, 0x41C

SNAP_FIELDS = (
    ('f', 0x0EF6DC, 2), ('mode', 0x104B00, 2), ('st', 0x0F0A64, 2),
    ('tick', 0x101500, 4), ('t508', 0x101508, 4), ('t50c', 0x10150C, 4),
    ('raw', 0x0E1C30, 4), ('pad', 0x0E1C34, 4), ('new', 0x1088E4, 4), ('held', 0x1088D8, 4),
    ('e0', 0x1088E0, 2), ('e2', 0x1088E2, 2), ('rng', 0x0EF6D8, 4),
    ('cred', 0x105C00, 4), ('fp', 0x105D60, 1), ('b1d', 0x104B1D, 1), ('b1f', 0x104B1F, 1),
    ('b25', 0x104B25, 1), ('w10d', 0x10810D, 1), ('cnt', 0x108110, 1),
    ('s0_52', 0x107802, 1), ('s0_54', 0x107804, 1), ('s0_5a', 0x10780A, 1),
    ('s1_52', 0x107896, 1), ('s1_54', 0x107898, 1), ('s1_5a', 0x10789E, 1),
    ('ent', 0x10741C, 4),
)
TRACE_FIELDS = ('mode', 'st', 'raw', 'pad', 'e0', 'e2', 'rng', 'cred', 's0_5a', 's1_5a')

# BIOS keys: (scan, the BIOS word a press queues; record named-gaps-a §A.9).
KEYS = {'enter': (0x1C, 0x1C0D), 'esc': (0x01, 0x011B)}
# Pads (spec §3.2): (scan, BIOS word, kb bit). Letters and P2's keys use the
# config words at 0x122C62 (record §A.1.5); arrows the E0 form AUTOTYPE left
# (record §A.9); F1/F2 the standard make words. Spec §7 Q4 governs whether a
# pad press queues its word (gp_capture --no-pad-bios turns it off).
PAD = {
    'p1.up': (0x1F, 0x1F73, 0x8000), 'p1.down': (0x2D, 0x2D78, 0x4000),
    'p1.left': (0x2C, 0x2C7A, 0x2000), 'p1.right': (0x2E, 0x2E63, 0x1000),
    'p1.b0': (0x16, 0x1675, 0x0100), 'p1.b1': (0x17, 0x1769, 0x0200),
    'p1.b2': (0x31, 0x316E, 0x0400), 'p1.b3': (0x32, 0x326D, 0x0800),
    'p1.start': (0x3B, 0x3B00, 0x0100),
    'p2.up': (0x48, 0x48E0, 0x0080), 'p2.down': (0x50, 0x50E0, 0x0040),
    'p2.left': (0x4B, 0x4BE0, 0x0020), 'p2.right': (0x4D, 0x4DE0, 0x0010),
    'p2.b0': (0x47, 0x4700, 0x0001), 'p2.b1': (0x49, 0x4900, 0x0002),
    'p2.b2': (0x4F, 0x4F00, 0x0004), 'p2.b3': (0x51, 0x5100, 0x0008),
    'p2.start': (0x3C, 0x3C00, 0x0001),
}

_DEC = ('ms', 'rc', 'step', 'late', 'ring')
_TEXT = ('reason', 'press', 'release')


def raw_to_kb(raw):
    """DS_000E1C30 = (+0x2D8 << 24) | (+0x2D9 << 8) (0x500C4) -> the kb word."""
    return ((raw >> 24) & 0xFF) << 8 | (raw >> 8) & 0xFF


def format_s(ms, vals, kb, head, tail):
    body = ' '.join('%s=%0*X' % (n, 2 * sz, vals[n]) for n, _, sz in SNAP_FIELDS)
    return 'S ms=%d %s kb=%04X head=%04X tail=%04X' % (ms, body, kb, head, tail)


def parse(line):
    parts = line.split()
    if not parts:
        return None
    rec = {'kind': parts[0]}
    for part in parts[1:]:
        key, eq, val = part.partition('=')
        if not eq:
            rec[key] = True                 # the X record's bare `end`
        elif key in _TEXT:
            rec[key] = val
        elif val == '-':
            rec[key] = None
        elif key in _DEC:
            rec[key] = int(val)
        else:
            rec[key] = int(val, 16)
    return rec
```

(`ring` is decimal 0/1, so `_DEC` holds it; `bios=-` parses to `None`.)

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_gp_session -v`
Expected: `Ran 6 tests … OK`.

- [ ] **Step 5: Mutation proof**

Swap `'p1.b0'` and `'p1.b1'` bits in `PAD`; run the tests: expected `FAIL: test_pad_bits_follow_the_isr_sampler`. Restore; `OK`. Record both lines in §G.2 with spec §7 Q4's decision.

- [ ] **Step 6: Commit**

```bash
git add tools/gp_session.py tools/tests/test_gp_session.py docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp_session constants, pad table and the poll.log v2 format (record §G.2)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: The frame-keyed scheduler

**Files:**
- Modify: `tools/gp_session.py` (append)
- Modify: `tools/tests/test_gp_session.py` (append `TestSchedule`)

**Interfaces:**
- Consumes: `KEYS`, `PAD`, `HOLD_FRAMES` (Task 2)
- Produces: `SCENARIOS` (`name -> dict(time_limit=int, steps=tuple)`), `class Schedule(steps)` with `on_mode(f, mode)` (feed every observed mode), `due_boot(now_s) -> list[(step, action)]`, `due(f) -> list[(step, action)]` (call at a spin snapshot `f`), `end_frame -> int | None`, `ended(f) -> bool`, `fired -> int`, `total -> int`; `expand(action) -> list[(name, scan, bios_word, hold_frames)]`

- [ ] **Step 1: Write the failing tests**

```python
class TestSchedule(unittest.TestCase):
    STEPS = (('boot', 25.0, ('key', 'enter')),
             ('after_mode', 0x27, 150, ('key', 'enter')),
             ('after', 150, ('pad', ('p1.up', 'p1.b1'), 4)),
             ('until_mode', 0x03, 2))

    def test_boot_fires_on_wall_time_only(self):
        s = gs.Schedule(self.STEPS)
        self.assertEqual(s.due_boot(24.9), [])
        self.assertEqual(s.due_boot(25.0), [(0, ('key', 'enter'))])
        self.assertEqual(s.due_boot(99.0), [])

    def test_after_fires_at_the_spin_before_its_frame(self):
        s = gs.Schedule(self.STEPS)
        s.due_boot(25.0)
        s.on_mode(0x120, 0x27)
        self.assertEqual(s.due(0x120 + 148), [])
        self.assertEqual(s.due(0x120 + 149), [(1, ('key', 'enter'))])   # frame F = f0 + 150
        self.assertEqual(s.due(0x120 + 150 + 148), [])
        self.assertEqual(s.due(0x120 + 150 + 149), [(2, ('pad', ('p1.up', 'p1.b1'), 4))])

    def test_a_late_snapshot_still_fires_once(self):
        s = gs.Schedule(self.STEPS)
        s.due_boot(25.0)
        s.on_mode(0x120, 0x27)
        self.assertEqual(s.due(0x120 + 160), [(1, ('key', 'enter'))])
        self.assertEqual(s.due(0x120 + 161), [])

    def test_until_mode_counts_only_after_the_last_action(self):
        s = gs.Schedule(self.STEPS)
        s.on_mode(0x10, 0x03)                  # mode 3 before the steps: ignored
        s.due_boot(25.0)
        s.on_mode(0x120, 0x27)
        s.due(0x120 + 149)
        s.due(0x120 + 299)
        self.assertIsNone(s.end_frame)
        s.on_mode(0x2000, 0x03)
        self.assertEqual(s.end_frame, 0x2002)
        self.assertFalse(s.ended(0x2001))
        self.assertTrue(s.ended(0x2002))
        self.assertEqual((s.fired, s.total), (3, 3))

    def test_expand(self):
        self.assertEqual(gs.expand(('key', 'enter')), [('enter', 0x1C, 0x1C0D, gs.HOLD_FRAMES)])
        self.assertEqual(gs.expand(('pad', ('p1.up', 'p2.b3'), 4)),
                         [('p1.up', 0x1F, 0x1F73, 4), ('p2.b3', 0x51, 0x5100, 4)])
        with self.assertRaises(KeyError):
            gs.expand(('pad', ('p3.up',), 1))
```

- [ ] **Step 2: Run to verify it fails**

Run: `python3 -m unittest tools.tests.test_gp_session -v`
Expected: `AttributeError: module 'gp_session' has no attribute 'Schedule'`.

- [ ] **Step 3: Implement**

```python
# Scenarios (spec §4.1). Harness values: the 150-frame gaps follow the
# planner's probe's 2.5 s (spec §3.5); time limits cover the probe's timings.
SCENARIOS = {
    # U1 Task 8: every pad name, two frames each, 30 apart, on the MAIN MENU
    # (mode 0x27: 0x500C4 runs every frame, spec §3.1).
    'gp-pads': dict(time_limit=75, steps=(
        ('boot', ENTER_WAIT, ('key', 'enter')),
        ('after_mode', 0x27, 120, ('pad', ('p1.up',), 2)),
    ) + tuple(('after', 30, ('pad', (n,), 2)) for n in (
        'p1.down', 'p1.left', 'p1.right', 'p1.b0', 'p1.b1', 'p1.b2', 'p1.b3', 'p1.start',
        'p2.up', 'p2.down', 'p2.left', 'p2.right', 'p2.b0', 'p2.b1', 'p2.b2', 'p2.b3', 'p2.start'))
      + (('after', 30, ('pad', ('p1.up', 'p1.b1', 'p2.left'), 5)),     # a chord held 5
         ('after', 60, ('end',))),
    ),
}


def expand(action):
    """An action -> [(name, scan, bios_word, hold_frames)]."""
    if action[0] == 'key':
        scan, word = KEYS[action[1]]
        return [(action[1], scan, word, HOLD_FRAMES)]
    if action[0] == 'pad':
        return [(n, PAD[n][0], PAD[n][1], action[2]) for n in action[1]]
    return []


class Schedule:
    """Steps: ('boot', s, act) | ('after_mode', mode, n, act) | ('after', n, act)
    | ('until_mode', mode, n); an ('end',) action ends the scenario at its frame.
    An action for frame F fires at the first spin snapshot with f >= F - 1, so
    iteration F samples it (spec §3.1)."""

    def __init__(self, steps):
        self.steps = list(steps)
        self.i = 0
        self.prev_frame = None
        self.mode_first = {}
        self.end_frame = None
        self.fired = 0
        self.total = sum(1 for st in self.steps if st[0] != 'until_mode' and st[-1] != ('end',))

    def _target(self, st):
        if st[0] == 'after_mode':
            f0 = self.mode_first.get(st[1])
            return None if f0 is None else f0 + st[2]
        if st[0] == 'after':
            return None if self.prev_frame is None else self.prev_frame + st[1]
        return None

    def on_mode(self, f, mode):
        if self.i < len(self.steps):
            st = self.steps[self.i]
            if st[0] == 'until_mode' and mode == st[1] and self.end_frame is None:
                self.end_frame = f + st[2]
                self.i += 1
                return
        if self.i > 0 or self.steps[0][0] != 'boot':
            self.mode_first.setdefault(mode, f)

    def due_boot(self, now_s):
        if self.i < len(self.steps) and self.steps[self.i][0] == 'boot' and now_s >= self.steps[self.i][1]:
            self.i += 1
            self.fired += 1
            return [(self.i - 1, self.steps[self.i - 1][2])]
        return []

    def due(self, f):
        out = []
        while self.i < len(self.steps):
            st = self.steps[self.i]
            if st[0] in ('boot', 'until_mode'):
                break
            F = self._target(st)
            if F is None or f < F - 1:
                break
            self.prev_frame = F
            self.i += 1
            if st[-1] == ('end',):
                self.end_frame = F
                break
            self.fired += 1
            out.append((self.i - 1, st[-1]))
        return out

    def ended(self, f):
        return self.end_frame is not None and f >= self.end_frame
```

Note on `on_mode`: modes seen before the boot step fires are not recorded (so mode 3 of the attract does not satisfy a later `after_mode 3`); the capture calls `on_mode` for every `S` and `P` record in order.

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_gp_session -v`
Expected: `Ran 11 tests … OK`.

- [ ] **Step 5: Mutation proof**

Change `f < F - 1` to `f < F` in `due`; expected `FAIL: test_after_fires_at_the_spin_before_its_frame`. Restore; `OK`. Record in §G.3.

- [ ] **Step 6: Commit**

```bash
git add tools/gp_session.py tools/tests/test_gp_session.py docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp_session frame-keyed scheduler and the gp-pads scenario (record §G.3)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Port script v2 and the trace diff

**Files:**
- Modify: `tools/gp_session.py` (append, plus `main`)
- Modify: `tools/tests/test_gp_session.py` (append `TestPortScript`, `TestTraceDiff`)

**Interfaces:**
- Consumes: `parse`, `raw_to_kb`, `KEYS`, `PAD`, `TRACE_FIELDS`
- Produces: `class ScriptError(Exception)`; `port_script(name, lines) -> str` (the v2 text, spec §4.1); `snapshots(lines) -> dict[f, rec]`; `trace_diff(a_lines, b_lines) -> dict(first=int|None, field=str|None, compared=int, tick_first=int|None)`; CLI `gp_session.py port-script --scenario N --capture DIR --out FILE` and `gp_session.py trace-diff --a DIR --b DIR`

- [ ] **Step 1: Write the failing tests**

```python
def _s(f, head=0x1E, raw=0, mode=0x27, st=0, **kw):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, raw=raw, mode=mode, st=st, t508=1, t50c=2, **kw)
    return gs.format_s(0, vals, gs.raw_to_kb(raw), head, 0x30)


def _log(extra_after=()):
    L = ['B ms=0 base=00266000 ptr=0000FE20']
    L += [_s(f, mode=3) for f in range(0x118, 0x11E)]
    L.append('I ms=1 f=011D step=0 press=enter scan=1C lin=00010090 old=FF bios=1C0D ring=1 late=0')
    L += [_s(0x11E, mode=3), _s(0x11F, mode=3)]
    L.append('H ms=2 f=0120 head=0020')
    L += [_s(f, head=0x20) for f in range(0x120, 0x126)]
    L.append('I ms=3 f=0125 step=1 press=p1.up scan=1F lin=00010093 old=FF bios=1F73 ring=1 late=0')
    L.append('H ms=4 f=0126 head=0022')
    L += [_s(0x126, head=0x22, raw=0x80000000), _s(0x127, head=0x22, raw=0x80000000)]
    # 0x12A: a pad change with no BIOS word (a --no-pad-bios press): bits only
    L += [_s(f, head=0x22, raw=0x100 if f == 0x12A else 0) for f in range(0x128, 0x131)]
    L += list(extra_after)
    L.append('X ms=5 f=0130 step=2 end')
    L.append('E ms=6 reason=time-limit rc=0')
    return L


class TestPortScript(unittest.TestCase):
    def setUp(self):
        gs.SCENARIOS['_t'] = dict(time_limit=1, steps=())

    def test_script_keys_by_consumption_and_bits_by_raw(self):
        text = gs.port_script('_t', _log())
        lines = [l for l in text.splitlines() if not l.startswith('#')]
        self.assertEqual(lines, ['enter_frame 288', 'enter_state 0000',
                                 'key 288 1C 0D', 'key 294 1F 73',
                                 'bits 294 8000', 'bits 296 0000',
                                 'bits 298 0001', 'bits 299 0000', 'end 304'])

    def test_unpinned_key_is_an_error(self):
        L = [l for l in _log() if not (l.startswith('S ') and gs.parse(l)['f'] == 0x11F)]
        with self.assertRaises(gs.ScriptError):
            gs.port_script('_t', L)

    def test_bits_need_the_previous_snapshot(self):
        L = [l for l in _log() if not (l.startswith('S ') and gs.parse(l)['f'] == 0x129)]
        with self.assertRaises(gs.ScriptError):
            gs.port_script('_t', L)

    def test_enter_frame_must_be_the_first_key(self):
        L = [l.replace('mode=0027', 'mode=0003') if l.startswith('S ') and gs.parse(l)['f'] == 0x120 else l
             for l in _log()]
        with self.assertRaises(gs.ScriptError):
            gs.port_script('_t', L)


class TestTraceDiff(unittest.TestCase):
    def test_first_difference_and_tick_apart(self):
        a = [_s(f, rng=f) for f in range(10)]
        b = [_s(f, rng=f if f < 7 else 0) for f in range(10) if f != 3]
        b[0] = _s(0, rng=0, tick=5)
        r = gs.trace_diff(a, b)
        self.assertEqual((r['first'], r['field'], r['compared'], r['tick_first']), (7, 'rng', 9, 0))

    def test_identical_traces(self):
        a = [_s(f) for f in range(4)]
        self.assertEqual(gs.trace_diff(a, list(a))['first'], None)
```

- [ ] **Step 2: Run to verify it fails**

Run: `python3 -m unittest tools.tests.test_gp_session -v`
Expected: `AttributeError: module 'gp_session' has no attribute 'port_script'`.

- [ ] **Step 3: Implement**

```python
class ScriptError(Exception):
    pass


def snapshots(lines):
    out = {}
    for l in lines:
        r = parse(l)
        if r and r['kind'] == 'S':
            out.setdefault(r['f'], r)
    return out


def port_script(name, lines):
    """poll.log v2 -> port script v2 (spec §4.1). Keys at their consumption
    frame (the H record paired FIFO with the I press, pinned by S(f-1) showing
    the old head); bits at each change of S.raw, pinned by S(f-1)."""
    recs = [r for r in (parse(l) for l in lines) if r]
    snap = snapshots(lines)
    presses = [r for r in recs if r['kind'] == 'I' and 'press' in r and r.get('bios') is not None]
    heads = [r for r in recs if r['kind'] == 'H']
    if len(heads) < len(presses):
        raise ScriptError('%d BIOS words queued, %d consumed' % (len(presses), len(heads)))
    keys = []
    for k, (p, h) in enumerate(zip(presses, heads)):
        c = h['f']
        prev = snap.get(c - 1)
        if prev is None or prev['head'] == h['head']:
            raise ScriptError('key %d (%s) consumption at f=%X unpinned' % (k, p['press'], c))
        if c in snap and snap[c]['head'] != h['head']:
            raise ScriptError('key %d (%s): S(%X) disagrees with its H record' % (k, p['press'], c))
        word = p['bios']
        keys.append((c, word >> 8, word & 0xFF))
    p27 = next((r for r in recs if r['kind'] in ('S', 'P') and r.get('mode') == 0x27), None)
    if p27 is None:
        raise ScriptError('mode 0x27 never observed')
    if not keys or keys[0][0] != p27['f']:
        raise ScriptError('the first key (f=%s) is not the frame mode 0x27 appears (f=%X)'
                          % (keys[0][0] if keys else None, p27['f']))
    bits, prev_kb = [], 0
    for f in sorted(snap):
        if f < p27['f']:
            continue
        kb = raw_to_kb(snap[f]['raw'])
        if kb != prev_kb:
            if f - 1 not in snap:
                raise ScriptError('pad change at f=%X unpinned (no S record at f=%X)' % (f, f - 1))
            bits.append((f, kb))
            prev_kb = kb
    end = next((r for r in recs if r['kind'] == 'X'), None)
    if end is None:
        raise ScriptError('the scenario end (X record) was not reached')
    out = ['# gp port script v2: scenario %s' % name,
           'enter_frame %d' % p27['f'],
           'enter_state %04X' % p27['st']]
    ev = [(c, 0, 'key %d %02X %02X' % (c, s, a)) for c, s, a in keys]
    ev += [(f, 1, 'bits %d %04X' % (f, kb)) for f, kb in bits]
    out += [t for _, _, t in sorted(ev)]
    out.append('end %d' % end['f'])
    return '\n'.join(out) + '\n'


def trace_diff(a_lines, b_lines):
    a, b = snapshots(a_lines), snapshots(b_lines)
    common = sorted(set(a) & set(b))
    first = field = tick_first = None
    for f in common:
        if tick_first is None and a[f]['tick'] != b[f]['tick']:
            tick_first = f
        if first is None:
            for n in TRACE_FIELDS:
                if a[f][n] != b[f][n]:
                    first, field = f, n
                    break
    return dict(first=first, field=field, compared=len(common), tick_first=tick_first)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('port-script', 'trace-diff'))
    ap.add_argument('--scenario')
    ap.add_argument('--capture')
    ap.add_argument('--out')
    ap.add_argument('--a')
    ap.add_argument('--b')
    a = ap.parse_args()
    if a.cmd == 'trace-diff':
        with open(os.path.join(a.a, 'poll.log')) as fa, open(os.path.join(a.b, 'poll.log')) as fb:
            r = trace_diff(fa.read().splitlines(), fb.read().splitlines())
        print('gp_session: trace-diff: %d frames compared; first difference %s%s; first tick difference %s'
              % (r['compared'], 'none' if r['first'] is None else 'f=%X' % r['first'],
                 '' if r['field'] is None else ' (%s)' % r['field'],
                 'none' if r['tick_first'] is None else 'f=%X' % r['tick_first']))
        return 0
    with open(os.path.join(a.capture, 'poll.log')) as f:
        try:
            text = port_script(a.scenario, f.read().splitlines())
        except ScriptError as e:
            print('gp_session: port-script: %s: %s' % (a.scenario, e))
            return 1
    with open(a.out, 'w') as f:
        f.write(text)
    print('gp_session: port-script: %s: wrote %s (%d lines)' % (a.scenario, a.out, text.count('\n')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
```

Check the expected lines by hand before running: the Enter's `H` is at `f = 0x120` (288) and `S(0x11F)` holds head `0x1E` ≠ `0x20`; p1.up's `H` at `0x126` (294) with `S(0x125)` head `0x20`; `raw` becomes `0x80000000` at `0x126` (kb `0x8000`) and 0 at `0x128` (296); `raw = 0x100` (kb `0x0001`) at `0x12A` (298) only, 0 again at `0x12B` (299); `X` at `0x130` (304). `test_bits_need_the_previous_snapshot` removes `S(0x129)`, which no key needs, so only the bits check can reject it.

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_gp_session -v`
Expected: `Ran 17 tests … OK`.

- [ ] **Step 5: Mutation proofs**

(a) Drop the `prev is None or` guard in the key pinning: expected `ERROR` or `FAIL` in `test_unpinned_key_is_an_error`. (b) Remove the `f - 1 not in snap` check: expected `FAIL: test_bits_need_the_previous_snapshot`. Restore each; `OK`. Record in §G.4.

- [ ] **Step 6: Commit**

```bash
git add tools/gp_session.py tools/tests/test_gp_session.py docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp_session port script v2 (frame-keyed) and trace diff (record §G.4)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: `tools/gp_capture.py`: snapshot, spin predicate, injector

**Files:**
- Create: `tools/gp_capture.py`
- Create: `tools/tests/test_gp_capture.py`

**Interfaces:**
- Consumes: `gp_session` (Tasks 2–4), `k11_capture.find_base`, `k11_capture.bios_insert`, `k11_capture.u16`
- Produces: `read_snap(mm, base) -> dict`, `spinning(vals) -> bool`, `consistent(v1, v2) -> bool`, `class Injector(mm, ptr, log, pad_bios=True)` with `press(step, f, name, scan, word, hold, late) ` and `release_due(f, ms)`; `class Poller(threading.Thread)` (the run loop; used by Task 6)

- [ ] **Step 1: Write the failing tests**

```python
# tools/tests/test_gp_capture.py
import io, os, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_capture as gc
import gp_session as gs

BASE = 0x266000


def _mem():
    m = bytearray(0x400000)
    m[0x41A:0x41C] = (0x1E).to_bytes(2, 'little')      # BDA head/tail/start/end
    m[0x41C:0x41E] = (0x1E).to_bytes(2, 'little')
    m[0x480:0x482] = (0x1E).to_bytes(2, 'little')
    m[0x482:0x484] = (0x3E).to_bytes(2, 'little')
    return m


def _put(m, ds, size, v):
    o = BASE + ds - gs.DATA_BASE_VA
    m[o:o + size] = v.to_bytes(size, 'little')


class TestSnapshot(unittest.TestCase):
    def test_read_snap_uses_the_base(self):
        m = _mem()
        _put(m, 0x0EF6DC, 2, 0x1234)
        _put(m, 0x0EF6D8, 4, 0xDEADBEEF)
        v = gc.read_snap(m, BASE)
        self.assertEqual((v['f'], v['rng']), (0x1234, 0xDEADBEEF))

    def test_spinning_is_the_0x256C6_predicate_with_wrap(self):
        self.assertTrue(gc.spinning({'t50c': 2, 't508': 1}))
        self.assertFalse(gc.spinning({'t50c': 2, 't508': 2}))
        self.assertTrue(gc.spinning({'t50c': 0, 't508': 0xFFFFFFFF}))

    def test_consistent_rejects_a_moving_counter(self):
        v = {'f': 5, 't508': 1, 't50c': 2}
        self.assertTrue(gc.consistent(v, dict(v)))
        self.assertFalse(gc.consistent(v, dict(v, f=6)))
        self.assertFalse(gc.consistent(v, dict(v, t508=2)))


class TestInjector(unittest.TestCase):
    def test_press_holds_and_queues_once(self):
        m, log = _mem(), io.StringIO()
        ptr = 0xFE20
        m[ptr + gs.KEYTAB_OFF + 0x1F] = 0xFF
        inj = gc.Injector(m, ptr, log)
        inj.press(1, 0x100, 'p1.up', 0x1F, 0x1F73, 2, 0, 10)
        self.assertEqual(m[ptr + gs.KEYTAB_OFF + 0x1F], 0x7F)
        self.assertEqual(m[0x400 + 0x1E:0x400 + 0x20], (0x1F73).to_bytes(2, 'little'))
        inj.release_due(0x100, 11)
        self.assertEqual(m[ptr + gs.KEYTAB_OFF + 0x1F], 0x7F)       # held for iterations 0x101, 0x102
        inj.release_due(0x101, 12)
        self.assertEqual(m[ptr + gs.KEYTAB_OFF + 0x1F], 0xFF)       # released at the spin of 0x101
        recs = [gs.parse(l) for l in log.getvalue().splitlines()]
        self.assertEqual([r.get('press') or r.get('release') for r in recs], ['p1.up', 'p1.up'])
        self.assertEqual(recs[0]['bios'], 0x1F73)

    def test_pad_bios_off_queues_nothing(self):
        m, log = _mem(), io.StringIO()
        inj = gc.Injector(m, 0xFE20, log, pad_bios=False)
        inj.press(1, 0x100, 'p1.up', 0x1F, 0x1F73, 2, 0, 10)
        self.assertEqual(m[0x41C:0x41E], (0x1E).to_bytes(2, 'little'))
        inj.press(2, 0x100, 'enter', 0x1C, 0x1C0D, 3, 0, 10)          # a BIOS key still queues
        self.assertEqual(m[0x41C:0x41E], (0x20).to_bytes(2, 'little'))


if __name__ == '__main__':
    unittest.main()
```

(The release rule: a hold of `n` pressed at the spin of `f` releases at the spin of `f + n − 1`, so iterations `f + 1 .. f + n` sample it; with `n = 2` pressed at `0x100` that is the spin of `0x101`.)

- [ ] **Step 2: Run to verify it fails**

Run: `python3 -m unittest tools.tests.test_gp_capture -v`
Expected: `ModuleNotFoundError: No module named 'gp_capture'`.

- [ ] **Step 3: Implement the core**

```python
#!/usr/bin/env python3
"""Gameplay ground-truth capture (spec 2026-09-30-gameplay-ground-truth-design.md
§4.1, record §G.5). The pinned original runs in DOSBox-X under DX-CAPTURE /V /O.
A poller thread maps the [dosbox] memory file, takes one consistent snapshot
per frame while the master loop spins ([DS_0010150C] - 1 == [DS_00101508],
0x256C6), fires the scenario's frame-keyed injections from that state and
writes poll.log (format: gp_session). Writes only data/k11-captures/gp-*/.
Usage: gp_capture.py --scenario NAME --out data/k11-captures/NAME [--exe PATH]
                     [--time-limit S] [--keep-avi] [--no-pad-bios]"""
import argparse
import gzip
import hashlib
import mmap
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs
import k11_capture as kc
import smk_capture as sc
import title_capture as tcap


def read_snap(mm, base):
    out = {}
    for name, ds, size in gs.SNAP_FIELDS:
        o = base + ds - gs.DATA_BASE_VA
        out[name] = int.from_bytes(mm[o:o + size], 'little')
    return out


def spinning(v):
    return (v['t50c'] - 1) & 0xFFFFFFFF == v['t508']


def consistent(v1, v2):
    return v1['f'] == v2['f'] and v1['t508'] == v2['t508']


class Injector:
    """Writes key presses into the IRQ1 key-state table (bit 7 clear = down)
    and, for a key or (pad_bios) a pad, queues its BIOS word once."""

    def __init__(self, mm, ptr, log, pad_bios=True):
        self.mm, self.ptr, self.log, self.pad_bios = mm, ptr, log, pad_bios
        self.held = []            # [(release_at_f, lin, old, name, step)]

    def press(self, step, f, name, scan, word, hold, late, ms):
        lin = self.ptr + gs.KEYTAB_OFF + scan
        old = self.mm[lin]
        self.mm[lin] = old & 0x7F
        self.held.append((f + hold - 1, lin, old, name, step))
        bios = word if (name in gs.KEYS or self.pad_bios) else None
        ring = kc.bios_insert(self.mm, word) if bios is not None else None
        self.log.write('I ms=%d f=%04X step=%d press=%s scan=%02X lin=%08X old=%02X bios=%s ring=%s late=%d\n'
                       % (ms, f, step, name, scan, lin, old, '-' if bios is None else '%04X' % bios,
                          '-' if ring is None else int(ring), late))

    def release_due(self, f, ms):
        for h in [h for h in self.held if f >= h[0]]:
            self.mm[h[1]] = h[2] | 0x80
            self.held.remove(h)
            self.log.write('I ms=%d f=%04X step=%d release=%s lin=%08X\n' % (ms, f, h[4], h[3], h[1]))
```

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_gp_capture -v`
Expected: `Ran 5 tests … OK`.

- [ ] **Step 5: Mutation proof**

In `release_due` use `f > h[0]`; expected `FAIL: test_press_holds_and_queues_once`. In `consistent` drop the `t508` compare; expected `FAIL: test_consistent_rejects_a_moving_counter`. Restore; `OK`. Record in §G.5.

- [ ] **Step 6: Commit**

```bash
git add tools/gp_capture.py tools/tests/test_gp_capture.py docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp_capture snapshot, spin predicate and injector (record §G.5)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: `gp_capture.py`: the poller, the run, the frames and the checks

**Files:**
- Modify: `tools/gp_capture.py` (append)
- Modify: `tools/tests/test_gp_capture.py` (append)

**Interfaces:**
- Consumes: Task 5; `k11_capture.guard_out`, `check_cmos`, `find_base`, `u16`; `title_capture.stage`, `post_logo_start`, `collapse_from`, `PIN_EXE`; `smk_capture.which`, `read_avi_frames`, `ffprobe_fps`
- Produces: `guard_gp(out) -> str`; `write_frames(out, paths, indices) -> int` (streams `frame_%05d.raw.gz`); `class Poller`; `main()`; the capture directory layout `poll.log`, `frame_%05d.raw.gz`, `window.txt`, `session.txt`, `dosbox.log`, `*.dro`, `last_frame.png`

- [ ] **Step 1: Write the failing tests**

```python
class TestOutput(unittest.TestCase):
    def test_guard_requires_gp_prefix(self):
        ok = os.path.join(ROOT, 'data', 'k11-captures', 'gp-x')
        self.assertEqual(gc.guard_gp(ok), os.path.realpath(ok))
        for bad in (os.path.join(ROOT, 'data', 'k11-captures', 'walk'),
                    os.path.join(ROOT, 'data', 'game', 'C', 'gp-x'), '/tmp/gp-x'):
            with self.assertRaises(SystemExit):
                gc.guard_gp(bad)

    def test_frames_stream_to_gzip(self):
        import shutil, tempfile
        d = tempfile.mkdtemp(prefix='gpcap-test-')
        self.addCleanup(shutil.rmtree, d, True)
        frames = [bytes([i]) * gc.FRAME_BYTES for i in range(5)]
        orig = gc.sc.read_avi_frames
        gc.sc.read_avi_frames = lambda paths: iter(frames)
        try:
            n = gc.write_frames(d, ['x.avi'], [1, 3, 4])
        finally:
            gc.sc.read_avi_frames = orig
        self.assertEqual(n, 3)
        import gzip
        for k, raw in enumerate((1, 3, 4)):
            with gzip.open(os.path.join(d, 'frame_%05d.raw.gz' % k)) as f:
                self.assertEqual(f.read(), frames[raw])
        with open(os.path.join(d, 'window.txt')) as f:
            self.assertEqual(f.read().split('\n')[-2], '00002 4')
```

- [ ] **Step 2: Run to verify it fails**

Run: `python3 -m unittest tools.tests.test_gp_capture -v`
Expected: `AttributeError: module 'gp_capture' has no attribute 'guard_gp'`.

- [ ] **Step 3: Implement**

```python
FRAME_BYTES = 320 * 200 * 3
LOG_CON_ARGS = ['-set', 'dos log console=quiet']      # record named-gaps-a §A.2


def guard_gp(out):
    real = kc.guard_out(out)
    if not os.path.basename(real).startswith('gp-'):
        raise SystemExit('gp_capture: --out must be data/k11-captures/gp-<scenario>: %s' % out)
    return real


def write_frames(out, paths, indices):
    """Stream the AVI frames at `indices` (ascending raw indices) to
    frame_%05d.raw.gz; window.txt maps each to its raw index."""
    want = {ix: k for k, ix in enumerate(indices)}
    n = 0
    for i, data in enumerate(sc.read_avi_frames(paths)):
        k = want.get(i)
        if k is None:
            continue
        with gzip.open(os.path.join(out, 'frame_%05d.raw.gz' % k), 'wb', compresslevel=1) as f:
            f.write(data)
        n += 1
    with open(os.path.join(out, 'window.txt'), 'w') as f:
        for k, raw in enumerate(indices):
            f.write('%05d %d\n' % (k, raw))
    return n


def dosbox_cmd(root, game, iso, time_limit):
    return ([sc.which('dosbox-x'), '-defaultconf', '-fastlaunch', '-nopromptfolder',
             '-nogui', '-nomenu', '-time-limit', str(time_limit),
             '-set', 'sdl fullscreen=false',
             '-set', 'dosbox captures=%s' % os.path.join(root, 'avi'),
             '-set', 'dosbox memory file=%s' % os.path.join(root, 'guest.mem'),
             '-set', 'log logfile=%s' % os.path.join(root, 'dosbox.log')]
            + LOG_CON_ARGS
            + ['-c', 'MOUNT C "%s" -ro' % game, '-c', 'IMGMOUNT D "%s" -t iso' % iso, '-c', 'C:',
               '-c', 'DX-CAPTURE /V /O PRAGE.EXE -f', '-c', 'EXIT'])


class Poller(threading.Thread):
    def __init__(self, mem_path, log_path, steps, stop, pad_bios=True):
        super().__init__(daemon=True)
        self.mem_path, self.log_path, self.stop, self.pad_bios = mem_path, log_path, stop, pad_bios
        self.sched = gs.Schedule(steps)
        self.end_seen = False

    def run(self):
        deadline = time.monotonic() + 60
        while not (os.path.exists(self.mem_path) and os.path.getsize(self.mem_path) >= 1 << 20):
            if self.stop.is_set() or time.monotonic() > deadline:
                return
            time.sleep(0.05)
        with open(self.mem_path, 'r+b') as fh, open(self.log_path, 'w') as log:
            mm = mmap.mmap(fh.fileno(), 0)
            t0 = time.monotonic()
            base = ptr = inj = None
            last_f = last_ms = None
            head = None
            while not self.stop.is_set():
                now = time.monotonic() - t0
                ms = int(now * 1000)
                if base is None:
                    base = kc.find_base(mm)
                    if base is None:
                        time.sleep(0.2)
                    continue
                v = read_snap(mm, base)
                if ptr is None:
                    ptr = int.from_bytes(mm[base + gs.KB_PTR_DS - gs.DATA_BASE_VA:][:4], 'little')
                    if ptr == 0:
                        ptr = None
                        continue
                    log.write('B ms=%d base=%08X ptr=%08X\n' % (ms, base, ptr))
                    inj = Injector(mm, ptr, log, self.pad_bios)
                h = kc.u16(mm, gs.BDA_HEAD)
                if head is not None and h != head:
                    f_after = read_snap(mm, base)['f']
                    log.write('H ms=%d f=%04X head=%04X\n' % (ms, f_after, h))
                head = h
                if (v['mode'], v['st']) != last_ms:
                    log.write('P ms=%d f=%04X mode=%04X st=%04X tick=%08X\n' % (ms, v['f'], v['mode'], v['st'], v['tick']))
                    self.sched.on_mode(v['f'], v['mode'])
                    last_ms = (v['mode'], v['st'])
                for step, act in self.sched.due_boot(now):
                    for name, scan, word, hold in gs.expand(act):
                        inj.press(step, v['f'], name, scan, word, hold, 0, ms)
                if spinning(v) and v['f'] != last_f:
                    v2 = read_snap(mm, base)
                    if consistent(v, v2):
                        kb = mm[ptr + 0x2D8] << 8 | mm[ptr + 0x2D9]
                        log.write(gs.format_s(ms, v2, kb, kc.u16(mm, gs.BDA_HEAD), kc.u16(mm, gs.BDA_TAIL)) + '\n')
                        f = v2['f']
                        self.sched.on_mode(f, v2['mode'])
                        inj.release_due(f, ms)
                        for step, act in self.sched.due(f):
                            late = int(f != self.sched.prev_frame - 1)
                            for name, scan, word, hold in gs.expand(act):
                                inj.press(step, f, name, scan, word, hold, late, ms)
                        if not self.end_seen and self.sched.ended(f):
                            log.write('X ms=%d f=%04X step=%d end\n' % (ms, f, self.sched.i))
                            self.end_seen = True
                        last_f = f
                time.sleep(0.0003)
            log.write('E ms=%d reason=%s rc=%d\n' % (int((time.monotonic() - t0) * 1000),
                                                     getattr(self, 'reason', 'exit'), getattr(self, 'rc', -1)))
            mm.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--scenario', required=True, choices=sorted(n for n in gs.SCENARIOS if n.startswith('gp-')))
    ap.add_argument('--out', required=True)
    ap.add_argument('--game-dir', default=kc.DEFAULT_GAME_DIR)
    ap.add_argument('--exe', default=tcap.PIN_EXE)
    ap.add_argument('--time-limit', type=int)
    ap.add_argument('--keep-avi', action='store_true')
    ap.add_argument('--no-pad-bios', action='store_true')
    a = ap.parse_args()
    out = guard_gp(a.out)
    scn = gs.SCENARIOS[a.scenario]
    if not os.path.isfile(a.exe):
        sys.exit('gp_capture: %s missing (run make title-pin)' % a.exe)
    cmos = kc.check_cmos(a.game_dir)
    limit = a.time_limit or scn['time_limit']
    root = tempfile.mkdtemp(prefix='gpcap-')
    try:
        game = tcap.stage(root, a.exe, a.game_dir)
        iso = os.path.join(root, 'CD', 'RAGECD.ISO')
        os.makedirs(os.path.join(root, 'avi'))
        cmd = dosbox_cmd(root, game, iso, limit)
        print('gp_capture: %s' % shlex.join(cmd))
        stop = threading.Event()
        poll = Poller(os.path.join(root, 'guest.mem'), os.path.join(root, 'poll.log'),
                      scn['steps'], stop, pad_bios=not a.no_pad_bios)
        poll.start()
        t = time.monotonic()
        r = subprocess.run(cmd)
        wall = time.monotonic() - t
        poll.rc, poll.reason = r.returncode, ('time-limit' if wall >= limit - 1 else 'exit')
        stop.set()
        poll.join()
        avi_dir = os.path.join(root, 'avi')
        avis = sorted(f for f in os.listdir(avi_dir) if f.lower().endswith('.avi'))
        if not avis:
            sys.exit('gp_capture: no AVI in %s' % avi_dir)
        paths = [os.path.join(avi_dir, n) for n in avis]
        cap = [hashlib.md5(d).digest() for d in sc.read_avi_frames(paths)]
        start, logs = tcap.post_logo_start(cap, a.game_dir)
        _, idx = tcap.collapse_from(cap, start)
        os.makedirs(out, exist_ok=True)
        for old in os.listdir(out):
            if old.endswith('.raw.gz'):
                os.remove(os.path.join(out, old))
        n = write_frames(out, paths, idx)
        for name in ('poll.log', 'dosbox.log'):
            src = os.path.join(root, name)
            if os.path.exists(src):
                shutil.copyfile(src, os.path.join(out, name))
        dros = sorted(f for f in os.listdir(avi_dir) if f.lower().endswith('.dro'))
        for d in dros:
            shutil.copyfile(os.path.join(avi_dir, d), os.path.join(out, d))
        subprocess.run([sc.which('ffmpeg'), '-v', 'error', '-y', '-sseof', '-1', '-i', paths[-1],
                        '-update', '1', os.path.join(out, 'last_frame.png')])
        ver = subprocess.run([sc.which('dosbox-x'), '-version'], capture_output=True, text=True)
        ver = ver.stdout + ver.stderr
        with open(a.exe, 'rb') as fx:
            sha = hashlib.sha256(fx.read()).hexdigest()
        with open(os.path.join(out, 'session.txt'), 'w') as f:
            f.write('scenario=%s\n' % a.scenario)
            f.write('dosbox=%s\n' % next((l for l in ver.splitlines() if 'DOSBox-X version' in l), '?'))
            f.write('argv=%s\n' % shlex.join(cmd))
            f.write('exe=%s sha256=%s\n' % (a.exe, sha))
            f.write('cmos=%s pad_bios=%d\n' % (cmos, int(not a.no_pad_bios)))
            f.write('time_limit=%d wall_s=%.1f rc=%d\n' % (limit, wall, r.returncode))
            f.write('avis=%s fps=%.4f dro=%s frames=%d raw_window=%d..%d twg_last=%s\n'
                    % (avis, sc.ffprobe_fps(paths[0]), dros, n, idx[0], idx[-1], logs.get('twg')))
        with open(os.path.join(out, 'poll.log')) as f:
            lines = f.read().splitlines()
        recs = [gs.parse(l) for l in lines if l.strip()]
        try:
            gs.port_script(a.scenario, lines)
            script_ok, why = True, ''
        except gs.ScriptError as e:
            script_ok, why = False, ' (%s)' % e
        checks = [('base', any(x['kind'] == 'B' for x in recs)),
                  ('steps fired %d/%d' % (poll.sched.fired, poll.sched.total), poll.sched.fired == poll.sched.total),
                  ('end frame reached', any(x['kind'] == 'X' for x in recs)),
                  ('mode 0x27 after the Enter', any(x['kind'] in ('S', 'P') and x.get('mode') == 0x27 for x in recs)),
                  ('port script v2%s' % why, script_ok)]
        snaps = sorted(gs.snapshots(lines))
        missed = sum(b - a - 1 for a, b in zip(snaps, snaps[1:]) if b > a + 1)
        print('gp_capture: snapshots %d, f %s..%s, %d frames missed (spec §3.7)'
              % (len(snaps), snaps and '%X' % snaps[0], snaps and '%X' % snaps[-1], missed))
        for name, ok in checks:
            print('gp_capture: CHECK %s: %s' % (name, 'ok' if ok else 'FAIL'))
        print('gp_capture: wrote %d frames to %s (raw %d..%d), wall %.1fs' % (n, out, idx[0], idx[-1], wall))
        return 0 if all(ok for _, ok in checks) else 1
    finally:
        if a.keep_avi:
            print('gp_capture: kept %s' % root)
        else:
            shutil.rmtree(root, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
```

Also add the missing name to `k11_capture` use: `kc.DEFAULT_GAME_DIR` exists in `k11_capture.py` (line 40) — do not edit that file.

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_gp_capture tools.tests.test_gp_session -v`
Expected: `Ran 24 tests … OK` (7 + 17).

- [ ] **Step 5: Mutation proof**

Drop the `startswith('gp-')` check in `guard_gp`: expected `FAIL: test_guard_requires_gp_prefix`. Restore; `OK`. Record in §G.5.

- [ ] **Step 6: Commit**

```bash
git add tools/gp_capture.py tools/tests/test_gp_capture.py docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp_capture poller, DOSBox-X run, gzip frames and checks (record §G.5)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: `make gp-capture` and the verify tool-test line

**Files:**
- Modify: `Makefile` (the `.PHONY` list near line 52; a `gp-capture` target after `k11-report`; the `verify` tool-test line)
- Modify: record §G.6

**Interfaces:**
- Produces: `make gp-capture scenario=gp-pads` (writes `data/k11-captures/gp-pads/`)

- [ ] **Step 1: Add the target**

After the `k11-report` recipe:

```make
# Gameplay capture (spec 2026-09-30-gameplay-ground-truth-design.md §4.1): the
# pinned original under DOSBox-X with frame-keyed injection and a per-frame
# snapshot log. Writes only data/k11-captures/gp-<scenario>/ (gp_capture.guard_gp).
gp-capture: title-pin ## Capture a gameplay scenario (scenario=gp-pads|gp-idle-loss; writes data/k11-captures/)
	$(PYTHON) tools/gp_capture.py --scenario $(scenario) --out $(K11_CAPTURES)/$(scenario) --exe $(TITLE_PIN_DIR)/PRAGE.EXE $(GP_ARGS)
```

Add `gp-capture` to the `.PHONY` list. In `verify`, extend the tool-test line to
`PR_ORACLE_REQUIRED=1 $(PYTHON) -m unittest tools.tests.test_k11_fields tools.tests.test_k11_session tools.tests.test_k11_capture tools.tests.test_k11_compare tools.tests.test_gp_session tools.tests.test_gp_capture`.

- [ ] **Step 2: Gate**

```bash
S=/tmp/gameplay-u1
make help | grep gp-capture
make verify > "$S/t7_verify.txt" 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|k11_compare)' "$S/t7_verify.txt" | diff - "$S/or_base.txt" && echo ORACLES-EQUAL
grep -E '^Ran [0-9]+ tests' "$S/t7_verify.txt"
```

Expected: the help line; `verify-exit=0`; `ORACLES-EQUAL`; the tool-test line's `Ran N tests` equals the Task 0 baseline's K11 count plus 24 (17 gp_session + 7 gp_capture). Record in §G.6.

- [ ] **Step 3: Commit**

```bash
git add Makefile docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
build: make gp-capture and the gp tool tests in make verify (record §G.6)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: The `gp-pads` capture: the pad map and frame-exactness (investigation)

**Files:**
- Modify: record §G.7
- Writes (git-ignored): `data/k11-captures/gp-pads/`

**Interfaces:**
- Produces: the capture evidence for spec §3.2's table (§7 Q2) and for the injection timing (Review Focus 1–2)

- [ ] **Step 1: Capture**

```bash
make gp-capture scenario=gp-pads 2>&1 | tail -12
```

Expected: every `CHECK … ok`; `steps fired 20/20` (the Enter, 18 pads, the chord). If `port script v2` fails with "unpinned", rerun once (a snapshot gap under a press; spec §3.7) and record both runs; if it fails twice at the same step, move that step's `after` by 30 frames in `SCENARIOS['gp-pads']`, add a line to §G.7 saying so, and rerun.

- [ ] **Step 2: The map, from the log**

```bash
python3 - <<'EOF'
import sys; sys.path.insert(0, 'tools')
import gp_session as gs
L = open('data/k11-captures/gp-pads/poll.log').read().splitlines()
snap = gs.snapshots(L)
for l in L:
    r = gs.parse(l)
    if r and r['kind'] == 'I' and 'press' in r and r['press'] in gs.PAD:
        f0 = r['f']
        hit = next((f for f in range(f0 + 1, f0 + 8) if f in snap and snap[f]['raw']), None)
        prev = snap.get(hit - 1) if hit else None
        kb = gs.raw_to_kb(snap[hit]['raw']) if hit else None
        print('%-9s press f=%X  first raw f=%s kb=%s want=%04X  S(f-1) raw=%s'
              % (r['press'], f0, hit and '%X' % hit, kb is not None and '%04X' % kb,
                 gs.PAD[r['press']][2], prev and '%X' % prev['raw']))
EOF
```

Expected per the spec's prediction: each pad's first `raw` frame is `press f + 1` and its `kb` equals `want` (the chord line: `kb=8220` for `p1.up + p1.b1 + p2.left`), with `S(f−1) raw=0`. Record every line verbatim in §G.7. Any `kb` that differs from `want` is a correction of spec §3.2: record it with the line and fix `PAD` in a separate commit with its test updated (raw/capture wins). A first `raw` frame of `press f + 2` is a late injection; record how many.

- [ ] **Step 3: BIOS consumption and the menu**

From the same log, list each `H` record with the `I` press it pairs with (FIFO) and the frame difference, and whether the MAIN MENU cursor or mode changed (`P` records). Record the distribution of `H.f − I.f` (spec §3.6, §7 Q3) and any mode change a pad caused (for example `p1.start` or `p1.b0` selecting "Start").

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
docs: gp-pads capture: the pad map and injection timing measured (record §G.7)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 9: U1 closure

**Files:**
- Modify: record §G.8; `docs/PROGRESS.md` (append one paragraph)

- [ ] **Step 1: Final gate**

```bash
S=/tmp/gameplay-u1
make verify > "$S/t9_verify.txt" 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|k11_compare)' "$S/t9_verify.txt" | diff - "$S/or_base.txt" && echo ORACLES-EQUAL
git diff --stat 934992a -- tools/k11_capture.py tools/k11_session.py tools/k11_fields.py tools/k11_compare.py tools/title_compare.py port/
```

Expected: `verify-exit=0`, `ORACLES-EQUAL`, and an empty diff stat (no K11 tool or port file changed in U1).

- [ ] **Step 2: Record and progress**

§G.8: what U1 delivers (the two tools, the formats, `make gp-capture`), the answers to spec §7 Q2/Q3/Q4 with their §G.7 evidence, and "Not tested": the real keyboard controller and IRQ1 handler (injection writes the key-state table, as K11's inject), typematic repeat, keys in blocking loops (Q5). Append to `docs/PROGRESS.md` one paragraph: "Gameplay U1: capture harness v2 …" with the commit range and the gp-pads result.

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md docs/PROGRESS.md
git commit -m "$(cat <<'EOF'
docs: gameplay U1 closure: capture harness v2 (record §G.8)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review

- Spec §4.1 coverage: frame-exact injection (Tasks 3, 5, 6), P1/P2 pad names and chords/holds (Tasks 2, 3, 8), per-frame poll log of f/raw/pad/RNG/command words (Tasks 2, 6), the port script v2 and trace diff (Task 4), `make gp-capture` (Task 7), the pad-map evidence (Task 8). Raised limits are U2's (spec §4.2).
- Names used across tasks: `SNAP_FIELDS`, `TRACE_FIELDS`, `PAD`, `KEYS`, `HOLD_FRAMES`, `Schedule.{on_mode,due_boot,due,ended,end_frame,fired,total,prev_frame,i}`, `expand`, `port_script`, `snapshots`, `trace_diff`, `read_snap`, `spinning`, `consistent`, `Injector.{press,release_due}`, `guard_gp`, `write_frames`, `Poller` — each defined once, in the task that first needs it.
- The `late` flag in Task 6 compares against `Schedule.prev_frame` (set by `due` to the step's frame `F`), so `late = 0` exactly when `f == F − 1`.
</content>
</invoke>
