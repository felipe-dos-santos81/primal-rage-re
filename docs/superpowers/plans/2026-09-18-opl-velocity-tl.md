# Derivation: `SBPRO2.MDI`'s velocity/volume → carrier-TL function

Plan: `docs/superpowers/plans/2026-09-18-opl-driver.md` (Task 2).
Ground truth: the driver's own instructions in `data/game/C/SBPRO2.MDI` plus the
capture `data/audio-captures/prage_000.dro`. `tools/opl_seq.py` and the C
sequencer were both wrong the same way before this task; neither is evidence.

## 1. The derived function

The driver writes the carrier operator's `0x40`-family byte as:

```
base  = 0x3f - (p[10] & 0x3f)                 ; p[10] = FAT.OPL carrier KSL/TL
V     = scale7( scale7(cc7, cc11), VELCURVE[velocity >> 3] )
scal  = (base * V) / 0x7f                     ; integer division
carrier_0x40 = ((~scal) & 0x3f) | (p[10] & 0xc0)
```

where

```
scale7(a,b):                                   ; driver 0x31a8-0x31c4
    al = ((a * b) << 1) >> 8                   ; keep bits 7..14, in AL
    return al == 0 ? 0 : al + 1

VELCURVE = { 0x52,0x55,0x58,0x5b,0x5e,0x61,0x64,0x67,   ; driver 0xc27
             0x6a,0x6d,0x70,0x73,0x76,0x79,0x7c,0x7f }
```

`cc7` / `cc11` are the driver's per-MIDI-channel controller-7 (volume) and
controller-11 (expression) state; the shipped title holds both at their maximum
after `t=14`, so the driver's own product is `scale7(scale7(0x7f,0x7f),t)`. The
sequencer models no controllers, so it substitutes the title's **effective
volume product `0x53`** measured from the capture (below): `scale7(0x53, t)`.

`VELCURVE` is the driver's 16-entry table selected by `velocity >> 3`; the
modulator `0x40` byte is written verbatim (`p[4]`) because its
velocity-sensitivity bit is clear in the shipped bank.

## 2. Driver evidence (addresses are image offsets; `code_origin = 0x0132`)

**Velocity curve** — note-on, `0x3a2f`, 0x3ac5-0x3ad3:

```
0x3ac5  mov ax, word ptr [bp + 8]     ; velocity (3rd event arg)
0x3ac8  shr al, 1
0x3aca  shr al, 1
0x3acc  shr al, 1                     ; velocity >> 3
0x3ace  mov bx, 0xc27
0x3ad1  xlatb                         ; al = 0xc27[velocity >> 3]
0x3ad3  mov byte ptr [si + 0x1511], al
```

Table `0xc27` (16 bytes): `52 55 58 5b 5e 61 64 67 6a 6d 70 73 76 79 7c 7f`.

**Volume product** — function `0x3184`, 0x31a1-0x31c4 (the `0x31a8` multiply is
`[di+0x1909]` = controller 7, handler 0x3c1c/0x3c50; `0x31ac` is `[di+0x1949]`
= controller 11, handler 0x3c24/0x3c50):

```
0x31a1  mov di, word ptr [si + 0x14c1]
0x31a5  and di, 0xf                   ; MIDI channel
0x31a8  mov al, byte ptr [di + 0x1909]; CC7
0x31ac  mul byte ptr [di + 0x1949]    ; * CC11
0x31b0  shl ax, 1
0x31b2  mov al, ah
0x31b4  cmp al, 1
0x31b6  sbb al, 0xff                  ; +1 if nonzero
0x31b8  mul byte ptr [si + 0x1511]    ; * VELCURVE[velocity >> 3]
0x31bc  shl ax, 1
0x31be  mov al, ah
0x31c0  cmp al, 1
0x31c2  sbb al, 0xff
0x31c4  mov byte ptr [bp - 8], al     ; V
```

**Carrier base stored at patch load** — function `0x38f1`, 0x394d-0x3964 (the
modulator twin is 0x3932-0x3949, storing to `[bx+0x172d]`):

```
0x394d  mov al, 0
0x394f  mov ah, byte ptr [di + 0xa]   ; FAT.OPL payload[10]
0x3952  mov dl, ah
0x3954  and dl, 0xc0
0x3957  mov byte ptr [si + 0x159d], dl ; KSL bits
0x395b  not ah
0x395d  and ah, 0x3f                  ; 0x3f - (p[10] & 0x3f)
0x3960  shl ax, 1
0x3962  shl ax, 1                     ; <base> << 10
0x3964  mov word ptr [bx + 0x1705], ax
```

**Carrier `0x40` byte** — function `0x3184`, 0x34a0-0x34d3 (shared write helper
`0x2aa4` maps operator index to slot and emits `0x40 + slot`):

```
0x34a0  mov bx, word ptr [bp - 0x20]  ; <base> << 10
0x34a3  shr bx, 1
0x34a5  shr bx, 1
0x34a7  mov bl, bh                    ; bl = base
0x34a9  test byte ptr [bp - 0x14], 2  ; carrier velocity-sensitive?
0x34ad  je 0x34ba
0x34af  mov cl, 0x7f
0x34b1  mov al, bh
0x34b3  mul byte ptr [bp - 8]         ; base * V
0x34b6  div cl                        ; / 0x7f
0x34b8  mov bl, al
0x34ba  not bl
0x34bc  and bl, 0x3f                  ; 0x3f - scal
0x34bf  or bl, byte ptr [bp - 0x24]   ; | KSL
0x34cd  push word ptr [bp - 6]        ; operator index
0x34d0  call 0x2aa4                   ; write 0x40 + slot
```

The modulator uses `[bp-0x1e]` and tests bit 0 (0x3473) instead of bit 1; in the
shipped bank that bit is clear, so `p[4]` is written verbatim and the capture
agrees.

## 3. Evaluation against every captured case

The `0x49`/`0x1e`/`0x58`/`0x24`/`0x74` rows use the effective volume `0x53`;
`0x34` is evaluated both ways.

| patch | `[10]` | velocity | captured | derived (V=0x53) | fit |
|---|---|---|---|---|---|
| `0x49` | `0x00` | 127 | `0x16` | `0x16` | yes |
| `0x49` | `0x00` | 122 | `0x16` | `0x16` | yes |
| `0x49` | `0x00` | 113 | `0x17` | `0x17` | yes |
| `0x49` | `0x00` | 104 | `0x18` | `0x18` | yes |
| `0x1e` | `0x00` | 127 | `0x16` | `0x16` | yes |
| `0x58` | `0x00` | 127 | `0x16` | `0x16` | yes |
| `0x24` | `0x40` | 120, 127 | `0x56` | `0x56` | yes |
| `0x34` | `0x83` | 127 | `0x9a` | `0x98` | **NO** |
| `0x74` | `0x03` | 115 | `0x19` | `0x19` | yes |
| `0x74` | `0x03` | 126, 127 | `0x18` | `0x18` | yes |

`0x34` is the one row that does not fit. The derivation doc records it as an
open mismatch rather than fitting a term to it:

* the code path above gives the same low 6 bits for `0x34` and `0x74` (both
  `p[10] & 0x3f == 3`, so both `base == 60`); the capture differs by 2
  (`0x1a` vs `0x18`).
* the only differing patch operand is the KSL field (`0x80` vs `0x00`), and the
  driver only `or`s that field into bits 6-7 at 0x34bf — it cannot change the
  low 6 bits.
* the `0x9a` is **consistent across every captured `0x34` instance** (MIDI
  channels 2 and 4, and OPL channels 0,1,2,5,6,7,8), so it is not a channel
  volume: those channels' other patches fit with `0x53`.
* reproducing `0x9a` needs either `base == 58` (as if `p[10] == 0x85`) or an
  effective volume of `~0x4f`. Neither is derivable from the instruction stream
  read here, and the other four bytes of the `0x34` payload match `FAT.OPL`
  exactly.

Per the plan's Task 2 Step 2 rule, the mismatch is recorded and the term is not
fitted. The port transcribes the driver function above; the `0x34` residual is
named in the report as unresolved.

## 4. Capture oracle before/after

Both before and after (this task changes a value, not the write order, and the
carrier-TL registers are in `documented_excluded`):

```
capture oracle first difference at C write 2: C tick=60 reg=0x20 val=0000 vs capture tick=60 reg=0xb0 val=0x2b (C 9340 writes, capture 6380 normalised)
```

C/capture write counts: 9340 / 6380 before and after (unchanged).

## 5. TDD evidence

**RED** — `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
with only the new test present and `apply_patch` still writing `p[10]`:

```
FAIL port/tests/test_sequencer.c:375: 0 != 22
FAIL port/tests/test_sequencer.c:389: 3 != 24
FAILURES: 2
```

Expected: program `0x49` (p[10]=0) must write carrier TL `0x16` and the port
wrote `0`; program `0x74` (p[10]=3) must write `0x18` and the port wrote `3`.

**GREEN** — same command after implementing `carrier_tl`:

```
oracle C-vs-Python: 9340 writes byte-exact
capture oracle first difference at C write 2: ... (C 9340 writes, capture 6380 normalised)
all checks passed
```

`python3 -m pytest tools/tests/ -q` → 46 passed; `python3 tools/opl_seq.py
--self-test` → ok. No compiler warnings.

## 6. Files changed

* `port/src/platform/audio/sequencer.c` — `carrier_tl`/`drv_scale`/`VEL_CURVE`;
  `apply_patch` takes the note velocity and writes the derived carrier TL.
* `port/tests/test_sequencer.c` — section 4b asserts the `0x49`/`0x74` rows on
  the write seam.
* `tools/opl_seq.py` — the independent Python oracle encodes the same contract,
  so the governing C-vs-Python byte gate stays meaningful (the brief's commit
  list omitted this file; see the report for the deviation).
* `docs/superpowers/plans/2026-09-18-opl-velocity-tl.md` — this doc.

## 7. Self-review and concerns

* The transform is applied at note setup in `sequencer.c`, never in
  `patches.c`, as required.
* `data/`, `tools/opl_trace.py` and the `.dro` captures are untouched.
* The `0x53` constant is the shipped title's effective volume product, not a
  driver constant; if the sequencer ever models CC7/CC11/sequence volume, it
  should replace the constant with the real product.
* The `0x34` row of the spec's table is **not reproduced** (derived `0x98` vs
  captured `0x9a`). This is the material concern; the driver instructions read
  here do not contain a term that produces the difference, so per the plan it is
  reported rather than fitted.
* No audio-fidelity claim is made: the oracle is the write stream.
