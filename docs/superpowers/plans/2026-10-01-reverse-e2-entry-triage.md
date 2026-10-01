# E2: triage of the non-Ghidra entry points Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Classify every plausible entry point that no Ghidra function contains (U0's "575", re-derived; 579 under the corrected rule) by how the original reaches it, one evidence line per row, and commit the counted target list that track P consumes, with each row's ported status, live-Ghidra status, P batch and what E1 can verify today.

**Architecture:** One static tool, `tools/entry_triage.py`, over the fixed-up image `build/diffrun --image-out` dumps: it rebuilds the universe, trusts Ghidra's functions plus the entries the evidence proves (to a fixpoint), applies fifteen ordered class rules defined in the record before the tool runs on the image, and renders a markdown table. Additive helpers in `tools/diff_emu.py` reuse E1's capstone decoder and `static_scan`. A `make entry-triage` target (in `make verify`) runs the unit tests and fails when the committed table differs from a fresh run.

**Tech Stack:** Python 3.12, `capstone` 5.0.7 (`tools/requirements-diff.txt`, already installed by E1), `unittest`, the existing `diffrun` binary, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 (track E2: "classifies the 575 candidates as code reached by a jump table, pointer or caller, dead code, or data; one evidence line per row; output is a counted target list"), §6 ("E2: the 575 split into a counted list with evidence per row"), §9 ("Triage in E2 sets the real count before porting starts").

**Derivation record:** `docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md` (§E2.1 the universe and U0's reproduction, §E2.2 the classes, §E2.3 the span tables, §E2.4-§E2.8 the per-row facts, §E2.9 the planning-run figures, §E2.10 the call-stub follow-up, §E2.11 named gaps, Appendix A the live-Ghidra data). Every number in this plan comes from there or from a command this plan runs.

**What the planner ran (scratch, 2026-10-01, against the image at `e9271df`, sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`):** every code block of Tasks 2-6 exactly as written, at each task's stage: the red runs (the failures quoted in each task), the green runs (11, 19, 29, 39 tests), the 33 mutations of `/tmp/e2_mutate.py` with the failing tests quoted per task (none survives), the real-image run (`579 candidates (575 by U0's rule)`, 8.4 s), `make entry-triage` in a full copy of the tree at `e9271df` with every change of this plan applied (exit 0, 19 s; exit 2 with an edited table; the skip line under `python3 -S`), and `make verify` in that copy with the parallel-safe overrides: exit 0, the 45 oracle lines equal to the base, the `make audio-render` WAV identical to `before-t2.wav`, `771 1203 64` / `731 731 100`.

## Decisions needed from the user

**Decided by the user on 2026-10-01:** all recommendations accepted — D1 standardise on 579 (the corrected scan; the `u0` column keeps the 575); D2 do not port the 231 span writers one by one: verify the six dispatchers in table `0x80C8C`; D3 the committed table is a `make verify` gate (every track-P commit that ports a target regenerates it).

1. **D1: which universe the table standardises on.** U0's rule, reconstructed, gives exactly 575; it hides a defect (an entry right after `ret N` is missed because the `00` of `C2 0N 00` is skipped as a zero filler), and the corrected rule gives 579, adding `0x10602`, `0x10604`, `0x36114`, `0x3C87C`, of which `0x10604` and `0x36114` are real unported code (record §E2.1). **Recommendation: 579**, with a `u0` column (575 rows say yes) and `--expect-u0 575` pinned next to `--expect 579`, so the spec's "575" stays checkable. Cost if wrong: choosing 575 drops two real functions from track P's list; choosing 579 when the user wanted 575 costs one line (`plausible` calling `after_ret_u0`) and two pins.
2. **D2: how track P counts the 231 span writers.** They are tiny register-convention fragments (`mov [edi],dl; add edi,eax; ret` runs) reached only through five tables whose dispatchers the port's `sprite.c` re-implements as one blit (its `PORT:` notes name `0x57F80`, `0x57FFB`, `0x5215C`, `0x58CBD`, `0x5D28F`), verified today by the pixel oracles. **Recommendation:** P verifies the six dispatchers the table `0x80C8C` holds (`0x5215C`, `0x57F80`, `0x57FFB`, `0x58CBD`, `0x5D218`, `0x5D28F`) against the original with the writers allow-listed (227 of them are E1 leaves), and does not write 231 C functions the port would never call. Cost if wrong: either 231 dead C functions, or writers that no E input reaches stay named gaps. This plan records both views (the table keeps 231 rows); only P's batch size depends on the answer.
3. **D3: is the committed table a gate?** **Recommendation: yes** (`make entry-triage` in `make verify` fails when a fresh run differs), so a P commit that ports a target must regenerate the table in the same commit and the counts never drift. Cost if wrong: a little friction per P commit (one `--out` run), against a target list that silently goes stale. If the answer is no, Task 6 adds the target without the `verify` line.

## Global Constraints

Copied verbatim from their sources:

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- Spec §8: "`make verify` is the gate after every task." Common brief: the 45 oracle lines (`grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>`) must equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; the `make audio-render` WAV must be byte-identical to `.superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav`; `python3 tools/port_progress.py` prints `771 1203 64` and `731 731 100`. **E2 changes nothing under `port/src`**, so all three hold unchanged.
- AGENTS.md: "Ghidra address == linear address" / "those bytes are pre-fixup ... **use Ghidra (fixups applied) for any data address**, or replicate `mem_load_le` + `mem_load_le_fixups`". The tool reads only the image `build/diffrun --image-out` writes (the loader's own dump, fixups applied).
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." The tool writes only `/tmp` or `tempfile` paths and the committed table.
- Common brief (relaunch): "The committed plan/tools must not depend on a live Ghidra session; record Ghidra evidence as data with the query that produced it." Do not modify the Ghidra program. The live-Ghidra delta is a committed file (Task 1); nothing in this plan calls Ghidra.
- AGENTS.md: "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests, and never assert an unseeded BSS-zero". Python tests are `unittest` in `tools/tests/`, imported with `sys.path.insert(0, ROOT/tools)`.
- E1 Global Constraints: the harness "skips like the other oracles when `unicorn` or `PRAGE.EXE` is absent (spec §5.5): exit 0 with a "skipped:" line, and exit 1 when `PR_ORACLE_REQUIRED=1`." `entry_triage.py` follows this for `capstone`.
- AGENTS.md: "Commit style: `<area>: <what changed>`. **Never `git add -A`** — stage named files." Trailer: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Spec §5.3 / E1 §E.7, for every readiness statement: readiness is a static prediction of what E1 can run, never a verification; only `diff_verify` verdicts are verification.

## Review Focus

1. **A classification fitted to the result.** The rules are committed in the record (§E2.2) in Task 1, before the tool ever runs on the image (Task 5); each rule has a planted candidate in the synthetic world. Pinned by `test_every_planted_candidate_gets_its_class` and `test_the_evidence_names_the_proving_address`; a rule change after Task 1 must change the record in the same commit (Task 5 Step 5 checks).
2. **A universe that is not U0's, unnoticed.** Pinned by `test_the_universe_is_579_and_u0s_rule_gives_575` (the four additions by address), `test_u0s_greedy_rule_misses_the_entry_after_ret_4`, `test_ret_imm16_and_the_byte_level_c3_count` and `--expect 579 --expect-u0 575` in `make entry-triage`.
3. **Table walks that overrun or evidence that goes stale.** A table's slots stop only at the next displacement, and a later-trusted reader can shrink a walk. Pinned by `test_an_entry_whose_evidence_the_final_index_lost_is_stale`, `test_no_entry_is_stale`, `test_the_span_tables` (five tables, `0x81010` read above the runtime cut) and `test_above_the_cut_only_span_code_is_trusted`.
4. **"Ported" or "reached" misread.** Non-Ghidra addresses use the strict header rule (`test_strict_and_loose`); Ghidra ones use `port_progress.py`'s rule (mutation M20 fails `test_e1_readiness`); a candidate inside another body is never a target (`test_batches_and_targets`); the image's first byte never wraps to its last (`test_the_first_bytes_of_the_image_never_wrap_around`).
5. **Readiness read as verification, or the table drifting from the code.** Each readiness category is pinned on a planted body (`test_e1_readiness`), the record states readiness is static (§E2.6), and `make entry-triage --check` fails on any difference (`test_the_counts_the_table_and_the_check`, Task 6 Step 3).

## Where to run

A worktree off `main`, branch `reverse-e2`:

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse
git worktree add .worktrees/reverse-e2 -b reverse-e2 main
cd .worktrees/reverse-e2
ln -s ../../data data && ln -s ../../.superpowers .superpowers
cp ../../port/tests/ghidra_data.bin ../../port/tests/title_screen_ref.ppm port/tests/
ls -d data .superpowers port/tests/ghidra_data.bin port/tests/title_screen_ref.ppm   # all four must exist
python3 -c "import capstone; print(capstone.__version__)"                              # 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -2                          # builds diffrun too
```

If this plan and its record are not on `main` yet, bring them from the planning branch first: `git checkout reverse-plans -- docs/superpowers/plans/2026-10-01-reverse-e2-entry-triage.md docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md`. Every command below runs from the worktree root. Ledger: `.superpowers/sdd/2026-10-01-reverse-e2-entry-triage/progress.md`. Parallel-safe overrides for `make verify` (another worktree may run at the same time): `t=e2; make verify SMK_DUMP=/tmp/pr_${t}_smk TITLE_DUMP=/tmp/pr_${t}_title ATTRACT_DUMP=/tmp/pr_${t}_att FRONTEND_DUMP=/tmp/pr_${t}_fe TITLE_PIN_DIR=/tmp/pr_${t}_pin AUDIO_WAV=/tmp/pr_${t}.wav K11_DUMP=/tmp/pr_${t}_k11 GP_DUMP=/tmp/pr_${t}_gp DIFF_IMAGE=/tmp/pr_${t}_diffimg DIFF_TABLE=/tmp/pr_${t}_diff.md E2_IMAGE=/tmp/pr_${t}_e2img` (the last one exists after Task 6).

## File Structure

| File | Responsibility |
|---|---|
| `docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md` | the record (from the planning branch; Task 7 finalises §E2.9's status) |
| `docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt` (new) | the live-Ghidra delta, extracted from the record's Appendix A |
| `tools/diff_emu.py` | append `decode_at` and `disasm_range` (additive; nothing above them changes) |
| `tools/entry_triage.py` (new) | the universe, the trusted code, the classes, the per-row facts, the table, the CLI |
| `tools/tests/test_entry_triage.py` (new) | the synthetic world (one planted candidate per class), the CLI tests, the real-image pins |
| `docs/superpowers/plans/2026-10-01-reverse-e2-triage.md` (new, generated) | the committed counted target list |
| `Makefile` | `E2_*` variables, the `entry-triage` target, one `.PHONY` entry, one `verify` line |
| `docs/PROGRESS.md`, `AGENTS.md` | one appended paragraph; one command line |

**Appending.** Tasks 3-5 append blocks to `tools/entry_triage.py` and `tools/tests/test_entry_triage.py`, and Task 2 to `tools/diff_emu.py`. Put two blank lines before a block that starts at column 0 and one blank line before a block of indented methods (they continue `class Triage`, the last thing in the file until Task 5). The planner assembled the files exactly this way and ran every test and mutation on the result.

## Shared-file touch points

E2 runs beside track G (U5-U8, U11), so every shared edit is additive:

| file | region | what is added |
|---|---|---|
| `tools/diff_emu.py` | end of file, after `coverage` | `decode_at(image, addr)`, `disasm_range(image, start, end)`; no existing line changes |
| `Makefile` | after `DIFF_TABLE ?=` | `E2_IMAGE ?=`, `E2_TABLE =`, `E2_LIVE =` |
| `Makefile` | `.PHONY` list, last line | ` \` and a new line `        entry-triage` |
| `Makefile` | after the `diff-verify` recipe | the `entry-triage` target |
| `Makefile` | `verify` recipe, after `@$(MAKE) --no-print-directory diff-verify` | `@$(MAKE) --no-print-directory entry-triage` |
| `docs/PROGRESS.md` | end | one paragraph |
| `AGENTS.md` | Commands block, after the `make diff-verify` line | one line |

---

### Task 1: The record's rules and the live-Ghidra data, committed before any code runs on the image

**Files:**
- Add (from the planning branch if needed): `docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md`, `docs/superpowers/plans/2026-10-01-reverse-e2-entry-triage.md`
- Create: `docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt`

**Interfaces:**
- Produces: the class rules (record §E2.2) as the fixed specification Tasks 2-5 implement; the live-delta file Task 4 reads with `load_live` and Task 5 passes as `--live`.

- [ ] **Step 1: Check the record holds the rules this plan implements**

```bash
R=docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md
grep -oE '^\| ([1-9]|1[0-5]) \| [a-z-]+' $R | awk '{print $4}' | tr '\n' ' '; echo
grep -c '^## §E2' $R
```

Expected: `finisher move-callback span-writer call-table jump-table anim-target mid-instruction data code-immediate direct interior data-pointer interior dead unclassified` (rules 1-15 of §E2.2, in order) and `11` (§E2.1-§E2.11).

- [ ] **Step 2: Extract the live-Ghidra delta from Appendix A and check it**

```bash
python3 - <<'PY'
s = open("docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md").read()
body = s.split("## Appendix A")[1].split("```text\n", 1)[1].split("\n```", 1)[0] + "\n"
open("docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt", "w").write(body)
PY
shasum -a 256 docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt
wc -l < docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt
grep -vc '^#' docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt
```

Expected: `fe938f1c2bffcbd0c406e2ea0b706a107678de7ef33f81663a754fc293a69eb7`, `180`, `176` (152 live-only entries + 24 changed bodies, record §E2.8). Any other hash: stop; the record was altered.

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md docs/superpowers/plans/2026-10-01-reverse-e2-entry-triage.md docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt
git commit -m "docs: E2 record (the universe, the class rules) and the live-Ghidra delta

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: The universe (record §E2.1)

**Files:**
- Modify: `tools/diff_emu.py` (append at the end of the file)
- Create: `tools/entry_triage.py`
- Create: `tools/tests/test_entry_triage.py`

**Interfaces:**
- Consumes: `diff_emu.Image`, `diff_emu._decode`, `diff_emu._md`, `diff_emu.capstone`, `diff_emu.cx86`.
- Produces: `diff_emu.decode_at(image, addr) -> capstone insn | None`, `diff_emu.disasm_range(image, start, end) -> list`; `entry_triage.load_ghidra(path) -> [(entry, size)]`, `load_ported(src) -> (strict, loose)`, `load_live(path) -> {entry: [(lo, hi)]}`; `Triage(image, ghidra, ported_strict=(), ported_ghidra=(), live=None)` with `ins`, `u8`, `u16`, `u32`, `in_ghidra`, `direct_target`, `ret_before`, `filler_before`, `after_ret`, `after_ret_u0`, `plausible`, `candidates() -> sorted list` (sets `self.dwords`); the constants `CODE_LO CODE_HI DATA_LO RUNTIME_BASE FILLERS MAX_FILL FINISHER_TABLES FINISHER_SLOTS MOVE_TABLE MOVE_STRIDE MOVE_CHARS MOVE_REACTIONS ANIM_CODE_OPS SPAN_BLIT VOICE_FN MAX_TABLE CLASS_ORDER NOT_TARGET BATCH BATCHES`. The test module's `World`, `build_world`, `EXPECTED`, `world_triage`, `le32`, `rel32` are used by Tasks 3-5.

- [ ] **Step 1: Write the failing tests**

Create `tools/tests/test_entry_triage.py` with exactly this content (Tasks 3-5 append to it):

```python
"""Unit tests for tools/entry_triage.py (record docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md).

The synthetic tests build one hand-assembled image (WORLD) with one candidate per class; every byte
sequence is listed with its assembly. The real-image tests run the tool on the image
`build/diffrun --image-out` writes and pin the universe and the class counts (record §E2.9).
"""
import collections
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import diff_emu as E
import entry_triage as T

REQUIRED = os.environ.get("PR_ORACLE_REQUIRED") == "1"
needs_capstone = unittest.skipUnless(E.capstone is not None or REQUIRED, "capstone not installed")


def le32(v):
    return (v & 0xFFFFFFFF).to_bytes(4, "little")


def rel32(at, target, op=b"\xe8"):
    """call/jmp rel32 at `at` to `target` (E8/E9 + displacement from the next instruction)."""
    return op + le32(target - (at + len(op) + 4))


class World:
    """An image from 0x10000 to 0xC0000 (the code object, the runtime cut at 0x5D000, the data object
    from 0x80000, the move table 0xA3528 and the finisher tables 0xBDAE4/0xBDB00 all inside it)."""

    def __init__(self):
        self.data = bytearray(0xB0000)
        self.ghidra = []

    def put(self, addr, b):
        self.data[addr - 0x10000:addr - 0x10000 + len(b)] = b

    def fn(self, addr, b):
        """A Ghidra function: its bytes, listed as (entry, size)."""
        self.put(addr, b)
        self.ghidra.append((addr, len(b)))

    def entry(self, addr, b):
        """A body after a `ret` byte."""
        self.put(addr - 1, b"\xc3")
        self.put(addr, b)

    def image(self):
        return E.Image(bytes(self.data))


XOR_RET = bytes.fromhex("31C0C3")                    # xor eax,eax; ret


def build_world():
    w = World()
    # G1 10000: mov dword [eax+0x1c],0x10200 / mov eax,[0x10300] / call 0x10400 / jge 0x10500 / ret
    g1 = bytearray(bytes.fromhex("C7401C") + le32(0x10200) + b"\xa1" + le32(0x10300))
    g1 += rel32(0x10000 + len(g1), 0x10400)
    g1 += rel32(0x10000 + len(g1), 0x10500, b"\x0f\x8d") + b"\xc3"
    w.fn(0x10000, bytes(g1))
    # the span blit 51E5C: call dword [eax*4+0x80100]; ret
    w.fn(T.SPAN_BLIT, bytes.fromhex("FF1485") + le32(0x80100) + b"\xc3")
    # 11000: call dword [eax*4+0x80200]; ret                         (a call table outside the span code)
    w.fn(0x11000, bytes.fromhex("FF1485") + le32(0x80200) + b"\xc3")
    # 11100: jmp dword [eax*4+0x80300]; ret                          (a jump table)
    w.fn(0x11100, bytes.fromhex("FF2485") + le32(0x80300) + b"\xc3")
    w.fn(T.VOICE_FN, b"\xc3")                          # the voice entry, a ported leaf
    w.fn(0x18000, bytes.fromhex("ECC3"))               # in al,dx; ret
    # 18100: xor eax,eax / ret / inc eax (18103, after a ret but inside this Ghidra function) /
    # mov eax,0x5D030 (an operand above the runtime cut) / ret
    w.fn(0x18100, bytes.fromhex("31C0C340B8") + le32(0x5D030) + b"\xc3")
    w.put(0x90078, le32(0x18103))
    w.entry(0x5D030, XOR_RET)
    w.entry(0x10200, XOR_RET)                          # code-immediate
    w.entry(0x10300, bytes.fromhex("40C3"))            # data: G1 reads it as a memory operand
    w.entry(0x10400, XOR_RET)                          # direct: G1 calls it
    w.entry(0x10500, XOR_RET)                          # interior: G1's jge lands on it
    w.entry(0x12000, XOR_RET)                          # finisher
    w.put(0xBDAE4, le32(0x12000))
    w.put(0x80400, le32(0x12000))                      # and an aligned dword: the finisher rule wins
    # 12100 move callback (char 2, reaction 5): call 0x13000 / mov dword [eax+0x10],0x13100 /
    # mov edi,0x9090C390 (1210C; its immediate's C3 is at 1210E) / mov ebx,eax / xor eax,eax (12113) / ret
    mc = bytearray(rel32(0x12100, 0x13000) + bytes.fromhex("C74010") + le32(0x13100))
    mc += b"\xbf" + bytes.fromhex("90C39090") + bytes.fromhex("89C3") + bytes.fromhex("31C0") + b"\xc3"
    w.entry(0x12100, bytes(mc))
    w.put(T.MOVE_TABLE + (2 * 64 + 5) * 20, le32(0x12100))
    w.put(0x80500, le32(0x1210F))                      # mid-instruction: inside the mov edi,imm at 1210C
    w.put(0x80504, le32(0x12113))                      # interior: the xor after `89 C3`
    w.put(0x13000, XOR_RET)                            # helper: called only from 12100
    w.entry(0x13100, XOR_RET)                          # stored by 12100's immediate
    # 14000 span writer: call dword [eax*4+0x80180]; ret              (a second-level span table)
    w.entry(0x14000, bytes.fromhex("FF1485") + le32(0x80180) + b"\xc3")
    w.put(0x80100, le32(0x14000))
    w.entry(0x14100, XOR_RET)
    w.put(0x80180, le32(0x14100))
    # 5D020, above the runtime cut, is slot 1 of the span table 80100:
    # mov eax,[0x80108] (ends the table at its slot 2 once 5D020 is trusted) / call dword [eax*4+0x80600] / ret
    w.entry(0x5D020, b"\xa1" + le32(0x80108) + bytes.fromhex("FF1485") + le32(0x80600) + b"\xc3")
    w.put(0x80104, le32(0x5D020))
    w.entry(0x5D040, XOR_RET)                          # slot 2 until 5D020 is trusted: a stale entry
    w.put(0x80108, le32(0x5D040))
    w.entry(0x5D060, XOR_RET)                          # above the cut in a call table that is not span code
    w.put(0x80204, le32(0x5D060))
    w.fn(0x18200, rel32(0x18200, 0x5D080) + b"\xc3")  # call 0x5D080 (above the cut); ret
    w.entry(0x5D080, XOR_RET)
    w.entry(0x14200, XOR_RET)                          # span writer through 5D020's table
    w.put(0x80600, le32(0x14200))
    # 15000 call table: call 0x2C3FC; ret                              (a voice site, a ported callee)
    w.entry(0x15000, rel32(0x15000, T.VOICE_FN) + b"\xc3")
    w.put(0x80200, le32(0x15000))
    # 15100 jump table: call 0x18000; ret                              (`in` in its call tree)
    w.entry(0x15100, rel32(0x15100, 0x18000) + b"\xc3")
    w.put(0x80300, le32(0x15100))
    # 16000 animation target: the word D100 at 90000, the dword at 90002; body xor eax,eax; ret 4
    w.entry(0x16000, bytes.fromhex("31C0C20400"))
    w.put(0x90000, bytes.fromhex("00D1") + le32(0x16000))
    w.entry(0x16100, XOR_RET)                          # the 0x1F prefix form: DF11, an operand word, the dword
    w.put(0x90010, bytes.fromhex("11DF3412") + le32(0x16100))
    w.entry(0x16200, XOR_RET)                          # D200 is opcode 0x12, not a code opcode: dead
    w.put(0x90020, bytes.fromhex("00D2") + le32(0x16200))
    w.entry(0x16300, XOR_RET)                          # B100 is mode 0x2000; a rel32 in untrusted bytes
    w.put(0x90030, bytes.fromhex("00B1") + le32(0x16300))
    w.put(0x17000, rel32(0x17000, 0x16300))
    w.entry(0x16400, XOR_RET)                          # data-pointer: an aligned dword only
    w.put(0x90040, le32(0x16400))
    w.entry(0x16500, bytes.fromhex("8BC031C0C3"))      # starts with the filler mov eax,eax: data
    w.put(0x90044, le32(0x16500))
    w.entry(0x16600, bytes.fromhex("40DDC8"))          # inc eax, then undecodable DD C8: data
    w.put(0x90048, le32(0x16600))
    # 16900 after `ret 4` (C2 04 00): U0's greedy rule eats the 00 as a filler and misses it
    w.put(0x168FD, bytes.fromhex("C20400"))
    w.put(0x16900, XOR_RET)
    w.put(0x90064, le32(0x16900))
    # 17200, reached only by the rel32 at 17020 (untrusted bytes): xor eax,eax / mov ebx,eax /
    # inc eax (17204, after the C3 of `89 C3`) / call 0x2C3FC (17205, a voice site) / ret
    w.entry(0x17200, bytes.fromhex("31C089C340") + rel32(0x17205, T.VOICE_FN) + b"\xc3")
    w.put(0x17020, rel32(0x17020, 0x17200))
    w.put(0x90061, le32(0x17204))                      # an unaligned dword only
    # 16B00: the word 5100 has mode 0x4000 and opcode 0x11 but not the command bit 0x8000: dead
    w.entry(0x16B00, XOR_RET)
    w.put(0x90070, bytes.fromhex("0051") + le32(0x16B00))
    # not candidates
    w.entry(0x16A00, XOR_RET)                          # its only dword is in the code object (17100)
    w.put(0x17100, le32(0x16A00))
    w.put(0x166FF, b"\x41")
    w.put(0x16700, XOR_RET)                            # no ret before it
    w.put(0x9004C, le32(0x16700))
    w.put(0x90050, le32(0x10002))                      # inside G1
    w.entry(0x5D010, XOR_RET)                          # at or above RUNTIME_BASE
    w.put(0x90054, le32(0x5D010))
    w.entry(0x16800, bytes.fromhex("DDC8C3"))          # its first instruction does not decode
    w.put(0x90058, le32(0x16800))
    w.put(0x17010, rel32(0x17010, T.VOICE_FN))         # a voice call in untrusted bytes, in no body
    return w


EXPECTED = {
    0x10200: "code-immediate", 0x10300: "data", 0x10400: "direct", 0x10500: "interior",
    0x12000: "finisher", 0x12100: "move-callback", 0x1210F: "mid-instruction", 0x12113: "interior",
    0x14000: "span-writer", 0x14100: "span-writer", 0x14200: "span-writer", 0x15000: "call-table",
    0x15100: "jump-table", 0x16000: "anim-target", 0x16100: "anim-target", 0x16200: "dead",
    0x16300: "unclassified", 0x16400: "data-pointer", 0x16500: "data", 0x16600: "data",
    0x16900: "data-pointer", 0x16B00: "dead", 0x17204: "interior",
}


def world_triage(w=None):
    w = w or build_world()
    return T.Triage(w.image(), sorted(w.ghidra), {0x10200}, {T.VOICE_FN})


@needs_capstone
class UniverseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.t = world_triage()
        cls.cands = cls.t.candidates()

    def test_the_universe_is_exactly_the_planted_candidates(self):
        # 13100 is stored by 12100's immediate (not Ghidra code) and 5D020 is above the cut: not in it
        self.assertEqual(self.cands, sorted(EXPECTED))

    def test_each_exclusion_rule_bites(self):
        for a in (0x16700, 0x10002, 0x5D010, 0x16800, 0x13100, 0x5D020, 0x16A00, 0x18103, 0x5D030):
            self.assertNotIn(a, self.cands, "%X" % a)

    def test_u0s_greedy_rule_misses_the_entry_after_ret_4(self):
        self.assertTrue(self.t.after_ret(0x16900))
        self.assertFalse(self.t.after_ret_u0(0x16900))
        self.assertTrue(self.t.after_ret_u0(0x10200))

    def test_fillers_are_skipped_backwards_in_order(self):
        w = World()
        w.put(0x10000, bytes.fromhex("C3" "8D4000" "90" "8BC0" "0000"))
        t = T.Triage(w.image(), [])
        self.assertTrue(t.after_ret(0x10009))          # 00 00, 8B C0, 90, 8D 40 00, then C3
        self.assertTrue(t.after_ret_u0(0x10009))
        w.put(0x10010, bytes.fromhex("C3" "8D4000"))
        t = T.Triage(w.image(), [])
        self.assertTrue(t.after_ret(0x10014))          # 8D 40 00 must be tried before 00
        self.assertTrue(t.after_ret_u0(0x10014))

    def test_ret_imm16_and_the_byte_level_c3_count(self):
        w = World()
        w.put(0x10010, bytes.fromhex("C20400" "90"))
        w.put(0x10020, bytes.fromhex("89C3"))          # mov ebx,eax: its C3 byte admits 0x10022 (§E2.1)
        t = T.Triage(w.image(), [])
        self.assertTrue(t.after_ret(0x10013))
        self.assertTrue(t.after_ret(0x10014))
        self.assertFalse(t.after_ret_u0(0x10013))      # U0's rule ate the 00 of `ret 4`
        self.assertFalse(t.after_ret_u0(0x10014))
        self.assertTrue(t.after_ret(0x10022))
        self.assertFalse(t.after_ret(0x10021))

    def test_the_first_bytes_of_the_image_never_wrap_around(self):
        w = World()
        w.put(0x10000, b"\x00\x90")
        w.data[-1] = 0xC3                              # a C3 at the image's last byte
        t = T.Triage(w.image(), [])
        self.assertEqual(t.u8(0xFFFF), -1)
        self.assertFalse(t.after_ret(0x10002))
        self.assertFalse(t.after_ret_u0(0x10002))

    def test_more_than_max_fill_bytes_of_filler_is_not_after_a_ret(self):
        w = World()
        w.put(0x10100, b"\xc3" + b"\x90" * (T.MAX_FILL + 2))
        t = T.Triage(w.image(), [])
        self.assertFalse(t.after_ret(0x10101 + T.MAX_FILL + 2))
        self.assertTrue(t.after_ret(0x10101 + T.MAX_FILL))


class LoaderTests(unittest.TestCase):
    def test_strict_and_loose(self):
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, "a.c"), "w") as f:
                f.write("/* 0x12345 — record */\nvoid f(void) { fn_register(0x23456u, g); x(); /* 0x34567 */ }\n")
            with open(os.path.join(d, "symbols.h"), "w") as f:
                f.write("/* 0x45678 */\n")
            strict, loose = T.load_ported(d)
        self.assertEqual(strict, {0x12345, 0x23456})
        self.assertEqual(loose, {0x12345, 0x23456, 0x34567})

    def test_ghidra_rows_skip_the_image_noise(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "f.csv")
            with open(p, "w") as f:
                f.write("entry,size,name,n_callers,n_callees,decompiled\n00010024,16,F,1,0,ok\n"
                        ".image::00062e89,1,G,0,0,ok\n00010010,8,H,0,0,ok\n")
            self.assertEqual(T.load_ghidra(p), [(0x10010, 8), (0x10024, 16)])

    def test_the_live_delta(self):
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, "live.txt")
            with open(p, "w") as f:
                f.write("# entry,ranges\n000186d0,00018625-000186c0;000186d0-00018712\n00010034,00010034-00010034\n")
            self.assertEqual(T.load_live(p), {0x186D0: [(0x18625, 0x186C0), (0x186D0, 0x18712)],
                                              0x10034: [(0x10034, 0x10034)]})


@needs_capstone
class DiffEmuHelperTests(unittest.TestCase):
    def test_decode_at_and_disasm_range(self):
        img = E.Image(bytes.fromhex("31C0" "40" "C3" "DDC8"))   # xor eax,eax / inc eax / ret / undecodable
        self.assertEqual(E.decode_at(img, 0x10000).mnemonic, "xor")
        self.assertIsNone(E.decode_at(img, 0x10004))
        self.assertEqual([i.address for i in E.disasm_range(img, 0x10000, 0x10006)], [0x10000, 0x10002, 0x10003])
```

- [ ] **Step 2: Run them and see them fail**

Run: `python3 -m unittest tools.tests.test_entry_triage 2>&1 | tail -4`
Expected: `ModuleNotFoundError: No module named 'entry_triage'`, `Ran 1 test`, `FAILED (errors=1)`.

- [ ] **Step 3: Append the decoding helpers to `tools/diff_emu.py`**

Append at the end of the file (after `coverage`):

```python
# ---- decoding helpers for the static tools (E2 tools/entry_triage.py); additive, used by nothing above --

def decode_at(image, addr):
    """The one instruction at `addr` (a capstone instruction with details), or None if undecodable."""
    return _decode(image.bytes_at(addr, 15), addr)


def disasm_range(image, start, end):
    """Linear decode of [start, end): the capstone instructions, stopping at the first undecodable byte."""
    return list(_md.disasm(image.bytes_at(start, end - start), start))
```

- [ ] **Step 4: Create `tools/entry_triage.py`**

```python
#!/usr/bin/env python3
"""E2: triage of the entry candidates Ghidra never listed (spec 2026-09-30-reverse-completion-design
§4 E2, §6; record docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md).

The universe (record §E2.1; U0 §U0.12's "575 plausible entries"): every address V that
  (a) equals a 4-byte little-endian value at ANY byte offset of the data object [DATA_LO, image end),
      or an immediate or a memory displacement of an instruction of a Ghidra function (a linear
      decode of [entry, entry + size) for every row of port/decomp/prage.functions.csv without '::'
      whose entry is in the code object);
  (b) lies in [CODE_LO, RUNTIME_BASE) and in no Ghidra function's [entry, entry + size);
  (c) follows a `ret` (C3 just before, or C2 three bytes before), directly or after FILLERS
      skipped backwards (at most MAX_FILL bytes), the ret tested before each filler is skipped;
  (d) decodes as one instruction.
U0's own rule (after_ret_u0) skipped the fillers greedily before testing, so the 00 that ends
`ret 4` (C2 04 00) was eaten as a filler: every row says whether U0's rule admits it (§E2.1).
Every candidate gets exactly one class: the first rule of CLASS_ORDER whose evidence exists (§E2.2).

  tools/entry_triage.py --image IMG [--functions CSV] [--src DIR] [--live FILE]
                        [--out TABLE.md | --check TABLE.md] [--expect N] [--expect-u0 N]
"""
import argparse
import bisect
import collections
import csv
import glob
import os
import re
import sys

import diff_emu as de

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CODE_LO = 0x10000          # AGENTS.md: code object 0x10000-0x73B14
CODE_HI = 0x73B14
DATA_LO = 0x80000          # AGENTS.md: data object 0x80000-0x10B0CF
RUNTIME_BASE = 0x5D000     # tools/port_progress.py: WATCOM libc and DOS/4GW glue from here up
# The fillers between functions (§E2.1): nop, `mov eax,eax` (E1 record §E.5, 0x19066),
# `lea eax,[eax+0]`, and zero fill (k7-k12 §1.2, 0x475C0). Order matters: 8D 40 00 before 00.
FILLERS = (b"\x90", b"\x8b\xc0", b"\x8d\x40\x00", b"\x00")
MAX_FILL = 16
FINISHER_TABLES = (0xBDAE4, 0xBDB00)          # U0 §U0.4: one dword per character
FINISHER_SLOTS = 7
MOVE_TABLE, MOVE_STRIDE, MOVE_CHARS, MOVE_REACTIONS = 0xA3528, 20, 7, 64   # U0 §U0.12: callback at +0
ANIM_CODE_OPS = (0x10, 0x11, 0x15)            # 0x2B2A0's opcodes that call through DS_00105BD4 (actors.c)
SPAN_BLIT = 0x51E5C                           # sprite.c: the renderer's span blit (§E2.3)
VOICE_FN = 0x2C3FC                            # k7-k12 §0.2: the voice entry every voice site calls
MAX_TABLE = 512

CLASS_ORDER = ("finisher", "move-callback", "span-writer", "call-table", "jump-table", "anim-target",
               "mid-instruction", "data", "code-immediate", "direct", "interior", "data-pointer",
               "dead", "unclassified")
NOT_TARGET = ("mid-instruction", "data", "interior", "dead", "unclassified")
BATCH = {"finisher": "finishers", "move-callback": "callbacks", "span-writer": "span-writers",
         "anim-target": "animation-targets"}
BATCHES = ("callbacks", "finishers", "voice", "animation-targets", "span-writers", "other")


def load_ghidra(path):
    """Sorted (entry, size) of the Ghidra functions (rows without '::', the .image noise)."""
    with open(path) as f:
        return sorted((int(r["entry"], 16), int(r["size"])) for r in csv.DictReader(f)
                      if "::" not in r["entry"])


def load_ported(src):
    """(strict, loose). strict: an fn_register(0xADDR or a `/* 0xADDR` comment at column 0 (§E2.4);
    loose: tools/port_progress.py's rule (a `/* 0xADDR` anywhere on a line, or fn_register)."""
    strict, loose = set(), set()
    for ext in ("c", "h"):
        for f in glob.glob(os.path.join(src, "**", "*." + ext), recursive=True):
            if f.endswith("symbols.h"):
                continue
            with open(f, errors="ignore") as fh:
                t = fh.read()
            reg = {int(m, 16) for m in re.findall(r"fn_register\(\s*0x([0-9A-Fa-f]+)", t)}
            strict |= reg | {int(m, 16) for m in re.findall(r"^/\* 0x([0-9A-Fa-f]{4,6})\b", t, re.M)}
            loose |= reg | {int(m, 16) for m in re.findall(r"/\* 0x([0-9A-Fa-f]{4,6})\b", t)}
    return strict, loose


def load_live(path):
    """The live Ghidra project's functions that differ from the committed export (§E2.8): one line per
    function, `entry,lo-hi[;lo-hi...]` in hex (the body's address ranges, inclusive)."""
    live = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            e, rs = line.split(",")
            live[int(e, 16)] = [tuple(int(x, 16) for x in r.split("-")) for r in rs.split(";")]
    return live


class Triage:
    def __init__(self, image, ghidra, ported_strict=(), ported_ghidra=(), live=None):
        self.img = image
        self.ghidra = list(ghidra)
        self.gstart = [s for s, _ in self.ghidra]
        self.gentries = set(self.gstart)
        self.ported_strict = set(ported_strict)
        self.ported_ghidra = set(ported_ghidra)
        self.live = dict(live or {})
        self._dec = {}
        self._rel32 = None
        self._closure = {}

    # ---- bytes and instructions
    def ins(self, a):
        if a not in self._dec:
            self._dec[a] = de.decode_at(self.img, a)
        return self._dec[a]

    def u8(self, a):
        """The byte at a, or -1 outside the image (never Python's wrap-around index)."""
        return self.img.data[a - self.img.base] if self.img.contains(a) else -1

    def u16(self, a):
        return int.from_bytes(self.img.bytes_at(a, 2), "little")

    def u32(self, a):
        return int.from_bytes(self.img.bytes_at(a, 4), "little")

    def in_ghidra(self, a):
        i = bisect.bisect_right(self.gstart, a) - 1
        return i >= 0 and a < self.ghidra[i][0] + self.ghidra[i][1]

    @staticmethod
    def direct_target(ins):
        if ins is not None and ins.operands and ins.operands[0].type == de.cx86.X86_OP_IMM and (
                ins.group(de.capstone.CS_GRP_JUMP) or ins.group(de.capstone.CS_GRP_CALL)
                or ins.mnemonic in ("loop", "loope", "loopne")):
            return ins.operands[0].imm & 0xFFFFFFFF
        return None

    # ---- the universe (§E2.1)
    def ret_before(self, a):
        return self.u8(a - 1) == 0xC3 or self.u8(a - 3) == 0xC2

    def filler_before(self, a):
        for f in FILLERS:
            if self.img.contains(a - len(f), len(f)) and self.img.bytes_at(a - len(f), len(f)) == f:
                return len(f)
        return 0

    def after_ret(self, v):
        """The ret is tested at v and again after each filler skipped backwards (§E2.1 (c))."""
        a = v
        while v - a <= MAX_FILL:
            if self.ret_before(a):
                return True
            n = self.filler_before(a)
            if not n:
                return False
            a -= n
        return False

    def after_ret_u0(self, v):
        """U0's rule, reconstructed (§E2.1): skip every filler first, then test the ret once."""
        a = v
        while v - a <= MAX_FILL:
            n = self.filler_before(a)
            if not n:
                break
            a -= n
        return self.ret_before(a)

    def plausible(self, v):
        return (CODE_LO <= v < RUNTIME_BASE and not self.in_ghidra(v) and self.after_ret(v)
                and self.ins(v) is not None)

    def candidates(self):
        self.dwords = collections.defaultdict(list)       # value -> data-object addresses (any offset)
        d, base = self.img.data, self.img.base
        for off in range(max(DATA_LO - base, 0), len(d) - 3):
            v = int.from_bytes(d[off:off + 4], "little")
            if CODE_LO <= v < RUNTIME_BASE:
                self.dwords[v].append(base + off)
        operands = set()
        for s, n in self.ghidra:
            if CODE_LO <= s < CODE_HI:
                for ins in de.disasm_range(self.img, s, s + n):
                    for op in ins.operands:
                        if op.type == de.cx86.X86_OP_IMM:
                            operands.add(op.imm & 0xFFFFFFFF)
                        elif op.type == de.cx86.X86_OP_MEM:
                            operands.add(op.mem.disp & 0xFFFFFFFF)
        return sorted(v for v in set(self.dwords) | operands if self.plausible(v))
```

- [ ] **Step 5: Run the tests and see them pass**

Run: `python3 -m unittest tools.tests.test_entry_triage 2>&1 | tail -3`
Expected: `Ran 11 tests`, `OK`. Also run `python3 -m unittest tools.tests.test_diff_emu 2>&1 | tail -1`: `OK` (27 tests; nothing above the appended helpers changed).

- [ ] **Step 6: Commit**

```bash
git add tools/diff_emu.py tools/entry_triage.py tools/tests/test_entry_triage.py
git commit -m "tools: entry_triage universe (U0's 575 reconstructed, the corrected ret rule) and diff_emu decode helpers

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 7: Mutation proofs**

Save the runner below as `/tmp/e2_mutate.py` (it is a scratch tool, never committed; Tasks 3-5 reuse it). It applies one mutation of `tools/entry_triage.py` at a time, runs the suite, prints the tests that fail and restores the file:

```python
"""E2 mutation proofs (plan 2026-10-01-reverse-e2-entry-triage): apply one mutation of
tools/entry_triage.py at a time, run tools.tests.test_entry_triage, print the tests that fail, restore.
Usage: python3 /tmp/e2_mutate.py M1,M2,...   (from the worktree root; a mutation whose text is not in
the file yet is reported as `absent`)."""
import os
import re
import subprocess
import sys

P = "tools/entry_triage.py"
M = [
 ("M1 any-step ret test removed", "            if self.ret_before(a):\n                return True\n            n = self.filler_before(a)\n            if not n:\n                return False\n            a -= n\n        return False",
  "            n = self.filler_before(a)\n            if not n:\n                break\n            a -= n\n        return self.ret_before(a)"),
 ("M2 u8 guard removed", "return self.img.data[a - self.img.base] if self.img.contains(a) else -1", "return self.img.data[a - self.img.base]"),
 ("M3 filler order", 'FILLERS = (b"\\x90", b"\\x8b\\xc0", b"\\x8d\\x40\\x00", b"\\x00")', 'FILLERS = (b"\\x90", b"\\x8b\\xc0", b"\\x00", b"\\x8d\\x40\\x00")'),
 ("M4 MAX_FILL bound", "        while v - a <= MAX_FILL:\n            if self.ret_before(a):", "        while v - a <= MAX_FILL + 4:\n            if self.ret_before(a):"),
 ("M5 dwords from the whole image", "for off in range(max(DATA_LO - base, 0), len(d) - 3):", "for off in range(0, len(d) - 3):"),
 ("M6 no Ghidra operands", "        return sorted(v for v in set(self.dwords) | operands if self.plausible(v))", "        return sorted(v for v in set(self.dwords) if self.plausible(v))"),
 ("M7 no runtime cut", "        return (CODE_LO <= v < RUNTIME_BASE and not self.in_ghidra(v) and self.after_ret(v)\n                and self.ins(v) is not None)\n\n    def candidates", "        return (CODE_LO <= v < CODE_HI and not self.in_ghidra(v) and self.after_ret(v)\n                and self.ins(v) is not None)\n\n    def candidates"),
 ("M8 Ghidra bodies not excluded", "        return (CODE_LO <= v < RUNTIME_BASE and not self.in_ghidra(v) and self.after_ret(v)", "        return (CODE_LO <= v < RUNTIME_BASE and self.after_ret(v)"),
 ("M9 no finisher table", "        if v in self.fin:\n            return \"finisher\"", "        if False:\n            return \"finisher\""),
 ("M10 command bit not required", "if not (w & 0x8000) or (w & 0x6000) != 0x4000:", "if (w & 0x6000) != 0x4000:"),
 ("M11 no prefix form", "            if prefix and op == 0x1F and (w & 0xFF) in ANIM_CODE_OPS:", "            if False:"),
 ("M12 no span fixpoint", "            if more <= span:\n                break", "            if True:\n                break"),
 ("M13 no slot trust", "        elif v in self.slots and self.entry_like(v):", "        elif False:"),
 ("M14 no mid-instruction rule", "        a = self.covering(v)\n        if a is not None:", "        a = None\n        if a is not None:"),
 ("M15 untrusted ignores trusted sites", "                             and not any(a in self.trusted for a in s)}", "                             }"),
 ("M16 no untrusted interior rule", "        if x is not None and x != v:", "        if False:"),
 ("M17 switch bound off by one", "n = (c.operands[1].imm & 0xFF if c.operands[1].size == 1 else c.operands[1].imm) + 1", "n = (c.operands[1].imm & 0xFF if c.operands[1].size == 1 else c.operands[1].imm) + 2"),
 ("M18 ret N ignored", "        if retn:\n            return \"stack-args\"", "        if False:\n            return \"stack-args\""),
 ("M19 unmodeled not a blocker", "                if unmodeled:\n                    blocker", "                if False:\n                    blocker"),
 ("M20 port_progress rule ignored for Ghidra", "return a in self.ported_ghidra if a in self.gentries else a in self.ported_strict", "return a in self.ported_strict"),
 ("M21 voice for non-targets", "voice = self.body_facts(self.bodies[v])[1] if target else []", "voice = self.body_facts(self.bodies[v])[1]"),
 ("M22 no voice batch", 'BATCH.get(c) or ("voice" if voice else "other")', 'BATCH.get(c) or "other"'),
 ("M23 check never fails", "            if f.read() != text:", "            if False:"),
 ("M24 expect ignored", "    if a.expect is not None and len(rows) != a.expect:", "    if False:"),
 ("M25 live ignored", '        if v in self.live:\n            return "entry"', '        if False:\n            return "entry"'),
 ("M26 u0 column is the corrected rule", "u0=self.after_ret_u0(v),", "u0=self.after_ret(v),"),
 ("M28 data-pointer needs alignment", "aligned = [p for p in self.dwords.get(v, ()) if p % 4 == 0]", "aligned = list(self.dwords.get(v, ()))"),
 ("M29 starts_with_fill off", "        if self.starts_with_fill(v):", "        if False:"),
 ("M30 truncated scan not data", "        if self.bodies[v].truncated:", "        if False:"),
 ("M31 stale check off", "        self.stale = sorted(e for e in self.entries if self.reason(e) is None)", "        self.stale = []"),
 ("M32 slot trust above the cut for any table", "            if v < RUNTIME_BASE or any(s[1] in self.span_tables for s in self.slots[v]):", "            if True:"),
 ("M33 calls above the cut trusted", "        if v in self.calls and CODE_LO <= v < RUNTIME_BASE and not self.in_ghidra(v):", "        if v in self.calls and CODE_LO <= v < CODE_HI and not self.in_ghidra(v):"),
 ("M34 reason not recomputed for the supplement", 'why=self.reason(e) or "stale: " + self.entries[e]', 'why=self.entries[e]'),
]

want = sys.argv[1].split(",")
orig = open(P).read()
try:
    for name, a, b in M:
        if name.split()[0] not in want:
            continue
        if orig.count(a) != 1:
            print(name, "-> absent")
            continue
        with open(P, "w") as f:
            f.write(orig.replace(a, b))
        r = subprocess.run([sys.executable, "-m", "unittest", "tools.tests.test_entry_triage"],
                           capture_output=True, text=True, env=dict(os.environ))
        fails = sorted(set(re.findall(r"^(?:FAIL|ERROR): (\w+)", r.stderr, re.M)))
        print(name, "->", ", ".join(fails) if fails else "NOT CAUGHT")
finally:
    with open(P, "w") as f:
        f.write(orig)
```

Run: `python3 /tmp/e2_mutate.py M1,M2,M3,M4,M5,M6,M7,M8; git diff --exit-code tools/entry_triage.py`
Expected, exactly (the last command prints nothing and exits 0):

```
M1 any-step ret test removed -> test_ret_imm16_and_the_byte_level_c3_count, test_the_universe_is_exactly_the_planted_candidates, test_u0s_greedy_rule_misses_the_entry_after_ret_4
M2 u8 guard removed -> test_the_first_bytes_of_the_image_never_wrap_around
M3 filler order -> test_fillers_are_skipped_backwards_in_order
M4 MAX_FILL bound -> test_more_than_max_fill_bytes_of_filler_is_not_after_a_ret
M5 dwords from the whole image -> test_each_exclusion_rule_bites, test_the_universe_is_exactly_the_planted_candidates
M6 no Ghidra operands -> test_the_universe_is_exactly_the_planted_candidates
M7 no runtime cut -> test_each_exclusion_rule_bites, test_the_universe_is_exactly_the_planted_candidates
M8 Ghidra bodies not excluded -> test_each_exclusion_rule_bites, test_the_universe_is_exactly_the_planted_candidates
```

---

### Task 3: The trusted code and the classes (record §E2.2, §E2.3, §E2.5)

**Files:**
- Modify: `tools/entry_triage.py` (append to the end: the block continues `class Triage`)
- Modify: `tools/tests/test_entry_triage.py` (append to the end)

**Interfaces:**
- Consumes: Task 2's `Triage`, `World`, `build_world`, `EXPECTED`, `world_triage`.
- Produces: `Triage.load_tables()` (sets `fin`, `mcb`), `entry_like`, `index()` (sets `imm disp rel calls readers slots span_tables tstarts`), `switch_targets`, `scan(e) -> StaticInfo` (memoised in `self.bodies`), `trust`, `build(cands)` (sets `cands cset trusted owner bodies entries stale`), `reason(v) -> str | None`, `anim_word`, `table_class(v) -> (cls, evidence) | None`, `covering`, `starts_with_fill`, `rel32_anywhere(v) -> [site]`, `untrusted() -> {X: [site]}` (sets `uentries`, `uowner`), `classify(v) -> (cls, evidence)`.

- [ ] **Step 1: Write the failing tests**

Append to `tools/tests/test_entry_triage.py`:

```python
def classified(w=None):
    t = world_triage(w)
    cands = t.candidates()
    t.load_tables()
    t.build(cands)
    return t, {v: t.classify(v) for v in cands}


@needs_capstone
class ClassTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.t, cls.got = classified()

    def test_every_planted_candidate_gets_its_class(self):
        self.assertEqual({a: c for a, (c, _) in self.got.items()}, EXPECTED)

    def test_the_evidence_names_the_proving_address(self):
        ev = {a: e for a, (_, e) in self.got.items()}
        self.assertEqual(ev[0x12000], "dword BDAE4")
        self.assertEqual(ev[0x12100], "dword A3F8C (char 2, reaction 0x05)")
        self.assertEqual(ev[0x14000], "dword 80100 = table 80100[0], read by `call` at 51E5C")
        self.assertEqual(ev[0x14100], "dword 80180 = table 80180[0], read by `call` at 14000")
        self.assertEqual(ev[0x14200], "dword 80600 = table 80600[0], read by `call` at 5D025")
        self.assertEqual(ev[0x15100], "dword 80300 = table 80300[0], read by `jmp` at 11100")
        self.assertEqual(ev[0x16000], "dword 90002 after the opcode word D100 at 90000")
        self.assertEqual(ev[0x16100], "dword 90014 after the opcode word DF11 at 90010")
        self.assertEqual(ev[0x1210F], "inside the instruction at 1210C (code of 12100)")
        self.assertEqual(ev[0x12113], "an instruction of the code of 12100")
        self.assertEqual(ev[0x10500], "`jge` at 10011 in the code of 10000")
        self.assertEqual(ev[0x10400], "`call` at 1000C")
        self.assertEqual(ev[0x10300], "memory operand of the instruction at 10007")
        self.assertEqual(ev[0x10200], "immediate of the instruction at 10000")
        self.assertEqual(ev[0x16500], "filler at 16500 (8bc031)")
        self.assertEqual(ev[0x16600], "undecodable bytes on the scan from 16600")
        self.assertEqual(ev[0x16400], "aligned dword 90040")
        self.assertEqual(ev[0x17204], "an instruction of the scan from 17200, which only the rel32 at 17020 "
                                      "(untrusted bytes) reaches")
        self.assertEqual(ev[0x16300], "rel32 at 17000 outside the trusted code; unaligned dwords 90032")
        self.assertTrue(ev[0x16200].startswith("no rel32 at any code offset"), ev[0x16200])

    def test_the_table_classes_win_over_a_plain_aligned_dword(self):
        # 0x12000 is also the aligned dword at 0x80400; without the finisher table it is a data pointer
        w = build_world()
        w.put(0xBDAE4, le32(0))
        self.assertEqual(classified(w)[1][0x12000][0], "data-pointer")

    def test_a_span_table_read_above_the_runtime_cut_is_found(self):
        # 5D020 is trusted as a slot of the span table 80100 although it lies above RUNTIME_BASE
        self.assertIn(0x5D020, self.t.entries)
        self.assertEqual(self.t.span_tables, {0x80100, 0x80180, 0x80600})

    def test_an_entry_whose_evidence_the_final_index_lost_is_stale(self):
        self.assertEqual(self.t.stale, [0x5D040])
        self.assertIsNone(self.t.reason(0x5D040))
        self.assertEqual(self.t.entries[0x5D040], "slot 80108 of table 80100, read by `call` at 51E5C")

    def test_above_the_cut_only_span_code_is_trusted(self):
        self.assertNotIn(0x5D060, self.t.entries)      # slot 80204 of the call table 80200
        self.assertNotIn(0x5D080, self.t.entries)      # called by the Ghidra function 18200
        self.assertIn(0x5D060, self.t.slots)
        self.assertIn(0x5D080, self.t.calls)

    def test_the_untrusted_entries(self):
        self.assertEqual(self.t.untrusted(), {0x16300: [0x17000], 0x17200: [0x17020]})


@needs_capstone
class SwitchTests(unittest.TestCase):
    def test_a_bounded_switch_extends_the_body_and_an_unbounded_one_stays_indirect(self):
        w = World()
        # 10000: cmp al,1 / ja 0x10010 / and eax,0xff / jmp dword [eax*4+0x80000]; 10010: ret
        w.put(0x10000, bytes.fromhex("3C01" "770C" "25FF000000" "FF2485") + le32(0x80000))
        w.put(0x10010, b"\xc3")
        w.put(0x80000, le32(0x10020) + le32(0x10030) + le32(0x10040))
        w.put(0x10020, bytes.fromhex("31C0C3"))
        w.put(0x10030, bytes.fromhex("40C3"))
        w.put(0x10040, bytes.fromhex("48C3"))          # case 2: beyond `cmp al,1`, not part of the body
        # 10100: the same jmp with no guard
        w.put(0x10100, bytes.fromhex("FF2485") + le32(0x80000))
        t = T.Triage(w.image(), [])
        t.bodies = {}
        body = t.scan(0x10000)
        self.assertIn(0x10020, body.insns)
        self.assertIn(0x10030, body.insns)
        self.assertNotIn(0x10040, body.insns)
        self.assertEqual(body.indirect, [])
        self.assertEqual(t.scan(0x10100).indirect, [0x10100])
```

- [ ] **Step 2: Run them and see them fail**

Run: `python3 -m unittest tools.tests.test_entry_triage 2>&1 | grep -E "Error:|^Ran|FAILED"`
Expected: `AttributeError: 'Triage' object has no attribute 'load_tables'`, `AttributeError: 'Triage' object has no attribute 'scan'`, `Ran 12 tests`, `FAILED (errors=2)` (the `ClassTests` class setup counts as one error).

- [ ] **Step 3: Append the classes to `tools/entry_triage.py`**

Append to the end of the file; the block is indented four spaces because it continues `class Triage`:

```python
    # ---- the data tables (§E2.2 rules 1-2)
    def load_tables(self):
        self.fin = collections.defaultdict(list)
        for T in FINISHER_TABLES:
            for i in range(FINISHER_SLOTS):
                if self.u32(T + 4 * i):
                    self.fin[self.u32(T + 4 * i)].append(T + 4 * i)
        self.mcb = collections.defaultdict(list)
        for c in range(MOVE_CHARS):
            for r in range(MOVE_REACTIONS):
                e = MOVE_TABLE + (c * MOVE_REACTIONS + r) * MOVE_STRIDE
                if self.u32(e):
                    self.mcb[self.u32(e)].append((e, c, r))

    # ---- trusted code (§E2.2): Ghidra's functions, then the entries the evidence proves, to a fixpoint
    def entry_like(self, v):
        """An entry outside Ghidra anywhere in the code object (the RUNTIME_BASE cut is the universe's,
        not the trusted code's: span code continues above it, §E2.3)."""
        return (CODE_LO <= v < CODE_HI and not self.in_ghidra(v) and self.after_ret(v)
                and self.ins(v) is not None)

    def index(self):
        self.imm, self.disp, self.rel, self.calls = (collections.defaultdict(list) for _ in range(4))
        self.readers = collections.defaultdict(list)
        for a in sorted(self.trusted):
            ins = self.trusted[a]
            t = self.direct_target(ins)
            if t is not None:
                (self.calls if ins.mnemonic == "call" else self.rel)[t].append(a)
                continue
            for op in ins.operands:
                if op.type == de.cx86.X86_OP_IMM:
                    self.imm[op.imm & 0xFFFFFFFF].append(a)
                elif op.type == de.cx86.X86_OP_MEM:
                    T = op.mem.disp & 0xFFFFFFFF
                    self.disp[T].append(a)
                    if ins.mnemonic in ("call", "jmp") and op.mem.index != 0 and op.mem.scale == 4:
                        self.readers[T].append((ins.mnemonic, a))
        self.slots = collections.defaultdict(list)       # value -> (slot, table, index, kind, reader)
        for T, rs in sorted(self.readers.items()):
            kind, a = min(rs, key=lambda r: r[1])
            p = T
            while (self.img.contains(p, 4) and CODE_LO <= self.u32(p) < CODE_HI and p - T < 4 * MAX_TABLE
                   and (p == T or p not in self.disp)):
                self.slots[self.u32(p)].append((p, T, (p - T) // 4, kind, a))
                p += 4
        span = {T for T, rs in self.readers.items()
                if any(k == "call" and self.owner[a] == SPAN_BLIT for k, a in rs)}
        while True:                                      # a table read inside a span writer is a span table
            wcode = {a for v, ss in self.slots.items() if v in self.bodies and any(s[1] in span for s in ss)
                     for a in self.bodies[v].insns}
            more = {T for T, rs in self.readers.items() if any(k == "call" and a in wcode for k, a in rs)}
            if more <= span:
                break
            span |= more
        self.span_tables = span
        self.tstarts = sorted(self.trusted)

    def switch_targets(self, insns, a):
        """The case targets of the switch `jmp dword ptr [reg*4 + T]` at a, bounded by its own
        `cmp <reg>, N; ja` (N + 1 cases) among the five instructions before it; None without that guard."""
        ins = self.ins(a)
        mem = [op for op in ins.operands if op.type == de.cx86.X86_OP_MEM]
        if ins.mnemonic != "jmp" or not mem or mem[0].mem.index == 0 or mem[0].mem.scale != 4:
            return None
        for b in sorted(x for x in insns if a - 24 <= x < a)[-5:]:
            c = self.ins(b)
            if (c.mnemonic == "cmp" and len(c.operands) == 2 and c.operands[0].type == de.cx86.X86_OP_REG
                    and c.operands[1].type == de.cx86.X86_OP_IMM):
                T = mem[0].mem.disp & 0xFFFFFFFF
                n = (c.operands[1].imm & 0xFF if c.operands[1].size == 1 else c.operands[1].imm) + 1
                if not self.img.contains(T, 4 * n):
                    return None
                return [self.u32(T + 4 * k) for k in range(n)]
        return None

    def scan(self, e):
        """static_scan from e, extended through every bounded switch (§E2.2): the body P would port."""
        if e in self.bodies:
            return self.bodies[e]
        info = de.static_scan(self.img, e)
        insns, leaders, indirect = dict(info.insns), set(info.leaders), []
        truncated, unresolved = info.truncated, list(info.unresolved)
        work, seen = list(info.indirect), set()
        while work:
            a = work.pop()
            if a in seen:
                continue
            seen.add(a)
            targets = self.switch_targets(insns, a)
            if targets is None:
                indirect.append(a)
                continue
            for t in targets:
                sub = de.static_scan(self.img, t)
                for x, n in sub.insns.items():
                    insns.setdefault(x, n)
                leaders |= set(sub.leaders)
                truncated = truncated or sub.truncated
                unresolved += sub.unresolved
                work += sub.indirect
        self.bodies[e] = de.StaticInfo(sorted(leaders), insns, sorted(set(indirect)), sorted(set(unresolved)),
                                       truncated)
        return self.bodies[e]

    def trust(self, e):
        for a in sorted(self.scan(e).insns):
            if a not in self.trusted:
                self.trusted[a] = self.ins(a)
                self.owner[a] = e

    def build(self, cands):
        self.cands = cands
        self.cset = set(cands)
        self.trusted, self.owner, self.bodies = {}, {}, {}
        for s, n in self.ghidra:
            if CODE_LO <= s < CODE_HI:
                for ins in de.disasm_range(self.img, s, s + n):
                    self.trusted[ins.address] = ins
                    self.owner[ins.address] = s
        self.entries = {}                                # trusted non-Ghidra entry -> how it is reached
        while True:
            self.index()
            pool = (self.cset | set(self.slots) | set(self.calls) | set(self.imm)) - set(self.entries)
            added = {}
            for v in sorted(pool):
                why = self.reason(v)
                if why:
                    added[v] = why
            if not added:
                break
            self.entries.update(added)
            for e in sorted(added):
                self.trust(e)
        # a table walk can shrink as more code is trusted (a newly trusted reader ends it): an entry
        # whose evidence the final index no longer holds is stale and is reported, never dropped
        self.stale = sorted(e for e in self.entries if self.reason(e) is None)
        for v in cands:
            self.scan(v)

    def reason(self, v):
        """Why v is a trusted entry, from the current index (§E2.2), or None."""
        if v in self.cset:
            hit = self.table_class(v)
            if hit:
                return hit[1]
        elif v in self.slots and self.entry_like(v):
            p, T, k, kind, a = self.slots[v][0]
            if v < RUNTIME_BASE or any(s[1] in self.span_tables for s in self.slots[v]):
                return "slot %X of table %X, read by `%s` at %X" % (p, T, kind, a)
        if v in self.calls and CODE_LO <= v < RUNTIME_BASE and not self.in_ghidra(v):
            return "called at %X" % self.calls[v][0]
        if v in self.imm and self.plausible(v):
            return "immediate at %X" % self.imm[v][0]
        return None

    # ---- the classes (§E2.2), first match wins
    def anim_word(self, p):
        """The opcode word that makes the dword at p an animation code pointer, or None (§E2.2 rule 6)."""
        for at, prefix in ((p - 2, False), (p - 4, True)):
            if not self.img.contains(at, 2):
                continue
            w = self.u16(at)
            if not (w & 0x8000) or (w & 0x6000) != 0x4000:
                continue
            op = (w >> 8) & 0x1F
            if not prefix and op in ANIM_CODE_OPS:
                return at, w
            if prefix and op == 0x1F and (w & 0xFF) in ANIM_CODE_OPS:
                return at, w
        return None

    def table_class(self, v):
        if v in self.fin:
            return "finisher", "dword %s" % " ".join("%X" % p for p in self.fin[v])
        if v in self.mcb:
            return "move-callback", " ".join("dword %X (char %d, reaction 0x%02X)" % x for x in self.mcb[v])
        if v in self.slots:
            p, T, k, kind, a = self.slots[v][0]
            c = "span-writer" if T in self.span_tables else ("call-table" if kind == "call" else "jump-table")
            return c, "dword %X = table %X[%d], read by `%s` at %X" % (p, T, k, kind, a)
        for p in self.dwords.get(v, ()):
            w = self.anim_word(p)
            if w:
                return "anim-target", "dword %X after the opcode word %04X at %X" % (p, w[1], w[0])
        return None

    def covering(self, v):
        """The trusted instruction that holds v strictly inside it, or None."""
        i = bisect.bisect_right(self.tstarts, v) - 1
        while i >= 0 and self.tstarts[i] > v - 16:
            a = self.tstarts[i]
            if a < v < a + self.trusted[a].size:
                return a
            i -= 1
        return None

    def starts_with_fill(self, v):
        """True when v begins with a filler (a zero filler needs two zero bytes: `add [eax],al`)."""
        return any(self.img.bytes_at(v, len(f)) == f for f in FILLERS[:-1]) or self.img.bytes_at(v, 2) == b"\x00\x00"

    def rel32_anywhere(self, v):
        """Byte level: every E8/E9/0F 8x rel32 at any offset below RUNTIME_BASE that lands on v."""
        if self._rel32 is None:
            self._rel32 = collections.defaultdict(list)
            d, base = self.img.data, self.img.base
            for o in range(0, min(RUNTIME_BASE - base - 5, len(d) - 6)):
                b = d[o]
                if b in (0xE8, 0xE9):
                    t = base + o + 5 + int.from_bytes(d[o + 1:o + 5], "little", signed=True)
                elif b == 0x0F and 0x80 <= d[o + 1] <= 0x8F:
                    t = base + o + 6 + int.from_bytes(d[o + 2:o + 6], "little", signed=True)
                else:
                    continue
                self._rel32[t].append(base + o)
        return self._rel32.get(v, [])

    def untrusted(self):
        """Code outside the trusted set that only untrusted bytes reach (§E2.5): every rel32 target X
        below RUNTIME_BASE that is a plausible entry, not trusted, and whose rel32 sites are all outside
        the trusted instructions -> those sites; and self.uowner, each instruction of their scans -> the
        first such X (in address order)."""
        if not hasattr(self, "uentries"):
            self.rel32_anywhere(0)
            self.uentries = {t: s for t, s in sorted(self._rel32.items())
                             if t not in self.trusted and self.plausible(t)
                             and not any(a in self.trusted for a in s)}
            self.uowner = {}
            for x in self.uentries:
                for a in sorted(de.static_scan(self.img, x).insns):
                    self.uowner.setdefault(a, x)
        return self.uentries

    def classify(self, v):
        hit = self.table_class(v)
        if hit:
            return hit
        a = self.covering(v)
        if a is not None:
            return "mid-instruction", "inside the instruction at %X (code of %X)" % (a, self.owner[a])
        if v in self.disp:
            return "data", "memory operand of the instruction at %X" % self.disp[v][0]
        if self.starts_with_fill(v):
            return "data", "filler at %X (%s)" % (v, self.img.bytes_at(v, 3).hex())
        if self.bodies[v].truncated:
            return "data", "undecodable bytes on the scan from %X" % v
        if v in self.imm:
            return "code-immediate", "immediate of the instruction at %X" % self.imm[v][0]
        if v in self.calls:
            return "direct", "`call` at %X" % self.calls[v][0]
        if v in self.trusted and self.owner[v] != v:
            return "interior", "an instruction of the code of %X" % self.owner[v]
        if v in self.rel:
            a = self.rel[v][0]
            return "interior", "`%s` at %X in the code of %X" % (self.trusted[a].mnemonic, a, self.owner[a])
        aligned = [p for p in self.dwords.get(v, ()) if p % 4 == 0]
        if aligned:
            return "data-pointer", "aligned dword %s" % " ".join("%X" % p for p in aligned[:4])
        self.untrusted()
        x = self.uowner.get(v)
        if x is not None and x != v:
            return "interior", "an instruction of the scan from %X, which only the rel32 at %s (untrusted bytes) reaches" % (
                x, " ".join("%X" % s for s in self.uentries[x][:4]))
        refs = " ".join("%X" % p for p in self.dwords.get(v, ())[:4])
        far = self.rel32_anywhere(v)
        if not far:
            return "dead", "no rel32 at any code offset, no immediate, no aligned dword; unaligned dwords %s" % refs
        return "unclassified", "rel32 at %s outside the trusted code; unaligned dwords %s" % (
            " ".join("%X" % x for x in far[:4]), refs)
```

- [ ] **Step 4: Run the tests and see them pass**

Run: `python3 -m unittest tools.tests.test_entry_triage 2>&1 | tail -3`
Expected: `Ran 19 tests`, `OK`.

- [ ] **Step 5: Commit**

```bash
git add tools/entry_triage.py tools/tests/test_entry_triage.py
git commit -m "tools: entry_triage trusted code, tables and the fifteen class rules (record E2 §E2.2)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 6: Mutation proofs**

Run: `python3 /tmp/e2_mutate.py M9,M10,M11,M12,M13,M14,M15,M16,M17,M28,M29,M30,M31,M32,M33; git diff --exit-code tools/entry_triage.py`
Expected, exactly:

```
M9 no finisher table -> test_every_planted_candidate_gets_its_class, test_the_evidence_names_the_proving_address
M10 command bit not required -> test_every_planted_candidate_gets_its_class
M11 no prefix form -> test_every_planted_candidate_gets_its_class, test_the_evidence_names_the_proving_address
M12 no span fixpoint -> test_a_span_table_read_above_the_runtime_cut_is_found, test_every_planted_candidate_gets_its_class
M13 no slot trust -> test_a_span_table_read_above_the_runtime_cut_is_found, test_an_entry_whose_evidence_the_final_index_lost_is_stale, test_every_planted_candidate_gets_its_class, test_the_evidence_names_the_proving_address
M14 no mid-instruction rule -> test_every_planted_candidate_gets_its_class, test_the_evidence_names_the_proving_address
M15 untrusted ignores trusted sites -> test_the_untrusted_entries
M16 no untrusted interior rule -> test_every_planted_candidate_gets_its_class, test_the_evidence_names_the_proving_address
M17 switch bound off by one -> test_a_bounded_switch_extends_the_body_and_an_unbounded_one_stays_indirect
M28 data-pointer needs alignment -> test_every_planted_candidate_gets_its_class, test_the_evidence_names_the_proving_address
M29 starts_with_fill off -> test_every_planted_candidate_gets_its_class, test_the_evidence_names_the_proving_address
M30 truncated scan not data -> test_every_planted_candidate_gets_its_class, test_the_evidence_names_the_proving_address
M31 stale check off -> test_an_entry_whose_evidence_the_final_index_lost_is_stale
M32 slot trust above the cut for any table -> test_above_the_cut_only_span_code_is_trusted
M33 calls above the cut trusted -> test_above_the_cut_only_span_code_is_trusted
```

---

### Task 4: The per-row facts: ported, live, size, batch, E1 readiness, supplement, voice (record §E2.4-§E2.8)

**Files:**
- Modify: `tools/entry_triage.py` (append to the end: continues `class Triage`)
- Modify: `tools/tests/test_entry_triage.py` (append to the end)

**Interfaces:**
- Consumes: Task 3's `Triage`.
- Produces: `Triage.is_ported`, `live_status(v) -> "entry" | "body of X" | "-"`, `next_boundary`, `body_facts(info) -> (calls, voice_sites, ret_n, unmodeled)`, `closure(e) -> (callees, blocker)`, `readiness(e) -> "stack-args" | "stubs (...)" | "callees (...)" | "allow-list" | "leaf"`, `run() -> [row]` where a row is `dict(addr, size, cls, evidence, u0, live, ported, batch, e1, voice)`, `supplement() -> [dict(addr, why, live, ported, e1, voice)]`, `voice_placement(rows, supp) -> [dict(site, entry, kind, ported)]`.

- [ ] **Step 1: Write the failing tests**

Append to `tools/tests/test_entry_triage.py`:

```python
LIVE = {0x12000: [(0x12000, 0x12002)], 0x103F0: [(0x103F0, 0x10410)]}


@needs_capstone
class RowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        w = build_world()
        cls.t = T.Triage(w.image(), sorted(w.ghidra), {0x10200}, {T.VOICE_FN}, LIVE)
        cls.rows = {r["addr"]: r for r in cls.t.run()}
        cls.supp = {s["addr"]: s for s in cls.t.supplement()}

    def test_run_keeps_the_classes(self):
        self.assertEqual({a: r["cls"] for a, r in self.rows.items()}, EXPECTED)

    def test_batches_and_targets(self):
        b = {a: r["batch"] for a, r in self.rows.items()}
        self.assertEqual((b[0x12000], b[0x12100], b[0x14000], b[0x16000]),
                         ("finishers", "callbacks", "span-writers", "animation-targets"))
        self.assertEqual((b[0x15000], b[0x10400], b[0x16900]), ("voice", "other", "other"))
        for a in (0x10300, 0x10500, 0x1210F, 0x12113, 0x16200, 0x16300, 0x16500, 0x16600, 0x17204):
            self.assertEqual(b[a], "-", "%X" % a)
            self.assertEqual(self.rows[a]["e1"], "-", "%X" % a)

    def test_the_voice_column_names_the_site_for_targets_only(self):
        self.assertEqual(self.rows[0x15000]["voice"], [0x15000])
        self.assertEqual(self.rows[0x17204]["voice"], [])      # its scan reaches 17205's call: not a target

    def test_the_u0_column(self):
        self.assertFalse(self.rows[0x16900]["u0"])
        self.assertEqual(sum(1 for r in self.rows.values() if r["u0"]), len(EXPECTED) - 1)

    def test_size_runs_to_the_next_candidate_or_ghidra_entry(self):
        self.assertEqual(self.rows[0x10500]["size"], 0x11000 - 0x10500)   # the Ghidra function 11000
        self.assertEqual(self.rows[0x12000]["size"], 0x12100 - 0x12000)   # the candidate 12100

    def test_the_live_column_is_evidence_only(self):
        self.assertEqual(self.rows[0x12000]["live"], "entry")
        self.assertEqual(self.rows[0x10400]["live"], "body of 103F0")
        self.assertEqual(self.rows[0x10200]["live"], "-")
        self.assertEqual(self.rows[0x12000]["cls"], "finisher")

    def test_the_supplement_holds_the_helper_the_stored_entry_and_the_span_code(self):
        self.assertEqual({a: s["why"] for a, s in self.supp.items()},
                         {0x13000: "called at 12100", 0x13100: "immediate at 12105",
                          0x5D020: "slot 80104 of table 80100, read by `call` at 51E5C",
                          0x5D040: "stale: slot 80108 of table 80100, read by `call` at 51E5C"})

    def test_e1_readiness(self):
        e1 = {a: r["e1"] for a, r in self.rows.items()}
        self.assertEqual(e1[0x10200], "leaf")
        self.assertEqual(e1[0x15000], "allow-list")
        self.assertEqual(e1[0x12100], "callees (13000)")
        self.assertEqual(e1[0x14000], "stubs (indirect at 14000 in 14000)")
        self.assertEqual(e1[0x15100], "stubs (in in 18000)")
        self.assertEqual(e1[0x16000], "stack-args")

    def test_ported_rows(self):
        self.assertTrue(self.rows[0x10200]["ported"])
        self.assertFalse(self.rows[0x10400]["ported"])

    def test_voice_sites_are_placed(self):
        v = {x["site"]: (x["entry"], x["kind"]) for x in self.t.voice_placement(list(self.rows.values()),
                                                                             list(self.supp.values()))}
        self.assertEqual(v, {0x15000: (0x15000, "row"), 0x17205: (0x17200, "untrusted"), 0x17010: (None, "-")})
```

- [ ] **Step 2: Run them and see them fail**

Run: `python3 -m unittest tools.tests.test_entry_triage 2>&1 | grep -E "Error:|^Ran|FAILED"`
Expected: `AttributeError: 'Triage' object has no attribute 'run'`, `Ran 19 tests`, `FAILED (errors=1)`.

- [ ] **Step 3: Append the per-row facts to `tools/entry_triage.py`**

Append to the end of the file (four-space indent, still `class Triage`):

```python
    # ---- per-row facts (§E2.4-§E2.8)
    def is_ported(self, a):
        """A Ghidra function by port_progress's rule; any other address by the strict rule (§E2.4)."""
        return a in self.ported_ghidra if a in self.gentries else a in self.ported_strict

    def live_status(self, v):
        """Evidence only, never a class (§E2.8): `entry` when the live project has a function at v,
        `body of X` when v is inside the body of the live function X, else `-`."""
        if v in self.live:
            return "entry"
        for e in sorted(self.live):
            if any(lo <= v <= hi for lo, hi in self.live[e]):
                return "body of %X" % e
        return "-"

    def next_boundary(self, v):
        """The first Ghidra entry or candidate above v, else RUNTIME_BASE: the row's size bound."""
        nxt = [RUNTIME_BASE]
        i = bisect.bisect_right(self.gstart, v)
        if i < len(self.gstart):
            nxt.append(self.gstart[i])
        j = bisect.bisect_right(self.cands, v)
        if j < len(self.cands):
            nxt.append(self.cands[j])
        return min(nxt)

    def body_facts(self, info):
        calls, voice, retn, unmodeled = set(), [], False, set()
        for a in sorted(info.insns):
            ins = self.ins(a)
            t = self.direct_target(ins)
            if ins.mnemonic in ("call", "jmp") and t == VOICE_FN:
                voice.append(a)
            if ins.mnemonic == "call" and t is not None:
                calls.add(t)
            if ins.mnemonic == "ret" and ins.operands:
                retn = True
            if ins.mnemonic in de.UNMODELED_MNEMONICS:
                unmodeled.add(ins.mnemonic)
        return calls, voice, retn, sorted(unmodeled)

    def closure(self, e):
        """The direct-call tree under e, breadth first in address order: (callees, blocker). The blocker
        names the nearest thing in the tree the emulator cannot run unaided (§E2.6): an unmodeled
        instruction, an indirect call or jump that is not a bounded switch, or a truncated scan, with
        the function that holds it."""
        if e in self._closure:
            return self._closure[e]
        seen, queue, blocker = {e}, collections.deque([e]), None
        while queue:
            f = queue.popleft()
            info = self.scan(f)
            calls, _, _, unmodeled = self.body_facts(info)
            if blocker is None:
                if unmodeled:
                    blocker = "%s in %X" % (unmodeled[0], f)
                elif info.indirect:
                    blocker = "indirect at %X in %X" % (info.indirect[0], f)
                elif info.truncated:
                    blocker = "truncated scan in %X" % f
            for t in sorted(calls):
                if CODE_LO <= t < CODE_HI and t not in seen:
                    seen.add(t)
                    queue.append(t)
        seen.discard(e)
        self._closure[e] = (seen, blocker)
        return self._closure[e]

    def readiness(self, e):
        """What E1 can do with the entry today (§E2.6), first match:
        stack-args  the entry returns with `ret N` (E1 has no stack-argument binding, E1 §E.6.3);
        stubs       something in its call tree cannot run in the emulator (named);
        callees     every callee in its tree runs, but one below RUNTIME_BASE is not ported yet (port it
                    first, or stub it);
        allow-list  every callee in its tree is ported: E1's allow-list runs it today;
        leaf        no call at all."""
        _, _, retn, _ = self.body_facts(self.scan(e))
        if retn:
            return "stack-args"
        tree, blocker = self.closure(e)
        if blocker:
            return "stubs (%s)" % blocker
        unported = sorted(t for t in tree if t < RUNTIME_BASE and not self.is_ported(t))
        if unported:
            return "callees (%s)" % " ".join("%X" % t for t in unported[:4])
        return "allow-list" if tree else "leaf"

    def run(self):
        cands = self.candidates()
        self.load_tables()
        self.build(cands)
        rows = []
        for v in cands:
            c, ev = self.classify(v)
            target = c not in NOT_TARGET
            voice = self.body_facts(self.bodies[v])[1] if target else []
            batch = "-" if not target else BATCH.get(c) or ("voice" if voice else "other")
            rows.append(dict(addr=v, size=self.next_boundary(v) - v, cls=c, evidence=ev,
                             u0=self.after_ret_u0(v), live=self.live_status(v), ported=self.is_ported(v),
                             batch=batch, e1=self.readiness(v) if target else "-", voice=voice))
        return rows

    def supplement(self):
        """Trusted entries outside the universe and outside Ghidra (§E2.5): helpers and stored callbacks."""
        out = []
        for e in sorted(set(self.entries) - self.cset):
            voice = self.body_facts(self.scan(e))[1]
            out.append(dict(addr=e, why=self.reason(e) or "stale: " + self.entries[e], live=self.live_status(e),
                            ported=self.is_ported(e),
                            e1=self.readiness(e), voice=voice))
        return out

    def voice_placement(self, rows, supp):
        """Every rel32 call/jmp to VOICE_FN outside the Ghidra functions, placed in the body of a target
        row, a supplement entry or an untrusted entry, or nowhere (§E2.7)."""
        self.untrusted()
        owners = ([(r["addr"], "row") for r in rows if r["batch"] != "-"]
                  + [(s["addr"], "supplement") for s in supp]
                  + [(x, "untrusted") for x in self.uentries])
        out = []
        for s in self.rel32_anywhere(VOICE_FN):
            if self.in_ghidra(s):
                continue
            where = None
            for e, kind in owners:
                body = self.scan(e) if kind != "untrusted" else de.static_scan(self.img, e)
                if s in body.insns:
                    where = (e, kind)
                    break
            out.append(dict(site=s, entry=where[0] if where else None, kind=where[1] if where else "-",
                            ported=self.is_ported(where[0]) if where else False))
        return out
```

- [ ] **Step 4: Run the tests and see them pass**

Run: `python3 -m unittest tools.tests.test_entry_triage 2>&1 | tail -3`
Expected: `Ran 29 tests`, `OK`.

- [ ] **Step 5: Commit**

```bash
git add tools/entry_triage.py tools/tests/test_entry_triage.py
git commit -m "tools: entry_triage rows: ported, live Ghidra, batch, E1 readiness, supplement and voice placement

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 6: Mutation proofs**

Run: `python3 /tmp/e2_mutate.py M18,M19,M20,M21,M22,M25,M26,M34; git diff --exit-code tools/entry_triage.py`
Expected, exactly:

```
M18 ret N ignored -> test_e1_readiness
M19 unmodeled not a blocker -> test_e1_readiness
M20 port_progress rule ignored for Ghidra -> test_e1_readiness
M21 voice for non-targets -> test_the_voice_column_names_the_site_for_targets_only
M22 no voice batch -> test_batches_and_targets
M25 live ignored -> test_the_live_column_is_evidence_only
M26 u0 column is the corrected rule -> test_the_u0_column
M34 reason not recomputed for the supplement -> test_the_supplement_holds_the_helper_the_stored_entry_and_the_span_code
```

---

### Task 5: The CLI, the committed table and the real-image pins (record §E2.9)

**Files:**
- Modify: `tools/entry_triage.py` (append to the end: module level, after `class Triage`)
- Modify: `tools/tests/test_entry_triage.py` (append to the end)
- Create (generated): `docs/superpowers/plans/2026-10-01-reverse-e2-triage.md`

**Interfaces:**
- Consumes: Task 4's `Triage.run/supplement/voice_placement/untrusted/stale`.
- Produces: `entry_triage.summary(rows)`, `hexes(xs)`, `render_table(t, rows, supp, voice) -> str`, `main(argv) -> int`; the CLI `tools/entry_triage.py --image IMG [--functions CSV] [--src DIR] [--live FILE] [--out T | --check T] [--expect N] [--expect-u0 N]` (exit 1 on a `--check` difference or a wrong count; skip line and exit 0 without `capstone`, exit 1 then under `PR_ORACLE_REQUIRED=1`).

- [ ] **Step 1: Write the failing tests**

Append to `tools/tests/test_entry_triage.py`:

```python
@needs_capstone
class CliTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        d = self.tmp.name
        w = build_world()
        self.img = os.path.join(d, "image.bin")
        with open(self.img, "wb") as f:
            f.write(bytes(w.data))
        self.csv = os.path.join(d, "functions.csv")
        with open(self.csv, "w") as f:
            f.write("entry,size,name,n_callers,n_callees,decompiled\n")
            for s, n in sorted(w.ghidra):
                f.write("%08x,%d,FUN_%08x,0,0,ok\n" % (s, n, s))
        self.src = os.path.join(d, "src")
        os.mkdir(self.src)
        with open(os.path.join(self.src, "a.c"), "w") as f:
            f.write("/* 0x10200 — record */\n/* 0x2C3FC — record */\n")
        self.out = os.path.join(d, "table.md")

    def tearDown(self):
        self.tmp.cleanup()

    def main(self, *extra):
        import contextlib
        import io
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            rc = T.main(["--image", self.img, "--functions", self.csv, "--src", self.src] + list(extra))
        return rc, buf.getvalue()

    def test_the_counts_the_table_and_the_check(self):
        rc, text = self.main("--out", self.out, "--expect", "23", "--expect-u0", "22")
        self.assertEqual(rc, 0, text)
        self.assertIn("entry-triage: 23 candidates (22 by U0's rule)", text)
        with open(self.out) as f:
            table = f.read()
        self.assertIn("| 15000 | 256 | call-table | dword 80200 = table 80200[0], read by `call` at 11000 | yes | - "
                      "| no | voice | allow-list | 15000 |", table)
        self.assertIn("| 16900 | 512 | data-pointer | aligned dword 90064 | no | - | no | other | leaf | - |", table)
        self.assertIn("| 10200 | 256 | code-immediate | immediate of the instruction at 10000 | yes | - | yes | other "
                      "| leaf | - |", table)
        self.assertIn("| 17205 | 17200 | untrusted | no |", table)
        self.assertIn("| 17200 | 17020 | - | no |", table)
        self.assertEqual(self.main("--check", self.out)[0], 0)
        with open(self.out, "a") as f:
            f.write("edited\n")
        rc, text = self.main("--check", self.out)
        self.assertEqual(rc, 1)
        self.assertIn("differs from a fresh run", text)

    def test_a_wrong_expected_count_fails(self):
        self.assertEqual(self.main("--expect", "22")[0], 1)
        self.assertEqual(self.main("--expect-u0", "23")[0], 1)


DIFFRUN = os.path.join(ROOT, "build", "diffrun")
EXE = os.path.join(os.environ.get("PR_GAME_DIR", os.path.join(ROOT, "data", "game", "C")), "PRAGE.EXE")
LIVE_FILE = os.path.join(ROOT, "docs", "superpowers", "plans", "2026-10-01-reverse-e2-live-functions.txt")
U0_CALLBACKS = {int(x, 16) for x in (
    "14EF8 14F50 15478 21114 21374 22938 22A00 231C0 23208 237D0 2381C 3C048 3D10C 3D1EC 3DADC 3DB34 "
    "3DCEC 3F0A8 475EC 47608 47624 47720 47874 47FCC 48608 48964 489A0").split()}
U0_FINISHERS = {0x1567C, 0x15908, 0x23BF8, 0x23EC0, 0x402FC, 0x45D14}
U0_VOICE = {int(x, 16) for x in (
    "11A3D 11C38 14F46 14F9E 154DD 1550B 155EA 156CA 15780 15802 158CD 15956 1599B 212AF 2236D 223EF "
    "224E2 2260E 228EF 2292B 22AA7 231FB 23243 23810 2385C 23BD8 23E9D 23F05 24001 2406D 24214 242DA "
    "24317 243CF 243ED 2455E 295FD 342EF 3440D 3448B 34517 345A3 3462F 3D12D 3D395 3D730 3DAC5 3DB2A "
    "3DB82 3F0C1 4012D 40137 402B1 402E0 41880 45C8C 45D0A 475D9 4779D 478C7 47DF7 47E1B 48518 48548 "
    "4B0B4 4B0BE").split()}
# Measured on the image `diffrun --image-out` writes at e9271df (record §E2.9). A change of a class
# rule moves these: update them with the record, never alone.
REAL_CLASSES = {"finisher": 9, "move-callback": 71, "span-writer": 231, "call-table": 7, "anim-target": 112,
                "mid-instruction": 3, "data": 75, "code-immediate": 15, "direct": 2, "interior": 6,
                "data-pointer": 48}


@needs_capstone
@unittest.skipUnless((os.path.exists(DIFFRUN) and os.path.exists(EXE)) or REQUIRED,
                     "build/diffrun or PRAGE.EXE absent")
class RealImageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        img = os.path.join(cls.tmp.name, "image.bin")
        subprocess.run([DIFFRUN, "--exe", EXE, "--image-out", img], check=True, capture_output=True)
        strict, loose = T.load_ported(os.path.join(ROOT, "port", "src"))
        ghidra = T.load_ghidra(os.path.join(ROOT, "port", "decomp", "prage.functions.csv"))
        cls.t = T.Triage(E.Image.load(img), ghidra, strict, loose & {s for s, _ in ghidra}, T.load_live(LIVE_FILE))
        cls.rows = cls.t.run()
        cls.by = {r["addr"]: r for r in cls.rows}
        cls.supp = cls.t.supplement()

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_the_universe_is_579_and_u0s_rule_gives_575(self):
        self.assertEqual(len(self.rows), 579)
        self.assertEqual(sum(1 for r in self.rows if r["u0"]), 575)
        self.assertEqual({r["addr"] for r in self.rows if not r["u0"]}, {0x10602, 0x10604, 0x36114, 0x3C87C})

    def test_the_class_counts(self):
        self.assertEqual(dict(collections.Counter(r["cls"] for r in self.rows)), REAL_CLASSES)

    def test_the_unported_callbacks_and_finishers_are_u0s(self):
        un = lambda c: {r["addr"] for r in self.rows if r["cls"] == c and not r["ported"]}
        self.assertEqual(un("move-callback"), U0_CALLBACKS)
        self.assertEqual(un("finisher"), U0_FINISHERS)

    def test_u0s_animation_examples_are_animation_targets(self):
        for a in (0x241A8, 0x37DCC, 0x400E0):
            self.assertEqual(self.by[a]["cls"], "anim-target", "%X" % a)

    def test_no_entry_is_stale(self):
        self.assertEqual(self.t.stale, [])

    def test_the_span_tables(self):
        self.assertEqual(self.t.span_tables, {0x80C8C, 0x80D0C, 0x80E0C, 0x81010, 0x81110})

    def test_the_live_column(self):
        self.assertEqual(collections.Counter(r["live"].split(" ")[0] for r in self.rows),
                         {"-": 553, "entry": 22, "body": 4})

    def test_the_voice_sites(self):
        sites = self.t.rel32_anywhere(T.VOICE_FN)
        self.assertEqual(len(sites), 303)               # k7-k12 §0.2
        placed = {v["site"]: v for v in self.t.voice_placement(self.rows, self.supp)}
        self.assertTrue(U0_VOICE <= set(placed))
        self.assertEqual(collections.Counter((placed[s]["kind"], placed[s]["ported"]) for s in U0_VOICE),
                         {("row", False): 40, ("supplement", False): 9, ("-", False): 17})
```

- [ ] **Step 2: Run them and see them fail**

Run: `python3 -m unittest tools.tests.test_entry_triage 2>&1 | grep -E "Error:|^Ran|FAILED"`
Expected: two `AttributeError: module 'entry_triage' has no attribute 'main'`, `Ran 39 tests`, `FAILED (errors=2, skipped=8)` when `build/diffrun` is absent; with the Setup build present the eight `RealImageTests` run and pass already (they pin Task 4's behaviour; Step 7 proves they can fail), so the line reads `FAILED (errors=2)`.

- [ ] **Step 3: Append the CLI and the table to `tools/entry_triage.py`**

Append to the end of the file (module level, no indent):

```python
# ---- the committed table (§E2.9)
def summary(rows):
    by_cls = collections.Counter(r["cls"] for r in rows)
    by_batch = collections.Counter((r["batch"], r["ported"]) for r in rows if r["batch"] != "-")
    by_e1 = collections.Counter(r["e1"].split(" ")[0] for r in rows if r["batch"] != "-" and not r["ported"])
    return by_cls, by_batch, by_e1


def hexes(xs):
    return " ".join("%X" % x for x in xs) or "-"


def render_table(t, rows, supp, voice):
    by_cls, by_batch, by_e1 = summary(rows)
    u0 = sum(1 for r in rows if r["u0"])
    out = ["# E2 entry triage: the counted target list", "",
           "Generated by `tools/entry_triage.py` from the image `build/diffrun --image-out` writes "
           "(record `2026-10-01-reverse-e2-derivations.md`). Do not edit by hand: `make entry-triage` "
           "fails when this file differs from a fresh run.", "",
           "Universe: %d candidates, of which U0's rule admits %d (column `u0`)." % (len(rows), u0), "",
           "## Counts by class", "", "| class | rows | of them U0's |", "|---|---|---|"]
    out += ["| %s | %d | %d |" % (c, by_cls[c], sum(1 for r in rows if r["cls"] == c and r["u0"]))
            for c in CLASS_ORDER if by_cls[c]]
    out += ["| **total** | **%d** | **%d** |" % (len(rows), u0), "", "## P-track batches (targets only)", "",
            "| batch | unported | ported |", "|---|---|---|"]
    out += ["| %s | %d | %d |" % (b, by_batch[(b, False)], by_batch[(b, True)]) for b in BATCHES]
    out += ["", "## E1 readiness of the unported targets (record §E2.6)", "", "| readiness | rows |", "|---|---|"]
    out += ["| %s | %d |" % (k, by_e1[k]) for k in sorted(by_e1)]
    out += ["", "## Rows", "",
            "| addr | size | class | evidence | u0 | live Ghidra | ported | batch | E1 | voice sites |",
            "|---|---|---|---|---|---|---|---|---|---|"]
    for r in rows:
        out.append("| %05X | %d | %s | %s | %s | %s | %s | %s | %s | %s |" % (
            r["addr"], r["size"], r["cls"], r["evidence"], "yes" if r["u0"] else "no", r["live"],
            "yes" if r["ported"] else "no", r["batch"], r["e1"], hexes(r["voice"])))
    out += ["", "## Supplement: trusted entries outside the universe (record §E2.5)", "",
            "| addr | reached | live Ghidra | ported | E1 | voice sites |", "|---|---|---|---|---|---|"]
    for s in supp:
        out.append("| %05X | %s | %s | %s | %s | %s |" % (s["addr"], s["why"], s["live"],
                                                          "yes" if s["ported"] else "no", s["e1"], hexes(s["voice"])))
    out += ["", "## Untrusted entries: plausible rel32 targets only untrusted bytes reach (record §E2.5)", "",
            "| addr | rel32 sites | live Ghidra | ported |", "|---|---|---|---|"]
    for x, sites in t.untrusted().items():
        out.append("| %05X | %s | %s | %s |" % (x, hexes(sites[:6]), t.live_status(x),
                                               "yes" if t.is_ported(x) else "no"))
    out += ["", "## Voice sites outside the Ghidra functions (record §E2.7)", "",
            "| site | in the code of | as | ported |", "|---|---|---|---|"]
    for v in voice:
        out.append("| %05X | %s | %s | %s |" % (v["site"], "%05X" % v["entry"] if v["entry"] else "-",
                                               v["kind"], "yes" if v["ported"] else "no"))
    return "\n".join(out) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(description="E2 triage of the non-Ghidra entry candidates")
    ap.add_argument("--image", required=True, help="the fixed-up image `build/diffrun --image-out` writes")
    ap.add_argument("--functions", default=os.path.join(ROOT, "port", "decomp", "prage.functions.csv"))
    ap.add_argument("--src", default=os.path.join(ROOT, "port", "src"))
    ap.add_argument("--live", help="the live-Ghidra delta (record §E2.8); without it the column reads '-'")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--out", help="write the markdown table here")
    g.add_argument("--check", help="exit 1 unless this committed table equals a fresh run")
    ap.add_argument("--expect", type=int, help="exit 1 unless the universe holds exactly this many candidates")
    ap.add_argument("--expect-u0", type=int, help="exit 1 unless U0's rule admits exactly this many of them")
    a = ap.parse_args(argv)
    if de.capstone is None:
        print("entry-triage: skipped: capstone is not installed (pip install -r tools/requirements-diff.txt)")
        return 1 if os.environ.get("PR_ORACLE_REQUIRED") == "1" else 0
    strict, loose = load_ported(a.src)
    ghidra = load_ghidra(a.functions)
    t = Triage(de.Image.load(a.image), ghidra, strict, loose & {s for s, _ in ghidra},
               load_live(a.live) if a.live else None)
    rows = t.run()
    supp = t.supplement()
    voice = t.voice_placement(rows, supp)
    by_cls, by_batch, _ = summary(rows)
    u0 = sum(1 for r in rows if r["u0"])
    print("entry-triage: %d candidates (%d by U0's rule); %s" % (len(rows), u0, " ".join(
        "%s=%d" % (c, by_cls[c]) for c in CLASS_ORDER if by_cls[c])))
    print("entry-triage: targets %d unported, %d ported; supplement %d (%d unported, %d stale); untrusted entries %d" % (
        sum(n for (b, p), n in by_batch.items() if not p), sum(n for (b, p), n in by_batch.items() if p),
        len(supp), sum(1 for s in supp if not s["ported"]), len(t.stale), len(t.untrusted())))
    print("entry-triage: voice sites outside Ghidra %d: %d in unported code, %d in ported code, %d nowhere" % (
        len(voice), sum(1 for v in voice if v["entry"] and not v["ported"]),
        sum(1 for v in voice if v["ported"]), sum(1 for v in voice if not v["entry"])))
    status = 0
    text = render_table(t, rows, supp, voice)
    if a.out:
        with open(a.out, "w") as f:
            f.write(text)
    if a.check:
        with open(a.check) as f:
            if f.read() != text:
                print("entry-triage: FAIL: %s differs from a fresh run (regenerate it with --out)" % a.check)
                status = 1
    if a.expect is not None and len(rows) != a.expect:
        print("entry-triage: FAIL: %d candidates, expected %d" % (len(rows), a.expect))
        status = 1
    if a.expect_u0 is not None and u0 != a.expect_u0:
        print("entry-triage: FAIL: %d candidates by U0's rule, expected %d" % (u0, a.expect_u0))
        status = 1
    return status


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Run the tests and see them pass, the real-image pins included**

Run: `PR_ORACLE_REQUIRED=1 python3 -m unittest tools.tests.test_entry_triage 2>&1 | tail -3`
Expected: `Ran 39 tests`, `OK` (about 9 s; the real-image class builds the image with `build/diffrun`).

If a `RealImageTests` pin fails, the image or `port/src` differs from the planning run: do not edit the pin to match. Print the run (Step 5), find the rows that moved, and record each difference and its cause in the record's §E2.9 (raw wins); then update the pin and the record in the same commit.

- [ ] **Step 5: Generate the committed table**

```bash
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_e2_image.bin && shasum /tmp/pr_e2_image.bin
python3 tools/entry_triage.py --image /tmp/pr_e2_image.bin \
    --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
    --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md --expect 579 --expect-u0 575; echo "exit $?"
```

Expected: `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`, then exactly the three lines of record §E2.9 and `exit 0`:

```
entry-triage: 579 candidates (575 by U0's rule); finisher=9 move-callback=71 span-writer=231 call-table=7 anim-target=112 mid-instruction=3 data=75 code-immediate=15 direct=2 interior=6 data-pointer=48
entry-triage: targets 332 unported, 163 ported; supplement 131 (35 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 49 in unported code, 66 in ported code, 19 nowhere
```

Then: `grep -c '^| [0-9A-F]\{5\} | ' docs/superpowers/plans/2026-10-01-reverse-e2-triage.md` prints `874` (579 rows + 131 supplement entries + 30 untrusted entries + 134 voice sites, each a line starting with a five-digit address), and `git diff --stat $(git log -1 --format=%h --grep='E2 record (the universe') HEAD -- docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md` prints nothing: the rules were not changed after Task 1 committed them (Review Focus 1).

- [ ] **Step 6: Commit**

```bash
git add tools/entry_triage.py tools/tests/test_entry_triage.py docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git commit -m "tools: entry_triage CLI and the committed E2 target list (579 candidates, 575 by U0's rule)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 7: Mutation proofs (the CLI and the real-image pins)**

Run: `PR_ORACLE_REQUIRED=1 python3 /tmp/e2_mutate.py M1,M12,M23,M24,M26; git diff --exit-code tools/entry_triage.py`
Expected, exactly (M1, M12 and M26 now also fail the real-image pins `test_the_class_counts`, `test_the_universe_is_579_and_u0s_rule_gives_575`, `test_the_span_tables`, `test_no_entry_is_stale`, `test_the_live_column`):

```
M1 any-step ret test removed -> test_a_wrong_expected_count_fails, test_batches_and_targets, test_every_planted_candidate_gets_its_class, test_ret_imm16_and_the_byte_level_c3_count, test_run_keeps_the_classes, test_the_class_counts, test_the_counts_the_table_and_the_check, test_the_live_column, test_the_u0_column, test_the_universe_is_579_and_u0s_rule_gives_575, test_the_universe_is_exactly_the_planted_candidates, test_u0s_greedy_rule_misses_the_entry_after_ret_4
M12 no span fixpoint -> test_a_span_table_read_above_the_runtime_cut_is_found, test_every_planted_candidate_gets_its_class, test_no_entry_is_stale, test_run_keeps_the_classes, test_the_class_counts, test_the_span_tables
M23 check never fails -> test_the_counts_the_table_and_the_check
M24 expect ignored -> test_a_wrong_expected_count_fails
M26 u0 column is the corrected rule -> test_a_wrong_expected_count_fails, test_the_counts_the_table_and_the_check, test_the_u0_column, test_the_universe_is_579_and_u0s_rule_gives_575
```

---

### Task 6: `make entry-triage`, in `make verify`

**Files:**
- Modify: `Makefile` (four additive edits, listed under Shared-file touch points)

**Interfaces:**
- Consumes: Task 5's CLI and table.
- Produces: `make entry-triage [E2_IMAGE=...]`; one line in `verify`.

- [ ] **Step 1: Edit the Makefile**

After the line `DIFF_TABLE ?= /tmp/pr_diff_table.md` add:

```make
E2_IMAGE ?= /tmp/pr_e2_image.bin
E2_TABLE = docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
E2_LIVE = docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt
```

Replace the last `.PHONY` line `        attract2-oracle attract2-compare k11-capture k11-oracle k11-report gp-capture gp-replay gp-oracle gp-report diff-verify` with:

```make
        attract2-oracle attract2-compare k11-capture k11-oracle k11-report gp-capture gp-replay gp-oracle gp-report diff-verify \
        entry-triage
```

After the `diff-verify` recipe (its last line ends `... is not installed (pip install -r tools/requirements-diff.txt)"; fi`) and its blank line, add:

```make
# E2 (record docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md): triage of the entry
# candidates Ghidra never listed. The committed table must equal a fresh run on the image the port's
# loader dumps, so a port change that ports a target regenerates it in the same commit. Skips without
# capstone or PRAGE.EXE.
entry-triage: build ## E2 triage of the non-Ghidra entry candidates: unit tests + the committed table must equal a fresh run (skips without capstone)
	@echo "== entry triage (the non-Ghidra entry candidates; record E2) =="
	@if ! $(PYTHON) -c "import capstone" 2>/dev/null; then \
		echo "entry-triage: skipped: capstone is not installed (pip install -r tools/requirements-diff.txt)"; \
	elif [ ! -f $(GAME_DIR)/PRAGE.EXE ]; then echo "entry-triage: skipped: $(GAME_DIR)/PRAGE.EXE is absent"; \
	else $(PYTHON) -m unittest tools.tests.test_entry_triage && \
		./$(BUILD_DIR)/diffrun --exe $(GAME_DIR)/PRAGE.EXE --image-out $(E2_IMAGE) && \
		$(PYTHON) tools/entry_triage.py --image $(E2_IMAGE) --live $(E2_LIVE) --check $(E2_TABLE) \
			--expect 579 --expect-u0 575; fi
```

In the `verify` recipe, after `	@$(MAKE) --no-print-directory diff-verify`, add:

```make
	@$(MAKE) --no-print-directory entry-triage
```

(If the user answered D3 "no", skip this last edit.)

- [ ] **Step 2: Run the target**

Run: `make entry-triage E2_IMAGE=/tmp/pr_e2_t6.bin > /tmp/pr_e2_t6.log 2>&1; echo "exit $?"; grep -E '^(Ran|OK|entry-triage)' /tmp/pr_e2_t6.log`
Expected: `exit 0`, `Ran 39 tests in ...`, `OK`, and the three `entry-triage:` lines of Task 5 Step 5 (about 19 s).

- [ ] **Step 3: Prove the check can fail**

```bash
echo "edited" >> docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
make entry-triage E2_IMAGE=/tmp/pr_e2_t6.bin > /tmp/pr_e2_t6.log 2>&1; echo "exit $?"; tail -2 /tmp/pr_e2_t6.log
git checkout -- docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
make entry-triage PYTHON="python3 -S" > /tmp/pr_e2_skip.log 2>&1; echo "exit $?"; tail -1 /tmp/pr_e2_skip.log
```

Expected: `exit 2`, then `entry-triage: FAIL: docs/superpowers/plans/2026-10-01-reverse-e2-triage.md differs from a fresh run (regenerate it with --out)` and `make: *** [entry-triage] Error 1`; after the checkout the table is clean. The last run hides `site-packages` (`-S`), so `capstone` cannot import: `exit 0` and `entry-triage: skipped: capstone is not installed (pip install -r tools/requirements-diff.txt)`.

- [ ] **Step 4: Commit**

```bash
git add Makefile
git commit -m "make: entry-triage target (E2 unit tests + the committed table must equal a fresh run), in verify

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Close the record, PROGRESS and AGENTS, and the gate

**Files:**
- Modify: `docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md` (the "Status of the numbers" paragraph near the top)
- Modify: `docs/PROGRESS.md` (append one paragraph)
- Modify: `AGENTS.md` (one line in the Commands block)

- [ ] **Step 1: The record's status**

Replace the paragraph that begins `**Status of the numbers.**` with this one, putting in the output of `git log -1 --format=%h` at the Task 5 commit (`git log --format='%h %s' | grep 'committed E2 target list'`):

```markdown
**Status of the numbers.** Every figure in §E2.1-§E2.9 was measured in the planning scratch run and
re-measured in the plan's Task 5 at commit `<the short sha printed above>`, with the same result; the
real-image tests (`RealImageTests` in `tools/tests/test_entry_triage.py`) pin them, and `make
entry-triage` fails when the committed table differs from a fresh run.
```

If Task 5 found a difference, write instead which figure moved, from what to what, and why (raw wins), and keep §E2.9's table matching the committed one.

- [ ] **Step 2: PROGRESS and AGENTS**

Append to `docs/PROGRESS.md`:

```markdown
**Reverse completion E2 (triage of the non-Ghidra entry points; record `docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md`).** `tools/entry_triage.py` re-derives U0's "575 plausible entries in no Ghidra function" (exactly one reading of U0's description gives 575: dwords at any offset of the data object or operands of Ghidra code, below `0x5D000`, after a `ret` once the fillers `90`/`8B C0`/`8D 40 00`/`00` are skipped, decoding) and corrects its rule: U0 skipped the `00` of `ret N` as a filler, so the table standardises on 579 candidates with a `u0` column (575). Each candidate gets the first of fifteen rules the record fixed before the tool ran, with the proving address: finisher 9, move-callback 71, span-writer 231 (five tables, `0x81010` read by span code above the runtime cut), call-table 7, anim-target 112, mid-instruction 3, data 75, code-immediate 15, direct 2, interior 6, data-pointer 48, and no dead or unclassified row. The committed list (`docs/superpowers/plans/2026-10-01-reverse-e2-triage.md`) gives track P 332 unported targets: callbacks 27 and finishers 6 (exactly U0's), animation targets 65 (with `0x36114`, which U0's rule missed), span writers 231, other 3; plus 35 unported supplement entries, 8 unported entries only untrusted bytes reach, and 19 voice sites in no known body. What E1 can run today: 236 leaves and 6 allow-list targets; the other 90 wait on call stubs, and every one of the 86 outside the span code has a clean body of its own, with the blocker in a ported callee (the animation dispatcher's `0x2B56D`, `actor_spawn`'s `0x2B0E9`), so the call-stub design in record §E2.10 gates P. The live Ghidra project (1504 functions, 152 more than the export) is recorded as data, not consulted: 22 candidates are live entries and 4 sit inside live bodies. `make entry-triage` (in `make verify`; skips without `capstone` or `PRAGE.EXE`) runs 39 tests and fails when the committed table differs from a fresh run, so a P commit that ports a target regenerates it. Readiness is a static prediction, not verification. No `port/src` change; the counters stay `771 1203 64` and `731 731 100`.
```

In `AGENTS.md`'s Commands block, after the `make diff-verify` line, add:

```bash
make entry-triage          # E2: triage of the non-Ghidra entry candidates; the committed table must equal a fresh run (in make verify; skips without capstone)
```

- [ ] **Step 3: The gate**

```bash
t=e2; make verify SMK_DUMP=/tmp/pr_${t}_smk TITLE_DUMP=/tmp/pr_${t}_title ATTRACT_DUMP=/tmp/pr_${t}_att FRONTEND_DUMP=/tmp/pr_${t}_fe TITLE_PIN_DIR=/tmp/pr_${t}_pin AUDIO_WAV=/tmp/pr_${t}.wav K11_DUMP=/tmp/pr_${t}_k11 GP_DUMP=/tmp/pr_${t}_gp DIFF_IMAGE=/tmp/pr_${t}_diffimg DIFF_TABLE=/tmp/pr_${t}_diff.md E2_IMAGE=/tmp/pr_${t}_e2img > /tmp/pr_e2_verify.log 2>&1; echo "verify exit $?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_e2_verify.log > /tmp/pr_e2_oracle.txt
diff /tmp/pr_e2_oracle.txt .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo "oracle lines equal"
grep -E '^entry-triage: ' /tmp/pr_e2_verify.log
make audio-render AUDIO_WAV=/tmp/pr_e2.wav > /dev/null 2>&1; cmp /tmp/pr_e2.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo "wav identical"
python3 tools/port_progress.py | head -2
git status --short
```

Expected: `verify exit 0`, `oracle lines equal`, the three `entry-triage:` lines of Task 5, `wav identical`, `771 1203 64` and `731 731 100 (portable: ...)`, and `git status` showing only the three files of this task.

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md docs/PROGRESS.md AGENTS.md
git commit -m "docs: E2 record closed, PROGRESS paragraph, AGENTS command

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

## Execution notes

- **Order:** 1 → 2 → 3 → 4 → 5 → 6 → 7, strictly (each task appends to the files the previous one made). Task 1 must land before Task 5 runs the tool on the image (Review Focus 1).
- **Model tiers:** Task 1: haiku (copying and hashing). Tasks 2-4: sonnet (paste the code exactly; the risk is an indentation slip in the appended `class Triage` blocks, which the tests catch). Task 5: sonnet, with opus as the reviewer: it is the only task that meets the real image, and a pin that moves must be explained, not edited. Task 6: sonnet. Task 7: sonnet; the reviewer re-runs the gate commands.
- **Reviewer checks per task:** the red output matches the plan's quoted failure; the green count matches (11, 19, 29, 39); the mutation output matches the quoted lines exactly and `git diff --exit-code tools/entry_triage.py` is clean afterwards; no file outside the task's list changed.
- **The gate:** `make verify` exit 0 with the 45 oracle lines equal to the baseline, the WAV identical, the counters `771 1203 64` / `731 731 100` (E2 ports nothing), and `make entry-triage` green inside it. A fresh checkout without `capstone` must skip `entry-triage` cleanly; without `PRAGE.EXE` it skips too.
- **After E2:** track P reads the committed table (record §E2.9's counted list) and is gated by the call-stub unit (record §E2.10); D2 decides the span-writer batch size.
