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
