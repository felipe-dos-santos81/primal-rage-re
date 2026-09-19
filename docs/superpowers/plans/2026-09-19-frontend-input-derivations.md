# Task 3 derivations — selector `0x11F6C`, membership `0x1C6D4`, iterator `0x33904`

Raw bytes: `data/game/C/PRAGE.EXE`. For object 0 (code) the file offset of a
linear address is `file_offset = va + 0x52E54`; for object 1 (data) it is the
page-mapped offset the LE loader computes (verified against the loaded image
with a throwaway probe linking `port/src/mem.c`).

The disassembly below is the plan's Step 1 command output, run before any code
was written.

```
=== 0x11f6c
0x11f6c 53 push ebx
0x11f6d 51 push ecx
0x11f6e 52 push edx
0x11f6f a06f0a0700 mov al, byte ptr [0x70a6f]
0x11f74 3c04 cmp al, 4
0x11f76 0f871e020000 ja 0x1219a
0x11f7c 25ff000000 and eax, 0xff
0x11f81 2eff2485581f0000 jmp dword ptr cs:[eax*4 + 0x1f58]
0x11f89 e8d27d0100 call 0x29d60
0x11f8e b801000000 mov eax, 1
0x11f93 e85c9b0100 call 0x2baf4
0x11f98 31c9 xor ecx, ecx
0x11f9a 31db xor ebx, ebx
0x11f9c 31c0 xor eax, eax
0x11f9e 31d2 xor edx, edx
0x11fa0 e82bd20300 call 0x4f1d0
0x11fa5 b8bc870400 mov eax, 0x487bc
0x11faa e8696b0200 call 0x38b18
0x11faf b801000000 mov eax, 1
0x11fb4 e8b3a00100 call 0x2c06c
0x11fb9 30c9 xor cl, cl
0x11fbb 31d2 xor edx, edx
0x11fbd 880d6e0a0700 mov byte ptr [0x70a6e], cl
0x11fc3 8a2d29450800 mov ch, byte ptr [0x84529]
0x11fc9 8915440a0700 mov dword ptr [0x70a44], edx
0x11fcf f6c502 test ch, 2
0x11fd2 7406 je 0x11fda
0x11fd4 8915400a0700 mov dword ptr [0x70a40], edx
0x11fda b801000000 mov eax, 1
0x11fdf e8109b0100 call 0x2baf4
0x11fe4 31c9 xor ecx, ecx
0x11fe6 31db xor ebx, ebx
0x11fe8 31c0 xor eax, eax
0x11fea 31d2 xor edx, edx
0x11fec e8dfd10300 call 0x4f1d0
0x11ff1 b8bc870400 mov eax, 0x487bc
0x11ff6 e81d6b0200 call 0x38b18
0x11ffb 31c0 xor eax, eax
0x11ffd a06e0a0700 mov al, byte ptr [0x70a6e]
0x12002 8d88e0000000 lea ecx, [eax + 0xe0]
0x12008 89c3 mov ebx, eax
0x1200a c1e002 shl eax, 2
0x1200d 6a00 push 0
0x1200f 29d8 sub eax, ebx
0x12011 31d2 xor edx, edx
0x12013 bb00060000 mov ebx, 0x600
0x12018 8b0485e0ae0100 mov eax, dword ptr [eax*4 + 0x1aee0]
0x1201f e8f08d0100 call 0x2ae14
0x12024 a3440a0700 mov dword ptr [0x70a44], eax
0x12029 31c0 xor eax, eax
0x1202b e8d4180200 call 0x33904
0x12030 89c3 mov ebx, eax
0x12032 85c0 test eax, eax
0x12034 7424 je 0x1205a
0x12036 89d8 mov eax, ebx
0x12038 e897a60000 call 0x1c6d4
0x1203d 84c0 test al, al
0x1203f 750c jne 0x1204d
0x12041 ba01000000 mov edx, 1
0x12046 89d8 mov eax, ebx
0x12048 e8db1d0000 call 0x13e28
0x1204d 89d8 mov eax, ebx
0x1204f e8b0180200 call 0x33904
0x12054 89c3 mov ebx, eax
0x12056 85c0 test eax, eax
0x12058 75dc jne 0x12036
0x1205a f6052945080002 test byte ptr [0x84529], 2
0x12061 742b je 0x1208e
0x12063 6a00 push 0
0x12065 31c0 xor eax, eax
0x12067 b9ff000000 mov ecx, 0xff
0x1206c a06e0a0700 mov al, byte ptr [0x70a6e]
0x12071 bb00340000 mov ebx, 0x3400
0x12076 ba002a0000 mov edx, 0x2a00
0x1207b 8b0485c8ae0100 mov eax, dword ptr [eax*4 + 0x1aec8]
0x12082 e88d8d0100 call 0x2ae14
0x12087 a3400a0700 mov dword ptr [0x70a40], eax
0x1208c eb6e jmp 0x120fc
0x1208e 31db xor ebx, ebx
0x12090 8a1d6e0a0700 mov bl, byte ptr [0x70a6e]
0x12096 8d049d00000000 lea eax, [ebx*4]
0x1209d 29d8 sub eax, ebx
0x1209f b903400000 mov ecx, 0x4003
0x120a4 8b0485e4ae0100 mov eax, dword ptr [eax*4 + 0x1aee4]
0x120ab ba18000000 mov edx, 0x18
0x120b0 e84ba40000 call 0x1c500
0x120b5 89c3 mov ebx, eax
0x120b7 b8ffffffff mov eax, 0xffffffff
0x120bc e8d7d00100 call 0x2f198
0x120c1 31db xor ebx, ebx
0x120c3 8a1d6e0a0700 mov bl, byte ptr [0x70a6e]
0x120c9 8d049d00000000 lea eax, [ebx*4]
0x120d0 29d8 sub eax, ebx
0x120d2 c1e002 shl eax, 2
0x120d5 8b88e8ae0100 mov ecx, dword ptr [eax + 0x1aee8]
0x120db 85c9 test ecx, ecx
0x120dd 741d je 0x120fc
0x120df 89c8 mov eax, ecx
0x120e1 ba1b000000 mov edx, 0x1b
0x120e6 e815a40000 call 0x1c500
0x120eb b903400000 mov ecx, 0x4003
0x120f0 89c3 mov ebx, eax
0x120f2 b8ffffffff mov eax, 0xffffffff
0x120f7 e89cd00100 call 0x2f198
0x120fc b402 mov ah, 2
0x120fe b204 mov dl, 4
0x12100 b95a000000 mov ecx, 0x5a
0x12105 8825700a0700 mov byte ptr [0x70a70], ah
0x1210b 88156f0a0700 mov byte ptr [0x70a6f], dl
0x12111 66890d680a0700 mov word ptr [0x70a68], cx
0x12118 5a pop edx
0x12119 59 pop ecx
0x1211a 5b pop ebx
0x1211b c3 ret
0x1211c 8a156e0a0700 mov dl, byte ptr [0x70a6e]
0x12122 31c0 xor eax, eax
0x12124 fec2 inc dl
0x12126 88d0 mov al, dl
0x12128 88156e0a0700 mov byte ptr [0x70a6e], dl
0x1212e 83f806 cmp eax, 6
0x12131 751b jne 0x1214e
0x12133 c605700a070003 mov byte ptr [0x70a70], 3
0x1213a c6056f0a070004 mov byte ptr [0x70a6f], 4
0x12141 66c705680a07001e00 mov word ptr [0x70a68], 0x1e
0x1214a 5a pop edx
0x1214b 59 pop ecx
0x1214c 5b pop ebx
0x1214d c3 ret
0x1214e c6056f0a070001 mov byte ptr [0x70a6f], 1
0x12155 5a pop edx
0x12156 59 pop ecx
0x12157 5b pop ebx
0x12158 c3 ret
0x12159 a1440a0700 mov eax, dword ptr [0x70a44]
0x1215e ba03000000 mov edx, 3
0x12163 e8e88f0100 call 0x2b150
0x12168 30e4 xor ah, ah
0x1216a 668915640a0700 mov word ptr [0x70a64], dx
0x12171 88256f0a0700 mov byte ptr [0x70a6f], ah
0x12177 5a pop edx
0x12178 59 pop ecx
0x12179 5b pop ebx
0x1217a c3 ret
0x1217b 66a1680a0700 mov ax, word ptr [0x70a68]
0x12181 89c3 mov ebx, eax
0x12183 4b dec ebx
0x12184 66891d680a0700 mov word ptr [0x70a68], bx
0x1218b 6685c0 test ax, ax
0x1218e 7f0a jg 0x1219a
0x12190 a0700a0700 mov al, byte ptr [0x70a70]
0x12195 a26f0a0700 mov byte ptr [0x70a6f], al
0x1219a 5a pop edx
0x1219b 59 pop ecx
0x1219c 5b pop ebx
0x1219d c3 ret
=== 0x1c6d4
0x1c6d4 8b00 mov eax, dword ptr [eax]
0x1c6d6 3d94998000 cmp eax, 0x809994
0x1c6db 722c jb 0x1c709
0x1c6dd 0f8649000000 jbe 0x1c72c
0x1c6e3 3da4998000 cmp eax, 0x8099a4
0x1c6e8 7215 jb 0x1c6ff
0x1c6ea 7640 jbe 0x1c72c
0x1c6ec 3dac998000 cmp eax, 0x8099ac
0x1c6f1 723c jb 0x1c72f
0x1c6f3 7637 jbe 0x1c72c
0x1c6f5 3dcc998000 cmp eax, 0x8099cc
0x1c6fa 7430 je 0x1c72c
0x1c6fc 30c0 xor al, al
0x1c6fe c3 ret
0x1c6ff 3d9c998000 cmp eax, 0x80999c
0x1c704 7426 je 0x1c72c
0x1c706 30c0 xor al, al
0x1c708 c3 ret
0x1c709 3d7c998000 cmp eax, 0x80997c
0x1c70e 7215 jb 0x1c725
0x1c710 761a jbe 0x1c72c
0x1c712 3d84998000 cmp eax, 0x809984
0x1c717 7216 jb 0x1c72f
0x1c719 7611 jbe 0x1c72c
0x1c71b 3d8c998000 cmp eax, 0x80998c
0x1c720 740a je 0x1c72c
0x1c722 30c0 xor al, al
0x1c724 c3 ret
0x1c725 3d5c998000 cmp eax, 0x80995c
0x1c72a 7503 jne 0x1c72f
0x1c72c b001 mov al, 1
0x1c72e c3 ret
0x1c72f 30c0 xor al, al
0x1c731 c3 ret
```

## Base-fixup rule

Raw absolute displacements below an object's mapped base are link-time offsets.
The LE loader adds the target object's base (`obj0 = 0x10000`, `obj1 = 0x80000`),
so data references below `0x80000` fix to `+0x80000`. Confirmed against the
loaded image (probe linking `port/src/mem.c`):

| raw displacement | fixed linear (loaded mem) | note |
|---|---|---|
| `0x1aee0` | `0x9AEE0` | entry descriptor table, stride 12 |
| `0x1aee4` | `0x9AEE4` | entry string-id table, stride 12 |
| `0x1aee8` | `0x9AEE8` | entry second string-id table, stride 12; has a generated symbol `DS_0009AEE8` |
| `0x1aec8` | `0x9AEC8` | sprite-alternate descriptor table, stride 4 |
| `0x487bc` | **`0xC87BC`** | the select state's `0x38B18` descriptor |
| `0x1ac1c` | `0x9AC1C` | the title's descriptor (already used by `game_state_title`) |
| `0x84529` | `0x104529` | `DS_00104528 + 1` |
| `0x70a6e/6f/70/68/44/40` | `DS_000F0A6E/6F/70/68/44/40` | |

`0x9AEE0`, `0x9AEE4`, `0x9AEC8` have no generated symbol, so the port uses the
raw literals `0x9AEE0u`, `0x9AEE4u`, `0x9AEC8u` with address-tag comments.

The descriptor at `mem + 0xC87BC` is real: its first dword is `0x3F11` (the
title's `mem + 0x9AC1C` first dword is `0x2BEF`).

## Phase dispatch and case boundaries

Entry tests `DSB(DS_000F0A6F)` (`al`), `cmp al,4; ja 0x1219a` (default returns),
masks to 8 bits, then `jmp cs:[eax*4 + 0x1f58]`. The table's displacement
`0x1f58` is below the code base, so it fixes to `0x11F58` (file offset
`0x64DAC`). Loaded table (raw dword values, each +`0x10000` at run time):

| index | raw value | linear target | meaning |
|---|---|---|---|
| 0 | `0x00001F89` | `0x11F89` | phase 0 |
| 1 | `0x00001FDA` | `0x11FDA` | phase 1 |
| 2 | `0x0000211C` | `0x1211C` | phase 2 |
| 3 | `0x00002159` | `0x12159` | phase 3 |
| 4 | `0x0000217B` | `0x1217B` | phase 4 |

Case 0 **falls through into case 1**: phase 0 ends at `0x11FD4`
(`mov [0x70a40],edx`) and phase 1 begins at `0x11FDA` with no `jmp` between, so
the same call runs phase 0 then phase 1.

## Register arguments of every call

Watcom register passing: first argument `eax`, then `edx`, `ebx`, `ecx`; extra
arguments on the stack.

### Phase 0 (`0x11F89`)

| call | register setup | port call |
|---|---|---|
| `0x29D60` | none | omitted (ret-only no-op) |
| `0x2BAF4` | `eax=1` | `actors_reset()` |
| `0x4F1D0` | `eax=edx=ebx=ecx=0` | `title_input_reset()` |
| `0x38B18` | `eax=0x487BC` (→`mem+0xC87BC`), `edx=0`, `ebx=0` | `title_spawn_row(desc,0,0)` |
| `0x2C06C` | `eax=1` | `config_set_credit_row(1u)` |

Then `DSB(DS_000F0A6E)=0` (`cl=0`), `DSD(DS_000F0A44)=0` (`edx=0`); if
`(DSB(DS_00104528+1) & 2) != 0` then `DSD(DS_000F0A40)=0`. Falls through.

### Phase 1 (`0x11FDA`)

| call | register setup | port call |
|---|---|---|
| `0x2BAF4` | `eax=1` | `actors_reset()` |
| `0x4F1D0` | `eax=edx=ebx=ecx=0` | `title_input_reset()` |
| `0x38B18` | `eax=0x487BC`, `edx=ebx=0` | `title_spawn_row(desc,0,0)` |
| `0x2AE14` | `edx=0`; `ecx=0xE0+n`; `ebx=0x600`; stack `0`; `eax=DSD(0x1AEE0 + 12n)` | `actor_spawn(desc,0,0xE0+n,0x600,0)` stored to `DSD(DS_000F0A44)` |
| `0x33904` | `eax=0` | `frontend_list_next(0)` |
| `0x1C6D4` | `eax=node` | `frontend_resource_known(node)` (raw order: `0x33904` first, then `0x1C6D4`) |
| `0x13E28` | `edx=1`, `eax=node`, only when unknown | `effects_spawn_pulse(node,1u)` |

`n = DSB(DS_000F0A6E)`; the descriptor arithmetic `shl eax,2; sub eax,ebx` is
`4n - n = 3n`, then `[eax*4 + 0x1AEE0]` is `DSD(0x1AEE0 + 12n)`.

Branch on `(DSB(DS_00104528+1) & 2)`:

- set (`0x12063`): `edx=0x2A00`, `ecx=0xFF`, `ebx=0x3400`, stack `0`,
  `eax=DSD(0x1AEC8 + 4n)` → `actor_spawn(desc,0x2A00,0xFF,0x3400,0)` stored to
  `DSD(DS_000F0A40)`; `jmp 0x120FC`.
- clear (text branch, `0x1208E`):
  - `ecx=0x4003`, `edx=0x18`, `eax=DSD(0x1AEE4 + 12n)`; `call 0x1C500`
    (`game_string_get(id)`), result `eax`→`ebx`; `eax=0xFFFFFFFF`;
    `call 0x2F198` → `text_cursor_set(-1, 0x18, str, 0x4003)`.
  - `ecx=DSD(0x1AEE8 + 12n)`; `test ecx,ecx; je done`; `eax=ecx`, `edx=0x1B`;
    `call 0x1C500`; `ecx=0x4003`, `ebx=eax`, `eax=0xFFFFFFFF`;
    `call 0x2F198` → `text_cursor_set(-1, 0x1B, str2, 0x4003)`.

Tail (`0x120FC`): `ah=2`, `dl=4`, `ecx=0x5A`; `[0x70A70]=ah` (2),
`[0x70A6F]=dl` (4), `word [0x70A68]=cx` (0x5A).

### Phase 2 (`0x1211C`)

`dl = DSB(DS_000F0A6E); al = dl + 1; DSB(DS_000F0A6E) = dl; cmp eax,6`.
If `== 6`: `DSB(DS_000F0A70)=3`, `DSB(DS_000F0A6F)=4`, `DSW(DS_000F0A68)=0x1E`;
else `DSB(DS_000F0A6F)=1`.

### Phase 3 (`0x12159`)

`eax=DSD(DS_000F0A44)`, `edx=3`, `call 0x2B150`, then `xor ah,ah`,
`word [0x70A64]=dx` (literal 3), `byte [0x70A6F]=ah` (0).

### Phase 4 (`0x1217B`)

`ax = word [0x70A68]` (original), `ebx=eax`, `dec ebx`,
`word [0x70A68]=bx`, `test ax,ax; jg 0x1219A` (skip). Otherwise
`DSB(DS_000F0A6F) = DSB(DS_000F0A70)`. **The test reads the original value, so
the transition fires when the original is `<= 0` (signed).** The plan's
`if (--DSW(DS_000F0A68) < 1)` fires one frame early; see errata below.

## `0x1C6D4` membership (`0x6F528`)

`mov eax,[eax]` first: it dereferences its argument (a linear `mem[]` address).
The tree accepts exactly nine dwords:

`0x80995C 0x80997C 0x809984 0x80998C 0x809994 0x80999C 0x8099A4 0x8099AC 0x8099CC`

The `cmp`/`jb`/`jbe` offsets (addresses; add `0x52E54` for file offsets):

| cmp | jb | jbe/je | target |
|---|---|---|---|
| `0x1C6D6 cmp 0x809994` | `0x1C6DB jb 0x1C709` | `0x1C6DD jbe 0x1C72C` accept | lower half |
| `0x1C6E3 cmp 0x8099A4` | `0x1C6E8 jb 0x1C6FF` | `0x1C6EA jbe 0x1C72C` accept | |
| `0x1C6EC cmp 0x8099AC` | `0x1C6F1 jb 0x1C72F` reject | `0x1C6F3 jbe 0x1C72C` accept | |
| `0x1C6F5 cmp 0x8099CC` | | `0x1C6FA je 0x1C72C` accept | |
| `0x1C6FF cmp 0x80999C` | | `0x1C704 je 0x1C72C` accept | |
| `0x1C709 cmp 0x80997C` | `0x1C70E jb 0x1C725` | `0x1C710 jbe 0x1C72C` accept | |
| `0x1C712 cmp 0x809984` | `0x1C717 jb 0x1C72F` reject | `0x1C719 jbe 0x1C72C` accept | |
| `0x1C71B cmp 0x80998C` | | `0x1C720 je 0x1C72C` accept | |
| `0x1C725 cmp 0x80995C` | | `0x1C72A jne 0x1C72F` reject | lowest |

Reject (`xor al,al; ret`) at `0x1C6FC`, `0x1C706`, `0x1C722`, `0x1C72F`;
accept (`mov al,1; ret`) at `0x1C72C`.

## Literal-3 proof for phase 3

- `0x1215E` (`mov edx, 3`, file offset `0x64FB2`, bytes `ba03000000`).
- `0x2B150` preserves the caller's `edx`: it uses `edx` internally
  (`mov edx,eax` at `0x2B153`) but saves it with `push edx` at `0x2B152`
  (file `0x7DFA6`) and restores it with `pop edx; pop ecx; pop ebx; ret` at
  `0x2B1E0` (file `0x7E034`).
- `0x1216A` (`mov word [0x70A64], dx`, file offset `0x64FBE`) stores `dx`, so
  `DSW(DS_000F0A64) = 3`.

## Loaded entry tables (probe against `mem_load_le`)

For `n = 0..5`:

| n | `desc = DSD(0x9AEE0+12n)` | `str1 = DSD(0x9AEE4+12n)` | `str2 = DSD(0x9AEE8+12n)` | `alt = DSD(0x9AEC8+4n)` |
|---|---|---|---|---|
| 0 | `0x9AD9C` | 9 | 0xA | `0x9AE50` |
| 1 | `0x9ADB0` | 0xB | 0xC | `0x9AE64` |
| 2 | `0x9ADC4` | 0xD | 0xE | `0x9AE78` |
| 3 | `0x9ADD8` | 0xF | 0x10 | `0x9AE8C` |
| 4 | `0x9ADEC` | 0x11 | 0x12 | `0x9AEA0` |
| 5 | `0x9AE28` | 0x13 | 0x14 | `0x9AEB4` |

`DSB(DS_00104528 + 1) = 0x00` on the shipped image, so the text branch runs and
phase 0 does not clear `DS_000F0A40`.

## Frame-count arithmetic

State 2 is entered at phase 0. Iterations below are 0-based; iteration `i` is
the `(i+1)`-th presented frame.

- Iteration 0: phase 0 falls into phase 1, draws entry 0, sets phase 4 and
  `DSW(DS_000F0A68)=0x5A`.
- Phase 4 with count `0x5A`: raw tests the original value, so it takes 91
  iterations (originals `90..0`; the call whose original is `0` transitions).
  Then one phase-2 iteration advances and sets phase 1, then the next iteration
  draws. That is a 93-iteration cycle per entry.
- Entry `k` is drawn on iteration `93k`: `0, 93, 186, 279, 372, 465`.
- After entry 5 at iteration 465: 91 phase-4 iterations (`466..556`), one
  phase-2 iteration (`557`, `n=6`, count `0x1E`), 31 phase-4 iterations
  (`558..588`; originals `30..0`), then iteration `588` is phase 3, which sets
  `DSW(DS_000F0A64)=3`.
- State 3 is therefore reached on frame **589** (iteration 588). The 640-frame
  window of the driver covers it with margin, and all six entries are seen
  (`seen_entries == 0x3F`).

Measured log transitions (deterministic, `select.log`): phase 4 at `i=0`,
phase 2 at `i=91,184,277,370,463,556`, first phase 3 at `i=588`, phase 0 at
`i=589`. Under the plan's off-by-one phase-4 test the first phase 3 moved to
`i=581`, which is how the discrepancy was caught.

## Where the raw contradicted the plan

1. The plan's Step 4 test passed host pointers; `0x1C6D4` dereferences its
   argument, so the test must use a scratch linear address inside `mem[]`.
2. The plan declared the helpers `static` while its own test calls them from
   another translation unit; they are exported instead.
3. The plan's phase-0/1 `0x38B18` descriptor was `mem + 0x9AC1C`; the raw
   immediate is `0x487BC`, which fixes to `mem + 0xC87BC`.
4. The plan's phase-0 `0x2C06C` argument marker resolved to row **1**, not 0.
5. The plan's `0x2AE14` call had placeholder arguments; the raw passes
   `(desc, 0, 0xE0+n, 0x600, 0)` (phase 1) and `(desc, 0x2A00, 0xFF, 0x3400, 0)`
   (`&2` arm).
6. The plan's `effects_spawn_pulse` placeholder argument is **1**.
7. The plan's phase-4 countdown `--count < 1` is **not** equivalent to the raw
   `test original; jg`: it fires one frame early. The port tests the original
   value (`v <= 0`). The errata's claim of equivalence is itself wrong; the raw
   bytes govern.
8. The plan expected Step 3 to fail to compile, but `test_frontend.c` is not in
   `run_tests` until Step 8, so the failure only appears once the test is
   registered. The report records the actual failing build after registering the
   test and before the implementation (undeclared `frontend_resource_known` /
   `frontend_list_next`).
