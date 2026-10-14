<<<<<<< HEAD
# Track N — the last twelve named gaps — derivation record

Brief: `.superpowers/sdd/2026-10-14-reverse-n/brief.md`. Three batches off `main` `95afed00` (`995/995 functions
VERIFIED; 14825/14825 mutants detected; 1 named gaps; 636/826 rows with callees closed (169 have none)`), each
re-deciding from the bytes whether a ported function with no differential row can honestly take one. Ground
truth: the fixed-up image (`build/diffrun --image-out`, linear addresses) and the Ghidra project `rage`. The
claim of every row is the track's: equivalence on the exercised blocks and inputs only, the callees stubbed or
run as stated.

## §N.1 Batch n1: the BIOS query, the aperture blit, the fatal exit, the boot chain, `game_main`'s init

Branch `reverse-n1`. Result: **one partial row** (`0x51ED8`, 4 cases, 4 mutants), **four stay named gaps**.
No port correction; one comment correction in `sprite.c`/`sprite.h` (§N.1.3).

| entry | port | outcome | what decides it |
|---|---|---|---|
| `0x4FBA2` | `int10h_query` (flow.c, static) | **stays a gap** | `int 0x10` at `0x4FBAD` (measured: `unmodeled int at 0x4FBAD`). The function is `push`×6; `mov eax,0xf00`; `int 0x10`; `and eax,0xff`; `pop`×6; `ret`: everything it returns is the BIOS's AL. A poke `mov al,0x13` (B0 13, the same two bytes) would compare that constant with the port's `return 0x13u` (`PORT:` the host owns the window): a constant of the harness against a constant of the port. d6a's decision (record 2026-10-13-reverse-d6a §4) stands. |
| `0x51ED8` | `sprite_blit_aperture` (sprite.c) | **partial row** (weaker) | The guards and the `+0x14` store compare exactly on the renderer table's `ret` entries (`0x51E58`). Every live renderer the one caller reaches uses the node as its scratch, which the port's by-value renderers do not (`PORT:`); measured with a poked prototype, the pixels agree and the node does not (§N.1.1). |
| `0x2EA68` | `menu_fatal_error` (menu.c) | **stays a gap** | `mov eax,1` (`0x2EA68`, overwriting the message pointer the callers pass in EAX, e.g. `mov eax,0x80b54` `0x2FDCB`) and `jmp 0x62003`, the runtime's exit: `0x62003` calls the handlers `[0xEF934]`, `[0xEF938]` (measured: `unmodeled indirect call at 0x62006`), then `0x6201B` -> `0x626EB` -> `0x62710` -> `0x6C4CF(0, 0xFF)` -> `mov ah,0x4c; int 0x21` (`0x62721`/`0x62723`, DOS terminate with AL = 1). Nothing returns. The port's function returns and does nothing (`PORT:` menu.c). A poke of the `jmp` to `ret` leaves `mov eax,1; ret`, whose EAX no caller reads (each would have exited): the row would compare nothing. Callers model it as track S's stub `SA_FATAL`. |
| `0x1BEC4` | none (`game_main` + `game_init`, flow.c) | **stays a gap** | No single C function corresponds. The raw is the boot chain from the argv probe (`0x623B0` `0x1BED3`) to `0x20C10` (`0x1C0BD`), `0x10D34`, `0x1BE30` and `xor eax,eax; ret`. The port spreads it over `game_main` (which calls `game_init`, `game_loop_begin`, `game_loop`, `game_shutdown`: `game_loop` is `0x20C10`'s last call `0x20DE8`, not `0x1BEC4`'s) and `game_init` (which runs the host loads `mem_load_le`, `res_load_index`, `game_string_table_load` and also `0x20C10`'s head `0x20C15`). Its first callee stops it (measured: `unmodeled call 0x4FBA2 from 0x1BEE3`, `int 0x10` at `0x4FBAD`). |
| `0x20C10` | none (`game_init`, `game_init_resume`, `game_main`) | **stays a gap** | No single C function corresponds. `0x20C15` `0x2F9CC` is in `game_init`; the runtime `setjmp` `0x653FC` (`0x20C1F`) is `game_loop`'s restart point (record named-gaps-b §B.2); `0x20C24..0x20DE3` is `game_init_resume`, less `0x20D0F..0x20DE3` (`PORT:` the controller checks); `0x20DE8` `0x255CC` is called by `game_main`. A row of `game_init_resume` entered at `0x20C24` is not a function entry: the frame `sub esp,0x28` (`0x20C12`) that `0x20CF0..0x20D00` read (`mov eax,esp`; `[esp+0x24]`) would not exist. |

### §N.1.1 `0x51ED8`: the bytes, the row, and why it is partial

`0x51ED8`: `test [eax+0x18],-1; je 0x51F44` (zero width), `test [eax+0x14],-1; je 0x51F44` (zero rows); six
pushes; `mov ebp,eax`; `0x1B544([ebp+0x20])` (`0x51EF5`); EBX = `0x81310[byte [[ebp+0x1C]+8]]`; EDI =
`[0x1088F8 + [ebp+4]*4] + 0xA0000 + [ebp]` (`add edi,0xa0000` `0x51F1B`, `81 C7 00 00 0A 00`); `[ebp+0x14] =
[ebp+0x14] - [ebp+0x34]` (`0x51F23..0x51F2F`); ECX = `[ebp+0x18]`; `call [([ebp+0x10] & 0x7F)*4 + 0x80C8C]`
(`0x51F35`); `mov eax,ebp`; six pops; `ret`. The one caller `0x1C5E8` (Ghidra: one caller) passes its frame
(`mov eax,esp` `0x1C63B`), builds the node with `0x1C528` (types 1, 2 and their mirrored forms 9, 0xA) and
zeroes the four clips (`0x1C620..0x1C62C`); after the call it reads only `[esp+0x1C]` (`0x1C64F`), not EAX:
mask 0.

The prototype (measured, not kept): every case with the imm32 at `0x51F1D` poked to a scratch in the image's
zero BSS (`0xF2000`, `0xFA00` bytes) and the binding copying those bytes into `gfx_aperture()` before the call
and back after it. Types 1, 2 and 9 (`0x5D218`, `0x58CBD`, `0x57F80`) drew the same pixels on both sides and
differed only in the node: `+0x14` original 0, port `rows - clip_b`; `+0x38` original `0x138` (types 1, 2) or
`0x148` (type 9), port unchanged. The renderers' own stores: `mov [ebp+0x38],edx` (`0x5D21F`, `0x58CDD`,
`0x57F8B`: `0x140 -/+ ecx`), `dec dword [ebp+0x14]` per row (`0x5D262`, `0x5D289`, `0x57FCE`, `0x57FF5`;
`0x58CBD`'s measured), and in `0x58CBD` `sub [ebp+0x14],eax` (`0x58CC4`) and `mov [ebp+0x18],ecx` (`0x58CD3`).
The port's renderers take the node's fields by value and store nothing back (`sprite.c` `PORT:`, and
`sprite_blit_at`'s "the node is left intact"): a documented deviation, unobservable by the one caller (a dead
frame local read again only at `+0x1C`). The brief forbids hiding it and forbids "correcting" a documented
deviation, so no live renderer is cased and the poke is not kept (on the `ret` entries the immediate never
reaches a store).

The row (`N1_SPECS`, `tools/diff_verify.py`; binding `b_n1_51ed8`, `port/tests/diff_runner.c`): EAX = the node
at `0x10A900` between seeded dwords, its `+0x24`, `+0x38`, `+0x3C` seeded; the palette entry and the two
sprites of the d5b fixtures (`0x1B544` resolves the pixel handle on both sides); allow `0x1B544`, `0x51E58`.
The binding loads the C node from the raw offsets (`d5b_51e5c_load`), calls `sprite_blit_aperture`, and writes
every field of the C node back to the raw offsets.

| case | node | what it shows |
|---|---|---|
| `a0` | type 1, width 0, clip_b 2 | `je 0x51F44` at `0x51EDF`: no store |
| `a1` | type 2, rows 0, clip_b 1 | `je 0x51F44` at `0x51EE8`: no store |
| `a2` | type 0xA (`ret` entry; 0x1C528 builds it), clip_b 1 | `+0x14` = 3 |
| `a3` | type 5 (`ret` entry, not built by 0x1C5E8), clips 1, 1, 5, 2 | `+0x14` = 2: `+0x34` is subtracted, not `+0x30` |

Blocks 4/4. Not compared (named): the draw, the aperture base (`0x51F1B`), the bank and row-table reads that
feed only EDI/EBX, and the three live renderers of the caller's domain (above); the clipped types (0x10 set),
which `0x1C5E8` never builds and `render_list_blit` runs through `0x51E5C` on the same table.

### §N.1.2 Mutants and mutation proofs

| mutant | the old/wrong code | caught by | cases |
|---|---|---|---|
| `@nostore` | no `rows -= clip_b` | byte | `a2`, `a3` |
| `@clipt` | `rows -= clip_t` | byte | `a2`, `a3` |
| `@nowidth` | no width guard (the store runs) | byte | `a0` |
| `@norows` | no rows guard (the store runs) | byte | `a1` |

The binding against the real port: `sprite.c`'s `n->rows -= n->clip_b` replaced by `n->rows -= n->clip_t`,
rebuilt: `sprite_blit_aperture` MISMATCH (`a2: byte 0x10A914: original 0x03, port unchanged`; `a3: original
0x02, port 0xFF`); restored. Every node field is seeded with a value that differs from what is stored, and
the store's byte `0x10A914` sits between seeded fields.

### §N.1.3 Comment correction (no behaviour change)

`sprite.c`'s `0x51ED8` header said the raw renderer's `+0x30` write-back is not modelled; the raw shows the
renderers the caller reaches store `+0x14`, `+0x38` (and `0x58CBD` `+0x18`), so the stored `rows - clip_b`
survives only the `ret` entries. The `PORT:` note now names those instructions; `sprite.h` says the node is
then the renderers' scratch. Commit `37fa2e1b`.

### §N.1.4 Counters

Full `--self-check`: `996/996 functions VERIFIED; 14829/14829 mutants detected; 1 named gaps; 636/827 rows
with callees closed (169 have none)` (the new row is open on `0x1B544`, a named non-row, and `0x51E58`). The
harness suite's counter pin is updated to it. After n1: 17 ported functions without a row (11 named gaps, 6
named non-rows); 11 weaker rows (`0x51ED8` joins: partial).
=======
# Track N derivations: the last twelve named gaps

Brief `.superpowers/sdd/2026-10-14-reverse-n/brief.md`. Sources: Ghidra (project `rage`, read-only), capstone
over the fixed-up image (`./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_n2_img.bin`; image
offset = address − `0x10000`) and the harness's emulator (`tools/diff_emu.py` `run_original`). One section
per batch; the integrator merges §N.1, §N.2 and §N.3.

## §N.2 Batch n2: the audio initialisation and the teardown

| Function | Port | Outcome | Deciding addresses |
|---|---|---|---|
| `0x1BE30` | `game_shutdown` (`flow.c`) | **poked partial row** (weaker class) | the `int 0x10` at `0x4FB9E` (poked `39 C0`); the three `0x1ADE4` blocks `0x1BE4B`, `0x1BE6B`, `0x1BE93` named unhit; `0x10D34` with `0x101520 = 1` |
| `0x1CF40` | `game_audio_init` (`flow.c`) | **stays a gap** | the handle stores `0x1CF8E`, `0x1CFAC`, `0x1CFD6`, `0x1CFE8` in reached blocks, which the port does not make (or makes as `1`) |
| `0x1C930` | `title_music_start` (`flow.c`, static) | **stays a gap** | the gate `0x1CF30`/`0x1C93B` on `0x1028CC`/`0x1028DC`, the store `0x1C943`, the copy `0x1C961..0x1C977`, the stops `0x1C956`/`0x1C9BD`, the clear `0x1CA08`: none of them in the port |

Callers (scan of the code object for `call`/`jmp rel32`, and of the image for the entry as a dword):

| Entry | `call rel32` | `jmp rel32` | dword |
|---|---|---|---|
| `0x1BE30` | `0x1C0C7` (`0x1BEC4`), `0x1C1C4`, `0x1C2AF` (`0x1C0F0`), `0x1D293` (`0x1D290`) | none | none |
| `0x1CF40` | `0x1BFDB` (`0x1BEC4`, `EAX = EDX = 1`: `0x1BFD1`, `0x1BFD9`) | none | none |
| `0x1C930` | `0x1CF39` (`0x1CF20`, after `cmp dword [0x1028cc],0; je` `0x1CF30`) | none | none |

### §N.2.1 `0x1BE30` — `game_shutdown`

```
01BE30  push ebx / push ecx / push edx
01BE33  call 0x1d018                         ; sound_shutdown (row since track I)
01BE38  mov edx,[0x101514] ; test edx,edx ; je 0x1beab
01BE42  mov eax,edx ; cmp word [edx+0x16],0 ; je 0x1be61
01BE4B  xor ebx,ebx ; xor edx,edx ; mov bx,[eax+0x14] ; mov dx,[eax+0x16] ; mov eax,9 ; call 0x1ade4
01BE61  cmp word [0x10151c],0 ; je 0x1be87
01BE6B  mov eax,0x23 ; bx = [0x10151a] ; dx = [0x10151c] ; call 0x1ade4
01BE87  mov cx,[0x10151e] ; test cx,cx ; je 0x1beab
01BE93  mov eax,0x1b ; bx = [0x101518] ; dx = cx ; call 0x1ade4
01BEAB  mov eax,[0x101504] ; call 0x4fb98 ; call 0x1b084 ; call 0x10d34
01BEBF  pop edx / pop ecx / pop ebx ; ret
```

**A correction of the port's reading (comment only, commit `413c30d2`).** The port's `PORT:` note called
`0x1BE38..0x1BEA6` "memory frees". They are not. `0x1ADE4` builds a register block on the stack with
`[esp] = 0x201` (`mov ecx,0x201` `0x1ADF1`, `mov [esp],ecx` `0x1AE0C`), BL = the vector (`mov [esp+4],al`
`0x1ADE8`), ECX = DX, EDX = BX, and calls the runtime's int386 `0x61564` with `0x31` (`0x1AE07`): DPMI `0x201`,
set real-mode interrupt vector. Its partner `0x1ADAC` is DPMI `0x200` (get, `mov edx,0x200` `0x1ADB5`),
which the init calls at `0x1C017` (vector 9 into `[0x101514]+0x14/+0x16`), `0x1C02B` (`0x23` into
`0x10151A/0x10151C`) and `0x1C03F` (`0x1B` into `0x101518/0x10151E`) before installing its own handlers
(`0x1C05B..0x1C093`). The teardown restores each of the three vectors whose saved segment word is non-zero.
`[0x101514]` is the DOS block (`0x1AD64`, the selector `<< 4`, `0x1BF1F..0x1BF31`). The port has no real
mode: `[0x101514]` is `GAME_BIOS_BASE`, zeroed at init (`flow.c`), written only at `+0x2D4..` and `+0x800..`,
and `0x101518..0x10151F` are written only by `0x1ADAC`, which the port does not make. So in a run of the
port the raw's own tests skip all three restores. `0x1BEC4`'s note at `flow.c` ("the DPMI locks
`0x1ADAC`/`0x1ADE4`") has the same misreading; it is batch `n1`'s function and is left to it (report).

**The outcome: 3 with 2, a partial row under one code poke.**

1. *Outcome 1 is not honest.* The port makes two of the five kinds of call (`0x1D018`, `0x1B084`); the
   vector restores, `0x4FB98` and `0x10D34` are `PORT:` omissions, and `0x4FB98` stops the emulator on
   `int 0x10` (`unmodeled: int at 0x4FB9E`, measured on both cases).
2. *The poke.* `0x4FB98` is `pushad; and eax,0xff; int 0x10; popad; ret`. With `int 0x10` at `0x4FB9E`
   poked to `cmp eax,eax` (`39 C0`, the d5h/d6a mechanism: a case poke) it runs from its bytes (allow
   mode): `popad` restores all eight registers and no flag is read after it (`0x1BEB5` is a `call`), so
   the poke is exact on every path except the BIOS's mode set itself, which the row does not see (nor
   the mode byte `[0x101504]` it would take). The port's equivalent is the host's window (`PORT:`).
3. *`0x10D34`* runs from its bytes with the -f byte `0x101520 = 1` (`cmp byte [0x101520],0; jne`
   `0x10D38`): it returns at once keeping EAX and EDX, the treatment of the rows of `0x1AFE8` and
   `0x1B084` (record S §S.7, `SC_NO_CD`). Without the byte the original stops on the runtime's drive call
   (`unmodeled call 0x61984 from 0x10D48`, measured). Its other path is not seen.
4. *The partial part.* The three restore blocks are separate blocks (`0x1BE4B`, `0x1BE6B`, `0x1BE93`), each
   reached only with a non-zero segment word; there the original reaches `0x1ADE4` (measured: `unmodeled
   call 0x1ADE4 from 0x1BE5C / 0x1BE82 / 0x1BEA6` with the word set, the image has no row for it), and a
   stub would record a call the port does not make. They are named unhit; the omission is the `PORT:`
   note above.

**Seams added:** `PR_SEAM0(0x1D018u)` first in `sound_shutdown` and `PR_SEAM_RET0(0x1B084u)` first in
`config_file_save`. No existing binding has `0x1D018` or `0x1B084` in a call set, so no existing row moves
(measured: every pin unchanged). The track I unit probe `if_sd_probe` (`test_game.c`) stubs every seamed
call, so `sound_shutdown`'s own new seam made it return at once (25 failures); the probe now runs the body
for `0x1D018` (one line, no assertion changed).

### The row

| row | entry | cases | blocks | mutants | EAX mask | callees |
|---|---|---|---|---|---|---|
| `game_shutdown` | `0x1BE30` | 2 | 5/8 | 4 | 0 | `0x1D018`, `0x1B084`: recorded stubs (each VERIFIED by its row); `0x4FB98` (poked) and `0x10D34`: allow, no row |

Clobbers (`E.callee_clobbers` over the image): `0x1D018: ()`, `0x1B084: ("edi", "ebp")`, both new in the
pinned dictionary. EAX: the original's is `0x1B084`'s stub's; none of the four call sites reads it
(`0x1C0CC xor eax,eax`; `0x1C1C9` and `0x1D298` push and call `0x62734`; `0x1C2C2 mov eax,edx`), and the
port's function is `void`: mask 0.

| case | regs | `[0x101514]` | words `0x101518..0x10151F` | at `ptr+0x12..+0x1B` | stub EAX (`0x1D018`, `0x1B084`) | what it shows |
|---|---|---|---|---|---|---|
| `t0` | dirty | 0 | `1A1A 5A5A 1E1E 1C1C` (both segment words set) | — | `0x11111111`, `0x12345678` | the `je` `0x1BE40` skips the tests even with the words set |
| `t1` | — | `0x10A900` | `1A1A 5A5A 0000 0000` | `12 13 14 15 00 00 18 19 1A 1B` | `0xFFFFFFFF`, 1 | every test made, none restores |

Both cases carry the poke, `0x101520 = 1` and `0x101504 = 3`. Blocks hit: `0x1BE30`, `0x1BEAB` (t0, t1),
`0x1BE42`, `0x1BE61`, `0x1BE87` (t1). The original writes no byte outside its stack.

| mutant | the wrong port | caught by |
|---|---|---|
| `@noshut` | `0x1D018` dropped | call #0, call #1 (`t0`, `t1`) |
| `@nosave` | `0x1B084` dropped | call #1 (`t0`, `t1`) |
| `@order` | `0x1B084` before `0x1D018` | call #0, call #1 (`t0`, `t1`) |
| `@free` | the old note's reading as code: the DOS block pointer `0x101514` dropped after `0x1D018` | byte, call #1 memory (`t1`) |

**What the row does not compare:** the BIOS mode set and its byte `[0x101504]` (the poke); `0x10D34`'s
drive path (the -f byte); the three restores and their arguments (named unhit); the stubs' insides (their
own rows). It is open on `0x4FB98` and `0x10D34`, which have no row. It joins the weaker rows (poked and
partial).

The counter, measured: `996/996 functions VERIFIED; 14829/14829 mutants detected; 1 named gaps; 636/827
rows with callees closed (169 have none)` (before: `995/995`, `14825/14825`, `636/826`). No existing row,
mask, case or pin moved. `tools/port_progress.py` unchanged (`787 1203 65`, `747 747 100`); the E2 table
equals a fresh run.

### §N.2.2 `0x1CF40` — `game_audio_init`: stays a gap

```
01CF40  push ebx/ecx/esi ; sub esp,8 ; [esp] = AL ; [esp+4] = DL
01CF4D  call 0x5d851                          ; AIL_startup
01CF52  mov ah,1 ; mov [0xa2cb1],ah ; test dl,dl ; je 0x1cfbf
01CF62  0x5d87e(4,4) ; 0x5d87e(1,0x2b11) ; 0x5d87e(3,0x14) ; call 0x5db7c   ; DIG install
01CF8E  mov [0x1028c8],eax ; test eax,eax ; je 0x1cfbf
01CF97  loop esi = 0..0x48 step 0x18: 0x5dbcb([0x1028c8]) ; [esi+0x102860] = eax (0x1CFAC) ; 0x5dc0f(eax)
01CFBF  cmp byte [esp],0 ; je 0x1cfed
01CFC5  0x5d87e(0xb,1) ; call 0x5ddd0 ; mov [0x1028c4],eax (0x1CFD6) ; test ; je 0x1cfed
01CFDF  0x5de1f(eax) ; mov [0x1028c0],eax (0x1CFE8)
01CFED  0x5da12(0x1bdf4) -> esi ; 0x5da87(esi,0x3c) ; 0x5daa6(esi)
01D010  add esp,8 ; pop esi/ecx/ebx ; ret
```

1. *Outcome 1:* the handles the raw stores in `mem[]` are host objects in the port. `0x1CF8E` stores the
   DIG driver; the port stores `1` (a non-zero stand-in, `PORT:`). `0x1CFAC` stores each slot's sample
   handle, `0x1CFD6` the MDI driver, `0x1CFE8` the sequence: the port stores none of them (they live in
   `s_samples`/`s_sequence`, the sound-module comment). The timer callback pushed at `0x1CFED` is the ISR
   `0x1BDF4`; the port registers `NULL`. The port also makes stores and calls the raw has not: `mixer_reset`,
   `DSD(0xA2CB4) = 0x7F`, the FAT.OPL load.
2. *Outcome 2 (poke):* the obstacle is no `int`/`in`/`out`, runtime call or aperture; no poke removes it.
3. *Outcome 3 (partial):* the differing stores sit in blocks the port does run (`0x1CF62`, `0x1CF97`,
   `0x1CFC5`, `0x1CFDF`), so they cannot be named unhit; a row would pass only by masking them, or by a
   DIG stub that answers `1` (a fitted value: any other non-zero handle mismatches). The AL/DL gates
   (`0x1CFBF`, `0x1CF5E`) could be named unhit (the one caller passes 1, 1), but that does not help.
4. **It stays a named gap:** stopped by `mov [0x1028c8],eax` at `0x1CF8E` (the first reached store of a
   host handle) and the slot stores `0x1CFAC`. One more raw difference, not corrected (host audio,
   brief): the raw calls `0x5DC0F` on every allocated slot (`0x1CFB2`) and the port only on a non-NULL
   handle. A `PORT:` note now names the stores, `0x1CFB2`, `0x1CFED` and the three additions (commit
   `02dde41b`, comments only).

### §N.2.3 `0x1C930` — `title_music_start`: stays a gap

```
01C930  push ebx/ecx/edx/esi/edi/ebp
01C936  mov eax,[0x1028cc] ; cmp eax,[0x1028dc] ; je 0x1c9b6
01C943  mov [0x1028dc],eax ; call 0x1b544      ; resolve the song handle
01C94D  0x5deaf([0x1028c0])                   ; stop
01C961  copy [esi-4] bytes from the resolved pointer + 4 to [0x1028d0] (rep movsd/movsb, 0x1C970/0x1C977)
01C97A  0x5de48([0x1028c0], [0x1028d0], 0) ; 0x5deca([0x1028c0], [0xa2cb8], 0x1f4) ; jmp 0x1c9fe
01C9B6  0x5deaf([0x1028c0]) ; 0x5de48(...) ; 0x5deca(...)   ; the same song: no store, no copy
01C9FE  0x5de79([0x1028c0]) ; xor eax,eax ; mov [0x1028cc],eax (0x1CA08)
01CA0D  pops ; ret
```

1. *Outcome 1:* the port's function is a host replacement, not a reading of these bytes. It reads neither
   `0x1028CC` nor `0x1028DC` (its request is the C flag `s_music_request`), stores neither, does not call
   `0x1B544`, makes no stop and no copy (its bank is S16TITLE's FORM XMID, read in place through the
   resource layer), does not clear `0x1028CC`, and returns early with no bank or no sequence (host objects).
   Its handle argument is `s_sequence`, never `[0x1028C0]`.
2. *Outcome 2:* no `int`/`in`/`out` or aperture; the difference is the function itself.
3. *Outcome 3:* every block the raw has (both arms and the tail) contains a store or a call the port does
   not make; there is no reached subset that corresponds to the port.
4. **It stays a named gap:** stopped at `0x1C936`/`0x1C93B` (the pending/last-song gate the port does not
   have), with the store `0x1C943`, the copy `0x1C961..0x1C977`, the stops `0x1C956`/`0x1C9BD` and the clear
   `0x1CA08`. In a run of the port the original's `0x1C930` is not reached either: the port never sets
   `0x1028CC` (`0x1CA14` stores it only with `[0x1028C0]` non-zero, which the port keeps 0), so `0x1CF20`'s
   row names that arm unhit (`0x1CF39`). The header's `PORT:` note now names these addresses (commit
   `02dde41b`).

### Mutation proofs

The row's four mutants are its proofs (all detected, pinned in `N2_KINDS`/`N2_CASES`). The poke and the
-f byte were each removed in a scratch run on both cases: the original then stops (`unmodeled: int at
0x4FB9E`; `unmodeled call 0x61984 from 0x10D48`), so neither is hiding a difference the row could see. A
set segment word in `t1` (each of the three) stops the original at `call 0x1ADE4` (`0x1BE5C`, `0x1BE82`,
`0x1BEA6`), the blocks named unhit. No unit assertion was added.
>>>>>>> reverse-n2
