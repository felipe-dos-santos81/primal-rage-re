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
  requests at `mov ebx,0x7f8` 0x1B006 / `mov edx,1` 0x1B00B) and the run terminates —
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

## §G5 — gp-keys-fight frame ratchet (lane B task 5)

**Measurement before any change (Task 5 Step 1).** Base `5f874678`, worktree
`reverse-g2`, `make build`; `make gp-report scenario=gp-keys-fight` (the `PR_GP_DUMP`
replay + `tools/gp_compare.py --report`) over `data/k11-captures/gp-keys-fight`
(`poll.log` sha256 `8425afbc46d51173532f6f4c27a8f16c594e2bc572b4273e056bdbf51a9678ae`,
re-verified on disk, 1166 frames — the same capture U11 pinned; the
`GP_KEYS_CAPTURE_SHA256` pin is NOT re-pinned)
and its dump at `/tmp/pr_gp_dump/gp-keys-fight` (789 `.ipx` frames, `frames.txt` 00592 =
`f=0x832`, 00593 = `f=0x833`, last 00788 = `f=0x872`; the replay's script ends at the
capture's X record `f=0873 step=19 end`, poll.log line 2223). The report lines equal
§K.12's Step-6 lines byte-for-byte on the shared fields — nothing moved between the U11
measurement and this base:

```
gp_compare: gp-keys-fight: frames: window from capture 100 (raw 1747); 1028 classified: 660 clean, 361 splice, 2 transition, 5 unexplained, 12 all-black
gp_compare: gp-keys-fight: frames: FIRST UNEXPLAINED capture 835 (raw 3934): nearest port 593, rows 0..91, x 0..319 (17138 px)
gp_compare: gp-keys-fight: trace: 1841 frames compared (f 13A..), 8 without a capture snapshot; first tick difference f=13B (reported, not ratcheted)
gp_compare: gp-keys-fight: trace: 0 differing through 2163
```

A full pass (the report caps at `REPORT_MAX` 5 unexplained) gives the complete unexplained
set of the capture: **[835] + [1136..1165]**. 1136..1165 are the capture's 30-frame tail
after the replay's end — the restart's boot movies (§K.12 "Captures 1136–1139 …"), which
the port's script never ran, like gp-twop's 612 and gp-charsel's 516: how far the port
got, not a divergence. Inside the replay's range the ONLY unexplained frame is 835.

**The pause/ABANDON/QUIT screens without any allowance.** An open pause freezes `f`
(§K.10 item 3), so its screen simply repeats in the capture and repeats of a dumped port
frame classify clean. Measured (a per-frame classification pass over the dump, no
allowance): capture 780..834 (the pause `c=0x809`, alt-s, and the ABANDON-prompt path
`c=0x81E`, ending just before the altq-n event at `c=0x833`): 25 clean, 30 splice, 0
unexplained; capture 836..899 (past the QUIT answer): 25 clean, 38 splice, 1 black, 0
unexplained; capture 1090..1135 (up to the replay's end): 35 clean, 9 splice, 2 black, 0
unexplained. So the pause and ABANDON screens need NO allowance, and everything after the
835 splice (the QUIT band's own rows aside) needs none either — 835 alone blocks a ratchet
past it.

**The 835 byte anatomy (re-measured from this dump; §K.12's finding holds exactly).**
Capture 835 (raw 3934): rows 0..80 (bytes 0..77759) are byte-equal to port 592
(`f=0x832`) and capture 834 equals port 592 whole; rows 92..199 (bytes 88320..191999) are
byte-equal to port 593 (`f=0x833`) and capture 836's rows 0..91 equal port 593. The band
rows 81..91 (bytes 77760..88319, sha256
`d9c0ac62f7e0c0d4082fba8d195eae4f767c56a7887bdf7346450511f998cad8`): of its 10 560 bytes
1918 equal BOTH bound frames at the same offset, 5714 equal only 592's, 486 equal only
593's, and **2442 equal NEITHER** (per-row neither-x-ranges: x 56..187 in rows 81..88,
x 0..189 in rows 89..91 — around and in Sauron's sprite, no prompt text). No splice point
s makes the band `592[:s] ++ 593[s:]`; no port frame in the whole 789-frame dump holds
those 11 rows. The band is therefore **not port-derivable** — it is the third, never-dumped
present state (§K.10 item 2: `0x2EA78(-1)` presents the prompt's one frame with text and
the dump holds one frame per `f`; §K.12's other candidate, a sprite drawn during scan-out,
stays open — the cause is not isolated, and this task does not decide it). Exactly ONE
adjacent pair in the dump matches the top+bottom binding: (592, 593).

**Decision (Step 2): explain 835 with a named per-frame allowance.** Leaving the ratchet
below it (N = 835) would claim the pause and ABANDON frames but stop at the QUIT event;
the allowance (band pinned byte-exactly, top/bottom byte-bound to the port's CURRENT dump,
unique-pair required — see `tools/gp_compare.py` `BANDS_ALLOWED`) explains exactly this
one frame at exactly these bytes and lifts the claim to the script end. The band pin is
seconded by the capture-identity pins (`--capture-sha256`/`--capture-frames` reject any
other capture before comparison starts).

**The pins (Step 3).** `GP_KEYS_MIN_FIRST = 1136` (measured: with the allowance active the
enforced pass reports `first unexplained 1136, ratchet N 1136 ok` — 1136 is the end of the
port's script, per the precedent above, not a divergence; raise only when the script is
lengthened). `GP_KEYS_MAX_START = 100` (window start, raw 1747; the slide guard).
`GP_KEYS_TRACE_MIN_FIRST = 2164` (the trace's compared range ends at the replay's `f =
0x873`, so 0x873 + 1 = 2164 = the exact pin: `0 differing through 2163`).
`GP_KEYS_CAPTURE_FRAMES = 1166` (measured count of `frame_*.raw.gz`, §K.14's 1166).
`GP_KEYS_CAPTURE_SHA256` stays `8425afbc…678ae` — same capture, NOT re-pinned. No moves
pin: the capture records the move fields and the report pass showed 0 differing, but
pinning the moves claim is outside this task's frame-ratchet scope (gp_compare runs it
only with `--moves-min-first`). The limits of the claim are the standard gp_compare ones
(record gameplay-ground-truth §G.16, §K.12): it cannot detect an under-rendering port,
the order of the port's frames and that every port frame appears are not claimed
(coverage is reported: 13 non-black port frames up to 788 unexhibited), frames at/after
the pinned window start are classified from the port's own dump, and the band of 835 is
capture-pinned bytes with no port binding — its content and cause stay the named gap of
§K.10 item 2 / §K.12. §H.4 carve-out: this allowance injects no keys (the replay script
and its key count are unchanged) and reads no screen/data overlap bytes — the §H.4 hang
limit and `[0xA0000,0xAFA00)` overlap do not bind it, named here per the plan's rule.

**Implementation (Step 2/3).** `tools/gp_compare.py`: `BANDS_ALLOWED` (per-scenario named
per-frame allowance list, the `title_compare.ATTRACT2_SPLICE3_ALLOWED` style — never a
class), `band_pair` (the full-dump search for the unique adjacent port pair that byte-binds
the capture frame outside the band) and `band_explain` (band sha256 first, then the pair;
zero or multiple matches reject); `frame_claim` consults the scenario's entries only when
the two-frame model leaves a named frame unexplained, prints `allowed by name as a band
splice` on success, a loud `FAIL: allowed capture <j> is not its named band splice`
otherwise (counted into the enforced rc via `allow_bad`), and a `retire the name` note when
the model explains a named frame again (the 3545 precedent). The classification becomes
kind `band`, exhibiting P and P+1 (the same exhibition rule as a splice), and the summary
line gained the `band-allowed` count. `exhibited`/`main` carry the table keyed by scenario
name; every other gp scenario passes no bands → unchanged, and no other tool parses the
changed summary line (`gp_win.py` uses only `ratchet`/`trace_claim`; k11/title/attract
print their own lines). The Makefile target runs `gp_compare` after the two `gp_keys`
steps with the pins above; `make gp-keys-oracle` green (rc=0): `captured frame 835 (raw
3934) allowed by name as a band splice: port 592/593 rows 0..80/92..199, band rows 81..91
the named bytes`; `1025 classified: 660 clean, 361 splice, 2 transition, 1 band-allowed,
1 unexplained, 12 all-black`; `first unexplained 1136, ratchet N 1136 ok`; `trace: 0
differing through 2163; ratchet N 2164 ok`; `poll.log sha256 8425afbc..9678ae, 1166
frames: matches the pin`.

**Review Focus #1 / Step 4 proofs.** Unit (`tools/tests/test_gp_compare.py BandAllowance`,
6 cases, in the `make verify` gp tool-test step): the named bytes explain; a mutated
BOTTOM-bind port frame un-explains (the allowance FAILs and the ratchet catches it);
different band bytes over the same top/bottom are not the named frame; two matching pairs
(ambiguous) are not the named frame; a model-explained named frame prints the stale-name
note; and the table's `gp-keys-fight` entry is pinned, so a silent name change is a test
failure. Disabling the mechanism (bands forced empty) fails 5 of the 6 — the tests can
fail. Real data on a COPY of the dump (the original dump and the capture untouched):
poking port 592's row 40 index byte breaks 835's top bind — `band_explain` on capture 835
answers `no unique adjacent port pair binds its rows [0,81)/[92,200)`, and the enforced
run FAILs: rc=1, `FIRST UNEXPLAINED capture 834 (raw 3933): nearest port 592, rows
40..40, x 160..160 (1 px)` with `first unexplained 834 < ratchet N 1136` (834's clean
sample of the mutated frame breaks with it). Poking port 593's row 150 breaks only the
bottom bind: the enforced run stops exactly at 835 with rc=1 and the allowance's own
`FAIL: allowed capture 835 ... no unique adjacent port pair` line. Step 4 on a PLAIN
explained frame (not the allowed one): a single index-byte poke on port 300 was absorbed
by the splice model (the neighbour frame holds the old pixel — the two-frame model's
documented power, not a hole in the allowance), so the recorded proof pokes port 300's DAC
entry for its most-used index (52, 2284 px): every capture frame exhibiting that frame's
exact bytes breaks, the enforced run rc=1 with `FIRST UNEXPLAINED capture 479 (raw 2970)`
(the first frame exhibiting port 300) while `band_explain` still returns P=592 — the
failure is the plain regression, not the band. The allowance silences exactly one capture
frame at exactly its named bytes.

**AGENTS.md.** The keys-paragraph claim sentence moved from "no frame is compared (the
pause and prompt screens are a named gap)" to the narrow frame claim with its named
limits (the 835 band's cause, the script-end N, under-render/order/coverage unclaimed).

## §G6 — the mode-8 three-frame scan-out (lane B task 6)

**Base and measurements.** Base `93b24171` (`reverse-g2`), `make build`; the three
re-measured with fresh `make gp-replay scenario=…` runs on this tree — dumps at
`/tmp/pr_gp_dump/{gp-idle-loss, gp-u9-win, gp-u10-ending}` (6301 / 1977 / 4664
presented frames; every `fn-miss` set on its pinned known-set, `all checks passed`).
The per-frame tick log the brief asks for is the driver's own `frames.txt` tick
column (one line per presented frame: `index f=… tick=… mode=…`) — no extra
instrumentation was needed, and its ticks advance **exactly +1 per frame** across
every stretch (idle-loss 1665..1670: 0xCCE..0xCD3; U9 215..221: 0x4E1..0x4E7; U10
213..221: 0x4DA..0x4E2).

**The composite anatomy, re-measured per row against these dumps** (a row-run pass:
for each stored capture frame, which dump frames equal each row exactly):

- **gp-idle-loss capture 2064 (raw 5190)**: rows 0..2 = port 1665/1666 (f=0xC70/0xC71,
  identical there), rows 3..97 = **port 1669 (f=0xC74) exclusively**, rows 98..199 =
  **port 1670 (f=0xC75) exclusively**. `window.txt` 2062..2065 = raws 5185, 5187,
  5190, 5191: raw 5186 was deduplicated against 5185 and raws 5188/5189 against 5187,
  so the original held **port 1666's screen for three consecutive scan-outs** (~43 ms
  of refresh time) and then put **two new frames' blits inside one scan-out**. Port
  1667/1668 (f=0xC72/0xC73) differ from every neighbour (row-anatomy `next_diff`) and
  match no row of any capture frame in the window: their blits never reached VRAM in
  any form the CRTC could show.
- **gp-u9-win capture 346 (raw 2830)**: rows 0..4 = port 218 (f=0x48A, the first
  mode-8 frame, held by dedup across raws 2827..2829), rows 5..199 = port 221 (f=0x48D)
  exclusively. Ports 219/220 (f=0x48B/0x48C) skipped; capture 347 (raw 2831) shows
  port 222 whole — the next tick's blit landed after this scan-out ended, so this
  composite is two segments.
- **gp-u10-ending capture 331 (raw 2831)**: rows 0..1 = port 216 (f=0x483, the first
  mode-8 frame, held across raws 2828..2830), rows 2..63 = port 218 (f=0x485)
  exclusively, rows 64..199 = port 219 (f=0x486) exclusively. Port 217 (f=0x484)
  skipped; capture 332 (raw 2832) = the 219/220 splice — normal cadence resumes.

**The original-side cadence, from each capture's own `poll.log`** (S records are
logged only while the loop is caught in its spin state `t50c - 1 == t508`,
`gp_capture.py` `spinning`/`accept`; P records mark (mode, st) changes):

- idle-loss: `S f=0C70 ms=74174 tick=0x10F1 t508=0x544` → `S f=0C74 ms=74250
  tick=0x10F5 t508=0x548` → `S f=0C75 ms=74255 tick=0x10F6 t508=0x549`; no S for
  f=0C71..0C73; `P f=0C71 mode=8 ms=74192` (the poller saw f=0C71 **not** in the
  spin state — it could log only the P). Δtick = Δt508 = Δf = 4 across the stretch,
  then 1/1; the f=0C74→0C75 pair was observed 5 wall-ms apart.
- U9: `S f=0489 … tick=0x908 t508=0xCE` → `S f=048D ms=40520 tick=0x90C t508=0xD2`
  (Δ4/Δ4/Δ4 across the stretch), `P f=048A ms=40483 tick=0x909` (tick 1:1 on the
  mode-change frame); no S for 048A..048C.
- U10: `S f=0482 … t508=0xCE` → `P f=0483 ms=40492 tick=0x90C` → `S f=0485 ms=40534
  t508=0xD1` → `S f=0486 ms=40538 t508=0xD2`; no S for 0483/0484.

At every observed endpoint the ISR counters advanced exactly 1 per frame — **no
counter jump anywhere** — and the missing S records are the loop being caught
outside its spin state (or the poller starved by the same host CPU; the 76 ms and
55 ms observation gaps are the poller's wall clock, the blit structure below comes
from dedup'd raw indices, which count guest refreshes, not wall time).

**The loader re-sync hypothesis, tested and rejected** (0x1B45F/0x1B464 — the
mechanism the attract2 3545 name rests on, Makefile:288-295, title_compare.py:510-524).
Three independent grounds on the unpoked gp-idle-loss evidence; U9/U10 share the
shape (same mode-6→8 entry, same anatomy, same cadence):

1. A mid-game INDEX read goes through 0x1B3AC's draw=1 lazy path, which **always
   ends with a blit**: the loader draws string 489 (`- LOADING -`) into the aperture,
   the disk read stalls the loop with the ISR ticking, 0x1B464 re-syncs t50c := t508
   and the gate (0x25643) then passes on the reading frame — res.c's own comment
   names that captured presents ("without it the loader's frame — which the capture
   holds — would never be presented"). Every hold window here is **byte-identical
   across its raws** (5188/5189 dedup'd against 5187; U9 2828/2829 against 2827;
   U10 2829/2830 against 2828): no blit landed during any hold, so no read completed
   inside it.
2. **The mode-8 entry path reads no INDEX resource at all.** The port implements the
   same dispatch and models every draw=1 read as a tick advance (res.c); the port
   replay's tick column advances exactly +1 per frame across each stretch — a read
   anywhere in f=0C71..0C75 (or the U9/U10 analogues) would jump it. 0x1B3AC is not
   on the path.
3. A re-sync changes the *relation* between the counters (t50c pulled up to t508
   mid-frame, an observable t508-Δ vs f-Δ asymmetry). Observed on all three captures:
   Δtick = Δt508 = Δf = 1:1 at every endpoint.

**The mechanism: the master loop's own gate + catch-up (0x25643 / 0x256C5).** The
ported and differentially-verified loop (flow.c:7141-7193): each iteration computes
(`game_frame` 0x24C5C, then the render passes), blits **only when t50c == t508**
(sort, render, flush, aperture→VRAM copy, swap — all inside the gate), then t50c++
(0x256C0) and spins while `t50c - 1 == t508` (0x256C5). Steady state: one present
per ISR tick, its phase inside the 16.65 ms tick window equal to that iteration's
own compute time W. When an iteration's compute exceeds the tick gap the gate
**fails** (t508 has advanced past t50c): that frame's present is skipped entirely
and the spin condition fails too — the loop burns the backlog without waiting, and
no blit touches VRAM until t50c catches t508 again. Mode-8's entry chain is exactly
such a stretch: `game_mode_08_step` (0x28468) walks fight_slot_clear (0x3C5CC), the
two camera_screen_base/camera_project passes (0x16D58/0x17FA0), camera_decay, the
fighter passes, fight_slot_pass, two fight_hud_pass walks, fight_effects_pass
(0x49C78) and camera_y_commit, plus the mode-change setup inside `game_frame`; the
skipped-iteration chain's compute spans ~2–3 tick intervals on every one of the
three captures. The reconstruction that fits the idle-loss bytes and counters:
0C71 blits normally (the held screen, raws 5187..5189), 0C72's compute overruns its
tick and skips its present, the backlog carries 0C73 (also skipped) and 0C74 —
whose gate re-opens late in tick 0x548's window (≈10–16 ms in: the skipped chain's
compute tail), blitting 1669's buffer at raw 5190 row ~3 — and 0C75's blit follows
≈5–7 ms later just after tick 0x549 (a light iteration at the natural post-tick
phase), inside the **same** scan-out. 14.27 ms refresh < 16.65 ms tick: two
sub-tick-spaced blits fit in one capture frame and the rows above them still show
the held screen; U9's tail blit (0x48E) instead landed after raw 2830's scan ended,
hence two segments there. Which skipped iteration first overruns a tick is **not**
isolable from the byte record (no S inside the chain) and the allowance does not
depend on that attribution.

This is the same mechanism family task 5 named for 3545 — "a present that follows a
re-sync … or a catch-up (the gate skips presents with no spin while DS_0010150C is
behind) lands at an arbitrary phase of the ISR tick" (title_compare.py:517-521) —
and idle-loss's instance is the **catch-up arm driven by the mode-8 entry's own
compute**, with no read, no re-sync and no poke: its 2064 rows come from the clean
fight→results transition, as required. **Not a port divergence and not a wrong
port tick model**: the port's gate/catch-up C is the raw's (differential rows), its
driver advances exactly one modelled tick per spin, host-side work never overruns a
modelled tick in a replay, so the port presents every f (its dump, keyed on new
displayed images sampled at the pump and loader hooks, can hold MORE than one
image per f but never fewer — the model part below records the U9/U10 dump lines
that showed this, correcting the shorthand this paragraph first carried), and it
can never express the skipped presents the original's mode-8 entry had — a
comparison-model limit, now with its
cause isolated (§W.12/§W.14's suspicion confirmed, escalated only in the sense of
"isolated, modelled by name"; the plan's MISMATCH-escalation carve-out for a wrong
port tick model does not apply).

**Bound measurements for the named allowance** (full-dump searches over these
current dumps; the frames themselves and their row boundaries are as above):

| capture frame | top rows [0,r1) equal port | unique adjacent pair (B,B+1) binding [r1,r2)+[r2,200) | top-segment sha256 |
|---|---|---|---|
| gp-idle-loss 2064 (r1=3, r2=98) | {1665, 1666} (6301 frames scanned) | **(1669, 1670)** — exactly one | `7bdf2c561c38a264337bc74abd093172ac99ceebb8c818635e189530cf1c58dc` |
| gp-u9-win 346 (r1=r2=5) | {218} (1977 frames scanned) | **(220, 221)** — exactly one (vacuous middle) | `703828af123edf1a8c3bade8d42bdd56a68576cd87e13de7339bdd203dc8f0b6` |
| gp-u10-ending 331 (r1=2, r2=64) | {216, 243, 1127} (4664 frames scanned) | **(218, 219)** — exactly one | `2d6fb6ad829baae61c65a844fb44f05cb51393322afdce4ab8117960e1306ddd` |

(§H.4 names for this allowance, per the plan's rule: it injects no keys — the
replay scripts and their key counts are unchanged — and reads no screen/data
bytes in `[0xA0000, 0xAFA00)`; it reads capture `.raw.gz` files and dump `.ipx`
files only, so the §H.4 hang limit and overlap bound do not bind it.)

**Model (step 2): `SCAN3_ALLOWED` in `tools/gp_compare.py`.** The plan's file list
pointed at `title_compare.py`, but the brief blesses reusing task 5's mechanism and
its home moved to `gp_compare.py` with `BANDS_ALLOWED` (§G5); the scan-out table
lives beside it. An entry is `(capture j, r1, r2, top_sha)`; the named frame is
explained ONLY if (1) the sha256 of its rows `[0,r1)` equals `top_sha` (the still-held
old screen — the segment no *unique* port frame answers for), (2) EXACTLY ONE adjacent
pair `(B, B+1)` in the CURRENT dump holds: the capture's rows `[r1,r2)` equal port[B]'s
and its rows `[r2,200)` equal port[B+1]'s — the middle test is vacuous when `r1 == r2`
(the U9 two-segment shape), where uniqueness is therefore of the single bottom holder
and the pair is (holder−1, holder) — and (3) at least one dump frame with index
A < B holds the capture's rows `[0,r1)`: the old screen being scanned IS one of the
port's own earlier frames. Classification kind `scan3` exhibits the top holders plus
B+1, and additionally B when the middle range is non-vacuous (a frame whose bound
range has no pixels is never exhibited). Like `BANDS_ALLOWED`: consulted only when the
two-frame model leaves a named frame unexplained; a named frame the model explains is
reported as a stale name (`retire the name`); a named frame that is not its recipe
prints `FAIL: allowed capture … is not its named three-frame scan-out: …` and fails
the enforced run in both modes (`allow_bad`); the summary line gained the
`scan3-allowed` count (re-checked: no other tool parses that line). Every other
scenario passes no entries — keys/moves/twop/modes/charsel behaviour is unchanged,
and task 5's band path is untouched (`make gp-keys-oracle` re-ran green).

**The entries (step 3, measured on the dumps re-played on `93b24171`).**

| scenario | capture (raw) | r1/r2 | top holders A<B (rows [0,r1)) | unique pair (B,B+1) | top sha256 |
|---|---|---|---|---|---|
| gp-idle-loss | 2064 (5190) | 3/98 | {1665, 1666} | (1669, 1670) of 6301 | `7bdf2c561c38a264337bc74abd093172ac99ceebb8c818635e189530cf1c58dc` |
| gp-u9-win | 346 (2830) | 5/5 | {218} (vacuous middle) | (220, 221) of 1977 | `703828af123edf1a8c3bade8d42bdd56a68576cd87e13de7339bdd203dc8f0b6` |
| gp-u10-ending | 331 (2831) | 2/64 | {216} of {216,243,1127} | (218, 219) of 4664 | `2d6fb6ad829baae61c65a844fb44f05cb51393322afdce4ab8117960e1306ddd` |

**The pins (step 3).** `make gp-replay` on this tree, then the enforced
`tools/gp_compare.py` calls with the Makefile pins:

- `GP_IDLE_LOSS_MIN_FIRST 2064 -> 7559`. Measured: `captured frame 2064 (raw 5190)
  allowed by name as a three-frame scan-out: top rows 0..2 held by port 1665 1666,
  port 1669 rows 3..97, port 1670 rows 98..199 the named bytes`; `7455 classified:
  4093 clean, 3352 splice, 8 transition, 1 scan3-allowed, 1 unexplained, 15
  all-black`; `first unexplained 7559, ratchet N 2064 ok (improved: raise N)`; N =
  7560 prints `FAIL: first unexplained 7559 < ratchet N 7560` — the pin is exact.
  7559 (raw 12207) is the first fully-content frame past the replayed script: the
  port's replay ends `0 restart(s) landed` with last presented f=0x207D while the
  X record is ms=174412 f=0x207F step=4 end, and the capture goes on to the soft
  restart's screens (7557 all-black, 7558 six non-black rows still matched). How far
  the port got, not a divergence — the gp-keys 1136 class. `GP_IDLE_LOSS_MAX_START
  = 90` unchanged (the window still starts at 90, exact); TRACE 8320 unchanged
  (`0 differing through 8319; ratchet N 8320 ok`).
- `GP_WIN_MIN_FIRST 346 -> 2209`. Measured: allowed line `top rows 0..4 held by port
  218, port 221 rows 5..199 the named bytes`; `2098 classified: 1105 clean, 988
  splice, 3 transition, 1 scan3-allowed, 1 unexplained, 12 all-black`; first
  unexplained `capture 2209 (raw 4953): nearest port 1777, rows 192..197, x 0..85
  (166 px)`; N = 2210 fails (exact). 2209 is NOT the §G6 class: its strip (rows
  192..197, sha256 `a886c0f3db8b2a973dfce49b8b3d3829958d788fe221da24f7d14cd02b2f23aa`)
  is held by NO frame of the 1977-frame dump — including the loader/pump-hook
  samples of the mode-0x17→0x12 entry (dumps 1775..1777 all at f=0xBA1 with its
  tick advancing 0xBFA→0xC00 inside the iteration, then one static dedup'ed image
  through f=0xBBF). The strip lands in the (0,192) text-row region — but the port
  DID dump that entry's sampled states and none matches, so unlike §K.10 item 2
  the state is not merely undumped: cause NOT attributed here, a named gap, and
  the ratchet stops at it. The strip's bytes are IDENTICAL to gp-u10-ending's
  2062 (same sha) — one state recurring at the mode-0x12 entry of both poked
  captures. §W.12's then-next-two (356/1374) are explained on the current dump
  (the P4–P7 batches ported animation targets since §W.12 measured; per-frame
  re-attribution is not done here); the report's next behind 2209 are
  2452..2455, past the 1977-frame dump. MAX_START 100,
  TRACE/WIN/MILESTONES 3503/3503/8 unchanged (`ratchet ok` lines above).
- `GP_ENDING_MIN_FIRST 331 -> 2062`. Measured: allowed line `top rows 0..1 held by
  port 216, port 218 rows 2..63, port 219 rows 64..199 the named bytes`; `1968
  classified: 965 clean, 997 splice, 4 transition, 1 scan3-allowed, 1 unexplained,
  12 all-black`; first unexplained `capture 2062 (raw 4954): nearest port 1686,
  rows 192..197, x 0..85 (166 px)` — §W.16's already-named `2062 (port 1686,
  f=0xB9A, mode 0x12, 166 px)`, the SAME strip bytes (sha above) at the same
  class of place (dumps 1684..1686 all at f=0xB9A, its tick 0xBF3→0xBF9 inside
  the iteration, then one static dedup'ed image through f=0xBB8); N = 2063 fails
  (exact). Not allowed. §W.16's other next-one, 3686, is explained on the current
  dump since the P4–P7 batches (the report's next unexplained after 2062 are
  4113..4116). MAX_START 83,
  TRACE/WIN/MILESTONES 9954/9954/30 unchanged and at the replay's end.

**Proofs (step 4).** Unit (`tools/tests/test_gp_compare.py Scan3Allowance`, 8 cases):
the named bytes explain (with the message asserted); a mutated MIDDLE-bind dump frame
un-explains it again (allowance FAILs `no unique adjacent port pair…` and the ratchet
catches, rc=1, `first unexplained 1 < ratchet N 4`); a mutated top holder answers
`no port frame before 1 holds its named top rows`; different top bytes (capture side)
are `not the named…` and fail; two matching pairs are not the named frame; the
vacuous-middle shape explains with the message `top rows 0..4 held by port 0, port 2
rows 5..199` and does NOT exhibit the middle-less B; a model-explained named frame
prints the stale-name note; and the table's three entries are pinned, so a silent
name change is a test failure. (`test_gp_compare` 46/46; the `make verify` gp tool
line — k11/gp session/capture/compare/moves/keys — 198/198; `test_title_compare`
10/10.) Real data on COPIES (`/tmp/g6_mut`, the sources untouched): poking port 1669's
row 50 index byte (42→0, DAC-differing) gives
`FAIL: allowed capture 2064 (raw 5190) is not its named three-frame scan-out: no
unique adjacent port pair binds its rows [3,98) and [98,200)` +
`FIRST UNEXPLAINED capture 2064 …` + `FAIL: first unexplained 2064 < ratchet N 7559`,
EXIT 1; the same for 1670's row 150 (bottom bind), U9's port 221 row 100 (FAIL at 346,
`< ratchet N 2209`, EXIT 1) and U10's port 218 row 30 (FAIL at 331, `< ratchet N 2062`,
EXIT 1) — per-frame per-bytes, and the traces stayed `ratchet ok` throughout. Mechanism
sensitivity on the real dumps with `SCAN3_ALLOWED` emptied in-process: all three enforce
runs print the FIRST UNEXPLAINED at their named frame and `FAIL: first unexplained <
ratchet N` with rc=1 — the pins depend on the names, and the no-mechanism lines
reproduce §U6.21/§W.12/§W.14's measured report lines byte-for-byte (idle-loss `1967
classified: 1024 clean, 941 splice, 1 transition, 1 unexplained, 8 all-black`, 346
`nearest port 221, rows 0..4, x 0..319 (933 px)`, 331 `nearest port 219, rows 0..63,
x 0..319 (10336 px)`), so the allowance adds no silent absorption.

**Named limits of the claim.** (a) the top strip is bound by EXISTENCE, not uniqueness:
several frames share the old screen's first rows (idle-loss {1665,1666}, U10 {216,243,
1127}), so a mutation of ONE holder while another holds the same rows is not felt by
THIS frame — the holders' full frames are claimed by the captures below the ratchet
(2062/2063, 345, 330) and a change to the strip in every earlier holder un-explains
(rule 3, unit-tested); (b) where `r1 == r2` (U9) the bound is on the single bottom
holder — the "pair" is (holder−1, holder) and holder−1 contributes no pixels and is
not exhibited; a bottom holder at dump index 0 would have no predecessor and fail
closed; (c) the skipped frames (idle-loss 1667/1668, U9 219/220, U10 217) stay
unexhibited by design — §G6's finding is that the ORIGINAL never blitted them; their
content is claimed by NO frame comparison, and the coverage line lists them;
(d) the standard gp_compare limits (§G.16): no under-render detection, order and
full coverage not claimed, frames at/after the pinned start classified against the
port's own dump.

**Development notes (fail-first, honest).** The first test run failed 8/8 in this
class for two test-side bugs found and fixed before green: a doubly-wrapped table
(`s3 = ((entry,),)` keyed the guard dict by the whole entry instead of the frame —
the enforced loop then never consulted the recipe: fixed to `(entry,)` as in task 5)
and a vacuous-shape expectation that printed the bind's dump INDEX (2) not value;
the model itself needed no behaviour change after those test fixes — the real-data
enforce lines above were measured before them (the allowance consulted path was
already exercised there via `main()`) and re-measured after, identical.

## §G7 — the gp-moves 2139 diagnosis (pending, lane B task 7)
