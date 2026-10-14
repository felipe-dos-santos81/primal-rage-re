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
