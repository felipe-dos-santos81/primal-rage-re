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
  `jmp 0x2EA68` with `EAX = 0x80AA8` (the named no-op printer).

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

Every containing function is ported except `0x32970`. The two sites the spec had not checked
(`0x1E9BB`, `0x1E9EB`) are in the ported `hiscore_audit_reset_due`.

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
