# Demo fight: the camera, the think chain and `0x49C78` — raw-byte derivation (Task 1)

Register-level derivation of the attract demo fight's motion path from the shipped
`data/game/C/PRAGE.EXE`: the per-frame projection `0x17FA0`, the fight-camera chain,
the arena frame `0x263F4` and its remaining callees, the scene/effects pass
`0x49C78`, the think/AI chain, and the state 6/7 handlers. This is the record Tasks
2–5 implement from. It changes no source.

Everything below is read from the shipped bytes with `capstone` in **32-bit** mode.
On any conflict between the plan/brief text and the bytes, the bytes win and the
conflict is recorded in §0.2.

This record also answers the cycle's open question 2 (§5.9): **how often the demo's
AI consumes the RNG.**

---

## 0. Addressing, reproduction, and corrections

### 0.1 The image, the formulas and the reproduction recipe

`PRAGE.EXE` is a Microsoft DOS/4GW **bound** executable with the LE image at file
`0x290A4`. Its two objects are unchanged from the previous cycle:

```
[0] code  base 0x10000  size 0x63B15  pages=100 pageidx=1   flags 0x2045
[1] data  base 0x80000  size 0x8B0D0  pages=113 pageidx=101 flags 0x2043
```

For the code object `file_offset = VA + 0x52E54`; for the data object
`file_offset = VA + 0x46E54`. The page map is linear (`phys == logical`); the data
object is file-backed only to `VA 0xF1000`, above which it is BSS. All the fighter
and camera state below (`0x100Axx`, `0x100Bxx`, `0x1077xx`, `0x1088xx`) is BSS,
zero-filled at load; the static tables cited (`0xAxxxx`, `0xBedxx`, `0xCxxxx`,
`0xDxxxx`, `0xExxxx`) are file-backed data.

**Raw operands are pre-relocation in the file, post-relocation in the listings.**
Every listing below is taken **after** applying the LE internal 32-bit fixups, so a
code immediate is a linear VA (`0x17FA0`, `0x2AE14`) and a data operand is a linear
address (`[0xf0af0]`, `[0x1077b0]`). This is the same convention as
`docs/superpowers/plans/2026-09-20-frontend-chain-derivations.md` §0.1.

Reproduction: rebuild the flat image exactly as `port/src/mem.c`'s `mem_load_le` +
`mem_load_le_fixups` do, then disassemble with capstone in 32-bit mode. A working
replica of those two functions (and the caller/member scanners used for this
record) is a read-only scratch script; it is not committed. The essential shape:

```python
# LE header 0x290A4, bound base 0x26654 (the inner MZ), page data = bound + 0x3C800,
# page size 0x1000. Code object file = VA + 0x52E54, data object = VA + 0x46E54.
# Map each object's pages linearly, zero its BSS tail, then apply every fixup record
# (src==0x07, tf&~0x50==0) as target_base + target_off at src_off.
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
md = Cs(CS_ARCH_X86, CS_MODE_32)
for ins in md.disasm(bytes(MEM[0x17fa0:0x17fa0+0x22d]), 0x17fa0):
    print("%#08x  %-18s %s %s" % (ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str))
```

`MEM` is a `bytearray(0x4000000)`. The two reference scans used repeatedly below:

* **direct callers**: scan the code object for `E8 rel32` with `addr+5+rel == target`;
* **data references**: scan the fixed-up image for the little-endian 4-byte target.

Decompilation quotations are from `port/decomp/prage.c`; Ghidra's `size=`, `callers=`
and `callees=` header counts are cited as the decompiler's claim and are **not**
authoritative — the raw scan is.

### 0.2 Corrections against the plan/brief (raw wins)

1. **`0x17FA0`'s arguments and outputs are not what the brief/plan describe.**
   The brief says it "takes a byte pointer and a record pointer (`&DS_00100B60`/
   `&DS_00100AF0` for P1 …)". The raw takes **six** arguments (four registers plus
   two stack words, `ret 8`) and it **does not take a record pointer** — it derives
   each fighter's record from the slot table itself. Task 2's test snippet
   (`camera_project(0, rec0)` and `DSD(rec0 + 0x1Cu)`) asserts a write the raw never
   makes. The true arguments, record globals and output globals are in §1. The
   correct record pointers are `DSD(DS_001077B0)` (P0) and `DSD(DS_00107844)` (P1),
   **not** `DS_001077B4`.
2. **`DS_001077B4` is not the player-1 record pointer.** It has three references
   (`0x276E2`, `0x29775`, `0x42884`), all as `[idx*4 + 0x1077B4]` or a store
   `mov [esi + 0x1077B4], eax` — the `+4` field of a 0x94-byte slot, not the slot
   table itself. The slot table base is `0x1077B0` with stride `0x94`; the P1 slot
   begins at `0x107844` (§1.1).
3. **`0x17FA0` has no clamp and no "multiply".** Its fixed-point operation is an
   arithmetic `(x + 0x20) >> 6` on two actor fields, followed by a sprite-origin
   subtraction. The `0x100`/`0x40` stepping and the `±0x5D00` clamp the brief
   attributes to the projection live in the **camera** chain (`0x12CD4` steps by
   `0x100`; `0x12D48` clamps `DS_000F0AF0` to `±0x5D00`; `0x13290`/`0x1333C` step
   by `0x40`). §1 and §2.
4. **`0x1324C` (`camera_shake_decay`) stays dormant.** `0x1324C` is update-table
   entry 0 and runs only when `DS_00104AE8` bit 0 is set. An exhaustive scan of every
   reference to `0x104AE8` in the fixed image finds **no store that sets bit 0**: the
   writes are `and …,0xDF/0xFB/0xEF/0xFD` (clears), `or …,0x04/0x40` (bits 2/6), and
   `mov [0x104AE8],reg` / `0` where the register is zeroed immediately before. The
   pattern `or byte [0x104AE8],1` occurs zero times. **No demo path sets bit 0**, so
   the demo does not make the shake process live and there is nothing to re-port
   (§2.11). This confirms the previous cycle's finding.
5. **`camera_init` registers nothing.** The update table `DS_000A8644` contains no
   camera function; its only camera-related entry is `0x1324C` at index 0 (bit 0,
   dormant). The render table `DS_000A86C4` contains none. `0x12D48`, `0x1282C`,
   `0x12DA8`, `0x12DF0`, `0x12E3C`, `0x13290` and `0x1333C` are all reached by direct
   call. Task 2's `camera_init` must not register anything (§2.10).
6. **`0x12CD4` steps by `0x100`, not `0x40`.** The plan's Task 2 Step 3 conflates the
   `0x12CD4` y-stepper (`0x100` threshold, `0x100` step) with `0x13290`'s x-follower
   (`0x100` threshold, `0x40` step). §2.3, §2.5.
7. **The think chain does not reach `0x18C14`.** A breadth-first walk of the direct
   `E8` call graph from the demo roots (`0x263F4`, `0x24C5C`, `0x11A8C`) reaches 922
   functions; `0x18C14` is **not** among them (its 37 call sites are all off the demo
   path). Task 4 must not port `0x18C14` for cycle 1; §5.7.
8. **`0x35658` (the HUD/health path) is load-bearing for motion, not inert.** The
   arena frame calls it twice per frame (`0x2651B`, `0x26525`), and `0x35658` calls
   `0x34B6C` at `0x357FC`, which calls `0x1A978` at `0x34C75`, which calls the command
   mapper `0x3B134` at `0x1A9AE`. So the command word `DS_001088E0`/`E2` is written
   again after the think step. Task 3 cannot skip `0x35658` as an inert stub without
   breaking the think mapping; the record says which parts are needed (§3.7, §5.6).
9. **`0x1088E4` is not the think-chain input cursor.** The command word is
   `word[0x1088E0 + side*2]` (`0x1088E0` side 0, `0x1088E2` side 1). `0x1088E4` has
   nine references outside `0x3B134`/`0x3B298` (`0x11F35`, `0x28CE9`, `0x42F6D`,
   `0x4F650`…) and is not read by the command mapper. Task 4's test snippet must seed
   the mapped inputs, not `DSB(DS_001088E4)`; §5.4 gives the anchors.
10. **The state-7 timer exit fires on the pre-decrement value 1, not 0.** The
    dispatcher `0x11D04` computes `eax = (u16)DS_000F0A6A - 1` **before** the switch
    (`0x11D62`–`0x11D69`), and case 7 stores `ax` then tests `ax >= 1`: the arena runs
    while the new value is non-zero, so the exit frame is the one whose **pre** value
    is 1. §6.2. (The plan's Task 5 Step 4 wording "the frame the pre-decrement value
    is zero" is off by one.)
11. **State 6 draws the RNG twice, and the two picks are coupled, not independent.**
    `0x11AAD` draws `rng(7)` and uses it both for `DS_00104AFC` and for P0's
    character; `0x11AE9` draws `rng(6)` and P1's character is
    `(draw1 + draw2) % 7`. §6.1.
12. **All brief function sizes are confirmed by the raw body extents.** The sizes in
    the brief match Ghidra's and, for every function checked, the raw extent
    (last instruction + next function boundary): `0x17FA0` 557, `0x12D48` 93,
    `0x12DF0` 74, `0x12E3C` 409, `0x13290` 169, `0x1333C` 134, `0x12CD4` 93,
    `0x1317C` 122, `0x12DA8` 72, `0x1282C` 166, `0x3C5CC` 23, `0x16D58` 73,
    `0x17580` 330, `0x1958C` 464, `0x19068` 250, `0x3CB68` 91, `0x35658` 478,
    `0x49C78` 2492, `0x1975C` 186, `0x3B464` 608, `0x3B298` 441, `0x3B134` 355,
    `0x3BDDC` 401, `0x18C14` 1035, `0x1A978` 408, `0x263F4` 329. No correction.
13. **`0x35658`'s call to `0x34B6C` is direct and real**, not a misparse: raw
    `0x357FC e86bf3ffff call 0x34b6c`, inside `0x35658`'s extent, and `0x34B6C`
    (`size=541`) reaches `0x1A978` at `0x34C75`.

### 0.3 Addresses derived in this document

| function | VA | file (obj-0) | decompiler | size | brief | call sites (raw) |
|---|---|---|---|---|---|---|
| `0x17FA0` | `0x17FA0` | `0x6ADF4` | `:5117` | 557 | 557 | 40 (12 arena frames) |
| `0x17EEC` | `0x17EEC` | `0x6AD40` | `:5092` | 107 | — | 9 |
| `0x16D58` | `0x16D58` | `0x69BAC` | `:4333` | 73 | 73 | 22 |
| `0x17580` | `0x17580` | `0x6A3D4` | `:4723` | 330 | 330 | 7 |
| `0x16308` | `0x16308` | `0x6915C` | `:3856` | 19 | — | 1 |
| `0x1A570` | `0x1A570` | `0x6D3C4` | `:6458` | 57 | — | 42 |
| `0x12D48` | `0x12D48` | `0x65B9C` | `:1894` | 93 | 93 | 2 |
| `0x12DF0` | `0x12DF0` | `0x65C44` | `:1946` | 74 | 74 | 1 |
| `0x12E3C` | `0x12E3C` | `0x65C90` | `:1975` | 409 | 409 | 1 |
| `0x13290` | `0x13290` | `0x660E4` | `:2183` | 169 | 169 | 1 |
| `0x1333C` | `0x1333C` | `0x66190` | `:2238` | 134 | 134 | 1 |
| `0x12CD4` | `0x12CD4` | `0x65B28` | `:1864` | 93 | 93 | 1 |
| `0x1317C` | `0x1317C` | `0x65FD0` | `:2138` | 122 | 122 | 3 |
| `0x12DA8` | `0x12DA8` | `0x65BFC` | `:1923` | 72 | 72 | 15 |
| `0x1282C` | `0x1282C` | `0x65680` | `:1639` | 166 | 166 | 4 |
| `0x12C7C` | `0x12C7C` | `0x65AD0` | `:1831` | 85 | — | 1 |
| `0x1324C` | `0x1324C` | `0x660A0` | `:2166` | 67 | — | 0 direct (table[0]) |
| `0x263F4` | `0x263F4` | `0x79248` | `:13049` | 329 | 329 | 2 (`0x11C3F`, `0x11E8F`) |
| `0x3C5CC` | `0x3C5CC` | `0x8F420` | `:26156` | 23 | 23 | 12 |
| `0x3CB68` | `0x3CB68` | `0x8F9BC` | `:26447` | 91 | 91 | 13 |
| `0x1958C` | `0x1958C` | `0x6C3E0` | `:6218` | 464 | 464 | 7 |
| `0x19068` | `0x19068` | `0x6BEBC` | `:6001` | 250 | 250 | 12 |
| `0x35658` | `0x35658` | `0x884AC` | `:21877` | 478 | 478 | 28 |
| `0x34B6C` | `0x34B6C` | `0x879C0` | `:21353` | 541 | — | 1 |
| `0x33F08` | `0x33F08` | `0x86D5C` | `:21193` | 302 | — | 6 |
| `0x49C78` | `0x49C78` | `0x9CACC` | `:31799` | 2492 | 2492 | 7 |
| `0x1975C` | `0x1975C` | `0x6C5B0` | `:6305` | 186 | 186 | 5 |
| `0x3B134` | `0x3B134` | `0x8DF88` | `:25258` | 355 | 355 | 2 |
| `0x3B298` | `0x3B298` | `0x8E0EC` | `:25318` | 441 | 441 | 14 |
| `0x3B464` | `0x3B464` | `0x8E2B8` | `:25396` | 608 | 608 | 1 |
| `0x3BDDC` | `0x3BDDC` | `0x8EC30` | `:25822` | 401 | 401 | 7 |
| `0x18C14` | `0x18C14` | `0x6BA68` | `:5798` | 1035 | 1035 | 37 (off demo path) |
| `0x1A978` | `0x1A978` | `0x6D7CC` | `:6640` | 408 | 408 | 1 (`0x34C75`) |
| `0x186D0` | `0x186D0` | `0x6B524` | `:5425` | 223 | — | 21 |
| `0x2A690` | `0x2A690` | `0x7D4E4` | `:15570` | 397 | — | 5 |
| `0x33A10` | `0x33A10` | `0x86864` | `:20937` | 88 | — | 35 |
| `0x3AFC4` | `0x3AFC4` | `0x8DE18` | `:25185` | 116 | — | 17 |
| `0x5D7DC` | `0x5D7DC` | `0xB0630` | `:38892` | 42 | — | 113 |
| `0x11A8C` | `0x11A8C` | `0x648E0` | `:1114` | 317 | — | 1 |
| `0x11BCC` | `0x11BCC` | `0x64A20` | `:1154` | 41 | — | 1 (`0x11E77`) |
| `0x11D04` | `0x11D04` | `0x64B58` | `:1169` | 546 | — | 1 |

---

## 1. `0x17FA0` — the per-frame projection (the bellwether)

`0x17FA0` is called **six times per demo frame** from `0x263F4`, at
`0x26450`, `0x26473`, `0x264A4`, `0x264C7`, `0x264EC`, `0x2650F`. It has 40 direct
call sites across the arena-frame variants. The raw signature is `ret 8`,
so it takes **six** arguments:

| arg | register / stack | side 0 | side 1 |
|---|---|---|---|
| 1 | `EAX` | `0` | `1` |
| 2 | `EDX` | `0x100B08` | `0x100B0C` |
| 3 | `EBX` | `0x100B00` | `0x100B04` |
| 4 | `ECX` | `0x100B62` | `0x100B63` |
| 5 | `[esp+4]` | `0x100B60` | `0x100B61` |
| 6 | `[esp+8]` | `0x100AF0` | `0x100AF4` |

The demo-frame call sequence (raw, at `0x263F4`) is:

```
; first pair, before 0x17580
0x26427  push 0x100af0        ; arg6
0x2642c  mov  edx, [0x1077e4] ; 0x26432 xor eax,eax ; 0x26434 mov [0x1077e8],edx
0x2643a  mov  edx, [0x107878] ; 0x26440 push 0x100b60 ; 0x26445 mov [0x10787c],edx
0x2644b  mov  edx, 0x100b08   ; arg2
0x26450  call 0x17fa0         ; (EBX=0x100b00, ECX=0x100b62 set in the prologue)
0x26455  push 0x100af4
0x2645a  mov  ecx, 0x100b63
0x2645f  mov  ebx, 0x100b04
0x26464  push 0x100b61
0x26469  mov  edx, 0x100b0c
0x2646e  mov  eax, 1
0x26473  call 0x17fa0
; second pair, after 0x17580/0x1958C/0x19068
0x26489  push 0x100af0 ; 0x26498 push 0x100b60 ; edx=0x100b08, ecx=0x100b62,
         ebx=0x100b00, eax=0  ; 0x264a4 call
0x264a9 ... edx=0x100b0c, ecx=0x100b63, ebx=0x100b04, eax=1 ; 0x264c7 call
; third pair, after 0x1975C
0x264d1 ... (side 0) ; 0x264ec call
0x264f1 ... (side 1) ; 0x2650f call
```

So each pair repeats the identical argument set; only `0x1958C`/`0x19068`/`0x1975C`
run between the pairs.

### 1.1 The slot table — where the fighter record comes from

`0x17FA0` does **not** receive a record pointer. It derives each side's record from a
two-entry slot table at `DS_001077B0`, stride `0x94` bytes, indexed by the side
argument:

```
0x1805b  lea  eax, [esi*8]
0x18062  add  eax, esi
0x18064  shl  eax, 2
0x18067  add  eax, esi        ; eax = side * 0x25
0x18069  mov  ecx, [eax*4 + 0x1077b0]   ; ecx = *(u32*)(0x1077B0 + side*0x94)
```

* **P0 record pointer** = `DSD(DS_001077B0)` (`DSD(DS_00107844 - 0x94)`).
* **P1 record pointer** = `DSD(DS_00107844)` (`DSD(DS_001077B0 + 0x94)`).

`0x13290` independently confirms the layout: it reads player 0 x as
`*(u32*)(DS_001077B0 + 0x18)` and player 1 x as `*(u32*)(DS_00107844 + 0x18)`
(§2.5 and the previous record §5.3). The slot struct, read off its consumers:

| slot offset | global (slot 0) | meaning | evidence |
|---|---|---|---|
| `+0x00` | `0x1077B0` | fighter record pointer | `0x18069`, `0x186D0` |
| `+0x04` | `0x1077B4` | second actor pointer (spawned from `0xBB8D0`) | `0x42882` |
| `+0x08` | `0x1077B8` | secondary record pointer `R0` | `0x17FB5`, `0x3B134@0x3B1F1` |
| `+0x2C` | `0x1077DC` | copy of `rec+0x18` (x) | `0x186FC` |
| `+0x30` | `0x1077E0` | copy of `rec+0x1C` (y) | `0x186B0`/`0x18ACA` |
| `+0x34` | `0x1077E4` | signed word compared by `0x12E3C`/`0x12FD8` | `0x12E4B`, `0x12E98` |
| `+0x38` | `0x1077E8` | previous `+0x34` latch | `0x2643A` |
| `+0x48` | `0x1077F8` | byte, cleared by `0x276DB`/`0x2976E` | — |
| `+0x63` | `0x107813` | byte gate for the think mapper | `0x3B168` |
| `+0x64` | `0x107814` | byte target/stance selector | `0x3B1DC` |
| `+0x7A` | `0x10782A` | character index | `0x17EF9`, `0x33F08` |

The second slot begins at `0x107844`, so its `R1` secondary pointer is
`DS_0010784C`, its `+0x34` is `DS_00107878`, its character byte is `DS_001078BE`,
and its `+0x7A`-family fields are the `0x1078xx`/`0x108xx` addresses used below.

### 1.2 The body (fixed-up)

Prologue and the two "world" blocks (`R0`/`R1` are the secondary pointers):

```
0x17fa0  push esi; push edi; push ebp; sub esp, 0x18
0x17fa6  mov  esi, eax            ; esi = side
0x17fa8  mov  edi, edx            ; edi = arg2
0x17faa  mov  ebp, ebx            ; ebp = arg3
0x17fac  mov  [esp+0x10], ecx     ; arg4
0x17fb0  call 0x17eec             ; eax = per-character constant (side)
0x17fb5  mov  edx, [0x1077b8]     ; R0 = slot[0].+0x08
0x17fbb  mov  [esp+0x14], eax     ; save the constant (local_10)
0x17fbf  test edx, edx ; je 0x1800a
0x17fc5  mov  ax, [edx+0x56]      ; R0->actor index
0x17fc9  mov  ebx, [0x1014ec]     ; actor table base
0x17fcf  shl  eax, 5 ; add eax, ebx
0x17fd4  mov  ebx, [eax+4]        ; actor+4
0x17fd7  mov  eax, [eax+8]        ; actor+8
0x17fda  sar  eax, 6
0x17fdd  mov  [0x100aa0], eax     ; DAT_00100aa0 = (actor+8) >> 6
0x17fe4  sar  ebx, 6
0x17fe7  mov  al, [0x10782a]      ; slot[0] character
0x17fec  mov  [0x100aa8], ebx     ; DAT_00100aa8 = (actor+4) >> 6
0x17ff2  mov  bx, [edx+0x34]      ; R0->+0x34 (signed word)
0x17ff6  mov  eax, [eax*4 + 0xa174c]
0x17ffd  test bx, bx ; jle 0x18004 ; neg eax
0x18004  add  [0x100aa8], eax     ; += ±0xA174C[char]
; R1 block identical for slot[1] using [0x1078be] -> 0x100aa4/0x100aac
```

The `R0`/`R1` blocks are independent of the six arguments; they write the four
"world anchor" globals `0x100AA0`, `0x100AA4`, `0x100AA8`, `0x100AAC`. The table at
`0xA174C` is `{0x28,0x28,0x00,0x1E,0x1E,0x14,0x28,0,0,0}`.

Then the per-side work:

```
0x18069  ecx = P = *(u32*)(0x1077B0 + side*0x94)
0x18070  mov ax, [ecx+0x28]
0x18074  xor al, al
0x18076  and ah, 0x40             ; keep only bit 0x4000
0x1807e  setne al                 ; al = (P->+0x28 & 0x4000) != 0
0x1808a  mov [ecx_arg4], al       ; *arg4 = facing flag
0x18092  mov byte [arg5], 0       ; *arg5 = 0
0x18095  dx = P->+0x56            ; actor index
0x180a1  ax = word[actor]         ; actor word 0
0x180a5  and ah, 0x7f             ; clear bit 15
0x180b5  sub eax, [esp+0x14]      ; (actor.word0 & 0x7fff) - 0x17EEC(side)
0x180be  mov [arg6], eax          ; *arg6 = that
0x180c9  call 0x16afc(edx=0x100a78+side*4, eax=side)
0x180ce  ... if al: copy dword 0x100a78[side] -> 0x100ac0[side]
              else: zero 0x100ac0..0x100ac3[side]
0x18108  call 0x164f4(edx=0x100a90+side*4, eax=side)
0x1810d  ... if al: *arg5 = 1 ; copy dword 0x100a90[side] -> 0x100ac8[side]
              else: zero 0x100ac8..0x100acb[side]
```

Then the projection proper and the sprite-origin subtraction:

```
0x18140  ax = [ecx+0x56]                       ; actor index
0x1814e  edx = *(u32*)(actor + 4)
0x18152  add edx, 0x20
0x18155  sar edx, 6
0x18158  mov [edi], edx                        ; *arg2 = ((actor+4) + 0x20) >> 6
0x1815c  dx = [ecx+0x56]
0x18163  eax = *(u32*)(actor + 8)
0x18167  add eax, 0x20
0x1816a  sar eax, 6
0x1816d  mov [ebp], eax                        ; *arg3 = ((actor+8) + 0x20) >> 6
0x18170  &local_20 = &[esp+4], &local_24 = &[esp]
0x1817e  edx = *arg6                           ; the projected index base
0x18186  call 0x16308(side, edx)               ; res_resolve(0xA8B30[actor.word0 & 0x7fff])
0x1818b  ebx = sprite
0x1818d  ecx = *(u32*)(sprite+2) >> 16
0x18190  edx = *(u32*)(sprite+4) >> 16
0x1819b  call 0x1a570(side)
0x181a2  test al, al ; jne 0x181ac
0x181a4  movsx eax, word[sprite]
0x181a7  sub eax, ecx
0x181a9  lea ecx, [eax-1]                      ; ecx = word[sprite] - ecx - 1
0x181b0  local_20 = ecx ; local_24 = edx
0x181bb  sub [ebp], local_24                   ; *arg3 -= ecx/edx
0x181c2  sub [edi], local_20                   ; *arg2 -= the other
0x181ca  ret 8
```

**Final arithmetic, exactly.**

```
outA = (u32)(actor+4) ; outA = (outA + 0x20) >> 6 (arithmetic) ; outA -= local_A
outB = (u32)(actor+8) ; outB = (outB + 0x20) >> 6 (arithmetic) ; outB -= local_B
```

where `outA = *arg2` (`0x100B08`/`0x100B0C`), `outB = *arg3`
(`0x100B00`/`0x100B04`), `local_B = (u32)(sprite+4) >> 16`, and

```
local_A = ((s32)(u32)(sprite+2) >> 16)                     if 0x1A570(side) != 0
local_A = (s16)word[sprite] - ((s32)(u32)(sprite+2) >> 16) - 1   otherwise
```

and `actor = MEM[0x1014EC] + (u16)(P->+0x56) * 0x20`.

`0x16AFC` (read `0x100A78[side]`) and `0x164F4` (read `0x100A90[side]`) return the
booleans that copy or zero the `0x100AC0`/`0x100AC8` byte groups and set `*arg5`.
They are themselves large (601 B and 547 B) and read resource/page state; the
record lists them as gaps (§7.4). `0x16308` is `res_resolve(0xA8B30[actor.word0 &
0x7fff])` (§7.3).

`0x17EEC(side)` is statically readable: it indexes `slot[side].+0x7A`
(`0x10782A + side*0x94`), takes the byte `0..6`, and returns a zero-extended 16-bit
constant from the jump table at `0x17ED0`:

| char | source | value |
|---|---|---|
| 0 or >6 | `word[0xE6DD0]` | `0x0EE4` |
| 1 | `word[0xE39D0]` | `0x12A2` |
| 2 | `word[0xECBD8]` | `0x0BD4` |
| 3 | `word[0xD2134]` | `0x16B5` |
| 4 | `word[0xEA604]` | `0x1F9E` |
| 5 | `word[0xD3E08]` | `0x2F19` |
| 6 | `word[0xE061C]` | `0x32D7` |

`0x33F08` (the health-bar pass) reads the **same** per-character table
(`0x33F08` decompiler `:21208`–`:21229`) and uses it the same way, which is the
strongest cross-check that `0x17EEC`'s values are the fighter's per-character ground
height.

### 1.3 Worked pairs (statically-determinable stage)

These pin the `(x + 0x20) >> 6` stage byte-for-byte. `actor+4` and `actor+8` are the
two source fields; `outA`/`outB` are the values written by `0x18158`/`0x1816D`
**before** the sprite-origin subtraction.

| # | `actor+4` | `actor+8` | `outA = (actor+4+0x20)>>6` | `outB = (actor+8+0x20)>>6` |
|---|---|---|---|---|
| 1 | `0x00001000` | `0x00002000` | `0x00000040` | `0x00000080` |
| 2 | `0x000003FF` | `0x00000040` | `0x00000010` | `0x00000001` |
| 3 | `0xFFFFFFFF` | `0xFFFFFFC0` | `0x00000000` | `0xFFFFFFFF` |
| 4 | `0x00000040` | `0x00000080` | `0x00000001` | `0x00000002` |

Derivation, pair 3: `0xFFFFFFFF + 0x20 = 0x1F`; `0x1F >> 6 = 0`. `0xFFFFFFC0 + 0x20
= 0xFFFFFFE0`; arithmetic `>> 6 = 0xFFFFFFFF` (`-1`). The `sar` is arithmetic, so
negative inputs round toward `-inf` after the `+0x20`, i.e. the rounding is
`floor((x+32)/64)`, not round-half-up for negatives — this is the drift risk the
brief flags.

The **final stored** values subtract the sprite origin (§7.3); they are not
statically pinnable until the resource reader is modelled. §8.1 gives the exact test
shape.

---

## 2. The camera chain

The camera is a self-contained state machine on `DS_000F0AF0` (camera x),
`DS_000F0AEC` (camera y), the shake pair `DS_000F0AF4`/`DS_000F0AF6`, and the mode
byte `DS_000F0AFE`. The previous cycle
(`2026-09-20-frontend-chain-derivations.md` §5) already derived `0x12D48`, `0x12CD4`,
`0x1317C`, `0x1324C`, `0x13290`, `0x1333C`; this section confirms those against the
raw and adds the four it deferred (`0x12DF0`, `0x12E3C`, `0x12DA8`, `0x1282C`).

### 2.1 `0x12D48` — the dispatcher

```
0x12d48  mov al, [0xf0afe]
0x12d4d  cmp al, 4 ; ja 0x12d78
0x12d51  and eax, 0xff
0x12d56  jmp dword ptr cs:[eax*4 + 0x12d34]   ; table = {0x12D5E,0x12D65,0x12D6C,0x12D73,0x12D78}
     mode 0 -> 0x12DF0            (0x12d5e call 0x12df0)
     mode 1 -> 0x12E3C            (0x12d65 call 0x12e3c)
     mode 2 -> 0x13290            (0x12d6c call 0x13290)
     mode 3 -> 0x1333C            (0x12d73 call 0x1333c)
     mode 4 or >4 -> no call
0x12d78  cmp dword [0xf0af0], 0x5d00 ; jle 0x12d8e
0x12d84  mov dword [0xf0af0], 0x5d00
0x12d8e  cmp dword [0xf0af0], 0xffffa300 ; jge 0x12da4
0x12d9a  mov dword [0xf0af0], 0xffffa300
0x12da4  ret
```

So the clamp is `DS_000F0AF0 = clamp(DS_000F0AF0, -0x5D00, +0x5D00)` after every mode.
`0x12D48` has two direct call sites (raw): `0x12D6?` no — its callers are in
`0x24C5C`'s tail family (`0x25422`, `0x2554B`). The arena frame does not call it; the
`game_frame` tail does.

### 2.2 `0x12DF0` — mode 0 (track one player)

```
0x12df0  push ebx/edx
0x12df2  edx = DS_000F0AF0
0x12dfa  al = DS_000F0AFF
0x12dff  eax = *(u32*)(0x1077A8 + al*4)     ; camera-target record pointer
0x12e06  eax = *(s32*)(record + 0x34)       ; target x
0x12e09  eax -= edx                          ; diff
0x12e0d  ebx = |eax|
0x12e17  if (|diff| >= 0x1800) {
0x12e1f      if (diff > 0)  diff -= 0x1800
0x12e2a      else           diff += 0x1800
0x12e2f      edx += diff
0x12e31  }
0x12e31  DS_000F0AF0 = edx
```

`DS_001077A8` is a separate two-entry pointer table (stride 4) of "camera target"
records; `DS_000F0AFF` selects the entry. Note this is a **different** table from the
fighter slot table at `0x1077B0`; the previous cycle's §5 did not cover mode 0.

### 2.3 `0x12CD4` — the y-stepper

```
0x12cd4  edx = DS_000F0AEC ; ebx = arg
0x12cdf  eax = arg - DS_000F0AEC ; ecx = |eax|
0x12ced  if (|diff| <= 0x100) new = arg
         else if (diff > 0)    new = old + 0x100
         else                  new = old - 0x100
0x12d03/0x12d1d  eax = *(s32*)0xF0AF2
0x12d08/0x12d22  sar  eax, 0x10
0x12d0b/0x12d25  new += eax
0x12d0d/0x12d27  DS_000F0AEC = new
```

The added term is a **32-bit read at `0xF0AF2` arithmetic-shifted right by 16**,
which is exactly the sign-extended 16-bit word at `0xF0AF4`:
`(s32)DSD(DS_000F0AF2) >> 16 ≡ (s16)DSW(DS_000F0AF4)` — the **shake offset**
`0x1324C` maintains and `port/src/game/effects.c` already treats as such. The demo's
camera-x high word is *not* this value; `0x12CD4` never reads `DS_000F0AF0` for the
term. The common tail at `0x12D1D` applies the term on all three paths (the
`|diff| <= 0x100` path jumps to it from `0x12CF7`).

Step `0x100`, threshold `0x100`. (Plan's `0x40` belongs to `0x13290`.)

### 2.4 `0x1317C` — camera-y clamp (unchanged from the previous record)

Body and float path are already derived in
`2026-09-20-frontend-chain-derivations.md` §5.2: `x = (s32)DS_001078F2 >> 16`; if
`x > 0x1400`, `d = x - 0x1400`, `arg = (s32)(d*d*C)` truncated (`0x61A4C` forces
FPU round-toward-zero), `C = 3.616898175096139e-05` (float at `0x804A8`, bytes
`26 B4 17 38`); then `0x12CD4(arg)` and the `DS_0009AF28[DS_00104AFC] >> 16` clamp.
Raw confirmed at `0x1317C`–`0x131F5`.

### 2.5 `0x13290` / `0x1333C` — modes 2 and 3 (unchanged from the previous record)

Already derived in the previous record §5.3/§5.4; the raw matches. Mode 2 centers on
the midpoint of `DS_001077B0`/`DS_00107844` `+0x18`, steps by `0x40`, and sets mode 4
only when `DS_001078FE != 0`. Mode 3 centers on the single record
`DS_001077B0 + DS_0010810D*0x94`, steps by `0x40`, and sets mode 4 **without** the
`DS_001078FE` gate.

### 2.6 `0x12E3C` — mode 1 (track the pair, front/back ordered)

`0x12E3C` selects the "front" fighter by comparing `DS_001077E4` (slot 0 `+0x34`)
with `DS_00107878` (slot 1 `+0x34`):

```
0x12e45  ebx = DS_000F0AF0
0x12e4b  eax = DS_001077E4
0x12e50  cmp eax, DS_00107878 ; setge al ; ebp = (DS_001077E4 >= DS_00107878)
0x12e5c  esi = 1 ; edi = ebp ; esi = 1 - ebp
         ; edx = slot[ebp], ecx = slot[esi]; the two +0x34 values are put in
         ; local[8 + ebp*4] and local[8 + esi*4]
0x12ebd  eax = local[8 + ebp*4] ; ebp = eax           (slot a's +0x34)
0x12ec5  eax = local[8 + esi*4]
0x12ecd  ebp -= eax ; diff = |diff|
0x12ed7  ebp = word[0x9af28]                          (the per-camera y limit)
0x12ee2  if (|diff| > limit) {
             if (slot_a[+0x34] < slot_a[+0x38]) { slot_a[+0x34] = slot_a[+0x38];
                                                   slot_a[+0x2C] = slot_a[+0x38];
                                                   call 0x18714; rec+0x18 = result }
             if (slot_b[+0x34] > slot_b[+0x38]) { slot_b[+0x34] = slot_b[+0x38];
                                                   slot_b[+0x2C] = slot_b[+0x38];
                                                   call 0x18714; rec+0x18 = result }
             |diff| = rec[a]+0x18 - rec[b]+0x18 (abs)
         }
0x12f44  for i in {a,b}: local[i] = slot_i[+0x34] - DS_000F0AF0
              if (|local[i]| > 0x1800) local[i] = local[i] ∓ 0x1800
0x12f89  if (|diff| > 0x3000)  eax = (local[0] + local[1]) / 2
         else if (local[0] == 0) eax = local[1]
         ; else eax = local[0]
0x12fb2  eax += DS_000F0AF0
0x12fba  call 0x12c7c(eax)         ; store via 0x12C7C
```

`0x12C7C` is the shared "camera x commit" helper (85 B, one caller) and is also used
by `0x12FD8`. Its body should be transcribed with `0x12E3C`. This is the most
complex camera mode and its exact behaviour is a partial gap (§7.5).

### 2.7 `0x12DA8` — the y-commit helper

```
0x12da8  push edx
0x12da9  al = DS_000F0AFE
0x12dae  test al, al
0x12db0  jne 0x12dd2
0x12db2     edx = DS_000F0AFF
0x12dba     eax = edx*0x25 ; mov ax, word[eax*4 + 0x1077e0]   ; slot[idx].+0x30
0x12dd0     jmp 0x12de3
0x12dd2     eax = DS_001077E0
0x12dd7     edx = DS_00107874
0x12ddd     cmp eax, edx ; jg 0x12de3   ; if (eax <= edx) eax = edx
0x12de1     mov eax, edx                ; -> signed MAX(eax, edx)
0x12de3  mov word [0x1078f4], ax        ; DS_001078F2 high word = selected y
0x12de9  call 0x1317C
0x12dee  ret
```

So `0x12DA8` chooses the "selected player **y**" (mode 0: the `DS_000F0AFF` slot's
`+0x30`, a copy of the record's `+0x1C`; otherwise the **signed max** of
`DS_001077E0`/`DS_00107874`), stores its low word to `DS_001078F4`, and runs the
y-clamp `0x1317C`. It is called from the arena frame at `0x26534`. The label is y, not
x: `DS_001078F4` is the high word of `DS_001078F2`, which `0x1317C` reads as its
`x = (s32)DS_001078F2 >> 16` and compares against `0x1400` before stepping the
camera **y** `DS_000F0AEC`.

### 2.8 `0x1282C` — the dust/scene spawner

```
0x1282c  ... ax = word[0xef6dc] ; and al, 0x3f ; jne 0x128c9    ; gate: every 64th frame
0x12849  eax = 7 ; call 0x5d7dc                                  ; rng(7)
0x12853  test al, 3 ; jne 0x128c9                                ; 3 of 4 returns
0x12857  test al, 4 ; if set:  edi=0x2800, esi=0xFF80, ebx=0
                    else:      edi=0xFFFFD800, esi=0x80, ebx=0x4000
0x12878  eax = 0x1300 ; call 0x5d7dc                              ; rng(0x1300)
0x1288f  eax = 0x2000 ; call 0x5d7dc                              ; rng(0x2000)
0x128b6  eax = 0xBB254
0x128c0  call 0x2ae14                                            ; actor_spawn(0xBB254, edx=DS_000F0AF0+edi, ebx, ecx, push ee)
0x128c5  mov word [eax+0x34], si
0x128c9  ret
```

So `0x1282C` is a **rare** per-frame effect: it is gated on
`(DS_000EF6DC & 0x3F) == 0` (every 64th frame) and then `rng(7) & 3 == 0` (one in
four of those), i.e. roughly one in 256 frames, and it draws 3 RNG values per
activation.

### 2.9 The `DS_000F0AEC`/`DS_000F0AF0`/`DS_000F0AF4` semantics

| global | type | meaning | writers |
|---|---|---|---|
| `DS_000F0AF0` | `s32` | camera x; clamped to `[-0x5D00, 0x5D00]` by `0x12D48` | `0x12D48` modes, `0x12DF0`, `0x12E3C`, `0x12FD8`, `0x13290`, `0x1333C` |
| `DS_000F0AEC` | `s32` | camera y; stepped by `0x12CD4`, clamped by `0x1317C` | `0x12CD4`, `0x1317C` |
| `DS_000F0AF4` | `s16` | shake offset; `0x12CD4` adds `(s16)DSW(0xF0AF4)` to camera y | `0x1324C` (dormant) |
| `DS_000F0AF6` | `s16` | shake velocity (`-= 0x20`/call) | `0x1324C` (dormant) |
| `DS_000F0AFE` | `u8` | camera mode `0..4` | `0x13290`/`0x1333C` set 4; `0x12DA8` reads |
| `DS_000F0AFF` | `u8` | player index for mode 0 | camera-select code (elsewhere) |
| `DS_001077A8` | ptr[2] | camera-target record pointers (stride 4) | elsewhere |
| `DS_00104AFC` | `u16` | camera index into `DS_0009AF28` | set by state 6 to `rng(7)` (§6.1) |

**`DS_000F0AF2` is not a global of its own.** It is only ever the address of the
32-bit read `*(s32*)0xF0AF2` in `0x12CD4`; the read's high 16 bits are `DS_000F0AF4`,
so the term is the sign-extended shake offset, **not** the high word of the camera x
`DS_000F0AF0`.

### 2.10 `camera_init` registers nothing

The update table `DS_000A8644` and render table `DS_000A86C4` contain no camera
function (the tables are dumped in the previous record §4.3). `0x1324C` at update
index 0 is the only camera-related entry and it is already registered by
`port/src/game/effects.c:512`. **Task 2's `camera_init` must not double-register it
and has nothing else to register**; if it needs a body it should only initialise the
camera globals (all of which are BSS-zero at load).

### 2.11 `0x1324C` stays dormant (the brief's Step 2 question)

`0x1324C` runs only when `DS_00104AE8` bit 0 is set. Every reference to `0x104AE8` in
the fixed image (27 sites) is:

* `0x1326A`/`0x13277` — `0x1324C` reads then clears bit 0 itself;
* `0x198BF` `or byte [0x104AE8],4` (bit 2); `0x25E1F` `or byte [0x104AE8],0x40` (bit 6);
* `0x230C9`, `0x27E68`, `0x37CA0`, `0x48D77` — `and …,0xDF/0xFB/0xEF/0xFD` (clears);
* `0x20E1E`, `0x20EC5`, `0x28DC6`, `0x2BB15`, `0x41437` — `mov dword [0x104AE8],reg`
  where `reg` is `xor`-zeroed immediately before (`0x20E10`, `0x20EB9`, `0x28DB8`…);
* `0x24CD7`, `0x25FC5`, `0x26B1E`, `0x29003`, `0x290B8`, `0x294BE`, `0x48D2C` —
  reads/read-modify-writes that preserve or clear bits.

`or byte [0x104AE8],1` occurs **zero** times. Therefore **no demo path makes
`0x1324C` live**; it stays a dormant, already-ported updater. Do not propose
re-porting it. (This supersedes the brief's "the demo may be what makes it live".)

---

## 3. The arena frame `0x263F4` and its remaining callees

### 3.1 `0x263F4` — the demo arena frame, exact order

Raw call order (all confirmed at the listed addresses):

```
0x263f4  push ebx/ecx/edx
0x263f7  call 0x3c5cc
0x263fc  xor edx,edx ; xor eax,eax ; mov dl,[0x10782a]
0x26406  mov ecx, 0x100b62
0x2640b  call 0x16d58                       ; side 0: (ax=0, dx=char0)
0x26410  xor edx,edx ; mov eax,1 ; mov dl,[0x1078be]
0x2641d  mov ebx, 0x100b00
0x26422  call 0x16d58                       ; side 1: (ax=1, dx=char1)
0x2642c  edx = DS_001077E4 ; DS_001077E8 = edx     ; latch x
0x2643a  edx = DS_00107878 ; DS_0010787C = edx     ; latch x
0x26450  call 0x17fa0   ; side 0 projection (pre)
0x26473  call 0x17fa0   ; side 1 projection (pre)
0x26478  call 0x17580
0x2647d  call 0x1958c
0x26482  xor eax,eax ; call 0x19068
0x2649d  ... ; 0x264a4 call 0x17fa0               ; side 0 projection (mid)
0x264c7  call 0x17fa0                             ; side 1 projection (mid)
0x264cc  call 0x1975c                             ; the think step
0x264e5  ... ; 0x264ec call 0x17fa0               ; side 0 projection (post)
0x2650f  call 0x17fa0                             ; side 1 projection (post)
0x26514  call 0x3cb68
0x2651b  xor eax,eax ; call 0x35658               ; HUD/health side 0
0x26520  mov eax,1 ; call 0x35658                 ; HUD/health side 1
0x2652a  call 0x49c78
0x2652f  call 0x1282c
0x26534  call 0x12da8
0x26539  pop edx/ecx/ebx ; ret
```

This is the order Task 3's `fight_arena_frame` must transcribe. The six `0x17FA0`
calls are not redundant. Note the two `0x16D58` calls in the prologue use
`(ax,dx) = (0, char_slot0)` and `(1, char_slot1)`; the character bytes are
`DS_0010782A`/`DS_001078BE`.

### 3.2 `0x3C5CC` — three stores

```
0x3c5cc  mov dword [0x107ee0], 0
0x3c5d3  mov dword [0x107d50], 0
0x3c5da  mov dword [0x107d54], 0
0x3c5e1  ret
```

(23 B; the previous cycle's transcription is confirmed.)

### 3.3 `0x16D58` — per-side screen base

```
0x16d58  if (ax in [0,1] && dx in [0,10)) {
0x16d73      edx = dx
0x16d75      eax = (dx << 10) + 0xCC300            ; edx << 10 == edx*0x400
0x16d7d      dword [0x100a70 + ax*4] = eax
0x16d84      eax = dx*0x3E8 + 0xC9BF0
0x16d98      dword [0x100a98 + ax*4] = eax
0x16d9f  }
```

Wait: the raw computes `eax = (dx << 10) + 0xCC300`? Re-read the listing:
`mov eax,edx ; shl eax,0xa ; add eax,0xcc300` — yes `dx*0x400 + 0xCC300`. And
`eax = edx; shl eax,5; sub eax,edx; shl eax,2; add eax,edx; shl eax,3; add
eax,0xc9bf0` = `dx*0x1F*4*... ` = `dx*0x3E8 + 0xC9BF0`. So the two tables at
`0x100A70` and `0x100A98` hold per-side pointers. `ax` is the sign-extended first
argument; the second is `dx`. Note the demo prologue passes `ax` as the side in
`eax`, but `0x16D58`'s first parameter is the *word* `ax` and second is `dx`.

### 3.4 `0x17580` — the per-frame decay

```
0x17580  if (DS_00107824) DS_00107824--
         if (DS_001078B8) DS_001078B8--
         if (DS_00107826) DS_00107826--
         if (DS_001078BA) DS_001078BA--
0x17644  DS_00100B54 = 0 ; DS_00100AF8 = 0 ; DS_00100AFC = 0
0x17647  DS_00100B08 = (DS_00100B08 * 0xF3D + 0x800) / 0x1000   (signed, trunc toward 0)
0x17650  DS_00100B0C = (DS_00100B0C * 0xF3D + 0x800) / 0x1000
0x17653  DS_00100B00 = (DS_00100B00 * 0xD56 + 0x800) / 0x1000
0x17656  DS_00100B04 = (DS_00100B04 * 0xD56 + 0x800) / 0x1000
0x17698  if (0x140E4()) {                         ; exactly ONE call (corrected, see below)
0x176A1      if (DS_00100B60) 0x170A0(EAX=0)
0x176B1      if (DS_00100B61) 0x170A0(EAX=1)
         }
```

**Correction (raw wins).** This section previously transcribed two `0x140E4()`
calls, one per store. The raw has exactly **one** `call 0x140e4` at `0x17698`
(`e847caffff`); its `test al,al; je 0x176C4` gates **both** stores:
`0x176A1`–`0x176AF` (`DS_00100B60` → `0x170A0(EAX=0)`) and `0x176B1`–`0x176C3`
(`DS_00100B61` → `0x170A0(EAX=1)`). Task 2 found and independently verified
this; `port/src/game/camera.c`'s `camera_decay` already carries the single-gate
form as a PORT skip. The listing above is corrected to match.

The raw form of the signed divide is
`iVar3 = x >> 31; result = ((x + iVar3*-0x1000) - (iVar3<<11 < 0)) >> 12`, i.e. a
signed truncating divide by `0x1000` with `+0x800` rounding, which the port should
transcribe exactly. `0xF3D/0x1000 = 0.95239...`, `0xD56/0x1000 = 0.83350...`.
Because the projection **overwrites** `0x100B00`–`0x100B0C` each call, the decay
only affects whatever reads them between `0x17580` and the next projection pair
(`0x1958C`/`0x19068`).

### 3.5 `0x1958C` and `0x19068` — the two per-frame fighter passes

Both loop over the two sides. `0x1958C` is the pre-think pass; `0x19068` is called
with `EAX = 0` from the arena frame. Neither is a "render": the plan's Task 3 label
`fighter_render(u8 side)` does not match the raw. Both are characterised, not fully
derived:

* `0x1958C` (464 B) gates on `DS_001078FA == 2`, calls `0x33950`/`0x19020`/`0x3AFC4`
  per side, decrements the `DS_00100AF8`/`DS_00100AFC` pair, computes the two
  `0x18950` reachabilities, and picks a winner. Its **only RNG site** is `0x19714`
  (`eax=2`, `call 0x5d7dc`) inside the exact-tie branch
  (`DS_00107838 == DS_001078CC` and `DS_0010789E == DS_0010780A`), used as
  `[0x100AF8 + rng*4] = 0`. Then `0x193B0` runs when a flag is non-zero.
* `0x19068` (250 B) gates on `DS_00107802`/`DS_00107896 != 0x13`, calls `0x3C570`,
  and loops a hit-stun/timer update over `DS_00100B58`/`DS_00100B5A`/`DS_00100B5C`/
  `DS_00100B5E` and the record pair, calling `0x3CF38` and `0x1922C`. **No RNG.**

The full semantics of both are gaps (§7.6); the record pins their call order, gates,
globals and RNG so Task 3 can wire them and defer their interiors.

### 3.6 `0x3CB68` — the 2×32 slot pass

```
0x3cb68  DS_00107EDC = 0
0x3cb6e  do {
0x3cb74      DS_00107EE4 = 0x1A570(side)
0x3cb7e      DS_00107ED8 = 0
0x3cb84      do { 0x3C88C(); DS_00107ED8++ } while (DS_00107ED8 < 0x20)
0x3cba0      DS_00107EDC++
         } while (DS_00107EDC < 2)
```

So `0x3C88C` is called 64 times (2 sides × 32 slots). `0x3C88C` (730 B) is a large
slot/draw helper with 4 callees (`0x3C600`, `0x3C6A8`, `0x3C758`, `0x3C800`); it is
on the demo path and contributes to the frame, but is a gap (§7.7).

### 3.7 `0x35658` — the HUD/health pass is load-bearing

`0x35658(side)` is called twice per frame (`0x2651B`, `0x26525`). Its raw call order
includes:

```
0x357d6  call 0x33C78
0x357ee  call 0x34038
0x357f5  call 0x38D24
0x357fc  call 0x34B6C     <-- reaches the think mapper
0x35803  call 0x3531C
0x35813  call 0x2A1FC
0x3581c  call 0x354F0
0x35824  call 0x354F0
0x35829  call 0x186C4
```

and `0x34B6C` (541 B) calls `0x1A978` at `0x34C75`, which calls `0x3B134` at
`0x1A9AE`. Therefore the command word `DS_001088E0`/`E2` is re-derived by the HUD
pass after the think step. Task 3 must port at least the `0x35658 → 0x34B6C →
0x1A978` spine (or make `fight.c` call `0x1A978` directly at the right point); the
rest of the HUD/health path is cycle 2. `0x35658` also reads the per-side timers
`DS_00107828`, `DS_0010783C`, `DS_00107838`, `DS_00107840`–`0x42` and the
camera-target table `DS_001077A8`. Full body is a gap (§7.8).

### 3.8 `0x33F08` — health bars

`0x33F08` loops `iVar4` over `{0,4}` reading `DS_001077A8[i]` (camera-target
records); for each it selects the per-character constant from the **same** table as
`0x17EEC` (see §1.2), computes `s = (actor.word0 & 0x7fff) - char_const`, and if
`(rec+0x41 & 0x20) == 0 && 0 <= s < 0x4B1` writes
`*(u16*)(rec[9] + s*2)` into `rec[1]+8`, else writes `0x1E1`. It sets/clears bit
`0x40` of `rec[1]+0x29` from the actor's bit 15, then calls `0x2A408`. Called by the
state-7 case at `0x11E94` and by `0x263F4`? No — the arena frame does **not** call
`0x33F08`; the state-7 dispatcher calls it after `0x263F4` (`0x11E94`, and the
alternate handler at `0x11C44`). The record's Task 5 wiring must call it there.
Reading `rec[9]` (a health-bar sprite table) is a gap (§7.9).

---

## 4. `0x49C78` — the scene/effects pass

`0x49C78` is 2492 B, called every demo frame at `0x2652A` (and from six other arena
variants). It is a per-frame pass over a doubly-linked list rooted at
`DS_0010884C`, switching on each entry's byte `+0x1E`:

```
0x49c78  push ebx/ecx/edx/esi/edi/ebp ; sub esp, 0x1c
0x49c81  if (word[0x104b00] == 9) { initialise four locals }
0x49caa  eax = 0x493F0() ; DS_00108874 = eax
0x49caf  ecx = DS_0010884C
0x49cc5  if (ecx == 0x10884C) goto 0x4a476          ; empty list -> tail
0x49cd1  loop over list entries {
             ebp = [ecx]                             ; next
             al = [ecx+0x21]                         ; side/index
             ebx = [ecx+8]                           ; record pointer
             si = [ebx+0x48]
             word[esp + al*2]++ ; dword[esp+8]++
             0x4B69C(...)
             switch ([entry+0x1E]) { ... cases ... }
             ecx = ebp
         }
0x4a476  ... epilogue: clear DS_001088BF, and byte[0x104AEC] &= 0x7F
```

The 22 direct callees are `0x493F0`, `0x4B69C`, `0x4AAD0`, `0x4BD4C`, `0x4AC38`,
`0x496AC`, `0x2BE1C`, `0x2BC30`, `0x5D7DC`, `0x2B150`, `0x2C3FC`, `0x4AF04`,
`0x4B470`, `0x4B430`, `0x4B2AC`, `0x4A7D4`, `0x2BE00`, `0x496DC`, `0x4A868`,
`0x4A928`, `0x4A634`, `0x4987C`.

**RNG sites (8 direct encodings, 3–5 logical draws), with ranges and gates:**

| site | call | gate |
|---|---|---|
| `0x49E3A` | `rng(0x3C)` then `+0x3C` → `[entry+0x18] = 60..119` | inside the `+0x1E` case body reached from `0x49D1?` |
| `0x4A305` | `rng(0xC00)`, `edx - rng` | taken when `0x2BE00() > 0` |
| `0x4A315` | `rng(0xC00)`, `edx + rng` | taken when `0x2BE00() <= 0` (the other arm) |
| `0x4A439` | `rng(0xC00)`, `edx - rng` | mirror of the above for a second entry |
| `0x4A449` | `rng(0xC00)`, `edx + rng` | mirror |
| `0x4A4C5` | `rng(0x14)` then `+0x78` → `[0x1088B0]` | when `byte[0x1088C3] == 0` |
| `0x4A4F7` | `rng(0x14)` then `+0x78` → `[0x1088B0]` | when `[0x1088C3] != 0 && word[0x1088B0] == 0` |
| `0x4A611` | `rng(2)` then `0x4987C` | at the common tail |

(That is eight `call 0x5d7dc` encodings, but three pairs are `if/else`: each pass
executes one of each pair, so a pass makes **3–5 draws** depending on entry states (and
**zero** when the list is empty — `0x49CC5` jumps straight to the epilogue at
`0x4A476`) —
the fixed `0x49E3A`, one of `0x4A305/0x4A315`, one of `0x4A439/0x4A449`, one of
`0x4A4C5/0x4A4F7`, and the tail `0x4A611`.)

**Correction (raw wins).** Those eight are only `0x49C78`'s **own direct** sites.
The pass's **subtree** draws further RNG every frame: `0x4A634` is called
unconditionally at `0x4A591` and carries `0x4A664`, `0x4A69E`, `0x4A6A9`; the tail
callee `0x4987C` carries `0x499B1`, `0x49B08`; and `0x496DC` (called from the
case-13/14 bodies) carries `0x4978E`, `0x4981D`, `0x4982E`. So a frame whose
`0x49C78` reaches those bodies consumes RNG beyond the direct-site bound — the
per-frame count is not bounded by 3–5 once the effect list is non-empty.
`0x493F0` (`0x49CAA`) was checked and draws nothing. The `0x4A634`/`0x4987C`/
`0x496DC` bodies are the §7.4 gap; this note records the stream consequence, it
does not port them. (Task 3 issued the eight direct sites with their gates and
recorded the subtree shortfall in its report.)

The internal entry semantics (`0x4B69C`, `0x4AC38`, `0x4987C`, `0x4A634`, …) are a
gap: this pass is the largest single unknown in the slice and it operates on the
effect list, not the fighters, so it is **not load-bearing for fighter motion**. It
is load-bearing for pixels, so it is a named gap for cycle 1 and cycle 2's to close.
§7.10.

---

## 5. The think/AI chain and the RNG answer

### 5.1 `0x1975C` — the think step

```
0x1975c  push ...
0x19763  call 0x17CB0
0x1976d  iVar2 = 0
0x19770  loop {
0x19770      call 0x33950                  ; fighter context for iVar2
0x19778      if (*(s32*)(0x100AD0 + side*4) > 2) {
0x19791          call 0x1922C
0x1979b          if (0x3962C()) { *(u8*)(rec+0x8A)=0; 0x18B44(); return; }
0x197bf          if (0x396AC()) { *(u8*)(rec+0x8A)=0; 0x18B44(); return; }
0x197e?          *(u8*)(rec+0x67) = cl
0x197ef          call 0x3B464           ; the think
0x197f8          call 0x3B938
0x197ff          call 0x39278
             }
0x197f0?     iVar2++
         } while (iVar2 < 2)
```

So `0x1975C` iterates both sides and calls `0x3B464` once per active fighter
(`DS_00100AD0[side] > 2`), at most twice per frame.

### 5.2 `0x3B464` — the per-fighter think driver

```
0x3b464  push ... ; 0x33A10(0x3B476) ; 0x3C59C()
0x3b491  if (0x3C59C()) return
0x3b4a5  if (*(s8*)(rec+0x64) == -1) return
0x3b4b1  0x3AFC4()
0x3b4c5  *(u8*)(ctx+0x67) = 1
0x3b4cc  if (0x3B298()) { *(u8*)(rec+0x8A) = 0; ... 0x3B080; 0x3AD98; goto done }
0x3b4f0  0x2BD44() gated on slot+0x4B ...
0x3b515  if (*(s32*)(ctx+0x14)) { call *(ctx+0x14); if nonzero ctx+0x14 = 0 }
0x3b51f  bVar1 = *(u8*)(*(s32*)(rec+8) + 0x48)
0x3b52b  switch on bVar1: 4/5 -> 0x36D20, ctx+0x18=ctx+0x1C=0 ; 8 -> 0x1922C; 0x235C4
0x3b554  0x39834()
0x3b56c  if (ctx+0x54 == 2) { 0x18B04; 0x39F40; ... }
0x3b594  else { 0x3AFC4; 0x3A95C; if (*(s8*)(rec+100) not in {0,5}) switch(ctx+0x90) {1..4 pass; default 0x188DC} ; if (ctx+0x54 != 2) 0x3B080 }
0x3b6ac  *(u8*)(*(s32*)(ctx+0xC)+0x41) |= 0x80
0x3b6c0  *(u8*)(*(s32*)(ctx+8)+100) = 0xFF
```

`ctx` is the `0x33A10` context: `ctx[1]=side`, `ctx[2]=&slot[other]`,
`ctx[3]=&slot[self]`, `ctx[4]=rec_other`, `ctx[5]=rec_self`; `rec` is the current
fighter's record. Called once per active fighter from `0x1975C`.

### 5.3 `0x3B298` — the command/state dispatch

```
0x3b298  0x33A10 ; 0x3AFC4 ; 0x3B134          ; <-- the mapper runs here
         *(u16*)(ctx+0x86) = *(u16*)(ctx+0x84)
0x3b2?d  if (anim-bit tests) {
             uVar3 = 0x1AB5C()
             for (i = 0; i < DAT_000BEEF2 >> 16; i++)
                 if (0x46460() & uVar3) { bVar1/bVar2 = true based on bits 0x4000/0x8000 }
             uVar4 = DS_001088E0[side]
             if (uVar4 & uVar3) { ... }
             ... set/clear DS_00100C43[side] bits 0x20/0x10 ...
             0x1A734(); return 1
         } else return 0
```

### 5.4 `0x3B134` — the command-word mapper (raw, precisely)

`0x3B134(side=EAX, ctx_ptr=EDX, ctx2=EBX, override_byte=BL)` writes a 16-bit command
to `word[DS_001088E0 + side*2]`. It calls `0x33A10` (fills the context) and `0x3AFC4`
(fills three animation pointer slots at `ctx+0x18`), then:

```
0x3b168  if (slot[side].+0x63 == 0) return             ; gate A
0x3b17a  if (0x1AB10(...) == 0) return                 ; gate B
0x3b187  eax = 0x64 ; call 0x5d7dc                     ; RNG DECISION ROLL, rng(100)
0x3b191  ebx = DS_001082C8[side]
0x3b19a  cl = DS_0010452C ; ecx <<= 4
0x3b1a3  ebx = *(u32*)(0xBEDF2 + ebx*2 + ecx) ; sar ebx,0x10
0x3b1ad  if (rng > threshold && override_byte == 0) return
```

Then the command decision:

```
0x3b1bc  if (slot[self].+0x2C > slot[other].+0x2C) base = 0x1000 else base = 0x2000
0x3b1d8  if (slot[other].+0x64 != 0xFF && slot[other].+0x08 != 0) {
0x3b1fc      cx = (s16)word[ slot[other].+0x08 -> +0x34 ]
0x3b205      cmd = (cx < 0) ? 0x6000 : 0x5000
0x3b207/15   word[0x1088E0 + side*2] = cmd ; return
         } else {
0x3b220      bl = byte[ anim[2] + 2 ] ; bit0 = bl & 1
0x3b22d      if (bit0 == 0) { word[0x1088E0+side*2] = base ; return }
0x3b24a      bit1 = byte[ anim[2] + 2 ] & 2
0x3b25d      if (bit1 == 0) { word[0x1088E0+side*2] = base | 0x4000 ; return }
0x3b26f      word[0x1088E0+side*2] = 0x8000
0x3b27f      if (0x3BDB0()) 0x3BDDC()
         }
```

`slot[self]` is `ctx[3]`, `slot[other]` is `ctx[2]`; `anim[2]` is
`ctx[0x18+8] = *(u32*)(ctx+0x20)`, the third `0x3AFC4` pointer. `base` is the
facing-derived direction (`0x1000` if self is to the right of the other).

The mapper therefore turns: (slot gate, fighter state, a `rng(100)` reaction roll
against a per-character/per-`DS_0010452C` threshold, the two `+0x2C` positions, the
other slot's `+0x64`/`+0x08->+0x34` stance, and the animation pointer's bits) into one
of `{0x1000, 0x2000, 0x4000, 0x5000, 0x6000, 0x8000, base|0x4000}` at
`DS_001088E0`/`E2`. `0x3B298` then reads `DS_001088E0[side]` bits 0x2000/0x1000/0x4000/
0x8000; `0x3CBC4`/`0x3CC58`/`0x1A5D4`/`0x1A978` also read it.

### 5.5 `0x3BDDC` — the attack/command consumer

`0x3BDDC(side)` reads `DS_001088E0[side]`; if bit 15 is set it clears the fighter's
`rec+0x34`/`rec+0x43`/`rec+0x42`, sets `ctx+0x5F = 0xFF`, and (when `DS_00107803[side]
!= 0`) selects one of two static tables `0xBEF28`/`0xBEF64` by `0x4649C()`, stores it
at `DS_00107D40[side]`, calls `0x3C480(0x3F800000)`, sets `DS_00107802[side]=3`,
`DS_00107804[side]=2`, `DS_00107803[side]=4`, `DS_001078F8[side]=1`, and writes
`DS_001077FE[side]` from the command bits. It is the transition into the "attack"
state; 7 callers, all off the direct think spine except `0x3B28C` (inside `0x3B134`).

### 5.6 `0x1A978` — the second command pass

`0x1A978` is reached only from `0x34B6C@0x34C75`, which is reached from `0x35658`
(HUD). It calls `0x3B134` at `0x1A9AE` when `*(s8*)(ctx+0x5F) != -1 &&
*(s8*)(ctx+0x62) != 0`, and then manages the stance timer `ctx+0x61`/`ctx+0x60`,
stance byte `ctx+0x54`, and transition byte `ctx+0x53`, calling `0x1A6AC`,
`0x1A640`, `0x1A8F4`. Because `0x35658` runs after `0x1975C`, this pass can overwrite
the think's command word. Task 4 must include it for fidelity; §7.11 for the
remaining helpers.

### 5.7 `0x18C14` is off the demo path

BFS over the direct `E8` call graph from `{0x263F4, 0x24C5C, 0x11A8C}` reaches 922
functions and does **not** include `0x18C14`. Its 37 call sites
(`0x14D17 … 0x4869F`) are the interactive-match/mode handlers, out of cycle 1's
scope. Task 4's "whichever of `0x18C14`/`0x1A978` the record finds on the demo path"
resolves to `0x1A978` only. `0x18C14` is a correction (§0.2.7) and is not ported.

### 5.8 Behaviour helpers reached by the demo think

| helper | role | in cycle-1 scope |
|---|---|---|
| `0x33A10` | fill `{1-side, side, &slot[1-side], &slot[side], rec_other, rec_self}` | yes (pure) |
| `0x39F40`/`0x3AFC4` | select animation pointer triples from `0xDE114`/`0xA3528`/`0xA6728` | yes (static) |
| `0x1AB10` | fighter-state gate (`rec+0x54`/`rec+0x53` ∈ {0,1,…}) | yes (pure) |
| `0x1AB5C` | input-mask builder (feeds `0x3B298`) | gap (§7.12) |
| `0x46460` | per-player input record read | yes (reads `DS_001088xx`) |
| `0x1A570` | "actor bit 15 clear" predicate (`(word[actor] & 0x8000) == 0`) | yes (pure) |
| `0x1922C`/`0x18B44`/`0x3B938`/`0x39278` | think-step tails | gap (§7.12) |

### 5.9 The RNG answer (the cycle's decision point)

**The demo's command mapper (`0x3B134`) does not draw every frame.** It draws at most
one `rng(100)` (`0x3B18C`, `mov eax,0x64; call 0x5d7dc`) per fighter per frame, and
only when both gates pass:
`slot[side].+0x63 != 0` (`0x3B168`) and `0x1AB10(...) != 0` (`0x3B17A`). It is a
**conditional reaction roll**, not a per-frame or per-decision guaranteed draw: after
the roll it may still return without writing a command (`rng > threshold &&
override == 0`, `0x3B1AD`).

**But the demo frame as a whole draws every frame.** Three non-AI passes consume the
stream on the state-7 path:

1. **`0x49C78`** (`0x2652A`, every frame): **8 direct `0x5D7DC` encodings**
   forming 3–5 logical draws — the fixed `0x49E3A` `rng(0x3C)`, one of
   `{0x4A305,0x4A315}` `rng(0xC00)`, one of `{0x4A439,0x4A449}` `rng(0xC00)`, one of
   `{0x4A4C5,0x4A4F7}` `rng(0x14)` (mode-9 only), and the tail `0x4A611` `rng(2)` —
   **but that is not the frame's bound.** The pass's subtree draws more:
   `0x4A591` calls `0x4A634` **unconditionally**, and `0x4A634` carries
   `0x4A664`/`0x4A69E`/`0x4A6A9`; the tail callee `0x4987C` carries
   `0x499B1`/`0x49B08`; and `0x496DC` (called from the case-13/14 bodies) carries
   `0x4978E`/`0x4981D`/`0x4982E`. An empty list (`0x49CC5 je 0x4A476`) skips the
   entry walk but **still reaches `0x4A634`** at `0x4A591`, so "empty list → zero
   draws" is not guaranteed. So the non-AI draw count is 3–5 direct **plus** the
   subtree's conditional draws. (Corrected — §4's correction carries the addresses.)
2. **`0x1958C`** (`0x2647D`, every frame): one `rng(2)` at `0x19714`, only in the
   exact-tie branch.
3. **`0x1282C`** (`0x2652F`, every frame): three draws
   (`0x1284E rng(7)`, `0x1287D rng(0x1300)`, `0x12898 rng(0x2000)`), but all gated on
   `(DS_000EF6DC & 0x3F) == 0` (every 64th frame) and then `rng(7)&3 == 0`, i.e.
   roughly once per 256 frames.

Plus, because `0x35658` calls `0x1A978 → 0x3B134` (`0x34C75`/`0x1A9AE`), the
`rng(100)` reaction roll can fire **twice per fighter per frame** (once in the think
step at `0x1975C`, once in the HUD pass), each conditional.

**Order per demo frame, with draw counts:**

```
state 7 (case 7 of 0x11D04):
  0x263F4
    0x1958C                        -> rng(2)            (rare, tie-break)
    0x1975C -> 0x3B464 -> 0x3B298 -> 0x3B134   rng(100) (conditional, per fighter)
    0x49C78                        -> 3..5 direct draws (order 0x49E3A, 0x4A305|0x4A315,
                                       0x4A439|0x4A449, 0x4A4C5|0x4A4F7, 0x4A611)
                                       + the 0x4A634/0x4987C/0x496DC subtree draws
    0x1282C                        -> rng(7), rng(0x1300), rng(0x2000) (every 64th frame)
    0x35658 x2 -> 0x34B6C -> 0x1A978 -> 0x3B134   rng(100) (conditional, per side)
    0x12DA8                        -> none
  0x33F08                           -> none
  tails 0x10DB0 / 0x10E18 / 0x2BF08 -> none
game_frame tail (0x24C5C, if DS_00104B15):
  0x3BB90 (cycle 2), 0x12D48, 0x186D0, 0x2A690, 0x33F08 -> none
```

**Consequence for Task 7.** The demo's determinism cannot be pinned by the two
character picks alone: on frames where `0x49C78` runs it advances the stream by its
3–5 direct draws **plus** the `0x4A634`/`0x4987C`/`0x496DC` subtree draws, and it is
advanced conditionally by `0x1958C`/`0x3B134`. If the port reproduces `0x49C78`'s
call order and its entry states, the stream stays aligned; if `0x49C78` is deferred
as a gap (§7.10) the port will **not** consume the same RNG stream and the demo will
diverge at the first frame whose `0x49C78` draws affected an entry state — which can
be **earlier than the first landing hit**. The cycle-1 bound is therefore conditional
on how much of `0x49C78` Task 3 ports: at minimum the direct RNG call sites must be
issued in order even if the entry effects are stubbed, and the subtree draws are a
second, separate source of drift once their bodies run. This record flags that
explicitly rather than assuming the bound is the first landing hit.

`0x5D7DC` itself is the 32-bit LCG already reproduced by `port/src/game/rng.c`:
`state = state * 0xB90D12B9 + 0x38CE051F`; `result = ((state >> 16) * (range & 0xFFFF)) >> 16`.

---

## 6. States 6 and 7 and the `game_frame` tail

### 6.1 `0x11A8C` — state 6 (317 B)

Full fixed-up body:

```
0x11a8c  push ebx/ecx/edx
0x11a8f  mov eax, 0x100 ; call 0x2c3fc
0x11a99  call 0x29d60
0x11a9e  mov eax, 0x1d ; call 0x2c06c
0x11aa8  mov eax, 7 ; call 0x5d7dc            ; draw 1: rng(7)
0x11ab2  mov ebx, eax
0x11ab4  mov word [0x104afc], ax               ; DS_00104AFC = draw1
0x11aba  xor eax, eax ; mov edx, 1
0x11ac1  mov ax, bx
0x11ac4  call 0x20df4
0x11ac9  mov edx, ebx ; xor eax, eax
0x11acd  call 0x41350                          ; P0 char = draw1
0x11ad2  xor eax, eax ; mov edx, 7 ; call 0x33eb4
0x11ade  mov eax, 6
0x11ae3  mov [0x1082c8], edx                   ; DS_001082C8 = 0x33EB4 result
0x11ae9  call 0x5d7dc                          ; draw 2: rng(6)
0x11aee  lea edx, [ebx + eax]
0x11af1  cmp edx, 7 ; jl 0x11af9
0x11af6  sub edx, 7                            ; P1 char = (draw1+draw2) % 7
0x11af9  mov eax, 1 ; call 0x41350
0x11b03  mov eax, 1 ; call 0x33eb4
0x11b0d  mov ah, 1
0x11b0f  mov ebx, 3
0x11b14  mov [0x104b15], ah                    ; DS_00104B15 = 1
0x11b1a  xor eax, eax ; mov dl, 1
0x11b1e  call 0x1d890                          ; HUD spawn
0x11b23  mov [0x104b1b], dl                    ; DS_00104B19.byte2 = 1
0x11b29  mov dh, [0x104529]                    ; DS_00104528+1
0x11b2f  mov [0x1082cc], ebx                   ; DS_001082CC = 3
0x11b35  test dh, 2 ; jne 0x11b9a
0x11b3a  (eax,edx,ecx) = (6,2,0x2000) ; call 0x1c500 ; ebx=eax ; eax=-1 ; call 0x2f198
0x11b5a  (7,4,0x2000) ; 0x1c500 ; 0x2f198
0x11b7a  (8,6,0x2000) ; 0x1c500 ; 0x2f198
0x11b9a  mov edx, 7 ; xor ah, ah ; mov ebx, 0x384 ; mov al, [0xf0a72]
0x11bab  mov word [0xf0a64], dx                ; DS_000F0A64 = 7
0x11bb2  mov word [0xf0a6a], bx                ; DS_000F0A6A = 900
0x11bb9  mov word [0xf0a6c], ax                ; DS_000F0A6C = (u16)DS_000F0A72
0x11bbf  mov byte [0xf0a6f], ah                ; DS_000F0A6F = 0
0x11bc5  pop edx/ecx/ebx ; ret
```

Corrections vs plan: the two RNG draws are **coupled** (P1 = `(draw1+draw2)%7`) and
`draw1` also sets `DS_00104AFC`; `DS_00104B19.byte2` is written `1` (from `mov dl,1`
before `0x1D890`), not a register handoff (verify `0x1D890` preserves `DL` — it is
the HUD spawner; §7.13). `DS_001082C8` is the `0x33EB4` result. The three
`0x1C500`/`0x2F198` pairs are `(col,row,mode) = (6,2),(7,4),(8,6)` with `ecx=0x2000`.

### 6.2 `0x11BCC` — the timer exit (41 B) and `0x11D04` case 7

```
0x11bcc  xor ah, ah
0x11bce  mov [0x104b1b], ah                 ; DS_00104B19.byte2 = 0
0x11bd4  mov [0x104b15], ah                 ; DS_00104B15 = 0
0x11bda  mov ax, [0xf0a6c]
0x11be0  mov [0xf0a64], ax                  ; DS_000F0A64 = DS_000F0A6C
0x11be6  call 0x29d60
0x11beb  mov eax, 0x100 ; jmp 0x2c3fc
```

The dispatcher `0x11D04` (raw, `0x11D4A`–`0x11D69`):

```
0x11d4a  ax = word[0xf0a64]
0x11d50  cmp ax, 9 ; ja 0x11d70
0x11d56  and eax, 0xffff ; edx = eax*4
0x11d62  ax = word[0xf0a6a]
0x11d68  dec eax                            ; eax = (u16)timer - 1
0x11d69  jmp dword ptr cs:[edx + 0x11cdc]   ; 10-entry table, states 0..9
```

Case 7 (`0x11E67`):

```
0x11e67  mov word [0xf0a6a], ax             ; store timer-1
0x11e6d  and eax, 0xffff
0x11e72  cmp eax, 1
0x11e75  jge 0x11e8f                        ; new != 0 -> run arena
0x11e77  call 0x11bcc                       ; new == 0 -> exit
0x11e7c  call 0x10db0 ; 0x10e18 ; 0x2bf08 ; ret
0x11e8f  call 0x263f4
0x11e94  call 0x33f08
0x11e99  call 0x10db0 ; 0x10e18 ; 0x2bf08 ; ret
```

So the timer counts `900 → 1`; the frame with **pre** value `1` stores `0` and exits
(899 arena frames), then `0x11BCC` restores the state. `0x11BF8` is a second, similar
handler (decrement, exit below 2, else `0x263F4`+`0x33F08`) that has no direct
caller or jump-table reference in the fixed image; it is recorded as a dead/duplicate
and not wired (§7.14).

### 6.3 `0x186D0` — the slot position latch

`0x186D0(side)` computes the slot pointer `edx = 0x1077B0 + side*0x94` and, when
`byte[edx+0x42]` has bit 3 set, copies the fighter record's `+0x18` to
`slot+0x2C` (`0x1077DC`) and `+0x1C` to `slot+0x30` (`0x1077E0`); otherwise it calls
`0x18540` first. It reads `DS_001077DC`/`DS_001077E0` (the slot copies) — not
`DS_001077B0`/`B4` — which is why the plan's Task 2 test target is wrong. The tail
reads `DS_00100AF0[side]` and `0x18350`, and returns `rec+0x18 - anim` (raw
`0x18780 sub eax,edi`). It is called from the `game_frame` tail at `0x25438` etc.

### 6.4 `0x2A690` — pset/screen sync (called from the tail)

`0x2A690(rec)` computes `DAT_00105BDC` (screen x) from `rec+0x18` and
`DAT_000F0AF0` (+0x2A00 offset), writes the actor entry's `+4`/`+8`, and sets
`DAT_00105BE0` (screen y) from `DS_000F0AEC` and the record's z. It is the pset-sync
path already partly mirrored in `port/src/game/actors.c`. It is called for each live
player from the `game_frame` tail (`0x25443`, `0x2552F`).

### 6.5 `0x3BB90` — cycle 1's combat skip

`0x3BB90` (221 B, 2 callers, 3 callees) is the collision entry; it is the `game_frame`
tail's first call and is explicitly deferred to cycle 2. It reads
`DS_000BEEF8[fighter*4]` (hit radius, halved when `+0x54 == 2`) and calls `0x4FB20`.
Task 5 leaves it as a `/* PORT: */` skip; cycle 2 owns it.

---

## 7. What could not be determined (named gaps)

1. **`0x17FA0`'s sprite-origin subtraction (`0x16308`).** The final `*arg2`/`*arg3`
   are reduced by `word[sprite]`, `*(u32*)(sprite+2)>>16`, `*(u32*)(sprite+4)>>16`,
   where `sprite = res_resolve(0xA8B30[actor.word0 & 0x7FFF])` (`0x16308`). The
   `0xA8B30` handles (`0x1060218` index 2, `0xF8045DC` index 31, `0x1D8080C8` index
   59, …) require the runtime page table, which the port does not model for this
   resource class (the same class as the previous cycle's §7.1). **Consequence:** the
   pre-subtraction projection (`(x+0x20)>>6`, §1.3) is unit-testable; the final
   stored globals are not until the resource reader exists. Evidence: `0x16308`
   body, `0x18055`–`0x181C2`, table at `0xA8B30`.
2. **`0x16AFC` (601 B) and `0x164F4` (547 B).** Their boolean results copy/zero
   `0x100AC0`–`0x100ACB` and set `*arg5`. Both read resource/page state; not decoded.
   Evidence: `0x180C9`, `0x18108`; bodies at `0x16AFC`, `0x164F4`.
3. **`0xA8B30` handle → asset mapping.** Pinned only as the `res_resolve` index/offset
   decoding (index = `handle >> 23`, offset = `handle & 0x7FFFFF`); the runtime page
   table is not statically readable.
4. **`0x49C78`'s entry bodies.** The pass structure, list root `0x10884C`, state
   switch `+0x1E`, callee set and the 8 RNG sites are pinned (§4); the internal
   effect/entry semantics (what each case draws or spawns, what `0x4B69C`/`0x4AC38`/
   `0x4987C`/`0x4A634` do) are not. It is not load-bearing for fighter motion but is
   load-bearing for pixels and for the RNG stream. §5.9 explains the Task 7 impact.
5. **`0x12E3C` (mode 1).** The raw body is transcribed in §2.6 but its exact
   min/midpoint selection, the `0x18714` calls and `0x12C7C`'s commit need the
   `0x18714`/`0x12C7C` bodies transcribed to be unit-pinned; the `0x13290`/`0x1333C`
   modes (2/3) are already pinned by the previous record and are the ones the demo
   uses when it settles, so this is lower risk.
6. **`0x1958C`/`0x19068` interiors.** Call order, gates, globals and the single RNG
   site are pinned (§3.5); the hit-stun/`0x1922C`/`0x193B0`/`0x3CF38` semantics are
   not. `0x1958C` calls `0x33950`/`0x19020`/`0x3AFC4`/`0x18950`; `0x19068` calls
   `0x3C570`/`0x1922C`/`0x3CF38`.
7. **`0x3C88C` (730 B) and its draw helpers.** Called 64×/frame from `0x3CB68`; 4
   callees (`0x3C600`, `0x3C6A8`, `0x3C758`, `0x3C800`). Not decoded.
8. **`0x35658` (478 B) interior.** The call spine (`0x35658 → 0x34B6C → 0x1A978 →
   0x3B134`) is pinned and load-bearing (§3.7); the `0x35813` call to `0x2A1FC`
   is `actor_sync` and is wired. `0x33C78`, `0x34038`, `0x38D24`, `0x3531C`,
   `0x354F0`, `0x186C4` are not decoded, and the health-bar drawing is cycle 2.
9. **`0x33F08` health sprite table.** The `rec[9]`-indexed table and `0x2A408` are
   not decoded; the per-character constant selection is pinned (§3.8).
10. **`0x34B6C` (541 B) interior.** Retired by Task 8's re-scope: the `0x36E2C`
    gate, the `0x34B97` position branch, the 22-entry `0x34B14` table and the
    `0x349C8` default handler are now pinned (§11). The remaining gaps are the
    `0x349C8` +0x42 bit 6/7 arms (`0x37178`, `0x37D18`), the `0x36638` +0x54 == 5
    arm (`0x366FB -> 0x38154`), the `0x34BE8` in-range arm (`0x36F10`, proven
    unreachable — its own `0x36FD4` write is bit 0x20, and no writer sets slot
    +0x42 bit 0x10), and the handlers for the states whose transition needs the
    unported hit-detection chain `0x3CF38` (`0x35D7C`, case 3).
11. **`0x1A978`'s helper chain.** `0x1A6AC`, `0x1A640`, `0x1A8F4`, `0x1A734` are not
    decoded; the call into `0x3B134` and the timer/stance bytes are pinned (§5.6).
12. **`0x1AB5C` (302 B), `0x1922C`, `0x18B44`, `0x3B938`, `0x39278`, `0x3AD98`,
    `0x3B080`, `0x3A95C`, `0x188DC`, `0x39834`, `0x235C4`, `0x36D20`, `0x2BD44`.**
    The think driver's branch targets are listed with their call addresses; their
    interiors are gaps.
13. **`0x1D890` HUD spawn `DL` preservation — retracted; not a gap.** The raw
    `0x1D890` preserves `DL` across its body (`push edx` … `pop edx`), so the
    `DS_00104B1B` write at `0x11B23` stores the caller's constant; the port
    depends on exactly that (`port/src/game/flow.c:805-806`). Task 1's ruling
    already recorded it, and the earlier "unverified" line is withdrawn.
    Evidence: `0x1D890`'s `push/pop edx`, `0x11B1E`, `0x11B23`.
14. **`0x11BF8`** is a second timer handler with no direct caller or jump-table
    reference in the fixed image; recorded as dead/duplicate, not wired.
15. **`0x17FA0`'s `0x1A570` interaction.** `0x1A570(side)` returns
    `(word[actor] & 0x8000) == 0`; the branch selecting between the two `local_A`
    forms is pinned, but the *meaning* of actor bit 15 (facing/airborne) is inferred,
    not proven.
16. **`0x3B298`'s input scan and `0x3BDDC`'s continuation.** `0x3B298`'s entry
    (the `slot[self]+0x86 = slot[other]+0x84` copy at `0x3B2D6`, the `anim[2]+2`
    bit 0/1 gate at `0x3B2E8`–`0x3B30C`) is pinned and anchored (§8.18); the scan it
    then runs (`0x1AB5C` at `0x3B315`, `0x46460` at `0x3B326`) and the record `+0x43`
    bit `0x20`/`0x10` writes on `ctx[3]` (`0x3B405`/`0x3B40D` for `0x20`,
    `0x3B433`/`0x3B43B` for `0x10`, where `ctx[3] = &slot[side]`) are a gap because
    `0x1AB5C`/`0x46460` are not decoded (also §7.12). `0x3BDDC`'s entry clears are
    anchored (§8.17), but its continuation (`0x4649C` at `0x3BE7F` selecting
    `0xBEF28`/`0xBEF64` and `0x3C480` at `0x3BF05` reading `0xC8B30[char]`) is a gap.
    Evidence: `0x3B2CE`–`0x3B44A`, `0x3BE43`–`0x3BF64`.

---

## 8. Unit-test values per helper

Every anchor below is expressible as "set these `mem[]` globals / record fields, call
this function, read these globals", so a test that cannot call `game_init()` can use
them. Functions whose values cannot be pinned are in §7 instead. `DSD`/`DSW`/`DSB`
are the port's typed `mem[]` accessors.

### 8.1 `0x17FA0` — `camera_project(side)` (pre-subtraction stage)

Input: `side = 0`; `DSD(DS_001077B0) = P` (a `mem[]` offset); `DSW(P+0x56) = I` (an
actor index); `DSD(DS_001014EC + I*0x20 + 4) = actor4`;
`DSD(DS_001014EC + I*0x20 + 8) = actor8`.
Expected: `DSD(DS_00100B08) == ((actor4 + 0x20) >> 6) - <sprite local_20>`,
`DSD(DS_00100B00) == ((actor8 + 0x20) >> 6) - <sprite local_24>`. **The subtraction
is a gap (§7.1); a test may assert the four §1.3 pairs against the
`(x+0x20)>>6` stage**, or inject `0x16308`'s return in a port seam. Assert the byte
outputs `DSB(DS_00100B62) == ((DSW(P+0x28) & 0x4000) != 0)` and `DSB(DS_00100B60) == 0`
before `0x16AFC`/`0x164F4`, and that `DSD(DS_00100AF0) == (DSW(actor0) & 0x7FFF) -
0x17EEC(0)`.

Worked stage pairs (from §1.3), with `actor4`/`actor8` seeded and the sprite seam
forced to zero:

| # | `actor4` | `actor8` | `DSD(0x100B08)` | `DSD(0x100B00)` |
|---|---|---|---|---|
| 1 | `0x1000` | `0x2000` | `0x40` | `0x80` |
| 2 | `0x3FF` | `0x40` | `0x10` | `0x1` |
| 3 | `0xFFFFFFFF` | `0xFFFFFFC0` | `0` | `0xFFFFFFFF` |
| 4 | `0x40` | `0x80` | `0x1` | `0x2` |

`0x17EEC` anchors (pure): `DSB(0x10782A + side*0x94) = 1` → returns `0x12A2`;
`= 3` → `0x16B5`; `= 6` → `0x32D7`; `= 0` and `= 7` → `0x0EE4`.

### 8.2 `0x12D48` — `camera_dispatch()`

Input A: `DSB(DS_000F0AFE) = 4`, `DSD(DS_000F0AF0) = 0x7000`.
Expected: no mode helper runs; `DSD(DS_000F0AF0) == 0x5D00` (clamp).
Input B: `DSB(DS_000F0AFE) = 4`, `DSD(DS_000F0AF0) = 0xFFFFA000` (`-0x6000`).
Expected: `DSD(DS_000F0AF0) == 0xFFFFA300` (`-0x5D00`).
Input C: `DSB(DS_000F0AFE) = 2`, players seeded (see 8.6).
Expected: mode-2 `0x13290` ran.

### 8.3 `0x12CD4` — y-stepper

Input A: `DSD(DS_000F0AEC) = 0`, argument `0x250`, `DSD(DS_000F0AF0) = 0`,
`DSW(DS_000F0AF4) = 0`. Expected: `DSD(DS_000F0AEC) == 0x100` (diff `0x250 > 0x100`,
step `+0x100`).
Input B: `DSD(DS_000F0AEC) = 0x100`, argument `0x150`, `DSW(DS_000F0AF4) = 0`.
Expected: `DSD(DS_000F0AEC) == 0x150` (`|diff| <= 0x100`).
Input C: `DSD(DS_000F0AEC) = 0x200`, argument `0x50`, `DSW(DS_000F0AF4) = 0`.
Expected: `DSD(DS_000F0AEC) == 0x100`.
Input D (the shake term): `DSD(DS_000F0AEC) = 0x200`, argument `0x50`,
`DSW(DS_000F0AF4) = 0x0020`. Expected: `DSD(DS_000F0AEC) == 0x120`
(`0x100 + (s16)0x0020`). A test that seeds `DSW(DS_000F0AF0+2)` instead does **not**
catch a wrong term, because `0xF0AF4` is BSS-zero in the demo; seed `DS_000F0AF4`.

### 8.4 `0x12DF0` — mode 0

Input: `DSB(DS_000F0AFF) = 0`; `DSD(DS_001077A8) = T`; `DSD(T+0x34) = 0x4000`;
`DSD(DS_000F0AF0) = 0`.
Expected: `|diff| = 0x4000 >= 0x1800` → `DSD(DS_000F0AF0) == 0x2800`.

### 8.5 `0x1282C` — gate

Input: `DSW(DS_000EF6DC) = 1` (so `& 0x3F != 0`).
Expected: no actor spawns, `DSD(DS_000EF6DC)` unchanged, and the RNG counter unchanged
(the function returned at `0x12843` before `0x1284E`).
Input B: `DSW(DS_000EF6DC) = 0x40`, `rng` seeded so `rng(7) & 3 == 0`.
Expected: three RNG advances and one actor spawned from `0xBB254`.

### 8.6 `0x1317C`, `0x13290`, `0x1333C`, `0x1324C`

Carry over verbatim from `2026-09-20-frontend-chain-derivations.md` §8.6–§8.8:
`0x1317C` input A `DSD(DS_001078F2)=0`, `DSW(DS_00104AFC)=0`,
`DSD(DS_000F0AEC)=0x250` → `0x150`; input B `DSD(DS_001078F2)=0x18000000` → `37`.
`0x13290` input A → `DSD(DS_000F0AF0)==0x40`; input B → `==0x2000` and
`DSB(DS_000F0AFE)==4`; input C `0x5D00` → settled `4`. `0x1333C` input A → `0x40`;
input B → `DSB(DS_000F0AFE)==4` without the `DS_001078FE` gate. `0x1324C` inputs A/B
as recorded there; note it is dormant and the `DS_00104AE8` sentinel must be
pre-seeded by the test.

### 8.7 `0x12DA8`

Input: `DSB(DS_000F0AFE) = 2`; `DSD(DS_001077E0) = 0x12345678`;
`DSD(DS_00107874) = 0x9ABCDEF0` (negative); `DSD(DS_000F0AEC) = 0`;
`DSW(DS_00104AFC) = 0`. Expected: `DSW(DS_001078F4) == 0x5678` — `0x9ABCDEF0` is
signed-negative, so the **signed max** of `0x12345678`/`0x9ABCDEF0` is `0x12345678`,
whose low word is `0x5678`.
Input B: `DSB(DS_000F0AFE) = 0`; `DSB(DS_000F0AFF) = 1`;
`DSW(0x1077E0 + 0x94) = 0x2222` (i.e. slot 1 `+0x30`).
Expected: `DSW(DS_001078F4) == 0x2222`.
Input C (max, not min): `DSB(DS_000F0AFE) = 2`; `DSD(DS_001077E0) = 0x00001000`;
`DSD(DS_00107874) = 0x00003000`. Expected: `DSW(DS_001078F4) == 0x3000` (signed max;
a min implementation returns `0x1000` and fails).

### 8.8 `0x263F4` — the arena frame

Input: seed `DSD(DS_001077E4) = 0x1111`, `DSD(DS_0010787C) = 0x2222` sentinels;
`DSD(DS_001077E4) = 0x3333`, `DSD(DS_00107878) = 0x4444`.
Expected: `DSD(DS_001077E8) == 0x3333` and `DSD(DS_0010787C) == 0x4444` (the latches
take the pre-frame values). The six `0x17FA0` calls, both `0x16D58`, `0x17580`,
`0x1958C`, `0x19068`, `0x1975C`, `0x3CB68`, two `0x35658`, `0x49C78`, `0x1282C`,
`0x12DA8` run in that order (a call-order trace seam is the honest assertion for the
gap functions).

### 8.9 `0x3C5CC`

Input: `DSD(0x107EE0) = 0xDEADBEEF`, `DSD(0x107D50) = 0xDEADBEEF`,
`DSD(0x107D54) = 0xDEADBEEF`. Call. Expected: all three `== 0`.

### 8.10 `0x16D58`

Input: `ax = 0`, `dx = 3`. Expected:
`DSD(DS_00100A70) == 3*0x400 + 0xCC300 == 0xCCF00`;
`DSD(DS_00100A98) == 3*0x3E8 + 0xC9BF0 == 0xCA7A8`.
Input B: `ax = 5` (out of `[0,1]`). Expected: no writes.

### 8.11 `0x17580`

Input: `DSD(DS_00100B08) = 0x1000`, others 0; `DSD(DS_00107824..) = 0`;
`DSD(DS_000EF6DC)` etc. Expected: `DSD(DS_00100B08) == (0x1000*0xF3D + 0x800) >> 12
== 0xF3D` (since `0x1000*0xF3D = 0xF3D000`, `+0x800 = 0xF3D800`, `>>12 = 0xF3D`).
Input B: `DSD(DS_00100B00) = 0x1000` → `(0x1000*0xD56 + 0x800)>>12 = 0xD56`. The four
counter globals `0x107824`, `0x1078B8`, `0x107826`, `0x1078BA` decrement by 1 when
non-zero.

### 8.12 `0x186D0`

Input: `side = 0`, `DSD(DS_001077B0) = P`, `DSD(P+0x18) = 0xAA`, `DSD(P+0x1C) = 0xBB`,
`DSB(0x1077B0 + 0x42) = 0x08` (bit 3 set). Expected: `DSD(0x1077DC) == 0xAA`,
`DSD(0x1077E0) == 0xBB`.

### 8.13 `0x3B134` — the command mapper

The mapper's clean, anim-independent branch is the `+0x64 != 0xFF && +0x08 != 0`
path, which does not read `0x3AFC4`. To reach it both gates must pass:
`DSB(0x1077B0 + side*0x94 + 0x63) != 0` and `0x1AB10` true (seed the context's
fighter-state bytes `+0x54`/`+0x53`).

Input A: `side = 0`; `DSB(0x107813) = 1`;
`DSD(DS_001077B0) = P0`, `DSD(P0+0x2C+... )` — set the slot copies
`DSD(0x1077DC) = 0x3000` (self), `DSD(0x107844 + 0x2C = 0x107870) = 0x1000` (other);
`DSB(0x107844 + 0x64 = 0x1078A8) = 0x01`; `DSD(0x107844 + 0x08 = 0x10784C) = R`;
`DSW(R+0x34) = 0xFFFF` (`-1`). Expected: `DSW(DS_001088E0) == 0x6000`.
Input B: same but `DSW(R+0x34) = 0x0001`. Expected: `DSW(DS_001088E0) == 0x5000`.
Input C: same but `DSD(0x1077DC) = 0x1000`, `DSD(0x107870) = 0x3000` (self left of
other) and force the anim path (`DSB(0x1078A8) = 0xFF`). Expected: `0x2000` (or
`0x2000|0x4000`/`0x8000` per the `anim[2]+2` bits — §7.11).

Seed the sentinel `DSW(DS_001088E0) = 0xFFFF` first so each assertion depends on the
mapper, and assert A and B together so a swapped branch fails.

**Reaching the mapper.** A test that calls `fighter_think(side)` (the port's
`0x1975C`) must seed every gate on the path, or it will never reach `0x3B134`:
`DSD(DS_00100AD0 + side*4) > 2` (`0x19778`), `0x3C59C() == 0` and
`*(s8*)(rec + 0x64) != -1` (`0x3B47F`/`0x3B4A5`), and then in `0x3B134` the slot
gate `DSB(0x1077B0 + side*0x94 + 0x63) != 0` (`0x3B168`) and `0x1AB10(...) != 0`
(`0x3B17A`). The `rng(100)` gate at `0x3B1AD` can be bypassed by passing the
override byte non-zero (`cmp byte [esp+0x24],0`), so a direct `0x3B134` test with a
non-zero override is deterministic; a `fighter_think` test must additionally seed the
RNG or set the override path. If the port cannot inject the override, the mapper's
command-word assertions are only reachable through the seeded state and the RNG gate
becomes part of the fixture.

### 8.14 `0x11A8C` — state 6

Input: `DSB(DS_00104B15) = 0`; `DSW(DS_001082CC) = 0`; `DSW(DS_000F0A72) = 5`;
`DSW(DS_000F0A6C) = 0`; `DSB(DS_000F0A6F) = 0xFF`; `DSB(DS_00104529) = 0`;
`DSB(DS_00104B1B) = 0`. Call `game_state_step()` with `DSW(DS_000F0A64) == 6`.
Expected: two RNG advances (draw1 = `rng(7)`, draw2 = `rng(6)`); P0 char = draw1,
P1 char = `(draw1+draw2)%7`; `DSW(DS_00104AFC) == (u16)draw1`;
`DSB(DS_00104B15) == 1`; `DSW(DS_001082CC) == 3`; `DSW(DS_000F0A64) == 7`;
`DSW(DS_000F0A6A) == 900`; `DSW(DS_000F0A6C) == 5`; `DSB(DS_000F0A6F) == 0`;
`DSB(DS_00104B1B) == 1`. With `DSB(DS_00104529) == 2`, the three `0x1C500`/`0x2F198`
pairs do not run.

### 8.15 `0x11BCC` and case 7

Input: `DSW(DS_000F0A6A) = 1`; `DSW(DS_000F0A6C) = 4`; `DSB(DS_00104B15) = 1`;
`DSB(DS_00104B1B) = 1`. Call the dispatcher with `DSW(DS_000F0A64) = 7`.
Expected: `DSW(DS_000F0A6A) == 0`; `DSW(DS_000F0A64) == 4`; `DSB(DS_00104B15) == 0`;
`DSB(DS_00104B1B) == 0`; `0x263F4` and `0x33F08` did **not** run.
Input B: `DSW(DS_000F0A6A) = 2`. Expected: `DSW(DS_000F0A6A) == 1`;
`0x263F4` and `0x33F08` ran; state still 7.

### 8.16 `0x33F08`

Input: `DSD(DS_001077A8) = R` (non-zero); `DSD(R+0x7A) = 3`;
`DSW(actor + 0) = 0x8000 | X` so `(actor & 0x7FFF) = X`; `DSB(R+0x41) = 0`.
Expected: `s = X - 0x16B5`; if `0 <= s < 0x4B1`,
`DSD(DSD(R[1]) + 8) == DSW(DSD(R[9]) + s*2)`, else `0x1E1`. This is a gap beyond the
table selection (§3.8, §7.9).

### 8.17 `0x3BDDC` — the command consumer's entry clears

Raw `0x3BDDC`–`0x3BF64`: `0x33950` fills the context; `slot[side].+0x40` bit 7
(`0x3BDF3 test byte [eax+0x40],0x80`) must be clear and
`DSW(0x1088E0 + side*2)` bit 15 (`0x3BE09 and ah,0x80`) must be set; then
`rec = DSD(0x1077B0 + side*0x94)` has `+0x34`/`+0x43`/`+0x42` cleared
(`0x3BE2F`/`0x3BE35`/`0x3BE39`) and `slot[side].+0x5F` set to `0xFF` (`0x3BE43`).

Input A: `side = 0`; `DSD(DS_001077B0) = P`; `DSB(DS_001077B0 + 0x40) = 0`;
`DSW(DS_001088E0) = 0x8000`; seed `DSW(P + 0x34) = 0xAA`, `DSB(P + 0x43) = 0xAA`,
`DSB(P + 0x42) = 0xAA`, `DSB(DS_001077B0 + 0x5F) = 0xAA`. Call `0x3BDDC(0)`.
Expected: `DSW(P + 0x34) == 0`, `DSB(P + 0x43) == 0`, `DSB(P + 0x42) == 0`,
`DSB(DS_001077B0 + 0x5F) == 0xFF`.
Input B (slot gate): `DSB(DS_001077B0 + 0x40) = 0x80` → none of the four change.
Input C (command bit): `DSW(DS_001088E0) = 0x0000` → none of the four change.
Input D (continuation): as A plus `DSB(DS_00107803) = 1` (so `0x3BE5D` jumps to the
table selection). Expected `DSB(DS_00107802) == 3`, `DSB(DS_00107803) == 4`,
`DSB(DS_00107804) == 2`, `DSB(DS_001078F8) == 1`, `DSW(DS_001077FE) == 0` (command
`0x8000` has neither bit `0x1000` nor `0x2000`), and `DSD(DS_00107D40)` one of
`0xBEF28 + char*6` / `0xBEF64 + char*6` with `char = DSB(DS_0010782A)` (`0x4649C`
selects; gap §7.16). A command with bit `0x1000` sets `DSW(DS_001077FE) == 1`; bit
`0x2000` sets `0xFFFF` (`0x3BF22`–`0x3BF51`).

Array strides (raw): `DS_00107802`/`03`/`04` and `DS_001077FE` are slot fields
(`0x107802`/`0x107803`/`0x107804`/`0x1077FE` at `side 0`, `+ side*0x94`);
`DS_001078F8` is byte-stride (`[esi + 0x1078F8]`, `esi = side`); `DS_00107D40` is
dword stride **4** — the store is `0x3BED4 mov dword ptr [ebx + 0x107D40], eax` with
`ebx` set at `0x3BE97 lea ebx, [esi*4]`, so the address is `0x107D40 + side*4`. This
is distinct from the `0x94`-stride slot fields: those are written only after `ebx` is
reloaded at `0x3BEE8 mov ebx, 0x1077b0` and `0x3BEF0 add ebx, eax` (`eax = side*0x94`),
which happens *after* the `0x107D40` store.

### 8.18 `0x3B298` — the command-state dispatch (entry copy)

Raw `0x3B2B2`–`0x3B2DD`: `0x33A10` fills the context, so `[esp+8] = ctx[2] =
&slot[1-side]` and `[esp+0xc] = ctx[3] = &slot[side]`; then
`word[&slot[side] + 0x86] = word[&slot[1-side] + 0x84]` runs unconditionally (the
`0x3B134` call at `0x3B2C9` returns to `0x3B2CE`).

Input: `side = 0` (EAX), second arg (EDX) `= 0`;
`DSW(DS_00107844 + 0x84 = 0x1078C8) = 0x1234`;
`DSW(DS_001077B0 + 0x86 = 0x107836) = 0x0000`.
Expected: `DSW(0x107836) == 0x1234` and `DSW(0x1078C8) == 0x1234` (source unchanged).
The `anim[2]+2` bit gate (`0x3B2E8`–`0x3B30C`) then decides whether the input scan
runs; the scan and the `ctx[3]+0x43` bit writes are gap §7.16.

---

## 10. The fighter spawn chain (Task 6)

Task 5 left the demo advancing into state 7 with **no live fighters**: the only
writer of `DS_001077A8` (`0x33CA0`) lives in `0x33C78`, reached through the spawn
entry `0x33EB4` (state 6) and the HUD respawn `0x357D6`. This section derives the
whole state-6 spawn path and answers the Task 6 resource question (§10.9). It
supersedes the §7.8 one-line treatment of `0x33C78`.

Addresses added by this section:

| function | VA | decompiler | size | callers | reached from |
|---|---|---|---|---|---|
| `0x33EB4` | `0x33EB4` | `:21174` | 53 | 6 | state 6 `0x11AD9`/`0x11B08` |
| `0x33C78` | `0x33C78` | `:21069` | 571 | 9 | `0x33EB4`, HUD `0x357D6` |
| `0x29BC8` | `0x29BC8` | `:17009` | 32 | 6 | spawn, think, HUD |
| `0x2A17C` | `0x2A17C` | `:15728` | 127 | 22 | `0x29BC8` and the actor syncs |
| `0x1CEBC` | `0x1CEBC` | `:1120` | 24 | 1 | spawn tail |
| `0x494A8` | `0x494A8` | `:31418` | 515 | 1 | spawn (gate `0x104B14 == 0`) |
| `0x1D890` | `0x1D890` | `:9235` | 375 | 2 | state 6 `0x11B1E` |
| `0x20DF4` | `0x20DF4` | `:11482` | 155 | 4 | state 6 `0x11AC4` |

### 10.1 `0x33EB4` — the spawn entry (53 B)

```
0x33eb4  push ebx; push ecx; push edx
0x33eb7  test eax, eax ; jne 0x33ec2
0x33ebb  mov ecx, 0x4000 ; jmp 0x33ec4
0x33ec2  xor ecx, ecx
0x33ec4  mov edx, [eax*2 + 0xbda38]      ; dword load
0x33ecb  sar edx, 0x10                   ; arithmetic: the high word, sign-extended
0x33ece  and ecx, 0xffff
0x33ed4  push ecx                        ; the callee's stack arg (0x33C78 ret 4)
0x33ed5  xor ecx, ecx
0x33ed7  xor ebx, ebx
0x33ed9  mov cx, [0xbd898]               ; ecx = 0x400 (zero-extended)
0x33ee0  call 0x33c78
0x33ee5  pop edx                          ; EDX = the caller's original EDX (push/pop)
0x33ee6  pop ecx
0x33ee7  pop ebx
0x33ee8  ret
```

Arguments: `EAX = side`; `EDX = (s32)(u32)dword[0xBDA38 + side*2] >> 16`; `ECX =
word[0xBD898] = 0x400`; the stack arg `= 0x4000` for side 0, `0` for side 1.
`DS_000BDA38` is the per-side initial-x table: side 0 reads the dword
`0xE8001001` → `sar 16 = 0xFFFFE800` (`-0x1800`), side 1 reads the dword
`0x1800E800` → `0x1800` (bytes `01 10 00 e8 00 18 18 00` at `0xBDA38`).
**`0x33EB4`
preserves EDX** — it pushes it at `0x33EB6` and pops it after the `ret 4` — so
the value state 6 stores in `DS_001082C8` is the caller's `7`, not a callee
return (§10.8).

### 10.2 `0x33C78` — the spawn core (571 B)

Entry: `EAX = side`, `EDX = the initial x` (§10.1), `ECX = 0x400`, stack arg `a5 =
0x4000/0`. `0x33C78` ends `add esp,8; pop edi; pop esi; ret 4`, i.e. it consumes
its one stack argument.

```
0x33c78  push esi; push edi; sub esp, 8
0x33c7d  mov [esp+4], eax                ; local = side
0x33c81  mov [esp], ecx                  ; local = 0x400
0x33c84  lea esi,[eax*8]; add esi,eax; shl esi,2; add esi,eax; shl esi,2
0x33c95  add esi, 0x1077b0               ; esi = slot = 0x1077B0 + side*0x94
0x33c9b  shl eax, 2                      ; eax = side*4
0x33c9e  mov edi, esi
0x33ca0  mov [eax + 0x1077a8], esi       ; DW: DS_001077A8[side] = slot (LIVENESS)
0x33ca6  test esi, esi ; je 0x33eab      ; never taken (slot is a fixed address)
0x33cae  mov ecx, [esp+4]
0x33cb2  mov cl, [ecx + 0x10816a]        ; character = DS_0010816A[side]
0x33cb8  mov [esi + 0x7a], cl            ; slot+0x7A = character
0x33cbb  xor ecx, ecx
0x33cbd  mov cx, [esp+0x14]              ; a5 (0x4000/0), the stack argument
0x33cc2  push ecx                        ; the 0x2AE14 stack arg
0x33cc3  xor ecx, ecx
0x33cc5  mov cl, [esi + 0x7a]            ; ecx = character
0x33cc8  mov eax, [eax + ecx*8 + 0xbb7e0]; fighter descriptor, idx = char*2+side
0x33ccf  mov ecx, [esp+4]                ; ecx = 0x400 (the saved local)
0x33cd3  call 0x2ae14                    ; actor_spawn(desc, edx, a3=0x400, a4=0, a5)
0x33cd8  mov [esi], eax                  ; slot+0x00 = fighter record (LIVENESS)
0x33cda  mov ah, [0x1078fa]
0x33ce0  mov byte [esi+0x52], 0
0x33ce4  inc ah
0x33ce6  mov byte [esi+0x53], 0
0x33cea  mov [0x1078fa], ah              ; DS_001078FA += 1
0x33cf0  xor eax, eax
0x33cf2  mov byte [esi+0x5f], 0xff       ; slot+0x5F = 0xFF
0x33cf6  mov al, [0x1078fa]
0x33cfb  mov byte [esi+0x64], 0xff       ; slot+0x64 = 0xFF
0x33cff  cmp eax, 2 ; jne 0x33d0d
0x33d04  mov byte [0xf0afe], 1           ; camera mode 1 when both are spawned
0x33d0b  jmp 0x33d1e
0x33d0d  xor dl, dl
0x33d0f  mov al, [esp+4]                 ; side
0x33d13  mov [0xf0afe], dl               ; camera mode 0
0x33d19  mov [0xf0aff], al               ; DS_000F0AFF = side
0x33d1e  mov eax, [esi]                  ; the fighter record
0x33d20  mov ax, [eax+0x56]              ; its actor index
0x33d24  or ah, 4                        ; | 0x400
0x33d27  and eax, 0xffff
0x33d2c  push eax                        ; a5 = index | 0x400 (0x2AE14 child arm)
0x33d2d  xor eax, eax
0x33d2f  xor ecx, ecx
0x33d31  mov al, [esi+0x7a]              ; character
0x33d34  xor ebx, ebx
0x33d36  xor edx, edx
0x33d38  mov eax, [eax*4 + 0xbb8d0]      ; secondary-actor descriptor [char]
0x33d3f  call 0x2ae14
0x33d44  mov edx, eax
0x33d46  mov [esi+4], eax                ; slot+0x04 = secondary actor
0x33d49  mov al, [0xbdb38]
0x33d4e  mov [edx+0x59], al              ; secondary+0x59 = DS_000BDB38 (0xFD)
0x33d51  xor eax, eax
0x33d53  mov al, [esi+0x7a]
0x33d56  mov eax, [eax*4 + 0xbda8c]      ; character health/anim table
0x33d5d  mov [esi+0x24], eax             ; slot+0x24 = DS_000BDA8C[char]
0x33d60  mov eax, [esi]
0x33d62  mov [eax+0x14], esi             ; rec+0x14 = slot
0x33d65  mov eax, [esi]
0x33d67  mov dl, [esp+4]
0x33d6b  mov [eax+0x51], dl              ; rec+0x51 = side
0x33d6e  mov eax, [esi]
0x33d70  mov byte [eax+0x4c], 0
0x33d74  mov eax, 0x1e
0x33d79  mov dl, al
0x33d7b  mov eax, [esi]
0x33d7d  mov [eax+0x4d], dl              ; rec+0x4D = 0x1E
0x33d80  mov ebx, [0xc9520]              ; 0xC350
0x33d86  mov eax, [esi+0x3c]
0x33d89  xor edx, edx
0x33d8b  div ebx                         ; unsigned: slot+0x3C / 0xC350
0x33d8d  mov byte [esi+0x55], 0xff
0x33d91  mov dword [esi+0x40], 0x80008000
0x33d98  mov byte [esi+0x54], 0
0x33d9c  mov byte [esi+0x5d], 0
0x33da0  mov dword [esi+8], 0            ; slot+0x08 = 0
0x33da7  mov word [esi+0x6a], 0
0x33dad  mov word [esi+0x6c], 0
0x33db3  mov byte [esi+0x7b], 0
0x33db7  xor edx, edx
0x33db9  mov byte [esi+0x7c], 0
0x33dbd  mov dl, [0x1088cb]
0x33dc3  mov word [esi+0x88], 0
0x33dcc  cmp eax, edx ; jl 0x33dd2
0x33dd0  mov eax, edx                    ; eax = min(quotient, DS_001088CB)
0x33dd2  mov ah, [0x1088cc]              ; AH, not AL
0x33dd8  add ah, al
0x33dda  mov [esi+0x81], ah              ; slot+0x81 = DS_001088CC + low(min)
0x33de0  mov eax, [esp+4]                ; side
0x33de4  mov dword [esi+0x20], 0xffffffff
0x33deb  call 0x186d0                    ; fighter_slot_latch(side)
0x33df0  mov dword [esi+0xc], 0
0x33df7  mov dword [esi+0x10], 0
0x33dfe  mov dword [esi+0x14], 0
0x33e05  xor edx, edx
0x33e07  mov dword [esi+0x18], 0
0x33e0e  mov eax, [esp+4]                ; side
0x33e12  mov ebx, [esi]                  ; fighter record
0x33e14  mov dl, [esi+0x7a]              ; character
0x33e17  mov dword [esi+0x1c], 0
0x33e1e  call 0x29bc8                    ; the character palette (§10.3)
0x33e23  mov bl, [esi+0x41]
0x33e26  xor ecx, ecx
0x33e28  and bl, 0x7f
0x33e2b  mov eax, [esp+4]
0x33e2f  mov [esi+0x41], bl
0x33e32  mov bh, [0x104b14]
0x33e38  mov [eax*4 + 0x1077a0], ecx     ; DS_001077A0[side] = 0
0x33e3f  test bh, bh ; jne 0x33e48
0x33e43  call 0x494a8                    ; the dust entry (§10.5)
0x33e48  call 0x1cebc                    ; the audio gate (§10.4)
0x33e4d  test al, al ; je 0x33eab
0x33e51  ... per-character sound handle table 0xBDB1C[char] ...
0x33e9c  call 0x1b544                    ; res_resolve(handle), return ignored
0x33ea1  mov eax, 0x287b2f5
0x33ea6  call 0x1b544                    ; res_resolve(0x287B2F5), return ignored
0x33eab  add esp, 8; pop edi; pop esi; ret 4
```

The three stores that make a slot live are `DS_001077A8[side] = slot` (0x33CA0),
`slot+0x7A = character` (0x33CB8) and `slot+0x00 = fighter record` (0x33CD8).
They precede every unported call.

Descriptor selectors (data object): the fighter descriptor is
`dword[0xBB7E0 + (char*2 + side)*4]` — the table is interleaved `[char][side]`
(0xBB7E0 = char 0 side 0 = `0xBABB0`, 0xBB7E4 = char 0 side 1 = `0xBABC4`,
stride 0x14) — and the secondary actor is `dword[0xBB8D0 + char*4]` (0xBB8D0
char 0 = `0xBB010`). Both descriptors carry `+0x04 = 0x00`, the render type that
`0x2AE14` sends down the visible path, so `0x2AE14` returns a live record without
any resource reader (§10.9). The `desc+0x10` palette handles are `0x1BFD58`
(fighter) and `0x0105FEB0` (secondary).

`a3 = 0x400` reaches `0x2AE14` as the record's layer word (`DSW(rec+0x32) =
0x400` on the non-child arm, because the descriptor's `+0x08` flag word is
`0x0001` so the `0x20` bit is clear); `a4 = 0` is the record's world y.

### 10.3 `0x29BC8` (32 B) and `0x2A17C` (127 B) — the character palette

```
0x29bc8  push ecx
0x29bc9  mov ecx, eax                    ; ecx = side
0x29bcb  mov eax, ebx                    ; eax = record (0x33C78's EBX)
0x29bcd  mov ebx, [edx*4 + 0xa8a98]      ; ebx = DS_000A8A98[char] (a pointer)
0x29bd4  xor edx, edx
0x29bd6  mov dl, [ecx + 0x105b34]        ; DS_00105B34[side] (0 or 1)
0x29bdc  mov ebx, [ebx + edx*4]          ; the palette handle
0x29bdf  xor edx, edx
0x29be1  call 0x2a17c                    ; (rec, edx=0, ebx=handle)
0x29be6  pop ecx
0x29be7  ret
```

`0x2A17C` (pinned by the raw: `0x2A17E mov ecx,eax`; `0x2A192 mov eax,edx`)
writes `pset+2 = word | (rec+0x5F ? 0x800 : 0)` and, for a non-zero handle,
releases the existing `pset+0x18` entry through `0x33864` and sets it to
`palette_acquire(handle)`. A **zero** handle returns at `0x2A1AC` (`test ebx,ebx
/ je 0x2A1F8`) **leaving `pset+0x18` unchanged** — the store at `0x2A1F5` is
dead, reachable only from `0x2A1E6`'s `test ebx,ebx / je` inside the guarded
acquire path, where EBX is still the non-zero entry value. The spawn passes
`word = 0`, so the fighter's palette becomes `DS_000A8A98[char][DS_00105B34[side]]`.

### 10.4 `0x1CEBC` (24 B) and the tail

```
0x1cebc  cmp dword [0x1028c8], 0 ; je 0x1ced1
0x1cec5  cmp byte [0x1028db], 0 ; jne 0x1ced1
0x1cece  mov al, 1 ; ret
0x1ced1  xor al, al ; ret
```

`0x1CEBC` is `DS_001028C8 != 0 && DS_001028DB == 0` — the AIL sequence handle
test. When it returns non-zero the tail resolves `0xBDB1C[char]` and the fixed
`0x287B2F5` through `res_resolve` and discards both returns. The port keeps its
AIL handles outside `mem[]`, so `DS_001028C8` is 0 and the tail is skipped; it is
a named gap regardless (the handles are unported audio resources) and neither
return is read, so nothing is lost for the spawn.

### 10.5 `0x494A8` (515 B) — the dust entry (gap)

Reached only when `DS_00104B14 == 0`. `0x494A8(side)`:
`DS_00104AFA == 0x23` diverts to `0x4CF20`; otherwise it clears ten `0x1088xx`
bytes, computes `n = DSB(slot+0x81) ? 0x300 / DSB(slot+0x81) : 0x300`, and loops
`n` times: unlink `DS_001083C4` (`0x249D0`), insert it after the root `0x10884C`
(`0x249B0`), pick a descriptor from `0xC9524[0x49388(side)]` (`0x49388`), write
`0x29CDC(side, char)` into its `+0x10`, and call `0x2AE14` with
`edx = rec+0x18 - 0xC00 + rng(0x1800)` and `ecx = rng(0x300)` added to
`((s32)rec+0x30 >> 16) + 0x400`, then `0x496AC`. It draws **two RNG values per
iteration** and builds the dust entries the effects pass `0x49C78` renders. The
entry semantics are a gap (the same class as §7.4); it is **not** what makes a
slot live, because every liveness store precedes it.

### 10.6 `0x1D890` (375 B) — state 6's call is a 4-byte reset

`0x1D890(AL)`: for `side = 0,1` it always writes
`DSB(0x10780E + side*0x94) = 0`, `DSB(0x10290C + side) = 0`,
`DSB(0x10780A + side*0x94) = 0`, `DSB(0x10290E + side) = 0`; then, **only when
AL != 0**, it spawns the four HUD actors per side (`0x2AE14` from the `0xA76xx`
tables), calls `0x1D2F0`/`0x1D464`/`0x1D838`, and ORs `2` into `DS_00104AEC`.
State 6 calls it at `0x11B1E` with `EAX = 0` (`xor eax,eax` at `0x11B1A`; only DL
survives, which is why `0x11B23` stores the constant 1). **The state-6 call is
therefore the four-byte reset only** — no HUD actor is spawned and
`DS_00104AEC` is untouched. The AL != 0 arm is cycle 2's HUD and a named gap.

### 10.7 `0x20DF4` (155 B) — the fight reset (gap)

State 6 calls it at `0x11AC4` with `EAX = draw1, EDX = 1`. Body: clear
`DS_000F0A48`; `0x29B70`; clear `DS_00100B4C`, `DS_00104AE8`,
`DS_001088EC`, `DS_00104B15`; `0x2C390`, `0x12750`, `0x49300`, `0x28E98`,
`0x34978`, `0x2C074`; clear `DS_000F0AEC`/`DS_000F0AF0`; `DS_000F0AFA =
(u8)CL`, `DS_000F0AF8 = 0`; `0x12C70` (which only writes
`DS_000F0AFC = 0x400`); then, **iff EDX != 0 after `0x2C074`**, `0x2BAF4`
(`actors_reset`) + `0x38730` + `0x412A0`. The EDX branch value comes from
`0x2C074`'s return and is not statically pinned, and its six resets are unported,
so it is a named gap. It is the function that owns the pre-spawn actor-pool
reset; the spawn itself does not depend on it for liveness, so the unit test
establishes the pool directly (it calls `actors_reset`).

### 10.8 `DS_001082C8` — the EDX handoff

State 6's `0x11AD2 xor eax,eax; 0x11AD4 mov edx,7; 0x11AD9 call 0x33eb4;
0x11ADE mov eax,6; 0x11AE3 mov [0x1082c8],edx`. `0x33EB4` push/pops EDX
(§10.1) and its callee cleans only the stack argument (`0x33C78 ret 4`), so EDX
is still the caller's `7` after the call. **`DS_001082C8 = 7`**, a caller
constant, not a callee return. (Task 5 left it unwritten rather than guess it.)

### 10.9 The resource question (Task 6 Step 2) — answered

**The spawn populates the live-slot count and both slot records without the
paged-resource reader `0x2DBC4`/`0x2DB58`.** Evidence:

1. The liveness stores (`0x33CA0`, `0x33CB8`, `0x33CD8`) precede every call in
   `0x33C78` except the two `0x2AE14` spawns, whose descriptors are file-backed
   data with render type `0x00` (§10.2), so `0x2AE14` takes its visible path.
2. A direct-`E8` breadth-first walk of `0x33C78`'s call graph (227 functions,
   over `port/decomp/prage.calls.csv`) **does not reach `0x2DBC4` or `0x2DB58`**.
3. Those two addresses have **zero 4-byte data references** anywhere in the
   fixed-up image, so no indirect call table can reach them either.
4. The functions the brief flags — `0x494A8` (dust), `0x29BC8` (palette) and
   `0x1CEBC` (AIL gate) — reach only `0x249B0`/`0x249D0`/`0x49388`/`0x29CDC`/
   `0x2AE14`/`0x496AC`/`0x2A17C`/`0x33864`/`0x33754`, none of which is the
   paged reader.

The fallback (re-scope the cycle bound and move the spawn to cycle 2) is
therefore **not** needed. `0x494A8` and the `res_resolve` tail remain named gaps
for pixel/RNG fidelity, but they are not a liveness precondition.

### 10.10 Corrections against the plan/brief (§0.2 convention)

1. **`0x1D890` at `0x11B1E` is not a spawn.** The brief's files list calls it
   "the 375-byte HUD spawn"; state 6 passes `EAX = 0`, so the raw runs only the
   four-byte per-side reset (§10.6). The plan's "state 6's observables" list is
   otherwise correct.
2. **`DS_001082C8` is `7`.** §6.1 says it is "the `0x33EB4` result"; the raw
   makes that result the caller's EDX, i.e. the constant 7 (§10.8).
3. **`0x33F08`'s `rec[9]`-indexed table is `DS_000BDA8C[char]` for the spawn's
   `slot+0x24` write** (0x33D56/0x33D5D), which is the `+0x24` table §3.8 refers
   to; this pins where `slot+0x24` comes from.
4. **The fighter descriptor table `0xBB7E0` is `[char][side]`-interleaved**
   (index `char*2 + side`), not `[side][char]`; the brief's "`0x33EB4` (state 6's
   spawn entry)" carries no such claim, but the interleave is the kind of thing
   that silently picks the wrong record, so it is recorded.

### 10.11 Unit-test values

`DSD`/`DSW`/`DSB` are the port's typed accessors.

**`0x33EB4` / `0x33C78` (spawn).** Input: pool initialized (`actors_reset`),
`DS_001077B0`/`DS_00107844` valid, `DS_0010816A[0] = c0`, `DS_0010816A[1] = c1`,
`DS_001077A8[0/1]` seeded to values that differ from the result, `DS_001078FA`
seeded below 2. Expected: `DS_001077A8[0] == 0x1077B0`,
`DS_001077A8[1] == 0x107844`, `DSB(0x1077B0+0x7A) == c0`,
`DSB(0x107844+0x7A) == c1`, `DSD(0x1077B0) != 0`, `DSD(0x107844) != 0`,
`DS_001078FA == 2`, and `slot+0x00`'s record has `0x1077B0` at `rec+0x14` and
`side` at `rec+0x51`. With `c0`/`c1` picked through `0x41350`, the slot char is
`0xC835A[draw]`.

**`0x494A8`.** Gate: `DSB(0x104B14) == 0` runs it; `!= 0` skips. Input:
`DS_00104AFA != 0x23`; `slot+0x81 = n`. Expected: one `0x10884C` entry per
`n`, each with an actor, and `2n` RNG draws. Input B: `DS_00104AFA == 0x23` →
diverts to `0x4CF20` (no list, no RNG).

**`0x1D890`.** Input: `EAX = 0`. Expected: the four per-side bytes zeroed;
`DS_00104AEC` unchanged, no actor spawned. Input B: `EAX != 0` → the HUD arm
(`DS_00104AEC |= 2`), a named gap.

**`DS_001082C8`.** Input: state 6. Expected `7`.

**`0x2A17C`.** Input: `rec+0x56 = 0`; `pset+0x18` seeded to a sentinel;
`handle = 0`. Expected: `pset+2 = word | (rec+0x5F ? 0x800 : 0)` and
`pset+0x18` **unchanged** (the raw returns at `0x2A1AC`). Input B: a non-zero
handle → the existing entry is released and `pset+0x18 = palette_acquire(handle)`.

**Arena frame with live slots.** After the spawn, seed `DSD(0x1077B0) + 0x3C`
to a sentinel; a state-7 `game_frame` (the tail's `0x2A690`) writes
`rec+0x3C = pset+4`, so the sentinel must move. A skipped spawn leaves
`DS_001077A8` null and the record untouched.

---

## 11. The demo's CPU-AI command generator and the `+0x52` dispatch (Task 8)

This supersedes §7.10's one-line treatment. It is the delivery of the re-scoped
Task 8: the demo's command words come from `game_frame`'s `0x24C73` block, not
from the think chain, and the `+0x52` table only reacts to them.

### 11.1 `0x24C73` — the game_frame command block

`0x24C5C` after `0x4F644` (`input_state_update`). The gate is
`DS_00104B26 == 0 && DS_00104B1B != 0` (`0x24C73`/`0x24C7C`). `DS_00104B26` is
read at `0x24C73`/`0x24CBA`/`0x2A32B`/`0x2A214` and **written nowhere** in the
image (BSS 0); `DS_00104B1B` is set to 1 by state 6 (`0x11B23`). So the block
runs for the whole demo. Per side: if `slot+0x41 & 0x10` is clear call
`0x47208(side)`, else `cmd[side] = 0` (`0x24C96`); then `0x461DC()` (`0x24CB5`).

Writers of `DS_001088E0/E2`: this block, `0x4F644`, and `0x3B134` (reachable
only from the think chain, which is dead — §11.4).

### 11.2 `0x47208` — the CPU-AI move generator

`0x47208(side)`, 344 B. Body (addresses inline):

```
0x33950(ctx, side)                            ; ctx[0]=side, [1]=1-side, [2]=&slot[side], [3]=&slot[1-side]
0x469A8(param = ctx[1])                       ; classify the other slot into block[side]
if (slot[side]+0x63 == 0) return             ; 0x472AA the think gate
cmd[side] = 0                                 ; 0x472CA
if (DS_00105B39 != 0) return                  ; 0x472D4 (BSS, read-only)
0x470F8(side)                                 ; manage/ pick a move
entry = DSD(b+0x1C) + DSB(b+0x30)*8           ; the current move step
cmd[side] = 0x3C6E8(0x1A570(side) & 0xff, DSD(entry))   ; 0x472F7/0x472FF
if (DSD(entry) == 0xFFFF) { b+0x04 = 0; cmd[side] = 0; }          ; 0x4731C
else if ((s16)DSB(b+0x31) >= (s16)DSW(entry+4)) { b+0x31 = 0; b+0x30++; }  ; 0x47338
DSB(b+0x31)++; DSD(b+0x0C)++                  ; 0x4734F/0x47355
```

The `0x2D974`/`0x2CAA8` block at `0x47241` is **inert in the demo**:
`0x2CAA8` is `return DS_00105D60 == 0` (`0x2CAAC..0x2CAB7`
`cmp byte [0x105d60],0; setz al; and eax,0xff`; the port's
`config_not_free_play`, `config.c:203`). With FREE PLAY `DS_00105D60 == 0` in
the demo, `0x2CAA8` returns 1 and `(dip(0x29) & 0x800) && 0x2CAA8() == 0` is
false at `0x47237`/`0x4723B`. The block is a named gap because under free play
plus the DIP bit it runs (it zeroes `DS_001082C8` and does text work).

The per-side AI block is `DS_001081F0 + side*0x40` (`b`), 0x40 bytes:

| off | use | off | use |
|---|---|---|---|
| `+0x00` | frame counter | `+0x2C` | weight id |
| `+0x04` | active flag | `+0x2D` | band id |
| `+0x08` | move-step index | `+0x2E` | move id |
| `+0x0C` | frames-on-move | `+0x2F` | weight sub-index |
| `+0x10` | last state | `+0x30` | step cursor |
| `+0x14` | band-table ptr | `+0x31` | step timer |
| `+0x18` | move-id table ptr | `+0x34` | aux table ptr |
| `+0x1C` | move-step table ptr | `+0x38` | aux table ptr |
| `+0x20` | weight table ptr | `+0x3C` | aux dword |
| `+0x24` | prev state | `+0x28` | state |

**The chain.**

* **`0x469A8`** classifies `slot[param]`'s state into `block[1-param]+0x28`. It
  first copies `block[1-param]+0x24 = +0x28` when `fighter_frame_flag(0, param)`
  is clear (`0x469BB`), then calls `0x46958` (`slot+0x53 in {7,8}` and
  `slot+0x5F < 0x40`); on 1 it maps `block[1-param]+0x24` in `{0,1}` to state 1,
  else `0x46698` to 0xD, else 0. On 0 it tries, in order:
  `0x466F4`->2, `0x4673C`->3, `0x46794`->4, `0x467DC`->6, `0x4682C`->5,
  `0x468D8`->7, `slot[param]+0x53 == 0x0B`->9, `slot[param]+0x53 == 6`->0xB,
  `0x46898`->0xC, else 8. Predicates:
  `0x466F4` `+0x5F==0xFF && +0x54==0 && +0x53==0 && +0x52==0`;
  `0x4673C` `+0x5F==0xFF && +0x54==1 && +0x53==0 && +0x52 in {5,0x15}`;
  `0x46794` `+0x5F==0xFF && +0x54==2 && +0x53==0 && +0x52==4`;
  `0x467DC`/`0x4682C` `+0x54==0 && +0x53==0 && +0x52==1` plus `0x1A5D4` == 0 / != 0;
  `0x468D8`/`0x46898` the `rec_self+0x10 == 0x22BEC` identity; `0x1A5D4` reads
  `rec+0x28` bit 0x4000 and `cmd` bits 0x2000/0x1000.
* **`0x470F8`** clears `b+0x04` when the state is 0/7/9 and `slot+0x54 != 2` and
  the prior state differs, then, when `slot+0x53 == 0` and `b+0x04 == 0`, calls
  `0x46F4C` and sets `b+0x04=1`, `b+0x0C=0`, `b+0x00++`, `b+0x30=0`, `b+0x31=0`.
* **`0x46F4C`** selects the char's move-step and aux tables (`0x8C0BC`/`0x8A4A0`,
  `0x83C68`/`0x81E24`, `0x91D0C`/`0x90154`, `0x949E4`/`0x92F9C`,
  `0x8EF0C`/`0x8D2FC`, `0x892AC`/`0x87B1C`, `0x868D4`/`0x84F78`), calls `0x46C78`
  and `0x46DD4`, clamps `DS_001082C8` to 0..7, sets
  `b+0x20 = 0x95FC8 + DSB(b+0x2F)*0x50 + diff*0x0A`, draws `rng(0x64)`, and picks
  the first weighted index (index 9 is the fallback), setting `b+0x08`, `b+0x2E`
  and the `b+0x1C`/`b+0x3C` pointers.
* **`0x46C78`** maps the *other* slot's `+0x5F`/`+0x8A`/`+0x64` by state to a
  weight id and calls `0x46BBC`; `0x46BBC` stores `b+0x2C = id` and selects the
  band table `b+0x14 = 0x95CA8[char_self*10+char_other][id]` and the aux
  `b+0x34 = 0x95E38[...][id]`. `0x46C78` then sets `b+0x10 = state`.
* **`0x46DD4`** computes `|0x187FC()| >> 6` (clamped to 0xFF), where `0x187FC`
  latches both slots (`0x186D0` twice) and returns `slot0+0x2C - slot1+0x2C`,
  scans the `b+0x14` band table (4-byte `{id, sub, lo, hi}` records, 20 max) for
  `lo <= d <= hi`, and sets `b+0x2D`, `b+0x2F`,
  `b+0x18 = 0x8C238[...][id]`, `b+0x38 = 0x8A4A4[...][id]`.
* **`0x3C6E8(facing, input)`** maps `input` bits `0x10000/0x40000/0x20000/0x80000`
  to `0x10/0x20/0x1000/0x2000` (swapped for facing != 0) and returns `input & 0xffff`,
  so a move step's input word's other bits pass through.
* **`0x461DC`** writes each side's command word into the 0x14-word ring at
  `DS_00108270` (+0x28 per side) and advances `DS_001082D4`.

**RNG consumption.** `0x46F4C` draws exactly one `0x5D7DC(0x64)` (`0x47063`)
each time a move is committed, i.e. on the frame `0x470F8` finds `slot+0x53 == 0`
and `b+0x04 == 0` after a state transition or a `0xFFFF` terminator — not every
frame. There is no RNG draw elsewhere in `0x24C73`/`0x47208`/`0x469A8`/`0x461DC`.

### 11.3 `0x36E2C` and the `0x34B14` dispatch

`fight_health_sync` (`0x34B6C`) calls `0x36E2C(rec = slot)` at `0x34B8E`.
`0x36E2C` returns 1 iff
`(DS_00104B1D == 3 || DS_00104B14 == 0 || slot+0x63 != 0)` **and**
`slot+0x42 & 0x10` **and** `slot+0x52 in {0,1,5,6,7}` **and** `slot+0x54 <= 1`.

Gate 1 runs the position branch (`0x34B97..0x34BEF`):
`x = rec+0x18 ± 0x3000` (sign from `rec+0x29 & 0x40`); if
`-DS_000BE018 < x < DS_000BE018` (`DS_000BE018 = 0x7C00`) call `0x36F10(slot)`,
else `slot+0x43 |= 0x40` and `0x36638(slot, rec)`. **The branch is unreachable:**
slot+0x42 bit 0x10 is never written (the exhaustive `MOV/OR/AND ... +0x42]` scan
gives no writer; `0x36F10`'s own `0x36FD4` write is `| 0x820` = bits 0x20/0x800),
so the gate can never return 1 before `0x36F10` has run.

Gate 0 dispatches on `slot+0x52` through the 22-entry table at `0x34B14`:

| `+0x52` | entry | handler | port status |
|---|---|---|---|
| 0, >0x15 | `0x34C08` | `0x349C8` | ported (`fighter_state_default`) |
| 1 | `0x34C15` | `0x359E0` | gap |
| 2 | `0x34C26` | `0x35C1C`/`0x35D20` | gap |
| 3 | `0x34C46` | `0x35D7C` | ported (`fighter_state_35d7c`; `0x3CF38` arm gap) |
| 4 | `0x34C53` | `0x35F84` | gap |
| 5 | `0x34C62` | `0x36430` | gap |
| 6 | `0x34C73` | `0x1A978` | ported (`fight_stance_pass`) |
| 7 | `0x34C80` | `0x399CC` | gap |
| 8 | `0x34C8D` | `0x37464` | gap |
| 9,10,11,14,15,16 | `0x34D83` | epilogue no-op | ported |
| 12 | `0x34C9A` | `0x361C8` | gap |
| 13 | `0x34CA9` | `0x36300` | gap |
| 17 | `0x34CB8` | `0x36710` | gap |
| 18 | `0x34CC7` | `0x3BDDC`/`0x18B04` | partial (`fighter_attack_consume`) |
| 19 | `0x34D22` | `0x33B00` per side | gap |
| 20 | `0x34D69` | `0x35E6C` | gap |
| 21 | `0x34D78` | `0x364FC` | gap |

**`0x349C8` (default).** `rec = DS_001077A8[side]`, `fighter = DSD(rec)`. If
`slot+0x42` bit 6 -> `0x37178` (gap); bit 7 -> `0x37D18` (gap). Else:
`0x365C8(slot, rec, side)` sets/clears `slot+0x43` bit 0x40 (1 when the other
slot exists, its `+0x43` bit 0x80 is set, and this slot is behind it in the
facing direction); `0x36638(slot, rec)` returns 1 after clearing `slot+0x40`
bits and restarting the animation when `slot+0x43` bit 0x40 is set.
Then `bVar2 = (cmd>>8 & 3) != 0 && (cmd>>8 & 0xC) != 0`; if not, `0x3BDDC(side)`.
If `cmd & 0x4000`: `0x2BC30(fighter, 0xC8978[char], 0x40000000)`, `slot+0x52 = 5`,
`slot+0x54 = 1`. Else if `(slot+0x53 == 0 && cmd & 0x1000) || cmd & 0x2000`:
`0x35838(slot, fighter, cmd & 0xF000)`, which restarts the animation and sets
`slot+0x52 = 0x0E`.

**`0x36638(slot, rec)`.** `slot+0x54 == 3` clears `slot+0x43` bit 0x40 and
returns 0. Otherwise, with `slot+0x43` bit 0x40 set: `slot+0x40 &= 0xBFFF7FFF`,
`slot+0x41 |= 0x80`, `slot+0x53 = (mode == 0x25 ? 0xC : 0)`, then by `slot+0x54`:
default (0,2,3,>5) and 1 restart at `0xC91E8[char]`/`0xC9210[char]` and set
`slot+0x52 = 0x12` (return 1 / 0); 4 restarts at `0xC91E8[char]`, `slot+0x52 = 8`,
`slot+0x57 = 2` (return 0); 5 calls `0x38154` (gap).

**`0x2BC30(rec, anim, frame)`.** Resets `rec+0x0C/+0x10/+0x50/+0x52/+0x61`, sets
`rec+0x08 = anim`, `rec+0x28 &= 0xF7EB`, `rec+0x2B &= 0xFB`,
`rec+0x24 = rec+0x20 = frame`, consumes the leading animation command words with
`0x2B2A0(rec, rec+0x56)` (EBX = 0), then `pset = 0x1014EC + rec+0x56*0x20`,
`pset = 0x2A408(rec, pset)`. Ported as `actor_anim_start` (actors.c).

### 11.4 The think chain is dead

Confirmed (Task 8's carry-forward): `0x3B464` returns at `0x3B49F` when
`slot[other]+0x64 == 0xFF`. Every writer of `slot+0x64` sets `0xFF` — `0x33CFB`
(spawn), `0x33B00` (0x94-byte bulk copy plus a conditional `0xFF`; case 19 only),
`0x3B6B8`, `0x3B985`, `0x3B9D2` — and the only non-`0xFF` writer, `0x2A620`
(`0x2A65C`), writes an **actor record** (`0x2A690` is called at `0x25443` with
`EAX = DSD(slot[side])`), not the slot. The exhaustive `MOV ... +0x64]` and
`+0x62]/+0x63]` scans confirm no word/dword store overlaps `+0x64`. So
`fight_command_map` (`0x3B134`) and the Task-4 think chain are unreachable for
the demo's fighter slots. `0x3B298`'s other callers (`0x18C14`, `0x3B714`) are
off the demo path.

### 11.5 What the demo enters (measured in the port)

With the generator wired, the demo's command words are non-zero and vary
(`cmd0` 0x1010/0x0000, `cmd1` 0xA0A0/0x2020/0x0C0C/…). The `+0x52` states the
demo reaches: side 0 `0 -> 0x0E` (via `0x35838`), side 1 `0 -> 3` (via `0x3BDDC`
on the command's bit 15). `0x0E` is a table no-op; side 1 now takes its real
handler `0x35D7C`, which clears `slot+0x53`/`slot+0x54` and then declares the
`if (0x3CF38 == 0) { +0x54 = 2; +0x53 = 4; }` arm — `0x3CF38`
(`0x3CD44`/`0x3CE58`/`0x3C6A8`/`0x32BAC`) is the pre-existing §7.6 hit-detection
gap. So the port's arena output varies for ~430 frames and then freezes; closing
the fight needs cycle 2's combat chain.

### 11.6 Unit-test values

* **Generator (`fighter_command_block`).** Two live slots (`DS_001077A8[side] =
 *slot`, `slot+0x00 = fighter`, `slot+0x7A = 0`, `slot+0x63 = 1`,
  `slot+0x5F = 0xFF`, `slot+0x52 = 0`, fighter `+0x56 = 1`/`2`, actor word 0),
  `DS_00104B26 = 0`, `0x104B1B = 1`, `0x105B39 = 0`, `DS_001082C8 = 7`, the AI
  block zeroed, `rng_seed(0x1234)`. 30 calls must produce a non-zero
  `DS_001088E0` or `E2`. Evidence: `port/tests/test_fight.c:check_command_generator`.
* **Gate/dispatch (`fight_hud_pass`).** Input A: `slot+0x42 = 0x10`, `+0x52 = 0`,
  `+0x54 = 0`, fighter `+0x18 = 0x00010000`, `+0x28 = 0` -> gate 1 -> 0x36638
  arm -> `slot+0x52 == 0x12`, `slot+0x41 & 0x80`. Input B: `slot+0x42 = 0`,
  `+0x52 = 0`, `cmd = 0x1000` -> gate 0 -> table -> `0x349C8` -> `0x35838` ->
  `slot+0x52 == 0x0E`, `slot+0x43 & 1`. Input C: `+0x52 = 0x0E` with `cmd = 0x1000`
  leaves `+0x52 == 0x0E` and `slot+0x43 & 1 == 0`. Evidence:
  `port/tests/test_fight.c:check_state_dispatch`.

---

## 12. Provenance

* Raw bytes: `data/game/C/PRAGE.EXE` (read-only), 32-bit `capstone` over the LE
  page-mapped image with the internal 32-bit fixups applied, exactly as
  `port/src/mem.c`'s `mem_load_le` + `mem_load_le_fixups`.
* Decompilation: `port/decomp/prage.c` (read-only), line numbers cited inline.
* Port cross-references: `port/src/game/effects.c:512` (`0x1324C` update-table
  entry 0), `port/src/game/flow.c` (`game_state_step`, `game_frame`),
  `port/src/game/actors.c` (pset/animation machinery), `port/src/game/rng.c`
  (`0x5D7DC`), `port/src/platform/res.h` (`0x1B544` `res_resolve`).
* Prior derivations used: `2026-09-20-frontend-chain-derivations.md` §4.3 (process
  tables), §5 (camera `0x12D48`/`0x12CD4`/`0x1317C`/`0x1324C`/`0x13290`/`0x1333C`
  and the `0x1324C` dormancy proof), §8.6–§8.8 (camera anchors);
  `2026-09-17-actor-system-args.md` §0 (`0x2AE14` register binding).
* This record is the delivery of Task 1 of
  `docs/superpowers/plans/2026-09-20-demo-fight-motion.md`. Corrections to the
  plan/brief are in §0.2; the named gaps are in §7; the RNG answer is §5.9.
* §10 is the delivery of Task 6 of the same plan: the spawn chain `0x33EB4` /
  `0x33C78` and its callees, the resource-question answer, and the
  `DS_001082C8 = 7` handoff. Its port is `fighter_spawn` (fighter.c),
  `actor_pset_palette` (actors.c) and `fight_hud_spawn` (fight.c).
