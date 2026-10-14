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
  handle's value is model-internal: the rows use it only as a token (`0x1B004 mov esi,eax`,
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
