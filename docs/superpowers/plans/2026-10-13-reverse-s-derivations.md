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
