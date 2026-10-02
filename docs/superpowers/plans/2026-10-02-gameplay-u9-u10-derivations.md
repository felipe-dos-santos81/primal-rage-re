# Gameplay U9 (the win path) and U10 (the endings) under pokes: derivation record

Plan: `docs/superpowers/plans/2026-10-02-gameplay-u9-u10-win-and-endings.md`. Spec:
`docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 G ("then U9 win path under
pokes, U10 endings"), §6 ("U9, U10 ratcheted under pokes"), §7 ("Wins and endings without
pokes" stays a named gap); `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md`
§3-§5, §7. §W.0-§W.9 were written while planning (main `b5beff9`); the plan's tasks append
§W.10 on. All addresses are linear (Ghidra) addresses. Raw listings are capstone over the
fixed-up image `./build/diffrun --exe data/game/C/PRAGE.EXE --image-out IMG` (code from
`0x10000`), not the pre-fixup file bytes (AGENTS.md).

## §W.0 What the planner ran (reproducible)

- **The image:** `./build/diffrun --exe data/game/C/PRAGE.EXE --image-out $S/img.bin`
  (1 028 304 bytes). `rawdis.py` (capstone, `CS_MODE_32`, from `IMG[a - 0x10000:]`) printed
  every listing quoted below.
- **A scratch tree of main `b5beff9`** (`git archive b5beff9 | tar -x`, `data` symlinked,
  the two git-ignored fixtures copied), with the plan's tool and driver edits applied: the four
  gp suites plus `test_gp_win` `Ran 124 … OK`; `./build/run_tests` `all checks passed`; the
  gp-twop oracle with its pinned values (`N 612`, `F 1506`, moves `1506`, window 83, sha
  `9c01a73b…`) `ok` on the edited driver (the edits change no existing replay).
- **A planner harness** (`sim.c`, not a repo file): `game_init()`, the title pin's opcode-8
  mirror, the gp-idle-loss port script's three Enters (`key 321/474/622`), pad holds and
  pokes keyed on the k-th entry into a mode, one line per mode transition. On the idle path it
  reproduces the capture's mode sequence frame for frame where the capture has an `S` record
  (mode 6 `0x7F5`, mode 8 `0xC71`/capture S `0xC74`, mode 7 `0x1B4B`/`0x1B4E`, mode 9 `0x1BBC`,
  mode 3 `0x207A`/`0x207F`): the port matches gp-idle-loss's whole trace (`GP_IDLE_LOSS_TRACE_MIN_FIRST
  = 8320`, Makefile), so the port is the best available model of the original's timing.
- **The scratch `PR_GP_DUMP` driver** (the plan's Task 4 edits) on two hand-written port
  scripts with the pokes at the frames the harness found (§W.7): both replays end at their
  `end`, every driver check passes, and only the miss-set check fails (no `k_gp_sets` row yet).

## §W.1 The config the path depends on (run)

`0x2D974(0x29)` with the zero CMOS (the defaults every K11/gp capture uses,
`k11_capture.check_cmos`): **`0x00142095`** (planner harness, `DS_00104528` printed at the
character select; the eeprom-config report's default, `2026-09-19-eeprom-config-report.md:68`).
Decoded by `0x25071` (mode `0x2D`, `game_mode_2d_step`):

| global | expression | value |
|---|---|---|
| `DS_0010452C` (difficulty row) | `(v & 0xF0) >> 4` | **9** |
| `DS_00105B3A` | `(v & 0x100) >> 4` | **0** |
| `DS_00104ADC` (rounds) | `((v & 0x300000) >> 20) * 2 + 1` (`0x25A14..0x25A28`) | **3** (best of 3) |
| `DS_001088D0` | `(v & 0xF) * 5 + 0x1E` | 55 |
| credits | `((v & 0xF0000) >> 16) + 1` | 5 (4 after mode `0x2D`) |
| `DS_00108113` | set only by the service menu (`0x1A53B`) | **0** |
| `DS_00104529` bit 1 (the sprite layout) | byte 1 of `v` = `0x20` | **clear**: the text layouts |

## §W.2 The WIN fields (`gp_session.WIN_EXTRA`)

The `S` fields both scenarios add (and the port's `T` line appends), each with the code that
writes it:

| name | address, size | what writes it |
|---|---|---|
| `afc` | `DS_00104AFC`, 2 | the stage word: `0x25848` (stage pick), `0x26F58`/`0x271E0` (= 7, the final) |
| `ad4` | `DS_00104AD4`, 4 | the match result. Every store (`mov dword ptr [0x104ad4], x`, found by scanning the fixed-up image for the dword `0x104AD4` and decoding each hit, capstone base `0x10000`): `0x27C3D` (`0x27BA4`: 0, 1, 2 or -1), `0x25A0E` (`0x259CC`: -1) and the mode-9/7 paths `0x283AE`, `0x283EE`, `0x28409` (each right after `or byte ptr [0x104aec], 2`), `0x288F3` (2), `0x28961` (0, after `cmp eax, 1`) and `0x28973` (1, after the same test on `DS_001078A7`) |
| `w2`, `w3` | `DS_00104AF2`, `DS_00104AF3`, 1 each | the round wins: `0x27C48` |
| `b1e` | `DS_00104B1E`, 1 | the round index (`0x259CC` = 0; 1, 2 at each round start) |
| `b21` | `DS_00104B21`, 1 | the final's KO count: `0x274FC` (`0x2758A`) |
| `b14` | `DS_00104B14`, 1 | the final flag: `0x26F58` (`0x27037`), `0x271E0` |
| `b0c` | `DS_00104B0C`, 1 | death-done: set only at `0x37FF6` (§W.5) |
| `t104` | `DS_00108104`, 2 | the lands held per side: `0x41C28` states 1/3, `0x4142C` (= 0) |
| `m106`, `m10a` | `DS_00108106`, `DS_0010810A`, 4 each | the seven land marks `0x108106..0x10810C` (+ `w10d`) |
| `sc0` | `DS_001077EC`, 4 | slot 0's `+0x3C`, the score (`0x41310`) |
| `c82` | `DS_00107832`, 1 | slot 0's `+0x82`, the world-domination count (`0x4160C`, `0x416C2`) |

Every one has a `symbols.h` name (`grep -c "define DS_<addr> "` = 1 each).

## §W.3 Winning a round and a match (raw)

```
27FAE mov al, byte ptr [0x10780a]     ; A = slot 0 +0x5A (damage taken)
27FB3 cmp eax, 0x78
27FB6 jge 0x27fc8
27FBA mov al, byte ptr [0x10789e]     ; B = slot 1 +0x5A
27FBF cmp eax, 0x78
27FC2 jl 0x28049
```

`0x27FA8` (`flow_round_end_check`, run by the fight frame `0x26254` every mode-6 frame):
either `+0x5A` byte at `0x78` → `0x27C48` (the round winner on the two bytes: the lower one
wins, `DS_00104AF2`/`AF3` += 1) → `0x27BA4` (the match result): with `B1E == ADC` a draw (2)
or the higher count; otherwise with `1 < B1E <= ADC`, `d = ADC − B1E`: 0 when `AF2 > AF3 +
d`, 1 when `AF3 > AF2 + d`, else -1. `0x27FA8` then stores mode 9 for a result in 0..2,
else mode 8. With ADC = 3 (§W.1): round 1 (`B1E = 1`) always gives -1 → mode 8; round 2
(`B1E = 2`, d = 1) with P1's two wins gives 0 → mode 9. U4's idle loss shows the same with the
sides swapped (round 1 mode 8 at `0xC71`, round 2 timeout → mode 7 → 9).

**The KO poke:** `DS_0010789E = 0x78` (P2's `+0x5A`) written in the spin before an iteration
of mode 6 makes that iteration's `0x27FA8` end the round. The harness found the round ended
in the same iteration (poke before `f = 0x490`, mode 8 at `0x490`). What it replaces: the hits
that would have raised `+0x5A` to `0x78`, and the KO reaction they would start (no
`0x39FF4` on that path). The claim never covers those (§W.9; corrected from §W.10, Task 7 review).

## §W.4 The tour (raw)

Mode 9's dispatch (`0x28788`, `game_mode_09_step`), once its `DS_00104AF8 = 0x258` hold runs out,
calls `0x286BC`:

```
28730 xor dh, dh                       ; the winner's think byte (+0x63) is not 1: a human won
2873E shl dl, 6
28741 mov al, byte ptr [eax + 0x10782a]
28747 or dl, 0x80
2874A or al, dl
2874E mov dx, word ptr [0x104afc]
28755 mov byte ptr [edx + 0x108106], al   ; the land mark: 0x80 | winner << 6 | the winner's character
```

then, for a human winner:

```
28B2C mov ax, word ptr [0x104afc]
28B32 cmp eax, 7
28B35 jne 0x28bae                      ; stage 7 (the final's) clears the marks; any other:
28BB5 mov byte ptr [0x104b17], ch      ;   DS_00104B17 = 1
28BBB mov ecx, 0x4142c
28BC0 mov word ptr [0x104b00], ax      ;   mode 0x17
28BC6 mov dword ptr [0x104ae4], ecx    ;   hook 0x4142C
```

`0x4142C` (`fight_hook_4142c`): `DS_00108104/5 = 0`, mode `0x12`. Mode `0x12` (`0x41C28`,
`game_mode_12_step`) is the conquered-lands screen ("CONQUERED LANDS", string `0x4F`):
state 1 flashes each marked land other than the current one and counts it for the side whose
`(char | side << 6)` matches (`0x41D62`); state 3 counts the current land (`0x42060`); state 5:

```
42117 mov eax, dword ptr [0x104ad4]
4211C mov al, byte ptr [eax + 0x108104]
42127 cmp eax, 7
4212A jne 0x4213b                      ; fewer than 7: 0x41760, the next opponent
4212C call 0x4160c                     ; all seven: WORLD DOMINATION
```

`0x41760` (fewer than 7): `0x25848` picks the next stage (`DS_00104B17 = 1`: a random
unmarked land, `rng(n)` at `0x258FD`), `0x41350` makes the other side the CPU with the
stage's character (`0xC835A[stage]`), hook `0x259CC` → the wipe → mode 5 → mode 6. That is
U9's path.

`0x4160C` (all seven): "WORLD DOMINATION" (string `0xDC`), the marks cleared
(`0x41676`), slot `+0x82` += 1, state 6/7 animate, 100 000 points (`0x422C1`), then:

```
42363 mov al, byte ptr [0x10452c]
42368 cmp eax, 9
4236B jge 0x423da                      ; difficulty below 9 and
4236D cmp byte ptr [0x108113], 0
42374 jne 0x423da                      ; no service-menu flag: the tour starts again
423DA cmp byte ptr [0x105b3a], 0
423E1 je 0x42434                       ; DS_00105B3A == 0:
42434 call 0x417c4                     ;   "YOU MUST / REPLENISH YOUR HEALTH / BEFORE THE FINAL BATTLE"
```

With the defaults (§W.1: difficulty 9, `DS_00105B3A = 0`) the path is `0x417C4` (strings
`0x1E`/`0x1F`/`0x20`), hook `0x26978` → the wipe → mode `0x23` (`0x26A50`: for character 0
"EAT OF YOUR TRIBE / THAT YOU MIGHT REMAIN / IMMORTAL", strings `0x21..0x23` from `0xA8908`)
→ mode `0x22` (`0x26C8C`, the health bonus) → mode `0x24` (`0x26F58`):

```
2702D mov byte ptr [0x104b21], dl      ; the KO count 0
27037 mov byte ptr [0x104b14], dh      ; the final flag 1
2703D mov byte ptr [0x104b0c], dl      ; death-done 0
27057 mov word ptr [0x104afc], cx      ; stage 7
```

→ hook `0x27134` → the wipe → mode 5, whose state 3 sends the final to mode `0xC`:

```
25F02 cmp byte ptr [0x104b14], 0
25F09 je 0x25f6b                       ; 0: mode 6 (a match)
25F18 mov esi, 0xc                     ; else mode 0xC (stored at 0x25F20) and the opponent's entrance
```

**The marks poke:** the seven bytes `0x108106..0x10810C = 0x80 | 0 << 6 | c0`
(`gp_session.lands_marked(0, c0)`), written with round 1's KO, make the first won match the
seventh land: state 1 counts six, state 3 the seventh. The current land's byte is rewritten by
`0x286BC` with the same value. What it replaces: six won matches (§W.9; corrected from §W.10, Task 7 review).

## §W.5 The final and the death-done poke (raw, and a run)

Mode `0xC`'s arena frame `0x27380` ends with `0x272DC`:

```
272DD mov edx, dword ptr [0x10810a]
272E3 sar edx, 0x18                    ; w = (s8)DS_0010810D, the human side
272F4 mov al, byte ptr [eax*4 + 0x10780a]
27300 cmp eax, 0x78
27303 jl 0x2731b                       ; the human KO'd: 0x278B0, the continue screen (mode 0xE)
2731D mov dl, byte ptr [0x104b12]      ; else the DS_00104B12 side (the CPU) at 0x78:
2733D cmp eax, 0x78
27340 jl 0x2737e                       ;   its +0x41 bit 4, mode 0xD
```

Mode `0xD` (`0x274FC`) does nothing more until `DS_00104B0C` is set; then it clears it and
counts the KO:

```
27562 mov dl, byte ptr [0x104b0c]
27582 mov byte ptr [0x104b0c], dh
2758A mov byte ptr [0x104b21], bl      ; DS_00104B21 += 1
27590 cmp eax, 7                       ; 7: "YOU ARE MASTER / OF THE NEW URTH" (0x3F/0x40), 5 000 000
                                       ;    or 1 000 000 points, mode 0xF; else the next opponent, mode 0xC
```

A linear capstone sweep of the code object for `[0x104b0c]` finds the readers `0x27562`
(mode `0xD`) and `0x2971E` (mode `0x32`) and the writers `0x2703D`, `0x27200`, `0x27582`,
`0x29744` (all store 0) and **`0x37FF6 mov byte ptr [0x104b0c], ch` — the only non-zero
store**, in `0x37EA0` (`fighter_37ea0`): the end of a KO'd fighter's death animation (an
animation-opcode target), which also spawns the remains actor and draws `rng(0x10)`.
A poked KO starts no death animation: the planner harness with only the KO pokes **stayed in
mode `0xD` from `f = 0x1830` to its limit `0x4800`** (§W.7). So U10 pokes
`DS_00104B0C = 1` (`gp_session.DEATH_DONE`) 10 frames into each mode-`0xD` entry; what it
replaces (the death animation, its remains actor, its `rng(0x10)` draw) is not claimed, and
`gp_win.py check` fails the capture if the game had set the byte itself before the poke.
**Corrected by the capture (§W.13, raw wins):** the original confirms the mode-`0xD` half (with no
death-done poke it stayed in mode `0xD` from `f = 0x14C0` to `0x413A`, 190 s, §W.13 run 2), but "a
poked KO starts no death animation" is too strong: in `gp-u10-ending` the game itself sets
`DS_00104B0C = 1` at `f = 0x1602`, in mode `0xF`, 190 frames after the 7th poked KO was counted
(`0x1544`). `0x37FF6` is the only non-zero store among the seven direct references to
`0x104B0C` (the capstone sweep above and Ghidra's xrefs agree: reads `0x27562`, `0x2971E`;
writes `0x2703D`, `0x27200`, `0x27582`, `0x29744`, `0x37FF6`), so `0x37EA0` ran there. Why the death
animation ends in mode `0xF` and not in mode `0xD` is not established (a named gap, §W.13).

## §W.6 The ending (raw)

Mode `0xF` (`0x277C0`) waits `0x258` frames, then mode `0x17` with return mode `0x1F` and
the darken hook `0x29B74` → mode `0x15` → mode `0x1F` (`0x208F8`, `game_mode_1f_step`), whose
content is indexed by the winner's character `DS_0010782A[DS_00104AD4 * 0x94]` (`0x20900..
0x20914`): the actors `0xA8090[c]` (text layout; `0xA80AC[c]` in the sprite layout) and
`0xA80C8[c]`, the stream `0xA7EBC[c]`, and in state 4 the title string `0xA80E4[c]`:

| c | character (`0xC866C`) | ending title (`0xA80E4`) |
|---|---|---|
| 0 | SAURON | THE FEAST OF SAURON (`0xDE`) |
| 1 | BLIZZARD | BLIZZARD'S SOLITUDE (`0xDF`) |
| 2 | TALON | TALON'S DREAM (`0xE0`) |
| 3 | VERTIGO | THE PALACE OF VERTIGO (`0xE1`) |
| 4 | ARMADON | THE MEDITATION OF ARMADON (`0xE2`) |
| 5 | DIABLO | DIABLO'S HELL (`0xE3`) |
| 6 | CHAOS | CHAOS' REDEMPTION (`0xE4`) |

(strings decoded from `ENGLISH.TXT` with `0x474E4`'s scheme: groups of 0x40, each entry
length-prefixed and XORed with its length.) Then mode `0x15` (`0x4B0` frames), `0x1F` state
3/4, mode `0x15` (`0x384`), mode `0x1E` (`0x1EEB0`: with a qualifying score the high-score
name entry) and back to mode 3. The code path is the same for every character; only the
table entries differ.

## §W.7 The port preview: timelines, sizes, misses (runs)

Planner harness and scratch driver, character 0 confirmed at once (`p1.start` held 6 from 60
frames into mode `0x10`; the harness applied the bit only, the capture also queues F1's word,
as gp-u5-charsel's confirm did). Frames are the first frame in the mode (hex):

**U9 (`gp-u9-win`):** `0x27` 141, `0x10` 293, confirm bits 2CF..2D4, mode 6 (stage 5, P2 =
CHAOS) 486, KO poke → mode 8 490, `0x16` 6E8, 5 7D9, mode 6 (round 2) 854, KO poke → mode 9
85E, `0x17` AB6, `0x12` BA7 (lands 1 at C14), `0x17` C60, `0x1A` CD9, 5 CFD, **mode 6 (match 2,
stage 3, P2 = VERTIGO) D78**; the scenario ends 60 frames later, **f = 0xDB4 (3508)**. Port
dump 2 029 frames, 129 MB; replay 58.8 s wall.

**U10 (`gp-u10-ending`):** as U9 to mode 9 85E, `0x12` BA7 (lands 7 at C4A, WORLD DOMINATION
C96), `0x23` DC8, `0x22` E43, `0x24` 12EA, `0x17` 139E (stage 7), 5 143B, **mode `0xC` 14B6**,
then `0xD`/`0xC` every 10 frames (14C0 … 1538; opponents VERTIGO … 5, 4, 1, 3, 6, 2, 0 by
`0x2716C`'s draws), **mode `0xF` 1542** (score 5 180 000), `0x17` 179A, `0x15` 179B, **mode
`0x1F` 1814**, `0x15` 1905, `0x1F` 1DB6, `0x15` 1DF7, **mode `0x1E` 217C** (score 5 380 000),
**mode 3 26DF (9951)**. `gp_win.py check` on the replay's own trace (`T` read as `S`): 30/30
milestones. Port dump 4 750 frames, 302 MB.
**Corrected by the capture (§W.13):** the preview models the original's timing but not its
memory. The port has no DOS memory limit, and under DOSBox-X's default `memsize` (16 MB) the
original stops in the final ("Primal Rage is out of memory.", at the 4th opponent). So the
capture runs with `memsize=64`, a harness value of `gp-u10-ending` only.

**Wall time and storage (estimates, harness values):** mode `0x27` appears ~25 s after
DOSBox-X starts (`ENTER_WAIT`), and the game runs at 60.05 frames/s, so U9's end is at
~25 + (3508 − 321) / 60.05 = **78 s**, U10's at ~25 + (9951 − 321) / 60.05 = **185 s**
(+1 s `STOP_TAIL` each). The time limits 110 s and 240 s are 1.4x and 1.3x those. Stored
frames run from the post-logo start (raw ~1 371-1 388, ~19.6 s) to the end at 70.09 Hz
times the measured distinct ratio of the existing captures (gp-twop 0.36 … gp-idle-loss
0.65) at the measured 44-49 KB per frame (gp-twop 30 MB/680, gp-idle-loss 376 MB/8173):
**U9 ≈ 4 160 AVI frames → 1 500-2 700 stored → ~70-130 MB; U10 ≈ 11 660 AVI frames →
4 200-7 600 stored → ~200-360 MB.**

**The predicted `fn_resolve` misses** (the scratch driver's `fn-miss` lines; first frame from
the harness): both scenarios `0x29D60` and `0x5D812` from `frontend_mode_1b_step` (§G.24's
harmless pair, f 293/40B), and the E2 animation targets (`2026-10-01-reverse-e2-triage.md`
rows, not yet ported on `b5beff9`): **`0x400E0` anim_indirect at f = 5A2 (mode 8, P2 =
CHAOS), `0x21044` at 866 and `0x21084` at 87A (mode 9)**; U10 also **`0x3DA50` at 14F7 (mode
`0xC`, VERTIGO)**. `0x400E0` clears bit 2 of a record's `+0x42`; `0x21044` sets `+0x34/+0x36/
+0x44/+0x43` and `+0x57 = 1`; `0x21084` zeroes `+0x1C/+0x34`, the slot's `+0x54` and sets
`+0x57 = 2`: state the original writes and the port skips, so **the trace may first differ at
or after f = 0x5A2**. A P batch that ports them first removes the rows (Task 9/12 measure, never
predict).
**Corrected by the U10 replay (§W.14, raw wins):** the capture's replay reaches none of U9's three
targets nor `0x3DA50`; its set is the two wipe hooks plus `0x2381C` (`hit_reaction_apply`, f =
`0x848`), `0x37DD4` and `0x29C78` (`anim_indirect`, f = `0x1518`/`0x151B`).

## §W.8 The harness extension (designed and run in the scratch tree)

- `gp_session`: the step `('after_entry', mode, k, n, action)`, F = the first `S` frame of
  the k-th entry into `mode` (an `S` record whose mode differs from the previous `S`
  record's; `Schedule.on_snap`, counted once the boot step fired, like `mode_first`) + n. The
  action `('poke', ((addr, bytes), …))`. The record `W ms f step addr len was now late race`
  (`format_w`). `port_script` emits `poke <W.f + 1> <addr> <now>`: the write landed in the
  spin of `W.f`, so iteration `W.f + 1` reads it, as a `bits` line; a raced or unpinned
  `W` record is a `ScriptError`.
- `gp_capture`: `apply_poke` writes at `base + addr − 0x80000` (as `read_snap` reads), then
  re-reads `f`/`DS_00101508`: a moved value means the iteration may have started before the
  bytes landed (`race=1`). The CHECK `pokes written N/N, 0 raced`.
- The timer ISR body `0x1BDF4` writes only `DS_00101508`, `DS_00101500`, the key block
  (`0x1BBAC`), `DS_000EF6DE` and, through `0x2D62C`, `DS_00105D88` (capstone, both bodies):
  none of the poked addresses, so a byte written in the spin is still there when the
  iteration runs.
- The port driver: `poke <f> <addr hex8> <hex>` (1..16 bytes, inside `0x80000..0x10B0CF`),
  written by `gp_apply` before the iteration raising the counter to f; `CHECK_EQ_INT(pokes
  applied, pokes)`. The `T` line appends the WIN fields.
- Tests run (scratch): `TestWinPokes` (3), `TestPokeScript` (4), `TestPoke` (3), `test_gp_win`
  (9), `test_gp_poke_script` (C). Mutations run, each failing its test: the poke frame
  `W.f + 1 → W.f` (2 failures); `on_snap` without `prev == mode` (1); the driver's `memcpy`
  length − 1 (`17 != 120`, `34 != 135`); the driver's range check removed (`1 != 0`); a
  milestone's `w2=1 → 2` (4 failures); `reproduced` without `c != p` (1).

## §W.9 The claims and what stays a named gap

`gp_win.py check` (evidence, on the capture) and `path` (the port, a ratchet on the leading
milestones reproduced at the capture's frame over the frames the capture snapshotted), plus
the WIN-fields trace ratchet and gp_compare's frame and trace ratchets. Narrow as every gp
oracle: a green line says the named fields agree up to the pinned frame, not that the frames
after it are right, and not that what the pokes replaced (§W.3-§W.5) is reproduced.

Named gaps that stay (spec §7, with this record's evidence): the KO by fighting (the hits and
the KO reaction, §W.3), the six earlier won matches the marks poke stands for (§W.4), the death
animation and its `rng(0x10)` draw (§W.5), the endings of the characters not captured (§W.6:
the same code, other table entries), the tour's repeat at difficulty < 9 and the
`DS_00105B3A != 0` branch into the final (`0x423E3`/`0x271E0`, §W.4), the continue in the
final (mode `0xE` from `0x272DC`), the sprite layouts (`DS_00104529` bit 1 clear in the
defaults, §W.1).
Added by the capture (§W.13): the original's memory growth over the poked final (it runs out
of memory in the 4th final fight under DOSBox-X's default 16 MB, `gp-u10-ending` runs with
`memsize=64`): not root-caused, and whether the real game, with fought and unpoked KOs, runs out
the same way is unknown. Also the death animation that ends in mode `0xF` but not in mode `0xD`
(§W.5's correction).

## §W.10 The Makefile targets and the port preview (Task 7, runs on `a76af68`)

`make gp-win-oracle gp-ending-oracle GP_DUMP=/tmp/pr_u910_gp` (no capture): each prints `Ran 9
tests` / `OK` (`tools.tests.test_gp_win`) and then `gp-win-one: no capture at
data/k11-captures/gp-u9-win (skipped)` / `… gp-u10-ending (skipped)`, exit 0. In `verify` the
two lines follow `gp-modes-oracle` (U8's lines kept).

An unpinned present capture fails (stand-in `poll.log` built from the U9 preview's `T` lines
read as `S`, in a scratch `K11_CAPTURES`, never `data/`): `gp_win: gp-u9-win: capture: FAIL:
poll.log sha256 622b3e9f…54c2 != the pinned (unpinned): re-measure, then re-pin` and
`make: *** [gp-win-one] Error 1`.

The port preview (scripts of §W.7's frames; each replay `test_gp_replay: 0 restart(s) landed`):

- U9: dump 2 031 entries, 129 MB, 58.8 s wall; `fn-miss` distinct = 7: `0x5D812`
  `actor_spawn` (3 523 hits) and `set_dead` (3 015) (the driver's known pair, §G.24),
  `0x29D60` and `0x5D812` from `frontend_mode_1b_step`, `0x400E0`, `0x21044`, `0x21084`
  `anim_indirect` (1 hit each): exactly §W.7's prediction; the only failures are the
  miss-set check (`7 != 2` and the five `unexpected` lines). `gp_win: gp-u9-win: evidence:
  8/8 milestones ok`.
- U10: dump 302 MB, 2 min 46 s wall; distinct = 8: the same plus `0x3DA50` `anim_indirect`
  (1 hit; `actor_spawn`/`set_dead` 66 079 / 65 219); failures `8 != 2` and six `unexpected`
  lines. `gp_win: gp-u10-ending: evidence: 30/30 milestones ok`.

No other driver failure. The previews were deleted afterwards. `make -n verify` stops in
`gp-modes-one` before the new lines (it executes the recursive `gp_compare`; the same on the
unmodified Makefile), so the order is read from the Makefile: `gp-modes-oracle`,
`gp-win-oracle`, `gp-ending-oracle`, `diff-verify`.

**The counts (Task 7 review note).** §W.7's "2 029 frames" and the "2 031 entries" above count two
different preview runs (the planner's scratch driver and Task 7's preview script, both on the
preview's own frames, ending at `f = 0xDB4`), and a dump directory's entries include three files
that are not frames: `frames.txt`, `gp.log`, `trace.txt`. The replay of the capture
(§W.12, script ending at the capture's `X`, `f = 0xDAE`) holds 2 029 entries = 2 026
`frame_*.ipx` + those three. Neither preview dump survives, so the two-entry difference between
the previews is not resolved; no value is pinned from either.

## §W.11 The `gp-u9-win` capture (Task 8, on `9603d45`)

Taken by the controller (`make gp-capture scenario=gp-u9-win TITLE_PIN_DIR=/tmp/pr_u910_pin`,
log `/tmp/gameplay-u9u10/u9_capture.txt`) with the user at the machine; three earlier runs on
a sleeping host failed to align the TWG logo or were suspended (ledger), and their
`gp-u9-win.failed` directory was replaced by the tool. DOSBox-X's default `memsize` (16 MB):
U9's argv has no `memsize` pair (§W.13).

`session.txt`, all of it:

```
scenario=gp-u9-win
dosbox=DOSBox-X version 2026.08.31 SDL2, copyright 2011-2026 The DOSBox-X Team.
argv=/opt/homebrew/bin/dosbox-x -defaultconf -fastlaunch -nopromptfolder -nogui -nomenu -time-limit 110 -set 'sdl fullscreen=false' -set 'dosbox captures=/var/folders/h_/rk2gng5d0x99pw7x_3dg6mj40000gn/T/gpcap-vbdp0k6f/avi' -set 'dosbox memory file=/var/folders/h_/rk2gng5d0x99pw7x_3dg6mj40000gn/T/gpcap-vbdp0k6f/guest.mem' -set 'log logfile=/var/folders/h_/rk2gng5d0x99pw7x_3dg6mj40000gn/T/gpcap-vbdp0k6f/dosbox.log' -set 'dos log console=quiet' -set 'dosbox quit warning=false' -c 'MOUNT C "/var/folders/h_/rk2gng5d0x99pw7x_3dg6mj40000gn/T/gpcap-vbdp0k6f/C" -ro' -c 'IMGMOUNT D "/var/folders/h_/rk2gng5d0x99pw7x_3dg6mj40000gn/T/gpcap-vbdp0k6f/CD/RAGECD.ISO" -t iso' -c C: -c 'DX-CAPTURE /V /O PRAGE.EXE -f' -c EXIT
exe=/tmp/pr_u910_pin/PRAGE.EXE sha256=8120f1bd1df389ed94cb329c030f95d9e38caad193bbd9840717af557161a68d
cmos=zero pad_bios=1
time_limit=110 wall_s=81.7 rc=0
stop_at_end=1 tail=60 signal_f=0DEA
avis=['prage_000.avi', 'prage_001.avi'] fps=70.0866 dro=['prage_000.dro'] frames=2521 raw_window=1380..5671 avi_frames=5672 twg_last=1329
check=ok base
check=ok steps fired 6/6
check=ok end frame reached
check=ok mode 0x27 after the Enter
check=ok snapshots kb == raw (0 differ)
check=ok frames written 2521/2521
check=ok port script v2
check=ok no unscripted input
check=ok pokes written 2/2, 0 raced
check=ok stopped at the end (SIGTERM at f=0DEA, rc=0)
```

The CHECK lines and the tool's summary:

```
title_pin: wrote /tmp/pr_u910_pin/PRAGE.EXE (pinned: title entry 12, 111, 0 + anim opcode-8 0 + master-loop draws 0x256B1, 0x256D6 -> 0)
gp_capture: snapshots 3541, f 5..DEA, 17 frames missed (spec §3.7)
gp_capture: CHECK base: ok
gp_capture: CHECK steps fired 6/6: ok
gp_capture: CHECK end frame reached: ok
gp_capture: CHECK mode 0x27 after the Enter: ok
gp_capture: CHECK snapshots kb == raw (0 differ): ok
gp_capture: CHECK frames written 2521/2521: ok
gp_capture: CHECK port script v2: ok
gp_capture: CHECK no unscripted input: ok
gp_capture: CHECK pokes written 2/2, 0 raced: ok
gp_capture: CHECK stopped at the end (SIGTERM at f=0DEA, rc=0): ok
gp_capture: wrote 2521 frames to /Users/felipe.dos.santos/code/mine/primal-rage-reverse/data/k11-captures/gp-u9-win (raw 1380..5671), wall 81.7s
```

The evidence (`python3 tools/gp_win.py check --scenario gp-u9-win --capture data/k11-captures/gp-u9-win`,
`/tmp/gameplay-u9u10/u9_evidence.txt`) and the input audit:

```
gp_win: gp-u9-win: evidence: character select (mode 0x10) at f=28D ok
gp_win: gp-u9-win: evidence: round 1: P1 is character 0 (cursor 0 confirmed, 0x43CAD) at f=480 ok
gp_win: gp-u9-win: evidence: round 1 KO: 0x27C48 counts P1, 0x27BA4 leaves the match open, 0x27FA8 -> mode 8 at f=48D ok
gp_win: gp-u9-win: evidence: round 2 (mode 6, round index 2) at f=84E ok
gp_win: gp-u9-win: evidence: round 2 KO: P1 wins the match (0x27BA4 result 0) -> mode 9 at f=85A ok
gp_win: gp-u9-win: evidence: the conquered-lands screen (mode 0x12, 0x4142C), the won land marked by 0x286BC (0x80 | side 0 | character) at f=BA4 ok
gp_win: gp-u9-win: evidence: 0x41C28 state 3 counts one land for P1 at f=C0E ok
gp_win: gp-u9-win: evidence: match 2, round 1: a new, unmarked land (0x25848 picks the stage, 0x41760) at f=D72 ok
gp_win: gp-u9-win: evidence: 8/8 milestones ok
$ python3 tools/gp_capture.py check-input data/k11-captures/gp-u9-win
check=ok no unscripted input
```

Size and identity:

```
$ du -sh data/k11-captures/gp-u9-win
119M	data/k11-captures/gp-u9-win
$ ls data/k11-captures/gp-u9-win | grep -c raw.gz
2521
$ shasum -a 256 data/k11-captures/gp-u9-win/poll.log
7dcea0f16403c0fa52b6ef690edbcccb5340d9b9d1ec96c66ca4d11770708ba8  data/k11-captures/gp-u9-win/poll.log
$ grep '^W' data/k11-captures/gp-u9-win/poll.log
W ms=40465 f=0489 step=4 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=56683 f=0857 step=5 addr=0010789E len=1 was=00 now=78 late=0 race=0
```

The first `S` record of each mode entry a milestone names (modes `0x10`/1, 6/1, 8/1, 6/2,
9/1, `0x12`/1, 6/3):

```
S ms=30942 f=028D mode=0010 st=0000 tick=000006CE t508=0000001B t50c=0000001C raw=00000000 pad=00000000 new=00000000 held=00000000 e0=0000 e2=0000 rng=723D2EA7 cred=00000004 fp=00 b1d=00 b1f=01 b25=00 w10d=00 cnt=00 s0_52=00 s0_54=00 s0_5a=00 s1_52=00 s1_54=00 s1_5a=00 ent=002A2CDC r0=00 r1=00 c0=00 c1=00 s0_43=00 afc=0000 ad4=00000000 w2=00 w3=00 b1e=00 b21=00 b14=00 b0c=00 t104=0000 m106=00000000 m10a=00000000 sc0=00000000 c82=00 kb=0000 head=0024 tail=0024
S ms=40306 f=0480 mode=0006 st=0000 tick=000008FF t508=000000C5 t50c=000000C6 raw=00000000 pad=00000000 new=00000000 held=00000000 e0=0000 e2=0000 rng=BA9703D6 cred=00000004 fp=00 b1d=00 b1f=01 b25=03 w10d=00 cnt=00 s0_52=00 s0_54=00 s0_5a=00 s1_52=00 s1_54=00 s1_5a=00 ent=002A2CDC r0=FF r1=FF c0=00 c1=06 s0_43=80 afc=0005 ad4=FFFFFFFF w2=00 w3=00 b1e=01 b21=00 b14=00 b0c=00 t104=0000 m106=00000000 m10a=00000000 sc0=00000000 c82=00 kb=0000 head=0026 tail=0026
S ms=40520 f=048D mode=0008 st=0000 tick=0000090C t508=000000D2 t50c=000000D3 raw=00000000 pad=00000000 new=00000000 held=00000000 e0=0000 e2=0000 rng=B486137C cred=00000004 fp=00 b1d=00 b1f=01 b25=03 w10d=00 cnt=00 s0_52=00 s0_54=00 s0_5a=00 s1_52=04 s1_54=02 s1_5a=78 ent=002A2CDC r0=FF r1=FF c0=00 c1=06 s0_43=80 afc=0005 ad4=FFFFFFFF w2=01 w3=00 b1e=01 b21=00 b14=00 b0c=00 t104=0000 m106=00000000 m10a=00000000 sc0=00004E20 c82=00 kb=0000 head=0026 tail=0026
S ms=56541 f=084E mode=0006 st=0000 tick=00000CCD t508=0000007B t50c=0000007C raw=00000000 pad=00000000 new=00000000 held=00000000 e0=0000 e2=4C4C rng=A364E551 cred=00000004 fp=00 b1d=00 b1f=01 b25=03 w10d=00 cnt=00 s0_52=00 s0_54=00 s0_5a=00 s1_52=00 s1_54=00 s1_5a=00 ent=002A2CDC r0=FF r1=FF c0=00 c1=06 s0_43=80 afc=0005 ad4=FFFFFFFF w2=01 w3=00 b1e=02 b21=00 b14=00 b0c=00 t104=0000 m106=00000000 m10a=00000000 sc0=00009C40 c82=00 kb=0000 head=0026 tail=0026
S ms=56743 f=085A mode=0009 st=0000 tick=00000CD9 t508=00000087 t50c=00000088 raw=00000000 pad=00000000 new=00000000 held=00000000 e0=0000 e2=0000 rng=8D1ACCD4 cred=00000004 fp=00 b1d=00 b1f=01 b25=03 w10d=00 cnt=00 s0_52=00 s0_54=00 s0_5a=00 s1_52=09 s1_54=00 s1_5a=78 ent=002A2CDC r0=FF r1=23 c0=00 c1=06 s0_43=80 afc=0005 ad4=00000000 w2=02 w3=00 b1e=02 b21=00 b14=00 b0c=00 t104=0000 m106=00000000 m10a=00000000 sc0=0000EA60 c82=00 kb=0000 head=0026 tail=0026
S ms=70901 f=0BA4 mode=0012 st=0000 tick=00001029 t508=00000009 t50c=0000000A raw=00000000 pad=00000000 new=00000000 held=00000000 e0=0000 e2=0000 rng=EFA7BD3D cred=00000004 fp=00 b1d=00 b1f=01 b25=08 w10d=00 cnt=00 s0_52=00 s0_54=00 s0_5a=49 s1_52=01 s1_54=00 s1_5a=78 ent=002A2CDC r0=FF r1=21 c0=00 c1=06 s0_43=80 afc=0005 ad4=00000000 w2=02 w3=00 b1e=02 b21=00 b14=00 b0c=00 t104=0000 m106=00000000 m10a=00008000 sc0=00013880 c82=00 kb=0000 head=0026 tail=0026
S ms=79106 f=0D72 mode=0006 st=0000 tick=00001215 t508=000000AB t50c=000000AC raw=00000000 pad=00000000 new=00000000 held=00000000 e0=0000 e2=2000 rng=B3DCA3FB cred=00000004 fp=00 b1d=00 b1f=01 b25=03 w10d=00 cnt=00 s0_52=00 s0_54=00 s0_5a=00 s1_52=00 s1_54=00 s1_5a=00 ent=002A2CDC r0=FF r1=FF c0=00 c1=04 s0_43=80 afc=0006 ad4=FFFFFFFF w2=00 w3=00 b1e=01 b21=00 b14=00 b0c=00 t104=0001 m106=00000000 m10a=00008000 sc0=00013880 c82=00 kb=0000 head=0026 tail=0026
```

Each poke ended its round in the iteration that read it: the `P` records (logged at each mode
or state change, read outside the spin, so `f` is the iteration that stored the mode) show mode 8 at `f = 0x48A` after `W f=0489` and mode 9 at `f = 0x858`
after `W f=0857` (the write lands in the spin of `W.f`, iteration `W.f + 1` reads it, §W.8).
The first `S` records of those entries are 3 and 2 frames later (`0x48D`, `0x85A`): the
snapshots of `0x48A..0x48C` and `0x858..0x859` were not taken consistently (the
`17 frames missed` of the summary line), so the milestone frames are the `S` frames.

**Against the preview (§W.7).** The mode-entry frames (`P` records) against the preview's
first frame in each mode:

| mode | capture `P` | preview | Δ |
|---|---|---|---|
| `0x27` | `13A` | `141` | −7 |
| `0x10` | `28D` | `293` | −6 |
| 6 (round 1, stage 5, P2 = CHAOS `c1 = 6`) | `480` | `486` | −6 |
| 8 (KO poke) | `48A` | `490` | −6 |
| `0x16` | `6E2` | `6E8` | −6 |
| 5 | `7D3` | `7D9` | −6 |
| 6 (round 2) | `84E` | `854` | −6 |
| 9 (KO poke) | `858` | `85E` | −6 |
| `0x17` | `AB0` | `AB6` | −6 |
| `0x12` | `BA1` | `BA7` | −6 |
| lands 1 (`t104`, the evidence row) | `C0E` (`S`) | `C14` | −6 |
| `0x17` | `C5A` | `C60` | −6 |
| `0x1A` | `CD3` | `CD9` | −6 |
| 5 | `CF7` | `CFD` | −6 |
| 6 (match 2) | `D72` | `D78` | −6 |
| the end (`X`) | `DAE` | `DB4` | −6 |

From mode `0x10` on every entry is exactly 6 frames before the preview's (mode `0x27`: 7).
The shift is the harness's: the preview pressed the three menu Enters at gp-idle-loss's
frames (321/474/622), the capture's boot Enter is the wall-clock step `ENTER_WAIT`
(`I f=0139`) and its later steps key on the capture's own `S` records (`I f=01CF`,
`0265`, `02C8`). From mode `0x10` on, the game's durations between the entries are the
preview's frame for frame.
**One difference that is not a timing shift:** match 2's opponent. The capture's mode 6
at `D72` has `afc = 6`, `c1 = 4` (ARMADON); the preview had stage 3, P2 = VERTIGO. The
stage is `0x25848`'s `rng(n)` pick at `0x258FD` (§W.4), so it follows the RNG state at the
pick, which a 6-frame shift of the whole run changes; the milestone only requires a new,
unmarked land, which holds (`m106/m10a` mark only land 5). Whether the port reproduces
the pick at the capture's frames is the Task 9 replay's trace ratchet (§W.12).
Wall time 81.7 s against the predicted ~78 s; 2 521 stored frames and 119 MB inside §W.7's
estimate (1 500-2 700 frames, ~70-130 MB).

## §W.12 The U9 replay: the miss set and the pins (Task 9, measured at `de44e7f`)

**The miss set.** `make gp-replay scenario=gp-u9-win GP_DUMP=$S/gp GP_OPTIONAL=1` (the full
replay to the capture's `X`, `f = 0xDAE`), with no `k_gp_sets` row:

```
fn-miss PR_GP_DUMP 0x5D812 actor_spawn hits=3462
fn-miss PR_GP_DUMP 0x5D812 set_dead hits=2954
fn-miss PR_GP_DUMP 0x29D60 frontend_mode_1b_step hits=1
fn-miss PR_GP_DUMP 0x5D812 frontend_mode_1b_step hits=2
fn-miss PR_GP_DUMP 0x400E0 anim_indirect hits=1
fn-miss PR_GP_DUMP 0x21044 anim_indirect hits=1
fn-miss PR_GP_DUMP 0x21084 anim_indirect hits=1
fn-miss PR_GP_DUMP distinct=7 dropped=0
FAIL …/port/tests/test_platform.c:328: 7 != 2
fn-miss PR_GP_DUMP: unexpected 0x29D60 from frontend_mode_1b_step   (and 0x5D812, 0x400E0, 0x21044, 0x21084)
FAILURES: 6
```

Exactly §W.7's prediction and §W.10's preview set. **The first frames (raw wins over the
plan's method).** The plan said to read each pair's first frame with `grep -n 'fn-miss\|miss'
$S/gp/gp-u9-win/gp.log`, but the driver's `gp.log` holds only the `key`/`bits`/`poke` lines,
and the miss log (`mem.c` `fn_resolve_from`) keeps no frame. So each first frame was read with
a breakpoint on the miss log's new-entry store (`lldb -b`, `breakpoint set --file mem.c --line
71`, printing `orig_addr`, `ctx` and the words at `mem + 0xEF6DC` (f) and `mem + 0x104B00`
(mode)) on the same script:

| pair | first f | mode | classification |
|---|---|---|---|
| `0x5D812` `actor_spawn` | `0x3` | 3 | the driver's known pair, `k_miss_known` (§G.24) |
| `0x5D812` `set_dead` | `0x4` | 3 | the same |
| `0x29D60` `frontend_mode_1b_step` | `0x28D` | `0x1B` | a wipe hook, the bare `ret` (§G.24) |
| `0x5D812` `frontend_mode_1b_step` | `0x405` | `0x1B` | a wipe hook, the runtime stub (§G.24) |
| `0x400E0` `anim_indirect` | `0x59C` | 8 | E2 triage row `400E0`: `anim-target`, dword `D2816` after the opcode word `D000` at `D2814`, unported (P track) |
| `0x21044` `anim_indirect` | `0x860` | 9 | E2 row `21044`: `anim-target`, dword `E1606` after `D100` at `E1604`, unported (P track) |
| `0x21084` `anim_indirect` | `0x874` | 9 | E2 row `21084`: `anim-target`, dword `E162C` after `D100` at `E162A`, unported (P track) |

Each animation target is 6 frames before §W.7's preview frame (`5A2`, `866`, `87A`): this is
the 6-frame harness shift of §W.11.

**The pin.** `k_miss_gp_u9_win` (the five rows above other than the known pair, in the printed
order) and its `k_gp_sets` row `{ "gp-u9-win", 0, … }` after U8's seven. Rerun: `distinct=7
dropped=0` and `all checks passed`. Mutation (the `0x400E0u` row deleted, rebuilt, rerun):

```
FAIL …/port/tests/test_platform.c:343: 7 != 6
fn-miss PR_GP_DUMP: unexpected 0x400E0 from anim_indirect
FAIL …/port/tests/test_platform.c:348: the driver's miss log holds only its pinned known-set
FAILURES: 2
```

Restored (byte-identical to the backup), then rebuilt.

**The report** (`python3 tools/gp_compare.py --report --scenario gp-u9-win --capture
data/k11-captures/gp-u9-win --port $S/gp/gp-u9-win`, `/tmp/gameplay-u9u10/u9_report.txt`):

```
gp_compare: gp-u9-win: frames: window from capture 100 (raw 1744); 1266 classified: 662 clean, 597 splice, 2 transition, 5 unexplained, 11 all-black
gp_compare: gp-u9-win: frames: FIRST UNEXPLAINED capture 346 (raw 2830): nearest port 221, rows 0..4, x 0..319 (933 px)
gp_compare: gp-u9-win: frames: UNEXPLAINED capture 356 (raw 2840): nearest port 229, rows 44..199, x 0..319 (31501 px)
gp_compare: gp-u9-win: frames: UNEXPLAINED capture 1374 (raw 3973): nearest port 1089, rows 108..190, x 76..260 (4957 px)
gp_compare: gp-u9-win: frames: UNEXPLAINED capture 1375 (raw 3974): nearest port 1090, rows 110..190, x 83..258 (4434 px)
gp_compare: gp-u9-win: frames: UNEXPLAINED capture 1376 (raw 3975): nearest port 1091, rows 108..192, x 0..258 (5789 px)
gp_compare: gp-u9-win: frames: coverage (reported, not ratcheted): 16 non-black port frame(s) up to port 1089 not exhibited by any classified capture frame: [9, 11, 92, 129, 132, 133, 134, 135, 137, 138, 219, 220, 221, 809, 1019, 1083]
gp_compare: gp-u9-win: trace: 1824 frames compared up to the first difference (f 13A..), 13 without a capture snapshot; first tick difference f=13B (reported, not ratcheted)
gp_compare: gp-u9-win: trace: first difference f=866 (2150) in rng: capture 9BE3F91A, port 8D1ACCD4
gp_compare: gp-u9-win: moves: 1849 frames compared up to the first difference (f 13A..) over c0 c1 r0 r1 s0_43, 13 without a capture snapshot
gp_compare: gp-u9-win: moves: first difference f=87F (2175) in r1: capture 23, port 8
```

and `python3 tools/gp_win.py path --scenario gp-u9-win … --min-milestones 0 --win-min-first 0`
(`/tmp/gameplay-u9u10/u9_path.txt`): every milestone row has `capture F port F` at the same frame
(`28D`, `480`, `48D`, `84E`, `85A`, `BA4`, `C0E`, `D72`), then

```
gp_compare: gp-u9-win: path: 0 not reproduced through 7; ratchet N 0 ok (every item is explained: N = 8 is the exact pin)
gp_compare: gp-u9-win: win: 2833 frames compared up to the first difference (f 13A..) over afc ad4 w2 w3 b1e b21 b14 b0c t104 m106 m10a sc0 c82, 16 without a capture snapshot
gp_compare: gp-u9-win: win: first difference f=C5A (3162) in afc: capture 6, port 3
```

**The pins** (Makefile `GP_WIN_*`, with the report lines quoted above them):

| pin | value | provenance |
|---|---|---|
| `GP_WIN_MIN_FIRST` | 346 | `FIRST UNEXPLAINED capture 346` |
| `GP_WIN_TRACE_MIN_FIRST` | 2150 | `trace: first difference f=866 (2150)` |
| `GP_WIN_MAX_START` | 100 | `window from capture 100` |
| `GP_WIN_MILESTONES` | 8 | `0 not reproduced through 7` (all eight) |
| `GP_WIN_WIN_MIN_FIRST` | 3162 | `win: first difference f=C5A (3162)` |
| `GP_WIN_CAPTURE_SHA256` | `7dcea0f16403c0fa52b6ef690edbcccb5340d9b9d1ec96c66ca4d11770708ba8` | `shasum -a 256 …/poll.log` (§W.11) |
| `GP_WIN_CAPTURE_FRAMES` | 2521 | `ls … \| grep -c raw.gz` (§W.11), pinned with the sha256 |

**Re-measure (Task 11 review).** A P batch that ports `0x400E0` or `0x21084` (P4) or
`0x21044` (P5; batches of record `2026-10-02-reverse-p1-derivations.md`) drops its row from
`k_miss_gp_u9_win`, re-measures the U9 set (match 2's stage follows the `rng`, so the set can
change) and re-pins TRACE/WIN/MIN_FIRST. The miss-set check fails when a row's target is
ported, but the trace and win ratchets fail only when they get worse, so an improved value
would otherwise stay pinned low. The same sentence is in the Makefile `GP_WIN_*` comment.

**The first differences, named (none fixed here):**

- **Frames, 346: a long frame at the mode-8 entry, then the catch-up: the frame model's
  limit, not a port divergence** (corrected in fix round 1 from the raw indices). In
  `window.txt`, capture 345 is raw 2827 and capture 346 is raw 2830. Raws 2828 and 2829 were
  not stored because they repeat 2827. So the screen held port 218 (`f = 0x48A`, mode 8's first
  frame, `P f=048A`) for three capture frames, about 43 ms. Then the game caught up through
  `0x48B..0x48D`. Measured on a fresh replay's dump, row by row against the `.ipx` frames:
  - capture 344 (raw 2826) is port 217 (`f = 0x489`) on all 200 rows;
  - capture 345 (raw 2827) is port 218 on all rows;
  - capture 346 (raw 2830) is port 218 on rows 0..4 and port 221 (`f = 0x48D`) on rows 5..199,
    0 px each;
  - capture 347 is port 222.

  Capture 346 is therefore a splice of ports 218 and 221, which the two-adjacent-frame model
  (clean, a splice of two adjacent frames, one transition row) cannot express. Ports 219/220
  (`f = 0x48B/0x48C`) appear in no capture frame. **The poke did not cause it.** The `S` records
  are missed at many mode entries with no poke (`13A`, `268`, `2DD`, `6E2`, `7D3`, `BA1`,
  `CE5`; and at `48A` and `858`, which follow a `W`). The same three-frame scan-out at a mode-8
  entry is gp-idle-loss's capture 2064, with no poke (Makefile `GP_IDLE_LOSS_MIN_FIRST`
  comment, record §U6.21). Why the original's scan-out does this is not isolated.
  **Named gap:** the frame ratchet stops at the first such frame until the comparison models
  it. Raise N then.
  The next unexplained frame, 356 (raw 2840, mode 8), is also the model's limit. Rows 0..43
  equal port 229 (`f = 0x495`) and rows 44..199 equal port 230 (`f = 0x496`), except 221 px at
  x 270..293, rows 155..195 (one of the arena's small figures). Those 221 px all equal port 231
  (`f = 0x497`). Every pixel comes from ports 229/230/231: a partial-draw composite, not a row
  splice. So N's cap is the comparison model. The first unexplained frame that could be real
  content is 1374 (port 1089-1091, `f = 0x85F..0x861`, mode 9, with 1375 and 1376), at the
  `0x21044` miss (`f = 0x860`): the P track.
- **Trace, 2150 (`f = 0x866`, mode 9): the P track.** The original's `rng` steps from
  `8D1ACCD4` (the value both sides hold from `0x85A`) to `9BE3F91A` at `f = 0x866`; the port's
  does not. That is 6 frames after the port's first miss of `0x21044` (`f = 0x860`), which the
  original runs (`+0x57 = 1` and its fields, §W.7). It is at or after the first animation-target
  miss (`0x59C`), so it is that unported target's (P track), as §W.7 predicted ("the trace may
  first differ at or after f = 0x5A2"). Every traced field agrees up to `0x865`. The moves
  ratchet, not pinned by this oracle, first differs at `f = 0x87F` (`r1` 23 vs 8), after
  `0x21084`'s miss at `0x874`.
- **Win fields, 3162 (`f = 0xC5A`, mode `0x17`): a consequence of the trace's.** Match 2's
  stage is `0x25848`'s `rng(n)` pick (`0x258FD`, §W.4), drawn after the `rng` divergence: the
  capture picks stage 6 (ARMADON, §W.11), the port stage 3 (the preview's VERTIGO).
- **Milestones, 8: all reproduced at the capture's frames.** This includes the last one
  ("a new, unmarked land"), because its predicate holds for either stage.

**Each pin fails** (each run on its own on the command line, `make gp-win-oracle
GP_DUMP=/tmp/pr_u910_gp <override>`, `/tmp/gameplay-u9u10/u9_mut.txt`):

| override | the FAIL line (each run exits 2, `make: *** [gp-win-oracle] Error 2`) |
|---|---|
| `GP_WIN_MIN_FIRST=347` | `gp_compare: gp-u9-win: frames: FAIL: first unexplained 346 < ratchet N 347` |
| `GP_WIN_TRACE_MIN_FIRST=2151` | `gp_compare: gp-u9-win: trace: FAIL: first differing 2150 < ratchet N 2151` |
| `GP_WIN_MAX_START=99` | `gp_compare: gp-u9-win: frames: FAIL: window starts at capture 100 (raw 1744) > pinned start 99` |
| `GP_WIN_MILESTONES=9` | `gp_compare: gp-u9-win: path: FAIL: N 9 > end 8: N is unreachable` |
| `GP_WIN_WIN_MIN_FIRST=3163` | `gp_compare: gp-u9-win: win: FAIL: first differing 3162 < ratchet N 3163` |
| `GP_WIN_CAPTURE_SHA256=0000` | `gp_win: gp-u9-win: capture: FAIL: poll.log sha256 7dcea0f1…708ba8 != the pinned 0000: re-measure, then re-pin` (before the replay) |
| `GP_WIN_CAPTURE_FRAMES=2520` | `gp_compare: gp-u9-win: capture: FAIL: poll.log sha256 7dcea0f1…708ba8 (2521 frames) != the pinned 7dcea0f1…708ba8 (2520 fr…` (the log line was cut at 220 characters by the mutation script) |

**The damaged port frame (raw wins over the plan's choice).** The plan's frame did not fail.
`frame_00010.ipx` (`f = 0x13A`, mode `0x27`) with byte 100 set to `0xFF` still gave
`first unexplained 346, ratchet N 346 ok`, exit 0, because the static menu's neighbouring port
frames explain the same capture frames. `frame_00221.ipx` byte 32 100 (inside the
already-unexplained frame 346) did not fail either: `FIRST UNEXPLAINED capture 346 … rows
0..100 … (934 px)`, `ratchet N 346 ok`. The frame ratchet only sees a damaged port frame
that is the sole explanation of a capture frame below N. Three such frames, each with byte
32 100 (row 100, x 100) flipped, on a kept dump of the pinned replay, running the oracle's
`gp_compare` line directly (exit 1 each):

```
## frame_00214.ipx (f=0486, mode 6)
gp_compare: gp-u9-win: frames: FIRST UNEXPLAINED capture 339 (raw 2821): nearest port 214, rows 0..100, x 0..319 (14446 px)
gp_compare: gp-u9-win: frames: FAIL: first unexplained 339 < ratchet N 346
## frame_00200.ipx (f=046A, mode 5)
gp_compare: gp-u9-win: frames: FIRST UNEXPLAINED capture 325 (raw 2789): nearest port 200, rows 100..100, x 100..100 (1 px)
gp_compare: gp-u9-win: frames: FAIL: first unexplained 325 < ratchet N 346
## frame_00150.ipx (f=03FF, mode 0x1B)
gp_compare: gp-u9-win: frames: FIRST UNEXPLAINED capture 262 (raw 2664): nearest port 150, rows 100..100, x 100..100 (1 px)
gp_compare: gp-u9-win: frames: FAIL: first unexplained 262 < ratchet N 346
```

Each frame was restored and the dump deleted afterwards. A last unmodified run of
`make gp-win-oracle` printed every line `ok` (exit 0). As in U8 (§U8.23), the damaged-frame
proof depends on the frame chosen.

**`gp-win-one` removes its dump** after the comparison, as `gp-modes-one` does (`rc=$$?;
[ -n "$(GP_WIN_KEEP)" ] || rm -rf $(GP_DUMP)/$(scenario); exit $$rc`, keep variable
`GP_WIN_KEEP`; Task 7 review). After the pinned run `/tmp/pr_u910_gp` holds no `gp-u9-win`
directory, only its `.script`. The damaged-frame proof therefore keeps the dump
(`GP_WIN_KEEP=1`) and runs the oracle's `gp_compare` line on it directly: `make gp-win-oracle`
replays first, and the replay rewrites every frame.

**U10's round-1 `w3 == 0` (Task 6 re-review).** `TestU10.test_a_cpu_round_win_at_round_1_fails`
gives the synthetic U10 path's mode-8 records `w3 = 1` and expects `FAIL: round 1 KO with the
seven lands poked P1's`. Mutation (`r['w3'] == 0` removed from that `gp_win.MILESTONES` row):
`FAIL: test_a_cpu_round_win_at_round_1_fails … AssertionError: 0 != 1`, then restored.
`test_gp_win` runs 18 tests, and the eight gp suites 191 (188 at `9603d45`, plus
`TestMemsize`'s 2 and this one).

**The task gate** (on the Task 9 tree; logs `/tmp/gameplay-u9u10/g9*.txt`):

- `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest` over the eight gp suites: `Ran 191 tests`, `OK`.
  `PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests | tail -1`: `all checks passed`.
- `make gp-win-oracle GP_DUMP=/tmp/pr_u910_gp`: exit 0, every line `ok` (above).
- `make gp-ending-oracle GP_DUMP=/tmp/pr_u910_gp`: exit 2. This is expected until Task 11 pins
  it: the capture is present and unpinned, so the oracle fails before the replay:
  `gp_win: gp-u10-ending: capture: FAIL: poll.log sha256 a88de48a…feff38 != the pinned (unpinned): re-measure, then re-pin`.
- `make gp-oracle gp-charsel-oracle gp-moves-oracle gp-keys-oracle gp-twop-oracle gp-modes-oracle
  GP_DUMP=/tmp/pr_u910_gp`: exit 0. Its 24 `gp_compare … ratchet N` lines equal Task 0's
  (`diff` empty), and `gp_keys: gp-keys-fight: effects: first not reproduced 11, ratchet N 11 ok`.
- `make diff-verify …`: `diff-verify: 30/30 functions VERIFIED; 47/47 mutants detected; 1 named
  gaps; 1/18 rows with callees closed (12 have none).`, the post-P1 baseline (ledger).
- `git diff main -- port/src`: empty.

## §W.13 The `gp-u10-ending` capture (Task 10, on `9603d45` + the `memsize` hack, committed as `bfb0058`)

Taken by the controller after three probes (below), with DOSBox-X's guest memory at 64 MB
(`-set 'dosbox memsize=64'`, right after the `dosbox memory file=` pair). That flag was an
uncommitted one-line edit of `gp_capture.dosbox_cmd` when the capture ran; `bfb0058` commits
it as the scenario's harness value (`SCENARIOS['gp-u10-ending']['memsize'] = 64`, every other
scenario's argv unchanged), and `TestMemsize.test_the_captures_argv` rebuilds both U9's and this
capture's `argv=` line from the committed code exactly. Log `/tmp/gameplay-u9u10/u10_capture_final.txt`.

`session.txt`, all of it:

```
scenario=gp-u10-ending
dosbox=DOSBox-X version 2026.08.31 SDL2, copyright 2011-2026 The DOSBox-X Team.
argv=/opt/homebrew/bin/dosbox-x -defaultconf -fastlaunch -nopromptfolder -nogui -nomenu -time-limit 240 -set 'sdl fullscreen=false' -set 'dosbox captures=/var/folders/h_/rk2gng5d0x99pw7x_3dg6mj40000gn/T/gpcap-t03kpurf/avi' -set 'dosbox memory file=/var/folders/h_/rk2gng5d0x99pw7x_3dg6mj40000gn/T/gpcap-t03kpurf/guest.mem' -set 'dosbox memsize=64' -set 'log logfile=/var/folders/h_/rk2gng5d0x99pw7x_3dg6mj40000gn/T/gpcap-t03kpurf/dosbox.log' -set 'dos log console=quiet' -set 'dosbox quit warning=false' -c 'MOUNT C "/var/folders/h_/rk2gng5d0x99pw7x_3dg6mj40000gn/T/gpcap-t03kpurf/C" -ro' -c 'IMGMOUNT D "/var/folders/h_/rk2gng5d0x99pw7x_3dg6mj40000gn/T/gpcap-t03kpurf/CD/RAGECD.ISO" -t iso' -c C: -c 'DX-CAPTURE /V /O PRAGE.EXE -f' -c EXIT
exe=/tmp/pr_u910_pin/PRAGE.EXE sha256=8120f1bd1df389ed94cb329c030f95d9e38caad193bbd9840717af557161a68d
cmos=zero pad_bios=1
time_limit=240 wall_s=205.6 rc=0
stop_at_end=1 tail=60 signal_f=271D
avis=['prage_000.avi', 'prage_001.avi'] fps=70.0866 dro=['prage_000.dro'] frames=5704 raw_window=1391..14354 avi_frames=14359 twg_last=1340
check=ok base
check=ok steps fired 20/20
check=ok end frame reached
check=ok mode 0x27 after the Enter
check=ok snapshots kb == raw (0 differ)
check=ok frames written 5704/5704
check=ok port script v2
check=ok no unscripted input
check=ok pokes written 17/17, 0 raced
check=ok stopped at the end (SIGTERM at f=271D, rc=0)
```

The CHECK lines and the tool's summary:

```
title_pin: wrote /tmp/pr_u910_pin/PRAGE.EXE (pinned: title entry 12, 111, 0 + anim opcode-8 0 + master-loop draws 0x256B1, 0x256D6 -> 0)
gp_capture: snapshots 9972, f 4..271D, 38 frames missed (spec §3.7)
gp_capture: CHECK base: ok
gp_capture: CHECK steps fired 20/20: ok
gp_capture: CHECK end frame reached: ok
gp_capture: CHECK mode 0x27 after the Enter: ok
gp_capture: CHECK snapshots kb == raw (0 differ): ok
gp_capture: CHECK frames written 5704/5704: ok
gp_capture: CHECK port script v2: ok
gp_capture: CHECK no unscripted input: ok
gp_capture: CHECK pokes written 17/17, 0 raced: ok
gp_capture: CHECK stopped at the end (SIGTERM at f=271D, rc=0): ok
gp_capture: wrote 5704 frames to /Users/felipe.dos.santos/code/mine/primal-rage-reverse/data/k11-captures/gp-u10-ending (raw 1391..14354), wall 205.6s
```

The evidence (`python3 tools/gp_win.py check --scenario gp-u10-ending --capture data/k11-captures/gp-u10-ending`,
`/tmp/gameplay-u9u10/u10_evidence.txt`) and the input audit:

```
gp_win: gp-u10-ending: evidence: character select (mode 0x10) at f=286 ok
gp_win: gp-u10-ending: evidence: round 1: P1 is character 0 (cursor 0 confirmed, 0x43CAD) at f=479 ok
gp_win: gp-u10-ending: evidence: round 1 KO with the seven lands poked P1's at f=485 ok
gp_win: gp-u10-ending: evidence: round 2 (mode 6, round index 2) at f=847 ok
gp_win: gp-u10-ending: evidence: round 2 KO: P1 wins the match (0x27BA4 result 0) -> mode 9 at f=854 ok
gp_win: gp-u10-ending: evidence: the conquered-lands screen (mode 0x12, 0x4142C), the won land marked by 0x286BC (0x80 | side 0 | character) at f=B9C ok
gp_win: gp-u10-ending: evidence: 0x41C28 state 3 counts the seventh land at f=C3D ok
gp_win: gp-u10-ending: evidence: WORLD DOMINATION: 0x4160C clears the marks, +0x82 = 1 at f=C89 ok
gp_win: gp-u10-ending: evidence: YOU MUST REPLENISH YOUR HEALTH: mode 0x23 (0x417C4, 0x26978) at f=DBB ok
gp_win: gp-u10-ending: evidence: the health bonus (mode 0x22) at f=E36 ok
gp_win: gp-u10-ending: evidence: mode 0x24 (0x26F58) at f=12EA ok
gp_win: gp-u10-ending: evidence: the final: mode 0xC, stage 7, flag DS_00104B14 (0x26F58, 0x25C88) at f=14B6 ok
gp_win: gp-u10-ending: evidence: final opponent 1 KO'd -> mode 0xD (0x272DC) at f=14C2 ok
gp_win: gp-u10-ending: evidence: final opponent 1 replaced (0x274FC, count 1) at f=14CC ok
gp_win: gp-u10-ending: evidence: final opponent 2 KO'd -> mode 0xD (0x272DC) at f=14D6 ok
gp_win: gp-u10-ending: evidence: final opponent 2 replaced (0x274FC, count 2) at f=14E0 ok
gp_win: gp-u10-ending: evidence: final opponent 3 KO'd -> mode 0xD (0x272DC) at f=14EA ok
gp_win: gp-u10-ending: evidence: final opponent 3 replaced (0x274FC, count 3) at f=14F4 ok
gp_win: gp-u10-ending: evidence: final opponent 4 KO'd -> mode 0xD (0x272DC) at f=14FE ok
gp_win: gp-u10-ending: evidence: final opponent 4 replaced (0x274FC, count 4) at f=1508 ok
gp_win: gp-u10-ending: evidence: final opponent 5 KO'd -> mode 0xD (0x272DC) at f=1512 ok
gp_win: gp-u10-ending: evidence: final opponent 5 replaced (0x274FC, count 5) at f=151C ok
gp_win: gp-u10-ending: evidence: final opponent 6 KO'd -> mode 0xD (0x272DC) at f=1526 ok
gp_win: gp-u10-ending: evidence: final opponent 6 replaced (0x274FC, count 6) at f=1530 ok
gp_win: gp-u10-ending: evidence: final opponent 7 KO'd -> mode 0xD (0x272DC) at f=153A ok
gp_win: gp-u10-ending: evidence: YOU ARE MASTER OF THE NEW URTH: mode 0xF, count 7 (0x274FC) at f=1544 ok
gp_win: gp-u10-ending: evidence: SAURON's ending (mode 0x1F, 0x208F8): the winner's character DS_0010782A[DS_00104AD4 * 0x94] (0x20900..0x20914) is 0 at f=1816 ok
gp_win: gp-u10-ending: evidence: the ending, second part (mode 0x1F, state 3) at f=1DB8 ok
gp_win: gp-u10-ending: evidence: the high-score entry (mode 0x1E) at f=217E ok
gp_win: gp-u10-ending: evidence: back in mode 3 at f=26E1 ok
gp_win: gp-u10-ending: evidence: 30/30 milestones ok
$ python3 tools/gp_capture.py check-input data/k11-captures/gp-u10-ending
check=ok no unscripted input
```

Size and identity:

```
$ du -sh data/k11-captures/gp-u10-ending
259M	data/k11-captures/gp-u10-ending
$ ls data/k11-captures/gp-u10-ending | grep -c raw.gz
5704
$ shasum -a 256 data/k11-captures/gp-u10-ending/poll.log
a88de48ad39e90df1e3d3329fbd5aa876c69a08484fa22c66db8deebd3feff38  data/k11-captures/gp-u10-ending/poll.log
$ grep '^W' data/k11-captures/gp-u10-ending/poll.log
W ms=40477 f=0482 step=4 addr=00108106 len=7 was=00000000000000 now=80808080808080 late=0 race=0
W ms=40477 f=0482 step=4 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=56708 f=0850 step=5 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=110443 f=14BF step=6 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=110636 f=14CB step=7 addr=00104B0C len=1 was=00 now=01 late=0 race=0
W ms=111203 f=14D5 step=8 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=111372 f=14DF step=9 addr=00104B0C len=1 was=00 now=01 late=0 race=0
W ms=111856 f=14E9 step=10 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=112019 f=14F3 step=11 addr=00104B0C len=1 was=00 now=01 late=0 race=0
W ms=112188 f=14FD step=12 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=112355 f=1507 step=13 addr=00104B0C len=1 was=00 now=01 late=0 race=0
W ms=112521 f=1511 step=14 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=112687 f=151B step=15 addr=00104B0C len=1 was=00 now=01 late=0 race=0
W ms=113188 f=1525 step=16 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=113355 f=152F step=17 addr=00104B0C len=1 was=00 now=01 late=0 race=0
W ms=113820 f=1539 step=18 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=113997 f=1543 step=19 addr=00104B0C len=1 was=00 now=01 late=0 race=0
```

The first `S` record of each mode entry a milestone names (modes `0x10`/1, 6/1, 8/1, 6/2, 9/1,
`0x12`/1, `0x23`/1, `0x22`/1, `0x24`/1, `0xC`/1-8 interleaved with `0xD`/1-7, `0xF`/1, `0x1F`/1-2,
`0x1E`/1 and mode 3's second entry), cut to the fields the milestones and this section read
(the full lines are in `poll.log` at these `f`, pinned by its sha256):

```
S f=0286 mode=0010 st=0000 rng=723D2EA7 c0=00 c1=00 s0_5a=00 s1_5a=00 afc=0000 ad4=00000000 w2=00 w3=00 b1e=00 b21=00 b14=00 b0c=00 t104=0000 m106=00000000 m10a=00000000 sc0=00000000 c82=00
S f=0479 mode=0006 st=0000 rng=BA9703D6 c0=00 c1=06 s0_5a=00 s1_5a=00 afc=0005 ad4=FFFFFFFF w2=00 w3=00 b1e=01 b21=00 b14=00 b0c=00 t104=0000 m106=00000000 m10a=00000000 sc0=00000000 c82=00
S f=0485 mode=0008 st=0000 rng=9F40FBD1 c0=00 c1=06 s0_5a=00 s1_5a=78 afc=0005 ad4=FFFFFFFF w2=01 w3=00 b1e=01 b21=00 b14=00 b0c=00 t104=0000 m106=80808080 m10a=00808080 sc0=00004E20 c82=00
S f=0847 mode=0006 st=0000 rng=38ABE3AD c0=00 c1=06 s0_5a=00 s1_5a=00 afc=0005 ad4=FFFFFFFF w2=01 w3=00 b1e=02 b21=00 b14=00 b0c=00 t104=0000 m106=80808080 m10a=00808080 sc0=00009C40 c82=00
S f=0854 mode=0009 st=0000 rng=A854BFB9 c0=00 c1=06 s0_5a=00 s1_5a=78 afc=0005 ad4=00000000 w2=02 w3=00 b1e=02 b21=00 b14=00 b0c=00 t104=0000 m106=80808080 m10a=00808080 sc0=0000EA60 c82=00
S f=0B9C mode=0012 st=0000 rng=9C0059AF c0=00 c1=06 s0_5a=50 s1_5a=78 afc=0005 ad4=00000000 w2=02 w3=00 b1e=02 b21=00 b14=00 b0c=00 t104=0000 m106=80808080 m10a=00808080 sc0=00013880 c82=00
S f=0DBB mode=0023 st=0000 rng=B7068FEC c0=00 c1=06 s0_5a=50 s1_5a=78 afc=0005 ad4=00000000 w2=02 w3=00 b1e=03 b21=00 b14=00 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=0E36 mode=0022 st=0000 rng=CDA69EAB c0=00 c1=06 s0_5a=50 s1_5a=78 afc=0005 ad4=00000000 w2=02 w3=00 b1e=03 b21=00 b14=00 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=12EA mode=0024 st=0000 rng=B9817EFB c0=00 c1=06 s0_5a=50 s1_5a=78 afc=0005 ad4=00000000 w2=02 w3=00 b1e=03 b21=00 b14=00 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=14B6 mode=000C st=0000 rng=6489969D c0=00 c1=01 s0_5a=00 s1_5a=00 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=00 b14=01 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=14C2 mode=000D st=0000 rng=6489969D c0=00 c1=01 s0_5a=00 s1_5a=78 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=00 b14=01 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=14CC mode=000C st=0000 rng=A7CBE694 c0=00 c1=04 s0_5a=00 s1_5a=00 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=01 b14=01 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=14D6 mode=000D st=0000 rng=A7CBE694 c0=00 c1=04 s0_5a=00 s1_5a=78 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=01 b14=01 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=14E0 mode=000C st=0000 rng=7AE20E13 c0=00 c1=03 s0_5a=00 s1_5a=00 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=02 b14=01 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=14EA mode=000D st=0000 rng=5D1E86DA c0=00 c1=03 s0_5a=00 s1_5a=78 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=02 b14=01 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=14F4 mode=000C st=0000 rng=0C6ACCA9 c0=00 c1=00 s0_5a=00 s1_5a=00 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=03 b14=01 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=14FE mode=000D st=0000 rng=0C6ACCA9 c0=00 c1=00 s0_5a=00 s1_5a=78 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=03 b14=01 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=1508 mode=000C st=0000 rng=FD64D85F c0=00 c1=06 s0_5a=00 s1_5a=00 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=04 b14=01 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=1512 mode=000D st=0000 rng=FD64D85F c0=00 c1=06 s0_5a=00 s1_5a=78 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=04 b14=01 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=151C mode=000C st=0000 rng=DAFB5735 c0=00 c1=05 s0_5a=00 s1_5a=00 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=05 b14=01 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=1526 mode=000D st=0000 rng=DAFB5735 c0=00 c1=05 s0_5a=00 s1_5a=78 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=05 b14=01 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=1530 mode=000C st=0000 rng=6CC68132 c0=00 c1=02 s0_5a=00 s1_5a=00 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=06 b14=01 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=153A mode=000D st=0000 rng=6CC68132 c0=00 c1=02 s0_5a=00 s1_5a=78 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=06 b14=01 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=0002BF20 c82=01
S f=1544 mode=000F st=0000 rng=C25C7220 c0=00 c1=02 s0_5a=00 s1_5a=78 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=07 b14=01 b0c=00 t104=0007 m106=00000000 m10a=00000000 sc0=004F0A60 c82=01
S f=1816 mode=001F st=0000 rng=164D416B c0=00 c1=02 s0_5a=00 s1_5a=78 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=07 b14=01 b0c=01 t104=0007 m106=00000000 m10a=00000000 sc0=005217A0 c82=01
S f=1DB8 mode=001F st=0000 rng=164D416B c0=00 c1=02 s0_5a=00 s1_5a=78 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=07 b14=01 b0c=01 t104=0007 m106=00000000 m10a=00000000 sc0=005217A0 c82=01
S f=217E mode=001E st=0000 rng=164D416B c0=00 c1=02 s0_5a=00 s1_5a=78 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=07 b14=01 b0c=01 t104=0007 m106=00000000 m10a=00000000 sc0=005217A0 c82=01
S f=26E1 mode=0003 st=0000 rng=164D416B c0=00 c1=02 s0_5a=00 s1_5a=78 afc=0007 ad4=00000000 w2=00 w3=00 b1e=01 b21=07 b14=00 b0c=01 t104=0007 m106=00000000 m10a=00000000 sc0=005217A0 c82=01
```

**The ending's frames.** Mode `0x1F` entry 1 (SAURON's ending, `ad4 = 0`, `c0 = 0`) at
`f = 0x1816`, entry 2 (state 3) at `f = 0x1DB8` (`S` and `P` agree on both). The capture's last
stored frame is `data/k11-captures/gp-u10-ending/last_frame.png` (61 283 bytes, written by the
tool; mode 3 at the stop, `f = 0x271D`).

**The path's details (from the records above).** The marks poke wrote all seven bytes
(`W … addr=00108106 len=7 … now=80808080808080`), the round-1 KO landed in the same spin, and
mode `0x12` showed the seventh land (`t104 = 7` at `0xC3D`); WORLD DOMINATION cleared the marks
and set `c82 = 1`. The final's seven opponents were, in order, `c1` = 1, 4, 3, 0, 6, 5, 2
(BLIZZARD, ARMADON, VERTIGO, SAURON, CHAOS, DIABLO, TALON); each death-done poke found
`b0c = 0` and the next `0xC` entry has `b21 = k`. Scores: `sc0 = 0x4F0A60` (5 180 000) at mode
`0xF`, `0x5217A0` (5 380 000) from the ending on, both as the preview's. Mode `0x1E` was entered
three times (`0x217E`, `0x24FB`, `0x2629`, with mode `0x15` between) before mode `0x14`, `0x17`
and mode 3 at `0x26E1`; the milestone is the first entry. **The game set `b0c` itself** at
`f = 0x1602`, in mode `0xF` (§W.5's correction): no milestone reads `b0c` after the 7th KO.

**Against the preview (§W.7)**, mode-entry frames (`P` records) against the preview's:

| span | capture `P` | preview | Δ |
|---|---|---|---|
| `0x27` … `0x22` (`0x27` 134, `0x10` 286, 6 479, 8 483, 6 847, 9 851, `0x12` B9A, `0x23` DBB, `0x22` E36) | | 141 … E43 | −13 |
| `0x24` | `12EA` | `12EA` | 0 |
| `0x17` 139E, 5 143B, `0xC` 14B6, `0xD` 14C0 | | the same | 0 |
| the next `0xC`/`0xD` entries (`14CC`, `14D6` … `153A`), every 10 frames | | `14CA` … `1538` | +2 |
| `0xF` 1544, `0x17` 179C, `0x15` 179D, `0x1F` 1816, `0x15` 1907, `0x1F` 1DB8, `0x15` 1DF9, `0x1E` 217E, mode 3 26E1 | | 1542 … 26DF | +2 |

The −13 is the harness's Enter timing, as in U9 (§W.11). It vanishes in mode `0x22`, which
lasts `0x4B4` frames against the preview's `0x4A7`. P1's `+0x5A` is `0x50` at its entry, from
the CPU's hits in modes 8 and 9 after the poked KOs. Whether `0x26C8C`'s duration follows that
damage, or an absolute clock, is not established here. The Task 11 replay runs at the capture's
frames and judges it. **Judged (§W.14):** the replay leaves mode `0x22` at `0x12EA` too, with
P1's `+0x5A = 0x78`, so the duration does not follow that damage; the cause stays open. The +2 is the harness's: the first `S` record of the first mode-`0xD`
entry is `0x14C2`, not `0x14C0` (the snapshots of `0x14C0..0x14C1` were missed), so that
`after_entry` poke landed 2 frames later than in the preview. The opponent order also differs
from the preview's (`0x2716C`'s draws follow the RNG). The capture ends (`X`) at 189.6 s on the
session clock against the predicted ~185 s, with 5 704 stored frames and 259 MB, inside
§W.7's estimate (4 200-7 600 frames, ~200-360 MB).

### The memory limit (raw wins)

Four runs of the original, all on the pinned `PRAGE.EXE` (sha256 `8120f1bd…`), the zero CMOS
and DOSBox-X 2026.08.31 (ledger; logs in `/tmp/gameplay-u9u10/`). Runs 2 and 3 used uncommitted
scenario variants and a 300 s time limit. Their `gp-u10-ending.failed` directories were replaced
by the next run.

1. **The plan's scenario, DOSBox-X's default `memsize`** (16 MB: `memsize = 16` in
   `/opt/homebrew/Cellar/dosbox-x/2026.08.31/share/dosbox-x/dosbox-x.reference.full.conf:525`;
   the argv has no `memsize` pair), `u10_capture.txt`. The original printed
   `LOG: Primal Rage is out of memory.` and exited to DOS in the 4th final fight. Snapshots
   `f 5..14F6`, `steps fired 12/20`, `pokes written 9/9, 0 raced`, `no unscripted input: ok`,
   wall 116.3 s. The kept capture's 4th mode-`0xC` entry is `f = 0x14F4`. The message comes from the
   fatal-error exit `0x1D290(code = eax)`: `0x65380(0xEF998, table[code], "Primal Rage"
   (0x80880), edx)`, then `0x62003(code)`. The table is `0xA2CBC`, entry 8 =
   `0xA2CDC` → `0x807F0` `"%s is out of memory.\n"`. Its only code-8 caller is `0x1B5DB` in
   `0x1B544`, the resource-handle resolver (`res.c`/`sprite.c` cite it): an entry not yet
   loaded is loaded by `0x1E774`, and if its pointer is still 0 (and flag `0x40000000` is
   clear), the call is fatal. Capstone over the fixed-up image `/tmp/pr_u910_e2.bin` plus
   Ghidra's xrefs to `0x1D290` (18 call sites; the only `eax = 8` is `0x1B5D4`, and
   `0x1BF1A` passes the zero result of `0x1AD64`). So the 4th opponent's
   resource load found no memory.
2. **No death-done pokes** (only the `0x0C` KO steps; `u10_probe.txt`). The original stayed in
   mode `0xD` from `f = 0x14C0` to `0x413A`, 11 386 frames ≈ 190 s, until the time limit.
   That span was read by the controller from the run's `gp-u10-ending.failed/poll.log`, which
   no longer exists (the next run replaced it). What survives is
   `/tmp/gameplay-u9u10/u10_probe.txt`: `snapshots 16671, f 5..413A`, `steps fired 7/13`,
   `pokes written 4/4, 0 raced`, wall 300.6 s. Seven steps are the four menu steps, the two
   round KOs and the first final KO. The 8th step, the second final KO, keys on mode `0xC`'s 2nd
   entry, which never came. That is consistent with the claim. This confirms §W.5's
   mode-`0xD` claim in the original: the poke is needed.
3. **The final's pokes at +120 frames instead of +10, under DOSBox-X's default 16 MB** (no
   `memsize` pair in its argv; `u10_probe2.txt`). Five final fights completed (`steps fired
   15/20`, `pokes written 12/12, 0 raced`, snapshots to `f = 0x1914`). Then the game crawled:
   121 frames in 135 s, from the controller's reading of that run's `poll.log` (P/W records
   `ms=128091 f=0x1882` → `ms=263442 f=0x18FB`: `0x79` = 121 frames in 135.4 s; that
   `poll.log` was replaced by the next run). There was no host sleep (the session clock and the
   wall agree, wall 300.7 s). That is consistent with memory exhaustion (paging or allocator
   thrash under 16 MB), but it is not proven.
4. **The plan's scenario with `memsize=64`**, the kept capture: every CHECK `ok` and evidence
   30/30, as above.

**Named gap (stays):** the growth of the original's memory use over the poked final is not
root-caused. What is known is the failure site: a resource load in `0x1B544`, at the 4th opponent
under 16 MB. Which allocations accumulate is not known: the KO'd opponents' resources, the death
animations the pokes cut short, or the remains actors that never spawn. Nor is it known whether
the real game, with fought and unpoked KOs, would run out in 16 MB too. `memsize=64` is a
harness value of this scenario, not a game value. It claims nothing about the original's memory
requirement.

## §W.14 The U10 replay: the miss set and the pins (Task 11, measured at `0ea42d0`)

**The miss set (raw wins over the plan's prediction).** `make gp-replay scenario=gp-u10-ending
GP_DUMP=$S/gp GP_OPTIONAL=1` (the full replay to the capture's `X`, `f = 0x26E1`, 4 797 port
frames, 2 min 47 s wall), with no `k_gp_sets` row:

```
fn-miss PR_GP_DUMP 0x5D812 actor_spawn hits=65970
fn-miss PR_GP_DUMP 0x5D812 set_dead hits=65104
fn-miss PR_GP_DUMP 0x29D60 frontend_mode_1b_step hits=1
fn-miss PR_GP_DUMP 0x5D812 frontend_mode_1b_step hits=3
fn-miss PR_GP_DUMP 0x2381C hit_reaction_apply hits=1
fn-miss PR_GP_DUMP 0x37DD4 anim_indirect hits=66
fn-miss PR_GP_DUMP 0x29C78 anim_indirect hits=66
fn-miss PR_GP_DUMP distinct=7 dropped=0
FAIL …/port/tests/test_platform.c:344: 7 != 2
fn-miss PR_GP_DUMP: unexpected 0x29D60 from frontend_mode_1b_step   (and 0x5D812, 0x2381C, 0x37DD4, 0x29C78)
FAILURES: 6
```

The plan predicted gp-u9-win's five pairs plus `0x3DA50` (§W.7, `8 != 2`). The measure differs:
none of U9's three animation targets (`0x400E0`, `0x21044`, `0x21084`) nor `0x3DA50` is reached,
and three other unported targets are. The capture's match 1 is not U9's: the CPU's actions after
the poked KOs differ (in U10 CHAOS is in reaction `0x25` from round 2's second frame, `r1 = 25`
at `f = 0x848` in both the capture and the port), and the final's opponent order follows the
`rng`, which the port no longer shares from `f = 0x849` (below): the capture's 3rd opponent is VERTIGO,
the port's 7th, and the port's VERTIGO fight records no `0x3DA50` miss. The first frames,
read as in §W.12 (`lldb -b`, a breakpoint on `mem.c:71`, the miss log's new-entry store, printing
`orig_addr`, `ctx`, the words at `mem + 0xEF6DC` (f) and `mem + 0x104B00` (mode), on the same
script):

| pair | first f | mode | classification |
|---|---|---|---|
| `0x5D812` `actor_spawn` | `0x3` | 3 | the driver's known pair, `k_miss_known` (§G.24) |
| `0x5D812` `set_dead` | `0x4` | 3 | the same |
| `0x29D60` `frontend_mode_1b_step` | `0x286` | `0x1B` | a wipe hook, the bare `ret` (§G.24) |
| `0x5D812` `frontend_mode_1b_step` | `0x3FE` | `0x1B` | a wipe hook, the runtime stub (§G.24) |
| `0x2381C` `hit_reaction_apply` | `0x848` | 6 (round 2) | E2 triage row `2381C`: `move-callback`, dword `A560C` (character 6, reaction `0x25`), unported (P track, batch P2 of record reverse-p1 §P1.3) |
| `0x37DD4` `anim_indirect` | `0x1518` | `0xD` | E2 row `37DD4`: `anim-target`, dword `D2BCE` after the opcode word `D100` at `D2BCC`, unported (P track, P6) |
| `0x29C78` `anim_indirect` | `0x151B` | `0xD` | outside E2: the code right after the jump table at `0x29C5C` (7 entries), reached by 14 stream dwords (`0xD2BDA` …), record reverse-p1 §P1.2, unported (P track, P7) |

`0x2381C` (capstone on the fixed-up image `/tmp/pr_u910_e2.bin`): when the record's `+8` is 0 it
starts the stream `0xE1506` (`0x3C4CC`), sets `+0x52 = 0xB`, `+0x53 = 6`, `+0x54 = 0`, `+0x0C =
0`, `+0x64 = +0x5F`, `+0x5F = 0xFF`, plays voice `0xAA` (`0x2C3FC`) and returns 1. `0x37DD4`
and `0x29C78` are the first two targets of a fighter's death stream: in each of the seven copies
(`0xD2BCE`, `0xD4850`, `0xE1180`, `0xE454A`, `0xE7916`, `0xEB18A`, `0xED58E` for `0x37DD4`) the
dword of `0x29C78` follows at `+0xC` and that of `0x37EA0` (the death-animation end, the only
non-zero store of `DS_00104B0C`, §W.5) at `+0x1C`: the stream at `0xD2BCC` is
`D100 37DD4 …, D100 29C78 …, C420 …, D100 37EA0`. A second lldb run (breakpoints on
`anim_code_37EA0` and on the miss log's repeat-hit line `mem.c:67` for the two targets) shows
the port's stream re-entering the two missed targets every 2-5 frames from `f = 0x151F` to
`0x1628` (65 repeat hits each; modes `0xC`, `0xD`, `0xF`) and reaching `0x37EA0` at `f = 0x161F`
and `0x162C`, both in mode `0xF`.

**The pin.** `k_miss_gp_u10_ending` (the five rows above other than the known pair, in the
printed order) and its `k_gp_sets` row `{ "gp-u10-ending", 0, … }` after gp-u9-win's. Rerun:
`distinct=7 dropped=0` and `all checks passed`. Mutation (the `0x37DD4u` row deleted, rebuilt,
rerun):

```
FAIL …/port/tests/test_platform.c:360: 7 != 6
fn-miss PR_GP_DUMP: unexpected 0x37DD4 from anim_indirect
FAIL …/port/tests/test_platform.c:365: the driver's miss log holds only its pinned known-set
FAILURES: 2
```

Restored (byte-identical to the backup), then rebuilt.

**The report** (`python3 tools/gp_compare.py --report --scenario gp-u10-ending --capture
data/k11-captures/gp-u10-ending --port $S/gp/gp-u10-ending`):

```
gp_compare: gp-u10-ending: frames: window from capture 83 (raw 1745); 1182 classified: 592 clean, 581 splice, 4 transition, 5 unexplained, 11 all-black
gp_compare: gp-u10-ending: frames: FIRST UNEXPLAINED capture 331 (raw 2831): nearest port 219, rows 0..63, x 0..319 (10336 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 341 (raw 2841): nearest port 227, rows 147..199, x 0..319 (12481 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 342 (raw 2842): nearest port 228, rows 155..199, x 0..293 (942 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 1274 (raw 3954): nearest port 1025, rows 156..192, x 63..288 (1268 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 1275 (raw 3955): nearest port 1025, rows 101..192, x 63..288 (3563 px)
gp_compare: gp-u10-ending: frames: coverage (reported, not ratcheted): 17 non-black port frame(s) up to port 1024 not exhibited by any classified capture frame: [9, 11, 29, 31, 32, 90, 91, 127, 131, 132, 133, 135, 217, 218, 228, 972, 973]
gp_compare: gp-u10-ending: trace: 1801 frames compared up to the first difference (f 134..), 13 without a capture snapshot; first tick difference f=135 (reported, not ratcheted)
gp_compare: gp-u10-ending: trace: first difference f=849 (2121) in rng: capture 5A8FCA6A, port A854BFB9
gp_compare: gp-u10-ending: trace: normalised (reported, not ratcheted): ent 0 of 9612 differ; t508 6936 of 9612 differ (first f=264)
gp_compare: gp-u10-ending: moves: 1803 frames compared up to the first difference (f 134..) over c0 c1 r0 r1 s0_43, 13 without a capture snapshot
gp_compare: gp-u10-ending: moves: first difference f=84B (2123) in r1: capture 25, port 15
```

and `python3 tools/gp_win.py path --scenario gp-u10-ending … --min-milestones 0 --win-min-first
0`: every milestone row has `capture F port F` at the same frame (`286`, `479`, `485`, `847`,
`854`, `B9C`, `C3D`, `C89`, `DBB`, `E36`, `12EA`, `14B6`, the fourteen final rows `14C2` …
`153A`, `1544`, `1816`, `1DB8`, `217E`, `26E1`), then

```
gp_compare: gp-u10-ending: path: 0 not reproduced through 29; ratchet N 0 ok (every item is explained: N = 30 is the exact pin)
gp_compare: gp-u10-ending: win: 5305 frames compared up to the first difference (f 134..) over afc ad4 w2 w3 b1e b21 b14 b0c t104 m106 m10a sc0 c82, 22 without a capture snapshot
gp_compare: gp-u10-ending: win: first difference f=1602 (5634) in b0c: capture 1, port 0
```

**The pins** (Makefile `GP_ENDING_*`, with the report lines quoted above them):

| pin | value | provenance |
|---|---|---|
| `GP_ENDING_MIN_FIRST` | 331 | `FIRST UNEXPLAINED capture 331` |
| `GP_ENDING_TRACE_MIN_FIRST` | 2121 | `trace: first difference f=849 (2121)` |
| `GP_ENDING_MAX_START` | 83 | `window from capture 83` |
| `GP_ENDING_MILESTONES` | 30 | `0 not reproduced through 29` (all thirty) |
| `GP_ENDING_WIN_MIN_FIRST` | 5634 | `win: first difference f=1602 (5634)` |
| `GP_ENDING_CAPTURE_SHA256` | `a88de48ad39e90df1e3d3329fbd5aa876c69a08484fa22c66db8deebd3feff38` | `shasum -a 256 …/poll.log` (§W.13) |
| `GP_ENDING_CAPTURE_FRAMES` | 5704 | `ls … \| grep -c raw.gz` (§W.13), pinned with the sha256 |

**Re-measure (Task 11 review).** A P batch that ports `0x2381C` (P2), `0x37DD4` (P6) or
`0x29C78` (P7) drops its row from `k_miss_gp_u10_ending`, re-measures the U10 set (the
final's opponent order follows the `rng`, so the set can change, e.g. `0x3DA50`, §W.7) and
re-pins TRACE/WIN/MIN_FIRST. The trace and win ratchets fail only when they get worse, so an
improvement stays pinned low until it is re-measured. The same sentence is in the Makefile
`GP_ENDING_*` comment. The wipe hooks' first frames (lldb on the miss log's new-entry store,
as §W.12) are `0x29D60` at `f = 0x286` and `0x5D812` (`frontend_mode_1b_step`) at
`f = 0x3FE` (mode `0x1B` both; U9: `0x28D` and `0x405`).

**The first differences, named (none fixed here):**

- **Frames, 331: a long frame at the mode-8 entry, then the catch-up, as U9's 346 (§W.12):
  the frame model's limit, not a port divergence** (corrected in fix round 1 from the raw
  indices). In `window.txt`, capture 330 is raw 2828 and capture 331 is raw 2831. Raws 2829
  and 2830 were not stored because they repeat 2828. So the screen held port 216 (`f = 0x483`,
  mode 8's first frame, `P f=0483`) for three capture frames, about 43 ms. Then the game caught
  up through `0x484..0x486`. Row by row against a fresh replay's `.ipx` frames:
  - capture 329 (raw 2827) is port 215 (`f = 0x482`);
  - capture 330 is port 216 on all 200 rows;
  - capture 331 is port 216 on rows 0..1, port 218 (`f = 0x485`) on rows 2..63 and port 219
    (`f = 0x486`) on rows 64..199, 0 px each;
  - capture 332 is ports 219/220 (a two-frame splice).

  Capture 331 is a three-frame composite, which the two-adjacent-frame model cannot express.
  Port 217 (`f = 0x484`) appears in no capture frame. The poke did not cause it: the `S`
  records are missed at many mode entries with no poke (`134`, `261`, `274`, `2D6`, `6DB`,
  `7CC`, `B9A`, `DA9`, `1429`, `246E`, `24FC`, `262D`). The same scan-out is at gp-idle-loss's
  unpoked mode-8 entry (record §U6.21). **Named gap (as §W.12):** the frame ratchet stops at
  the first such frame until the comparison models it. Raise N then. The U9 and U10 frame
  ratchets are both capped by it, each at its mode-8 entry.
  The next unexplained frames, 341/342 (mode 8), are the model's limit as well; no pixel lacks
  a port source.
  - 341 (raw 2841) is port 227 (`f = 0x48E`) on rows 0..146 and port 228 (`f = 0x48F`) on rows
    147..199, except 221 px at x 270..293, rows 155..195, which all equal port 229
    (`f = 0x490`).
  - 342 (raw 2842) is port 228 on rows 0..199, except 942 px at x 0..293, rows 155..199, which
    all equal port 229.

  Both are partial-draw composites (the arena figure at the bottom), as U9's 356. The first
  unexplained frames that could be real content are 1274/1275 (port 1025, `f = 0x847..0x849`,
  rows 101..192), at the `0x2381C` miss (`f = 0x848`): the P track.
- **Trace, 2121 (`f = 0x849`, mode 6, round 2): the P track (`0x2381C`).** Both sides hold
  `rng = 38ABE3AD` to `0x847` and at `f = 0x848` both draw it to `5A8FCA6A` and set `r1 = 0x25`
  (CHAOS's reaction `0x25`), the reaction whose move callback is `0x2381C`, which the port misses
  at that frame. The original keeps `5A8FCA6A` and `r1 = 25` until the round-2 KO
  (`W f=0850`); the port draws `rng` again at `0x849` (`A854BFB9`) and turns to reaction `0x15`
  at `0x84B` (the moves line). So the first difference is one frame after an unported target's
  miss: P track. Every traced field agrees up to `0x848`.
- **Win fields, 5634 (`f = 0x1602`, mode `0xF`): the P track.** The original's death-animation
  end `0x37EA0` sets `DS_00104B0C = 1` at `f = 0x1602` (§W.13); the port's does at `f = 0x161F`
  (its trace's first `b0c = 1`, the lldb hit on `anim_code_37EA0` above), 29 frames later. Its
  stream runs through the two unported targets `0x37DD4`/`0x29C78` before `0x37EA0`, and the
  dying fighter is not the capture's (the final's opponent order follows the `rng` after `0x849`:
  `c1` = 1, 4, 3, 0, 6, 5, 2 in the capture, 4, 2, 1, 6, 5, 0, 3 in the port). Which of the two
  moves the store is not separated; both are P track. Every other WIN field (`afc ad4 w2 w3 b1e
  b21 b14 t104 m106 m10a sc0 c82`) agrees through `0x1601`, including the scores.
- **Milestones, 30: all reproduced at the capture's frames,** including the ending's
  (`0x1816`, `0x1DB8`), the high-score entry (`0x217E`) and mode 3 (`0x26E1`). The milestones do
  not read the opponents' characters, so the different order does not touch them.
- **Mode `0x22`'s duration (§W.13's open question):** the replay enters mode `0x22` at `0xE36`
  and mode `0x24` at `0x12EA`, as the capture, while P1's `+0x5A` there is `0x78` in the port and
  `0x50` in the capture; so the duration does not follow that damage. The preview entered `0x22`
  at `0xE43` and also left at `0x12EA`: the three runs leave mode `0x22` at the same frame. Its
  cause (an absolute clock or another counter in `0x26C8C`/`0x26D4C`) is not established, and
  nothing in the ratchets depends on it (the trace agrees there).

**Each pin fails** (each run on its own on the command line, `make gp-ending-oracle
GP_DUMP=/tmp/pr_u910_gp <override>`):

| override | the FAIL line (each run exits 2, `make: *** [gp-ending-oracle] Error 2`) |
|---|---|
| `GP_ENDING_MIN_FIRST=332` | `gp_compare: gp-u10-ending: frames: FAIL: first unexplained 331 < ratchet N 332` |
| `GP_ENDING_TRACE_MIN_FIRST=2122` | `gp_compare: gp-u10-ending: trace: FAIL: first differing 2121 < ratchet N 2122` |
| `GP_ENDING_MAX_START=82` | `gp_compare: gp-u10-ending: frames: FAIL: window starts at capture 83 (raw 1745) > pinned start 82` |
| `GP_ENDING_MILESTONES=31` | `gp_compare: gp-u10-ending: path: FAIL: N 31 > end 30: N is unreachable` |
| `GP_ENDING_WIN_MIN_FIRST=5635` | `gp_compare: gp-u10-ending: win: FAIL: first differing 5634 < ratchet N 5635` |
| `GP_ENDING_CAPTURE_SHA256=0000` | `gp_win: gp-u10-ending: capture: FAIL: poll.log sha256 a88de48a…feff38 != the pinned 0000: re-measure, then re-pin` (before the replay) |
| `GP_ENDING_CAPTURE_FRAMES=5703` | `gp_compare: gp-u10-ending: capture: FAIL: poll.log sha256 a88de48a…feff38 (5704 frames) != the pinned a88de48a…feff38 (5703 frames):` (then the re-measure advice) |

The unmodified run (`make gp-ending-oracle GP_DUMP=/tmp/pr_u910_gp`, exit 0): `Ran 18 tests` `OK`,
`evidence: 30/30 milestones ok`, the capture-pin match, `ratchet N 331 ok`, `ratchet N 2121 ok`,
`path: … ratchet N 30 ok`, `win: … ratchet N 5634 ok`.

**The damaged port frame.** The plan's frame (Task 9 Step 5's `frame_00010.ipx`, byte 100) does
not fail here either: `frame_00010.ipx` (`f = 0x263`, mode `0x1A`) with byte 100 set to `0xFF`
still gave `first unexplained 331, ratchet N 331 ok`. Three frames that are the sole explanation
of a capture frame below N do fail, each with byte 32 100 (row 100, x 100) flipped on the kept
dump of the pinned replay, running the oracle's `gp_compare` line directly (exit 1):

```
## frame_00214.ipx (f=0481, mode 6)
gp_compare: gp-u10-ending: frames: FIRST UNEXPLAINED capture 327 (raw 2825): nearest port 214, rows 0..100, x 0..319 (2395 px)
gp_compare: gp-u10-ending: frames: FAIL: first unexplained 327 < ratchet N 331
## frame_00200.ipx (f=0469, mode 5)
gp_compare: gp-u10-ending: frames: FIRST UNEXPLAINED capture 309 (raw 2797): nearest port 200, rows 100..100, x 100..100 (1 px)
gp_compare: gp-u10-ending: frames: FAIL: first unexplained 309 < ratchet N 331
## frame_00150.ipx (f=03FA, mode 0x1B)
gp_compare: gp-u10-ending: frames: FIRST UNEXPLAINED capture 244 (raw 2668): nearest port 151, rows 59..151, x 0..48 (1667 px)
gp_compare: gp-u10-ending: frames: FAIL: first unexplained 244 < ratchet N 331
```

Each frame was restored (`cmp` equal) and the dump deleted afterwards. As in §W.12 and U8
(§U8.23), the damaged-frame proof depends on the frame chosen.

**Corrections:** §W.7's predicted U10 misses (gp-u9-win's five pairs and `0x3DA50`) are not the
replay's: the measured set is the two wipe hooks plus `0x2381C`, `0x37DD4`, `0x29C78` (above).
Task 9's note that the U10 trace and win pins would stop at U9's mode-9 `rng` difference
(`0x21044`) does not hold either: the trace stops in round 2's mode 6 (`0x2381C`), the win
fields at the death-animation end.

**The task gate** (on the Task 11 tree):

- `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest` over the eight gp suites: `Ran 191 tests`, `OK`.
- `cmake --build build && PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests | tail -1`:
  `all checks passed`.
- `make gp-ending-oracle GP_DUMP=/tmp/pr_u910_gp`: exit 0, every line `ok` (above).
- `make gp-oracle gp-charsel-oracle gp-moves-oracle gp-keys-oracle gp-twop-oracle gp-modes-oracle
  gp-win-oracle GP_DUMP=/tmp/pr_u910_gp`: exit 0, no `FAIL`; the 29 `ratchet N … ok` lines (the keys line included) are
  Task 0's and §W.12's values (idle-loss 2064/8320, charsel 516/1513, moves-b 1005/2262/2949,
  twop 612/1506/1506, RA 726/1978, LT 1076/2338, RT 1098/2402, TOW 1107/2466, HC 1022/2274,
  END 278/1174, AS 1087/2018, U9 346/2150/8/3162) with `gp_keys: gp-keys-fight: effects: first
  not reproduced 11, ratchet N 11 ok`.
- `make diff-verify DIFF_IMAGE=/tmp/pr_u910_diff_image.bin DIFF_TABLE=/tmp/pr_u910_diff_table.md`:
  `diff-verify: 30/30 functions VERIFIED; 47/47 mutants detected; 1 named gaps; 1/18 rows with
  callees closed (12 have none).`, the post-P1 baseline.
- `git diff --stat 1085402 -- port/src`: empty.
- The full `make verify` was not run (instructed).
