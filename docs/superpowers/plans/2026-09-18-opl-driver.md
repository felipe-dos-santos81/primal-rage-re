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

Parse the container: read the `AIL3MDI` magic (offset 0) and the header fields after it; identify where the code body starts and the segment it is loaded at. Determine the origin **from evidence, not assumption** — e.g. by locating a `call`/`jmp` target that lands on a plausible instruction boundary, or by matching a header field against the file's structure — and record the evidence in the tool's docstring and in the Task 4 report. Load the body flat at that origin, disassemble with capstone in 16-bit x86 mode (`Cs(CS_ARCH_X86, CS_MODE_16)`), and emit `(addr, raw_bytes, text)`.

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

### Task 3: Reproduce the note-setup register order

The capture's write 2 is `0xB0 = 0x2b` (a key-on) where the port writes `0x20` (an operator register); `sequencer.c`'s `apply_patch` writes all operators then `0xC0`, then `key_on` writes `0xA0` then `0xB0|0x20`. The driver's order differs and this task matches it.

**Files:**
- Modify: `port/src/platform/audio/sequencer.c` (`apply_patch`, `key_on`, `key_off`)
- Modify: `port/tests/test_sequencer.c`
- Modify: `docs/superpowers/plans/2026-09-18-opl-velocity-tl.md` (extend with the order derivation)

**Interfaces:**
- Consumes: the write seam `opl_write`; Task 2's carrier TL.
- Produces: the per-note register sequence (order and register set) the driver emits, including the `0xB0`/`0x1B0` key-on placement and `0xC0 |= 0x30`.

- [ ] **Step 1: Derive the driver's write order**

From the driver (Task 1's harness) and the capture, establish the exact order of the writes for one note: which operator registers, where the `0xB0` key-on falls relative to them, where `0xC0` is written, and the second-set (`0x1B0`) mirror. Record the driver instructions and a capture sample in the derivation doc.

- [ ] **Step 2: Write the failing test**

In `port/tests/test_sequencer.c`, assert the register order for one keyed note matches the derived sequence (e.g. the index of the `0xB0` key-on write and of the `0xC0` write within the note's write run). Assert on the trace, not audio.

- [ ] **Step 3: Run it and watch it fail**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: FAIL — the port keys on last, after every operator write.

- [ ] **Step 4: Implement the derived order**

Reorder/extend `apply_patch`/`key_on` to match, keeping the `key_off` path correct (a key-off must still clear the key bit on the same `0xB0`). Keep the existing `insert`-time `0xC0 = p[8] | 0x30` unless the driver places it elsewhere.

- [ ] **Step 5: Run the tests and the capture comparison**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`, and the **`capture oracle first difference` line advances beyond C write 2** — or reaches `all compared and matched`. Record the exact new line.

- [ ] **Step 6: Negative control**

Revert the ordering locally and confirm the first-difference line returns to write 2. Restore.

- [ ] **Step 7: Commit**

```bash
git add port/src/platform/audio/sequencer.c port/tests/test_sequencer.c docs/superpowers/plans/2026-09-18-opl-velocity-tl.md
git commit -m "audio: match SBPRO2.MDI's note-setup register order"
```

---

### Task 4: Record the outcome and verify the ladder

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

**Spec coverage:** §2 harness → Task 1. §3/§4 velocity→TL and its captured cases → Task 2. §5 note-setup order and the advancing first-difference line → Task 3. §6 DoD and §7/§8 dispositions and limits → Task 4 Steps 1–3. §1.1 decision 2 (residual named, not tuned) is enforced by Task 2 Step 2's "stop, do not fit" and Task 4 Step 1's write-count disclosure.

**Placeholder scan:** no TBD. Task 1 Step 3's load origin and Tasks 2–3's exact register sequences are genuine RE deliverables, each with a stated method and an evidence requirement — the plan's interface mechanism, not placeholders. The one thing the plan deliberately does not pre-state is the function itself, because inventing it is the failure mode the spec exists to avoid.

**Type consistency:** `mdi_disasm.load`/`MdiImage.disassemble` are used only in Task 1's tests; `opl_write(u16,u8)` and `patches_lookup(u16)` are the existing signatures; `apply_patch`/`key_on`/`key_off` keep their current names and roles across Tasks 2 and 3.
