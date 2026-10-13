# Track S derivations: the storage layer

Plan `2026-10-13-reverse-s-storage-layer.md`, spec `../specs/2026-10-13-storage-layer-design.md`.
Sources: capstone over the fixed-up image (`./build/diffrun --exe data/game/C/PRAGE.EXE --image-out
/tmp/pr_s_img.bin`; image offset = address − `0x10000`), the Ghidra listing (project `rage`, `/PRAGE.EXE`),
and the harness's emulator (`tools/diff_emu.py` `run_original`). §S.1-§S.4 are the integrator's start
for the three batches; each batch re-reads the bytes for its own functions and its section (§S.5 `sa`,
§S.6 `sb`, §S.7 `sc`) is the authority for what it ported.

## §S.1 The image and its records

The image is `0x7F8` bytes at `0x100CE4..0x1014DB` (`[0x2D48C] = 0x100CE4`). Its last byte `0x1014DB`
is the file checksum: the XOR of bytes `0..0x7F6` (`0x1AFE8`, `0x1B084`). The byte `0x2D490` is 4 in
the image and no instruction stores it (track D, d6a).

A record is stored one byte per **cell** of two image bytes (only the first byte of a cell is read or
written: stride 2 at `0x2E9D4`, `0x2D5C3`). A record of `len` data bytes takes a codeword of `n` cells,
`n` being the result of the loop at `0x2E9A7..0x2E9B3` (`a = 1; do { n++; if (a >= n) break; a *= 2; }`
from `n = len`): the data bytes sit at the positions that are not 0 and not a power of two, position 0
holds the XOR of all, and position `2^j` holds the XOR of the data bytes whose position has bit `j` set
(`0x2E9E0..0x2EA05` read side, `0x2D5C2..0x2D61C` write side).

The kind table `0x2D444`, 9 entries of 8 bytes (`u16 cell, u16 len, u32 source`), read from the image:

| Kind | Cell | Len | Source | Codeword cells | Image bytes |
|---|---|---|---|---|---|
| 0 | (computed) | `0x26` | `0x105DB0` | 45 | from cell `0x1A3` (`0x10102A`), see below |
| 1 | `0x0` | `0x53` | `0x105DE1` | 91 | `0x100CE4..0x100D99` |
| 2 | `0x5B` | `0x53` | `0x105DE1` | 91 | `0x100D9A..0x100E4F` |
| 3 | `0xB6` | `0x14` | `0x105ECD` | 26 | `0x100E50..0x100E83` |
| 4 | `0xD0` | `0x14` | `0x105EE1` | 26 | `0x100E84..0x100EB7` |
| 5 | `0xEA` | `0x7` | `0x105EF5` | 12 | `0x100EB8..0x100ECF` |
| 6 | `0xF6` | `0x78` | `0x105E34` | 128 | `0x100ED0..0x100FCF` |
| 7 | `0x176` | `0x1C` | `0x105EAC` | 35 | `0x100FD0..0x101015` |
| 8 | `0x199` | `0x5` | `0x105EC8` | 10 | `0x101016..0x101029` |

Kinds 1 and 2 are two copies of the same `0x53`-byte buffer `0x105DE1` (it ends with `0x105E2F` and
the magic `0x105E30 = 0x9C94D2C4`). Kind 0's table cell is not used: `0x2D4EC` computes
`cell = 0x2D4B4(0x26) * [0x105E2F] + 0x1A3` (`0x2D524..0x2D53B`: EDX is the kind, 0 on this arm, `mov
dl,[0x105E2F]`, and `0x2D4B4` pushes and pops EDX; `0x2D4B4(0x26)` is the codeword length 45), a slot
that moves with the byte `0x105E2F`. The helper at `0x2D4C8` computes the same cell from EAX. `0x2D45C` is the table's entry 3 (the
`0x2DF8C` loop over kinds 3..5) and `0x2D474` its entry 6 (the `0x2DE98` loop over kinds 6..8): data,
not code.

## §S.2 The 15 functions

Extent, `ret` form, direct calls and the data they name (from the image):

| Address | Ends | `ret` | Calls (site -> target) | Data named |
|---|---|---|---|---|
| `0x2E990` | `0x2EA30` | `ret` | `2E9C3->61A70` | `2D48C` |
| `0x2D498` | `0x2D4B3` | `ret` | tail `2D4AB->2EA68` | `80AA8`, `100CE4`, `1014DC` |
| `0x2D4EC` | `0x2D62B` | `ret` | `2D50D->2EA68`, `2D533->2D4B4`, `2D576->2EA68`, `2D59E->61A70`, `2D612->2D498` | `2D444`, `2D48C`, `80AC8`, `80AE4`, `105E2F` |
| `0x2D638` | `0x2D6F7` | `ret` | `2D64E`, `2D673`, `2D6D0 ->2E990`; `2D6E6->61A70` | `105DDC` |
| `0x2DE98` | `0x2DF89` | `ret` | `2DEC1->2DB58`, `2DED1->2EA68`, `2DEDD->61A70`, `2DF08->2E990`, `2DF26->61A70`, `2DF5C->2D4EC` | `2D474`, `80B08`, `105DDC` |
| `0x2DF8C` | `0x2E032` | `ret` | `2DFC0->2E990`, `2DFD6->61A70`, `2DFE1->2D4EC` | `2D444`, `2D45C` |
| `0x2DAE4` | `0x2DB58` | `ret` | `2DAE9->2D974`, `2DB11->2D4B4`, `2DB24->2D4B4`, `2DB4D->2DA0C` | `105DD8`, `105E2F` |
| `0x2E934` | `0x2E98D` | `ret` | `2E983->2E180` | — |
| `0x2E180` | `0x2E217` | `ret` (two) | `2E1EB->2E0A4`, `2E20B->2E034` | `2D444` |
| `0x2E0A4` | `0x2E11B` | `ret` | `2E10D->2D4EC` | `2D444`, `105DD8` |
| `0x2E034` | `0x2E0A2` | `ret` | `2E096->2D4EC` | `2D444`, `105DD8` |
| `0x32B94` | `0x32BAB` | none | `32BA3->2DAE4` | — |
| `0x32BB0` | `0x32BD8` | `ret` | `32BC0->2DAE4`, `32BD2->2DAE4` | — |
| `0x1AFE8` | `0x1B081` | `ret` | `1AFEC->10D34`, `1AFFB->61C60`, `1B017->61CF6`, `1B020->61EC0`, `1B054->62003`, `1B070->61A70`, `1B075->10D0C` | `80598`, `A2C5C`, `100CE4`, `1014DB` |
| `0x1B084` | `0x1B117` | `ret` | `1B088->10D34`, `1B097->61C60`, `1B0A7->2D4EC`, `1B0B3->2D4EC`, `1B0E1->62031`, `1B0EA->61EC0`, `1B0FE->62003`, `1B10B->10D0C` | `8059C`, `A2C5C`, `100CE4`, `1014DB` |

Notes that correct the plan's name table:

- **`0x2DAE4` returns a value its callers read**: `and eax,0xffff` follows the calls at `0x1E9BB` and
  `0x1EC50`, `cmp eax,0xc8` those at `0x1E9EB` and `0x1EC84`. It is `s32 audit_add(u32 field, u32 n)`,
  not `void`. Entry: `mov ebx,eax; call 0x2D974` (the field's value), `jl` out when negative (`0x2DAF0`),
  `lea ecx,[eax+edx]`, a special case for field 3 (`0x2DAF5..0x2DB01`), a store through `0x2DA0C`
  (`config_field_set`, `0x2DB4D`).
- **`0x2E0A4` ends at `0x2E11B`.** The next function `0x2E11C` is already ported (`audit_hist_clear`,
  `svcmenu.c`, record §K11.8) and holds the calls at `0x2E16C` and `0x2E173`. `svcmenu.c` already has
  this family's table base (`AUDIT_STORE`) and the readers at `0x2E218..0x2E5FD`.
- **`0x32B94` has no `ret` of its own**: after `pop edx` (`0x32BA8`) and a 3-byte `lea eax,[eax]` it
  falls into `0x32BAC`, a named non-row of track D. Entry `push edx; test al,1; je`.
- `0x2E934`, `0x2E180`, `0x2E0A4`, `0x2E034` test their index first (`cmp eax,3`: `jbe` at `0x2E93D`,
  `jb` at `0x2E18B`, `0x2E0AD`, `0x2E03D`) and return -1 past it.
- `0x2E990(EAX = cell, EDX = dst, EBX = len)` returns 1 (`0x2EA1C`) or -1 (`0x2EA23`) and nothing else.
- `0x2D498(EAX = address, DL = value)`: `0x100CE4 <= EAX < 0x1014DC` stores the byte, else
  `jmp 0x2EA68` with `EAX = 0x80AA8`. (`0x2EA68` is the runtime's exit, `mov eax,1; jmp 0x62003`,
  not a printer: §S.5.)

## §S.3 What the full chain leaves behind (measured)

Script `s_measure.py` (plan, task 1 step 3; `run_original` on `0x2D6F8`). Run A is today's model:
the two validator calls nopped (`0x2D912`, `0x2D919`) and `0x2D4EC` a recorded stub. Run B is the
raw: `0x2D4EC`, `0x2D498`, `0x2DE98`, `0x2DF8C`, `0x2DB58`, `0x2EA68` run from their bytes. In both,
`0x2CADC` (`config_set_defaults`) and `0x2D974` are stubs, and `0x2D638`, `0x2E990`, `0x61A70`,
`0x65490`, `0x654C7`, `0x2D4B4` run.

| Image at entry | Bytes B writes inside the image | Bytes outside the image where B differs from A | Written by A only | Reads outside image and stack |
|---|---|---|---|---|
| zero (the captures) | 22, `0x100CE4..0x100E4E` | **0** | none | none |
| `0xFF` (no `cmos` file) | 464, `0x100CE4..0x101082` | **0** | none | none |

Calls recorded: A `0x2CADC()`, `0x2D4EC(1)`, `(2)`, `(0)`, `0x2D974(0x24)`; B `0x2CADC()`,
`0x2D974(0x24)`.

Reading: on the path every capture took, making `0x2DE98`, `0x2DF8C` and `0x2D4EC` real adds no
state outside the image. **No pin can move from `config_validate`'s storage becoming real.**

Limits. `0x2CADC` is a stub on both sides, so the records are encoded from a zeroed config block:
this does not give the image's bytes after a real boot (the unit case `validate_zero` and the
`save-reload` capture do). And it measures `0x2D6F8` only: the audit adds (§S.4) write config fields
and the dirty byte `0x105DD8` during play, which is new state outside the image, measured by the
gate's oracles. The `0xFF` row has no capture behind it: the `session.txt` of all 22 captures that
record it says `cmos=zero`.

## §S.4 Call sites

From a rel32 scan of the code object. "Port line" is where the port has the call or its `PORT:` note
today; "—" means the port names nothing at that address and the batch finds the place from the raw.

| Callee | Site | In | Port status of the caller | Port line today | Owner |
|---|---|---|---|---|---|
| `0x1AFE8` | `0x2F9CF` | `0x2F9CC` | inlined in `game_init` | `flow.c` ~6878, before `config_validate()` | `sc` |
| `0x1B084` | `0x1BEB5` | `0x1BE30` | `game_shutdown` | `flow.c` ~6974 (`PORT:`) | `sc` |
| `0x1B084` | `0x24AA1` | `0x249F0` | ported | `flow.c` ~6791 (`PORT:`) | `sc` |
| `0x1B084` | `0x2CBAF` | `0x2CB94` | `svc_options_menu` | `svcmenu.c` ~91 (`PORT:`) | `sc` |
| `0x2D638` | `0x2D71B` | `0x2D6F8` | `config_validate` | `config.c` ~178 (`PORT:`) | `sa` |
| `0x2E990` | `0x2D729`, `0x2D745`, `0x2D8DF` | `0x2D6F8` | `config_validate` | `config.c` ~178, ~218 (`PORT:`) | `sa` |
| `0x2DE98`, `0x2DF8C` | `0x2D912`, `0x2D919` | `0x2D6F8` | `config_validate` | `config.c` ~222 (`PORT:`) | `sa` |
| `0x2D4EC` | `0x2D7A9`, `0x2D7C0`, `0x2D7EB`, `0x2D802` | `0x2D6F8` | `config_validate` | — (the version arms are dead in the port) | `sa` |
| `0x2D4EC` | `0x2D8AA`, `0x2D8B4`, `0x2D90B` | `0x2D6F8` | `config_validate` | `config.c` 213-215 (calls) | `sa` |
| `0x2D4EC` | `0x2DA49`, `0x2DACA`, `0x2DAD4` | `0x2DA0C` | `config_field_set` | `config.c` 85, ~116 | (no change) |
| `0x2D4EC` | `0x2DDCD` | `0x2DCA0` | ported | `config.c` 335 | (no change) |
| `0x2D4EC` | `0x2E173` | `0x2E11C` | `audit_hist_clear` | `svcmenu.c` 1351 | (no change) |
| `0x2DAE4` | `0x2D962` | `0x2D6F8` | `config_validate` | `config.c` ~228 (comment) | integrator |
| `0x2DAE4` | `0x1E9BB`, `0x1E9EB` | `0x1E988` | `hiscore_audit_reset_due` (`flow.c` 5095) | — (the port reads the fields with `config_field_get`) | `sb` |
| `0x2DAE4` | `0x1EC50`, `0x1EC84` | `0x1EC38` | `hiscore_rank_single` (`flow.c` 5205) | `flow.c` ~5212 (`PORT:` in the header) | `sb` |
| `0x2DAE4` | `0x270A9` | `0x26F58` | ported | `flow.c` ~5866 (`PORT:`) | `sb` |
| `0x2DAE4` | `0x27A5F` | `0x27A2C` | ported | `flow.c` ~1965 (`PORT:`) | `sb` |
| `0x2DAE4` | `0x27F1F` | `0x27ED8` | ported | `flow.c` ~3049 (`PORT:`) | `sb` |
| `0x2DAE4` | `0x28E53` | `0x28DA4` | ported | `flow.c` ~1620 (`PORT:`) | `sb` |
| `0x2DAE4` | `0x42425` | `0x41C28` | `game_mode_12_step` | `flow.c` ~942 (`PORT:`) | `sb` |
| `0x2DAE4` | `0x32A85`, `0x32A9B`, `0x32AAA`, `0x32AB6`, `0x32ADE`, `0x32AF5` | `0x32A3C` | `config_play_time_close` | `config.c` ~482 (`PORT:`) | `sb` |
| `0x2DAE4` | `0x32A17` | `0x32970` | host-owned (wall time) | not ported; stays omitted and named | — |
| `0x2E934` | `0x32B35` | `0x32B00` | ported | `config.c` ~506 (`PORT:`) | `sb` |
| `0x2E934` | `0x32B7E` | `0x32B4C` | ported | `config.c` ~528 (`PORT:`) | `sb` |
| `0x2E934` | `0x43E77` | `0x43D60` | ported | `fight.c` ~729 (`PORT:`) | `sb` |
| `0x32B94` | `0x2786B` | `0x277C0` | ported | `flow.c` ~4367 (`PORT:`) | `sb` |
| `0x32BB0` | `0x27E28` | `0x27DC8` | ported | `flow.c` ~3010 (`PORT:`) | `sb` |

Every containing function is ported except `0x32970`. Correction (§S.6): `0x1E9BB` and `0x1E9EB` are
not in `0x1E988`. They sit in two fragments (`0x1E9B0..0x1E9DC`, `0x1E9E0..0x1EA07`) that nothing in
the image references; nothing is wired there and the two rows above that name them are wrong.

## §S.5 Batch `sa`: the image and its validators

Branch `reverse-sa`. Sources: capstone over the fixed-up image `/tmp/pr_sa_img.bin` and the harness's
emulator. The six functions and the new body of `0x2D6F8` are in `port/src/game/config.c`; the rows are
the `SA_SPECS` block of `tools/diff_verify.py`, the bindings the `sa` block of `port/tests/diff_runner.c`,
the pins the `S sa` block of `tools/tests/test_diff_verify.py`. This section is the authority for what
batch `sa` ported; where it differs from §S.1-§S.4, the plan or the spec, the address is given.

### §S.5.1 Corrections (the raw over the record, the plan and the spec)

1. **`0x2EA68` is the runtime's exit, not a no-op printer** (§S.1, §S.2 and the plan call it "the named
   no-op"). Its bytes are `mov eax,1; jmp 0x62003` (`0x2EA68`, `0x2EA6D`): the message pointer its
   callers load into EAX is overwritten, and `0x62003` is `exit` (`push edx; mov edx,eax; call
   [0xEF934]; call [0xEF938]; mov eax,edx; call 0x6201B`, whose tail `jmp 0x626EB` reaches `mov
   ah,0x4c; int 0x21` at `0x62721`, the DOS terminate call). So in the original the four storage sites (`0x2D4AB`, `0x2D50D`,
   `0x2D576`, `0x2DED1`) end the program with status 1 and the instructions after them never run. The
   port's `menu_fatal_error` (`menu.c`, record §49-X) returns: that is an existing `PORT:` decision
   and it is kept. What each site does in the port after the call is in §S.5.2 and the limits it puts
   on the rows are in §S.5.5.
2. **`0x2D638` is the reader of an older image layout, not the load of the current copies** (the plan's
   queue note and Review Focus 3 place the copy-2 fallback at `0x2D673`). It reads a `0x51`-byte record
   (`mov ebx,0x51` `0x2D643`; the current buffer is `0x53` bytes, `0x2D701`), its second copy at cell
   `0x59` (the loop `0x2D659..0x2D66A` on `0x51`; the current second copy is at `0x5B`), tests the
   magic at `+0x4D` of the record (`[esp+edx+0xa1]` with the buffer at `esp+0x54`, `0x2D6AC`; the
   current magic is at `+0x4F`, `0x105E30 - 0x105DE1`) and then reads a 5-byte record at cell `0x195`
   to `0x105DDC` (`0x2D6C1..0x2D6D0`; the current kind 8 is at cell `0x199`). Over an image that holds
   the current magic its test cannot hold (the bytes at `+0x4D` are then `[0x105E2E]`, `[0x105E2F]`
   and the magic's first two, `0xC4 0xD2`, where the test wants `0x94 0x9C`), so it stores nothing. Its one store, `0x105DDC..0x105DE0`, is what `0x2DE98` copies into
   table 2 under the defaults flag (`0x2DF37`). **The fallback of a current save whose first copy is
   damaged is in `0x2D6F8`, `0x2D76F..0x2D7C5`.**
3. **`config_storage_read` takes a pointer**, `s32 config_storage_read(u32 cell, u8 *dst, u32 len)` (the
   name table has `u32 dst`). Three of its eight call sites pass a buffer of the caller's own frame
   (`lea edx,[esp+0x54]` `0x2D648`, `mov edx,esp` `0x2D671` and `0x2D743`), which has no `mem[]`
   offset. The seam reports `dst - mem`, or `0xFFFFFFFF` for a C array (the `spawn_desc_arg` form,
   record E3 §E3.10 item 6), and a row whose caller passes a frame buffer runs `0x2E990` on both sides.
4. **The bound of `0x2D4EC` counts cells against the image's byte length.** `0x2D561..0x2D56F` is
   `cell + n < 0x7F8` with `cell` a zero-extended word and `n` the codeword's cells, but a cell is two
   bytes (`0x2D589 add ebx,ebx`): a record can pass the bound and still lie past the image, where
   `0x2D498` (`0x100CE4 <= address < 0x1014DC`) is the fatal exit. For kind 0 the slot byte
   `[0x105E2F]` puts the record whole inside the image up to 12 (`2 * (45 * 12 + 0x1A3 + 45) = 0x7D8`);
   from 13 to 35 the original exits in `0x2D498`, from 36 at `0x2D576`.
5. **The `< -1` tests of `0x2D638` and `0x2D6F8` are dead** (the spec's §3 says the callers set such a
   value themselves; only `0x2DE98` and `0x2DF8C` do, `-2` at `0x2DEE9` and `0x2DFAC`). `0x2D69B`,
   `0x2D6D5`, `0x2D828` and `0x2D8E6` test results that come straight from `0x2E990`, which returns 1
   (`0x2EA1C`) or -1 (`0x2EA23`). The same range makes the two repairs after a negative winner dead
   (`0x2D7AE..0x2D7C0`, `0x2D7F0..0x2D802`: the winner is the greater of two results, so it is 1).
6. **The call at `0x2D962` is not reachable.** It needs `[0x105DA5] + [0x105DA4] != 0`; the function
   zeroes both at entry (`0x2D70F`, `0x2D715`) and a scan of the code object for the two addresses finds
   only those stores and the reads `0x2D94D`, `0x2D953`. Its arguments, for the integrator: `EAX =
   0x24`, `EDX = [0x105DA5] + [0x105DA4]` (`xor ecx,ecx` `0x2D93E`, `mov cl,[0x105da5]`, `xor edx,edx`,
   `mov dl,[0x105da4]`, `add edx,ecx`); EAX of `0x2DAE4` is then the function's return value, which the
   one caller does not read (`mov eax,0x2a` `0x2FA10`).
7. **Two frame overruns in the raw**, neither reachable from the image's kind table (its longest
   record is `0x78` bytes, 128 cells, 8 check bytes): `0x2D4EC` clears `n - len` check bytes at `esp`
   (`0x2D59E`) with its pointer `0x2D48C` at `esp+8`, so a record with 9 to 12 check bytes zeroes that
   pointer's low bytes and the image's address is then read through it (`0x2D5A3`, `0x2D5B0`); the
   port keeps the 12-byte frame and reproduces this (case `f0`). `0x2E990` XORs a cell at a position
   with bit 7 into `[esp+8]`, the low byte of its own pointer to the syndrome (`0x2E9FC` with EDX =
   `esp+8`), so past 128 cells its result depends on the stack address; the port refuses such a length
   (`PORT:`, -1, nothing read).

### §S.5.2 The functions

**`0x2D498` `config_image_store(addr, value)`** — EAX, DL. `cmp eax,0x100ce4; jb` (`0x2D498`), `cmp
eax,0x1014dc; jb` (`0x2D49F`): inside, `mov [eax],dl; ret` (`0x2D4B0`); outside, `mov eax,0x80aa8; jmp
0x2ea68` (`0x2D4A6`). The port drops the byte after `menu_fatal_error` (`PORT:`). No seam (the one
caller's row runs it on both sides).

**`0x2E990` `config_storage_read(cell, dst, len)`** — EAX, EDX, EBX; `push ecx/esi/edi/ebp`, EBX and EDX
not preserved. `ebp = [0x2D48C] + 2 * cell` (`0x2E997..0x2E9A5`); `n` = the loop of `0x2D4B4` on `len`
(`0x2E9A7..0x2E9B3`); the 8 syndrome bytes at `esp` cleared (`0x61A70(esp, 0, 8)`, `0x2E9C3`); `edi =
dst + len`. From the last cell down (`dec ecx; sub esi,2`, `0x2E9D3`): the byte `[esi]`; when `pos &
(pos - 1)` is not zero it is a data byte, stored at `--edi` (`0x2E9E4`); it is XORed into syndrome
byte 0 (`0x2E9E8`) and, for `bit = 1, 2, 4.. <= pos`, into byte `1 + j` when `pos` has that bit
(`0x2E9EB..0x2EA03`). The OR of the 8 bytes (`0x2EA07..0x2EA16`): zero gives 1, else -1. The data is
stored whatever the result.

**`0x2D4EC` `config_storage_touch(kind)`** — EAX; all six registers saved. A kind below 0 or above 8 is
the fatal exit with `0x80AC8` (`0x2D4FF..0x2D50D`); the port returns there (`PORT:`: the bytes after
the call run on into the table with the bad kind, which the original never does). `ebx = 0x2D444 + 8 *
kind`; the cell `[ebx]` (word), for kind 0 `0x2D4B4(0x26) * [0x105E2F] + 0x1A3` instead
(`0x2D524..0x2D53B`); the length `[ebx+2]` (word); `ebp = [ebx+4] + len`; `n` by the inline loop
(`0x2D541..0x2D55F`); the bound of correction 4, with the fatal exit `0x80AE4` and `mov eax,-1`
(`0x2D571..0x2D580`). `0x61A70(esp, 0, n - len)`; `ebx = [[esp+8]] + (word)(2 * cell) + 2 * n`. From
the last cell down (`dec esi; sub ebx,2`): a data position takes `ch = [--ebp]` and XORs it into
`[esp+1+j]` for each bit `j < 7` of the position (`0x2D5D1..0x2D5EB`); a power of two takes `ch =
[esp+1+j]` (`0x2D5F1..0x2D600`); position 0 takes `ch = [esp]` (`0x2D606`); then `xor [esp],ch` and
`0x2D498(ebx, ch)` (`0x2D609..0x2D612`). Returns 1; no caller reads EAX (the 18 sites, scanned: each
is followed by a load of EAX, a pop sequence or a loop back).

**`0x2D638` `config_storage_load()`** — see correction 2. `r = 0x2E990(0, A, 0x51)`; when `r < 0`, `r2 =
0x2E990(0x59, B, 0x51)` and, when `r2 > r` (`jle` `0x2D67C`), `A = B` and `r = r2`; `r < -1` returns
(dead); the magic test on `A + 0x4D`; `0x2E990(0x195, 0x105DDC, 5)`, cleared when below -1 (dead).
`A` and `B` are C arrays in the port.

**`0x2DE98` `hiscore_storage_load(defaults)`** — EAX, kept as a dword at `[esp+4]`. For `table` 0..2,
the entry `0x2D474 + 8 * table` and the kind `6 + table`: `p = 0x2DB58(0, table, &left, 0)`; `p == 0`
is the fatal exit with `0x80B08` (the port returns: the bytes after it clear from address 0 with a
stale length); `0x61A70(p, 0, left)`; `r = -2` under the flag, else `0x2E990(entry.cell, p,
entry.len)`; `r < -1`: `0x61A70(p, 0, entry.len)` (the length read after the call, `0x2DF20`); flag
and `table == 2`: five bytes from `0x105DDC` to `p` (a forward `movs`); `r < 0`: `0x2D4EC(kind)`.

**`0x2DF8C` `config_storage_validate(defaults)`** — EAX at `[esp]`. For the kinds 3..5, `ecx = 0x2D45C +
8 * (kind - 3)`: `edi = [ecx+4]` (loaded before the call); `r = -2` under the flag, else
`0x2E990([ecx], edi, [ecx+2])`; `r < -1`: `0x61A70(edi, 0, [ecx+2])` (read after the call, `0x2DFD2`);
`r < 0`: `0x2D4EC(kind)`.

**`0x2D6F8` `config_validate()`** — `[0x105DA5] = 0`, `[0x105DA4] = 0`; `0x2D638()`; `r1 = 0x2E990(0,
0x105DE1, 0x53)`, **in place**: the stored copy replaces the buffer before any test; `r2 =
0x2E990(0x5B, the frame, 0x53)`. `(r1 < 0 || r2 < 0) && [0x2D490] == 0`: return (`0x2D74C..0x2D76A`).
`r2 > r1`: `[0x105DD8] |= 2`, the frame copied over the buffer, `0x2D4EC(1)` under the gate byte, `r =
r2`; else `r1 > r2`: `[0x105DD8] |= 4`, `0x2D4EC(2)` under the gate byte; `r = r1`. The magic
(`0x2D807..0x2D820`) absent, or `r < -1` (dead): the defaults arm, unchanged from the earlier port
(`0x2D83A..0x2D8B9`). Else `r3 = 0x2E990(0x2D4B4(0x26) * [0x105E2F] + 0x1A3, 0x105DB0, 0x26)` and
`0x2D4EC(0)` when `r3 < 0` under the gate byte (`0x2D8BB..0x2D90B`). Then `0x2DE98(flag)`,
`0x2DF8C(flag)` (`0x2D910..0x2D919`); `r < 0 && [0x2D490] == 0`: return (`0x2D91E..0x2D92B`);
`[0x105DA7] = 1`; `0x2D974(0x24)`; the dead `0x2DAE4` call of correction 6, left as a `PORT:` note.
The gate byte `0x2D490` is read at each test (`0x2D754`, `0x2D79B`, `0x2D7D6`, `0x2D89C`, `0x2D900`,
`0x2D922`), and the port reads it at each.

Over the zero image (every capture): `r1 = r2 = 1`, the buffer is the image's zeros, no magic, the
defaults arm. The earlier port reached the same arm through its declared `-1` reads; the bytes it
left out are the image itself, the two validators' clears and the encodes.

### §S.5.3 Rows (measured)

| Row | Entry | Cases | Blocks | Mutants | EAX mask | Callees |
|---|---|---|---|---|---|---|
| `config_image_store` | `0x2D498` | 6 | 4/4 | 8 | 0 (`0x2D617 jmp 0x2d5c2`: EAX loaded at `0x2D5CA` or `0x2D61C`) | `0x2EA68` stub |
| `config_storage_read` | `0x2E990` | 21 | 18/18 | 14 | full (all 8 callers compare EAX) | `0x61A70` real |
| `config_storage_touch` | `0x2D4EC` | 15 | 26/27 | 15 | 0 (no site of the 18 reads EAX) | `0x2D498`, `0x2D4B4`, `0x61A70` real; `0x2EA68` stub |
| `config_storage_load` | `0x2D638` | 8 | 12/13 | 11 | 0 (`xor eax,eax` `0x2D725`) | `0x2E990`, `0x61A70` real |
| `hiscore_storage_load` | `0x2DE98` | 8 | 14/15 | 15 | 0 (`mov eax,ebp` `0x2D917`) | `0x2DB58`, `0x61A70` real; `0x2E990`, `0x2D4EC` stubs |
| `hiscore_storage_load_rr` | `0x2DE98` | 1 | 11/15 | 1 | 0 | the same; the `0x2E990` stub rewrites the entries' lengths |
| `config_storage_validate` | `0x2DF8C` | 6 | 10/10 | 9 | 0 (`0x2D91E`: EAX loaded at `0x2D92B` or `0x2D939`) | `0x61A70` real; `0x2E990`, `0x2D4EC` stubs |
| `config_storage_validate_rr` | `0x2DF8C` | 1 | 9/10 | 2 | 0 | the same; the stub rewrites the lengths and one source |
| `config_validate` | `0x2D6F8` | 13 | 36/43 | 41 | 0 (`mov eax,0x2a` `0x2FA10`) | `0x2E990`, `0x2D4B4`, `0x61A70` real; `0x2D638`, `0x2CADC`, `0x2D4EC`, `0x2DE98`, `0x2DF8C`, `0x2D974` stubs |
| `config_validate_rr` | `0x2D6F8` | 2 | 24/43 | 2 | 0 | the same; the `0x2D4EC` and `0x2DF8C` stubs zero the byte `0x2D490` |
| `config_validate_chain` | `0x2D6F8` | 4 | 30/43 | 5 | 0 | the whole storage layer real; `0x2CADC`, `0x2D974` stubs |

Eleven specs, 123 mutants. `config_validate` replaces track D's partial row of the same name (5
cases, 20 mutants, the nop pins `0x2D912`/`0x2D919` and the no-storage model): the row count rises by
10 and the mutant count by 103. No row of this batch pokes code. Every unhit block is named (§S.5.5).
No case reads outside the image and the stack (`SA_OUTSIDE` is empty).

**`config_validate_chain` and §S.3.** Case `z0` is §S.3's measurement made a row: the zero image, the
config block zero, `0x2CADC` and `0x2D974` stubs, everything else from the bytes. The original
writes 22 image bytes, `0x100CE4..0x100E4E` (the two copies' cells of the magic and their check
bytes), and 8 bytes outside the image (the flags byte `0x105DD8`, the magic's four, and the seeded
`0x105DA4`, `0x105DA5`, `0x105DA7`: the bytes today's model writes too); the port writes the same
bytes. Case `zf` (the `0xFF` image): 464 image bytes, `0x100CE4..0x101082`, as §S.3.
Both agree with the integrator's figures. `s0` (a whole stored image) writes no image byte and 323
bytes of RAM; `s1` (the first copy, the trailing-byte record and the kinds 4 and 7 damaged) writes
10 image bytes, the repairs.

### §S.5.4 Mutants

Each mutant restates the function's body with one change (`sa_*_core` in `diff_runner.c`); its exact
catching case set is pinned in `SA_CASES` and what catches it in `SA_KINDS`. The tables below are
generated from those pins.

**`config_image_store`**

| Mutant | Caught by | In |
|---|---|---|
| `@lo` | e0 | byte, call |
| `@lo1` | e2 | byte, call |
| `@hi` | e3 | byte, call |
| `@hi1` | e1 | byte, call |
| `@width` | e0 e1 e4 | byte |
| `@nofatal` | e2 e3 e5 | call |
| `@msg` | e2 e3 e5 | call |
| `@both` | e2 e3 e5 | byte |

**`config_storage_read`**

| Mutant | Caught by | In |
|---|---|---|
| `@stride` | b0 d0 l1 l2 l3 l4 p0 r0 r1 r2 y0 y1 y2 y3 y4 y5 y6 y7 | byte, eax |
| `@cell1` | b0 d0 l0x l1 l2 l3 l4 p0 r0 r1 r2 y0 y1 y2 y3 y4 y5 y6 y7 | byte, eax |
| `@base` | b0 | byte |
| `@n` | b0 d0 l0 l0x l1 l2 l3 l4 p0 r0 z0 | byte |
| `@nge` | r1 y0 y1 y2 y3 y4 y5 y6 y7 | byte, eax |
| `@asc` | b0 d0 l2 l3 l4 p0 r0 r1 r2 y0 y1 y2 y3 y4 y5 y6 y7 | byte |
| `@pos0` | b0 d0 l0 l0x l1 l2 l3 l4 p0 r0 r1 r2 y0 y1 y2 y3 y4 y5 y6 y7 z0 | byte |
| `@syn0` | l0x y0 | eax |
| `@syn7` | y7 | eax |
| `@bit` | p0 | eax |
| `@le` | b0 l1 l2 l3 l4 r0 r1 r2 | eax |
| `@ret` | d0 l0x p0 y0 y1 y2 y3 y4 y5 y6 y7 | eax |
| `@nowrite` | d0 p0 y0 y1 y2 y3 y4 y5 y6 y7 | byte |
| `@or` | b0 l1 l2 l3 l4 r0 r1 r2 | eax |

**`config_storage_touch`**

| Mutant | Caught by | In |
|---|---|---|
| `@k0cell` | s0 s12 s2 | byte |
| `@k0mul` | s12 s2 | byte |
| `@k0base` | s0 s12 s2 | byte |
| `@bound` | b0 | call |
| `@bound1` | b1 | byte, call |
| `@nofatal` | b0 b2 | call |
| `@msg` | b0 b2 | call |
| `@stride` | b1 f0 k1 k2 k3 k4 k5 k6 k7 k8 s0 s12 s2 | byte, call |
| `@up` | b1 f0 k1 k2 k3 k4 k5 k6 k7 k8 s0 s12 s2 | byte |
| `@par0` | b1 f0 k1 k2 k3 k4 k5 k6 k7 k8 s0 s12 s2 | byte |
| `@overall` | b1 f0 k1 k2 k3 k4 k5 k6 k7 k8 s0 s12 s2 | byte |
| `@bit` | b1 f0 k1 k2 k3 k4 k5 k6 k7 k8 s0 s12 s2 | byte |
| `@six` | f0 k1 k2 k6 | byte |
| `@frame` | f0 | byte |
| `@cellsext` | b2 | call |

**`config_storage_load`**

| Mutant | Caught by | In |
|---|---|---|
| `@len53` | o4 | byte |
| `@cell5b` | o2 | byte |
| `@nofb` | o2 | byte |
| `@ge` | o3 o5 | byte |
| `@magicoff` | o1 o2 o3 o6 o7 | byte |
| `@magicbe` | o1 o2 o3 o6 | byte |
| `@nomagic` | o0 o4 o5 o7 | byte |
| `@cell199` | o1 o2 o3 o6 | byte |
| `@len4` | o1 o2 o3 o6 | byte |
| `@dst` | o1 o2 o3 o6 | byte |
| `@clear` | o6 | byte |

**`hiscore_storage_load`**

| Mutant | Caught by | In |
|---|---|---|
| `@noclear` | h0 h1 h2 h3 h6 h7 | byte, call memory |
| `@clearlen` | h6 h7 | byte, call memory |
| `@al` | h5 | byte, call, call memory |
| `@readdflt` | h4 h5 | byte, call, call memory |
| `@norefill` | h2 h7 | byte, call memory |
| `@refillleft` | h7 | byte, call memory |
| `@copy1` | h4 h5 | byte, call memory |
| `@copy4` | h4 h5 | byte, call memory |
| `@nocopy` | h4 h5 | byte, call memory |
| `@copyalways` | h0 h1 h2 h3 h6 h7 | byte, call memory |
| `@touchle` | h3 | call, call memory |
| `@notouch` | h1 h2 h4 h5 h7 | call, call memory |
| `@kind` | h1 h2 h4 h5 h7 | call |
| `@order` | h4 h5 | call memory |
| `@ent` | h0 h1 h2 h3 h4 h5 h6 h7 | byte, call, call memory |

**`hiscore_storage_load_rr`**

| Mutant | Caught by | In |
|---|---|---|
| `@lencache` | r0 | byte, call memory |

**`config_storage_validate`**

| Mutant | Caught by | In |
|---|---|---|
| `@al` | a5 | byte, call, call memory |
| `@readdflt` | a4 a5 | byte, call, call memory |
| `@norefill` | a2 a4 a5 | byte, call memory |
| `@touchle` | a3 | call, call memory |
| `@notouch` | a1 a2 a4 a5 | call, call memory |
| `@kind` | a1 a2 a4 a5 | call |
| `@ent` | a0 a1 a2 a3 a4 a5 | byte, call, call memory |
| `@from4` | a0 a1 a2 a3 a4 a5 | byte, call, call memory |
| `@to5` | a0 a1 a2 a3 a4 a5 | byte, call |

**`config_storage_validate_rr`**

| Mutant | Caught by | In |
|---|---|---|
| `@lencache` | r0 | byte, call memory |
| `@srcreread` | r0 | byte, call memory |

**`config_validate`**

| Mutant | Caught by | In |
|---|---|---|
| `@noda5` | c0 c1 c2 g1 g2 m0 n0 v0 v1 v2 v3 v4 v5 | byte, call memory |
| `@noda4` | c0 c1 c2 g1 g2 m0 n0 v0 v1 v2 v3 v4 v5 | byte, call memory |
| `@noret` | v3 v5 | byte, call |
| `@or6` | g2 m0 v0 v1 v2 v4 | byte, call memory |
| `@nofill1` | m0 | byte, call memory |
| `@nofill2` | g2 m0 v1 v2 v4 | byte, call memory |
| `@nofill` | g2 m0 v1 v2 v4 | byte, call memory |
| `@fill1len` | m0 | call memory |
| `@or1` | g2 m0 v0 v1 v2 v4 | byte, call memory |
| `@fill2len` | g2 m0 v0 v1 v2 v4 | byte, call memory |
| `@magicearly` | g2 m0 v0 v1 v2 v4 | call memory |
| `@nodefaults` | g2 m0 v0 v1 v2 v4 | call, call memory |
| `@magicbe` | g2 m0 v0 v1 v2 v4 | byte, call memory |
| `@touchorder` | m0 v0 v1 v2 v4 | call |
| `@notouch2` | m0 v0 v1 v2 v4 | call, call memory |
| `@notouch0` | m0 v0 v1 v2 v4 | call, call memory |
| `@noda7` | c0 c1 c2 g1 g2 m0 n0 v0 v1 v2 v4 | byte, call memory |
| `@da7` | c0 c1 c2 g1 g2 m0 n0 v0 v1 v2 v4 | byte, call memory |
| `@noget` | c0 c1 c2 g1 g2 m0 n0 v0 v1 v2 v4 | call |
| `@getfield` | c0 c1 c2 g1 g2 m0 n0 v0 v1 v2 v4 | call |
| `@noload` | c0 c1 c2 g1 g2 m0 n0 v0 v1 v2 v3 v4 v5 | call, call memory |
| `@r1len` | c0 c1 g1 m0 n0 v5 | byte, call, call memory |
| `@r1dst` | c0 c1 c2 g1 g2 m0 n0 v1 v2 v4 v5 | byte, call, call memory |
| `@cell2` | c0 c2 g1 m0 | byte, call, call memory |
| `@swap` | c0 c1 c2 n0 v5 | byte, call, call memory |
| `@nofb` | c2 | byte, call, call memory |
| `@fbge` | c0 g1 m0 n0 v0 v1 v2 v4 | byte, call, call memory |
| `@or2` | c2 | byte, call memory |
| `@nocopy` | c2 | byte, call, call memory |
| `@norep1` | c2 | call, call memory |
| `@or4` | c1 | byte, call memory |
| `@norep2` | c1 | call, call memory |
| `@storedcell` | c0 c2 | byte, call memory |
| `@nostoredread` | c0 c1 c2 g1 n0 | byte, call, call memory |
| `@storedtouch` | c0 c2 n0 | call, call memory |
| `@nohs` | c0 c1 c2 g1 g2 m0 n0 v0 v1 v2 v4 | call, call memory |
| `@nosv` | c0 c1 c2 g1 g2 m0 n0 v0 v1 v2 v4 | call, call memory |
| `@hsorder` | c0 c1 c2 g1 g2 m0 n0 v0 v1 v2 v4 | call |
| `@magicm1` | n0 | byte, call, call memory |
| `@hsflag` | c0 c1 c2 g1 g2 m0 n0 v0 v1 v2 v4 | call |
| `@svflag` | c0 c1 c2 g1 g2 m0 n0 v0 v1 v2 v4 | call |

**`config_validate_rr`**

| Mutant | Caught by | In |
|---|---|---|
| `@gatecache` | r0 r1 | byte, call, call memory |
| `@tailret` | r0 | byte, call |

**`config_validate_chain`**

| Mutant | Caught by | In |
|---|---|---|
| `@nohs` | s0 s1 zf | byte, call memory |
| `@nosv` | s0 s1 zf | byte, call memory |
| `@norep1` | s1 | byte, call memory |
| `@notouch0` | zf | byte, call memory |
| `@nocopy` | s1 | byte, call memory |

Two mutants that were considered and are not rows, because they are equivalent: a signed compare in
`0x2D498` (both bounds are positive, so an address above `0x7FFFFFFF` is refused either way) and a
cell truncated to 16 bits before the doubling in `0x2D4EC` (the bound keeps `cell + n` below
`0x7F8`).

### §S.5.5 Unhit blocks and named gaps

| Row | Block | Why it is not exercised |
|---|---|---|
| `config_storage_touch` | `0x2D508` | a kind outside 0..8: the fatal exit. Under a stub the bytes run on into the table with the bad kind, which the original never does; the port returns. The unit case `badkind` covers the port's stop. |
| `config_storage_load` | `0x2D6DA` | the clear of `0x105DDC` below -1 (correction 5) |
| `hiscore_storage_load` (and `_rr`) | `0x2DECC` | a table `0x2DB58` does not find: the fatal exit; the image's descriptors hold the counts 10, 1, 1 |
| `config_validate` (and `_rr`, `_chain`) | `0x2D7B2`, `0x2D7BB`, `0x2D7F4`, `0x2D7FD` | the repairs after a negative winner (correction 5) |
| | `0x2D8EB` | the clear of `0x105DB0` below -1 (correction 5) |
| | `0x2D95D` | the `0x2DAE4` call (correction 6); the port has a `PORT:` note there |
| `config_validate`, `_chain` | `0x2D92B` | the tail's return: hit by `config_validate_rr` `r0`, where the `0x2DF8C` stub zeroes the byte `0x2D490` |

The `_rr` and `_chain` rows name the arms they leave to `config_validate` (13 and 7 blocks, listed in
their `unhit_named`).

Named gaps, none a block:

- **The fatal exits.** `0x2EA68` does not return in the original and does in the port. A row stubs it
  only where the bytes after the call do what the port does: `0x2D498` (a tail jump: the stub's return
  is the function's) and `0x2D576` (`mov eax,-1` and the epilogue). Not compared: a bad kind
  (`0x2D508`), a table `0x2DB58` does not find (`0x2DECC`), and **a store outside the image from
  inside `0x2D4EC`'s loop** (kind 0 with `[0x105E2F] >= 13`, or a poked table): the original exits at
  the first such store, the port drops each such byte and stores the ones that fall inside. The
  stub's poisoned registers (`E.callee_clobbers(0x2EA68)` is all six) make the original's run after
  the first stubbed exit meaningless there, so no case goes there. In the port's state the only
  thing these paths can change is which message `menu_fatal_error` receives.
- **`config_storage_read` past 128 cells** (correction 7): refused by the port, not compared.
- **`0x2D962`**: not ported in this batch (the brief; correction 6).

### §S.5.6 The unit test `test_config_storage` and its mutation proofs

`port/tests/test_game.c`, registered after `test_config`. It saves and restores the image, the config
region `0x105D88..0x105F07`, kind 5's table entry and the dword `0x2D48C`. 40 `CHECK` lines: one
evidence line on the kind table (image only, not port logic) and the 39 below (the five of
`roundtrip` run once per kind). Each was seen to fail under the mutation named; the text is that
assertion's own `FAIL` line.

| Case | Assertion | Mutation of `config.c` | First failure |
|---|---|---|---|
| `store` | the first image byte takes DL | `addr <= 0x100CE4` refused | `165 != 52` |
| | its neighbour is not written | `DSW` in place of `DSB` | `18 != 165` |
| | the last image byte | `addr >= 0x1014DB` refused | `165 != 86` |
| | the byte below the image | the lower bound dropped | `119 != 165` |
| | the byte past the image | `addr > 0x1014DC` | `136 != 165` |
| `roundtrip` | the read returns 1 | `fr[0] ^= ch` dropped (`0x2D609`) | `-1 != 1` |
| | the bytes return | the source read upwards | `roundtrip: the record's bytes` |
| | the byte past the destination | `dst[left--]` in the read | `129 != 238` |
| | one byte a cell, `n` cells | the first cell one too high | `roundtrip: one byte a cell, n cells` |
| | kind 0's slot 0 is not written when the slot byte is 2 | the slot byte ignored | `1 != -1` |
| `parity` | a changed second byte of a cell does not matter | the read takes the second byte | `-1 != 1` |
| | a changed cell fails the read | the read returns 1 always | `1 != -1` |
| `bound` | a record that ends at cell `0x7F8` is refused | `> 0x7F8` | `bound: a record that ends at cell 0x7F8 is refused` |
| | one that ends at `0x7F7` is written | `>= 0x7F7` | `90 != 103` |
| `badkind` | kind 10 writes nothing | `kind >= 11` | `badkind: 10` |
| | kind `0x80000000` writes nothing | a signed test | `badkind: negative` |
| `load` | the second old-layout copy's magic lets the 5 bytes in | the fallback dropped; the cell `0x5B` | `load: the second copy's magic lets the 5 bytes in` |
| | the byte below `0x105DDC` | the destination one lower | `113 != 238` |
| | the zero image reads nothing | the magic test dropped | `0 != 4008636142` |
| `tables` | kinds 6..8 read back | the read dropped (`0x2DEE2`) | `tables: read back from the image` |
| | a failed record is encoded again | `r < -1` at `0x2DF51` | `-1 != 1` |
| | the flag zeroes tables 0 and 1 | both clears dropped | `tables: the flag zeroes the first two` |
| | table 2 takes `0x105DDC` | `table == 1` | `tables: the third takes 0x105DDC` |
| | encoded after the copy | `r == -1` at `0x2DF51` | `tables: encoded after the copy` |
| | kinds 3..5 read back | the read dropped (`0x2DFA8`) | `tables: kinds 3..5 read back` |
| | a failed record is encoded again | `r < -1` at `0x2DFDB` | `-1 != 1` |
| | the flag zeroes kinds 3..5 | the clear dropped | `tables: the flag zeroes kinds 3..5` |
| | kind 3 encoded zero | `r == -1` at `0x2DFDB` | `tables: kind 3 encoded zero` |
| `validate_zero` | copy 1 holds the defaults and the magic | `0x2D4EC(1)` dropped (`0x2D8AA`) | `validate_zero: copy 1 holds the defaults and the magic` |
| | copy 2 too | `0x2D4EC(2)` dropped (`0x2D8B4`) | `validate_zero: copy 2 too` |
| `validate_stored` | the field buffer | the first read dropped | `validate_stored: the field buffer` |
| | the trailing-byte record at its slot | the slot byte ignored (`0x2D8C2`) | `validate_stored: the kind 0 record at its slot` |
| | the six tables | `0x2DE98(1)` | `validate_stored: the six tables` |
| | the flags byte is left alone | `r1 >= r2` at `0x2D7C9` | `68 != 64` |
| `copy2` | the second copy's bytes | the copy dropped (`0x2D798`) | `copy2: the second copy's bytes` |
| | flags `0x42` | `\|= 8` | `72 != 66` |
| | the first copy repaired | `0x2D4EC(1)` dropped (`0x2D7A9`) | `copy2: the first copy repaired` |
| | (mirror) flags `0x44` | `\|= 8` | `72 != 68` |
| | (mirror) the second copy repaired | `0x2D4EC(2)` dropped (`0x2D7EB`) | `copy2: the second copy repaired` |

Three assertions written first were removed because no mutation could fail them or another assertion
failed for the same cause: the read's result in `bound` (12 cells of one fill byte check as a
codeword), `config_field_get(0x35)` and the byte `0x105DA7` in `validate_stored`.

`validate_zero` and §S.3: with the real `config_set_defaults` the image holds the default fields, so
its bytes are not §S.3's 22 (that run stubs `0x2CADC`). The unit case asserts that both copies decode
to the live buffer; the byte-exact agreement with §S.3 is `config_validate_chain` `z0` (§S.5.3).

`test_config`'s validate block (the one existing test this batch touches): no assertion value
changed. Its setup now zeroes the image and saves and restores the image and the config region to
`0x105F07`, because the function reads the image (`0x2D729`) and its validators zero the tables up to
`0x105EFB` (`0x2DEDD`, `0x2DFD6`).

### §S.5.7 For the integrator

- **Call site left**: `0x2D962` (`config.c`, the `PORT:` note at the end of `config_validate`),
  correction 6. Wiring it needs a case that reaches `0x2D95D` (a stub that stores `0x105DA4` or
  `0x105DA5`) and the removal of that address from `SA_VALIDATE_DEAD`.
- **Seams added**: `menu_fatal_error` (`PR_SEAM(0x2EA68u, msg)`, `menu.c`), `config_storage_read`
  (`PR_SEAM_RET(0x2E990u, cell, storage_dst_arg(dst), len)`), `config_storage_load`
  (`PR_SEAM0(0x2D638u)`), `hiscore_storage_load` (`PR_SEAM(0x2DE98u, defaults)`),
  `config_storage_validate` (`PR_SEAM(0x2DF8Cu, defaults)`). `config_image_store` has none (it runs on
  both sides under its one caller).
- **Stub clobbers added to the expected dictionary** (the `S sa` lines at its top): `0x2EA68` all six
  registers, `0x2E990` `ebx`, `edx`, `0x2D638`, `0x2DE98`, `0x2DF8C` none; each is
  `E.callee_clobbers`.
- **Existing harness lines removed**: the d6a `config_validate` row (`d6a_validate`, its four
  constants and the `Spec`; `D6A_DEFAULTS` stays), its binding core and 21 table lines, its 41 pin
  lines. Nothing else outside the `sa` blocks changed.
- **Comments this batch made stale and did not edit** (another batch may edit the same lines):
  `config_codeword_len`'s header in `config.c` and `config.h` ("none of those paths runs in the port":
  `0x2D533` and `0x2D8C8` now do), the first comment of `config.h` ("the storage image ... declared
  no-ops this cycle"), and `D5D_TOUCH`'s trailing comment in `tools/diff_verify.py` ("a declared no-op
  in the port").
- **`tools/port_progress.py`** prints `776 1203 65` / `736 736 100` on this branch (five new headers;
  the classification file and the E2 table are untouched, as the brief says).
- **The frame dump** of `./build/prageport --game-dir data/game/C --check 820` is byte-identical
  before the batch, after `0x2D4EC` became real and after `config_validate` changed (one sha256 over
  the 2461 files). The oracles themselves were not run.
- **The full `--self-check`** on this branch (`PR_DIFF_JOBS=4`, rc 0): `973/973 functions VERIFIED;
  14452/14452 mutants detected; 1 named gaps; 607/806 rows with callees closed (167 have none)`.
  Against the baseline `963/963`, `14349/14349`, `604/796`: 10 rows and 103 mutants more, as §S.5.3
  counts them. The harness unittest suite was not run by this batch; the pins it alone sees (one
  clobber set per stub address, the expected dictionary, the pinned keys against the specs, no
  duplicate name) were checked by script.

## §S.6 Batch `sb`: the audit counters

Sources: capstone over `/tmp/pr_sb_img.bin` (fixups applied). `0x2D4EC` (`config_storage_touch`) is a
stub in every row of this section: its body is batch `sa`'s. Rows, bindings and pins are the `SB` blocks
of `tools/diff_verify.py`, `port/tests/diff_runner.c` and `tools/tests/test_diff_verify.py`.

### §S.6.1 The seven functions

Where they live: `0x2E034`, `0x2E0A4`, `0x2E180` and `0x2E934` in `port/src/game/svcmenu.c`, beside
`audit_hist_clear` (`0x2E11C`) and its `AUDIT_STORE`/`AUDIT_DESC` bases (the ledger's ruling);
`0x2DAE4`, `0x32B94` and `0x32BB0` in `port/src/game/config.c`, one block after `config_play_time_snap_b`.

The data the family reads (image):

| Table | Entry | Bytes |
|---|---|---|
| storage descriptors `0x2D444`, 8 bytes `{u16 cell, u16 size, u32 source}` | 3 (histogram 0) | size `0x14`, source `0x105ECD` |
| | 4 (histogram 1) | size `0x14`, source `0x105EE1` |
| | 5 (histogram 2) | size `7`, source `0x105EF5` |
| histogram descriptors `0x2D414`, `0x10` bytes | 0 | `+4` first width 10, `+8` step 10, `+0xC` mask word 1, `+0xE` count byte `0x14` |
| | 1 | 15, 15, mask 2, count `0x14` |
| | 2 | 1, 1, mask 4, count 7 |
| | "3" | the storage descriptors' first 16 bytes: `+4` = `0x105DB0`, `+8` = `0x530000`, `+0xE` = `0x10` |

**`0x2E034` `s32 audit_counter_store(u32 table, u32 index, u32 value)`** — EAX, EDX, EBX.

- `0x2E038 mov esi,edx`; `0x2E03A cmp eax,3; jb`: a table >= 3 returns -1 (`0x2E03F`).
- `0x2E049..0x2E058`: EBP = `0x2D444 + 8 * table + 0x18`; `0x2E05C mov dx,[ebp+2]`, `0x2E060 cmp esi,edx;
  jb`: an index >= the size word returns -1 (`0x2E064`).
- `0x2E06E..0x2E082`: `or [((table + 3) >> 3) + 0x105DD8], 1 << ((table + 3) & 7)`.
- `0x2E088 mov eax,[ebp+4]`; `0x2E08B and ebx,0xff`; `0x2E091 mov [esi+eax],bl`.
- `0x2E094 mov eax,edx` (= table + 3); `0x2E096 call 0x2d4ec`; `0x2E09B mov eax,ebx`; `ret`.

**`0x2E0A4` `s32 audit_counters_halve(u32 table)`** — EAX.

- `0x2E0AA cmp eax,3; jb`: else -1.
- `0x2E0B9..0x2E0E8`: the same dirty bit; `0x2E0E5 mov eax,[edx+4]` (the source).
- `0x2E0EE..0x2E0F9`: EDX = the size word - 1; `test edx,edx; jl 0x2e10a`.
- `0x2E0FD..0x2E108`: `mov cl,[eax]; inc eax; shr cl,1; dec edx; mov [eax-1],cl; test edx,edx; jge`.
- `0x2E10A lea eax,[ebx+3]; call 0x2d4ec; xor eax,eax; ret`. The function ends at `0x2E118`.

**`0x2E180` `s32 audit_counter_add(u32 table, u32 index, u32 add)`** — EAX, EDX, EBX.

- `0x2E184 mov edi,eax; mov ebp,edx`; `0x2E188 cmp eax,3; jb` else ESI = -1.
- `0x2E194..0x2E1A7`: the size test of `0x2E034` (`jb`), else ESI = -1; `0x2E1B3 movzx esi,byte [edx+eax]`.
- `0x2E1B7 test esi,esi; jl 0x2e210`: -1 is returned.
- `0x2E1BB add esi,ebx`; `0x2E1BD cmp esi,0xff; jle 0x2e205` (signed).
- `0x2E1C5..0x2E1D6`: EBX = the word `[table * 0x10 + 0x2D420]`, EDX = 2.
- `0x2E1DC..0x2E201`: `if (ebx & (1 << edx)) { eax = 0x2E0A4(edx); if (eax < 0) return -1; }`, EDX from 2
  down to 0 (`test eax,eax; jge` `0x2E1F0`; `dec edx; test edx,edx; jge` `0x2E1FE`).
- `0x2E203 sar esi,1`.
- `0x2E205 mov ebx,esi; mov edx,ebp; mov eax,edi; call 0x2e034`; `0x2E210 mov eax,esi`; `ret`.

The stored value is `(old + add) >> 1`: the counter is read before the halving and the halved table's
own byte is overwritten. The result is the whole sum, not `0x2E034`'s masked byte.

**`0x2E934` `s32 audit_post(u32 kind, u32 value)`** — EAX, EDX.

- `0x2E93A cmp eax,3; jbe 0x2e949`: a kind above 3 returns -1 (`0x2E93F`). Kind 3 passes this test.
- `0x2E949..0x2E94E`: EBX = `0x2D414 + 0x10 * kind`; `0x2E954 mov ecx,[ebx+4]`; `xor eax,eax`.
- `0x2E959 cmp edx,ecx; jb 0x2e97a`; else `eax = 1; edx -= ecx; jmp 0x2e970`.
- `0x2E966..0x2E96E`: `ebp = [ebx+8]; cmp edx,ebp; jb 0x2e97a; inc eax; sub edx,ebp`.
- `0x2E970..0x2E978`: `xor ecx,ecx; mov cl,[ebx+0xe]; dec ecx; cmp eax,ecx; jl 0x2e966`.
- `0x2E97A mov ebx,1; mov edx,eax; mov eax,esi; call 0x2e180`; `ret` with its EAX.

**`0x2DAE4` `s32 audit_add(u32 field, u32 n)`** — EAX, EDX.

- `0x2DAE7 mov ebx,eax; call 0x2d974`; `0x2DAEE test eax,eax; jl 0x2db54`: a negative read is returned
  with no store.
- `0x2DAF2 lea ecx,[eax+edx]`; `0x2DAF5 cmp ebx,3; jne 0x2db49`.
- `0x2DAFA xor eax,ecx; cmp eax,0x100; jl 0x2db49` (signed).
- `0x2DB03..0x2DB10`: EDX = the byte `[0x105E2F]` + 1 (zero-extended, then `inc edx`).
- `0x2DB0B mov eax,0x26; call 0x2d4b4; imul eax,edx; lea esi,[eax+0x1a3]`.
- `0x2DB1F mov eax,0x26; call 0x2d4b4; add eax,esi; cmp eax,0x7f8; jb 0x2db34; xor edx,edx`.
- `0x2DB34 mov ah,[0x105dd8]; or ah,6; mov [0x105e2f],dl; mov [0x105dd8],ah`.
- `0x2DB49 mov edx,ecx; mov eax,ebx; call 0x2da0c`; `0x2DB52 mov eax,ecx`; `ret`.

`0x2D4B4(0x26)` is 45 (the existing `config_codeword_len`), so the slot byte runs 0..35 and wraps:
`0x1A3 + 45 * 35 + 45 = 0x7F7 < 0x7F8`, `0x1A3 + 45 * 36 + 45 >= 0x7F8`. This is the "clamp" of the
plan's unit case: it is a wrap to 0, not a saturation, and it is the only bound in `0x2DAE4`. The sum
itself is not clamped here (the clamps at 2000 and 200 are the caller `0x1EC38`'s).

**`0x32B94` `void audit_add_if_bit0(u32 flags)`** — AL.

`push edx; test al,1; je 0x32ba8; mov edx,1; mov eax,0xe; call 0x2dae4; 0x32BA8 pop edx; lea eax,[eax];
0x32BAC ret`. No `ret` of its own.

**`0x32BB0` `void audit_characters(u32 c0, u32 c1)`** — EAX, EDX.

`push ebx; mov ebx,edx; cmp eax,6; ja 0x32bd7; mov edx,1; add eax,0x1b; call 0x2dae4; cmp ebx,6; ja
0x32bd7; mov edx,1; lea eax,[ebx+0x1b]; call 0x2dae4; 0x32BD7 pop ebx; ret`. A first character above 6
skips the second one too.

### §S.6.2 Corrections (the raw over the plan and §S.2-§S.4)

| # | Address | The plan or the record said | The bytes say |
|---|---|---|---|
| 1 | `0x2E08B`, `0x2E091` | `audit_counter_store(table, value)`, EAX and EDX | three arguments: EAX = table, EDX = **index**, EBX = value (`and ebx,0xff; mov [esi+eax],bl`); it returns the masked byte |
| 2 | `0x2E1BB` | `audit_counter_add(table, value)`, EAX and EDX | three arguments: EAX = table, EDX = **index**, EBX = the addend (`add esi,ebx`); it returns the whole sum (`mov eax,esi` `0x2E210`) |
| 3 | `0x2E18B`, `0x2E1B9` | "-1 path when `table >= 3`" | also when the index is not below the size word (`0x2E1A7`), and when a halving returns negative (`0x2E1F2`) |
| 4 | `0x2E93D` | "-1 when `kind > 3`" | true, and kind 3 goes on: it reads the 16 bytes after the three histogram descriptors (the storage descriptors) and is refused later by `0x2E180` (`0x2E18B`), with nothing stored |
| 5 | `0x1E9BB`, `0x1E9EB` | "in `0x1E988`, the ported `hiscore_audit_reset_due`" (§S.4, the brief) | `0x1E988` ends at its `ret`s `0x1E9AC`/`0x1E9AF` and holds no `0x2DAE4` call. The two sites are in two fragments, `0x1E9B0..0x1E9DC` (`0x2DAE4(0x27, 1)`, clamp 2000) and `0x1E9E0..0x1EA07` (`0x2DAE4(0x26, 1)`, clamp 200), that nothing references: no rel32 `call`/`jmp`, no near `jcc`, no short jump from outside and no absolute dword in the fixed-up image names `0x1E9B0` or `0x1E9E0`, and neither is a `symbols.h` function. Dead code: nothing to port, nothing to wire. `hiscore_audit_reset_due` is right as it is (two `0x2D974` reads) |
| 6 | `0x2DB2B` | the plan's unit case `clamp` ("each clamp the raw applies") | `0x2DAE4` clamps nothing: its one bound is the slot byte's wrap to 0 (above) |
| 7 | `0x32BB6` | the plan's unit case "a character below 7, at 7" | at 7 the first character ends the call before the second is looked at (`ja 0x32bd7`) |
| 8 | `0x27DF0` | the port's old `PORT:` note at `0x27E28` ("(c0, c1) or (c, -1) with DS_00104ABC == 1") | the dword `0x104ABC` is read again after `0x32B4C` (`cmp dword [0x104abc],1`), as are slot 0's `+0x63` byte and both characters; the port reads them there and the rows `flow_match_end_rr`/`_rr2` prove it |

### §S.6.3 The rows

New rows (12):

| Row | Entry | Cases | Blocks | Mutants | EAX mask | Callees |
|---|---|---|---|---|---|---|
| `audit_counter_store` | `0x2E034` | 7 | 5/5 | 14 | full | `0x2D4EC` stub |
| `audit_counters_halve` | `0x2E0A4` | 6 | 5/5 | 14 | full | `0x2D4EC` stub |
| `audit_counter_add` | `0x2E180` | 13 | 15/15 | 21 | full | `0x2E0A4`, `0x2E034` stubs |
| `audit_post` | `0x2E934` | 14 | 8/8 | 17 | full | `0x2E180` stub |
| `audit_add` | `0x2DAE4` | 11 | 8/8 | 23 | full | `0x2D974`, `0x2DA0C` stubs; `0x2D4B4` both sides |
| `audit_add_if_bit0` | `0x32B94` | 4 | 3/3 | 6 | 0 | `0x2DAE4` stub |
| `audit_characters` | `0x32BB0` | 6 | 4/4 | 11 | 0 | `0x2DAE4` stub |
| `config_play_time_close_audit` | `0x32A3C` | 6 | 12/12 | 23 | 0 | `0x2DAE4` stub; `0x32970` original only |
| `config_play_time_snap_audit` | `0x32B00` | 6 | 3/4, 1 named | 12 | 0 | `0x2E934` stub |
| `config_play_time_snap_b_audit` | `0x32B4C` | 6 | 3/4, 1 named | 12 | 0 | `0x2E934` stub |
| `flow_match_end_rr` | `0x27DC8` | 1 | 6/11, 5 named | 3 | 0 | the plain row's, `0x32B4C` writing |
| `flow_match_end_rr2` | `0x27DC8` | 1 | 6/11, 5 named | 3 | 0 | the plain row's, `0x32B4C` writing |

Masks. The five functions that return a value keep the full mask (the port returns the raw's EAX on
every path). `0x32B94`: `xor eax,eax` `0x27870` follows its one site. `0x32BB0`: `mov eax,ebx` `0x27E37`
or `xor eax,eax` `0x27E3E` follows its one site. The three `_audit` rows have mask 0 because EAX at
return is the last stub's: `0x32A3C` is followed by `mov eax,0x10` (`0x2709E`, and `0x42420` after `xor
ah,ah` `0x42413`, a write), `lea eax,[edx*8]` (`0x28661`, `0x289BA`, `0x28AE0`, `0x28B83`), `mov
eax,0x33` (`0x415D2`), and at `0x27886` it is the mode handler `0x277C0`'s EAX at return (void: the
`game_mode_0f_step` row). `0x32B00`: `mov eax,0xd` `0x28E49`; `mov eax,[0x104abc]` `0x25EF8` or `xor
eax,eax` `0x25F0B`; `lea eax,[ebx*8]` `0x289E5` or `xor eax,eax` `0x2894B`. `0x32B4C`: `xor eax,eax`
`0x27E02`/`0x27E10`/`0x27E1B`; `xor eax,eax` `0x25F0B`, or `0x25F6B` returns it from the mode handler
`0x25C88` (void: the `game_mode_05_step` row). The D1I rows of the three keep their full mask and their
arms (mode 0, arm != 0); their unhit reason (`D1I_PT_AUDIT`) now names the `_audit` rows.

Stub clobbers, from `E.callee_clobbers` over the image: `0x2E034` (ebx, edx), `0x2E0A4` (), `0x2E180`
(ebx, edx), `0x2E934` (edx), `0x2DAE4` (edx), `0x32B94` (edx), `0x32BB0` (edx). `0x32B94`'s `edx` is an
over-approximation: it pushes and pops EDX, but the `lea eax,[eax]` between the `pop` and the shared
`ret` defeats the scan's pops-before-ret rule (`0x29D04`'s case). It is harmless under its one caller
(`xor edx,edx` `0x27872` follows) and no `_CALLEE_CLOBBER_FIXES` entry was added. The seven are added
at the top of the expected dictionary of `test_each_stub_declares_the_registers_its_callee_clobbers`.

Rows of earlier batches changed by a call site (each stubbed `0x2DAE4`/`0x2E934`/`0x32B94`/`0x32BB0`
where it had run it "turned off" by a poke; the pokes `D5D_AUDIT_OFF`, `D2E_M12_AUDIT_OFF`,
`D3A_AUDIT_OFF_F`, `D4A_AUDIT_OFF_D`, `D5I_AUDIT_OFF_11` and `D4C_AUDIT_OFF` are gone):

| Row | Entry | Site | Cases | Blocks | Mutants added | What moved in the older pins |
|---|---|---|---|---|---|---|
| `hiscore_rank_single` | `0x1EC38` | `0x1EC50`, `0x1EC84` | 6 -> 14 | 5/7 + 2 named -> 7/7 | 16 | call indexes + 1; the new cases join 14 case sets |
| `game_mode_24_step`, `_rr` | `0x26F58` | `0x270A9` | 6, 2 | 3/3 | 5 | `call #10` appears; the y-commit moves `#9` -> `#10` |
| `game_mode_0e_step`, `_rr`, `_rr2` | `0x27A2C` | `0x27A5F` | 15, 2, 2 | 19/19 | 6 | the taken arm's indexes + 1; `@b1flate` gains `t0` |
| `flow_round_over` | `0x27ED8` | `0x27F1F` | 8 | 9/9 | 5 | call indexes + 1 |
| `flow_player_join` | `0x28DA4` | `0x28E53` | 5 | 1/1 | 5 | `call #6` appears |
| `game_mode_12_step_s7` | `0x41C28` | `0x42425` | 9 | 20/67 | 5 | `call #6` appears (5 pins) |
| `game_mode_0f_step` | `0x277C0` | `0x2786B` | 6 | 3/3 | 5 | `call #7` appears; the close moves `#6` -> `#7` |
| `flow_match_end` | `0x27DC8` | `0x27E28` | 5 | 11/11 | 8 | call indexes + 1 |
| `fight_char_confirm` | `0x43D60` | `0x43E77` | 14 -> 15 | 16/16 | 7 | call indexes + 1; the new case `f14` joins 20 case sets |

No case set of an older mutant lost a case. The 62 added mutants are pinned in the `SB` block
(`SB_KINDS`, `SB_CASES`); the older ones were re-pinned in place. The mutant ids of the added calls are
`SB_M_*` (`diff_runner.c`, above every batch's own ids), read by the older batches' mutant cores, which
restate the call in raw order.

### §S.6.4 The call sites

| Site | In | Port | The raw's call |
|---|---|---|---|
| `0x1EC50` | `0x1EC38` `hiscore_rank_single` | `flow.c` | `0x2DAE4(0x27, 1)`; `(eax & 0xFFFF) > 0x7D0` -> `0x2DA0C(0x27, 0x7D0)` (`0x1EC55..0x1EC6B`) |
| `0x1EC84` | same, rank >= 10 | `flow.c` | `0x2DAE4(0x26, 1)`; `eax > 0xC8` signed -> `0x2DA0C(0x26, 0xC8)` (`0x1EC89..0x1EC9A`) |
| `0x270A9` | `0x26F58` `game_mode_24_step` | `flow.c` | `0x2DAE4(0x10, 1)` after `[0x104B19] = 0` |
| `0x27A5F` | `0x27A2C` `game_mode_0e_step` | `flow.c` | `0x2DAE4(0x11, 1)` after `[0x104B1F]` is stored |
| `0x27F1F` | `0x27ED8` `flow_round_over` | `flow.c` | `0x2DAE4(0xF, 1)` after the two side resets |
| `0x28E53` | `0x28DA4` `flow_player_join` | `flow.c` | `0x2DAE4(0xD, 1)` after `0x32B00` |
| `0x42425` | `0x41C28` `game_mode_12_step`, state 7 | `flow.c` | `0x2DAE4(0x10, 1)` after `[0x104B19] = 0` |
| `0x2786B` | `0x277C0` `game_mode_0f_step` | `flow.c` | `0x32B94(the byte 0x104B1F, zero-extended)` |
| `0x27E28` | `0x27DC8` `flow_match_end` | `flow.c` | `0x32BB0`: `[0x104ABC] == 1` (read after `0x32B4C`) -> (`[0x107813] != 0 ? [0x1078BE] : [0x10782A]`, -1), else (`[0x10782A]`, `[0x1078BE]`), bytes zero-extended |
| `0x32A85..0x32AF5` | `0x32A3C` `config_play_time_close` | `config.c` | six `0x2DAE4` posts, §S.6.1's table in the function's header |
| `0x32B35` | `0x32B00` `config_play_time_snap` | `config.c` | `0x2E934(1, t)` after `[0x107478] = t` |
| `0x32B7E` | `0x32B4C` `config_play_time_snap_b` | `config.c` | `0x2E934(0, t)` after `[0x107480] = t` |
| `0x43E77` | `0x43D60` `fight_char_confirm` | `fight.c` | `0x2E934(2, the byte 0x10816A[side] sign-extended)` (`sar edx,0x18` `0x43E74`) |

Every containing function has a differential row, and each row now records the call. Not wired here:
`0x2D962` (the integrator's), `0x32A17` (inside the host-owned `0x32970`; omitted and named in
`config_play_time_close`'s `PORT:` note), `0x1E9BB`/`0x1E9EB` (dead, correction 5).

What each post writes that the port did not write before. `0x2DAE4(f, n)` with a non-negative field:
the field's low byte `0x105DAF + trail`, its nibbles in `0x105DE1..`, and through `0x2DA0C` the dirty
byte `0x105DD8 |= 7` and `0x2D4EC(0)`, `(1)`, `(2)` (the stored image, once `sa`'s body is in):

| Field | Posted by | Low byte | Nibble bytes |
|---|---|---|---|
| `0x06`, `0x07`, `0x08`, `0x09` | `0x32A3C` | `0x105DB6..0x105DB9` | `0x105DE8..0x105DEC` |
| `0x0A`, `0x0B`, `0x0C` | `0x32A3C` | `0x105DBA..0x105DBC` | `0x105DEC..0x105DF1` |
| `0x0D` | `0x28E53` | `0x105DBD` | `0x105DF1..0x105DF2` |
| `0x0E` | `0x2786B` (bit 0 of `0x104B1F`) | `0x105DBE` | `0x105DF2..0x105DF3` |
| `0x0F` | `0x27F1F` | `0x105DBF` | `0x105DF3..0x105DF4` |
| `0x10` | `0x270A9`, `0x42425` | `0x105DC0` | `0x105DF4..0x105DF5` |
| `0x11` | `0x27A5F` | `0x105DC1` | `0x105DF5..0x105DF6` |
| `0x12`, `0x13` | `0x32A3C` | `0x105DC2`, `0x105DC3` | `0x105DF6..0x105DFC` |
| `0x1B..0x21` | `0x27E28` | `0x105DCB..0x105DD1` | `0x105E03..0x105E0A` |
| `0x26` | `0x1EC84` | `0x105DD4` | `0x105E10` |
| `0x27` | `0x1EC50` | `0x105DD5` | `0x105E11` |

`0x2E934(kind, v)`: one counter byte of histogram `kind` (0: `0x105ECD..0x105EE0`, 1:
`0x105EE1..0x105EF4`, 2: `0x105EF5..0x105EFB`), all of that histogram's bytes when the counter passes
`0xFF`, bit `kind + 3` of `0x105DD8`, and `0x2D4EC(kind + 3)`. No site of this batch posts field 3, so
the slot byte `0x105E2F` is stepped only through the omitted `0x32A17`.

### §S.6.5 The unit test `test_audit` and its mutation proofs

`port/tests/test_game.c`, registered once (`X(test_audit)`). It saves and restores `0x105DA0..0x105EFF`.
Each mutation was applied alone to the function under test, the suite rebuilt and run, and reverted.

| Function | Assertion | Mutation | First FAIL |
|---|---|---|---|
| `audit_counter_store` | the result is the low byte | drop `value &= 0xFF` | `4660 != 52` |
| | index == size refused | index `>=` -> `>` | `119 != -1` |
| | table 3 refused | table `>=` -> `>` | `119 != -1` |
| | the dirty byte is `0x90` | bit `table` for `table + 3` | `130 != 144` |
| | the byte `0x105DD9` stays | the dirty byte's index 1 | `128 != 144` |
| | the next table's first byte stays | a word store | `0 != 104` |
| | the counter and its neighbour | store at `index - 1` | `103 != 52` |
| `audit_counters_halve` | `0xFF` -> `0x7F` | arithmetic shift | `255 != 127` |
| | the last counter `0x81` -> `0x40` | size - 2 iterations | `129 != 64` |
| | `0x15` bytes changed | size + 1 iterations | `22 != 21` |
| | the dirty byte | bit `table` | `130 != 144` |
| | table 3 refused | table `>=` -> `>` | `0 != -1` |
| | the result 0 | return 1 | `1 != 0` |
| `audit_counter_add` | `0xFE + 1` stays `0xFF` | `> 0xFF` -> `>=` | `127 != 255` |
| | `0x100` -> `0x80` | drop `v >>= 1` | `256 != 128` |
| | the neighbour `9` -> `4` | drop the `0x2E0A4` call | `9 != 4` |
| | table 1 is not halved | the mask test dropped | `42 != 84` |
| | table 3 refused | table `<` -> `<=` | `1 != -1` |
| | index == size refused | index `<` -> `<=` | `1 != -1` |
| | the counter holds the sum | drop the `0x2E034` call | `254 != 255` |
| `audit_post` | 9 -> bucket 0, 10 -> bucket 1 | first width `>=` -> `>` | `0 != 1` |
| | `0xFFFFFFFF` -> the last bucket | signed first-width test | `0 != 19` |
| | the same | limit count for count - 1 | `-1 != 19` |
| | 190 -> bucket 19 | limit count - 2 | `18 != 19` |
| | 10 -> bucket 1 (and 189 -> 18, kind 1's 30 -> 2) | drop `value -= first` | `2 != 1` |
| | 9 -> bucket 0 | the bucket starts at 1 | `1 != 0` |
| | kind 1's 30 -> bucket 2 | descriptor stride 8 | `256 != 258` |
| | the result is `0x2E180`'s | return 0 | `0 != 108` |
| | kind 2's -1 -> bucket 6 | signed step test | `1 != 19` |
| `audit_add` | the field holds the sum | drop the `0x2DA0C` call | `254 != 259` |
| | the result is the sum | return the old value | `254 != 259` |
| | the slot byte stays for field `0x10` | the step for every field | `8 != 7` |
| | a negative field is returned and kept | drop the `jl` return | `-2147483642 != -2147483643` |
| | `0xFF -> 0x100` steps the slot | `sum - old` for `old ^ sum` | `7 != 8` |
| | `0xFE -> 0xFF` does not | threshold 1 | `8 != 7` |
| | slot 35 -> 0 | drop the trailing `0x2D4B4` | `36 != 0` |
| | slot 34 -> 35 | `0x1A4` for `0x1A3` | `0 != 35` |
| `audit_add_if_bit0` | `0x102` posts nothing | `flags != 0` | `19 != 18` |
| | `0xFFFFFF01` posts | the test inverted on bit 8 | `18 != 19` |
| `audit_characters` | field `0x22` and `0x1C` stay under (7, 1) | c0 `> 6` -> `> 7` | `61 != 60` |
| | field `0x22` stays under (6, 7) | c1 `> 6` -> `> 7` | `67 != 66` |
| | field `0x1A` stays under c1 = -1 | signed c1 test | `59 != 58` |
| | a first above 6 skips the second | fall through to c1 | `61 != 60` |
| | field `0x21` gets the second post | `0x1C` for `0x1B` | `66 != 67` |

Named limits of the unit test, each covered by the row instead:

- `0x2DB43` (`audit_add`'s own `or 6` into `0x105DD8`): the mutation "store 0 there" survives the unit
  test, because `0x2DA0C` ORs 6 into the same byte right after (`0x2DA4E`). The row's `@no6` and `@or4`
  catch it (`0x2DA0C` is a stub there, and the memory at its call is compared).
- `audit_post` for kind 3 and kind 4 store nothing whichever function refuses them, so no unit
  assertion can tell `0x2E93D` from `0x2E18B`; the row does (`p7`/`p8` record `0x2E180(3, ...)`,
  `p9`/`pa` record no call; mutants `@kle`, `@ksigned`). The plan's unit case "`kind = 4` returns -1 and
  writes nothing" is therefore a row case, not a unit assertion.
- `0x2D4EC`: no unit assertion sees its calls (a no-op body in this tree); every row records them
  (`@kind`, `@notouch`, `@touchearly`).
- The call sites have no unit assertion of their own: each is observed by its row's call list.
- Nine evidence lines in `test_audit` read image data only (the descriptors at `0x2D45E`, `0x2D460`,
  `0x2D46E`, `0x2D470`, `0x2D420`, `0x2D418`, `0x2D42C`, `0x2D422`, `0x2D442`): labelled as such, not
  port logic.

## §S.7 Batch `sc`: the save file (`0x1AFE8`, `0x1B084`) and its four call sites

Sources: capstone over `/tmp/pr_sc_img.bin`, `E.callee_clobbers`, `E.static_scan`. Port: `config_file_load` and
`config_file_save` at the end of `port/src/game/config.c`; the sites in `flow.c` and `svcmenu.c`.

### §S.7.1 `0x1AFE8` `config_file_load`

No argument. `push ebx; push ecx; push edx; push esi` … `pop`s, `ret` (`0x1B080`). 11 blocks.

| Block | Instructions | What it does |
|---|---|---|
| `0x1AFE8` | `call 0x10D34`; `mov edx,0x80598; mov eax,0xA2C5C; call 0x61C60`; `mov esi,eax; test eax,eax; je 0x1B05B` | `fopen("cmos", "rb")` (the strings at `0xA2C5C`, `0x80598`); NULL sets the flag |
| `0x1B006` | `mov ebx,0x7f8; mov edx,1; mov ecx,eax; mov eax,0x100ce4; call 0x61CF6`; `mov edx,eax; mov eax,esi; call 0x61EC0`; `cmp edx,0x7f8; jne 0x1B04D` | `fread(0x100CE4, 1, 0x7F8, file)`, the count kept in EDX across `fclose(file)` (`0x61EC0` pushes and pops EDX; its EAX is not read: `0x1B02D` and `0x1B04D` load EAX) |
| `0x1B02D`, `0x1B031` | `xor eax,eax; xor dl,dl`; `mov dh,[eax+0x100ce4]; inc eax; xor dl,dh; cmp eax,0x7f7; jl` | DL = the XOR of bytes `0..0x7F6` (signed bound, `0x7F7` passes) |
| `0x1B041` | `cmp dl,[0x1014db]; jne 0x1B05B` | against the last byte |
| `0x1B049` | `xor cl,cl; jmp 0x1B05D` | flag 0 |
| `0x1B04D` | `mov eax,1; mov cl,1; call 0x62003; jmp 0x1B05D` | a count other than `0x7F8`: `exit(1)` |
| `0x1B05B` | `mov cl,1` | flag 1 |
| `0x1B05D`, `0x1B061` | `test cl,cl; je 0x1B075`; `mov ebx,0x7f8; mov edx,0xff; mov eax,0x100ce4; call 0x61A70` | the flag fills the image with `0xFF` |
| `0x1B075` | `call 0x10D0C; mov al,cl`; pops; `ret` | AL = the flag |

`0x62003` is the runtime's `exit`: `push edx; mov edx,eax; call [0xEF934]; call [0xEF938]; mov eax,edx; call
0x6201B`, and `0x6201B` ends `jmp 0x626EB` → `0x62710 push eax … mov ah,0x4c; int 0x21`. The three hook dwords
hold `0x62002`, a `ret`. It does not return, so the `jmp 0x1B05D` after it is never reached.

EAX at return is AL = the flag over what the code before left: `0x10D0C` loads EAX only when the byte `0x101520`
is 0, and with it set the success path returns `0x700` (the loop's `0x7F7` with AL cleared). The one caller,
`0x2F9CF`, does not read it: `call 0x13ADC` follows, whose first use of EAX is `mov ah,1` (`0x13ADF`).

### §S.7.2 `0x1B084` `config_file_save`

No argument. Same prologue and epilogue, `ret` at `0x1B116`. 8 blocks.

| Block | Instructions | What it does |
|---|---|---|
| `0x1B084` | `call 0x10D34`; `mov edx,0x8059c; mov eax,0xA2C5C; call 0x61C60`; `mov esi,eax; test eax,eax; je 0x1B109` | `fopen("cmos", "wb")`; NULL sets the flag and does nothing else |
| `0x1B0A2` | `mov eax,1; call 0x2D4EC`; `mov eax,2; xor dl,dl; call 0x2D4EC`; `xor eax,eax` | records 1 and 2 are encoded; DL is cleared before the second call, which keeps EDX (`E.callee_clobbers(0x2D4EC) = ()`) |
| `0x1B0BA` | `mov dh,[eax+0x100ce4]; inc eax; xor dl,dh; cmp eax,0x7f7; jl` | DL = the XOR of bytes `0..0x7F6` |
| `0x1B0CA` | `mov ebx,0x7f8; mov eax,0x100ce4; mov [0x1014db],dl; mov ecx,esi; mov edx,1; call 0x62031`; `mov edx,eax; mov eax,esi; call 0x61EC0`; `cmp edx,0x7f8; je 0x1B105` | the XOR is stored, then `fwrite(0x100CE4, 1, 0x7F8, file)` and `fclose` (its EAX not read) |
| `0x1B0F7` | `mov eax,1; mov dl,1; call 0x62003; jmp 0x1B10B` | a short write: `exit(1)` |
| `0x1B105` | `xor dl,dl; jmp 0x1B10B` | flag 0 |
| `0x1B109` | `mov dl,1` | flag 1 |
| `0x1B10B` | `call 0x10D0C; mov al,dl`; pops; `ret` | AL = the flag |

No caller reads AL: `0x1BEBA call 0x10D34` (which does not read EAX: it loads it at `0x10D43` or returns),
`0x24AA6 mov edx,1; mov eax,0x1044f4`, `0x2CBB4 call 0x2C304` (`mov eax,0x29` first). `0x2CB94` keeps the menu's
result in EDX across the call (`0x2CBAD mov edx,eax` … `0x2CBB9 mov eax,edx`): `0x1B084` pushes and pops EDX.

### §S.7.3 The host mapping (`PORT:`, named in `config.c`)

| Raw | Port | Difference |
|---|---|---|
| `0x10D34` before and `0x10D0C` after the file access (`0x1AFEC`, `0x1B075`, `0x1B088`, `0x1B10B`) | left out | the CD-drive switch, host-owned (record §50-E). With the `-f` byte `0x101520` set (`0x1BEDC`) both return at once (`cmp byte [0x101520],0; jne`, `0x10D38`/`0x10D10`): the port is that path |
| `fopen("cmos","rb")` + `fread` + `fclose` | `host_save_read(mem + 0x100CE4, 0x7F8)` | the path is the save file, else the game dir's `cmos`/`CMOS`, not the current directory. 0 = the raw's NULL |
| a count below `0x7F8` (`0x1B04D`) | the same 0 from `host_save_read` | **the raw exits the program** (`0x62003(1)`); the port fills `0xFF` and returns 1, as for a missing file. The host call cannot tell the two apart |
| a file longer than `0x7F8` | the same 0 | **the raw reads its first `0x7F8` bytes** and checks them; the port's host refuses another length (`host.c save_read_exact`), so a long file whose head is a valid image loads in the raw and gives `0xFF` in the port |
| `fopen("cmos","wb")` | `host_save_writable()` | the raw truncates the file at the open; the host probes in append mode and truncates at the write. 0 = NULL, and a run with no save path |
| `fwrite` + `fclose` | `host_save_write(mem + 0x100CE4, 0x7F8)` | the raw does not read `fclose`'s result; the host reports a failed close as a failed write, so the port can return 1 where the raw returns 0. No caller reads AL |
| a short write (`0x1B0F7`) | returns 1 | the raw exits the program |

The two wrong-length rows contradict the plan's Review Focus 1 ("the image becomes `0xFF` as at `0x1B05D`") and
the spec's §3 ("a short read … fills the image with `0xFF`"): the raw has no such path. The port implements the
plan's behaviour, which is what `host_save_read`'s contract gives, and the unit cases `short` and `long` pin it.

### §S.7.4 The rows

Both run the raw under code pokes: each of its three file `call`s is `mov eax,<result>` (`B8 imm32`), the
case's value, and the binding reads the same dwords from the poked bytes to set up a scratch save path
(`sc_1afe8_run`, `sc_1b084_run` in `diff_runner.c`). `0x10D34`, `0x10D0C` and the memset `0x61A70` (with
`0x65490`/`0x654C7`) run from their bytes, the first two with the byte `0x101520` = 1. The rows are blind to
the file calls' arguments (read from the bytes above). No new seam, no `tools/diff_emu.py` change.

| Row | Entry | Cases | Blocks | Mutants | EAX mask | Notes |
|---|---|---|---|---|---|---|
| `config_file_load` | `0x1AFE8` | 6 (`l0`..`l5`) | 10/11 | 12 | `0xFF` | `0x1B04D` named unhit (the `exit` block). Port side: the file holds the case's image and the buffer every byte XOR `0x5A`, so only the read gives the original's bytes |
| `config_file_save` | `0x1B084` | 4 (`s0`..`s3`) | 7/8 | 18 | `0xFF` | `0x1B0F7` named unhit. `0x2D4EC` is a stub that stores two bytes at image offset `0x20 + kind`. The binding reads the written file back and sets the byte `0x3900200` unless it is the image's `0x7F8` bytes at return |

Mask `0xFF` although no caller reads AL: `mov al,cl` / `mov al,dl` is each function's last store to EAX and the
port returns the same flag.

Cases. `l0`: no handle over a valid image (it must not be consulted). `l1`: a valid image, `fclose` = -1.
`l2`: one checksum bit off, dirty registers. `l3`: the last byte is the XOR of the first `0x7F6` bytes only.
`l4`: the zero image (the captures' file), handle 1. `l5`: handle `0x100`, a second valid image. `s0`: no handle,
a seeded last byte that must stay. `s1`: a handle, `0x2D4EC`'s EAX `0x12345678`, `fclose` = -1. `s2`: handle
`0x100`, dirty registers. `s3`: the zero image.

Mutants and the cases that catch them (pinned as `SC_KINDS`/`SC_CASES`):

| Mutant | What it is | Caught by |
|---|---|---|
| `config_file_load@noread` | no host read | `l0 l1 l4 l5` (byte, eax) |
| `@dst` / `@readlen` | reads to image + 1 / reads `0x7F7` bytes | `l1 l2 l3 l4 l5` / `l1 l4 l5` |
| `@len6` / `@len8` | XOR over `0x7F6` / `0x7F8` bytes | `l1 l3 l5` / `l1 l5` |
| `@add` / `@sumbyte` | an additive sum / compares with `0x1014DA` | `l1 l5` |
| `@nofill` / `@fill0` / `@filllen` | no fill / fills 0 / fills `0x7F7` bytes | `l0 l2 l3` (byte) |
| `@fillall` | fills on success too | `l1 l4 l5` (byte) |
| `@flag` | the flag inverted | all six (eax) |
| `config_file_save@gatelate` / `@nogate` | `0x2D4EC` before the open test / no open test | `s0` (call #0, call #1, byte) |
| `@sumfirst` / `@summid` | the XOR stored before the calls / between them | `s1 s2 s3` (call memory, byte) |
| `@notouch1` / `@notouch2` / `@touchorder` / `@touch0` | a call dropped / swapped / kinds 0 and 1 | `s1 s2 s3` (call #0/#1) |
| `@nosum` / `@add` / `@sumat` | no store / additive / stored at `0x1014DA` | `s1 s2 s3` (byte) |
| `@len6` / `@len8` | XOR over `0x7F6` / `0x7F8` bytes | `s1 s2` (byte) |
| `@flag` | the flag inverted | all four (eax) |
| `@nowrite` / `@writefirst` / `@wlen` / `@wsrc` | no write / written before the XOR store / `0x7F7` bytes / from image + 1 | `s1 s2 s3` (the verdict byte `0x3900200`) |

Each row was run ten times: `1/1 functions VERIFIED`, `12/12` and `18/18 mutants detected` every time.

### §S.7.5 The call sites

| Site | In | Port | Row of the caller | Unit assertion |
|---|---|---|---|---|
| `0x2F9CF` | `0x2F9CC` (inlined in `game_init`) | `(void)config_file_load();` first line of the inlined block, before `DS_0010740C` and `config_validate()` | none (`game_init`) | the `PR_RESTART` driver boots from a scratch save and finds its bytes `0x1014DA/DB` = `0xA5` |
| `0x1BEB5` | `0x1BE30` `game_shutdown` | `(void)config_file_save();` after the audio teardown | none (not rowed, track D d3d) | `test_config_file`: the teardown writes the file |
| `0x24AA1` | `0x249F0` `game_quit_prompt` | between `sound_resume()` and `game_restart_longjmp()` | `game_quit_prompt`: the arm `0x24A9C` stays its named gap (the `0x65431` longjmp does not return into the function); the reason text no longer says the port defers the writer | `ch_check_quit_prompt`: only pass 2 (ABANDON, yes) writes the file |
| `0x2CBAF` | `0x2CB94` `svc_options_menu` | between `menu_run` and `config_credits_init()` | none | the GAME OPTIONS case: leaving the menu writes the file |

With no save path (`--check`, `run_tests`, every driver) the load reads the game dir's zero `CMOS` and the
save's open fails. Measured: the 120 `--check` frames (`.ppm`, `.pal`, `.idx`) are byte-identical with and
without the call at `0x2F9CF`.

**`0x2F9CC` against the port's inlined lines** (reported, not changed). The raw's order is `0x1AFE8`; `0x13ADC`;
`0x4F1E4(0)`; `0x2BAF4(1, EBX = ECX = 0)`; `0x4F1D0(0, 0)`; `0x38B18(0x9AD84)`; `[0x10740C] = 0x1D2D0`;
`0x2D6F8`; `[0x107410] = 0x2D974(0x2A)` with `and al,0xfc`; `0x2C9B8(0)`; `0x2C8F0(-1)`; `0x2C8F0(-2)`. The port
has the load, the two stores and `config_validate()`, then `svcmenu_register()` (`PORT:`). It runs `0x13ADC`
earlier, inside `actors_init`, and has no line in this block for `0x4F1E4`, `0x2BAF4`, `0x4F1D0`,
`0x38B18(0x9AD84)`, `0x2C9B8(0)`, `0x2C8F0(-1)` or `0x2C8F0(-2)`.

### §S.7.6 Corrections

- **`0x1B084` returns AL** (`0x1B110 mov al,dl`): `u32 config_file_save(void)`, not the plan's `void`.
- **A wrong-length file is not the `0xFF` path in the raw** (`0x1B025 jne 0x1B04D`, `0x1B054 call 0x62003`;
  `0x1B0EF`, `0x1B0FE`): §S.7.3. Spec §3 and the plan's Review Focus 1 state otherwise.
- **`0x62003` is `exit`**, not an error printer that returns (`0x62721 mov ah,0x4c; int 0x21`).
- `0x1AFE8`'s EAX is the flag in AL only (`0x700` on success with the byte `0x101520` set): a caller would
  have to test AL. None reads it.

### §S.7.7 Mutation proofs of the unit assertions

`port/tests/test_game.c` (line numbers at this commit), each mutation applied alone to `port/src`:

| Mutation | Failing lines (in run order) |
|---|---|
| load: XOR over all `0x7F8` bytes | 8356 `1 != 0`, 8357, 8368 |
| load: the flag is never cleared | 8356, 8357, 8368, 8409, 8410 (the fallback) |
| load: no host read | 8357 "the load brings the saved image back", 8364, 8365, 8369 `85 != 255`, 8374, 8375, 8384, 8385, 8410 |
| load: no fill | 8365, 8369, 8375, 8385 |
| load: returns 0 | 8364 `0 != 1`, 8374, 8384 |
| load: fills `0x7F9` bytes | 8386 `255 != 165` |
| load: compares with `0x1014DA` | 8356, 8357 |
| save: no open test | 8401 `2 != 0`, 8402 |
| save: no `0x2D4EC(1)` | 8350 `1 != 2` |
| save: no checksum store | 8353 `27 != 228`, 8356, 8357 |
| save: no write | 8463 `0 != 2040`, 8352, 8354, 8356, 8357, 8368, 8394, 8700 |
| save: writes `0x7F7` bytes | 8463 `2039 != 2040`, 8352, 8353, 8354, 8356, 8357, 8368, 8394, 8700 |
| save: returns 1 after a good write | 8349, 8366 |
| save: XOR over all `0x7F8` bytes | 8353 `255 != 228`, 8356, 8357, 8368 |
| save: returns 0 on a failed open | 8400 `0 != 1` |
| `game_shutdown`: no save | 8394 `-1 != 2040` |
| `game_quit_prompt`: no save / a save on every yes | 8463 `-1 != 2040` / `2040 != -1` |
| `svc_options_menu`: no save | 8700 `-1 != 2040` |
| `game_init`: no load (`PR_RESTART` driver) | 13050 `0 != 42405` |

### §S.7.8 Named gaps

- The two `exit` blocks (`0x1B04D`, `0x1B0F7`): no row case, no port equivalent.
- The file calls' arguments are not compared by any row.
- Loading or writing a real save has no recording of the original (task 8's capture).
- `svc_options_menu`, `game_shutdown` and `game_init` have no differential row; their sites rest on the unit
  assertions above.

### §S.7.9 Measured on the batch's tree

- `PR_DIFF_JOBS=4 python3 tools/diff_verify.py … --self-check`: `965/965 functions VERIFIED; 14379/14379 mutants
  detected; 1 named gaps; 604/798 rows with callees closed (167 have none)`, `rc 0` (the base line plus 2
  rows and 30 mutants; neither row closes, `0x10D34`/`0x10D0C` having no row). The run preceded a
  comment-only edit of `config.c`; the two rows and `game_quit_prompt` were run again after it.
- `PR_ORACLE_REQUIRED=1 ./build/run_tests` and `PR_RESTART=1 PR_GAME_DIR=data/game/C ./build/run_tests`: `all
  checks passed`.
- Not run: `make verify`, the harness unittest suite, any oracle. The counter pin is the integrator's.

## §S.8 Integration and the audit

**Integration** (`reverse-sint`). The three batches were merged with the additive compose merge (no
conflicting change). The one cross-batch site, `0x2D94B..0x2D962`, is wired: `config_validate` calls
`audit_add(0x24, [0x105DA4] + [0x105DA5])` when the sum is not zero. Both bytes are zeroed at entry
and no other instruction names them, so the row `config_validate_audit` reaches the call through a
`0x2DF8C` stub that stores them (2 cases, mutants `@noaudit`, `@auditsum`, `@auditfield`, each caught
by the call list). Wiring it moved two pins: `config_validate@noda5` and `@noda4` (the mutants that
do not zero the bytes) now also differ in the call list. The 14 storage rows left
`tools/port_classification.txt`; `tools/port_progress.py` prints `785 1203 65` and `745 745 100`.
Counter: `988/988 functions VERIFIED; 14706/14706 mutants detected; 1 named gaps; 622/821 rows with
callees closed (167 have none)` (25 rows and 357 mutants over track D's last line).

**Audit** (one read-only agent, re-deriving from the bytes; its report is in the ledger). It found the
bodies of the 16 functions and the changed call sites equal to the bytes, and backed that with its own
whole-chain run (the original under unicorn against the port with every callee real, the whole data
object compared: 2,720 random cases, no difference, the defaults writer included). Its findings, and
what was done:

| # | Finding | Outcome |
|---|---|---|
| F1 | `0x2EA68` does not return (`0x62003`, `int 0x21 AH=4Ch`); the port's `menu_fatal_error` returns. With the slot byte `0x105E2F` at 13..35, `0x2D4EC(0)` passes the bound `0x2D56A` and its first store (position 44) is outside the image: the raw exits with no store, the port stored the 16 cells that land inside | **Fixed in part.** `config_storage_touch` stops the encode after a store outside the image, so the image is what the raw leaves (unit case `slot 13`, mutation-proved). The port still returns where the raw exits: **named gap**, with `0x2D508`, `0x2D571`, `0x2DECC` |
| F2 | The run clock `0x32970` is not ported, nor any of its 17 call sites, so `0x10746C[1..2]` are only ever zeroed: the time values posted by `0x32A3C`, `0x32B00`, `0x32B4C` are 0 (time totals do not move, the time histograms count bucket 0), fields 3..5 are never posted and the slot byte never steps. The rows pass because their cases pin the clock to the state where it changes nothing | **Named gap**, the track's largest. The comments in `config.c` and `config.h` now say so. Porting `0x32970` needs a tick source for `0x105D88` and is a track of its own |
| F3 | A `cmos` longer than `0x7F8` bytes: the raw's `fread` takes its first `0x7F8` bytes and can load it; the port refused it | **Fixed.** `host_save_read` reads the first `len` bytes of a longer file (unit cases `long` in `test_config_file` and `test_host_save`, mutation-proved) |
| F4 | A shorter `cmos`, or a short write: the raw exits (`0x1B04D`, `0x1B0F7`) and leaves the file; the port takes the missing-file path and the next save replaces the file | **Named deviation**, kept on purpose: a damaged save does not stop the port |
| F5 | The rows of `0x1AFE8` and `0x1B084` run under six code pokes (the libc calls become `mov eax,imm32`) and the byte `0x101520 = 1`: they do not see the file calls' arguments, any count but `0x7F8`, the host's path choice, a failed write after a successful open, or `0x10D34`/`0x10D0C` | **Named**: two rows of the weaker class |
| F6 | No row runs `0x2CADC` with the storage real; `config_validate_rr`/`_audit` and the `0x2E990` stubs prove re-reads and values the image cannot produce | **Named**. The audit's own run covered the defaults writer (312 cases); the gate does not |
| F7 | No recording of the original behind a written or a loaded save | Task 8 (`save-reload`), or the track's main named gap |
| F8 | Host: a path that does not fit was cut silently; the writable probe left an empty file; the runner's scratch path fell back to `/cmos` when `mkdtemp` failed | **Fixed** (a path that does not fit is no path; the probe removes an empty file, mutation-proved; the runner stops). The path fix has no assertion: on this host a path that does not fit the buffer cannot be opened either |
| F9 | The gate's port side reads `data/game/C/CMOS` and nothing checked it is the captures' file | **Fixed.** `make verify` starts with `check_cmos` (`cmos=zero` or `absent`, else it fails) |
| F10, F11 | Statements the bytes contradict (the spec's `0x2EA68` and short read, this record's §S.2 and §S.4) and docs not yet updated | **Fixed** here and in the track's docs commit |

Not settled (the audit's PLAUSIBLE items): whether the original can itself save a slot byte of 13 or
more (the runtime's exit-routine walk `0x6C4CF` was not traced); the two "nothing else stores it"
claims rest on absolute-operand scans (the byte `0x2D490`; `0x105DA4`/`0x105DA5`); which statistics
screens a capture compares under F2 (the gate decides); and that a checksum-valid save is now input to
39 `config_field_get` consumers that were not audited for an index leaving `mem[]`.

Also open, found by batch `sc` and not changed: the port's inlined `0x2F9CC` has no line for
`0x4F1E4(0)`, `0x2BAF4(1)`, `0x4F1D0(0,0)`, `0x38B18(0x9AD84)`, `0x2C9B8(0)`, `0x2C8F0(-1)` and
`0x2C8F0(-2)`, and runs `0x13ADC` earlier, inside `actors_init`.
