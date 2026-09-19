# OPL Driver Fidelity (2a-ii) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reproduce `SBPRO2.MDI`'s FM note-setup path and its velocity→TL function so the port's OPL register stream matches the captured original further than write 2, with every remaining divergence named.

**Architecture:** A new disassembly harness (`tools/mdi_disasm.py`) derives the driver's behaviour from the shipped `SBPRO2.MDI`; the derived facts are transcribed into `port/src/platform/audio/sequencer.c`. The existing capture comparison in `port/tests/test_sequencer.c` is the oracle: it compares the C register stream against `data/audio-captures/prage_000.dro` and reports the first difference.

**Tech Stack:** C99 (fixed profile), python3 + capstone (16-bit x86), the existing `make verify` ladder.

## Global Constraints

- Spec: `docs/superpowers/specs/2026-09-18-opl-driver-design.md`. Base is `main` at `d7082e5`.
- The oracle is the **write stream**, not audio. No audio/audibility claim is made by this cycle.
- `data/` is read-only. `tools/opl_trace.py` and the `.dro` captures must not be modified.
- No new dependencies beyond what is already installed (capstone is already used by `tools/`).
- `mem[]` holds original state at original offsets; never write `mem[0xA0000]`.
- SDL only in `port/src/host.c` and `port/src/main.c`.
- Comments in `port/src` only `/* PORT: ... */`, `/* TODO(verify): ... */`, and address tags.
- Tests: only `CHECK(cond,msg)` / `CHECK_EQ_INT(a,b)`; declared in `port/tests/test.h`, called from `run_tests.c`; new sources added explicitly to `port/CMakeLists.txt`.
- `port/src/symbols.h` must regenerate byte-identically: `python3 tools/gen_symbols.py port/decomp port/src/symbols.h`.
- Build 0 warnings. Stage explicit paths only; never `git add -A`. Commit style `<area>: <what changed>`.
- The velocity→TL transform is applied **at note setup in `sequencer.c`**, never in `patches.c` at decode time.

---

### Task 1: The `SBPRO2.MDI` disassembly harness

`data/game/C/SBPRO2.MDI` (16541 bytes) begins `AIL3MDI` and is 16-bit real-mode code; nothing in the repo can read it yet. Nothing can be derived until this exists.

**Files:**
- Create: `tools/mdi_disasm.py`
- Create: `tools/tests/test_mdi_disasm.py`

**Interfaces:**
- Produces: `mdi_disasm.py FILE.mdi` prints a listing (address, bytes, instruction) to stdout, and `--info` prints the parsed header fields. Importable functions: `load(path) -> MdiImage` and `MdiImage.disassemble() -> list[(addr, bytes, text)]`.

- [ ] **Step 1: Write the failing test**

In `tools/tests/test_mdi_disasm.py` (pytest, matching `tools/tests/test_title_pin.py`'s style), assert the container parse on the shipped file:

```python
import mdi_disasm

def test_loads_the_shipped_driver():
    img = mdi_disasm.load("data/game/C/SBPRO2.MDI")
    assert img.magic == b"AIL3MDI"
    assert img.size == 16541
    assert img.code_origin > 0          # the real-mode load origin, derived in Step 3

def test_disassembles_to_instructions():
    img = mdi_disasm.load("data/game/C/SBPRO2.MDI")
    ins = img.disassemble()
    assert len(ins) > 1000
    # addresses are monotonic and inside the image
    addrs = [a for a, _b, _t in ins]
    assert addrs == sorted(addrs)
```

- [ ] **Step 2: Run it and watch it fail**

Run: `python3 -m pytest tools/tests/test_mdi_disasm.py -q`
Expected: FAIL — `ModuleNotFoundError: mdi_disasm`.

- [ ] **Step 3: Determine the load origin, then write the harness**

Parse the container: read the `AIL3MDI` magic (offset 0) and the header fields after it; identify where the code body starts and the segment it is loaded at. Determine the origin **from evidence, not assumption** — e.g. by locating a `call`/`jmp` target that lands on a plausible instruction boundary, or by matching a header field against the file's structure — and record the evidence in the tool's docstring and in the Task 5 report. Load the body flat at that origin, disassemble with capstone in 16-bit x86 mode (`Cs(CS_ARCH_X86, CS_MODE_16)`), and emit `(addr, raw_bytes, text)`.

The tool is a derivation aid, not port code: it does not need to be exhaustive, only correct for the regions used in Tasks 2–3. Skip an undecodable region by advancing one byte, as a debugger would, and mark it.

- [ ] **Step 4: Run the tests green**

Run: `python3 -m pytest tools/tests/test_mdi_disasm.py -q`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add tools/mdi_disasm.py tools/tests/test_mdi_disasm.py
git commit -m "tools: disassemble the AIL3MDI SBPRO2.MDI driver"
```

---

### Task 2: Derive and implement the velocity→TL function

Spec §1 records the open divergence: the carrier TL velocity term. A dominant `+ 0x16 + ((127 - velocity) >> 3)` fits the `[10] = 0x00`/`0x40` patches but patch `0x34` is +1 and `0x74` is −1. A formula that fits only the easy patches is the already-rejected answer.

**Files:**
- Modify: `port/src/platform/audio/sequencer.c` (the carrier TL write in `apply_patch`, around `sequencer.c:130`)
- Modify: `port/tests/test_sequencer.c`
- Create: `docs/superpowers/plans/2026-09-18-opl-velocity-tl.md` (the derivation: the driver instructions and the function)

**Interfaces:**
- Consumes: `mdi_disasm.py` from Task 1; `patches_lookup(key)`; the `opl_write` seam.
- Produces: the carrier TL byte written at `0x40 + base` is the driver's function of velocity and patch, not `p[10]` verbatim.

- [ ] **Step 1: Locate the path in the driver**

Using `mdi_disasm.py`, find where the driver computes the carrier TL from a note's velocity: look for the patch `[10]`/`[9..13]` byte being combined with a value derived from the key-on velocity, and for the `0x40`-family register write. Record the exact instructions and addresses in the derivation doc.

- [ ] **Step 2: Derive the function and check it against every captured case**

State the derived function and evaluate it against the cases the spec lists, each from `prage_000.dro` + `FAT.OPL`:

| patch | `[10]` | velocity | captured carrier TL written |
|---|---|---|---|
| `0x49` | `0x00` | 127 | `0x16` |
| `0x49` | `0x00` | 122 | `0x16` |
| `0x49` | `0x00` | 113 | `0x17` |
| `0x49` | `0x00` | 104 | `0x18` |
| `0x1e` | — | 127 | `0x16` |
| `0x58` | — | 127 | `0x16` |
| `0x24` | — | 120, 127 | `0x56` |
| `0x34` | `0x83` | 127 | `0x9a` |
| `0x74` | `0x03` | 115 | `0x19` |
| `0x74` | `0x03` | 126, 127 | `0x18` |

If the driver's function reproduces all of these, it is the answer; if it does not, record the mismatch in the doc and stop — do not fit a formula to the table. Write the derivation and the instruction addresses to the doc.

- [ ] **Step 3: Write the failing test**

In `port/tests/test_sequencer.c`, using the existing `opl_trace_*` seam: load `FAT.OPL`, load a bank that keys the relevant program at a known velocity, and assert the carrier TL byte written to `0x40 + base` equals the captured value for at least the `0x49` and `0x74` cases (the `[10] = 0x00` patch and the ±1 patch). Assert on the write stream, not on audio.

- [ ] **Step 4: Run it and watch it fail**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: FAIL — the port writes `p[10]` verbatim, so the `0x74` case is wrong.

- [ ] **Step 5: Implement the derived function**

Replace the verbatim `p[10]` carrier write at `sequencer.c:130` with the derived function, with a `/* PORT: */` marker citing the driver address and the derivation doc. Keep the modulator write (`0x43 + base`, `p[4]`) unchanged unless the driver proves otherwise.

- [ ] **Step 6: Run green and re-check the capture line**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`, and the `capture oracle first difference …` line unchanged in position (this task changes a value, not the order) — record the line.

- [ ] **Step 7: Commit**

```bash
git add port/src/platform/audio/sequencer.c port/tests/test_sequencer.c docs/superpowers/plans/2026-09-18-opl-velocity-tl.md
git commit -m "audio: apply SBPRO2.MDI's velocity-to-TL term to the carrier"
```

---

### Task 3 (re-scoped): Match the driver's patch-application timing

> **Errata (2026-09-18).** The original Task 3 assumed the write-2 divergence was a *register-order* difference. Task 1's harness and the Task 3 investigation **falsified** that: the driver's note-setup order is `20 23 40 43 60 63 80 83 E0 E3 C0 A0 B0` (key-on `0xB0` last) and the port already emits exactly that, so no order change exists. The write-2 divergence is caused by *when* the driver emits the operator families, not their order. This task replaces the original Task 3. Evidence: `.superpowers/sdd/2026-09-18-opl-driver/task-3-report.md` §1–§3.

The original's tick-0 block (capture `prage_000.dro` lines 3–143, `0x20..0x95` values `0x01/0x3f/0xf6/0x0c`) is the driver's init/patch load: the operator families are emitted once, up front. The port instead re-emits the whole patch at every note-on, so its first post-tick-0 write is `0x20` where the capture's is the key-on `0xB0`.

Driver facts already established (Task 3 report §1, to be recorded durably in this task's derivation doc):
- `0x3184` is a flag-driven state machine over the per-voice byte `[si+0x1539]`: each bit names a register family; a handler writes that family, clears its bit, and jumps to the next test.
- Note-on sets `[si+0x1539] = 0xf9` immediately before calling `0x3184` (`0x30c8`); the percussion scheduler sets it identically (`0x3802`).
- Families/handlers: `0x80`→`0x3409` (`0x20`/`0x23`), `0x40`→`0x346a` (`0x40`/`0x43`), `0x20`→`0x34de` (`0x60`,`0x63`,`0x80`,`0x83`), `0x10`→`0x3542` (`0xE0`/`0xE3`), `0x08`→`0x3578` (`0xC0`), `0x01`→`0x35c2` (`0xA0`,`0xB0`).
- The mod/car slot tables are `0xc37` (mod) and `0xc49` (car); emitters `0x2aa4`/`0x2abe`/`0x2ad6`.

**Files:**
- Modify: `port/src/platform/audio/sequencer.c`
- Modify: `port/tests/test_sequencer.c`
- Create: `docs/superpowers/plans/2026-09-18-opl-patch-application.md`

**Interfaces:**
- Consumes: the write seam `opl_write`.
- Produces: the per-voice "pending families" state that decides which registers a note-on emits, so unchanged operators are not re-sent.

- [ ] **Step 1: Derive when each family bit is set and cleared**

Establish from `SBPRO2.MDI` (and `tools/mdi_disasm.py`) exactly what writes `[si+0x1539]`, what its bit values mean, and where the tick-0 patch load originates. Resolve: is `0xf9` set on every note-on, or only when the patch changed? Record the instructions and addresses in the derivation doc, **and also record the already-established note-setup order and the state-machine table above**, so the durable doc carries both results.

- [ ] **Step 2: Establish how the oracle normalises**

Read `port/tests/test_sequencer.c`'s capture comparison and determine precisely how the capture stream and the port stream are aligned (what the "normalised" capture count of 6380 means, and which writes are dropped on each side). The DoD depends on both streams being reduced the same way; if the oracle drops only the capture's tick-0 block, record that as the thing to reconcile.

- [ ] **Step 3: Write the failing test**

In `port/tests/test_sequencer.c`, assert on the trace (not audio) that the port's first post-tick-0 write equals the capture's first post-tick-0 write for the same tick, or that the port's tick-0 stream carries the init/patch-load block the capture has. It must fail against the current tree.

- [ ] **Step 4: Implement the derived timing**

Model the per-voice pending-family state in `sequencer.c`: emit the operator families when the driver does, and do not re-emit unchanged families at note-on. Keep the `0xC0`/`0xA0`/`0xB0` placement per the derived order (key-on last).

- [ ] **Step 5: Run the tests and the capture comparison**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`, and the **`capture oracle first difference` line advances beyond C write 2**, or reaches `all compared and matched`. Record the exact new line.

- [ ] **Step 6: Re-check the count gap**

Record the new C/capture write counts (were 9340 / 6380) and whether the gap narrowed — without tuning toward a count.

- [ ] **Step 7: Commit**

```bash
git add port/src/platform/audio/sequencer.c port/tests/test_sequencer.c docs/superpowers/plans/2026-09-18-opl-patch-application.md
git commit -m "audio: emit operator families when SBPRO2.MDI does, not per note-on"
```

---

### Task 4 (re-scoped): Derive and implement the percussion note→fnum remap

> **Errata (2026-09-18).** New task, per the same re-scope. The capture keys MIDI note 47 on the percussion channel as `(block 2, fnum 0x3CF)` → `0xB0 = 0x2B`; the port uses its 12-TET `NOTE_TAB[47] = (2, 0x28B)` → `0x2A` (spec divergence #6). Deriving the driver's frequency path is a separate, small RE task.

**Files:**
- Modify: `port/src/platform/audio/sequencer.c`
- Modify: `port/tests/test_sequencer.c`
- Create: `docs/superpowers/plans/2026-09-18-opl-percussion-fnum.md`

**Interfaces:**
- Consumes: Task 3's patch-application timing.
- Produces: the block/fnum pair the driver emits for a note, percussion and melodic.

- [ ] **Step 1: Derive the percussion and melodic frequency paths**

From the driver: percussion load `0x381d` sets `[si+0x1499] = 3` (`0x383b`); the note-on fnum is computed at `0x35fa`–`0x36a6` from `[si+0x14d5]`/`[si+0x14fd]` plus the patch's `[2]` base (`0x3aac`–`0x3ac1`). Establish the exact arithmetic for both the percussion and the melodic path, and verify the melodic anchors the Task 3 report says are already correct (note 84 = `0x2B2`, note 79 = `0x205`). Record instructions and addresses in the derivation doc.

- [ ] **Step 2: Write the failing test**

In `port/tests/test_sequencer.c`, assert on the trace (not audio) that the `0xB0` written for the capture's first note equals `0x2B`. It must fail against the current tree.

- [ ] **Step 3: Implement the derived mapping**

Replace the 12-TET `NOTE_TAB` path with the derived function for the percussion case (and melodic if the derivation differs from the current table), keeping `0xA0` (fnum low) and `0xB0` (block / fnum-high / key bit) consistent.

- [ ] **Step 4: Run the tests and the capture comparison**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`, and the `capture oracle first difference` line advances further, or reaches `all compared and matched`. Record the exact new line and the C/capture counts.

- [ ] **Step 5: Commit**

```bash
git add port/src/platform/audio/sequencer.c port/tests/test_sequencer.c docs/superpowers/plans/2026-09-18-opl-percussion-fnum.md
git commit -m "audio: derive SBPRO2.MDI's note frequencies"
```

---

### Task 5: Record the outcome and verify the ladder

**Files:**
- Create: `docs/superpowers/plans/2026-09-18-opl-driver-report.md`
- Modify: `port/spec/audio.md` ("Known capture divergences")
- Modify: `README.md`

**Interfaces:**
- Consumes: every prior task's result.

- [ ] **Step 1: Write the report**

Record: the derived velocity→TL function and the driver evidence for it; the derived write order; the **before and after** `capture oracle first difference` lines; the C/capture write counts (were 9340 vs 6380) and whether the gap narrowed; and every divergence in `port/spec/audio.md` with its disposition (matched, or excluded by name with cause). State plainly that no audio-fidelity claim is made.

- [ ] **Step 2: Update the divergence list and README**

In `port/spec/audio.md`, mark divergence #1 resolved (or state exactly what remains and why). Move each remaining divergence to matched/excluded. `README.md`'s audio paragraph says the register stream now matches the driver further than before, without claiming audibility.

- [ ] **Step 3: Full ladder**

Run: `rm -rf build && make verify`
Expected: exit 0, 0 warnings, `all checks passed`, `smk_compare` 120/120 + 41/41, the title oracle unchanged, the capture comparison line as recorded, `symbols.h` byte-identical.

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/plans/2026-09-18-opl-driver-report.md port/spec/audio.md README.md
git commit -m "docs: record the OPL driver-fidelity outcome"
```

---

## Self-Review

**Re-scope note (2026-09-18):** the original Task 3 and Task 4 were replaced after execution. Task 2 ended in record-and-stop (the velocity→TL input is engine/config-supplied, not derivable from `SBPRO2.MDI`); the original Task 3's order premise was falsified (the port already matches the driver's order), so the write-2 blocker was re-attributed to divergence #5 (patch-application timing) and #6 (percussion note→fnum). The re-scoped Tasks 3 and 4 target those; the original report task is now Task 5.

**Spec coverage:** §2 harness → Task 1. §3/§4 velocity→TL and its captured cases → Task 2 (recorded negatively, V engine-supplied). §5's advancing first-difference line → re-scoped Tasks 3–4. §6 DoD and §7/§8 dispositions and limits → Task 5 Steps 1–3. §1.1 decision 2 (residual named, not tuned) is enforced by Task 2's record-and-stop and Task 5 Step 1's write-count disclosure.

**Placeholder scan:** no TBD. Task 1 Step 3's load origin, the state-machine semantics in Task 3, and the frequency arithmetic in Task 4 are genuine RE deliverables, each with a stated method and an evidence requirement — the plan's interface mechanism, not placeholders. The one thing the plan deliberately does not pre-state is the functions themselves, because inventing them is the failure mode the spec exists to avoid.

**Type consistency:** `mdi_disasm.load`/`MdiImage.disassemble` are used only in Task 1's tests; `opl_write(u16,u8)` and `patches_lookup(u16)` are the existing signatures; `apply_patch`/`key_on`/`key_off` keep their current names and roles across Tasks 3 and 4.
