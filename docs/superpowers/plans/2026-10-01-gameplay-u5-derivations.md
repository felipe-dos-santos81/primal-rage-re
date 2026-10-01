# Gameplay U5 (character-select walk and divergence 1): derivation record

Plan: `docs/superpowers/plans/2026-10-01-gameplay-u5-char-select.md`. Spec:
`docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §2 O1, §4 track G;
`docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md` §3.1-§3.3, §3.6, §4.1.
This record is U5's own (the parallelism rule of the 2026-10-01 planning brief); it does not
append to `2026-09-30-gameplay-ground-truth-derivations.md`, and cites it as "record §G.n".

Sections §C5.1-§C5.7 were established by the planner (2026-10-01, planning worktree
`.worktrees/reverse-plans` at `e9271df`); every number below was produced by a command that was
run, named with it. Scratch: `S=/private/tmp/claude-501/-Users-felipe-dos-santos-code-mine-primal-rage-reverse/6102b0fb-fcd6-4551-b6e4-2e65618a97ed/scratchpad/plans/u5`.
Sections §C5.10 onward are written by the executor, one per task.

Sources of addresses: **raw** = capstone 5.0.7 over the fixed-up image
(`build/diffrun --exe data/game/C/PRAGE.EXE --image-out $S/img_r2.bin`, 1 028 304 bytes, base
`0x10000`; identical by `cmp` to the earlier planner's `$S/image.bin`). **Ghidra (live)** = the
read-only MCP session on project `rage`. **Ghidra (committed)** = `port/decomp/prage.functions.csv`
(1352 functions). Where they disagree the raw wins.

---

## §C5.1 Divergence 1: the candidates and what each predicts

The observation (record §G.20, U4 report): in `gp-idle-loss` the first unexplained capture frame
is 203 (raw 2359); from it the capture's scene runs *backwards* through port frames 106..80 while
the port's keeps going forward and turns ~96 frames later (port frame 147, `f = 0x3A0`); the
turn coincides with the pick-countdown step at `f = 0x340`. The `0x43AAC` hypothesis is refuted
(§G.20, §J 11), the trace is equal there, and the miss log holds only the bare `ret` `0x29D60` in
mode `0x10` (§G.24).

| # | candidate | prediction | how a run discriminates | result (§C5.3) |
|---|---|---|---|---|
| C1 | the animation step timer (`rec+0x20/+0x24`, the opcode-8 pin `0x2B2A0`) runs at another rate in the port | the port shows the same frame values as the original, shifted in time; no frame value outside the idle set | trace `rec+0x52` and the sprite id per frame; a timing fault changes *when* values change, never *which* values appear | refuted: the step period is 3 frames throughout (`AT` lines), and the divergence is a value outside `0..0x1D` |
| C2 | the animation reads a different counter (the host tick, `DS_00101500`) | a drift that grows with the tick offset (U4: 15 ticks over 1 764 frames) | the same trace; compare to tick | refuted: the direction flips (`rec+0x58`) happen at the same `f` with and without the fix (`0x33D`, `0x39A`, `0x454`), so the countdown that draws them is frame-driven and equal |
| C3 | per-frame ordering (the update table runs the tick before/after another writer) | a one-frame offset | the same trace | refuted: no one-frame offset; the first difference is a value (`0x1E` against `0`) at one frame |
| C4 | state initialised differently (`rec+0x4D`, `+0x4C`, `+0x58`) | a different period or wrap point from the first frame | the trace from `f = 0x293` | refuted: both builds are equal from `0x293` to `0x33F` (`at_summary.py` "first difference f=340"), including the wrap `0 → 0x1D` at `0x337` (the bottom branch `0x37B16..0x37B21` agrees) |
| **C5** | **0x37A58's top wrap compares the wrong byte** (`0x37B03 mov edx,[ebx+0x4f]` + `0x37B08 sar edx,0x18` is the byte at `rec+0x52`; the port reads `rec+0x4F`) | the first time the idle frame counts **up** past `0x1D`, the original wraps to 0 and the port goes on to `0x1E`, `0x1F`, … (sprites past the character's 30 idle frames) until the next countdown expiry turns it | the trace (first out-of-range value and its frame); the original's bytes under the E1 emulator on that input; the gp-idle-loss frame ratchet with the one-byte fix | **confirmed** by all three (§C5.3) |

**Evidence standard (what the plan's gate requires):** a candidate is the cause only when
(i) the raw shows the instruction that differs, (ii) the original's bytes, run alone on the
discriminating input, give the value the capture implies while the port gives the other, and
(iii) the change confined to that instruction moves the gp-idle-loss first unexplained frame past
the character select with the 45 oracle lines, the WAV and the K11 lines unchanged. Anything less
is a named gap with the ratchet pinned at its measured value.

## §C5.2 The raw of 0x37A58 (the fighters' idle-animation tick)

Not a function in either Ghidra list: live Ghidra `decompile_function 0x37A58` → `No function
found for 0x37a58`; `grep -i 37a58 port/decomp/prage.functions.csv` → no line. It is reached as an
animation opcode-`0x10` target (`actors.c` `anim_code_37A58`, registered by `fn_register(0x37A58u,
…)` at `actors.c:192`). Raw (`$S/img_r2.bin`, capstone):

```
037A9B  8a4358         mov al, byte ptr [ebx + 0x58]
037A9E  8a7352         mov dh, byte ptr [ebx + 0x52]
037AA1  00c6           add dh, al
037AA3  31c0           xor eax, eax
037AA5  66a1004b1000   mov ax, word ptr [0x104b00]
037AAB  887352         mov byte ptr [ebx + 0x52], dh        ; the new frame
...
037B03  8b534f         mov edx, dword ptr [ebx + 0x4f]      ; bytes +0x4F..+0x52
037B06  31c0           xor eax, eax
037B08  c1fa18         sar edx, 0x18                        ; = (s8) byte +0x52
037B0B  8a434d         mov al, byte ptr [ebx + 0x4d]
037B0E  39c2           cmp edx, eax
037B10  7c04           jl 0x37b16
037B12  c6435200       mov byte ptr [ebx + 0x52], 0
037B16  807b5200       cmp byte ptr [ebx + 0x52], 0
037B1A  7d08           jge 0x37b24
037B1C  8a434d         mov al, byte ptr [ebx + 0x4d]
037B1F  fec8           dec al
037B21  884352         mov byte ptr [ebx + 0x52], al
```

`0x37B03` loads the little-endian dword at `rec+0x4F`; `sar edx,0x18` keeps its top byte, which is
`rec+0x52` (`0x4F + 3`), the frame stored at `0x37AAB`. The port (`port/src/game/actors.c:1387`,
at `e9271df`):

```c
    if ((s32)(s8)DSB(rec + 0x4fu) >= (s32)DSB(rec + 0x4du))
        DSB(rec + 0x52u) = 0;                               /* 0x37B12 */
```

compares `rec+0x4F` (the child count, 0 for the select fighter). `rec+0x4D = 0x1E` for the
character-select fighter (`0x43A7D`, `fight.c` `fight_char_select_actor`), so the original wraps
`0x1D + 1` to 0 and the port keeps `0x1E`.

## §C5.3 The discriminating runs

**(a) The original's bytes under the E1 emulator** (`$S/e1/orig_idle_tick.py`, the plan's Task 1
Step 2 script; `tools/diff_emu.py` `run_original` on `$S/img_r2.bin`, record at `0x10A000`,
`allow_calls=(0x5D7DC,)`), output verbatim:

```
top 0x1D +1, 0x4F=0      outcome=ok rec+0x52 -> 0x00
kids 3 +1, 0x4F=0x20     outcome=ok rec+0x52 -> 0x04
mid 0x10 +1, 0x4F=0      outcome=ok rec+0x52 -> 0x11
bottom 0 -1              outcome=ok rec+0x52 -> 0x1D
```

The port at `e9271df` gives `0x1E` for "top" and `0` for "kids" (the unit assertions of Task 2,
run under the unfixed operand: `FAIL …test_game.c:4776: 30 != 0`, `FAIL …:4790: 0 != 4`,
`FAILURES: 2`; with the fix `all checks passed`).

**(b) The port's character-select fighter, frame by frame** (`$S/instr_at.py` applied to a fresh
clone of `e9271df` in `$S/t1`, the gp-idle-loss script cut at `f = 1120`; `$S/at_summary.py`),
output verbatim:

```
current: 462 frames, max v52 3B, first out of 0..1D 0x340 id AD78, flips [('0x33d', '01'), ('0x39a', 'FF'), ('0x454', '01')]
fixed: 462 frames, max v52 1D, first out of 0..1D none, flips [('0x33d', '01'), ('0x39a', 'FF'), ('0x454', '01')]
first difference: f=340 current v52=1E fixed v52=0
```

Reading: the countdown `rec+0x4C` expires at `f = 0x33D` and `rng(2)` turns the direction to `+1`
in both builds (equal draws). At `f = 0x340` the frame passes `0x1D`: the original wraps to 0
(sprite `0xAD5A`, the first idle frame; the idle set is `0xAD5A..0xAD77`, 30 frames), the current
port draws `0x1E` (sprite `0xAD78`, outside the set) and climbs to `0x3B` until the next expiry at
`0x39A` turns it down. That is exactly U4's observation: the original's scene "runs backwards"
from `f ≈ 0x340` (ascending ids `0xAD5A, 0xAD5B, …` are the frames the port showed earlier while
descending), and the port "reverses at `f ≈ 0x3A0`" (`0x39A`, 93 frames after `0x33D`).
Candidates C1-C4 predict none of this (§C5.1).

**(c) The gp-idle-loss oracle with the one-byte fix** (scratch clone `$S/r2` = `e9271df` plus
the four edits of the plan's Tasks 2-3: the operand `0x4f → 0x52`, the Tick C/D assertions, the
`0x3640C` row removed from `k_miss_gp_idle_loss`, `GP_IDLE_LOSS_MIN_FIRST = 787`; `make verify`
with the parallel-safe overrides into `$S/vo2`, log `$S/verify_r2.txt`):

```
gp_compare: gp-idle-loss: frames: window from capture 90 (raw 1744); 690 classified: 487 clean, 202 splice, 0 transition, 1 unexplained, 8 all-black
gp_compare: gp-idle-loss: frames: FIRST UNEXPLAINED capture 787 (raw 3899): nearest port 575, rows 147..199, x 0..319 (11357 px)
gp_compare: gp-idle-loss: frames: first unexplained 787, ratchet N 787 ok
gp_compare: gp-idle-loss: trace: first difference f=828 (2088) in rng: capture 73A05D37, port CE92DD04
gp_compare: gp-idle-loss: trace: first differing 2088, ratchet N 2088 ok
verify-exit=0
```

The frame claim now runs from capture 90 through the whole character select (`f = 0x293..0x640`),
the time-out wipes, `0x11`, `0x17`, the round start (mode 5) and into round 1 up to capture 787
(raw 3899), whose nearest port frame by the row-hash heuristic is 575 (`f = 0x823`, mode 6; `nearest()` is a heuristic, see §C5.19 item 9, and no longer the evidence). Pixel measurement (Task 9 fix wave, §C5.19 item 13): the capture frame before it, 786, is a splice of port 574/575 (`f = 0x822`/`0x823`; 575 alone differs in 1878 px), and for capture 787 the port frame with the fewest differing pixels is 576 (`f = 0x824`, 7470 px; port 575 differs in 11357 px), the frame where the unregistered `0x23208` is missed (record §G.24, divergence 2, U6's). The trace ratchet is unchanged (2088). Gate
evidence from the same run: the 45 oracle lines `diff` against
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt` → `ORACLES-EQUAL`;
`make audio-render AUDIO_WAV=$S/vo2/a.wav` → `cmp` with `before-t2.wav`: `WAV-IDENTICAL`;
`k11_compare: walk: … 0 unexplained`, `menuesc: … 0 unexplained` (the same lines as the base);
`python3 tools/port_progress.py` → `771 1203 64` / `731 731 100` (0x37A58 has no `FN_` symbol:
the counters do not move).

**The miss set moves by one pair.** With the fix the full gp-idle-loss replay records 6 pairs
(`distinct=6`), not 7: `0x3640C anim_indirect` (U4: one hit at `f = 0x173A`, round 2) is no longer
reached. It lies 3 890 frames after the trace's first difference (`0x828`), in a round that no
longer follows the original (record §G.20: round 1 is a time-up in the port, a KO in the
original), so its presence depended on the port's own post-divergence path; the fix changes which
idle frames reach 0, and so the mode-6 `rng(3)` draws of `0x37AAE..0x37AD6`. Without removing the
row the driver fails `test_platform.c:184: 6 != 7` (the earlier planner's run,
`$S/verify_fix.txt:300`). The converse was run too: the same replay with only the operand
switched back (a scratch build, `$S/r3`, the old operand selected by an environment switch;
`$S/bugdump`) records `distinct=7` (`0x3640C` again, `7 != 6` against the edited set) and
`gp_compare … --min-first 787` prints `frames: FAIL: first unexplained 203 < ratchet N 787`,
`trace: first differing 2088, ratchet N 2088 ok`. `0x3640C` stays an unported animation-opcode
target (U0's list, track P); only the replay stops reaching it.

**Conclusion:** C5 meets the evidence standard (i)-(iii). Divergence 1 (spec O1) is the
`0x37B03` operand; the plan fixes it.

## §C5.4 The character select: input, cursor, confirm (raw)

**The pad path.** `0x500C4` (raw above, `$S` listing): `raw = (+0x2D8 << 24) | (+0x2D9 << 8)`;
`changed = old ^ raw`; `level = (~changed & raw) | (changed & level)` (a bit that changed this
iteration keeps its previous level: a one-iteration press never reaches the level; record §G.7);
`latch &= level`; then the repeat: when `changed & old level & rpt` is non-zero or `level & rpt` is
zero the counter `DS_000E1C40` reloads `DS_000E1C42`; else it counts down and at 0 reloads
`DS_000E1C44` and clears the repeat bits from the latch `DS_000E1C38` (`0x5010F..0x50128`).
`0x4F644` (`input.c` `input_state_update`): `new = select(0xFF00FF00)` (the edge against the
latch), `held = select(0x00FF00FF) & 0xFF00FF00`; `e0 = (new >> 24) | ((held >> 24) << 8)`.
So the **low byte of `e0` is P1's newly pressed bits** (an edge, once per press) and the high
byte the held bits.

**The repeat in mode `0x10`.** `0x50146` writes `DS_000E1C3C/42/40/44`. Raw `E8` call scan of the
code object: four callers, `0x251DA`, `0x2CFF3`, `0x30B40`, `0x3128B`. Live Ghidra
`get_xrefs_to 0x50146` lists three (`0x251DA`, `0x2CFF3`, `0x3128B`); the raw's `0x30B40` is the
fourth (raw wins). `0x251C6..0x251DA` (mode `0x27`, every MAIN/START MENU frame): `mask
0xC000C000, first 0x1E, next 0xF`. The other three are the blocking service-menu editors
(`0xF000F000`), not on the U5 path. The image's initial words are 0 (`0xE1C3C` = `00000000`).
So in mode `0x10` the repeat mask is `0xC000C000` (P1 and P2 up/down only), left by the last
mode-`0x27` frame: **up/down held for more than `0x1E` iterations repeat every `0xF`; left,
right and the buttons never repeat.** A model of the two routines (`$S/inputsim.py`, the earlier
planner's, re-run) gives: right held 1 → no edge; held 2 → one edge at the second frame (`e0 =
0x1010`); up held 40 → edges at 11 and 40 (the repeat); two presses of right separated by 0
frames → one edge, by 2 frames → two edges.

**The stick and the confirm** (`0x43B24`, raw `$S` listing, the port's `fight_char_select_pass`
agrees line by line): for each side whose byte `DS_00108170[side]` is 1 or above 2,
`stick = e0[side] & 0xF0` (`0x43BB5..0x43BC2`, AH cleared):

| `stick` | raw | effect on the signed cursor `c = DS_00108166[side]` |
|---|---|---|
| `0x10` (right) | `0x43C75..0x43C83` | `c < 6` → `c + 1` |
| `0x20` (left) | `0x43C8B..0x43C99` | `c > 0` → `c − 1` |
| `0x40` (down) | `0x43C49..0x43C6C` | `c < 4` → `c + 4`; then `c >= 7` → 6 |
| `0x80` (up) | `0x43C32..0x43C40` | `c >= 4` → `c − 4` |
| other non-zero | `0x43C9F` | unchanged |

Every non-zero `stick` then runs `0x43EA0` (portrait, highlight, voice `0xC888A[c]`) and `0x43FBC`
(the fighter's select stream `0xBB988[class]`). `e0 & 1` (`0x43CAD..0x43CBD`) confirms through
`0x43D60`. **The grid is 7 cells, rows `0..3` and `4..6`** (the portrait y words `0xC88A6[c]` =
`0x540` ×4, `0xEC0` ×3; x `0xC8898` = `0x1000 0x1C80 0x2900 0x3580 0x1640 0x22C0 0x2F40`).

**The cursor start.** `0x4367C` (the per-game hook): `DS_00108173 == 0` → `0x43700..0x43722`
stores `DS_00108166[side] = 0xC8880[side]` = **`00 05`** (P1 on 0, P2 on 5) and
`DS_0010816E[side] = 0xFF`; `!= 0` → the rotating `DS_00108168/9` (`0x436A4..0x436FE`). The image
byte `0x108173` is `00`, and the raw address scan finds seven references, every one a read
(instruction starts `0x114C6` `mov dl`, `0x288DB` `mov dl`, `0x43103` `cmp`, `0x4369B` `cmp`,
`0x437BD` `cmp`, `0x4455C` `cmp`, `0x46596` `cmp`); U4's capture
confirms the 0-path at runtime (the countdown starts at `0xF`: the time-out at the 15th
`f & 0x3F == 0` step, record §G.24 item 5).

**The class table** `0xC8882[c]` (`c = 0..6`) = **`00 02 03 01 06 04 05`**; the cursor voices
`0xC888A[c]` = `0xC0 0xC2 0xC4 0xC1 0xC5 0xC6 0xC3`. Names: the audit string at `0x80A54`
(`prage.strings.csv:99`) reads `Selects Per Character: SAURON BLIZZARD TALON VERTIGO ARMADON
DIABLO CHAOS`, and the pick audit `0x43E77` counts by class (`0x2E934(2, class)`), so class
`0..6` = SAURON BLIZZARD TALON VERTIGO ARMADON DIABLO CHAOS is the **inferred** naming (U4's capture
shows Sauron for P1's start cell, class 0). Cell → class → name: `0→0 SAURON`, `1→2 TALON`,
`2→3 VERTIGO`, `3→1 BLIZZARD`, `4→6 CHAOS`, `5→4 ARMADON`, `6→5 DIABLO`. Two of the seven are
corroborated by a capture: `gp-idle-loss` capture frame 787 (rendered by the plan's Task 7
`pngs.py`, `$S/cap_787.png`) shows the HUD names SAURON (P1, class 0) and BLIZZARD (the CPU, whose
move table is "character 1", record §G.24). The plan claims class indices only.

**The confirm** (`0x43D60`): `DS_0010816A[side] = 0xC8882[c]`, side byte `= 2`,
`DS_00107813 + side*0x94 = 0`, `DS_00105B34[side] = 0x43D0C(side)` (1/2/3 for the *held* `e0`
bits `0x200/0x400/0x800`, i.e. b1/b2/b3 held, else 0), the stage drop `0x4248C`,
`DS_0010816E = class`, the audit `0x2E934` (deferred in the port: `fight.c` `PORT:` comment at
`0x43E77`), the entry sprite dead, `0x432EC`, voice `0x6C`. On the next pass the side byte 2
with the other side's byte 0 (P2 not joined) arms the hook `0x430E8` and the wipe to `0x11`
(`0x43BF0..0x43C1E`).

**The time-out.** `0x43CF0..0x43D01`: `0x43AAC` runs when `DS_000EF6DC & 0x3F == 0`; the countdown
`DS_0010816C` starts at `0xF` (`0x437C6`) and at 0 confirms every side whose byte is 1. The
earliest time-out is therefore **14 × 64 = 896 frames** after mode `0x10` begins (the first step
can fall on its first frame), the latest 960.

## §C5.5 What a one-player ARCADE capture can and cannot show

- **Selectable characters:** 7 cells, cursor `0..6`, clamped by `0x43C7E` (`< 6`), `0x43C93`
  (`> 0`), `0x43C67` (`>= 7 → 6`), `0x43C3B` (`>= 4`). Every cell is reachable in two rows; the
  walk of §C5.6 visits all seven.
- **Hidden or boss characters, an unlock:** none in this path. The cursor byte's writers (raw
  address scan of `0x108166`: `0x43C42`, `0x43C59`, `0x43C6E`, `0x43C85`, `0x43C9B` (the stick),
  `0x445E0`, `0x448xx` (the `DS_00104B1D == 3` team pass `0x44798`), and the indexed init at
  `0x43719`/`0x436B7`) keep it in `0..6`, and P1's class comes only from `0xC8882[c]` (7 entries;
  `0x43D6E`). Other writers of the class byte `DS_0010816A` (`0x2719C`, `0x292E8`, `0x4136F` =
  `0x41350`, the CPU's character from `DS_00104AFC`) are not P1's pick. Not shown by U5: that an
  indexed write elsewhere cannot set `DS_00108173` (the scan finds only absolute-address reads).
- **The second-player slot:** in LEFT PLAYER ARCADE `DS_00104B1F = 1`, side 1's byte stays 0, and
  each frame `0x43B24` polls `0x11F28(1)` (P2's start with a credit) and otherwise draws the prompt
  `0x432A0` (credits 4 > 0: "press start"). U5 presses nothing for P2: the capture shows the prompt
  only. P2 joining and walking is U7's.
- **The cursor and class bytes are not traced fields** (`SNAP_FIELDS` has no `0x108166`/
  `0x10816A`): the trace claim sees the pad (`raw`, `pad`, `e0`) and `mode/rng`; the cursor is seen
  only in the pictures (portrait highlight, name, fighter, voice not compared).
- **Not exercised:** the button variant `DS_00105B34 != 0` (b1/b2/b3 held at the confirm), the team
  pass (`DS_00104B1D == 3`, U8), the `DS_00108173 != 0` path, the time-out confirm (U4's capture
  already covers it), the audit count `0x2E934` (port-deferred; CMOS, not in frames or trace).

## §C5.6 The walk and its port prediction

**The script** (`SCENARIOS['gp-u5-charsel']`, the plan's Task 4 code; harness values with their
source): boot Enter at `ENTER_WAIT`, Enter at mode `0x27` + 150, Enter + 150 (gp-idle-loss's
path to mode `0x2D`); then from the first frame of mode `0x10` + 60: `p1.left` (a no-op at 0: the
clamp), `p1.right` ×3 (`1, 2, 3`), `p1.down` (`3 + 4 = 7 → 6`, the clamp), `p1.left` ×2 (`5, 4`),
`p1.up` (`0`), `p1.right` (`1`), then `p1.start` (confirm cell 1, class 2); each held 6
iterations, 40 apart; the end at the first frame of mode 6. Cells visited `0 0 1 2 3 6 5 4 0 1`:
all seven. Hold 6: `>= 2` (the level, `0x500C4`) and `< 0x1F` (no up/down repeat, §C5.4). Gap 40:
`>= hold + 2` (separate edges, `inputsim.py`). Confirm 420 frames into mode `0x10` < 896 (the
earliest time-out).

**The port's prediction** (`$S/sim/sim.script`, the plan's Task 5 generator: gp-idle-loss's three
Enter frames 321/474/622, mode `0x10` at `f = 0x293`, the walk at `0x293 + 60 + 40k`, each pad's
BIOS word consumed 2 frames after its press; replayed by `PR_GP_DUMP` on `$S/r3` = the fixed port
plus throw-away `fprintf`s in `fight_char_select_pass`/`fight_char_countdown`/`fight_char_confirm`),
verbatim:

```
U5DBG f=02C0 countdown 15
U5DBG f=02D0 side=0 stick=20 cursor=0 class=0
U5DBG f=02F8 side=0 stick=10 cursor=1 class=2
U5DBG f=0300 countdown 14
U5DBG f=0320 side=0 stick=10 cursor=2 class=3
U5DBG f=0340 countdown 13
U5DBG f=0348 side=0 stick=10 cursor=3 class=1
U5DBG f=0370 side=0 stick=40 cursor=6 class=5
U5DBG f=0380 countdown 12
U5DBG f=0398 side=0 stick=20 cursor=5 class=4
U5DBG f=03C0 side=0 stick=20 cursor=4 class=6
U5DBG f=03C0 countdown 11
U5DBG f=03E8 side=0 stick=80 cursor=0 class=0
U5DBG f=0400 countdown 10
U5DBG f=0410 side=0 stick=10 cursor=1 class=2
U5DBG f=0438 side=0 CONFIRM cursor=1
U5DBG confirm side=0 cursor=1 btn=0
```

Each edge lands one frame after its press (`0x293 + 60 = 0x2CF`, edge `0x2D0`). Mode path of the
port (`trace.txt`): `0x10` at `0x293`, `0x1A` `0x439`, `0x1B` `0x44B`, `0x11` `0x45D`, `0x17`
`0x45E`, `0x1A` `0x54F`, `0x1B` `0x561`, **5 at `0x573`, 6 at `0x5EE`** (the same mode sequence
as U4's time-out path, record §G.18; spec §3.5 has no `0x15` before the round: `0x15` follows the
match). The replay to `end 1518` (`f = 0x5EE`) records `fn-miss … distinct=4`: the base pair plus
`0x29D60 frontend_mode_1b_step` and `0x5D812 frontend_mode_1b_step` (both classified harmless in
§G.24); 360 `.ipx`, 23 MB. Run on to `end 2300` it adds `0x3A588 fighter_state_3531c` (654 hits,
round 1: U6's divergence), which is why the scenario ends at mode 6's first frame.

## §C5.7 Size and time estimates (for the storage decision)

From `data/k11-captures/gp-idle-loss` (read-only): its frames up to raw 4206 (~60 s of DOSBox at
70.09 Hz) are 1 095 files, 53.5 MB; average sizes by stretch: before mode `0x10` 28.5 KB, the
character select 59.5 KB, `0x1A..5` 38.3 KB, the fight 45.5 KB. U4's wall clock: mode `0x10` at
`ms = 30955`, the 437 frames from the time-out wipe to mode 6 took 8.3 s (`0x640` at 46 739 ms,
`0x7F5` at 55 068 ms). U5's mode 6 is expected at ≈ 31.0 + 7.0 (420 frames) + 8.3 ≈ 46.3 s, so
`time_limit = 60` leaves ~14 s. Expected capture ≈ 55 MB (the same 60 s of DOSBox at U4's
density); the upper bound, every AVI frame from raw 1371 to 4206 distinct at 60 KB, is 170 MB.
The port dump to mode 6 is 23 MB (§C5.6). Measured in the plan's Task 6.

## §C5.10 Baseline

Run in the worktree `.worktrees/gameplay-u5c` (branch `gameplay-u5c`; the plan's `.worktrees/gameplay-u5` was already set up under this name). Base commit: `80db456` (`main` `e9271df` plus the committed plans).

`make verify` (through the `$S/mkv` wrapper, `S=/tmp/gameplay-u5`) ends `all checks passed`, `verify-exit=0`. The `gp-oracle` and miss-set lines, as measured:

```
gp_compare: gp-idle-loss: frames: first unexplained 203, ratchet N 203 ok
gp_compare: gp-idle-loss: trace: first differing 2088, ratchet N 2088 ok
fn-miss PR_GP_DUMP distinct=2 dropped=0     (gp-pads)
fn-miss PR_GP_DUMP distinct=7 dropped=0     (gp-idle-loss)
```

The baseline log is `/tmp/gameplay-u5/base_verify.txt` (outside the repo). The gate check: the 45 oracle lines equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt` (`ORACLES-EQUAL`); `make audio-render` is byte-identical to `.superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav` (`WAV-IDENTICAL`); `python3 tools/port_progress.py` prints `771 1203 64` and `731 731 100`. The 11 `k11_compare:` lines (walk 5, menuesc 6) are saved as the baseline (`$S/k11_base.txt`; the `K11-EQUAL` diff is trivially equal here): walk `0 unexplained in the window` with `k11_compare: walk: allowed by name: [138, 151]` (the two mid-draw frames; `/tmp/gameplay-u5/k11_base.txt:4`), menuesc plain `0 unexplained in the window` (it has no allowed frames; `tools/k11_compare.py:182` prints that line), menuesc open end `END 388 must be >= 388: ok`. The base is the one the plan was measured on.

## §C5.11 The gate

Measured at `80db456` on the unmodified source (no source change committed). Scratch only: `$S/img.bin` (`build/diffrun --exe data/game/C/PRAGE.EXE --image-out`), `$S/orig_idle_tick.py`, `$S/instr_at.py`, `$S/at_summary.py`, `$S/t1/` (a throw-away clone, instrumented, never committed).

### (i) The raw (the fixed-up image, capstone, `0x37A9B..0x37B24`)

```
037B03  8b534f           mov edx, dword ptr [ebx + 0x4f]
037B06  31c0             xor eax, eax
037B08  c1fa18           sar edx, 0x18
037B0B  8a434d           mov al, byte ptr [ebx + 0x4d]
037B0E  39c2             cmp edx, eax
037B10  7c04             jl 0x37b16
037B12  c6435200         mov byte ptr [ebx + 0x52], 0
037B16  807b5200         cmp byte ptr [ebx + 0x52], 0
```

The dword at `rec+0x4F` shifted right arithmetically by 24 is the signed byte at `rec+0x52`. `port/src/game/actors.c:1387` reads `if ((s32)(s8)DSB(rec + 0x4fu) >= (s32)DSB(rec + 0x4du))`.

### (ii) The original's bytes on the discriminating input (`python3 $S/orig_idle_tick.py . $S/img.bin`)

```
top 0x1D +1, 0x4F=0      outcome=ok rec+0x52 -> 0x00
kids 3 +1, 0x4F=0x20     outcome=ok rec+0x52 -> 0x04
mid 0x10 +1, 0x4F=0      outcome=ok rec+0x52 -> 0x11
bottom 0 -1              outcome=ok rec+0x52 -> 0x1D
```

### (iii) The port's character-select fighter, current against fixed (scratch clone; `PR_AT=1`, `PR_GP_DUMP`, the 1120-frame script of `gp-idle-loss`; 462 `AT` lines each)

```
current: 462 frames, max v52 3B, first out of 0..1D 0x340 id AD78, flips [('0x33d', '01'), ('0x39a', 'FF'), ('0x454', '01')]
fixed: 462 frames, max v52 1D, first out of 0..1D none, flips [('0x33d', '01'), ('0x39a', 'FF'), ('0x454', '01')]
first difference: f=340 current v52=1E fixed v52=0
```

### The candidates (§C5.1) against this run

| # | prediction | this run |
|---|---|---|
| C1 timer rate | same values shifted in time; no value outside the idle set | refuted: a value outside `0..0x1D` (`0x1E`, max `0x3B`) in the current build; the fixed build never leaves `0..0x1D` |
| C2 other counter | drift growing with the tick offset | refuted: the direction flips (`rec+0x58`) are identical with and without the fix (`0x33D`, `0x39A`, `0x454`) |
| C3 ordering | a one-frame offset | refuted: the first difference is a value (`0x1E` against `0`) at one frame, `f = 0x340` |
| C4 initial state | different period or wrap from the first frame | refuted: the traces are equal from the first `AT` line to `0x33F` (the first difference is `f=340`) |
| C5 wrong byte at `0x37B03` | out-of-range value at the first up-pass of `0x1D`; original wraps to 0 | confirmed by (i), (ii), (iii) |

### The gate

| evidence | required | observed |
|---|---|---|
| (i) raw | `0x37B03 mov edx,[ebx+0x4f]` + `0x37B08 sar edx,0x18` | both present, verbatim |
| (ii) original's bytes | `top … -> 0x00`, `kids … -> 0x04` | `0x00` and `0x04` (all four lines verbatim) |
| (iii) port frame | `first difference: f=340 current v52=1E fixed v52=0` | verbatim (all three lines verbatim) |

**Decision: PASS. All three as expected, go to Task 2 (Task 3 adds the capture-level evidence).**

## §C5.12 The fix (Task 2)

`anim_code_37A58` (`port/src/game/actors.c`) now compares `rec+0x52` at the top wrap, the byte `0x37B03 mov edx,[ebx+0x4f]` / `0x37B08 sar edx,0x18` leaves in `edx` (§C5.11 (i)). One operand changed (`0x4f` to `0x52`); the header comment states the raw.

**Step 2 (RED, the new Tick C and Tick D against the unfixed operand):**

```
FAIL .../port/tests/test_game.c:4776: 30 != 0
FAIL .../port/tests/test_game.c:4790: 0 != 4
FAILURES: 2
```

**Step 4 (GREEN):** `all checks passed`. A mutation of the fix (`>=` to `>` at `0x37B10`) fails Tick C alone (`test_game.c:4776: 30 != 0`, `FAILURES: 1`); restored.

**Step 5 (the gp-idle-loss miss set).** The replay no longer reaches `0x3640C` (the divergent idle frame `0x1E` was the sprite id that led to it, §C5.3). With the row still present:

```
fn-miss PR_GP_DUMP distinct=6 dropped=0
FAIL .../port/tests/test_platform.c:185: 6 != 7
```

After deleting the `0x3640C anim_indirect` row and rewording the comment: `distinct=6`, `all checks passed`. (The comment's `5353` hits of `0x3A588` was U4's measurement on the old trajectory; this run records `hits=4995` for it. Task 2 left the comment as U4 wrote it; Task 3 corrected it, §C5.13.)

**Step 6 (gate, `make verify` through `$S/mkv`; log `/tmp/gameplay-u5/t2_verify.txt`):** `all checks passed`, `verify-exit=0`.

```
gp_compare: gp-idle-loss: frames: FIRST UNEXPLAINED capture 787 (raw 3899): nearest port 575, rows 147..199, x 0..319 (11357 px)
gp_compare: gp-idle-loss: frames: first unexplained 787, ratchet N 203 ok (improved: raise N)
gp_compare: gp-idle-loss: trace: first differing 2088, ratchet N 2088 ok
fn-miss PR_GP_DUMP distinct=2 dropped=0     (gp-pads)
fn-miss PR_GP_DUMP distinct=6 dropped=0     (gp-idle-loss)
```

The gate check: `ORACLES-EQUAL` (45 lines), `K11-EQUAL`, `WAV-IDENTICAL`, `771 1203 64`, `731 731 100`. The first unexplained capture frame moves from 203 to 787 (the plan's predicted value); Task 3 pins it.

## §C5.13 The frame ratchet pin (Task 3)

**Step 1 (the nearest port frame, `/tmp/pr_u5_gp/gp-idle-loss/frames.txt`, Task 2's nearest port index 575):** `00575 f=0823 tick=00000881 mode=0006`; the next line is `00576 f=0824 tick=00000882 mode=0006`, so the first unexplained capture frame sits one port frame before the unregistered move callback `0x23208` (divergence 2, record §G.24).

**Step 2 (pin):** `GP_IDLE_LOSS_MIN_FIRST` 203 to 787 (raw 3899), the value Task 2's gate measured (`FIRST UNEXPLAINED capture 787 (raw 3899): nearest port 575`); the Makefile comment now states the cause of the old 203 (the `0x37A58` top wrap's operand, §C5.12) and what the claim covers.

**Step 3 (it holds, and it fails):**

```
frames: first unexplained 787, ratchet N 787 ok
trace: first differing 2088, ratchet N 2088 ok          (make gp-oracle, exit 0)
frames: FAIL: first unexplained 787 < ratchet N 788      (GP_IDLE_LOSS_MIN_FIRST=788; the log ends `make: *** [gp-oracle] Error 1`, /tmp/gameplay-u5/t3_n788.txt)
```

With the old operand (`rec+0x4f`, one line changed in `actors.c`, rebuilt, then restored with `git checkout`):

```
fn-miss PR_GP_DUMP distinct=7 dropped=0
FAIL .../test_platform.c:188: 7 != 6
FAIL .../test_platform.c:193: the driver's miss log holds only its pinned known-set
gp_compare: gp-idle-loss: frames: FAIL: first unexplained 203 < ratchet N 787
gp_compare: gp-idle-loss: trace: first differing 2088, ratchet N 2088 ok
```

**Comment correction (`test_platform.c`, `k_miss_gp_idle_loss`):** the `0x3A588 fighter_state_3531c` line read `5353 hits from f = 0x927` (U4, before the fix). Measured after the `0x37B03` fix: `hits=4995` (the Task 2 verify log, `/tmp/gameplay-u5/t2_verify.txt`, `fn-miss PR_GP_DUMP 0x3A588 fighter_state_3531c hits=4995`, and again in the Task 3 replay) and the first hit at `f = 0x8E7` (re-measured reproducibly in Task 9, below). The comment now states both, labelled as measured after the fix, with U4's figures kept as the pre-fix ones.

**The first-hit frame `0x8E7`, re-measured (Task 9).** Task 3's one-sentence method had no captured output. Method: a temporary line at the top of `fn_resolve_from` in `port/src/mem.c` (NOT committed; `git checkout port/src/mem.c` afterwards; the diff is `/tmp/gameplay-u5/t9_instr.diff`), `if (orig_addr == 0x3A588u) { static int n; if (n++ < 3) fprintf(stderr, "INSTR 0x3A588 miss #%d at f=0x%X (%u)\n", n, (unsigned)DSW(0xEF6DCu), (unsigned)DSW(0xEF6DCu)); }`, where `DS_000EF6DC` is the replay's `f` (the driver's own `const u32 f = DSW(DS_000EF6DC)`, `test_game.c` `test_gp_replay`), then the replay `$S/mkv gp-replay scenario=gp-idle-loss` (log `/tmp/gameplay-u5/t9_instr.txt`, `exit=0`, `all checks passed`). Printed output, verbatim:

```
INSTR 0x3A588 miss #1 at f=0x8E7 (2279)
INSTR 0x3A588 miss #2 at f=0x8E8 (2280)
INSTR 0x3A588 miss #3 at f=0x8E9 (2281)
fn-miss PR_GP_DUMP 0x3A588 fighter_state_3531c hits=4995
fn-miss PR_GP_DUMP distinct=6 dropped=0
```

The first hit is `f = 0x8E7` (2279) and the count is `hits=4995`, both as the committed comment states; the figures stand. (`f` is the frame counter read at the call, while the iteration that leaves `f = 0x8E7` is running.)

**Step 4 (gate, `make verify` through `$S/mkv`, log `/tmp/gameplay-u5/t3_verify.txt`):** `all checks passed`, `verify-exit=0`; `FIRST UNEXPLAINED capture 787 (raw 3899)`, `first unexplained 787, ratchet N 787 ok`, `first differing 2088, ratchet N 2088 ok`, `fn-miss PR_GP_DUMP distinct=2` (gp-pads) and `distinct=6` (gp-idle-loss). The gate check: `ORACLES-EQUAL`, `K11-EQUAL`, `WAV-IDENTICAL`, `771 1203 64`, `731 731 100`.

## §C5.14 The `gp-u5-charsel` scenario (Task 4)

`tools/gp_session.py` `SCENARIOS['gp-u5-charsel']` (time_limit 60) with `CHARSEL_WALK`: the `gp-idle-loss` prefix (two Enter presses 150 frames apart to mode 0x2D), then in mode 0x10 a 60-frame wait and the nine stick presses `left right right right down left left up right` (hold 6, 40-frame gaps), `p1.start` to confirm, `until_mode 0x06`. The cursor path from the raw stick (§C5.4; `0x43B24`, `0x43BB5..0x43C99`) is `0 0 1 2 3 6 5 4 0 1`: all seven cells visited, the first `left` the `> 0` clamp (a no-op), the confirm on cell 1 (`e0` bit 0, `0x43CAD`). The confirm lands 60 + 9 * 40 = 420 frames after mode 0x10 begins, before the earliest pick time-out (14 * 64 = 896, §C5.4).

`TestCharsel` (`tools/tests/test_gp_session.py`) models the stick independently of the scenario (hold in `[2, 0x1F)`; every press separated from the previous by at least hold + 2; the schedule fires at `0x293 + 60 + 40k`, ends at the mode 6 frame with 13 of 13 steps fired).

**RED (Step 2):** `KeyError: 'gp-u5-charsel'`, `Ran 25 tests`, `FAILED (errors=3)`.

**GREEN (Step 4):** `Ran 25 tests`, `OK`; with `test_gp_capture` and `test_gp_compare`: `Ran 67 tests`, `OK`.

**Mutations (each applied to `tools/gp_session.py`, run, restored):**
- `'p1.right', 'p1.down',` to `'p1.right', 'p1.up',`: `FAIL: test_the_walk_visits_every_cell_and_confirms_on_1`.
- the hold 6 to 1 on the walk presses: `FAIL: test_the_walk_visits_every_cell_and_confirms_on_1`.
- the 40-frame gap to 7: `FAIL: test_presses_are_separate_edges_before_the_time_out` and `FAIL: test_schedule_fires_the_walk`.

Restored: `Ran 25 tests`, `OK`. No DOSBox-X capture was run (Task 6).

## §C5.15 The replay driver's miss set for `gp-u5-charsel` (Task 5)

`k_miss_gp_charsel[]` in `port/tests/test_platform.c` (`fnm_known` takes a fifth flag, `charsel`; `test_fn_misslog_driver` adds the set's size to `want` when the script's scenario line names `gp-u5-charsel`): `0x29D60 frontend_mode_1b_step` (a bare `ret`, the wipe's end into mode 0x10) and `0x5D812 frontend_mode_1b_step` (the runtime stub, the wipe into mode 5), both classified in §G.24 and predicted in §C5.6. The set is the port's own walk, cut at the first mode-6 frame (`end 1518` = `0x5EE`); a capture's replay replaces the stand-in pad frames in Task 7.

**The port's walk (Step 1's v2 script, pad frames on `gp-idle-loss`'s Enter frames, each pad's BIOS word consumed 2 frames after its press; `/tmp/gameplay-u5/sim/sim6.script`):** `fn-miss PR_GP_DUMP 0x29D60 frontend_mode_1b_step hits=1`, `0x5D812 frontend_mode_1b_step hits=1`, with the base pair `0x5D812 actor_spawn hits=3688` and `0x5D812 set_dead hits=3390`: `distinct=4 dropped=0`.

**RED:** before the set, `FAIL test_platform.c:188: 4 != 2` and `unexpected 0x29D60 from frontend_mode_1b_step`, `unexpected 0x5D812 from frontend_mode_1b_step` (`FAILURES: 3`, `/tmp/gameplay-u5/t5_red.txt`). **GREEN:** `distinct=4 dropped=0`, `all checks passed` (`t5_green.txt`).

**Mutations:** (a) the script's scenario line changed to `gp-u5-sim`: `FAIL: 4 != 2` and both `unexpected` lines (the set applies only to its scenario, `t5_mut1.txt`); (b) the `0x29D60` row deleted from `k_miss_gp_charsel`: `FAIL: 4 != 3` and `unexpected 0x29D60 from frontend_mode_1b_step` (`t5_mut2.txt`). Row restored: `all checks passed`.

**The walk's mode changes (the trace of the replay, `t5_walk.txt`):** `0x10` at `0x293`, `0x1A` at `0x439`, `0x1B` at `0x44B`, `0x11` at `0x45D`, `0x17` at `0x45E`, `0x1A` at `0x54F`, `0x1B` at `0x561`, `5` at `0x573`, `6` at `0x5EE`: equal to §C5.6's prediction.

## §C5.16 The `gp-u5-charsel` capture (Task 6)

**Decision 1 (storage), logged as §G.18 logged Q7:** the user approved the capture's storage (about 55 MB, at most 170 MB), relayed by the controller at Task 6's dispatch. The capture went only through `make gp-capture` (`/tmp/gameplay-u5/mkv`, the parallel-safe overrides); no other path under `data/` was written, and the existing captures were not touched. One run, no rerun (`end frame reached` ok on the first).

**Command:** `$S/mkv gp-capture scenario=gp-u5-charsel` (log `/tmp/gameplay-u5/cap.txt`). Output: `snapshots 2332, f 5..925, 5 frames missed (spec §3.7)` (`f 5..925` is hex: `0x5..0x925`; the five missed snapshot frames are `0x13A 0x1D0 0x268 0x445 0x446`, found by listing the `f` values absent from `gp_session.snapshots` over `0x5..0x925`; `trace_diff` and `gp_compare`'s trace claim skip a frame that has no snapshot, so those five are not compared; their modes are still checked from the `P` records, §C5.17); `CHECK base`, `steps fired 13/13`, `end frame reached`, `mode 0x27 after the Enter`, `snapshots kb == raw (0 differ)`, `frames written 1464/1464`, `port script v2`: all `ok`; `wrote 1464 frames to data/k11-captures/gp-u5-charsel (raw 1382..4203), wall 60.7s`.

**`session.txt`:** `scenario=gp-u5-charsel`, DOSBox-X 2026.08.31 SDL2, `time_limit=60 wall_s=60.7 rc=0`, `exe=/tmp/pr_u5_pin/PRAGE.EXE sha256=8120f1bd1df389ed94cb329c030f95d9e38caad193bbd9840717af557161a68d`, `cmos=zero pad_bios=1`, `avis=['prage_000.avi','prage_001.avi'] fps=70.0866 dro=['prage_000.dro'] frames=1464 raw_window=1382..4203 avi_frames=4204 twg_last=1331`.

**Size:** `du -sh data/k11-captures/gp-u5-charsel` = 68 MB (1470 entries), against §C5.7's ~55 MB estimate (under the 170 MB cap; the estimate was low by about 24%, the walk window has more frames/content than gp-idle-loss's). **Frames:** 1464. **`poll.log` sha256:** `138fb537cd8c364bab0ccc68fc8fdd9079de19f3b0fc789d427f6d9652a18d7e`.

**Step 2 (`/tmp/gameplay-u5/modepath.py data/k11-captures/gp-u5-charsel`), verbatim:**
```
poll.log:2 P f=0 mode=0
poll.log:3 P f=1 mode=3
poll.log:315 P f=13A mode=27
poll.log:622 P f=268 mode=2D
poll.log:623 P f=269 mode=1A
poll.log:643 P f=27B mode=1B
poll.log:662 P f=28D mode=10
poll.log:1114 P f=433 mode=1A
poll.log:1134 P f=445 mode=1B
poll.log:1151 P f=457 mode=11
poll.log:1153 P f=458 mode=17
poll.log:1395 P f=549 mode=1A
poll.log:1414 P f=55B mode=1B
poll.log:1433 P f=56D mode=5
poll.log:1557 P f=5E8 mode=6
e0 edges in mode 0x10: f=2CA:20(poll.log:726) f=2F2:10(poll.log:769) f=31A:10(poll.log:812) f=342:10(poll.log:854) f=36A:40(poll.log:897) f=392:20(poll.log:941) f=3BA:20(poll.log:984) f=3E2:80(poll.log:1027) f=40A:10(poll.log:1070) f=432:01(poll.log:1113)
sticks: [32, 16, 16, 16, 64, 32, 32, 128, 16, 1]
```

**Against the prediction (§C5.6, brief Step 2):**
- Mode path equal: `3, 0x27, 0x2D, 0x1A, 0x1B, 0x10, 0x1A, 0x1B, 0x11, 0x17, 0x1A, 0x1B, 5, 6`. Credits 5 to 4 at `0x2D`: `cred=5` at the S record f=0x267 (mode 0x27, poll.log:620; the line `poll.log:310` first cited was wrong), `cred=4` at f=0x269 (mode 0x1A, poll.log:624); f=0x268 (mode 0x2D) has an `H` and `P` record but no `S` (one of the 5 missed snapshots), so the edge frame itself is "not observed".
- Sticks equal: `[32, 16, 16, 16, 64, 32, 32, 128, 16, 1]` (left, right x3, down, left x2, up, right, confirm on cell 1, bit 0).
- **Deviation 1 (the brief's wording, not the port; mechanism corrected in Task 7):** each `e0` edge is TWO frames after its press's `I` record, not one. The mechanism: the `I` record is stamped F-1 (the injector's Schedule fires at the first snapshot with `f >= F-1`, `I ... f=0x340` at poll.log:852), the raw bits change at F (`S f=0x341 raw=10000000`, poll.log:853), and the `e0` edge is at F+1 (`S f=0x342 e0=1010`, poll.log:854): the edge one frame after the press, exactly as §C5.4 predicts; the brief's "one frame after the I record" was the mismatch. **The key-frame offsets (corrected in the Task 7 fix round; the earlier "the third `p1.right` is the one exception" was wrong).** Only the raw BITS are I+1 for all ten presses; the script's `key` frame is the capture's own consumption frame (the `H` record), and it varies from press to press. Derived from `poll.log` by `/tmp/gameplay-u5/t7_keyoffs.txt` (I record line:f, H record line:f, the first `S` whose `raw` changes), and equal to the generated script's `key` frames (13 keys: 314 464 616 713 753 793 835 875 914 954 994 1033 1073):

| press | I (poll.log:f) | H, the key's frame (poll.log:f) | H-I | bits change (poll.log:f) | bits-I |
|---|---|---|---|---|---|
| enter 1 | 312:0x138 | 314:0x13A | +2 | | |
| enter 2 | 466:0x1CF | 467:0x1D0 | +1 | | |
| enter 3 | 618:0x265 | 621:0x268 | +3 | | |
| left | 723:0x2C8 | 724:0x2C9 | +1 | 725:0x2C9 | +1 |
| right | 766:0x2F0 | 767:0x2F1 | +1 | 768:0x2F1 | +1 |
| right | 809:0x318 | 810:0x319 | +1 | 811:0x319 | +1 |
| right | 852:0x340 | 855:0x343 | +3 | 853:0x341 | +1 |
| down | 895:0x368 | 898:0x36B | +3 | 896:0x369 | +1 |
| left | 938:0x390 | 940:0x392 | +2 | 939:0x391 | +1 |
| left | 981:0x3B8 | 983:0x3BA | +2 | 982:0x3B9 | +1 |
| up | 1024:0x3E0 | 1026:0x3E2 | +2 | 1025:0x3E1 | +1 |
| right | 1067:0x408 | 1068:0x409 | +1 | 1069:0x409 | +1 |
| start | 1110:0x430 | 1111:0x431 | +1 | 1112:0x431 | +1 |

Five of the ten pad keys (the third right, the down, the two lefts, the up) and two of the three Enters (the first, I+2, and the third, I+3) differ from I+1: the BIOS word is consumed 1 to 3 frames after its press, the existing open cause Q3 (gameplay spec §7 Q3, "why a BIOS key is consumed 1-3 iterations after it is queued"; record §G gap item 6, "1-4 iterations later", which this capture's range of +1..+3 sits inside). The design does not depend on it (spec §7 Q3: the replay uses the observed consumption frame), and the replay does: the script carries the capture's `key` frames and the capture's `bits` frames, so the port replays the capture's own timing (§C5.17: 0 differing through 1512). No new gap is opened. The `e0` edges, measured: `I press` f=0x2C8 (poll.log:723) to the edge f=0x2CA (poll.log:726); likewise 0x2F0 to 0x2F2, 0x318 to 0x31A, 0x340 to 0x342, 0x368 to 0x36A, 0x390 to 0x392, 0x3B8 to 0x3BA, 0x3E0 to 0x3E2, 0x408 to 0x40A, and the confirm `I` f=0x430 (poll.log:1110) to f=0x432 (poll.log:1113). §C5.15's port walk modelled a constant consumption 2 frames after the press; the table above shows the real range is +1..+3, which is why Task 7 replaced the stand-in with the capture's own key and bits frames. Every edge frame was observed (none among the 5 missed snapshots).
- **Deviation 2 (timing, not behaviour):** the capture's mode-0x10 entry is f=0x28D and the confirm edge f=0x432, so the pick ends the mode at f=0x433 (one frame after the confirm edge, as predicted), 0x433 - 0x28D = 422 frames into mode 0x10 (predicted about 420; below the 896-frame earliest time-out). The port's own walk (§C5.15, stand-in pad frames) had `0x10` at `0x293` and `0x1A` at `0x439` (edge `0x438`): the capture is 6 frames earlier throughout (0x28D vs 0x293; 0x433 vs 0x439; 0x445 vs 0x44B; 0x457 vs 0x45D; 0x458 vs 0x45E; 0x549 vs 0x54F; 0x55B vs 0x561; 0x56D vs 0x573; 0x5E8 vs 0x5EE), a constant offset that comes from the stand-in Enter frames. The offset against `gp-idle-loss` (whose Enter frames the stand-in used), measured: this capture's third Enter `I` is f=0x265 (poll.log:618) with mode 0x2D at 0x268 (poll.log:622); `gp-idle-loss`'s third Enter `I` is f=0x26C (its poll.log:628) with 0x2D at 0x26E (its poll.log:631), a 7-frame offset on the `I` and 6 on the mode change; the interval 0x2D to 0x10 is 0x25 frames in both (this capture 0x268 to 0x28D; `gp-idle-loss` 0x26E to 0x293). Task 7 replaced the stand-in pad frames with the capture's, and the offset closed (§C5.17: the port's mode changes equal the capture's with their frames).
- The pick, not the time-out, ends mode 0x10: confirmed (edge bit 0 at f=0x432, `0x1A` at f=0x433).

## §C5.17 The port replay of `gp-u5-charsel` and its first divergences (Task 7)

**Command (Step 1, verbatim):** `$S/mkv gp-report scenario=gp-u5-charsel 2>&1 | tee $S/report.txt | grep -E 'fn-miss|FAIL|passed|gp_compare'` (`S=/tmp/gameplay-u5`; the whole output is `$S/report.txt`, the filtered lines `$S/t7_step1.txt`; run twice, the `gp_compare` lines identical). No stall, no fault: the driver prints `all checks passed`. The script `/tmp/pr_u5_gp/gp-u5-charsel.script` (37 lines) is generated from the capture: `# gp port script v2: scenario gp-u5-charsel`, `enter_frame 314` (0x13A), `enter_state 0000`, 13 `key` lines, 20 `bits` lines (10 presses, each a press and a release), `end 1512` (0x5E8: the capture's mode-6 frame; §C5.15 cut its stand-in walk at 1518 = 0x5EE, a correction: the capture's script ends at 1512). The script's header carries no `(cut at N)` (no `--end` was passed); had it been generated with `--end`, the header would read `(cut at N)` and the driver (`fnm_gp_scenario`, `test_platform.c`) would check only `count <= want`, so a cut script cannot prove the full set, and here the check is the exact `==`. Dump: `du -sh /tmp/pr_u5_gp/gp-u5-charsel` = 23 MB (362 entries: 359 `.ipx` frames 0..358, `frames.txt`, `trace.txt` (1199 `T` lines), `gp.log`); `frames.txt` ends `00358 f=05E8 tick=0000063F mode=0006`.

**Step 2, the miss set, against the pinned set:** `distinct=4 dropped=0`: `0x5D812 actor_spawn hits=3625`, `0x5D812 set_dead hits=3317` (the base pair), `0x29D60 frontend_mode_1b_step hits=1`, `0x5D812 frontend_mode_1b_step hits=1`. This is exactly the pinned gp-u5-charsel set (§C5.15) on the REAL capture's script; no pair was added and `port/tests/test_platform.c` is unchanged. The base pair's hit counts differ from §C5.15's stand-in walk (3688 and 3390) because the replay now ends at 0x5E8 and the pad frames are the capture's (the counts are not pinned).

**The mode trace against the capture, with frames (`$S/t7_modes.txt`, re-saved in the fix round with the one corrected run only: it prints the port's and the capture's mode lists and `port == cap from 0x13A: True`):** the port's `trace.txt` mode changes `27@13A 2D@268 1A@269 1B@27B 10@28D 1A@433 1B@445 11@457 17@458 1A@549 1B@55B 5@56D 6@5E8` are equal to the capture's `P`-record mode path from 0x13A (mode `3` at f=1 precedes the walk) change for change and frame for frame, 0x28D through 0x5E8 included: no frame differs. The 6-frame offset of the stand-in walk (§C5.15: `10@293`... `6@5EE`) is closed. (The `P` records, not the `S` snapshots, give the capture's frames, so the missed snapshots 0x268, 0x445, 0x446 do not hide a mode change.)

**Step 3, the frame claim:** `window from capture 100 (raw 1746); 411 classified: 285 clean, 121 splice, 0 transition, 5 unexplained, 10 all-black`; `FIRST UNEXPLAINED capture 516 (raw 3237): nearest port 294, rows 0..192, x 0..319 (14100 px)`, then 517..520 (the report stops at `REPORT_MAX` = 5, so 5 is the cap, not the count). Every content-bearing capture frame from 100 through 515 is explained. **It is how far the port got, not a defect (spec §4.3):** the port's last frame (`00358 f=05E8 mode=0006`, the script's `end`) is byte-identical to capture frame 515 (0 differing pixels, measured with `gp_compare`'s loaders), and capture 516 onward is round 1 running on until the 60 s limit (spec's capture runs past the script end): capture 516 against port 358 differs in 5968 px (rows 0..192, x 7..319), the scene and the HUD (names TALON and SAURON, timer 60) are the same, the sprites and the sky have moved. The `nearest port 294` in the report is `nearest()`'s row-hash maximum, an artefact (port 294 is `f=056C mode=001B`, the wipe's last frame before mode 5; its diff box and `14100 px` are against that frame), not the last port frame; the nearest by the criterion of the brief is port 358. Rendered with `$S/pngs.py` (`cap_516.png`, `port_294.png`, and for the check `cap_515.png`, `port_358.png`, all in `$S`): `cap_516.png` shows round 1 with the HUD, `port_294.png` the same arena without the HUD; `port_358.png` and `cap_515.png` are the same picture. Coverage (reported, not ratcheted): 8 non-black port frames not exhibited by any classified capture frame, `[9, 11, 231, 272, 273, 274, 276, 277]` (`f=0x26A` mode 0x1A twice, `f=0x446` mode 0x1B, `f=0x55B..0x55C` modes 0x1A/0x1B: the 70.09 Hz capture against the 60.05 Hz game, the order of the port's frames and that every one appears being not claimed, as for `gp-idle-loss`). Class: harness/host-timed, none a port divergence; the one frame claim result is "the window reaches the script's end".

**The trace claim:** `1194 frames compared (f 13A..), 5 without a capture snapshot`, `0 differing through 1512` over `mode st raw pad e0 e2 rng cred s0_5a s1_5a`. The 5 uncompared frames are the capture's missed snapshots `0x13A 0x1D0 0x268 0x445 0x446` (the claim skips a frame without a snapshot; 1199 port frames less 5). So there is no first trace difference on this walk: `rng` and `cred` (5 to 4 at 0x268/0x269) agree on every compared frame. Reported apart: `first tick difference f=13B` (host-timed, spec §7 Q6); `ent 0 of 1194 differ`; `t508 856 of 1194 differ (first f=27B)`. `t508` is not a `TRACE_FIELDS` member and `normalised()` already applies the §H `+1` rule (`want = capture t508 + 1`), so the `856 of 1194` is the residual AFTER that normalisation, i.e. the frames whose raw port-minus-capture `t508` is not 1. The raw difference (`$S/t7_t508_steps.txt`, from `trace.txt` and `poll.log`, compared frames only) takes the value 1 from f=0x13B and steps at only five frames: `f=0x27B` (mode 0x1B, 1 to 0), `f=0x432` (mode 0x10, the confirm-edge frame, 0 to 1), `f=0x447` (mode 0x1B, 1 to -1), `f=0x55B` (mode 0x1B, -1 to 1) and `f=0x55C` (mode 0x1B, 1 to 2); it is constant between the steps. Four of the five steps (0x27B, 0x447, 0x55B, 0x55C) fall at the entries of or inside the 0x1B load/wipe modes, the class already recorded as record §G gap item 5b and §G.20: port minus capture `t508` jumps at loads, host-timed in the original, the port's loader tick model (`RES_READ_BYTES_PER_TICK`, `res.c`); same class as `gp-idle-loss`'s 7655 of 7973. The fifth, `f=0x432` (mode 0x10, the pick's confirm-edge frame, one frame before mode 0x1A), is NOT in a load mode: it is listed with the others only because it is the next step in the sequence, and its cause is not shown here (it may or may not be the same loader tick model; this record does not claim it is). No ratchet reads `t508`, so none of the five is isolated. No new gap and no new owner: the existing item 5b. (The first version of this paragraph listed per-mode offsets and a "per-mode sampling points, owner U6" gap; that attribution is withdrawn.)

**Divergences:** none of the port. (1) The frame claim's first unexplained frame (capture 516) is the end of the script (harness: how far the port got). (2) The trace claim has none through 1512. (3) The miss log: equal to the pinned set (no unregistered callback beyond §C5.6's). The trace ratchet needs no code change for the "no first difference" case: `ratchet()` handles `first = None` (N = 1513 = `end` is the exact pin, printed as "every item is explained"; N > 1513 fails as unreachable). Nothing was changed in the port, the harness or the pinned set; Task 8 reads `$S/report.txt` for its pins.

## §C5.18 The `gp-charsel-oracle` ratchets (Task 8)

**Pins (`sh $S/pins.sh $S/report.txt data/k11-captures/gp-u5-charsel`, `$S/pins.txt`), read mechanically from Task 7's report lines (§C5.17):** `GP_CHARSEL_MIN_FIRST = 516` (`frames: FIRST UNEXPLAINED capture 516 (raw 3237)`; the capture frame after the port's last one, N = how far the port got); `GP_CHARSEL_TRACE_MIN_FIRST = 1513` (`trace: 0 differing through 1512`, so F = 1512 + 1, the exact pin); `GP_CHARSEL_MAX_START = 100` (`frames: window from capture 100 (raw 1746)`; 100 < 516); `GP_CHARSEL_CAPTURE_SHA256 = 138fb537cd8c364bab0ccc68fc8fdd9079de19f3b0fc789d427f6d9652a18d7e` (`shasum -a 256 data/k11-captures/gp-u5-charsel/poll.log`); `GP_CHARSEL_CAPTURE_FRAMES = 1464` (`frame_*.raw.gz` count). Inserted by `$S/add_target.py`: `1 file changed, 19 insertions(+), 1 deletion(-)` (the provenance comment under the block's header, five lines, was added by hand afterwards; Task 9 corrected its second sentence, see §C5.19). The port script is generated without `--end` (`make gp-replay`'s `GP_SCRIPT_ARGS` is empty), so the script ends at the capture's 1512 and the driver's miss-set check is the exact `==`.

**Green** (`$S/mkv gp-charsel-oracle`, `$S/t8_green.txt`): `capture: poll.log sha256 138fb537..a18d7e, 1464 frames: matches the pin`; `frames: window from capture 100 (raw 1746); 407 classified: 285 clean, 121 splice, 0 transition, 1 unexplained, 10 all-black` (the ratchet run classifies up to N; Task 7's report run printed 411 with 5 unexplained, the five after 515); `frames: first unexplained 516, ratchet N 516 ok`; `trace: 0 differing through 1512; ratchet N 1513 ok`.

**Each pin can fail** (`$S/t8_fail.txt`, `$S/t8_damage.txt`):
- N+1 (`GP_CHARSEL_MIN_FIRST=517`): `frames: FAIL: first unexplained 516 < ratchet N 517`, make exit 1.
- F+1 (`GP_CHARSEL_TRACE_MIN_FIRST=1514`): `trace: FAIL: N 1514 > end 1513: N is unreachable`, make exit 1.
- A wrong hash (`GP_CHARSEL_CAPTURE_SHA256=00ff`): `capture: FAIL: poll.log sha256 138fb537…18d7e (1464 frames) != the pinned 00ff (1464 frames): a re-capture invalidates the pinned N, F and window start…`, make exit 1 (a failure, not a skip).
- Skip (`K11_CAPTURES=/tmp/gameplay-u5/no-captures`): `gp-replay: no capture at /tmp/gameplay-u5/no-captures/gp-u5-charsel`, `gp_compare: no capture at /tmp/gameplay-u5/no-captures/gp-u5-charsel (skipped)`, exit 0. The real capture was not touched.
- Port frames 147..294 damaged in a scratch copy of the dump (`$S/damage.py`, byte 32000 xor 0xFF; the real dump `/tmp/pr_u5_gp/gp-u5-charsel` keeps its 359 frames): `frames: FAIL: first unexplained 272 < ratchet N 516`; the trace `0 differing through 1512; ratchet N 1513 ok` (the frame claim reads the frames; the trace claim is independent).
- A port that stops one frame early (scratch copy with `frame_00358.ipx` removed, 358 frames 0..357): `frames: FAIL: first unexplained 515 < ratchet N 516`; trace ok. So a port that runs one frame short fails the pin.

**Gate** (`$S/mkv verify > $S/t8_verify.txt`, exit 0, which now includes the `== gameplay oracle: gp-u5-charsel (frame and trace ratchets) ==` section with both `ok` lines; `$S/t8_gate.txt`): `ORACLES-EQUAL`, `K11-EQUAL`, `WAV-IDENTICAL`, `python3 tools/port_progress.py` = `771 1203 64` and `731 731 100`; `gp-idle-loss` still `first unexplained 787, ratchet N 787 ok` and `first differing 2088, ratchet N 2088 ok`. The claim is as narrow as `gp-oracle`'s (§G.16): no content-bearing capture frame from 100 to 515 is unexplained and the traced fields agree through f 1512 (0x5E8, the capture's mode-6 frame); nothing is claimed past the script's end, nor about the order or presence of port frames (coverage is reported: 8 non-black port frames, [9, 11, 231, 272, 273, 274, 276, 277], not exhibited by a classified capture frame).

## §C5.19 Closure: the narrow claims, named gaps, coverage and corrections (Task 9)

**The narrow claims, stated.** `make gp-charsel-oracle` (in `make verify`) proves two things about `data/k11-captures/gp-u5-charsel` (the `poll.log` sha256 and the 1464 frames pinned): (1) no content-bearing capture frame from the window start (capture frame 100, `GP_CHARSEL_MAX_START`) up to N = 516 is unexplained by the port's replay (`GP_CHARSEL_MIN_FIRST = 516`); (2) the traced fields `mode st raw pad e0 e2 rng cred s0_5a s1_5a` agree on every compared frame below F = 1513 (`GP_CHARSEL_TRACE_MIN_FIRST`, through f = 1512 = 0x5E8). It does **not** trace the cursor or class bytes (`0x108166`, `0x10816A` are not `SNAP_FIELDS`; record §C5.5), does not claim the port renders everything (a port that under-renders passes; the order of the port's frames and that every one appears are not claimed, 8 non-black port frames are reported as not exhibited), and says nothing past the script's end (mode 6's first frame, f = 0x5E8; capture 516 onward is round 1 running on, how far the port got, not a defect). `gp-oracle` (the `gp-idle-loss` run) now claims the whole `gp-idle-loss` character select: after the `0x37B03` fix its frame claim runs from capture 90 through the character select (`f = 0x293..0x640`), the time-out wipes, `0x11`, `0x17` and the round start into round 1 up to capture 787 (§C5.3 (c)); the trace ratchet is unchanged at 2088 (the `rng` difference at `f = 0x828`).

**Named gaps and coverage** (each is a gap, not a finding; record §C5.5 and Tasks 7-8):
- The second player's slot: LEFT PLAYER ARCADE shows only P2's prompt (`0x43B24` polls `0x11F28(1)`, draws `0x432A0`); P2 joining and walking is U7's.
- The team pass `DS_00104B1D == 3` (`0x44798`): U8's.
- The button variant `DS_00105B34 != 0` (b1/b2/b3 held at the confirm): not exercised.
- The `DS_00108173 != 0` path: no writer found by the address scan (the scan finds only absolute-address reads; an indexed write elsewhere is not excluded).
- The audit count `0x2E934` (CMOS, port-deferred): not in frames or trace.
- The character names: inferred from the audit string order, not claimed (the frames show TALON and SAURON on the HUD; the seven cells' names are not compared).
- Voices: not compared (audio is outside the frame and trace claims).
- `0x3640C` (`anim_indirect`, an unported animation-opcode target, U0's list, track P): still unported; the `gp-idle-loss` replay no longer reaches it after the fix (§C5.12), and `gp-u5-charsel` never did.
- The ENDURANCE fight (START MENU row 5, `0x2CC3C`, mode `0x2B` / sub 3; gameplay-ground-truth spec §3.3): not walked; handed to U8 (other modes), a named gap here.
- Hidden or boss characters: a negative finding, not a gap (§C5.5: the cursor byte's writers keep it in `0..6` and P1's class comes only from `0xC8882[c]`, 7 entries; the caveat that an indexed write could set `DS_00108173` stays the named gap above).
- The gp oracles never check port frames the capture does not exhibit (owner: a later gp-oracle unit): for `gp-u5-charsel` 8 non-black port frames, `[9, 11, 231, 272, 273, 274, 276, 277]`, all 8 are wipe/load screens by `frames.txt` (port 9, 11: `f=0x26A` mode `0x1A`; 231: `f=0x446` mode `0x1B`; 272-274: `f=0x55B` mode `0x1A`; 276, 277: `f=0x55C` mode `0x1B`; the review's "6 of them" is corrected to the measured 8 of 8); the whole-branch review garbled port frame 231 in a scratch dump and the oracle still passed.
- The capture-identity pin covers `poll.log` (sha256) and the frame count only, not the contents of `frame_*.raw.gz` (owner: a later gp-oracle unit).
- Divergences Task 7 left open, each with its owner: (a) the frame claim's first unexplained frame (capture 516) is the end of the script: harness, how far the port got; round 1 after it is U6's (the `gp-idle-loss` round-1 divergences, record §G.24: the unregistered `0x23208` at `f = 0x824`, the `0x3A588` state-10 callback with `hits=4995` from `f = 0x8E7`, §C5.13, and the `rng` difference at `f = 0x828`); (b) `t508` (not a trace field): 856 of 1194 differ after the §H `+1` normalisation, steps at `f = 0x27B, 0x432, 0x447, 0x55B, 0x55C`; four sit in load/wipe modes and belong to the existing record §G gap item 5b / §G.20 (the loader tick model, `RES_READ_BYTES_PER_TICK`), the fifth (`0x432`, mode 0x10) is not shown to be (§C5.17); no owner beyond that existing item; (c) `tick` differs from `f = 0x13B` (host-timed, spec §7 Q6); (d) the BIOS key is consumed 1 to 3 frames after its press (spec §7 Q3, record §G gap item 6): the replay uses the observed frames, so it does not depend on the cause; (e) 5 capture frames without a snapshot (`0x13A 0x1D0 0x268 0x445 0x446`, spec §3.7), not compared by the trace claim, their modes checked from the `P` records; (f) the 8 reported-only non-black port frames `[9, 11, 231, 272, 273, 274, 276, 277]` (the 70.09 Hz capture against the 60.05 Hz game).

**Corrections of the plan by the raw or the capture (the raw wins), numbered:**
1. The brief's "countdown modes 0x15-0x17" is wrong: before the round the one-player path runs `0x11`, `0x17` (the raw pick path, `0x10` to `0x1A`, `0x1B`, `0x11` at `f = 0x457`, `0x17` at `0x458`; capture `poll.log:1151` and `:1153`; record §C5.6, §G.18); `0x15` follows the match, not the pick.
2. The script ends at the capture's mode-6 frame `f = 0x5E8` (1512; `poll.log:1557`), not `0x5EE` (1518), which was the stand-in walk's (§C5.15, §C5.17).
3. The stand-in walk's 6-frame offset (`0x10` at `0x293` against the capture's `0x28D`) came from `gp-idle-loss`'s Enter frames; with the capture's own pad frames the port's mode changes equal the capture's frame for frame (§C5.16 deviation 2, §C5.17).
4. The brief's "the `e0` edge one frame after the `I` record" was wrong: the edge is two frames after (`poll.log:723` to `:726`, f = 0x2C8 to 0x2CA; the `I` record is stamped F-1, the bits change at F, the edge at F+1); and the key frames are `I+1..I+3`, five of the ten pad keys differ from `I+1` (§C5.16 table, from `poll.log` `I`/`H`/`S` records), not "the third right is the one exception".
5. The credits edge `5 to 4` is cited at `poll.log:620` (`cred=5`, f = 0x267) and `:624` (`cred=4`, f = 0x269), not `:310` (§C5.16).
6. The capture is 68 MB (`du -sh`), not §C5.7's 55 MB estimate (under the 170 MB cap).
7. Task 7's claim that "the trace pin needs end semantics" was wrong: `ratchet()` already handles `first = None`, N = 1513 is the exact pin and N > 1513 fails as unreachable (§C5.17, §C5.18).
8. The first version of §C5.17's `t508` attribution ("per-mode sampling points, owner U6") was withdrawn: the steps are the existing item 5b (and the `0x432` step is not shown to be a load step).
9. `nearest port 294` in the report is `nearest()`'s row-hash artefact; the nearest by the brief's criterion is port 358 (0 differing pixels against capture 515), §C5.17.
10. `GP_CHARSEL_MIN_FIRST`'s classification line reads 407 classified / 1 unexplained in the enforced run against Task 7's 411 / 5 (the enforced `frame_claim` breaks at the first unexplained frame, the report run continues to `REPORT_MAX = 5`): not a conflict (§C5.18).
11. Task 9 wording fixes: §C5.13's `make exit 2` is `make: *** [gp-oracle] Error 1` (`/tmp/gameplay-u5/t3_n788.txt:44`); the Makefile provenance comment's "The port's script ends where the capture's poll.log ends" is wrong (the script ends at the mode-6 frame f = 0x5E8, `poll.log` runs to about f = 0x925) and is corrected in the Makefile; §C5.18's "four lines" is five; §C5.12's "left as U4 wrote it" was superseded by Task 3's comment correction; §C5.10 names the baseline log and the full scratch paths; §C5.17's `0x432` attribution and "2 frames" sentence are reworded.
12. The `0x8E7` first-hit frame of `0x3A588` (the `k_miss_gp_idle_loss` comment in `test_platform.c`, §C5.13) had no captured output at Task 3. Task 9 re-measured it with a temporary, uncommitted `fprintf` in `fn_resolve_from` (`/tmp/gameplay-u5/t9_instr.diff`, `t9_instr.txt`, printed lines pasted in §C5.13): `miss #1 at f=0x8E7 (2279)`, `hits=4995`. The committed comment's figures stand; no `port/` change.

**Scratch.** The throw-away instrumented clone `/tmp/gameplay-u5/t1` (§C5.11, never committed, outside the repo) was removed in Task 9; the outputs it produced are pasted in §C5.11. The other `/tmp/gameplay-u5` artefacts are not in the repo and are not needed to read this record.

**Final gate** (`$S/mkv verify > $S/t9_verify.txt`, `verify-exit=0`, `all checks passed`; the check output is `$S/t9_gate.txt`, started after the Makefile comment fix of Task 9: the Makefile's last write 14:15:05, the verify log created 14:16:13 and last written 14:27:59, the commit at 14:28:46, from file timestamps and the commit time):

The two `attract: twi5.smk playback failed` / `attract: twg.smk playback failed` lines `make verify` prints are pre-existing, not from U5: they are in the Task 0 baseline log too (`/tmp/gameplay-u5/base_verify.txt`, lines 75 and 106 for `twi5.smk`).

```
oracle lines: 45
ORACLES-EQUAL
K11-EQUAL
WAV-IDENTICAL
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
fn-miss PR_GP_DUMP distinct=2 dropped=0        (gp-pads)
fn-miss PR_GP_DUMP distinct=6 dropped=0        (gp-idle-loss)
gp_compare: gp-idle-loss: frames: first unexplained 787, ratchet N 787 ok
gp_compare: gp-idle-loss: trace: first differing 2088, ratchet N 2088 ok
fn-miss PR_GP_DUMP distinct=4 dropped=0        (gp-u5-charsel)
gp_compare: gp-u5-charsel: frames: first unexplained 516, ratchet N 516 ok
gp_compare: gp-u5-charsel: trace: 0 differing through 1512; ratchet N 1513 ok
```

**Fix wave (after the Task 9 and whole-branch reviews; docs and comments only).** Item 13: the pixel measurement behind `GP_IDLE_LOSS_MIN_FIRST = 787`, on the `make verify` dump `/tmp/pr_u5_gp/gp-idle-loss` (6502 port frames) and `data/k11-captures/gp-idle-loss` (8173 frames) with `gp_compare`'s own loaders (`load_capture_frame`, `load_port_frame`, `diff_box`, `nearest`, `tc.explain`; scratch script `c787.py`, not committed), printed output:

```
capture 786 px per port frame {573: 18188, 574: 15995, 575: 1878, 576: 15031, 577: 17420}   (fewest: port 575 f=0823, 1878 px)
  explain over port 570..579: splice {574, 575}
capture 787 px per port frame {574: 18531, 575: 11357, 576: 7470, 577: 16425}   (fewest: port 576 f=0824, 7470 px)
  nearest() row-hash heuristic: port 575 px 11357
```

(Ports 568..581 were printed; the others are 15000..40000 px.) `frames.txt`: `00575 f=0823`, `00576 f=0824` (mode 6). Capture 786 is a splice of port 574/575 and capture 787 is nearest in pixels to port 576, the frame of the `0x23208` miss, so the claim that the first unexplained frame sits at the divergence-2 frame holds; the earlier "one frame before" came from the `nearest()` heuristic and is withdrawn. The Makefile comment and §C5.3 (c) now cite this.
