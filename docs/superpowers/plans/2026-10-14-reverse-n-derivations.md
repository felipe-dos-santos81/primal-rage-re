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

## §N.2 Batch n2: the audio initialisation and the teardown

| Function | Port | Outcome | Deciding addresses |
|---|---|---|---|
| `0x1BE30` | `game_shutdown` (`flow.c`) | **poked partial row** (weaker class) | the `int 0x10` at `0x4FB9E` (poked `39 C0`); the three `0x1ADE4` blocks `0x1BE4B`, `0x1BE6B`, `0x1BE93` named unhit; `0x10D34` with `0x101520 = 1` |
| `0x1CF40` | `game_audio_init` (`flow.c`) | **stays a gap** | the handle stores `0x1CF8E`, `0x1CFAC`, `0x1CFD6`, `0x1CFE8` in reached blocks, which the port does not make (or makes as `1`) |
| `0x1C930` | `title_music_start` (`flow.c`, static) | **stays a gap** | the gate `0x1CF30`/`0x1C93B` on `0x1028CC`/`0x1028DC`, the store `0x1C943`, the copy `0x1C961..0x1C977`, the stops `0x1C956`/`0x1C9BD`, the clear `0x1CA08`: none of them in the port |

Callers (scan of the code object for `call`/`jmp rel32`, and of the image for the entry as a dword):

| Entry | `call rel32` | `jmp rel32` | dword |
|---|---|---|---|
| `0x1BE30` | `0x1C0C7` (`0x1BEC4`), `0x1C1C4`, `0x1C2AF` (`0x1C260`, which nothing calls; corrected at integration, §N.4), `0x1D293` (`0x1D290`) | none | none |
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

## §N.3 Batch `n3`: the resource loaders, the string table, the boot logos' player

Branch `reverse-n3` off `main` `95afed00`. Ground truth: the fixed-up image `/tmp/pr_n3_img.bin` (`build/diffrun
--image-out`), disassembled with capstone 5.0.7, and the Ghidra project `rage` (read-only). Every figure is
measured with `tools/diff_verify.py --self-check`; the pins (`N3_MASKS`/`N3_KINDS`/`N3_CASES`) come from
`verify_all` on `N3_SPECS`, run five times with identical results.

| function | port | outcome | what decides it |
|---|---|---|---|
| `0x1B120` | `res_load_index` (res.c) | **poked row, partial** (21/35 blocks) — **withdrawn at integration: stays a gap (§N.4)** | The six file calls are pokeable (`mov eax,<result>`, track S's form); every block between them corresponds to the port's C once the port allocates through `0x1C308`'s seam and `0x1B3AC`'s head has one. The 14 unhit blocks are the fatal `0x1D290` and the allocator's retries. Two corrections (§N.3.3). |
| `0x1B3AC` | none whole: the head is `res_load_present` (res.c) | **stays a gap** | No port function is `0x1B3AC`. Its head `0x1B3B8..0x1B3F8` is `res_load_present`; its read `0x1B403..0x1B45A` (`0x61C60`, `0x61CF6`, `0x61EC0`, `int 0x21` at `0x6F53A` under them) is the host's, done by `res_load_index` for every entry at init (PORT); its mark `0x1B47A` is made by `res_load_index` after the read and by `res_resolve` before the head; `0x1E75C`/`0x1E808` (`0x1B423`, `0x1B475`) and the stores to `[[0x101514]+0x253]` (`0x1B3D3`, `0x1B42E`, `0x1B451`) have no port counterpart; and the port's head advances the ISR's counters (`game_isr_ticks`, the read-stall model, PORT) where the raw writes none. A row would compare a composition the binding assembles, not the port. |
| `0x47370` | `game_string_table_load` (flow.c) | **stays a gap** | The port's loader is a replacement that makes none of the raw's calls: `0x1E6D8` (the free of the old table, `0x47389`), `0x1C308(0x42, size)` (`0x47429`, retried with 2 at `0x47442`), `0x1E75C(handle, 1)` (`0x4746B`), `0x1E808` (`0x474A7`), and on failure `0x1D290` (`0x474D3`, `0x47496`) or the runtime's `exit` (`0x474BA`, a bad magic: `cmp dword [0x1082d8],0x33340001` `0x473E0`). Its two stores take values of its own making: `0x1082DC` is a fixed handle `0x3800000` (STRING_HANDLE), not the allocator's, and `0x1082D8` the bytes read capped at `0x2000`, not ftell's size. It ignores the language index (`[edi+0xC9418]`, `0x473A0`) and returns at once on a second call (a C static), where the raw frees and reloads (`0x20C7F`, `0x336AE`). The file calls could be poked as `0x1B120`'s are; the rest could not. |
| `0x1C740` | `movie_play` (movie.c) | **poked row, partial** (4/23 blocks) | The entry blank, the key test, the quit byte, the open and the epilogue are the port's prefix. Two pokes: the runtime's kbhit (`0x1C752`) and the Smacker open (`0x1C76C..0x1C778`). The play loop (19 blocks) is the Smacker library and the hardware, which the port replaces with its own decoder (movie.c, smacker.c, PORT). |

### §N.3.1 `0x1B120`: the bytes

No argument. `push` of six registers, `pop`s and `ret` at `0x1B3AA`. One caller, `0x1C09D` in `0x1BEC4`, whose
next call `0x51F45` begins `pushal; mov eax,[0x1014e4]`: **mask 0**.

| block | instructions | what it does |
|---|---|---|
| `0x1B120` | `mov edx,0x805a0; mov eax,0x805a4; call 0x61c60`; `mov ecx,eax; mov ebp,eax; test eax,eax; jz 0x1B1EB` | `fopen("index", "rb")` |
| `0x1B141` | `call 0x6225a` (EBX = 2), `call 0x62356`, `call 0x6225a` (EBX = 0); `mov edi,eax; mov esi,eax`; `test edi,edi; jle 0x1B1DF` | fseek end, ftell, fseek 0: the size in EDI and ESI |
| `0x1B16B`, `0x1B17A` | `div ecx` (0x14); `jz 0x1B189`; else `0x1D290(2, 0x805A4)` | a size that is not a multiple of 0x14 is fatal |
| `0x1B189`, `0x1B1AE`, `0x1B1BF` | `[0x1014F0] = size / 0x14` (`0x1B194`); `0x1C308(0x41, size)` (`0x1B1A0`) to `[0x1014E0]` (`0x1B1A5`), retried with EAX = 1; `fread([0x1014E0], 1, size, file)` (`0x1B1CD`), `fclose` (`0x1B1D6`, EDX kept); `cmp edx,esi; jz 0x1B1FA` | the count first, then the table |
| `0x1B1DF`, `0x1B1EB`, `0x1B1F5` | `0x1D290(2, ..)` / `0x1D290(1, ..)` | a size not above 0, a short read, no file: fatal |
| `0x1B1FA`, `0x1B210` | ECX = `[0x1014E0]`, ESI = 0; `test [ecx+0xc],0x1000000; jz 0x1B259` | the walk; a preloaded entry? |
| `0x1B219`..`0x1B257` | `0x1C308(0x41, [ecx+0xc] & 0xFFFFFF)` to `[ecx+0x10]`, retried with 1 (`0x1B233`); `0x1B3AC(ecx, EDX = 1, EBX = 0)` (`0x1B252`) | preloaded: allocate and read now |
| `0x1B259`..`0x1B289` | `0x1C308(0xE, size)` to `[ecx+0x10]`; `mov eax,[ecx+0xc]; and eax,0xffffff; mov edx,[0x1014f8]; cmp eax,edx; jle 0x1B28E`; `[0x1014F8] = size` | lazy: allocate; raise the stored maximum, signed |
| `0x1B28E` | `mov ebx,[0x1014f0]; inc esi; add ecx,0x14; cmp esi,ebx; jl 0x1B210` | the count read again each entry |
| `0x1B2A0`..`0x1B3A4` | `0x1C308(0x41, 0xFA00)` to `0x1014E8`, `(0xFA00)` to `0x1014E4`, `(0x4880)` to `0x1014EC`, `(0xEBA0)` to `0x1014F4`, each retried with 1 and then `0x1D290(4, 0)` | the four buffers |

The fopen leads to `int 0x21` at `0x6F53A` (measured: with each runtime callee allowed in turn the original
stops there, through `0x61C60`, `0x61C45`, `0x6BDBC`, `0x61423`, `0x6B36F`, `0x6B4B3`, `0x6B19F`, `0x6F506`).
The calls are the runtime's, the obstacle is not the function: the same pokes as track S's `0x1AFE8` row work.

### §N.3.2 The rows

| row | entry | cases | blocks | mutants | EAX mask | weaker |
|---|---|---|---|---|---|---|
| `res_load_index` | `0x1B120` | 5 (`x0`..`x4`) | 21/35 (14 named) | 11 | 0 | poked, partial |
| `movie_play` | `0x1C740` | 6 (`k0`, `k1`, `q0`, `q1`, `o0`, `o1`) | 4/23 (19 named) | 7 | 0 | poked, partial |

**`res_load_index` under six pokes.** The file calls at `0x1B130` (fopen, the case's handle), `0x1B148` and
`0x1B15E` (fseek, 0 or -1: their EAX is not read, `0x1B14D` and `0x1B170` load EAX), `0x1B14F` (ftell) and
`0x1B1CD` (fread), both the table's size, and `0x1B1D6` (fclose, its EAX not read) are `B8 imm32`. Exact for
the paths taken: the code after each call reads only EAX of it (ECX, EBP, ESI, EDI are reused and so kept by
the runtime; fclose keeps EDX, `0x1B1DB`). `0x1C308` is a stub answering `N3_TABLE` (`0x10AC00`, zero BSS of
the image) where the case seeds the INDEX bytes; `0x1B3AC` is a stub (clobbers `ebx edx ebp`, derived) recorded
as (EAX = the entry, EBX). The binding (`n3_*` in `diff_runner.c`) writes the poked ftell's count of bytes from
`N3_TABLE` to a scratch `INDEX` and passes a game directory that does not exist. The row sees: the allocation
sizes and their order (seven fixed calls plus one per entry), each `+0x10` store, the count, the table pointer,
the four buffers, the maximum, which entries get `0x1B3AC` and with which EBX, and the memory at every call.
**It is blind to:** the INDEX read (the raw's fread is poked and writes nothing; the port reads the bytes already
there), the file calls' arguments (`0x805A4`, `0x805A0`, the handle, the buffer), which block lands in which
field (one stub EAX per case: all are `N3_TABLE`), the allocation type in EAX (`0x41`, `0xE`, the retry's 1:
the port's allocator has none), `0x1B3AC`'s EDX (the fatal flag, 1), and everything `0x1B3AC` does: the payload
read and the mark are not in the row. The port reads every payload in `res_load_index` and marks the preloaded
entries it read (res.c, PORT); the binding's missing directory makes that read nothing, so the difference
between the port's in-line read and the raw's stubbed one does not appear. A case with the payload files
present would show the port's mark where the stubbed raw has none.

Cases. `x0`: a preloaded entry is the largest (`0x20000`); the maximum is the lazy `0x3000`. `x1`: the stored
maximum `0x800000` is above every size and stays; flag bytes `0x02` and `0x20` on lazy entries; dirty
registers; handle `0x100`. `x2`: the stored maximum `0x80000000` is negative and the first lazy size replaces
it (signed); flag byte `0x03`. `x3`: one preloaded entry; the maximum `0x777` stays. `x4`: six entries, a
12-character name with no NUL, the size `0xFFFFFF` on a lazy entry with flag byte `0xFE`, a preloaded entry
with `0x81`, the fseeks and fclose answer -1. The seven dwords `0x1014E0..0x1014F8` and the byte `0x1014FC`
are seeded.

Named unhit (14): `0x1B17A`, `0x1B1DF`, `0x1B1EB`, `0x1B1F5` and the buffers' `0x1B2D5`, `0x1B316`,
`0x1B357`, `0x1B398` reach `0x1D290`, the fatal error, which the port does not reach (`res_load_index` returns
0 and `game_init`'s `game_fatal` exits, flow.c PORT); `0x1B1AE`, `0x1B233`, `0x1B2B8`, `0x1B2F9`, `0x1B33A`,
`0x1B37B` are the retries after a `0x1C308` that answers 0, which one stub EAX per case cannot give after a
non-zero one (the port returns 0 there). The port's own guard of more than 256 entries (`RES_MAX`) has no raw
counterpart and is not reached (the cases have up to six).

**`movie_play` under two pokes.** EAX = the name (`0x80038` or `0x80044`, the two call sites `0x11050`,
`0x1105A`). The kbhit call at `0x1C752` is `mov eax,<result>`: the runtime's `0x62756` answers 1 when
`[0xEF910]` is set and else the BIOS's AL `0` or `0xFF` widened (`int 0x21` AH = 0Bh at `0x62767`, `cbw;
cwde`); the binding gives the port's key queue one key when the result is not 0. The open
`0x1C76C..0x1C778` (`push esi; push 0xfe00; push -1; call 0x6345C`, which pops its 12 bytes: `ret 0xc` at
`0x635EC`) is `mov byte [0x10AF00],1; xor eax,eax` and four nops: reached, it fails. The binding's game
directory holds both names as two-byte files that are not movies: the port's `res_load_file` reads one into
the bump heap and `smk_open` rejects it, and when the heap moved the binding clears what was read (mem[] above
the image is zero) and stores 1 at `0x10AF00`. `0x52106` is the existing stub (`D4D_SCREEN`). The row sees the
entry blank and its argument, the key and quit gates (the quit byte as a byte: `o0` sets the byte above), that
the open is reached exactly when neither gate leaves, and that none of these exits makes the exit blank. **It is
blind to** the open's arguments, any movie that opens (a non-zero open enters the loop, whose first call `0x649B0`
is the library's: not exercisable), and the 19 blocks of the play loop from `0x1C785` (named: the Smacker library
`0x649B0`, `0x65340`, `0x64130`, `0x64ED8`, `0x643CC`, `0x65240`, `0x63CE8`; the retrace `in al,dx` at `0x1C7CA`;
the host-owned blit `0x50D23`; the key and pad tests in the loop; the exit blank `0x1C873`). Mask 0: the first
call site loads EAX (`0x11055`) and the second's EAX is `attract_step`'s return, whose row's mask is 0.

Not compared, a port-only guard: `movie_play` returns 0 before the entry blank for a NULL directory or name or
an empty name; the raw has no such test and its two callers pass the fixed names.

### §N.3.3 The corrections

| correction | raw | caught by (old code = mutant) |
|---|---|---|
| `res_load_index` stores the count `0x1014F0` before the table's `0x1C308` and the table pointer `0x1014E0` right after it | `0x1B194` before `0x1B1A0`; `0x1B1A5` | `@countlate`: `call #0 memory`, all five cases |
| `res_load_index` raises `0x1014F8` only for an entry without `0x1000000`, by the size read again after the allocation, when greater **signed** than the stored dword (it was the largest size of every entry, from 0, stored after the walk) | `0x1B210 jz`, `0x1B26F..0x1B289` (`cmp eax,edx; jle`) | `@maxall`: `byte` and `call #2..#8 memory`, all five cases |

Before the corrections the row was MISMATCH on those differences alone (measured: every case's `call #0 memory`
at `0x1014F0`, and `0x1014F8` in each case). The shipped INDEX gives the same maximum both ways (its largest size
`0x3A2AF4` is a lazy entry's; the preloaded `s16fonts` `0xAC18` and `s16statu` `0x61C94` are smaller), and the
only readers of `0x1014F8` are `0x1C264`, `0x1C28C`, `0x1C2B4`, in `0x1C260`, a memory check (it asks the
allocator `0x1E458` for the maximum + `0x1003` bytes and else prints the shortfall and exits) that nothing calls:
no rel32 `call`/`jmp` to it and no dword holding it in the image. So no oracle can move. (The commit message of
`f7a40032` says these readers are `0x1C0F0`'s; they are `0x1C260`'s, which follows `0x1C0F0`.) `test_res` (test_platform.c) computes its expected maximum by the raw's rule now;
its assertion is unchanged.

The port change that makes the row possible (no behaviour change outside `build/diffrun`): `res_load_index`'s
seven allocations go through `res_block_alloc` (the seamed form of the same bump allocator; the raw's `call
0x1C308` at each), and `res_load_present` takes the entry (the raw's EAX, `0x1B3B2 mov esi,eax`), is exported,
and opens with `PR_SEAM(0x1B3AC, entry, draw)`.

### §N.3.4 Mutants

`res_load_index`: `@countlate` (the old store order), `@maxall` (the old maximum), `@maxuns` (unsigned compare:
`x2`), `@nopreload`, `@preloadall`, `@draw` (EBX = 1), `@noslot`, `@nomask` (the flag byte in the size), `@count`
(size / 0x10: `x4`), `@bufsize` (0x4880 and 0xEBA0 swapped), `@nobuf`. `movie_play`: `@noreset`, `@reset1`,
`@nokey` (`k0`), `@noquit` (`q0`, `q1`), `@quitw` (`o0`), `@exitblank`, `@noopen` (`o0`, `o1`). 18 of 18
detected; the per-mutant kinds and cases are pinned (`N3_KINDS`, `N3_CASES`).

### §N.3.5 The gaps, and what the bytes show beside them

- `0x1B3AC` (§ table): besides the split above, the raw re-syncs the frame counter (`0x1B45F/0x1B464`) after
  every successful read, the init's (BL = 0) included; the port's head does so only when drawing. At init the
  port's pair has not moved (only the drawing read stall, the master loop's spin and config.c's key wait advance
  it) and `0x255D4/0x255DA` zero both before the loop, so the difference is not observable. The three stores to
  `[[0x101514]+0x253]` have no reader in the code object (a capstone scan of `0x10000..0x73B14` finds only these
  three accesses at `+0x253`): the block is the real-mode driver's (`0x1BF51` loads `RM_DRV` with `0x1B3AC`), which
  the port does not have. Neither is changed.
- `0x47370` (§ table): no partial row either: every path makes a call the port does not make.
- The boot chain's own `0x1B3AC` call (`0x1BF51`, `RM_DRV`) belongs to `0x1BEC4` (batch `n1`).

### §N.3.6 Counter and tests

`--self-check`: **`997/997 functions VERIFIED; 14843/14843 mutants detected; 1 named gaps; 638/828 rows with
callees closed (169 have none)`** (from 995/995; 14825; 636/826). The two new rows have callees; the movie row is
closed (`0x52106` VERIFIED) and closes `attract_step`'s row (`0x11000` stubs `0x1C740`); the loader's row is open
on `0x1B3AC` and `0x1C308`, which have no row. Harness suite: 180 tests OK. `run_tests` (PR_ORACLE_REQUIRED=1):
all checks passed. `tools/port_progress.py`: `787 1203 65`, `747 747 100` (unchanged). Unrowed ported functions:
18 → 16; the named gaps of this batch: 4 → 2 (`0x1B3AC`, `0x47370`). Weaker rows: 10 → 12.

## §N.4 Integration and audit (2026-10-14)

The three batches merged onto `reverse-n` (the harness files without conflict; this record rebuilt from
the three sections). A read-only audit (`.superpowers/sdd/2026-10-14-reverse-n/audit-report.md`) found one
Important and ten Minor findings, none Critical; it confirmed both `res_load_index` corrections, all eight
gaps, and the rows of `0x51ED8`, `0x1BE30` and `0x1C740` within their stated claims.

**`0x1B120`'s row is withdrawn; `0x1B120` stays a named gap.** The row passed only because its binding gave
the port a game directory that does not exist. With the payloads present, the port reads each one into the
block the stubbed `0x1C308` answered (the INDEX table itself, `N3_TABLE`) and marks the preloaded entries
`0x20000000` before the presentation (res.c), which `0x1B120` never does: the read and the mark `0x1B47A`
belong to `0x1B3AC`. `res_load_index` therefore also does part of `0x1B3AC`'s work, the reason
`0x1BEC4` and `0x20C10` stay gaps (no single C function corresponds). The `PR_SEAM(0x1B3AC)` that batch
n3 put on `res_load_present` came off with the row: that function is only `0x1B3AC`'s head
`0x1B3B8..0x1B3F8`, so a stub of `0x1B3AC` there would not span the callee.

**The two corrections stay**, each pinned by a unit test (`check_res_index_rules`, `test_platform.c`, called
from `test_res`) on a two-entry INDEX in a scratch directory holding both payloads (a preloaded `0x300`, a
lazy `0x20`):

| rule | raw | assertion | fails under |
|---|---|---|---|
| the count is stored before the table's allocation, the table pointer after it and before its test | `0x1B194` `mov [0x1014f0],eax`; `0x1B1A0` `call 0x1C308`; `0x1B1A5` `mov [0x1014e0],eax`; `0x1B1AA` `test eax,eax` | the table's allocation failed (`res_fail_alloc_nth(1)`), sentinels `0xA5A5A5A5`: count 2, table 0 | the old order (both lines) |
| `0x1014F8` is raised only by an entry that is not preloaded, compared signed | `0x1B26F..0x1B289` (`cmp eax,edx; jle`) | seeded `0x80000000`: ends at `0x20` | the old all-entries maximum (`0x300`); an unsigned compare (`0x80000000`) |

**Comment corrections** (no behaviour change): `game_init`'s note that "0x1B3AC resource-file setup is
replaced by res_load_index()" was false: `0x1BF51` calls `0x1B3AC(0xA2C9C)`, the entry `RM.DRV` (`+0xC =
0x05000C00`, preloaded, `0xC00` bytes) with `+0x10 = [0x101514]` (`0x1BF47`), the real-mode driver, which
the port does not load. The DPMI note now says `0x1ADE4` also installs RM.DRV's handlers for vectors 9,
`0x23` and `0x1B` (`0x1C05B`, `0x1C077`, `0x1C093`). `menu_fatal_error`'s note now says `0x2EA68`'s `mov eax,1`
discards the message. `res_load_index` gained `PORT:` notes for its allocation failure (the raw retries with
type 1, `0x1B1AE`, then calls the fatal `0x1D290`; the port returns 0) and for its `RES_MAX` bound (no raw
counterpart). The caller table of §N.2 placed `0x1C2AF` in `0x1C0F0`; it is in `0x1C260`, which nothing calls.

**Known limit (F11):** `attract_step` (`0x11000`) counts as closed because `movie_play`'s row is VERIFIED, and
that row covers 4 of 23 blocks. The closed count does not distinguish a weaker callee row.

**After track N:** `998/998 functions VERIFIED; 14840/14840 mutants detected; 1 named gaps; 638/829 rows with
callees closed (169 have none)`. 15 ported functions have no row: 9 named gaps (`0x4FBA2`, `0x2EA68`,
`0x1BEC4`, `0x20C10`, `0x1CF40`, `0x1C930`, `0x1B120`, `0x1B3AC`, `0x47370`) and the 6 named non-rows. 13 rows
are weaker (the 10 before, plus `0x51ED8` partial, `0x1BE30` poked and partial, `0x1C740` poked and
partial); 719 full rows. `tools/port_progress.py` is unchanged (`787 1203 65`, `747 747 100`).
