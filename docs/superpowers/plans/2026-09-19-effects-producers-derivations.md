# Effects producers `0x13xxx` — raw-byte derivation (Task 1: `0x13D4C`)

Register-argument binding and written-offset derivation for the type-4 producer
`0x13D4C`, read from the shipped `data/game/C/PRAGE.EXE` with capstone (32-bit,
GNU syntax). Method as in `2026-09-18-title-residuals-args.md`: object-0 code maps
as `file_offset = va + 0x52E54`; a `DS_*` immediate `I` is linear `I + 0x80000`.

Reproduction (read-only):

```python
from capstone import *
d = open('data/game/C/PRAGE.EXE','rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_32)
def dis(va, n):
    for i in md.disasm(d[va + 0x52E54: va + 0x52E54 + n], va):
        print(hex(i.address), i.bytes.hex(), i.mnemonic, i.op_str)
dis(0x13d4c, 164)   # type-4 producer
dis(0x13c70, 219)   # type-3 producer, for the shared pop
```

---

## 1. Incoming registers (`0x13D4C`, file `0x66BA0`, size 164)

| role | register | raw evidence |
|---|---|---|
| source record | `EAX` | `0x13D51 89c6 mov esi, eax` (file `0x66BA5`) |
| byte arg (state reload) | `DL` | `0x13D98 88510d mov [ecx+0xd], dl` (file `0x66BEC`) |
| handle | **no register** — read from `[source_rec]` | `0x13D86 8b06 mov eax, [esi]` (file `0x66BDA`) immediately before `0x13D88 e8b7770000 call 0x1b544` (file `0x66BDC`) |

The handle is **not** an incoming register. `EAX` (source record) is copied to
`ESI`, and the resolver `0x1B544` is fed `[ESI] = *(u32 *)source_rec`. The only
caller, `0x415A2` in `0x41578` (file `0x3E214`), confirms it: it passes
`EAX = EBX` (the source record) and `EDX = 2` and never loads a handle.

**Port consequence.** The port has **no** handle parameter: it reads
`DSD(source_rec)` and resolves that, matching the raw. The test is falsifiable —
it sets the handle to index 0, offset 4, so `+0x10` copies `blk+8`/`blk+12`
rather than `blk+4`/`blk+8`; a port that used a handle of `0` would fail.

The `0x13D4C` prologue is `push ebx/ecx/esi/edi/ebp`; `EBX` is scratch
(`0x13D9B mov ebx, eax` = the resolved pointer), not an argument.

The same binding holds for the other two new producers (Tasks 2–3 own their full
derivations): `0x13E28` reads the handle at `0x13E66 mov eax, [ecx]` (file
`0x66CBA`) and takes no handle register; `0x13B3C` at `0x13B85 mov eax, [edi]`
(file `0x669D9`), likewise.

## 2. The shared free-list pop

`0x13C70` (file `0x66AC4`):

```
0x13c7f 8b1de8cc0700      mov  ebx, [0x7cce8]    ; free_head.next
0x13c85 81fbe8cc0700      cmp  ebx, 0x7cce8      ; self-linked -> empty
0x13c8b 7504              jne  0x13c91
0x13c8d 31db              xor  ebx, ebx          ; -> 0
0x13c8f eb1e              jmp  0x13caf
0x13c91 a03caf0100        mov  al, [0x1af3c]     ; save lock
0x13c96 880424            mov  [esp], al
0x13c99 c6053caf010001    mov  byte [0x1af3c], 1 ; lock = 1
0x13ca0 89d8              mov  eax, ebx
0x13ca2 e8290d0100        call 0x249d0           ; unlink(rec)
0x13ca7 8a0424            mov  al, [esp]
0x13caa a23caf0100        mov  [0x1af3c], al     ; restore lock
```

`0x13D4C` (file `0x66BA0`):

```
0x13d53 8b0de8cc0700      mov  ecx, [0x7cce8]    ; free_head.next
0x13d59 81f9e8cc0700      cmp  ecx, 0x7cce8
0x13d5f 7504              jne  0x13d65
0x13d61 31c9              xor  ecx, ecx          ; -> 0
0x13d63 eb1b              jmp  0x13d80
0x13d65 b401              mov  ah, 1
0x13d67 8a353caf0100      mov  dh, [0x1af3c]     ; save lock
0x13d6d 88253caf0100      mov  byte [0x1af3c], ah ; lock = 1
0x13d73 89c8              mov  eax, ecx
0x13d75 e8560c0100        call 0x249d0           ; unlink(rec)
0x13d7a 88353caf0100      mov  [0x1af3c], dh     ; restore lock
```

**Verdict: semantically identical.** Both implement the same four steps —
sentinel self-test (empty → 0), save lock, set lock, `0x249D0` unlink, restore
lock. They differ only in scratch registers (`EBX`/`AL`/`[esp]` vs
`ECX`/`DH`/`AH`), which is exactly the compiler register allocation the port does
not reproduce. `0x13E28` (file `0x66C7C`) and `0x13B3C` (file `0x66990`) carry
the same instruction sequence again. Factoring `effect_take_free()` is therefore
justified; the original itself also factors this exact pop at the uncalled
`0x133D0` (file `0x66224`, 55 bytes, zero callers — see §4).

## 3. Offsets written by `0x13D4C`

| offset | width | value | source instruction (file) |
|---|---|---|---|
| `+0x00` | dword | next link | `0x249D0`/`0x249B0` |
| `+0x04` | dword | prev link | `0x249D0`/`0x249B0` |
| `+0x08` | dword | `source_rec` (`ESI`) | `0x13D95 897108 mov [ecx+8], esi` (`0x66BE9`) |
| `+0x0C` | byte | `4` (type) | `0x13D91 c6410c04` (`0x66BE5`) |
| `+0x0D` | byte | `DL` (byte arg) | `0x13D98 88510d` (`0x66BEC`) |
| `+0x0E` | byte | `1` (state) | `0x13DCB c6470e01 mov byte [edi+0xe],1` (`0x66C1F`) |
| `+0x0F` | byte | `0` | `0x13D8D c6410f00` (`0x66BE1`) |
| `+0x10 + 4*i` | dword × count | `resolved[1+i]` | `0x13DAE 89480c mov [eax+0xc], ecx` (`0x66C02`) |
| `+0x410 + 4*i` | — | **not written** (unlike `0x13C70`) | — |
| `DS_0009AF3D` | byte | `++` (active count) | `0x13DD4..0x13DE4` (`0x66C28`..`0x66C38`) |

Copy loop (file `0x66BF1`+): `0x13D9D mov ebp,[esi+0xc]` reads the count from the
**source** record's `+0xC`; `test ebp,ebp; jle` skips a non-positive count;
`0x13DAB mov ecx,[ebx+4]` with `EBX = resolved`, then `EBX += 4` per iteration, so
the values are `resolved[1], resolved[2], …` stored at `rec+0x10, rec+0x14, …`.
The raw dereferences `resolved` unconditionally; the port's `resolved != NULL`
guard matches `effects_spawn`'s existing PORT note (0x1B544 can fail to resolve).

Head-insert: `0x13DCF call 0x249b0` with `EAX = 0x7cce0` (active head), `EDX =
EDI = rec` — front-inserts after the active sentinel, same as `0x13C70`.

## 4. Producer-set proof: types 1 and 5 are dead

The four functions that pop a record from the free list and then write its type
byte are:

| producer | type written | evidence |
|---|---|---|
| `0x13B3C` | `0` or `2` | `0x13C2F 88760c mov [esi+0xc], dh` (0) / `0x13C25 c6460c02` (2) |
| `0x13C70` | `3` | `0x13CC4 c6430c03` |
| `0x13D4C` | `4` | `0x13D91 c6410c04` |
| `0x13E28` | `6` | `0x13E71 c6430c06` |

No path writes type `1` or `5`, so both types are unreachable in the shipped EXE
and their `effects_step` bodies are dead. (Spec §5's phrasing "referenced by
exactly four functions" is imprecise: the free sentinel `0x7cce8` is *also*
referenced by `0x13ADC`'s build (`0x6693B`), by `effect_teardown`'s re-insert
(`0x13496`, file `0x662EA`), by `effects_step`'s removal re-inserts
(`0x1373B`/`0x137DA`), and by the uncalled pop helper `0x133D0` — none of which
writes a type. The *producer* set is exactly the four above, which is the claim
the types-1/5 proof needs.)

`0x133D0` (file `0x66224`) is an out-of-line copy of the same pop that returns
the record in `EAX`; a scan for `call 0x133D0` finds **zero** callers, so it
produces nothing. It does not weaken the dead-type proof.

## 5. Files and invariants

* This document records raw bytes only; no `data/` change.
* Every claim above carries its file offset and byte sequence for re-verification.

---

# Task 2: the type-6 producer `0x13E28`

`0x13E28` (file `0x66C7C`, 200 bytes, next function `0x13EF0`) is the
"darken toward a target" producer: `+0x10` is filled white (`0xFFFFFF`) and the
`effects_step` case-6 body darkens it toward the resolved block in `+0x410`.
Reproduction: `dis(0x13e28, 210)` with the mapping above.

## 6. Incoming registers (`0x13E28`, file `0x66C7C`)

| role | register | raw evidence |
|---|---|---|
| source record | `EAX` | `0x13E2D 89c1 mov ecx, eax` (file `0x66C81`) |
| byte arg (state reload) | `DL` | `0x13E78 88530d mov [ebx+0xd], dl` (file `0x66CCC`) |
| handle | **no register** — read from `[source_rec]` | `0x13E66 8b01 mov eax, [ecx]` (file `0x66CBA`) immediately before `0x13E68 e8d7760000 call 0x1b544` (file `0x66CBC`) |

As with `0x13D4C`, the handle is not an incoming register: `EAX` is copied to
`ECX`, and the resolver `0x1B544` is fed `[ECX] = *(u32 *)source_rec`. The
prologue `push ebx/ecx/esi/edi/ebp` (`0x13E28`, file `0x66C7C`) leaves `EBX`
free for the popped record. This is the Task 1 human ruling, binding for all
four producers.

The shared free-list pop is inlined at `0x13E2F..0x13E60` (file
`0x66C83`..`0x66CB2`) — same four steps as `0x13C70`/`0x13D4C`: self-test the
free sentinel (`cmp ebx, 0x7cce8`), save the lock `[0x1af3c]`, set it, call
`0x249D0` to unlink, restore it. `test ebx, ebx; je 0x13eea` returns 0 when the
pop yielded nothing. `effect_take_free()` covers this.

## 7. Offsets written by `0x13E28`

| offset | width | value | source instruction (file) |
|---|---|---|---|
| `+0x00` | dword | next link | `0x249D0`/`0x249B0` |
| `+0x04` | dword | prev link | `0x249D0`/`0x249B0` |
| `+0x08` | dword | `source_rec` (`ECX`) | `0x13E75 894b08 mov [ebx+8], ecx` (`0x66CC9`) |
| `+0x0C` | byte | `6` (type) | `0x13E71 c6430c06 mov byte [ebx+0xc], 6` (`0x66CC5`) |
| `+0x0D` | byte | `DL` (byte arg) | `0x13E78 88530d mov [ebx+0xd], dl` (`0x66CCC`) |
| `+0x0E` | byte | `1` (state) | `0x13ECB c6470e01 mov byte [edi+0xe], 1` (`0x66D1F`) |
| `+0x0F` | byte | `0x80` | `0x13E6D c6430f80 mov byte [ebx+0xf], 0x80` (`0x66CC1`) |
| `+0x10 + 4*i` | dword × count | `0x00FFFFFF` | `0x13E86 c74310ffffff00 mov dword [ebx+0x10], 0xffffff` (`0x66CDA`) |
| `+0x410 + 4*i` | dword × count | `resolved[1+i]` | `0x13EAB 89b00c040000 mov [eax+0x40c], esi` (`0x66CFF`) |
| `DS_0009AF3D` | byte | `++` (active count) | `0x13ED4..0x13EE4` (`0x66D28`..`0x66D38`) |

**Confirmed against the raw bytes**: type `6` at `0x13E71 c6430c06`, `+0x0F =
0x80` at `0x13E6D c6430f80`, and the white fill at `0x13E86
c74310ffffff00`. The fill loop advances `EBX` by 4 per iteration (`0x13E91
83c304`), so the dword `0x00FFFFFF` lands at `+0x10, +0x14, …`.

Two loops, each bound by the **source** record's `+0xC` read signed
(`test`/`jle` skips a non-positive count):

* white fill: `0x13E7D mov edx, [ecx+0xc]` (`0x66CD1`), `test edx,edx; jle`
  (`0x13E82`), body `0x13E86..0x13E96`.
* resolved copy: `0x13E98 mov eax, [ecx+0xc]` (`0x66CEC`); `EDI` holds `rec`
  (`0x13E5C mov edi, ebx`), `ESI` holds `resolved` (`0x13E7B mov esi, eax`).
  `0x13EA1 mov eax, edi; 0x13EA5 add eax, 4; 0x13EA8 mov esi, [edx+4]; 0x13EAB
  mov [eax+0x40c], esi` (`0x66CF5..0x66CFF`): after `0x13EA5 add eax, 4` the
  base is `eax = rec+4`, so the store `[eax+0x40c]` is `rec+0x410` from
  `resolved[1]`; `edx += 4` per iteration, so the sequence is
  `resolved[1], resolved[2], …`. A port that copied `resolved[0]` (or from
  `+0x10`) would fail the test.

**Loop ordering (observable).** The two loops are strictly sequential in the
raw: the white-fill loop's back-edge is `0x13E96 jl 0x13E86` (`0x66CEA`), and
the resolved-copy loop only starts after `0x13E98` (`0x66CEC`) sets up its own
pointer — `0x13E82..0x13E84` branches past the whole white loop, and
`0x13E9D test eax,eax; 0x13E9F jle` (`0x66CED`/`0x66CEF`) branches past the
whole resolved loop. Therefore all `count` white dwords at `+0x10` are written
before the first resolved dword lands at `+0x410`. The two address ranges
overlap when `count == 257`: white's `i = 256` writes `+0x10 + 1024 = +0x410`,
and resolved's `j = 0` writes `+0x410` too. Because the resolved loop runs
second, `+0x410` ends as `resolved[1]`. A merged loop (white then resolved per
`i`) would instead leave `0x00FFFFFF` at `+0x410`; the boundary regression test
uses `count == 257` to pin this (`+0x810` is the last dword, inside the `0x814`
record stride).

* The raw dereferences `resolved` unconditionally; the port keeps the
  `resolved != NULL` guard introduced in Task 1 (0x1B544 can fail), matching
  `effects_spawn`'s existing PORT note.

Head-insert: `0x13EBE mov eax, 0x7cce0` (active sentinel), `0x13EC9 mov edx,
edi = rec`, `0x13ECF call 0x249b0` (`0x66D23`) — front-inserts after the active
sentinel, same as `0x13C70`/`0x13D4C`. The lock is taken at `0x13EC3 mov
[0x1af3c], dl` (`DL = 1`) and released at `0x13EDE mov [0x1af3c], dh`
(`xor dh,dh` → 0), around the insert and the `DS_0009AF3D++`.

The test is falsifiable on the handle binding: it sets `DSD(source_rec) = 4`
(index 0, offset 4), so the copy reads `blk+8`/`blk+12`; a port that passed a
handle of `0` would read `blk+4`/`blk+8` and fail `+0x410`/`+0x414`.

---

# Task 3: the types-0/2 producer `0x13B3C`

`0x13B3C` (file `0x66990`, size 308, next function `0x13C70`) copies a run of
the resolved block into BOTH `+0x14` and `+0x414`, walking the source by a
signed offset, and selects type 0 or 2 from a flag. Reproduction:
`dis(0x13b3c, 308)` with the mapping above. Ghidra lists it `callers=0` — it is
dead in the shipped EXE, like `0x133D0`; there is no caller to corroborate the
register roles, so they are read directly from the body.

## 8. Incoming registers (`0x13B3C`, file `0x66990`)

| role | register | raw evidence |
|---|---|---|
| source record | `EAX` | `0x13B41 89c7 mov edi, eax` (file `0x66995`) |
| signed offset | `DL` | `0x13B43 88542408 mov [esp+8], dl` (`0x66997`); the arm test `0x13B93 84d2 test dl,dl` / `0x13B95 7d56 jge 0x13bed` |
| flag (type select) | `BL` | `0x13B47 885c2404 mov [esp+4], bl` (`0x6699B`); `0x13C1D 8a742404 mov dh,[esp+4]` / `0x13C21 84f6 test dh,dh` |
| count | `CL` | `0x13B4B 880c24 mov [esp], cl` (`0x6699F`); loop bound `0x13C16 8a0c24 mov cl,[esp]` |
| handle | **no register** — read from `[source_rec]` | `0x13B85 8b07 mov eax, [edi]` (`0x669D9`) immediately before `0x13B8B e8b4790000 call 0x1b544` (`0x669DF`) |

`__regparm3` maps `param_1 = EAX` (source record), `param_2 = DL` (offset),
`param_3 = ECX/CL` (count); Ghidra lost the fourth register `BL` (the flag) as
`unaff_BL`. This matches the port signature
`effects_spawn_scroll(source_rec, offset, count, flag)`.

The shared free-list pop is inlined at `0x13B4E..0x13B7F` (file
`0x669A2`..`0x669D3`) — same four steps as Tasks 1–2; `0x13B7D test ecx,ecx; je`
returns 0 when the pop yielded nothing. `effect_take_free()` covers it.

## 9. Does it increment `DS_0009AF3D`? — **No**

The function body (`0x13B3C..0x13C6F`) writes only `0x1AF3C` (the lock):
`0x13B68 88253caf0100 mov [0x1af3c], ah` (`DL=1`), `0x13B75` restore,
`0x13C4D 881d3caf0100 mov [0x1af3c], bl` (`BL=1`), `0x13C62 883d3caf0100 mov
[0x1af3c], bh` (`BH=0`). There is **no** access to `0x1AF3D` anywhere in the
range. The port therefore does **not** bump the active count: a scroll record
is inserted into the active list but `effects_active()` does not see it. This
is faithful to the raw; the other three producers do bump it.

## 10. Does the zero-flag arm set `+0x0E = 0`? — **Yes**

`0x13C1D 8a742404 mov dh,[esp+4]` loads the flag into `DH`; `0x13C21 84f6 test
dh,dh; 0x13C23 740a je 0x13C2F`. The zero path is:

```
0x13C2F 88760c  mov [esi+0xc], dh   ; type 0   (DH == 0)
0x13C32 88760e  mov [esi+0xe], dh   ; +0x0E = 0 (DH == 0)
```

The nonzero path is `0x13C25 c6460c02 mov [esi+0xc],2` /
`0x13C29 c6460e01 mov [esi+0xe],1`. **Confirmed.** With `+0x0E == 0`,
`effects_step`'s type-0 branch (`effects.c` case 0:
`if (DSB(rec+0x0e)==0) continue;`) skips the record forever and never reloads
the state byte, so a type-0 record never retires. The port reproduces this
faithfully (the test pins `+0x0C = 0`, `+0x0E = 0`); it is **not** "fixed".

## 11. Source walk and the `-0x80` / signed-offset handling

`0x13B90 8d5804 lea ebx,[eax+4]` sets `EBX = resolved + 4` (`&resolved[1]`).
The arm is chosen by `0x13B93 test dl,dl; 0x13B95 jge 0x13bed`: `DL` is a
**signed byte**, so `offset < 0` takes the descending arm, `offset >= 0` the
forward arm.

**Forward arm** (`0x13BED..0x13C1B`, file `0x669ED`):

```
0x13BED 8b442405   mov  eax,[esp+5]   ; edx = (s32)(s8)[esp+8] = offset
0x13BF1 c1f818     sar  eax,0x18
0x13BF4 c1e002     shl  eax,2          ; offset*4
0x13BF7 8d1403     lea  edx,[ebx+eax]  ; edx = resolved + 4 + 4*offset
0x13BFA 89c8       mov  eax,ecx        ; eax = rec
0x13BFC 31db       xor  ebx,ebx        ; i = 0
0x13BFE eb14       jmp  0x13c14
0x13C00 89d1       mov  ecx,edx        ; (loop) value ptr
0x13C02 83c004     add  eax,4
0x13C05 8b09       mov  ecx,[ecx]
0x13C07 43         inc  ebx
0x13C08 894810     mov  [eax+0x10],ecx ; rec+0x14 + 4*i
0x13C0B 83c204     add  edx,4
0x13C0E 898810040000 mov [eax+0x410],ecx ; rec+0x414 + 4*i
0x13C14 31c9       xor  ecx,ecx
0x13C16 8a0c24     mov  cl,[esp]       ; count
0x13C19 39cb       cmp  ebx,ecx
0x13C1B 7ce3       jl   0x13c00
```

A pre-tested `while` loop: `count` iterations, `i = 0..count-1`, storing
`resolved[1 + offset + i]` at `+0x14 + 4*i` and `+0x414 + 4*i`. The destination
base is `+0x14` (not `+0x10`): the store is at `[eax+0x10]` after `eax += 4`
from `rec`.

**Descending arm** (`0x13B97..0x13BEB`, file `0x66997`):

```
0x13B97 8b442405   mov  eax,[esp+5]
0x13B9B c1f818     sar  eax,0x18       ; eax = (s32)(s8)offset
0x13B9E 83f880     cmp  eax,-0x80
0x13BA1 750f       jne  0x13bb2
0x13BA3 31c0       xor  eax,eax
0x13BA5 8a0424     mov  al,[esp]       ; count
0x13BA8 c1e002     shl  eax,2
0x13BAB 01c3       add  ebx,eax        ; ebx = resolved + 4 + 4*count
0x13BAD 83eb04     sub  ebx,4          ;   ... - 4  => resolved + 4*count
0x13BB0 eb0f       jmp  0x13bc1
0x13BB2 31d2       xor  edx,edx
0x13BB4 8a1424     mov  dl,[esp]       ; count
0x13BB7 c1e202     shl  edx,2
0x13BBA c1e002     shl  eax,2          ; offset*4
0x13BBD 01d3       add  ebx,edx        ; ebx = resolved + 4 + 4*count
0x13BBF 29c3       sub  ebx,eax        ;   ... - 4*offset
0x13BC1 31d2       xor  edx,edx
0x13BC3 8a1424     mov  dl,[esp]       ; edx = count (zero-extended byte)
0x13BC6 85d2       test edx,edx
0x13BC8 7c53       jl   0x13c1d
0x13BCA 8d049500000000 lea eax,[edx*4]
0x13BD1 01f0       add  eax,esi        ; eax = rec + 4*count
0x13BD3 89d9       mov  ecx,ebx        ; (loop)
0x13BD5 83e804     sub  eax,4
0x13BD8 8b09       mov  ecx,[ecx]
0x13BDA 4a         dec  edx
0x13BDB 894818     mov  [eax+0x18],ecx ; rec+0x14 + 4*(count-j)
0x13BDE 83eb04     sub  ebx,4
0x13BE1 898818040000 mov [eax+0x418],ecx ; rec+0x414 + 4*(count-j)
0x13BE7 85d2       test edx,edx
0x13BE9 7c32       jl   0x13c1d
0x13BEB ebe6       jmp  0x13bd3
```

* `offset == -0x80` (`0x13B9E cmp eax,-0x80`): source starts at
  `resolved + 4*count = &resolved[count]`.
* otherwise: source starts at `resolved + 4 + 4*count - 4*offset =
  &resolved[1 + count - offset]`.

The loop is a **do-while** (`0x13BC6 test/jl` before the body only skips a
negative count, and the bottom `0x13BE7 test/jl` exits once `edx < 0`), so it
runs `count+1` times: the destination index runs `count` down to `0` inclusive
(`eaX = rec + 4*count` then `sub eax,4` each pass) while the source pointer runs
`sp, sp-1, ..., sp-count`. With `count == 0` it still writes index 0 once. This
asymmetry with the forward arm (`count` iterations) is the raw truth and the
port transcribes it; the port test pins `offset = -1, count = 2` →
`+0x14..+0x1C = resolved[2], resolved[3], resolved[4]` in descending
destination order, and `offset = -0x80, count = 2` → `resolved[0], resolved[1],
resolved[2]`.

## 12. Offsets written by `0x13B3C`

| offset | width | value | source instruction (file) |
|---|---|---|---|
| `+0x00` | dword | next link | `0x249D0`/`0x249B0` |
| `+0x04` | dword | prev link | `0x249D0`/`0x249B0` |
| `+0x08` | dword | `source_rec` (`EDI`) | `0x13C39 897e08 mov [esi+8], edi` (`0x66A...`) |
| `+0x0C` | byte | `flag ? 2 : 0` | `0x13C25 c6460c02` / `0x13C2F 88760c` |
| `+0x0D` | byte | `BL` (flag) | `0x13C3E 88460d mov [esi+0xd], al` |
| `+0x0E` | byte | `flag ? 1 : 0` | `0x13C29 c6460e01` / `0x13C32 88760e` |
| `+0x0F` | byte | `CL` (count) | `0x13C53 88460f mov [esi+0xf], al` (AL = `[esp]`) |
| `+0x10` | byte | `DL` (offset) | `0x13C47 884610 mov [esi+0x10], al` (AL = `[esp+8]`) |
| `+0x14 + 4*i` | dword × count | walked source dword | `0x13C08` / `0x13BDB` |
| `+0x414 + 4*i` | dword × count | the same dword | `0x13C0E` / `0x13BE1` |
| `DS_0009AF3D` | — | **not written** | — |

Head-insert: `0x13C56 b8e0cc0700 mov eax,0x7cce0` (active sentinel),
`0x13C45 89f2 mov edx,esi = rec`, `0x13C5D e84e0d0100 call 0x249b0` (`0x66A2D`)
— front-inserts after the active sentinel, same as the other producers. The
lock is set at `0x13C4D`, released at `0x13C62` around the insert.

**Named overlap risk — not reachable here.** The count is a byte register
(`CL`), so `count <= 0xFF`. In the forward arm the highest `+0x14` index is
`count-1 <= 254`, so `+0x14 + 4*i <= +0x40C`, and the highest `+0x414` index is
`254` (byte `+0x80C`): the runs stay disjoint (`+0x14..+0x40C` vs
`+0x414..+0x80C`). In the descending arm both runs use the same index
`0..count <= 255`, so `+0x14` tops out at `+0x410` and `+0x414` starts at
`+0x414`; they never share an address. The `i = j + 256` interleave the Task 2
test exercises at `count == 257` cannot occur for a byte count. The port's two
independent loops per arm (`+0x14` and `+0x414` written in the same iteration)
preserve the raw's per-iteration write order, so the stored values are exact
regardless.

**Null-resolve guard.** `0x13B90 lea ebx,[eax+4]` dereferences the resolver
result unconditionally; the port keeps the `resolved != NULL` guard the other
producers use (the fields and the list insert still run, only the copy is
skipped).

The test is falsifiable on the binding: it passes `offset = 0` with handle 0
(`DSD(source_rec) == 0`), so the forward copy reads `blk+4`/`blk+8`; the raw
stores those at `+0x14`/`+0x18` and `+0x414`/`+0x418`. A port that read from
`+0x10`, or used offset `-128`'s base, would fail.
