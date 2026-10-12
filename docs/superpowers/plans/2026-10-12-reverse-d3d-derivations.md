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

## §2 The rows

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `camera_dispatch` | `0x12D48` | 19 | 11/11 | 17/17 | 0 | The switch is `cmp al,4; ja 0x12d78; and eax,0xff; jmp cs:[eax*4+0x12d34]`; the table's five dwords are `0x12D5E 0x12D65 0x12D6C 0x12D73 0x12D78` (mode 4 is the tail itself). The four mode bodies (`0x12DF0`, `0x12E3C`, `0x13290`, `0x1333C`; rows of d1h and d2d) and `0x12C7C` run on both sides: camera.c has no seam and the row adds none. One fixture makes every body move the camera x by its own amount (measured on the original, cam `0x1000`: mode 0 `0x1200`, or `0xD80` with the index byte 1; mode 1 `0x1090`; mode 2 `0x1040`; mode 3 `0xFC0`), so a dropped or swapped call is a byte difference. `d4..d8` (modes 4, 5, 6, `0xFF`, `0x80`) write nothing. The clamp is signed on both sides (`jle` `0x12D82`, `jge` `0x12D98`): `h0/h1/h2` are `0x5D01`, `0x5D00`, `0x7FFFFFFF`; `l0/l1/l2` are `-0x5D01`, `-0x5D00`, `0x80000000`; `e0/e1` cross a bound inside the mode body (`0x5CF0` + `0x40`, `-0x5CF0` - `0x40`), which only a clamp after the call gets right. |

## §3 Corrections

None so far.

## §4 Named gaps and unhit blocks

None so far.

## §5 Store-sweep findings

- `camera_dispatch`: its own stores are the two clamp writes of `0xF0AF0`. The camera block
  `0xF0AEC..0xF0AFF` is poked whole (y, x, the shake words, a sentinel dword, the step word, the mode byte,
  the index byte), so a store wider than the dword would show; `@nohi`/`@nolo` drop each store and are caught
  by `e0 h0 h2` / `e1 l0 l2`. A `>=`/`<=` variant of either compare stores the value already there and is not
  observable (named, not a mutant).

## §6 Queue entries not done

(filled at the end)
