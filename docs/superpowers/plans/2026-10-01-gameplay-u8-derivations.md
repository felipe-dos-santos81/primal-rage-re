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

## §U8.12 The six START MENU row scenarios (Task 2)

`gp_session._u8_menu(moves, reach, time_limit)` and six `SCENARIOS` entries, inserted before `def expand` after the `gp-keys-fight` block (the base moved since the plan's `8eaf25a` anchor: U6b, U11 and U7 added their scenarios first; re-anchored by content, no code invented). Source of every value: §U8.3 (the walk: Enter, Enter, k x `p1.down` or one `p1.up`, Enter) and §U8.6 (the time limits `65 + k`, ENDURANCE 46); the gaps (150, 60), the 4-frame hold and the 300-frame tail are harness values.

| scenario | row | moves | reach | time_limit | STOP_AT_END |
|---|---|---|---|---|---|
| `gp-u8-right-arcade` | 1 | 1 x `p1.down` | 6 | 66 | yes |
| `gp-u8-left-training` | 2 | 2 x `p1.down` | 6 | 67 | yes |
| `gp-u8-right-training` | 3 | 3 x `p1.down` | 6 | 68 | yes |
| `gp-u8-tug-of-war` | 4 | 4 x `p1.down` | 6 | 69 | yes |
| `gp-u8-endurance` | 5 | 5 x `p1.down` | 0x10 | 46 | yes |
| `gp-u8-handicap` | 6 | 1 x `p1.up` (the wrap) | 6 | 66 | yes |

STOP_AT_END decision (the controller's ruling: opt in unless a later task compares frames after the script's end; plan Tasks 3 and 6 read). Per scenario the answer is the same, no frame past the script's end is compared: Task 3's `gp-modes-one` runs `gp_compare` with the port dump of a replay that ends at the script's end (`--end` only when a stall cuts it earlier); Task 6 Step 5 pins `N` as the first unexplained capture frame, which for a replay that ran to its end is the capture frame after the port's last frame ("how far the port got, not a divergence", the plan's own wording, `GP_CHARSEL_MIN_FIRST = 516` and Task 3 Step 4's 1070), `F` as the replay's last `T` frame + 1 (an exact pin), `MAX_START` the window start, and the capture sha/frame count (the pin of the capture as recorded, including its 60-frame tail). `gp_capture.STOP_TAIL` = 60 game frames after the `X` record keeps the capture frames past the port's last frame that `N` names (the same arrangement as `gp-twop`, `gp-u6-moves`). So the six row scenarios are in `STOP_AT_END`: rows 1-4 and 6 save the idle fight's remainder of the time limit, ENDURANCE ends in its team select 300 frames in (D3) where the tail is a still screen. `gp-u8-attract-start` (Task 4) decides separately; it is not added here. Test: `test_gp_capture.TestStopAtEnd.test_opt_in_scenarios` gained six `assertIn` lines (the existing opt-in pattern; its loop already asserts each member ends in an `until_mode` or `end` step).

Tests: `tools.tests.test_gp_modes` `Ran 11 tests ... OK` (the plan's 11); the per-task gate `test_gp_modes test_gp_session test_gp_capture test_gp_compare` `Ran 103 tests OK`; verify's k11/gp line nine modules `Ran 171` plus `test_gp_twop` and `test_gp_modes` = `Ran 195` OK on this base. `port_progress.py`: `771 1203 64` / `731 731 100`, unchanged.

Mutations: `gp-u8-handicap` `_u8_menu(('p1.up',), 6, 66)` -> `_u8_menu(('p1.down',) * 6, 6, 66)`: `FAIL: test_every_row_has_a_scenario_and_the_walk_is_derived`; dropping `gp-u8-endurance` from `STOP_AT_END`: `FAIL: test_opt_in_scenarios` (`'gp-u8-endurance' not found in frozenset(...)`). Both restored.

## §U8.13 `make gp-modes-oracle` in `make verify`, proved on gp-idle-loss (Task 3)

`Makefile`: `GP_MODES_SCENARIOS` (empty until Task 6a) and `GP_MODES_KEEP`, the targets `gp-modes-oracle` and `gp-modes-one` (the brief's text verbatim, placed before the `gp-report` recipe, i.e. after `gp-twop-oracle`'s recipe on this base), the line `@$(MAKE) --no-print-directory gp-modes-oracle` in `verify` after `gp-twop-oracle`'s, and its own `.PHONY: gp-modes-oracle gp-modes-one` line directly after the shared `.PHONY` list (the shared list is not edited). Anchors re-found by content: the plan's `8eaf25a` line numbers are stale.

Step 1: before the edit `make gp-modes-oracle` printed `No rule to make target 'gp-modes-oracle'.  Stop.`, `exit=2`. Step 3 after it: the empty list runs the 11 `test_gp_modes` tests, `OK`, `exit=0`; `PR_ORACLE_REQUIRED=1 … GP_MODES_SCENARIOS="RA:gp-u8-right-arcade"` prints `gp-modes-oracle: no capture at data/k11-captures/gp-u8-right-arcade, skipped`, `exit=0`.

Step 4, the reference row `REF:gp-idle-loss` cut at `--end 2325` (the planning numbers 203/2088/204 and the re-baseline's 1070/2326 on `8eaf25a` were not used as pins; the values were measured first on this head, `34331fb`, with `GP_DUMP=/tmp/pr_u8_gp`):

- `make gp-replay scenario=gp-idle-loss GP_OPTIONAL=1 GP_SCRIPT_ARGS="--end 2325"`: `fn-miss PR_GP_DUMP distinct=4 dropped=0`, `all checks passed`.
- `gp_compare --report`: `window from capture 90 (raw 1744)`; `FIRST UNEXPLAINED capture 1070 (raw 4182): nearest port 817, rows 121..199`; `trace: 0 differing through 2325`; the dump's last line `00817 f=0915 tick=00000973 mode=0006` (`f = 0x915` = 2325, the replay's last frame, so capture 1070 is how far the cut replay got).
- Pins: `REF_N = 1070` (the first unexplained capture frame), `REF_F = 2326` (`0 differing through 2325`, plus 1: the exact pin; `gp_compare.ratchet` fails any N above the end as unreachable), start 90 (the window start), capture sha `773e2647…8c8447`, 8173 frames. They equal the `8eaf25a` re-baseline's, so U6b and U7 did not move this cut replay.

The run through `gp-modes-oracle` (`GP_MODES_KEEP=1`): `exit=0`; the eight `gp_modes: gp-idle-loss: … ok` lines (f=26E P, 26F, 5 -> 4, 293, 7F5, 207F); `all checks passed`; `capture: poll.log sha256 773e2647..8c8447, 8173 frames: matches the pin`; `frames: window from capture 90 (raw 1744); 973 classified … 1 unexplained`; `FIRST UNEXPLAINED capture 1070 (raw 4182)`; `first unexplained 1070, ratchet N 1070 ok`; `trace: 0 differing through 2325; ratchet N 2326 ok`; the kept dump 52 MB; without `GP_MODES_KEEP` the dump directory is removed (`ls /tmp/pr_u8_gp` showed only `gp-idle-loss.script` for this scenario).

Failure matrix (every pin one step beyond the measured value must fail; each run exit 2; logs `/tmp/gameplay-u8/t3_ref*.txt`):

| mutation | verbatim FAIL line |
|---|---|
| `MIN_FIRST` 1071 (N + 1) | `gp_compare: gp-idle-loss: frames: FAIL: first unexplained 1070 < ratchet N 1071` |
| `TRACE_MIN_FIRST` 2327 (F + 1) | `gp_compare: gp-idle-loss: trace: FAIL: N 2327 > end 2326: N is unreachable` |
| `MAX_START` 89 (start - 1) | `gp_compare: gp-idle-loss: frames: FAIL: window starts at capture 90 (raw 1744) > pinned start 89` |
| `CAPTURE_FRAMES` 8174 | `gp_compare: gp-idle-loss: capture: FAIL: poll.log sha256 773e2647…8c8447 (8173 frames) != the pinned 773e2647…8c8447 (8174 frames): a re-capture invalidates the pinned N, F and window start; re-measure them from the new capture (record §G.24) before pinning` |

Not a pin: with the cut replay and no divergence, a lower N (the planner's 204 over 203) passes as `ratchet N 204 ok (improved: raise N)`; only N = j + 1 and F = end + 1 can fail.

Per-task gate on this head (logs `/tmp/gameplay-u8/t3_*.txt`, dumps under `GP_DUMP=/tmp/pr_u8_gp`): the gp tool suites (`test_gp_modes test_gp_session test_gp_capture test_gp_compare test_gp_twop test_gp_moves test_gp_keys`) `Ran 155 tests OK`; `make gp-modes-oracle` (empty list, banner `== gameplay U8: other modes …`, 11 tests OK) exit 0; `gp-oracle`, `gp-charsel-oracle`, `gp-moves-oracle`, `gp-keys-oracle`, `gp-twop-oracle` each exit 0 with every verdict line, fn-miss pair and distinct count identical to the Task 0 baseline (`base_gp_lines.txt`: 2064/8320, 516/1513, 1005/2262/2949, keys 11/11, twop 612/1506/1506, distinct=4); `diff-verify: 13/13 functions VERIFIED; 17/17 mutants detected; 1 named gaps; 0/5 rows with callees closed (8 have none)` (equal to Task 0); `port_progress.py` `771 1203 64` / `731 731 100`. No full `make verify` (the controller's speed-up ruling); no capture.

## §U8.14 The pad arm (no Enter) and the gp-u8-attract-start scenario (Task 4)

`tools/gp_session.py`: `SCENARIOS['gp-u8-attract-start'] = dict(time_limit=61, arm='pad', steps=(('boot', ENTER_WAIT, ('pad', ('p1.start',), 4)), ('until_mode', 6, 300)))` (the brief's text; `time_limit` 61 is §U8.6's harness value), `pad_arm(snap, keys)`, and three edits inside `port_script` (the `p27` lookup, the `--end` guard, the header). The arm is the first `S` record whose bitmap is non-zero, pinned by `S(f - 1)` and `S(f)` both in mode 3 (§U8.3: `0x11D04` runs in mode 3 only, `0x25238`), and no key may be consumed before it; the script gets the line `arm pad` before `enter_frame`. `tools/gp_capture.py` `run_checks`: for `arm='pad'` the label is `mode left 3 after the pad arm` (the first `S`/`P` record after the first press whose mode is not 3); every other scenario keeps `mode 0x27 after the Enter` and its predicate. The pad's press is a scripted `I` record, so `input_check`'s unscripted-input check stays ok (the capture's BIOS word `3B00` is the scripted press's own).

Base drift: the plan's line numbers were stale; every replaced text was found verbatim by content on `7557188` (U6b, U11, E3, U7 merged since `8eaf25a`), the Task 2 block's last line `gp-u8-handicap` is the insertion anchor. No code was invented.

STOP_AT_END decision (the controller's ruling; Task 2 left the attract start open): `gp-u8-attract-start` is added to `STOP_AT_END`, for the same reason as the six rows (§U8.12): its replay ends at the script's end (`until_mode 6, 300`), Task 6's `N` is the capture frame after the port's last frame, `F` the replay's end + 1, nothing later is compared, and the 60-frame `STOP_TAIL` keeps the frames `N` names. It saves the fight remainder of the 61 s limit (§U8.6's 73 MB estimate assumed recording to the limit). `test_gp_capture.TestStopAtEnd.test_opt_in_scenarios` names it (its loop asserts the last step is an `until_mode`).

Tests: `tools.tests.test_gp_modes` `Ran 16 tests ... OK` (11 + `TestPadArm` 5: `test_the_attract_start_scenario`, `test_the_script_arms_on_the_press` (`arm pad`, `enter_frame 272`, `enter_state 0004`, `bits 272 0100`, `key 274 3B 00`, `bits 276 0000`, `end 279`), `test_the_arm_must_be_pinned_in_mode_3` (no `S(f - 1)`, an all-zero bitmap, mode `0x27` at the arm), `test_an_enter_scenario_still_needs_mode_0x27`, `test_the_capture_check_follows_the_arm`); `test_gp_modes test_gp_session test_gp_capture test_gp_compare` `Ran 108 tests OK`; with `test_gp_twop test_gp_moves test_gp_keys` `Ran 160 tests OK`.

Backward compatibility: `port-script` of each of the eight existing captures (`gp-pads`, `gp-idle-loss`, `gp-idle-loss-run2`, `gp-u5-charsel`, `gp-u6-moves`, `gp-u6-moves-b`, `gp-keys-fight`, `gp-twop`) under the new `tools/gp_session.py` and under `HEAD`'s (`7557188`) copy: exit 0 each and `cmp` byte-identical. `gp-pads`: `enter_frame 321`, `enter_state 0000`, no `arm pad` line (record §G.7.3).

Mutations (each restored): M5 `pad = False` in `port_script`: `ERROR: test_the_script_arms_on_the_press`, `FAIL: test_the_capture_check_follows_the_arm`; M6 `if False:` in `run_checks`: `ERROR: test_the_capture_check_follows_the_arm`; M7 `if False:` in `pad_arm`'s pin: `FAIL: test_the_arm_must_be_pinned_in_mode_3`.

Gate (per-task): `make gp-modes-oracle` exit 0 (16 tests OK, no scenarios yet); `make diff-verify`: `13/13 functions VERIFIED; 17/17 mutants detected; 1 named gaps; 0/5 rows with callees closed (8 have none)` (= Task 0); `port_progress.py` `771 1203 64` / `731 731 100`. No capture; no full `make verify`.

## §U8.15 The PR_GP_DUMP driver accepts a pad arm (Task 5)

`port/tests/test_game.c` (gp driver; re-anchored by content, the plan's line numbers are stale: statics at 12192, `gp_parse` 12197, the arm checks at 12409-12410 on this base): the static `gp_arm_pad`; `gp_parse` resets it, reads the line `arm pad` and, for a pad arm, requires the first step to be `bits <enter_frame> <non-zero>`; every other script still needs the mode-3 Enter first (`key <enter_frame> 1C 0D`, `0x24ECF`). In `test_gp_replay` the arm checks are `mode_before == 3` (0x24ECF / 0x25238: either arm needs mode 3), `mode_after == (gp_arm_pad ? 3 : 0x27)` (a pad arm leaves mode 3 one iteration later, record §U8.3), `frame_after == enter_frame`, `state_after == enter_state`. The K11 driver's look-alike checks (12162-12163) are untouched.

Step 1 (red, the pre-edit driver, hand-built `arm pad` script `bits 320 0100`, `key 322 3B 00`, `bits 324 0000`, `end 400`): `FAIL ...test_game.c:12340: PR_GP_SCRIPT names a parsable gp port script v2` and `test_platform.c:254: 0 != 2` (the parse fails before `game_init`).

Step 3 (green, `end 400`): no `test_game.c` FAIL; the only FAILs are the miss-log ones (`test_platform.c:254: 3 != 2`, `:259 unexpected 0x29D60 from frontend_mode_1b_step`), because `gp-u8-attract-start` has no pinned set until Task 6g; the log holds the base pair and `0x29D60` (distinct=3).

The long preview (`end 2600`, the base `7557188`): mode entries from `trace.txt` `f=0140 mode=0003 cred=00000005 b1f=00`, `f=0141 mode=001A cred=00000004 b1f=01`, `f=0153 mode=001B`, `f=0165 mode=0010`, `f=0500 mode=001A`, `f=0512 mode=001B`; mode 6 first at `f=06B5` (`T f=06B5 mode=0006`), the same entries as the re-baseline's `8eaf25a` preview (so U6b, U11, E3 and U7 did not move this path). Misses beyond the base pair: only `0x29D60` and `0x5D812` (both `frontend_mode_1b_step`); `distinct=4 dropped=0`, the test_game.c checks all pass (the only FAILs: `test_platform.c:254: 4 != 2` and the two `:259` lines for those two addresses; Task 6g pins them as a `gp-u8-attract-start` table entry, never a new `fnm_known` parameter). Not evidence for the original (a port preview).

Step 4 (mutation): `CHECK_EQ_INT((int)mode_after, gp_arm_pad ? 3 : 0x27);` -> `CHECK_EQ_INT((int)mode_after, 0x27);`, rebuilt, the long script: `FAIL ...test_game.c:12417: 3 != 39`. Restored, rebuilt, `git diff` is the planned edit only.

Enter regression and gate (per-task; logs `/tmp/pr_u8_t5/`): `PR_ORACLE_REQUIRED=1 ./build/run_tests`: `all checks passed`, exit 0. `make gp-replay scenario=gp-pads`: `all checks passed`. `make gp-oracle`, `gp-charsel-oracle`, `gp-moves-oracle`, `gp-keys-oracle`, `gp-twop-oracle`, `gp-modes-oracle` each exit 0; every line of `base_gp_lines.txt` (the fn-miss pairs with their hit counts, `distinct=4`, the ratchet lines 2064/8320, 516/1513, 1005/2262/2949, keys 11/11, twop 612/1506/1506) is present and identical (`diff` shows only additional report lines). `make diff-verify`: `13/13 functions VERIFIED; 17/17 mutants detected; 1 named gaps; 0/5 rows with callees closed (8 have none)` (= Task 0). `port_progress.py`: `771 1203 64` / `731 731 100`. No capture; no full `make verify`.

## §U8.16 gp-u8-right-arcade: capture, replay and pins (Task 6a)

Base `bc51fd0`. `S=/tmp/gameplay-u8`, logs `cap_/path_/replay_/report_gp-u8-right-arcade.txt`, `o_RA.txt`.

**Capture** (`make gp-capture scenario=gp-u8-right-arcade TITLE_PIN_DIR=/tmp/pr_u8_pin`, one run): `snapshots 2323, f 4..91D, 7 frames missed (spec §3.7)`; every check `ok` (`base`, `steps fired 4/4`, `end frame reached`, `mode 0x27 after the Enter`, `snapshots kb == raw (0 differ)`, `frames written 1140/1140`, `port script v2`, `no unscripted input`, `stopped at the end (SIGTERM at f=091D, rc=0)`); `wrote 1140 frames … (raw 1377..4184), wall 60.4s`. `session.txt`: `exe=/tmp/pr_u8_pin/PRAGE.EXE sha256=8120f1bd…161a68d`, `cmos=zero pad_bios=1`, `time_limit=66 wall_s=60.4 rc=0`, `stop_at_end=1 tail=60 signal_f=091D`, `frames=1140 raw_window=1377..4184 avi_frames=4185`. Size 55 MB (§U8.6 estimated 75 MB to the time limit; `STOP_AT_END` stops 60 frames after X).

**Evidence** (`gp_modes.py check`, `rc=0`): the row mode `0x2E` at `f=24D (P)`; no other game-start mode; START MENU open; the divert `b1d=0 b1f=2` at `f=24E`; credits 5 -> 4; `0x10` at `f=272`; mode 6 at `f=7B5`; X at `f=8E1`. Path (`poll.log:<n>`): `27` (320), `2E` (599), `1A` (600, `cred=4 b1d=0 b1f=2`), `1B` (620), `10` (637), `1A` (1548), `1B` (1567), `11` (1584), `17` (1586), `1A` (1828), `1B` (1847), `5` (1866), `6` (1990) — the expected path. The Tasks 1–2 review's two points: one `p1.down` held 4 frames moved the cursor exactly once (row 1's mode `0x2E`, not row 2's `0x28`), and the capture holds frames after the port's last one (`STOP_TAIL` 60 frames, SIGTERM at `f=91D` > X `f=8E1`). Every path entry equals the `8eaf25a` preview (§U8.3: `0x24F` vs the capture's `0x24D` for the row mode: the capture's Enter landed two frames earlier, `key 589` in the port script).

**Replay** (port script: `enter_frame 318`, `enter_state 0000`, …, `key 589 1C 0D`, `end 2273`; 826 port frames, the last `00825 f=08E1`): misses beyond the base pair, in printed order: `0x29D60 frontend_mode_1b_step hits=1` (the bare `ret`, §G.24), `0x5D812 frontend_mode_1b_step hits=1` (the runtime stub, §G.24), `0x14EF8 hit_reaction_apply hits=3`, `0x14F50 hit_reaction_apply hits=4`; `distinct=6 dropped=0`. Classification of the two new ones from the raw (post-fixup image, §U8.A `img.py`): the dwords `0xA46A8 = 0x00014EF8` and `0xA46BC = 0x00014F50` (reverse-e2 triage rows: `move-callback`, character 3, reactions 0x20 and 0x21, called through `call [0x105BD4]` at `0x2B56D` in `0x2B2A0`); each body (88 bytes) is `cmp dword [ebx+8],0` / `0x3C4CC(…, 0xD2E26 resp. 0xD2E56, 0x40000000)` / `mov byte [ebx+0x52],0xB; [ebx+0x53],6; [ebx+0x54],0` / `0x2C3FC(0xB2)` / `mov al,1`. Unported move callbacks, owner track P (batch P2); not registered here. `0x15510` (`anim_indirect`, the `8eaf25a` preview's to `f = 2900`) is not reached by this replay (it ends at `f = 0x8E1`). Pinned as `k_miss_gp_u8_right_arcade` (exact name) in `k_gp_sets`; rerun: `distinct=6`, `all checks passed`. Mutation (the `0x14F50` row deleted): `test_platform.c:271: 6 != 5`, `unexpected 0x14F50 from hit_reaction_apply`; restored.

**Claims** (`gp_compare --report`): `window from capture 88 (raw 1745); 633 classified: 449 clean, 179 splice, 0 transition, 5 unexplained, 10 all-black`; `FIRST UNEXPLAINED capture 726 (raw 3767): nearest port 526, rows 93..194, x 0..319 (8271 px)` (726..730 all near port 526/530); `trace: 1654 frames compared …, 7 without a capture snapshot; first tick difference f=13F`; `trace: first difference f=7BA (1978) in e0: capture 0, port 1010`; moves (reported, not pinned by `gp-modes-one`): `first difference f=7BD (1981) in s0_43: capture 82, port 80`. `poll.log` sha256 `b72dbaa7486b651bd1acfddffe886aeb515eed3f916220d44375f0f67227906f`, 1140 frames.

Triage: a new divergence, not the end of the replay (port 526 is `f = 0x7B6`, the port runs to `0x8E1`). Both sides take `r0 = 0x20` at `f = 0x7B7` (the CPU side 0 reacting); the capture then has `s0_52 = 0x0B` from `f = 0x7B7` (the `0x14F16` store of `0x14EF8`), the port `0x0E` and then `0x01` at `0x7B9` (`s0_52` is not a traced field); `e0` (the side-0 command word) and `rng` differ from `0x7BA`. The PNG pair (capture 725/726 vs port 526/530) shows side 0 (Vertigo) in the reaction pose in the capture only. Cause: the unported `0x14EF8` (miss above); owner track P batch P2. Not fixed here. Determinism: one capture only; U4's two runs agreed at every `f` from `0x625` (§G.19), which is the only run-to-run evidence and does not cover this fight.

**Pins** (Makefile): `GP_MODES_RA_MIN_FIRST = 726`, `_TRACE_MIN_FIRST = 1978`, `_MAX_START = 88`, `_CAPTURE_SHA256 = b72dbaa7…27906f`, `_CAPTURE_FRAMES = 1140`, `GP_MODES_SCENARIOS += RA:gp-u8-right-arcade`. `make gp-modes-oracle GP_DUMP=/tmp/pr_u8_gp`: `exit=0`, the eight `gp_modes` lines `ok`, `all checks passed`, `matches the pin`, `first unexplained 726, ratchet N 726 ok`, `first differing 1978, ratchet N 1978 ok`. Failures (each `exit=2`): `MIN_FIRST=727` `frames: FAIL: first unexplained 726 < ratchet N 727`; `TRACE_MIN_FIRST=1979` `trace: FAIL: first differing 1978 < ratchet N 1979`; `CAPTURE_SHA256=0000` `capture: FAIL: poll.log sha256 b72dbaa7… (1140 frames) != the pinned 0000 (1140 frames): …`; `MAX_START=87` `frames: FAIL: window starts at capture 88 (raw 1745) > pinned start 87`. Damaged frame (kept dump, byte 32000 of port frame 400 flipped, `--max-start 88`): `FIRST UNEXPLAINED capture 578 (raw 3204): nearest port 400, rows 100..100, x 0..0 (1 px)`, `frames: FAIL: first unexplained 578 < ratchet N 726`, `rc=1`.

Gate (per-sub-task): `PR_ORACLE_REQUIRED=1 ./build/run_tests`: `all checks passed`, exit 0. `make gp-oracle gp-charsel-oracle gp-moves-oracle gp-keys-oracle gp-twop-oracle GP_DUMP=/tmp/pr_u8_gp`: `gp-exit=0`, the extracted lines (`gplines.sh`) `diff`-equal to Task 0's `base_gp_lines.txt` (the fn-miss pairs with their hit counts, `distinct=4`, every ratchet line): the new exact-name entry changes no other scenario's set. No full `make verify` (Task 7).

## §U8.17 gp-u8-left-training: capture, replay and pins (Task 6b)

Base `b7f13c6`. Logs in `/tmp/gameplay-u8/` (`cap_/path_/replay_/report_gp-u8-left-training.txt`, `o_LT.txt`).

**Capture** (one run): `snapshots 2386, f 4..95D, 8 frames missed (spec §3.7)`; every check `ok` (`base`, `steps fired 5/5`, `end frame reached`, `mode 0x27 after the Enter`, `snapshots kb == raw (0 differ)`, `frames written 1141/1141`, `port script v2`, `no unscripted input`, `stopped at the end (SIGTERM at f=095D, rc=0)`); `wrote 1141 frames … (raw 1381..4276), wall 61.9s`. `session.txt`: the same pinned exe sha256 `8120f1bd…161a68d`, `cmos=zero pad_bios=1`, `time_limit=67 wall_s=61.9 rc=0`, `stop_at_end=1 tail=60 signal_f=095D`, `frames=1141 raw_window=1381..4276 avi_frames=4277`. 55 MB (§U8.6: 75 MB to the time limit).

**Evidence** (`rc=0`): the row mode `0x28` at `f=27F (P)`; no other game-start mode; START MENU open; the divert `b1d=1 b1f=3` at `f=280`; credits 5 -> 5; `0x10` at `f=2A4`; 6 at `f=7F5`; X at `f=921`. Path: `27` (`poll.log:309`), `28` (651), `1A` (652), `1B` (671), `10` (689), `1A` (1614), `1B` (1633), `11` (1650), `17` (1652), `1A` (1894), `1B` (1913), `5` (1932), `6` (2056): the expected path. The two `p1.down` holds (`bits 518 4000`..`522`, `bits 578 4000`..`582`) moved the cursor twice (row 2). `b1f` (the `S` records' changes): `3` from `f=280`, `1` from `f=654` (in the wipe `0x1B` after the character select; §U8.3's port preview gave "1 by `0x756`", a later sample point of the same change; the port's trace has `b1f=01` at `f=654` too).

**Replay** (`enter_frame 308`, Enters at 308, 460, 639, `end 2337`; 848 port frames, the last `00847 f=0921`): beyond the base pair only `0x29D60 frontend_mode_1b_step hits=1` and `0x5D812 frontend_mode_1b_step hits=1` (both §G.24: the bare `ret` and the runtime stub), `distinct=4`. Pinned as `k_miss_gp_u8_left_training`; rerun `all checks passed`. Mutation (the `0x29D60` row deleted): `test_platform.c:281: 4 != 3`, `unexpected 0x29D60 from frontend_mode_1b_step`; restored.

**Claims** (`--report`): `window from capture 83 (raw 1735); 988 classified: 594 clean, 388 splice, 1 transition, 5 unexplained, 10 all-black`; `FIRST UNEXPLAINED capture 1076 (raw 4207): nearest port 847, rows 162..199, x 0..319 (5744 px)`; `trace: 2023 frames compared (f 134..), 7 without a capture snapshot; first tick difference f=135`; `trace: 0 differing through 2337`; `moves: 0 differing through 2337`. `poll.log` sha256 `90eeef83f77dab25d9ed7be30fcadf278bfd0413c068c12bd700ff183ab09917`, 1141 frames. Triage: port 847 is the last `.ipx` (`f = 0x921`, the script's end = the capture's X), so 1076 is how far the port got, not a divergence (capture 1076.. is the 60-frame `STOP_AT_END` tail); `F` = 2337 + 1 = 2338, the exact pin. The training fight's first 300 frames agree on every traced field.

**Pins**: `GP_MODES_LT_MIN_FIRST = 1076`, `_TRACE_MIN_FIRST = 2338`, `_MAX_START = 83`, `_CAPTURE_SHA256 = 90eeef83…b09917`, `_CAPTURE_FRAMES = 1141`, `GP_MODES_SCENARIOS += LT:gp-u8-left-training`. `make gp-modes-oracle GP_DUMP=/tmp/pr_u8_gp` (RA and LT): `exit=0`; LT `matches the pin`, `first unexplained 1076, ratchet N 1076 ok`, `0 differing through 2337; ratchet N 2338 ok` (RA's lines as §U8.16). Failures (each `exit=2`): `MIN_FIRST=1077` `frames: FAIL: first unexplained 1076 < ratchet N 1077`; `TRACE_MIN_FIRST=2339` `trace: FAIL: N 2339 > end 2338: N is unreachable`; `CAPTURE_SHA256=0000` `capture: FAIL: … != the pinned 0000 (1141 frames) …`; `MAX_START=82` `frames: FAIL: window starts at capture 83 (raw 1735) > pinned start 82`. Damaged frame (port frame 600, byte 32000, `--max-start 83`): `FIRST UNEXPLAINED capture 788 (raw 3914): nearest port 601, rows 89..122, x 62..319 (1696 px)`, `frames: FAIL: first unexplained 788 < ratchet N 1076`, `rc=1`.

Gate: `PR_ORACLE_REQUIRED=1 ./build/run_tests` `all checks passed`, exit 0; the five gp oracles `gp-exit=0`, their extracted lines `diff`-equal to `base_gp_lines.txt`.

## §U8.18 gp-u8-right-training: capture, replay and pins (Task 6c)

Base `9170f5c`. Logs in `/tmp/gameplay-u8/` (`cap_/path_/replay_/report_gp-u8-right-training.txt`, `o_RT.txt`).

**Capture** (one run): `snapshots 2450, f 4..99D, 8 frames missed (spec §3.7)`; every check `ok` (`base`, `steps fired 6/6`, `end frame reached`, `mode 0x27 after the Enter`, `snapshots kb == raw (0 differ)`, `frames written 1167/1167`, `port script v2`, `no unscripted input`, `stopped at the end (SIGTERM at f=099D, rc=0)`); `wrote 1167 frames … (raw 1372..4340), wall 62.6s`. `session.txt`: the pinned exe sha256 `8120f1bd…161a68d`, `cmos=zero pad_bios=1`, `time_limit=68 wall_s=62.6 rc=0`, `stop_at_end=1 tail=60 signal_f=099D`, `frames=1167 raw_window=1372..4340 avi_frames=4341`. 55 MB (§U8.6: 75 MB to the time limit).

**Evidence** (`rc=0`): the row mode `0x29` at `f=2CB (P)`; no other game-start mode; START MENU open; the divert `b1d=1 b1f=3` at `f=2CC`; credits 5 -> 5; `0x10` at `f=2F0`; 6 at `f=835`; X at `f=961`. Path: `27` (`poll.log:324`), `29` (730), `1A` (731), `1B` (751), `10` (768), `1A` (1681), `1B` (1700), `11` (1717), `17` (1719), `1A` (1961), `1B` (1980), `5` (1999), `6` (2123): the expected path. Three `p1.down` holds of 4 frames each (`bits 534`/`594`/`654 4000`) moved the cursor three times. `b1f`: `3` from `f=2CC`, `2` from `f=694` (the port's trace has `b1f=02` at `f=694` too; §U8.3's preview "2 by `0x796`" is a later sample of the same change).

**Replay** (`enter_frame 324`, Enters at 324, 474, 715, `end 2401`; 846 port frames, the last `00845 f=0961`): beyond the base pair only `0x29D60 frontend_mode_1b_step hits=1` and `0x5D812 frontend_mode_1b_step hits=1` (§G.24), `distinct=4`. Pinned as `k_miss_gp_u8_right_training`; rerun `all checks passed`. (The dropped-row mutation ran in 6a and 6b; this set has the same two rows.)

**Claims** (`--report`): `window from capture 90 (raw 1745); 1003 classified: 612 clean, 386 splice, 0 transition, 5 unexplained, 10 all-black`; `FIRST UNEXPLAINED capture 1098 (raw 4272): nearest port 844, rows 27..199, x 0..319 (16397 px)`, then 1099..1102 `nearest port 0` (63935–63943 px); `trace: 2072 frames compared (f 144..), 6 without a capture snapshot; first tick difference f=145`; `trace: 0 differing through 2401`; `moves: 0 differing through 2401`. `poll.log` sha256 `496964928c537f3b428414e21d02f254b399af4b2f39b393d10d9855c74f1ea5`, 1167 frames.

Triage: the end of the port's replay, not a divergence. Pixel diffs (direct, `load_capture_frame`/`load_port_frame`): capture 1096 = port 844 (0 px), capture 1097 = port 845 (0 px; port 845 is the last `.ipx`, `f = 0x961` = X), capture 1098 against port 845 10134 px, against 844 16397 px. So 1098 is the game frame after X (`S f=0962 ms=61081`, X at `ms=61067`), which the port never ran; `gp_compare`'s `nearest` ranks by common top/bottom rows (`tools/gp_compare.py:239`), hence "844". Capture 1099.. are the `STOP_AT_END` tail (the fight continuing, the camera scrolled: `nearest port 0`). `F` = 2401 + 1 = 2402, the exact pin.

**Pins**: `GP_MODES_RT_MIN_FIRST = 1098`, `_TRACE_MIN_FIRST = 2402`, `_MAX_START = 90`, `_CAPTURE_SHA256 = 49696492…4f1ea5`, `_CAPTURE_FRAMES = 1167`, `GP_MODES_SCENARIOS += RT:gp-u8-right-training`. `make gp-modes-oracle GP_DUMP=/tmp/pr_u8_gp` (RA, LT, RT): `exit=0`, 24 `gp_modes … ok` lines; RT `matches the pin`, `first unexplained 1098, ratchet N 1098 ok`, `0 differing through 2401; ratchet N 2402 ok` (RA and LT as §U8.16–§U8.17). Failures (each `exit=2`): `MIN_FIRST=1099` `frames: FAIL: first unexplained 1098 < ratchet N 1099`; `TRACE_MIN_FIRST=2403` `trace: FAIL: N 2403 > end 2402: N is unreachable`; `CAPTURE_SHA256=0000` `capture: FAIL: … != the pinned 0000 (1167 frames) …`; `MAX_START=89` `frames: FAIL: window starts at capture 90 (raw 1745) > pinned start 89`. Damaged frame (port frame 600, byte 32000, `--max-start 90`): `FIRST UNEXPLAINED capture 811 (raw 3982): nearest port 601, rows 89..113, x 74..280 (967 px)`, `frames: FAIL: first unexplained 811 < ratchet N 1098`, `rc=1`.

Gate: `PR_ORACLE_REQUIRED=1 ./build/run_tests` `all checks passed`, exit 0; the five gp oracles `gp-exit=0`, their extracted lines `diff`-equal to `base_gp_lines.txt`.

## §U8.19 gp-u8-tug-of-war: capture, replay and pins (Task 6d)

Base `c212a83`. Logs in `/tmp/gameplay-u8/` (`cap_/path_/replay_/report_gp-u8-tug-of-war.txt`, `o_TW.txt`, `o_TW_f.txt`, `dmg_TW.txt`).

**Capture** (one run): `snapshots 2516, f 5..9DD, 5 frames missed (spec §3.7)`; every check `ok` (`base`, `steps fired 7/7`, `end frame reached`, `mode 0x27 after the Enter`, `snapshots kb == raw (0 differ)`, `frames written 1176/1176`, `port script v2`, `no unscripted input`, `stopped at the end (SIGTERM at f=09DD, rc=0)`); `wrote 1176 frames … (raw 1368..4411), wall 63.6s`. `session.txt`: the pinned exe sha256 `8120f1bd…161a68d`, `cmos=zero pad_bios=1`, `time_limit=69 wall_s=63.6 rc=0`, `stop_at_end=1 tail=60 signal_f=09DD`, `frames=1176 raw_window=1368..4411 avi_frames=4412`. 57 MB (§U8.6: 75 MB to the time limit).

**Evidence** (`rc=0`): the row mode `0x2A` at `f=306 (P)`; no other game-start mode; START MENU open; the divert `b1d=2 b1f=3` at `f=307`; credits 5 -> 5; `0x10` at `f=32B`; 6 at `f=875`; X at `f=9A1`. Path: `27` (`poll.log:325`), `2A` (792), `1A` (794), `1B` (813), `10` (832), `1A` (1750), `1B` (1769), `11` (1786), `17` (1788), `1A` (2030), `1B` (2049), `5` (2068), `6` (2192): the expected path. Four `p1.down` holds of 4 frames each (`bits 534`/`594`/`654`/`714 4000`) moved the cursor four times. `b1f` is `3` from `f=307` to the end (both sides human; no change after the character select, unlike the training rows).

**Replay** (`enter_frame 324`, Enters at 324, 474, 774, `end 2465`; 827 port frames, the last `00826 f=09A1`): beyond the base pair only `0x29D60 frontend_mode_1b_step hits=1` and `0x5D812 frontend_mode_1b_step hits=1` (§G.24: the bare `ret` and the runtime stub), `distinct=4`, the §U8.3 preview's set. Pinned as `k_miss_gp_u8_tug_of_war`; rerun `distinct=4`, `all checks passed`. (The dropped-row mutation ran in 6a and 6b; this set has the same two rows.)

**Claims** (`--report`): `window from capture 104 (raw 1741); 998 classified: 712 clean, 281 splice, 0 transition, 5 unexplained, 10 all-black`; `FIRST UNEXPLAINED capture 1107 (raw 4343): nearest port 825, rows 91..168, x 53..168 (507 px)`, then 1108..1111 `nearest port 665`; `trace: 2137 frames compared (f 144..), 5 without a capture snapshot; first tick difference f=145`; `trace: 0 differing through 2465`; `moves: 0 differing through 2465`. `poll.log` sha256 `30cd09b8d24a51a41cc37c0ffebd433a08e5a741b248f28e8112c28ff04d1439`, 1176 frames.

Triage: the end of the port's replay, not a divergence. Direct pixel diffs: capture 1103/1104/1105/1106 = port 823/824/825/826 (0 px each); port 826 is the last `.ipx` (`f = 0x9A1` = X); capture 1107 differs from 826 by 1024 px and from 825 by 1446 px: the game frame after X, which the port never ran (`gp_compare`'s row-hash `nearest` gives 825). 1108.. are the `STOP_AT_END` tail. `F` = 2465 + 1 = 2466, the exact pin. The window start 104 (the other rows: 83–90) is the same raw position (raw 1741; RA 1745, LT 1735, RT 1745): this capture's frames start earlier (raw 1368), so the same raw has a larger index.

**Pins**: `GP_MODES_TW_MIN_FIRST = 1107`, `_TRACE_MIN_FIRST = 2466`, `_MAX_START = 104`, `_CAPTURE_SHA256 = 30cd09b8…4d1439`, `_CAPTURE_FRAMES = 1176`, `GP_MODES_SCENARIOS += TW:gp-u8-tug-of-war`. `make gp-modes-oracle GP_DUMP=/tmp/pr_u8_gp` (RA, LT, RT, TW): `exit=0` (2:59), 32 `gp_modes … ok` lines, four `all checks passed`; TW `matches the pin`, `first unexplained 1107, ratchet N 1107 ok`, `0 differing through 2465; ratchet N 2466 ok` (RA, LT, RT as §U8.16–§U8.18). Failures (each `exit=2`, run with `GP_MODES_SCENARIOS=TW:gp-u8-tug-of-war`): `MIN_FIRST=1108` `frames: FAIL: first unexplained 1107 < ratchet N 1108`; `TRACE_MIN_FIRST=2467` `trace: FAIL: N 2467 > end 2466: N is unreachable`; `CAPTURE_SHA256=0000` `capture: FAIL: … != the pinned 0000 (1176 frames) …`; `MAX_START=103` `frames: FAIL: window starts at capture 104 (raw 1741) > pinned start 103`. Damaged frame (`--max-start 104`): byte 32000 of port frame 600 flipped (index 23 -> 0xE8, a different DAC colour) did **not** fail (`ratchet N 1107 ok`, rc 0): the damage is invisible to the claim, most likely because every capture frame explained near port 600 is explained without port 600's row 100 (a 70.09/60.05 Hz splice taking that row from a neighbour; not isolated) — an instance of the narrow claim (that every port frame appears is not claimed). Port frame 400 instead (index 72 flipped): `FIRST UNEXPLAINED capture 621 (raw 3412): nearest port 399, rows 38..88, x 0..114 (3090 px)`, `frames: FAIL: first unexplained 621 < ratchet N 1107`, `rc=1`.

Gate: `PR_ORACLE_REQUIRED=1 ./build/run_tests` `all checks passed`, exit 0; the five gp oracles `gp-exit=0`, their extracted lines `diff`-equal to `base_gp_lines.txt`.

## §U8.20 gp-u8-handicap: capture, replay and pins (Task 6e)

Base `1380841`. Logs in `/tmp/gameplay-u8/` (`cap_/path_/replay_/report_gp-u8-handicap.txt`, `o_HC.txt`, `o_HC_f.txt`, `dmg_HC.txt`).

**Capture** (one run): `snapshots 2324, f 4..91D, 6 frames missed (spec §3.7)`; every check `ok` (`base`, `steps fired 4/4`, `end frame reached`, `mode 0x27 after the Enter`, `snapshots kb == raw (0 differ)`, `frames written 1081/1081`, `port script v2`, `no unscripted input`, `stopped at the end (SIGTERM at f=091D, rc=0)`); `wrote 1081 frames … (raw 1404..4223), wall 61.0s`. `session.txt`: the pinned exe sha256 `8120f1bd…161a68d`, `cmos=zero pad_bios=1`, `time_limit=66 wall_s=61.0 rc=0`, `stop_at_end=1 tail=60 signal_f=091D`, `frames=1081 raw_window=1404..4223 avi_frames=4224`. 53 MB (§U8.6: 75 MB to the time limit).

**Evidence** (`rc=0`): the row mode `0x2C` at `f=236 (P)`; no other game-start mode; START MENU open; the divert `b1d=4 b1f=3` at `f=237`; credits 5 -> 5; `0x10` at `f=25B`; 6 at `f=7B5`; X at `f=8E1`. Path: `27` (`poll.log:296`), `2C` (577), `1A` (578), `1B` (598), `10` (615), `1A` (1549), `1B` (1568), `11` (1585), `17` (1587), `1A` (1829), `1B` (1848), `5` (1867), `6` (1991): the expected path. The one `p1.up` hold (`bits 504 8000`..`508`, BIOS word `1F73` at 504) wrapped the cursor from row 0 to row 6 (`0x2C`, §U8.3's `0x30511..0x3052F`). `b1f` is `3` from `f=237` to the end. (§U8.3's preview has the row mode at `0x24F`; the capture's Enters are earlier, `key 294`/`444`/`566`.)

**Replay** (`enter_frame 294`, Enters at 294, 444, 566, `end 2273`; 790 port frames, the last `00789 f=08E1`): beyond the base pair only `0x29D60 frontend_mode_1b_step hits=1` and `0x5D812 frontend_mode_1b_step hits=1` (§G.24), `distinct=4`, the §U8.3 preview's set. Pinned as `k_miss_gp_u8_handicap`; rerun `distinct=4`, `all checks passed`. (Same two rows as LT, whose dropped-row mutation failed.)

**Claims** (`--report`): `window from capture 80 (raw 1739); 937 classified: 663 clean, 269 splice, 0 transition, 5 unexplained, 10 all-black`; `FIRST UNEXPLAINED capture 1022 (raw 4155): nearest port 652, rows 7..181, x 0..319 (3734 px)`, then 1023..1026; `trace: 1974 frames compared (f 126..), 6 without a capture snapshot; first tick difference f=127`; `trace: 0 differing through 2273`; `moves: 0 differing through 2273`. `poll.log` sha256 `8f35fd3abd3a5c527e0f72984ed8ba419cadbcefa4e53b349d5a1ed7b69cdfa3`, 1081 frames.

Triage: the end of the port's replay, not a divergence. Direct pixel diffs: capture 1017 = port 786, 1018 = port 787, 1021 = port 789 (0 px each; port 789 is the last `.ipx`, `f = 0x8E1` = X); capture 1022 differs from port 789 by 3854 px: the game frame after X, which the port never ran (the row-hash `nearest` names port 652). `F` = 2273 + 1 = 2274, the exact pin. The handicap fight's first 300 frames agree on every traced and move field.

**Pins**: `GP_MODES_HC_MIN_FIRST = 1022`, `_TRACE_MIN_FIRST = 2274`, `_MAX_START = 80`, `_CAPTURE_SHA256 = 8f35fd3a…9cdfa3`, `_CAPTURE_FRAMES = 1081`, `GP_MODES_SCENARIOS += HC:gp-u8-handicap`. `make gp-modes-oracle GP_DUMP=/tmp/pr_u8_gp` (RA, LT, RT, TW, HC): `exit=0`, 40 `gp_modes … ok` lines, five `all checks passed`; HC `matches the pin`, `first unexplained 1022, ratchet N 1022 ok`, `0 differing through 2273; ratchet N 2274 ok` (the others as §U8.16–§U8.19). Failures (each `exit=2`, with `GP_MODES_SCENARIOS=HC:gp-u8-handicap`): `MIN_FIRST=1023` `frames: FAIL: first unexplained 1022 < ratchet N 1023`; `TRACE_MIN_FIRST=2275` `trace: FAIL: N 2275 > end 2274: N is unreachable`; `CAPTURE_SHA256=0000` `capture: FAIL: … != the pinned 0000 (1081 frames) …`; `MAX_START=79` `frames: FAIL: window starts at capture 80 (raw 1739) > pinned start 79`. Damaged frame (`--max-start 80`): byte 32000 of port frame 400 flipped (index 72, a different DAC colour) did not fail (rc 0; the same invisibility as §U8.19's port 600); port frame 600 (index 23): `FIRST UNEXPLAINED capture 783 (raw 3868): nearest port 600, rows 89..100, x 0..118 (204 px)`, `frames: FAIL: first unexplained 783 < ratchet N 1022`, `rc=1`.

Gate: `PR_ORACLE_REQUIRED=1 ./build/run_tests` `all checks passed`, exit 0; the five gp oracles `gp-exit=0`, their extracted lines `diff`-equal to `base_gp_lines.txt`.

## §U8.21 gp-u8-endurance: capture, replay and pins (Task 6f)

Base `20c379e`. Logs in `/tmp/gameplay-u8/` (`cap_/path_/replay_/report_gp-u8-endurance.txt`, `replay_gp-u8-endurance_mut.txt`, `o_EN.txt`, `o_EN_f.txt`, `dmg_EN.txt`). Decision D3: the scenario ends 300 frames into the team select; the ENDURANCE fight (modes `0x30`/`0x31`) stays a named gap (§U8.9).

**Capture** (one run): `snapshots 1228, f 4..4D1, 2 frames missed (spec §3.7)`; every check `ok` (`base`, `steps fired 8/8`, `end frame reached`, `mode 0x27 after the Enter`, `snapshots kb == raw (0 differ)`, `frames written 308/308`, `port script v2`, `no unscripted input`, `stopped at the end (SIGTERM at f=04D1, rc=0)`); `wrote 308 frames … (raw 1371..2826), wall 41.0s`. `session.txt`: the pinned exe sha256 `8120f1bd…161a68d`, `cmos=zero pad_bios=1`, `time_limit=46 wall_s=41.0 rc=0`, `stop_at_end=1 tail=60 signal_f=04D1`, `frames=308 raw_window=1371..2826 avi_frames=2827`. 14 MB (§U8.6 estimated 24 MB to the time limit).

**Evidence** (`rc=0`, nine lines): the row mode `0x2B` at `f=344 (P)`; no other game-start mode; START MENU open; the divert `b1d=3 b1f=3` at `f=345`; credits 5 -> 5; `0x10` at `f=369`; `mode 0x10 reached: ok (f=369)`; X at `f=495` (= `0x369 + 300`); `still in mode 0x10 at the end: ok (last S f=495 mode=10)`. Path: `27` (`poll.log:326`), `2B` (859), `1A` (862), `1B` (880), `10` (899), and nothing after: the expected path. The capture's last `S` (the `STOP_AT_END` tail) is `f=04D1 mode=0010`. Five `p1.down` holds (`bits 534`/`594`/`654`/`714`/`774 4000`) moved the cursor five times.

**Replay** (`enter_frame 324`, Enters at 324, 474, 836, `end 1173`; 164 port frames, the last `00163 f=0494`; the trace's last `T f=0495 mode=0010`): beyond the base pair only `0x29D60 frontend_mode_1b_step hits=1` (§G.24: the bare `ret`; §U8.4: the hook `0x444C8` installs), `distinct=3`; no `0x5D812 frontend_mode_1b_step` (the replay never leaves mode `0x10`), the §U8.3 preview's set. Pinned as `k_miss_gp_u8_endurance` (one row); rerun `distinct=3`, `all checks passed`. Before the pin: `test_platform.c:309: 3 != 2`, `unexpected 0x29D60 from frontend_mode_1b_step`. Mutation (a one-row array cannot drop its row: the row's address changed to `0x5D812`): `fn-miss PR_GP_DUMP: unexpected 0x29D60 from frontend_mode_1b_step`, `FAIL …test_platform.c:323: the driver's miss log holds only its pinned known-set`; restored, rebuilt.

**Claims** (`--report`): `window from capture 90 (raw 1744); 201 classified: 147 clean, 49 splice, 0 transition, 5 unexplained, 6 all-black`; `FIRST UNEXPLAINED capture 278 (raw 2759): nearest port 98, rows 152..170, x 2..291 (826 px)`, then 280, 281, 293, 296; `trace: 848 frames compared (f 144..), 2 without a capture snapshot; first tick difference f=145`; `trace: 0 differing through 1173`; `moves: 0 differing through 1173`. `poll.log` sha256 `01c9069076ce231d652dbe1d97f0a80155d5e73bef932b0254816a79ec746204`, 308 frames.

Triage: the end of the port's replay, i.e. where the team-select tail of D3 ends, not a divergence. The team select presents every third `f` (the port's last frames `f = 0x48E, 0x491, 0x494`). Direct pixel diffs: capture 268/270/271/273/274/276/277 = port 157/158/159/160/161/162/163 (0 px each; 272 and 275 are splices); port 163 (`f = 0x494`) is the last `.ipx`; capture 278 differs from 163 by 2218 px and from every earlier port frame by more: the next present, which the port never ran (the row-hash `nearest` names port 98). The later unexplained frames (280, 281, 293, 296) are in the `STOP_AT_END` tail too; the tail frames the claim counts as explained (279, 282..) match earlier port frames of the team select's animation, which says nothing past N. `F` = 1173 + 1 = 1174 (`0x495`, the X record), the exact pin. So the ENDURANCE claim covers the walk, the divert and the first 300 frames of the team select; the team pass's picks and the fight are the named gap.

**Pins**: `GP_MODES_EN_MIN_FIRST = 278`, `_TRACE_MIN_FIRST = 1174`, `_MAX_START = 90`, `_CAPTURE_SHA256 = 01c90690…746204`, `_CAPTURE_FRAMES = 308`, `GP_MODES_SCENARIOS += EN:gp-u8-endurance` (after HC: sub-task order). No `_END` (no stall). `make gp-modes-oracle GP_DUMP=/tmp/pr_u8_gp` (RA, LT, RT, TW, HC, EN): `exit=0`, 49 `gp_modes … ok` lines (8 per row, 9 for EN), six `all checks passed`, twelve `ratchet N … ok`; EN `matches the pin`, `first unexplained 278, ratchet N 278 ok`, `0 differing through 1173; ratchet N 1174 ok`. Failures (each `exit=2`, with `GP_MODES_SCENARIOS=EN:gp-u8-endurance`): `MIN_FIRST=279` `frames: FAIL: first unexplained 278 < ratchet N 279`; `TRACE_MIN_FIRST=1175` `trace: FAIL: N 1175 > end 1174: N is unreachable`; `CAPTURE_SHA256=0000` `capture: FAIL: … != the pinned 0000 (308 frames) …`; `MAX_START=89` `frames: FAIL: window starts at capture 90 (raw 1744) > pinned start 89`. Damaged frame (port frame 140, byte 32000, index 72 flipped, `--max-start 90`): `FIRST UNEXPLAINED capture 245 (raw 2686): nearest port 140, rows 100..100, x 0..0 (1 px)`, `frames: FAIL: first unexplained 245 < ratchet N 278`, `rc=1`.

Gate: `PR_ORACLE_REQUIRED=1 ./build/run_tests` `all checks passed`, exit 0; the five gp oracles `gp-exit=0`, their extracted lines `diff`-equal to `base_gp_lines.txt`.
