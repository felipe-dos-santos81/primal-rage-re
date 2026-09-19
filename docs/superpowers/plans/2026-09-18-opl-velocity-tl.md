# Derivation: `SBPRO2.MDI`'s velocity/volume → carrier-TL function

Plan: `docs/superpowers/plans/2026-09-18-opl-driver.md` (Task 2).
Ground truth: the driver's instructions in `data/game/C/SBPRO2.MDI`, the
capture `data/audio-captures/prage_000.dro`, and the engine that drives the
driver (`port/decomp/prage.c`, from `RAGE.S16/PRAGE.EXE` on `RAGECD.ISO`).

**Outcome: record-and-stop.** The driver's carrier-TL function is derived and
transcribed below, but its volume input `V` is set by the *engine*, not by the
driver, and the captured `V` value is an engine/config parameter that is not
derivable from the driver. One captured row (`0x34`) is also unexplained. The
fitted constant `0x53` from commit `7bf339a` has been reverted; the port keeps
`p[10]` verbatim. See §5–§7.

## 1. The driver's functions

From `SBPRO2.MDI`'s instructions (all addresses are image offsets,
`code_origin = 0x0132`):

```
base  = 0x3f - (p[10] & 0x3f)                 ; p[10] = FAT.OPL carrier KSL/TL
V     = scale7( scale7(cc7, cc11), VELCURVE[velocity >> 3] )
scal  = (base * V) / 0x7f
carrier_0x40 = ((~scal) & 0x3f) | (p[10] & 0xc0)

scale7(a,b):                                   ; driver 0x31a8-0x31c4
    al = ((a * b) << 1) >> 8
    return al == 0 ? 0 : al + 1

VELCURVE = { 0x52,0x55,0x58,0x5b,0x5e,0x61,0x64,0x67,   ; driver 0xc27
             0x6a,0x6d,0x70,0x73,0x76,0x79,0x7c,0x7f }
```

The modulator `0x40` byte is `p[4]` verbatim in the shipped bank (its
velocity-sensitivity bit is clear; the capture agrees).

## 2. Driver instructions (REQUIRED — feeds Task 4)

**Velocity curve** — note-on `0x3a2f`, 0x3ac5-0x3ad3:

```
0x3ac5  mov ax, word ptr [bp + 8]      ; velocity (event arg)
0x3ac8  shr al, 1
0x3aca  shr al, 1
0x3acc  shr al, 1                      ; velocity >> 3
0x3ace  mov bx, 0xc27
0x3ad1  xlatb                          ; al = 0xc27[velocity >> 3]
0x3ad3  mov byte ptr [si + 0x1511], al
```

**Volume product** — function `0x3184`, 0x31a1-0x31c4; `[di+0x1909]` is
controller 7, `[di+0x1949]` is controller 11 (handlers 0x3c1c / 0x3c24 store
the received values at 0x3c50):

```
0x31a1  mov di, word ptr [si + 0x14c1]
0x31a5  and di, 0xf
0x31a8  mov al, byte ptr [di + 0x1909] ; received CC7
0x31ac  mul byte ptr [di + 0x1949]     ; * received CC11
0x31b0  shl ax, 1
0x31b2  mov al, ah
0x31b4  cmp al, 1
0x31b6  sbb al, 0xff
0x31b8  mul byte ptr [si + 0x1511]     ; * VELCURVE[velocity >> 3]
0x31bc  shl ax, 1
0x31be  mov al, ah
0x31c0  cmp al, 1
0x31c2  sbb al, 0xff
0x31c4  mov byte ptr [bp - 8], al      ; V
```

**Carrier base at patch load** — function `0x38f1`, 0x394d-0x3964 (modulator
twin 0x3932-0x3949):

```
0x394f  mov ah, byte ptr [di + 0xa]    ; FAT.OPL payload[10]
0x3954  and dl, 0xc0                   ; KSL bits
0x3957  mov byte ptr [si + 0x159d], dl
0x395b  not ah
0x395d  and ah, 0x3f                   ; 0x3f - (p[10] & 0x3f)
0x3960  shl ax, 1
0x3962  shl ax, 1                      ; base << 10
0x3964  mov word ptr [bx + 0x1705], ax
```

**Carrier `0x40` byte** — function `0x3184`, 0x34a0-0x34d3:

```
0x34a0  mov bx, word ptr [bp - 0x20]   ; base << 10
0x34a3  shr bx, 1
0x34a5  shr bx, 1
0x34a7  mov bl, bh                     ; bl = base
0x34a9  test byte ptr [bp - 0x14], 2   ; carrier velocity-sensitive?
0x34ad  je 0x34ba
0x34af  mov cl, 0x7f
0x34b1  mov al, bh
0x34b3  mul byte ptr [bp - 8]          ; base * V
0x34b6  div cl                         ; / 0x7f
0x34b8  mov bl, al
0x34ba  not bl
0x34bc  and bl, 0x3f
0x34bf  or bl, byte ptr [bp - 0x24]    ; | KSL
0x34d0  call 0x2aa4                    ; write 0x40 + slot
```

## 3. Where `V` comes from — and why its value is not driver state

The driver has **no master volume**: `V` is exclusively the product of the
*received* CC7, CC11 and the velocity curve. The received CC7 is set by the
engine before it reaches the driver. From `port/decomp/prage.c` (the original
engine):

* controller 7 is scaled by the sequence volume before dispatch —
  `prage.c:49121`:
  `param_4 = (param_1[0xd] * param_4) / 0x7f` (clamped to 0x7f);
* the sequence's `[0xd]` (its current volume) is initialised from
  `DAT_00108d94` — `prage.c:50323`;
* sequence init sends `CC7 = DAT_00108d94` and `CC11 = 0x7f` —
  `prage.c:50090` (`FUN_00068570(seq, ch|0xb0, 7, DAT_00108d94)`);
* the music volume itself is set through AIL_set_sequence_volume from
  `DAT_000a2cb8` (the port mirrors this at `port/src/game/flow.c:570`) and is
  derived at runtime from a packed configuration table (`FUN_0002c8f0` /
  `FUN_0002d974`, music volume = `query(0x35) / 2`).

Reading the shipped image (`RAGE.S16/PRAGE.EXE`, `DS` object at linear
`0x80000`) gives `DS_000A2CB8 = 0x7f` and `DS_00108D94 = 0x0` as file
defaults; the runtime value used by the capture is produced by the engine's
setup/config path. Therefore:

* the driver's `V` is a function of an **engine input**, not of driver state;
* the captured title's effective `V` is `0x53` (measured below), which is the
  game's runtime music-volume setting, not a driver constant.

The earlier commit `7bf339a` hard-coded `0x53`; that is a capture-fitted
constant and has been reverted.

## 4. Re-evaluated captured-case table

Derived with `V` set to the capture's effective value `0x53` (i.e. assuming the
correct engine volume):

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

`0x34` is not explained: the code gives the same low 6 bits for `0x34` and
`0x74` (both `p[10] & 0x3f == 3`), the only differing operand is the KSL field
which the driver only `or`s into bits 6-7, and the `0x9a` recurs for every
captured `0x34` instance across MIDI and OPL channels. Reproducing it needs a
different volume or a term that is not present in the code path read.

## 5. Verification

The reverted tree (this doc + `bd1f08e`'s three source files):

```
cmake --build build --clean-first   # 0 warnings
PR_GAME_DIR=data/game/C ./build/run_tests
  oracle C-vs-Python: 9340 writes byte-exact
  capture oracle first difference at C write 2: C tick=60 reg=0x20 val=0000 vs capture tick=60 reg=0xb0 val=0x2b (C 9340 writes, capture 6380 normalised)
  all checks passed
python3 -m pytest tools/tests/ -q   # 46 passed
python3 tools/gen_symbols.py port/decomp port/src/symbols.h   # byte-identical
```

The capture's first difference remains at write 2 (the Task 3 ordering issue).

## 6. Files changed

* `port/src/platform/audio/sequencer.c` — reverted to `bd1f08e` (`p[10]`
  verbatim; no fitted constant).
* `port/tests/test_sequencer.c` — reverted to `bd1f08e` (no capture-value
  assertions for a constant that is not shipped).
* `tools/opl_seq.py` — reverted to `bd1f08e`.
* this doc.

## 7. Concerns / disposition

1. **Task 2's DoD is not met.** The driver's carrier TL is a function of the
   *engine's* music volume, which the port does not model in the sequencer.
   The captured values cannot be reproduced from a driver-derived function
   alone. Per the plan's Step 2 rule the mismatch is recorded and no term is
   fitted.
2. **`0x34` remains unexplained** by the code path read.
3. **What would unblock it:** model the engine's CC7 scaling
   (`(seq[0xd] * cc7) / 0x7f`) and thread the real sequence volume into the
   sequencer, then re-derive `0x34` with that input. That is engine work, not
   Task 2's driver derivation.
4. No audio/audibility claim is made; the oracle is the write stream.
