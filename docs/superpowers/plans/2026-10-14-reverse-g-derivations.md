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

## §G3 — the six non-rows (pending, lane A task 3)

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

## §G6 — the mode-8 three-frame scan-out (pending, lane B task 6)

## §G7 — the gp-moves 2139 diagnosis (pending, lane B task 7)
