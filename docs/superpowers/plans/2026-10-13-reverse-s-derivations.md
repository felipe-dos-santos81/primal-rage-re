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
