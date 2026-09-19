# SBPRO2.MDI note -> fnum: melodic and percussion

Derivation for Task 4 of the OPL driver-fidelity cycle (spec divergence #6,
"Percussion note frequency"). All addresses are image offsets
(`code_origin = 0x132`). Evidence: `tools/mdi_disasm.py data/game/C/SBPRO2.MDI`,
`tools/opl_seq.py --capture-anchors`, and the capture oracle in
`port/tests/test_sequencer.c`.

**Outcome.** The driver has one note-on frequency routine; the difference
between melodic and percussion is which byte feeds it. The port's 12-TET
`NOTE_TAB` is the melodic result; a percussion note must index that same table
by its 0x7F-bank patch's **base byte** ([2]), not by the MIDI note. Implementing
that moved the capture oracle's first difference from C write 14 (`0xB0`
`0x2A` vs `0x2B`) to C write 16 (channel reuse, divergence #7).

## 1. The note-on frequency routine `0x35fa`-`0x36a6`

It is the `0x01` (A0/B0) family handler of the note-setup state machine
`0x3184`; `0x35c2` enters the frequency path when the voice flag
`[si+0x1561] & 0x20` is set (`0x35d6`-`0x35db`). With pitch bend centred it
collapses to a table lookup; the pieces:

| addr | what |
|---|---|
| `0x35fa` | `bl = [si+0x14c1]` — the MIDI channel |
| `0x3600` | `al = [bx+0x1939]`, the pitch-wheel MSB (set with the LSB at `0x3bb1`-`0x3bb7` by the `0xE0` handler `0x3b54`) |
| `0x360b`-`0x360f` | `ax = (([ch+0x1939] << 7) | [ch+0x1929]) - 0x2000` — the 14-bit wheel, signed around centre |
| `0x3617`-`0x361d` | `ax = (ax >> 5) * [ch+0x18f9]` (`imul`); `[ch+0x18f9]` is the bend scale, written only by controller 6 at `0x3c93` |
| `0x361f`-`0x362c` | `bx = [si+0x14d5] + sign8([si+0x14fd])` |
| `0x3630`-`0x3644` | normalise `bx` into the pitch-table index domain (two add/sub-`0xc` loops) |
| `0x3646`-`0x364e` | `ax = (ax + (bx & 0xff) << 8 + 8) >> 4` (signed) |
| `0x3650`-`0x3664` | clamp `ax` to `0..0x5ff` |
| `0x3666`-`0x366b` | `idx = ax >> 4` |
| `0x366f`/`0x367a` | `sem = t9dd[idx]` (table `0x9dd` = `idx mod 12`), row = `sem << 5` (32 **bytes** per semitone: `shl di,5`) |
| `0x3681`-`0x3683` | `w = pitch_tbl[16*sem + ((2*ax) & 0x1f)]` (16 **words** per semitone — the same 32 bytes; word table at `0x7fd`) |
| `0x3689`-`0x369b` | `oct = t97d[idx]` (table `0x97d` = `idx / 12`); `block = oct - 1 + (w < 0)`; if `block < 0` then `block++`, `w >>= 1` |
| `0x36a1`-`0x36a6` | `a0 = w & 0xff`; `b0 = (block << 2) | ((w >> 8) & 0x03)` |

The tables are 96-entry (8 octaves x 12) arrays: `t97d` is the octave,
`t9dd` the semitone, and the word table at `0x7fd` is one octave of fnum with
16 fine steps per semitone. The `block = oct - 1` and the negative-`w` fold are
the driver's octave convention; the port's `NOTE_TAB` absorbs them.

Verification: with `[ch+0x18f9] = 0` (or wheel centred) the routine reduces to
`F(norm(B)) = NOTE_TAB[B]` for the index input `B`. Checked against the shipped
table at `B = 47, 54, 79, 84, 100` and re-checked against the capture by
`tools/opl_seq.py --capture-anchors`: note 84 = block 5 fnum `0x2B2`, note 79 =
block 5 fnum `0x205`.

## 2. Where `[si+0x14d5]` / `[si+0x14fd]` come from — the melodic/percussion split

Set in the note-on function `0x3a2f` (`0x3aac`-`0x3ac1`); `di` points at the
selected patch, so `[di+2]` is the patch base byte:

```
0x3aac  mov al, 0
0x3aae  mov cl, byte ptr [di + 2]     ; patch base
0x3ab1  mov bx, word ptr [bp + 4]     ; MIDI channel
0x3ab4  cmp bx, 9
0x3ab7  je  0x3abd                    ; percussion
0x3ab9  mov al, cl                    ; melodic: al = base
0x3abb  mov cl, dl                    ; melodic: cl = note
0x3abd  mov byte ptr [si + 0x14d5], cl
0x3ac1  mov byte ptr [si + 0x14fd], al
```

* **melodic** (`channel != 9`): `[14d5] = note`, `[14fd] = base`, so the table
  index is `norm(note + base)`, i.e. `NOTE_TAB[note + base]`.
* **percussion** (`channel == 9`): `[14d5] = base`, `[14fd] = 0`, so the index
  is `norm(base)`, i.e. `NOTE_TAB[base]` — the MIDI note does not enter the
  frequency at all.

`[si+0x1499]` is the voice type: the percussion loader `0x381d` sets it to 3 at
`0x383b` (melodic loader `0x38f1` sets 0 at `0x390b`); it selects the `0x1499 == 2`
alternate at `0x35cb`-`0x35d3` but not the melodic/percussion base handling.

## 3. The shipped bank pins the split

`data/game/C/FAT.OPL` (`tools/opl_seq.load_patches`):

* all **128** melodic entries (bank 0) have payload `[2] = 0`, so melodic is
  exactly the port's `NOTE_TAB[note]`;
* the percussion entry for keyed note 47 is `0x7F2F`, whose payload `[2] = 54`
  (`0x36`), so `NOTE_TAB[54] = (block 2, fnum 0x3CF)` -> `0xA0 = 0xCF`,
  `0xB0 = 0x2B`, matching the capture's first key-on.

The driver's index/table clamp to 96 entries is not reached by the shipped bank
(percussion bases range 0x23..0x57 = 35..87); the port bounds the table index to
`0..127` for safety only.

## 4. Implemented functions (executable expressions)

```c
/* melodic */
(block, fnum) = NOTE_TAB[note];                       /* base == 0 for every melodic FAT.OPL entry */

/* percussion (midi == 9) */
const u8 *p  = patches_lookup(PATCH_KEY(0x7F, note));
(block, fnum) = NOTE_TAB[p ? p[2] : note];            /* p[2] = the patch base byte */
```

Mirrored in `tools/opl_seq.py` `Sequencer.key_on` so the governing C-vs-Python
byte gate stays exact.

## 5. Result

* Before: `capture oracle first difference at C write 14: C tick=60 reg=0xb0
  val=0x2a vs capture tick=60 reg=0xb0 val=0x2b (C 9340 writes, capture 6380
  normalised)`.
* After: `capture oracle first difference at C write 16: C tick=68 reg=0x20
  val=0000 vs capture tick=68 reg=0x21 val=0000 (C 9340 writes, capture 6380
  normalised)`.
* `oracle C-vs-Python: 9340 writes byte-exact`; `all checks passed`.

The new first difference is the next real divergence: the port reuses OPL ch0
for its second note (`0x20`) where the capture rotates to ch1 (`0x21`) —
spec divergence #5/#7 (channel allocation), not this task.

## 6. Gaps (not fitted)

* `[ch+0x18f9]` (bend scale, controller 6) and the pitch-wheel path are not
  modelled by the port; the capture window carries no bend, so they do not
  affect the compared stream. `TODO(verify)` if a later capture exercises bend.
* The index clamp is stated as the driver's 96-entry domain, not reproduced
  exactly (unreachable for the shipped bank).
