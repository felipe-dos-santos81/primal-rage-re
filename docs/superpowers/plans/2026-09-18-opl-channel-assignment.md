# SBPRO2.MDI OPL channel assignment

Derivation for Task 5 (re-scoped) of the OPL driver-fidelity cycle (spec
divergence #7, "successive notes routed differently"). All addresses are image
offsets (`code_origin = 0x132`). Evidence: `tools/mdi_disasm.py
data/game/C/SBPRO2.MDI`, `tools/opl_seq.load_patches data/game/C/FAT.OPL`,
`tools/opl_trace.py data/audio-captures/prage_000.dro`, and the capture oracle
in `port/tests/test_sequencer.c`.

**Outcome.** The driver assigns OPL channels with a **monotonic rotation cursor**
over the 18 OPL3 operator-pair slots: each new note takes the next slot after
the last one, wrapping at 18, skipping only slots still owned by a sounding
note. It is *not* "lowest free channel". The port reused the lowest free channel,
so its second note re-entered OPL ch0 where the capture rotates to ch1. The
shipped `FAT.OPL` routes every note (melodic and percussion keys alike) through
the melodic allocator, so the driver's separate 6-slot percussion allocator is
never reached by this game's banks; the port models the melodic one.

## 1. Where a note is started — `0x3085`

`0x3a2f` (the note-on function) allocates a *voice* (a 20-entry voice table,
first free `[si+0x1485]`, `0x3a8f`-`0x3a9e`), stores the MIDI channel/note, then
at `0x3b0d`-`0x3b13` sets `[si+0x14ad] = 0xff` and calls `0x3085`. `0x3085`
assigns the OPL **channel**:

```
0x308e  cmp byte ptr [si + 0x1499], 3
0x3093  je  0x30e1                    ; percussion voice -> separate allocator
0x3095  mov dx, 0xffff
0x3098  mov bx, word ptr [0x1408]     ; rotation cursor (global)
0x309c  inc dx
0x309d  cmp dx, 0x12                  ; 18 slots
0x30a0  je  0x30d9                    ; none free -> 0x36f6 (steal)
0x30a2  inc bx
0x30a3  cmp bx, 0x12
0x30a8  mov bx, 0                      ; wrap at 18
0x30ab  mov word ptr [0x1408], bx     ; store cursor
0x30af  cmp byte ptr [bx + 0x1a49], 0xff   ; slot owner == 0xff -> free
0x30b4  jne 0x309c
0x30b6  mov byte ptr [si + 0x14ad], bl      ; voice's OPL channel = bl
0x30bc  mov bl, byte ptr [si + 0x14c1]      ; MIDI channel
0x30c0  inc byte ptr [bx + 0x1a39]          ; per-MIDI-channel voice count
0x30c4  mov byte ptr [di + 0x1a49], bl      ; slot owner = MIDI channel
```

* `[0x1408]` is the **rotation cursor**, init `0xffff` at reset (`0x306e`); it is
  written only here (and at init), never when a voice is freed.
* `[0x1a49 + ch]` (18 bytes, `0x3046`-`0x304f`) is the **slot owner**: the MIDI
  channel currently using OPL slot `ch`, or `0xff` when free.
* The scan starts at `cursor`, advances (wrapping at 18) and takes the first
  free slot. So for back-to-back notes the channel advances 0,1,2,...,17 and
  only reuses a slot after a full wrap or when a slot is still owned.
* The cursor is stored on **every** iteration, so it lands on the allocated
  slot, not just the last attempt.

## 2. Freeing — `0x3127`

The note-release/key-off helper `0x3127` runs the note-setup state machine to
clear the key bit, then:

```
0x3152  mov bl, [si+0x14ad]           ; the voice's OPL channel
0x3156  cmp byte ptr [si+0x1499], 3
0x315b  jne 0x3162
0x315d  mov byte ptr [bx+0x1a4c], 0xff ; percussion: release the paired slot
0x3162  mov byte ptr [bx+0x1a49], 0xff ; free the slot
0x3167  mov byte ptr [si+0x14ad], 0xff
```

The **cursor `[0x1408]` is not touched**, so freeing a low channel does not pull
the next allocation back to it — the rotation is monotonic.

## 3. Percussion allocator `0x30e1` (derived, unreachable for this game)

`[si+0x1499] == 3` selects it. It cycles the cursor `[0x140a]` through the
6-entry OPL channel table at `0xd21` = `{0, 1, 2, 9, 0x0a, 0x0b}` (channels 0-2
of each OPL3 bank), requiring both `[bx+0x1a49]` and `[bx+0x1a4c]` free, and
sets both owners (`0x310f`-0x3121`). Voice type 3 is set by the **percussion
patch loader** `0x381d` (`0x383b`); the **melodic loader** `0x38f1` sets type 0
(`0x390b`). Which loader runs is chosen at `0x3aed`-`0x3b0b` from the patch's
type word `[di]`: `0x19` -> percussion loader, `0x0e` -> melodic loader.

`tools/opl_seq.load_patches(data/game/C/FAT.OPL)` shows all **181** payloads
(128 melodic + 53 percussion keys) have type byte `[0] = 0x0e`. The driver
therefore always runs the melodic loader and the melodic allocator; the
percussion allocator is dead for this bank and is not modelled. This also means
MIDI channel 9 notes (percussion *keys*) still rotate like melodic channels — as
the capture shows (its first four key-ons, all MIDI ch9 percussion keys, are OPL
ch0,1,2,3).

## 4. Channel -> register mapping

Two indirection tables turn a slot index into a register number
(`0xc37`/`0xc49` -> `0xc5b`/`0xc7f` for operator families; `0xca3`/`0xcb5` for
the channel families `0xC0`/`0xA0`/`0xB0`):

| slot | bank | modulator reg offset | carrier reg offset |
|---|---|---|---|
| 0 | 0 | 0x00 | 0x03 |
| 1 | 0 | 0x01 | 0x04 |
| 2 | 0 | 0x02 | 0x05 |
| 3 | 0 | 0x08 | 0x0b |
| 4 | 0 | 0x09 | 0x0c |
| 5 | 0 | 0x0a | 0x0d |
| 6 | 0 | 0x10 | 0x13 |
| 7 | 0 | 0x11 | 0x14 |
| 8 | 0 | 0x12 | 0x15 |
| 9-17 | 1 | same as 0-8, `+0x100` | same as 0-8, `+0x100` |

`0xc37` = `{0,1,2,6,7,8,0x0c,0x0d,0x0e,0x12,0x13,0x14,0x18,0x19,0x1a,0x1e,0x1f,0x20}`
are the modulator *linear operator* indices; `0xc5b` maps a linear operator
index to its register offset (`{0,1,2,3,4,5,8,9,10,11,12,13,16,17,18,19,20,21}`
per bank) and `0xc7f` supplies the bank bit (0 for indices 0-17, 1 for 18-35).
Composed, slot 0-8 -> operator offsets `{0,1,2,8,9,10,16,17,18}` in bank 0, and
slots 9-17 repeat them in bank 1. The `0xC0`/`0xA0`/`0xB0` families use the
channel index within the bank: `0xca3[ch] = ch mod 9`, `0xcb5[ch] = ch / 9`.

The capture confirms it: its tenth note uses `0x120`/`0x1c0`/`0x1a0`/`0x1b0`
(slot 9, bank 1), not a wrapped bank-0 channel.

## 5. Implemented allocator (executable expression)

```c
/* slot ch: bank = ch/9, c = ch%9. Operator regs use OPL_SLOT[ch] (banked
 * operator offset); C0/A0/B0 use ch%9 + 0x100*(ch/9). */
static int alloc_voice(void) {
    int v = S.next;                       /* last allocated slot, or 17 */
    for (int i = 0; i < SEQ_OPL_CHANNELS; i++) {
        v = (v + 1) % SEQ_OPL_CHANNELS;   /* driver 0x30a2-0x30ab */
        if (S.voice[v].note == SEQ_NOTE_FREE) {
            S.next = v;                   /* driver 0x30ab stores the cursor */
            return v;
        }
    }
    /* all 18 busy: the driver steals at 0x36f6; the port keeps its documented
     * oldest-voice policy. */
    ...
}
```

Initial `S.next = SEQ_OPL_CHANNELS - 1` mirrors the driver's `0xffff` cursor
(the first trial wraps to slot 0). `key_off` frees only the voice's slot and
leaves `S.next` untouched (driver `0x3162`), so the rotation is monotonic.

## 6. Result

* Before: `capture oracle first difference at C write 16: C tick=68 reg=0x20
  val=0000 vs capture tick=68 reg=0x21 val=0000 (C 9340 writes, capture 6380
  normalised)`.
* After: see `port/tests/test_sequencer.c`'s output and Task 5's report.

## 7. Gaps (named, not fitted)

* The driver's exhaustion/stealing handler `0x36f6`-`0x3816` (quietest-voice,
  per-MIDI-channel voice counts `[ch+0x1a39]`, per-MIDI-channel priority
  `[ch+0x1979]`) is **not** modelled; the port keeps its oldest-voice steal.
  With 18 slots and this bank's note density the allocator was not observed to
  exhaust in the compared window, so the policy does not affect the stream.
* The percussion allocator (§3) is not modelled because the shipped bank never
  selects it. A bank with type-`0x19` percussion payloads would need it.
