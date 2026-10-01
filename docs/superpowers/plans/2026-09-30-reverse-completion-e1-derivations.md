# Reverse completion E1: the differential-emulation harness (record)

**Scope.** Sub-project E1 of `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §5 (plan
`2026-09-30-reverse-completion-e1-harness.md`, branch `reverse-e1`, base `4fc2bde`). It builds the
harness and verifies four already-ported functions with it; it changes nothing under `port/src`
(`git diff --stat main -- port/src` is empty). It settles the two questions of spec §5.4.

**Tooling.** The original side is `tools/diff_emu.py` (`unicorn` 2.1.4 runs the original bytes,
`capstone` 5.0.7 decodes them for the static scan). The port side is `build/diffrun`
(`port/tests/diff_runner.c`, linked with `prage_core`). The driver and verdicts are
`tools/diff_verify.py`; `make diff-verify` runs it and the 47 Python tests, and `make verify` calls it
after `gp-oracle`. Every address below was read from the image the loader leaves in `mem[]` (fixups
applied: the dump `diffrun --image-out` writes, 1 028 304 bytes from `0x10000`), disassembled with
`capstone` in 32-bit mode.

**Verdict.** The harness works: four ported functions are VERIFIED with every reachable block hit,
and five deliberately mutated ports are each reported as MISMATCH. No real port function disagreed
with the original on any of the 19 cases (§E.5). The limits that bound what it can claim are in §E.6.

---

## §E.1 Addressing (spec §5.4, settled)

The image is flat 32-bit with no segment base: operands are plain linear addresses. Evidence, from the
loaded image (fixups applied):

| function | address | instruction | operand |
|---|---|---|---|
| `rng_next` | `0x5D7E5` | `mov eax, dword ptr [0xef6d8]` | the seed, read |
| `rng_next` | `0x5D7F6` | `mov dword ptr [0xef6d8], eax` | the seed, written |
| `fighter_slot_flag` | `0x3C57A` | `mov edx, dword ptr [0x107ee0]` | the slot-flag word |
| `config_credit_spend` | `0x2CA7C` | `cmp byte ptr [0x105d60], 0` | the free-play byte |

`port/src/symbols.h` names the same linear addresses (`DS_000EF6D8` is `0x000EF6D8u`, `DS_00107EE0`,
`DS_00105D60`, `DS_00105C00`, `DS_00104B1F`) and `DSD(o)` is `*(u32 *)(mem + o)`, so the port indexes
`mem[]` with exactly the address the original's instruction carries. The emulator therefore maps a
64 MB flat space (`MEM_SIZE` `0x4000000`) with the image at `0x10000` (the dump `mem_load_le` writes:
`0x10B0D0 - 0x10000 = 1 028 304` bytes) and needs no segment setup. The "DS offset = address
`- 0x80000`" wording in `AGENTS.md` describes how Ghidra names globals (`DAT_0008xxxx`), not how the
code addresses them: the `0x80000` is already in the operand.

## §E.2 Install (spec §5.4, settled)

`unicorn` 2.1.4 and `capstone` 5.0.7 install with `pip` on this host (macOS arm64, Python 3.12.12):
`python3 -m pip install -r tools/requirements-diff.txt` prints "Successfully installed capstone-5.0.7
unicorn-2.1.4" from prebuilt wheels, no build step. No interpreter fallback was needed. The pins are in
`tools/requirements-diff.txt`. A host without `unicorn` skips the step (below); `capstone` is already a
repo dependency.

## §E.3 The harness's contract and what it compares

**Flow.** `tools/diff_verify.py` holds `SPECS`, one per function: a binding name, the original's entry,
and cases (registers plus `poke`s). For each spec it (1) writes the cases to a `diffrun --cases` file,
(2) runs `build/diffrun --exe data/game/C/PRAGE.EXE --image-out <image> --cases <file>`, which loads the
LE image into `mem[]`, dumps it, and for each case applies the registers and pokes, calls the C function
through its binding, and prints `ret eax`, every changed byte (`w <addr> <value>`) and `end`; then (3)
loads that dump as the original side's image and runs the original's bytes in `unicorn` from the same
image with the same registers and pokes, from the entry to a sentinel return address; (4) compares.
Both sides start from identical memory because the original side reads the port loader's own dump.

**Case-file format** (one block per case, `#` lines ignored): `case <id>`, `fn <binding>`, `reg <name>
<hex>` (eax, ebx, ecx, edx, esi, edi, ebp; the others are zero), `poke <hex addr> <hex bytes>` (at most
16 per case, at most 64 bytes each, inside the image), `end`. Output is `case`, `ret eax <v> mask <m>`,
`w`, `error <text>`, `end`.

**Compared, per case:**
1. EAX under the binding's mask. The mask is the part of EAX the original's callers read; a function
   that returns through AL leaves EAX bits 8 and up as scratch that the C return value does not
   reproduce (`fighter_slot_flag` is bound with mask `0xFF`, the others `0xFFFFFFFF`). No other
   register is compared.
2. Every byte of memory that changed, as `addr -> final byte`, on the original side and the port side
   (the original's stack, `0x3F00000` up, is excluded: the port has none). A write of the value
   already there is not a change. The port side scans the whole `mem[]`, outside the image as well.
3. Block coverage of the original: `static_scan` walks the function from its entry by recursive descent
   (conditional branches, `loop`/`loope`/`loopne`, unconditional jumps, tail jumps, out-of-line blocks)
   to get the block leaders; a block is hit when its leader instruction executed in some case.

**Verdicts** (`verify_spec`), in this precedence:
- `MISMATCH`: any compared quantity differs, or the port side reports an error.
- `NOT_EXERCISABLE`: the original stopped on something the emulator cannot model (a call outside the
  case's allow-list, `int`, `in`/`out`, `hlt`, `cli`/`sti`, undecodable bytes at run time, a fault, a
  timeout); the instruction is named.
- `PARTIAL`: no discrepancy, but a reachable block was never executed and has no stated reason
  (`unhit_named`), or the scan found an indirect `jmp`/`call` (targets unknown) or was truncated.
- `VERIFIED`: none of the above: every reachable block hit, or each unhit one named with its reason.

**The four specs and their blocks.** All four have no `unhit_named` entries.

| function | original | blocks | what the cases cover |
|---|---|---|---|
| `rng_next` | `0x5D7DC` | 1 | seed and range edges (`range & 0xFFFF == 0`, `0xFFFF`, a range above 16 bits) |
| `fighter_slot_flag` | `0x3C570` | 3 | bit clear (sets, returns 0), bit already set (returns 1, no write), AL-only return, bits 31 and 33 |
| `config_credit_spend` | `0x2CA7C` | 7 | free play, credits short, exact, enough with and without the no-debit byte, and a count with the sign bit set (`0x80000001`) that only an unsigned `ja` treats correctly. The out-of-line `xor eax,eax; ret` at `0x2CA78` (reached by the `ja` at `0x2CA91`) is one of the seven |
| `config_codeword_len` | `0x2D4B4` | 4 | the doubling loop at 0, 1, `0x26`, `0xFF` and `0x1000` |

## §E.4 Results

Measured on the Task 5 run (`python3 tools/diff_verify.py --self-check`), verbatim:

```
| function | original | cases | blocks hit/total | verdict |
|---|---|---|---|---|
| rng_next | 0x5D7DC | 4 | 1/1 | VERIFIED |
| fighter_slot_flag | 0x3C570 | 5 | 3/3 | VERIFIED |
| config_credit_spend | 0x2CA7C | 5 | 7/7 | VERIFIED |
| config_codeword_len | 0x2D4B4 | 5 | 4/4 | VERIFIED |
| rng_next@mutant | 0x5D7DC | 4 | 1/1 | MISMATCH |
| fighter_slot_flag@mutant | 0x3C570 | 5 | 3/3 | MISMATCH |
| config_credit_spend@mutant | 0x2CA7C | 5 | 7/7 | MISMATCH |
| config_credit_spend@signed | 0x2CA7C | 5 | 7/7 | MISMATCH |
| config_codeword_len@mutant | 0x2D4B4 | 5 | 4/4 | MISMATCH |
diff-verify: 4/4 functions VERIFIED; 5/5 mutants detected. Claim: equivalence on the exercised blocks and inputs only.
```

The five mutants live in `port/tests/diff_runner.c` (never in `prage_core`), each a plausible porting
bug: `rng_next` increment `0x38CE0520` instead of `0x38CE051F` (the original's `add eax, 0x38ce051f` is
at `0x5D7F1`); `fighter_slot_flag` that forgets to set the bit; `config_credit_spend` with `>=` for the
unsigned `ja`, and with a signed compare for it; `config_codeword_len` with `>` for the signed `jge`.
The `@signed` mutant is caught only by case `c5` (`0x80000001`); removing `c5` makes two tests fail.

**Mutation proofs.** Each was applied to the committed file, the named tests were run, and the file was
restored with `git checkout` (none was committed). "Mutation -> test that failed", all other tests
passing:

Task 1 (`tools/diff_emu.py`, 15 tests):
- `if new != old:` -> `if True:` -> `test_a_write_of_the_value_already_there_is_not_a_change`
- `"int", "int3"` -> `"int3"` in `UNMODELED_MNEMONICS` -> `test_int_is_unmodeled_and_named`
- drop the `STACK_LOW` `continue` in the writes diff -> `test_stack_traffic_is_not_a_write`
- `elif tgt not in allow:` -> `elif False:` -> `test_a_call_outside_the_allow_list_is_unmodeled`

Task 2 (static scan, 20 tests, then 24 after the review fix):
- `if m == "jmp": break` -> `pass` -> `test_a_direct_target_outside_the_image_is_unresolved` and
  `test_a_tail_jump_is_followed_into_the_next_function`
- remove the `loop`/`loope`/`loopne` handling -> `test_loop_targets_and_fall_through_are_leaders`
- remove `truncated = True` on undecodable bytes -> `test_undecodable_bytes_mark_the_scan_truncated`
- `>=` -> `>` in the `max_insns` guard -> `test_the_instruction_budget_marks_the_scan_truncated`
- remove the `if addr in insns: break` guard -> the back-edge test hangs (killed by a 10 s timeout);
  this guard is provable only by a hang, not by a failure (§E.6)

Task 3 (`diffrun`, reproduced on the pre-fix binary with scratch case files, then fixed):
- a bad poke followed by a clean case: the clean case printed `ret eax 0x1` with no `w` line (stale
  `0xFFFFFFFF` left at `0x107EE0`); after the fix the same pair gives `ret eax 0x0` and
  `w 0x107EE0 0x20`; the Python test `test_an_error_case_leaves_no_state_behind` fails on the pre-fix
  binary (`(1, {}) != (0, {1081056: 32})`) and passes now
- `poke FFFFFFF0` plus 32 bytes: accepted and a segfault (exit 139) before; `error poke 0xFFFFFFF0
  outside the image` after

Task 4 (`tools/diff_verify.py`, 47 tests in all):
- remove case `c5` -> `test_every_mutant_is_reported_as_a_mismatch` and
  `test_the_unsigned_guard_case_is_the_only_one_that_catches_the_signed_mutant`
- the byte-diff loop -> `for a in []` -> `test_a_write_the_original_did_not_make_is_a_discrepancy`,
  `test_a_write_the_port_forgot_is_a_discrepancy`, `test_a_forgotten_write_is_caught_by_the_byte_diff_alone`
  and `test_every_mutant_is_reported_as_a_mismatch`
- the `PARTIAL` branch -> `elif False:` -> `test_an_indirect_jump_keeps_the_function_partial` and
  `test_an_unexercised_block_is_partial_not_verified`
- `fighter_slot_flag`'s binding mask `0xFF` -> `0xFFFFFFFF` in `diff_runner.c`, `diffrun` rebuilt ->
  `test_the_four_ported_functions_agree_with_the_original_on_every_block` (the AL-only return is the
  one case where an unmasked EAX would be flagged)

`python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify` runs 47 tests, OK, no skips.

**Cost (measured on the Task 5 host, macOS arm64, Apple clang 21.0.0, no other work running):**
- Original side (`run_original`: map 64 MB, write the 1 MB image, run, diff): 0.44 ms for
  `config_codeword_len` at `eax = 0x1000`; 0.39 ms per case averaged over all 19 real cases.
- Port side (`build/diffrun`): 10.5 ms per invocation to load the LE image and dump it (`time` reads
  0.012 s), and about 12.3 ms per case (3 runs of 5 cases: 71.0, 72.5, 72.3 ms against 10.1, 10.7,
  11.1 ms for the load alone). The per-case cost is the whole-`mem[]` byte diff and restore (the 1 MB
  image compared and copied, the 64 MB above and below it scanned for writes); it was not profiled.
- Whole runs: `python3 tools/diff_verify.py` (four functions, 19 cases, four `diffrun` invocations)
  0.59 s; with `--self-check` (nine bindings, 43 cases) 1.35 s; the 47 Python tests 2.6 s.
- So a batch costs about 12.7 ms per case plus about 10.5 ms per function (a `diffrun` launch each),
  dominated by the port side; 1 000 cases is of the order of 13 s. Batching all of a function's cases
  into one `diffrun` call (as `diff_verify` does) already amortises the load.

## §E.5 Findings

**No real port function disagreed with the original** on any of the 19 cases (`rng_next` 4,
`fighter_slot_flag` 5, `config_credit_spend` 5, `config_codeword_len` 5).

**`fighter_slot_flag` case `f33` (shift count 33).** The original's `shl eax, cl` at `0x3C580` masks the
count to 5 bits in hardware, so `bit = 33` tests and sets bit 1. The port writes `1u << (bit & 0xffu)`
(`port/src/game/fighter.c`), which for a count of 32 to 255 is undefined behaviour in C: the language
does not say the count is masked. On this build (Apple clang 21.0.0, arm64, `CMAKE_BUILD_TYPE` empty so
no `-O` flag) the port agreed with the original on `f33`: EAX, the changed byte and the coverage all
matched. That agreement depends on the compiler, the optimisation level and the target (arm64 and x86
both mask a 32-bit register shift to 5 bits; a different optimiser may constant-fold or reorder it). It
is not a property of the port's source. Whether the game ever passes a count above 31 is not shown here (the three callers in `port/src`
pass the constants 0, 1, 2, 3 and 5: `camera.c` lines 189 and 237, `fighter.c` line 526). Nothing is fixed in E1; the P track should decide
whether to mask the count explicitly (`& 31u`), which would make the agreement a property of the code.

**The AL-only return of `fighter_slot_flag`.** The original leaves EAX bits 8 and up as scratch (the
shifted mask) and returns through AL (`mov al, 1` at `0x3C586`, `xor al, al` at `0x3C596`). The port
returns an `int`. The binding therefore compares `eax & 0xFF`; case `f9` (`eax = 9`, flags `0x205`)
exercises it, and `test_the_four_ported_functions_...` fails when the mask is widened (mutation above).

**Findings in the harness itself, made visible by review of Tasks 2 and 3.** These are defects of the
harness's own development, fixed before it was relied on; they are not defects of the port. The plan's
Task 2 and Task 3 code blocks were amended to match the committed code.
1. Task 2: `static_scan` did not follow `loop`, `loope` and `loopne` (capstone reports them as
   relative branches but not in its jump group), so a reachable block behind a loop was missed and the
   coverage could read 100% with a block never counted. They are now followed as conditional branches.
   Separately, an undecodable instruction ended a path silently; it now sets `truncated = True`, which
   keeps the function `PARTIAL`. The Python tests went from 20 to 24 for Task 2 (42 to 46 in all).
2. Task 3, state leak: `run_case` applied pokes one at a time and returned on a later bad poke without
   restoring the image, so the next case started from dirty memory (reproduced: a stale `0x1` where
   `0x0` was expected). All pokes are now validated before any is applied.
3. Task 3, wrap: the poke bounds check wrapped in `u32` (`FFFFFFF0` plus 32 bytes was accepted and
   segfaulted) and `parse_hex` truncated a 48-bit address to 32 bits (`100000107EE0` aliased `0x107EE0`).
   The check cannot wrap now and `parse_hex` rejects values above `0xFFFFFFFF`.
4. Task 3, three input errors were silent and are now rejected: a `case` opened inside an unterminated
   case, a case with no `end` at end of file, and a trailing option flag with no value. A Python test for
   case independence was added (46 to 47 tests).

## §E.6 What E1 does not do (limits, each a named gap for later tracks)

1. **Calls out are an allow-list, not stubs.** A call outside the case's `allow_calls` stops the run as
   `unmodeled`, so a function with an unported callee is `NOT_EXERCISABLE` until the P track adds stubs
   that both sides share (spec §5.2). The four functions verified here are leaves.
2. **Only EAX is compared as a register output.** Flags, the other registers and the callee-saved set
   are not compared.
3. **Stack-argument functions (`ret N`) and functions with C out-parameters have no binding form yet.**
   A binding gets the seven registers only.
4. **The only snapshot is the boot image plus `poke`s** (at most 16 per case, at most 64 bytes each).
   Capture-frame snapshots (spec §5.3) are the next increment.
5. **The emulator has no port I/O, interrupts, FPU or segment registers.** Such functions are
   `NOT_EXERCISABLE` with the instruction named (`int`, `in`, `out`, `hlt`, `cli`, `sti`, and so on).
6. **The cost**, which bounds the P track's batch sizes: see the measured figures at the end of §E.4.
7. **Review minors, deferred and not fixed in E1** (each a known limit of the harness, none of which
   changes the table above):
   - `tools/diff_verify.py --function <name>` with a name that is not in `SPECS` selects no spec and
     exits 0. `make diff-verify` never passes it.
   - `parse_port_output` accepts a case with no `ret` or `error` line and merges duplicate case ids
     (a block with no `ret` or `error` line parses as `eax 0` with no writes; `diffrun` is the only
     producer, and `verify_spec` would raise on a case missing altogether).
   - No test pins `MISMATCH` over `NOT_EXERCISABLE` in the verdict precedence: a mutation of the
     ordering is not caught (the one of twelve driver mutations the review ran that survived).
   - `PR_ORACLE_REQUIRED=1` does not force a failure through `make` when `unicorn` is missing: the
     Makefile's own check prints the skip line first. Run as `python3 tools/diff_verify.py` it does exit 1.
     Spec §5.5 asks for a skip in `make verify`, so this was accepted.
   - A stale `unhit_named` entry (a named block that is now hit) is not flagged, by design.
   - `tools/diff_emu.py` imports `capstone` unconditionally: a host without it errors instead of
     skipping (it is already a repo dependency).
   - The tests for the timeout, fault, indirect-call, outside-image-read and poke paths of the emulator
     were checked by reading, not by a mutation; `test_runs_are_independent` cannot detect state
     leaking between emulator runs. A `uc.mem_read(addr, 15)` near `MEM_SIZE` is unreachable from the
     entries used.
   - The static scan's loop test covers forward loops only; `jecxz`/`jcxz` are followed through
     capstone's jump group, which no test pins; the back-edge guard is provable only by a hang.
   - `diffrun`'s `parse_hex` (`strtoul`) accepts a leading `+` and a `0x` prefix; a trailing unknown flag is reported
     as "needs a value"; `end` with trailing tokens is accepted; case ids are truncated to 63
     characters; the buffers are not freed before exit.

## §E.7 The narrow claim

`VERIFIED` proves equivalence of the port's C function with the original's bytes on the exercised blocks
and inputs only: every reachable block was executed by at least one case, and on every case EAX (under
the mask) and every changed byte agreed. It cannot detect a bug in a state the cases never produce
(a value range, a combination of fields, or a poke the case list does not contain), and covering a
block does not cover every path through it. The emulator and the port share the same image (the port
loader's dump is the original side's memory), so a wrong image, such as a mis-applied fixup, would not
show: both sides would read the same wrong bytes. Callees outside the allow-list, flags, the other
registers and the stack are not compared (§E.6). A green `diff-verify` line is not "the function is
correct".
