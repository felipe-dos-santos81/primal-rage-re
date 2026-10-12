# Track D, wave 3, batch d3e — derivation record

Differential rows for a finding batch d3d measured but did not land (`camera_y_clamp`, `0x1317C`), an audit
finding the coordinator added (`fight_384f8`, `0x384F8`, and its callee `0x38434`), and the queue entries d3c
and d3d did not reach. Branch `reverse-d3e` off `7a9dfe7` (the base of the other wave-3 batches). Ground
truth: the fixed-up image `build/diffrun --image-out` dumps (linear addresses), disassembled with capstone
5.0.7. Every figure in §2 is measured with `python3 tools/diff_verify.py --image /tmp/pr_d3e_img.bin
--function <row> --self-check`; the pins (`D3E_MASKS`/`D3E_KINDS`/`D3E_CASES` in
`tools/tests/test_diff_verify.py`) come from `verify_all` on the row's own spec. Each row and its mutants
were run ten times in a row (identical output). The claim of each row is the track's: equivalence on the
exercised blocks and inputs only, with the callees stubbed or run as stated. Every correction of §3 is
unproven against the capture oracles until the integrator's gate.

**How the EAX masks were derived.** A walk over the image's bytes from each return address: the three
parts of the returned EAX (AL, AH, bits 16..31) stay "alive" until an instruction writes them; both arms
of every `jcc`, every bounded switch, every direct callee and, at a `ret`, every `call`/`jmp rel32` site of
the function the path is in are followed; a read of a live part, an indirect call or jump, or a dword in
the image equal to a function's entry is reported. Mask 0 below means the walk ended with no read and
nothing open other than what the row names; the instructions that write the last live part are listed so
the claim can be checked by reading them. (`xor r,r` and `and eax,imm` with the part's bits clear count as
writes; `push eax` would count as a read and none occurs.)

## §1 The members

| addr | port function | size | callers and what they read of EAX |
|---|---|---|---|
| `0x1317C` | `camera_y_clamp` (camera.c; *static* before this batch) | 122 | Three `call rel32` sites, no `jmp`, no pointer. `0x1321D` and `0x1323C` sit in `0x131F8` and `0x13224`, entries nothing calls and no dword of the image equals (dead). `0x12DE9` is in `0x12DA8` (`pop edx; ret` `0x12DEE`), which has 15 call sites; from each, every path writes EAX before reading it — table below. Mask 0. |
| `0x384F8` | `fight_384f8` (fighter.c) | 183 | Two sites, both in `0x266AC`: `0x2676B` returns to `mov eax,1` (`0x26770`), `0x26775` to `xor eax,eax` (`0x2677A`). No pointer. Mask 0. |
| `0x38434` | `fighter_38434` (fighter.c; *static* before this batch) | 195 | One site, `0x3856B` (in `0x384F8`), which returns to `mov eax,esi` (`0x38570`). No pointer. Mask 0. |
| `0x3B464` | `fighter_think_side` (fighter.c; *static* before this batch) | 608 | One site, `0x197EF` (in `0x1975C`), which returns to `mov eax,[esp+8]` (`0x197F4`). No pointer. Mask 0. |

`0x12DA8`'s call sites (the function each is in, then the instructions that write the last live part of
EAX on its paths; "returns" = the function can return with EAX alive, and the walk continues in its caller):

| site | in | closed by |
|---|---|---|
| `0x263A5` | `0x26254` | `0x1DA2B` `mov eax,[ecx+0x1028f8]` (in `0x1DA08`), `0x27FAC` `xor eax,eax` |
| `0x26534` | `0x263F4` | returns; its callers `0x11C3F`/`0x11E8F` reach `0x33F15` `mov eax,[ebx+0x1077a8]` |
| `0x26691` | `0x26540` | `0x1DA2B`, `0x27FAC` |
| `0x26D23` | `0x26C8C` | `0x4D2FB` `xor eax,eax`, `0x4D767` `lea eax,[ecx*8]` (in `0x4D2D0`) |
| `0x270AE` | `0x26F58` | returns to the mode switch of `0x24C5C` (site `0x2532B`): see below |
| `0x274E2` | `0x27380` | `0x1DA2B`, `0x272E6` `lea eax,[edx*8]` |
| `0x27554` | `0x274FC` | `mov ah,[0x104aec]` (`0x27559`) writes AH; `0x2757E` `xor eax,eax`; else returns (site `0x25367`) |
| `0x277EE` | `0x277C0` | `mov ah,[0x104aec]` (`0x277F3`); `0x2781A` `mov eax,0x2b`; else returns (site `0x2537B`) |
| `0x28307` | `0x282C4` | `0x2830C` `xor eax,eax` |
| `0x2851D` | `0x28468` | `0x4A70C` `mov eax,[0x10884c]`; else returns (site `0x25335`) |
| `0x2884C` | `0x28788` | `0x288A5` `xor eax,eax` |
| `0x28C09` | `0x28BD4` | `0x28C20` `mov eax,0xd8`; else returns (site `0x2527D`) |
| `0x29664` | `0x29638` | `mov ah,[0x104aec]` (`0x29669`); `0x2968C` `mov eax,0x2b`; else returns (site `0x251BC`) |
| `0x29710` | `0x296B8` | `mov ah,[0x104aec]` (`0x29715`); `0x29759`, `0x29922` `lea eax,[ecx*8]`; else returns (site `0x251B2`) |
| `0x29B4A` | `0x299E8` | `0x1DA2B`, `0x29972` `xor eax,eax` |

The seven mode steps that return do so into `0x24C5C`, whose tail and one caller (`0x2560B`) write EAX on
every path before reading it: `0x2A353` `mov eax,[0x105bcc]` (`0x2A31C`), `0x3BB9B` `xor eax,eax`,
`0x12D51` `and eax,0xff` (after `mov al,[0xf0afe]`), `0x25499` `lea eax,[edx*8]`, `0x25550`/`0x25591` `xor
eax,eax`, `0x33F15`, then `0x25621` `mov eax,ebx` or `0x134C0` (`0x13528` `and eax,0xff`, `0x135AB`,
`0x139A0`, `0x139B8`) and `0x2563E` `mov eax,[0x10150c]`. The one dword of the image equal to a function
on these paths is `0x14C48` = `0x000274FC`: it is the `rel32` of `call 0x3c148` at `0x14C47` (`E8 FC 74 02
00`), not a pointer.

## §2 The rows

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `camera_y_clamp` | `0x1317C` | 31 | 8/9 (1 named) | 25/25 | 0 | No argument. x = the dword `0x1078F2` `sar 16` (`0x13185`); x <= `0x1400` (`jle` `0x1318D`) gives the target 0 (`g0..g5`: `0x1400`, `0x1401`, `0x1000`, `0x8000`, `0xFFFF`, `0x14D2`); else d = x - `0x1400`, `fild`/`fstp dword` (exact: d < 2^24), `fld; fld st(0); fmul dword [0x804a8]; fmulp` and `0x61A4C` (`frndint` under a control word whose high byte is `0x1F`: chop; run from the original's bytes, where the port casts), `fistp`. `f0..f3` are the four x where a single-precision product truncates to another integer (§3.1); `f4` (8000) the x where only an inner product in single precision does, `f5` (29229) only a double product stored to a float; `f6` is `0x7FFF`. Then `0x12CD4` (its one call site is `0x131CB`; run from the original's bytes, inlined in the port; its 9 blocks are all executed, checked apart): snap on \|target - cam\| <= `0x100` (`jg` `0x12CF3`; `s0`, `s2` at exactly `0x100`, `s8`/`s9` at `0x90`), else a `0x100` step (`s1`, `s3`, `s4`, `s5`), plus the dword `0xF0AF2` `sar 16` = the signed shake word `0xF0AF4` (`s4` +`0x30`, `s5` -`0x10`, `s7` `0x8000`); `s6` is a difference of `0x80000000` (`neg` leaves it negative: snap). Then the cap `[0x9AF28 + word[0x104AFC]*2] sar 16` (index zero-extended, `xor eax,eax; mov ax,..` `0x131D0`: `c4` is index `0x8000`; `c3` index 1), stored when below the camera y read back (`jge` `0x131EA`: `c0` `0x1FF` < `0x200`, `c1` equal, `c2` above, `c5` a negative cap `0x8000`, `c6` a negative camera y against cap 0, `c7` the image's own table at index 2). |
| `fight_384f8` | `0x384F8` | 16 | 10/10 | 30/30 | 0 | EAX = side (kept in ECX; the record in ESI). Stubs: `0x3C59C(3, side)` (**AL tested**, `test al,al` `0x3850A`: `a2` returns `0x100`, `a3` `0xFFFFFF00`, `a4` `0x1FF`), `0x38D24`, `0x382C4`, `0x38434` (the side), `0x2A4FC` (the record: `mov eax,esi; call 0x2a4fc` `0x38570/0x38572`), `0x188AC`, `0x186C4`; `0x34038(side)` runs real and is recorded (§4). AL == 0 increments the **own** slot's `+0x88` word (`inc word [eax*4+0x107838]`, EAX = side*37; `a2` wraps `0xFFFF`); `0x1078FB` = (byte `0x1088D4` bit 2 clear) + 2 (`d4` = `0xFB`, 4, `0xFF`, 0); slot = `[0x1077A8 + side*4]` and rec = `[slot]` (a scratch slot and record, not the fighters'), either null returns (`n0`, `n1`); slot`+0x28` = rec`+0x18` before the calls (the memory at the first call); the clamps: `[0xBE018]` signed against rec`+0x18` (`jg` `0x38580`: `x0` equal, `x1` one below, `x2` above, `x7` a negative threshold, `x8` x = `0x80000000`), then its negation (`jl` `0x38599`: `x3` equal, `x4` one above, `x5` below, `x6` threshold 0 and x 0: both), each `0x188AC(side, bound, rec+0x1C)`. |
| `fight_384f8_chain` | `0x384F8` | 2 | 9/10 (1 named: the first row's) | 6/6 | 0 | The same function with stubs that write what the real callees can: `0x2A4FC` sets the record's x to `0x5000`; `0x188AC` sets it to `-0x7000`, the record's y to `0xC0FFEE` and the threshold dword to `0x6000`. The first clamp reads x after `0x2A4FC` (`0x3857D`); the second reads the threshold, x and y again (`0x3858C`, `0x38592`, `0x3859D`). `c0` runs both clamps (the second with `0xFFFFA000` and the new y); `c1` neither (x `0x6000` before the motion call, `0x5000` after, threshold `0x5001`). One mutant per re-read (`@xearly`, `@thrstale`, `@x2stale`, `@y2stale`), plus `@late28` (a byte here, the memory at a call in the first row) and `@nomotion`. |
| `fighter_38434` | `0x38434` | 28 | 15/15 | 30/30 | 0 | EAX = side. slot = `[0x1077A8 + side*4]`, rec = `[slot]` (`p0`, `p1`: null). The slot's `+0x53` through the 13-entry table `0x38400` (`cmp al,0xc; ja 0x38467`): `0x38467` x4, `0x384BB`, `0x38467` x2, `0x384C3`, `0x384E1`, `0x38467` x3, `0x384F2`; `t0..t12` are its entries, `tD`/`t80`/`tFF` above it. 4: `inc byte [ecx+0x56]` (`t4` wraps `0xFF`). 7: `+0x41` bit 7, `+0x43 &= 0xCF`, then the `+0x0C` hook when non-null with EAX = slot, EDX = rec, EBX = side (`0x384D7/0x384D9`): `h0` the registered `0x23130` (run on both sides; its `0x3C4CC` stub call shows the record, its stores the slot), `h1` the registered `0x47E9C` (its per-side counter `0x108370` shows the side, its state byte the slot), `t7` a null hook. 8: `+0x56` + 1 and `+0x43 &= 0xCF`. `0xC`: nothing. The rest: `0x1922C(side)` (a stub; the clears come after it: the memory at the call), `+0x74` = 0 (a word), `+0x56` = 0, then with byte `0x1088BC` == side, `0x1078F0`, `0x1078F1` and `0x1088BE` all non-zero (`g0..g3` fail one each) `0x3D004(side)` (a stub, **AL tested**, `test al,al` `0x384AA`: `r0` 0, `r1` `0x100`, `r2` `0xFFFFFF00`, `r3` `0x80`) clears the byte `0x1078F0[side]`. No port change. |

| `fighter_think_side` | `0x3B464` | 21 | 29/29 | 44/44 | 0 | EAX = side (kept in ECX for the `+0x48 == 8` arm, in ESI the stance). `0x33A10`, `0x3AFC4` and the `+0x14` hook `0x29D04` run on both sides; fourteen stubs. ctx = `0x33A10(side)`: ctx[0] = the thrower, ctx[1] = the side. **AL of `0x3C59C(1, ctx[1])`** (`test al,al` `0x3B484`: `k0` 1, `k1` `0x100`, `k2` `0xFFFFFF00`) and the thrower's `+0x64` == `0xFF` (`k3`) return. The triple of (ctx[0], `+0x64`), the side's `+0x67` = 1, then **AL of `0x3B298(ctx[1], +0x64)`** (`test al,al` `0x3B4CC`: `b0` 1, `b2` `0x100`, `b3` `0xFFFFFF01`) selects the block arm: the thrower's `+0x8A` = 0, `0x3B080(ctx[1], anim+3, anim+2, 0)` unless the side's `+0x54` is 2 (`b1`), `0x3AD98` with the triple by value. Else `0x2BD44(rec, row)` when the side's record's `+0x4B` row has `+0x60` set (`d0`; `d1` clear); the hook when non-null, EAX = EDX = the slot, its EAX tested whole (`test eax,eax` `0x3B51E`: `d1` returns 0, `d2`/`d3` 1 after its `0x2A17C` call: the field is cleared); the thrower's projectile `+0x48` (`cmp al,5; jb; jbe; cmp al,8; je`): 5 -> `0x36D20(slot)`, `+0x18`/`+0x1C` = 0 (`d0`); 8 -> `0x1922C(side)`, `0x235C4(side)` (`d1`); else (4, 6, 7, 9, `0xFF`) `0x39834(ctx[1], stance)` and, `+0x54` == 2, `0x18B04` and `0x39F40(ctx[1], -0x50, 0x64, 0xF, 0x14)` (`m0`, `m1`), or `0x3A95C(ctx[1], the signed byte anim+6)` (`m7`: `0x80`) and, `+0x64` neither 0 nor 5 (`m5`, `m6`) and the byte `+0x90` - 1 above 3 unsigned (the table `0x3B454`'s four entries are all `0x3B5EA`: 5 in most cases, `m2` 4, `m3` 1, `m4` 0), `0x188DC(ctx[1], +0x2C)`; `0x3B080`; `+0x18`/`+0x1C` = 0. The tail: `+0x41` bit 7, the thrower's `+0x64` = `0xFF`. |

## §3 Corrections

Each is its own commit with a mutant that is the old port.

### §3.1 `camera_y_clamp` (`0x1317C`): the curve is evaluated at 53 bits (commit `7d077167`)

- **Raw.** `0x13198` `fild dword [esp]`, `0x1319B` `fstp dword [esp+8]` (d = x - `0x1400` as a float:
  exact), `0x131AC` `fld dword [esp+8]`, `0x131B0` `fld st(0)`, `0x131B2` `fmul dword [0x804a8]`, `0x131B8`
  `fmulp st(1)`: d * (d * C) in x87 registers, C = the float `0x3817B426`. The harness runs the original at
  the game's control word `0x127F` (53-bit precision, round to nearest; record P3 §P3.6). `0x131BA` `call
  0x61a4c` truncates (`mov byte [esp+1],0x1f; fldcw; frndint`), `0x131BF` `fistp`.
- **Old port.** `float c, d; target = (s32)(d * (d * c));` — both products rounded to 24 bits.
- **Measured.** Running the original's bytes for every word `0x1401..0x7FFF` (27 647 runs, scratch
  script, `0x61A4C` and `0x12CD4` allowed, the camera y seeded 3 above the expected target so `0x12CD4`
  snaps) gives `(s32)((double)d * ((double)d * (double)C))` every time (0 differences) and the
  single-precision expression's value everywhere but four words: `0x1078F4` = **12320, 19520, 22483, 27181**
  (original **1875, 7500, 10903, 17602**; single precision 1874, 7499, 10904, 17603). On the port's own
  function, before the fix, the row gave `f0: byte 0xF0AEC: original 0x53, port 0x52`, `f1: byte 0xF0AEC:
  original 0x4C, port 0x4B`, `f2: byte 0xF0AEC: original unchanged, port 0x98`, `f3: byte 0xF0AEC:
  original 0xC2, port 0xC3`. d3d's claim is confirmed in full.
- **New port.** `double c, d; target = (s32)(d * (d * c));`. Why a C `double` is the x87 here: d has at
  most 15 bits and C 24, so d * C is exact in 53 bits; the outer product (at most 54 bits) is rounded once
  to 53 bits, to nearest, by the x87 under `0x127F` and by an IEEE double multiply alike (no overflow, no
  denormal in this range); `frndint` under chop and the C cast both truncate. The order d * (d * C) is the
  raw's; (d * d) * C gives the same integer for every x of the range (not a mutant: not observable).
- **Mutants.** `@float` (the old code), caught by `f0 f1 f2 f3` alone; `@mixed` (the inner product
  rounded to a float: `f0 f1 f4 f6`), `@outer` (the product stored to a float before the truncation: `f3
  f5`), `@round` (to nearest: `f2 f3 f5 g5`).
- **Ordinary play.** The word `0x1078F4` is the low word of a fighter slot's `+0x30` (`0x12DA8`: camera
  mode byte `0xF0AFE` 0, slot `[0xF0AFF]`; any other mode, the signed larger of the two slots' `+0x30`
  dwords), i.e. the tracked fighter's height; `0x186D0` copies it from the record's `+0x1C` plus the
  anchor offset each frame. A scratch build of the port that logged the word at every `camera_y_clamp`
  call (not committed) was run over the port's replays of ten gameplay captures (`gp-idle-loss`,
  `gp-u6-moves`, `gp-u6-moves-b`, `gp-keys-fight`, `gp-twop`, `gp-pads`, `gp-u8-right-arcade`,
  `gp-u8-endurance`, `gp-u9-win`, `gp-u10-ending`): 12 356 calls, 647 distinct words, 563 of them above
  `0x1400`, the largest 30066; the words above the threshold have no coarse grid (62 of the 64 low-6-bit
  residues occur). **12320 occurs twice**: in `gp-u9-win` and `gp-u10-ending`, at the 12th call of game mode 8
  (the round's end), camera mode 1 (the sequence there is 12032, 12160, 12256, **12320**, 12352, 11840).
  At that call the camera y was 1841, the shake 0 and the cap index 5 (cap `0x1700`): the original's
  bytes snap to 1875 where the old port stored 1874, for that one frame (the next frame has another
  word). So ordinary play does reach a differing input; the effect seen is one unit of camera y on one
  frame. Limits, named: both scenarios reach mode 8 through memory pokes (the KO), and the captures do
  not snapshot the word, so that the original game had 12320 on that frame is the port's replay, not a
  capture reading; whether one unit of `0xF0AEC` moves a pixel was not traced; 19520, 22483 and 27181
  did not occur in these replays (words up to 30066 did, so they are in range).

### §3.2 `fight_384f8` (`0x384F8`): the record's motion step was missing (commit `fe41d15f`)

- **Raw.** `0x3856B` `call 0x38434`, **`0x38570` `mov eax,esi`, `0x38572` `call 0x2a4fc`**, `0x38577`
  `mov edx,[0xbe018]`. ESI = `[slot]`, loaded at `0x38548` and preserved by the four calls between.
  `0x2A4FC` is `motion_step` (actors.c).
- **Old port.** No call between `fighter_38434(side)` and the clamp (actors.h already listed `0x38572`
  among `0x2A4FC`'s callers).
- **Measured before the fix.** Every case that passes the null checks: `a0: call #5: original
  0x2A4FC(0x10A400), port 0x186C4()`, `a0: call #6: original 0x186C4(), port none` (14 of the 16 cases; 49
  problem lines in all with §3.3's).
- **New port.** `motion_step(rec);` after `fighter_38434(side);`.
- **Mutant.** `@nomotion` (the old code): every case but `n0`, `n1`; in the chain row both cases.
- **Ordinary play.** Reached whenever game mode `0x25` runs: `0x266AC` calls `0x384F8` once per side per
  frame (`0x2676B`, `0x26775`), and the call is unconditional once the slot pointer and its record are
  non-null. `0x2A4FC` returns at once unless the record's `+0x2A` word has bit 15 set (`0x2A4FC`'s own
  gate, its row `motion_step`); which mode-`0x25` records have it set was not traced. Where it is set the
  old port integrated that record's velocity once less per frame than the original, and the x clamp that
  follows read the x from before the step.
- **Not built alone.** Commit `fe41d15f` (and `7584f91e`) were not compiled in isolation; the tree was
  built with the row's seams on top.

### §3.3 `fight_384f8` (`0x384F8`): AL after `0x3C59C` (commit `7584f91e`)

- **Raw.** `0x38505` `call 0x3c59c`, `0x3850A` `test al,al`, `0x3850C` `jne 0x38526`.
- **Old port.** `if (fighter_pass_flag(3u, side) == 0)` on the whole `int`.
- **Measured before the fix** (`a2`: the stub returns `0x100`; `a3`: `0xFFFFFF00`): `a2: byte 0x107838:
  original 0x00, port unchanged`, `a2: byte 0x107839: original 0x00, port unchanged` (the `+0x88` word
  `0xFFFF` wraps to 0 in the original only), `a3: byte 0x1078CC: original 0x35, port unchanged`.
- **New port.** `if ((u8)fighter_pass_flag(3u, side) == 0u)`.
- **Mutant.** `@eax` (the old code), caught by `a2`, `a3`.
- **Ordinary play.** Not reached: `0x3C59C` ends `mov al,1` (`0x3C5B8`) or `xor al,al` (`0x3C5C7`) and
  its port returns 0 or 1.

### §3.4 `fighter_think_side` (`0x3B464`): AL after `0x3C59C` and `0x3B298` (commit `1fce2b23`)

- **Raw.** `0x3B47F` `call 0x3c59c`, `0x3B484` `test al,al`, `0x3B486` `jne 0x3b6bc`; `0x3B4C7` `call
  0x3b298`, `0x3B4CC` `test al,al`, `0x3B4CE` `jne 0x3b665`. d3c saw the first without reading the bytes
  (its §6): the raw does have `test al,al` there.
- **Old port.** `fighter_frame_flag(1u, ctx[1]) != 0` and `fighter_command_dispatch(ctx[1], stance) != 0`
  on the whole `int`.
- **Measured before the fix** (47 problem lines over `k1`, `k2`, `b2`): `k1: call #1: original
  0x3B298(0x0, 0x10), port none` (the stub returned `0x100`; the port returned at once); `b2: call #3:
  original 0x3A95C(0x1, 0x44), port 0x3AD98(0x1, 0xDE484, 0xA3B68, 0xA6908)` (the dispatch stub returned
  `0x100`; the port took the block arm).
- **New port.** `(u8)... != 0u` at both.
- **Mutants.** `@eaxflag` (`k1`, `k2`), `@eaxdisp` (`b2`).
- **Ordinary play.** Not reached: `0x3C59C` and `0x3B298` set AL to 0 or 1 and their ports return 0 or 1.
- **Not built alone**, as §3.2.

### Seen, not changed

- **`fighter_frame_flag`** (fighter.c, the static second C body of `0x3C59C`) shifts by `bit & 0xff`;
  the raw's `shl ebx,cl` (`0x3C5AC`) masks the count to 5 bits, as the exported body `fighter_pass_flag`
  does (`bit & 0x1F`). The audit's reading is right. A count of 32 or more is undefined in C, so no row
  can pin what the port does there, and its two call sites pass the constants 1 (`0x3B47F`) and 0
  (`0x469BB`): not reached, not rowed, not changed. (The clean fix is to drop the second body for
  `fighter_pass_flag`; d3c's branch adds a seam line to it, so it is left to the integrator.)
- **`fighter_38434`'s AL test** (`0x384AA`) cannot differ in the port: `hit_3d004` returns `u8`.

## §4 Named gaps and unhit blocks

- **`camera_y_clamp`: `0x131A6` is dead.** `sar eax,0x10` (`0x13185`) leaves x below `0x8000`, so `jl
  0x131ac` (`0x131A4`) is always taken and the plain `fld` arm never runs.
- **`camera_y_clamp`: not observable, so not mutants.** (1) A snap at \|diff\| < `0x100` instead of <=: a
  `0x100` step from a difference of exactly `0x100` lands on the target. (2) The cap stored on `<=`: it
  stores the value already there. (3) (d * d) * C: the same integer for every x. (4) The port's
  `diff < 0 ? -diff : diff` and `target - cam` are undefined in C when the difference is `0x80000000`
  (`s6`); the build under test gives the raw's answer (snap). Not changed.
- **`camera_y_clamp`: the x87 claim's limit** is record P3 §P3.6's: a control-word change outside the
  code object is not excluded.
- **`fight_384f8`: `0x34038` runs real.** Its stub would declare the clobbers `(esi, edi, ebp)` (what
  `E.callee_clobbers` derives, from its callee `0x29BC8`), and `0x384F8` keeps the record in ESI across
  it (`0x38548` load, `0x38570` read): the poison would fault the original side. Its own bytes write
  only EAX, EBX, ECX, EDX and save the last three, so it is run real (`E.Call(0x34038, ("eax",),
  mode="real")`: its argument is recorded and its bytes run), with the own slot's `+0x8C` timer word 3
  or 4, never 1 (`cmp eax,1; jne` `0x34051/0x34054`): it returns at once on both sides. Its timer arm is
  its own row's (`fighter_34038`).
- **`fight_384f8`: the four state callees are stubs that write nothing** in the first row, so what
  `0x34038`..`0x38434` do to the slot and record is not this row's; the chain row covers the re-reads
  after `0x2A4FC` and `0x188AC` only.
- **`fight_384f8_chain`'s unhit block** `0x3850E` (the AL == 0 increment) is the first row's (`a0 a2 a3`).
- **`fighter_38434`: the hook.** Only two registered callbacks are run (`0x23130`, `0x47E9C`); the row
  shows the three registers of the call and that the two stores precede it, not what another hook does.
  A hook address the port has not registered resolves to no call in the port (`fn_resolve`), which the
  row does not exercise.

- **`fighter_think_side`: the hook runs real.** A stub of `0x29D04` would carry the scan's clobbers
  `(ebx, ecx, edx)` (its second `ret` has `mov eax,eax` between the pops and itself), and `0x3B464` keeps
  the side in ECX across the hook for the `+0x48 == 8` arm; d3c's branch declares it `()` with a
  `_CALLEE_CLOBBER_FIXES` entry. To depend on neither, the row runs `0x29D04` on both sides (its gate byte
  `0x9AF3D` gives 0, else its `0x2A17C` call, a stub, and 1). Consequence, named: the hook's EAX is only
  ever 0 or 1, so the raw's whole-EAX test (`0x3B51E`) is not separated from an AL test, and no other hook
  address is exercised.
- **`fighter_think_side`: the stubs write nothing**, so a port that kept the side's `+0x54`, the
  thrower's `+0x64` or the `+0x90` byte from before `0x39834`/`0x3A95C` (the raw reads them again at
  `0x3B558`, `0x3B5B2`, `0x3B5C9`, `0x3B5EE`) is not separated; the port does read them again. The first
  triple lives in the raw's frame (`[esp+0x24]`) and in a C array in the port: it is compared only through
  `0x3B080`'s arguments and `0x3AD98`'s by-value triple.
- **`fighter_think_side`: a stance of `0x40` or more** takes `0x3AFC4`'s fatal `0x62003` in the raw;
  not cased (`0xFF` returns before it).

## §5 Store-sweep findings

- `camera_y_clamp`: its stores are the stepper's and the cap's dword at `0xF0AEC`. The camera block
  `0xF0AEC..0xF0AFF` is poked whole and the dword before it seeded; `0x1078F0..0x1078F7`, `0x104AF8..0x104AFF`
  and the cap table words on both sides of the selected dword are seeded. `@storew`/`@capw` (word
  stores) are caught where the high word changes (`g4 s6 s7 c6`, `c5`), `@nocap` by `c0 c3 c4 c5 c7`,
  `@noshake` by nine cases. First run: `@snaplt` was not detected (not observable, §4); replaced by
  `@snapwide`/`@snapnarrow` with the cases `s8`, `s9`. `f3` first seeded the camera `0x100` below the
  target, where the old port's 17603 stepped to the same 17602: reseeded 5 above.
- `fight_384f8`: both own slots' `+0x86..+0x8F`, the scratch slots' `+0x24..+0x2F`, the record's
  `+0x14..+0x23`, `0xBE014..0xBE01F`, `0x1088D3..0x1088D5` and `0x1078FA..0x1078FC` are seeded. The three
  stores have dropping, widening and misplacing mutants: the `+0x88` word (`@noinc`, `@incd`: the carry
  into `+0x8A` in `a2`, `@inc8a`, `@incptr`), `0x1078FB` (`@nofb`, `@fbw`, `@fbbit`, `@fbinv`, `@fbbase`),
  slot`+0x28` (`@no28`, `@28w`, `@28y`, `@late28`).
- `fighter_38434`: the scratch slot's `+0x08..+0x13`, `+0x40..+0x44`, `+0x52..+0x57`, `+0x72..+0x77`, both
  own slots' same fields (with a state of another arm), `0x1088BB..0x1088BF` and `0x1078EF..0x1078F1` are
  seeded. Each store has a dropping mutant (`@c4noinc`, `@c7no41`, `@c7no43`, `@c8noinc`, `@c8noand`,
  `@no74`, `@no56`; the flag clear by `@always`/`@clrother`), the widths `@c4w` (the carry into `+0x57`:
  `t4`) and `@b74`, the order against the calls `@early` and `@c7late` (the memory at the call).

- `fighter_think_side`: both slots' `+0x08`, `+0x10..+0x1F`, `+0x28..+0x33`, `+0x40..+0x7F` and
  `+0x88..+0x93` are seeded with bytes that are none of the stored values, the thrower's with the other
  answer for every field the function reads. Each store has a dropping mutant and a wrong-slot one where
  the function has the choice: `+0x67` (`@no67`, `@67other`), the thrower's `+0x8A` (`@no8a`, `@8aself`),
  `+0x14` (`@hooknoclr`, `@hookclr`), `+0x18`/`+0x1C` on both paths (`@c5no18`, `@c5no1c`, `@midno18`,
  `@midno1c`), `+0x41` (`@no41`), the thrower's `+0x64` (`@no64`, `@64self`).

## §6 Queue entries not done

| entry | port function | why |
|---|---|---|
| `0x3C88C` (d3c, 730) | `hit_slot_step` (fighter.c) | Not reached in the session; nothing read. |
| `0x1958C` (d3c, 464) | `fighter_pass_a` (fighter.c) | Not reached; nothing read. `0x193B0` still has no seam on this branch. |
| `0x33B00` (d3c, 279) | fighter.c | Not reached; nothing read. It calls the named non-row `0x2EA30`. |
| `0x2BAF4` (d3d, 316) | actors.c | Not reached; nothing read. |
| `0x31A78` (d3d, 137) | svcmenu.c | Not reached; nothing read. |
| `0x3157C` (d3d, 1057) | `config_key_name` (config.c) | Not reached; nothing read. |

Done: `0x1317C` (d3d's finding), `0x384F8` (two rows) and `0x38434` (the coordinator's item), `0x3B464`
(d3c's queue): 5 rows, 98 cases, 135 mutants, 4 corrections in 4 commits.

## §7 Counters, seams and harness lines

- **The one full self-check** (`PR_DIFF_JOBS=4 python3 tools/diff_verify.py --image /tmp/pr_d3e_img.bin
  --self-check`, on commit `33c0301f`'s tree): `rc 0`, `diff-verify: 657/657 functions VERIFIED; 5785/5785
  mutants detected; 1 named gaps; 428/490 rows with callees closed (167 have none)` (the branch point:
  652 rows, 5650 mutants, 428/485; + 5 rows, + 135 mutants).
- **Seams added** (the first statement of each): fighter.c `PR_SEAM0(0x186C4u);`
  (`fighter_slot_latch_both`), `PR_SEAM(0x38D24u, side);` (`fighter_38d24`), `PR_SEAM(0x34038u, side);`
  (`fighter_34038`; d3c's line), `PR_SEAM(0x382C4u, side);` (`fighter_382c4`), `PR_SEAM(0x38434u, side);`
  (`fighter_38434`), `PR_SEAM(0x1922Cu, side);` (`hit_stance_timer`; d3c's line), `PR_SEAM_RET(0x3D004u,
  side);` (`hit_3d004`), `PR_SEAM_RET(0x3C59Cu, bit, index);` (`fighter_frame_flag`; d3c's line),
  `PR_SEAM(0x235C4u, side);` (`fighter_235c4`); actors.c `PR_SEAM(0x2A4FCu, rec);` (`motion_step`; d3d's
  line). The existing rows that name any of these addresses were re-run and are unchanged
  (`hit_reaction_apply`, `hit_stance_timer`, `fighter_slot_latch_both`, `fighter_pass_b`,
  `fighter_34038`, `fighter_38d24`, `motion_step`, `fighter_39ff4`, `fighter_state_35f84`,
  `fighter_reaction`, `fighter_235c4`).
- **Linkage.** `camera_y_clamp` (camera.c/.h), `fighter_382c4` and `hit_3d004` (the same line changes as
  d3c's), `fighter_38434` and `fighter_think_side` lose `static`; the declarations are one block in
  `fighter.h` after `fight_384f8` and one in `camera.h`.
- **Harness lines outside the blocks.** One inserted line in the expected dictionary of
  `test_each_stub_declares_the_registers_its_callee_clobbers`, ending `# track D3 batch d3e`: `0x38D24:
  (), 0x382C4: (), 0x38434: ("edi", "ebp"), 0x2A4FC: ("ebp",), 0x186C4: ("ebx", "ecx", "edx", "esi",
  "edi", "ebp"), 0x1922C: (), 0x3D004: ("edi", "ebp"), 0x235C4: ("ebp",)`. No `tools/diff_emu.py` change.
- **Trial merges** (`git merge-tree`, nothing checked out): `port/src` merges without conflict against
  `reverse-d3a`, `-d3b`, `-d3c` and `-d3d`; the three harness files conflict where every batch inserts at
  the same anchor (additive).
- **Not run by this batch:** the Python unittest, `make verify`, `make diff-verify`, any `*-oracle`
  target, DOSBox-X. The four corrections are unproven against the oracles.
