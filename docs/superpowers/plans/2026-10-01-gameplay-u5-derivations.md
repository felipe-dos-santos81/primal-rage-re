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
(raw 3899), whose nearest port frame 575 is `f = 0x823` mode 6 (`$S/vo2/gp/gp-idle-loss/frames.txt`
line 576: `00575 f=0823 tick=00000881 mode=0006`): one frame before the unregistered `0x23208` at
`f = 0x824` (record §G.24, divergence 2, U6's). The trace ratchet is unchanged (2088). Gate
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

The gate check: the 45 oracle lines equal `oracle-lines-base.txt` (`ORACLES-EQUAL`); `make audio-render` is byte-identical to `before-t2.wav` (`WAV-IDENTICAL`); `python3 tools/port_progress.py` prints `771 1203 64` and `731 731 100`. The 12 `k11_compare:` lines (walk 6, menuesc 6) are saved as the baseline (`$S/k11_base.txt`; the `K11-EQUAL` diff is trivially equal here): walk `0 unexplained in the window`, menuesc `0 unexplained in the window`, menuesc open end `END 388 must be >= 388: ok`. The base is the one the plan was measured on.

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
