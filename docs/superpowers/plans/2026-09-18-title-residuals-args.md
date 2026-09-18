# Effects list `0x13xxx` — register-argument binding, pinned by disassembly

Task 4 brief, steps 1–4. `__regparm3` hides the register arguments, and the
decompiler mis-models them, so the binding below is read from the shipped
`PRAGE.EXE` with capstone (32-bit, GNU syntax), **not** inferred from
`prage.c`. Raw bytes are given for every claim so the review can re-run it.

Method (same as 4a-ii's `2026-09-17-actor-system-args.md`): object-0 code maps
as `file_offset = va + 0x52E54`; verified here again — `0x13C70 + 0x52E54 =
0x66AC4` decodes as the `push ecx` prologue below. DS data globals in
instructions are object-relative; the shipped data object loads at
`0x80000`, so a `DS_*` immediate `I` is linear `I + 0x80000` (the Ghidra
symbol name). The port's `symbols.h` uses the linear name:
`0x7CCE0 → 0xFCCE0 → DS_000FCCE0`, `0x70B00 → 0xF0B00 → DS_000F0B00`,
`0x1AF3C → 0x9AF3C → DS_0009AF3C`, `0x1AF3D → 0x9AF3D → DS_0009AF3D`.

Three independent raw instructions agree on the link fields; the decompiler
contradiction is resolved in §3.

---

## 0. Register-argument summary (the interface Tasks 5–6 consume)

| VA | role | incoming registers | returned | notes |
|---|---|---|---|---|
| `0x13C70` | effect spawn | `EAX` = source record ptr, `DL` = byte arg, `EBX` = handle | void (EAX junk) | pops one free record, fills it, head-inserts into active list, `DS_0009AF3D++` |
| `0x13DF0` | effect list clear | none | void | walks active list `0xFCCE0`, tears each down, zeroes count and lock |
| `0x13ADC` | free-list build | none | void | inits both sentinels, tail-appends 24 records to free list `0xFCCE8` |
| `0x249B0` | list insert **after** | `EAX` = anchor node, `EDX` = new node | void | inserts `EDX` immediately after `EAX` |
| `0x249C0` | list insert **before** | `EAX` = anchor node, `EDX` = new node | void | inserts `EDX` immediately before `EAX` |
| `0x249D0` | list unlink | `EAX` = node | `EAX` = node (unchanged) | unlinks `EAX`; zeroes its two link fields |
| `0x13420` | per-entry teardown | `EAX` = node | void | unlinks from active list, runs type dispatch, re-inserts into free list |
| `0x1B544` | handle → pointer | `EAX` = handle | `EAX` = resolved ptr | one register arg only; see §6 |

The link/unlink helpers and `0x1B544` push and pop the caller's `EBX`/`ECX`/
`EDX` (and `ESI` for `0x1B544`), so those are restored on return. That does
**not** make `EBX`/`ECX`/`EDX` uniformly "callee-preserved arguments": `0x13C70`
consumes `EBX` as its handle argument, and `0x249B0`/`0x249C0` consume `EDX` as
the new node. The `__regparm3` signatures in `prage.c` (`undefined8`,
`param_2`, `extraout_*`) are decompiler artefacts.

---

## 1. Per-function disassembly (file `va + 0x52E54`)

### 1.1 `0x13C70` — effect spawn (file `0x66AC4`, size 219)

Prologue and record writes:

```
00013c70  51                push ecx
00013c71  56                push esi
00013c72  57                push edi
00013c73  55                push ebp
00013c74  83ec08            sub  esp, 8
00013c77  89c1              mov  ecx, eax          ; ECX = EAX = arg1 (source record)
00013c79  88542404          mov  [esp+4], dl       ; save arg2 byte (DL)
00013c7d  89da              mov  edx, ebx          ; EDX = EBX = arg3 (handle)
00013c7f  8b1de8cc0700      mov  ebx, [0x7cce8]    ; EBX = free_head.next
00013c85  81fbe8cc0700      cmp  ebx, 0x7cce8      ; empty?
00013c8b  7504              jne  0x13c91
00013c8d  31db              xor  ebx, ebx          ; EBX = 0 -> no record
00013c8f  eb1e              jmp  0x13caf
00013c91  a03caf0100        mov  al, [0x1af3c]     ; save lock
00013c96  880424            mov  [esp], al
00013c99  c6053caf010001    mov  byte [0x1af3c], 1 ; lock = 1
00013ca0  89d8              mov  eax, ebx
00013ca2  e8290d0100        call 0x249d0            ; unlink the record from free list
00013ca7  8a0424            mov  al, [esp]
00013caa  a23caf0100        mov  [0x1af3c], al     ; restore lock
00013caf  89df              mov  edi, ebx          ; EDI = record
00013cb1  85db              test ebx, ebx
00013cb3  0f848a000000      je   0x13d43           ; none -> return
00013cb9  89d0              mov  eax, edx
00013cbb  e884780000        call 0x1b544            ; EAX = resolve(arg3 handle)
00013cc0  c6430f80          mov  byte [ebx+0xf], 0x80
00013cc4  c6430c03          mov  byte [ebx+0xc], 3
00013cc8  89c6              mov  esi, eax          ; ESI = resolved table base
00013cca  894b08            mov  [ebx+8], ecx      ; rec+8 = source record (arg1)
00013ccd  8a442404          mov  al, [esp+4]
00013cd1  88430d            mov  [ebx+0xd], al     ; rec+0xd = arg2 byte
00013cd4  8b690c            mov  ebp, [ecx+0xc]    ; count = source_rec+0xc
00013cd7  31d2              xor  edx, edx
00013cd9  85ed              test ebp, ebp
00013cdb  7e14              jle  0x13cf1
00013cdd  89d8              mov  eax, ebx
00013cdf  c7401000000000    mov  dword [eax+0x10], 0 ; rec+0x10+4i = 0
00013ce6  42                inc  edx
00013ce7  8b590c            mov  ebx, [ecx+0xc]
00013cea  83c004            add  eax, 4
00013ced  39da              cmp  edx, ebx
00013cef  7cee              jl   0x13cdf
00013cf1  8b690c            mov  ebp, [ecx+0xc]    ; count again, from source+0xc
00013cf4  31d2              xor  edx, edx
00013cf6  85ed              test ebp, ebp
00013cf8  7e1b              jle  0x13d15
00013cfa  89fb              mov  ebx, edi          ; ebx = record
00013cfc  89f0              mov  eax, esi          ; eax = resolved
00013cfe  83c304            add  ebx, 4
00013d01  8b7004            mov  esi, [eax+4]      ; read resolved+4+4i
00013d04  89b30c040000      mov  [ebx+0x40c], esi  ; rec+0x410+4i = value
00013d0a  42                inc  edx
00013d0b  8b710c            mov  esi, [ecx+0xc]
00013d0e  83c004            add  eax, 4
00013d11  39f2              cmp  edx, esi
00013d13  7ce9              jl   0x13cfe
00013d15  b201              mov  dl, 1
00013d17  b8e0cc0700        mov  eax, 0x7cce0      ; EAX = active head
00013d1c  88153caf0100      mov  [0x1af3c], dl
00013d22  89fa              mov  edx, edi          ; EDX = record
00013d24  c6470e01          mov  byte [edi+0xe], 1
00013d28  e8830c0100        call 0x249b0            ; insert record AFTER active head
00013d2d  8a1d3daf0100      mov  bl, [0x1af3d]
00013d33  30f6              xor  dh, dh
00013d35  fec3              inc  bl
00013d37  88353caf0100      mov  [0x1af3c], dh
00013d3d  881d3daf0100      mov  [0x1af3d], bl      ; DS_0009AF3D++
00013d43  83c408            add  esp, 8
...                      pops + ret
```

So the title call site `0x123EA` (see §5) passes `EAX = node`, `DL = 3`,
`EBX = 0x419786C`.

### 1.2 `0x13DF0` — effect list clear (file `0x66C44`, size 53)

```
00013df0  52                push edx
00013df1  c6053caf010001    mov  byte [0x1af3c], 1   ; lock = 1
00013df8  a1e0cc0700        mov  eax, [0x7cce0]      ; active_head.next
00013dfd  3de0cc0700        cmp  eax, 0x7cce0
00013e02  7411              je   0x13e15
00013e04  8b10              mov  edx, [eax]          ; EDX = node->next
00013e06  e815f6ffff        call 0x13420             ; EAX = node
00013e0b  89d0              mov  eax, edx
00013e0d  81fae0cc0700      cmp  edx, 0x7cce0
00013e13  75ef              jne  0x13e04
00013e15  30d2              xor  dl, dl
00013e17  88153daf0100      mov  [0x1af3d], dl       ; DS_0009AF3D = 0
00013e1d  88153caf0100      mov  [0x1af3c], dl       ; lock = 0
00013e23  5a                pop  edx
00013e24  c3                ret
```

No register arguments are read (EDX is scratch). The walk follows `[0]`
(node→next) and stops at the `0xFCCE0` sentinel; the loop body preserves EDX
across `0x13420` (which pushes/pops EDX), so `edx` is the saved next.

### 1.3 `0x13ADC` — free-list build (file `0x66930`, size 96)

```
00013adc  53                push ebx
00013add  51                push ecx
00013ade  52                push edx
00013adf  b401              mov  ah, 1
00013ae1  bae0cc0700        mov  edx, 0x7cce0
00013ae6  b9e8cc0700        mov  ecx, 0x7cce8
00013aeb  bb000b0700        mov  ebx, 0x70b00        ; first record
00013af0  88253caf0100      mov  [0x1af3c], ah       ; lock = 1
00013af6  8915e4cc0700      mov  [0x7cce4], edx       ; active.prev = active head
00013afc  8915e0cc0700      mov  [0x7cce0], edx       ; active.next = active head
00013b02  890deccc0700      mov  [0x7ccec], ecx       ; free.prev   = free head
00013b08  890de8cc0700      mov  [0x7cce8], ecx       ; free.next   = free head
00013b0e  81fbe0cc0700      cmp  ebx, 0x7cce0
00013b14  731a              jae  0x13b30
00013b16  b8e8cc0700        mov  eax, 0x7cce8        ; EAX = free head
00013b1b  89da              mov  edx, ebx             ; EDX = record
00013b1d  81c314080000      add  ebx, 0x814            ; stride
00013b23  e8980e0100        call 0x249c0             ; insert record BEFORE free head
00013b28  81fbe0cc0700      cmp  ebx, 0x7cce0
00013b2e  72e6              jb   0x13b16
00013b30  30d2              xor  dl, dl
00013b32  88153caf0100      mov  [0x1af3c], dl       ; lock = 0
...                      pops + ret
```

No register arguments are read (EAX/EBX/ECX/EDX are all loaded from
immediates).

### 1.4 `0x249B0` — insert **after** (file `0x77804`, size 15)

```
000249b0  53                push ebx
000249b1  8b18              mov  ebx, [eax]          ; ebx = A->[0]
000249b3  8910              mov  [eax], edx          ; A->[0] = B
000249b5  891a              mov  [edx], ebx          ; B->[0] = old A->[0]
000249b7  894204            mov  [edx+4], eax        ; B->[4] = A
000249ba  895304            mov  [ebx+4], edx        ; old A->[0]->[4] = B
000249bd  5b                pop  ebx
000249be  c3                ret
```

`EAX = A` (anchor), `EDX = B` (new). Inserts `B` **after** `A`.

### 1.5 `0x249C0` — insert **before** (file `0x77814`, size 16)

```
000249c0  53                push ebx
000249c1  8b5804            mov  ebx, [eax+4]        ; ebx = A->[4]
000249c4  895004            mov  [eax+4], edx        ; A->[4] = B
000249c7  8902              mov  [edx], eax          ; B->[0] = A
000249c9  895a04            mov  [edx+4], ebx        ; B->[4] = old A->[4]
000249cc  8913              mov  [ebx], edx          ; old A->[4]->[0] = B
000249ce  5b                pop  ebx
000249cf  c3                ret
```

`EAX = A` (anchor), `EDX = B` (new). Inserts `B` **before** `A`.

### 1.6 `0x249D0` — unlink (file `0x77824`, size 31)

```
000249d0  53                push ebx
000249d1  52                push edx
000249d2  8b18              mov  ebx, [eax]          ; ebx = node->[0] (next)
000249d4  8b5004            mov  edx, [eax+4]        ; edx = node->[4] (prev)
000249d7  895304            mov  [ebx+4], edx        ; next->[4] = prev
000249da  89d3              mov  ebx, edx
000249dc  8b10              mov  edx, [eax]          ; edx = next
000249de  8913              mov  [ebx], edx          ; prev->[0] = next
000249e0  c7400400000000    mov  dword [eax+4], 0    ; node->[4] = 0
000249e7  8b5004            mov  edx, [eax+4]        ; edx = 0
000249ea  8910              mov  [eax], edx          ; node->[0] = 0
000249ec  5a                pop  edx
000249ed  5b                pop  ebx
000249ee  c3                ret
```

`EAX = node` in and out (unchanged); returns void in practice. `prage.c`
sometimes treats `EAX` as a returned pop value — this is only ever correct when
the caller already put the node in `EAX` (as `0x13C70` does).

### 1.7 `0x13420` — per-entry teardown (file `0x66274`, size 133)

```
00013420  53                push ebx
00013421  51                push ecx
00013422  52                push edx
00013423  89c1              mov  ecx, eax            ; ECX = EAX = node
00013425  b401               mov  ah, 1
00013427  8a153caf0100      mov  dl, [0x1af3c]
0001342d  88253caf0100      mov  [0x1af3c], ah       ; lock = 1 (ok to unlink)
00013433  89c8              mov  eax, ecx
00013435  e896150100        call 0x249d0             ; unlink node from active list
0001343a  8a410c            mov  al, [ecx+0xc]       ; type byte
0001343d  88153caf0100      mov  [0x1af3c], dl       ; restore lock
00013443  3c05              cmp  al, 5
00013445  774e              ja   0x13495
00013447  25ff000000        and  eax, 0xff
0001344c  2eff248508340000  jmp  dword [cs:eax*4 + 0x3408]
00013454  31d2              xor  edx, edx             ; case 1
00013456  8b4108            mov  eax, [ecx+8]
00013459  8a510f            mov  dl, [ecx+0xf]
0001345c  8b4008            mov  eax, [eax+8]
0001345f  8d5910            lea  ebx, [ecx+0x10]
00013462  01d0              add  eax, edx
00013464  ba01000000        mov  edx, 1
00013469  e8c6020200        call 0x33734
0001346e  eb25              jmp  0x13495
00013470  8b4108            mov  eax, [ecx+8]         ; case 4
00013473  bbf0cc0700        mov  ebx, 0x7ccf0
00013478  8b500c            mov  edx, [eax+0xc]
0001347b  8b4008            mov  eax, [eax+8]
0001347e  e8b1020200        call 0x33734
00013483  eb10              jmp  0x13495
00013485  8b4108            mov  eax, [ecx+8]         ; cases 0/2/3/5
00013488  8b18              mov  ebx, [eax]
0001348a  8b500c            mov  edx, [eax+0xc]
0001348d  8b4008            mov  eax, [eax+8]
00013490  e87f020200        call 0x33714
00013495  b8e8cc0700        mov  eax, 0x7cce8         ; free head
0001349a  89ca              mov  edx, ecx             ; EDX = node
0001349c  e80f150100        call 0x249b0             ; insert node AFTER free head
000134a1  5a                pop  edx
000134a2  59                pop  ecx
000134a3  5b                pop  ebx
000134a4  c3                ret
```

The dispatch table at `cs:0x3408` — linear `0x13408`, file `0x6625C`, six
32-bit code-segment offsets (code segment base `0x10000`, so `0x3454` →
`0x13454`):

```
linear 0x13408 / file 0x6625C:  85 34 00 00 -> 0x3485 (case 0)
                   54 34 00 00 -> 0x3454 (case 1)
                   85 34 00 00 -> 0x3485 (case 2)
                   85 34 00 00 -> 0x3485 (case 3)
                   70 34 00 00 -> 0x3470 (case 4)
                   85 34 00 00 -> 0x3485 (case 5)
```

`0x13C70` writes type `3`, so the teardown path for spawn-produced records is
the `0x13485` body → `0x33714`. The record is then re-inserted **after** the
free head (`0x249B0`), i.e. pushed to the front of the free list. Incoming is
`EAX = node` only; the callee preserves `EBX`/`ECX`/`EDX` and returns void.

---

## 2. Record layout

### 2.1 Stride, count, bounds (from `0x13ADC`)

`0x13AEB` loads the first record `0x70B00` (linear `0xF0B00` =
`DS_000F0B00`), `0x13B1D` advances by `0x814`, and `0x13B0E`/`0x13B28`
compare against the active sentinel `0x7CCE0` (linear `0xFCCE0`). Therefore:

| item | value |
|---|---|
| stride | `0x814` = 2068 bytes |
| count | `(0x7CCE0 - 0x70B00) / 0x814` = `0xC1E0 / 0x814` = **24** (remainder 0) |
| first record | `0x70B00` (linear `0xF0B00`, `DS_000F0B00`) |
| last record | `0x70B00 + 23*0x814` = `0x7C4CC` (linear `0xFC4CC`) |
| active sentinel | `0x7CCE0` (linear `0xFCCE0`, `DS_000FCCE0`) — next at `+0`, prev at `0xFCCE4` |
| free sentinel | `0x7CCE8` (linear `0xFCCE8`, `DS_000FCCE8`) — next at `+0`, prev at `0xFCCEC` |

`0x7C4CC + 0x814 = 0x7CCE0`, so the 24 records exactly fill the space before
the active sentinel. (The decompiler's `puVar1 += 0x205` at `prage.c:2608` is
the same `0x814` in dwords.)

### 2.2 Fields written by `0x13C70`

| offset | width | value written | source instruction |
|---|---|---|---|
| `+0x00` | dword | next pointer (list link) | via `0x249D0`/`0x249B0` |
| `+0x04` | dword | prev pointer (list link) | via `0x249D0`/`0x249B0` |
| `+0x08` | dword | source-record pointer = arg1 (`EAX`) | `0x13CCA mov [ebx+8],ecx` |
| `+0x0C` | byte | type = `3` | `0x13CC4 mov byte [ebx+0xc],3` |
| `+0x0D` | byte | arg2 (`DL`) | `0x13CD1 mov [ebx+0xd],al` |
| `+0x0E` | byte | state = `1` | `0x13D24 mov byte [edi+0xe],1` |
| `+0x0F` | byte | `0x80` | `0x13CC0 mov byte [ebx+0xf],0x80` |
| `+0x10 + 4*i` | dword × count | `0` | `0x13CDF mov dword [eax+0x10],0` |
| `+0x410 + 4*i` | dword × count | `*(resolve(arg3) + 4 + 4*i)` | `0x13D04 mov [ebx+0x40c],esi` |

`+0x10` and `+0x410` each span `0x400` bytes (256 dwords); the `0x814` stride
leaves `+0x810..+0x813` unused by this slice.

**The count is the *source* record's `+0xC`, read via `[[new+8]+0xC]`:** the
loops load `ebp = [ecx+0xc]` (`0x13CD4`, `0x13CF1`) where `ECX = arg1 = the
source record`, and `rec+0xC` itself is overwritten with the constant `3` at
`0x13CC4`. So the iteration bound comes from the source pointer at `rec+0x8`,
not from the new record's own `+0xC`.

---

## 3. Link-field resolution

**`[+0x00]` is next; `[+0x04]` is prev. `0x249B0` inserts its `EDX` node
immediately AFTER its `EAX` anchor; `0x249C0` inserts its `EDX` node
immediately BEFORE its `EAX` anchor.**

Raw proof, three independent instructions:

1. `0x249B0` / `0x249B3`: `89 10  mov [eax], edx` — writes the anchor's `[0]`
   with the new node, and `0x249B5`: `89 1a  mov [edx], ebx` where
   `ebx = [eax]` was read first (`0x249B1 8b 18`). So `[0]` is the forward
   chain-pointer and the new node goes **after** the anchor.
2. `0x249C0` / `0x249C4`: `89 50 04  mov [eax+4], edx` — writes the anchor's
   `[4]` with the new node, and `0x249C7`: `89 02  mov [edx], eax` sets the new
   node's `[0]` to the anchor. So `[4]` is the backward chain-pointer and the
   new node goes **before** the anchor.
3. `0x249D0` (`0x249D2` `8b 18 mov ebx,[eax]`, `0x249D4` `8b 50 04 mov
   edx,[eax+4]`, `0x249D7` `89 53 04 mov [ebx+4],edx`, `0x249DE` `89 13 mov
   [ebx],edx`) treats `[0]` as next and `[4]` as prev while unlinking.

`0x13DF0` confirms it independently: it walks `[0x7cce0]` and then `[eax]` as
the next pointer, and `0x13ADC`'s `0x249C0` calls tail-append (insert before
the free head) while `0x13C70`'s `0x249B0` call (`EAX = 0x7cce0`, `EDX = rec`)
head-inserts into the active list.

**The decompiler does not actually contradict the field order.** Both
`FUN_000249b0` (`prage.c:11869`) and `FUN_000249c0` (`prage.c:11886`) write
`param_2[1]` as the back-pointer and `*param_2` as the forward pointer — `[0]`
is next, `[1]` (dword index; byte `+4`) is prev. The brief's worry comes from
Ghidra's regparm model, and the real Ghidra artefact is the `FUN_00013c70`
signature: `void __regparm3 FUN_00013c70(undefined4 param_1, undefined1
param_2)` (`prage.c:2699`) drops the `EBX` argument entirely and models the
source record as the hidden `extraout_ECX` (`prage.c:2723` `puVar5[2] =
extraout_ECX`, `:2726` the `+0xC` count). The record pointer itself is correct:
`puVar5 = DAT_000fcce8` (`prage.c:2709`) is a **value** load of the free-list
head pointer — Ghidra treats the global as a pointer variable, as its own
`prage.c:2606 DAT_000fcce8 = &DAT_000fcce8;` shows — so `puVar5` is the first
free record, and `prage.c:2721-2746` writes that record. The raw
`0x13C7F 8b 1d e8 cc 07 00  mov ebx,[0x7cce8]` agrees: the record is
`free_head.next` (in `EBX`). The decompiler is right about the record; what it
cannot show is the `EBX` handle argument. Raw bytes win for the arg binding.
(The brief's `prage.c:2690-2720` is `FUN_00013b3c`/`FUN_00013c70`, not the
primitives; the primitives are at `prage.c:11869`, `:11886`, `:11903`.)

---

## 4. Is the free list populated before the title draw?

**Yes.** `0x13ADC` runs on the shipped title path before `0x123EA`, and its
only caller that can clear the list afterwards rebuilds it.

Callers of `0x13ADC` (from `port/decomp/prage.calls.csv`): `0x20C10`,
`0x2BAF4`, `0x2F9CC`.

The decisive caller is `0x2BAF4`. The title function `0x121A0` calls it at
`0x121EB`, in the `DAT_000F0A6F == 0` init arm:

```
000121c1  84c0              test al, al
000121c3  0f8597020000      jne  0x12460          ; state != 0 -> skip init
000121c9  b841000000        mov  eax, 0x41
000121ce  e829a20100        call 0x2c3fc
...
000121e4  b801000000        mov  eax, 1
000121e9  31c9              xor  ecx, ecx
000121eb  e804990100        call 0x2baf4        ; <-- init/free-list builder
```

Inside `0x2BAF4`, `0x13DF0` is called first, then `0x13ADC` unconditionally
along the fall-through after the resource-table loop (no branch skips it):

```
0002bb2b  e8c082feff        call 0x13df0        ; clear
...
0002bbaa  a1f4140800        mov  eax, [0x814f4]
0002bbaf  05a0eb0000        add  eax, 0xeba0
0002bbb4  39c1              cmp  ecx, eax
0002bbb6  72e6              jb   0x2bb9e
0002bbb8  e81f7ffe00        call 0x13adc        ; build free list (24 records)
```

State `0` then ends by incrementing the title state and returning to the
caller's tail — it does **not** reach `0x123EA`:

```
00012344  8a1d6f0a0700      mov  bl, [0x70a6f]
0001234a  fec3              inc  bl
0001234c  a3540a0700        mov  [0x70a54], eax
00012351  881d6f0a0700      mov  [0x70a6f], bl    ; state 0 -> 1
00012357  e904010000        jmp  0x12460          ; end of this frame
```

The state-`1` arm (`0x121AA cmp al,1; 0x121AE jbe 0x1235C`) does **not** reach
`0x123EA` immediately. It decrements the countdown at `0x70A66` by `0x10` each
frame (`0x12365 sub ebx,0x10`) and jumps to `0x12402` while
`(value - 0x10) > 0x10` (`0x12372 cmp eax,0x10; 0x12375 jg 0x12402`). State 0
armed that countdown with `0x600` (`0x12304 mov eax,0x600; 0x1230B mov word
[0x70a66],ax`), so `0x123EA` is first reached many frames into state 1, after
the fade. The free list built in the state-0 frame is untouched across all
those frames, so the answer is unchanged: when `0x13C70` finally runs it finds
the 24-record pool.

The only `0x13DF0` callers are `0x29B74`, `0x2BAF4`, `0x43738`, `0x444C8`
(`calls.csv`). Only `0x2BAF4` is on the title path, and it calls `0x13ADC`
immediately after clearing, so nothing empties the list between the build and
`0x123EA`. The other `0x13ADC` callers (`0x20C10` game init, `0x2F9CC`) are not
needed for this conclusion.

**Consequence for the port:** a ported `0x13C70` on the title path must not
run against an unbuilt pool. Task 6 must call `effects_init()` (`0x13ADC`) at
the point `0x2BAF4` calls it (`0x2BBB8`) and `effects_clear()` (`0x13DF0`)
where `0x2BAF4` calls it (`0x2BB2B`), so the title spawn finds 24 free
records. The "empty list, count-only effect" branch in the brief does not
apply to the shipped path.

---

## 5. The `0x123EA` call site (Task 6 input)

```
000123cb  e834150200        call 0x33904            ; EAX = node
000123d0  89c1              mov  ecx, eax
000123d2  85c0              test eax, eax
000123d4  7426              je   0x123fc
000123d6  813988e60300      cmp  dword [ecx], 0x3e688
000123dc  7511              jne  0x123ef
000123de  bb6c781904        mov  ebx, 0x419786c      ; handle -> arg3
000123e3  ba03000000        mov  edx, 3              ; DL = 3 -> arg2
000123e8  89c8              mov  eax, ecx            ; node -> arg1
000123ea  e881180000        call 0x13c70
```

⇒ `effects_spawn(node, 0x03, 0x419786C)` with the raw binding
`EAX/ECX = source record`, `DL = byte`, `EBX = handle`. The plan's declared
`effects_spawn(u32 source_rec, u32 byte_arg)` omits the `EBX` handle that the
raw function receives and passes to `0x1B544`; Task 5 should carry the handle
(see §6) or pin it for the title call.

---

## 6. `0x1B544` and `res_resolve`

`0x1B544` (file `0x6E398`, size 200), prologue and fast path:

```
0001b544  53                push ebx
0001b545  51                push ecx
0001b546  52                push edx
0001b547  56                push esi
0001b548  89c6              mov  esi, eax          ; ESI = handle
0001b54a  c1e817            shr  eax, 0x17         ; index = handle >> 23
0001b54d  8d0c8500000000    lea  ecx, [eax*4]
0001b554  01c8              add  eax, ecx
0001b556  8b0de0140800      mov  ecx, [0x814e0]    ; table base (DS_001014E0)
0001b55c  c1e002            shl  eax, 2            ; index * 20
0001b55f  89f2              mov  edx, esi
0001b561  01c1              add  ecx, eax          ; entry ptr
0001b563  81e2ffff7f00      and  edx, 0x7fffff     ; offset = handle & 0x7FFFFF
0001b569  f7410c00000001    test dword [ecx+0xc], 0x1000000
0001b570  740a              je   0x1b57c
0001b572  8b4110            mov  eax, [ecx+0x10]   ; data block
0001b575  01d0              add  eax, edx
0001b577  5e5a595b          pop esi/edx/ecx/ebx
0001b57b  c3                ret                    ; EAX = block + offset
```

Everything after `0x1B57C` (`0x1B57C`–`0x1B60B`) is the on-demand path: when
`[[entry+0x10]+8]` is zero it calls `0x1E774`/`0x1D290` to load the block and
returns `[[entry+0x10]+8] + offset`, or (flag `0x20000000`) allocates through
`0x500BB` and returns the same. **`0x1B544` reads exactly one register
argument — `EAX` (the handle) — and returns `EAX` (the resolved pointer).**
The decompiler's `undefined8 __regparm3 FUN_0001b544(uint param_1, undefined4
param_2)` and `CONCAT44(param_2, …)` are wrong: `EDX` is overwritten from `ESI`
at `0x1B55F` before any use, and restored from the stack at the epilogue, so
the caller's `EDX` is not an argument and not part of the result.

`res_resolve` (`port/src/platform/res.c:164`) implements the `0x1B57C`-skipped
fast path: `index = handle >> 23`, take `DSD(table + index*20 + 16)`, return
`mem + data + (handle & 0x7FFFFF)`. It adds two checks the original fast path
does not have — `index >= DSD(DS_001014F0)` and `data == 0` — and returns NULL
on those. For the effect slice this is sufficient **iff** the title handle's
entry has flag `0x1000000`; otherwise the original takes the `[block+8]` /
on-demand branch that `res_resolve` does not port. In that case (entry
unflagged but `entry+0x10` nonzero) `res_resolve` returns a pointer from the
descriptor — `mem + data + offset` — rather than the original's
`[[entry+0x10]+8] + offset`: non-NULL but wrong. It returns NULL only when the
index is out of range or `entry+0x10 == 0`. Task 5 must treat the new work as:
call `res_resolve(handle)` and read dwords from `resolved + 4 + 4*i`; no new
resolver logic unless the title entry is not flagged `0x1000000`.

The port handle to resolve is `EBX = 0x419786C` at the title call: index
`0x419786C >> 23 = 8`, offset `0x419786C & 0x7FFFFF = 0x19786C`.

---

## 7. Reproduction

All commands are read-only; nothing under `data/` is modified. Capstone 5.x,
GNU syntax, 32-bit. `file_offset = va + 0x52E54` (object 0).

```python
from capstone import *
d = open('data/game/C/PRAGE.EXE', 'rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_32)
def dis(va, n):
    for i in md.disasm(d[va + 0x52E54: va + 0x52E54 + n], va):
        print(hex(i.address), i.bytes.hex(), i.mnemonic, i.op_str)
dis(0x13c70, 219)   # spawn
dis(0x13df0, 53)    # clear
dis(0x13adc, 96)    # free-list build
dis(0x249b0, 15); dis(0x249c0, 16); dis(0x249d0, 31)
dis(0x13420, 133)   # teardown
dis(0x1b544, 200)   # resolver
```

```python
# jump table at cs:0x3408 -> linear 0x13408 -> file 0x6625C, six 32-bit offsets
d = open('data/game/C/PRAGE.EXE', 'rb').read()
for i in range(6):
    v = int.from_bytes(d[0x13408 + i*4 + 0x52E54: 0x13408 + i*4 + 0x52E54 + 4], 'little')
    print(i, hex(0x10000 + v))   # case body VA
```

```python
# stride/count from the raw immediates
start, end, stride = 0x70b00, 0x7cce0, 0x814
print((end - start) // stride, (end - start) % stride, hex(start + ((end-start)//stride - 1)*stride))
```

---

## 8. Files and invariants

* This document only. No port, oracle, pin, driver, `data/`, or capture change.
* `data/` untouched; no capture re-run.
* Every claim is instruction-level (raw bytes at the file offsets above) or
  the decompiler line it corrects. The `0x249B0`/`0x249C0` link-field order is
  proved by three independent instructions (§3); the free-list answer is proved
  by the raw `0x121EB`/`0x2BBB8` call chain and the state write at `0x12351`.

### Concerns carried to Tasks 5–6

1. The plan's `effects_spawn(u32 source_rec, u32 byte_arg)` omits the `EBX`
   handle; the raw function needs it for `0x1B544`.
2. `res_resolve` covers only the `0x1000000` fast path; if the title handle's
   resource entry lacks that flag, Task 5 must port the `[block+8]`/on-demand
   branch or the resolve will differ from the original.
3. The state-0/state-1 split means `0x13ADC` builds the pool in the frame
   *before* the frame that calls `0x13C70`; the port must run `effects_init()`
   on the title entry path, not lazily at the spawn.
