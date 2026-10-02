# Gameplay U7 (two players): derivation record

Plan: `docs/superpowers/plans/2026-10-01-gameplay-u7-two-players.md`. Specs:
`docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track G
("U7 two players"), `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md`
§2, §3.2-§3.4. Sections §T.0-§T.4 were written by the planners (2026-10-01:
a first draft, then a relaunch that re-ran every listing and capture read
cited here; §T.R lists what the relaunch changed). §T.5 onwards are appended
by the plan's tasks. Every claim cites a raw address, a `poll.log` line or a
run. On any conflict the raw wins and the correction is recorded here with its
address.

## §T.0 Method

The fixed-up image (the LE fixups applied, so data operands are the runtime
linear addresses; AGENTS.md "raw-file displacements are pre-fixup"):

```bash
S=<scratch>; cmake -S port -B $S/build && cmake --build $S/build --target diffrun
$S/build/diffrun --exe data/game/C/PRAGE.EXE --image-out $S/image.bin    # 1 028 304 bytes from 0x10000
```

`PRAGE.EXE` sha256 `eecba701…0e91b` (the installed, unpinned copy; the pins of
`tools/title_pin.py` touch only the title draws, the anim opcode 8 and the
master loop's spin draws, spec §3.8, none of the routines below). The
disassembler (`$S/dx.py`, capstone 5.0.7; `d ADDR N` dumps N dwords):

```python
import sys, capstone, os
img = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'image.bin'), 'rb').read()
BASE = 0x10000
def rd(a, n): return img[a - BASE:a - BASE + n]
def dis(a, n):
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    for i in md.disasm(rd(a, n), a):
        print('%08X  %-16s %s %s' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
if __name__ == '__main__':
    if sys.argv[1] == 'd':
        a = int(sys.argv[2], 16); n = int(sys.argv[3], 16)
        print(' '.join('%08X' % int.from_bytes(rd(a + 4*k, 4), 'little') for k in range(n)))
    else:
        dis(int(sys.argv[1], 16), int(sys.argv[2], 16))
```

Operand scans (`$S/opscan.py`): every occurrence of a dword in the code object
`0x10000..0x73B14`, decoded from the instruction that contains it. Ghidra MCP
(project `rage`, read-only) was used for cross-references only; each result
quoted below names its query (`get_xrefs_to`, `decompile_function`).

Captures read (read-only): `data/k11-captures/gp-pads` (`poll.log` sha256
`9f886034…09fa0c`, record gameplay-ground-truth §G.8a run 3),
`data/k11-captures/gp-idle-loss` (`773e2647…8c8447`, §G.18) and
`gp-idle-loss-run2` (`5ce62b39…0530c7`, §G.19).

## §T.1 The raw: what a second player changes

### §T.1.1 The START MENU rows and the player each one starts

`0xBCCCC` (dwords, fixed-up image: `d BCCCC 0x20`) is the title entry (string
`0x215` "START MENU") and seven 16-byte rows `0xBCCDC..0xBCD3C`, each `+0`/`+4`
strings, `+8` callback, `+0xC = 1` (strings named in the K11 record §0.2). Each
callback `0x2CBC4..0x2CC6A` is `mov edx, <mode>; mov ah, <sub>; mov word
[0x104b00], dx; mov byte [0x104b1d], ah`. The mode jump table `0x24B8C`
(`0x24F01 jmp [eax*4+0x24b8c]`, modes `0..0x33`) gives each mode's handler:

| row | label | callback | mode | `DS_00104B1D` | handler | credit | `0x257A4(arg)`: `DS_00104B1F` |
|---|---|---|---|---|---|---|---|
| 0 | LEFT PLAYER ARCADE | `0x2CBC4` | `0x2D` | 0 | `0x25071` | `0x250BA call 0x2ca7c(1)` | 1 (`0x250BF`/`0x250C4`): side 0 |
| 1 | RIGHT PLAYER ARCADE | `0x2CBDC` | `0x2E` | 0 | `0x250CE` | `0x25117 call 0x2ca7c(1)` | 2 (`0x2511C`/`0x25121`): side 1 |
| 2 | LEFT PLAYER TRAINING | `0x2CBF4` | `0x28` | 1 | `0x24F09` | none | 3 (`0x24F51`/`0x24F5C`) |
| 3 | RIGHT PLAYER TRAINING | `0x2CC0C` | `0x29` | 1 | `0x24F66` | none | 3 (`0x24FAF`/`0x24FBA`) |
| 4 | TUG OF WAR | `0x2CC24` | `0x2A` | 2 | `0x24FC4` | none | 3 (`0x25002`/`0x25014`) |
| 5 | ENDURANCE | `0x2CC3C` | `0x2B` | 3 | `0x25187` | none | 3 (`0x25187`/`0x25194`) |
| 6 | Start 2 PLAYER HANDICAP | `0x2CC54` | `0x2C` | 4 | `0x2501E` | none | 3 (`0x2505C`/`0x25067`) |

`0x257E0 mov byte [0x104b1f], dl` stores the argument (EDX = EAX at `0x257A6`):
**`DS_00104B1F` holds one bit per human side, bit 0 the left player (side 0),
bit 1 the right (side 1).** LEFT PLAYER ARCADE lines up with side 0 (P1: S X Z
C, U I N M, F1), RIGHT PLAYER ARCADE with side 1 (P2: the arrows,
Home/PgUp/End/PgDn, F2). Rows 2-6 start both sides as human. The arcade
handlers do not test `0x2CA7C`'s return: the divert happens with or without a
credit.

### §T.1.2 The start masks and what a second start does

`d 9ACBC 4`: `01000000 00000100 3A000040 00000000`. Side 0's mask is bit 24
of the newly-pressed word `DS_001088E4` (the `+0x2D8` byte's bit 0: F1 or U),
side 1's bit 8 (the `+0x2D9` byte's bit 0: F2 or Home), as spec §3.2 says.
There is no coin input in the play path (spec §3.4): "a second coin" is not a
thing this build reads; a second **start** is. Four routines test the masks:

- `0x11F28(side)` (callers `0x11D15`, `0x11D28`, `0x43939`, `0x43B4E`):
  `0x11F2B call 0x2c060` (a credit or FREE PLAY: `0x2CA2C`, `[0x105c00] |
  [0x105d60] != 0`), `0x11F39 test [edx*4+0x9acbc], [0x1088e4]`, then
  `0x11F47 call 0x2ca7c(1)`, return 1.
- `0x28CC8` (callers `0x2525F` mode 6, `0x25349` mode `0xC`): for each side
  without its `DS_00104B1F` bit (`0x28CD7 test eax, ebx; jne`), a credit and
  the mask newly pressed: `0x28D13` ORs the bit in, `0x28D19 call 0x2ca7c(1)`,
  returns side + 1 (`0x28D1E mov eax, ebx`). Mode 6 runs it only with
  `DS_00104B1D == 0` (`0x25256`), and a non-zero result goes to `0x28DA4`
  (`0x25269`), the mid-match join.
- `0x42F60` (callers `0x27A37`, `0x42CB4`): the continue/challenge start
  (`0x42F7A..0x42F97`, then `0x42F9C mov byte [0x105c04], 1`).
- `0x11D04` (the attract, mode 3, only with `DS_00104B1D == 0`, `0x11D0F`):
  `0x11F28(0)` and `0x11F28(1)` (`0x11D15`, `0x11D28`), the accepted mask
  (1, 2, or 3 when both arrive in one frame) to `0x257A4` (`0x11D41`).

So **a second start depends on the mode**: in the attract it starts a game for
the sides that pressed; in the character select (mode `0x10`, `0x43B4E`, and
`0x43939` in sub-state 1) it joins that side (`0x43B65..0x43B71`:
`DS_00104B1F |= side + 1`; `0x43B77`: `DS_00108170[side] = 1`); in an arcade
fight (mode 6 or `0xC`, `DS_00104B1D == 0`) it joins mid-match through
`0x28DA4`; on the continue and challenge screens it continues.

### §T.1.3 Credits

`0x2CA7C(n)`: FREE PLAY (`[0x105d60]`) returns 1; `n > [0x105c00]` (unsigned
`0x2CA91 ja 0x2ca78`, `xor eax,eax; ret`) returns 0; then **`0x2CA93 cmp byte
[0x104b1f],0; jne 0x2caa2` skips the debit `0x2CA9C sub [0x105c00], eax`.**

- LEFT PLAYER ARCADE debits: mode `0x2D` calls `0x2CA7C` at `0x250BA` before
  `0x257A4` sets `DS_00104B1F` (`0x250C4`), and `b1f = 0` in mode `0x27`
  (every `S` record of mode `0x27` in `gp-idle-loss`, §T.2.2): 5 → 4 (§G.18).
- **A join never debits**: `0x43B4E`'s `0x11F28` runs with `DS_00104B1F = 1`
  already set by the divert, and `0x28CC8` stores the bit (`0x28D13`) before
  its `0x2CA7C` (`0x28D19`). A join only *requires* a credit (`0x2C060`).
  Prediction for U7's scenario: `cred` stays 4 across the join.
- 2 PLAYER HANDICAP (`0x2C`) and the training/tug/endurance rows spend none.
- An attract double start (`0x11D15`, `0x11D28`, both with `DS_00104B1F = 0`)
  debits twice (the second only if a credit is left: `0x2C060`).

### §T.1.4 The character select with two sides

Mode `0x10`'s case `0x25385 call 0x438b4`: with `DS_00104B1D != 3` and the
sub-state `DS_00108174 == 0`, `0x438F3 call 0x43b24` (the select pass); sub-state
1 (`0x438FA`) runs only the joins (`0x43928`) and the countdown. The entry
`0x43738` (set by the divert as the mode-`0x10` hook, `0x25806`/`0x25810`
`DS_00104AE4 = 0x4367C`) sets `DS_00108170[side] = 1` (and spawns that side's
entry, `0x43964`/`0x43A08`) for each side whose `DS_00104B1F` bit is set, else 0
(`0x4377D..0x437BB`), the countdown `DS_0010816C = 0xF` (`0x437D1`; 5 when
`DS_00108173 != 0`, `0x437BD..0x437C6`) and the sub-state 0 (`0x4380B`). The
cursor bytes `DS_00108166[side]` start at `0xC887F[1..2]` = `00 05`; the class
map `0xC8882` is `00 02 03 01 06 04 05`. Per frame, per side (`0x43B2B`):

- byte 0: `0x11F28(side)` (`0x43B4E`); accepted: the spawns, `DS_00104B1F |=
  side + 1`, byte = 1, and no move or confirm this frame (`0x43B7E jmp
  0x43cc6`); refused: the prompt `0x432A0`.
- byte 1: the blink (`0x43464`), the stick from the low byte of the side's
  command word (`0x43BB5 mov ax,[ebx+0x1088e0]; xor ah,ah; and al,0xf0`): `0x10`
  right (cursor + 1 while < 6, `0x43C75`), `0x20` left (− 1 while > 0,
  `0x43C8B`), `0x40` down (+ 4 when < 4, then clamp to 6, `0x43C49`), `0x80` up
  (− 4 when ≥ 4, `0x43C32`), any other value nothing; then **the confirm on bit
  0 of the low byte** (`0x43CAD..0x43CC1 call 0x43d60`). The low byte is the
  newly-pressed byte (§T.1.6), so one press moves or confirms once.
- byte 2 (confirmed): when the other side's byte is 0 or 2, the versus hook
  `0x430E8` and the wipe to `0x11` (`0x43BF0..0x43C1E`); otherwise the next side.

`0x43D60` (the confirm) stores the class `0xC8882[cursor]` in
`DS_0010816A[side]`, byte = 2, **the slot's `+0x63 = 0`**
(`0x43D84..0x43D94`, `[side*0x94 + 0x107813]`), and `DS_00105B34[side] =
0x43D0C(side)`: 1/2/3 when the high (held) byte has `0x02`/`0x04`/`0x08`
(b1/b2/b3 held at the confirm), else 0. The countdown `0x43AAC` runs when
`DS_000EF6DC & 0x3F == 0` (`0x43CF0..0x43D01`): `DS_0010816C − 1`, and at 0
it confirms every byte-1 side (`0x43AC5..0x43AD3`) and wipes.

So in a one-player game P1's confirm ends the select at once (the other byte
is 0), and with two players both must confirm (or the countdown ends it).

### §T.1.5 The CPU slot

The versus hook `0x430E8`: `0x43103 cmp byte [0x108173],0; je` — when
`DS_00108173 != 0` both slots' `+0x63 = 1` (`0x4310C..0x43114`). Then for
`DS_00104B1D != 1`, `0x43149 mov al,[0x104b1f]; cmp eax,3; je 0x43166` skips
`0x41350` when both sides are human; otherwise `0x41350((b1f − 1) ^ 1,
DS_00104AFC)` (`0x43157 dec eax`, `0x4315F xor al,1`, `0x43161`) sets that
side's character from `0xC835A[stage]` (`0x41367`, when `DS_00104B1D != 1`)
and **`0x41385 mov byte [eax*4+0x107813], 1`: `+0x63` is the CPU flag.** With
`b1f = 1` (LEFT PLAYER ARCADE alone) the CPU is side 1.

`DS_00108173` has no writer in the code object (operand scan of
`0x108170..0x108174`: only reads of `0x108173`, at `0x114C6`, `0x288DB`,
`0x4369B`, `0x437BD`, `0x4455C`, `0x46596`, `0x43103`; Ghidra `get_xrefs_to
0x108173` lists the same seven reads and no write). In `gp-idle-loss` side 0
was never a CPU (§T.2.2), so it was 0 there; with it 0 and `b1f = 3`, no
routine sets either `+0x63` between the two confirms (each writes 0) and the
fight.

### §T.1.6 The command words: who writes them

`0x4F644` (`input.c input_state_update`), `0x4F69F..0x4F6DE`: `DS_001088E0 =
(new >> 24) | (held >> 16 & 0xFF00)` (side 0, `0x4F6BD`), `DS_001088E2 =
(new >> 8 & 0xFF) | (held & 0xFF00)` (side 1, `0x4F6DE`): the low byte newly
pressed, the high byte held, each in the kb bit layout of spec §3.2
(`0x4F68B`: when `DS_001088D4 & 2`, `new |= held` first; the logged `new` is
the result). `game_frame` calls it every frame but in mode `0x27`
(`0x24C69 cmp eax,0x27; je`). The operand scan finds 68 uses of `0x1088E0` and
1 of `0x1088E2`; the stores are exactly these 13 (the other 56 are loads; no
`lea` or immediate use):

| store | routine | runs for a human slot? |
|---|---|---|
| `0x4F6BD`, `0x4F6DE` | `0x4F644`, the pads | yes |
| `0x24C96` | `0x24C73`, the command block (only with `DS_00104B26 == 0` and `DS_00104B1B != 0`): 0 when the slot's `+0x41` has bit `0x10` | yes |
| `0x246EE`, `0x246F9` | `0x246D4`, character 1's entrance word `0xA000`/`0x9000` | yes (mode 5, class 1 only) |
| `0x472CA`, `0x472FF`, `0x47325` | `0x47208`, the CPU's generator (called at `0x24CA1`) | no: `0x472AA mov dh,[eax*4+0x107813]; test dh,dh; 0x472B9 je 0x47358` |
| `0x3B207`, `0x3B215`, `0x3B23D`, `0x3B262`, `0x3B278` | `0x3B134`, the CPU's command mapper | no: `0x3B168 cmp byte [eax*4+0x107813],0; 0x3B170 je 0x3b291` |

So **with `b1f = 3` and both slots' `+0x63 = 0`, each command word is the pad
word, 0, or (mode 5, class 1) the entrance word.** That is the per-frame
evidence `tools/gp_twop.py` checks (§T.2.2). Neither `+0x63` byte is a
`poll.log` field (`gp_session.SNAP_FIELDS`); `b1f`, `e0`/`e2`, `new`/`held`
are, so the check is indirect (§T.4.3).

### §T.1.7 What else reads the human/CPU state in a short fight

`0x1D642..0x1D65C` (the meter rate: both `+0x63` tested, `0x1D647`,
`0x1D655`), `0x39360..0x39388` (the damage zeroing applies only when exactly
one slot is a CPU: `0x39378 xor eax, edx` of `DS_00107813`/`DS_001078A7`),
`0x461EF` (the input ring: a CPU slot takes 2, `0x461F8`; a human slot the
device word `[DS_00101514]+0x2D4 + side*2`, both 0 = keyboard in the probe's
`+0x2D4..+0x2D7`, spec §3.2), and `0x472AA`/`0x3B168` above. Past a short
fight (not reached by U7's cut): the round and match end (`0x41DD5`,
`0x269C3`, `0x28171`), the continue offer (`0x423EA`).

### §T.1.8 The handicap dwords (the first-boot `0x64`)

Code uses of `DS_00107468` (operand scan: three): `0x20D05` (the init store,
record named-gaps-b §B.11: both dwords take the key record's `+0x24`, `0x64`
on the first boot as in the raw; the port has matched the raw here since unit
Z, so this is not a port-vs-original difference any more), `0x313CE` (the 2
PLAYER HANDICAP menu's store) and **`0x394AC`**, the only read: `0x3939A mov
dl,[0x104b1d]; cmp dl,2` … `0x3947C cmp dl,4; jne 0x39558` … `0x39494 mov
al,[0x104b1f]; cmp eax,3; jne 0x394c2` … `0x394AC imul edx,[eax*4+0x107468];
idiv 0x64`. The damage is scaled only in mode `0x2C` (`DS_00104B1D = 4`)
with both sides human. **In an arcade game (`DS_00104B1D = 0`) the handicap
dwords are not read, so the `0x64` cannot affect U7's capture.** A lead for U8
(not U7's): `DS_0010746C` (side 1's handicap dword) is also slot 0 of a
play-time accumulator read and zeroed by `0x32A3C` (`0x32A52`, `0x32A59`) for
a mode argument with `& 3 == 0`, and read by `0x32B00`/`0x32B4C`; `0x32A3C`'s
callers are `0x27094 0x27886 0x28656 0x289AF 0x28AD5 0x28B78 0x415CD 0x4240E`
(their EAX not derived here).

### §T.1.9 Inputs that are inert in U7's path

- The attract's chords `0x10DB0` (held P1 left + P2 right, `0x20000000 |
  0x1000`: the pause tail) and `0x10E18` (held P1 right + P2 left, `0x10000000
  | 0x2000`: the continue tail; `test_game.c` `frontend_pause_tail`/
  `frontend_continue_tail`) are called only from `0x11D04` (Ghidra
  `get_xrefs_to 0x10db0` / `0x10e18`: twelve calls each, all in `FUN_00011d04`),
  the attract. U7's walk-in chord is `0x10002000` in mode 6, where `0x11D04`
  does not run.
- The pad presses' BIOS words (spec §7 Q4, ruled yes, §G.2): the int 16h key
  loop acts only on ascii `0x0D`, `0x1B`, `0x20` and ascii-0 scans `0x10 0x1F
  0x24 0x32` (§G.2); U7's words `3C00 2E63 4BE0 1675 4700 4BE0+2E63 4F00+1769
  4900+316E` are none of them. Each latches `DS_00105F30` (`0x24D4D`), read only
  by `0x2EB80` (Ghidra `get_xrefs_to 0x105f30`), whose callers are
  `0x2EBF0`, `0x2FFC4` (the menu step), `0x33058`, `0x33230` (Ghidra
  `get_function_callers 0x2eb80`): menu and name-entry code, not modes `0x10`,
  5 or 6.

## §T.2 Captured evidence available now (existing captures)

### §T.2.1 The P2 pad bits in the original (`gp-pads`)

`gp-pads` presses every P2 name for one frame on the MAIN MENU (record §G.7.2).
Re-read (`gs.snapshots` over `poll.log`; `S(F)` is the press's sample frame,
the `I` record's `f` + 1):

```
poll.log:741  p2.up    press f=2C6 S(2C7) kb=0080 raw=00008000 pad=00000000 mode=27 ent=2A2BEC | S(2C8) kb=0000
poll.log:774  p2.down  press f=2E4 S(2E5) kb=0040 raw=00004000
poll.log:807  p2.left  press f=302 S(303) kb=0020 raw=00002000
poll.log:840  p2.right press f=320 S(321) kb=0010 raw=00001000
poll.log:873  p2.b0    press f=33E S(33F) kb=0001 raw=00000100
poll.log:906  p2.b1    press f=35C S(35D) kb=0002 raw=00000200
poll.log:939  p2.b2    press f=37A S(37B) kb=0004 raw=00000400
poll.log:972  p2.b3    press f=398 S(399) kb=0008 raw=00000800
poll.log:1005 p2.start press f=3B6 S(3B7) kb=0001 raw=00000100
```

Each sets exactly its spec §3.2 bit in the `+0x2D9` byte (bits 8..15 of
`raw`); F2 and Home share bit 0 (the side-1 start mask `0x100` of §T.1.2). A
one-frame press never reaches the level (`pad` stays 0), so the MAIN MENU
entry stays `0x2A2BEC`. The held chord `p1.left + p1.b2 + p2.right`
(`poll.log:1040`, 5 frames) in mode `0x27` (where `0x25210` runs `0x4F644`):

```
f=3D5 kb=2410 raw=24001000 pad=00000000 new=00000000 held=00000000 e0=0000 e2=0000
f=3D6 kb=2410 raw=24001000 pad=24001000 new=24001000 held=24001000 e0=2424 e2=1010
f=3D7 kb=2410 raw=24001000 pad=24001000 new=00000000 held=24001000 e0=2400 e2=1000
f=3DA kb=0000 raw=00000000 pad=24001000 new=00000000 held=24001000 e0=2400 e2=1000
f=3DB kb=0000 raw=00000000 pad=00000000 new=00000000 held=00000000 e0=0000 e2=0000
```

**P2's `right` held is `e2 = 0x1010` in its first level frame (new | held)
and `0x1000` after**, exactly §T.1.6's layout, one iteration after `raw`
(the level lag, spec §3.1). No capture yet shows a P2 key where it acts (the
character select or a fight): that is what U7 captures.

**The harness covers P2's pads as it is** (no extension needed): `PAD` holds
the nine `p2.*` names (`tools/gp_session.py:55-59`); the injector writes the
key-state byte `ptr + 0x254 + scan` for any scan (`gp_capture.Injector.press`);
`port_script` emits the full 16-bit kb (`raw_to_kb`) and the port driver
applies both bytes (`test_game.c` `k11_key_bits` → `host_set_key_bits_override`);
the captures above and the preview of §T.4 show both sides' bits land. What it
does not model: typematic repeat, the `E0` grey/keypad distinction in the ISR
(it writes the table by scan), joysticks (§T.4.3).

### §T.2.2 The two-human check against the one-player control

`tools/gp_twop.py` (the plan's Task 2, run in scratch before it went into the
plan) classifies each `S` record's command word per side as `pad`, `zero`,
`entrance` or `other` (§T.1.6). On the existing captures (all `S` records):

```
gp-idle-loss      9807 S: side 0 {pad: 9807}; side 1 {pad: 7540, other: 2267}; first other f=77C mode 5 word 2020
gp-pads           2447 S: side 0 {pad: 2447}; side 1 {pad: 2447}
gp-idle-loss-run2 9793 S: side 0 {pad: 9793}; side 1 {pad: 7526, other: 2267}; first other f=77C mode 5 word 2020
```

Per mode in `gp-idle-loss` (side 1 `other`): mode 5 206 (of 245 `S`), mode 6
2040 (of 4398), mode 8 21 (of 185); side 0 never. In the first 120 mode-6
records (from `f = 0x7F5`) side 1's word is non-zero 44 times (`1010 2020 2B20
2F20 4C4C 8080`). `b1f`: 0 in modes 3 and `0x27`, 1 from the divert (`0x1A`) to
the match end, 0 again in `0x17`/`0x13`/`0x1E`. The CPU's words are what make
side 1 `other`, so the classifier separates a CPU side from a human one on real
data. `gp_twop.py check` on `gp-idle-loss`, `gp-pads`, `gp-idle-loss-run2`:
`FAIL: b1f never reaches 3 (no S record has both sides human)`, `rc=1`.

## §T.3 The scenario `gp-twop` (harness values, with their source)

Path (the plan's Decision 2): LEFT PLAYER ARCADE (the three Enters of
`gp-idle-loss`), P2 joins in the character select, each side moves its cursor
once and confirms, the fight, both press. Frames are `F`, the iteration that
samples the press (spec §4.1):

| step | when | action | raw it exercises | predicted |
|---|---|---|---|---|
| 0-2 | boot 25 s, `0x27`+150, +150 | Enter ×3 | as `gp-idle-loss` (§G.18) | mode `0x2D`, `b1f = 1`, `cred` 5 → 4 |
| 3 | mode `0x10` + 60 | `p2.start` held 5 | `0x11F28(1)` on `new` at `F + 1` (the level lag) | `b1f = 3` from `F + 1`, `cred` stays 4 (§T.1.3) |
| 4 | +60 | `p1.right` held 5 | side 0 cursor 0 → 1 (class 2) | |
| 5 | +30 | `p2.left` held 5 | side 1 cursor 5 → 4 (class 6) | |
| 6 | +30 | `p1.b0` held 5 | side 0 confirms (byte 2); side 1 is 1, the select goes on | |
| 7 | +30 | `p2.b0` held 5 | side 1 confirms at `F + 1`; the next frame's side-0 pass sees 2/2 and wipes (`0x43BF0`) | mode `0x1A` at `F + 2` |
| 8 | mode 6 + 60 | `p1.right + p2.left` held 30 | both walk in (held bits, the high bytes) | |
| 9 | +60 | `p1.b1 + p2.b2` held 5 | both attack | |
| 10 | +30 | `p1.b2 + p2.b1` held 5 | both attack | |
| end | +60 | `('end',)` | the `X` record; the port script ends here | |

Harness values: the 60/30-frame gaps (stimulus spacing; the picks end ~210
frames into mode `0x10`, inside the first countdown step: `DS_0010816C` starts
at `0xF` (or 5) and steps every 64 frames, so the countdown cannot end the
select before 5 × 64 = 320 frames), holds of 5 (the `gp-pads` chord's hold,
which reached the level, §T.2.1; a 1-frame press never acts), the end 60 frames
after the last press, `time_limit = 70` s. The limit: in `gp-idle-loss` mode
`0x10` began at `ms=30953` (`poll.log:670`) and the stretch `0x1A → 6` after
the select took `46739 → 55059` ms (`poll.log:1612`, `:2054`) for 437 frames;
with the select ending ~210 frames in, mode 6 is expected near 43 s and the end
near 47 s, so 70 s leaves ~23 s for host-timed loads. Neither cursor ends on
class 1, so `0x246D4`'s entrance word is not expected (the check allows it in
mode 5). The confirms press b0 alone, so `DS_00105B34[side] = 0` (§T.1.4).
The BIOS words are inert (§T.1.9).

**Size estimate** (Decision 1): `gp-idle-loss` stored 8 173 distinct frames in
376 MB (46 KB each) from 12 646 AVI frames after the post-logo start (raw
1 371..14 016, `session.txt`; 0.646 distinct per AVI frame). A 70 s run at
70.0866 fps holds ~4 906 AVI frames, ~3 535 after the post-logo start, so
~2 280 distinct frames: **~105 MB expected, at most ~165 MB** (every frame
distinct). Task 4 measures it. (A later data point, record gameplay-u5
§C5.16: `gp-u5-charsel`, 60 s, came in at 68 MB for 1 464 frames, 46.4 KB per
frame, 24% above its estimate. The per-frame size agrees with this estimate's;
the frame count was the low part.) The port dump: §T.4's preview wrote 466 `.ipx`
frames, 30 MB, in 26 s.

## §T.4 Port preview (a hand-built script; not the capture's)

### §T.4.1 The run

To see whether the port can run a two-player arcade game before a capture
exists, a hand-built v2 script was replayed in a scratch build of `main`
`e9271df`: `gp-idle-loss`'s three Enter keys (321, 474, 622), then `bits`
lines at the scenario's frames counted from the port's own mode `0x10` at
`f = 0x293` (P2 start `719`, P1 right `779`, P2 left `809`, P1 b0 `839`, P2 b0
`869`, each held 5) and from its mode 6 at `0x51C` (`1368 1020` held 30, `1428
0204` and `1458 0402` held 5), `end 1518`, no pad BIOS words.
`PR_GP_DUMP=… PR_GP_SCRIPT=… ./build/run_tests`, 26 s:

```
T f=26E mode=2D b1f=0 cred=5
T f=26F mode=1A b1f=1 cred=4      the divert, 5 -> 4
T f=293 mode=10 b1f=1 cred=4
T f=2D0 mode=10 b1f=3 cred=4      P2's join at 720 = its bits frame 719 + 1; cred stays 4 (§T.1.3)
T f=367 mode=1A b1f=3 cred=4      871 = P2's confirm bits 869 + 2: the wipe
T f=38B 0x11, 38C 0x17, 47D 0x1A, 48F 0x1B, 4A1 5, 51C 6 (b1f 3, cred 4 throughout)
fn-miss PR_GP_DUMP 0x29D60 frontend_mode_1b_step hits=1
fn-miss PR_GP_DUMP 0x5D812 frontend_mode_1b_step hits=1
fn-miss PR_GP_DUMP 0x3A588 fighter_state_3531c hits=82
fn-miss PR_GP_DUMP distinct=5 dropped=0
```

(plus the base `0x5D812` pair). With the scenario name `gp-preview` the
driver fails on the three unknown pairs (`test_platform.c:185: 5 != 2`): a
scenario without its own miss set may miss only the base pair, which is why
Task 5 pins `k_miss_gp_twop`. With the header renamed `scenario gp-twop` and
the Task 5 table (these three rows) in a scratch `test_platform.c`, the same
run ends `all checks passed`, `rc=0`; with the `0x3A588` row dropped it fails
(`5 != 4`, `unexpected 0x3A588 from fighter_state_3531c`).
`gp_twop.py check --trace <dump>/trace.txt`: `join f=2D0 (cred 4 -> 4); 799 S
records from the join to the end; side 0 [('pad', 799)], side 1 [('pad', 799)];
fight presses 40/40 … two-human match: ok`. At the end (`0x5EE`) slot 1 `+0x52
= 0x10`, `+0x5A = 0xF` (P1's attack landed). (All of this is on `e9271df`. The
re-run on `main` `8eaf25a`, where `0x3A588` is ported, is in §T.RB.)

### §T.4.2 What the preview says

The port takes the raw's two-player path (join on a credit with no debit,
both-confirm wipe, no CPU words on either side). On `e9271df` the only miss
past the select was `0x3A588`, the unported state-10 callback U4 named (§G.23
item 3, owner U6). U6a has ported it (record gameplay-u6 §U6.21). On `main`
`8eaf25a` the preview misses only the two harmless wipe hooks, `0x29D60` and
`0x5D812` from `frontend_mode_1b_step` (§T.RB). So the first fight divergence,
if any, is expected in move code the merged base has not ported (U6b's scope),
not in the two-player logic. U4's O1 (the character select's idle animation)
was fixed by U5 (record gameplay-u5 §C5.12), and the one-player select now
matches the capture through mode 6 (§C5.17). A first frame divergence in this
select would therefore be new and specific to two players: the select here
spans `f = 0x293..0x367`, across the countdown steps at `0x2C0`, `0x300` and
`0x340`. A preview only: the capture's frames, key latency and BIOS words
differ, and the plan pins nothing from it.

### §T.4.3 What U7 cannot show (stated before the capture)

- The `+0x63` CPU flags themselves (not a `poll.log` field, and adding one
  would change the shared `SNAP_FIELDS`): the evidence is `b1f = 3` (the raw
  makes it skip `0x41350`, §T.1.5), each confirm's `+0x63 = 0` (§T.1.4), and the
  per-frame command-word source (§T.1.6). A CPU word that happens to equal the
  pad word is not detected frame by frame; over a fight a CPU side is (§T.2.2).
- The cursor and class bytes (`DS_00108166`, `DS_0010816A`) are not logged:
  the picks are visible only in the frames. (Once U6b merges, the snapshot
  carries `c0`/`c1`, the slots' characters at `+0x7A` (record gameplay-u6
  §U6.11). That is the character, not the cursor or class byte, and its
  mapping to the class is not derived here.)
- The physical keyboard (the claims start at the IRQ1 key-state table), the
  grey/keypad `E0` distinction, typematic repeat (spec §7 Q4), joysticks (the
  device words `+0x2D4/+0x2D6` stay 0).
- The other two-player entries: RIGHT PLAYER ARCADE + P1 join, the attract
  double start, the mid-fight join (`0x28DA4`), 2 PLAYER HANDICAP (the only
  reader of the handicap dwords, §T.1.8), training/tug/endurance (U8).
- Past the cut: round end, match end, the continue/challenge offers with two
  humans, winner/loser credits.
- Run-to-run determinism under two-human input is not re-measured (one
  capture, Decision 3); §G.19 showed the original deterministic given the same
  input frames.
- Everything the gameplay oracle's narrow claim excludes (§G.16): the order of
  the port's frames, frames after N, a port that under-renders.

## §T.R Relaunch re-verification (2026-10-01)

Every listing above was re-run on a fresh image (`cmp` equal to the first
draft's), every capture number re-read, the preview re-run, and the scratch
tree's tests, mutations and `make verify` run (plan "What was run"). Changes
from the first draft (raw wins):

1. §T.1.4: the countdown's start is `0x437D1` (`0xF`), `0x437C6` is the `5`
   used when `DS_00108173 != 0`; the first draft cited `0x437C6` for 15.
2. §T.1.5: the `DS_00108173` path (`0x4310C`, both slots CPU) was missing.
3. §T.1.6: the CPU-gate branch addresses are `0x472B9` and `0x3B170` (the
   draft's tool docstring said `0x3B168`, the compare); the 56 loads are
   68 operand uses minus the 13 stores (`0x1088E0` 68, `0x1088E2` 1).
4. §T.1.8: the `0x64` is no longer a port-vs-original difference (§B.11 made
   the port follow the raw); store addresses `0x20D05`/`0x313CE` (instruction
   starts), read `0x394AC`.
5. §T.1.9 is new (the attract chords, the key latch).
6. §T.2.2's check now stops at the `X` record (the capture runs on to its time
   limit with no input; a match end there would drop `b1f`), and reads a port
   `trace.txt` (`--trace`).

## §T.RB Re-baseline to `main` `8eaf25a` (2026-10-01)

Since the plan was written, U5 (`b09b9e6`) and U6a (`8eaf25a`) have merged. U7
runs after U6b. Re-measured in a scratch `git archive` of `8eaf25a`, with
`data` linked and the fixtures copied:

1. **The preview (§T.4.1's script, verbatim, header `scenario gp-twop`).** With
   no gp-twop table, `rc=1` and the driver's `test_platform.c:193: 4 != 2`. The
   miss lines: `0x5D812 actor_spawn hits=3524`, `0x5D812 set_dead hits=3222`,
   `0x29D60 frontend_mode_1b_step hits=1`, `0x5D812 frontend_mode_1b_step
   hits=1`, `distinct=4 dropped=0`. `0x3A588` is gone: U6a ported it (record
   gameplay-u6 §U6.21). With the plan's re-baselined Task 5 code (the two-row
   `k_miss_gp_twop` and `fnm_known`'s new last parameter `twop`), `rc=0`,
   `all checks passed`. Without the `0x5D812` row, `4 != 3` and `unexpected
   0x5D812 from frontend_mode_1b_step`.
2. **The mode path equals §T.4.1's frame for frame** (`gp_twop.py path --trace`):
   `0x2D@26E`, `0x1A@26F` (b1f 1, cred 4), `0x10@293`, `0x1A@367` (b1f 3),
   `0x1B@379`, `0x11@38B`, `0x17@38C`, `0x1A@47D`, `0x1B@48F`, `5@4A1`,
   `6@51C`. `gp_twop.py check --trace`: `join f=2D0 (cred 4 -> 4); 799 S records
   … side 0 [('pad', 799)], side 1 [('pad', 799)]; fight presses 40/40 …
   two-human match: ok`.
3. **The tools on the merged base.** The three gp suites now hold 67 tests (U5
   added tests), so the plan's counts are `B+2`/`B+12` (69/79). `gp_twop.py
   check` on `gp-idle-loss`, `gp-pads` and `gp-idle-loss-run2` still gives
   `rc=1`, `b1f never reaches 3`. The 12 tests pass with U6b's five
   `SNAP_FIELDS` appended (`r0 r1 c0 c1 s0_43`; a scratch edit as in the U6b
   plan's Task 1), because the fixtures are built from `gs.SNAP_FIELDS`.
4. **The U6b dependencies** (stated, not measured: U6b is not on `main`):
   - Every capture made after U6b carries the five fields. `gp_twop.py` ignores
     them; `c0`/`c1` name the slots' characters (§T.4.3).
   - `fnm_known` gains U6b's `moves` parameter before U7's `twop`.
   - U6b's ported callbacks change what the replay reaches. Task 5 Step 1's set
     and Step 6's divergences are re-measured on the merged base.
   - U6b's `gp_compare` `moves:` claim: reported by U7 as planned, pinned at the final review (§T.10).
5. **Corrected in place:** §T.4.2 (the `0x3A588` and O1 owners: both closed by
   U6a and U5); §T.4.3 (`c0`/`c1`); §T.3 (U5's size data point).
6. **A cross-record discrepancy (raw wins; not edited here, it is U5's).** The
   U5 record §C5.4 and the comment above `gp-u5-charsel` in `tools/gp_session.py`
   cite `0x437C6` as the countdown's `0xF`. The fixed-up image (capstone 5.0.7,
   `dx.py 437BD 20`) has `0x437C4 je 0x437d1`, `0x437C6 mov word [0x10816c], 5`
   and `0x437D1 mov word [0x10816c], 0xf`, as §T.1.4 and §T.R item 1 say.

## §T.5 The scenario (Task 1)

`SCENARIOS['gp-twop']` as §T.3. `tools/tests/test_gp_twop.py` (TestScenario, 2 tests): before the
block `KeyError: 'gp-twop'` (2 errors); after, the four gp suites `Ran 94 tests … OK` (B = 92 on
`1142462` + 2; `gp_capture.py --help` lists `gp-twop`). Mutation (P2 start hold 5 -> 1): both tests
FAIL; restored OK.

**Rebase decision: `gp-twop` is in `STOP_AT_END`** (U6b/capture-hygiene opt-in, `tools/gp_session.py`).
What compares past the scenario's end: nothing. `gp_twop.py` (Task 2) checks up to the `X` record and
ignores later records (`test_records_after_the_end_are_not_checked`); the port replay's script ends at
`X` (§T.3), so a capture frame past the port's last frame is unexplained whether or not the capture
holds a tail (`gp_compare.frame_claim` classifies every capture frame from the window start, `N` is
the first unexplained one; `trace_claim` walks the port's `T` records only); the ratchets (Tasks 5-6)
pin `N`/`F` at or before the port's end. The 70 s limit (§T.3) then only bounds a stall; the capture
stops `STOP_TAIL` = 60 frames after `X` (~47 s expected) instead of idling ~23 s more. Guarded by the
`gp-twop` assertion in `test_gp_capture.TestStopAtEnd.test_opt_in_scenarios` (which also requires
the scenario's last step to be `('end',)`); removing the name from the set fails that test.

**Correction (raw wins): the character-select countdown's `0xF` store is `0x437D1`, not `0x437C6`.**
Fixed-up image (`build/diffrun --exe data/game/C/PRAGE.EXE --image-out`, capstone, base `0x10000`):
`0x437BD cmp byte [0x108173],0` / `0x437C4 je 0x437D1` / `0x437C6 mov word [0x10816C],5` /
`0x437CF jmp 0x437DA` / `0x437D1 mov word [0x10816C],0xF`. So `0xF` is the `DS_00108173 == 0` arm
(`0x437D1`), `5` the other (`0x437C6`), as §T.1.4 and §T.R item 1 say. The comment above
`gp-u5-charsel` in `tools/gp_session.py` and the U5 record §C5.4 ("The time-out") cited `0x437C6`
for the `0xF`; both corrected in this commit. Neither changes a pinned value (the earliest time-out
stays 14 x 64 = 896 frames).

## §T.6 The two-human check (Task 2)

`tools/gp_twop.py` (`pad_word`, `classify`, `load`, `two_human`, `mode_path`; CLI `check|path`),
`tools/tests/test_gp_twop.py` gains `TestTwoHuman` (10 tests; the file has 12). The fixtures build from
`gs.SNAP_FIELDS` (U6b's five fields included), and the check ran on a real port `trace.txt` (the
`gp-idle-loss` replay's, whose `T` lines carry U11's `lat spz mpz` after `s0_43`): the extra fields do
not matter (`b1f never reaches 3`, as for a one-player run). Before the module:
`ModuleNotFoundError: No module named 'gp_twop'` (1 error); after, the four gp suites
`Ran 104 tests … OK` (B = 92 + 12).

Mutations (each alone, restored after; `PYTHONDONTWRITEBYTECODE=1`):

| mutation | result |
|---|---|
| `pad_word` side test `== 0` -> `== 1` | FAIL `test_pad_word_is_0x4f644`, `test_a_two_human_match_passes`, `test_a_side_that_never_pressed_fails` |
| `if r['b1f'] != 3 and` -> `if False and` | ERROR `test_b1f_dropping_after_the_join_fails` |
| `end = None if xrec is None else xrec['f']` -> `end = None` | FAIL `test_a_two_human_match_passes`, `test_records_after_the_end_are_not_checked` |
| `if out['pressed'][side] == 0:` -> `if False:` | FAIL `test_a_side_that_never_pressed_fails` |
| `r['mode'] == 5 and e in ENTRANCE_WORDS` -> `e in ENTRANCE_WORDS` | ERROR `test_the_entrance_word_is_allowed_in_mode_5_only` |
| the T -> S line rewrite in `load` -> `pass` | FAIL `test_a_port_trace_reads_as_s_records` |
| `mode_path`'s `r['mode'] != prev` dropped | FAIL `test_mode_path` |

Control (`check` on the one-player captures): `gp-idle-loss`, `gp-pads`, `gp-idle-loss-run2` each print
`gp_twop: <name>: FAIL: b1f never reaches 3 (no S record has both sides human)`, rc=1. `path` on
`gp-idle-loss`: `poll.log:4 f=4 mode=3 b1f=0 cred=5`, `:324 f=141 mode=27 b1f=0 cred=5`,
`:633 f=26F mode=1A b1f=1 cred=4`, `:652 f=281 mode=1B b1f=1 cred=4`, `:671 f=293 mode=10 b1f=1 cred=4`
(then `:1613 f=640 mode=1A b1f=1 cred=4`).

## §T.7 `make gp-twop-oracle` (Task 3; not in `verify` yet)

The target sits after `gp-keys-oracle`'s recipe (the base is `main` `1142462`, where the gp oracles are
`gp-oracle`, `gp-charsel-oracle`, `gp-moves-oracle`, `gp-keys-oracle`; the plan's anchor after
`gp-charsel-oracle` was re-anchored by content); `gp-twop-oracle` is appended to the `.PHONY` list. Its six
variables are empty until Tasks 5-6 measure them.

1. Red (the target absent): ``make: *** No rule to make target `gp-twop-oracle'.  Stop.``, `exit=2`.
2. No capture (`data/k11-captures/gp-twop` absent), plain and under `PR_ORACLE_REQUIRED=1`: `exit=0` both;
   `Ran 13 tests … OK` (12 from Task 2 plus the no-debit test, below),
   `gp-twop-oracle: no capture at data/k11-captures/gp-twop (skipped)`,
   `gp-replay: no capture at data/k11-captures/gp-twop`, `gp_compare: no capture at data/k11-captures/gp-twop (skipped)`.
3. A capture that is not two-human (a scratch capture root holding `gp-idle-loss`'s `poll.log`; nothing
   under `data/` written), `PR_ORACLE_REQUIRED=1`: `exit=2`;
   `gp_twop: gp-twop: FAIL: b1f never reaches 3 (no S record has both sides human)`,
   `make: *** [gp-twop-oracle] Error 1` (the replay and the ratchets never run).

Fold-ins from the Task 1-2 review:

- (t1) `two_human` fails when the join frame has a prior record and the credit changed at the join
  (`credit B -> J at the join f=…`): the raw's no-debit rule (§T.1.3; 0x2CA93 skips the debit 0x2CA9C when
  b1f != 0). Test `test_a_credit_debited_at_the_join_fails` (credit 5 before, 4 at the join). Mutation
  (the `if out['cred_join'] != out['cred_before']` test replaced by `if False`, scratch copy): that test
  FAILs (`0 != 1`); restored OK.
- (t2) the `gp_twop.py` usage text says a port trace is converted only when the file is named `trace.txt`;
  any other name is read as a `poll.log` and finds no S records (checked: a T-record log under another name
  reports `b1f never reaches 3`).

## §T.8 The capture `gp-twop` (Task 4)

`make gp-capture scenario=gp-twop`, captured once (`session.txt`: DOSBox-X 2026.08.31, exe sha256
`8120f1bd…a68d`, `time_limit=70 wall_s=47.7 rc=0`, `stop_at_end=1 tail=60 signal_f=061D`, `frames=680
raw_window=1388..3290 avi_frames=3291`). Every check is ok: base, steps fired 11/11, end frame reached,
mode 0x27 after the Enter, kb == raw (0 differ), frames written 680/680, port script v2, no unscripted
input, stopped at the end. The tool reported 8 frames missed (spec §3.7); none falls where a press is
checked (the gaps are S(f+1) of Enter step 0 and S(f+3), S(f+2) of Enter steps 1 and 2: mode-0x27
frames nothing checks). `poll.log` sha256
`9c01a73bfb04be19f794316e80b784a86b782b65f06592f2091d1544edf2e22c` (1614 lines; X `poll.log:1553`,
`f=05E1 step=12 end`; E `reason=end rc=0`; re-checked at closure with `shasum -a 256`). Size: 30 MB
(30 592 KB for 680 frames, 45.0 KB per frame), against the §T.3 estimate of ~105 MB and the approved
maximum of ~165 MB. The difference is the STOP_AT_END cut (§T.5): 47.7 s instead of 70 s, and 680
distinct frames out of the 1903 AVI frames after the post-logo start (0.357) instead of §T.3's 0.646.

Mode path (`gp_twop.py path`): `:311` 0x27 (b1f 0, cred 5), `:618` 0x1A (b1f 1, cred 4), `:637` 0x1B,
`:654` 0x10 f=286, join at f=2C3 (b1f 3), `:881` 0x1A f=35A, `:901` 0x1B, `:918` 0x11, `:920` 0x17,
`:1162` 0x1A, `:1181` 0x1B, `:1200` 5, `:1324` 6 f=50F. b1f is 3 and cred is 4 from the join to the end.
This is the path §T.3 predicted; the CPU did not take the join, the countdown did not end mode 0x10,
and mode 8 never appears before the end.

Two-human: `join f=2C3 (cred 4 -> 4); 797 S records from the join to X f=5E1; side 0 [('pad', 797)],
side 1 [('pad', 797)]; fight presses 40/40`, `two-human match: ok`. Only the `pad` class appears (no
`zero`, `entrance` or `other`).

Presses (all late=0): `:714` p2.start f=2C1 (`I` record: `scan=3C`), S(2C3) new 00000100, b1f 3, cred 4
(S(2C2) still b1f 1, cred 4: no debit, 0x2CA93); `:777` p1.right S(2FF) e0 1010; `:810` p2.left S(31D)
e2 2020; `:843` p1.b0 S(33B) e0 0101, mode 0x10; `:876` p2.b0 S(359) e2 0101, S(35A) mode 0x1A
(0x43BF0); `:1384/:1385` S(54C) e0 1010 e2 2020, mode 6; `:1450/:1451` S(588) e0 0202 e2 0404;
`:1486/:1487` S(5A6) e0 0404 e2 0202. Every press matched §T.1/§T.3, with no corrections. The first
mode-6 S record (`:1324`) has c0=02 c1=06, and c0/c1 first became 2/6 at `:1181` (f=482). The fight
frames (capture 600, 640, 676) show TALON (side 0) against CHAOS (side 1), city ruins, timer 56 to 55.
This record does not derive how c0/c1 map to the class bytes `DS_0010816A`.

## §T.9 The replay, its miss set and the first divergences (Task 5)

Base `ff1b32a` (main `1142462` + Tasks 1-3; U5, U6a, U6b, U11 merged). The capture is Task 4's
`data/k11-captures/gp-twop` (`poll.log` sha256 `9c01a73b…2e22c`, 680 frames, X at f=5E1).

**The replay.** `gp_session.py port-script --scenario gp-twop` writes 34 lines, header
`# gp port script v2: scenario gp-twop`, 16 `bits` lines, `end 1505` (= X, f=5E1). The
`PR_GP_DUMP` driver runs the whole script: the last `T` record is `f=05E1 mode=0006`, the last port
frame is `463 f=05E1`, and there is no stall and no fault. No `GP_TWOP_END` cut is needed. Before the
table (`rc=1`):

```
fn-miss PR_GP_DUMP 0x5D812 actor_spawn hits=3349
fn-miss PR_GP_DUMP 0x5D812 set_dead hits=3047
fn-miss PR_GP_DUMP 0x29D60 frontend_mode_1b_step hits=1
fn-miss PR_GP_DUMP 0x5D812 frontend_mode_1b_step hits=1
fn-miss PR_GP_DUMP distinct=4 dropped=0
FAIL …test_platform.c:219: 4 != 2
fn-miss PR_GP_DUMP: unexpected 0x29D60 from frontend_mode_1b_step
fn-miss PR_GP_DUMP: unexpected 0x5D812 from frontend_mode_1b_step
```

The measurement matches the prediction (§T.RB item 1) exactly. No already-ported address is
missed, and no unexpected target appears: this fight's replay misses only the harmless pairs on a
base with U6b's ports (an observation of this one fight, not a claim about other fights).
Classification, both from §G.24 of the ground-truth record:
- `0x29D60 frontend_mode_1b_step`: the bare `ret`, the wipe's end hook into mode 0x10.
- `0x5D812 frontend_mode_1b_step`: the runtime `xor eax,eax; ret` stub, the hook at the wipe into
  mode 5.

Neither is a two-player routine (§T.1) or a porting target.

**The pin.** `port/tests/test_platform.c`:
- `k_miss_gp_twop[]` holds those two rows.
- `fnm_known` gains `twop` as its 8th parameter, after U6b's `moves` and U11's `keys_fight`.
- The selector is `twop = strcmp(sc, "gp-twop") == 0`.
- The `want` count gains the term `(twop ? FNM_N(k_miss_gp_twop) : 0u)`.
- The driver's `fnm_known` call gains `, twop`, and the `--check` log's call gains one more `0`.

After the edit: no compiler output, `rc=0`, `all checks passed`. Mutation (the `0x5D812` row
dropped): `rc=1`, `FAIL …test_platform.c:233: 4 != 3`,
`fn-miss PR_GP_DUMP: unexpected 0x5D812 from frontend_mode_1b_step`. Restored: `rc=0`.
`PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests` prints `all checks passed`.

**The port stays two-human.** `gp_twop.py check --trace` on the replay's `trace.txt` gives
`join f=2C3 (cred 4 -> 4); 799 S records from the join to the end; side 0 [('pad', 799)], side 1
[('pad', 799)]; fight presses 40/40`, `two-human match: ok`, `rc=0`. The join is at f=2C3, the
capture's frame. `path --trace` lists the following mode changes:
- f=134 0x27
- f=261 0x2D
- f=262 0x1A (b1f 1, cred 4)
- f=274 0x1B
- f=286 0x10
- f=35A 0x1A (b1f 3)
- f=36C 0x1B
- f=37E 0x11
- f=37F 0x17
- f=470 0x1A
- f=482 0x1B
- f=494 5
- f=50F 6

The capture's `path` prints 0x1B at f=276 and f=36E and has no 0x2D line. That comes from its `S`
gaps, not from a divergence: the capture's `P` records read `f=0261 mode=002D` (`poll.log:616`),
`f=0274 mode=001B` (`:636`) and `f=036C mode=001B` (`:900`). In f=0x134..0x5E1 the capture
has no `S` record at 8 frames: 134, 1CC, 261, 263, 274, 275, 36C and 36D. These are Task 4's
"8 frames missed" and the trace's "8 without a capture snapshot".

**The comparison** (`make gp-report scenario=gp-twop GP_DUMP=/tmp/pr_u7_gp`):

```
gp_compare: gp-twop: frames: window from capture 83 (raw 1742); 524 classified: 347 clean, 171 splice, 1 transition, 5 unexplained, 10 all-black
gp_compare: gp-twop: frames: FIRST UNEXPLAINED capture 612 (raw 3223): nearest port 463, rows 91..169, x 55..184 (477 px)
gp_compare: gp-twop: frames: UNEXPLAINED capture 613 (raw 3224): nearest port 463, rows 91..189, x 10..184 (2578 px)
gp_compare: gp-twop: frames: UNEXPLAINED capture 614 (raw 3225): nearest port 461, rows 91..191, x 9..269 (4879 px)
gp_compare: gp-twop: frames: UNEXPLAINED capture 615 (raw 3226): nearest port 461, rows 91..191, x 9..269 (5032 px)
gp_compare: gp-twop: frames: UNEXPLAINED capture 616 (raw 3227): nearest port 461, rows 91..191, x 3..269 (5312 px)
gp_compare: gp-twop: frames: coverage (reported, not ratcheted): 11 non-black port frame(s) up to port 463 not exhibited by any classified capture frame: [9, 11, 31, 175, 215, 216, 217, 218, 220, 222, 260]
gp_compare: gp-twop: trace: 1190 frames compared (f 134..), 8 without a capture snapshot; first tick difference f=135 (reported, not ratcheted)
gp_compare: gp-twop: trace: normalised (reported, not ratcheted): ent 0 of 1190 differ; t508 840 of 1190 differ (first f=264)
gp_compare: gp-twop: trace: 0 differing through 1505
gp_compare: gp-twop: moves: 1190 frames compared (f 134..) over c0 c1 r0 r1 s0_43, 8 without a capture snapshot
gp_compare: gp-twop: moves: 0 differing through 1505
```

**The triage.**

1. *Frames: the first unexplained capture frame is j = 612, and it marks the end of the port's
   script, not a divergence.* With `gp_compare.classify` around port 440, capture 598-611 exhibit
   port 452-463 in order (clean, or a splice of adjacent frames). Capture 611 is clean on port
   463, the port's last frame (`frames.txt` `00463 f=05E1`, the script's `end 1505`). Capture
   612-616 are the five unexplained frames (the report's "5 unexplained"), with nearest port
   463/463/461/461/461.

   I rendered capture 611, capture 612 and port 463 side by side (Pillow,
   `/tmp/gameplay-u7/cap611_cap612_port463.png`). All three show the round-1 fight: TALON (left)
   against CHAOS (right), city ruins, timer 56. Capture 612 differs from port 463 only in the
   fighters' and spectators' bodies (rows 91..169), which are their next animation pose. That is
   the frame after X, which the capture's 60-frame STOP_AT_END tail holds (§T.5) and the port's
   script never ran.

   So every content-bearing capture frame from the window start 83 through 611 is explained:
   - the START MENU and its wipes;
   - the two-player character select, f=0x286..0x35A, with P2's join, both cursors and both
     confirms;
   - the wipes, the versus screen and the stage entrance;
   - the fight to X.

   There is no select divergence, so no new gap opens in U5's area. There is no fight divergence,
   so nothing goes to U6b or a new gap. This is the same as gp-u5-charsel's N = 516, "how far the
   port's replay got" (gameplay-u5 §C5.17).

2. *Trace: there is no differing frame.* The `S`/`T` fields agree on all 1190 compared frames,
   f=0x134..0x5E1 = 1505, the script's end. 8 frames have no capture snapshot (the gaps above).
   The tick difference from f=135 and the t508 normalisation (first f=264) are the host-timed
   fields, reported and not ratcheted, as in every gp scenario (spec §7 Q6). So the trace has no
   first difference X to triage (no `poll.log`/`trace.txt` line pair, no preceding miss). For
   Task 6, the measured end is 1505.

3. *Moves: 0 differing through 1505* (reported here; pinned at 1506 in §T.10). The comparison covers `c0 c1 r0 r1
   s0_43` on 1190 frames. Both sides' move bytes agree through the fight's 40 presses (P1 b1/b2
   against P2 b2/b1, record §T.3).

Window start 83 (raw 1742). There are no named divergences. The one boundary is the script's end
at f=5E1: capture frames from 612 onward lie past the port's last frame, and the record names
that as a harness boundary, not a gap.

## §T.10 The pins and the failure proofs (Task 6)

Base `597c78c`. `make gp-report scenario=gp-twop GP_DUMP=/tmp/pr_u7_gp` was re-run on this head and prints
the §T.9 lines unchanged. The values (all measured, none fitted):

| Makefile variable | value | source |
|---|---|---|
| `GP_TWOP_MIN_FIRST` | 612 | "FIRST UNEXPLAINED capture 612 (raw 3223): nearest port 463": the port's last frame (f=5E1, the script's `end 1505`); capture 612..679 is the STOP_AT_END tail (the 60 game frames after X) the port never ran |
| `GP_TWOP_TRACE_MIN_FIRST` | 1506 | "trace: 0 differing through 1505" (f=0x134..0x5E1; 8 f without a snapshot) -> end + 1, the exact pin (1507 fails as unreachable) |
| `GP_TWOP_MOVES_MIN_FIRST` | 1506 | "moves: 0 differing through 1505" over `c0 c1 r0 r1 s0_43` (1190 frames, 8 without a snapshot) -> end + 1, the exact pin (1507 fails as unreachable); added by the final review |
| `GP_TWOP_MAX_START` | 83 | "window from capture 83 (raw 1742)"; 83 < 612 |
| `GP_TWOP_CAPTURE_SHA256` | `9c01a73b…f2e22c` (full value in the Makefile) | §T.8 |
| `GP_TWOP_CAPTURE_FRAMES` | 680 | §T.8 |

**What the three ratchets are.** N = 612, F = 1506 and the moves N = 1506 are the end of the port's script, "how far the port
got" (like gp-u5-charsel's 516/1513, gameplay-u5 §C5.17), not divergences: no content-bearing capture frame
from 83 up to 611 is unexplained and no traced or moves field differs through f=1505, so there is no divergence to
name. They would be raised only by lengthening the port's script (a harness change, re-measured then).

**The moves claim is pinned** (final review of U7; the plan's "reported, not pinned" predates U6b's
`gp-moves-oracle`, which pins it, and the measured value is already the exact end: no cost). `gp_compare`
prints the claim only with `--moves-min-first` (or `--report`): the recipe passes
`--moves-min-first "$(GP_TWOP_MOVES_MIN_FIRST)"` and prints "moves: 0 differing through 1505; ratchet N 1506 ok".
The moves fields (`c0 c1 r0 r1 s0_43`) are not in `gp_session.TRACE_FIELDS`, so the trace pin does not cover
them: the moves pin is a separate gate (proof below). Until that review the Makefile and the claims in this
record said "reported" while the recipe did not print it.

**Green** (`make gp-twop-oracle GP_DUMP=/tmp/pr_u7_gp`, `exit=0`): `Ran 13 tests ... OK`; `gp_twop: gp-twop:
join f=2C3 (cred 4 -> 4); 797 S records from the join to X f=5E1; side 0 [('pad', 797)], side 1 [('pad', 797)];
fight presses 40/40`; `two-human match: ok`; `capture: poll.log sha256 9c01a73b..f2e22c, 680 frames: matches
the pin`; `frames: window from capture 83 (raw 1742); 520 classified: 347 clean, 171 splice, 1 transition, 1
unexplained, 10 all-black`; `frames: first unexplained 612, ratchet N 612 ok`; `trace: 0 differing through
1505; ratchet N 1506 ok`; `moves: 0 differing through 1505; ratchet N 1506 ok`. (520 classified against the report's 524: `gp_compare.frame_claim` keeps classifying
past an unexplained frame in report mode, up to `REPORT_MAX` of them, and stops at the first with a ratchet.)

**Each pin can fail** (each `exit=2`):
- `GP_TWOP_MIN_FIRST=613`: `frames: FAIL: first unexplained 612 < ratchet N 613`
- `GP_TWOP_TRACE_MIN_FIRST=1507`: `trace: FAIL: N 1507 > end 1506: N is unreachable`
- `GP_TWOP_MOVES_MIN_FIRST=1507`: `moves: FAIL: N 1507 > end 1506: N is unreachable`
- `GP_TWOP_MAX_START=82`: `frames: FAIL: window starts at capture 83 (raw 1742) > pinned start 82`
- `GP_TWOP_CAPTURE_SHA256=0000…0` (64 zeros): `capture: FAIL: poll.log sha256 9c01a73b… (680 frames) != the pinned 0000… (680 frames): a re-capture invalidates the pinned N, F and window start; ...`
- `GP_TWOP_CAPTURE_FRAMES=681`: `capture: FAIL: ... (680 frames) != the pinned 9c01a73b… (681 frames)`

**Damaged output fails the ratchets** (a copy of the replay's dump, `gp_compare.py` directly, `--min-first 612
--trace-min-first 1506 --moves-min-first 1506 --max-start 83`):
- port frame 400 (f=05A2, mode 6) with byte 32000 flipped: `FIRST UNEXPLAINED capture 537 (raw 3148): nearest
  port 400, rows 100..100, x 0..0 (1 px)`, `FAIL: first unexplained 537 < ratchet N 612`, `rc=1`.
- port frame 200 (f=0474, mode 0x1A) the same: capture 308 unexplained, `rc=1`.
- Not detected: port frames 116 (f=0307) and 300 (f=0523) damaged the same way left `first unexplained 612 ... ok`.
  The damaged row (row 100, the 320 bytes at 32000) is identical in the neighbouring port frames (equal across
  115-117 and 299-301, not across 399-401), so a capture frame still matches a neighbour or a splice; frame 200's
  row equals 199's but not 201's and was detected. The oracle does not claim that every port frame appears (AGENTS.md),
  and a damage confined to rows the neighbouring port frame shares is not seen by it. Frames 400 and 200 are the
  proof; 116 and 300 are the limit.
- trace: `cred` at `f=0400` changed 4 -> 5 in the copy's `trace.txt`: `trace: first difference f=400 (1024) in cred:
  capture 4, port 5`, `FAIL: first differing 1024 < ratchet N 1506`.
- moves: `c1` at `f=0500` changed 06 -> 07 in the copy's `trace.txt`: the trace claim stays `0 differing through
  1505; ratchet N 1506 ok` (`c1` is not a trace field) and `moves: first difference f=500 (1280) in c1: capture 6,
  port 7`, `moves: FAIL: first differing 1280 < ratchet N 1506`, `rc=1`. (f=0400 has c1=00: the characters are set
  at f=482, §T.8.)

**Wiring.** `make verify` runs `gp-twop-oracle` after `gp-keys-oracle`. The task gate: tool suites `Ran 105 ... OK`
(B = 92 + 13); `diff-verify: 6/6 functions VERIFIED; 7/7 mutants detected` (the baseline's); `git diff --stat main --
port/src` empty. The full `make verify` runs at Task 7 (§T.11).

## §T.11 U7 closure (Task 7)

**The narrow claim, with the pinned values.** `make gp-twop-oracle` (in `make verify`, skipped without
`data/k11-captures/gp-twop`, even under `PR_ORACLE_REQUIRED`) shows, for the capture whose `poll.log`
sha256 is `9c01a73b…f2e22c` (680 frames), and no more:
- the capture is a two-human match by `tools/gp_twop.py check`, from the join at f=2C3 (`cred 4 -> 4`) to the
  scenario's end X at f=5E1: 797 `S` records, each side's command words written from its own pad (`pad` class
  only, never `zero`, `entrance` or `other`), 40 of 40 fight presses landed (§T.8);
- no content-bearing capture frame from the window start 83 up to N = 612 is unexplained (the START MENU, the
  character select with P2's join, both cursors and both confirms, the wipes, the versus screen, the entrance
  and the fight to X);
- the traced fields agree on every compared frame below F = 1506 (f=0x134..0x5E1; 1190 frames, 8 without a
  capture snapshot); the moves fields (`c0 c1 r0 r1 s0_43`) agree on the same frames below their own N = 1506 (pinned, §T.10).

N = 612 and F = 1506 are the end of the port's script (how far the port got, like gp-u5-charsel's 516/1513), not
divergences. A green run does not say the two-player game is correct: the oracle cannot detect a port that
under-renders, and the order of the port's frames and that every port frame appears are not claimed (11
non-black port frames up to port 463 are not exhibited by any classified capture frame, reported in §T.9). One
limit is measured (§T.10): a damage confined to rows that the neighbouring port frame shares (row 100 in port
frames 116 and 300) leaves the frame ratchet green.

**What this unit was asked, and where it is answered.**
- P2's pad bits (§T.2.1): each P2 key sets its spec §3.2 bit in the `+0x2D9` byte; F2 and Home share bit 0 (the
  side-1 start mask `0x100`, `0x9ACBC`); held, they become the high byte of `DS_001088E2`, newly pressed the low
  byte (`0x4F6DE`). Confirmed in the capture: p2.left `e2 2020`, p2.b0 `e2 0101`, the chord `e0 1010 e2 2020` (§T.8).
- The START MENU rows and which side each starts (§T.1.1): seven rows. `0x2D` LEFT PLAYER ARCADE starts side 0,
  `0x2E` RIGHT PLAYER ARCADE side 1, and `0x28`/`0x29`/`0x2A`/`0x2B`/`0x2C` both sides human from the start;
  `DS_00104B1F` holds one bit per human side. Only the two arcade rows spend a credit (5 -> 4).
- What a second start does (§T.1.2): it depends on the mode. In the attract it starts a game for the sides that
  pressed; in the character select (mode `0x10`) it joins that side (`0x43B4E`: `DS_00104B1F |= side + 1`); in an
  arcade fight it joins mid-match (`0x28CC8` -> `0x28DA4`); on the continue/challenge screens it continues.
  The capture shows the select join: b1f 1 -> 3 at f=2C3, no debit (§T.8).
- Credits (§T.1.3): a join never debits (`0x2CA93 cmp byte [0x104b1f],0` skips the debit; a join only requires a
  credit, `0x2C060`), measured `cred 4 -> 4` at the join and 4 to the end (§T.8). There is no coin input in this
  build's play path.
- The handicap `0x64` (§T.1.8): its only read is `0x394AC`, reached only in mode `0x2C` (`DS_00104B1D = 4`) with
  both sides human, so it cannot affect an arcade game.

**Named gaps.** None opened by this unit: Task 5 found no divergence in the window, so no select divergence
(U5's O1 is fixed, gameplay-u5 §C5.12) and no fight divergence (nothing for U6b's moves). Two reported items are
not triaged and not claimed: the 11 coverage-only port frames and the one `transition` frame (§T.9). Harness
values with their sources: the 60/30-frame gaps, the holds of 5, the 70 s limit, the STOP_AT_END tail of 60
(§T.3, §T.5). The capture is 30 MB, below the §T.3 estimate of ~105 MB, because of the STOP_AT_END cut (§T.8).

**Not covered** (§T.4.3, plus the capture's path past N):
- the `+0x63` CPU flags themselves (the evidence is `b1f = 3`, each confirm's `+0x63 = 0` and the per-frame
  command-word source), and the cursor and class bytes (the picks are visible only in the frames; c0/c1 = 2/6
  are logged but their mapping to the class bytes is not derived);
- the physical keyboard, the `E0` grey/keypad distinction, typematic repeat (spec §7 Q4), joysticks;
- the other two-player entries: RIGHT PLAYER ARCADE + P1 join, the attract double start, the mid-fight join
  (`0x28DA4`), 2 PLAYER HANDICAP, training/tug/endurance (U8);
- everything past X (round end, match end, continue/challenge offers with two humans, winner/loser credits): the
  capture's tail after X is unexplained by construction and the port never ran it;
- run-to-run determinism under two-human input (one capture, Decision 3).

**The full gate, before the rebase onto E3** (`make verify` on `c80c7b7`'s pre-rebase head with the t=u7 overrides, log `/tmp/gameplay-u7/final_verify.txt`,
577 lines):
- `verify-exit=0`; the 45 oracle lines equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`;
- all five gp oracles ok: gp-idle-loss 2064/8320, gp-u5-charsel 516/1513, gp-u6-moves-b 1005/2262/2949,
  gp-keys-fight effects 11 of 11, and gp-twop as §T.10 (`two-human match: ok`, `matches the pin`, `first
  unexplained 612, ratchet N 612 ok`, `0 differing through 1505; ratchet N 1506 ok`); the first four equal the
  baseline's lines (`base_gp_lines.txt`);
- `diff-verify: 6/6 functions VERIFIED; 7/7 mutants detected`, `python3 tools/port_progress.py` `771 1203 64` and
  `731 731 100`, all equal to the baseline;
- the unit-test line of `verify` `Ran 171 tests ... OK` (the baseline's count on this base);
- `make audio-render AUDIO_WAV=/tmp/pr_u7.wav` (`verify` does not render it under that override): `cmp` against
  `before-t2.wav` is silent;
- `git diff --stat 1142462 -- port/src` (the branch's merge base) is empty. `git diff --stat main -- port/src` is
  not: `main` has since merged E3 (`834b703`, which edits `port/src`), none of it U7's. Re-check against `main`
  after the merge.

**The full gate after the rebase onto E3** (`make verify` on `5c34f5b` (main `834b703` + U7), the t=u7m overrides,
log `/tmp/pr_u7m_verify.txt`, 690 lines, `/tmp/pr_u7m_rc.txt`; run before the moves pin of the final-review fix,
which adds one printed line and one pin and changes no port code):
- `verify-exit=0`, `ORACLES-EQUAL` (the 45 oracle lines), `WAV-EQUAL` (`/tmp/pr_u7m.wav`, rendered by `verify`,
  `cmp` against `before-t2.wav` silent);
- the five gp oracles ok: gp-idle-loss 2064/8320, gp-u5-charsel 516/1513, gp-u6-moves-b 1005/2262/2949,
  gp-keys-fight effects 11 of 11, gp-twop 612/1506 (`two-human match: ok`, `matches the pin`);
- `diff-verify: 13/13 functions VERIFIED; 17/17 mutants detected; 1 named gaps; 0/5 rows with callees closed (8
  have none)` (the E3-era line, replacing the pre-E3 6/6 and 7/7 above);
- `entry-triage: 579 candidates (575 by U0's rule) ...; targets 323 unported, 172 ported; supplement 131 (31
  unported, 0 stale); untrusted entries 30`;
- `python3 tools/port_progress.py`: `771 1203 64` and `731 731 100`; the unit-test lines `Ran 171 ... OK`.

**The final-review fix** (§T.10: the moves pin; `make gp-twop-oracle GP_DUMP=/tmp/pr_u7_gp` `exit=0` with `moves: 0
differing through 1505; ratchet N 1506 ok`; `GP_TWOP_MOVES_MIN_FIRST=1507` fails as unreachable; a changed `c1`
fails the moves claim) is gated by the task gate in `task-7-report.md`.
