# Driver Truth — derivation record

**Design:** `docs/superpowers/specs/2026-10-31-driver-truth-design.md`.
**Plan:** `docs/superpowers/plans/2026-10-31-driver-truth.md`.
**Register:** `docs/superpowers/specs/2026-10-30-final-claims.md` (R10).

Date: 2026-10-31. Tools: the checked-in `port/decomp/prage.c` (Ghidra
decompilation) and the shipped `data/game/C/SBPRO2.MDI` disassembled with
capstone (16-bit; file offset == address, 16,541 bytes). The live Ghidra MCP
instance answered `list_instances`/`list_tool_groups`/`search_tools`, but its
analysis tools are not bridged into the Code Mode catalog, so the decomp and
the checked-in driver binary carried the derivation.

## D1. The original's start routine resets the sequence record only

`AIL_start_sequence` is `0x5DE79` → `0x6A780` → `0x69250`.
`FUN_0006a780` (`prage.c:50399`) is:

```
if (seq != 0 && seq[1] != 1) {
    FUN_0005de94(seq);        /* AIL_stop_sequence -> 0x6A7C0 */
    FUN_00069250(seq);        /* the sequence-record reset */
    seq[1] = 4;               /* playing */
    seq[5] = seq[4] + 8;      /* the stream pointer */
}
```

`FUN_00069250` (`prage.c:49385`, size 165) writes:

- `seq + 0x90 + ch*4 = ch` for `ch` 0..15 — the per-MIDI-channel **track map**
  (`0x68AB0` reads `param_1[ch + 0x24]`, i.e. `+0x90`, as the track base), not a
  program cache;
- `seq + 0x7c + i*4 = -1` (4 dwords), `seq + 0x510 + i*4 = -1` (32 dwords) then
  `seq + 0x510 = 0` (the CC7 log and the note queue);
- the counters `+0x2c`, `+0x58`, `+0x5c`, `+0x68`, `+0x64`, `+0x60`, `+0x6c`,
  `+0x30`, `+0x28`;
- `FUN_00061a70` → `FUN_00065490`, a memset (not a driver-state reset).

It writes **none** of the driver's per-MIDI-channel state. The driver install
(`SBPRO2.MDI 0x3000`-`0x3084`) is where that state is initialised:

```
0x300c  mov byte [di+0x133b], 0        ; the 0xc0-byte patch flags
0x301b  mov byte [di+0x1989], 0xff     ; ch -> patch byte (none)
0x3020  mov byte [di+0x1a39], 0        ; ch -> ? (install 0)
0x3025  mov byte [di+0x19a9], 0xff     ; ch -> program (none)
0x302a  mov byte [di+0x1999], 0        ; ch -> bank (CC114)
0x3038  mov byte [di+0x1485], 0        ; 0x14 voice records: inactive
0x3046  mov byte [di+0x1a49], 0xff     ; 0x12 voice slots: free
0x305b  rep stosb es:[0x19b9], 0xff    ; the 0x80 percussion patch cache
0x306e  mov word [0x1408], 0xffff      ; the rotation cursor
0x3074  mov word [0x140a], 0xffff      ; the second cursor
```

The controller arrays the event handlers write (record
`2026-09-20-midi-controllers-derivations.md` §1) live at `0x18f9` (bend scale),
`0x1909` (CC7), `0x1919` (CC10), `0x1929`/`0x1939` (wheel LSB/MSB), `0x1949`
(CC11), `0x1959` (CC1), `0x1969` (CC64) and persist across a start. The
program change (`0x3bed`) stores `[ch+0x19a9] = program` and
`[ch+0x1989] = 0x2ca6(([ch+0x1999] << 8) | program)`; both arrays are driver
state, not sequence state.

**The model:** initialise the per-channel driver state once, at install; let
it persist across `seq_load`/`seq_start`; reset only the sequence record (the
voice array, `svc`, the CC7 log, the stream position) at start.

## D2. Correction: a missing patch does not drop the note

The design's investigation had a second hypothesis (from the earlier
read-only probe): `SBPRO2.MDI 0x3a61` would drop a note whose patch byte is
`0xff`, and `gp-u10-ending` section 5's tick-5 row would close. The
disassembly does show `0x3a61: cmp bl,0xff; jne 0x3a69; jmp 0x3b19` (the
return), with the free-voice search at `0x3a8c` **after** it.

But the measured allocation contradicts the drop. With the drop implemented,
`gp-u18-talon-ending` sections 7–9 lost their anchors: the port's first key-on
of section 7 moved from OPL channel `0x13` to `0x10`. The shipped code (which
keys the note) matches the capture's `0x13`, and the three `0x7F0B` misses are
exactly the three voice slots (the capture's key-on sits three slots later).
The raw therefore consumed a voice for the note; its `0x2ca6(0x7f0b)` lookup
resolves a key the port's FAT.OPL parse does not. The port keeps the note
(frequency family only, no operator payload), which is what the capture's
allocation shows; the missing operator payload is a measured byte difference.
The drop was reverted. Record §D3's `gp-u10-ending` section 5 row therefore
stays a voice-allocation difference, not a dropped note.

## D3. The experiment and the measured effect

The throwaway experiment (per-channel reset once instead of every start) moved
`gp-u12-endurance` section 3 (`s16sound.gra+0x3E64`, bank `3D536DDE`) from
`FIRST DIFFERENCE at tick 180` to a **608-tick whole-section match**. The
differing group was `FAT.OPL` program `0x0C`'s patch (capture
`0x20=0x18, 0x40=0x23, 0x60=0xf6, 0xC0=0x30`) against the port's program 0
(`0x20=0x01, 0x40=0x8f, 0x60=0xf2`, no `0xC0`) — program 12, set on MIDI
channel 3 by `selmo+0x25C` in the earlier section.

The shipped implementation (`driver_install_state` at first load; `seq_start`
leaves the per-channel arrays alone; `seq_driver_reset` restores the install
state for unit oracles) measures, over the 29-trace oracle:

<!-- MEASURED-TABLE -->

| bank | resource | old residue row | new residue row | measured effect |
|---|---|---|---|---|
| `20DA829D` | `s16citys.gra+0x08` | `gp-twop` 3, tick 0 (unanchored) | `gp-u10-ending` 6, tick 211 | `gp-twop` 3 now a **647-tick whole-section match**; `gp-u16-twop2` 3/6 **5,294**; `gp-u23` 3 **5,294** |
| `8FE546D4` | `s16conti.gra+0x08` | `gp-idle-loss` 9, 960 | unchanged | tail-lag, unchanged |
| `15B3F30E` | `s16hghsc.gra+0x08` | `gp-u10-ending` 13, 1,437 | unchanged | tail-lag, unchanged |
| `BF1C78E2` | `s16himal.gra+0x08` | `gp-idle-loss` 3, 30 | `gp-u22-diablo-ending` 3, 200 | `gp-idle-loss` 3 **2,538-tick match**, 6 **3,440**; `gp-u6-moves` 3 **2,660**; `gp-u15-join` 3 **364** |
| `B40FF4A6` | `s16sound.gra+0x335E` | `gp-u10-ending` 12, 120 | `gp-u10-ending` 12, **1,634** | first difference moved 1,514 ticks later |
| `19AB8B2B` | `s16sound.gra+0x39F4` | `gp-idle-loss-run2` 10, 303 | `gp-idle-loss` 10, 301 | tail-lag, unchanged |
| `3D536DDE` | `s16sound.gra+0x3E64` | `gp-u12-endurance` 3, 180 | `gp-u19-vertigo-ending` 6, 234 | `gp-u12` 3 **608-tick match**, `gp-u19` 3 **272** |
| `3BA4C376` | `s16sound.gra+0x5B88` | `gp-idle-loss` 4, tick 0 (unanchored) | `gp-idle-loss-run2` 4, 253 | now anchors and diffs (the section-boundary cursor); `gp-u10` 4 at 483 |
| `E3533961` | `s16sound.gra+0x6CA8` | `gp-u10-ending` 5, 5 | `gp-idle-loss` 5, 5 | `gp-u10` 5 **191-tick match**, as do the u16/u18/u20/u21/u23 sections |
| `1895A942` | `s16sound.gra+0x72D4` | `gp-u23-u16-finisher` 8, 177 | unchanged | the release/retrigger row, unchanged |

**Not fixed.** The `1895A942` (`sound+0x72D4`) release/retrigger row is
unchanged at tick 177. The design predicted it from the port's
release-before-events order, but the driver's note queue (`0x69372` decrements
`[v+0x185]` before the tick's events) and the port's `seq_tick` agree on that
order, so the divergence is in the retrigger path itself and was not derived
this phase. It stays a residue row, not a claimed fix.

**Counters.** The 12 whole-section-claimed banks are unchanged; the 10 residue
rows are re-pinned at first-diff ticks **5..1,634**. The diff-allowed prefix
claim grows from **74 sections / 29,656 ticks** to **84 sections / 44,103
ticks** (the sections that used to be unanchored now anchor and match a
prefix). `TRACE_PINS`' whole-section tick minimums are unchanged except where
a scenario's total rose (a minimum, so no re-pin was needed), and four
scenarios gained the `3BA4C376`/`20DA829D`/`BF1C78E2` diff allowance their
sections now need (`gp-idle-loss`, `gp-idle-loss-run2`, `gp-u10-ending`,
`gp-u13-ending2`, `gp-u16-twop2`, the five ending traces, `gp-u23`,
`gp-u6-moves-b`, `gp-u9-win`). The title/attract oracle's 9,866 writes are
byte-identical, and `make audio-render` still renders the selmo at peak
**2790**, sha256 `530a94301f4be46f9a5c666c94516f8a1f7fca6f136d3be0562faed802c47cef`
(the render reseeds the driver with `seq_driver_reset`, so the install state is
unchanged; the engine sequence volume is deliberately **not** part of the
install state, so the render's `seq_set_sequence_volume(0x50)` survives).


## D4. The `0x21` sub-game

Not taken in this phase: the subagent fleet was unavailable (the provider
reported insufficient account funds for three of the four dispatches and
aborted the fourth), and the audio derivation took the session. The mode's
entry, gate chain and content remain the design's §3 and the
`2026-10-30-final-truth-derivations.md` §F6 derivation; R05 stays declared,
with no capture and no claim change.
