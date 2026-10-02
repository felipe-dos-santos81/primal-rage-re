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
death-done poke it stayed in mode `0xD` from `f = 0x14C0` to `0x413A`, 190 s), but "a poked KO
starts no death animation" is too strong: in `gp-u10-ending` the game itself sets
`DS_00104B0C = 1` at `f = 0x1602`, in mode `0xF`, 190 frames after the 7th poked KO was counted
(`0x1544`). `0x37FF6` being the only non-zero store, `0x37EA0` ran there. Why the death
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
frames and judges it. The +2 is the harness's: the first `S` record of the first mode-`0xD`
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
   mode `0xD` from `f = 0x14C0` to `0x413A`, 11 386 frames ≈ 190 s, until the time limit
   (`steps fired 7/13`, `pokes written 4/4, 0 raced`). This confirms §W.5's mode-`0xD` claim in
   the original: the poke is needed.
3. **The final's pokes at +120 frames instead of +10** (`u10_probe2.txt`). Five final fights
   completed (`steps fired 15/20`, `pokes written 12/12, 0 raced`, snapshots to `f = 0x1914`).
   Then the game crawled: 121 frames in 135 s, with no host sleep (the session clock and the
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
