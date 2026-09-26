# Front-end chain: the uncharacterized functions — raw-byte derivation (Task 2)

Register-level derivation of the five uncharacterized helpers (`0x2F4BC`, `0x12658`,
`0x1EA08`, `0x29B74`, `0x41578`), the four camera/scene functions (`0x1317C`,
`0x1324C`, `0x13290`, `0x1333C`) and the state-5 register handoff, from the shipped
`data/game/C/PRAGE.EXE`. This is the record Tasks 3–7 implement from. It changes no
source.

Everything below is read from the shipped bytes with `capstone` in **32-bit** mode.
On any conflict between the plan/brief text and the bytes, the bytes win and the
conflict is recorded in §0.2.

---

## 0. Addressing, reproduction, and corrections

### 0.1 The image and the two file-offset formulas

`PRAGE.EXE` is a Microsoft DOS/4GW **bound** executable with an LE image at file
`0x290A4` (`tools/le_info.py`). Its object table is:

```
[0] code  base 0x10000  size 0x63B15  pages=100 pageidx=1   flags 0x2045 (read|exec|32-bit)
[1] data  base 0x80000  size 0x8B0D0  pages=113 pageidx=101 flags 0x2043 (read|write|32-bit)
```

**The brief's disassembly mode is wrong.** The brief says
`Cs(CS_ARCH_X86, CS_MODE_16)`; the image is a 32-bit LE (`flags 0x2045/0x2043`,
and the decompilation is full of `eax/edx/…`). `CS_MODE_16` decodes every function
as garbage — e.g. `0x2F4BC` decodes as `push si; mov si,[di]; xor al,0x5f …`, while
`CS_MODE_32` gives the real `push esi; mov esi,[0x85f34]; call 0x2F198 …` (raw file
form; post-fixup `[0x105f34]`). All listings here use
`Cs(CS_ARCH_X86, CS_MODE_32)`.

**The brief's file-offset formulas are right.** For the code object,
`file_offset = VA + 0x52E54` (`VA 0x10000` → file `0x62E54`); for the data object,
`file_offset = VA + 0x46E54` (`VA 0x80000` → file `0xC6E54`). Dumping the LE page map
confirms all 213 logical pages map linearly (`phys == logical`; zero non-linear
entries), so both formulas hold for every file-backed address. The prior cycle's
`docs/superpowers/plans/2026-09-17-actor-system-args.md` verified the code formula
independently. The data object is file-backed only up to
`VA 0x80000 + 113*0x1000 = 0xF1000`; addresses above it (e.g. `0x104AE8`, `0x105F34`)
are **BSS**, zero-filled at load, and no file formula yields bytes for them. The port
maps and fixes up the image in `port/src/mem.c` (`mem_load_le` +
`mem_load_le_fixups`); the derivation below uses a Python replica of those two
functions.

**Raw operands are pre-relocation.** In the file bytes a code address is encoded as
`VA - 0x10000` and a data address as `VA - 0x80000`; the LE fixup records add the
object base. Two consequences for reading the raw file directly:

* `mov ecx,0x12c` is a literal (no fixup);
* `mov eax,0x1ac44` is the *data* operand for `0x9AC44` (fixup adds `0x80000`);
* `mov edx,0x19b74` is the *code* operand for `0x29B74` (fixup adds `0x10000`).

So a raw-file listing shows DS-relative data operands (e.g. `[0x70a58]` = `DS_000F0A58`)
and obj-relative code immediates. Every listing below is taken **after** applying the
LE fixups, so operands are absolute linear addresses (`[0xf0a58]`, `0x9ac44`).

Reproduction (read-only; this is the shape of every listing in this document):

```python
# map the LE objects (linear pages: phys == logical) into a 0x2000000 flat image,
# then apply the internal 32-bit fixups exactly as port/src/mem.c's mem_load_le +
# mem_load_le_fixups do.
# LE header 0x290A4, bound base 0x26654, page data = bound + 0x3C800, page size 0x1000.
# Code object file = VA + 0x52E54; data object file = VA + 0x46E54 (file-backed to
# VA 0xF1000; above that is BSS). For each object: map its pages, then for each fixup
# record (src=0x07, tf&~0x50==0) write base(target_obj) + target_off at src_off.
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
md = Cs(CS_ARCH_X86, CS_MODE_32)
for i in md.disasm(bytes(MEM[0x2f4bc:0x2f4bc+20]), 0x2f4bc):
    print('%#08x  %-22s %s %s' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
```

Decompilation quotations are from `port/decomp/prage.c` (line numbers cited inline).

### 0.2 Corrections against the plan/brief (raw wins)

1. **Disassembly mode.** `CS_MODE_16` → `CS_MODE_32` (§0.1).
2. **Data-object file offset (this record's earlier claim corrected).** The first
   version of this record said the data object's pages are non-linear and must be
   page-walked. That is wrong: dumping the page map shows `phys == logical` for all
   213 logical pages (zero non-linear entries), so `VA + 0x46E54` is valid for every
   file-backed data address. `0x104AE8` is a **BSS** address (file-backed data ends
   at `VA 0xF1000`), which is why no file offset resolves it — not a non-linearity
   (§0.1).
3. **`0x12658` spawns three actors, not two.** The brief/plan say "`0x2AE14` twice
   with different arguments". The raw calls `0x2AE14` **three** times
   (`0x12672`, `0x1269D`, `0x126CA`) from descriptors `0x9AC44`, `0x9AC58`, `0x9AC6C`
   (§2).
4. **The two call sites do not share a predicate.** The brief/plan say both
   `0x29B74` and `0x41578` walk the list and call `0x13D4C` for records matching
   `0x3E688`. The raw shows `0x29B74` has **no predicate at all** (every non-empty
   list entry) and `0x41578` matches `*rec == 0x3E688 || *rec == 0x88874B0`. The two
   predicates differ; that difference is the finding, not a typo (§4).
5. **Neither function registers into a process table.** The brief/plan say both
   register into the update/render tables. The raw shows neither `0x29B74` nor
   `0x41578` appears in either 32-entry table; `0x29B74` is installed as the
   mode-`0x17` handler at `DS_00104AE4` and `0x41578` is reached by direct call. The
   `DS_00104AE4` global is the state-function pointer, **not** the update mask
   `DS_00104AE8` (they are adjacent: `0x104AE4`/`0x104AE8`/`0x104AEC`) (§4.3).
6. **The camera/scene functions are not an effect draw.** They are the fight-camera
   state updaters (camera x/y tracking and a screen-shake decay). **None of the four
   draws**; the consumer of their state is the existing render pass (`0x14328` and
   the actor-pset sync `0x2A690`), which already reads `DS_000F0AEC`/`DS_000F0AF0`
   in the port (`port/src/platform/render.c:167,236`, `port/src/game/actors.c:806,
   825,848,891`) (§5).
7. **The state-5 register handoff is `DS_000F0A6A = 0x12C`, `DS_000F0A6F = 0`.** The
   decompiler's `extraout_CX`/`extraout_DH` are the raw's `mov ecx,0x12c` (before the
   call sequence) and `xor dh,dh` (before the last two calls). Both survive because
   every intervening callee preserves the register (§6).
8. **The decompiler's store order in `0x11D04` case 5 is not the raw order.** Raw:
   `DS_000F0A6F = DH`, then `DS_000F0A72 = BL`, then `DS_000F0A6A = CX`, then
   `DS_000F0A6C = 6`, then `DS_000F0A64 = 9` (§6).

### 0.3 Addresses derived in this document

| function | VA | file (obj-0) | decompiler | size | call sites (raw) |
|---|---|---|---|---|---|
| `0x2F4BC` | `0x2F4BC` | `0x82310` | `:19185` | 20 | 59 (functions.csv) / 61 (raw scan) |
| `0x12658` | `0x12658` | `0x654AC` | `:1586` | 198 | 1 (`0x12592`) |
| `0x1EA08` | `0x1EA08` | `0x7185C` | `:10174` | 559 | 5 (3 functions, §3) |
| `0x29B74` | `0x29B74` | `0x7C9C8` | `:14991` | 81 | 1 (`0x27B17`) + ptr stores |
| `0x41578` | `0x41578` | `0x943CC` | `:27708` | 146 | 4 (`0x41755`,`0x41DE6`,`0x42337`,`0x42352`) |
| `0x1317C` | `0x1317C` | `0x65FD0` | `:2138` | 122 | 3 (`0x12DE9`,`0x1321D`,`0x1323C`) |
| `0x1324C` | `0x1324C` | `0x660A0` | `:2166` | 67 | 0 direct (update-table entry 0) |
| `0x13290` | `0x13290` | `0x660E4` | `:2183` | 169 | 1 (`0x12D6C`) |
| `0x1333C` | `0x1333C` | `0x66190` | `:2238` | 134 | 1 (`0x12D73`) |

---

## 1. `0x2F4BC` — text cursor hold (already ported)

**This helper already exists in the port** as `text_cursor_hold`
(`port/src/game/actors.c:1541`, declared `port/src/game/actors.h:83`). The raw
confirms the existing transcription byte-for-byte.

Body (`0x2F4BC`–`0x2F4CF`, file `0x82310`–`0x82323`), post-fixup operands (the
raw file stores the same instructions with the DS-relative operand `[0x85f34]`,
bytes `8b35345f0800`; the LE fixup adds the data base `0x80000`):

```
0x2f4bc  56                push esi
0x2f4bd  8b35345f1000      mov  esi, [0x105f34]     ; DS_00105F34
0x2f4c3  e8d0fcffff        call 0x2f198            ; 0x2F198 = text_cursor_set
0x2f4c8  8935345f1000      mov  [0x105f34], esi     ; restore the two-word cursor
0x2f4ce  5e                pop  esi
0x2f4cf  c3                ret
```

`0x2F198` (`prage.c:18974`; file `0x81FEC`) is the text cursor
setter already ported as `text_cursor_set`: `EAX = col` (`-1` centers on the row),
`EDX = row` (`-1` reuses the cursor), `EBX = string pointer`, `ECX = mode`; it emits
the string through `0x2F830` and writes `{row, col+glyphs}` as two words at
`DS_00105F34`. `0x2F4BC` calls it with the cursor saved and restored, i.e. **"draw
this string at this position without moving the cursor"**.

What it allocates/spawns: `0x2F198 → 0x2F830` walks the string; each **non-space**
character is spawned as an actor through `0x2F5A0 → 0x2AE14` into the 31×43 glyph
grid at `DS_00105F38`, and an existing occupant is released through `0x2AD40`
(`port/src/game/actors.c:1369-1489`, already ported).

### 1.1 The 8/12/13 call counts in state 4 (`0x11578`)

`0x11578` (`prage.c:1025`, file `0x643CC`) is a 5-case switch on `DS_0009AD98`. The
three setup cases are unrolled sequences of `0x2F4BC` calls; the counts are literal:

* **case 0** (`0x1159A`–`0x116BE`, file `0x643EE`): `0x2C3FC(0x100)`, `0x4F1E4`,
  `0x2BAF4(1)`, `0x2C06C(0x1D)`, `0x2AE14(0x9AD84, …)`, then **8** `0x2F4BC` calls
  (`0x115E8`…`0x11696`), then `DS_000F0A76=0xB4`, `DS_000F0A74=1`, `DS_0009AD98=4`.
* **case 1** (`0x116BF`–`0x1183E`): same setup minus `0x2C06C`, then **12** `0x2F4BC`
  calls (`0x11703`…`0x11815`), then `DS_0009AD98=4`, `DS_000F0A76=0xB4`,
  `DS_000F0A74=2`.
* **case 2** (`0x1183F`–`0x119D9`): same setup, then **13** `0x2F4BC` calls
  (`0x11883`…`0x119B1`), then `DS_000F0A76=0xB4`, `DS_000F0A74=3`, `DS_0009AD98=4`.

**Why the count differs:** the raw has three literal, unrolled credit-roll pages of
8, 12 and 13 lines. There is **no loop** and no relation to any live-list count — the
count is baked into the instruction stream. This is the answer to Step 1's question,
and it is not data-driven.

The strings are **direct data-object pointers**, not localisation ids (there is no
`0x1C500` call before these sites). After the LE fixups the first call in each page
passes `EBX = 0x8005C`; the raw file shows `mov ebx,0x5c` (fixup to obj-1). The
strings are the credits roll, starting at `0x8005C`:

```
0x8005C  "T E A M    R A G E"
0x80070  "ORIGINAL COIN-OP DEVELOPMENT"
0x80090  "Dennis Harper        Producer/Programmer"
0x800BC  "Frank Kuan           Programmer/Designer"
0x800E8  "Jason Leong          Art Director"
...
```

The `(col,row,mode)` triples are literal; the full per-call argument list is in §8.

---

## 2. `0x12658` — the state-3 handoff spawner

`0x12658` (file `0x654AC`, `prage.c:1586`), called once at the end of state-3
phase 0 (`0x12484` phase 0, `0x12550`).

### 2.1 Three `0x2AE14` spawns (not two)

`0x2AE14`'s register binding is pinned in
`docs/superpowers/plans/2026-09-17-actor-system-args.md` §0: `EAX = descriptor`,
`EDX = arg2`, `ECX = arg3`, `EBX = arg4`, `[stack] = arg5`. The port's
`actor_spawn(desc, a2, a3, a4, a5)` matches (`a2=EDX, a3=ECX, a4=EBX`).

```
; call 1  (0x1265C-0x12672)
0x1265c  6a00              push 0
0x1265e  b9e0000000        mov  ecx, 0xe0
0x12663  bb005a0000        mov  ebx, 0x5a00
0x12668  ba002a0000        mov  edx, 0x2a00
0x1266d  b844ac0900        mov  eax, 0x9ac44        ; raw 0x1ac44 + 0x80000
0x12672  e89d870100        call 0x2ae14
0x12677  66c74036c0ff      mov  word [eax+0x36], 0xffc0
0x1267d  a3580a0f00        mov  [0xf0a58], eax      ; DS_000F0A58 = first actor
```

⇒ `actor_spawn(0x9AC44, 0x2A00, 0xE0, 0x5A00, 0)`, then
`first[+0x36] = 0xFFC0`, then **`DS_000F0A58 = first`** (the binding the brief asks
for).

```
; call 2  (0x12682-0x1269D)
0x12682  668b4056          mov  ax, [eax+0x56]      ; first[+0x56]
0x12686  80cc04            or   ah, 4              ; | 0x400
0x12689  b9e2000000        mov  ecx, 0xe2
0x1268e  25ffff0000        and  eax, 0xffff
0x12693  31db              xor  ebx, ebx
0x12695  50                push eax                 ; arg5 = (first[+0x56] & 0xffff) | 0x400
0x12696  31d2              xor  edx, edx
0x12698  b858ac0900        mov  eax, 0x9ac58
0x1269d  e872870100        call 0x2ae14
0x126a2  89c6              mov  esi, eax             ; esi = second
0x126a4  a1580a0f00        mov  eax, [0xf0a58]       ; first
0x126a9  8a5e56            mov  bl, [esi+0x56]       ; second[+0x56]
0x126ac  88584b            mov  [eax+0x4b], bl       ; first[+0x4b] = second[+0x56]
```

⇒ `actor_spawn(0x9AC58, 0, 0xE2, 0, (first[+0x56] & 0xFFFF) | 0x400)`, then the
field copy **`first[+0x4B] = second[+0x56]`**.

```
; call 3  (0x126AF-0x126D2)
0x126af  668b4056          mov  ax, [eax+0x56]      ; first[+0x56] again
0x126b3  80cc04            or   ah, 4
0x126b6  b9e2000000        mov  ecx, 0xe2
0x126bb  25ffff0000        and  eax, 0xffff
0x126c0  31db              xor  ebx, ebx
0x126c2  50                push eax
0x126c3  31d2              xor  edx, edx
0x126c5  b86cac0900        mov  eax, 0x9ac6c
0x126ca  e845870100        call 0x2ae14
0x126cf  8a4056            mov  al, [eax+0x56]       ; third[+0x56]
0x126d2  88464b            mov  [esi+0x4b], al       ; second[+0x4b] = third[+0x56]
```

⇒ `actor_spawn(0x9AC6C, 0, 0xE2, 0, (first[+0x56] & 0xFFFF) | 0x400)`, then
**`second[+0x4B] = third[+0x56]`**.

The three descriptors (obj-1 data, first six dwords):

```
0x9AC44: 0x000E899A 0x00000000 0x00802000 0x00001000 0x41975EC 0x000E89DA
0x9AC58: 0x000E89DA 0x00040000 0x00802000 0x00001000 0x419766C 0x000E89E8
0x9AC6C: 0x000E89E8 0x00080000 0x00802000 0x00001000 0x41977EC 0x000E89F6
```

### 2.2 The list walk and the effect spawn

```
0x126d5  31c0              xor  eax, eax
0x126d7  e828120200        call 0x33904            ; frontend_list_next(0)
0x126dc  89c1              mov  ecx, eax
0x126de  85c0              test eax, eax
0x126e0  742e              je   0x12710
0x126e2  813988e60300      cmp  dword [ecx], 0x3e688
0x126e8  7419              je   0x12703            ; skip if *rec == 0x3E688
0x126ea  89c8              mov  eax, ecx
0x126ec  e8e39f0000        call 0x1c6d4            ; frontend_resource_known
0x126f1  84c0              test al, al
0x126f3  750e              jne  0x12703            ; skip if known
0x126f5  ba06000000        mov  edx, 6
0x126fa  89c8              mov  eax, ecx
0x126fc  8b19              mov  ebx, [ecx]          ; handle = *rec
0x126fe  e86d150000        call 0x13c70            ; effects_spawn
0x12703  89c8              mov  eax, ecx
0x12705  e8fa110200        call 0x33904
...
0x12710  31d2              xor  edx, edx
0x12712  668915447a1000    mov  word [0x107a44], dx  ; DS_00107A44 low word = 0
```

So for every non-empty list entry `rec` with `*rec != 0x3E688` and
`frontend_resource_known(rec) == 0`:

```
effects_spawn(rec, 6, *rec)      ; 0x13C70
```

`0x13C70` (`prage.c:2697`; file `0x66AC4`) writes: `rec+0x0C = 3` (type), `rec+0x0F = 0x80`, `rec+0x08 = source_rec`,
`rec+0x0D = byte_arg`, and copies `resolved[1+i]` to `rec+0x10…` for
`i < *(int*)(source_rec+0x0C)`. The port's `effects_spawn(source_rec, byte_arg,
handle)` matches (`port/src/game/effects.c:119`). Here `byte_arg = 6`.

Finally `DS_00107A44`'s low 16 bits are zeroed.

---

## 3. `0x1EA08` — the state-5 match-start builder

`0x1EA08` (file `0x7185C`, `prage.c:10174`). It has **five direct call sites in
three functions**, not one:

* `0x11DF7` — `0x11D04` case 5 (state 5), `prage.c:1222`.
* `0x1F140`, `0x1F278`, `0x1F39B` — `FUN_0001EEB0` (the match sub-state machine,
  `prage.c:10374`), cases 2, 6 and 9 respectively (`prage.c:10409`, `:10464`,
  `:10514`); the case-6/9 calls are guarded by `DAT_00104abc == 1`.
* `0x11A42` — `FUN_00011A30` (a function Ghidra did not emit; raw body
  `0x11A30`–`0x11A88`, `ret` at `0x11A88`). It is **unreachable in the shipped
  image**: no `call`/`jmp` targets `0x11A30`, and the value `0x11A30` appears nowhere
  in either LE object, so it is neither installed as a pointer nor reached
  indirectly.

**Downstream impact.** A Task-3 implementer told "only state 5" would wire `0x1EA08`
into state 5 alone and miss the match-start triggers at `FUN_0001EEB0` cases 2/6/9
(and would silently drop the dead `0x11A30` copy). The live call sites are the four
in state 5 and `FUN_0001EEB0`. The body and globals below are unchanged.

Full fixed-up body:

```
0x1ea08  push ebx/ecx/edx/esi/edi/ebp ; sub esp, 0x34
0x1ea11  mov eax,4 ; call 0x4f1e4            ; frontend_input_reset (eax ignored, see below)
0x1ea1b  xor ecx,ecx ; xor ebx,ebx ; mov eax,1 ; xor edx,edx ; call 0x2baf4  ; actors_reset(1)
0x1ea2b  mov eax,0xa7b6c ; call 0x38b18      ; frontend_spawn_row(0xA7B6C, 0, 0)
0x1ea35  mov edx,0x3000 ; mov [esp+0x24],edx ; local_28 = 0x3000
0x1ea40  mov edx,1 ; mov ecx,2 ; call 0x2dbc4 ; esi = resource/string blob
0x1ea4f  push 0x3000 ; push 1 ; mov ebx,1 ; xor eax,eax ; mov edx,ecx(=2)
0x1ea61  mov al,[0xa7b95] ; call 0x2f4d0      ; formatted text draw (row 2)
0x1ea6b  copy 0x12 bytes from [esi+4] to the stack buffer; [esp+0x12]=0
0x1ea8f  mov ecx,0x2000 ; mov ebx,esp ; xor ah,ah ; xor eax,eax
0x1ea9e  mov edx,2 ; mov al,[0xa7b96] ; call 0x2f4bc   ; text_cursor_hold
0x1eaad  push 0x2000 ; push 1 ; mov ecx,7 ; mov edx,2 ; xor eax,eax
0x1eac0  mov ebx,[esi] ; mov al,[0xa7b97] ; call 0x2f4d0
0x1eacc  scan i=0..9: eax = tableL[i] ; if [esi+0x16] == *(u8*)eax -> local = i
                                            ; tableL = 0xA7DA0 (letter pointers)
0x1eb04  if (local > 6) local = 0            ; 0x1EB0A cmp eax,6 / jle keeps 0..6
0x1eb15  loop i=0..9:
           0x1eb2f  call 0x2dbc4(i, 0) ; esi = blob
           0x1eb5a  call 0x2f4d0(eax=table2[i], edx=table0[i], ebx=i+1, ecx=2, push 1, push 0x3000)
           0x1eba6  call 0x2f4bc(eax=table2[i], edx=table0[i], ebx=esp, ecx=0x3000)
           0x1ebc7  call 0x2f4d0(eax=table3[i], edx=table0[i], ebx=[esi], ecx=7, push 1, push table1[i])
           0x1ebe2  call 0x2f4bc(eax=table2[i], edx=table0[i], ebx=esp, ecx=0x3000)
0x1ebfe  push 0 ; mov ecx,0xff ; mov al,[esp+0x34](=local)
0x1ec0b  mov ebx,0x1c80 ; mov edx,0x2a00
0x1ec15  mov eax,[local*4 + 0xa7dcc]          ; descriptor table
0x1ec1c  call 0x2ae14                          ; actor_spawn(desc, 0x2A00, 0xFF, 0x1C80, 0)
0x1ec21  mov ebx,0x105fd30 ; xor edx,edx ; call 0x2a17c   ; set the spawned pset
```

`0x4F1E4` ignores the incoming `eax` (it sets its own `eax=0x2700` and writes
`DS_00104B15 = 0`; raw at file `0x5D038`). So the `mov eax,4` is inert.

### 3.1 The four data tables (all obj-1)

| address | shape | contents |
|---|---|---|
| `0xA7B6C` | 20-byte descriptor | `{0x2C4F, 0, 0x00802800, 0x00001000, 0x0A0A13B4}` |
| `0xA7B94` | 10 × 4 bytes, stride 4 | `0E 03 06 1D`, `10 0C 0F 15`, `12 0C 0F 15`, `14 0C 0F 15`, `17 02 05 0B`, `19 02 05 0B`, `1B 02 05 0B`, `17 16 19 1F`, `19 16 19 1F`, `1B 16 19 1F` |
| `0xA7DA0` | 10 pointers (fixed up) | `0x8090C,0x80910,0x80914,0x80918,0x8091C,0x80920,0x80924,0x80928,0x80928,0x80928` → `"R","K","T","C","S","D","H","X","X","X"` |
| `0xA7DCC` | 7 descriptors | `0xBB6DC,0xBB6F0,0xBB704,0xBB718,0xBB72C,0xBB740,0xBB754` |

The byte table `0xA7B94[i]` = column, `[i+1]` = row, `[i+2]`/`[i+3]` are the two
formatted-draw modes. The pointer table's single letters (`R K T C S D H X`) are the
character/button labels; the scan at `0x1EADB` loads `tableL[i]` from `0xA7DA0`
(`0x1EADB mov eax,[eax*4 + 0xa7da0]`), compares `[esi+0x16]` against the byte it
points to (`0x1EAE5 cmp dl,[eax]`), and sets `local = i`. The guard at `0x1EB0A`
(`cmp eax,6` / `jle`) keeps indices `0..6`, so `'H'` (index 6) selects the seventh
descriptor `0xBB754`; an index of `7..9` falls back to `0`. This is a **match-start
roster/character page**: draw the per-character name rows, then spawn the selected
character's actor.

### 3.2 The helper chain (partly unmodeled)

* `0x2DBC4` (file `0x81018`, `prage.c:17958`) calls `0x2DB58` to read a paged
  resource by index and decodes it (a run-length/escape decoder writing
  `DS_00105EFC` and `DS_00105F00`). The returned pointer is used as
  `{+4: first string, +0: next}` by the consumer (`0x1EA6B`, `0x1EAC0`, `0x1EBC7`) —
  that layout is **read off the consumer, not decoded from `0x2DB58`**. **`0x2DB58`
  is the paged-memory manager the port does not model for this resource class** —
  named gap §7.1.
* `0x2F4D0` (file `0x82324`, `prage.c:19200`)
  is `0x2EFD4` (a `sprintf`-style formatter, `prage.c:18819`) followed by
  `0x2F198` — "draw formatted text at (col,row)". Its `EAX`/`EDX` are the col/row
  that reach `0x2F198`; `EBX`/`ECX` and the two stack words feed the formatter
  `0x2EFD4`, whose exact output is **not decoded** (named gap §7.2).
  (Derived since, demo-pose record §33: `0x2EFD4` is `"%i"` of EBX fitted to
  width ECX by the stack pad 0..3; `0x2F4D0`/`0x2EFD4` are ported as
  `text_number_draw`/`text_number_format`.)
* `0x38B18` (file `0x8B96C`, `prage.c:23798`) — `frontend_spawn_row`. The binding is
  pinned by the raw, not assumed: `0x38B1B mov esi,eax` (descriptor),
  `0x38B1D mov ebp,edx` (a2), `0x38B51 shl ebx,3` (a3<<3), `0x38B54 lea edx,[ebp*8]`
  (a2<<3), `0x38B4C mov ecx,2`, `0x38B4A push 0`, `0x38B5D call 0x2AE14`, then
  `0x38B62 mov [edi*4 + 0x107a1c],eax` into the first free of the 7 slots. So
  `frontend_spawn_row(desc, a2, a3)` = `actor_spawn(desc, a2<<3, 2, a3<<3, 0)`.
* `0x2A17C` (file `0x7D7D0`, `prage.c:15272`) sets the spawned actor's pset flags
  from `rec+0x56` and, because `EBX = 0x105FD30` is non-zero, appends the
  `0x105FD30` entry through `0x33754` and stores it at `pset+0x18`.

### 3.3 Globals `0x1EA08` writes

`0x1EA08` writes **no mem[] global directly** (all its stores are to its own stack
frame). The globals it changes are all written through callees:

| global | written by | value |
|---|---|---|
| `DS_00104B15` | `0x4F1E4` | `0` |
| actor pool + `DS_001014EC` psets | `0x2BAF4`, `0x2AE14`, `0x2A17C` | reset / new actor / pset flags |
| `DS_00107A1C` (7 slots) | `0x38B18` | row spawned from `0xA7B6C` |
| `DS_00105EFC`, `DS_00105F00` | `0x2DBC4` | decoded resource words |
| `DS_00105F34` (cursor), `DS_00105F38` (glyph grid) | `0x2F4D0`, `0x2F4BC` | per-glyph actors |
| `DS_00105FD30` region | `0x2A17C → 0x33754` | pset link |

The **index-selection** `local` (which of the seven descriptors is spawned) depends on
`[esi+0x16]`, produced by `0x2DBC4`/`0x2DB58`; that value cannot be pinned from the
raw without modelling the paged resource (named gap §7.1). The rest of the body is
pinnable.

---

## 4. The two effect call sites — `0x29B74` and `0x41578`

### 4.1 `0x29B74` — no list predicate

`0x29B74` (file `0x7C9C8`, `prage.c:14991`). Fixed-up body:

```
0x29b74  push ebx/ecx/edx
0x29b77  call 0x13df0                 ; effects_clear: DAT_0009AF3C=1, drain 0x13420, DAT_0009AF3D=0
0x29b7c  xor  eax, eax
0x29b7e  call 0x33904                 ; frontend_list_next(0)
0x29b83  mov  ebx, eax
0x29b85  test eax, eax ; je 0x29ba2
0x29b89  mov  edx, 3
0x29b8e  mov  eax, ebx
0x29b90  call 0x13d4c                 ; effects_spawn_darken(rec, 3)
0x29b95  mov  eax, ebx ; call 0x33904 ; mov ebx,eax ; test ; jne 0x29b89
0x29ba2  mov  edx, 0x78
0x29ba7  mov  ecx, 0x15
0x29bac  mov  word [0x1088ee], dx     ; DS_001088EE = 0x78
0x29bb3  mov  word [0x104afe], dx     ; DS_00104AFE = 0x78
0x29bba  mov  word [0x104b00], cx     ; DS_00104B00 = 0x15
```

**Predicate: none.** Every non-empty entry returned by `0x33904` gets
`effects_spawn_darken(rec, 3)` (`0x13D4C`, `prage.c:2755`). There is no comparison
against `0x3E688`, no `0x1C6D4` test, nothing. The walk has no filter at all.

### 4.2 `0x41578` — a two-handle predicate

`0x41578` (file `0x943CC`, `prage.c:27708`). Fixed-up body:

```
0x41578  push ebx/ecx/edx/esi
0x4157c  xor  eax, eax ; call 0x33904
0x41583  mov  ebx, eax ; test ; je 0x415b4
0x41589  mov  edx, [ebx]              ; edx = *rec  (the +0 handle)
0x4158b  cmp  edx, 0x3e688 ; je 0x4159b
0x41593  cmp  edx, 0x88874b0 ; jne 0x415a7
0x4159b  mov  edx, 2
0x415a0  mov  eax, ebx ; call 0x13d4c ; effects_spawn_darken(rec, 2)
0x415a7  mov  eax, ebx ; call 0x33904 ; mov ebx,eax ; test ; jne 0x41589
0x415b4  mov  ecx, 0x13
0x415b9  mov  esi, 0x15
0x415be  xor  edx, edx
0x415c0  mov  eax, [0x104abc]
0x415c5  mov  dl, [0x104b19]
0x415cb  xor  ebx, ebx
0x415cd  call 0x32a3c
0x415d2  mov  eax, 0x33 ; mov  edx, 0x78 ; call 0x2c3fc
0x415e1  mov  word [0x104afe], dx     ; DS_00104AFE = 0x78
0x415e8  mov  word [0x1088ee], bx     ; DS_001088EE = 0
0x415ef  mov  word [0x104afa], cx     ; DS_00104AFA = 0x13
0x415f8  mov  word [0x104b00], si     ; DS_00104B00 = 0x15
0x415ff  mov  byte [0x104b25], ah     ; DS_00104B25 = 0
```

**Predicate:** `*rec == 0x0003E688 || *rec == 0x088874B0`, then
`effects_spawn_darken(rec, 2)`.

**The predicates differ** (`0x29B74`: none; `0x41578`: two exact handles) and the
`byte_arg` differs (`3` vs `2`). Both call the same producer `0x13D4C` (type-4
darken). Recorded as a finding.

### 4.3 Process-table registration — the raw answer

Neither function appears in either process table. The two tables are static pointer
arrays in the data object; after applying the LE fixups (32 entries each):

```
update  DS_000A8644: 0001324c 00048f98 00019b90 0005d812 00037c8c 00022fe8 00025fac 0002910c
                     00034648 0003800c 00028f08 0004f890 00024150 00040554 000407ec 000260bc
                     00026194 00045d98 0005d812 … (0x5d812 repeats)
render  DS_000A86C4: 0004f4e8 0001d540 0005d812 0001dc0c 0004f5c8 0005d812 … (0x5d812 repeats)
```

* `0x29B74` is **not** in either table. It is installed as the `DS_00104AE4`
  **state-function pointer** (`prage.c:13833`, and again at `:14456`, `:14479`,
  `:14493`) — the handler for mode `0x17`. Raw: `0x2788B mov edx,0x19b74` /
  `0x278A4 mov [0x84ae4],edx`, fixup → `0x29B74`. `DS_00104AE4` is invoked at
  `0x4F302`, `0x4F373`, `0x4F6F1`, `0x4F70D`, `0x4F9AA`, `0x4F9D1` (the `0x17`
  handlers).
* `0x41578` is **not** in either table. It is reached by **direct call** at four
  sites in two functions — `0x416D4` (`0x41755`) and `0x41C28`
  (`0x41DE6`,`0x42337`,`0x42352`) — the decompiler's `:27790`, `:28048`, `:28218`,
  `:28223`.
* The global `DS_00104AE4` is adjacent to but distinct from the update mask
  `DS_00104AE8` and the render mask `DS_00104AEC`. The plan's Step 4 conflated
  `0x104AE4` (state pointer) with `0x104AE8` (update mask).
* `DS_000A8644[0] = 0x1324C` (the camera-bob updater) — a real update-table entry
  relevant to §5.

**Therefore "register both into the process tables" (plan Task 7) is a plan-vs-raw
conflict.** The raw equivalents are: `0x29B74` is the mode-`0x17` state handler;
`0x41578` is called directly by two state handlers. Task 7 must implement what the
raw does, not add table entries.

### 4.4 The `+0` handle meaning

Every front-end list entry (stride `0x10` at `DS_00107608`; `0x33904` returns
entries whose `+4` is non-zero) has at `+0` a **resource handle** in the paged-memory
handle space. `0x33754` (`prage.c:20751`, file `0x865A8`) stores the *incoming*
handle at `+0` and `resolved[0]` at `+0xC`:

```
0x3375a  mov  ecx, eax            ; ecx = handle
0x3375c  call 0x1b544             ; res_resolve(handle)
...
0x337b0  mov  [ebx+4], 1          ; refcount
0x337b7  mov  [ebx+8], esi        ; offset
0x337ba  mov  [ebx], ecx          ; +0 = handle
0x337bc  mov  eax, [edi]          ; resolved[0]
0x337c0  mov  [ebx+0xc], eax
```

`0x1B544` (the port's `res_resolve`, `port/src/platform/res.h:30`) decodes a handle
as `index = handle >> 23`, `offset = handle & 0x7FFFFF`, and returns
`page_table[index].base + offset`:

```
0x1b54a  shr  eax, 0x17           ; index
0x1b563  and  edx, 0x7fffff       ; offset
0x1b572  mov  eax, [ecx+0x10]     ; base
0x1b575  add  eax, edx
```

So:

| constant | index | offset | meaning |
|---|---|---|---|
| `0x0003E688` | `0` | `0x3E688` | a resource handle whose resolved data is at code-object `0x3E688` |
| `0x088874B0` | `17` | `0x874B0` | a resource handle |

`0x3E688` is inside the code object `[0x10000,0x73B15)` and is a data blob (the
front-end row/effect type tag already used as an opaque constant by
`port/src/game/flow.c:566`). `0x088874B0` is **not** inside either LE object and is
**above the port's `MEM_SIZE` (`0x4000000`)**; in the port's flat model no static or
heap object can carry it, so in the port the second half of the predicate can never
match unless an allocator produces that exact handle. Which asset each handle names
is a named gap (§7). The `+0` values are the same handle space that
`frontend_resource_known` (`0x1C6D4`, `prage.c:8124`) compares against its nine
`0x8099xx` handles.

---

## 5. The camera/scene functions

The four functions are the **fight-camera state updaters**. They maintain camera x
(`DS_000F0AF0`), camera y (`DS_000F0AEC`), a screen-shake accumulator
(`DS_000F0AF4`/`DS_000F0AF6`) and the camera mode (`DS_000F0AFE`). **None of them
draws**; their state is consumed by the existing render pass (`0x14328` reads
`DS_000F0AEC` at `prage.c:3167`, and the actor-pset sync reads both
`DS_000F0AEC`/`DS_000F0AF0`), which the port already implements
(`port/src/platform/render.c`, `port/src/game/actors.c`).

The dispatcher is `0x12D48` (`prage.c:1894`, file `0x65B9C`), called from the
per-frame update `0x24C5C` (`prage.c:12468`, `:12519`):

```
0x12d48  mov al,[0xf0afe] ; cmp al,4 ; ja 0x12d78
0x12d56  jmp cs:[eax*4 + 0x12d34]    ; table = {0x12D5E, 0x12D65, 0x12D6C, 0x12D73, 0x12D78}
         mode 0 -> 0x12DF0
         mode 1 -> 0x12E3C
         mode 2 -> 0x13290
         mode 3 -> 0x1333C
         mode 4 -> (no call)
0x12d78  clamp DS_000F0AF0 to [-0x5D00, 0x5D00]
```

### 5.1 `0x1324C` — screen-shake decay (update-table entry 0)

`0x1324C` (file `0x660A0`, `prage.c:2166`). `DS_000F0AF4` and `DS_000F0AF6` are
**16-bit signed words** (loaded/stored with `mov dx,[…]` / `mov [word]`, `test dx,dx`
is 16-bit):

```
0x1324c  push ebx/edx
0x1324e  mov  dx, [0xf0af6]        ; velocity
0x13255  mov  bx, [0xf0af4]        ; accumulated offset
0x1325c  add  ebx, edx             ; low 16 bits = bx + dx
0x1325e  test dx, dx ; jge 0x1327b
0x13263  test bx, bx ; jg  0x1327b
0x13268  mov  ah, [0x104ae8]
0x1326e  xor  edx, edx
0x13270  and  ah, 0xfe             ; clear update mask bit 0
0x13273  xor  ebx, ebx
0x13275  mov  [0x104ae8], ah
0x1327b  sub  edx, 0x20
0x1327e  mov  [0xf0af6], dx        ; velocity -= 0x20
0x13285  mov  [0xf0af4], bx        ; offset = bx (0 when it settled)
```

Semantics (signed 16-bit):
```
offset += velocity
if (velocity < 0 && offset <= 0) { velocity = 0; DS_00104AE8 &= ~1; offset = 0 }
velocity -= 0x20
```

**Finding (dormant process).** `0x1324C` is update-table entry 0, so it runs only
when `DS_00104AE8` bit 0 is set. An exhaustive scan of every instruction in the code
object that references `0x104AE8` shows **no store that sets bit 0** — the only
writes are `and …,0xFE/0xDF/0xFB/0xEF/0xFD` (clears), `or …,0x04/0x20/0x40` (bits
2/5/6), and `mov [0x104AE8],0` / register moves whose sources are zeroed. The
pattern `or byte [0x104AE8],1` occurs zero times. So the camera-bob process is
**never dispatched in the shipped path**; `0x1324C`'s own `and ah,0xFE` is a clear
that can only fire if some other path set bit 0, and none does. Recorded as a raw
finding; the port should not assume a live shake unless it wires a trigger.

### 5.2 `0x1317C` — camera-y clamp

`0x1317C` (file `0x65FD0`, `prage.c:2138`), called from three sites in three
functions — `0x12DA8` at `0x12DE9` (`prage.c:1923`), `0x131F8` at `0x1321D`, and
`0x13224` at `0x1323C`. All three set `DS_001078F4` (the high word of
`DS_001078F2`) from a player record's `+0` word and then call `0x1317C`; `0x12DA8`
branches on `DS_000F0AFE` (mode 0 reads `DS_001077E0 + DS_000F0AFF*0x94`, else the max
of `DS_001077E0`/`DS_00107874`), while `0x131F8`/`0x13224` are the two arms split out:

```
0x1317c  push edx ; sub esp,0xc
0x13180  mov  eax, [0x1078f2]      ; DS_001078F2
0x13185  sar  eax, 0x10            ; x = (s32)DS_001078F2 >> 16
0x13188  cmp  eax, 0x1400
0x1318d  jle  0x131c9
0x1318f  lea  edx, [eax-0x1400]    ; d = x - 0x1400
0x13198  fild dword [esp]          ; d
0x1319b  fstp dword [esp+8]
0x1319f  cmp  eax, 0x8000 ; jl 0x131ac
0x131a6  fld  dword [esp+8]        ; (unreachable: x is a sign-extended word)
0x131ac  fld  dword [esp+8] ; fld st(0)
0x131b2  fmul dword [0x804a8]      ; * C
0x131b8  fmulp st(1)               ; d*d*C
0x131ba  call 0x61a4c              ; frndint with RC=truncate
0x131bf  fistp dword [esp+4] ; mov eax,[esp+4]
0x131c9  xor  eax, eax             ; x <= 0x1400 -> 0
0x131cb  call 0x12cd4              ; move DS_000F0AEC toward eax
0x131d0  mov  ax, [0x104afc]       ; DS_00104AFC (camera index, word)
0x131d8  mov  eax, [eax*2 + 0x9af28]   ; DS_0009AF28, dword read at index*2
0x131df  mov  edx, [0xf0aec]
0x131e5  sar  eax, 0x10            ; high word
0x131e8  cmp  eax, edx ; jge 0x131f1
0x131ec  mov  [0xf0aec], eax       ; DS_000F0AEC = min(DS_000F0AEC, table>>16)
```

`0x61A4C` (file `0xB48A0`) loads the FPU control word with its high byte forced to
`0x1F`, i.e. rounding control = `11b` (**truncate toward zero**), executes
`frndint`, and restores the old control word. So the float path is
`arg = (int)(d*d*C)` with `C` the 32-bit float at `0x804A8`
(`26 B4 17 38` = `3.616898175096139e-05`), truncated.

`0x12CD4` (file `0x65B28`, `prage.c:1864`) is the y-stepper (note `DS_000F0AF2` is
the **high word of `DS_000F0AF0`**):

```
0x12cd4  edx = DS_000F0AEC ; ebx = eax (=arg)
0x12cdf  eax = arg - DS_000F0AEC ; ecx = |eax|
0x12ced  if (|diff| <= 0x100)  new = arg
         else if (diff > 0)    new = old + 0x100
         else                  new = old - 0x100
0x12d1d  DS_000F0AEC = new + (DS_000F0AF2 >> 16)
```

The final block then clamps: `DS_000F0AEC = min(DS_000F0AEC, DS_0009AF28[DS_00104AFC]
>> 16)` (signed). The dword is read at byte `0x9AF28 + 2*index`, so its high word is
`word[index+1]` (not `word[2*index+1]`). For `index 0` that is `word[1] = 0x1700`.

### 5.3 `0x13290` — center on the two players (mode 2)

`0x13290` (file `0x660E4`, `prage.c:2183`):

```
0x13295  ebx = DS_000F0AF0
0x1329b  ecx = *(u32*)(DS_001077B0 + 0x18)    ; player 0 x
0x132a1  esi = *(u32*)(DS_00107844 + 0x18)    ; player 1 x
0x132ad  edi = 0
         if (ecx > esi)  eax = ((ecx-esi)/2) + esi
         else            eax = ((esi-ecx)/2) + ecx   ; midpoint (signed)
0x132d3  edx = eax - ebx ; eax = |edx|
0x132e3  if (|edx| > 0x100) {
             if (|ebx| != 0x5D00) { ebx += (edx>0 ? 0x40 : -0x40); goto store }
         } else if (DS_000F0AEC != 0) { goto store }
0x13314  edi = 1
store:   if (edi && DS_001078FE != 0) DS_000F0AFE = 4
0x1332d  DS_000F0AF0 = ebx
```

### 5.4 `0x1333C` — center on one player (mode 3)

`0x1333C` (file `0x66190`, `prage.c:2238`): identical shape to `0x13290`, but the
source x is `*(u32*)(DS_001077B0 + DS_0010810D*0x94 + 0x18)` (a single indexed
player, index = byte `DS_0010810D`), and the mode-4 transition is **not** gated on
`DS_001078FE`:

```
0x13347  bl = DSB(0x10810d)                   ; DS_0010810D
0x1335b  eax = *(u32*)(DS_001077B0 + bl*0x94 + 0x18)
0x13365  eax -= DS_000F0AF0
         ... same 0x100 / 0x5D00 / 0x40 logic ...
0x133b1  if (flag) DS_000F0AFE = 4            ; no DS_001078FE test
0x133b8  DS_000F0AF0 = edx
```

### 5.5 The camera state globals

| global | type | held across frames | written by |
|---|---|---|---|
| `DS_000F0AF0` | `s32` | camera x, clamped to `[-0x5D00, 0x5D00]` | `0x12D48` modes, `0x12DF0`, `0x12E3C`, `0x12FD8`, `0x13290`, `0x1333C` |
| `DS_000F0AEC` | `s32` | camera y; stepped toward the projection target then clamped | `0x12CD4`, `0x1317C` |
| `DS_000F0AF4` | `s16` | shake offset (`offset += velocity`) | `0x1324C` |
| `DS_000F0AF6` | `s16` | shake velocity (`-= 0x20` per call) | `0x1324C` |
| `DS_000F0AFE` | `u8` | camera mode `0..4` (dispatcher input) | `0x13290`/`0x1333C` set `4`; `0x12DA8` reads it |
| `DS_000F0AFF` | `u8` | player index for mode 0 | elsewhere |
| `DS_001078F2` | `u32` | high word (`DS_001078F4`) = selected player x | `0x12DA8` |
| `DS_001078FE` | `u8` | gates the mode-4 transition in `0x13290` | elsewhere |
| `DS_0010810D` | `u8` | player index for mode 3 | elsewhere |
| `DS_0009AF28` | word array | per-camera y limits (`word[index+1]` read) | static data |
| `DS_00104AFC` | `u16` | camera index into `DS_0009AF28` | camera-select logic |

---

## 6. The state-5 register handoff (hard prerequisite for Task 5)

`0x11D04` case 5 is at file `0x64C3C` (`prage.c:1220`). The raw body, in raw order:

```
0x11de8  mov  eax, 0x100
0x11ded  mov  ecx, 0x12c              ; <-- CX source
0x11df2  call 0x2c3fc                 ; FUN_0002c3fc(0x100, ...)
0x11df7  call 0x1ea08
0x11dfc  mov  eax, 0x1d
0x11e01  xor  dh, dh                  ; <-- DH source (0)
0x11e03  call 0x2c06c                 ; FUN_0002c06c(0x1d)
0x11e08  xor  eax, eax
0x11e0a  xor  bl, bl                  ; <-- BL source (0)
0x11e0c  call 0x32970
0x11e11  mov  [0x70a6f], dh           ; DS_000F0A6F = DH
0x11e17  mov  [0x70a72], bl           ; DS_000F0A72 = BL
0x11e1d  mov  [0x70a6a], cx           ; DS_000F0A6A = CX
0x11e24  mov  edx, 6
0x11e29  mov  ebx, 9
0x11e2e  mov  [0x70a6c], dx           ; DS_000F0A6C = 6
0x11e35  mov  [0x70a64], bx           ; DS_000F0A64 = 9
0x11e3c  call 0x10db0 ; call 0x10e18 ; call 0x2bf08
```

**The values are set literally inside case 5, not inherited from a callee.** `CX` is
loaded at `0x11DED` (`mov ecx,0x12c`) and `DH` is zeroed at `0x11E01` (`xor dh,dh`),
both **inside case 5 itself**; they are then stored after the intervening calls at
`0x11E11` (`mov [0x70a6f],dh`) and `0x11E1D` (`mov [0x70a6a],cx`). No call returns
them and no callee sets them — the calls only have to preserve the registers, which
every one of them does:

* `CX = 0x12C` is loaded at `0x11DED`. Every intervening callee preserves `ECX`:
  `0x2C3FC` never writes `ECX` (scanned all 395 instructions of its 1268-byte body:
  zero `ecx/cl/ch` destinations, and the callees it reaches — `0x1CE70`, `0x1CC28`,
  `0x1CE04`, `0x1B544`, `0x1C528`, `0x1C470` — all push/pop `ECX`), `0x1EA08`
  pushes and pops `ECX`, `0x2C06C` is exactly `mov [0x105C05],al; ret`, and `0x32970`
  pushes and pops `ECX`. So **`DS_000F0A6A = 0x12C`**.
* `DH = 0` is loaded by `xor dh,dh` at `0x11E01`. `0x2C06C` writes only `AL`;
  `0x32970` pushes and pops `EDX`. So **`DS_000F0A6F = 0`**.
* `BL = 0` (`xor bl,bl` at `0x11E0A`) survives the same calls (`0x32970` pushes/pops
  `EBX`), giving `DS_000F0A72 = 0` (which the decompiler also proved as a literal).

The decompiler's `extraout_CX`/`extraout_DH` are its inability to track registers
across `__regparm3` calls. The raw is unambiguous: case 5 sets `CX = 0x12C` and
`DH = 0` itself and stores them at `0x11E11`/`0x11E1D`. **No literal is fitted:** the
port transcribes exactly those two stores with those two in-case values, and the
callee-preservation analysis above is the proof the stores read the intended values.

The decompiler's order (`prage.c:1225-1229`) lists `DAT_000f0a72`, `DAT_000f0a6c`,
`DAT_000f0a64`, `DAT_000f0a6a`, `DAT_000f0a6f`; the raw order is `0x6f`, `0x72`,
`0x6a`, `0x6c`, `0x64` (all before the tails). The values are order-independent, but
the port should follow the raw order.

---

## 7. What could not be determined (named gaps)

1. **`0x2DB58`/`0x2DBC4` — the paged resource reader used by `0x1EA08`.** It reads a
   paged resource by index through the paged-memory manager, which the port does not
   model for this resource class. Consequently the `local` index (which of the seven
   `0xA7DCC` character descriptors `0x1EA08` spawns) cannot be pinned. Evidence: the
   body at file `0x81018`, calling `0x2DB58` at `0x81078`; the index comes from
   `[esi+0x16]` (`0x1EACC`). **The decoded resource format is also asserted, not
   decoded:** the run-length/escape decoder writes `DS_00105EFC`/`DS_00105F00`, and
   the returned blob's `+4` "first string" / `+0` "next pointer" layout is read off
   the consumer (`0x1EA6B` copies 0x12 bytes from `[esi+4]`; `0x1EAC0`/`0x1EBC7` use
   `[esi]`) rather than from a decoded format. Both go to a future task that models
   `0x2DB58`.
2. **The `0x2F4D0`/`0x2EFD4` formatter.** `0x2F4D0` is `0x2EFD4` followed by
   `0x2F198`; the exact output of `0x2EFD4` (a `sprintf`-style formatter) is not
   decoded — only its col/row pass-through to `0x2F198` and its call sites are
   pinned. The `0x1EA08` draws therefore have exact `(col,row,table)` inputs but
   un-asserted formatted output. (Derived since, demo-pose record §33: the
   formatter is decoded and ported; the `0x1EA08` draws stay a gap on the
   `0x2DB58` resource reader.)
3. **The assets named by the two list handles `0x3E688` and `0x88874B0`.** They are
   resource handles in the `0x1B544` space; the index/offset decode is pinned
   (`0x3E688`: index 0 offset `0x3E688`; `0x88874B0`: index 17 offset `0x874B0`) but
   the runtime page table that maps index → asset is not statically readable here.
   `0x88874B0` is above the port's `MEM_SIZE` and outside both LE objects.
4. **The camera-index/player-index writers.** `DS_00104AFC`, `DS_000F0AFF`,
   `DS_0010810D`, `DS_001078FE` are written by camera-select/fight code that is out of
   this cycle's scope; the camera functions' anchors below set them explicitly.
5. **`0x1324C` is dormant.** No shipped store sets `DS_00104AE8` bit 0 (§5.1). The
   port can transcribe the body, but there is no shipped trigger; wiring it live
   would be inventing behaviour.
6. **The state-4 `0x2C3FC` `ECX` argument.** In state-4 case 0 the raw does not set
   `ECX` before `0x2C3FC`; it inherits the caller's `ECX`. The port's `0x2C3FC`
   is already ported and does not need the value, so this is noted rather than
   resolved.

**Explicitly not a gap.** `0x38B18`'s argument binding is pinned by the raw (§3.2:
`0x38B1B`/`0x38B1D`/`0x38B51`/`0x38B54`/`0x38B5D`), so the `frontend_spawn_row(desc,
0, 0)` call in `0x1EA08` rests on disassembly, not on the port's existing comment.

---

## 8. Unit-test values per helper

Every helper below has a concrete input and an exact expected output/global
transition. Helpers whose values cannot be pinned are in §7 instead.

### 8.1 `0x2F4BC` — `text_cursor_hold` (already ported)

Input: `DSW(DS_00105F34) = 4`, `DSW(DS_00105F34+2) = 0x10`; call
`text_cursor_hold(0, 5, "AB", 0)`.
Expected: `DSW(DS_00105F34) == 4` and `DSW(DS_00105F34+2) == 0x10` **unchanged**
(whereas `text_cursor_set(0,5,"AB",0)` would set `+2 == 2`); two glyph actors
spawned into `DS_00105F38`.

State-4 page 0 (raw args after fixup):

| # | col(EAX) | row(EDX) | string(EBX) | mode(ECX) | string |
|---|---|---|---|---|---|
| 1 | `-1` | `1` | `0x8005C` | `0x2000` | `"T E A M    R A G E"` |
| 2 | `-1` | `3` | `0x80070` | `0x2000` | `"ORIGINAL COIN-OP DEVELOPMENT"` |
| 3 | `2` | `7` | `0x80090` | `0` | `"Dennis Harper…"` |
| 4 | `2` | `9` | `0x800BC` | `0` | `"Frank Kuan…"` |
| 5 | `2` | `0xB` | `0x800E8` | `0x1000` | `"Jason Leong…"` |
| 6 | `2` | `0xD` | `0x8010C` | `0x1000` | `"J. Cameron Petty…"` |
| 7 | `2` | `0xF` | `0x80130` | `0x2000` | `"Jeanne Parson…"` |
| 8 | `2` | `0x11` | `0x80154` | `0x2000` | `"Steve Riesenberger…"` |

Page 1 (12 calls) strings `0x8005C,0x8017C,0x80198,0x801BC,0x801E4,0x80208,0x80228,
0x8024C,0x8026C,0x80290,0x802B4,0x802D8`; page 2 (13 calls) strings
`0x802FC,0x80310,0x80328,0x8034C,0x8036C,0x80394,0x803B4,0x803DC,0x803FC,0x80420,
0x80444,0x80464,0x80488`. The exact `(col,row,mode)` triples are in the raw listings
(file `0x643EE`/`0x64513`/`0x64693`).

### 8.2 `0x12658`

Input: a clean actor pool and an empty effect pool; `frontend_list_next` seeded with
one entry whose `+0 = 0x1234` and `+4 != 0`.
Expected globals/state:
* three actors exist; the first is `DS_000F0A58` and `DSW(first+0x36) == 0xFFC0`;
* `DSB(first+0x4B) == DSB(second+0x56)` and `DSB(second+0x4B) == DSB(third+0x56)`;
* `DSW(DS_00107A44) == 0`;
* `effects_active() == 1`, and the spawned record has `+0x0C == 3`, `+0x0D == 6`,
  `+0x0F == 0x80`, `+0x08 == 0x1234`.
* With `*rec == 0x3E688` (or `frontend_resource_known(rec)` true), no effect spawns:
  `effects_active() == 0`.

### 8.3 `0x29B74`

Input: `DS_0009AF3D` set non-zero; two list entries `A` (`+0=0x1111`) and `B`
(`+0=0x3E688`), both `+4 != 0`.
Expected:
* `DS_0009AF3D == 2` (0x13DF0 zeroed it, then two `0x13D4C` increments);
  `effects_active() == 2` — **both** entries spawn (no predicate);
* each spawned record has `+0x0C == 4`, `+0x0D == 3`, `+0x08` = its source entry;
* `DS_001088EE == 0x78`, `DS_00104AFE == 0x78`, `DS_00104B00 == 0x15`.

### 8.4 `0x41578`

Input: three list entries `A` (`+0=0x3E688`), `B` (`+0=0x88874B0`),
`C` (`+0=0x2222`), all `+4 != 0`.
Expected:
* `effects_active() == 2` (only `A` and `B`), each `+0x0C == 4`, `+0x0D == 2`;
* `DS_00104AFE == 0x78`, `DS_001088EE == 0`, `DS_00104AFA == 0x13`,
  `DS_00104B00 == 0x15`, `DS_00104B25 == 0`.

### 8.5 `0x1EA08` (pinnable part; index is a gap)

Input: clean actor pool; `DS_00105F34 = 0`; tables as shipped.
Expected:
* `DS_00104B15 == 0` (via `0x4F1E4`);
* `frontend_spawn_row` was called with descriptor `mem + 0xA7B6C` and `(a2,a3)=(0,0)`;
* exactly one actor spawned from the `0xA7DCC` table with
  `(a2,a3,a4,a5) = (0x2A00, 0xFF, 0x1C80, 0)`, followed by `0x2A17C(actor, 0)` with
  `EBX = 0x105FD30`;
* the `0x2F4D0`/`0x2F4BC` draws used the byte tables at `0xA7B94`/`0xA7B97` and
  the letter table `0xA7DA0`.
* The `local` index is **not assertable** until `0x2DBC4` is modelled (§7.1); a test
  may set `[esi+0x16]` to a known letter (`"R"`) and assert `local == 0` once the
  resource reader exists.

### 8.6 `0x1317C`

Input A (non-float path): `DSD(DS_001078F2) = 0`, `DSW(DS_00104AFC) = 0`,
`DSD(DS_000F0AEC) = 0x250`, `DSD(DS_000F0AF0) = 0`.
Expected: `DSD(DS_000F0AEC) == 0x150` (0x12CD4 steps `0x250 → 0x150`, then
`0x1700 < 0x150` is false).

Input B (float path): `DSD(DS_001078F2) = 0x18000000` (`x = 0x1800`),
`DSD(DS_000F0AEC) = 0`, `DSD(DS_000F0AF0) = 0`, `DSW(DS_00104AFC) = 0`.
Expected: `DSD(DS_000F0AEC) == 37`. Derivation: `d = 0x400 = 1024`;
`C = 3.616898175096139e-05` (float at `0x804A8`, bytes `26 B4 17 38`);
`d*d*C = 37.925926…`; `0x61A4C` truncates → `37`; `0x12CD4(37)` with old `0` and
`|37| <= 0x100` sets `0 + (DS_000F0AF2>>16 = 0) = 37`; `0x1700 < 37` is false.

### 8.7 `0x1324C`

Input A: `DSW(DS_000F0AF4) = 0x10`, `DSW(DS_000F0AF6) = 0xFFE0` (`-0x20`),
`DSB(DS_00104AE8) = 0x01`.
Expected: `DSW(DS_000F0AF4) == 0`, `DSW(DS_000F0AF6) == 0xFFE0`,
`DSB(DS_00104AE8) == 0x00` (bit 0 cleared).

Input B: `DSW(DS_000F0AF4) = 0x100`, `DSW(DS_000F0AF6) = 0x20`,
`DSB(DS_00104AE8) = 0x01`.
Expected: `DSW(DS_000F0AF4) == 0x120`, `DSW(DS_000F0AF6) == 0x00`,
`DSB(DS_00104AE8) == 0x01` (unchanged).

### 8.8 `0x13290`

Input A: player records `DSD(DS_001077B0) = P0` with `DSD(P0+0x18) = 0x1000`,
`DSD(DS_00107844) = P1` with `DSD(P1+0x18) = 0x3000`, `DSD(DS_000F0AF0) = 0`,
`DSD(DS_000F0AEC) = 0`, `DSB(DS_001078FE) = 0`.
Expected: `DSD(DS_000F0AF0) == 0x40`, `DSB(DS_000F0AFE)` unchanged.

Input B (settle): same players, `DSD(DS_000F0AF0) = 0x2000`,
`DSD(DS_000F0AEC) = 0`, `DSB(DS_001078FE) = 1`.
Expected: `DSD(DS_000F0AF0) == 0x2000`, `DSB(DS_000F0AFE) == 4`.

Input C (x limit): `DSD(DS_000F0AF0) = 0x5D00`, players far apart,
`DSB(DS_001078FE) = 1`.
Expected: `DSD(DS_000F0AF0) == 0x5D00`, `DSB(DS_000F0AFE) == 4` (no move, settled).

### 8.9 `0x1333C`

Input A: `DSB(DS_0010810D) = 0`, `DSD(DS_001077B0) = P0` with
`DSD(P0+0x18) = 0x2000`, `DSD(DS_000F0AF0) = 0`, `DSD(DS_000F0AEC) = 0`.
Expected: `DSD(DS_000F0AF0) == 0x40`, `DSB(DS_000F0AFE)` unchanged.

Input B (settle, ungated): `DSD(DS_000F0AF0) = 0x2000`, `DSD(P0+0x18) = 0x2000`,
`DSD(DS_000F0AEC) = 0`, `DSB(DS_001078FE) = 0`.
Expected: `DSD(DS_000F0AF0) == 0x2000`, `DSB(DS_000F0AFE) == 4` — the transition
fires **without** the `DS_001078FE` gate that `0x13290` requires.

### 8.10 State 5 case 5 (`0x11D04`)

Input: any; call `game_state_step()` with `DSW(DS_000F0A64) == 5`,
`DSB(DS_00104B1D) == 0` (no coin accepted).
Expected: `DSW(DS_000F0A64) == 9`, `DSW(DS_000F0A6C) == 6`,
`DSD(DS_000F0A72) == 0`, `DSW(DS_000F0A6A) == 0x12C`, `DSB(DS_000F0A6F) == 0`,
and the calls `0x2C3FC`, `0x1EA08`, `0x2C06C`, `0x32970` ran in that order.

---

## 9. Provenance

* Raw bytes: `data/game/C/PRAGE.EXE` (read-only), 32-bit `capstone` over the LE
  page-mapped image with the internal 32-bit fixups applied.
* Decompilation: `port/decomp/prage.c` (read-only), line numbers cited inline.
* Port cross-references: `port/src/game/actors.c` (`text_cursor_hold`, `text_render`,
  `text_glyph_emit`), `port/src/game/effects.c` (`effects_spawn`), `port/src/mem.c`
  (`mem_load_le`, `mem_load_le_fixups`), `port/src/platform/res.h` (`res_resolve` =
  `0x1B544`), `port/src/game/flow.c` (`frontend_list_next`, `run_process_table`).
* Prior derivations used: `docs/superpowers/plans/2026-09-17-actor-system-args.md` §0
  (`0x2AE14` register binding), `2026-09-19-effects-producers-derivations.md`
  (`0x13D4C`/`0x13C70` contracts).
