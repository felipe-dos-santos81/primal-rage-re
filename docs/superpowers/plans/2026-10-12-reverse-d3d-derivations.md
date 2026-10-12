# Track D, wave 3, batch d3d — derivation record

Differential rows for the queue `.superpowers/sdd/2026-10-11-reverse-d1/d3d-queue.txt` (21 functions of
`game/actors.c`, `camera.c`, `config.c`, `movie.c`, `nameentry.c`, `svcmenu.c`, `main.c`, `platform/gfx.c`
and `sprite.c`). Branch `reverse-d3d` off `reverse-d2int` (`7a9dfe7`). Ground truth: the fixed-up image
`build/diffrun --image-out` dumps (linear addresses), disassembled with capstone 5.0.7. Every figure in §2
is measured with `python3 tools/diff_verify.py --image … --function <name> --self-check`; the pins in
`tools/tests/test_diff_verify.py` (`D3D_MASKS`/`D3D_KINDS`/`D3D_CASES`) come from `verify_all` on the row's
own spec. The claim of each row is the track's: equivalence on the exercised blocks and inputs only, with
the callees stubbed or run as stated.

## §1 The members

| addr | port function | size | callers and what they read of EAX |
|---|---|---|---|
| `0x12D48` | `camera_dispatch` (camera.c) | 93 | `0x25422` returns to `xor ebx,ebx` (`0x25427`) and then either `mov eax,ebx` (`0x25436`) or, both `0x1077A8` pointers null, `call 0x33f08` (`0x25457`), whose first EAX access is the load `mov eax,[ebx+0x1077a8]` at `0x33F15`; `0x2554B` returns to `xor eax,eax` (`0x25550`). No pointer to `0x12D48` is stored in the image. Mask 0. |
| `0x1461C` | `fighter_1461c` (fighter.c; the queue lists it under actors.c, which registers it) | 210 | No direct caller; the one stored pointer is the dword at `0x148A8` (inside `0x14814`, which stores it at the slot's `+0x0C`). The slot `+0x0C` callback is called at `0x35431` (`mov eax,ecx; call [ecx+0xc]`), which returns to `xor eax,eax` (`0x35434`). Mask 0. |
| `0x14988` | `fighter_14988` (fighter.c) | 210 | The twin: the one stored pointer is the dword at `0x14C22` (inside `0x14B90`); same call site `0x35431`. Mask 0. |
| `0x1D540` | `fight_hud_meter_step` (fight.c; registered in actors.c) | 521 | No direct caller; the one stored pointer is the render table's entry `0xA86C8` (`0xA86C4[1]`), called at `0x25623` (`mov eax,ebx; call [eax+0xa86c4]`). The call returns to `shr edx,1` (`0x25629`; EDX and EBX are live across it and `0x1D540` saves both), then either `mov eax,ebx` (`0x25621`) or `inc word [0x104af4]; call 0x134c0`, whose first EAX access is the load `mov al,[edi+0xd]` (`0x13515`) or none before its `ret` (`0x13AD2`), then `mov eax,[0x10150c]` (`0x2563E`). Mask 0. |
| `0x204F4` | `nameentry_arm` (nameentry.c) | 537 | One caller, `0x1ECA7` (`mov edx,ebx; call 0x204f4`), which returns to `mov eax,0x26` (`0x1ECAC`). No stored pointer. Mask 0. |
| `0x216EC` | `fighter_216ec` (fighter.c; registered in actors.c) | 677 | No direct caller; three stored pointers, each a `mov dword [reg+0xc],0x216ec` (the immediates at `0x219D0`, `0x21CD8`, `0x21D64`). The slot `+0x0C` callback is called at `0x35431`, which returns to `xor eax,eax` (`0x35434`). Mask 0. |
| `0x2A1FC` | `actor_sync` (actors.c) | 288 | Two callers: `0x2A389` (in `0x2A31C`), which returns to `mov eax,edx` (`0x2A38E`), and `0x35813`, which returns to `mov eax,[esp+4]` (`0x35818`). No stored pointer. Mask 0. |
| `0x2A31C` | `actors_update` (actors.c) | 127 | Two callers: `0x2EA8B`, which returns to `mov eax,0x105b44` (`0x2EA90`), and `0x2540F` in the `0x24C5C` tail: `cmp byte [0x104b15],0; je 0x2545c` then `call 0x3bb90` (`xor ah,ah; mov [0x107d30],ah; xor eax,eax` `0x3BB93..0x3BB9B`) or the mode switch `0x2545C` (`mov ax,[0x104b00]` and compares of AX alone; its arms reach `call 0x3bb90`, `call 0x12d48` + `xor eax,eax` `0x25550`, `xor eax,eax` `0x25591`, the mode-0xC arm's `lea eax,[edx*8]` `0x25499`, or the `ret` `0x255CA`). `0x24C5C`'s one caller `0x2560B` then reaches `mov eax,ebx` (`0x25621`) or `call 0x134c0` and `mov eax,[0x10150c]` (`0x2563E`), as for `0x1D540`. Mask 0. |
| `0x3EA24` | `fighter_3ea24` (fighter.c; registered in actors.c) | 508 | No direct caller; two stored pointers, `C7 43 0C` / `C7 40 0C` + `0x3EA24` (`mov dword [reg+0xc],imm`; the immediates at `0x3EC4E`, `0x3ED42`). Called at `0x35431`, returning to `xor eax,eax` (`0x35434`). Mask 0. |

## §2 The rows

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `camera_dispatch` | `0x12D48` | 19 | 11/11 | 17/17 | 0 | The switch is `cmp al,4; ja 0x12d78; and eax,0xff; jmp cs:[eax*4+0x12d34]`; the table's five dwords are `0x12D5E 0x12D65 0x12D6C 0x12D73 0x12D78` (mode 4 is the tail itself). The four mode bodies (`0x12DF0`, `0x12E3C`, `0x13290`, `0x1333C`; rows of d1h and d2d) and `0x12C7C` run on both sides: camera.c has no seam and the row adds none. One fixture makes every body move the camera x by its own amount (measured on the original, cam `0x1000`: mode 0 `0x1200`, or `0xD80` with the index byte 1; mode 1 `0x1090`; mode 2 `0x1040`; mode 3 `0xFC0`), so a dropped or swapped call is a byte difference. `d4..d8` (modes 4, 5, 6, `0xFF`, `0x80`) write nothing. The clamp is signed on both sides (`jle` `0x12D82`, `jge` `0x12D98`): `h0/h1/h2` are `0x5D01`, `0x5D00`, `0x7FFFFFFF`; `l0/l1/l2` are `-0x5D01`, `-0x5D00`, `0x80000000`; `e0/e1` cross a bound inside the mode body (`0x5CF0` + `0x40`, `-0x5CF0` - `0x40`), which only a clamp after the call gets right. |
| `fighter_1461c` | `0x1461C` | 18 | 10/10 | 30/30 | 0 | EAX = slot, EDX = rec, EBX = side. The table `0x1460C` is `0x1464A 0x14699 0x146D7 0x146E8` (`cmp al,3; ja 0x146e8`), switched on the `+0x57` byte of ctx[2] = `0x1077B0 + side*0x94` (`0x33950` runs on both sides; the cases pass a scratch slot in EAX so `@slotarg` differs). Stubs: `0x14590` (EAX = ctx[0]; **AL tested**, `test al,al; jbe` `0x14669/0x1466B`: `z3` returns `0x100`, `z4` `0xFFFFFF80`, `z5` 1), `0x4F944` (EAX = 1, after the counter store: memory at the call), `0x3F720` (EAX = ctx[0], EDX = the old counter sign-extended, `movsx ecx,dx` `0x146A4`; `o4/o5` are `0x8000/0x8001`), `0x146F0` (EDX = rec; its EAX is overwritten unread at `0x146F8`). The state-1 test re-reads the stored word (`mov eax,[eax*2+0xfd112]; sar eax,0x10; cmp eax,1; jge`): `o1` (2 -> 1) keeps state 1, `o2` (1 -> 0), `o3` (0 -> -1) and `o5` (`0x8001` -> `0x8000`) set 2. `n0..n3` are states 3, 4, `0x81`, `0x82` (nothing). |
| `fighter_14988` | `0x14988` | 18 | 10/10 | 30/30 | 0 | The twin, same cases and mutants: table `0x14978` = `0x149B6 0x14A05 0x14A43 0x14A54`, the counters `0xFD110 + side*2` (re-read through `0xFD10E`), the reload words `0x9AFEA`/`0x9AFE8`, `0x14A5C` for state 2. `@twin` runs the other function's addresses in each row. |
| `fight_hud_meter_step` | `0x1D540` | 12 | 35/35 | 46/46 | 0 | No argument; both sides in one call. Stubs: `0x468D8` (twice per side: first EAX = ctx[1] of `0x33A10`, `mov eax,[esp+4]` `0x1D5EC`, = the side; then EBX; **AL tested** at both, `test al,al; je` `0x1D5F5`, `0x1D6BB`: `e0` returns `0x100`, `p3` `0xFFFFFF80`) and `0x10D70` (reached inside `0x1D2F0`/`0x1D464`, which run on both sides with `0x33A10`, `0x1D4D0` and `0x1D74C`: the stub's record and table word show each bar call's value and side, and the memory at that call the order of the meter stores). Meter A steps +-1 toward the slot's `+0x5A`, meter B steps +1 or snaps down to `+0x5D`, both compared unsigned (`jbe`/`jae` `0x1D57E/0x1D596`, `0x1D5BB/0x1D5CF`; `m1/m2` use `0x90` against `0x10`/`0x7F`), the value handed on zero-extended (`xor eax,eax; mov al,..`). `p1..p7` walk the rate arm: gap `0x20` (6), `0x1B` (not above: 4), `0x1C`, negative, and `p7` a poked threshold `0x10` with gap `0x11`; `+0x5D` at `0xC`, `0xD`, `0x1A`, `0x1B`; the other slot's `+0x63` set (`p4`, `p5`: the absolute `0x107813`/`0x1078A7` test skips the adjustment); `+0x63` set with the limit byte `0xA76C8[idx]` = 9, `0xF` and `0x80` (zero-extended: `p5` stops at `+0x5E` = `0x80`, `p6` passes it). `m0..m2` walk the no-predicate decay against v. |
| `nameentry_arm` | `0x204F4` | 8 | 32/32 | 33/33 | 0 | EAX = rank, of which only the low word is read (`mov ax,si` `0x20514`, `mov bx,si` `0x20550`, `test dx,dx` `0x20561`, `cmp si,bx` `0x205AA`, `mov ax,[esp+0x1bc]` `0x20627`, `test si,si` `0x206BF`: `a4` = `0x00010003`, `a5` = `0x00010000`); EDX = score. `0x2DBC4` is a stub (seamed by this row): the call list `(rec, 0)` for rec = rank..8 (`cmp ebx,9; jl` `0x20553`) then 0..rank-1 (`cmp si,bx; jb` `0x205AA`) is everything the table build shows (see §4). Then: the name field `0x104367` is blank when all `0x24` bytes are spaces (`a3`: the last byte is `X`; `a6`: a NUL at +5); a blank field takes the default name of the `0x2C`-stride row `0xA7BC0 + rank*0x2C` up to its NUL or `0x24` characters (`a4`: a poked row with no NUL, the terminator lands on `0x10438B`; `a5`: `0x23` characters; `a7`: an empty row) and sets `0x10431E` = 1; the three `0x24`-byte fields are cleared/filled; `0x10431C` = 0, `0x104390` = score, `0x10431F` = `0x20`, `0x1044D4` = 0 (a byte), `0x10438C` = `0x2EE` (a word), and by rank == 0: `0x1044CC` = 2 or `0x12`, `0x1044C4` = `0x16`, `0x10431D` = `0x12` or 3. No port change. |
| `fighter_216ec` | `0x216EC` | 30 | 29/29 | 77/77 | 0 | EAX = slot (overwritten unread, `0x216F6`), EDX = rec (kept in ECX, read only for its `+0x51` at `0x2193E`: the cases pass a third record so `@c2rec`, `@c2ancrec`, `@c2animrec`, `@c4hitrec`, `@c4idxctx` differ), EBX = side. The side's counter word `0x104764[side]` is incremented before the state test (`inc word` `0x21702`; `n2/n3` are states 6 and `0xFF`; `s5` wraps `0xFFFF`), and read back sign-extended (`sar eax,0x10`). Table `0x216D4` = `0x21727 0x21807 0x21828 0x2198A 0x218B1 0x2198A`. Stubs: `0x1A570` (**AL tested**, `test al,al; jne` `0x217D6`: `s3` returns `0x100`, `s6` `0xFFFFFF80`), `0x188AC` (side, the record's `+0x18`, 0), `0x3C148`, `0x3C16C`, `0x2BC30` (ctx[4], `0xC8B58[char]`, 3.0), `0x3C4CC` (ctx[4], `0xE4A18` or `0xE16E6`, 3.0). State 0: `0x15/0x16/0x7FFF` around `c > 0x16`. State 1: `0x18/0x19`. State 2: the `0xBD882` dword by the character (`l2` is character `0x80`, zero-extended), its high half signed (`l3`: `0x9000`) against the slot's `+0x30` signed (`l4`: -1), `l0` equal. State 4: the same table **by the side** (EAX is still `side*2` at `0x218C0`: `f2`, `f8` separate `@c4char`), `c > 7` (`f2`: 7; `f9`: `0x8000`), the flag byte `0x10476E[side]` arm (`f0`), the target slot `0x1077A8[rec+0x51]` (index 0, 1 and `0x80`; null in `f7`: no store at all) with character 0, 1, 2, 6, 7. |
| `actor_sync` | `0x2A1FC` | 20 | 26/26 | 32/32 | 0 | EAX = rec; pset = `[0x1014EC] + word[rec+0x56]*0x20`, the word zero-extended (`xor esi,esi; mov si,[eax+0x56]`; `h0` is index `0x8003`: the entry and the `+2` store are at `0x20A862`, beyond the image, on both sides — pinned in `P45_OUTSIDE`; `@slotsext` lands at `0xA862`, inside `mem[]`). All five callees are stubs (seams added for three): `0x2AA70` (EAX = rec, EDX = the index, whole ESI), `0x2A39C` (the same pair), `0x2A4FC` (EAX), `0x2A820` and `0x2AD40` (EAX = rec, EDX = pset; ECX survives the stubs' EDX/EBP poison). `g0/g1`: the `0x104B26` arm writes the pset and returns. `t0..t4`: the timer test is `test dword [ebx+0x24],0x7fffffff` (`t2`: `0x80000000`; `t4`: 1), else `+0x2A` bit 9 and the byte `0x105BEE`. The `+0x2A` bit 2 is cleared after the timer call (`@clrearly`: the memory at the call) in the low byte alone (`t3`: `0xFDFF`). `e0..e4`: the hold counter `+0x4E` (1 -> `+0x2E`, else `+0x30`; `0x800` with `+0x5F` set). `m0..m4`: bit 3 and bit 9 of `+0x28`. |
| `actor_sync_chain` | `0x2A1FC` | 4 | 19/26 (7 named: the first row's) | 5/5 | 0 | The same function with stubs that write what the real callees can: `0x2AA70` sets the record's flag byte to 4, `0x2A39C` the hold counter to 1, `0x2A4FC` the flag byte to 8. Every flag test after a call reads the record again (`0x2A256`, `0x2A277`, `0x2A2BF`, `0x2A2E7`, `0x2A2FF`): `c0` runs timer -> id path -> hold -> step -> release with no pset write, `c3` starts with bit 3 set and ends in the pset write. One mutant per re-read (`@stale28`, `@stale4e`, `@stalegate`, `@stale2`, `@stalerel`); none of them shows in the first row, whose stubs write nothing. |
| `actors_update` | `0x2A31C` | 7 | 11/11 | 15/15 | 0 | No argument. `0x104B24` non-zero returns at once (`u0`, `u6` = `0x80`); with `0x104B26` zero the byte `0x105BEC` is decremented and, when it wraps to `0xFF`, reloaded with `0x105BEE` - 1 (`u3`: 7 -> 6; `u4`: 0 -> `0xFF`; `u5`: 1 -> 0, no reload). The walk from `[0x105BCC]` to the list head itself: bit 1 of `+0x28` skips, bit 0 is cleared in place (`and byte [eax+0x28],0xfe`), else `0x2A1FC` (a stub, seamed by this row, that rewrites the record's `+0` to `0x105BCC`: the walk continues from the next pointer read before the call, `mov edx,[eax]` `0x2A365`; `u5` links the records out of address order). |
| `fighter_3ea24` | `0x3EA24` | 25 | 19/19 | 62/62 | 0 | `0x216EC`'s sibling; EAX and EDX are not read (`mov edx,ebx` `0x3EA29`), EBX = side. The counter word is `0x1080A8[side]` (read back through `0x1080A6`); the table `0x3EA10` has five entries (`cmp dl,4; ja`): `0x3EA5C 0x3EB31 0x3EB51 0x3EC1A 0x3EBD6` (`n1..n3`: states 5, 6, `0xFF`). State 0 stores through ctx[4]/ctx[5] (`+0x34/+0x36/+0x44` = `0x96/0xA/0xA` and `0xFF6A/0xFE0C/0xA`; `@c0twin` is `0x216EC`'s `0x258/0x28`), then **AL of `0x1A570`** (`test al,al; jne` `0x3EB01`: `s3` returns `0x100`, `s6` `0xFFFFFF80`). State 2 is `0x216EC`'s (the table by the character). State 4 calls `0x36870(ctx[4])` on the same condition as `0x216EC`'s (the table **by the side**, `c > 7`) and stores nothing (`@c4s57`). The cases are `0x216EC`'s minus its target-slot arm. |

## §3 Corrections

Each is its own commit with a mutant that reproduces the old port.

| function | raw | old -> new | ordinary play |
|---|---|---|---|
| `fighter_1461c` (`0x1461C`) | `0x14669` `test al,al` / `0x1466B` `jbe 0x14686`, after `call 0x14590` at `0x1465F` | `if (fighter_14590(ctx[0]) != 0)` -> `if ((u8)fighter_14590(ctx[0]) != 0u)`: only AL selects the `0x9AFDA` reload and the `0x4F944(1)` call | Not reached: `0x14590` returns AL = 0 or 1 (`mov al,1` `0x145BE`, `xor al,al` `0x145C5`) and its port returns 0 or 1. Separated by the stub EAX `0x100` (case `z3`); mutant `@eax`. |
| `fighter_14988` (`0x14988`) | `0x149D5` `test al,al` / `0x149D7` `jbe 0x149f2`, after `call 0x14590` at `0x149CB` | the same change, for the `0x9AFEA` reload | Not reached, as above. Case `z3`, mutant `@eax`. |
| `fight_hud_meter_step` (`0x1D540`) | `0x1D5F5` and `0x1D6BB`, both `test al,al` (then `je 0x1d6b4` / `je 0x1d6fd`), after `call 0x468d8` at `0x1D5F0` and `0x1D6B6` | `if (ai_pred_468d8(side) != 0u)` -> `if ((u8)ai_pred_468d8(side) != 0u)` at both sites | Not reached: `0x468D8` returns AL = 0 or 1 and its port 0 or 1. Case `e0` (stub EAX `0x100`); mutants `@eax1`, `@eax2`. |
| `fighter_216ec` (`0x216EC`) | `0x217D6` `test al,al` / `0x217D8` `jne 0x2198a`, after `call 0x1a570` at `0x217D1` | `if (fighter_actor_bit15_clear(ctx[0]) != 0) return;` -> `if ((u8)fighter_actor_bit15_clear(ctx[0]) != 0u) return;` | Not reached: `0x1A570` ends `sete al` (`0x1A5A4`) or `xor al,al` (`0x1A59A`) and its port returns 0 or 1. Case `s3` (stub EAX `0x100`); mutant `@eax`. |
| `fighter_3ea24` (`0x3EA24`) | `0x3EB01` `test al,al` / `0x3EB03` `jne 0x3ec1a`, after `call 0x1a570` at `0x3EAFC` | `if (fighter_actor_bit15_clear(ctx[0]) == 0)` -> `if ((u8)fighter_actor_bit15_clear(ctx[0]) == 0u)` | Not reached, as for `0x216EC`. Case `s3`; mutant `@eax`. |

Seen, not changed (not this batch's rows): `fighter_146f0` (`0x14707`), `fighter_14a5c` (`0x14A73`),
`fighter_14814` (`0x14872`) and `fighter_14b90` (`0x14BEC`) test the whole return value of `fighter_14590`
where the raw has `test al,al`; their rows run `0x14590` real (allowed) and name the limit (d2a §4, d2d). With
`fighter_14590` now seamed a later batch can stub it there.

## §4 Named gaps and unhit blocks

- **`nameentry_arm`: the frame table is not compared.** `0x204F4` builds ten `0x2C` records at `[esp]`
  (`sub esp,0x1c0`) from what `0x2DBC4` returns and never reads them back before `add esp,0x1c0` (`0x20702`).
  The emulator's private stack is outside the comparison and the port's `tbl` is a C local, so the copy loops
  (`0x2052D..0x2054B`, `0x20585..0x205A0`, `0x205D7..0x205F3`) are exercised (every block is hit) but a wrong
  stride or offset in the port's `tbl` code cannot show. The row's core does not rebuild the table.
- **`nameentry_arm`: rank above 9 is not cased.** The frame holds ten records (`0x1B8` bytes) and the saved
  EDX/EAX at `+0x1B8`/`+0x1BC`; record 10 would overwrite them. The one caller reaches `0x1ECA7` only with
  rank < 10 (flow.c `0x1EC38`). Nor is a `0x2DBC4` that returns 0 (the raw then reads `[0]`): the stub returns
  a seeded record.
- **`actors_update`: records outside the pool are not cased.** The port's `actor_next` returns 0 for a record
  that is not a `0x68`-aligned member of the pool at `[0x1014F4]` (`in_pool`, a documented `PORT:` invariant);
  the raw follows any pointer. The cases link pool records only.
- **`actor_sync_chain`'s seven unhit blocks** (`0x2A21D`, `0x2A244`, `0x2A293`, `0x2A2A4`, `0x2A2B0`, `0x2A2B7`,
  `0x2A2B9`) are the `actor_sync` row's (`g0/g1`, `t1..t3`, `e1/e3/e4`): the chain row exists for the re-reads.

## §5 Store-sweep findings

- `camera_dispatch`: its own stores are the two clamp writes of `0xF0AF0`. The camera block
  `0xF0AEC..0xF0AFF` is poked whole (y, x, the shake words, a sentinel dword, the step word, the mode byte,
  the index byte), so a store wider than the dword would show; `@nohi`/`@nolo` drop each store and are caught
  by `e0 h0 h2` / `e1 l0 l2`. A `>=`/`<=` variant of either compare stores the value already there and is not
  observable (named, not a mutant).
- `fighter_1461c` / `fighter_14988`: every store has a mutant that drops it (`@nos1`, `@c1nodec`, `@c1nos2`,
  `@c2nos3`; the reload-word store is separated by `@swapk`/`@cntside`). The counter block `0xFD10C..0xFD11F` is
  seeded whole and the reload words sit between sentinel words, so `@kwidth` (a dword store) and `@c1dword` (a
  dword decrement, caught by `o3` alone: the borrow out of 0) are byte differences. The order of each store and
  its call is the memory at the call (`@late1`, `@late4f944`, `@c1late`, `@c2early`). Not separable: a port
  that tested the decremented local instead of re-reading the word after `0x3F720` (the stub writes nothing;
  the only code that names `0xFD110`..`0xFD116` or the read-back addresses `0xFD10E`/`0xFD112` is these two
  functions: `0x149E2 0x149FB 0x14A0C 0x14A18 0x14A29`, `0x14676 0x1468F 0x146A0 0x146AC 0x146BD`).
- `fight_hud_meter_step`: both slots are seeded whole, the four meter bytes sit between seeded dwords, the
  command words and index dwords between seeded neighbours, `0x104AEC` (`0xFF`) between sentinels. Every store
  has a dropping mutant (`@no5e`, `@no5eset`, `@no5ez`, `@fno5ez`, `@fnodec`, `@nobit`; the meter stores by
  `@asnap`/`@bstep` and the memory at the `0x10D70` call). Not separable, named: the two `0x468D8` calls of a
  side return the same stub EAX, so a port that reused the first result is not told apart; `0x1D74C` returns
  EAX = 0 or 1 whole (`and eax,0xffff; setne al`), so `test al,al` at `0x1D604` has no AL/EAX case; the
  `+0x5D >= rate` boundary gives 0 on both arms (`0x1D6E5`); `0x1D4D0`'s rate table `0xA769C` holds the same
  four bytes for every character in the image (its own row's concern).
- `nameentry_arm`: `0x104318..0x1043BF` and `0x1044C0..0x1044D7` are seeded whole with bytes that are none of
  the stored values (0, 1, 3, `0x12`, `0x20`), so each of the 13 stores has a dropping mutant (`@noterm`,
  `@noflag`, `@noclr1f`, `@noclr43`, `@nofill94`, `@nocount`, `@noscore`, `@nosp1f`, `@nod4`, `@notimer`,
  `@nocc`, `@noc4`, `@nolimit`) and the width mutants `@scorew`, `@d4w`, `@timerw`, `@limitw` are byte
  differences. `@clrlen` (0x23 bytes) is caught by every case.
- `fighter_216ec`: the two slots, the scratch slot and the three records are seeded whole; the counter block
  `0x104760..0x10476F` too. Each of the 22 stores has a dropping mutant (`@noinc`, `@c0nos57` .. `@c0noo44`,
  `@c1nos`, `@c2no54`, `@c2nos57`, `@c4no53` .. `@c4no1c`, `@c4nos57`, `@c4no8a`; the two negations by `@negone`
  and `@bitinv`), the widths by `@incd`/`@incb` and `@negdw`, the order against the stubbed calls by the memory
  at the call (`@c0late`, `@c2early`, `@c2late54`, `@c4early`). The first run of the case set left `@c4sign`
  (`<= 0` for the `+0x36` sign test) undetected: no state-4 case had `+0x36` = 0; `f11` closes it.
- `actor_sync`: the record (0x68 bytes) and pset entries 2..4 are seeded whole. Its three stores (`+0x2A`
  bit 2, `+0x4E`, pset `+2`) have dropping or widening mutants (`@noclr`, `@clrw`, `@e_nodec`, `@e_off`,
  `@e_d`: a dword store over the entry's `+4` word). A `+0x4E` of 0 writes nothing (`e2`; `@e_always`).
- `actors_update`: the counter and its reload sit between seeded bytes (`@decw`, a word decrement, borrows
  into `0x105BED`: `u3`, `u4`); each record's `+0..+0x2F` is seeded (`@clrbit1`, `@noclr`).
- `fighter_3ea24`: the `0x216EC` fixture with the counter block `0x1080A0..0x1080AF`; each of its 15 stores
  has a dropping mutant, the widths `@incd`/`@incb`/`@negdw`, the order `@c0late`/`@c2early`/`@c2late54`.

## §6 Queue entries not done

(filled at the end)
