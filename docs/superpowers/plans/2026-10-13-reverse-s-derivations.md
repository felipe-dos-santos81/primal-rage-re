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
