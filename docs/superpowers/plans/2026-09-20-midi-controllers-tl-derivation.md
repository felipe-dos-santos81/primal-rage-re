# SBPRO2.MDI total-level (TL) carrier scaling — derivation correction

**Verdict: `NOT PINNABLE`.** The driver's gated TL carrier branch is derived here
instruction by instruction, and the prior task's `<<2` reading of the patch loader
is **wrong** (the loader stores `<<10`, matching the branch's `>>10`). But the
resulting formula — the Task-1 record's own formula — does **not** reproduce the
capture: it matches **0 of 797** carrier-TL writes, and no non-fitted alternative
(a divisor family, or a level-function family) reproduces the capture either. The
binary's mechanism is pinned; a capture-faithful mechanism is not.

This document corrects `2026-09-20-midi-controllers-derivations.md` §2b (§1 below)
and records the verification that the record formula fails (§3).

All addresses are `SBPRO2.MDI` file offsets (`file_offset == VA`; the image has no
load segment). Disassembly is `capstone` `Cs(CS_ARCH_X86, CS_MODE_16)`; the
capture is read through `tools/opl_trace.py`.

---

## 1. Resolution of the `<<2` vs `<<10` contradiction

The prior task's report (§2, lines 62–71) claimed the loader's two `shl ax,1`
were `<< 2`, because `d1 e0 d1 e0` shifts `ax` left twice. That reading forgot
**which byte holds the field**. The loader puts the 6-bit attenuation in `AH`,
not `AL`:

```
0x3932  b0 00        mov al, 0          ; AL = 0
0x3934  8a 65 04     mov ah, [di+4]     ; AH = p[4]
0x3937  8a d4        mov dl, ah
0x3939  80 e2 c0     and dl, 0xc0
0x393c  88 94 89 15  mov [si+0x1589], dl   ; cached KSL = p[4] & 0xc0
0x3940  f6 d4        not ah             ; AH = ~p[4]
0x3942  80 e4 3f     and ah, 0x3f       ; AH = (~p[4]) & 0x3f
0x3945  d1 e0        shl ax, 1          ; AX = ((~p[4]) & 0x3f) << 9
0x3947  d1 e0        shl ax, 1          ; AX = ((~p[4]) & 0x3f) << 10
0x3949  89 87 2d 17  mov [bx+0x172d], ax ; 16-bit field at 2*v + 0x172d
```

Because the field sits in bits 8–13 (`AH`), each `shl ax,1` moves it **one bit
further up**, so the two shifts compose to `<< 10`, not `<< 2`. The op2 loader is
byte-for-byte identical (`0x394d`–`0x3964`, `mov [bx+0x1705], ax`).

The TL branch then recovers the field with `>> 10`:

```
0x346a  8b 5e e2     mov bx, [bp-0x1e]  ; bx = word at 2*v + 0x172d = F << 10
0x346d  d1 eb        shr bx, 1
0x346f  d1 eb        shr bx, 1          ; bx = F << 8
0x3471  8a df        mov bl, bh         ; bl = bh = F   (F = (~p) & 0x3f)
```

Ghidra agrees with this reading: `SBPRO2.MDI_decompiled.c:1178` renders the store
as `*(uint *)((undefined *)&DAT_0000_172d + iVar4) = (uint)(byte)~bVar2 << 10;`
and `:1181` the op2 store identically. (`:868` reads the field back as
`high_byte(word) >> 2`, which is the same value: `((F<<10) >> 8) >> 2 = F`.)

**So there is no contradiction.** The loader and branch are consistent at `<<10` /
`>>10`, and the record's `field = ((~p)&0x3f) << 10`, `att = field >> 10 =
(~p)&0x3f` stands. The prior task's `<<2` reading was a byte/word error; its
"degenerate branch" worry does not arise. `[v+0x172d]`/`[v+0x1705]` are written
**only** by the loader (`0x3949`/`0x3964`; byte-pattern search for `2d 17`/`05 17`
finds only `0x394b`/`0x3966` as stores and `0x339d`/`0x33a4` as loads), so nothing
else perturbs the field.

---

## 2. The TL carrier mechanism, instruction by instruction

### 2a. Channel level `local_a` — `0x319a`–`0x31c4`

Entered only when the TL family bit (`0x40`) is set (`0x319a test [si+0x1539],0x40`):

```
0x31a1  mov di, [si+0x14c1]   ; voice's MIDI channel
0x31a5  and di, 0xf
0x31a8  mov al, [di+0x1909]   ; volume
0x31ac  mul byte [di+0x1949]  ; AX = volume * expression
0x31b0  shl ax, 1
0x31b2  mov al, ah            ; AL = (volume*expression*2) >> 8
0x31b4  cmp al, 1
0x31b6  sbb al, 0xff          ; g(AL) = AL + (AL != 0)
0x31b8  mul byte [si+0x1511]  ; AX = g(...) * vlevel
0x31bc  shl ax, 1
0x31be  mov al, ah            ; AL = (g(...)*vlevel*2) >> 8
0x31c0  cmp al, 1
0x31c2  sbb al, 0xff          ; g(AL)
0x31c4  mov [bp-8], al        ; local_a
```

with `g(x) = x + (x != 0)` (the `cmp al,1; sbb al,0xff` idiom), `[di+0x1909]` =
volume, `[di+0x1949]` = expression (both stored verbatim by the controller
handler `0x3b54`, `mov byte cs:[bx+di], cl` at `0x3c50`), and `[v+0x1511]` = the
key-on velocity level, set at `0x3ac5`–`0x3ad3` from `[0xc27 + (velocity>>3)]`
(the 16-byte table at `0xc27` = `52 55 58 5b 5e 61 64 67 6a 6d 70 73 76 79 7c 7f`).
At full volume/expression/velocity this is `local_a = 127`.

### 2b. Per-operator 0x40 byte — `0x346a`–`0x349d` (op1), `0x34a0`–`0x34d0` (op2)

```
0x346a  mov bx, [bp-0x1e]     ; field = F << 10, F = (~p)&0x3f
0x346d  shr bx, 1
0x346f  shr bx, 1
0x3471  mov bl, bh            ; bl = F
0x3473  test byte [bp-0x14], 1 ; gate bit 0 = op1/modulator
0x3477  je 0x3484
0x3479  mov cl, 0x7f
0x347b  mov al, bh            ; al = F
0x347d  mul byte [bp-8]       ; AX = F * local_a
0x3480  div cl                ; AL = (F * local_a) / 0x7f
0x3482  mov bl, al            ; bl = scaled attenuation
0x3484  not bl
0x3486  and bl, 0x3f
0x3489  or bl, [bp-0x22]      ; | cached KSL (= p & 0xc0)
0x348c.. call 0x2aa4(target, 0x40, bx)
```

Op2 is identical (`0x34a0`–`0x34d0`): field `[bp-0x20]`, gate bit 1 (`test
[bp-0x14],2` at `0x34a9`), cached KSL `[bp-0x24]`. The writer `FUN_0000_2aa4`
(`0x2aa4`) uses only the **low byte** (`mov cl,[bp+8]` at `0x2ab9`; the pushed
`ax` is overwritten at `0x348d`–`0x3491`), so the register value is
`bl = ((~att) & 0x3f) | (p & 0xc0)`.

The gate byte `[bp-0x14]` is `[v+0x1629]` for a normal voice (`0x33e8`), which the
loader sets to `[v+0x1575] | 2` with `[v+0x1575] = p[8] & 1` (`0x39b8`–`0x39be`).
Bit 1 is therefore always set — the **carrier is always gated**; bit 0 gates the
modulator by `p[8] & 1`. (Type-3 voices use `[v+0x18e5]` from the 4-byte table
`0xd2b`, `0x3855`–`0x3861`; every shipped payload opens `0x000e`, so no type-3
voice occurs in the compared window.)

### 2c. The formula (the Task-1 record's, confirmed)

```
F   = (~p) & 0x3f
att = F                       if the operator's gate bit is clear
att = F * local_a / 0x7f      if the operator's gate bit is set   (unsigned div)
TL  = ((~att) & 0x3f) | (p & 0xc0)
```

This is exactly `2026-09-20-midi-controllers-derivations.md` §2b. The only
correction is §1: the field really is `F << 10`, so the record's `field >> 10`
reading is right (and the prior task's `<<2` is wrong).

---

## 3. Verification against the capture — the formula fails

Method. The port's write stream is produced by an instrumented copy of the
committed Python oracle (`tools/opl_seq.py`, which is byte-exact with the C
sequencer), logging `(tick, reg, patch_byte, level, vol, expr, vlevel, gated)` for
every TL write. The capture is `data/audio-captures/prage_000.dro` through
`tools/opl_trace.py`. For each port TL write the capture's value for that register
at the same port tick is the last capture write to that register at or before the
tick (`ms -> tick = (ms*120 + 500)/1000 + 60`).

Alignment check. The **non-gated** modulator path is `TL = p[4]` verbatim; over
the aligned window (`tick < 1010`, before the known frequency divergence at C
write 362) it reproduces **119 of 119** non-gated modulator writes exactly, so the
voice/patch/tick pairing is sound.

Record formula. Over all gated carrier-TL writes (KSL matching):

```
record formula over gated carrier rows (KSL-matched): 0/797 exact
```

Representative mismatches (tick, `p[10]`, `local_a`, capture, record):

| tick | p[10] | local_a | capture | record |
|---|---|---|---|---|
| 60 | 0x00 | 127 | 0x16 | 0x00 |
| 68 | 0x00 | 127 | 0x16 | 0x00 |
| 75 | 0x00 | 127 | 0x16 | 0x00 |
| 255 | 0x00 | 124 | 0x17 | 0x02 |
| 734 | 0x83 | 48 | 0xb1 | 0xa9 |
| 734 | 0x00 | 45 | 0x32 | 0x29 |

The task's index-5 case is row `tick=68`: the percussion payload
`0e 00 33 00 00 f6 0c 00 04 00 00 f6 06 00` has `p[10]=0`, `p[8]=4` (modulator
ungated, carrier gated), full `local_a=127`, so the binary computes
`att = 63*127/0x7f = 63`, `TL = (~63)&0x3f = 0x00`. The capture writes `0x16`.
This is real, not an alignment artefact: the neighbouring writes of the same
apply (`0x41=0`, `0x61=0xf6`, `0x64=0xf6`, `0x81=0x0c`, `0x84=0x06`, `0xc1=0x34`,
`0xa1=0x34`, `0xb1=0x2b`) all match the same patch.

No divisor fits. `att = F * local_a / D` was swept for every `D` in `0x40..0x1ff`:

```
best divisor over all rows:      0xc0 (192) -> 634/797 (80%)
best divisor over tick<1010:     0xcc (204) ->  89/141 (63%)
record divisor 0x7f (127):        0/797
```

The best divisor is **not** stable across windows (0xc0 vs 0xcc), and the residual
is structured, not noise: at low `local_a` the capture is consistently one step
*more* attenuated than any single divisor predicts, and at `local_a=127` the
capture's value depends on `p[10]`'s TL bits in a way no divisor explains
(`p[10]=0x00 -> 0x16`, `p[10]=0x83 -> 0x9a`, `p[10]=0x0a -> 0x1c`). A divisor near
`0xc0` is a **fit**, and the brief forbids shipping one.

No level function fits. `att = F * L / 0x7f` was also swept over a family of level
functions `L = g(g((vol*expr)>>a) * vlevel >> b)`, `L = (vol*expr*vlevel) >> c`,
`L = g((vol*expr*vlevel) >> c)` and `L = (vol*expr*vlevel) // d`. The best
candidate reproduces 26 of 162 aligned-window rows; none is close. The capture's
carrier attenuation is roughly `(local_a - 6)/3` for `p[10]=0` (slope ≈ 1/3),
whereas the binary's is `63*local_a/127` (slope ≈ 1/2.02); the two do not share a
scaling law.

---

## 4. Verdict and what would be needed

**`NOT PINNABLE`.** The binary's gated TL branch is unambiguous and fully derived
(§2), and the `<<2`/`<<10` contradiction is resolved in favour of `<<10` (§1). But
that branch, driven by the driver's own volume/expression/velocity state, produces
`0x00` where the capture produces `0x16`, and it matches **0 of 797** carrier-TL
writes. No divisor or level-function variant reproduces the capture without being
a fitted constant, and the residuals are systematic rather than random.

The contradiction is therefore between two ground truths, not a missing input:

* the **disassembly** says `TL = (~(F * local_a / 0x7f)) & 0x3f | (p & 0xc0)`;
* the **capture** says the carrier attenuation at full `local_a`, `p[10]=0`, is
  `0x16` (22), not `0x00`.

What would pin it:

1. **The exact driver build that produced the capture.** The repo's `SBPRO2.MDI`
   TL routine provably cannot be it. If a variant `SBPRO2.MDI` (or another
   dual-OPL2 AIL driver) exists with a different gated branch — e.g. the
   two-division form seen in `SBPRO1.MDI` at `0x3152`–`0x3173` — that branch would
   have to be disassembled and checked against the capture. A second capture made
   with the repo's `SBPRO2.MDI` on a known build would settle whether the capture's
   driver differs.
2. **The engine-supplied TL inputs, if they are not `[0x1909]`/`[0x1949]`/`[0x1511]`.**
   The controller handler stores CC7/CC11 verbatim (`0x3c50`), so under the repo's
   binary there is no free parameter here; but if the capture's driver sources its
   level elsewhere, that source would need to be read from the running driver.
3. **A second, independent capture** of a controlled volume sweep, so the carrier
   `TL(local_a)` curve can be measured without the write-on-change lossiness of the
   DRO. That would distinguish "the binary's linear `F*local_a/0x7f`" from the
   capture's roughly `(local_a-6)/3` curve on clean data.

Until one of those is available, the honest statement is: the binary mechanism is
known, and it is **not** the mechanism the capture exhibits.

---

## 5. Commands run (real output)

```sh
# loader + branch disassembly
python3 - <<'EOF'
from capstone import *
d=open('data/game/C/SBPRO2.MDI','rb').read()
md=Cs(CS_ARCH_X86,CS_MODE_16)
for i in md.disasm(d[0x3932:0x3965],0x3932): print('%#06x %s %s'%(i.address,i.mnemonic,i.op_str))
for i in md.disasm(d[0x346a:0x34d1],0x346a): print('%#06x %s %s'%(i.address,i.mnemonic,i.op_str))
EOF
# -> 0x3945/0x3947 shl ax,1 ; 0x3949 mov [bx+0x172d],ax
# -> 0x346d/0x346f shr bx,1 ; 0x3471 mov bl,bh ; 0x3480 div cl (cl=0x7f)

# field is written only by the loader
python3 - <<'EOF'
d=open('data/game/C/SBPRO2.MDI','rb').read()
for name,p in {'172d':b'\x2d\x17','1705':b'\x05\x17'}.items():
    o=[]; i=d.find(p)
    while i!=-1: o.append(hex(i)); i=d.find(p,i+1)
    print(name,o)
EOF
# -> 172d ['0x339d', '0x394b']  1705 ['0x33a4', '0x3966']

# capture at the task's index-5 voice
python3 tools/opl_trace.py data/audio-captures/prage_000.dro | sed -n '40,60p'
# -> 63 0x0044 0x16 ... 64 0x00c1 0x34 0x00a1 0x34 0x00b1 0x2b
```

The verification harness (built in `/tmp`, not committed) produced §3's numbers:
`record formula over gated carrier rows (KSL-matched): 0/797 exact`,
`modulator verbatim over non-gated rows: 119/119 (tick<1010)`,
`divisor 0xc0: 634/797`, `divisor 0xcc: 89/141 (tick<1010)`, `divisor 0x7f: 0/797`.

Ghidra references: `SBPRO2.MDI_decompiled.c:1178` (`<< 10`), `:868`/`:894`–`:900`
(the TL branch).
