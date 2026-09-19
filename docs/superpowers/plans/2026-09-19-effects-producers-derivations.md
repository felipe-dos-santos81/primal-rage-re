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
