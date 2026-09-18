# Actor spawn `0x2AE14` — register-argument binding, pinned by disassembly

Task 5 brief step 1. `__regparm3` hides the register arguments, so the binding
below is read from the shipped `PRAGE.EXE` with capstone (32-bit, GNU syntax),
**not** inferred from the decompilation. Raw bytes are given for every claim so
the review can re-run it.

Method (Format reference A/plan Task 1): object-0 code maps as
`file_offset = va + 0x52E54`; verified independently here by the byte at
`0x12207 + 0x52E54 = 0x6505B` decoding as the `push 0` that precedes the first
title spawn (below).

The descriptors passed as `EAX` are DS-relative immediates (`0x1AC30`, …); the
data object loads at `DATA_BASE = 0x80000`, so the descriptor is at the linear
address the plan writes (`0x9AC30`, …; `mem[0x80000 + 0x1AC30]`).

---

## 0. The incoming registers

`0x2AE14` prologue, file `0x7E068`… (`0x2AE14 + 0x52E54`):

```
0002ae14  56               push esi
0002ae15  57               push edi
0002ae16  55               push ebp
0002ae17  83ec18           sub  esp, 0x18
0002ae1a  89c5             mov  ebp, eax          ; EAX -> descriptor (param_1)
0002ae1c  89542404         mov  [esp+4], edx      ; EDX -> slot (arg 2)
0002ae20  891c24           mov  [esp], ebx        ; EBX -> slot (arg 4)
0002ae23  894c2408         mov  [esp+8], ecx      ; ECX -> slot (arg 3)
0002ae27  8b442428         mov  eax, [esp+0x28]   ; 5th argument, on the stack
0002ae2b  c1f810           sar  eax, 0x10
0002ae2e  6689442410       mov  [esp+0x10], ax    ; high 16 -> rec+0x5a layer
...
0002ae37  668b442428       mov  ax, [esp+0x28]    ; low 16 -> flags word, masked
```

After the 3 pushes + `sub esp,0x18` (0x24 bytes) the caller's stack argument is
at `[esp+0x28]`. So the function consumes **five** arguments: EAX, EDX, ECX,
EBX, then the stack word. Ghidra's prototype
(`FUN_0002ae14(undefined4 *param_1,undefined4 param_2,int param_3,uint param_4)`)
models only EAX/EDX/ECX and moves the 4th slot to the stack; the real 4th
register argument is the `unaff_EBX` the decompiler leaves unaffiliated
(`prage.c:15909`, used at `rec+0x1c`).

The descriptor byte that reaches `rec+0x48` is `desc+0x04` (`0x2AEAD`
`mov al, [ebp+4]`); for both title descriptors it is `0x00` (raw bytes in §5.1).
Arg 5's low 16 bits are also handed to `0x2AC80` as its argument (§3).

## 1. `0x2AE14` call sites in the title `0x121A0`

### Site 1 — `0x1221D` (mode-1 branch, `prage.c:1427`)

```
00012207  6a00             push 0
00012209  b9ff000000       mov  ecx, 0xff
0001220e  bb000c0000       mov  ebx, 0xc00
00012213  ba002a0000       mov  edx, 0x2a00
00012218  b83cae0100       mov  eax, 0x1ae3c
0001221d  e8f28b0100       call 0x2ae14
```

`EAX=0x9AE3C` `EDX=0x2A00` `ECX=0xFF` `EBX=0xC00` `[stack]=0`.
⇒ `actor_spawn(0x9AE3C, 0x2A00, 0xFF, 0xC00, 0)`.

### Site 2 — `0x122F1` (`DS_000F0A58`, `prage.c:1450`)

```
000122cd  6a00             push 0
000122cf  bb001e0000       mov  ebx, 0x1e00
000122d4  b9e0000000       mov  ecx, 0xe0
000122d9  29fb             sub  ebx, edi          ; edi = iVar1 << 6  (= 12 << 6 = 0x300)
000122db  0500150000       add  eax, 0x1500       ; eax = iVar2/2 + 0x1500 -> DS_00107A50 (= 0x2420)
000122e0  8d96002a0000     lea  edx, [esi + 0x2a00]; esi = iVar2     (= 0x1E40)
000122e6  66a3507a0800     mov  [0x87a50], ax
000122ec  b830ac0100       mov  eax, 0x1ac30
000122f1  e81e8b0100       call 0x2ae14
```

`EAX=0x9AC30`, `EDX=0x1E40+0x2A00=0x4840`, `ECX=0xE0`,
`EBX=0x1E00-0x300=0x1B00`, `[stack]=0` (with the pinned `iVar1=12`,
`iVar2=0x1E40`). ⇒ `actor_spawn(0x9AC30, 0x4840, 0xE0, 0x1B00, 0)`.

### Site 3 — `0x1233F` (`DS_000F0A54`, `prage.c:1458`)

```
00012325  6a00             push 0
0001232d  31db             xor  ebx, ebx
0001232f  31d2             xor  edx, edx
00012335  b9e4000000       mov  ecx, 0xe4
0001233a  b894ac0100       mov  eax, 0x1ac94
0001233f  e8d08a0100       call 0x2ae14
```

`EAX=0x9AC94` `EDX=0` `ECX=0xE4` `EBX=0` `[stack]=0`.
⇒ `actor_spawn(0x9AC94, 0, 0xE4, 0, 0)`.

### Site 4 — `0x123BF` (`DS_000F0A54`, `prage.c:1473`)

```
00012396  e8e5ce0100       call 0x2f280          ; return value ignored
0001239b  a1580a0700       mov  eax, [0x70a58]
000123a0  b9e4000000       mov  ecx, 0xe4
000123a5  e8a68d0000       call 0x2b150          ; preserves ECX/EDX/EBX
000123aa  a1540a0700       mov  eax, [0x70a54]
000123af  31db             xor  ebx, ebx
000123b1  e89a8d0000       call 0x2b150          ; preserves ECX/EDX/EBX
000123b6  6a00             push 0
000123b8  31d2             xor  edx, edx
000123ba  b8a8ac0100       mov  eax, 0x1aca8
000123bf  e8508a0100       call 0x2ae14
```

`EAX=0x9ACA8` `EDX=0` `ECX=0xE4` `EBX=0` `[stack]=0`.

`ECX=0xE4` is the value set at `0x123A0`; `0x2B150` is
`push ebx; push ecx; push edx; …; pop edx; pop ecx; pop ebx; ret` (below), so
both of its calls leave ECX and EBX intact; `xor ebx,ebx` at `0x123AF` then
gives EBX=0.
⇒ `actor_spawn(0x9ACA8, 0, 0xE4, 0, 0)`.

`0x2B150` register preservation (file `0x7DFA4`, `0x2B150 + 0x52E54`):

```
0002b150  53               push ebx
0002b151  51               push ecx
0002b152  52               push edx
...
0002b1e0  5a               pop  edx
0002b1e1  59               pop  ecx
0002b1e2  5b               pop  ebx
0002b1e3  c3               ret
```

## 2. `0x38B18`'s call site (`prage.c:23821`)

`0x38B18` is `__regparm3`: it saves `EAX`→ESI (descriptor) and `EDX`→EBP, and
uses `EBX` as its third register argument.

```
00038b18  56               push esi
00038b19  57               push edi
00038b1a  55               push ebp
00038b1b  89c6             mov  esi, eax          ; EAX = descriptor
00038b1d  89d5             mov  ebp, edx          ; EDX = arg2
...
00038b4a  6a00             push 0
00038b4c  b902000000       mov  ecx, 2
00038b51  c1e303           shl  ebx, 3            ; EBX = arg3 << 3
00038b54  8d14ed00000000   lea  edx, [ebp*8]      ; EDX = arg2 << 3
00038b5b  89f0             mov  eax, esi          ; EAX = descriptor
00038b5d  e8b222ffff       call 0x2ae14
```

So `0x38B18(desc, a2, a3)` calls
`actor_spawn(desc, a2 << 3, 2, a3 << 3, 0)`.
The title's four calls (`0x1224F`, `0x12262`, `0x12275`, `0x1228B`) all pass
`EAX=0x9AC1C` with `(EDX, EBX)` = `(0,0)`, `(0x2A,0)`, `(0,0x1E)`, `(0x2A,0x1E)`,
matching Format reference G.

## 3. The `0x2AC80` free-list flag

`0x2AE41` calls `0x2AC80` with EAX set to the low 16 bits of arg 5:

```
0002ae33  31c0             xor  eax, eax
0002ae35  31d2             xor  edx, edx
0002ae37  668b442428       mov  ax, [esp+0x28]    ; EAX = low16 of arg 5
0002ae3c  668954242a       mov  [esp+0x2a], dx
0002ae41  e83afeffff       call 0x2ac80
```

`0x2AC80` tests that argument, copied to ECX by its own prologue (not the
caller's ECX):

```
0002ac80  53               push ebx
0002ac81  51               push ecx
0002ac82  52               push edx
0002ac83  56               push esi
0002ac84  89c1             mov  ecx, eax          ; ECX = the argument (EAX)
0002ac86  8b153c5b0800     mov  edx, [0x85b3c]
...
0002acb1  e87a3d0000       call 0x2ea30
0002acb6  30c9             xor  cl, cl
0002acb8  89c3             mov  ebx, eax
0002acba  80e504           and  ch, 4             ; argument bit 0x400
0002acbd  31c0             xor  eax, eax
0002acbf  6689c8           mov  ax, cx
0002acc2  85c0             test eax, eax
0002acc4  750c             jne  0x2acd2
0002acc6  b8cc5b0800       mov  eax, 0x85bcc
0002accb  e8e09cffff       call 0x249b0           ; head insert (active list)
0002acd0  eb0a             jmp  0x2acdc
0002acd2  b8cc5b0800       mov  eax, 0x85bcc
0002acd7  e8e49cffff       call 0x249c0           ; tail insert (active list)
```

`0x2AC84` `mov ecx, eax` overwrites ECX with the argument **before** the test, so
the caller's ECX is irrelevant; `0x2ACB6` `xor cl,cl` then `0x2ACBA` `and ch,4`
tests **bit 0x400 of that argument** (EAX on entry). Neither intervening call
clobbers ECX: `0x249D0` (`53 52 8b 18 …`, eax/ebx/edx only) and `0x2EA30`
(`52 8a 35 … 5a 90 c3`, eax/dh/dl only).

The flag is therefore the **low 16 bits of spawn arg 5** — the same word the
decompilation tests at `prage.c:15960` to select the parent-relative branch. For
the title every spawn passes `[stack] = 0`, so bit 0x400 is clear and every title
spawn head-inserts; the tail path is unreachable this cycle. Plan errata
`66c989c` records the same `0x2AC84`/`0x2ACB6`/`0x2ACBA` basis and the
`actor_alloc(u32 flags)` interface.

## 4. Argument order conclusion

```c
u32 actor_spawn(const u32 *desc, u32 a2, u32 a3, u32 a4, u32 a5); /* 0x2AE14 */
```

| port arg | incoming register | field it drives (from `prage.c:15915-16044`) |
|---|---|---|
| `desc` | EAX | descriptor pointer: `+0x00`→`rec+0x08`, `+0x04`→`rec+0x48`, `+0x05`→`rec+0x20/24`, `+0x06`→`rec+0x2E`, `+0x08`→`rec+0x28`, `+0x0A`→`rec+0x40`, `+0x0C`→`rec+0x2C`, `+0x10` palette handle |
| `a2` | EDX | `rec+0x18` world x (normal); `rec+0x34` when flag 0x400 |
| `a3` | ECX | `rec+0x49` layer byte and (when `rec+0x28 & 0x2000` is clear) `rec+0x32` x-velocity |
| `a4` | EBX (Ghidra's `unaff_EBX`) | `rec+0x1c` world y (normal); `rec+0x36` when flag 0x400 |
| `a5` | stack `[esp+0x28]` | flags word: low 16 → `rec+0x28` OR'd over `(desc+0x08 & 0xFFC3)` and passed as EAX to `0x2AC80` (§3), byte1 bit `0x44` → `rec+0x28` high byte, high 16 → `rec+0x5A` layer; bit `0x400` selects the parent-relative branch |

---

## 5. Checkable findings (the plan's corrected errata)

These are recorded here because the same disassembly that fixes the binding also
decides them; they are now the plan's own corrected errata (the regenerated brief
and the render-check / `0x1E1` corrections), not open contradictions.

1. **Render-check type values.** `rec+0x48` is written from `desc+0x04`
   (`0x2AEAD` `mov byte ptr [ecx+0x48], al`, `al = [ebp+4]`). The shipped
   descriptor bytes are (`mem_load_le` of `data/game/C/PRAGE.EXE`, identical to
   the `ghidra_data.bin` oracle at these offsets):

   ```
   0x9AC30: 16 91 0e 00 00 01 00 00 ...   desc+0x04 = 0x00
   0x9AC94: 7a 89 0e 00 00 08 10 00 ...   desc+0x04 = 0x00
   ```

   So both title-reachable types are **0x00**, not 0x01 and 0x10. The design
   spec agrees (§4.2: `0x9AC30 = {… byte+4 0x00 …}`). The render table
   `DS_000BB9DC` is indexed `type*0xC` (`0x2B0D4-0x2B0E9`, `call [ebx+0x3B9DC]`
   with `ebx = (type*4-type)*4 = type*0xC`); entry 0 is `0x5D812` =
   `xor eax,eax; ret` (returns 0 → allocate/insert a display node).

2. **`0x1E1` is not the per-type render check's return.** The sprite id `0x1E1`
   is assigned at `0x2B046` when `0x2B2A0()` (the animation-opcode dispatcher,
   Task 7) returns 2, inside the initial stream walk. The per-type render check
   at `0x2B0E9` returns a char used as a boolean at `0x2B0EF` (`test al,al`):
   nonzero → mark `rec+0x28 |= 8` and return 0; zero → `rec+0x2b |= 0x40` and
   allocate/insert a display node.
