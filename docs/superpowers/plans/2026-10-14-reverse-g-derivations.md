# Track G derivations record — widen the proven picture (2026-10-14)

Plan: `docs/superpowers/plans/2026-10-14-reverse-g-widening.md`.
Spec: `docs/superpowers/specs/2026-10-14-track-g-design.md`.
Sections are per batch (§G1..§G7); the bare §G.n prefix stays reserved for the
gameplay-ground-truth record. Lane A workers own §G1-§G4; lane B owns §G5-§G7;
each appends only its own section, byte-evidence inline.

## §G1 — exit model (lane A task 1: done)

**Byte evidence.** Every address below was read twice and agrees: from the fixup-applied image
(`./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_g1_img.bin`, capstone over it) and
from the live Ghidra instance (project `primal-rage`, program `/PRAGE.EXE`, `read_memory`), on
2026-10-14. The raw EXE bytes were not used (pre-fixup).

- `0x62003` — the runtime's `_exit` the rows call: `52` `89C2` push edx / mov edx,eax (the caller's
  EAX, the exit code, is kept in EDX), `FF15 34F90E00` call [0xEF934] and `FF15 38F90E00` call
  [0xEF938] (the cleanup vectors), `89D0` mov eax,edx (restore), `E802000000` call 0x6201B, then
  `5A C3` pop edx / ret — never reached: 0x6201B ends `E9 BA060000` jmp 0x626EB (`89D0` mov
  eax,edx again after its own two vector calls), and `0x626EB: EB 23` jmp 0x62710.
- `0x62710..0x62724` — the terminator: `50` push eax (the code goes to the stack), `B8 00000000`
  mov eax,0 and `BA FF000000` mov edx,0xff and `E8 AF9D0000` call 0x6C4CF (the flush — EAX
  destroyed by design), `58` pop eax at 0x62720 (the code returns), `B4 4C` mov ah,0x4C at 0x62721
  (only the high byte replaced), `CD 21` int 0x21 at 0x62723: DOS Terminate Process, AL = the exit
  code, nothing returns. Between the push and the pop sits the `0x626ED..0x6270F` crash-log arm
  (`mov ax,0x3D01`/`int 0x21` at 0x626F8, `mov ah,0x40`/`int 0x21` at 0x6270D) — jumped over by
  0x626EB, not on the chain's path.
- The rows' arms carry AL = 1 to it: the load short-read block leader `0x1B04D: B8 01000000`
  mov eax,1 with `0x1B054: E8 AA6F0400` call 0x62003; the save arm `0x1B0F7: mov eax,1`,
  `0x1B0FE: call 0x62003` (record §S.7.1 named the leaders; the calls are at 0x1B054/0x1B0FE).
- One site in the game: the `B4 4C CD 21` pattern's only real-code occurrence is 0x62721 (Ghidra
  byte-pattern search; its other listed hits are the `.image` overlay, analysis noise per AGENTS).
- The three cleanup vectors in the image hold `0x62002`, one `C3` ret byte: the no-op default. A
  replayed chain still reaches them as *indirect* calls: the row must allow or stub 0x62002 (and
  the flush 0x6C4CF) or the run stops before the int — reachability evidence is task 2's (§G2),
  this mechanism claims nothing about those callees.

**The terminal event.** `E.Machine(exits=True)`: in `_Devices.step`, an `int 0x21` whose AH is 0x4C
stops the run AT the instruction — EIP never advances, so nothing after it executes — records
`_Devices.terminal = ("exit", AL)` and stops with the outcome `"exit"`, detail
`"terminate with code <AL> at 0x<addr>"`; `run_original` carries it into the new
`OrigResult.terminal` (None for every other stop). The same `int` with any other AH, every other
vector, and every `in`/`out` still stop `unmodeled`; with no `exits` flag (or no machine) the
terminate stops exactly as before, `("unmodeled", "int at 0x<addr>")` — all four pinned by
`G1ExitTests` (`tools/tests/test_diff_emu.py`, contiguous block, `test_g1_` methods).

**Why AL is the whole observable.** DOS function 0x4C takes one input, AL, the process return code,
and the process is then gone: its registers, stack, files and memory vanish with it, so nothing
else about the original's stop is anything's to compare — the code is the only fact that survives
the termination, and the chain's `push eax`/`pop eax` pair (0x62710/0x62720) proves the raw carries
precisely the caller's low byte into it (AH is rewritten at 0x62721). `("exit", al)` claims exactly
that and no DOS behaviour: no flush, no child cleanup, nothing past the int.

**The case runner (`tools/diff_verify.py`).** `Case` gained the `terminal` field (the declared
verdict) and `verify_spec` consumes it, matching in both directions (Review Focus 2: matched, never
merely permitted) on the model precedent of the `H2Case` declared difference (§H.3.2): a matched
declaration replaces the value comparison — the original's registers/calls/bytes stop inside the
chain and a returning port has nothing to hold against them — while the case still counts toward
block coverage (the exit arm is the point: exercised in a validation run of `verify_spec` on the
g1 fixture's one-case spec, hit 1/1 with a matched declaration), still reports
outside-image reads, still checks the declared `opens`, and still reports a port error; an exit
reached without a declaration, a declaration the run did not reach, and a wrong AL are each one
MISMATCH problem naming both sides. `verify_gap` is untouched: its cases must stop on exactly the
unmodeled instruction the gap names, so a gap row that turned `exits` on would fail its own check —
loudly, by design.

**Limits this creates (§H.4-style).**
- The port side needs nothing and gets nothing: like §H.1's `ins`, `exits` is emulator-only, no
  diffrun or `cases_text` change. On a matched-terminal case the port's return value is NOT
  compared, so a row whose only case is terminal detects no mutant (`mutant_detection` counts the
  per-case `diffs` the skipped comparison makes): rows keep non-terminating cases to catch their
  mutants and say so in the claim string — task 2's rows carry that.
- The stop is at the int: block coverage sees the chain up to 0x62723 only, and the model answers
  the int wherever it is reached — it does not prove the run arrived through 0x62003 (the game has
  the one site; a row's own path evidence is its regs/pokes and records, not this hook).
- A case that also models `keys` loses its `keys_left` comparison on the skipped case (same
  reason as the eax/writes skip).
- The run that exits reports the outcome "exit": the harness's two other consumers of the outcome
  are `verify_spec`'s blocked path (intercepted by the terminal branch above it) and `verify_gap`'s
  exact stop check (fails loudly, as stated), so no row silently mis-reads an exited run.

**Gates.** The full `--self-check` (both sides `--diffrun build/diffrun --exe data/game/C/PRAGE.EXE
--image <img> --self-check`) printed byte-identical output before and with the mechanism (a HEAD
copy under /tmp vs the working tree; both exit 0, same image bytes): the counter line stayed
`1000/1000 functions VERIFIED; 14885/14885 mutants detected; 1 named gaps; 638/831 rows with
callees closed (169 have none)` — no row uses `exits` or `terminal` yet, so nothing moved. The
mechanism's absence is felt: stashing only `diff_emu.py`/`diff_verify.py` turns `G1ExitTests` red
(errors=3), restoring turns it green. The harness suite
(`python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify`) ran 211 tests OK
with the mechanism. `PR_ORACLE_REQUIRED=1 ./build/run_tests` prints `all checks passed` — the first
worktree run failed only because `port/tests/ghidra_data.bin` (a git-ignored fixture) does not
exist in a fresh worktree; it was copied read-only from the main repository's working directory
into this worktree, where it stays untracked (git-ignored, never staged), and nothing C-side is
touched by this task.

## §G2 — save-row exit blocks + rowing 0x2EA68 (pending, lane A task 2)

## §G3 — the six non-rows (pending, lane A task 3)

## §G4 — game_loop's RNG pokes (pending, lane A task 4)

## §G5 — gp-keys-fight frame ratchet (pending, lane B task 5)

## §G6 — the mode-8 three-frame scan-out (pending, lane B task 6)

## §G7 — the gp-moves 2139 diagnosis (pending, lane B task 7)
