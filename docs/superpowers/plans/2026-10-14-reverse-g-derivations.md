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

## §G2 — save-row exit blocks + rowing 0x2EA68 (lane A task 2: done)

**Byte evidence.** Read twice and agrees, on 2026-10-14, from the fixup-applied image
(`./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_g2_img.bin`, capstone over it) and
from the live Ghidra instance (project `primal-rage`, program `/PRAGE.EXE`, `read_memory`); the raw
EXE bytes were not used (pre-fixup). §G1 already dual-read the chain `0x62003..0x62723`; this section
dual-reads what §G1 left to task 2 — the reachability of the rows' exit arms and the callees the
replayed chain meets — plus the new row's own entry:

- `0x2EA68` is ten bytes: `B8 01000000` `mov eax,1` and `E9 91350300` `jmp 0x62003` (0x2EA6D + 5 +
  0x33591 = 0x62003). It reads nothing: the message pointer its callers pass in EAX (e.g. `mov
  eax,0x80b54` 0x2FDCB — Ghidra shows the string `"Null Menu"` at 0x80B54) is overwritten before the
  tail jump and never dereferenced. Track S's callers already model it as the stub `SA_FATAL`
  (args `("eax",)`), and `menu_fatal_error` opens with `PR_SEAM(0x2EA68u, msg)` (menu.c:55), so the
  seam rule (§E3.3) was satisfied before this batch: `port/src` is unchanged.
- The arm into the exit on the load row is `0x1B025 cmp edx,0x7f8` (`81 FA F8 07 00 00`) `jne
  0x1B04D` (`75 20`), and 0x1B04D is `B8 01000000` `mov eax,1`, `B1 01` `mov cl,1`, `E8 AA6F0400`
  `call 0x62003` (0x1B059 + 0x46FAA = 0x62003) — as §G1 stated. The count EDX is fread's EAX
  (`89 C2` 0x1B01C), and the model answers the short count for a short file (§F.1), so the arm is
  reachable under the file model: l6 states a 0x7F7-byte file (one byte short of the 0x7F8 the body
  requests at `mov ebx,0x7f8` 0x1B00B / `mov edx,1` 0x1B00C) and the run terminates —
  `("exit", 1)`, `terminate with code 1 at 0x62723`, opens `[(b"cmos", b"rb")]` (probe, and the row
  with the case in it: 11/11 blocks).
- The save arm `0x1B0F7` is **not** reachable with the file model as built — named, not poked. Its
  only feeder is `0x1B0EF cmp edx,0x7f8` `je 0x1B105`, and EDX is fwrite's EAX (`89 C2` 0x1B0E6)
  whose three sources are the body's own constants: `mov ebx,0x7f8` 0x1B0CA (item count), `mov
  edx,1` 0x1B0DC (item size) and the buffer 0x100CE4..0x1014DB, wholly in the image; the model's
  fwrite (§F.1) returns the whole-items count of a memory-backed file — `0x7F8 * 1` — and there is
  no full disk to refuse it. An `F1_ABSENT` file under the write-open case is what s1..s3 already
  are: the open creates the file and the write of 0x7F8 bytes to a file that has no bytes yet is
  still whole. The brief's suggested forcing (s4's stale file) cannot make it short either: the
  model replaces the file at offset 0 with the write's bytes (§F.1's fopen bullet), never a partial
  transfer. The unhit reason in the save Spec now states exactly this and drops the old §S.7-era
  sentence (that an exit-reaching case would stop the original's run), which §G1's `exits` model made
  false.
- What a replayed chain actually meets (§G1 limit (b), now measured): `0x62003` and `0x6201B` run
  from their bytes (allowed; §G1's disassembly), the three cleanup vectors `[0xEF934]`, `[0xEF938]`,
  `[0xEF93C]` all hold `0x62002` (Ghidra: `02 20 06 00` ×3; the image agrees) and its one byte is
  `C3` `ret` — allowed and run, it returns to the site. The flush `call 0x6C4CF` 0x6271B **cannot
  run**: its stream table at 0xF0A0E (in the image, Ghidra-confirmed) holds two live close pointers
  — slot 0x000F0A0E: byte 0, byte 0x01, `call`-able `0x00061A47`; slot 0x000F0A14: byte 0, byte
  0x20, `0x00072B05` — and the flush walks the table, picks a slot and reaches `FF D0` `call eax`
  0x6C511: a replayed run stops there (`unmodeled indirect call to 0x72B05 at 0x6C511`, probe over
  the real image). It is therefore a stub — `SC_FLUSH = E.Call(0x6C4CF, (),
  clobbers=("ecx", "edx", "ebp"))`, the clobbers the §E3.10-item-3 analyzer derives from its bytes
  (`E.callee_clobbers`), re-pinned in `test_each_stub_declares_the_registers_its_callee_clobbers`
  (a shared entry: both exit-reaching rows stub the same callee and the harness requires one
  declared set). Its EAX (the stub answers 0) is discarded by `58` `pop eax` 0x62720, which restores
  the code the chain pushed — the caller reads no register of the flush; that is why one case value
  (0, the default) needs no per-case variation (§E3.10 item 4). With it, the chain reaches
  `B4 4C` 0x62721 `mov ah,0x4c` and `CD 21` 0x62723 and the run stops `("exit", 1)`.

**The load row converted (step 1).** Case l6 (7th) declares `terminal=("exit", 1)`; `sc_load`'s
`exits` flag sets `Machine(files=..., exits=True)` and the declaration together, so the case cannot
run one without the other. The Spec's exit block is now exercised: `unhit_named` emptied, coverage
10/11 → 11/11. The row's allow set grows by SC_EXIT (0x62002, 0x62003, 0x6201B) and its calls by
SC_FLUSH; the other six cases do not reach any of them, their runs are byte-identical to §F.1's, and
every mutant keeps its exact catching case set (the `SC_KINDS`/`SC_CASES` pins pass unchanged — see
the gates). The claim in the Spec comment names the skip: the terminal case compares no port value,
bytes or calls (§G1), the row's equivalence claim is the other six cases. The port side of the short
read is *not* left unverified repo-wide: `test_game.c`'s load case pins the failed-open behaviour
itself (a 0x7F7-byte file gives the 0xFF image and flag 1) — the row simply cannot compare it, and
AGENTS.md's weaker-list entry says so in those words (the row stays weaker, not because the block is
missing — it is no longer — but because one case compares nothing on the port side).

**The save row left named (step 2).** No case added; the 19 mutants and the s0..s4 sets are
untouched. The unhit reason rewritten above is the deliverable: evidence, not a shrug. The Spec
comment's pointer (see l6 in §F.1's table) is unchanged.

**The 0x2EA68 row (step 3).** `G2_SPECS` (placed after the SC rows it shares the chain constants
with — the file has no per-subsystem sections; `D1_SPECS += G2_SPECS` follows the batch convention):
`Spec("menu_fatal_error", 0x2EA68, [...], allow_calls=SC_EXIT, calls=(SC_FLUSH,), mutants=())`,
cases f0 (EAX = 0x80B54, a caller's real message pointer) and f1 (EAX = 0), each
`terminal=("exit", 1)`. The arg convention follows §E3.10 item 1 from the callee's bytes: the body
reads no register and touches no stack — EAX is its only input in the callers' convention (the
`SA_FATAL` stub's `("eax",)` is the same reading) — and its starting value varies between the two
cases precisely to show it is discarded: AL = 1 either way. The static scan of the tail jump follows
into `0x62003`, so the row covers two blocks (2/2, hit by both cases); the two `FF 15` sites in that
block are indirect *calls*, excluded from the jumps check by the §E3.7 rule, and the run resolves
them into SC_EXIT. The binding `b_g2_2ea68` (diff_runner.c, after the sc bindings, with the
`k_bindings` entry mask 0xFFFFFFFF): calls the ported `menu_fatal_error(r[R_EAX])` and answers
*eax = 0 — never compared (mask moot, §E3.10 item 7: the conservative default stated). The function's
`PR_SEAM` is inert on this row: the case text declares a stub at 0x6C4CF only, and `seam_hook` runs
a function whose seam address the case does not stub.

**No mutant on the 0x2EA68 row — the brief's expectation was impossible, ruled by the bytes.** The
brief's step 3 asked "mutant = the C body skipping its observable side". The body has none to skip:
menu.c:52-56 is the seam line and `(void)msg` — no store, no output, no return value anyone reads
(§N.1's own row would-compare-nothing argument). And a matched-terminal case skips the port's EAX,
writes and calls (§G1's limit, the same reason `mutant_detection` cannot see a difference: the skip
emits no `diffs`). Since the function *cannot* be called without exiting, every case of the row is
terminal and no mutant of the port's body could be caught by any case — the row would print NOT
DETECTED and the self-check would fail. The precedent is `0x18428`'s row: effect-free, `mutants=()`,
with the limit named in the Spec comment and this record — the same treatment here. The row is still
worth the +1 function it adds: it proves from the bytes that 0x2EA68 discards its argument and
carries exit code 1 into the modelled chain (the two starting values differ to prove discarding,
and `opens`/`executed`/`outside`/port-refusal checks still run on its cases), and it closes
`config_image_store`'s callee column (below). AGENTS.md lists it among the weaker rows, honestly.

**Re-measured counter and pins (step 4).** Base `1000/1000 functions VERIFIED; 14885/14885 mutants
detected; 1 named gaps; 638/831 rows with callees closed (169 have none)` → **`1001/1001 functions
VERIFIED; 14885/14885 mutants detected; 1 named gaps; 639/832 rows with callees closed (169 have
none)`**. Line by line: functions 1000 → 1001 (the new row, VERIFIED — its mutants=() adds none, so
the mutant figure is unchanged at 14885/14885 and every existing mutant still detects); named gaps
unchanged at 1 (the batch added no `gap=` Spec); with-callees rows 831 → 832 (the new row has
callees) and closed 638 → 639 — the single closure is `config_image_store` (0x2D498), whose only
callee was the 0x2EA68 stub, now VERIFIED by its own row; `config_storage_touch` and the other
SA_FATAL users keep their 0x2EA68 column flipped to VERIFIED but stay open on 0x61A70/0x65490/
0x654C7 (runtime, no rows); rows-without-callees 169 unchanged (1001 − 832). The full `--self-check`
diff against the recorded base output touches exactly these lines: the new row's table line, the
callee-column flips on 0x2D498/0x2D4EC rows and their mutants, the load row's case/block/callee
columns and its twelve mutants' columns, and the counter line — no other row's numbers, problems or
catch-sets moved (the pins and every other table line compare identical). The `0x2EA68` name is
added to the row-name and mask pins (G2_MASKS, merged into D1_MASKS) and `0x6C4CF` to the stub-
clobbers pin; the counter sentence re-pinned with a track G g2 comment naming the deltas.

**Review Focus #2, row-level.** The fail-first the plan asks of this task: with l6's `terminal`
declaration removed while the run still exits, `--function config_file_load` reports MISMATCH with
`l6: terminal: the original exit with AL=0x1 (exit: terminate with code 1 at 0x62723), the case
declares no terminal exit`; the declared row then passes (11/11 blocks, VERIFIED). The other two
directions on the real rows: declaring AL=2 on f0 — MISMATCH `the original exit with AL=0x1 ... the
case declares exit with AL=0x2`; and l6's machine without `exits` (declaration kept) — MISMATCH `the
original no terminal exit (unmodeled: int at 0x62723), the case declares exit with AL=0x1`. The
terminal verdict is matched, never permitted.

**Limits this section creates (§H.4-style).** SC_EXIT/SC_FLUSH allowances are per Spec, not global: a
future row that reaches the chain must name the same allowances (and the stub at 0x6C4CF says one
clobbers set across specs). The flush's modelled EAX 0 answers where the real close-callers would
have run — the row claims only that the chain discards it (`pop eax` 0x62720). A matched-terminal
case's block-coverage credit can make an unhit entry reachable in the table while its port side is
compared nowhere — the claim strings and AGENTS.md's weaker list carry that, not the counter. The
load row's l6 exercises the arm with a file one byte short only: other short lengths run the same
`jne 0x1B04D` (one equivalence class, plus the boundary). The `0x2EA68` row proves discarding and
the code, not that any caller reaches it (callers were §S.7's and §H's business; the menu rows name
their wrap arms unhit for the same exit).

**Gates.** Full `--self-check` with the pins green (the counter above, rc 0); harness suite
`python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify` — **211 tests OK** with
the conversion (the SC mutant catch-set pins pass unchanged); `PR_ORACLE_REQUIRED=1 ./build/run_tests`
— `all checks passed` (no `port/src` byte changed: the batch's C edit is the test-only diff_runner.c
binding and its table entry).

## §G3 — the six non-rows (lane A task 3; batches g3aa + g3b land all six)

**Rows landed: all six.** `0x500BB` (`dpmi_clock_read`, batch g3aa), and with batch g3b:
`0x1B544` (`res_resolve`, resident arm only), `0x29D60` (`hook_noop_29d60`), `0x2EA30`
(`interrupt_lock`), `0x32BAC` (`hit_sound`), `0x2EA64` (`ret_noop_2ea64`). §C3g.7's named
non-rows are now rows; the runtime non-rows (0x5D812, 0x5DEED, the AIL wrappers — all
`>= 0x5D000`, host libc per AGENTS.md, not porting targets) and the named gaps stay as they were.
The ported-function unrowed count drops 14 -> 8 (the 8 named gaps).

### Batch g3aa — 0x500BB (`dpmi_clock_read`)

- **The bytes** (the fixup-applied image, capstone at 0x500BB): `a1 00 15 10 00 c3` =
  `mov eax,[0x101500]; ret` — exactly the §H.2 claim, re-derived here. A leaf: it reads no register
  argument, writes nothing, and the bare `ret` (c3) pops only the return address. 0x101500 is the
  tick shadow the ISR tick advances (config.c `CFG_TICK_ISR 0x00101500u`).
- **The claim (one sentence)**: the row compares EAX at the return — the dword the original loads
  from 0x101500 — with the port's C read of the same dword, `DSD(DS_00101500)`, the inline form its
  callers use (flow.c 0x1E814/0x1E819 `call 0x500BB; mov [edx+0x10],eax` stores the full dword; the
  port has no C function at 0x500BB, so the binding mirrors that read — Review Focus #3's honest
  wording: a dword read, nothing more). Mask 0xFFFFFFFF for that caller analysis.
- **Cases** (`c0`/`c1`/`c2`) vary the dword — 0, 0x00C0FFEE (a typical tick, the D4D poll sentinel),
  0xFFFFFFFF (high) — and seed 0x101504 apart in every case, so a wrong-address read always differs.
  Blocks 1/1.
- **Mutants** (2): `@off4` returns `DSD(0x101504)` — caught by all three cases; `@zero` returns 0
  without reading — caught by c1/c2 (c0 agrees at 0 by design). The exact caught-by sets are pinned
  in `G3AA_CASES` (`tools/tests/test_diff_verify.py`).
- **Closure arithmetic** (measured on the lane at `make diff-verify`'s full `--self-check`): closed
  rows 639 -> 666. All 27 are rows whose callee 0x500BB flipped to VERIFIED-by-its-own-row with
  every other callee VERIFIED — §C3g.7's dependents split 27 closed + 20 still open on other callees
  (0x1B544, 0x65431, the AIL wrapper stubs; `snd_sample_queue` is among those 20, so §C3g.7's
  "open solely on 0x500BB" list closed 3 of its 4 named rows here). Counter: functions
  1001 -> 1002, mutants 14885 -> 14887, counted-apart rows 169 -> 170, named gaps 1, the
  with-callees count unchanged at 832 (the new row has no callees).
- **Allow callers are untouched**: rows that `allow_calls=(0x500BB,)` keep running it from its bytes
  (allow mode needs no seam, record §E3.10); the row only records the callee verdict — the precedent
  is 0x33950, allowed and rowed (`fighter_ctx_same`).

### Batch g3b — the remaining five, in order (bytes re-derived with capstone on the
fixup-applied image `/tmp/pr_g3a_img.bin`, never from the dispatch one-liners)

**`0x1B544` — `res_resolve`, the RESIDENT ARM ONLY.** Bytes `53 51 52 56 89 c6 c1 e8 17 8d 0c 85
00 00 00 00 01 c8 8b 0d e0 14 10 00 c1 e0 02 89 f2 01 c1 81 e2 ff ff 7f 00 f7 41 0c 00 00 00 01
74 0a 8b 41 10 01 d0 5e 5a 59 5b c3`: index = handle >> 23, entry = [0x1014E0] + index*0x14,
`test [entry+0xc],0x1000000` at 0x1B569 (the PRELOAD/resident bit), resident arm 0x1B572
`mov eax,[entry+0x10]; add eax,(handle & 0x7fffff)`, four pops, `ret`. Claim: EAX at the return —
the resolved pointer, payload base + the handle's low 23 bits — against the port C's `res_resolve`
(res.c:300) reported as its mem[] offset (camera_res_off's convention); mask full (the callers
dereference it, 0x1818D `mov ecx,[eax+2]` — camera_resolve_sprite's analysis). All cases seed the
resident bit with `d4b_res()`, so the port's guards (index<count, data!=0, PRELOAD skips present)
never fire and the arms agree. The non-resident arm — the nine leaders 0x1B57C..0x1B5E0, the clock
stamp `call 0x500bb` (0x1B58E/0x1B5EE), the allocate arm `call 0x1e774` (0x1B5BD), the fatal
`call 0x1d290` (0x1B5DB), the present tail `call 0x1b3ac` (0x1B5E9), which res.c's PORT comment
says have no port analogue — stays named unhit via `D4B_RES_SLOW` (whose wording this batch
updated for the rowed state). Cases c0..c3: entries 1/2 × offsets 0x40/0x100/0x400001 (bit 22
set)/0; mutants @idx22 (index from bit 22 — caught c0..c3), @noand (offset unmasked — caught
c0..c3), @base (offset dropped — caught c0/c1/c2, passes c3 by design). Blocks 2/11, 9 named.

**`0x29D60` — `hook_noop_29d60`, a bare `ret`.** One byte `c3`: reads nothing, writes nothing,
pops only the return address; no `call` — no seam, no callee machinery. The hook table 0x104AE4
stores it (0x43738/0x444C8, flow.c) and the port registers NO function at 0x29D60 — the fn_resolve
miss is a no-op (flow.c:1059 "a miss is a no-op only for 0x29D60 and 0x5D812"), which IS the bare
ret's semantics: the caller's registers survive. The binding mirrors that: entry EAX passthrough,
nothing written; mask full (the `b_d1d_4f1d0` precedent — "the void C function cannot change its
caller's value; record §1"). Cases 0 / 0x00C0FFEE / 0xFFFFFFFF; @zero caught by c1/c2 (c0 agrees
at 0 by design), @scrub (high half dropped) caught by c1/c2. Blocks 1/1.

**`0x2EA30` — `interrupt_lock`, the inert lock.** Bytes: `push edx; mov dh,[0xbcD60]; mov dl,dh;
cmp eax,0x2700` — the restore arm returns the OLD byte (`xor eax,eax; mov al,dh` ... `mov al,dl`)
and 0x2EA49 `xor dh,dh` clears the lock iff it was 1; the request arm `cmp eax,1`/0x2EA56
`mov dh,1` sets it and returns the NEW byte; any other request leaves DH alone. Both join at
0x2EA5C `mov [0xbcD60],dh; pop edx; nop` and fall through the SHARED `ret` at 0x2EA64 (which is
why 0x2EA64 is separately callable). ONE global, 0xbcD60 (DS_000BCD60 — symbols.h names it,
port/src uses it NOWHERE); EDX preserved by the push/pop pair; the returned dword always
zero-extended. The port models the callee inert at every site (actors.c:1124/1164,
fighter.c:2495/2502, flow.c:197) — the row compares the INERT binding (entry EAX passthrough,
nothing written) against the original in the agreement region only: r0 acquire-while-held (req 1,
lock 1), r1 restore-of-clear (req 0x2700, lock 0), r2 other-request (req 0x1234, lock 0x34). The
region is exact: a case compares only if the original leaves every byte at its start value AND
returns the lock byte, whose only value equal to the entry request's low byte is the one each
case seeds (restore of a 0x80 lock, e.g., stores back without changing memory but returns 0x80
where the inert passthrough reports 0 — an EAX-only disagreement; the clear of a SET lock changes
the byte — a memory disagreement). Mask 0xFF is therefore the widest mask the inert port supports:
it compares the returned lock BYTE (r0 1, r1 0 = the low byte of 0x2700, r2 0x34 = the low byte
of 0x1234) and nothing above it. @store1/@store0 (inert bindings that store the lock anyway) are
caught by r1/r2 and r0/r2 as `byte` kinds; @zero caught by r0/r2 as `eax`. Block 0x2EA49 (the
clear of a SET lock) is the one block whose store CHANGES memory — the inert port cannot agree
with any case reaching it, so it is named unhit with that byte evidence, not claimed. 7/8 blocks,
mask 0xFF.

**`0x32BAC` — `hit_sound`, a bare `ret`.** One byte `c3` at 0x32BAC — NOT the tick-read the
dispatch one-liner claimed: the §H.2:169 passage is about 0x500BB inside the 0x2EA78 key-loop
analysis and track H never mentions 0x32BAC; raw wins. (The 44-byte extent record §0.3.4 gives
0x32BAC is the orphaned 0x32BB0 body, whose callers are elsewhere — fighter.c:4334 already carries
this finding.) Unlike 0x29D60/0x2EA64 this non-row HAS a ported C — `hit_sound` (fighter.c:4338,
PR_SEAM-armed, inert body, "the raw entry the chain calls is a one-byte RET") — so the binding
CALLS it and reports the entry EAX; the seam records nothing (this row has no stub of 0x32BAC,
seam_hook passes through — the green run shows the empty call list agrees both sides). Mask full
(the d1d passthrough precedent). Cases 0 / 7 (a channel byte, the callers pass `DSB(slot+0x7A)`) /
0xFFFFFFFF; @zero caught by c1/c2, @plus1 caught by all three (c2's wraps to 0). Blocks 1/1.

**`0x2EA64` — `ret_noop_2ea64`, a bare `ret`.** One byte `c3`; the `pop edx` at 0x2EA62 and `nop`
at 0x2EA63 belong to 0x2EA30's body, which falls through — entered DIRECTLY, as its callers do,
only `ret` runs. Its references are exactly 8 direct `call`s and nothing else (Ghidra: 8
UNCONDITIONAL_CALL xrefs, zero data xrefs) — four in the 0x2B2A0 dispatcher
(0x2B308/0x2B52F/0x2B771/0x2B8E8), two in 0x2FA40 (0x2FAEC/0x2FD75) and two in 0x2FFC4
(0x300B0/0x30486); 0x2EA65's `8d 40 00` is alignment the ret never reaches. The rows of the
three ported callers (`spawn_anim_opcode`'s `allow_calls`, `menu_step`/`menu_run`'s
`D5G_MENU_ALLOW`) already run it from its bytes, so this row only adds the callee's VERDICT to
the closure ledger — and the ledger rule counts an allowed callee by its row (record §E3.4),
which is why `spawn_anim_opcode`, open on {0x29D60, 0x2EA64}, closed last, with this landing.
The port has no C at 0x2EA64 — all eight call sites inline the elision (actors.c:2369/2466/
2559/2619, menu.c:196/246/322/392) — so the binding mirrors that: entry EAX passthrough, nothing
written; mask full. Cases 0 / 0x00C0FFEE / 0xFFFFFFFF; @zero and @scrub caught by c1/c2.
Blocks 1/1.

**Batch g3b closure arithmetic (measured on the lane, full `--self-check`, each row's delta
enumerated from the printed table by callee verdict).** Counter 1002/14887/666/170 ->
1007/14899/716/175: functions +5 (the five rows), mutants +12 (3+2+3+2+2), counted-apart +5
(none of the five rows has a callee; the with-callees count stays 832), named gaps 1. Closed
rows per row, in landing order: **0x1B544 +22** (camera_box_clip/overlap/project/projectile_hit/
projectile_hit_real/resolve_sprite/sprite_height, effects_spawn/darken/pulse/scroll,
fighter_spawn_slot, gfx_flush_palette, palette_acquire/reflow, render_list, render_scroll_scene_a/b,
sprite_node_build/_1c528, text_blit_glyph/_rr); **0x29D60 +15** (fight_char_screen_setup,
flow_challenge_open, flow_round_hud_init, frontend_continue_tail/pause_tail, game_mode_0e_step/_rr,
game_mode_23_step, game_state_6/_rr, game_state_select/_rr, game_state_step_high,
game_state_timer_exit, hit_reaction_apply); **0x2EA30 +6** (actor_alloc, fighter_state_33b00,
flow_challenge_poll, frontend_input_reset, release_record, render_scroll_setup); **0x32BAC +6**
(frontend_mode12_darken_or_tally/_rr, game_mode_09_step/_rr, hit_3d004, hit_chain_resolve);
**0x2EA64 +1** (spawn_anim_opcode — it blocked on {0x29D60, 0x2EA64} and closed only with the
second). The static `closed_rows` replay over the g3aa-base table (flip-one-at-a-time) predicted
+22/+15/+6/+6/0 and +50 in all-five; the sequential measurement matched every row (0x2EA64's
measured +1 = the replay's all-five-minus-flip-0x2EA64 difference, the spawn_anim_opcode pair-up).
No closure was lost anywhere (each row's run closed a superset of the previous).

**The corrected expectation, stated exhaustively.** The per-row closure deltas above are the
measured truth; the dispatch one-liners' expectations were not all right — the "exactly ONE
dependent closes on 0x29D60 (game_mode_23_step)" claim measured **+15**, and the brief's
"tick-read twin of 0x500BB" for 0x32BAC is wrong on the bytes (a bare ret). What the static
analysis at head got right: the six rows' dependents do NOT all close. The +77 measured across
all six rows (27 in g3aa + 50 in g3b, 639 -> 716) is EXACTLY the set of dependent rows whose
blocker set was some subset of the six. The still-open census, measured from the final table:
**116 open with-callees rows, ZERO of them list one of the six as an open blocker** — each still
has an unrowed callee. **96 of the 116 have at least one unrowed >=0x5D000 runtime callee**
(79 block on those alone): the complete measured set is 0x5D218, 0x5D28F, 0x5D812, 0x5D86A, the
AIL wrappers 0x5DC0F/0x5DC2A/0x5DC4D/0x5DC70/0x5DC8B/0x5DCA6/0x5DCC5/0x5DCE4/0x5DD03/0x5DEAF/
0x5DECA/0x5DEED, 0x61A4C/0x61A70, the exit chain 0x62002/0x62003/0x6201B/0x6C4CF, and the
WATCOM/DOS-4GW helpers 0x653A1/0x653ED/0x65431/0x65490/0x654C7/0x65533/0x65546/0x655FF/0x65624/
0x6C8EB/0x6CB6C/0x6CC9F/0x6CCFA/0x6CE84/0x72CD6/0x72D20 — host libc per AGENTS.md, not porting
targets. **The other 20 block only on unrowed sub-0x5D000 targets**: the named item stub 0x2CB74
(menu_step, menu_run — the only two of the 47 0x500BB dependents not blocked on a >=0x5D000
callee), the host-owned classified pair 0x10D34/0x4FB98 (game_shutdown; 0x10D0C/0x1C308 likewise
co-block rows that also have runtime callees), and call targets never listed as functions in the
decomp index — E2 triage's field: 0x24568/0x24804/0x40CB0 (the game_mode_05/0d/32 rows),
0x29D04/0x3D0BC/0x3E484/0x3E4C4 (the fighter_19020/35050/pass_a(_al)/state_350d0/state_3531c/
think_side/winner_body rows) and 0x51E58 (sprite_blit; the same class — 0x521F0/0x5222C/0x529A0/
0x529C4/0x52A18/0x57F80/0x58564/0x5859C/0x58CBD/0x58D2C/0x58D4C/0x58D90 plus 0x5D218 — keeps
sprite_blit_aperture open, and 0x4F5C8 co-opens game_loop). The aperture store 0x501A3
(classified host-owned, record §K9.1) never stands alone: it co-blocks game_loop (with the
0x4F5C8 stub and the 0x5D812 allow) and the
config_screen_wait pair (with the 0x655FF allow). Of the 20 dependents still open after g3aa,
exactly ONE closed in this batch — game_mode_23_step (it blocked on {0x500BB, 0x29D60}); the
other 19 all still have >=0x5D000 blockers (snd_sample_queue on the AIL wrappers, the
game_frame_keys pair and game_quit_prompt on 0x65431, the fight_char_text_clear/blink/
team_text_blink family and game_mode_25_reveal on 0x65624, frontend_mode12_advance on
0x65490/0x654C7, flow_26d4c/config_key_name/svc_buttons_draw on the 0x65533/0x65546/
0x6Cxxx/0x72xxx family, the config_screen_wait pair on 0x655FF), except menu_step and menu_run,
blocked on 0x2CB74 alone. No further closure can come from the six rows: the closure-rule
amendment (counting a row closed when its only open callees are runtime/gap callees that are run
from bytes in allow mode) is a separate decision (recon-01 item 4) and stays open after this
batch.

## §G4 — game_loop's RNG pokes (pending, lane A task 4)

## §G5 — gp-keys-fight frame ratchet (pending, lane B task 5)

## §G6 — the mode-8 three-frame scan-out (pending, lane B task 6)

## §G7 — the gp-moves 2139 diagnosis (pending, lane B task 7)
