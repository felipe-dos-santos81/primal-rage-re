# Gameplay U8 (other modes): derivation record

Plan: `docs/superpowers/plans/2026-10-01-gameplay-u8-other-modes.md`. Specs:
`docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 (track G, U8)
and `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md` §3.3,
§3.4, §7 Q5/Q7. §U8.0–§U8.9 are the planner's evidence (2026-10-01, planning
worktree `reverse-plans` at `e9271df`); plan Task 1's `TestRowsFromTheRaw`
re-reads §U8.1–§U8.2's setters, divert arguments and credit calls from the exe,
and each task appends its section from `§U8.10` on (Task 0 §U8.10 … Task 5
§U8.15, Tasks 6a–6g §U8.16–§U8.22, Task 7 §U8.23). Raw wins over this text and
over the plan; a correction is recorded where it is found.

**Re-baseline (2026-10-01, main `8eaf25a`: U5 `b09b9e6` and U6a merged).** The
raw facts of §U8.1–§U8.6 and §U8.8–§U8.9 do not depend on the port and are
unchanged. The port-side observations were re-run on `8eaf25a` and are added in
place, marked "re-baseline": the row and attract-start previews (§U8.3) and the
plumbing proof (§U8.7, whose frame/trace values and mutation premise changed).
U6b and U7 merge before U8; their effect on these port-side values is measured
by the plan's own tasks, not here.

## §U8.0 Sources and method

- **Image.** `build/diffrun --exe data/game/C/PRAGE.EXE --image-out $S/image.bin`
  (the fixed-up image from `0x10000`; sha256
  `0cfd6f481182f050d5897871068bb9f8fa4410108a56cba0b17be7d513943dec`, 1 028 304
  bytes; exe sha256 `eecba701…0e91b`). Disassembly: capstone over that image
  (helper `img.py`, §U8.A). Immediates and `call rel32` targets are the same in
  the raw file (no fixup touches them), which is what `test_gp_modes.py` reads
  (file offset `VA + 0x52E54`).
- **Strings.** `data/game/C/ENGLISH.TXT` decoded with `strs.py` (§U8.A).
- **Ghidra (live MCP, project `rage`, read-only).** Decompiles of `0x438B4`,
  `0x43B24`, `0x43AAC`, `0x44798`, `0x4434C`, `0x4454C`, `0x444C8`; cross-references
  to `0x104B1D` (27), `0x104AB8` (9), `0x1088D0` (14). The live project has no
  function at `0x4367C` ("Function not found"; the raw disassembly is used). The
  committed plan and tools depend on none of these: each fact below is also
  stated as raw bytes or addresses.
- **Captures read (read-only).** `data/k11-captures/gp-idle-loss` (`poll.log`
  sha256 `773e2647…c8447`, 8 173 frames, 376 MB) and `gp-pads`.

## §U8.1 START MENU and MAIN MENU: tables, strings, setters

`0xBCCCC` (dwords, fixed-up image; `+0`/`+4` string ids, `+8` callback, `+C`):

```
BCCCC 00000215 00000000 00000000 00000000   "START MENU"
BCCDC 00000017 00000216 0002CBC4 00000001   "LEFT PLAYER" "ARCADE"
BCCEC 00000016 00000216 0002CBDC 00000001   "RIGHT PLAYER" "ARCADE"
BCCFC 00000017 00000217 0002CBF4 00000001   "LEFT PLAYER" "TRAINING"
BCD0C 00000016 00000217 0002CC0C 00000001   "RIGHT PLAYER" "TRAINING"
BCD1C 00000218 00000000 0002CC24 00000001   "TUG OF WAR"
BCD2C 00000219 00000000 0002CC3C 00000001   "ENDURANCE"
BCD3C 00000211 0000021A 0002CC54 00000001   "Start" "2 PLAYER HANDICAP"
BCD4C 00000000 00000000 00000000 00000000   end of list (7 items)
```

The setters `0x2CBC4 + 0x18·k` (`push edx; mov edx,M; xor ah,ah | mov ah,N;
mov word [0x104b00],dx; mov byte [0x104b1d],ah; pop edx; ret`):

| row | label | setter | mode `DS_00104B00` | `DS_00104B1D` |
|---|---|---|---|---|
| 0 | LEFT PLAYER ARCADE | `0x2CBC4` | `0x2D` | 0 (`30 e4`) |
| 1 | RIGHT PLAYER ARCADE | `0x2CBDC` | `0x2E` | 0 |
| 2 | LEFT PLAYER TRAINING | `0x2CBF4` | `0x28` | 1 (`b4 01`) |
| 3 | RIGHT PLAYER TRAINING | `0x2CC0C` | `0x29` | 1 |
| 4 | TUG OF WAR | `0x2CC24` | `0x2A` | 2 |
| 5 | ENDURANCE | `0x2CC3C` | `0x2B` | 3 |
| 6 | Start 2 PLAYER HANDICAP | `0x2CC54` | `0x2C` | 4 |

This equals spec §3.3 and U7's §T.1.1. The MAIN MENU `0xBCBDC`: title
"MAIN MENU", rows `0xBCBEC` "Start" (`0x2CB74`: `menu_step(0xBCCCC, 0x10, 0)`,
per frame) and `0xBCBFC` "GAME OPTIONS" (`0x2CB94`: `0x2FA40(0xBCC1C)`, the
blocking `menu_run` of the OPTIONS MENU, whose nine rows are `0xBCC2C..0xBCCAC`:
CONFIG OPTIONS, STATISTICS, SOUND TEST, MUSIC TEST, MODIFY CONTROLS, CONFIGURE
KEYBOARD, TEST CONTROLS, ADJUST VOLUME, 2 PLAYER HANDICAP).

**Against the U4 capture** (`rows_check.py`, §U8.A): the only START MENU row
`gp-idle-loss` takes is row 0:

```
poll.log:324 S f=141 mode=27 ent=2A2BEC (image BCBEC) b1d=0 b1f=0 cred=5    MAIN MENU
poll.log:481 S f=1DA mode=27 ent=2A2CDC (image BCCDC) b1d=0 b1f=0 cred=5    START MENU open
poll.log:631 P f=26E mode=2D                                                 row 0's setter
poll.log:633 S f=26F mode=1A ent=2A2CDC (image BCCDC) b1d=0 b1f=1 cred=4    the divert
poll.log:671 S f=293 mode=10 ent=2A2CDC (image BCCDC) b1d=0 b1f=1 cred=4    character select
```

`ent` is `DS_0010741C`, `menu.c`'s `MENU_ENTRIES` (the list's first item), not
the cursor: it tells which menu is open, never which row is selected. The
cursor (`DS_0010742C`/`DS_00107430`) is not a poll.log field, so a row is proven
by the mode its setter stores (below: each of `0x28..0x2E` has exactly one
store) plus `b1d`/`b1f`/`cred`. The row mode lives one iteration (`S(0x26E)` was
missed, the `P` record has it), so the check accepts a `P` record.

## §U8.2 The mode switch and the game-start handlers

`0x24EEC mov ax,[0x104b00]; cmp ax,0x33; ja 0x2540F; 0x24F01 jmp [eax*4 +
0x24B8C]`. Entries `0x28..0x33`: `0x24F09 0x24F66 0x24FC4 0x25187 0x2501E 0x25071
0x250CE 0x2512B 0x2519E 0x251A8 0x251B2 0x251BC`.

| mode | handler | config decode `0x2D974(0x29)` | `DS_00104AB8` | credits `0x2CA7C(1)` | `0x257A4(arg)` |
|---|---|---|---|---|---|
| `0x28` | `0x24F09` | yes | 1 (`0x24F38`) | — | 3 (`0x24F51 mov eax,3`) |
| `0x29` | `0x24F66` | yes | 2 (`0x24F7E`) | — | 3 (`0x24FAF`) |
| `0x2A` | `0x24FC4` | yes | 3 (`0x2500E`) | — | 3 (`0x25002 mov ebx,3; mov eax,ebx`) |
| `0x2B` | `0x25187` | no | 3 (`0x2518E`) | — | 3 (`0x25187 mov edx,3; mov eax,edx`) |
| `0x2C` | `0x2501E` | yes | — | — | 3 (`0x2505C`) |
| `0x2D` | `0x25071` | yes | — | `0x250BA` | 1 (`0x250BF`) |
| `0x2E` | `0x250CE` | yes | — | `0x25117` | 2 (`0x2511C`) |
| `0x2F` | `0x2512B` | yes | — | `0x25173` | 2 (`0x25178`) |

The decode stores `DS_00104528` (the field), `DS_00105B3A` (bit 8 >> 4),
`DS_0010452C` (bits 4..7) and `DS_001088D0 = 5·(field & 0xF) + 0x1E`.

`0x257A4(mask)`: voice `0x100`, `0x2BAF4(0)`, clears `DS_00104B17/19/11/1B/15`,
**`0x257E0 mov [0x104b1f],dl`** (the mask: bit 0 side 0, bit 1 side 1), resets
both slots (`0x33C18`), `0x46594`, `0x65490(0x104B02)`,
`DS_00104AE4 = 0x4367C` (`0x25806..0x25810`), **`0x25816 call 0x4F980` with
`EAX = 0x10`**: `0x4F980` stores the return mode `DS_00104AFA = 0x10`
(`0x4F98E`) and mode `0x1A` (`0x4F989..0x4F994`), then voice `0x53`.

`0x2CA7C(n)`: FREE PLAY (`DS_00105D60`) returns 1; `n > DS_00105C00` returns 0;
**the debit `0x2CA9C` runs only when `DS_00104B1F == 0`** (`0x2CA93`). In modes
`0x2D`/`0x2E` the call precedes the divert, so it debits (U4: 5 → 4).

**Credits per row** (zero CMOS: 5 credits, FREE PLAY 0; spec §3.4): rows 0 and
1 spend one; rows 2–6 spend none (no `0x2CA7C` in their handlers: the `call`
scan of `0x24F09..0x2519E` finds exactly `0x250BA 0x25117 0x25173`). The
harness does nothing about credits: every capture boots with the zero CMOS
(`k11_capture.check_cmos`), so each starts at 5, and the port's `game_init`
sets the same (the trace compares `cred`).

**Who stores which mode** (`modescan.py`, §U8.A: every `mov word [0x104b00]`
and `[0x104afa]` with its immediate source; `wipescan.py`: every `call 0x4F980`
with its `EAX`). `0x28..0x2E` are each stored once, by their setter
(`0x2CBFC 0x2CC14 0x2CC2C 0x2CC44 0x2CC5C 0x2CBCC 0x2CBE4`). **`0x2F` has no
immediate store** in either scan: it is not on any menu row and U8 does not
reach it (named, §U8.9). `0x30` comes from `0x25A4C` (`0x4F980(0x30)`, in
`0x259CC`, the endurance path) and `0x285E3`; `0x31` from `0x2957F`/`0x29965`;
`0x10` also from `0x28D79`/`0x28D9B` (the mid-match join, U7's).

## §U8.3 How the walk reaches a row, and the attract start

**The MAIN/START menus are per-frame.** Mode `0x27`'s handler `0x251C6..0x25215`
calls `0x50146(0xC000C000, 0x1E, 0xF)` (the repeat mask, delay, period) and then
`menu_step(0xBCBDC, 0x10, 4)` every iteration; `f` advances there (the U4
capture holds an `S` record for every `f` from `0x141` to `0x26D` but none in a
blocking loop).

**A move.** `menu_step` (`0x2FFC4`, `menu.c`) reads `0x2EEC8(0xC300C000, 1)`:
`input_select_bits` (`0x50161`) returns a level bit once per press (the latch
`DS_000E1C38`, cleared when the level drops, `0x500C4`), ORed with the latched
BIOS key's flags (`0x2EBF0`). `keys & 0x40004000` (either player's down) moves
the cursor down (`0x3055E..0x305C5`), `keys & 0x80008000` up, wrapping from row 0
to `count − 1` (`0x30511..0x3052F`; count 7, §U8.1). The repeat counter
`DS_000E1C40` is re-armed to `0x1E` by `0x50146` in every mode-`0x27` iteration
(`0x50152`), so a held direction never repeats there; a hold of 4 frames gives
the level 4 frames (`gp-pads` §G.7.2: the level lags `raw` by one) and exactly
one move. **The select.** The Enter's BIOS word latches `0x0D` (`0x24D4D`), which
`0x2EBF0` turns into `0x1000000`; `0x3046F..0x304A2` calls the selected item's
`+8`, the setter.

**The walk per row** (P1's pads, spec §3.2): row k (1..5) is k × `p1.down`;
row 6 is one `p1.up` (the wrap). The pad presses also queue their BIOS words
(Q4, ruled yes, record §G.2): `p1.down` `2D78` ('x'), `p1.up` `1F73` ('s'),
`p1.start` `3B00`. All are inert: the key loop `0x24D08..0x24EE7` acts on ascii
`0x0D` (`0x24ECF`, the mode-3 Enter), `0x1B` (`0x24E9E`), `0x20` (`0x24DE9`,
pause outside modes 3/`0x27`) and, for ascii 0, scans `0x10`, `0x1F`,
`0x20..0x24`, `0x32` only; `0x2EBF0` maps only `0x48 0x50 0x4B 0x4D 0x0D 0x1B`.
U7 §T.3 reaches the same conclusion for its words.

**Port preview** (not evidence for the original; a hand-built v2 script per row
in a scratch build of `e9271df` + the plan's driver change: Enters at 321 and
471, presses 60 apart from 531, each `bits <F> 4000|8000` for 4 frames with its
BIOS word at `F`, the Enter 60 after the last move, `end 2900`; `run_tests`
48–49 s each):

| row | mode at | divert `b1d`/`b1f` | `cred` | mode 6 at | misses (beyond the base pair) |
|---|---|---|---|---|---|
| 1 | `0x24F` `0x2E` | 0 / 2 | 5 → 4 | `0x7B5` | `0x29D60`, `0x5D812` (frontend_mode_1b_step), `0x14EF8`, `0x14F50` (hit_reaction_apply), `0x3A588`, `0x3640C` |
| 2 | `0x28B` `0x28` | 1 / 3, then 1 from `0x652` | 5 | `0x7F5` | `0x29D60`, `0x5D812`, `0x3640C`, `0x3A588` |
| 4 | `0x303` `0x2A` | 2 / 3 | 5 | `0x875` | `0x29D60`, `0x5D812` |
| 5 | `0x33F` `0x2B` | 3 / 3 | 5 | never (mode `0x10` to `f = 2900`) | `0x29D60` |
| 6 | `0x24F` `0x2C` (one up) | 4 / 3 | 5 | `0x7B5` | `0x29D60`, `0x5D812` |

(Row 3 to `0x10` only: `0x2C7` `0x29`, `b1d=1 b1f=3`.) The port takes every
row's raw path with the derived walk; its miss sets are previews, the plan pins
the ones measured on each capture's replay.

**The attract start** (spec §3.4's "second way into a match"). Mode 3's
handler is `0x25238 call 0x11D04`. `0x11D04..0x11D49`: with `DS_00104B1D == 0`
(`0x11D07..0x11D11`), `0x11F28(0)` and `0x11F28(1)` give the side mask in DL
(1, 2 or 3); a non-zero mask calls `0x32970(0)` and **`0x257A4(mask)`
(`0x11D41`)**. `0x11F28(side)`: `0x2C060` (a credit or FREE PLAY), `DS_001088E4
& [0x9ACBC + 4·side]` (the newly-pressed word from `0x4F644`; masks
`0x01000000`/`0x00000100`: F1/U and F2/Home, spec §3.2), then `0x11F47
0x2CA7C(1)` (debits: `DS_00104B1F` is still 0) and returns 1. So P1's F1 held
in mode 3 diverts with `b1f = 1`, `b1d = 0`, credits 5 → 4, **without** the
`0x2D974(0x29)` decode of §U8.2: `DS_001088D0` is written only at `0x20CB0`,
`0x24B48` and the seven handlers (Ghidra xrefs), so the attract start's match
runs with whatever `DS_001088D0`/`DS_00104528`/`DS_0010452C`/`DS_00105B3A` held —
the one difference from LEFT PLAYER ARCADE that its fight can show. Timing: the
press is sampled at iteration F (raw), reaches the level at F + 1, where `0x4F644`
then `0x11D04` see it: **mode 3 at F, the wipe from F + 1.** Port preview (hand-built
`arm pad` script, `bits 320 0100` held 4, `key 322 3B 00`): `f=140 mode 3`,
`f=141 mode 1A cred 4 b1f 1`, `0x10` at `0x165`, mode 6 at `0x6B5`; misses
`0x29D60`, `0x5D812` (frontend_mode_1b_step), `0x23208`, `0x3A588`.

**Re-baseline previews (`8eaf25a`).** The same hand-built scripts (rows: Enters
at 321 and 471, presses from 531 60 apart held 4 with their BIOS words, the
Enter 60 after the last move, `end 2900`; the attract start: `arm pad`, `bits
320 0100`, `key 322 3B 00`, `bits 324 0000`, `end 2600`), replayed by a scratch
copy of `port/` at `8eaf25a` with plan Task 5's driver edits (scratch helper
`apply.py`, not committed). Mode entries (from `trace.txt`) and the misses beyond
the base pair:

| run | row mode at | divert `b1d`/`b1f`, `cred` | `0x10` at | mode 6 at | misses beyond the base pair |
|---|---|---|---|---|---|
| row 1 | `0x24F` `0x2E` | 0 / 2, 5 → 4 | `0x274` | `0x7B5` | `0x29D60`, `0x5D812` (frontend_mode_1b_step), `0x14EF8` and `0x14F50` (hit_reaction_apply, 4 hits each), `0x15510` (anim_indirect, 1 hit) |
| row 2 | `0x28B` `0x28` | 1 / 3, then 1 by `0x756`; 5 | `0x2B0` | `0x7F5` | `0x29D60`, `0x5D812` |
| row 3 | `0x2C7` `0x29` | 1 / 3, then 2 by `0x796`; 5 | `0x2EC` | `0x835` | `0x29D60`, `0x5D812` |
| row 4 | `0x303` `0x2A` | 2 / 3; 5 | `0x328` | `0x875` | `0x29D60`, `0x5D812` |
| row 5 | `0x33F` `0x2B` | 3 / 3; 5 | `0x364` | never (`0x10` to `f = 2900`) | `0x29D60` |
| row 6 (one up) | `0x24F` `0x2C` | 4 / 3; 5 | `0x274` | `0x7B5` | `0x29D60`, `0x5D812` |
| attract | — (mode 3 at `0x140`) | 0 / 1, 5 → 4 (`0x141`) | `0x165` | `0x6B5` | `0x29D60`, `0x5D812` |

Every path equals the `e9271df` preview where both have it. The misses shrank by
U6a's ports (`0x3A588`, `0x3640C`, `0x23208` are registered, record
gameplay-u6 §U6.21); `0x15510` (row 1) is new against the `e9271df` preview,
cause not isolated (the row-1 fight runs a different path once the U6a
callbacks act). As before, these are previews: the plan pins the sets measured on
each capture's replay (Task 6 Step 3) on the base U8 runs on.

## §U8.4 Where each row leads (the first distinct screens)

- **Character select entry.** `0x4367C` (the `DS_00104AE4` hook): with
  `DS_00104B1D == 3` → `0x4454C` (endurance); else `0x43691..0x43733` then
  `0x43738`, which sets `DS_00108170[side] = 1` for each side whose `DS_00104B1F`
  bit is set (`0x4377F..0x437A6`), the countdown `DS_0010816C = 0xF`
  (`0x437D1`; 5 when `DS_00108173`), and `DS_00108174 = 0`.
- **Mode `0x10`** (`0x25385 call 0x438B4`): `DS_00104B1D == 3` runs the team select
  `0x44798`, else `0x43B24` (Ghidra decompiles). `0x43B24`: a side in state 0
  can join with `0x11F28` (U7's); state 1 with `DS_00104B1D == 1` and side + 1 ≠
  `DS_00104AB8` runs `0x435AC` (the training row's other side), else `0x43464`;
  every `f & 0x3F == 0` (`0x43CF0..0x43D01`) `0x43AAC` decrements the countdown
  and at 0 auto-picks every side in state 1 (`0x43D60`) and wipes to `0x11`
  (`0x43AEE`). So rows 1–4 and 6 reach the fight with nobody pressing (both
  sides of a `b1f = 3` row are picked by the time-out), like U4's row 0 (§G.18).
- **Endurance** (`b1d = 3`): `0x4454C` → `0x444C8` (both sides state 1, the
  hook `0x29D60`, a bare `ret`), then `0x44798` every frame: cursor moves on
  `DS_001088E0/E2 & 0xF0`, a pick on bit 0 (`0x4434C`, four slots per side, a
  repeated class un-picks), and **the only exit is `0x4482A 0x4F980(0x11)` once
  both sides reach state 3**. `0x44798..0x4493A` calls only `0x44638`,
  `0x4418C`, `0x442A0`, `0x4434C` and `0x4F980`; no `0x43AAC`, no countdown
  decrement. **Idle, ENDURANCE stays in the team select** (port preview: mode
  `0x10` from `0x364` to the script's end at 2900). Its fight (`0x259CC`:
  `0x4F980(0x30)` at `0x25A4C`, then mode `0x31`) needs eight picks.
- **The fight.** Mode 6's case `0x25256`: `DS_00104B1D != 0` jumps to
  `0x26254` directly, skipping `0x28CC8`'s join poll; only the arcade rows
  (`b1d = 0`) poll for a second player.

## §U8.5 The evidence checks (`tools/gp_modes.py`)

Per scenario, over `poll.log`: the row's mode appears (`S` or `P`); no other
mode of `0x28..0x2F`; START MENU was open (`S` in `0x27` with `ent − (base −
0x80000) == 0xBCCDC`) before it; the first `S` in `0x1A` after it shows the
row's `b1d`/`b1f`; `cred` before minus the row's spend equals `cred` there;
mode `0x10` follows; the reach mode (6, or `0x10` for ENDURANCE) follows; the
`X` record exists; ENDURANCE additionally stays in `0x10` to the end. The
attract start replaces the first three with: no mode `0x27`; the P1 start bit
(kb `0x0100`) first reaches the bitmap in an `S` record in mode 3 with
`S(f − 1)` in mode 3; the first mode after 3 is `0x1A`. On `gp-idle-loss` as
row 0 every check is `ok` (the row mode at `f=26E (P)`, divert `f=26F`, 5 → 4,
`0x10` at `0x293`, 6 at `0x7F5`, `X` at `0x207F`), and checking it as row 1
fails seven of eight lines (planner run, §U8.A).

## §U8.6 Timeline, time limits and storage (from `gp-idle-loss`)

`poll.log` (ms from the poller's start): mode `0x27` at `f=141` `ms=25031`
(Enter pressed at `ms=25000`); row 0's mode at `f=26E` `ms=30155`; `0x10` at
`f=293` `ms=30953`; 6 at `f=7F5` `ms=55059`. So: select → mode 6 = 24.9 s,
select → `0x10` = 0.8 s, `0x10` → 6 = 24.1 s, 60 frames ≈ 1 s in the menus
(`f 141 → 26E`: 301 frames in 5.05 s).

Time limits (harness values): a row with k moves selects at `0x27 + 150 +
60(k + 1)` frames, ≈ 25.03 + 3.5 + k s; mode 6 ≈ 24.9 s later; the end 300
frames (5 s) in: **58.4 + k s, + a 6 s margin → `65 + k`** (rows 1–4: 66–69;
row 6, one move: 66). ENDURANCE (5 moves) ends 300 frames into `0x10`: 25.03 +
3.5 + 5 + 0.8 + 5 = 39.3 s → **46**. The attract start: press at 25.0 s, `0x10`
≈ 25.8 s, 6 ≈ 49.9 s, end ≈ 54.9 s → **61**. The margin covers the pick
time-out's 64-frame phase (`f & 0x3F`, ≈ 1.07 s) and host-timed loads; a run
whose `end frame reached` fails is rerun once with `--time-limit` + 15 (U4's
rule).

Storage (`seg.py`, §U8.A: the capture's frames per segment of `gp-idle-loss`,
by `ms → raw` at 70.0866 fps): before the Enter 89 frames 2.9 MB; MAIN+START
menus 0.3 MB; the divert and wipe 0.8 MB; character select 449 frames 26.7 MB;
the wipes and countdowns `0x1A..0x17` 2.1 MB; round start 3.9 MB; mode 6, 600
frames (10 s) 33.4 MB (3.34 MB/s). A capture's frames run to DOSBox-X's time
limit, not to the `X` record (`gp_capture.write_frames` writes every distinct
AVI frame after the post-logo start), so the fight part is `time_limit − t(6)`
≈ 11.6 s ≈ 38.7 MB. **Estimates: rows 1–4 and 6 ≈ 75 MB each; ENDURANCE ≈
24 MB (11.7 s of team select at the character select's 1.69 MB/s); the attract
start ≈ 73 MB; total ≈ 475 MB** (U4's whole idle run: 376 MB). The port dump of
one replay (deleted after its comparison): ≈ 52 MB at `end 2325` (planner
plumbing run), 77–89 MB at `end 2900` (previews).

Runtime: the plumbing run (§U8.7) took 42 s for the check, a replay to
`f = 2325` and both claims; each scenario adds about that to `make verify`.

## §U8.7 Plumbing proof on real data (planner run)

The plan's `gp-modes-oracle`/`gp-modes-one` targets, in a scratch copy, on
`gp-idle-loss` as a reference row cut where a row capture ends:

```
make gp-modes-oracle GP_DUMP=$S/gpd2 GP_MODES_KEEP=1 GP_MODES_SCENARIOS="REF:gp-idle-loss" \
  GP_MODES_REF_END=2325 GP_MODES_REF_MIN_FIRST=203 GP_MODES_REF_TRACE_MIN_FIRST=2088 \
  GP_MODES_REF_MAX_START=90 GP_MODES_REF_CAPTURE_SHA256=773e2647…c8447 GP_MODES_REF_CAPTURE_FRAMES=8173
```

→ `exit=0 42s`; the eight `gp_modes` lines `ok`; the driver `all checks
passed` (`fn-miss PR_GP_DUMP distinct=5`, within the idle-loss set for a cut
script); `gp_compare … window from capture 90 (raw 1744) … FIRST UNEXPLAINED
capture 203 … ratchet N 203 ok`; `trace: first difference f=828 (2088) in rng …
ratchet N 2088 ok`; dump 52 MB. `GP_MODES_SCENARIOS="RA:gp-u8-right-arcade"`
with no capture under `PR_ORACLE_REQUIRED=1`: `gp-modes-oracle: no capture at
data/k11-captures/gp-u8-right-arcade, skipped`, exit 0.

**Re-baseline (`8eaf25a`).** The values above are `e9271df`'s: 203 was U4's
divergence 1 (fixed by U5, record gameplay-u5 §C5.12) and 2088 the `rng`
difference after the `0x23208` miss (ported by U6a, record gameplay-u6 §U6.21).
The same cut replay on `8eaf25a` (`gp_session.py port-script --scenario
gp-idle-loss --end 2325`, then the `PR_GP_DUMP` driver; equal to `make gp-replay
… GP_SCRIPT_ARGS="--end 2325"`, which was also run): `fn-miss PR_GP_DUMP
distinct=4 dropped=0` (the base pair, `0x29D60`, `0x5D812
frontend_mode_1b_step`), `all checks passed`; 818 port frames, the last `00817
f=0915 tick=00000973 mode=0006`; dump 52 MB. `gp_compare --report`: `window from
capture 90 (raw 1744)`, `FIRST UNEXPLAINED capture 1070 (raw 4182): nearest port
817, rows 121..199, x 0..319 (10696 px)` — the capture frame after the port's
last one (port 817 is `f = 0x915`, the cut), so 1070 is how far the cut replay
got, not a divergence; `trace: 2001 frames compared (f 141..), 4 without a
capture snapshot`, `0 differing through 2325`. Ratchet runs (`gp_compare.py`
with `--max-start 90`, the sha256 and 8173 frames pinned): N 1070 / F 2326 →
`first unexplained 1070, ratchet N 1070 ok`, `0 differing through 2325; ratchet
N 2326 ok`, rc 0; N 1071 → `frames: FAIL: first unexplained 1070 < ratchet N
1071`, rc 1; F 2327 → `trace: FAIL: N 2327 > end 2326: N is unreachable`, rc 1;
the planner's N 203 and N 204 (F 2088) both rc 0, `ratchet N 204 ok (improved:
raise N)` and `ratchet N 2088 ok (every item is explained: N = 2326 is the exact
pin)` — so the plan's original mutation (204) could not fail on a base holding
U5. By `gp_compare.ratchet` (`tools/gp_compare.py:258`): with a first frame `j`
an N fails iff `N > j`; with no trace difference an N fails iff `N > end` (end =
the last `T` `f` + 1). N = `j` + 1 and F = end + 1 are therefore the smallest
values that fail, and the plan's Task 3 Step 4 uses them.

## §U8.8 Spec Q5 (keys inside blocking loops): not needed by U8

Every U8 input is in a per-frame loop: the MAIN/START menu keys and pads (mode
`0x27`, §U8.3; the U4 capture snapshots every `f` there) and the attract
start's F1 (mode 3, where the master loop runs: `gp-idle-loss` has `S` records
for every `f` from 4 to `0x140`). No key follows the select. K11 handled the
blocking OPTIONS MENU with tick-keyed steps (`k11_session`, `dtick`); U8
excludes the OPTIONS MENU (§U8.9), so `tools/gp_session.py` gets no tick-keyed
step. A unit that keys a blocking loop (the OPTIONS MENU, the quit prompt
`0x249F0`, the pause `0x24DE9`) needs one.

## §U8.9 Exclusions and named gaps

| path | why U8 leaves it |
|---|---|
| GAME OPTIONS → OPTIONS MENU and its nine rows | K11 `walk` (enforced `k11-oracle`); a blocking `menu_run` (§U8.8) |
| START MENU Esc / `p1.b1` (`-5`, back to MAIN MENU) | K11 `walk` ("enter enter down down esc") |
| MAIN MENU Esc → `-1` → `0x2520B` restart | K11 `menuesc` (enforced; named-gaps-b §B.3 pins `0x30430..0x30440`); `p1.b1` reaches the same arm (`keys & 0x2000000`, `gp-pads` run 1, §G.7.1) |
| the menu idle time-out `0x2EBB3` restart | K11 `idle` (report-only, named-gaps-b §B.6) and `gp-pads` run 2 (§G.7.3) |
| `diags`, `de` | need pokes |
| the quit prompt (Esc in mode 3, `0x249F0(0)`), pause (Space) | key-loop keys outside the START MENU: U11 |
| mid-match and character-select joins (`0x28CC8`, `0x43B4E`) | U7 |
| mode `0x2F` | no store of `0x2F` found (§U8.2): not reached from a menu |
| ENDURANCE's fight (modes `0x30`/`0x31`) | needs the team-select picks (decision D3; §U8.4). Re-baseline: D3 named U5 as the owner, but U5 merged without the team pass and its record (gameplay-u5 §C5.19) hands both the team pass `0x44798` and the ENDURANCE fight to U8: the gap has no assigned owner (for the controller) |
| what a row's fight does past 300 frames | not captured (short captures, brief); `gp-idle-loss` covers the arcade match |

## §U8.A Scratch helpers (planner; not committed)

`img.py`: `IMG = open(image.bin).read(); BASE = 0x10000; b(a, n) = IMG[a −
BASE : a − BASE + n]; d(a)` a dword; `dis(a, end)` capstone `CS_MODE_32` over
`b`. `strs.py` (ENGLISH.TXT, groups of 0x40, XOR-coded Pascal strings):

```python
T = open('data/game/C/ENGLISH.TXT', 'rb').read()
def s(i):
    off = 0
    for g in range(i // 0x40): off = struct.unpack('<I', T[off + 4:off + 8])[0]
    p = off + 8
    for k in range(i % 0x40): p += T[p] + 1
    n = T[p]; return bytes(c ^ n for c in T[p + 1:p + 1 + n]).decode('latin1')
```

`modescan.py`: every occurrence of the dword `0x104B00`/`0x104AFA` in the code
object decoded as `mov word [X], src`; for a register source, the last write to
it in the 0x60 bytes before. `wipescan.py`: every `e8 rel32` whose target is
`0x4F980`, with the last write to EAX/AX/AL in the 0x40 bytes before.
`rows_check.py`: one line per change of `(mode, ent, b1d, b1f, cred)` in the U4
`poll.log` up to `f = 0x2A0`. `seg.py`: frames and MB of
`data/k11-captures/gp-idle-loss` between two `f` values (`raw = ms·0.0700866`,
`window.txt`, the `frame_*.raw.gz` sizes). Outputs are quoted above.

## §U8.10 Baseline (Task 0)

Base: branch `gameplay-u8` at
`git rev-parse HEAD` = `e4d03a74db706a153f9a5ee5e82c8e5366404566` (main after U5, U6a, E2, U6b, capture-hygiene,
U11, E3 and U7). Per the controller's speed-up ruling the baseline is not a full
`make verify` (main is U7's fully verified head plus its merge): it is the five gp
oracles, `diff-verify`, the tool unit tests, `entry-triage` and
`port_progress.py`, each run from the worktree root with the plan's `/tmp/pr_u8_*`
overrides. Logs in `/tmp/gameplay-u8/` (`t0_gp.txt` 183 lines, `t0_diff.txt` 146,
`t0_tooltests.txt` 11, `t0_e2.txt` 22); there is no `t0_verify.txt`, so the
brief's `grep -c . $S/t0_verify.txt` has no value on this base.

The four gate results:

1. `make gp-oracle gp-charsel-oracle gp-moves-oracle gp-keys-oracle gp-twop-oracle GP_DUMP=/tmp/pr_u8_gp`: exit 0; every replay `all checks passed` and `fn-miss PR_GP_DUMP distinct=4 dropped=0` (pairs `0x5D812 actor_spawn`, `0x5D812 set_dead`, `0x29D60 frontend_mode_1b_step`, `0x5D812 frontend_mode_1b_step` in each of the five).
2. `make diff-verify DIFF_IMAGE=/tmp/pr_u8_diffimg DIFF_TABLE=/tmp/pr_u8_diff.md`: exit 0, `diff-verify: 13/13 functions VERIFIED; 17/17 mutants detected; 1 named gaps; 0/5 rows with callees closed (8 have none).`
3. Tool unit tests (`verify`'s k11/gp line: `test_k11_fields test_k11_session test_k11_capture test_k11_compare test_gp_session test_gp_capture test_gp_compare test_gp_moves test_gp_keys`, `PR_ORACLE_REQUIRED=1`): `Ran 171 tests`, `OK`. `make entry-triage E2_IMAGE=/tmp/pr_u8_e2img`: exit 0 (`Ran 44 tests`, `OK`; the committed table equals a fresh run; `579 candidates`, `targets 323 unported, 172 ported`).
4. `python3 tools/port_progress.py`: `771 1203 64` and `731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)` (unchanged from `8eaf25a`).

The gp ratchet lines (the base's own; every later gate compares against these):

```
gp_compare: gp-idle-loss: frames: first unexplained 2064, ratchet N 2064 ok
gp_compare: gp-idle-loss: trace: 0 differing through 8319; ratchet N 8320 ok
gp_compare: gp-u5-charsel: frames: first unexplained 516, ratchet N 516 ok
gp_compare: gp-u5-charsel: trace: 0 differing through 1512; ratchet N 1513 ok
gp_compare: gp-u6-moves-b: frames: first unexplained 1005, ratchet N 1005 ok
gp_compare: gp-u6-moves-b: trace: first differing 2262, ratchet N 2262 ok
gp_compare: gp-u6-moves-b: moves: first differing 2949, ratchet N 2949 ok
gp_keys: gp-keys-fight: evidence: 11 of 11 events ok
gp_keys: gp-keys-fight: effects: first not reproduced 11, ratchet N 11 ok
gp_twop: gp-twop: two-human match: ok
gp_compare: gp-twop: frames: first unexplained 612, ratchet N 612 ok
gp_compare: gp-twop: trace: 0 differing through 1505; ratchet N 1506 ok
gp_compare: gp-twop: moves: 0 differing through 1505; ratchet N 1506 ok
```

`fnm_known`'s parameter list on this base (`grep -n 'static int fnm_known' port/tests/test_platform.c`):

```
199:static int fnm_known(u32 addr, const char *ctx, int frontend, int idle_loss, int charsel, int moves,
200-                     int keys_fight, int twop)
```

Eight parameters. The controller's "refactor first" ruling replaces them, in its
own commit after this one, with `fnm_known(u32 addr, const char *ctx, int
frontend, const gp_set *sc)` and a name-keyed table `k_gp_sets` (`{ name, prefix,
rows, n }`); U8's scenario sets are then added as table entries (the plan's
"scenario-table clause"), never as parameters.

## §U8.11 The evidence checker (Task 1)

`tools/gp_modes.py` (`ROWS`, `check`, `path`, the CLI) and `tools/tests/test_gp_modes.py`, both exactly as the plan's Task 1 gives them; no correction to the plan's text was needed (raw wins: `TestRowsFromTheRaw.test_setters_and_divert_arguments` reads the setters at `0x2CBC4 + 0x18 k`, the seven divert immediates and the three `0x2CA7C` call sites `0x250BA`/`0x25117`/`0x25173` from `data/game/C/PRAGE.EXE` (obj-0 immediates, file offset `VA + 0x52E54`) and agrees with every `ROWS` value).

Tests: `python3 -m unittest tools.tests.test_gp_modes -v` -> `Ran 9 tests ... OK` (the plan's 9, none skipped). Per-task gate: `tools.tests.test_gp_modes test_gp_session test_gp_capture test_gp_compare` `Ran 101 tests OK`; verify's k11/gp line (`PR_ORACLE_REQUIRED=1`, nine modules) `Ran 171 tests OK` (unchanged: `test_gp_modes` is not in that line; Task 3 adds it to `gp-modes-oracle`).

CLI on the U4 capture (read-only), as the plan expects:

```
gp_modes: gp-idle-loss: the row mode 0x2D appears: ok (f=26E (P))
gp_modes: gp-idle-loss: no other game-start mode: ok
gp_modes: gp-idle-loss: START MENU open (ent 0xBCCDC) before the select: ok
gp_modes: gp-idle-loss: the divert: b1d=0 b1f=1: ok (f=26F b1d=0 b1f=1)
gp_modes: gp-idle-loss: credits spent 1: ok (5 -> 4)
gp_modes: gp-idle-loss: the character select (mode 0x10) follows: ok (f=293)
gp_modes: gp-idle-loss: mode 0x6 reached: ok (f=7F5)
gp_modes: gp-idle-loss: the scenario end (X record): ok (f=207F)
rc=0
```

and `--scenario gp-u8-right-arcade` on the same capture: seven `FAIL` lines (`the row mode 0x2E appears`, `no other game-start mode (2D)`, `START MENU open ...`, `the divert: b1d=0 b1f=2`, `credits spent 1`, `the character select ... follows`, `mode 0x6 reached`) and only `the scenario end (X record): ok (f=207F)`; `rc=1`.

Mutations (each alone, restored after; `PYTHONDONTWRITEBYTECODE=1`):

- M1 `gp-u8-tug-of-war` `b1f=3` -> `b1f=1`: `FAIL: test_divert_and_credits_are_checked`, `FAIL: test_setters_and_divert_arguments` (`FAILED (failures=2)`).
- M2 the credits line -> `ok = after is not None`: `FAIL: test_divert_and_credits_are_checked` (`failures=1`).
- M3 `if want['stay']:` -> `if False:`: `FAIL: test_endurance_must_stay_in_the_team_select` (`failures=1`).
- M4 `r['ent'] - off == START_ENTRY` -> `True`: `FAIL: test_start_menu_must_be_open` (`failures=1`).
