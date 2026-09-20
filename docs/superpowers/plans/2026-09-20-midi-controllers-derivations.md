# MIDI controllers, the family applier and the `0x35fa` frequency path — raw-byte derivation (Task 1)

Register-level derivation of `SBPRO2.MDI`'s channel-controller handler (`0x3b54`),
the mask-driven register-family applier (`0x3184`) and the note-frequency routine
(`0x35fa`-`0x36a6`), from the shipped `data/game/C/SBPRO2.MDI`. This is the
raw-byte record Tasks 2-5 implement; it changes no source.

**Addressing.** `SBPRO2.MDI` is an `AIL3MDI` image with no load segment; every
address in the file is image-relative (see `tools/mdi_disasm.py`'s module
docstring). Ghidra loaded it with base `0`, so `file_offset == VA == the
`DAT_0000_xxxx` / `[0xyyyy]` addresses in the decompilation. All listings and
table offsets below are file offsets.

**Source size correction.** The Task-1 brief says the binary is 16458 bytes; the
shipped file is **16541** bytes (`16541` as reported by both `ls -l` and
`len(open(...).read())`). Trivial, but recorded because the brief is wrong.

Reproduction (read-only; `capstone` is already a repo tool dependency):

```python
from capstone import *
d = open('data/game/C/SBPRO2.MDI','rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_16)
for a, b in ((0x3184,0x36f0), (0x3a2f,0x3b54), (0x3b54,0x3c70)):
    print('===', hex(a))
    for i in md.disasm(d[a:b], a):
        print('%#06x  %-22s %s %s' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
```

The decompilation quotations below are from
`/var/folders/h_/rk2gng5d0x99pw7x_3dg6mj40000gn/T/opencode/ghidra_out/SBPRO2.MDI_decompiled.c`
(`FUN_0000_3184` at `:758-1004`, `FUN_0000_3b54` at `:1331-1452`).

---

## 1. The controller dispatch — `0x3b54`

`FUN_0000_3b54(status, b1, b2)` (`:1331`). Entry (`0x3b54`-`0x3b6e`):

```
0x3b54 55                 push bp
0x3b5a 8b7606             mov si, [bp+6]      ; b1
0x3b5d 81e6ff00           and si, 0xff        ; si = controller number / bend arg
0x3b61 8b7e04             mov di, [bp+4]      ; status
0x3b64 8bc7               mov ax, di
0x3b66 83e70f             and di, 0xf         ; di = channel (0..15)
0x3b69 25f000             and ax, 0xf0        ; ax = status hi-nibble
0x3b6c b500               mov ch, 0
0x3b6e 8a4e08             mov cl, [bp+8]      ; cl = b2
```

`ax` selects the status class; `si`/`cl` are the two data bytes. Dispatch:

| `status & 0xf0` | condition | action | store | family mask | address |
|---|---|---|---|---|---|
| `0xe0` | — | bend | `[ch+0x1929] = b1` (LSB), `[ch+0x1939] = b2` (MSB) | `0x01` | `0x3bb1`-`0x3bbd` |
| `0xb0` | ctrl 6 | bend scale | `[ch+0x18f9] = b2` | none (returns) | `0x3c93` |
| `0xb0` | ctrl 7 | volume | `[ch+0x1909] = b2` | `0x40` | `0x3c1a`/`0x3c50` |
| `0xb0` | ctrl 11 | expression | `[ch+0x1949] = b2` | `0x40` | `0x3c24`/`0x3c50` |
| `0xb0` | ctrl 1 | mod wheel | `[ch+0x1959] = b2` | `0x80` | `0x3c10`/`0x3c50` |
| `0xb0` | ctrl 10 | pan | `[ch+0x1919] = b2` | `0x08` | `0x3c2c`/`0x3c50` |
| `0xb0` | ctrl 64 | sustain | `[ch+0x1969] = b2`; if `b2 < 0x40` call `0x3b1e(ch)` | none | `0x3c7d`-`0x3c90` |
| `0xb0` | ctrl 121 | reset | reset block; call `0x3b1e(ch)` | `0xc1` | `0x3cc0`-`0x3ce2` |
| `0xb0` | ctrl 123 | all notes off | per active voice of `ch`: `0x39cc(ch, [v+0x14d5])` | none | `0x3c9a`-`0x3cbd` |
| `0xb0` | ctrl 112 | — | `[ch+0x1979] = b2` | none | `0x3c0a` |
| `0xb0` | ctrl 114 | — | `[ch+0x1999] = b2` | none | `0x3c04` |
| `0xb0` | ctrl 113 | — | `patch[ [ch+0x1989] ].0x133b = (b2 >= 0x40) ? 0x40 : 0` | none | `0x3bcf`-`0x3beb` |
| `0xb0` | anything else | — | — | none (returns) | `0x3c4d` |
| `0xc0` | — | program change | `[ch+0x19a9]=b1`; `patch = 0x2ca6(([ch+0x1999]<<8) | b1)`; `[ch+0x1989]=patch` | none | `0x3bed`-`0x3c02` |
| `0x80` | — | note off | `0x39cc(ch, b1)` | — | `0x3ba4` |
| `0x90` | `b2 != 0` | note on | `0x3a2f(ch, b1, b2)` | — | `0x3b96` |
| `0x90` | `b2 == 0` | note off | `0x39cc(ch, b1)` | — | `0x3b94` falls to `0x3ba4` |
| other | — | — | — | — | `0x3bac` (return) |

`0x3b54` never re-applies for ctrl 6, 64, 112, 113, 114 or 123; only the
bend/7/11/1/10/121 rows reach the re-apply loop. `0xE0` is one of the "hi"
classes (`0x3b7b`), and note-on/off are `0x90`/`0x80` (`0x3b76`-`0x3b8d`).
The decompilation collapses the `0x80` and `0x90`-with-zero-velocity arms into the
same `FUN_0000_39cc` call (`:1422`-`:1435`).

### 1a. The reset block (ctrl 121) and the re-apply loop

```
0x3cc0 c685691900         mov byte [di+0x1969], 0     ; sustain
0x3cc5 57                 push di
0x3cc6 e855fe             call 0x3b1e
0x3cc9 83c402             add sp, 2
0x3ccc c685591900         mov byte [di+0x1959], 0     ; mod
0x3cd1 c68549197f         mov byte [di+0x1949], 0x7f  ; expression
0x3cd6 c685291900         mov byte [di+0x1929], 0     ; wheel LSB
0x3cdb c685391940         mov byte [di+0x1939], 0x40  ; wheel MSB
0x3ce0 b0c1               mov al, 0xc1
0x3ce2 e96eff             jmp 0x3c53
```

So the reset values are exactly the brief's: `0x1929=0`, `0x1939=0x40`,
`0x1949=0x7f`, `0x1959=0`, `0x1969=0`; **`0x1909` (volume), `0x1919` (pan) and
`0x18f9` (bend scale) are not touched by this reset.** Volume and bend scale are
consistent with BSS zero; pan is **not** (the capture's first key-on shows a
non-zero pan, §5). Mask `0xC1` = `0x80|0x40|0x01` (AM/VIB + TL + frequency).

The common tail (`0x3c50`-`0x3c7a`, the brief's re-apply loop) is reached with
`al` = mask and `di` = channel:

```
0x3c50 2e8809             mov byte cs:[bx+di], cl   ; the state store, bx = state base
0x3c53 8bdf               mov bx, di                ; bx = channel
0x3c55 be0000             mov si, 0
0x3c58 80bc851400         cmp byte [si+0x1485], 0   ; voice active?
0x3c5d 7415               je  0x3c74
0x3c5f 389cc114           cmp byte [si+0x14c1], bl  ; same MIDI channel?
0x3c63 750f               jne 0x3c74
0x3c65 08843915           or  byte [si+0x1539], al  ; set the family bit
0x3c69 50                 push ax
0x3c6a 53                 push bx
0x3c6b 56                 push si
0x3c6c e815f5             call 0x3184
0x3c74 46                 inc si
0x3c75 83fe14             cmp si, 0x14
0x3c78 75de               jne 0x3c58
```

i.e. **for `v = 0..19`, if `[v+0x1485] != 0` and `[v+0x14c1] == ch`:
`[v+0x1539] |= mask; FUN_0000_3184(v)`**. The decompilation states the same at
`:1442`-`:1450`. Note the comparison is plain equality (`bl = ch` already `& 0xf`
at `0x3b66`), not a re-mask of `[0x14c1]`.

`0x3b1e` (the sustain-off/reset helper, `0x3b1e`-`0x3b53`) loops `v = 0..19` and
for each `[v+0x1485] != 0`, `[v+0x14c1] == ch`, `[v+0x1525] != 0` calls
`0x39cc(ch, [v+0x14d5])`. `0x39cc(channel, note)` (`0x39cc`-`0x3a2e`) loops active
voices (`[v+0x1485] == 1`) matching `[v+0x14e9] == note` and `[v+0x14c1] ==
channel`; for each, if sustain `[ch+0x1969] >= 0x40` it sets `[v+0x1525]=1`,
else it either frees the voice (type `[v+0x1499]` is `3` or `0`, after calling
`0x3127(v)` and clearing `[v+0x1485]`) or marks it releasing
(`[v+0x145d word] = 1`). The bodies of `0x3b1e`/`0x39cc` are **named gaps** for
Task 5 (spec §10), not implemented here.

---

## 2. The family applier — `0x3184`

`FUN_0000_3184(voice)` (`:758`). It returns immediately when the voice's patch
offset `[v+0x14ad] == 0xff` (`0x3190`). The mask is `[v+0x1539]`; each family is
tested in the order `0x80, 0x40, 0x20, 0x10, 0x08, 0x01` (`0x3313`-`0x334c`,
decomp `:882`-`:942`) and its bit is **cleared after writing**
(`and [v+0x1539],0x7f/0xbf/0xdf/0xef/0xf7/0xfe`), so a mask is consumed once per
pass. For a voice of type 3 (`[v+0x1499] == 3`, the decompiler's `bVar12`, set at
`0x31ca`/`:816`) the routine restores the saved mask and runs a second pass
(`0x3358`-`0x3360`, `:998`-`:1002`); every other type returns after one pass.

Write order: `0x20` → `0x40` (TL) → `0x60`/`0x80` → `0xE0` → `0xC0` →
`0xA0`/`0xB0`. Per-operator writes go through `FUN_0000_2aa4(target, reg, value)`,
per-channel writes through `FUN_0000_2abe(chan, reg, value)`.

Local voice fields are loaded from the voice record by one of two blocks: the
**voice-type-3 path** (`0x327e`-`0x3310`; decomp `:836`-`:857`) or the **normal
path** (`0x3364`-`0x33ef`; decomp `:860`-`:880`), selected at `0x3275` by
`[v+0x1499] == 3`. The two paths are parallel but use different arrays; the table
below lists the **normal-path** array for each local. (The type-3 path also
pre-shifts `[bp-0x32]` once, `0x32fc` vs `0x33e1`.)

**The gate array `[bp-0x14]` depends on the path** and this matters for the TL
fold: the normal path loads it from `[si+0x1629]` (`0x33e8 8a842916`, decomp
`:880`), the type-3 path from `[si+0x18e5]` (`0x3305 8a84e518`, decomp `:856`).
The two arrays are **independent** — the patch loader writes both from different
patch bytes (decomp `:1130` `[0x1629] = patch[0xd27]`, `:1131` `[0x18e5] =
patch[0xd2b]`). The `0x3473`/`0x34a9` tests read `[bp-0x14]`, so the gate source
is `0x1629` for a normal voice and `0x18e5` for a type-3 voice. Everything else in
the table is the normal-path array; the type-3 path uses the parallel block at
`0x327e`-`0x3310`.

| local | voice array (normal) | meaning, by family |
|---|---|---|
| `[bp-4]` / `[bp-6]` | `0xc37`/`0xc49` + patch idx | per-operator write target |
| `[bp-0x1a]` / `[bp-0x1c]` | `0x15b1`/`0x15c5` | `0x20` low byte, op1/op2 |
| `[bp-0x16]` / `[bp-0x18]` | `0x168d`/`0x1665` (16-bit) | `0x20` high nibble, op1/op2 |
| `[bp-0x22]` / `[bp-0x24]` | `0x1589`/`0x159d` | TL byte, op1/op2 |
| `[bp-0x1e]` / `[bp-0x20]` | `0x172d`/`0x1705` (16-bit) | TL attenuation, op1/op2 |
| `[bp-0x26]` / `[bp-0x28]` | `0x15d9`/`0x15ed` | `0x60` (AD), op1/op2 |
| `[bp-0x2a]` / `[bp-0x2c]` | `0x1601`/`0x1615` | `0x80` (SR), op1/op2 |
| `[bp-0x2e]` | `0x163d` (16-bit) | `0xE0` (wave): low byte = op2, high byte = op1 |
| `[bp-0x30]` | `0x16b5` (16-bit) | `0xC0` connection bits |
| `[bp-0x32]` | `0x1575` | `0xC0` bit 0 |
| `[bp-0x14]` | `0x1629` (type-3: `0x18e5`) | per-operator gate bits |
| `[bp-8]` | computed | TL channel level (below) |

### 2a. `0x80` — `0x20` family (AM/VIB/EG/KSR/MULT)

```
0x3409 mov di,[si+0x14c1]; and di,0xf
0x3410 mov al,[di+0x1959]        ; mod wheel
0x3414 mov di,0x40
0x3417 cmp al,0x40
0x3419 jge 0x341e
0x341b mov di,0
0x341e mov ax,[bp-0x16]          ; op1 16-bit field
0x3421 mov cl,4
0x3423 shr ax,cl                 ; >>4
0x3425 mov al,ah                 ; low byte = field >> 12
0x3427 or ax,di                  ; AM bit when mod >= 0x40
0x3429 or al,[bp-0x1a]           ; | op1 low byte
0x342c.. call 0x2aa4(...,0x20,...)
```

Op2 is identical at `0x3440`-`0x345f` with `[bp-0x18]`/`[bp-0x1c]`. **Controller
input:** `[ch+0x1959] >= 0x40` sets bit `0x40`. The brief's "patch `[3]`/`[9]`" is
the `[bp-0x1a]`/`[bp-0x1c]` low byte; the `0x3421`/`0x3425` `field >> 12` high
nibble is ORed in as well (source byte = `((field >> 4) >> 8) | am | lowbyte`, and
the same high byte is written into the `ax` high half that `0x2aa4` sees).

### 2b. `0x40` — TL family (the exact fold)

**Channel level**, `0x319a`-`0x31c4` (the brief's "lines `0x30`-`0x44`"; decomp
`:808`-`:814`):

```
0x319a test byte [si+0x1539],0x40
0x31a1 mov di,[si+0x14c1]; and di,0xf
0x31a8 mov al,[di+0x1909]        ; volume
0x31ac mul byte [di+0x1949]      ; * expression      -> ax (16-bit)
0x31b0 shl ax,1
0x31b2 mov al,ah                 ; al = (volume*expression*2) >> 8
0x31b4 cmp al,1
0x31b6 sbb al,0xff               ; al = (al == 0) ? 0 : al + 1
0x31b8 mul byte [si+0x1511]      ; * per-voice level factor
0x31bc shl ax,1
0x31be mov al,ah                 ; al = (prev * [0x1511] * 2) >> 8
0x31c0 cmp al,1
0x31c2 sbb al,0xff               ; same "+ (al != 0)"
0x31c4 mov [bp-8],al             ; = local_a
```

So `local_a = g( g( (volume*expression*2) >> 8 ) * [v+0x1511] * 2 >> 8 )`, where
`g(x) = (x==0) ? 0 : x+1`. `[v+0x1511]` is the per-voice velocity byte (set in
key-on from the `0xc27` `xlatb` table at `0x3ace`-`0x3ad3`). `sbb al,0xff` after
`cmp al,1` is exactly the decompiler's `(cVar6 + 1) - (cVar6 == 0)`; both share
the `+1` when nonzero.

**Per operator** (`0x346a`-`0x34d3`; decomp `:893`-`:910`):

```
0x346a mov bx,[bp-0x1e]          ; op1 16-bit field
0x346d shr bx,1
0x346f shr bx,1                  ; bx >>= 2
0x3471 mov bl,bh                 ; bl = field >> 10
0x3473 test byte [bp-0x14],1     ; gate array (0x1629 / 0x18e5), bit 0
0x3477 je 0x3484
0x3479 mov cl,0x7f
0x347b mov al,bh
0x347d mul byte [bp-8]           ; * local_a
0x3480 div cl                    ; / 0x7f
0x3482 mov bl,al
0x3484 not bl
0x3486 and bl,0x3f
0x3489 or bl,[bp-0x22]           ; | op1 TL byte
0x348c.. call 0x2aa4(...,0x40,...)
```

Op2 at `0x34a0`-`0x34d0`: same with `[bp-0x20]`, **bit 1** of `[bp-0x14]`
(`test ...,2` at `0x34a9`), and `[bp-0x24]`. Formally, per operator:

```
att  = (field >> 10)                         # field = 0x172d / 0x1705 (16-bit)
gate = ([v+0x1499] == 3) ? [v+0x18e5] : [v+0x1629]      # per voice type
if (gate & bit) att = (att * local_a) / 0x7f  # unsigned div
TL   = (att ^ 0x3f) | patch_TL                # ~att & 0x3f, then OR patch TL
```

where `bit` is `1` for op1 and `2` for op2. With the default state (volume 0,
expression 0, or the reset block's expression `0x7f`) `local_a` is 0 when
`volume == 0`, so `att` becomes 0 and `TL = 0x3f | patch_TL` — which is what the
capture's tick-0 TL writes show (below). This is the brief's candidate fold, made
precise: the re-scale is `att * local_a / 0x7f`, not `att * (local_a/0x7f)`, and
the source of `patch_TL` is a cached voice byte (`0x1589`/`0x159d`), not the raw
patch payload. Mapping those cached bytes back to `p[4]`/`p[10]` is **Task 5's**
job; the `0x40` register byte is `((att ^ 0x3f) & 0x3f) | cached_TL`.

### 2c. `0x20` — `0x60`/`0x80` family

`0x34de`-`0x353f`: write `0x60` op1 (`[bp-0x26]`), `0x60` op2 (`[bp-0x28]`),
`0x80` op1 (`[bp-0x2a]`), `0x80` op2 (`[bp-0x2c]`). No controller input. The
brief's "patch `[5]`/`[6]`, `[11]`/`[12]`" identifies the cached bytes
(`0x15d9`/`0x15ed` = AD, `0x1601`/`0x1615` = SR).

### 2d. `0x10` — `0xE0` wave family

`0x3542`-`0x3575`: writes the `0xE0` register **carrier first** — op2 gets the
low byte of the 16-bit field `[v+0x163d]` (`0x3542`), then op1 gets its high byte
(`0x3559`). No controller input.

### 2e. `0x08` — `0xC0` connection/pan family

`0x3578`-`0x35bf`:

```
0x3578 mov ax,[bp-0x30]          ; 16-bit field
0x357b mov cl,4; shr ax,cl       ; >>4
0x357f and ah,0xe                ; high byte & 0xe
0x3582 mov al,[bp-0x32]; and al,1
0x3587 or al,ah
0x3589 or al,0x30                ; base always sets 0x30
0x358b mov di,[si+0x14c1]; and di,0xf
0x3592 mov bl,[di+0x1919]        ; pan
0x3596 cmp bl,0x1b; jbe 0x35a4   ; unsigned <= 27
0x359b cmp bl,0x64; jb 0x35a6    ; unsigned < 100
0x35a0 and al,0xdf               ; > 99  -> clear 0x20 (leaves 0x10)
0x35a4 and al,0xef               ; <= 27 -> clear 0x10 (leaves 0x20)
0x35a6.. call 0x2abe(chan,0xc0,...)
```

**Controller input:** `[ch+0x1919] < 0x1c` → `0x20`, `> 99` → `0x10`, else
`0x30` (the brief's rule, confirmed; the compares are unsigned `jbe`/`jb`).

### 2f. `0x01` — `0xA0`/`0xB0` frequency family

`0x35c2`-`0x36ee`. A voice of type 3 (`[bp-0x10] == 1`, set at `0x31ca` when
`[v+0x1499] == 3`) **skips** the frequency computation entirely: `0x35c8` jumps to
the mask clear at `0x36ee`. For every other type: if `[v+0x1499] == 2` (`0x35d1`)
it takes the legacy `0x36ab` path (`uVar11 = word[0x1755 + 2*v] >> 6`, `0x36ab`:
`mov bx,si`; `0x36ad mov ax,[bx+si+0x1755]`; `0x36b3 shr ax,6`); else `0x35d6`
checks `[v+0x1561] & 0x20` — if clear it writes a key-off `0xB0` (`0x35dd`, value
`[v+0x154d] & ~0x20`), else it executes the `0x35fa` block. `0xB0` is always
`cached_b0 | [v+0x1561]` (`0x36d2`), so the key-on bit is preserved across a
re-apply. See §3.

---

## 3. The frequency routine — `0x35fa`-`0x36a6`

Entry (`0x35fa`, reached from `0x35d6` only for a non-type-3, non-type-2 voice
whose `[v+0x1561] & 0x20` is set). Full listing of the arithmetic:

```
0x35fa 8a9cc114   mov bl,[si+0x14c1]     ; MIDI channel
0x35fe b700       mov bh,0
0x3600 8a873919   mov al,[bx+0x1939]     ; wheel MSB
0x3604 b400       mov ah,0
0x3606 b90700     mov cx,7
0x3609 d3e0       shl ax,cl              ; ax = msb << 7
0x360b 0a872919   or al,[bx+0x1929]      ; | wheel LSB
0x360f 2d0020     sub ax,0x2000          ; 14-bit wheel - centre
0x3612 b90500     mov cx,5
0x3615 d3f8       sar ax,cl              ; >>5 (arithmetic)
0x3617 8a8ff918   mov cl,[bx+0x18f9]     ; bend scale
0x361b b500       mov ch,0
0x361d f7e9       imul cx                ; * scale (signed 16x16 -> dx:ax)
0x361f 8a9cd514   mov bl,[si+0x14d5]     ; note / percussion base
0x3623 b700       mov bh,0
0x3625 8bc8       mov cx,ax              ; save bend product
0x3627 8a84fd14   mov al,[si+0x14fd]     ; transpose (signed)
0x362b 98         cwde                   ; sign-extend
0x362c 03d8       add bx,ax
0x362e 8bc1       mov ax,cx
0x3630 83eb18     sub bx,0x18            ; bx = X - 24
0x3633 83c30c     add bx,0xc             ; do { bx += 12 }
0x3636 83fb00     cmp bx,0
0x3639 7cf8       jl 0x3633              ;    while (bx < 0)
0x363b 83c30c     add bx,0xc
0x363e 83eb0c     sub bx,0xc             ; (no-op pair; sets flags)
0x3641 83fb5f     cmp bx,0x5f
0x3644 7ff8       jg 0x363e              ; while (bx > 0x5f) bx -= 12
0x3646 02e3       add ah,bl              ; ax += idx << 8
0x3648 050800     add ax,8
0x364b b90400     mov cx,4
0x364e d3f8       sar ax,cl              ; ax = (bend + idx*256 + 8) >> 4
0x3650 2dc000     sub ax,0xc0
0x3653 05c000     add ax,0xc0            ; while (ax < 0)   ax += 0xc0
0x3656 3d0000     cmp ax,0
0x3659 7cf8       jl 0x3653
0x365b 05c000     add ax,0xc0
0x365e 2dc000     sub ax,0xc0            ; while (ax > 0x5ff) ax -= 0xc0
0x3661 3dff05     cmp ax,0x5ff
0x3664 7ff8       jg 0x365e
0x3666 8bf8       mov di,ax
0x3668 b90400     mov cx,4
0x366b d3ef       shr di,cl              ; idx2 = ax >> 4
0x366d 8bd7       mov dx,di
0x366f 8a9ddd09   mov bl,[di+0x9dd]      ; sem = T9DD[idx2]
0x3673 b700       mov bh,0
0x3675 8bfb       mov di,bx
0x3677 b90500     mov cx,5
0x367a d3e7       shl di,cl              ; di = sem * 32  (bytes)
0x367c d1e0       shl ax,1
0x367e 251f00     and ax,0x1f            ; (2*ax) & 0x1f  (byte step)
0x3681 03f8       add di,ax
0x3683 8b85fd07   mov ax,[di+0x7fd]      ; v = pitch_tbl[0x7fd + sem*32 + step]
0x3687 8bfa       mov di,dx
0x3689 8a9d7d09   mov bl,[di+0x97d]      ; oct = T97D[idx2]
0x368d fecb       dec bl                 ; oct - 1
0x368f 0bc0       or ax,ax
0x3691 7d02       jge 0x3695             ; if v < 0: bl = oct
0x3693 fec3       inc bl
0x3695 0adb       or bl,bl
0x3697 7d04       jge 0x369d             ; if bl < 0: bl++, v >>= 1
0x3699 fec3       inc bl
0x369b d1f8       sar ax,1
0x369d d0e3       shl bl,1
0x369f d0e3       shl bl,1               ; bl <<= 2
0x36a1 80e403     and ah,3
0x36a4 0ae3       or ah,bl               ; ah = (v>>8 & 3) | block<<2
0x36a6 8946f2     mov [bp-0xe],ax
```

Then `0x36b8` writes `0xA0 = al`, `0x36cf` writes `0xB0 = ah | [v+0x1561]`, and
caches the payload to `[v+0x154d]` (`0x36d6`).

The decompilation at `:953`-`:988` matches every step except the two index folds,
which it renders as `iVar10 + 0x18` (`:964`) instead of the raw's `+0xc` — a
12-unit decompiler error (see §6).

### 3a. The block/octave convention — settled

The brief's naive transcription (`idx = ax >> 4` with `ax` built from the **raw**
note, then `block = T97D[idx] - 1 + (v < 0)`) is one octave high because **the raw
folds the index down by 12 before it enters the high byte of `ax`**. The raw does
this with the first do-while (`0x3630`-`0x3639`): `bx = X - 24` then *at least one*
`bx += 12`, i.e. `X - 12`, followed by the `> 0x5f` wrap. The corrected form, all
in the routine's own terms:

```
X   = note + transpose          # melodic: [0x14d5]=note, [0x14fd]=base
                                # percussion: [0x14d5]=base, [0x14fd]=0
bx  = X - 0x18
do { bx += 0xc } while (bx < 0)         # == fold12(X - 12)
while (bx > 0x5f) bx -= 0xc
ax  = (bend + (bx << 8) + 8) >> 4       # arithmetic shift
while (ax < 0)   ax += 0xc0             # fold mod 0xc0 (NOT a clamp)
while (ax > 0x5ff) ax -= 0xc0
idx2 = ax >> 4                          # logical
sem  = T9DD[idx2]
oct  = T97D[idx2]
v    = PITCH_TBL[16*sem + (ax & 0xf)]   # byte step ((2*ax)&0x1f) / 2
block = oct - 1 + (v < 0)
if (block < 0) { block++; v >>= 1 }
a0 = v & 0xff
b0 = (block << 2) | ((v >> 8) & 3)
```

Three separate corrections against the plan's sketch (`pitch.c` in the plan,
Task 2):

1. **The index fold is missing.** `X` must be folded to `fold12(X-12)` before
   `<< 8`; `idx` (the `ax >> 4`) is that folded value, not the raw note/base.
2. **The `ax` normalisation is a mod-`0xc0` fold, not `clamp(0,0x5ff)`.** The
   plan's `if (ax<0) ax=0; if (ax>0x5ff) ax=0x5ff` differs for a bent wheel that
   crosses an octave boundary.
3. **The pitch-table step is a byte offset.** `(2*ax)&0x1f` addresses the table in
   bytes; the word index is `((2*ax)&0x1f)>>1 = ax & 0xf`. The plan's
   `PITCH_TBL[16*sem + ((2*ax)&0x1f)]` doubles the step.

The `v < 0` octave carry is real and load-bearing: the table stores the top
semitones (F#-ish and up) as negative 16-bit values because their fnum would
exceed the 10-bit field; the routine halves `v` and bumps the block.

**Anchor verification** (real output; `data/game/C/SBPRO2.MDI`):

```
note/base  84 naive=(6, 178, 26)  raw=(72, 5, 178, 22)  anchor=(block=5,a0=0xb2,b0=0x16)
note/base  79 naive=(6, 5, 26)    raw=(67, 5, 5, 22)    anchor=(block=5,a0=0x05,b0=0x16)
note/base  54 naive=(3, 207, 15)  raw=(42, 2, 207, 11)  anchor=(block=2,a0=0xcf,b0=0x0b)
```

`naive` is the brief's one-octave-high form (note 84 → block 6, base 54 → block
3 — both exactly the brief's stated failures); `raw` is the corrected form above
(returns `(folded_index, block, a0, b0)`). **All three capture-pinned anchors
reproduce:** note 84 → folded index 72 → `v = T[0] = 690` (positive) → block
`T97D[72]-1 = 5`, `a0=0xB2`, `b0=0x16`; note 79 → folded index 67 →
`v = T[112] = -507` (negative) → block `T97D[67]-1+1 = 5`, `a0=v&0xff=0x05`,
`b0=(5<<2)|((v>>8)&3)=0x16`; percussion base 54 → folded index 42 →
`v = T[96]=975` → block `T97D[42]-1 = 2`, `a0=0xCF`, `b0=0x0B`. The 79 case is
the one that exercises the negative-`v` carry, and it is why the anchor lands on
block 5 rather than 4.

### 3b. Equivalence to the retired `NOTE_TAB` (caveat for Task 2)

For the compared window (every shipped melodic patch has `[2] = 0`, so `X = note`)
the corrected routine reproduces the shipped `NOTE_TAB[note]` at the anchors and
through the used range. It does **not** equal `NOTE_TAB[i]` for all 128 `i`:

- `i = 0..11`: the raw returns `block 0` with the doubled fnum (`T[0] = 690` →
  `block<0` handling halves once → `0x159`), while `NOTE_TAB` stores the halved
  fnum (`0x0AC`).
- `i = 16, 18`: off by one fnum unit (rounding).
- `i = 108..127`: the raw's `while (bx > 0x5f)` wraps the index down an octave
  (block 6), while `NOTE_TAB` keeps block 7.

So the spec §8.1 claim "centred equals `NOTE_TAB` for all 128 indices" is
**overstated at the extremes**; the anchors, the used notes and the capture oracle
govern. Named here so Task 2 does not silently treat the two as identical.

---

## 4. The tables

Extraction (the brief's command, verbatim):

```sh
python3 - <<'EOF'
import struct
d=open('data/game/C/SBPRO2.MDI','rb').read()
tbl=[struct.unpack_from('<h',d,0x7fd+2*i)[0] for i in range(192)]  # signed
oct_=[b for b in d[0x97d:0x97d+96]]
sem=[b for b in d[0x9dd:0x9dd+96]]
print(len(tbl), tbl[:8], tbl[-4:])
print(len(oct_), sorted(set(oct_)), len(sem), sorted(set(sem)))
EOF
```

Observed output (real):

```
192 [690, 692, 695, 697, 700, 702, 705, 707] [-344, -342, -339, -337]
96 [0, 1, 2, 3, 4, 5, 6, 7] 96 [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]
```

The brief's expected `[65189, 65192, 65194, 65197]` are the **unsigned** forms of
entries `187..190` (`-347,-344,-342,-339`), i.e. a 191-entry read; the file's
192-entry read (required by the routine, which reaches word index `191`) ends at
`-337` (`0xFEAF`). The raw file wins; recorded rather than fitted.

Observed extents (real output):

```
file size: 16541
pitch table 0x7fd..0x97d  (384 bytes, 192 words)
  words[0]=690 0x2b2  words[191]=-337 0xfeaf
  signed min=-511 max=1022
T97D 0x97d..0x9dd (96 bytes) values 0..7
T9DD 0x9dd..0xa3d (96 bytes) values 0..11
```

- **`PITCH_TBL`** — file `0x7fd`, 192 **signed** 16-bit words (384 bytes), ending
  exactly at `0x97d`. 12 semitones × 16 fine steps; `words[0] = 690 = 0x2B2`
  (note 84's anchor). The upper semitones are negative; `signed min=-511
  max=1022`. The byte before it (`0x7fb`,`0x7fc`) is `00 00`.
- **`T97D`** — file `0x97d`, 96 bytes, values `0..7` (`idx/12`), ends at `0x9dd`.
- **`T9DD`** — file `0x9dd`, 96 bytes, values `0..11` (`idx mod 12`), ends at
  `0xa3d`. Its first byte is also the first byte past `T97D`, confirming the
  contiguous layout.

The routine can read `16*11 + 15 = 191`, the last word of `PITCH_TBL`; `idx2`
reaches at most `0x5f = 95`, the last entry of both 96-byte tables.

---

## 5. Defaults against the capture

The DRO capture (`data/audio-captures/prage_000.dro`) records **OPL register
writes only** — `tools/opl_trace.py`'s format has no MIDI-event channel, and
`data/audio-captures/prage-capture.log` is DOSBox-X startup logging. So the
"first ctrl-7/11/10/0xE0 events" cannot be read directly; what is directly
observable is the register-write baseline before any controller can have acted,
and the mid-note frequency re-applies a bend produces. Both are recorded here as
evidence, with the inference named.

The capture's tick-0 register baseline (the first key-on is percussion):

```
       0 0x00a0 0xcf
       0 0x00bd 0xc0
       0 0x00c0 0x34
       0 0x00b0 0x2b
```

and the TL family at tick 0 is `0x3f` everywhere except `0x43 = 0x16`.

- **Wheel default is centred (`0x2000`).** The first key-on is the percussion
  voice: `A0=0xCF`, `B0=0x2B` = key-on `0x20` | payload `0x0B`. The corrected
  routine above with `bend = 0` and base 54 gives exactly `a0=0xCF`, `b0=0x0B` —
  so the pre-controller wheel state is `0x2000`, not assumed.
- **TL is at the floor before any controller.** `0x40`/`0x41`/`0x42`/`0x44`… all
  `0x3f` at tick 0. With `volume = 0` (BSS, never reset by the 121 block) the
  level term `local_a` is 0, so `(att ^ 0x3f) | patch_TL` saturates to `0x3f` for
  the gated operators — consistent with volume defaulting to 0. `0x43 = 0x16`
  is one operator where the gate bit of `[bp-0x14]` (normal `[v+0x1629]`, type-3
  `[v+0x18e5]`) is clear (so `att` is the patch value) or where `patch_TL`
  dominates; `0x43` is not the modulator/carrier pair of a single channel, so no
  stronger claim is made here.
- **Pan default selects `0x30`.** Tick-0 `C0 = 0x34` = connection low bits `0x04`
  | `0x30`. The `0x30` (rather than `0x20`) requires `[ch+0x1919]` in
  `[0x1c, 99]`, so the pan default is **not** BSS-zero (a zero pan would give
  `0x24`). The MIDI centre `0x40` is the natural value and yields `0x30`. Named as
  a residual: the exact default of `[0x1919]` is not read from this capture.
- **The bend path is exercised later.** On channel 6 the capture writes
  `A6=0x7F` at DRO tick 7572 and then `B6=0x0E` at 7616 — `0x0E` has **no** key-on
  bit, so this rewrites a sounding voice's frequency mid-note (item 10). A second
  key-on-less pair is `B6=0x0F` at 8284. (Not every `A6` rewrite is mid-note:
  `B6=0x2F`/`0x2E` at 6448/7532/8200 ride the key-on bit `0x20`.) The
  capture-oracle tick `969` in `port/spec/audio.md` is the oracle's normalised
  tick; the DRO's raw tick for the same write is `7572`.

---

## 6. Corrections vs the brief / plan

On plan-vs-raw conflict the raw wins. Corrections recorded:

1. **Binary size** — 16541 bytes, not 16458.
2. **Table read** — 192 words ending at `-337`; the brief's expected last four
   are entries `187..190`.
3. **The block/octave one-octave error is the missing index fold, not the block
   formula.** `block = T97D[idx2] - 1 + (v < 0)` is correct; the raw folds
   `X - 12` into `idx` before the `<<8`. The plan's `pitch.c` uses the raw index
   directly, so it is one octave high for notes whose folded index crosses a
   `T97D` boundary (84, 79) and for the percussion base (54).
4. **`ax` is folded mod `0xc0`, not clamped.** The plan's `clamp(0,0x5ff)` is
   wrong for a wide bend across an octave boundary.
5. **Pitch-table step is a byte offset.** Word index `= 16*sem + (ax & 0xf)`, not
   `16*sem + ((2*ax)&0x1f)`.
6. **TL fold** — `att` is `(field>>10) * local_a / 0x7f` (the multiply then
   divide, integer), `patch_TL` is the cached voice byte `0x1589`/`0x159d`, and
   the gate bits are `[bp-0x14]` bit 0 (op1) / bit 1 (op2) — where `[bp-0x14]` is
   `[v+0x1629]` for a normal voice and `[v+0x18e5]` for a type-3 voice (`0x33e8`
   vs `0x3305`). `local_a` itself is
   `g(g((volume*expression*2)>>8) * [v+0x1511] * 2 >> 8)` with `g(x)=x+(x!=0)`.
7. **The decompilation's index fold** (`:964`, `iVar10 += 0x18`) is 12 too large;
   the raw is `+0xc` (`0x363b`) after the mandatory `+0xc` in the do-while.
8. **Reset does not touch volume/pan/bend scale** — only `0x1929`, `0x1939`,
   `0x1949`, `0x1959`, `0x1969`.

## 7. What could not be determined

- **The MIDI controller event stream.** The DRO carries no MIDI, so the first
  ctrl-7/11/10/`0xE0` events are not in evidence. The register-write baseline and
  the mid-note `0xA6` rewrites are the observable proxies.
- **The pan default value.** Tick-0 `C0 = 0x34` implies `[ch+0x1919]` is in
  `[0x1c,99]`, but the exact value is not read from the capture; the "BSS zero"
  claim in spec §4 is not confirmed for pan.
- **The cached voice byte ↔ patch payload mapping.** The TL/EG/wave sources are
  voice-record caches (`0x1589`, `0x15d9`, `0x163d`, …) populated by the patch
  loader `0x3085`; pinning each to `p[4]`/`p[5]`/`p[7]` etc. is left to Task 5.
- **`0x3b1e` (sustain) and `0x39cc` (all-notes-off) bodies** are quoted at the
  dispatch level only; their per-voice release paths are named gaps (spec §10).
