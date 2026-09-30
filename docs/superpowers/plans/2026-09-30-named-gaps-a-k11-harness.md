# K11 Ground-Truth Harness (named-gaps sub-project A) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Drive the pinned original `PRAGE.EXE` under DOSBox-X into the service menu (mode `0x27`) with a scripted, repeatable session. Capture its frames and a live-RAM poll log. Add a byte-exact K11 oracle to `make verify` that skips silently without the capture. Record, with capture evidence, what the original does at the idle-timeout `longjmp` (G1), at the `0xFFE80003` read (G2) and on the `0x33458` `idiv` `#DE` (G3). This closes G5 and gives sub-project B its ground truth.

**Architecture:** `tools/k11_capture.py` stages the pinned copy and runs DOSBox-X. `AUTOTYPE` types the scenario's keys and `DX-CAPTURE /V /O` records the video. While the game runs, a poller thread reads the `[dosbox] memory file` (guest RAM, memory-mapped). It writes `poll.log`, which holds the mode, state, ticks, key-buffer inserts, the key bitmap and the config-field image. The same thread applies the scenario's pokes. `tools/k11_session.py` turns `poll.log` into a port script. The env-gated C driver `test_k11_oracle` replays that script through `game_init()` and the master loop, and dumps every distinct frame. `tools/k11_compare.py` classifies the capture against that dump with `title_compare`'s clean/splice/transition model. It also requires every settled port screen to appear in the capture.

**Tech Stack:** Python 3 stdlib (plus `capstone` for the Task 1 raw reads only), `ffmpeg`/`ffprobe` (`/opt/homebrew/bin`), DOSBox-X 2026.08.31 SDL2 (`/opt/homebrew/bin/dosbox-x`), the C port (CMake, `port/tests/run_tests`), `unittest` under `tools/tests`.

**Spec:** `docs/superpowers/specs/2026-09-30-named-gaps-design.md` §4 "A. K11 ground-truth harness (G5, and evidence for G1/G2/G3)", §5, §6.

**Derivation record (created in Task 0, one section per task):** `docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md`.

**Where to run.** Run everything from the main checkout, `/Users/felipe.dos.santos/code/mine/primal-rage-reverse`, on a new branch `named-gaps-a`. Do not use a worktree: a worktree lacks `data/`, as the K11 plan also notes. Scratch files go in `/tmp/named-gaps-a` (`$S` below). Every command below assumes `cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse` and `S=/tmp/named-gaps-a`.

---

## Global Constraints

Verbatim from the spec (§5) and `AGENTS.md`. Each one binds every task.

- "Raw wins; never a fitted constant; a value that cannot be pinned is a named gap with its evidence."
- "`make verify` is the gate after every task; oracle lines equal the baseline (`§A` of the all-gaps ledger); the enforced front-end oracle, the demo-fight ratchet and the attract cycle-2 ratchet stay green."
- "Tests only `CHECK`/`CHECK_EQ_INT`; every assertion must be able to fail; every untested arm goes in the record's "Not tested"."
- "Subagent-driven development with a ledger and a derivation record per sub-project; the Ghidra MCP is unavailable, so raw analysis uses the LE mirror (`le.py`, capstone) and DOSBox-X."
- "Merge and push only when asked."
- "Nothing here changes behaviour the oracles observe today." (spec §1)
- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it."
- AGENTS.md: "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- AGENTS.md: "Captures are git-ignored. `data/` is git-ignored and read-only — never write to it." The one exception here follows the `make title-capture`/`frontend-capture` precedent: `tools/k11_capture.py` writes only `data/k11-captures/<scenario>/`, only when a human or executor runs `make k11-capture`, and its guard refuses every other path (Task 5).
- AGENTS.md: "**The byte-exact oracles SKIP silently unless `PR_ORACLE_REQUIRED=1`**". The K11 oracle follows the front-end oracle's convention. It runs in `make verify` without `PR_ORACLE_REQUIRED`, so it skips when the capture is absent and fails on any mismatch when the capture is present.
- AGENTS.md: "**`game_init()` may run only once per process** … Any test calling it must be env-gated, and `run_tests.c` must run that driver alone." The K11 driver goes only in `TEST_DRIVERS`.
- AGENTS.md: "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests, and never assert an unseeded BSS-zero."
- AGENTS.md: "Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment styles in `port/src`."
- AGENTS.md: "SDL and file/asset I/O live **only** in `port/src/host.c` and `main.c`."
- AGENTS.md: "**`port/src/symbols.h` is generated** … Never hand-edit it; where the generator emits no name, use a local `#define` with the raw address."
- AGENTS.md: "Raw-file disassembly … **those bytes are pre-fixup** … use Ghidra (fixups applied) for any data address, or replicate `mem_load_le` + `mem_load_le_fixups`." In this plan a raw-file displacement is read only through the conversion stated in Task 1 Step 1: a data-object displacement shows as `VA − 0x80000`, and a code-object displacement shows as `VA − 0x10000`.
- AGENTS.md: "Commit style: `<area>: <what changed>`. **Never `git add -A`** — stage named files." Every commit ends with the trailer `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.
- AGENTS.md: "The front-end oracle's claim is narrow … Do not read a green oracle as "the frame is correct"." The K11 oracle adds a coverage claim (Task 7) so that it can detect an under-rendering port.

## Review Focus

These are the five failure modes most likely to bite, each with the task that tests it:

1. **A vacuous oracle.** The window is derived from the port's own dump, so a port that renders less than the original would still pass. Task 7's `test_missing_settled_screen_fails` covers this, and so does Task 10 Step 6: deleting one settled frame from the dump must fail the gate.
2. **Wrong address arithmetic.** The risks are a pre-fixup raw displacement read as a runtime address, the wrong memory-file base, and a field-codec nibble order that is wrong in both directions at once. Task 1 Step 1 fixes the conversion rule and gives expected outputs. Task 3 pins `get`/`set_` against a hand-computed synthetic descriptor that does not round-trip through the code under test. Task 5 tests `find_base`. Task 13 Step 3 compares the Python codec with the port's `config_field_set`.
3. **Oracle lines that move, and a driver that leaks into the unit run.** A second `game_init()` or an active host seam would do this. Task 6 puts the driver only in `TEST_DRIVERS`, keeps the seam inert, and diffs the orlines. Task 8 reruns the full `make verify` against the Task 0 baseline.
4. **A capture that silently did something other than its scenario.** Examples: the Enter is swallowed by a logo movie, the AUTOTYPE taps are too short for the ISR key sampler, `-time-limit` cuts the keys, or the poke is not visible to the guest. Task 5's verdict checks cover this, as do Task 9's per-key `ent`/`kb` evidence and Task 12's DIAGS visibility test (which feeds fallback F1).
5. **Evidence discipline.** Examples: a seed or timing that was fitted, a frame allowed by name without a record reference, a write into `data/` outside `data/k11-captures`, and a G1/G2/G3 answer without poll-log line and capture frame references. Task 5 tests the guard. Task 10 fixes the triage protocol. Tasks 11–13 fix the record format. Task 15 audits all of it.

---

### Task 0: Branch, baseline and the derivation record

**Files:**
- Create: `docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md`
- Create (git-ignored, executor ledger): `.superpowers/sdd/2026-09-30-named-gaps-a-k11-harness/progress.md`

**Interfaces:**
- Consumes: branch `named-gaps-spec` at `a169296` (the spec commit)
- Produces: branch `named-gaps-a`; `$S/a_or_base.txt` (the oracle-line baseline every later task diffs against); record §A.0

- [ ] **Step 1: Branch in place**

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse
git status --porcelain            # expected: no output. If there is any, stop and ask the user.
git switch -c named-gaps-a named-gaps-spec
git log --oneline -1              # expected: a169296 docs: design for closing the six named gaps
mkdir -p /tmp/named-gaps-a
```

- [ ] **Step 2: Baseline gate and oracle lines**

```bash
S=/tmp/named-gaps-a
cmake -S port -B build && cmake --build build 2>&1 | tail -3
make verify > "$S/a_t0_verify.txt" 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare)' "$S/a_t0_verify.txt" > "$S/a_or_base.txt"
wc -l < "$S/a_or_base.txt"; shasum -a 256 "$S/a_or_base.txt"
python3 tools/port_progress.py
```

Expected: `verify-exit=0`. The grep lines equal the oracle lines of K11 record §K11.0 and all-gaps ledger §A: `smk_compare: 120/120 frames match`, `title_compare: frontend: window distinct [560..1884] …`, `attract_compare: … FIRST DIVERGENCE at capture frame 215 …`, and so on. `port_progress.py` prints `767 1203 64` and `731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)`. If any value differs, stop: the tree is not the spec's base.

- [ ] **Step 3: Fixed inputs**

```bash
shasum -a 256 data/game/C/PRAGE.EXE data/game/C/CMOS
python3 -c "d=open('data/game/C/CMOS','rb').read(); print(len(d), sum(1 for b in d if b))"
dosbox-x -version 2>&1 | grep -m1 'DOSBox-X version'
ffmpeg -version | head -1; ffprobe -version | head -1
python3 -c "import capstone, PIL; print(capstone.__version__, PIL.__version__)"
make title-pin && shasum -a 256 /tmp/pr_title_pin/PRAGE.EXE
```

Expected:
- `PRAGE.EXE` sha256 `eecba701576d36d1a217a271acd90e9aa4473121db8d51e8c8085c194ce0e91b`.
- `CMOS` sha256 `2c2bc349e0bcde11b38485c903432d1ddaaf0b4a89fa70444a3a3da1e85d5db0`, with `2040 0` (2040 bytes, none non-zero).
- `DOSBox-X version 2026.08.31 SDL2, copyright 2011-2026 The DOSBox-X Team.`
- `capstone 5.0.7`, `PIL 12.3.0`.
- `title_pin: wrote /tmp/pr_title_pin/PRAGE.EXE (pinned: …)`, and record the pinned copy's sha256.

- [ ] **Step 4: Write the record skeleton**

Create `docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md` with this content, filling §A.0 from Steps 2–3 verbatim:

```markdown
# Named gaps A — the K11 ground-truth harness: derivation record

Plan: `docs/superpowers/plans/2026-09-30-named-gaps-a-k11-harness.md`. Spec:
`docs/superpowers/specs/2026-09-30-named-gaps-design.md` §4 A. This record is
the authoritative source for sub-project B's G1/G2/G3 work. Every claim cites
a raw address, a `poll.log` line (`data/k11-captures/<scenario>/poll.log:<n>`)
or a capture frame (`<scenario> frame <i> (raw <r>)`). On any conflict the raw
wins; corrections are recorded here with their address.

## §A.0 Baseline (Task 0)
<branch, HEAD, verify-exit, the oracle-line file's line count and sha256, the
two port_progress lines, the PRAGE.EXE / CMOS / pinned-copy sha256s, the tool
versions — verbatim>

## §A.1 Raw facts (Task 1)
## §A.2 DOSBox-X probe (Task 2)
## §A.3 Tool and driver contracts (Tasks 3–8)
## §A.4 The walk capture (Task 9)
## §A.5 The walk oracle (Task 10)
## §A.6 G1 evidence: the idle timeout and the MAIN MENU Esc (Task 11)
## §A.7 G2 evidence: the 0xFFE80003 read (Task 12)
## §A.8 G3 evidence: the 0x33458 #DE (Task 13)
## §A.9 Fallbacks taken (Task 14)
## §A.10 Closure (Task 15)
```

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md
git commit -m "$(cat <<'EOF'
docs: named-gaps A derivation record and baseline (record §A.0)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 1: Raw facts for G1, G2, G3 and the menu input path (no game run)

**Files:**
- Modify: `docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md` (§A.1)
- Create (scratch, not committed): `$S/dx.py`

**Interfaces:**
- Consumes: `data/game/C/PRAGE.EXE` (read-only); `port/decomp/prage.c`
- Produces: §A.1.1–§A.1.6, the facts Tasks 4, 11–14 rely on (the scenario keys, the pokes, the fallback patch sites)

- [ ] **Step 1: The scratch disassembler and the conversion rule**

Write `$S/dx.py`:

```python
#!/usr/bin/env python3
"""Raw-file disassembly of PRAGE.EXE for named-gaps A. The operands are
PRE-FIXUP: a data-object displacement shows as VA - 0x80000 (0x87410 is
DS_00107410) and a code-object one as VA - 0x10000 (0x1d300 is 0x2D300).
rel32 call/jmp targets are exact. Usage: dx.py VA_HEX [LEN_HEX]"""
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
d = open('data/game/C/PRAGE.EXE', 'rb').read()
va = int(sys.argv[1], 16)
n = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x40
for i in Cs(CS_ARCH_X86, CS_MODE_32).disasm(d[va + 0x52E54:va + 0x52E54 + n], va):
    print('%08X  %-22s %s %s' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
```

Verify the rule on a known site: `python3 $S/dx.py 2FA10 14`. Expected: `0002FA10 b82a000000 mov eax, 0x2a`, `0002FA15 … call 0x2d974`, `0002FA1A 24fc and al, 0xfc`, `0002FA1C a310740800 mov dword ptr [0x87410], eax` (the store to `DS_00107410`, as `flow.c:6628` says).

- [ ] **Step 2: §A.1.1, G2 reachability (the DIAGS arm)**

```bash
python3 - <<'EOF'
import struct, re
d = open('data/game/C/PRAGE.EXE', 'rb').read()
ds = struct.unpack_from('<63I', d, 0x2D300 + 0x52E54)
w = lambda x: 4 * (((x >> 14) & 7) + 1) + (8 if x & 0x3F else 0)
print('field 2A desc %#x width %d' % (ds[0x2A], w(ds[0x2A])))
code = d[0x10000 + 0x52E54:0x73B14 + 0x52E54]
for m in re.finditer(rb'[\x00-\x13]\x74\x08\x00', code):
    v = int.from_bytes(code[m.start():m.start() + 4], 'little') + 0x80000
    print('ref %#x at %#x' % (v, 0x10000 + m.start()))
dobj = d[0x80000 + 0x46E54:0x10B0D0 + 0x46E54]
print('data-object dwords 0x87410:', [hex(0x80000 + m.start()) for m in re.finditer(rb'\x10\x74\x08\x00', dobj)])
EOF
python3 $S/dx.py 2D974 95
python3 $S/dx.py 32358 20
```

Expected, verified by the planner:
- `field 2A desc 0x1b80 width 4`.
- `ref 0x10740c at 0x2cacd`, `ref 0x10740c at 0x2f983`, `ref 0x10740c at 0x2fa03`, `ref 0x107410 at 0x2fa1d`, `ref 0x107410 at 0x32363`.
- `data-object dwords 0x87410: []`.

`0x2D974` with descriptor `0x1B80` takes the odd arm (`(0x6E + 1) & 1`), so `al = byte & 0xF` and it returns 4 bits.

Record the conclusion with these addresses. Field `0x2A` holds 0..15. `DS_00107410`'s only writer is `0x2FA1C`, which stores `field & ~3` (0, 4, 8 or 0xC). Its only reader is `0x32361`/`0x32363`, which tests `& 0x10`. So **`0x32573`'s `0xFFE80003` read is unreachable in the stock game.** This corrects K11 record §K11.5 ("config field `0x2A` bit 4") and spec §6 ("needs the diagnostic flag set"). Record both corrections with the addresses above. Task 12's capture reaches the arm only through a memory poke, a counterfactual that shows what DOS/4GW + DOSBox-X would do there.

- [ ] **Step 3: §A.1.2, G3 reachability (the `idiv` rows)**

```bash
python3 $S/dx.py 33458 105
python3 $S/dx.py 2DAE4 74
python3 - <<'EOF'
import struct
d = open('data/game/C/PRAGE.EXE', 'rb').read()
ds = struct.unpack_from('<63I', d, 0x2D300 + 0x52E54)
w = lambda x: 4 * (((x >> 14) & 7) + 1) + (8 if x & 0x3F else 0)
for f in (6, 7, 8, 9, 0x12, 0x13):
    print('%02X %#x %d' % (f, ds[f], w(ds[f])))
t = d[0x326C4 + 0x52E54:0x326F4 + 0x52E54]
for i in range(4):
    sid, num, _, d1, d2, _ = struct.unpack_from('<IBBHHH', t, 12 * i)
    print('row %#x num %#x den %#x + %#x' % (sid, num, d1, d2))
EOF
```

Expected: `06 0x243c7 16`, `07 0x24448 16`, `08 0x244c9 16`, `09 0x2454a 16`, `12 0x34ad3 32`, `13 0x34c54 32`. Then `row 0x94 num 0xa den 0x8 + 0x0`, `row 0x95 num 0xc den 0xb + 0x0`, `row 0x96 num 0x12 den 0x8 + 0x6`, `row 0x97 num 0x13 den 0x9 + 0x7`, as K11 record §K11.7 says.

Record three things. First, the `idiv` at `0x334CD..0x334E2` faults only when `d = field(f1) + field(f2)` is non-zero with a zero low word, which needs `f2 != 0`: rows `0x96` (fields 8 + 6) and `0x97` (9 + 7). With 16-bit fields that means a sum of exactly `0x10000` or `0x20000 − …`, for example 8 = 6 = `0x8000`. Second, from the `0x2DAE4` listing, the audit add's arithmetic on these fields: whether it saturates at the field width or wraps, with the instruction address. Third, the reachability verdict in play: the number of games needed, with the persistence through the CMOS file (`0x1B084`). Task 13's stimulus is fields 8 = 6 = `0x8000`, chosen as the minimum pair that triggers row `0x96`. It is a stimulus, not a fitted value.

- [ ] **Step 4: §A.1.3, G1: the setjmp/longjmp map**

```bash
python3 - <<'EOF'
import re
d = open('data/game/C/PRAGE.EXE', 'rb').read()
code = d[0x10000 + 0x52E54:0x73B14 + 0x52E54]
print([hex(0x10000 + m.start() - 1) for m in re.finditer(rb'\xf4\x44\x08\x00', code)])
EOF
python3 $S/dx.py 20C10 30
python3 $S/dx.py 24A9C 1A
python3 $S/dx.py 251F3 22
python3 $S/dx.py 2EB80 3C
python3 $S/dx.py 653FC 35
python3 $S/dx.py 65431 5D
```

Expected: the jmp_buf `0x1044F4` (pre-fixup `0x844f4`) is loaded at `0x20c1a`, `0x24aab`, `0x25206` and `0x2ebae`. `0x20C1A..0x20C1F` is `mov eax,0x844f4; call 0x653fc` (setjmp, the only call). `0x24AAB..0x24AB0`, `0x25206..0x2520B` and `0x2EBAE..0x2EBB3` are `mov eax,0x844f4; jmp 0x65431` (longjmp), each with `mov edx,1`.

Record:
- **One setjmp site (`0x20C1F`)** and **three longjmp sites**:
  - `0x24AB0`: the quit prompt's hard yes, `game_quit_prompt(1)`, `flow.c:6512`.
  - `0x2520B`: case `0x27` for a `menu_step` result other than 0/−5/−10, which includes the MAIN MENU Esc with flags 4 (K11 record §K11.2 mutation 5).
  - `0x2EBB3`: the idle timeout, `tick − DS_00105F2C > 0x4B0` at `0x2EB94..0x2EB9F`, reached only when the latch `DS_00105F30` is 0 (`0x2EB81..0x2EB89`).
- The continuation after setjmp returns, `0x20C24..`, as listed.

This answers the spec §3.4 question "if [the setjmp site] is single" from the raw. Task 11 captures the idle and MAIN MENU Esc sites. The `0x24AB0` site lies outside the service menu and is listed as not captured (reason: it is reached from a game mode, not from mode `0x27`).

- [ ] **Step 5: §A.1.4, exception handlers**

```bash
python3 - <<'EOF'
import re
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
d = open('data/game/C/PRAGE.EXE', 'rb').read()
code = d[0x10000 + 0x52E54:0x73B14 + 0x52E54]
md = Cs(CS_ARCH_X86, CS_MODE_32)
hits = [0x10000 + m.start() for m in re.finditer(rb'\xcd\x31', code)]
print('int 31h sites', len(hits))
for va in hits:
    ins = list(md.disasm(code[va - 0x10000 - 12:va - 0x10000 + 2], va - 12))
    print(hex(va), ' | '.join('%s %s' % (i.mnemonic, i.op_str) for i in ins[-4:]))
EOF
```

Expected: `int 31h sites 31`, the first at `0x625b4`, the rest from `0x664f2` in the runtime (>= `0x5D000`). No `mov ax,0x203`/`0x212` or `int 21h AX=2500h` immediate exists anywhere in the code object (planner's scan). Record every site whose AX is set in the four instructions before it, and whether any sets a DPMI exception handler (AX `0x0203`, `0x0212`, `0x0213`) or an interrupt vector 0 (AX `0x0205` with BL = 0). The verdict ("no game or runtime handler for #DE/#PF is installed" or the handler's address) predicts what Tasks 12–13 capture. The capture decides.

- [ ] **Step 6: §A.1.5, the menu input path (this decides the scenario keys)**

```bash
python3 -c "
import struct
d=open('data/game/C/PRAGE.EXE','rb').read(); o=0xA2C62+0x46E54
w=struct.unpack('<20H',d[o:o+0x28])
print('dev1',hex(w[0]),[hex(x) for x in w[1:9]]); print('dev2',hex(w[9]),[hex(x) for x in w[10:18]])"
```

Expected (verified): `dev1 0x0 ['0x1f73', '0x2d78', '0x2c7a', '0x2e63', '0x1675', '0x1769', '0x316e', '0x326d']` and `dev2 0x0 ['0x4800', '0x5000', '0x4b00', '0x4d00', '0x4700', '0x4900', '0x4f00', '0x5100']`.

Record the consequence from `config_key_flags`/`cfg_dir_bits` (`config.c`, `0x2EBF0`, `0x2EC6A`/`0x2EC85`). The arrows are player 2's keys and player 2's device word is 0 (keyboard). A latched arrow scan therefore gives **no** direction bit (`k2 == code && [+0x2D6] == 0` returns 0). In the original, menu Up/Down come from the pad level: the key bitmap `[DS_00101514]+0x2D8/+0x2D9`, which the host-owned ISR sampler `0x1BBAC` fills and `0x500C4` turns into `DS_000E1C34`. Enter and Esc come from the latched key (`0x2EDA3..0x2EDC9`). This is why the poller logs `kb` and the port driver holds the bitmap (Tasks 4–6).

- [ ] **Step 7: §A.1.6, entry and exits of mode `0x27`**

Record from `flow.c:6948..7010` / raw `0x24ECF..0x24EE0`: Enter (ascii `0x0D`) with `DS_00104B00 == 3` stores `0x27`. Esc in mode 3 opens the quit prompt, and Esc in mode `0x27` does nothing in the key loop. Also record, from K11 record §K11.2, the exits:
- the START MENU Esc gives −5 and re-initialises the MAIN MENU;
- the OPTIONS MENU Esc gives −1 inside the Enter arm, so `0x2FFC4` returns 0;
- the MAIN MENU Esc with flags 4 gives −1, and case `0x27` longjmps at `0x2520B`.

These fix the `walk` key list in Task 4. It never presses Esc on the MAIN MENU.

- [ ] **Step 8: Commit**

```bash
git add docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md
git commit -m "$(cat <<'EOF'
docs: named-gaps A raw facts for G1, G2, G3 and the menu input path (record §A.1)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: DOSBox-X capability probe (no game)

**Files:**
- Modify: `docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md` (§A.2)

**Interfaces:**
- Consumes: `/opt/homebrew/bin/dosbox-x`
- Produces: §A.2. It holds the verified button names (Task 4's `KEYS`), the exact `-set` spelling of the CON log (Task 5's `dosbox_cmd`) and the memory-file size.

- [ ] **Step 1: What the planner verified from `--help`/`-helpdebug`/the binary's strings (record verbatim)**

The planner ran these commands and nothing else. Record the facts in §A.2:
- `dosbox-x --version` prints `DOSBox-X version 2026.08.31 SDL2`.
- `dosbox-x --help` lists `-defaultconf -fastlaunch -nopromptfolder -nogui -nomenu -time-limit <n> -c <command> -set <section property=value> -exit -silent -startmapper -defaultmapper`.
- `dosbox-x -helpdebug` lists `-debug -break-start -log-con -log-int21 -log-fileio -nolog`.
- The strings hold `AUTOTYPE [-list] [-w WAIT] [-p PACE] button_1 [button_2 [...]]`: "-w WAIT: seconds before typing begins. Two second default; max of 30", "-p PACE: seconds between each keystroke. Half-second default; max of 10", "The , character inserts an extra PACE delay".
- The strings hold `DX-CAPTURE [/V|/-V] [/A|/-A] [/M|/-M] [/O|/-O] [/D|/-D] [command] [options]`: "/V for video, … /O for OPL FM (DROv2 format)".
- The strings hold `memory file`: "If set, guest memory is memory-mapped from a file on disk, rather than allocated from memory. … The file will be created if it does not exist".
- The strings hold `log console`, in the `[dos]` section next to `ansi.sys`: "If set, log DOS CON output to the log file. Setting to "quiet" will log DOS CON output only".
- The debugger is compiled in (`MEMDUMPBIN`, `BPINT`, `DEBUGBOX`). The earlier records (arena-backdrop §1.7, demo-pose §1.6) show its pty automation is unreliable, so this plan does not use it.

- [ ] **Step 2: Run the probe**

```bash
S=/tmp/named-gaps-a; P=$S/probe; rm -rf "$P"; mkdir -p "$P/c"
dosbox-x -defaultconf -fastlaunch -nopromptfolder -nogui -nomenu -time-limit 20 \
  -set "sdl fullscreen=false" \
  -set "dosbox memory file=$P/guest.mem" \
  -set "log logfile=$P/dosbox.log" \
  -set "dos log console=quiet" \
  -c "MOUNT C \"$P/c\"" -c "C:" \
  -c "AUTOTYPE -list > C:\\ATLIST.TXT" \
  -c "ECHO K11-LOGCON-PROBE" \
  -c "EXIT"
echo "exit=$?"
ls -l "$P/guest.mem"
grep -n 'K11-LOGCON-PROBE' "$P/dosbox.log"
for k in enter esc up down left right; do printf '%s: ' $k; grep -c -w "$k" "$P/c/ATLIST.TXT"; done
grep -n -i 'unknown\|invalid\|not found' "$P/dosbox.log" | head
```

Expected: `exit=0`. `guest.mem` exists, and its size (bytes) is recorded as the memory size. The log holds a line containing `K11-LOGCON-PROBE` (CON output logged). Each of the six names counts at least 1 in `ATLIST.TXT`. No `unknown`/`invalid` line names the `-set` options.

- [ ] **Step 3: If a check in Step 2 fails**

Take only the branch that matches the failure:
- **No `K11-LOGCON-PROBE` in the log.** Rerun Step 2 with `-log-con` in place of `-set "dos log console=quiet"`. Record the working spelling. Task 5's `LOG_CON_ARGS` uses it.
- **`ATLIST.TXT` empty or missing a name.** Rerun with `-defaultmapper` added. If a name is still missing, record the listed name that means the same key (for example `kp_enter` is *not* acceptable for Enter: the game needs ascii `0x0D` with scan `0x1C`). Task 4's `KEYS` keys must be names the list prints.
- **No `guest.mem`.** Stop. The poller design depends on it, so fall back per Task 14 F1 (pinned EXE variants) and F5 (no poll log: frames only, and the scenario's facts are then derived from frames).

- [ ] **Step 4: Record and commit**

Record Step 2's output verbatim (the size, the matching log line, the counts) and the spellings chosen in §A.2.

```bash
git add docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md
git commit -m "$(cat <<'EOF'
docs: named-gaps A DOSBox-X probe: AUTOTYPE, memory file, CON log (record §A.2)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: `tools/k11_fields.py`, the config-field image codec

**Files:**
- Create: `tools/k11_fields.py`
- Create: `tools/tests/test_k11_fields.py`

**Interfaces:**
- Consumes: the descriptor table at code VA `0x2D300` (raw file offset `0x80154`; bitfields, not fixup targets, as the Task 1 Step 3 widths prove)
- Produces: `WIN_LO = 0x105DAF`, `WIN_HI = 0x105E30`, `load_descriptors(exe) -> list[int]` (63 dwords), `width(desc) -> int`, `get(img, descs, field) -> int`, `set_(img, descs, field, value) -> int` (0 / −1). `img` is a `bytearray` covering DS `[WIN_LO, WIN_HI)`.

- [ ] **Step 1: Write the failing tests**

```python
# tools/tests/test_k11_fields.py
import os, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import k11_fields as kf

EXE = os.path.join(ROOT, 'data', 'game', 'C', 'PRAGE.EXE')


def blank():
    return bytearray(kf.WIN_HI - kf.WIN_LO)


class SyntheticDescriptor(unittest.TestCase):
    """Hand-computed from the 0x2D974/0x2DA0C listings, independent of the exe."""
    # 2 nibbles at nibble offset 3, no byte part: (1 << 14) | (3 << 6)
    D = [0] * 63
    D[1] = (1 << 14) | (3 << 6)
    # 1 nibble at nibble offset 4 plus byte-table entry 5: (0 << 14) | (4 << 6) | 5
    D[2] = (4 << 6) | 5

    def test_get_reads_the_listing_order(self):
        img = blank()
        img[kf.NIB_DS + 1 - kf.WIN_LO] = 0xC0     # high nibble C read last (0x2D9CD)
        img[kf.NIB_DS + 2 - kf.WIN_LO] = 0x0D     # low nibble D read first (0x2D9AB)
        self.assertEqual(kf.get(img, self.D, 1), 0xDC)

    def test_set_then_get_and_neighbour_nibbles_kept(self):
        img = blank()
        img[kf.NIB_DS + 1 - kf.WIN_LO] = 0x07     # low nibble belongs to a neighbour
        img[kf.NIB_DS + 2 - kf.WIN_LO] = 0x90     # high nibble belongs to a neighbour
        self.assertEqual(kf.set_(img, self.D, 1, 0xAB), 0)
        self.assertEqual(img[kf.NIB_DS + 1 - kf.WIN_LO], 0xB7)   # 0x2DA90
        self.assertEqual(img[kf.NIB_DS + 2 - kf.WIN_LO], 0x9A)   # 0x2DAAE
        self.assertEqual(kf.get(img, self.D, 1), 0xAB)

    def test_byte_part_is_the_low_byte(self):
        img = blank()
        kf.set_(img, self.D, 2, 0x3C5)
        self.assertEqual(img[kf.BYTES_DS + 5 - kf.WIN_LO], 0xC5)  # 0x2DA34
        self.assertEqual(kf.get(img, self.D, 2), 0x3C5)

    def test_out_of_range_field(self):
        self.assertEqual(kf.get(blank(), self.D, 0x3F), 0xFFFFFFFF)
        self.assertEqual(kf.set_(blank(), self.D, 0x3F, 1), -1)


@unittest.skipUnless(os.path.isfile(EXE) or os.environ.get('PR_ORACLE_REQUIRED') == '1',
                     'data/game/C/PRAGE.EXE absent')
class RealDescriptors(unittest.TestCase):
    def setUp(self):
        self.d = kf.load_descriptors(EXE)

    def test_descriptors_match_the_k11_record(self):
        self.assertEqual(self.d[0x35], 0xE0C0)                     # K11 record §K11.4
        widths = {3: 20, 4: 20, 5: 20, 6: 16, 7: 16, 8: 16, 9: 16, 0xA: 24,
                  0xB: 16, 0xC: 24, 0xD: 16, 0x11: 16, 0x12: 32, 0x13: 32,
                  0x29: 32, 0x2A: 4}                               # §K11.7, §A.1
        for f, bits in widths.items():
            self.assertEqual(kf.width(self.d[f]), bits, hex(f))

    def test_round_trip_and_isolation_for_every_field(self):
        for f in range(0x3F):
            bits = kf.width(self.d[f])
            mask = (1 << bits) - 1 if bits < 32 else 0xFFFFFFFF
            for v in (0, 1, mask, 0xA5A5A5A5 & mask):
                img = bytearray(b'\x5a' * (kf.WIN_HI - kf.WIN_LO))
                before = {g: kf.get(img, self.d, g) for g in range(0x3F)}
                kf.set_(img, self.d, f, v)
                self.assertEqual(kf.get(img, self.d, f), v, (hex(f), hex(v)))
                for g in range(0x3F):
                    if g != f and not self._overlap(f, g):
                        self.assertEqual(kf.get(img, self.d, g), before[g], (hex(f), hex(g)))
                self.assertEqual(img[kf.DD8_DS - kf.WIN_LO], 0x5A)  # dirty bits untouched

    def test_field_2a_keeps_four_bits(self):
        img = bytearray(kf.WIN_HI - kf.WIN_LO)
        kf.set_(img, self.d, 0x2A, 0x13)
        self.assertEqual(kf.get(img, self.d, 0x2A), 0x3)          # bit 4 cannot be stored

    def _overlap(self, f, g):
        def cells(x):
            s = set()
            start, n = (x >> 6) & 0xFF, ((x >> 14) & 7) + 1
            s.update(('n', i) for i in range(start, start + n))
            if x & 0x3F:
                s.add(('b', x & 0x3F))
            return s
        return bool(cells(self.d[f]) & cells(self.d[g]))


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run it and see it fail**

Run: `python3 -m unittest tools.tests.test_k11_fields -v`
Expected: an `ImportError`/`ModuleNotFoundError: No module named 'k11_fields'` error, and the run fails.

- [ ] **Step 3: Implement `tools/k11_fields.py`**

```python
#!/usr/bin/env python3
"""Config-field image codec for the K11 harness: 0x2D974 (get) and the image
half of 0x2DA0C (set), over a bytearray window of the data object.

A descriptor is the dword at code VA 0x2D300 + 4 * field (0x2D987 `mov
esi,[eax*4+0x2D300]`). It is a bitfield and not a fixup target, so the raw file
bytes at VA + 0x52E54 are the runtime bytes. Bits 0..5 give a byte index into
DS_00105DAF (0 = none), which holds the value's low byte. Bits 6..13 give a
nibble offset into DS_00105DE1. Bits 14..16 give the nibble count - 1.

set_() writes only the two image arrays. It does not write the dirty bits
DS_00105DD8 (0x2DA3B/0x2DA4E) or call the EEPROM mirror 0x2D4EC: a live poke of
the original must change the fields and nothing else. Stdlib only."""
import struct

WIN_LO = 0x105DAF          # DS_00105DAF: the byte table's base (index 0 unused)
WIN_HI = 0x105E30          # past the last nibble byte any descriptor reaches (§A.3)
BYTES_DS = 0x105DAF
NIB_DS = 0x105DE1
DD8_DS = 0x105DD8
DESC_VA = 0x2D300
N_FIELDS = 0x3F
CODE_FILE_DELTA = 0x52E54  # obj-0 raw file offset = VA + 0x52E54 (AGENTS.md)


def load_descriptors(exe_path):
    with open(exe_path, 'rb') as f:
        data = f.read()
    return list(struct.unpack_from('<%dI' % N_FIELDS, data, DESC_VA + CODE_FILE_DELTA))


def width(desc):
    """The field's bits: 4 per nibble, plus 8 for a byte-table entry."""
    return 4 * (((desc >> 14) & 7) + 1) + (8 if desc & 0x3F else 0)


def _at(ds):
    return ds - WIN_LO


def get(img, descs, field):
    """0x2D974. 0xFFFFFFFF above field 0x3E (0x2D978 `cmp eax,0x3e; jbe`)."""
    if field > 0x3E:
        return 0xFFFFFFFF
    d = descs[field]
    edx = ((d >> 14) & 7) + 1                        # 0x2D992..0x2D9A0
    eax = ((d >> 6) & 0xFF) + edx                    # 0x2D995..0x2D9A1
    ebx = eax >> 1                                   # 0x2D9A5 sar (eax >= 0)
    if eax & 1:                                      # 0x2D9A7
        v = img[_at(NIB_DS + ebx)] & 0xF             # 0x2D9AB..0x2D9B5
        edx -= 1                                     # 0x2D9BA
    else:
        v = 0                                        # 0x2D9BF
    while edx:                                       # 0x2D9C1
        ebx -= 1                                     # 0x2D9C5
        if edx == 1:                                 # 0x2D9C6
            v = (v << 4) | ((img[_at(NIB_DS + ebx)] >> 4) & 0xF)        # 0x2D9CD..0x2D9DC
            break
        v = ((v << 8) | img[_at(NIB_DS + ebx)]) & 0xFFFFFFFF           # 0x2D9E2..0x2D9EE
        edx -= 2                                     # 0x2D9EB
    if d & 0x3F:                                     # 0x2D9F2
        v = ((v << 8) | img[_at(BYTES_DS + (d & 0x3F))]) & 0xFFFFFFFF  # 0x2D9F9..0x2DA02
    return v & 0xFFFFFFFF


def set_(img, descs, field, value):
    """0x2DA0C's image writes. -1 above field 0x3E, else 0."""
    if field > 0x3E:
        return -1
    d = descs[field]
    value &= 0xFFFFFFFF
    if d & 0x3F:                                     # 0x2DA29
        img[_at(BYTES_DS + (d & 0x3F))] = value & 0xFF                 # 0x2DA34
        value >>= 8                                  # 0x2DA3E
    bitpos = (d >> 6) & 0xFF                         # 0x2DA59..0x2DA5F
    n = ((d >> 14) & 7) + 1                          # 0x2DA5C..0x2DA6A
    ebx = bitpos >> 1                                # 0x2DA6B
    if bitpos & 1:                                   # 0x2DA6D
        low = img[_at(NIB_DS + ebx)] & 0xF           # 0x2DA72..0x2DA78
        ebx += 1                                     # 0x2DA8A
        n -= 1                                       # 0x2DA86
        img[_at(NIB_DS - 1 + ebx)] = low | ((value & 0xF) << 4)        # 0x2DA90 [ebx+0x105DE0]
        value >>= 4                                  # 0x2DA8D
    while n:                                         # 0x2DA96
        if n == 1:                                   # 0x2DA9A
            img[_at(NIB_DS + ebx)] = (img[_at(NIB_DS + ebx)] & 0xF0) | (value & 0xF)  # 0x2DA9F..0x2DAAE
            break
        n -= 2                                       # 0x2DAB6
        img[_at(NIB_DS + ebx)] = value & 0xFF        # 0x2DAB9
        ebx += 1                                     # 0x2DABF
        value >>= 8                                  # 0x2DAC0
    return 0
```

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_k11_fields -v`
Expected: `Ran 7 tests`, `OK`.

- [ ] **Step 5: Mutation proof (then revert)**

In `get`, swap the two nibble reads (read `& 0xF` on the `edx == 1` arm and `>> 4` on the odd arm), then run the tests. Expected: `test_get_reads_the_listing_order` and the round-trip test FAIL. Revert with `git checkout tools/k11_fields.py` (unstaged) and rerun: `OK`. Record both runs in §A.3.

- [ ] **Step 6: Commit**

```bash
git add tools/k11_fields.py tools/tests/test_k11_fields.py docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md
git commit -m "$(cat <<'EOF'
tools: k11_fields, the config-field image codec 0x2D974/0x2DA0C (record §A.3)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: `tools/k11_session.py`: scenarios, the AUTOTYPE line, the poll log and the port script

**Files:**
- Create: `tools/k11_session.py`
- Create: `tools/tests/test_k11_session.py`

**Interfaces:**
- Consumes: `tools/k11_fields.py`; §A.1.5–§A.1.6 (the keys); §A.2 (the button names)
- Produces:
  - `SCENARIOS` (`walk`, `idle`, `menuesc`, `diags`, `de`), `KEYS`, `POLL_FIELDS`, `KB_PTR_DS`, `DATA_BASE_VA`, `ENTER_WAIT`, `PACE`, `END_TAIL_TICKS`.
  - `autotype_line(name, enter_wait, pace) -> str`, `parse(line) -> dict|None`, `format_p(ms, vals, kb, bios) -> str`, `port_script(name, lines) -> str` (raises `ScriptError`), `check_fields(name, port_dir, exe) -> int`.
  - CLI `autotype`, `port-script`, `check-fields`.
  - The poll-log and port-script formats in the module docstring below.

- [ ] **Step 1: Write the failing tests**

```python
# tools/tests/test_k11_session.py
import os, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import k11_session as ks


def p(ms, tick, f=0x100, st=0, mode=3, menu=0, ent=0, kb=0):
    vals = {n: 0 for n, _, _ in ks.POLL_FIELDS}
    vals.update(f=f, st=st, mode=mode, menu=menu, ent=ent, tick=tick)
    return ks.format_p(ms, vals, kb, 0x1E)


def k(ms, tick, word):
    return 'K ms=%d tick=%08X f=0100 key=%04X' % (ms, tick, word)


class Session(unittest.TestCase):
    def test_autotype_walk_is_exact(self):
        self.assertEqual(
            ks.autotype_line('walk', 25, 1),
            'AUTOTYPE -w 25 -p 1 enter enter down down esc down enter enter esc '
            'down enter esc esc esc esc esc '
            'down enter esc down enter esc down enter esc down enter esc '
            'down enter esc down enter esc down enter esc esc')

    def test_scenario_keys_are_known(self):
        for name, sc in ks.SCENARIOS.items():
            for key in sc['keys']:
                self.assertTrue(key == ',' or key in ks.KEYS, (name, key))
            self.assertEqual(sc['keys'][0], 'enter', name)   # the mode-3 Enter

    def test_parse_round_trips_a_p_record(self):
        r = ks.parse(p(12, 0x1234, f=0x2A, mode=0x27, kb=0x40))
        self.assertEqual((r['kind'], r['ms'], r['tick'], r['f'], r['mode'], r['kb']),
                         ('P', 12, 0x1234, 0x2A, 0x27, 0x40))

    def test_port_script_from_a_synthetic_poll(self):
        lines = ['B ms=0 base=00266000',
                 p(5, 0x100),
                 k(10, 0x200, 0x1C0D),                        # the mode-3 Enter
                 p(11, 0x200, f=0x155, st=0, mode=0x27),
                 p(40, 0x220, f=0x160, mode=0x27, kb=0x0040), # a held arrow
                 k(41, 0x221, 0x50E0),                        # grey Down, ascii 0xE0 in the buffer
                 p(60, 0x226, f=0x162, mode=0x27, kb=0),
                 k(90, 0x260, 0x011B)]                        # Esc
        ks.SCENARIOS['_t'] = dict(keys=('enter', ',', 'down', 'esc'),
                                  pokes=(('ds_or', 0x107410, 0x10),), time_limit=10)
        try:
            out = ks.port_script('_t', lines)
        finally:
            del ks.SCENARIOS['_t']
        self.assertEqual(out, '# k11 port script v1: scenario _t\n'
                              'enter_frame 341\n'
                              'enter_state 0000\n'
                              'ds_or 107410 10\n'
                              'pad 32 0040 6\n'
                              'key 33 50 00\n'
                              'key 96 01 1B\n'
                              'end %d\n' % (96 + ks.END_TAIL_TICKS))

    def test_port_script_rejects_other_keys(self):
        lines = [k(10, 0x200, 0x1C0D), p(11, 0x200, mode=0x27), k(20, 0x210, 0x011B)]
        with self.assertRaises(ks.ScriptError):
            ks.port_script('menuesc', lines + [k(30, 0x220, 0x011B)])

    def test_port_script_needs_mode_27(self):
        with self.assertRaises(ks.ScriptError):
            ks.port_script('idle', [k(10, 0x200, 0x1C0D), p(11, 0x200, mode=3)])


if __name__ == '__main__':
    unittest.main()
```

(`enter_frame 341` is the synthetic `f = 0x155`, the value after the Enter's iteration. `pad 32 0040 6` is the run from tick `0x220 − 0x200` held until `0x226`.)

- [ ] **Step 2: Run it and see it fail**

Run: `python3 -m unittest tools.tests.test_k11_session -v`
Expected: `ModuleNotFoundError: No module named 'k11_session'`.

- [ ] **Step 3: Implement `tools/k11_session.py`**

```python
#!/usr/bin/env python3
"""K11 harness scenarios, the DOSBox-X AUTOTYPE line, the live-RAM poll log and
the port-script generator (record 2026-09-30-named-gaps-a-derivations.md §A.3).

A scenario is an AUTOTYPE key list (',' is one extra PACE of delay; the first
key is the mode-3 Enter, 0x24ECF..0x24EE0) plus pokes applied once, when the
poll first sees mode 0x27.

poll.log (written by k11_capture.py), one record per line:
  B ms=<int> base=<hex8>                          the data object's runtime base
  P ms=<int> f=.. st=.. mode=.. menu=.. ent=.. diag=.. tick=.. ktime=.. kword=..
            latch=.. raw=.. pad=.. kb=<hex4> bios=<hex4>   any value changed
  K ms=<int> tick=<hex8> f=<hex4> key=<hex4>      one BIOS keyboard-buffer insert
  F ms=<int> tick=<hex8> img=<hex>                the field window [0x105DAF,0x105E30) changed
  W ms=<int> ds=<hex8> linear=<hex8> old=<hex2> new=<hex2>   one poked byte
  E ms=<int> reason=<exit|time-limit> rc=<int>    the run ended

The port script (read by test_k11_oracle, port/tests/test_game.c):
  enter_frame <dec>        DS_000EF6DC after the iteration that took mode 3 to 0x27
  enter_state <hex>        DS_000F0A64 then
  ds_or <ds hex> <byte hex>          at the Enter
  field <id hex> <value hex>         at the Enter (config_field_set)
  pad <dtick> <kb hex4> <nticks>     hold [DS_00101514]+0x2D8/+0x2D9 = kb >> 8, kb & 0xFF
  key <dtick> <scan hex> <ascii hex> queue in the int 16h buffer
  end <dtick>
dtick counts DS_00101500 ticks from the Enter's K record. Usage:
  k11_session.py autotype --scenario NAME [--enter-wait S] [--pace S]
  k11_session.py port-script --scenario NAME --capture DIR --out FILE
  k11_session.py check-fields --scenario NAME --port DIR [--exe PATH]"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import k11_fields as kf

DATA_BASE_VA = 0x80000
KB_PTR_DS = 0x101514       # DS_00101514 -> the key-config/BIOS record (+0x2D8/+0x2D9)
ENTER_WAIT = 25.0          # s: inside the boot attract (logos end ~19 s, title ~31 s; §A.4 checks it)
PACE = 1.0                 # s between keys
END_TAIL_TICKS = 180       # harness window after the last key (not a game value)

POLL_FIELDS = (
    ('f', 0x0EF6DC, 2),      # DS_000EF6DC the master frame counter (0x24CDB)
    ('st', 0x0F0A64, 2),     # DS_000F0A64 the 0x11D04 state
    ('mode', 0x104B00, 2),   # DS_00104B00 the mode word (0x24EE0 stores 0x27)
    ('menu', 0x107414, 1),   # DS_00107414 menu_step's active byte (0x2EBA8 clears it)
    ('ent', 0x10741C, 4),    # DS_0010741C the menu's current entry
    ('diag', 0x107410, 4),   # DS_00107410 TEST CONTROLS' flags (0x2FA1C)
    ('tick', 0x101500, 4),   # DS_00101500 the ISR clock 0x500BB reads
    ('ktime', 0x105F2C, 4),  # DS_00105F2C the key/menu stamp 0x2EB94 subtracts
    ('kword', 0x105F28, 4),  # DS_00105F28 the last BIOS key word (0x2EB3F)
    ('latch', 0x105F30, 4),  # DS_00105F30 the latched key (0x24D3E, 0x2EB66)
    ('raw', 0x0E1C30, 4),    # DS_000E1C30 the pad's previous raw word
    ('pad', 0x0E1C34, 4),    # DS_000E1C34 the pad level
)

KEYS = {'enter': (0x1C, 0x0D), 'esc': (0x01, 0x1B), 'up': (0x48, 0x00),
        'down': (0x50, 0x00), 'left': (0x4B, 0x00), 'right': (0x4D, 0x00)}
ARROW_SCANS = (0x48, 0x50, 0x4B, 0x4D)

WALK = tuple((
    'enter enter down down esc down enter enter esc '        # MAIN, START (2 rows), back, GAME OPTIONS, CONFIG OPTIONS
    'down enter esc esc esc esc esc '                        # STATISTICS: page 1, page 2, histograms 0..2
    'down enter esc down enter esc down enter esc down enter esc '   # SOUND, MUSIC, MODIFY CONTROLS, CONFIGURE KEYBOARD
    'down enter esc down enter esc down enter esc esc'       # TEST CONTROLS, ADJUST VOLUME, 2 PLAYER HANDICAP, leave
).split())

SCENARIOS = {
    'walk': dict(keys=WALK, pokes=(), time_limit=75),
    'idle': dict(keys=('enter',), pokes=(), time_limit=90),
    'menuesc': dict(keys=('enter', ',', ',', 'esc'), pokes=(), time_limit=60),
    'diags': dict(keys=tuple('enter down enter down down down down down down enter , , , esc esc'.split()),
                  pokes=(('ds_or', 0x107410, 0x10),), time_limit=55),
    'de': dict(keys=tuple('enter down enter down enter , , , esc esc esc esc esc esc'.split()),
               pokes=(('field', 8, 0x8000), ('field', 6, 0x8000)), time_limit=55),
}


class ScriptError(Exception):
    pass


def key_word(name):
    scan, asc = KEYS[name]
    return scan << 8 | asc


def normalise(word):
    """int 16h AH=0 hands a grey arrow's 0xE0 ascii back as 0 (0x24D26); the
    BIOS buffer the poller reads keeps the 0xE0."""
    if (word >> 8) in ARROW_SCANS and (word & 0xFF) == 0xE0:
        return word & 0xFF00
    return word


def autotype_line(name, enter_wait=ENTER_WAIT, pace=PACE):
    return 'AUTOTYPE -w %g -p %g %s' % (enter_wait, pace, ' '.join(SCENARIOS[name]['keys']))


def format_p(ms, vals, kb, bios):
    body = ' '.join('%s=%0*X' % (n, 2 * sz, vals[n]) for n, _, sz in POLL_FIELDS)
    return 'P ms=%d %s kb=%04X bios=%04X' % (ms, body, kb, bios)


def parse(line):
    parts = line.split()
    if not parts:
        return None
    rec = {'kind': parts[0]}
    for part in parts[1:]:
        key, _, val = part.partition('=')
        if key in ('ms', 'rc'):
            rec[key] = int(val)
        elif key == 'reason':
            rec[key] = val
        elif key == 'img':
            rec[key] = bytes.fromhex(val)
        else:
            rec[key] = int(val, 16)
    return rec


def port_script(name, lines):
    sc = SCENARIOS[name]
    recs = [r for r in (parse(l) for l in lines) if r]
    keys = [k for k in sc['keys'] if k != ',']
    kev = [r for r in recs if r['kind'] == 'K']
    if not kev:
        raise ScriptError('no K record: the BIOS keyboard buffer never advanced (plan Task 14 F2)')
    got = [normalise(r['key']) for r in kev]
    want = [key_word(k) for k in keys]
    if got != want:
        raise ScriptError('observed keys %s differ from scenario %s %s'
                          % (['%04X' % w for w in got], name, ['%04X' % w for w in want]))
    t0, ms0 = kev[0]['tick'], kev[0]['ms']
    p27 = next((r for r in recs if r['kind'] == 'P' and r['ms'] >= ms0 and r['mode'] == 0x27), None)
    if p27 is None:
        raise ScriptError('mode 0x27 never observed after the Enter (raise --enter-wait?)')
    dt = lambda tick: (tick - t0) & 0xFFFFFFFF
    timed = []
    for kname, r in zip(keys[1:], kev[1:]):
        scan, asc = KEYS[kname]
        timed.append((dt(r['tick']), 1, 'key %d %02X %02X' % (dt(r['tick']), scan, asc)))
    prev, start = 0, 0
    for r in recs:
        if r['kind'] != 'P' or r['ms'] < ms0:
            continue
        if r['kb'] != prev:
            if prev:
                timed.append((start, 0, 'pad %d %04X %d' % (start, prev, max(1, dt(r['tick']) - start))))
            prev, start = r['kb'], dt(r['tick'])
    if prev:
        raise ScriptError('the key bitmap is still held at the end of the poll log')
    last = max([d for d, _, _ in timed] + [0])
    out = ['# k11 port script v1: scenario %s' % name,
           'enter_frame %d' % p27['f'],
           'enter_state %04X' % p27['st']]
    out += ['%s %X %X' % (kind, a, b) for kind, a, b in sc['pokes']]
    out += [text for _, _, text in sorted(timed)]
    out.append('end %d' % (last + END_TAIL_TICKS))
    return '\n'.join(out) + '\n'


def check_fields(name, port_dir, exe):
    descs = kf.load_descriptors(exe)
    n = kf.WIN_HI - kf.WIN_LO
    before = bytearray(open(os.path.join(port_dir, 'fimg_before.bin'), 'rb').read())
    after = open(os.path.join(port_dir, 'fimg_after.bin'), 'rb').read()
    if len(before) != n or len(after) != n:
        print('k11_session: check-fields: the port window is not %d bytes' % n)
        return 1
    for kind, a, b in SCENARIOS[name]['pokes']:
        if kind == 'field':
            kf.set_(before, descs, a, b)
    for i in range(n):
        if before[i] != after[i]:
            print('k11_session: check-fields: %s: DS %08X python %02X port %02X'
                  % (name, kf.WIN_LO + i, before[i], after[i]))
            return 1
    print('k11_session: check-fields: %s: tools/k11_fields.set_ matches config_field_set over %d bytes'
          % (name, n))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('autotype', 'port-script', 'check-fields'))
    ap.add_argument('--scenario', required=True, choices=sorted(SCENARIOS))
    ap.add_argument('--enter-wait', type=float, default=ENTER_WAIT)
    ap.add_argument('--pace', type=float, default=PACE)
    ap.add_argument('--capture')
    ap.add_argument('--out')
    ap.add_argument('--port')
    ap.add_argument('--exe', default='data/game/C/PRAGE.EXE')
    a = ap.parse_args()
    if a.cmd == 'autotype':
        print(autotype_line(a.scenario, a.enter_wait, a.pace))
        return 0
    if a.cmd == 'check-fields':
        return check_fields(a.scenario, a.port, a.exe)
    with open(os.path.join(a.capture, 'poll.log')) as f:
        try:
            text = port_script(a.scenario, f.read().splitlines())
        except ScriptError as e:
            print('k11_session: port-script: %s: %s' % (a.scenario, e))
            return 1
    with open(a.out, 'w') as f:
        f.write(text)
    print('k11_session: port-script: %s: wrote %s (%d lines)' % (a.scenario, a.out, text.count('\n')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
```

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_k11_session -v`
Expected: `Ran 6 tests`, `OK`.

- [ ] **Step 5: Mutation proof (then revert)**

In `normalise`, change the return to `return word`. Expected: `test_port_script_from_a_synthetic_poll` FAILS with `ScriptError`, because `50E0` is not `5000`. Revert and rerun: `OK`.

- [ ] **Step 6: Commit**

```bash
git add tools/k11_session.py tools/tests/test_k11_session.py
git commit -m "$(cat <<'EOF'
tools: k11_session, the K11 scenarios, poll-log format and port script

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: `tools/k11_capture.py`, the DOSBox-X capture with the live-RAM poller

**Files:**
- Create: `tools/k11_capture.py`
- Create: `tools/tests/test_k11_capture.py`

**Interfaces:**
- Consumes:
  - `tools/smk_capture.py` (`which`, `read_avi_frames`, `ffprobe_fps`).
  - `tools/title_capture.py` (`stage`, `PIN_EXE`, `post_logo_start`, `collapse_from`, `frames_at`, `write_window`).
  - `tools/k11_session.py`, `tools/k11_fields.py`.
  - §A.2's CON-log spelling (`LOG_CON_ARGS`).
- Produces:
  - `data/k11-captures/<scenario>/`, containing `frame_%04d.raw`, `window.txt`, `poll.log`, `dosbox.log`, `session.txt`, `*.dro` and `last_frame.png`.
  - Testable pure functions `guard_out`, `find_base`, `check_cmos`, `dosbox_cmd`, `bios_new_keys` and `field_pokes`.

- [ ] **Step 1: Write the failing tests**

```python
# tools/tests/test_k11_capture.py
import os, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import k11_capture as kc
import k11_fields as kf


class Capture(unittest.TestCase):
    def test_guard_refuses_everything_but_a_capture_subdir(self):
        for bad in ('/tmp/x', os.path.join(ROOT, 'data', 'game', 'C', 'X'),
                    os.path.join(ROOT, 'data', 'k11-captures'),
                    os.path.join(ROOT, 'data', 'title-captures', 'k11')):
            with self.assertRaises(SystemExit, msg=bad):
                kc.guard_out(bad)
        ok = os.path.join(ROOT, 'data', 'k11-captures', 'walk')
        self.assertEqual(kc.guard_out(ok), os.path.realpath(ok))

    def test_find_base_uses_the_anchor_and_the_check_words(self):
        buf = bytearray(0x200000)
        buf[0x40000:0x40008] = b'RAGE.S16'                      # a decoy without check words
        base = 0x100000
        buf[base + 0x2D:base + 0x35] = b'RAGE.S16'              # data VA 0x8002D
        buf[base + 0x1AFD8:base + 0x1AFE0] = bytes.fromhex('1400040080110032')   # data VA 0x9AFD8
        self.assertEqual(kc.find_base(buf), base)
        self.assertIsNone(kc.find_base(bytearray(0x1000)))

    def test_check_cmos(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertEqual(kc.check_cmos(d), 'absent')
            with open(os.path.join(d, 'CMOS'), 'wb') as f:
                f.write(bytes(2040))
            self.assertEqual(kc.check_cmos(d), 'zero')
            with open(os.path.join(d, 'CMOS'), 'wb') as f:
                f.write(b'\x01' + bytes(2039))
            with self.assertRaises(SystemExit):
                kc.check_cmos(d)

    def test_dosbox_cmd_has_the_verified_flags(self):
        cmd = kc.dosbox_cmd('/r', '/r/C', '/r/CD/RAGECD.ISO', 'walk', 75, 25, 1)
        s = ' '.join(cmd)
        for want in ('-time-limit 75', 'dosbox memory file=/r/guest.mem',
                     'log logfile=/r/dosbox.log', 'MOUNT C "/r/C" -ro',
                     'AUTOTYPE -w 25 -p 1 enter enter down', 'DX-CAPTURE /V /O PRAGE.EXE -f'):
            self.assertIn(want, s)
        for arg in kc.LOG_CON_ARGS:
            self.assertIn(arg, cmd)

    def test_bios_new_keys_wraps(self):
        mem = bytearray(0x500)
        mem[0x43C:0x43E] = (0x1C0D).to_bytes(2, 'little')     # slot 0x3C
        mem[0x41E:0x420] = (0x011B).to_bytes(2, 'little')     # slot 0x1E after the wrap
        self.assertEqual(kc.bios_new_keys(mem, 0x3C, 0x20, 0x1E, 0x3E), [0x1C0D, 0x011B])

    def test_field_pokes_lists_only_changed_bytes(self):
        d = [0] * 63
        d[1] = (1 << 14) | (3 << 6)
        window = bytearray(b'\x11' * (kf.WIN_HI - kf.WIN_LO))
        pokes = kc.field_pokes(window, d, [('field', 1, 0xAB)])
        self.assertEqual(pokes, [(kf.NIB_DS + 1 - kf.WIN_LO, 0x11, 0xB1),
                                 (kf.NIB_DS + 2 - kf.WIN_LO, 0x11, 0x1A)])
        self.assertEqual(window, bytearray(b'\x11' * (kf.WIN_HI - kf.WIN_LO)))   # not mutated


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run it and see it fail**

Run: `python3 -m unittest tools.tests.test_k11_capture -v`
Expected: `ModuleNotFoundError: No module named 'k11_capture'`.

- [ ] **Step 3: Implement `tools/k11_capture.py`**

`LOG_CON_ARGS` is `['-set', 'dos log console=quiet']`, or `['-log-con']` if §A.2 Step 3 took that branch. Use exactly what §A.2 records.

```python
#!/usr/bin/env python3
"""K11 ground-truth capture (plan 2026-09-30-named-gaps-a-k11-harness.md,
record §A.3). The pinned original (make title-pin) runs in DOSBox-X. AUTOTYPE
types the scenario's keys and DX-CAPTURE /V /O records video and OPL. A
poller thread reads the `[dosbox] memory file` (guest RAM, memory-mapped; a
runtime linear address is the file offset) and writes poll.log (format:
k11_session.py). It also applies the scenario's pokes once mode 0x27 is seen.

Writes only data/k11-captures/<scenario>/ (guard_out), and never data/game.
The staging copy lives in a temp dir and mounts read-only, so the original's
CMOS save (0x1B084) cannot write. The CMOS in the game dir must be absent or
2040 zero bytes, because the port models the defaults path (config.c
config_validate). Usage:
  k11_capture.py --scenario NAME --out data/k11-captures/NAME
                 [--time-limit S] [--enter-wait S] [--pace S] [--exe PATH]
                 [--no-pokes] [--keep-avi]"""
import argparse
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
import smk_capture as sc
import title_capture as tcap
import k11_session as ks
import k11_fields as kf

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CAPTURE_ROOT = os.path.join(REPO_ROOT, 'data', 'k11-captures')
DEFAULT_GAME_DIR = os.path.join(REPO_ROOT, 'data', 'game', 'C')
CMOS_BYTES = 2040
ANCHOR_VA, ANCHOR = 0x8002D, b'RAGE.S16'                    # combat-fidelity record §0.2
CHECK_VA, CHECK = 0x9AFD8, bytes.fromhex('1400040080110032')  # demo-pose record §38.1
LOG_CON_ARGS = ['-set', 'dos log console=quiet']            # record §A.2
BDA_HEAD, BDA_TAIL, BDA_START, BDA_END = 0x41A, 0x41C, 0x480, 0x482


def guard_out(out):
    real = os.path.realpath(out)
    root = os.path.realpath(CAPTURE_ROOT)
    if not os.path.normcase(real).casefold().startswith(os.path.normcase(root).casefold() + os.sep):
        raise SystemExit('k11_capture: --out must be a directory under %s: %s' % (CAPTURE_ROOT, out))
    return real


def find_base(buf):
    i = buf.find(ANCHOR)
    while i != -1:
        base = i - (ANCHOR_VA - ks.DATA_BASE_VA)
        chk = base + (CHECK_VA - ks.DATA_BASE_VA)
        if base >= 0 and bytes(buf[chk:chk + len(CHECK)]) == CHECK:
            return base
        i = buf.find(ANCHOR, i + 1)
    return None


def check_cmos(game_dir):
    for name in os.listdir(game_dir):
        if name.upper() == 'CMOS':
            with open(os.path.join(game_dir, name), 'rb') as f:
                data = f.read()
            if len(data) != CMOS_BYTES or any(data):
                raise SystemExit('k11_capture: %s is not %d zero bytes; the port models '
                                 'the defaults path (record §A.0)' % (name, CMOS_BYTES))
            return 'zero'
    return 'absent'


def dosbox_cmd(root, game, iso, scenario, time_limit, enter_wait, pace):
    return ([sc.which('dosbox-x'), '-defaultconf', '-fastlaunch', '-nopromptfolder',
             '-nogui', '-nomenu', '-time-limit', str(time_limit),
             '-set', 'sdl fullscreen=false',
             '-set', 'dosbox captures=%s' % os.path.join(root, 'avi'),
             '-set', 'dosbox memory file=%s' % os.path.join(root, 'guest.mem'),
             '-set', 'log logfile=%s' % os.path.join(root, 'dosbox.log')]
            + LOG_CON_ARGS
            + ['-c', 'MOUNT C "%s" -ro' % game,
               '-c', 'IMGMOUNT D "%s" -t iso' % iso,
               '-c', 'C:',
               '-c', ks.autotype_line(scenario, enter_wait, pace),
               '-c', 'DX-CAPTURE /V /O PRAGE.EXE -f',
               '-c', 'EXIT'])


def u16(buf, off):
    return buf[off] | buf[off + 1] << 8


def bios_new_keys(mem, prev_tail, tail, start, end):
    out, t = [], prev_tail
    while t != tail and len(out) < 16:
        out.append(u16(mem, 0x400 + t))
        t += 2
        if t >= end:
            t = start
    return out


def field_pokes(window, descs, pokes):
    img = bytearray(window)
    for _, fid, value in pokes:
        kf.set_(img, descs, fid, value)
    return [(i, window[i], img[i]) for i in range(len(img)) if img[i] != window[i]]


def read_vals(mm, base):
    vals = {}
    for name, ds, size in ks.POLL_FIELDS:
        off = base + ds - ks.DATA_BASE_VA
        vals[name] = int.from_bytes(mm[off:off + size], 'little')
    return vals


def read_kb(mm, base):
    ptr = int.from_bytes(mm[base + ks.KB_PTR_DS - ks.DATA_BASE_VA:][:4], 'little')
    if ptr == 0 or ptr + 0x2DA > len(mm):
        return 0
    return mm[ptr + 0x2D8] << 8 | mm[ptr + 0x2D9]


class Poller(threading.Thread):
    def __init__(self, mem_path, log_path, pokes, descs, stop):
        super().__init__(daemon=True)
        self.mem_path, self.log_path, self.pokes, self.descs, self.stop = \
            mem_path, log_path, pokes, descs, stop

    def apply(self, mm, base, ms, log):
        wlo = base + kf.WIN_LO - ks.DATA_BASE_VA
        window = bytes(mm[wlo:wlo + kf.WIN_HI - kf.WIN_LO])
        for off, old, new in field_pokes(window, self.descs, [p for p in self.pokes if p[0] == 'field']):
            mm[wlo + off] = new
            log.write('W ms=%d ds=%08X linear=%08X old=%02X new=%02X\n'
                      % (ms, kf.WIN_LO + off, wlo + off, old, new))
        for kind, ds, byte in self.pokes:
            if kind == 'ds_or':
                lin = base + ds - ks.DATA_BASE_VA
                old = mm[lin]
                mm[lin] = old | byte
                log.write('W ms=%d ds=%08X linear=%08X old=%02X new=%02X\n' % (ms, ds, lin, old, old | byte))

    def run(self):
        deadline = time.monotonic() + 60
        while not (os.path.exists(self.mem_path) and os.path.getsize(self.mem_path) >= 1 << 20):
            if self.stop.is_set() or time.monotonic() > deadline:
                return
            time.sleep(0.05)
        with open(self.mem_path, 'r+b') as fh, open(self.log_path, 'w') as log:
            mm = mmap.mmap(fh.fileno(), 0)          # MAP_SHARED, read-write
            t0 = time.monotonic()
            base, prev, prev_img, prev_tail, pending, scan_at = None, None, None, None, bool(self.pokes), 0.0
            wlen = kf.WIN_HI - kf.WIN_LO
            while not self.stop.is_set():
                ms = int((time.monotonic() - t0) * 1000)
                if base is None:
                    if time.monotonic() - scan_at > 0.2:
                        scan_at = time.monotonic()
                        base = find_base(mm)
                        if base is not None:
                            log.write('B ms=%d base=%08X\n' % (ms, base))
                    time.sleep(0.005)
                    continue
                vals, kb, tail = read_vals(mm, base), read_kb(mm, base), u16(mm, BDA_TAIL)
                if prev_tail is None:
                    prev_tail = tail
                if tail != prev_tail:
                    start, end = u16(mm, BDA_START) or 0x1E, u16(mm, BDA_END) or 0x3E
                    for w in bios_new_keys(mm, prev_tail, tail, start, end):
                        log.write('K ms=%d tick=%08X f=%04X key=%04X\n' % (ms, vals['tick'], vals['f'], w))
                    prev_tail = tail
                if (vals, kb) != prev:
                    log.write(ks.format_p(ms, vals, kb, tail) + '\n')
                    prev = (vals, kb)
                wlo = base + kf.WIN_LO - ks.DATA_BASE_VA
                img = bytes(mm[wlo:wlo + wlen])
                if img != prev_img:
                    log.write('F ms=%d tick=%08X img=%s\n' % (ms, vals['tick'], img.hex()))
                    prev_img = img
                if pending and vals['mode'] == 0x27:
                    self.apply(mm, base, ms, log)
                    pending = False
                time.sleep(0.0005)
            log.write('E ms=%d reason=%s rc=%d\n' % (int((time.monotonic() - t0) * 1000),
                                                     getattr(self, 'reason', 'exit'), getattr(self, 'rc', -1)))
            mm.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--scenario', required=True, choices=sorted(ks.SCENARIOS))
    ap.add_argument('--out', required=True)
    ap.add_argument('--game-dir', default=DEFAULT_GAME_DIR)
    ap.add_argument('--exe', default=tcap.PIN_EXE)
    ap.add_argument('--time-limit', type=int)
    ap.add_argument('--enter-wait', type=float, default=ks.ENTER_WAIT)
    ap.add_argument('--pace', type=float, default=ks.PACE)
    ap.add_argument('--no-pokes', action='store_true')
    ap.add_argument('--keep-avi', action='store_true')
    a = ap.parse_args()
    out = guard_out(a.out)
    scn = ks.SCENARIOS[a.scenario]
    if not os.path.isfile(a.exe):
        sys.exit('k11_capture: %s missing (run make title-pin)' % a.exe)
    cmos = check_cmos(a.game_dir)
    pokes = () if a.no_pokes else scn['pokes']
    descs = kf.load_descriptors(a.exe)
    limit = a.time_limit or scn['time_limit']
    root = tempfile.mkdtemp(prefix='k11cap-')
    try:
        game = tcap.stage(root, a.exe, a.game_dir)
        iso = os.path.join(root, 'CD', 'RAGECD.ISO')
        os.makedirs(os.path.join(root, 'avi'))
        cmd = dosbox_cmd(root, game, iso, a.scenario, limit, a.enter_wait, a.pace)
        print('k11_capture: %s' % shlex.join(cmd))
        stop = threading.Event()
        poll = Poller(os.path.join(root, 'guest.mem'), os.path.join(root, 'poll.log'), pokes, descs, stop)
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
            sys.exit('k11_capture: no AVI in %s' % avi_dir)
        paths = [os.path.join(avi_dir, n) for n in avis]
        cap = [hashlib.md5(d).digest() for d in sc.read_avi_frames(paths)]
        start, logs = tcap.post_logo_start(cap, a.game_dir)
        hs, idx = tcap.collapse_from(cap, start)
        os.makedirs(out, exist_ok=True)
        tcap.write_window(out, idx, tcap.frames_at(paths, idx),
                          ['mode=k11-%s' % a.scenario, 'raw_window=%d..%d' % (idx[0], idx[-1]),
                           'twi5_last=%s twg_last=%s' % (logs.get('twi5'), logs.get('twg'))])
        for name in ('poll.log', 'dosbox.log'):
            src = os.path.join(root, name)
            if os.path.exists(src):
                shutil.copyfile(src, os.path.join(out, name))
        dros = sorted(f for f in os.listdir(avi_dir) if f.lower().endswith('.dro'))
        for n in dros:
            shutil.copyfile(os.path.join(avi_dir, n), os.path.join(out, n))
        subprocess.run([sc.which('ffmpeg'), '-v', 'error', '-y', '-sseof', '-1', '-i', paths[-1],
                        '-update', '1', os.path.join(out, 'last_frame.png')])
        ver = subprocess.run([sc.which('dosbox-x'), '-version'], capture_output=True, text=True).stdout
        with open(os.path.join(out, 'session.txt'), 'w') as f:
            f.write('scenario=%s\n' % a.scenario)
            f.write('dosbox=%s\n' % next((l for l in ver.splitlines() if 'DOSBox-X version' in l), '?'))
            f.write('argv=%s\n' % shlex.join(cmd))
            f.write('exe=%s sha256=%s\n' % (a.exe, hashlib.sha256(open(a.exe, 'rb').read()).hexdigest()))
            f.write('cmos=%s pokes=%s\n' % (cmos, list(pokes)))
            f.write('time_limit=%d wall_s=%.1f rc=%d\n' % (limit, wall, r.returncode))
            f.write('avis=%s fps=%.4f dro=%s\n' % (avis, sc.ffprobe_fps(paths[0]), dros))
        recs = []
        if os.path.exists(os.path.join(out, 'poll.log')):          # absent: no memory file (F5)
            with open(os.path.join(out, 'poll.log')) as f:
                recs = [ks.parse(l) for l in f if l.strip()]
        keys = [k for k in scn['keys'] if k != ',']
        kcount = sum(1 for x in recs if x['kind'] == 'K')
        first_k = next((x['ms'] for x in recs if x['kind'] == 'K'), None)
        saw27 = first_k is not None and any(x['kind'] == 'P' and x['ms'] >= first_k and x['mode'] == 0x27 for x in recs)
        checks = [('base', any(x['kind'] == 'B' for x in recs)),
                  ('keys %d/%d' % (kcount, len(keys)), kcount == len(keys)),
                  ('mode 0x27 after the Enter', saw27),
                  ('pokes', not pokes or any(x['kind'] == 'W' for x in recs))]
        for name, ok in checks:
            print('k11_capture: CHECK %s: %s' % (name, 'ok' if ok else 'FAIL'))
        print('k11_capture: wrote %d frames to %s (raw %d..%d), wall %.1fs'
              % (len(idx), out, idx[0], idx[-1], wall))
        return 0 if all(ok for _, ok in checks) else 1
    finally:
        if a.keep_avi:
            print('k11_capture: kept %s' % root)
        else:
            shutil.rmtree(root, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
```

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_k11_capture -v`
Expected: `Ran 6 tests`, `OK`.

- [ ] **Step 5: Mutation proof (then revert)**

In `guard_out`, drop the `+ os.sep` from the prefix test. Expected: `test_guard_refuses_everything_but_a_capture_subdir` FAILS, because `data/k11-captures` itself is then accepted. Revert and rerun: `OK`.

- [ ] **Step 6: Commit**

```bash
git add tools/k11_capture.py tools/tests/test_k11_capture.py
git commit -m "$(cat <<'EOF'
tools: k11_capture, the DOSBox-X service-menu capture and live-RAM poller

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: The port side: the K11 oracle driver and the host key-bits seam

**Files:**
- Modify: `port/src/host.h` (declare the seam), `port/src/host.c` (`host_key_bits`, the seam)
- Modify: `port/tests/test_platform.c` (`test_host`: two new checks)
- Modify: `port/tests/test_game.c` (append the driver after `test_svcmenu`'s block, at the end of the file; `fe_write_frame` at `:4754` is reused)
- Modify: `port/tests/test.h` (`TEST_DRIVERS`: one line)

**Interfaces:**
- Consumes: the port-script format (Task 4); `game_init`, `game_loop`, `actors_pin_anim_tick_zero`, `input_push`, `config_field_set`, `host_set_pump_hook`, `gfx_display`, `gfx_dac`, `fe_write_frame`
- Produces:
  - `void host_set_key_bits_override(u16 bits, int on)`.
  - `int test_k11_oracle(void)`, run alone under `PR_K11_DUMP=<dir> PR_K11_SCRIPT=<file>`.
  - `<dir>/frame_%04d.raw`, `<dir>/screens.txt` (`key <i> settled <frame>` … `end settled <frame>`), `<dir>/k11.log` (`key <i> dt=<dec> tick=<hex8> f=<hex4> mode=<hex4> st=<hex4> ent=<hex8>`), and `<dir>/fimg_before.bin`/`fimg_after.bin` when the script has `field` lines.

Why the seam: `game_loop` rewrites the key bitmap from `host_key_bits()` at the start of every iteration (`flow.c:6729..6737`), so a driver write there is lost. Task 1 Step 6 shows that the original's menu Up/Down arrive only through that bitmap.

- [ ] **Step 1: Failing seam test**

In `port/tests/test_platform.c`, inside `test_host`, just before its final `return g_failures - before;`:

```c
    /* The K11 key-bits seam (named-gaps A record §A.3): the driver holds the
     * key bitmap game_loop copies from host_key_bits(). The sentinel 0x1234 is
     * no SDL keyboard state a headless run can report. */
    host_set_key_bits_override(0x1234u, 1);
    CHECK_EQ_INT((int)host_key_bits(), 0x1234);
    host_set_key_bits_override(0x1234u, 0);
    CHECK(host_key_bits() != 0x1234u, "the key-bits override is off again");
```

Run: `cmake --build build 2>&1 | grep -E 'error|warning' | head`
Expected: an implicit-declaration/undefined-symbol error for `host_set_key_bits_override`.

- [ ] **Step 2: The seam**

In `port/src/host.h`, after `host_key_bits`'s declaration:

```c
/* PORT: test seam, no raw counterpart. With `on` set, host_key_bits() returns
 * `bits` instead of the SDL keyboard state; the K11 oracle driver holds the key
 * bitmap a capture's poll log recorded through it (named-gaps A record §A.3).
 * Off by default; nothing outside the tests sets it. */
void host_set_key_bits_override(u16 bits, int on);
```

In `port/src/host.c`, replace the `host_key_bits` definition with:

```c
static int g_key_bits_override_on;
static u16 g_key_bits_override;

void host_set_key_bits_override(u16 bits, int on)
{
    g_key_bits_override = bits;
    g_key_bits_override_on = on;
}

u16 host_key_bits(void)
{
    /* PORT: the test seam declared in host.h. */
    if (g_key_bits_override_on) return g_key_bits_override;
    const bool *st = SDL_GetKeyboardState(NULL);
    if (st == NULL) return 0u;
    u16 bits = 0u;
    for (int i = 0; i < 16; i++)
        if (st[k_input_bind[i]]) bits |= (u16)(1u << i);
    return bits;
}
```

Run: `cmake --build build 2>&1 | grep -cE 'error|warning'; PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests | tail -1`
Expected: `0`, then `all checks passed`.

- [ ] **Step 3: Seam mutation (then revert)**

Delete the `if (g_key_bits_override_on) …` line. Rebuild and rerun `./build/run_tests | grep -E 'FAIL|FAILURES'`. Expected: a `FAIL … test_platform.c` line for the `CHECK_EQ_INT`, then `FAILURES: 1`. Restore the line. Record this in §A.3.

- [ ] **Step 4: The driver**

Append to `port/tests/test_game.c`:

```c
/* ---- the K11 ground-truth driver (named-gaps A, record
 * docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md §A.3) ----
 * PR_K11_DUMP=<dir> PR_K11_SCRIPT=<file> (tools/k11_session.py port-script).
 * It runs game_init() and the master loop from boot, as the pinned original
 * does (actors_pin_anim_tick_zero mirrors the title pin's opcode-8 site). It
 * queues the capture's mode-3 Enter (scan 0x1C, ascii 0x0D) before the
 * iteration whose game_frame raises DS_000EF6DC to `enter_frame`, and applies
 * the script's pokes there. From the host pump hook it then queues each `key`
 * and holds each `pad` once the ISR clock DS_00101500 has run `dtick` past the
 * Enter's. Every distinct displayed frame (indices + gfx_dac) from the Enter on
 * becomes <dir>/frame_%04d.raw through fe_write_frame. */
#define K11_MAX_STEPS 512u
#define K11_MAX_LOOPS 20000u
#define K11_STALL_PUMPS 200000u
#define K11_FIMG_END 0x00105E30u   /* tools/k11_fields.py WIN_HI */
typedef struct { char op; u32 a, b, c; } K11Step;
static K11Step k11_step[K11_MAX_STEPS];
static u32 k11_n, k11_next, k11_nkeys, k11_keys_sent, k11_enter_frame, k11_enter_state;
static u32 k11_t0, k11_pad_end, k11_dumped, k11_hash_last, k11_idle_pumps;
static int k11_armed, k11_done, k11_pad_on, k11_failed;
static char k11_dir[1024];
static FILE *k11_screens, *k11_log;

static int k11_parse(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) return 0;
    char line[256];
    int have_frame = 0, have_state = 0, ok = 1;
    k11_n = 0u;
    k11_nkeys = 0u;
    while (fgets(line, sizeof line, f) != NULL) {
        unsigned a = 0u, b = 0u, c = 0u;
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "enter_frame %u", &a) == 1) { k11_enter_frame = a; have_frame = 1; continue; }
        if (sscanf(line, "enter_state %x", &a) == 1) { k11_enter_state = a; have_state = 1; continue; }
        if (k11_n >= K11_MAX_STEPS) { ok = 0; break; }
        K11Step *s = &k11_step[k11_n];
        if (sscanf(line, "key %u %x %x", &a, &b, &c) == 3) { s->op = 'k'; k11_nkeys++; }
        else if (sscanf(line, "pad %u %x %u", &a, &b, &c) == 3) s->op = 'p';
        else if (sscanf(line, "ds_or %x %x", &a, &b) == 2) s->op = 'o';
        else if (sscanf(line, "field %x %x", &a, &b) == 2) s->op = 'f';
        else if (sscanf(line, "end %u", &a) == 1) s->op = 'e';
        else { ok = 0; break; }
        s->a = a; s->b = b; s->c = c;
        k11_n++;
    }
    fclose(f);
    return ok && have_frame && have_state && k11_n > 0u && k11_step[k11_n - 1u].op == 'e';
}

static u32 k11_fnv(u32 h, const u8 *p, u32 n)
{
    for (u32 i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

static void k11_dump_if_new(void)
{
    const u8 *fb = gfx_display();
    if (fb == NULL) fb = mem + DSD(DS_000E87A0);
    const u32 h = k11_fnv(k11_fnv(2166136261u, fb, 64000u), &gfx_dac[0][0], 768u);
    if (k11_dumped > 0u && h == k11_hash_last) return;
    char path[1200];
    snprintf(path, sizeof path, "%s/frame_%04u.raw", k11_dir, k11_dumped);
    if (!fe_write_frame(path)) { k11_failed = 1; return; }
    k11_hash_last = h;
    k11_dumped++;
}

/* The key bitmap [DS_00101514]+0x2D8/+0x2D9 (record §A.1.5): game_loop copies
 * host_key_bits() into it each iteration (flow.c 0x500C4 prologue), and the
 * blocking loops' 0x500C4 pump reads it as it stands, so set both. */
static void k11_key_bits(u32 kb)
{
    host_set_key_bits_override((u16)kb, kb != 0u);
    u8 *k = mem + DSD(DS_00101514);
    k[0x2D8] = (u8)(kb >> 8);
    k[0x2D9] = (u8)kb;
}

static void k11_hook(void *ctx)
{
    (void)ctx;
    if (!k11_armed || k11_done || k11_failed) return;
    k11_dump_if_new();
    const u32 dt = DSD(DS_00101500) - k11_t0;
    if (k11_pad_on && dt >= k11_pad_end) { k11_key_bits(0u); k11_pad_on = 0; }
    while (k11_next < k11_n) {
        const K11Step *s = &k11_step[k11_next];
        if (s->op == 'o' || s->op == 'f') { k11_next++; continue; }   /* applied at the Enter */
        if (dt < s->a) break;
        if (s->op == 'k') {
            fprintf(k11_screens, "key %u settled %u\n", k11_keys_sent, k11_dumped - 1u);
            input_push((u8)s->b, (u8)s->c);
            fprintf(k11_log, "key %u dt=%u tick=%08X f=%04X mode=%04X st=%04X ent=%08X\n",
                    k11_keys_sent, dt, DSD(DS_00101500), DSW(DS_000EF6DC), DSW(DS_00104B00),
                    DSW(DS_000F0A64), DSD(DS_0010741C));
            k11_keys_sent++;
        } else if (s->op == 'p') {
            k11_key_bits(s->b);
            k11_pad_on = 1;
            k11_pad_end = s->a + s->c;
        } else {
            fprintf(k11_screens, "end settled %u\n", k11_dumped - 1u);
            k11_done = 1;
        }
        k11_next++;
        k11_idle_pumps = 0u;
        if (k11_done) break;
    }
    if (++k11_idle_pumps > K11_STALL_PUMPS) {
        printf("FAIL %s:%d: the K11 script stalled at step %u (dt %u)\n", __FILE__, __LINE__, k11_next, dt);
        exit(1);
    }
}

static void k11_write_fimg(const char *name)
{
    char path[1200];
    snprintf(path, sizeof path, "%s/%s", k11_dir, name);
    FILE *f = fopen(path, "wb");
    if (f == NULL || fwrite(mem + DS_00105DAF, 1, K11_FIMG_END - DS_00105DAF, f) != K11_FIMG_END - DS_00105DAF)
        k11_failed = 1;
    if (f != NULL) fclose(f);
}

static void k11_apply_pokes(void)
{
    int fields = 0;
    for (u32 i = 0; i < k11_n; i++) if (k11_step[i].op == 'f') fields = 1;
    if (fields) k11_write_fimg("fimg_before.bin");
    const u8 dd8 = DSB(DS_00105DD8);
    for (u32 i = 0; i < k11_n; i++) {
        const K11Step *s = &k11_step[i];
        if (s->op == 'o') DSB(s->a) = (u8)(DSB(s->a) | s->b);
        else if (s->op == 'f') (void)config_field_set(s->a, s->b);
    }
    /* The capture pokes the field image only (tools/k11_fields.py set_), not
     * config_field_set's dirty bits 0x2DA3B/0x2DA4E. */
    DSB(DS_00105DD8) = dd8;
    if (fields) k11_write_fimg("fimg_after.bin");
}

int test_k11_oracle(void)
{
    const int before = g_failures;
    const char *dump = getenv("PR_K11_DUMP");
    const char *script = getenv("PR_K11_SCRIPT");
    if (dump == NULL || dump[0] == '\0') return 0;
    CHECK(script != NULL && script[0] != '\0' && k11_parse(script),
          "PR_K11_SCRIPT names a parsable K11 port script");
    if (g_failures != before) return g_failures - before;
    snprintf(k11_dir, sizeof k11_dir, "%s", dump);
    mkdir(dump, 0777);      /* ignore EEXIST, as the front-end driver does */
    char p[1200];
    snprintf(p, sizeof p, "%s/screens.txt", dump);
    k11_screens = fopen(p, "w");
    snprintf(p, sizeof p, "%s/k11.log", dump);
    k11_log = fopen(p, "w");
    CHECK(k11_screens != NULL && k11_log != NULL, "the K11 dump files open");
    if (k11_screens == NULL || k11_log == NULL) return g_failures - before;

    const char *dir = getenv("PR_GAME_DIR");
    if (dir == NULL || dir[0] == '\0') dir = "data/game/C";
    game_set_game_dir(dir);
    game_init();
    actors_pin_anim_tick_zero(1);
    host_set_pump_hook(k11_hook, NULL);

    /* Sentinels: none is a mode, frame or state the checks below accept. */
    u32 mode_before = 0xFFFFu, mode_after = 0xFFFFu, frame_after = 0xFFFFFu, state_after = 0xFFFFFu;
    for (u32 i = 0; i < K11_MAX_LOOPS && !k11_done && !k11_failed; i++) {
        const int enter_now = !k11_armed && DSW(DS_000EF6DC) == (u16)(k11_enter_frame - 1u);
        if (enter_now) {
            mode_before = DSW(DS_00104B00);
            k11_apply_pokes();
            input_push(0x1Cu, 0x0Du);                   /* the capture's mode-3 Enter */
            k11_t0 = DSD(DS_00101500);
            k11_armed = 1;
        }
        DSB(DS_000A81A8) = 1;                           /* exactly one game_loop iteration */
        game_loop();
        if (enter_now) {
            mode_after = DSW(DS_00104B00);
            frame_after = DSW(DS_000EF6DC);
            state_after = DSW(DS_000F0A64);
        }
    }
    host_set_pump_hook(NULL, NULL);
    k11_key_bits(0u);
    fclose(k11_screens);
    fclose(k11_log);

    CHECK(k11_armed, "the loop reached the capture's Enter frame");
    CHECK_EQ_INT((int)mode_before, 3);                  /* 0x24ECF: the Enter arm needs mode 3 */
    CHECK_EQ_INT((int)mode_after, 0x27);                /* 0x24EE0 */
    CHECK_EQ_INT((int)frame_after, (int)k11_enter_frame);
    CHECK_EQ_INT((int)state_after, (int)k11_enter_state);
    CHECK(k11_done, "the K11 script ran to its end");
    CHECK_EQ_INT((int)k11_keys_sent, (int)k11_nkeys);
    CHECK(!k11_failed && k11_dumped > 1u, "the K11 frames were written");
    return g_failures - before;
}
```

In `port/tests/test.h`, extend `TEST_DRIVERS`:

```c
#define TEST_DRIVERS(X)                       \
    X(test_attract,  "PR_ATTRACT_DUMP")       \
    X(test_title,    "PR_TITLE_DUMP")         \
    X(test_frontend, "PR_FRONTEND_DUMP")      \
    X(test_k11_oracle, "PR_K11_DUMP")
```

Run: `cmake --build build 2>&1 | grep -cE 'error|warning'`
Expected: `0`. The planner built exactly these edits (seam, test, driver, `TEST_DRIVERS` line) on a scratch copy of `port/` at `a169296` with no error or warning. `test_game.c` already includes everything the driver uses.

- [ ] **Step 5: Red, then green, on a smoke script (no capture needed)**

```bash
S=/tmp/named-gaps-a
cat > $S/k11smoke.script <<'EOF'
# smoke: the MAIN MENU, START MENU and back (not a capture; Task 6 only)
enter_frame 300
enter_state FFFF
key 60 1C 0D
key 120 01 1B
end 240
EOF
rm -rf $S/k11smoke; PR_K11_DUMP=$S/k11smoke PR_K11_SCRIPT=$S/k11smoke.script PR_GAME_DIR=data/game/C ./build/run_tests; echo "exit=$?"
```

Expected (red): `exit=1`, with exactly one failure: `FAIL …/test_game.c:<line>: 0 != 65535`, the `state_after` check (the port is in state 0 at frame 300) followed by `FAILURES: 1`. `mode_before` is 3 and `mode_after` is `0x27`, so their checks pass. The planner ran this driver on a scratch copy of the tree and saw exactly that.

Set `enter_state 0000` and rerun. Expected (green): `all checks passed`, `exit=0`. The planner's run gave 9 frames (`frame_0000..0008.raw`). `screens.txt` held `key 0 settled 2`, `key 1 settled 6`, `end settled 8`, and `k11.log` held `key 0 dt=60 tick=00000173 f=0165 mode=0027 st=0000 ent=000BCBEC` and `key 1 dt=120 tick=000001AF f=019F mode=0027 st=0000 ent=000BCCDC`. Settled frame 6 is the START MENU (MAIN MENU "Start" → `0xBCCDC`). Record the executor's own values, which may differ only if the tree differs.

- [ ] **Step 6: Driver mutation (then revert)**

Change the Enter to `input_push(0x1Cu, 0x0Au)`, rebuild and rerun the green smoke. Expected: `FAIL` on `mode_after` (3 ≠ 0x27) and `exit=1`. Restore it, rebuild and rerun: `all checks passed`.

- [ ] **Step 7: Gate: no oracle line moves**

```bash
make verify > "$S/a_t6_verify.txt" 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare)' "$S/a_t6_verify.txt" | diff - "$S/a_or_base.txt" && echo ORACLES-EQUAL
```

Expected: `verify-exit=0` and `ORACLES-EQUAL`. The seam is off in every oracle run, and the driver runs only under `PR_K11_DUMP`.

- [ ] **Step 8: Record and commit**

In §A.3, record the seam's two checks and their mutation, the smoke run (the frame and state values), the driver mutation and the gate.

```bash
git add port/src/host.h port/src/host.c port/tests/test_platform.c port/tests/test_game.c port/tests/test.h docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md
git commit -m "$(cat <<'EOF'
port: K11 oracle driver (PR_K11_DUMP) and the host key-bits test seam (record §A.3)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 7: `tools/k11_compare.py`, the K11 oracle

**Files:**
- Create: `tools/k11_compare.py`
- Create: `tools/tests/test_k11_compare.py`

**Interfaces:**
- Consumes: `tools/title_compare.py` (`load_frames`, `raw_map`, `row_hashes`, `row_common`, `explain`, `FRAME_BYTES`, `FRAME_W`, `FRAME_H`, `ROW`); the capture dir; the port dump with `screens.txt`
- Produces: exit 0/1, and these lines:
  - `k11_compare: <name>: window distinct [a..b] (raw ra..rb)`
  - `k11_compare: <name>: N frames in window: c clean, s splice, t transition, u unexplained, k all-black`
  - `k11_compare: <name>: settled screens exhibited X/Y; missing [...]`
  - `k11_compare: <name>: 0 unexplained in the window`, or `k11_compare: <name>: FIRST UNEXPLAINED capture j (raw r): nearest port M, differs in rows r0..r1, x x0..x1 (n px)`

- [ ] **Step 1: Write the failing tests**

```python
# tools/tests/test_k11_compare.py
import os, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOL = os.path.join(ROOT, 'tools', 'k11_compare.py')
W, H = 320, 200
ROW = W * 3


def frame(v):
    """Every row distinct (so row hashes separate frames), seeded by v."""
    return b''.join(bytes([(v + r) & 0xFF]) * ROW for r in range(H))


def splice(a, b, row):
    return a[:row * ROW] + b[row * ROW:]


def put(d, frames):
    os.makedirs(d, exist_ok=True)
    for i, f in enumerate(frames):
        with open(os.path.join(d, 'frame_%04d.raw' % i), 'wb') as fh:
            fh.write(f)


def run(capture, port, *extra, env=None):
    e = dict(os.environ)
    e.pop('PR_ORACLE_REQUIRED', None)
    e.update(env or {})
    return subprocess.run([sys.executable, TOOL, '--capture', capture, '--port', port] + list(extra),
                          capture_output=True, text=True, env=e)


class K11Compare(unittest.TestCase):
    A, B, C, X, Y = frame(1), frame(50), frame(90), frame(200), frame(120)

    def case(self, capture, port, screens, *extra, env=None):
        with tempfile.TemporaryDirectory() as d:
            put(os.path.join(d, 'cap'), capture)
            put(os.path.join(d, 'port'), port)
            with open(os.path.join(d, 'port', 'screens.txt'), 'w') as fh:
                fh.write(screens)
            return run(os.path.join(d, 'cap'), os.path.join(d, 'port'), *extra, env=env)

    def test_all_explained_passes(self):
        r = self.case([self.X, self.A, splice(self.A, self.B, 100), self.B], [self.A, self.B],
                      'key 0 settled 0\nend settled 1\n')
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn('0 unexplained in the window', r.stdout)

    def test_unexplained_in_window_fails(self):
        r = self.case([self.X, self.A, self.Y, self.B], [self.A, self.B], 'key 0 settled 0\nend settled 1\n')
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn('FIRST UNEXPLAINED capture 2', r.stdout)

    def test_missing_settled_screen_fails(self):
        r = self.case([self.A, self.B], [self.A, self.C, self.B],
                      'key 0 settled 0\nkey 1 settled 1\nend settled 2\n')
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn('missing [1]', r.stdout)

    def test_all_black_is_excluded(self):
        r = self.case([self.A, bytes(W * H * 3), self.B], [self.A, self.B], 'key 0 settled 0\nend settled 1\n')
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn('1 all-black', r.stdout)

    def test_absent_capture_skips_unless_required(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertEqual(run(os.path.join(d, 'nope'), d).returncode, 0)
            self.assertEqual(run(os.path.join(d, 'nope'), d, env={'PR_ORACLE_REQUIRED': '1'}).returncode, 1)

    def test_report_always_exits_zero(self):
        r = self.case([self.X, self.A, self.Y, self.B], [self.A, self.B],
                      'key 0 settled 0\nend settled 1\n', '--report')
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn('differs in rows', r.stdout)

    def test_empty_window_fails(self):
        r = self.case([self.X], [self.A, self.B], 'key 0 settled 0\nend settled 1\n')
        self.assertEqual(r.returncode, 1, r.stdout)
        self.assertIn('window empty', r.stdout)


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run it and see it fail**

Run: `python3 -m unittest tools.tests.test_k11_compare -v`
Expected: 7 failures or errors (the tool does not exist: `can't open file`).

- [ ] **Step 3: Implement `tools/k11_compare.py`**

```python
#!/usr/bin/env python3
"""The K11 service-menu oracle (plan 2026-09-30-named-gaps-a-k11-harness.md).
It compares a DOSBox-X capture of the pinned original (tools/k11_capture.py,
data/k11-captures/<scenario>) with the port's PR_K11_DUMP frames
(test_k11_oracle). The comparison is byte-exact and uses title_compare's model:
clean, a byte splice of adjacent port frames, or one transition row.

The window runs from the first capture frame that exhibits a port frame at or
before the first settled screen (screens.txt `key 0 settled N`) to the last
capture frame that exhibits the final settled screen (`end settled N`). Each
claim is a failure when broken:
  1. the window is not empty;
  2. every capture frame in it is explained, except all-black frames (a
     documented capture artefact, port/spec/game_flow.md) and the frames
     K11_ALLOWED_UNEXPLAINED names with their record reference;
  3. every settled screen of screens.txt is exhibited by a capture frame in
     the window, so a port that renders less than the original fails.
--report prints the same, plus the first unexplained frames' difference boxes,
and always exits 0 (the evidence scenarios). An absent capture skips (exit 0)
unless PR_ORACLE_REQUIRED=1. Stdlib only; title_compare is read-only here."""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import title_compare as tc

# scenario -> {capture index: 'record §A.x: reason'}. Only plan Task 10 adds rows.
K11_ALLOWED_UNEXPLAINED = {}
REPORT_MAX = 5


def exhibited(kind, data):
    s = set()
    if kind == 'clean':
        s.add(data)
    elif kind == 'splice':
        for (N, lo, hi) in data:
            if hi > 0:
                s.add(N)
            if lo < tc.FRAME_BYTES:
                s.add(N + 1)
    elif kind == 'transition':
        for (N, _r, _a, _b) in data:
            s.update((N, N + 1))
    return s


def read_screens(path):
    keys, end = [], None
    with open(path) as f:
        for line in f:
            parts = line.split()
            if len(parts) == 4 and parts[0] == 'key' and parts[2] == 'settled':
                keys.append(int(parts[3]))
            elif len(parts) == 3 and parts[0] == 'end' and parts[1] == 'settled':
                end = int(parts[2])
    return keys, end


def diff_box(c, p):
    rows = [r for r in range(tc.FRAME_H) if c[r * tc.ROW:(r + 1) * tc.ROW] != p[r * tc.ROW:(r + 1) * tc.ROW]]
    if not rows:
        return None
    xs = [x for r in rows for x in range(tc.FRAME_W)
          if c[r * tc.ROW + 3 * x:r * tc.ROW + 3 * x + 3] != p[r * tc.ROW + 3 * x:r * tc.ROW + 3 * x + 3]]
    return rows[0], rows[-1], min(xs), max(xs), len(xs)


def nearest(ch, port_rows):
    best, best_m = -1, 0
    for m, ph in enumerate(port_rows):
        a, b = tc.row_common(ch, ph)
        if min(a + b, tc.FRAME_H) > best:
            best, best_m = min(a + b, tc.FRAME_H), m
    return best_m


def compare(name, frames, raws, port, port_rows, keys, end_settled, report):
    n = len(port)
    cache = {}

    def ex(j):
        if j not in cache:
            cache[j] = tc.explain(frames[j], tc.row_hashes(frames[j]), port, port_rows, n)
        return cache[j]

    first = keys[0] if keys else end_settled
    start = next((j for j in range(len(frames)) if any(frames[j])
                  and any(m <= first for m in exhibited(*ex(j)))), None)
    end = next((j for j in range(len(frames) - 1, -1, -1) if any(frames[j])
                and end_settled in exhibited(*ex(j))), None)
    if start is None or end is None or end < start:
        print("k11_compare: %s: window empty (start %s, end %s): the capture never exhibits the "
              "port's first or final settled screen" % (name, start, end))
        if not report or start is None:
            return 1
        end = len(frames) - 1
    allowed = K11_ALLOWED_UNEXPLAINED.get(name, {})
    counts = {'clean': 0, 'splice': 0, 'transition': 0, 'unexplained': 0}
    unexpl, black, exh = [], [], set()
    for j in range(start, end + 1):
        if not any(frames[j]):
            black.append(j)
            continue
        kind, data = ex(j)
        counts[kind] += 1
        if kind == 'unexplained':
            unexpl.append(j)
        else:
            exh |= exhibited(kind, data)
    wanted = sorted(set(keys + ([end_settled] if end_settled is not None else [])))
    missing = [m for m in wanted if m not in exh]
    bad = [j for j in unexpl if j not in allowed]
    print('k11_compare: %s: window distinct [%d..%d] (raw %d..%d)' % (name, start, end, raws[start], raws[end]))
    print('k11_compare: %s: %d frames in window: %d clean, %d splice, %d transition, %d unexplained, %d all-black'
          % (name, end - start + 1, counts['clean'], counts['splice'], counts['transition'],
             counts['unexplained'], len(black)))
    print('k11_compare: %s: settled screens exhibited %d/%d; missing %s'
          % (name, len(wanted) - len(missing), len(wanted), missing))
    if allowed and unexpl:
        print('k11_compare: %s: allowed by name: %s' % (name, sorted(j for j in unexpl if j in allowed)))
    if not bad:
        print('k11_compare: %s: 0 unexplained in the window' % name)
    for k, j in enumerate(bad[:REPORT_MAX if report else 1]):
        ch = tc.row_hashes(frames[j])
        m = nearest(ch, port_rows)
        box = diff_box(frames[j], port[m])
        label = 'FIRST UNEXPLAINED' if k == 0 else 'UNEXPLAINED'
        print('k11_compare: %s: %s capture %d (raw %d): nearest port %d, differs in rows %d..%d, x %d..%d (%d px)'
              % ((name, label, j, raws[j], m) + box))
    return 0 if not bad and not missing else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--capture', required=True)
    ap.add_argument('--port', required=True)
    ap.add_argument('--scenario')
    ap.add_argument('--report', action='store_true')
    a = ap.parse_args()
    name = a.scenario or os.path.basename(os.path.normpath(a.capture))
    required = os.environ.get('PR_ORACLE_REQUIRED') == '1'
    if not os.path.isdir(a.capture):
        print('k11_compare: no capture at %s (%s)' % (a.capture, 'FAIL (required)' if required else 'skipped'))
        return 1 if required and not a.report else 0
    screens = os.path.join(a.port, 'screens.txt')
    if not os.path.isfile(screens):
        print('k11_compare: no port dump at %s (screens.txt missing)' % a.port)
        return 0 if a.report else 1
    frames = tc.load_frames(a.capture, name)
    port = tc.load_frames(a.port, 'port')
    if not frames or not port:
        print('k11_compare: %s: empty or malformed frames (capture %s, port %s)'
              % (name, None if frames is None else len(frames), None if port is None else len(port)))
        return 0 if a.report else 1
    raws = tc.raw_map(a.capture) or list(range(len(frames)))
    keys, end_settled = read_screens(screens)
    rc = compare(name, frames, raws, port, [tc.row_hashes(p) for p in port], keys, end_settled, a.report)
    return 0 if a.report else rc


if __name__ == '__main__':
    sys.exit(main())
```

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_k11_compare -v`
Expected: `Ran 7 tests`, `OK`.

- [ ] **Step 5: Mutation proof (then revert)**

Replace `missing = [m for m in wanted if m not in exh]` with `missing = []`. Expected: `test_missing_settled_screen_fails` FAILS (exit 0). Revert and rerun: `OK`.

- [ ] **Step 6: Commit**

```bash
git add tools/k11_compare.py tools/tests/test_k11_compare.py
git commit -m "$(cat <<'EOF'
tools: k11_compare, the K11 service-menu oracle with a coverage claim

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: `make` targets and the `make verify` integration

**Files:**
- Modify: `Makefile`
- Modify: `AGENTS.md` (the Commands block: two lines)

**Interfaces:**
- Consumes: Tasks 3–7
- Produces:
  - `make k11-capture [scenario=walk] [K11_ARGS=...]`.
  - `make k11-oracle`, which is in `make verify`.
  - `make k11-report` (the evidence scenarios, report-only).
  - The unit-test line `python3 -m unittest tools.tests.test_k11_fields tools.tests.test_k11_session tools.tests.test_k11_capture tools.tests.test_k11_compare` in `make verify`.

- [ ] **Step 1: Add the targets**

In `Makefile`, after `ATTRACT_DUMP`/`FRONTEND_DUMP`:

```make
K11_CAPTURES = data/k11-captures
K11_DUMP = /tmp/pr_k11_dump
scenario ?= walk
K11_ARGS ?=
```

Add `k11-capture k11-oracle k11-report` to `.PHONY`. After the `attract2-compare` target:

```make
# K11 service-menu capture (named-gaps A): the pinned original under DOSBox-X,
# AUTOTYPE-driven into mode 0x27, with a live-RAM poll log. Writes only
# data/k11-captures/<scenario>/ (tools/k11_capture.py guards the path).
k11-capture: title-pin ## Capture the pinned original's service menu (scenario=walk|idle|menuesc|diags|de; writes data/k11-captures/)
	$(PYTHON) tools/k11_capture.py --scenario $(scenario) --out $(K11_CAPTURES)/$(scenario) $(K11_ARGS)

# K11 oracle (enforced in verify like the front-end oracle: no
# PR_ORACLE_REQUIRED, so it skips without the capture and fails on any
# mismatch with it). The port script comes from the capture's poll log; the
# PR_K11_DUMP driver runs alone (game_init once per process).
k11-oracle: build ## K11 service-menu oracle, the walk (skips without data/k11-captures/walk)
	@echo "== K11 service-menu oracle (pixel-exact, the walk) =="
	@if [ -d $(K11_CAPTURES)/walk ]; then \
		rm -rf $(K11_DUMP)/walk; mkdir -p $(K11_DUMP); \
		$(PYTHON) tools/k11_session.py port-script --scenario walk --capture $(K11_CAPTURES)/walk --out $(K11_DUMP)/walk.script && \
		PR_K11_DUMP=$(K11_DUMP)/walk PR_K11_SCRIPT=$(K11_DUMP)/walk.script PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "k11-oracle: no capture at $(K11_CAPTURES)/walk, frames not compared"; \
	fi
	@$(PYTHON) tools/k11_compare.py --capture $(K11_CAPTURES)/walk --port $(K11_DUMP)/walk

# The evidence scenarios (G1/G2/G3): the same driver and comparison, report-only.
k11-report: build ## Report-only K11 comparison of the evidence captures (idle, menuesc, diags, de)
	@for s in idle menuesc diags de; do \
		if [ -d $(K11_CAPTURES)/$$s ]; then \
			rm -rf $(K11_DUMP)/$$s; mkdir -p $(K11_DUMP); \
			$(PYTHON) tools/k11_session.py port-script --scenario $$s --capture $(K11_CAPTURES)/$$s --out $(K11_DUMP)/$$s.script && \
			PR_K11_DUMP=$(K11_DUMP)/$$s PR_K11_SCRIPT=$(K11_DUMP)/$$s.script PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
			$(PYTHON) tools/k11_compare.py --report --scenario $$s --capture $(K11_CAPTURES)/$$s --port $(K11_DUMP)/$$s; \
		else \
			echo "k11-report: no capture at $(K11_CAPTURES)/$$s"; \
		fi; \
	done
```

In `verify`, after the `attract-oracle` line and before the `title_compare unit tests` echo:

```make
	@echo "== K11 service-menu oracle (the walk; skips without data/k11-captures/walk) =="
	@$(MAKE) --no-print-directory k11-oracle
	@echo "== k11 tool unit tests =="
	$(PYTHON) -m unittest tools.tests.test_k11_fields tools.tests.test_k11_session tools.tests.test_k11_capture tools.tests.test_k11_compare
```

Add `+ K11` to `verify`'s `##` help text (`… attract cycle-2 ratchet oracles, K11 oracle, symbols.h idempotence`).

In `AGENTS.md`'s Commands block, after `make attract2-oracle …`:

```bash
make k11-oracle            # K11 service-menu oracle (the walk); in make verify; skips without data/k11-captures/walk
make k11-capture scenario=walk   # DOSBox-X capture of the service menu (writes data/k11-captures/<scenario>)
```

- [ ] **Step 2: Check the targets without a capture**

```bash
make help | grep -E 'k11-'
make k11-oracle; echo "exit=$?"
make k11-report; echo "exit=$?"
```

Expected: three `k11-…` help lines. Then `k11-oracle: no capture at data/k11-captures/walk, frames not compared`, `k11_compare: no capture at data/k11-captures/walk (skipped)`, `exit=0`. Then four `k11-report: no capture at …` lines and `exit=0`.

- [ ] **Step 3: Gate**

```bash
make verify > "$S/a_t8_verify.txt" 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare)' "$S/a_t8_verify.txt" | diff - "$S/a_or_base.txt" && echo ORACLES-EQUAL
grep -E '^(k11|Ran 26 tests)' "$S/a_t8_verify.txt"
```

Expected: `verify-exit=0`, `ORACLES-EQUAL`, the two skip lines, and `Ran 26 tests` (7 + 6 + 6 + 7).

- [ ] **Step 4: Commit**

```bash
git add Makefile AGENTS.md
git commit -m "$(cat <<'EOF'
build: k11-capture, k11-oracle and k11-report; k11-oracle in make verify

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 9: Capture the walk (investigation)

**Files:**
- Create (git-ignored): `data/k11-captures/walk/*`
- Modify: `docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md` (§A.4)
- Create (scratch): `$S/raw2png.py`

**Interfaces:**
- Consumes: `make k11-capture`, the pinned copy
- Produces: `data/k11-captures/walk/`, and §A.4 with the facts the walk oracle depends on: the Enter frame, per-key `ent`/`kb` evidence, the pad-level evidence and the absence of a timeout

- [ ] **Step 1: Capture**

```bash
make title-pin
make k11-capture scenario=walk 2>&1 | tee "$S/a_t9_capture.txt" | tail -8
```

Expected tail: `k11_capture: CHECK base: ok`, `k11_capture: CHECK keys 38/38: ok`, `k11_capture: CHECK mode 0x27 after the Enter: ok`, `k11_capture: CHECK pokes: ok`, and `k11_capture: wrote N frames to …/data/k11-captures/walk (raw a..b), wall ~75s`. The exit status is 0.

**If a CHECK fails**, take only the matching branch:
- `keys n/38` with n < 38: rerun with `K11_ARGS="--time-limit 100"`. If n is still short, the BIOS buffer is missing keys. Go to Task 14 F2.
- `mode 0x27 after the Enter` FAIL: find the Enter's context with `awk '$1=="K"{print NR": "$0; exit}' data/k11-captures/walk/poll.log`, and the P lines around that `ms`. If `f` is not advancing (a logo movie drains the buffer), rerun with `K11_ARGS="--enter-wait 28"` (inside the attract, which ends near 31 s per window.txt raw 2198 at 70.09 fps). Record the value used.
- `base` FAIL: the memory file is not being read. Go to Task 14 F5.

- [ ] **Step 2: Evidence extraction (record each output verbatim in §A.4)**

```bash
L=data/k11-captures/walk/poll.log
head -3 data/k11-captures/walk/window.txt; cat data/k11-captures/walk/session.txt
grep -m1 -n '^B ' $L
awk '$1=="K"{print NR": "$0}' $L | head -40
awk '$1=="P" && /mode=0027/{print NR": "$0; exit}' $L
awk '$1=="P"{for(i=2;i<=NF;i++){if($i~/^ent=/)e=$i; if($i~/^kb=/)k=$i} if(e!=pe||k!=pk){print NR": "$0; pe=e; pk=k}}' $L > "$S/a_t9_ent_kb.txt"; wc -l < "$S/a_t9_ent_kb.txt"
awk '$1=="P"' $L | awk 'f && !/mode=0027/{print "LEFT 0x27: " $0; exit} /mode=0027/{f=1}'
tail -2 $L
ls -l data/k11-captures/walk/*.dro
python3 tools/k11_session.py port-script --scenario walk --capture data/k11-captures/walk --out $S/walk.script && cat $S/walk.script
```

Expected and recorded:
- `B … base=` (the data base; earlier records measured `00266000`).
- 38 `K` lines whose `key=` sequence normalises to the WALK list (`1C0D 1C0D 50E0|5000 …`).
- The Enter's P line: its `f` and `st` (the attract state).
- The `ent`/`kb` change list.
- No `LEFT 0x27` line.
- The final `E … reason=time-limit`.
- The DRO file(s) and their sizes. They are kept as the OPL register-write artefact and are not compared (§A.4 says why: the walk plays no sample or tune; SOUND/MUSIC TEST are entered and left without Enter).
- The port script.

- [ ] **Step 3: Check the input path against §A.1.5**

From `$S/a_t9_ent_kb.txt`, confirm that each `down` tap produced a `kb` run (a non-zero `kb=`, then back to `0000`) and that each master-loop `down` in START MENU advanced `ent` by `0x10` (`000BCCDC → 000BCCEC → 000BCCFC`). Record the `kb` value of a Down tap, its duration in ticks and the count of `pad` lines in `$S/walk.script`.

**If a Down produced no `kb` run or no `ent` step,** the taps are too short for the ISR sampler. Go to Task 14 F2.

- [ ] **Step 4: Look at the screens**

Write `$S/raw2png.py`:

```python
import sys
from PIL import Image
for p in sys.argv[1:]:
    Image.frombytes('RGB', (320, 200), open(p, 'rb').read()).save(p[:-4] + '.png')
```

Run `python3 $S/raw2png.py data/k11-captures/walk/frame_*.raw` and view the PNGs listed in `$S/a_t9_ent_kb.txt`'s key moments with the Read tool: MAIN MENU, START MENU, OPTIONS MENU, each page and the final MAIN MENU. Record in §A.4 one line per screen: `walk frame <i> (raw <r>): <screen>`. Then delete the PNGs: `rm data/k11-captures/walk/*.png`.

- [ ] **Step 5: Commit the record**

```bash
git add docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md
git commit -m "$(cat <<'EOF'
docs: the K11 walk capture and its poll-log facts (record §A.4)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 10: The walk oracle green on the port

**Files:**
- Modify: `docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md` (§A.5)
- Modify only as Step 3 dictates, with that branch's own test: `tools/k11_session.py` + `tools/tests/test_k11_session.py`, or `tools/k11_compare.py` (`K11_ALLOWED_UNEXPLAINED`), or `port/src/game/svcmenu.c` + `port/tests/test_game.c`

**Interfaces:**
- Consumes: `data/k11-captures/walk`, `make k11-oracle`
- Produces: a green `make k11-oracle`; §A.5 with the oracle lines and every triage decision

- [ ] **Step 1: First run**

```bash
make k11-oracle 2>&1 | tee "$S/a_t10_run1.txt" | grep -E '^(k11|FAIL|all checks)'
```

Expected when green: `all checks passed` (the driver), `k11_compare: walk: window distinct […]`, `… 0 unexplained …`, `k11_compare: walk: settled screens exhibited Y/Y; missing []`, `k11_compare: walk: 0 unexplained in the window`, exit 0. If so, go to Step 5.

- [ ] **Step 2: If the driver fails**

For a `CHECK_EQ_INT` on `state_after`/`frame_after`, the port's frame counter from boot does not match the original's at the Enter. Record both values. Then use the attract dump to find the port frame whose presented image equals the capture's frame just before the Enter's MAIN MENU:

```bash
PR_ATTRACT_DUMP=$S/att PR_GAME_DIR=data/game/C ./build/run_tests
```

Compare its `frame_*.raw` md5s with the capture frame before `window distinct` start. Fix the mapping in `port_script` (`enter_frame` from the matched frame), with a new unit test, and record the offset with both frame references. Never pick an `enter_frame` without that match.

- [ ] **Step 3: Triage each unexplained frame or missing screen (first one first)**

Run `python3 tools/k11_compare.py --report --capture data/k11-captures/walk --port /tmp/pr_k11_dump/walk`, convert the capture frame and the nearest port frame to PNG (`$S/raw2png.py`) and view both. Compare `/tmp/pr_k11_dump/walk/k11.log`'s `ent=` column with the capture's `ent` sequence (`$S/a_t9_ent_kb.txt`). Then take the matching class:

- **C1: the run clock (all-gaps ledger §E row 30).** The box lies inside STATISTICS page 1's value cells, and the capture's last `F` record before the page decodes to a non-zero field 3, `0xA`, `0xC`, `0x12` or `0x13`. Check this with `python3 -c "import sys; sys.path.insert(0,'tools'); import k11_fields as kf, k11_session as ks; d=kf.load_descriptors('data/game/C/PRAGE.EXE'); img=bytearray(<F img>); print({hex(f): kf.get(img,d,f) for f in (3,0xA,0xC,0x12,0x13)})"`. The fix goes in `k11_session.port_script`: emit `field <id> <value>` at the Enter for each of those five fields, decoded from the last `F` record before the K record of the Enter that opens STATISTICS (the page draws once, 0x33058's redraw flag). Add a unit test with a synthetic F record, and record the values with their poll line. This is a seed from the capture of state the port leaves host-owned, as `FRONTEND_RNG_AFTER_ATTRACT` is.
- **C2: an input divergence.** The `ent` sequences differ. The fix goes in the script generator only (the `pad` timing or `kb` value from the poll), with a unit test. Never change `port/src` for this.
- **C3: a port rendering defect.** The same `ent` and field values give different pixels. Re-derive the drawing routine from the raw (`$S/dx.py`), write a failing `CHECK` in `test_game.c`'s svcmenu area, fix `svcmenu.c` citing the address, mutation-prove it and record it (raw wins). Rerun `make verify`: the other oracle lines must stay equal.
- **C4: a capture artefact that title_compare's model cannot express** (a three-source torn frame). Name it in `K11_ALLOWED_UNEXPLAINED['walk']` with `'record §A.5: <shape>'` and its evidence, and add a unit test that the name admits only that index.

Repeat Steps 1 and 3 until green. Every iteration appends its class, evidence and fix to §A.5.

- [ ] **Step 4: If the walk cannot be made green inside A's scope**

For example, C3 needs a port change outside the service menu. Record the first unexplained frame and its cause as a named gap in §A.5. Shorten `WALK` to the prefix the port explains, and recapture (Task 9 Step 1). This keeps the oracle's claim true, because the remaining pages are listed in §A.10 as "reached by the capture, not yet by the oracle".

- [ ] **Step 5: Prove the gate can fail (then restore)**

```bash
cp /tmp/pr_k11_dump/walk/screens.txt $S/screens.bak
N=$(awk '$1=="key"{print $4; exit}' /tmp/pr_k11_dump/walk/screens.txt)
python3 - "$N" <<'EOF'
import sys
p = '/tmp/pr_k11_dump/walk/frame_%04d.raw' % int(sys.argv[1])
b = bytearray(open(p, 'rb').read()); b[1000] ^= 0xFF; open(p, 'wb').write(b)
EOF
python3 tools/k11_compare.py --capture data/k11-captures/walk --port /tmp/pr_k11_dump/walk; echo "exit=$?"
make k11-oracle >/dev/null 2>&1; echo "restored-exit=$?"
```

Expected: `settled screens exhibited …; missing [N]` and `exit=1`. Then `restored-exit=0`, because the rerun regenerates the dump.

- [ ] **Step 6: Gate**

```bash
make verify > "$S/a_t10_verify.txt" 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare)' "$S/a_t10_verify.txt" | diff - "$S/a_or_base.txt" && echo ORACLES-EQUAL
grep '^k11_compare' "$S/a_t10_verify.txt"
```

Expected: `verify-exit=0`, `ORACLES-EQUAL`, and the four `k11_compare: walk:` lines. Record them verbatim in §A.5 as the K11 oracle's baseline lines.

- [ ] **Step 7: Commit**

Stage the record, plus exactly the files the Step 3 branches changed:

```bash
git add docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md
git commit -m "$(cat <<'EOF'
docs: the K11 walk oracle green on the port (record §A.5)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

If a branch changed code, commit it separately first, with its own message, for example `tools: seed the run-clock fields from the K11 capture (record §A.5)` or `game: <fix> (record §A.5)`.

---

### Task 11: G1 evidence: the idle timeout and the MAIN MENU Esc

**Files:**
- Create (git-ignored): `data/k11-captures/idle/*`, `data/k11-captures/menuesc/*`
- Modify: `docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md` (§A.6)

**Interfaces:**
- Consumes: §A.1.3 (the setjmp/longjmp map), `make k11-capture`, `make k11-report`
- Produces: §A.6, the answer to "what does the original do at the idle-timeout longjmp" (and at `0x2520B`), with poll lines and capture frames

- [ ] **Step 1: Capture both**

```bash
make k11-capture scenario=idle 2>&1 | tail -6
make k11-capture scenario=menuesc 2>&1 | tail -6
```

Expected: each ends with its `CHECK`s `ok` (keys `1/1` and `2/2`).

- [ ] **Step 2: The idle timeout from the poll log**

```bash
L=data/k11-captures/idle/poll.log
awk '$1=="P" && /mode=0027/{print NR": "$0; exit}' $L
awk '$1=="P" && /menu=01/{last=NR": "$0} $1=="P" && /menu=00/ && last{print "LAST menu=01: " last; print "FIRST menu=00: " NR": " $0; exit}' $L
awk '$1=="P"' $L | awk 'f && !/mode=0027/{print "LEFT 0x27: " $0; c++} /mode=0027/{f=1} c==3{exit}'
python3 - <<'EOF'
import sys; sys.path.insert(0, 'tools'); import k11_session as ks
recs = [ks.parse(l) for l in open('data/k11-captures/idle/poll.log') if l.strip()]
p = [r for r in recs if r['kind'] == 'P']
i = next(k for k, r in enumerate(p) if r['mode'] == 0x27)
j = next((k for k in range(i, len(p)) if p[k]['mode'] != 0x27), None)
last = p[j - 1] if j else p[-1]
print('at the last 0x27 poll: tick %08X ktime %08X diff %#x latch %08X' %
      (last['tick'], last['ktime'], (last['tick'] - last['ktime']) & 0xFFFFFFFF, last['latch']))
EOF
```

Record verbatim:
- the Enter line;
- the last `menu=01`/first `menu=00` pair, which is the `0x2EBA8` store;
- the first three P lines after mode `0x27` is left, which show the state `0x20C24..` restores;
- the `diff` value, which must be `> 0x4B0` (the `0x2EB9F` `jbe`), and the latch at 0 (`0x2EB89`).

If no `LEFT 0x27` line appears, rerun with `K11_ARGS="--time-limit 150"`. If it still does not appear, go to Task 14 F4.

- [ ] **Step 3: What the screen shows after the longjmp**

```bash
make k11-report 2>&1 | grep -E '^k11_compare: (idle|menuesc)'
python3 - <<'EOF'
import hashlib, os
def idx(d):
    out = {}
    for n in sorted(os.listdir(d)):
        if n.endswith('.raw'):
            out.setdefault(hashlib.md5(open(os.path.join(d, n), 'rb').read()).hexdigest(), n)
    return out
ref = {}
for c in ('data/title-captures/title', 'data/title-captures/frontend'):
    for h, n in idx(c).items():
        ref.setdefault(h, '%s/%s' % (c, n))
for s in ('idle', 'menuesc'):
    d = 'data/k11-captures/' + s
    hits = [(n, ref[h]) for h, n in sorted(idx(d).items(), key=lambda x: x[1]) if h in ref]
    print(s, len(hits), hits[:3], hits[-3:])
EOF
```

The report gives the first capture frame after the window (the first unexplained frame) with its raw index. The md5 search tells whether the post-longjmp frames are frames the boot/title/front-end captures already hold, which identifies where the original resumes. View that frame and the next distinct ones as PNG. Record: `idle frame <i> (raw <r>): <what it shows>; equal to <title-captures/…/frame_NNNN.raw> | no match`.

- [ ] **Step 4: The MAIN MENU Esc (`0x2520B`)**

Repeat Steps 2–3 for `menuesc`. The trigger is the Esc `K` line, and the expectation is mode `0x27` left within one frame of it, with no `menu` store (the `0x251F3..0x2520B` path).

- [ ] **Step 5: Write the answer in §A.6**

Use this exact form, one paragraph per site:

> **G1, idle timeout (`0x2EBB3`).** At `poll.log:<n>` (tick `T`), `tick − DS_00105F2C = 0x…` > `0x4B0` with the latch 0. The original clears `DS_00107414` (`menu=00`, `poll.log:<n>`) and longjmps to the setjmp at `0x20C1F`. The next polled state is `mode=… st=… f=…` (`poll.log:<m>`). The screen shows <…> from `idle frame <i> (raw <r>)` on, and is <equal to / not in> the earlier captures.

Also record the timing. From the Enter's last stamp to the timeout, the tick delta is `0x…`, which the wall time corroborates. The port keeps the store and not the longjmp (`config.c`), so `make k11-report`'s idle window ends at the timeout. That is B's G1 target.

- [ ] **Step 6: Commit**

```bash
git add docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md
git commit -m "$(cat <<'EOF'
docs: G1 evidence: the idle-timeout and MAIN MENU Esc longjmps (record §A.6)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 12: G2 evidence: the `0xFFE80003` read (counterfactual poke)

**Files:**
- Create (git-ignored): `data/k11-captures/diags/*`
- Modify: `docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md` (§A.7)

**Interfaces:**
- Consumes: §A.1.1 (the arm is dead in the stock game), the `diags` scenario (poke `DS_00107410 |= 0x10` at the first mode-`0x27` poll)
- Produces: §A.7, which says what DOS/4GW + DOSBox-X return or raise for the byte at linear `0xFFE80003`, and proves or disproves that the memory-file poke is visible to the guest (the trigger for F1)

- [ ] **Step 1: Capture**

```bash
make k11-capture scenario=diags 2>&1 | tail -6
L=data/k11-captures/diags/poll.log
grep -n '^W ' $L
awk '$1=="P" && /diag=/{for(i=2;i<=NF;i++) if($i~/^diag=/) d=$i; if(d!=pd){print NR": "$0; pd=d}}' $L
tail -2 $L; grep -n 'wall_s' data/k11-captures/diags/session.txt
grep -n -i -E 'dos/4g|exception|page fault|error|abort' data/k11-captures/diags/dosbox.log | head
```

Expected: one `W … ds=00107410 … new=` line with bit `0x10` set, and `diag=` reading `…10`/`…1C` from then on.

- [ ] **Step 2: Decide which case the capture shows**

```bash
make k11-report 2>&1 | grep '^k11_compare: diags'
```

Take the matching case:
- **(a) The run continued.** `E reason=time-limit`, and no DOS/4GW line. The TEST CONTROLS frames exist. The port's frame (DIAGS drawn, value `00000000`) and the capture's frame differ only in a box inside row 7 of the text grid (the report's `differs in rows … x …`). View both PNGs and read the 8 hex digits the original draws at column `0xE`, row 7. Record them with the frame reference and the box. If the capture's TEST CONTROLS frame has **no** "ADDRESS RAW DATA"/"DIAGS" rows at all (the report box then spans rows 3–7), the poke was not visible to the guest. Go to Task 14 F1.
- **(b) The run aborted.** `E reason=exit` well before 55 s, and `dosbox.log` holds the DOS/4GW message. Record the message lines verbatim with their line numbers, the last frame before the text mode (the report's window end) and `last_frame.png`, viewed and described.

- [ ] **Step 3: Write the answer in §A.7**

> **G2 (`0x32573..0x32578`).** Unreachable in the stock game (§A.1.1: field `0x2A` is 4 bits, descriptor `0x1B80`; `DS_00107410 = field & ~3`, `0x2FA1C`). With `DS_00107410 |= 0x10` poked (`poll.log:<n>`), the original <draws `XXXXXXXX` for the byte at `0xFFE80003` (`diags frame <i> (raw <r>)`, box …) | aborts with "<DOS/4GW text>" (`dosbox.log:<n>`)>.

Add the recommendation this evidence supports for B: resolved-as-unreachable with the proof, or model the observed behaviour. B decides.

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md
git commit -m "$(cat <<'EOF'
docs: G2 evidence: the 0xFFE80003 DIAGS read under a poke (record §A.7)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 13: G3 evidence: the `0x33458` `idiv` `#DE`

**Files:**
- Create (git-ignored): `data/k11-captures/de/*`
- Modify: `docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md` (§A.8)

**Interfaces:**
- Consumes: §A.1.2, the `de` scenario (fields 8 = 6 = `0x8000` poked through `k11_fields.set_`), `k11_session.py check-fields`
- Produces: §A.8, which says what the original does when row `0x96`'s divisor has a zero low word, plus the C-versus-Python codec cross-check

- [ ] **Step 1: Capture**

```bash
make k11-capture scenario=de 2>&1 | tail -6
L=data/k11-captures/de/poll.log
grep -n '^W ' $L
tail -2 $L; grep -n 'wall_s' data/k11-captures/de/session.txt
grep -n -i -E 'dos/4g|exception|divide|error|abort' data/k11-captures/de/dosbox.log | head
```

Expected: `W` lines only inside DS `0x105DAF..0x105E2F`, the bytes of fields 8 and 6.

- [ ] **Step 2: Verify the poke decodes as intended**

```bash
python3 - <<'EOF'
import sys; sys.path.insert(0, 'tools'); import k11_fields as kf, k11_session as ks
d = kf.load_descriptors('data/game/C/PRAGE.EXE')
recs = [ks.parse(l) for l in open('data/k11-captures/de/poll.log') if l.strip()]
w = max(r['ms'] for r in recs if r['kind'] == 'W')
img = [r for r in recs if r['kind'] == 'F' and r['ms'] >= w][0]['img']
print({hex(f): hex(kf.get(img, d, f)) for f in (6, 8, 0xA, 0x12)})
EOF
```

Expected: `'0x6': '0x8000', '0x8': '0x8000'`. Record the `0xA` and `0x12` values too; they are the dividends.

- [ ] **Step 3: The C-versus-Python cross-check**

```bash
make k11-report 2>&1 | grep '^k11_compare: de'
python3 tools/k11_session.py check-fields --scenario de --port /tmp/pr_k11_dump/de; echo "exit=$?"
```

Expected: `k11_session: check-fields: de: tools/k11_fields.set_ matches config_field_set over 129 bytes` and `exit=0`. If it fails, the capture poked the wrong bytes: fix `k11_fields.py` (with a new failing unit test first), then redo Steps 1–3.

- [ ] **Step 4: Decide which case the capture shows**

Take the matching case:
- **(a) Abort.** `E reason=exit` early, and DOS/4GW text in `dosbox.log`. Record the text verbatim (`dosbox.log:<n>`), the wall time, the last game frame (the report's window end) and `last_frame.png` described.
- **(b) The page drew.** A handler swallowed the fault. Record the STATISTICS page 1 frame (`de frame <i> (raw <r>)`), the report box against the port's frame (which draws 0 for row `0x96`, `svcmenu.c:1192`) and the digits the original shows in row `0x96`'s cells.

Record against §A.1.4 whether a handler was predicted.

- [ ] **Step 5: Write the answer in §A.8**

> **G3 (`0x334CD..0x334E2`).** With fields 8 = 6 = `0x8000` (`poll.log:<n>`, decoded above; the codec matches `config_field_set`), entering STATISTICS page 1 <aborts to DOS with "<text>" (`dosbox.log:<n>`), the last game frame `de frame <i> (raw <r>)` | draws `<digits>` in row `0x96` (`de frame <i>`, box …)>. Reachability in play: <from §A.1.2>.

- [ ] **Step 6: Commit**

```bash
git add docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md
git commit -m "$(cat <<'EOF'
docs: G3 evidence: the 0x33458 idiv #DE under a field poke (record §A.8)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 14: Fallbacks (run only the branch a trigger in Tasks 2, 9, 11, 12 or 13 sent you to)

**Files:**
- F1 only: create `tools/k11_pin.py`, `tools/tests/test_k11_pin.py`
- F2 only: modify `tools/k11_session.py` (`WALK`), `tools/tests/test_k11_session.py` (`test_autotype_walk_is_exact`)
- All: modify `docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md` (§A.9)

**Interfaces:**
- Consumes: the failing trigger's evidence
- Produces: either a working alternative stimulus with its evidence, or a smaller named gap with its evidence in §A.9 and §A.10

- [ ] **F1: Memory-file writes are not visible to the guest (trigger: Task 12 Step 2(a) with no DIAGS rows).** Use pinned-EXE variants on top of the title pin, as `title_pin.py` does: fail-closed, same-length patches, never under `data/`. The patch sites below were verified by the planner from the raw.

`diags`: file `0x8286E` (VA `0x2FA1A`) `24 FC` (`and al,0xfc`) becomes `0C 10` (`or al,0x10`), so `DS_00107410 = field 0x2A | 0x10`. Bits 0–1 are kept, and their only reader, `0x32361`, masks `0x10` (§A.1.1).

`de`: four sites.
- File `0x7F962` (VA `0x2CB0E`): `BA A0 00 00 00` becomes `BA 00 80 00 00`.
- File `0x7F967` (VA `0x2CB13`): `B8 35 00 00 00` becomes `B8 08 00 00 00`.
- File `0x7F971` (VA `0x2CB1D`): `BA A0 00 00 00` becomes `BA 00 80 00 00`.
- File `0x7F976` (VA `0x2CB22`): `B8 37 00 00 00` becomes `B8 06 00 00 00`.

`config_set_defaults` (`0x2CADC`) then stores fields 8 = 6 = `0x8000` in place of fields `0x35`/`0x37` = `0xA0`. The defaults arm runs because the CMOS is zero (demo-pose record, "the storage path finds no table"). Fields `0x35`/`0x37` stay 0, so ADJUST VOLUME takes its error arm, which the `de` scenario never visits.

`tools/k11_pin.py`:

```python
#!/usr/bin/env python3
"""Pinned-EXE variants for the K11 evidence scenarios (plan Task 14 F1, record
§A.9): same-length patches on top of the title pin, fail-closed like
title_pin.py. Usage: k11_pin.py --variant diags|de --src /tmp/pr_title_pin/PRAGE.EXE
--out /tmp/pr_k11_pin/<variant>/PRAGE.EXE"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import title_pin as tp

VARIANTS = {
    'diags': [(0x8286E, bytes.fromhex('24fc'), bytes.fromhex('0c10'))],          # 0x2FA1A
    'de': [(0x7F962, bytes.fromhex('baa0000000'), bytes.fromhex('ba00800000')),  # 0x2CB0E
           (0x7F967, bytes.fromhex('b835000000'), bytes.fromhex('b808000000')),  # 0x2CB13
           (0x7F971, bytes.fromhex('baa0000000'), bytes.fromhex('ba00800000')),  # 0x2CB1D
           (0x7F976, bytes.fromhex('b837000000'), bytes.fromhex('b806000000'))], # 0x2CB22
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--variant', required=True, choices=sorted(VARIANTS))
    ap.add_argument('--src', required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    saved = tp.PATCHES
    try:
        tp.PATCHES = VARIANTS[a.variant]
        tp.patch(a.src, a.out)
    finally:
        tp.PATCHES = saved
    print('k11_pin: wrote %s (variant %s)' % (a.out, a.variant))
    return 0


if __name__ == '__main__':
    sys.exit(main())
```

`tools/tests/test_k11_pin.py`: three tests.
1. A synthetic image holding each variant's original bytes at its offsets is patched and the replacement bytes read back.
2. A one-byte mismatch at any site raises `SystemExit` and writes nothing.
3. `--out` under the repo's `data/` raises `SystemExit`.

Write them first and see them fail (`ModuleNotFoundError`). Implement, then see `Ran 3 tests … OK`. Add `tools.tests.test_k11_pin` to the verify unit-test line; the expected count becomes `Ran 29 tests`.

Run:

```bash
python3 tools/k11_pin.py --variant <v> --src /tmp/pr_title_pin/PRAGE.EXE --out /tmp/pr_k11_pin/<v>/PRAGE.EXE
make k11-capture scenario=<v> K11_ARGS="--exe /tmp/pr_k11_pin/<v>/PRAGE.EXE --no-pokes"
```

Redo Task 12 or 13 from Step 2. The port script keeps the scenario's `ds_or`/`field` at the Enter, so the port side is unchanged. Record in §A.9 that the stimulus is a behaviour pin of the defaults, with the sites. Commit `tools: k11_pin, the pinned-EXE variants for the K11 evidence (record §A.9)`.

- [ ] **F2: The scripted input cannot reach a screen (trigger: Task 9 keys missing, no `kb` run, or no `ent` step).** Record the evidence in §A.9: the K count, the `kb` runs and the `ent` list. Shorten `WALK` to the longest prefix whose every key the poll shows acting (the `ent` step or the page change). Update `test_autotype_walk_is_exact` to the new exact string and see it fail, then pass. Recapture (Task 9) and redo Task 10. List every dropped screen in §A.10 as **not reached: AUTOTYPE taps are not seen by the ISR key sampler 0x1BBAC (poll.log:<n> shows …)**. That is a smaller named gap with its evidence. Commit `tools: shorten the K11 walk to the reachable screens (record §A.9)`.

- [ ] **F3: The abort text is not in `dosbox.log` (trigger: Task 12(b) or 13(a) without the DOS/4GW lines).** Rerun the scenario with `-log-con` added through `K11_ARGS` only if §A.2 did not already use it. Otherwise record `last_frame.png` (viewed; the text transcribed) and the `E` line's early end as the evidence. Record which source was used.

- [ ] **F4: The idle timeout does not fire within 150 s (trigger: Task 11 Step 2).** Record the maximum `tick − ktime` the poll saw and the latch values. A non-zero latch keeps `0x2EB80` from checking (`0x2EB89`). Name the gap in §A.10: **idle longjmp not observed: latch `DS_00105F30` stayed `…` (poll.log:<n>)**.

- [ ] **F5: No memory file or no base (trigger: Task 2 Step 3 or Task 9 `base` FAIL).** The frames still come from the AVI. Record the failure. The scenarios then rely on the frames only: `port_script` cannot run, so `make k11-oracle` would fail with the capture present. Move the walk capture aside to `data/k11-captures/walk.noframes-sync` (the guard allows it). §A.10 then states **G5 not closed: no live-RAM channel on this host (evidence …)**, and the plan stops at Task 15 with that result.

- [ ] **Commit the record for any branch taken**

```bash
git add docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md
git commit -m "$(cat <<'EOF'
docs: named-gaps A fallbacks taken and their evidence (record §A.9)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 15: Closure

**Files:**
- Modify: `docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md` (§A.10)
- Modify: `docs/PROGRESS.md` (append one paragraph)

**Interfaces:**
- Consumes: §A.0–§A.9
- Produces: §A.10, which holds the G1/G2/G3 answers table, every path reached or not reached with its reason, the oracle's claim and the final gate. Sub-project B's inputs.

- [ ] **Step 1: The reachability table**

Write §A.10 with one row per path the spec names:

| path | reached by | evidence | oracle |
|---|---|---|---|
| START MENU | walk | walk frame … | k11-oracle |
| CONFIG OPTIONS | walk | … | k11-oracle |
| CONFIGURE KEYBOARD | walk | … | k11-oracle |
| STATISTICS p1/p2/histograms | walk | … | k11-oracle |
| TEST CONTROLS (no DIAGS) | walk | … | k11-oracle |
| TEST CONTROLS DIAGS `0xFFE80003` | diags (poke / pin) | §A.7 | report only (B) |
| idle timeout `0x2EBB3` | idle | §A.6 | report only (B) |
| MAIN MENU Esc `0x2520B` | menuesc | §A.6 | report only (B) |
| `0x33458` `#DE` | de (poke / pin) | §A.8 | report only (B) |
| quit prompt longjmp `0x24AB0` | not captured | outside mode 0x27 (§A.1.3) | — |
| STATISTICS page 2 clear (Esc + Enter held) | not captured | AUTOTYPE types taps, not chords (§A.2) | — |
| SOUND/MUSIC TEST playback | not captured | no Enter is typed there; the DRO is kept, not compared (§A.4) | — |

Also record any rows F2 dropped. Fill every `…` with the recorded reference; no cell may stay a placeholder.

- [ ] **Step 2: The answers**

Copy the three answer paragraphs from §A.6–§A.8. Add the oracle's claim and its limit:

> Every non-black capture frame between the first MAIN MENU frame and the final settled MAIN MENU is explained by the port, and every settled screen the port draws is present in the capture. The claim does not cover the timing between keys, the SOUND/MUSIC TEST audio, or anything after the walk.

- [ ] **Step 3: Final gate and counters**

```bash
make verify > "$S/a_t15_verify.txt" 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare)' "$S/a_t15_verify.txt" | diff - "$S/a_or_base.txt" && echo ORACLES-EQUAL
grep '^k11_compare' "$S/a_t15_verify.txt"
python3 tools/port_progress.py
git status --porcelain
```

Expected:
- `verify-exit=0` and `ORACLES-EQUAL`.
- The `k11_compare: walk:` lines equal §A.5's.
- `767 1203 64` / `731 731 100 …` unchanged, because A ports no function. `README.md`'s title stays at 64%.
- `git status --porcelain` shows only the files about to be staged. `data/` never appears, since it is git-ignored.

- [ ] **Step 4: PROGRESS.md**

Append one paragraph:

> Named gaps A (K11 ground truth). The walk capture (`data/k11-captures/walk`) and `make k11-oracle` in `make verify` close G5 for the screens listed in record §A.10. G1, G2 and G3 evidence is in record §A.6, §A.7 and §A.8, and the capture-derived answers go to sub-project B. The G2 arm is unreachable in the stock game (field `0x2A` is 4 bits). Oracle lines are unchanged.

State the actual outcomes found, not these placeholders.

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md docs/PROGRESS.md
git commit -m "$(cat <<'EOF'
docs: named-gaps A closure: K11 oracle, G1/G2/G3 evidence (record §A.10)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

Do not merge or push (spec §5).

---

## Self-Review

**Spec coverage (spec §4 A and §6):**
- "A scripted DOSBox-X session … enters the service menu, visits START MENU, CONFIGURE, STATISTICS, TEST CONTROLS RAW DATA and the idle timeout": Tasks 4 (`walk`, `idle`, `diags`), 5 and 9. CONFIGURE covers both CONFIG OPTIONS and CONFIGURE KEYBOARD in `walk`. The RAW DATA row appears in `walk` (no DIAGS) and in `diags`.
- "records frames and register writes": the frames come from the AVI. The register writes are the live-RAM poll log (`P`/`K`/`F`/`W`) plus the OPL DRO, which is kept and not compared (§A.4).
- "A byte-exact K11 oracle, gated on its capture … added to `make verify` (skips silently without the capture)": Tasks 7 and 8.
- "what does the original do at `0xFFE80003`, on the `idiv` `#DE`, and at the idle-timeout `longjmp`": Tasks 12, 13 and 11, each with a fixed answer form.
- "Acceptance: the oracle passes on the port for the reached paths; every path the capture cannot reach is listed with the reason; existing oracle lines do not move": Task 10 Step 6, Task 15 Step 1's table, and the `ORACLES-EQUAL` diffs in Tasks 6, 8, 10 and 15.
- §6 "A may not reach every path … fall back to a raw-only model and record the path as a named gap": Task 14 F1–F5.

**Placeholder scan:** Every command in the plan is concrete. Values that depend on running the game are investigation steps with the exact command and the exact record format (Tasks 9 and 11–13). The only angle-bracket fields are the record templates the executor fills from named command output. Task 15 Step 1 forbids leaving a `…` cell.

**Type and format consistency:**
- The port-script grammar in `k11_session.py`'s docstring matches `k11_parse` in the C driver: `enter_frame %u`, `enter_state %x`, `key %u %x %x`, `pad %u %x %u`, `ds_or %x %x`, `field %x %x`, `end %u`.
- `k11_session.port_script` emits `ds_or/field` as `'%s %X %X'`, `pad` as `'pad %d %04X %d'` and `key` as `'key %d %02X %02X'`.
- `screens.txt` (`key <i> settled <n>`, `end settled <n>`) matches `k11_compare.read_screens`.
- `WIN_LO`/`WIN_HI` (`0x105DAF`/`0x105E30`) match the driver's `DS_00105DAF`/`K11_FIMG_END` and `check_fields`' 129 bytes.
- `POLL_FIELDS` is shared by `format_p` (the writer) and `parse` (the reader).
- The unit-test counts are fields 7, session 6, capture 6 and compare 7, so `Ran 26 tests` in Task 8. With F1 it becomes `Ran 29 tests`.

**Known conflicts carried to the report:**
1. G2's arm is unreachable in the stock game (§A.1.1). This contradicts K11 record §K11.5 and spec §6.
2. `data/` is "read-only" (AGENTS.md), yet this plan adds a capture dir under it. The title-capture precedent covers this, and a guard enforces it.
3. The spec's "`le.py`" is not in the repo. Raw reads use `$S/dx.py` on the raw file with the stated pre-fixup conversion.
4. G1 has three longjmp sites, not one. B must model or name all three.
