# Track F — a C-library file model for the save-file rows: derivation record

Brief: `.superpowers/sdd/2026-10-14-reverse-f/brief.md`. Base: `main` `acd5fc7a`
(`1000/1000 functions VERIFIED; 14884/14884 mutants detected; 1 named gaps;
638/831 rows with callees closed (169 have none)`).

## §F.1 Batch f1: the file model, and the two save-file rows converted to it

Sources: capstone over `/tmp/pr_f1_img.bin` and the strings it holds, `E.callee_clobbers`,
the bytes of the two rows' bodies (record `2026-10-13-reverse-s §S.7.1/§S.7.2`).

### The file callees' conventions, from the bytes

The four runtime functions the two rows call (`§S.7.4`: `0x61C60` fopen, `0x61CF6` fread,
`0x62031` fwrite, `0x61EC0` fclose) pass their arguments in registers and return in EAX
(WATCOM's register convention; the same reading as §S.7.1/§S.7.2, now confirmed from the
callees' own prologues):

| callee | arguments (entry registers) | EAX at return | preservation, and what the rows re-read |
|---|---|---|---|
| `0x61C60` fopen | `EAX` = the name, `EDX` = the mode — the sites load `mov edx,0x80598; mov eax,0xA2C5C` (`0x1AFF1/0x1AFF6`, `0x1B08D/0x1B092`); `0x61C60` pushes EBX and hands EAX to `0x61C45`, which moves it to ESI (`0x61C47`) and passes it to the opener `0x61B7D` with EDX (the mode) still live for `0x6BDBC` | a stream (non-zero) or `0` | the caller keeps it in ESI (`0x1B000 mov esi,eax`) and tests EAX (`0x1B002/0x1B09E test eax,eax`) |
| `0x61CF6` fread | `EAX` = buffer, `EDX` = item size, `EBX` = item count, `ECX` = stream — the site `0x1B006..0x1B017` (`mov ebx,0x7f8; mov edx,1; mov ecx,eax; mov eax,0x100ce4`); the prologue saves EAX and EDX (`0x61CFC/0x61CFD push`) and moves ECX to the stream register (`0x61CFE mov ebp,ecx`; the flag test `0x61D00 test byte[ecx+0xc],1`), and the byte total is `0x61D20 imul ebx,edx` | the count of whole items (total 0 answers 0: `0x61D23..0x61D29`); `0x1B025 cmp edx,0x7f8` reads it back | the caller keeps the count in EDX across fclose (`0x1B01C mov edx,eax`) and the stream in ESI |
| `0x62031` fwrite | the same four (`0x1B0CA..0x1B0E1`; prologue `0x62037/0x62038/0x62039`, stream `0x6203B`, total `0x62056 imul ebx,edx`) | the count of whole items (total 0: `0x62059..0x6205F` returns 0) | as fread (`0x1B0E6 mov edx,eax`, `0x1B0EF cmp edx,0x7f8`) |
| `0x61EC0` fclose | `EAX` = stream (`0x1B01E/0x1B0E8 mov eax,esi`; the callee `0x61EC2 mov edx,eax` and walks the stream list at `[0x109034]`) | its result: `0x61ECD` answers `0xFFFFFFFF` for a stream not on the list, the found path returns `0x61EEF`'s result (0 for a clean flush+close); **the rows never read it** (`0x1B02D/0x1B04D` load EAX, §S.7.1) | it pushes EDX and EBX (`0x61EC0/0x61EC1`) — the caller's EDX (the count) survives; its ESI is only read into EAX before the call |

`E.callee_clobbers` is the check on the third column: the registers a row reads after a call
(EAX's result, and the EDX count across fclose) are ones the real callee keeps; nothing else
is read between the calls (ESI only feeds the arguments). The model therefore clobbers only
EAX — a superset of every real callee's preservation, which cannot break a caller that respects
the convention the bytes just showed.

The strings the arguments point at (read from the image, which is the loaded program, fixups
applied): `0xA2C5C` = `cmos\0`, `0x80598` = `rb\0`, `0x8059C` = `wb\0`.

### The model (`tools/diff_emu.py`)

`E.Machine(files={name: state})`, given per case as before. `name` is 1..15 bytes with no NUL,
at most two names; `state` is the file's initial bytes, `E.F1_ABSENT` (the file is not there but
the directory is: fopen rb fails, fopen wb creates), or `E.F1_GONE` (no directory: every fopen
of the name fails — the shape row `s0` needs, whose raw fopen fails in write mode and whose
port side is a path under a subdirectory the binding never creates, the trick the current
binding already uses). With none, the four calls stop the run as before (proven: the full
`--self-check` printed byte-identical output with the mechanism in place and every row
unconverted — `base_selfcheck`/`mech_selfcheck`, `cmp` identical); with it, the four entry
addresses become modelled callees: a call arriving at one runs the model instead of the bytes
(the callset stub's entry shape — EAX set, return address popped, EIP returned), and anything
the rows never do stops the run as `unmodeled`, never a guess:

- **fopen**: reads both strings from memory (NUL within 16 bytes, else stop), records the
  `(name, mode)` pair in `OrigResult.file_opens` — even a failed open is recorded, the call
  happened — and answers a handle (a fresh non-zero integer) or 0 per the table above. The
  handle's value is model-internal: the rows use it only as a token (`0x1B000 mov esi,eax`,
  re-fed to fclose/fread), never compare it, and it never reaches memory. A name not declared
  stops (the case must cover its own vocabulary); a mode other than `rb`/`wb` stops (the game
  has only those two strings, `0x80598/0x8059C`).
- **fread**: `min(count, (len - pos) // size)` whole items copied from the file into the buffer
  (a memory write the run's own `writes`/call memory see — API writes seed `before` the way the
  stub writes do, since unicorn fires no hooks for them), EAX = the count; short at end of
  file. A read on a stream the model opened for writing stops the run (the real callee checks
  the stream's flag byte; the rows never read a write stream).
- **fwrite**: `count * size` bytes copied from the buffer into the file at its position,
  EAX = the count. A memory-backed file never comes back short — that is the model's one place
  where the hardware could disagree, and the record says so where it matters to the rows.
- **fclose**: a stream the model opened returns 0 (`0x61EEF`'s clean result), any other value
  0xFFFFFFFF (`0x61ECD`); either way the rows ignore it (§S.7.1).

**The mirror — how the two sides' files get compared.** fclose writes the file's final state
into `mem[]` at `F1_SHADOW` (0x74000: the gap between the objects — the code object ends
`0x73B14`, the data object starts `0x80000`, the dump holds zeros there and neither the rows'
bytes nor the port's `host_save_*` paths ever touch it), one slot per declared name in
declaration order, stride 0x840: name + NUL at +0, mode + NUL at +0x10, the byte count as a
dword at +0x20, the content at +0x24. The port's binding mirrors the scratch directory's real
files at the same layout (the name it pointed the port at as basename, and the mode the row
declares the original to pass), after the call. A plain `mem[]` byte comparison then compares
the two sides' file bytes — the file the port wrote against the bytes the original's fwrite
recorded — with no new output channel, and a difference shows as the ordinary `byte` lines.
fclose mirrors and nothing else: at the save row's fopen the mirror would land in the memory
reported at its two stubbed `0x2D4EC` calls (§E3.12), where the port cannot show bytes until
after its return; the rows always fclose what they open, so the final state arrives either way.

### The harness tests

`F1FilesTests` in `tools/tests/test_diff_emu.py` (each fails without the mechanism: the
constructor raises and the attribute is missing — proven by running them with `diff_emu.py`
stashed: 18 errors): the read program (fopen/fread/fclose) copies the file into an 0xEE-seeded
buffer and mirrors name/mode/length/content; a short file reads short (whole items only); a
missing file answers 0 with the open still recorded and no mirror; fwrite creates a new file,
returns the count, and its bytes arrive in the slot; fwrite truncates an existing file (the old
bytes are gone); an `r+` mode, an undeclared name, and a read on a write stream each stop the
run with their reason; fclose of a handle never opened answers 0xFFFFFFFF; and a case with no
`files` stops on `call 0x61C60` exactly as before.

### The port side (`port/tests/diff_runner.c`, `tools/diff_verify.py`)

**The case text** (`f1_machine_text`, wired into `cases_text`): a case with `Machine.files` emits
`files`, then per name (declaration order) `fdec <hexname> present|absent|gone`, its
`fmode <hexmode>` from the case's `opens`, and the content in `fd <hex>` chunks of at most 32
bytes. The C parser (`run_cases`) keeps them in file-scope state reset at every `case` line
(`f1_reset`, the `f1_parse_decl/f1_parse_mode/f1_parse_data` helpers); a malformed line fails
diffrun outright (the driver's `run_port` raises), never silently. A case without `files` parses
exactly as before (proven: the full `--self-check` byte-identical with the wiring in and no row
converted — `wire_selfcheck.txt`).

**The binding** (`sc_begin`/`f1_mirror`/`sc_end`): mkdtemp a scratch directory, write each
`present` declaration's bytes at `dir/<name>` (a `gone` name's path lives under `none/`, the
subdirectory never created — the port's `fopen` fails exactly as the model answers 0), point
`host_save_set_path` at the declared file with `host_save_set_fallback(NULL)`, run the port, then
mirror every file that now really exists in the directory into its F1_SHADOW slot — the declared
name + NUL, the declared mode + NUL, the byte count as a dword, the real bytes read back through
`host_read_file` — and clear the path (`sc_end`). The port's save path is only ever the scratch
directory's file: the game dir's `CMOS` is never opened, so the drop-writes mode of the test runs
cannot leak into the rows and `data/` stays untouched. The emulator's model mirrors the same
layout at its fclose (its own name/mode strings read from memory); the ordinary byte comparison
then compares the two sides' file names, modes, lengths and contents — the file the port wrote
against the bytes the original's fwrite recorded, and the name and mode the port opened (the
binding hands the port the name; the mode string is what the case states the original passes, and
a mismatch shows as differing mirror bytes).

**The opens check** (`Case.opens`, `f1_opens_diff`, wired into `verify_spec`): every modelled
fopen records its (name, mode) read from memory in `OrigResult.file_opens`, whether it succeeded
or not; a case that states its opens gets a difference (`opens: original …, the case states …`)
if the model read anything else — including the failed-open cases, whose mirror carries nothing.

### The rows converted (the pokes removed, cases and blocks before and after)

| Row | Before (track S) | After (track F) |
|---|---|---|
| `config_file_load` `0x1AFE8` | 3 pokes (`0x1AFFB`/`0x1B017`/`0x1B020` `mov eax,<result>`); 6 cases `l0..l5` (handle/closed values); 10/11 blocks, `0x1B04D` named unhit; 12 mutants | **no pokes**; 6 cases `l0..l5` with `Machine.files` (`l0` a real missing file `F1_ABSENT`; `l1` a valid image; `l2` one bit off + dirty regs; `l3` the sum of the first `0x7F6` stored; `l4` the captures' zero image; `l5` a second valid image) and `opens=((b"cmos", b"rb"),)`; the buffer seeded every byte XOR `0x5A` so the modelled fread and `host_save_read` each move the file's bytes on both sides; 10/11 blocks, `0x1B04D` named unhit (see below); the same 12 mutants, measured catch-sets identical to the track S pins |
| `config_file_save` `0x1B084` | 3 pokes (`0x1B097`/`0x1B0E1`/`0x1B0EA`); 4 cases `s0..s3`; the verdict byte `0x3900200` (the binding's own file-vs-memory check); 7/8 blocks, `0x1B0F7` named; 18 mutants | **no pokes**; 5 cases: `s0` `F1_GONE` (the missing directory, whose fopen answers 0 and does nothing else, the seeded last byte stays), `s1..s3` `F1_ABSENT` (the open creates the file; `s1` the touch-stub EAX `0x12345678`, `s2` dirty regs, `s3` the zero image), and **new `s4`**: an existing file (`b"stale cmos!!"`) the `wb` open truncates; `opens=((b"cmos", b"wb"),)`; 7/8 blocks, `0x1B0F7` named (below); 19 mutants — the same 18 plus **`@wmode`** (`f1_append`: write in append mode instead of the truncate the raw's `fopen("cmos","wb")` does at `0x1B097`), caught by `s4` alone, exactly what the new case is for |

The verdict byte is gone and nothing is lost: its load-side counterpart never existed, and on the
save side the mirror compares the port's written file against the bytes the original recorded
(what the verdict byte approximated against the port's own memory) — `@nowrite`/`@wlen`/`@wsrc`/
`@writefirst` are each caught by mirror bytes on every case the verdict caught them on, and now
also on `s4`. The `closed` values (`-1`, `0x100`) and the arbitrary handle values (`0x10A040`,
`1`, `0x100`) the pokes carried are gone with the pokes: `fclose`'s EAX is not read (0x1B02D and
0x1B04D load EAX, §S.7.1; the model answers 0/0xFFFFFFFF per the callee's bytes) and the handle
never reaches memory (it lives in ESI and is re-fed to the callees as a token; the mask 0xFF keeps
EAX's upper half out of the comparison).

**Why the exit blocks stay named unhit.** The brief asks for a short-read case; the model answers
one (the harness tests prove it), but such a case takes the original to `0x1B04D`/`0x1B0F7`, whose
`call 0x62003` is the runtime's exit — 0x62003..0x62723 ends `mov ah,0x4c; int 0x21` and never
returns (§S.7.1) — so the case cannot produce a return value to compare and every case reaching it
blocks the row (NOT_EXERCISABLE). A short write cannot happen at all: a memory-backed file has no
full disk, and the model's fwrite always returns the count (§F.1). The port's path for both is its
documented deviation — it takes the failed-open result instead (config.c PORT, §S.7.3). A long file
(no case added): the raw reads its first `0x7F8` bytes and may accept them while
`host_save_read` refuses another length (the §S.7.3 PORT row) — a case whose two sides differ by
a documented deviation cannot be compared on a VERIFIED row either. So both rows end with **no
poke** but still **one named unhit block each**, and stay in the weaker-row list for that reason
(AGENTS.md and README updated with the new reason, the 9/723 counts unchanged).

### Corrections, mutation proofs, pins

**No port correction**: both rows VERIFIED at the first run after the C build — the pokes' blind
spots (the name and mode strings, the buffer moving through the read, the written bytes) matched
with the port's real host calls. The two documented host deviations (§S.7.3: the save path is the
save file and not the current directory's `cmos`; the `ab` probe and the reported failed close) are
left as they are and reported here, not corrected.

**Mechanism mutation proofs** (each run against `F1FilesTests` and turning it red): fread asked
for more items than the file holds returns `count` (no EOF cap); the `wb` open does not truncate;
fclose of an unknown handle answers 0; the undeclared-name stop removed; the unknown-mode stop
removed. The `F1FilesTests` all fail without the mechanism (proven: 18 errors with `diff_emu.py`
stashed). The mirror-trigger claim — nothing mirrors at the `wb` open — is proven at the row level,
not in the unit tests: moving the mirror to the fopen would write the slot before the save row's
two stubbed `0x2D4EC` calls, and their recorded memory (§E3.12) would show bytes the port's binding
cannot show until after its return — the save row would go MISMATCH.

**Measured pins** (jobs=1, on the merge): the load row's 12 mutants' catch-sets and kinds are
identical to track S's pins; the save row's write-path and call mutants also catch `s4`
(the mirror shows the truncated file's bytes on `s4` too), `@len6`/`@len8` stay at `['s1','s2']`
(the zero buffer of `s3`/`s4` makes them invisible there), `@flag` gains `s4`, and `@wmode` is
caught by `['s4']` alone with kind `byte`. The counter pin is `1000/1000 functions VERIFIED;
14885/14885 mutants detected; 1 named gaps; 638/831 rows with callees closed (169 have none)` —
14884 + the one new `@wmode` mutant; the rows-with-callees count is unchanged (the two rows' callee
lists are unchanged: the file calls are modelled, not allowed or stubbed, so they never enter
`SpecResult.callees`). README.md's counter and AGENTS.md's weaker-row reasons updated to match.

**What the rows now claim.** `config_file_load`: with the case's directory holding the file, the
original's fopen returns non-zero for `present` and 0 for the missing file (`0x1B002`), the fread
moves the file's bytes into the buffer for every present case (the 0x5A seed proves the port moved
them too), the checksum and fill paths agree byte for byte, `0x10D34`/`0x10D0C`/`0x61A70` run from
their bytes as before, and the opened name/mode, the handle's non-zero-ness, the count's source
(the real file length) and the file's final bytes all agree; `config_file_save`: the same for the
open's real mode `wb`, the create-from-absent and truncate-existing paths, the fwrite's real count
and the bytes the port wrote. The narrowness unchanged: equivalence on the exercised blocks and
inputs only; the exit block is not exercised by either row and never can be.
