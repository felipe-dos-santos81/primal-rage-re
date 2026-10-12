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

## §2 The rows

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `camera_dispatch` | `0x12D48` | 19 | 11/11 | 17/17 | 0 | The switch is `cmp al,4; ja 0x12d78; and eax,0xff; jmp cs:[eax*4+0x12d34]`; the table's five dwords are `0x12D5E 0x12D65 0x12D6C 0x12D73 0x12D78` (mode 4 is the tail itself). The four mode bodies (`0x12DF0`, `0x12E3C`, `0x13290`, `0x1333C`; rows of d1h and d2d) and `0x12C7C` run on both sides: camera.c has no seam and the row adds none. One fixture makes every body move the camera x by its own amount (measured on the original, cam `0x1000`: mode 0 `0x1200`, or `0xD80` with the index byte 1; mode 1 `0x1090`; mode 2 `0x1040`; mode 3 `0xFC0`), so a dropped or swapped call is a byte difference. `d4..d8` (modes 4, 5, 6, `0xFF`, `0x80`) write nothing. The clamp is signed on both sides (`jle` `0x12D82`, `jge` `0x12D98`): `h0/h1/h2` are `0x5D01`, `0x5D00`, `0x7FFFFFFF`; `l0/l1/l2` are `-0x5D01`, `-0x5D00`, `0x80000000`; `e0/e1` cross a bound inside the mode body (`0x5CF0` + `0x40`, `-0x5CF0` - `0x40`), which only a clamp after the call gets right. |
| `fighter_1461c` | `0x1461C` | 18 | 10/10 | 30/30 | 0 | EAX = slot, EDX = rec, EBX = side. The table `0x1460C` is `0x1464A 0x14699 0x146D7 0x146E8` (`cmp al,3; ja 0x146e8`), switched on the `+0x57` byte of ctx[2] = `0x1077B0 + side*0x94` (`0x33950` runs on both sides; the cases pass a scratch slot in EAX so `@slotarg` differs). Stubs: `0x14590` (EAX = ctx[0]; **AL tested**, `test al,al; jbe` `0x14669/0x1466B`: `z3` returns `0x100`, `z4` `0xFFFFFF80`, `z5` 1), `0x4F944` (EAX = 1, after the counter store: memory at the call), `0x3F720` (EAX = ctx[0], EDX = the old counter sign-extended, `movsx ecx,dx` `0x146A4`; `o4/o5` are `0x8000/0x8001`), `0x146F0` (EDX = rec; its EAX is overwritten unread at `0x146F8`). The state-1 test re-reads the stored word (`mov eax,[eax*2+0xfd112]; sar eax,0x10; cmp eax,1; jge`): `o1` (2 -> 1) keeps state 1, `o2` (1 -> 0), `o3` (0 -> -1) and `o5` (`0x8001` -> `0x8000`) set 2. `n0..n3` are states 3, 4, `0x81`, `0x82` (nothing). |
| `fighter_14988` | `0x14988` | 18 | 10/10 | 30/30 | 0 | The twin, same cases and mutants: table `0x14978` = `0x149B6 0x14A05 0x14A43 0x14A54`, the counters `0xFD110 + side*2` (re-read through `0xFD10E`), the reload words `0x9AFEA`/`0x9AFE8`, `0x14A5C` for state 2. `@twin` runs the other function's addresses in each row. |

## §3 Corrections

Each is its own commit with a mutant that reproduces the old port.

| function | raw | old -> new | ordinary play |
|---|---|---|---|
| `fighter_1461c` (`0x1461C`) | `0x14669` `test al,al` / `0x1466B` `jbe 0x14686`, after `call 0x14590` at `0x1465F` | `if (fighter_14590(ctx[0]) != 0)` -> `if ((u8)fighter_14590(ctx[0]) != 0u)`: only AL selects the `0x9AFDA` reload and the `0x4F944(1)` call | Not reached: `0x14590` returns AL = 0 or 1 (`mov al,1` `0x145BE`, `xor al,al` `0x145C5`) and its port returns 0 or 1. Separated by the stub EAX `0x100` (case `z3`); mutant `@eax`. |
| `fighter_14988` (`0x14988`) | `0x149D5` `test al,al` / `0x149D7` `jbe 0x149f2`, after `call 0x14590` at `0x149CB` | the same change, for the `0x9AFEA` reload | Not reached, as above. Case `z3`, mutant `@eax`. |

Seen, not changed (not this batch's rows): `fighter_146f0` (`0x14707`), `fighter_14a5c` (`0x14A73`),
`fighter_14814` (`0x14872`) and `fighter_14b90` (`0x14BEC`) test the whole return value of `fighter_14590`
where the raw has `test al,al`; their rows run `0x14590` real (allowed) and name the limit (d2a §4, d2d). With
`fighter_14590` now seamed a later batch can stub it there.

## §4 Named gaps and unhit blocks

None so far.

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
  the real `0x3F720` does not write the counter either).

## §6 Queue entries not done

(filled at the end)
