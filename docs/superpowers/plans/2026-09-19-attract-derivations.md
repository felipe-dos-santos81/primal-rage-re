# Attract scroll/zoom projection `0x389C4` / `0x38A38` — raw-byte derivation (Task 2)

Register-level derivation of the attract scene's scroll/zoom projection from the
shipped `data/game/C/PRAGE.EXE` (capstone 32-bit, GNU syntax). Object-0 code maps
as `file_offset = va + 0x52E54`; a `DS_*` immediate `I` is linear `I + 0x80000`.

Reproduction (read-only):

```python
from capstone import *
d = open('data/game/C/PRAGE.EXE','rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_32)
for va, n in ((0x389c4, 113), (0x38a38, 224)):
    print('===', hex(va))
    for i in md.disasm(d[va + 0x52E54: va + 0x52E54 + n], va):
        print(hex(i.address), i.bytes.hex(), i.mnemonic, i.op_str)
```

---

## 1. The shared signed-divide-by-`2^n` idiom

Both functions divide by powers of two with this three-instruction sequence
(MSVC's signed division):

```
mov/sar edx, 31        ; edx = sign(v) in {0, -1}
shl edx, n             ; edx = 0 or -(1<<n); CF = 1 when edx was -1
sbb eax, edx           ; eax = v - edx - CF
sar eax, n             ; low 16 bits are the quotient
```

For `v >= 0`: `edx = 0`, `CF = 0`, result `v >> n`.
For `v < 0`: `edx = -(1<<n)`, `CF = 1`, result `(v + 2^n - 1) >> n`.

That is **truncation toward zero** — exactly C's `int` `/` operator. It is *not*
an arithmetic `>> n`, which floors. This is the transcription hazard the brief
names: `0x389C4`'s `>= 1` arm and every `/256`, `/32` in `0x38A38` use this
form. The port writes `/ 64`, `/ 256`, `/ 32` (with a `/* PORT: */` note) rather
than `>> n`, which is the same value for these operands but exercises the sign
correction.

## 2. `0x389C4` (file `0x8B818`, size 113)

```
0x389c4 53                     push ebx
0x389c5 52                     push edx
0x389c6 66833d3c7a080000       cmp  word ptr [0x87a3c], 0
0x389ce 7e3a                   jle  0x38a0a
0x389d0 31d2                   xor  edx, edx
0x389d2 a13a7a0800             mov  eax, dword ptr [0x87a3a]
0x389d7 668b15487a0800         mov  dx, word ptr [0x87a48]
0x389de c1f810                 sar  eax, 0x10
0x389e1 29c2                   sub  edx, eax
0x389e3 89d0                   mov  eax, edx
0x389e5 c1fa1f                 sar  edx, 0x1f
0x389e8 c1e206                 shl  edx, 6
0x389eb 1bc2                   sbb  eax, edx
0x389ed c1f806                 sar  eax, 6
0x389f0 668b1d4c7a0800         mov  bx, word ptr [0x87a4c]
0x389f7 66a3387a0800           mov  word ptr [0x87a38], ax
0x389fd 66a14a7a0800           mov  ax, word ptr [0x87a4a]
0x38a03 6639d8                 cmp  ax, bx
0x38a06 772a                   ja   0x38a32
0x38a08 eb22                   jmp  0x38a2c
0x38a0a 31d2                   xor  edx, edx
0x38a0c 668b15487a0800         mov  dx, word ptr [0x87a48]
0x38a13 89d0                   mov  eax, edx
0x38a15 c1fa1f                 sar  edx, 0x1f
0x38a18 c1e206                 shl  edx, 6
0x38a1b 1bc2                   sbb  eax, edx
0x38a1d c1f806                 sar  eax, 6
0x38a20 66a3387a0800           mov  word ptr [0x87a38], ax
0x38a26 66a14e7a0800           mov  ax, word ptr [0x87a4e]
0x38a2c 66a34c7a0800           mov  word ptr [0x87a4c], ax
0x38a32 5a                     pop  edx
0x38a33 5b                     pop  ebx
0x38a34 c3                     ret
```

Mapping:

- `cmp word [0x87a3c],0 ; jle` → signed `s16 DS_00107A3C <= 0`. The branch is
  taken for **negative as well as zero**; "`< 1`" is exact.
- `< 1` arm (`0x38a0a`): `DS_00107A48` is loaded zero-extended as a u16, so the
  signed idiom degenerates to `DS_00107A48 / 64`, stored to `DS_00107A38`;
  `uVar1 = DS_00107A4E`; fall through to `0x38a2c`.
- `>= 1` arm (`0x389d0`): `eax = dword [0x87a3a]` then `sar eax,0x10` — the high
  word of the dword at `0x87a3a` is the word at `0x87a3c`, i.e. the subtrahend is
  `(s16)DS_00107A3C`. `edx = (u16)DS_00107A48`, then `edx -= eax`, so
  `x = (u16)DS_00107A48 - (s16)DS_00107A3C`, `DS_00107A38 = x / 64`, and
  `uVar1 = DS_00107A4A`.
- `cmp ax,bx ; ja` is the **unsigned u16** compare `DS_00107A4A > DS_00107A4C`.
  If true, jump to the pop/ret and leave `DS_00107A4C` unchanged (early return).
- `0x38a2c`: `DS_00107A4C = uVar1` (the `< 1` arm always; the `>= 1` arm only
  when `DS_00107A4A <= DS_00107A4C`).

## 3. `0x38A38` (file `0x8B88C`, size 224)

```
0x38a38 53                     push ebx
0x38a39 51                     push ecx
0x38a3a 52                     push edx
0x38a3b 57                     push edi
0x38a3c 55                     push ebp
0x38a3d 83ec04                 sub  esp, 4
0x38a40 8b1df00a0700           mov  ebx, dword ptr [0x70af0]
0x38a46 31c9                   xor  ecx, ecx
0x38a48 c1e308                 shl  ebx, 8
0x38a4b 668b0d407a0800         mov  cx, word ptr [0x87a40]
0x38a52 89da                   mov  edx, ebx
0x38a54 89d8                   mov  eax, ebx
0x38a56 c1fa1f                 sar  edx, 0x1f
0x38a59 f7f9                   idiv ecx
0x38a5b 31c9                   xor  ecx, ecx
0x38a5d 668b0d527a0800         mov  cx, word ptr [0x87a52]
0x38a64 49                     dec  ecx
0x38a65 890424                 mov  dword ptr [esp], eax
0x38a68 85c9                   test ecx, ecx
0x38a6a 7c5d                   jl   0x38ac9
0x38a6c 668b3d4c7a0800         mov  di, word ptr [0x87a4c]
0x38a73 01cf                   add  edi, ecx
0x38a75 8d2c4d00000000         lea  ebp, [ecx*2]
0x38a7c 89da                   mov  edx, ebx
0x38a7e 89d8                   mov  eax, ebx
0x38a80 c1fa1f                 sar  edx, 0x1f
0x38a83 c1e208                 shl  edx, 8
0x38a86 1bc2                   sbb  eax, edx
0x38a88 c1f808                 sar  eax, 8
0x38a8b 89c2                   mov  edx, eax
0x38a8d 31c0                   xor  eax, eax
0x38a8f d1fa                   sar  edx, 1
0x38a91 6689f8                 mov  ax, di
0x38a94 66899500790800         mov  word ptr [ebp + 0x87900], dx
0x38a9b 3def000000             cmp  eax, 0xef
0x38aa0 7f19                   jg   0x38abb
0x38aa2 81c2002b0000           add  edx, 0x2b00
0x38aa8 89d0                   mov  eax, edx
0x38aaa c1fa1f                 sar  edx, 0x1f
0x38aad c1e205                 shl  edx, 5
0x38ab0 1bc2                   sbb  eax, edx
0x38ab2 c1f805                 sar  eax, 5
0x38ab5 66a33e7a0800           mov  word ptr [0x87a3e], ax
0x38abb 8b1424                 mov  edx, dword ptr [esp]
0x38abe 4f                     dec  edi
0x38abf 83ed02                 sub  ebp, 2
0x38ac2 49                     dec  ecx
0x38ac3 29d3                   sub  ebx, edx
0x38ac5 85c9                   test ecx, ecx
0x38ac7 7db3                   jge  0x38a7c
0x38ac9 31c0                   xor  eax, eax
0x38acb 8b0c24                 mov  ecx, dword ptr [esp]
0x38ace 66a1427a0800           mov  ax, word ptr [0x87a42]
0x38ad4 0fafc1                 imul eax, ecx
0x38ad7 29c3                   sub  ebx, eax
0x38ad9 89da                   mov  edx, ebx
0x38adb 89d8                   mov  eax, ebx
0x38add c1fa1f                 sar  edx, 0x1f
0x38ae0 c1e208                 shl  edx, 8
0x38ae3 1bc2                   sbb  eax, edx
0x38ae5 c1f808                 sar  eax, 8
0x38ae8 66a3467a0800           mov  word ptr [0x87a46], ax
0x38aee 89c2                   mov  edx, eax
0x38af0 31c0                   xor  eax, eax
0x38af2 d1fa                   sar  edx, 1
0x38af4 66a1507a0800           mov  ax, word ptr [0x87a50]
0x38afa 01c2                   add  edx, eax
0x38afc 89d0                   mov  eax, edx
0x38afe c1fa1f                 sar  edx, 0x1f
0x38b01 c1e205                 shl  edx, 5
0x38b04 1bc2                   sbb  eax, edx
0x38b06 c1f805                 sar  eax, 5
0x38b09 66a33a7a0800           mov  word ptr [0x87a3a], ax
0x38b0f 83c404                 add  esp, 4
0x38b12 5d                     pop  ebp
0x38b13 5f                     pop  edi
0x38b14 5a                     pop  edx
0x38b15 59                     pop  ecx
0x38b16 5b                     pop  ebx
0x38b17 c3                     ret
```

Mapping (`ebx`, `ecx`, `edi`, `ebp` all 32-bit; only the low 16 of `di` is read):

- `ebx = (int)(DSD(DS_000F0AF0) << 8)`.
- `idiv` divides the sign-extended `ebx` (via `sar edx,0x1f`) by the
  zero-extended u16 `DS_00107A40`; quotient `q = ebx / (s32)DS_00107A40`
  (trunc) is spilled to `[esp]`.
- `ecx = (u16)DS_00107A52 - 1`. If negative (i.e. `DS_00107A52 == 0`) skip the
  loop. `edi = (u16)DS_00107A4C + ecx`; `ebp = ecx*2`.
- Loop, `i = ecx` from `DS_00107A52 - 1` down to `0`:
  - `t = (ebx / 256) >> 1`: the truncating signed `/256` at `0x38a88`
    (`sar eax,8`) followed by the arithmetic `sar edx,1` at `0x38a8f`.
    `DSW(DS_00107900 + i*2) = (u16)t` (downward fill).
  - `ax = (u16)di`; if `(u16)edi <= 0xEF` then `DS_00107A3E = (t + 0x2B00) / 32`
    -- the *same* shifted `t`, since the raw reuses `edx`; otherwise the store
    is skipped.
  - `dec edi; ebp -= 2; ecx--; ebx -= q`.
- After the loop `ebx -= (u16)DS_00107A42 * q` (32-bit `imul`).
  `DS_00107A44+2 (= 0x87a46) = (u16)(ebx / 256)`.
  `DS_00107A3A = (((ebx/256) >> 1) + (u16)DS_00107A50) / 32`.

Note the `di` write at `0x38a6c` does not zero `edi`'s upper 16 bits, but the only
read is `mov ax,di` (low 16) and the `cmp eax,0xef` is preceded by
`xor eax,eax`, so the comparison is the unsigned low-16 value. The port models
`edi` as a plain `int` started at `(u16)DS_00107A4C + idx` and decremented per
row, matching that.

## 4. The decompiler vs the raw

`0x389C4` (`port/decomp/prage.c:23727`) transcribed the `< 1` arm as an unsigned
`>> 6`; the raw uses the full signed idiom there too. It is value-identical
because the operand is zero-extended, and the `>= 1` arm's bias term
`(iVar2 + iVar3 * -0x40) - (uint)(iVar3 << 5 < 0)` is the same
`+63`-when-negative truncation. Cosmetic only.

`0x38A38` (`port/decomp/prage.c:23755`) **matches the raw** once the two idioms
are read correctly, and the raw confirms it:

- line 23773, `iVar3 = (int)(... biased by -0x100 ...) >> 9`, is not a divide by
  512. The raw is `sar eax,8` (truncating `/256`) then `sar edx,1` (`>> 1`), and
  nested arithmetic shifts make `((x + 255) >> 8) >> 1 == (x + 255) >> 9` for
  negative `x` and `(x >> 8) >> 1 == x >> 9` for non-negative `x`. So the
  decompiler's `>> 9` is exactly the raw's table value `(ebx/256) >> 1`.
- line 23789, `(short)((uint)iVar1 >> 8)`, is an *unsigned* shift of the biased
  value, but only its low 16 bits are stored. An arithmetic `sar eax,8` and a
  logical `shr eax,8` differ only in bits 24..31, so the low-16 result is the
  same: `ebx = -65534` gives `0xFFFF0101`, and both shifts yield `0x...FF01`.
- line 23790's `(iVar1 >> 9)` is the raw's tail `/256` then `>> 1` (`0x38ae5
  sar eax,8` then `0x38af2 sar edx,1`), matching the same nested-shift identity.

The transcription hazard is real but sits in the *reader*, not the decompiler:
the loop computes a single shifted `t` and the raw stores it to both the table
and `DS_00107A3E`. An implementation that stores the unshifted `ebx/256` to the
table halves the shear instead. The port follows the raw exactly: the table
value and `DS_00107A3E` both use `(ebx/256) >> 1`, while the tail's
`DS_00107A44+2` keeps the unshifted `ebx/256`.

---

# The attract reset/scheduler/tails/volumes and the scene tick (Task 3)

Register-level derivation of `0x292AC`, `0x10EE4`, `0x10F28`, `0x10DB0`,
`0x10E18` and `0x2C8F0` (eax = -2), plus the `0x292AC` callee table at
`DS_000A8744`. Same mapping as Task 2: `file_offset = va + 0x52E54` for obj-0
code, and a `DS_*` immediate `I` is linear `I + 0x80000`.

Reproduction (read-only):

```python
from capstone import *
d = open('data/game/C/PRAGE.EXE','rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_32)
for va, n in ((0x292ac,39),(0x10ee4,66),(0x10f28,128),(0x2c8f0,199),
              (0x10db0,104),(0x10e18,104),(0x4f7f4,70),(0x5d812,3)):
    print('===', hex(va))
    for i in md.disasm(d[va + 0x52E54: va + 0x52E54 + n], va):
        print(hex(i.address), i.bytes.hex(), i.mnemonic, i.op_str)
```

## 1. `0x292AC` — the per-bit scene tick

```
0x292ac 53                     push ebx
0x292ad 52                     push edx
0x292ae 8b15d04a0800           mov  edx, dword ptr [0x84ad0]   ; DS_00104AD0
0x292b4 85d2                   test edx, edx
0x292b6 7418                   je   0x292d0
0x292b8 31db                   xor  ebx, ebx
0x292ba f6c201                 test dl, 1
0x292bd 7408                   je   0x292c7
0x292bf 89d8                   mov  eax, ebx
0x292c1 ff9044870200           call dword ptr [eax + 0x28744]  ; -> 0xA8744 + i
0x292c7 d1ea                   shr  edx, 1
0x292c9 83c304                 add  ebx, 4
0x292cc 85d2                   test edx, edx
0x292ce 75ea                   jne  0x292ba
0x292d0 5a                     pop  edx
0x292d1 5b                     pop  ebx
0x292d2 c3                     ret
```

Mapping: `mask = DSD(DS_00104AD0)`; for `i = 0, 4, 8, ...` while `mask != 0`,
when `mask & 1` call the function at `DSD(DS_000A8744 + i)` with `eax = i`
(`mov eax,ebx`). **One call per set bit**, not two (design §3.5's "two callees
per set bit" was wrong).

### 1a. The callee table `DS_000A8744`

The LE loader fills the data object's fixed-up table at linear `0xA8744`; the
shipped entries (dumped after `mem_load_le`) are:

| entry | i | value | function |
|---|---|---|---|
| 0 | 0x00 | `0x4F7F4` | 70 B scene advance |
| 1..31 | 0x04..0x7C | `0x5D812` | 3 B `xor eax,eax; ret` |

```
0x5d812 33c0                   xor  eax, eax
0x5d814 c3                     ret
```

```
0x4f7f4 53                     push ebx
0x4f7f5 52                     push edx
0x4f7f6 833d480a070000         cmp  dword ptr [0x70a48], 0     ; DS_000F0A48
0x4f7fd 7431                   je   0x4f830
0x4f7ff 8a15f1880800           mov  dl, byte ptr [0x888f1]
0x4f805 31db                   xor  ebx, ebx
0x4f807 88d3                   mov  bl, dl
0x4f809 a1480a0700             mov  eax, dword ptr [0x70a48]
0x4f80e fec2                   inc  dl
0x4f810 8b1c9da0980400         mov  ebx, dword ptr [ebx*4 + 0x498a0]
0x4f817 8815f1880800           mov  byte ptr [0x888f1], dl
0x4f81d 89da                   mov  edx, ebx
0x4f81f e85040feff             call 0x33874
0x4f824 31c0                   xor  eax, eax
0x4f826 a0f1880800             mov  al, byte ptr [0x888f1]
0x4f82b 83f80a                 cmp  eax, 0xa
0x4f82e 7207                   jb   0x4f837
0x4f830 8025d04a0800fe         and  byte ptr [0x84ad0], 0xfe
0x4f837 5a                     pop  edx
0x4f838 5b                     pop  ebx
0x4f839 c3                     ret
```

Both callees ignore the incoming `eax` (= `i`): `0x5D812` returns via
`xor eax,eax`, and `0x4F7F4` sets `eax` from `DS_000F0A48` at `0x4F809` before
using it. So the port's no-argument `fn_resolve` call is faithful for the
shipped table. `0x4F7F4` is unported (its `0x33874` call and `DS_000A498A0`
list are not in scope this cycle), so `fn_resolve` returns NULL and the entry is
skipped — the same declared-gap behaviour as `flow.c`'s `run_process_table`.

### 1b. The decompiler vs the raw

`prage.c:14649` writes `(**(code **)((int)&PTR_FUN_000a8744 + iVar2))(param_2)`
and `uVar1 = extraout_EDX` — it drops `mov eax,ebx` and reports the callee's
argument as `edx`. The raw's first argument is `eax = i`. It also re-loads
`uVar1` from a register the callee may clobber instead of keeping the mask in a
callee-preserved register; the raw keeps `edx` (the callees save/restore
`ebx`/`edx`), so the loop's mask is well-defined.

## 2. `0x10EE4` — the state reset

```
0x10ee4 53                     push ebx
0x10ee5 52                     push edx
0x10ee6 31c0                   xor  eax, eax
0x10ee8 e8831a0200             call 0x32970                 ; 0x32970(0)
0x10eed 31c0                   xor  eax, eax
0x10eef ba03000000             mov  edx, 3
0x10ef4 e8ebe20300             call 0x4f1e4                 ; DS_00104B15 = 0
0x10ef9 b801000000             mov  eax, 1
0x10efe 31db                   xor  ebx, ebx
0x10f00 e8efab0100             call 0x2baf4                 ; actors_reset()
0x10f05 668915004b0800         mov  word ptr [0x84b00], dx  ; DS_00104B00 = 3
0x10f0c 66891d640a0700         mov  word ptr [0x70a64], bx  ; DS_000F0A64 = 0
0x10f13 30e4                   xor  ah, ah
0x10f15 30d2                   xor  dl, dl
0x10f17 8825710a0700           mov  byte ptr [0x70a71], ah  ; DS_000F0A71 = 0
0x10f1d 88156f0a0700           mov  byte ptr [0x70a6f], dl  ; DS_000F0A6F = 0
0x10f23 5a                     pop  edx
0x10f24 5b                     pop  ebx
0x10f25 c3                     ret
```

Argument mapping: `0x32970` takes `eax = 0`; `0x4F1E4` receives `edx = 3`;
`0x2BAF4` receives `eax = 1`. `DS_00104B00 = dx` reads **3**: `0x4F1E4` saves and
restores `edx` (`push edx; ...; pop edx; ret`), so its internal `xor dl,dl`
cannot clobber the caller's 3. `0x2BAF4` likewise preserves `edx` across its
push/pop prologue/epilogue. `0x32970` is the run-clock/tick helper, out of scope
(spec §7); `0x29D60` is unrelated here.

The decompiler (`prage.c:731`) shows `DAT_00104b00 = extraout_DX` and
`FUN_00032970(param_2)`: it drops the constant `3` (and the fact that it
survives `0x4F1E4`).

## 3. `0x10F28` — the voice/rng scheduler

```
0x10f28 51                     push ecx
0x10f29 52                     push edx
0x10f2a 668b15600a0700         mov  dx, word ptr [0x70a60]
0x10f31 4a                     dec  edx
0x10f32 668915600a0700         mov  word ptr [0x70a60], dx
0x10f39 6685d2                 test dx, dx
0x10f3c 7f1f                   jg   0x10f5d
0x10f3e b8bd000000             mov  eax, 0xbd
0x10f43 e8b4b40100             call 0x2c3fc                 ; voice 0xBD
0x10f48 b82d000000             mov  eax, 0x2d
0x10f4d e88ac80400             call 0x5d7dc                 ; rng_next(0x2D)
0x10f52 052d000000             add  eax, 0x2d
0x10f57 66a3600a0700           mov  word ptr [0x70a60], ax
0x10f5d 668b0d620a0700         mov  cx, word ptr [0x70a62]
0x10f64 49                     dec  ecx
0x10f65 66890d620a0700         mov  word ptr [0x70a62], cx
0x10f6c 6685c9                 test cx, cx
0x10f6f 7f34                   jg   0x10fa5
0x10f71 b802000000             mov  eax, 2
0x10f76 e861c80400             call 0x5d7dc                 ; rng_next(2)
0x10f7b 85c0                   test eax, eax
0x10f7d 7407                   je   0x10f86
0x10f7f b8be000000             mov  eax, 0xbe
0x10f84 eb05                   jmp  0x10f8b
0x10f86 b8bf000000             mov  eax, 0xbf
0x10f8b e86cb40100             call 0x2c3fc                 ; voice 0xBE/0xBF
0x10f90 b83c000000             mov  eax, 0x3c
0x10f95 e842c80400             call 0x5d7dc                 ; rng_next(0x3C)
0x10f9a 053c000000             add  eax, 0x3c
0x10f9f 66a3620a0700           mov  word ptr [0x70a62], ax
0x10fa5 5a                     pop  edx
0x10fa6 59                     pop  ecx
0x10fa7 c3                     ret
```

Mapping: both countdowns are unsigned-16 stores tested with signed `jg` (equal
to `(s16)DSW <= 0`). Reloads are `(u16)(rng_next(0x2D) + 0x2D)` and
`(u16)(rng_next(0x3C) + 0x3C)`. The second branch draws `rng_next(2)` and picks
`0xBE` when it is nonzero, else `0xBF`; the draw stays so the rng stream is
faithful even though the port's voice call is the declared no-op stub.

The decompiler (`prage.c:750`) drops every argument and the branch:
`FUN_0002c3fc(param_2,param_3)` twice and `FUN_0005d7dc()` with no range, so it
loses `0x2D`/`2`/`0x3C` and the `0xBE`/`0xBF` choice.

## 4. `0x2C8F0` with `eax = -2` — the config volumes

```
0x2c939 83f8fe                 cmp  eax, -2
0x2c93c 0f8571000000           jne  0x2c9b3                 ; return
0x2c942 b82a000000             mov  eax, 0x2a
0x2c947 e828100000             call 0x2d974                 ; config_field_get(0x2A)
0x2c94c 89c3                   mov  ebx, eax
0x2c94e b835000000             mov  eax, 0x35
0x2c953 83e303                 and  ebx, 3                  ; scale
0x2c956 e819100000             call 0x2d974                 ; config_field_get(0x35)
0x2c95b 83f8ff                 cmp  eax, -1
0x2c95e 7507                   jne  0x2c967
0x2c960 b808000000             mov  eax, 8
0x2c965 eb15                   jmp  0x2c97c
0x2c967 89c2                   mov  edx, eax
0x2c969 0fafd3                 imul edx, ebx
0x2c96c b903000000             mov  ecx, 3
0x2c971 89d0                   mov  eax, edx
0x2c973 c1fa1f                 sar  edx, 0x1f
0x2c976 f7f9                   idiv ecx
0x2c978 89c2                   mov  edx, eax
0x2c97a d1f8                   sar  eax, 1
0x2c97c e83701ffff             call 0x1cab8                 ; DS_000A2CB8 = eax
0x2c981 b837000000             mov  eax, 0x37
0x2c986 e8e90f0000             call 0x2d974                 ; config_field_get(0x37)
0x2c98b 83f8ff                 cmp  eax, -1
0x2c98e 7507                   jne  0x2c997
0x2c990 b810000000             mov  eax, 0x10
0x2c995 eb15                   jmp  0x2c9ac
0x2c997 89c2                   mov  edx, eax
0x2c999 0fafd3                 imul edx, ebx
0x2c99c bb03000000             mov  ebx, 3
0x2c9a1 89d0                   mov  eax, edx
0x2c9a3 c1fa1f                 sar  edx, 0x1f
0x2c9a6 f7fb                   idiv ebx
0x2c9a8 89c2                   mov  edx, eax
0x2c9aa d1f8                   sar  eax, 1
0x2c9ac e82305ffff             call 0x1ced4                 ; DS_000A2CB4 = eax
0x2c9b1 89d0                   mov  eax, edx
0x2c9b3 5a                     pop  edx
0x2c9b4 59                     pop  ecx
0x2c9b5 5b                     pop  ebx
0x2c9b6 c3                     ret
```

Mapping: field `0x2A` masked with 3 is the scale; field `0x35` is the music
value, field `0x37` the SFX value. Each is `m*scale`, then `idiv 3` (truncation
toward zero), then `sar 1`; the `0x2D974` sentinel `-1` substitutes `8` (music)
and `0x10` (SFX). `0x1CAB8`/`0x1CED4` are the AIL volume setters for
`DS_000A2CB8`/`DS_000A2CB4`. Those fields are valid (`<= 0x3E`), so the `-1`
branch is defensive and unreachable from this call.

The decompiler (`prage.c:17316`) drops the whole arithmetic: both arms show only
`FUN_0002d974`/`FUN_0001cab8`/`FUN_0001ced4` with no immediates or operators.

## 5. `0x10DB0` / `0x10E18` — the pause/continue tails

```
0x10db0 53                     push ebx
0x10db1 8a25710a0700           mov  ah, byte ptr [0x70a71]   ; DS_000F0A71
0x10db7 84e4                   test ah, ah
0x10db9 755b                   jne  0x10e16
0x10dbb f605db88080020         test byte ptr [0x888db], 0x20 ; byte 3 bit 0x20
0x10dc2 7452                   je   0x10e16
0x10dc4 f605d988080010         test byte ptr [0x888d9], 0x10 ; byte 1 bit 0x10
0x10dcb 7449                   je   0x10e16
0x10dcd c605710a070001         mov  byte ptr [0x70a71], 1
0x10dd4 803d1b4b080000         cmp  byte ptr [0x84b1b], 0     ; DS_00104B19+2
0x10ddb 7430                   je   0x10e0d
0x10ddd bb04000000             mov  ebx, 4
0x10de2 88251b4b0800           mov  byte ptr [0x84b1b], ah
0x10de8 8825154b0800           mov  byte ptr [0x84b15], ah
0x10dee 66891d6c0a0700         mov  word ptr [0x70a6c], bx
0x10df5 66891d640a0700         mov  word ptr [0x70a64], bx
0x10dfc e85f8f0100             call 0x29d60                 ; ret-only no-op
0x10e01 b800010000             mov  eax, 0x100
0x10e06 e8f1b50100             call 0x2c3fc                 ; voice 0x100
0x10e0b 5b                     pop  ebx
0x10e0c c3                     ret
0x10e0d 66c705640a07000400     mov  word ptr [0x70a64], 4
0x10e16 5b                     pop  ebx
0x10e17 c3                     ret
```

`0x10E18` is byte-identical with `0x888db & 0x10`, `0x888d9 & 0x20`, `ebx = 5`
and the two `5` stores. Note the byte-2 clear writes `ah`, which is 0 on the
taken path (it passed `test ah,ah; jne`), so it clears `DS_00104B19` byte 2 and
`DS_00104B15`. `0x29D60` is `c3` (one-byte `ret`).

## 6. Test values

- Tails: pause gate `DS_001088D8 = 0x20000000 | 0x00001000` (byte 3 `0x20`,
  byte 1 `0x10`) -> `DS_000F0A64 = 4`, `DS_000F0A71 = 1`; continue gate
  `0x10000000 | 0x00002000` -> `5`. Crossed bits do not open the other gate.
- Reset: `edx = 3` survives `0x4F1E4` -> `DS_00104B00 = 3`; `0x4F1E4` clears
  `DS_00104B15`; `0x2BAF4` clears `DS_00104AD0`; the three state bytes zero.
- Voice, seed `0xABCD`: `rng_next(0x2D) = 6 -> 0x33`; `rng_next(2) = 1`
  (discarded; skipping it would make the next draw 52 -> `0x70`);
  `rng_next(0x3C) = 6 -> 0x42`.
- Volumes: scale 3, `m = 0x2A00 -> ((0x2A00*3)/3)>>1 = 0x1500`,
  `s = 0xA0 -> 80`; scale 2, `m = 0x2A01 -> 21506/3 = 7168 >>1 = 3584`,
  `s = 0xA1 -> 322/3 = 107 >>1 = 53`; scale 0 -> both 0; `(1*3)/3 >>1 = 0`.
- Scene tick: shipped `DS_000A8744` = `0x4F7F4`, `+4`/`+8` = `0x5D812`; probes
  registered at `0xF00D0`/`0xF00D4` prove bit 0 -> entry 0, bit 1 -> entry at
  `i = 4`, no bits -> no calls.
