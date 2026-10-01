# E1: differential-emulation harness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and prove the harness that runs a function's original x86 bytes and the port's C function from the same image and compares every changed byte, the return register and the block coverage, on four already-ported functions plus five deliberately wrong ones.

**Architecture:** Two sides and a driver. `tools/diff_emu.py` runs the original bytes in a `unicorn` flat 32-bit emulator over the image `mem_load_le` produces; `port/tests/diff_runner.c` (the `diffrun` binary) loads the same image into `mem[]` and calls the port's C function through a binding; `tools/diff_verify.py` writes the cases, runs both, compares, and measures basic-block coverage of the original with a `capstone` recursive-descent scan. A `make diff-verify` target runs the unit tests and the self-check; `make verify` calls it and it skips cleanly without `unicorn`.

**Tech Stack:** Python 3.12, `unicorn` 2.1.4, `capstone` 5.0.7 (already installed), C11 (CMake target `diffrun`), `unittest`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` (§4 track E1, §5 harness, §6 exit criteria). It was approved in conversation on 2026-09-30. Where this plan and the spec differ, the spec wins; the spec's §5.4 open questions are settled by Task 5 and recorded.

## Global Constraints

- Raw wins: never ship a fitted constant; every value is derived from the raw bytes (the disassembly addresses are in this plan) or a run; a value that cannot be pinned is a named gap with its evidence.
- `make verify` is THE gate. The 45 oracle lines (`grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>`) must equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`, and the `make audio-render` WAV must be byte-identical to `before-t2.wav` (`cmp`). E1 changes nothing under `port/src`.
- `python3 tools/port_progress.py` must still print `771 1203 64` and `731 731 100`: `diff_runner.c` lives in `port/tests`, which the counter does not read.
- `data/` is git-ignored and read-only: the harness writes only to `/tmp` or a `tempfile` directory.
- C tests use only `CHECK`/`CHECK_EQ_INT`; `diff_runner.c` is a tool, not a test, and its file name does not match the `tests/test_*.c` glob, so `run_tests` does not link it. Python tests are `unittest` in `tools/tests/`, imported the way the existing ones are (`sys.path.insert(0, ROOT/tools)`).
- Assertions must be able to fail: seeded sentinels (no unseeded zeros), and a mutation proof for every new assertion (the steps give the exact mutation and the test that must fail).
- The harness skips like the other oracles when `unicorn` or `PRAGE.EXE` is absent (spec §5.5): exit 0 with a "skipped:" line, and exit 1 when `PR_ORACLE_REQUIRED=1`.
- Commit style `<area>: <what changed>`, trailer `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Stage named files only, never `git add -A`.
- The claim every verdict carries: equivalence on the exercised blocks and inputs only.

## Review Focus

The spec implies these failure modes; each is pinned by the named test.

1. **A function that returns through AL leaves EAX bits 8+ as scratch** (`fighter_slot_flag` 0x3C570: `mov al,1` into an EAX of `0x200`). The binding states the mask the callers read (`0xFF`). Pinned by `test_the_return_mask_decides_what_counts` and case `f9`.
2. **A port that returns the right value but forgets a memory write.** Only the byte diff catches it. Pinned by `fighter_slot_flag@mutant` and `test_a_forgotten_write_is_caught_by_the_byte_diff_alone`.
3. **Unsigned against signed compares** (`config_credit_spend` 0x2CA7C uses `ja`). Pinned by case `c5` (n = `0x80000001`, 1 credit) and the `@signed` mutant, which only `c5` catches.
4. **A function that never returns, on either side.** The emulator stops at `max_insns` (`test_a_loop_that_never_returns_times_out`); a hung `diffrun` is killed by a timeout (`test_a_runner_that_never_returns_is_an_error_not_a_hang`).
5. **A function that reads state the image does not hold.** Reads outside the image return zero on both sides and are listed (`test_reads_outside_the_image_are_reported_and_read_zero`, `test_reads_outside_the_image_are_listed`); a shift count of 33 (x86 masks it to 5 bits, C leaves it undefined) is case `f33`, which agrees on this host and is recorded as compiler-dependent.
6. **Coverage below 100%** must read PARTIAL, never VERIFIED (`test_an_unexercised_block_is_partial_not_verified`).

## Setup (once, before Task 1)

Work in `.worktrees/reverse-e1` (branch `reverse-e1`, created from `main` at `4fc2bde` with the spec cherry-picked). It already exists; the shared pieces are linked or copied in:

```bash
cd .worktrees/reverse-e1
ls -d data .superpowers port/tests/ghidra_data.bin port/tests/title_screen_ref.ppm   # all four must exist
cmake -S port -B build && cmake --build build 2>&1 | tail -3                           # a first build, so later builds are incremental
```

The ledger for this plan is `.superpowers/sdd/2026-09-30-reverse-completion-e1/progress.md` (git-ignored).

## File Structure

| File | Responsibility |
|---|---|
| `tools/requirements-diff.txt` (new) | pins `unicorn` and `capstone` |
| `tools/diff_emu.py` (new) | the original side: `Image`, `run_original`, `static_scan`, `coverage` |
| `tools/tests/test_diff_emu.py` (new) | emulator tests on hand-assembled images, no `PRAGE.EXE` |
| `port/tests/diff_runner.c` (new) | the port side: the `diffrun` binary, its bindings and the self-check mutants |
| `port/CMakeLists.txt` | one `add_executable(diffrun ...)` |
| `tools/diff_verify.py` (new) | the cases, the comparison, the verdicts, the CLI |
| `tools/tests/test_diff_verify.py` (new) | comparison tests with fabricated port results, plus the real-function and self-check tests |
| `Makefile` | `diff-verify` target, wired into `verify` |
| `docs/superpowers/plans/2026-09-30-reverse-completion-e1-derivations.md` (new) | the record: addressing evidence, results, limits |
| `docs/PROGRESS.md`, `AGENTS.md`, `README.md`, the spec | the paragraph, the commands, the dependency line, §5.4 settled |

## The calling-convention facts the cases rest on

Disassembled from the loaded image (capstone over `mem_load_le`'s dump of `CODE_BASE..`), flat 32-bit, operands are plain linear addresses:

| function | original | arguments | returns | what it touches |
|---|---|---|---|---|
| `rng_next` | `0x5D7DC` | EAX = range (`and eax,0xffff`) | EAX | `[0xEF6D8]` read and written |
| `fighter_slot_flag` | `0x3C570` | EAX = bit (`mov cl,al`, `shl eax,cl`) | AL (`mov al,1` / `xor al,al`) | `[0x107EE0]` test, then set |
| `config_credit_spend` | `0x2CA7C` | EAX = n | EAX (`mov eax,1`, or the `xor eax,eax; ret` at `0x2CA78`) | `[0x105D60]`, `[0x105C00]`, `[0x104B1F]`; debits `[0x105C00]` |
| `config_codeword_len` | `0x2D4B4` | EAX = n | EAX | none (a signed-compare loop) |

---

### Task 1: The emulator core and its dependency

**Files:**
- Create: `tools/requirements-diff.txt`
- Create: `tools/diff_emu.py`
- Create: `tools/tests/test_diff_emu.py`

**Interfaces:**
- Produces: `diff_emu.Image(data, base=0x10000)`, `Image.load(path)`, `Image.contains/bytes_at/end`; `diff_emu.run_original(image, entry, regs=None, pokes=None, allow_calls=(), max_insns=200000) -> OrigResult(outcome, detail, regs, writes, executed, outside)` where `outcome` is `ok | fault | unmodeled | timeout`; `diff_emu.available()`; the constants `REGS`, `MEM_SIZE`, `IMAGE_BASE`. Task 2 appends `static_scan`/`coverage`; Task 4 consumes all of it.

- [ ] **Step 1: Install and pin the dependency**

```bash
printf 'unicorn==2.1.4\ncapstone==5.0.7\n' > tools/requirements-diff.txt
python3 -m pip install -r tools/requirements-diff.txt 2>&1 | tail -2
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"
```

Expected: `2.1.4 5.0.7`. (This installs into the active pyenv Python, the one `capstone` is already in.) This answers spec §5.4's install question (record it in Task 5). If `unicorn` does not install on this host, stop: the spec's fallback is a small 386 interpreter, which is a plan change, not a task detail.

- [ ] **Step 2: Write the failing tests**

Create `tools/tests/test_diff_emu.py` with this content:

```python
"""Unit tests for tools/diff_emu.py on small hand-assembled images (no PRAGE.EXE needed).

Every program is listed with its assembly. Memory is seeded with values that differ from what the
program writes (a write of the value already there, or an unseeded zero, would prove nothing).
"""
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import diff_emu as E

REQUIRED = os.environ.get("PR_ORACLE_REQUIRED") == "1"


def le32(v):
    return (v & 0xFFFFFFFF).to_bytes(4, "little")


def image(code, at=0x10000):
    """An image from 0x10000 to 0x90100 (so 0x80000 is inside it) with `code` placed at `at`."""
    data = bytearray(0x80100)
    data[at - 0x10000:at - 0x10000 + len(code)] = code
    return E.Image(bytes(data))


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed (pip install -r tools/requirements-diff.txt)")
class RunOriginalTests(unittest.TestCase):
    def test_unicorn_is_installed_when_required(self):
        self.assertTrue(E.available())

    # 10000: mov eax,[0x80000]; add eax,5; mov [0x80004],eax; ret
    ADD5 = bytes.fromhex("A1000008" "00" "0505000000" "A304000800" "C3")

    def test_returns_eax_and_reports_every_changed_byte(self):
        img = image(self.ADD5)
        pokes = {0x80000: le32(0x10), 0x80004: le32(0xFFFFFFFF)}   # seeded: all 4 bytes will change
        r = E.run_original(img, 0x10000, pokes=pokes)
        self.assertEqual(r.outcome, "ok")
        self.assertEqual(r.regs["eax"], 0x15)
        self.assertEqual(r.writes, {0x80004: 0x15, 0x80005: 0, 0x80006: 0, 0x80007: 0})

    def test_a_write_of_the_value_already_there_is_not_a_change(self):
        img = image(self.ADD5)
        pokes = {0x80000: le32(0x10), 0x80004: le32(0x15)}
        self.assertEqual(E.run_original(img, 0x10000, pokes=pokes).writes, {})

    def test_registers_are_inputs(self):
        # 10000: lea eax,[eax+edx*2]; ret
        img = image(bytes.fromhex("8D0450" "C3"))
        r = E.run_original(img, 0x10000, regs={"eax": 100, "edx": 7})
        self.assertEqual(r.regs["eax"], 114)

    def test_stack_traffic_is_not_a_write(self):
        # 10000: push eax; pop eax; ret
        r = E.run_original(image(bytes.fromhex("50" "58" "C3")), 0x10000, regs={"eax": 9})
        self.assertEqual((r.outcome, r.writes), ("ok", {}))

    def test_an_invalid_instruction_is_a_fault(self):
        r = E.run_original(image(bytes.fromhex("0F0B")), 0x10000)          # ud2
        self.assertEqual(r.outcome, "fault")

    def test_int_is_unmodeled_and_named(self):
        r = E.run_original(image(bytes.fromhex("CD21" "C3")), 0x10000)      # int 0x21
        self.assertEqual(r.outcome, "unmodeled")
        self.assertIn("int at 0x10000", r.detail)

    def test_port_io_is_unmodeled(self):
        r = E.run_original(image(bytes.fromhex("EC" "C3")), 0x10000)        # in al,dx
        self.assertEqual((r.outcome, r.detail), ("unmodeled", "in at 0x10000"))

    def test_a_loop_that_never_returns_times_out(self):
        r = E.run_original(image(bytes.fromhex("EBFE")), 0x10000, max_insns=1000)   # jmp $
        self.assertEqual(r.outcome, "timeout")

    # 10000: call 0x10010; ret      10010: mov eax,7; ret
    def call_image(self):
        code = bytearray(0x20)
        code[0:6] = bytes.fromhex("E80B000000" "C3")
        code[0x10:0x16] = bytes.fromhex("B807000000" "C3")
        return image(bytes(code))

    def test_a_call_outside_the_allow_list_is_unmodeled(self):
        r = E.run_original(self.call_image(), 0x10000)
        self.assertEqual(r.outcome, "unmodeled")
        self.assertIn("call 0x10010 from 0x10000", r.detail)

    def test_an_allowed_call_runs_the_callee_from_the_original_bytes(self):
        r = E.run_original(self.call_image(), 0x10000, allow_calls={0x10010})
        self.assertEqual((r.outcome, r.regs["eax"]), ("ok", 7))

    def test_an_indirect_call_is_unmodeled(self):
        r = E.run_original(image(bytes.fromhex("FFD0" "C3")), 0x10000, regs={"eax": 0x10010})   # call eax
        self.assertEqual((r.outcome, r.detail), ("unmodeled", "indirect call at 0x10000"))

    def test_reads_outside_the_image_are_reported_and_read_zero(self):
        # 10000: mov eax,[0x1000000]; ret
        r = E.run_original(image(bytes.fromhex("A100000001" "C3")), 0x10000, regs={"eax": 0xFFFFFFFF})
        self.assertEqual((r.outcome, r.regs["eax"]), ("ok", 0))
        self.assertEqual(r.outside, [(0x1000000, 4)])

    def test_a_poke_outside_the_image_is_rejected(self):
        with self.assertRaises(ValueError):
            E.run_original(image(bytes.fromhex("C3")), 0x10000, pokes={0x2000000: b"\x01"})

    def test_runs_are_independent(self):
        img = image(self.ADD5)
        pokes = {0x80000: le32(1), 0x80004: le32(0xFFFFFFFF)}
        a = E.run_original(img, 0x10000, pokes=pokes)
        b = E.run_original(img, 0x10000, pokes=pokes)
        self.assertEqual((a.regs, a.writes), (b.regs, b.writes))
```

- [ ] **Step 3: Run them to see them fail**

Run: `python3 -m unittest tools.tests.test_diff_emu 2>&1 | tail -5`
Expected: `ModuleNotFoundError: No module named 'diff_emu'`.

- [ ] **Step 4: Write the emulator**

Create `tools/diff_emu.py` with this content (Task 2 appends the static scan to it):

```python
#!/usr/bin/env python3
"""Differential-emulation harness, original side (spec 2026-09-30-reverse-completion-design §5).

Runs the ORIGINAL x86 bytes of one function from a flat image snapshot and reports what it did
(final registers, every byte it changed, which instructions it executed). The port side
(port/tests/diff_runner.c) runs the C function from the same snapshot; tools/diff_verify.py
compares the two. Narrow claim, repeated in every record: equivalence on the exercised blocks
and inputs only.

Flat model, confirmed in E1 (record §E.1): PRAGE.EXE is flat 32-bit, operands are plain linear
addresses (rng_next 0x5D7DC reads `mov eax,[0xef6d8]`), so the emulator maps a 64 MB flat space
with the image at its own addresses and no segment base.
"""
import capstone
from capstone import x86 as cx86
from dataclasses import dataclass, field

try:  # the harness skips cleanly (spec §5.5) when unicorn is not installed
    import unicorn
    from unicorn import (Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE,
                         UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE, UC_PROT_ALL)
    from unicorn import x86_const as ux
except ImportError:  # pragma: no cover - exercised only on a host without unicorn
    unicorn = None

MEM_SIZE = 0x4000000          # = port/src/mem.h MEM_SIZE
IMAGE_BASE = 0x10000          # = CODE_BASE; the dump starts here
STACK_TOP = 0x3FF0000         # private emulator stack; the port has none, so it is never diffed
STACK_LOW = 0x3F00000
SENTINEL = 0x3FFF000          # the return address pushed for the function; emulation ends here
DEFAULT_MAX_INSNS = 200_000
REGS = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")

# Instructions the harness cannot model faithfully: the function becomes NOT_EXERCISABLE with the
# instruction named (spec §5.2), never a guessed result.
UNMODELED_MNEMONICS = frozenset({
    "int", "int3", "into", "in", "out", "ins", "insb", "insw", "insd", "outs", "outsb", "outsw",
    "outsd", "hlt", "cli", "sti", "syscall", "sysenter", "iret", "iretd",
})


def available():
    return unicorn is not None


class Image:
    """The flat snapshot: raw bytes of the linear range [base, base+len)."""

    def __init__(self, data, base=IMAGE_BASE):
        self.data = bytes(data)
        self.base = base

    @classmethod
    def load(cls, path):
        with open(path, "rb") as f:
            return cls(f.read())

    @property
    def end(self):
        return self.base + len(self.data)

    def contains(self, addr, n=1):
        return self.base <= addr and addr + n <= self.end

    def bytes_at(self, addr, n):
        off = addr - self.base
        return self.data[off:off + n]


@dataclass
class OrigResult:
    outcome: str                 # ok | fault | unmodeled | timeout
    detail: str = ""
    regs: dict = field(default_factory=dict)       # final value of each REGS entry
    writes: dict = field(default_factory=dict)     # addr -> final byte, only bytes that changed, stack excluded
    executed: set = field(default_factory=set)     # addresses of executed instructions
    outside: list = field(default_factory=list)    # (addr, size) read or written outside image and stack


_md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
_md.detail = True


def _decode(data, addr):
    for ins in _md.disasm(bytes(data), addr, 1):
        return ins
    return None


def _direct_target(ins):
    """The immediate target of a direct jmp/jcc/call, else None."""
    if ins.operands and ins.operands[0].type == cx86.X86_OP_IMM:
        return ins.operands[0].imm & 0xFFFFFFFF
    return None


def run_original(image, entry, regs=None, pokes=None, allow_calls=(), max_insns=DEFAULT_MAX_INSNS):
    """Run the original function at `entry` once from `image` with `regs` and `pokes` applied.

    regs: {"eax": v, ...}; pokes: {addr: bytes}, applied inside the image before the run (the
    port side applies them to the same bytes, so both sides start from identical memory).
    allow_calls: direct call targets the emulator may enter; any other call stops the run as
    `unmodeled` (spec §5.2: the closure the port also runs).
    """
    if unicorn is None:
        raise RuntimeError("unicorn is not installed (pip install -r tools/requirements-diff.txt)")
    mu = Uc(UC_ARCH_X86, UC_MODE_32)
    mu.mem_map(0, MEM_SIZE, UC_PROT_ALL)
    mu.mem_write(image.base, image.data)
    for addr, data in (pokes or {}).items():
        if not image.contains(addr, len(data)):
            raise ValueError("poke 0x%X+%d is outside the image" % (addr, len(data)))
        mu.mem_write(addr, bytes(data))

    names = {"eax": ux.UC_X86_REG_EAX, "ebx": ux.UC_X86_REG_EBX, "ecx": ux.UC_X86_REG_ECX,
             "edx": ux.UC_X86_REG_EDX, "esi": ux.UC_X86_REG_ESI, "edi": ux.UC_X86_REG_EDI,
             "ebp": ux.UC_X86_REG_EBP}
    for r in REGS:
        mu.reg_write(names[r], 0)
    for r, v in (regs or {}).items():
        mu.reg_write(names[r], v & 0xFFFFFFFF)
    esp = STACK_TOP - 4
    mu.mem_write(esp, SENTINEL.to_bytes(4, "little"))
    mu.reg_write(ux.UC_X86_REG_ESP, esp)

    state = {"stop": None}
    executed = set()
    before = {}            # addr -> byte before the run's first write to it
    outside = set()
    seen = {}              # addr -> mnemonic class, so each address is decoded once
    allow = frozenset(allow_calls)

    def in_scope(addr):
        return image.contains(addr) or STACK_LOW <= addr < MEM_SIZE

    def on_code(uc, addr, size, _):
        executed.add(addr)
        if addr in seen:
            kind = seen[addr]
        else:
            ins = _decode(uc.mem_read(addr, 15), addr)
            kind = None
            if ins is None:
                kind = ("unmodeled", "undecodable bytes at 0x%X" % addr)
            elif ins.mnemonic in UNMODELED_MNEMONICS:
                kind = ("unmodeled", "%s at 0x%X" % (ins.mnemonic, addr))
            elif ins.mnemonic == "call":
                tgt = _direct_target(ins)
                if tgt is None:
                    kind = ("unmodeled", "indirect call at 0x%X" % addr)
                elif tgt not in allow:
                    kind = ("unmodeled", "call 0x%X from 0x%X" % (tgt, addr))
            seen[addr] = kind
        if kind is not None and state["stop"] is None:
            state["stop"] = kind
            uc.emu_stop()

    def on_write(uc, access, addr, size, value, _):
        for a in range(addr, addr + size):
            if a not in before:
                before[a] = uc.mem_read(a, 1)[0]
        if not in_scope(addr):
            outside.add((addr, size))

    def on_read(uc, access, addr, size, value, _):
        if not in_scope(addr):
            outside.add((addr, size))

    mu.hook_add(UC_HOOK_CODE, on_code)
    mu.hook_add(UC_HOOK_MEM_WRITE, on_write)
    mu.hook_add(UC_HOOK_MEM_READ, on_read)

    outcome, detail = "ok", ""
    try:
        mu.emu_start(entry, SENTINEL, count=max_insns)
        if state["stop"] is not None:
            outcome, detail = state["stop"]
        elif mu.reg_read(ux.UC_X86_REG_EIP) != SENTINEL:
            outcome, detail = "timeout", "no return within %d instructions" % max_insns
    except UcError as e:
        outcome, detail = "fault", str(e)

    final_regs = {r: mu.reg_read(names[r]) for r in REGS}
    writes = {}
    for a, old in before.items():
        if STACK_LOW <= a < MEM_SIZE:
            continue
        new = mu.mem_read(a, 1)[0]
        if new != old:
            writes[a] = new
    return OrigResult(outcome, detail, final_regs, writes, executed, sorted(outside))
```

- [ ] **Step 5: Run the tests**

Run: `python3 -m unittest tools.tests.test_diff_emu 2>&1 | tail -4`
Expected: `Ran 15 tests ... OK`.

- [ ] **Step 6: Commit, then prove the assertions can fail**

```bash
git add tools/requirements-diff.txt tools/diff_emu.py tools/tests/test_diff_emu.py
git commit -m "$(cat <<'EOF'
tools: diff_emu runs a function's original bytes in a flat unicorn image (E1)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

For each mutation, apply it, run `python3 -m unittest tools.tests.test_diff_emu 2>&1 | grep -E '^(FAIL|ERROR):|^Ran|^OK'`, check the named test fails, then restore with `git checkout -- tools/diff_emu.py`:

| mutation (`perl -0pi -e '<expr>' tools/diff_emu.py`) | must fail |
|---|---|
| `s/if new != old:/if True:/` | `test_a_write_of_the_value_already_there_is_not_a_change` |
| `s/"int", "int3"/"int3"/` | `test_int_is_unmodeled_and_named` |
| `s/if STACK_LOW <= a < MEM_SIZE:\n            continue/pass/` | `test_stack_traffic_is_not_a_write` |
| `s/elif tgt not in allow:/elif False:/` | `test_a_call_outside_the_allow_list_is_unmodeled` |

Record the four results in the ledger. Do not commit a mutation.

---

### Task 2: The static scan and block coverage

**Files:**
- Modify: `tools/diff_emu.py` (append)
- Modify: `tools/tests/test_diff_emu.py` (append)

**Interfaces:**
- Consumes: Task 1's `Image`, `_decode`, `_direct_target`.
- Produces: `static_scan(image, entry, max_insns=4000) -> StaticInfo(leaders, insns, indirect, unresolved, truncated)`; `coverage(info, executed) -> (hit_leaders, unhit_leaders)`. A block counts as hit when its leader instruction executed. Recursive descent follows tail `jmp`s into the next function, so a function's blocks include the code it falls into.

- [ ] **Step 1: Append the failing tests**

Append to `tools/tests/test_diff_emu.py`:

```python


# 10000: cmp eax,0; je 10008; inc eax; jmp 10009; (10008) dec eax; (10009) ret
DIAMOND = bytes.fromhex("83F800" "7403" "40" "EB01" "48" "C3")


@unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")
class StaticScanTests(unittest.TestCase):
    def test_blocks_of_a_diamond(self):
        info = E.static_scan(image(DIAMOND), 0x10000)
        self.assertEqual(info.leaders, [0x10000, 0x10005, 0x10008, 0x10009])
        self.assertEqual((info.indirect, info.truncated), ([], False))

    def test_coverage_reports_the_unhit_block(self):
        img = image(DIAMOND)
        info = E.static_scan(img, 0x10000)
        taken = E.run_original(img, 0x10000, regs={"eax": 0})       # je taken: skips 0x10005
        hit, unhit = E.coverage(info, taken.executed)
        self.assertEqual((hit, unhit), ([0x10000, 0x10008, 0x10009], [0x10005]))
        other = E.run_original(img, 0x10000, regs={"eax": 1})
        _, unhit2 = E.coverage(info, taken.executed | other.executed)
        self.assertEqual(unhit2, [])

    def test_an_indirect_jump_is_flagged(self):
        info = E.static_scan(image(bytes.fromhex("FFE0")), 0x10000)    # jmp eax
        self.assertEqual(info.indirect, [0x10000])

    def test_a_direct_target_outside_the_image_is_unresolved(self):
        # jmp 0x7000000
        rel = (0x7000000 - 0x10005) & 0xFFFFFFFF
        info = E.static_scan(image(b"\xE9" + le32(rel)), 0x10000)
        self.assertEqual(info.unresolved, [0x7000000])

    def test_a_tail_jump_is_followed_into_the_next_function(self):
        # 10000: jmp 0x10010      10010: xor eax,eax; ret
        code = bytearray(0x20)
        code[0:5] = b"\xE9" + le32(0x10010 - 0x10005)
        code[0x10:0x13] = bytes.fromhex("31C0" "C3")
        info = E.static_scan(image(bytes(code)), 0x10000)
        self.assertEqual(info.leaders, [0x10000, 0x10010])


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run them to see them fail**

Run: `python3 -m unittest tools.tests.test_diff_emu 2>&1 | tail -4`
Expected: errors `module 'diff_emu' has no attribute 'static_scan'`.

- [ ] **Step 3: Append the scan**

Append to `tools/diff_emu.py`:

```python


# ---- static scan: the function's basic blocks, for the coverage claim (spec §5.3) -------------

@dataclass
class StaticInfo:
    leaders: list                # sorted block-leader addresses
    insns: dict                  # addr -> size, every instruction reached by recursive descent
    indirect: list               # addrs of indirect jmp/call: their targets are unknown (a jump table)
    unresolved: list             # direct targets outside the image
    truncated: bool              # the descent hit max_insns


def static_scan(image, entry, max_insns=4000):
    insns, leaders, indirect, unresolved = {}, {entry}, [], []
    work, truncated = [entry], False
    while work:
        addr = work.pop()
        while True:
            if addr in insns:
                break
            if not image.contains(addr):
                unresolved.append(addr)
                break
            if len(insns) >= max_insns:
                truncated = True
                work.clear()
                break
            ins = _decode(image.bytes_at(addr, 15), addr)
            if ins is None:
                break
            insns[addr] = ins.size
            nxt = addr + ins.size
            m = ins.mnemonic
            if ins.group(capstone.CS_GRP_RET):
                break
            if ins.group(capstone.CS_GRP_JUMP):
                tgt = _direct_target(ins)
                if tgt is None:
                    indirect.append(addr)
                    break
                leaders.add(tgt)
                work.append(tgt)
                if m == "jmp":
                    break
                leaders.add(nxt)
                addr = nxt
                continue
            if ins.group(capstone.CS_GRP_CALL) and _direct_target(ins) is None:
                indirect.append(addr)
            addr = nxt
    return StaticInfo(sorted(a for a in leaders if a in insns), insns, sorted(set(indirect)),
                      sorted(set(unresolved)), truncated)


def coverage(info, executed):
    """(hit, unhit) block leaders: a block is hit when its leader instruction executed."""
    hit = [a for a in info.leaders if a in executed]
    unhit = [a for a in info.leaders if a not in executed]
    return hit, unhit
```

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_diff_emu 2>&1 | tail -4`
Expected: `Ran 20 tests ... OK`.

- [ ] **Step 5: Commit, then prove the assertions can fail**

```bash
git add tools/diff_emu.py tools/tests/test_diff_emu.py
git commit -m "$(cat <<'EOF'
tools: diff_emu static scan and block coverage for the verified-means-covered claim (E1)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

Mutation (restore with `git checkout -- tools/diff_emu.py`): `perl -0pi -e 's/if m == "jmp":\n                    break/pass/' tools/diff_emu.py` must fail `test_a_direct_target_outside_the_image_is_unresolved` and `test_a_tail_jump_is_followed_into_the_next_function`. Record it in the ledger.

---

### Task 3: The port side, `diffrun`

**Files:**
- Create: `port/tests/diff_runner.c`
- Modify: `port/CMakeLists.txt` (one target)

**Interfaces:**
- Produces: the `diffrun` binary. `diffrun --exe PRAGE.EXE --image-out FILE [--cases FILE]`. Without `--cases` it dumps the image (the loaded `mem[]` from `CODE_BASE` on, exactly what `mem_load_le` writes) and exits. With `--cases` it also runs each case. Case file lines: `case ID`, `fn NAME`, `reg eax|ebx|ecx|edx|esi|edi|ebp HEX`, `poke ADDR HEXBYTES` (at most 16, 64 bytes each, inside the image), `end`. Output per case: `case ID`, `ret eax 0xV mask 0xM` (V already masked), one `w 0xADDR 0xBB` per changed byte (ascending), or `error ...`, then `end`. Task 4 consumes this format.
- Bindings: `rng_next`, `fighter_slot_flag` (mask `0xFF`), `config_credit_spend`, `config_codeword_len`, and the mutants `rng_next@mutant`, `fighter_slot_flag@mutant`, `config_credit_spend@mutant`, `config_credit_spend@signed`, `config_codeword_len@mutant`.

- [ ] **Step 1: Write the runner**

Create `port/tests/diff_runner.c` with this content:

```c
/* port/tests/diff_runner.c — port side of the differential harness (spec
 * 2026-09-30-reverse-completion-design §5). A tool binary (`diffrun`), not a unit test: it loads
 * PRAGE.EXE's LE image into mem[], then for each case in --cases applies the registers and pokes,
 * calls the named C function through its binding, and prints the output register and every byte
 * of mem[] that changed. tools/diff_verify.py compares that with the original's own bytes run by
 * tools/diff_emu.py from the same image. */
#include "mem.h"
#include "symbols.h"
#include "game/config.h"
#include "game/fighter.h"
#include "game/rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { R_EAX, R_EBX, R_ECX, R_EDX, R_ESI, R_EDI, R_EBP, R_N };
static const char *const k_reg[R_N] = { "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp" };

/* A binding adapts the original's register calling convention (Watcom: EAX, EDX, EBX, ECX) to
 * the port function's C signature. eax_mask is the part of EAX the original's callers read: a
 * function that returns through AL leaves the upper bits of EAX as scratch, which the port's C
 * return value does not reproduce, so comparing them would flag a difference no caller can see. */
typedef struct {
    const char *name;
    void (*call)(const u32 in[R_N], u32 *eax);
    u32 eax_mask;
} binding_t;

static void b_rng_next(const u32 *r, u32 *eax)         { *eax = rng_next(r[R_EAX]); }
static void b_slot_flag(const u32 *r, u32 *eax)        { *eax = (u32)fighter_slot_flag(r[R_EAX]); }
static void b_credit_spend(const u32 *r, u32 *eax)     { *eax = config_credit_spend(r[R_EAX]); }
static void b_codeword_len(const u32 *r, u32 *eax)     { *eax = config_codeword_len(r[R_EAX]); }

/* Self-check mutants. Each is a plausible porting bug, kept only so tools/diff_verify.py
 * --self-check can prove the harness reports a difference (an assertion that cannot fail proves
 * nothing, AGENTS.md). They are bound under "<name>@mutant" (or another "@" suffix) and live in
 * this tool, never in prage_core. */
static void m_rng_next(const u32 *r, u32 *eax)         /* increment off by one */
{
    DSD(DS_000EF6D8) = DSD(DS_000EF6D8) * 0xB90D12B9u + 0x38CE0520u;
    *eax = (DSD(DS_000EF6D8) >> 16) * (r[R_EAX] & 0xFFFFu) >> 16;
}
static void m_slot_flag(const u32 *r, u32 *eax)        /* forgets to set the bit */
{
    u32 m = 1u << (r[R_EAX] & 0xffu);
    *eax = (DSD(DS_00107EE0) & m) != 0 ? 1u : 0u;
}
static void m_credit_spend(const u32 *r, u32 *eax)     /* `>=` for the unsigned guard */
{
    if (DSB(DS_00105D60) != 0u) { *eax = 1u; return; }
    if (r[R_EAX] >= DSD(DS_00105C00)) { *eax = 0u; return; }
    if (DSB(DS_00104B1F) == 0u) DSD(DS_00105C00) -= r[R_EAX];
    *eax = 1u;
}
static void m_credit_spend_signed(const u32 *r, u32 *eax)  /* signed compare for the unsigned `ja` */
{
    if (DSB(DS_00105D60) != 0u) { *eax = 1u; return; }
    if ((s32)r[R_EAX] > (s32)DSD(DS_00105C00)) { *eax = 0u; return; }
    if (DSB(DS_00104B1F) == 0u) DSD(DS_00105C00) -= r[R_EAX];
    *eax = 1u;
}
static void m_codeword_len(const u32 *r, u32 *eax)     /* `>` for the signed `jge` */
{
    u32 e = r[R_EAX], d = 1u;
    for (;;) {
        e++;
        if ((s32)d > (s32)e) break;
        d += d;
    }
    *eax = e;
}

static const binding_t k_bindings[] = {
    { "rng_next",                 b_rng_next,     0xFFFFFFFFu },
    { "fighter_slot_flag",        b_slot_flag,    0x000000FFu },
    { "config_credit_spend",      b_credit_spend, 0xFFFFFFFFu },
    { "config_codeword_len",      b_codeword_len, 0xFFFFFFFFu },
    { "rng_next@mutant",          m_rng_next,     0xFFFFFFFFu },
    { "fighter_slot_flag@mutant", m_slot_flag,    0x000000FFu },
    { "config_credit_spend@mutant", m_credit_spend, 0xFFFFFFFFu },
    { "config_codeword_len@mutant", m_codeword_len, 0xFFFFFFFFu },
    { "config_credit_spend@signed", m_credit_spend_signed, 0xFFFFFFFFu },
};

static const binding_t *find_binding(const char *name)
{
    for (size_t i = 0; i < sizeof k_bindings / sizeof k_bindings[0]; i++)
        if (strcmp(k_bindings[i].name, name) == 0) return &k_bindings[i];
    return NULL;
}

/* The image the dump covers: CODE_BASE..CODE_BASE+g_len. Everything above it is zero in mem[]
 * after the load, and a case may change it, so it is scanned for non-zero bytes and cleared. */
static u8 *g_pristine, *g_pre;
static u32 g_len;

static int load_image(const char *exe, const char *img_path)
{
    if (!mem_load_le(exe, img_path)) return 0;
    FILE *f = fopen(img_path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || (u32)n > MEM_SIZE - CODE_BASE || ((u32)n & 7u) != 0) { fclose(f); return 0; }
    g_len = (u32)n;
    g_pristine = malloc(g_len);
    g_pre = malloc(g_len);
    if (!g_pristine || !g_pre || fread(g_pristine, 1, g_len, f) != g_len) { fclose(f); return 0; }
    fclose(f);
    /* the dump is mem[] as the loader left it: a mismatch means the file is not this image */
    return memcmp(mem + CODE_BASE, g_pristine, g_len) == 0;
}

typedef struct { u32 addr; u32 len; u8 b[64]; } poke_t;
typedef struct {
    char id[64], fn[64];
    u32 reg[R_N];
    poke_t poke[16];
    int npoke;
} case_t;

static int parse_hex(const char *s, u32 *out)
{
    char *e;
    unsigned long v = strtoul(s, &e, 16);
    if (e == s || *e != '\0') return 0;
    *out = (u32)v;
    return 1;
}

static int parse_bytes(const char *s, poke_t *p)
{
    size_t n = strlen(s);
    if (n == 0 || (n & 1u) || n / 2 > sizeof p->b) return 0;
    for (size_t i = 0; i < n / 2; i++) {
        char t[3] = { s[2 * i], s[2 * i + 1], 0 }, *e;
        unsigned long v = strtoul(t, &e, 16);
        if (*e != '\0') return 0;
        p->b[i] = (u8)v;
    }
    p->len = (u32)(n / 2);
    return 1;
}

/* Prints, then clears, every non-zero byte of mem[lo..hi): the bytes the case wrote outside the
 * image, where the load left zeros. */
static void flush_outside(u32 lo, u32 hi)
{
    for (u32 a = lo; a < hi; a += 8) {
        uint64_t w;
        memcpy(&w, mem + a, 8);
        if (w == 0) continue;
        for (u32 i = 0; i < 8; i++)
            if (mem[a + i] != 0) { printf("w 0x%X 0x%02X\n", a + i, mem[a + i]); mem[a + i] = 0; }
    }
}

static void run_case(const case_t *c)
{
    printf("case %s\n", c->id);
    const binding_t *b = find_binding(c->fn);
    if (!b) { printf("error unknown binding %s\nend\n", c->fn); return; }
    for (int i = 0; i < c->npoke; i++) {
        const poke_t *p = &c->poke[i];
        if (p->addr < CODE_BASE || p->addr + p->len > CODE_BASE + g_len) {
            printf("error poke 0x%X outside the image\nend\n", p->addr);
            return;
        }
        memcpy(mem + p->addr, p->b, p->len);
    }
    memcpy(g_pre, mem + CODE_BASE, g_len);

    u32 eax = 0;
    b->call(c->reg, &eax);
    printf("ret eax 0x%X mask 0x%X\n", eax & b->eax_mask, b->eax_mask);
    for (u32 i = 0; i < g_len; i++)
        if (mem[CODE_BASE + i] != g_pre[i]) printf("w 0x%X 0x%02X\n", CODE_BASE + i, mem[CODE_BASE + i]);
    flush_outside(0, CODE_BASE);
    flush_outside(CODE_BASE + g_len, MEM_SIZE);
    memcpy(mem + CODE_BASE, g_pristine, g_len);
    printf("end\n");
}

static int run_cases(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "diffrun: cannot open %s\n", path); return 0; }
    case_t c;
    int open = 0;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *t[4] = { 0 };
        int n = 0;
        for (char *s = strtok(line, " \t\r\n"); s && n < 4; s = strtok(NULL, " \t\r\n")) t[n++] = s;
        if (n == 0 || t[0][0] == '#') continue;
        if (strcmp(t[0], "case") == 0 && n == 2) {
            memset(&c, 0, sizeof c);
            snprintf(c.id, sizeof c.id, "%s", t[1]);
            open = 1;
        } else if (!open) {
            fprintf(stderr, "diffrun: '%s' outside a case\n", t[0]);
            fclose(f);
            return 0;
        } else if (strcmp(t[0], "fn") == 0 && n == 2) {
            snprintf(c.fn, sizeof c.fn, "%s", t[1]);
        } else if (strcmp(t[0], "reg") == 0 && n == 3) {
            int r = 0;
            while (r < R_N && strcmp(t[1], k_reg[r]) != 0) r++;
            if (r == R_N || !parse_hex(t[2], &c.reg[r])) { fprintf(stderr, "diffrun: bad reg line\n"); fclose(f); return 0; }
        } else if (strcmp(t[0], "poke") == 0 && n == 3 && c.npoke < 16) {
            poke_t *p = &c.poke[c.npoke++];
            if (!parse_hex(t[1], &p->addr) || !parse_bytes(t[2], p)) { fprintf(stderr, "diffrun: bad poke line\n"); fclose(f); return 0; }
        } else if (strcmp(t[0], "end") == 0) {
            run_case(&c);
            open = 0;
        } else {
            fprintf(stderr, "diffrun: cannot parse '%s'\n", t[0]);
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return !open;
}

int main(int argc, char **argv)
{
    const char *exe = NULL, *img = NULL, *cases = NULL;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--exe") == 0) exe = argv[i + 1];
        else if (strcmp(argv[i], "--image-out") == 0) img = argv[i + 1];
        else if (strcmp(argv[i], "--cases") == 0) cases = argv[i + 1];
        else { fprintf(stderr, "diffrun: unknown option %s\n", argv[i]); return 2; }
    }
    if (!exe || !img) { fprintf(stderr, "usage: diffrun --exe PRAGE.EXE --image-out FILE [--cases FILE]\n"); return 2; }
    if (!load_image(exe, img)) { fprintf(stderr, "diffrun: cannot load %s\n", exe); return 1; }
    return cases && !run_cases(cases) ? 1 : 0;
}
```

- [ ] **Step 2: Add the target**

In `port/CMakeLists.txt`, after the `add_test(NAME unit ...)` line, add:

```cmake

# The port side of the differential harness (tools/diff_verify.py, spec
# 2026-09-30-reverse-completion-design §5). A tool, not a test: its file name does not match the
# tests/test_*.c glob above, so run_tests does not link it.
add_executable(diffrun tests/diff_runner.c)
target_link_libraries(diffrun PRIVATE prage_core)
```

- [ ] **Step 3: Build with no warnings**

Run: `cmake -S port -B build && cmake --build build 2>&1 | grep -E 'warning|error|diffrun' ; ls -l build/diffrun`
Expected: `build/diffrun` exists and no `warning:` or `error:` line mentions `diff_runner.c`.

- [ ] **Step 4: Smoke it by hand**

```bash
printf 'case s1\nfn fighter_slot_flag\nreg eax 5\npoke 0x107EE0 00000000\nend\ncase s2\nfn nope\nend\ncase s3\nfn rng_next\npoke 0x2000000 01\nend\n' > /tmp/pr_e1_smoke.cases
build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_e1_smoke.img --cases /tmp/pr_e1_smoke.cases; echo exit=$?
ls -l /tmp/pr_e1_smoke.img
```

Expected, exactly:

```
case s1
ret eax 0x0 mask 0xFF
w 0x107EE0 0x20
end
case s2
error unknown binding nope
end
case s3
error poke 0x2000000 outside the image
end
exit=0
```

and the image file is `1028304` bytes (`0x10B0D0 - 0x10000`). Run the same command a second time: the output must be identical (a case leaves no state behind).

- [ ] **Step 5: The gate that this changed nothing else**

Run: `python3 tools/port_progress.py`
Expected: the two lines `771 1203 64` and `731 731 100` (unchanged).

- [ ] **Step 6: Commit**

```bash
git add port/tests/diff_runner.c port/CMakeLists.txt
git commit -m "$(cat <<'EOF'
tests: diffrun, the port side of the differential harness (E1)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

The mutants in this file are proven in Task 4, where the driver reports them.

---

### Task 4: The driver, the verdicts and `make diff-verify`

**Files:**
- Create: `tools/diff_verify.py`
- Create: `tools/tests/test_diff_verify.py`
- Modify: `Makefile`

**Interfaces:**
- Consumes: Task 1-2's `diff_emu` API, Task 3's `diffrun` protocol.
- Produces: `diff_verify.Case/Spec/PortResult/SpecResult`, `cases_text`, `parse_port_output`, `run_port(diffrun, exe, image_path, text, timeout=60)`, `compare(orig, port) -> [discrepancy]`, `verify_spec(spec, image, port_results, port_name=None) -> SpecResult` (verdict `VERIFIED | PARTIAL | MISMATCH | NOT_EXERCISABLE`), `verify_all(...)`, `SPECS` (the four functions), the CLI (`--self-check`, `--table`, `--function`), and `make diff-verify`. Exit 0 only if every function is VERIFIED and (with `--self-check`) every mutant is MISMATCH.

- [ ] **Step 1: Write the failing tests**

Create `tools/tests/test_diff_verify.py` with this content:

```python
# tools/tests/test_diff_verify.py
"""Unit tests for tools/diff_verify.py.

The comparison logic is tested with fabricated port results on small hand-assembled images, so it
needs no PRAGE.EXE. The last class runs the real build/diffrun on the real functions; it skips when
either is absent (the other oracles skip the same way) unless PR_ORACLE_REQUIRED=1.
"""
import os
import stat
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import diff_emu as E
import diff_verify as V

REQUIRED = os.environ.get("PR_ORACLE_REQUIRED") == "1"
needs_unicorn = unittest.skipUnless(E.available() or REQUIRED, "unicorn not installed")


def le32(v):
    return (v & 0xFFFFFFFF).to_bytes(4, "little")


def image(code):
    data = bytearray(0x80100)
    data[:len(code)] = code
    return E.Image(bytes(data))


def orig(eax, writes=None):
    regs = {r: 0 for r in E.REGS}
    regs["eax"] = eax
    return E.OrigResult("ok", "", regs, writes or {}, set(), [])


class ParseTests(unittest.TestCase):
    def test_cases_text_round_trips_through_the_parser(self):
        spec = V.Spec("f", 0x10000, [V.Case("a", {"eax": 5, "edx": 0x10}, {0x80000: b"\x01\x02"})])
        text = V.cases_text(spec, "f@mutant")
        self.assertEqual(text, "case a\nfn f@mutant\nreg eax 0x5\nreg edx 0x10\npoke 0x80000 0102\nend\n")

    def test_port_output_is_parsed(self):
        out = V.parse_port_output("case a\nret eax 0x7 mask 0xFF\nw 0x80004 0x15\nw 0x80005 0x00\nend\n"
                                  "case b\nerror unknown binding x\nend\n")
        self.assertEqual((out["a"].eax, out["a"].mask), (7, 0xFF))
        self.assertEqual(out["a"].writes, {0x80004: 0x15, 0x80005: 0})
        self.assertEqual(out["b"].error, "unknown binding x")

    def test_output_outside_a_case_is_rejected(self):
        with self.assertRaises(ValueError):
            V.parse_port_output("w 0x1 0x2\n")


class CompareTests(unittest.TestCase):
    def test_agreement_has_no_discrepancy(self):
        self.assertEqual(V.compare(orig(5, {0x80000: 1}), V.PortResult(5, 0xFFFFFFFF, {0x80000: 1})), [])

    def test_the_return_mask_decides_what_counts(self):
        # fighter_slot_flag returns through AL: the original leaves EAX = 0x201 where the port's C
        # return is 1. Bits 8+ are scratch no caller reads, so under mask 0xFF they agree, and
        # under a full mask they do not: the mask is what the binding states, not a loophole.
        self.assertEqual(V.compare(orig(0x201), V.PortResult(1, 0xFF)), [])
        self.assertEqual(len(V.compare(orig(0x201), V.PortResult(1, 0xFFFFFFFF))), 1)

    def test_a_write_the_port_forgot_is_a_discrepancy(self):
        d = V.compare(orig(0, {0x107EE0: 1}), V.PortResult(0, 0xFFFFFFFF, {}))
        self.assertEqual(d, ["byte 0x107EE0: original 0x01, port unchanged"])

    def test_a_write_the_original_did_not_make_is_a_discrepancy(self):
        d = V.compare(orig(0, {}), V.PortResult(0, 0xFFFFFFFF, {0x105C00: 0}))
        self.assertEqual(d, ["byte 0x105C00: original unchanged, port 0x00"])

    def test_a_port_error_is_a_discrepancy(self):
        d = V.compare(orig(0), V.PortResult(0, 0xFFFFFFFF, {}, "unknown binding x"))
        self.assertIn("port: unknown binding x", d)


# 10000: cmp eax,0; je 10008; inc eax; jmp 10009; (10008) dec eax; (10009) ret
DIAMOND = bytes.fromhex("83F800" "7403" "40" "EB01" "48" "C3")


@needs_unicorn
class VerifySpecTests(unittest.TestCase):
    ZERO = V.PortResult(0xFFFFFFFF, 0xFFFFFFFF)        # what a correct port returns for eax = 0 (dec)
    ONE = V.PortResult(2, 0xFFFFFFFF)                  # ... and for eax = 1 (inc)

    def test_every_block_exercised_and_agreeing_is_verified(self):
        spec = V.Spec("d", 0x10000, [V.Case("z", {"eax": 0}), V.Case("o", {"eax": 1})])
        r = V.verify_spec(spec, image(DIAMOND), {"z": self.ZERO, "o": self.ONE})
        self.assertEqual((r.verdict, r.hit, r.total, r.unhit), ("VERIFIED", 4, 4, []))

    def test_an_unexercised_block_is_partial_not_verified(self):
        spec = V.Spec("d", 0x10000, [V.Case("z", {"eax": 0})])
        r = V.verify_spec(spec, image(DIAMOND), {"z": self.ZERO})
        self.assertEqual((r.verdict, r.hit, r.total, r.unhit), ("PARTIAL", 3, 4, [0x10005]))

    def test_a_named_unhit_block_is_accepted(self):
        spec = V.Spec("d", 0x10000, [V.Case("z", {"eax": 0})], unhit_named={0x10005: "the inc arm needs eax != 0"})
        r = V.verify_spec(spec, image(DIAMOND), {"z": self.ZERO})
        self.assertEqual((r.verdict, r.unhit), ("VERIFIED", []))

    def test_a_difference_is_a_mismatch_even_with_full_coverage(self):
        spec = V.Spec("d", 0x10000, [V.Case("z", {"eax": 0}), V.Case("o", {"eax": 1})])
        r = V.verify_spec(spec, image(DIAMOND), {"z": self.ZERO, "o": V.PortResult(3, 0xFFFFFFFF)})
        self.assertEqual(r.verdict, "MISMATCH")
        self.assertEqual(r.problems, ["o: eax (mask 0xFFFFFFFF): original 0x2, port 0x3"])

    def test_an_unmodeled_instruction_makes_the_function_not_exercisable(self):
        spec = V.Spec("i", 0x10000, [V.Case("x", {})])
        r = V.verify_spec(spec, image(bytes.fromhex("CD21" "C3")), {"x": V.PortResult()})
        self.assertEqual(r.verdict, "NOT_EXERCISABLE")
        self.assertIn("int at 0x10000", r.problems[0])

    def test_an_indirect_jump_keeps_the_function_partial(self):
        # 10000: jmp eax (target unknown to the static scan) — the run returns through 0x10002
        code = bytes.fromhex("FFE0") + bytes(14) + bytes.fromhex("C3")        # 10010: ret
        spec = V.Spec("j", 0x10000, [V.Case("x", {"eax": 0x10010})])
        r = V.verify_spec(spec, image(code), {"x": V.PortResult(0x10010, 0xFFFFFFFF)})
        self.assertEqual(r.verdict, "PARTIAL")
        self.assertIn("indirect jmp/call at 0x10000", r.problems[0])

    def test_reads_outside_the_image_are_listed(self):
        code = bytes.fromhex("A100000001" "C3")            # mov eax,[0x1000000]; ret
        spec = V.Spec("o", 0x10000, [V.Case("x", {"eax": 0xFFFFFFFF})])
        r = V.verify_spec(spec, image(code), {"x": V.PortResult(0, 0xFFFFFFFF)})
        self.assertEqual((r.verdict, r.outside), ("VERIFIED", [(0x1000000, 4)]))
        self.assertIn("reads outside the image: 0x1000000+4", V.table_row(r))


class RunPortTests(unittest.TestCase):
    def test_a_runner_that_never_returns_is_an_error_not_a_hang(self):
        with tempfile.TemporaryDirectory() as d:
            script = os.path.join(d, "slow")
            with open(script, "w") as f:
                f.write("#!/bin/sh\nsleep 30\n")
            os.chmod(script, os.stat(script).st_mode | stat.S_IXUSR)
            with self.assertRaises(RuntimeError) as cm:
                V.run_port(script, "x.exe", os.path.join(d, "img"), "", timeout=1)
            self.assertIn("timed out", str(cm.exception))

    def test_a_failing_runner_is_an_error(self):
        with tempfile.TemporaryDirectory() as d:
            script = os.path.join(d, "bad")
            with open(script, "w") as f:
                f.write("#!/bin/sh\necho boom >&2\nexit 1\n")
            os.chmod(script, os.stat(script).st_mode | stat.S_IXUSR)
            with self.assertRaises(RuntimeError) as cm:
                V.run_port(script, "x.exe", os.path.join(d, "img"), "")
            self.assertIn("boom", str(cm.exception))


DIFFRUN = os.path.join(ROOT, "build", "diffrun")
EXE = os.path.join(os.environ.get("PR_GAME_DIR", os.path.join(ROOT, "data", "game", "C")), "PRAGE.EXE")


@needs_unicorn
@unittest.skipUnless((os.path.exists(DIFFRUN) and os.path.exists(EXE)) or REQUIRED,
                     "build/diffrun or PRAGE.EXE absent")
class RealFunctionTests(unittest.TestCase):
    """The four ported functions against their original bytes, and the harness's own self-check."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        img = os.path.join(cls.tmp.name, "image.bin")
        cls.real = {r.name: r for r in V.verify_all(DIFFRUN, EXE, img, V.SPECS)}
        cls.mut = {r.name: r for r in V.verify_all(DIFFRUN, EXE, img, V.SPECS, mutants=True)}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_the_four_ported_functions_agree_with_the_original_on_every_block(self):
        self.assertEqual(sorted(self.real), ["config_codeword_len", "config_credit_spend",
                                             "fighter_slot_flag", "rng_next"])
        for name, r in self.real.items():
            self.assertEqual((r.verdict, r.problems, r.unhit, r.hit), ("VERIFIED", [], [], r.total), name)
            self.assertEqual(r.outside, [], name)

    def test_every_mutant_is_reported_as_a_mismatch(self):
        self.assertEqual(sorted(self.mut), [
            "config_codeword_len@mutant", "config_credit_spend@mutant", "config_credit_spend@signed",
            "fighter_slot_flag@mutant", "rng_next@mutant"])
        for name, r in self.mut.items():
            self.assertEqual(r.verdict, "MISMATCH", name)

    def test_the_unsigned_guard_case_is_the_only_one_that_catches_the_signed_mutant(self):
        # config_credit_spend 0x2CA7C: `cmp eax,[0x105c00]; ja` is unsigned. n = 0x80000001 against
        # 1 credit is "above" unsigned and negative signed; only case c5 tells them apart.
        r = self.mut["config_credit_spend@signed"]
        self.assertTrue(r.problems and all(p.startswith("c5:") for p in r.problems), r.problems)

    def test_a_forgotten_write_is_caught_by_the_byte_diff_alone(self):
        # fighter_slot_flag@mutant returns the right value and only forgets to set the bit.
        r = self.mut["fighter_slot_flag@mutant"]
        self.assertTrue(r.problems and not any("eax" in p for p in r.problems), r.problems)
        self.assertTrue(any("0x107EE0" in p for p in r.problems), r.problems)

    def test_the_slot_flag_case_with_scratch_eax_bits_exists(self):
        # case f9 is what makes the AL mask load-bearing: the original's EAX is 0x201 there.
        spec = [s for s in V.SPECS if s.name == "fighter_slot_flag"][0]
        c = [c for c in spec.cases if c.id == "f9"][0]
        img = E.Image.load(os.path.join(self.tmp.name, "image.bin"))
        o = E.run_original(img, spec.entry, c.regs, c.pokes)
        self.assertEqual(o.regs["eax"], 0x201)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run them to see them fail**

Run: `python3 -m unittest tools.tests.test_diff_verify 2>&1 | tail -4`
Expected: `ModuleNotFoundError: No module named 'diff_verify'`.

- [ ] **Step 3: Write the driver**

Create `tools/diff_verify.py` with this content:

```python
#!/usr/bin/env python3
"""Differential verification driver (spec 2026-09-30-reverse-completion-design §5).

For each function in SPECS: run the original bytes (tools/diff_emu.py) and the port's C function
(build/diffrun, port/tests/diff_runner.c) from the same image with the same registers and pokes,
and compare the output register, every changed byte, and the block coverage of the original.

Narrow claim, in every verdict: VERIFIED means equivalence on the exercised blocks and inputs
only. It does not mean the function is correct for states the cases never produce.
"""
import argparse
import os
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import diff_emu as E


def le32(v):
    return (v & 0xFFFFFFFF).to_bytes(4, "little")


@dataclass
class Case:
    id: str
    regs: dict
    pokes: dict = field(default_factory=dict)      # addr -> bytes


@dataclass
class Spec:
    name: str                  # the diffrun binding name
    entry: int                 # the original's entry address
    cases: list
    unhit_named: dict = field(default_factory=dict)   # block leader -> reason it is not exercised
    allow_calls: tuple = ()
    mutants: tuple = ("@mutant",)    # diffrun binding suffixes that must be reported as MISMATCH


@dataclass
class PortResult:
    eax: int = 0
    mask: int = 0xFFFFFFFF
    writes: dict = field(default_factory=dict)
    error: str = ""


def cases_text(spec, port_name):
    out = []
    for c in spec.cases:
        out.append("case %s" % c.id)
        out.append("fn %s" % port_name)
        for r, v in c.regs.items():
            out.append("reg %s 0x%X" % (r, v))
        for a, b in c.pokes.items():
            out.append("poke 0x%X %s" % (a, bytes(b).hex()))
        out.append("end")
    return "\n".join(out) + "\n"


def parse_port_output(text):
    res, cur = {}, None
    for line in text.splitlines():
        t = line.split()
        if not t:
            continue
        if t[0] == "case":
            cur = res.setdefault(t[1], PortResult())
        elif cur is None:
            raise ValueError("diffrun output outside a case: %r" % line)
        elif t[0] == "ret" and t[1] == "eax":
            cur.eax, cur.mask = int(t[2], 16), int(t[4], 16)
        elif t[0] == "w":
            cur.writes[int(t[1], 16)] = int(t[2], 16)
        elif t[0] == "error":
            cur.error = " ".join(t[1:])
        elif t[0] == "end":
            cur = None
    return res


def run_port(diffrun, exe, image_path, text, timeout=60):
    with tempfile.NamedTemporaryFile("w", suffix=".cases", delete=False) as f:
        f.write(text)
        path = f.name
    try:
        p = subprocess.run([diffrun, "--exe", exe, "--image-out", image_path, "--cases", path],
                           capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        raise RuntimeError("diffrun timed out after %ds (a port function did not return)" % timeout)
    finally:
        os.unlink(path)
    if p.returncode != 0:
        raise RuntimeError("diffrun failed (%d): %s" % (p.returncode, p.stderr.strip()))
    return parse_port_output(p.stdout)


def compare(orig, port):
    """The discrepancies between one original run and one port run (empty = they agree)."""
    out = []
    if port.error:
        out.append("port: " + port.error)
    if (orig.regs["eax"] & port.mask) != port.eax:
        out.append("eax (mask 0x%X): original 0x%X, port 0x%X" % (port.mask, orig.regs["eax"] & port.mask, port.eax))
    for a in sorted(set(orig.writes) | set(port.writes)):
        if orig.writes.get(a) != port.writes.get(a):
            o, p = orig.writes.get(a), port.writes.get(a)
            out.append("byte 0x%X: original %s, port %s" % (
                a, "unchanged" if o is None else "0x%02X" % o, "unchanged" if p is None else "0x%02X" % p))
    return out


@dataclass
class SpecResult:
    name: str
    entry: int
    verdict: str                 # VERIFIED | PARTIAL | MISMATCH | NOT_EXERCISABLE
    ncases: int = 0
    hit: int = 0
    total: int = 0
    problems: list = field(default_factory=list)
    unhit: list = field(default_factory=list)       # unhit leaders with no stated reason
    outside: list = field(default_factory=list)


def verify_spec(spec, image, port_results, port_name=None):
    """Compare every case of `spec` against `port_results` (case id -> PortResult)."""
    info = E.static_scan(image, spec.entry)
    executed, outside, problems, blocked = set(), set(), [], []
    for c in spec.cases:
        orig = E.run_original(image, spec.entry, c.regs, c.pokes, spec.allow_calls)
        if orig.outcome != "ok":
            blocked.append("%s: %s (%s)" % (c.id, orig.outcome, orig.detail))
            continue
        executed |= orig.executed
        outside |= set(orig.outside)
        for d in compare(orig, port_results[c.id]):
            problems.append("%s: %s" % (c.id, d))
    hit, unhit = E.coverage(info, executed)
    res = SpecResult(port_name or spec.name, spec.entry, "VERIFIED", len(spec.cases), len(hit),
                     len(info.leaders), problems, [a for a in unhit if a not in spec.unhit_named],
                     sorted(outside))
    if problems:
        res.verdict = "MISMATCH"
    elif blocked:
        res.verdict, res.problems = "NOT_EXERCISABLE", blocked
    elif res.unhit or info.indirect or info.truncated:
        res.verdict = "PARTIAL"
        if info.indirect:
            res.problems.append("indirect jmp/call at %s: targets unknown" % ",".join(hex(a) for a in info.indirect))
        if info.truncated:
            res.problems.append("static scan truncated")
    return res


# ---- the E1 specs: four ported functions, each cased to cover every block ----------------------

DS_RNG = 0xEF6D8
DS_FLAGS = 0x107EE0
DS_CREDITS = 0x105C00
DS_FREEPLAY = 0x105D60
DS_NODEBIT = 0x104B1F

SPECS = [
    Spec("rng_next", 0x5D7DC, [
        Case("r1", {"eax": 0x1234}, {DS_RNG: le32(0x12345678)}),
        Case("r2", {"eax": 0xFFFF}, {DS_RNG: le32(0xFFFFFFFF)}),
        Case("r3", {"eax": 0x10000}, {DS_RNG: le32(1)}),       # range & 0xFFFF == 0
        Case("r4", {"eax": 0}, {DS_RNG: le32(0xABCD)}),
    ]),
    Spec("fighter_slot_flag", 0x3C570, [
        Case("f0", {"eax": 0}, {DS_FLAGS: le32(0)}),           # sets bit 0, returns 0
        Case("f5", {"eax": 5}, {DS_FLAGS: le32(0x20)}),        # already set: returns 1, no write
        Case("f9", {"eax": 9}, {DS_FLAGS: le32(0x205)}),       # AL-only return: EAX bits 8+ are scratch
        Case("f31", {"eax": 31}, {DS_FLAGS: le32(0)}),
        Case("f33", {"eax": 33}, {DS_FLAGS: le32(2)}),         # x86 masks the count to 5 bits
    ]),
    Spec("config_credit_spend", 0x2CA7C, [
        Case("c1", {"eax": 5}, {DS_FREEPLAY: b"\x01", DS_CREDITS: le32(0)}),
        Case("c2", {"eax": 5}, {DS_FREEPLAY: b"\x00", DS_CREDITS: le32(3), DS_NODEBIT: b"\x00"}),
        Case("c3", {"eax": 5}, {DS_FREEPLAY: b"\x00", DS_CREDITS: le32(5), DS_NODEBIT: b"\x00"}),
        Case("c4", {"eax": 3}, {DS_FREEPLAY: b"\x00", DS_CREDITS: le32(10), DS_NODEBIT: b"\x01"}),
        Case("c5", {"eax": 0x80000001}, {DS_FREEPLAY: b"\x00", DS_CREDITS: le32(1), DS_NODEBIT: b"\x00"}),
    ], mutants=("@mutant", "@signed")),
    Spec("config_codeword_len", 0x2D4B4, [
        Case("w%d" % n, {"eax": n}) for n in (0, 1, 0x26, 0xFF, 0x1000)
    ]),
]


# ---- driver -------------------------------------------------------------------------------------

def verify_all(diffrun, exe, image_path, specs, only=None, mutants=False):
    """One SpecResult per function (or per mutant binding when `mutants`)."""
    results = []
    for spec in specs:
        if only and spec.name != only:
            continue
        names = [spec.name + s for s in spec.mutants] if mutants else [spec.name]
        for name in names:
            port = run_port(diffrun, exe, image_path, cases_text(spec, name))
            results.append(verify_spec(spec, E.Image.load(image_path), port, name))
    return results


def table_row(r):
    note = "; reads outside the image: " + ", ".join("0x%X+%d" % o for o in r.outside) if r.outside else ""
    return "| %s | 0x%05X | %d | %d/%d | %s%s |" % (r.name, r.entry, r.ncases, r.hit, r.total, r.verdict, note)


TABLE_HEAD = ("| function | original | cases | blocks hit/total | verdict |\n"
              "|---|---|---|---|---|")


def skip(why):
    """Skip like the other oracles (spec §5.5): exit 0, unless PR_ORACLE_REQUIRED=1."""
    print("diff-verify: skipped: " + why)
    return 1 if os.environ.get("PR_ORACLE_REQUIRED") == "1" else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--diffrun", default="build/diffrun")
    ap.add_argument("--exe", default="data/game/C/PRAGE.EXE")
    ap.add_argument("--image", default=os.path.join(tempfile.gettempdir(), "pr_diff_image.bin"),
                    help="where diffrun dumps the image both sides run from")
    ap.add_argument("--function", help="only this function")
    ap.add_argument("--self-check", action="store_true",
                    help="also require every mutant binding to be reported as MISMATCH")
    ap.add_argument("--table", help="write the verification table (markdown) to this file")
    args = ap.parse_args(argv)

    if not E.available():
        return skip("unicorn is not installed (pip install -r tools/requirements-diff.txt)")
    if not os.path.exists(args.exe):
        return skip("%s is absent" % args.exe)
    if not os.path.exists(args.diffrun):
        print("diff-verify: %s is not built (make build)" % args.diffrun)
        return 2

    real = verify_all(args.diffrun, args.exe, args.image, SPECS, args.function)
    bad = [r for r in real if r.verdict != "VERIFIED"]
    mutants, missed = [], []
    if args.self_check:
        mutants = verify_all(args.diffrun, args.exe, args.image, SPECS, args.function, mutants=True)
        missed = [r for r in mutants if r.verdict != "MISMATCH"]

    rows = [TABLE_HEAD] + [table_row(r) for r in real + mutants]
    print("\n".join(rows))
    for r in real:
        for p in r.problems:
            print("  %s: %s" % (r.name, p))
        for a in r.unhit:
            print("  %s: block 0x%X not exercised" % (r.name, a))
    for r in missed:
        print("  %s: the mutant was NOT detected (verdict %s)" % (r.name, r.verdict))
    if args.table:
        with open(args.table, "w") as f:
            f.write("\n".join(rows) + "\n")
    print("diff-verify: %d/%d functions VERIFIED%s. Claim: equivalence on the exercised blocks and "
          "inputs only." % (len(real) - len(bad), len(real),
                            "; %d/%d mutants detected" % (len(mutants) - len(missed), len(mutants))
                            if args.self_check else ""))
    return 1 if bad or missed else 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Run the unit tests**

Run: `python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify 2>&1 | tail -4`
Expected: `Ran 42 tests ... OK` (the five real-function tests run when `build/diffrun` and `data/game/C/PRAGE.EXE` exist, and skip otherwise).

- [ ] **Step 5: Run the CLI**

Run: `python3 tools/diff_verify.py --self-check; echo exit=$?`
Expected: nine table rows (four `VERIFIED` with `blocks hit/total` of `1/1`, `3/3`, `7/7`, `4/4`; five `MISMATCH` mutants), then `diff-verify: 4/4 functions VERIFIED; 5/5 mutants detected. Claim: equivalence on the exercised blocks and inputs only.` and `exit=0`.

- [ ] **Step 6: Wire the Makefile**

In `Makefile`: add `diff-verify` to the `.PHONY` list; add these variables next to `GP_DUMP = /tmp/pr_gp_dump`:

```make
DIFF_IMAGE ?= /tmp/pr_diff_image.bin
DIFF_TABLE ?= /tmp/pr_diff_table.md
```

add this target after `gp-report` (before the `audio-render` comment block):

```make
# Differential verification (spec 2026-09-30-reverse-completion-design §5): the original's own
# bytes run in an emulator against the port's C functions from the same image; compares every
# changed byte, the return register and the block coverage. Skips cleanly without unicorn
# (spec §5.5); tools/diff_verify.py skips without PRAGE.EXE. The claim is narrow: equivalence on
# the exercised blocks and inputs only.
diff-verify: build ## Differential verification: original x86 bytes vs the port's C functions (skips without unicorn)
	@echo "== differential verification (original bytes vs the port's C; record E1) =="
	@if $(PYTHON) -c "import unicorn" 2>/dev/null; then \
		$(PYTHON) -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify && \
		$(PYTHON) tools/diff_verify.py --diffrun $(BUILD_DIR)/diffrun --exe $(GAME_DIR)/PRAGE.EXE \
			--image $(DIFF_IMAGE) --table $(DIFF_TABLE) --self-check; \
	else echo "diff-verify: skipped: unicorn is not installed (pip install -r tools/requirements-diff.txt)"; fi
```

and in the `verify` recipe, after the `gp-oracle` line (`@$(MAKE) --no-print-directory gp-oracle`) and before the `== k11 and gp tool unit tests ==` echo, add:

```make
	@$(MAKE) --no-print-directory diff-verify
```

- [ ] **Step 7: Run the target, and the skip path**

Run: `make diff-verify 2>&1 | tail -16`
Expected: the unit tests `OK`, the nine-row table, `diff-verify: 4/4 functions VERIFIED; 5/5 mutants detected...`.

Skip path: `make diff-verify PYTHON=/usr/bin/python3 2>&1 | tail -2` prints `diff-verify: skipped: unicorn is not installed ...` and exits 0, provided `/usr/bin/python3` has no `unicorn` (check with `/usr/bin/python3 -c "import unicorn"`; if that interpreter does have it, skip this check and say so in the ledger).

- [ ] **Step 8: Commit, then prove the assertions can fail**

```bash
git add tools/diff_verify.py tools/tests/test_diff_verify.py Makefile
git commit -m "$(cat <<'EOF'
tools: diff_verify compares the original bytes with the port's C, make diff-verify (E1)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

Apply each mutation, run `python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify 2>&1 | grep -E '^(FAIL|ERROR):|^Ran|^OK'`, check the named tests fail, then restore (`git checkout -- <file>`; for the C mutation, rebuild with `cmake --build build --target diffrun` after restoring):

| mutation | must fail |
|---|---|
| `perl -0pi -e 's/        Case\("c5".*\n//' tools/diff_verify.py` | `test_every_mutant_is_reported_as_a_mismatch`, `test_the_unsigned_guard_case_is_the_only_one_that_catches_the_signed_mutant` |
| `perl -0pi -e 's/    for a in sorted\(set\(orig.writes\) \| set\(port.writes\)\):/    for a in []:/' tools/diff_verify.py` | `test_a_write_the_port_forgot_is_a_discrepancy`, `test_a_write_the_original_did_not_make_is_a_discrepancy`, `test_a_forgotten_write_is_caught_by_the_byte_diff_alone`, `test_every_mutant_is_reported_as_a_mismatch` |
| `perl -0pi -e 's/elif res.unhit or info.indirect or info.truncated:/elif False:/' tools/diff_verify.py` | `test_an_unexercised_block_is_partial_not_verified`, `test_an_indirect_jump_keeps_the_function_partial` |
| `perl -0pi -e 's/\{ "fighter_slot_flag",        b_slot_flag,    0x000000FFu \}/{ "fighter_slot_flag",        b_slot_flag,    0xFFFFFFFFu }/' port/tests/diff_runner.c` then rebuild | `test_the_four_ported_functions_agree_with_the_original_on_every_block` (the real `fighter_slot_flag` becomes a MISMATCH on case `f9`) |

Record the four results in the ledger. Do not commit a mutation.

---

### Task 5: The record, the docs and the gate

**Files:**
- Create: `docs/superpowers/plans/2026-09-30-reverse-completion-e1-derivations.md`
- Modify: `docs/PROGRESS.md`, `AGENTS.md`, `README.md`, `docs/superpowers/specs/2026-09-30-reverse-completion-design.md`

- [ ] **Step 1: Measure and collect the evidence for the record**

```bash
python3 tools/diff_verify.py --self-check --image /tmp/pr_e1_image.bin --table /tmp/pr_e1_table.md | tail -3
python3 - <<'EOF'
import sys, time
sys.path.insert(0, "tools")
import diff_emu as E
img = E.Image.load("/tmp/pr_e1_image.bin")
t = time.time()
for _ in range(50):
    E.run_original(img, 0x2D4B4, {"eax": 0x1000})
print("run_original: %.2f ms per case" % ((time.time() - t) / 50 * 1000))
EOF
time build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_e1_t.img
python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify 2>&1 | tail -3
```

Use the numbers you measure, not the ones this plan quotes.

- [ ] **Step 2: Write the record**

Create `docs/superpowers/plans/2026-09-30-reverse-completion-e1-derivations.md`. Sections, each with the evidence in it:

- **§E.1 Addressing (spec §5.4, settled).** The image is flat 32-bit with no segment base: operands are plain linear addresses. Evidence, from the loaded image (fixups applied): `rng_next` at `0x5D7DC` reads `mov eax, dword ptr [0xef6d8]` and writes `mov dword ptr [0xef6d8], eax`; `fighter_slot_flag` at `0x3C570` reads `mov edx, dword ptr [0x107ee0]`; `config_credit_spend` at `0x2CA7C` reads `cmp byte ptr [0x105d60], 0`. `symbols.h`'s `DS_000EF6D8` is that same linear address and `DSD(o)` indexes `mem[o]`. So the emulator maps a 64 MB flat space with the image at `0x10000` (the dump `mem_load_le` writes: `0x10B0D0 - 0x10000 = 1028304` bytes) and needs no segment setup. The "DS offset = address - 0x80000" wording in `AGENTS.md` describes how Ghidra names globals, not how the code addresses them.
- **§E.2 Install (spec §5.4, settled).** `unicorn` 2.1.4 and `capstone` 5.0.7 install with `pip` on this host (macOS arm64, Python 3.12.12); no interpreter fallback was needed.
- **§E.3 The harness's contract and what it compares.** The flow, the case-file format, the compared quantities (EAX under the binding's mask, every changed byte with the stack excluded, block coverage), and the verdict definitions `VERIFIED | PARTIAL | MISMATCH | NOT_EXERCISABLE`.
- **§E.4 Results.** The table from Step 1, verbatim, and the mutation proofs from the ledger (Tasks 1, 2, 4), each as "mutation -> test that failed".
- **§E.5 Findings.** Any case on which a real port function disagreed with the original (none is expected: `f33`, the shift count of 33, is the one to look at, because C leaves it undefined; state whether it agreed here and that it depends on the compiler). A disagreement is a finding to record here for the P track, not something E1 fixes.
- **§E.6 What E1 does not do (limits, each a named gap for later tracks).** (1) Calls out are an allow-list, not stubs: a call outside it stops the run as `unmodeled`, so a function with an unported callee is `NOT_EXERCISABLE` until the P track adds stubs that both sides share (spec §5.2). (2) Only EAX is compared as a register output; flags, other registers and the callee-saved set are not. (3) Stack-argument functions (`ret N`) and functions with C out-parameters have no binding form yet. (4) The only snapshot is the boot image plus `poke`s (at most 16 of 64 bytes); capture-frame snapshots are the next increment. (5) The emulator has no port I/O, interrupts, FPU or segment registers; such functions are `NOT_EXERCISABLE` with the instruction named. (6) The cost: the measured ms per case and per `diffrun` load, which bounds the P track's batch sizes.
- **§E.7 The narrow claim**, in the repo's usual wording: VERIFIED proves equivalence on the exercised blocks and inputs only; it cannot detect a bug in a state the cases never produce, and the emulator and the port share the image, so a wrong image would not show.

- [ ] **Step 3: The other docs**

- `docs/PROGRESS.md`: append one paragraph (E1: what the harness is, the four functions verified, five mutants detected, the limits in one sentence, a pointer to the record). Append it; do not edit earlier paragraphs.
- `AGENTS.md`: in the Commands block add `make diff-verify   # differential verification: the original's bytes vs the port's C functions (skips without unicorn; in make verify)`; in the Tooling for RE list add that the differential harness needs `unicorn` (`pip install -r tools/requirements-diff.txt`).
- `README.md`: in the Toolchain list add the same `unicorn` line after the Python/`capstone` bullet.
- The spec: in §5.4 replace the two open questions with their answers (addressing is flat, base 0, with the §E.1 evidence; `unicorn` 2.1.4 installs) and a pointer to the record.

- [ ] **Step 4: Run the full gate**

From `.worktrees/reverse-e1`, with a unique tag `<t>` such as `e1`:

```bash
T=e1
make verify SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md > /tmp/pr_${T}_verify.log 2>&1; echo EXIT=$?
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_${T}_verify.log | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLE-LINES-EQUAL
make audio-render AUDIO_WAV=/tmp/pr_${T}.wav > /dev/null 2>&1; cmp /tmp/pr_${T}.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-IDENTICAL
grep -n "diff-verify:" /tmp/pr_${T}_verify.log | tail -2
python3 tools/port_progress.py
git diff --stat main -- port/src
```

Expected: `EXIT=0`; `ORACLE-LINES-EQUAL`; `WAV-IDENTICAL`; the `diff-verify: 4/4 functions VERIFIED; 5/5 mutants detected` line in the log; `771 1203 64` and `731 731 100`; and an empty `git diff --stat main -- port/src`. A gate that differs in any of these stops the task: report it, do not paper over it. (A fresh checkout without `unicorn` must still pass: the `diff-verify` step prints its skip line.)

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/plans/2026-09-30-reverse-completion-e1-derivations.md docs/PROGRESS.md AGENTS.md README.md docs/superpowers/specs/2026-09-30-reverse-completion-design.md
git commit -m "$(cat <<'EOF'
docs: E1 record, the differential harness's evidence, limits and the settled addressing/install questions

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

Report: the head SHA, the gate output (EXIT, oracle lines, WAV, counters), the table, the mutation results, and any finding from §E.5.

---

## Execution notes

- Tasks 1, 2, 4 are Python integration with complete code: a standard-tier implementer. Task 3 is a transcription plus a build and a hand smoke: the cheapest tier is enough. Task 5 is a record plus the gate: standard tier. Review each with a standard-tier reviewer; the final whole-branch review goes to the most capable model.
- The code in Tasks 1-4 was prototyped and run end to end before this plan was written (42 Python tests pass, nine table rows, every mutation above fails the named tests), so a deviation from it needs a reason in the report.
- Handoff after E1: E2 (triage of the 575 candidates) and the U5-U8, U11 plans can start, and P (the port batches) starts once the stub design for calls (record §E.6 item 1) is planned.
