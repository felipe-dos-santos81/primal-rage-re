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
| `ad4` | `DS_00104AD4`, 4 | the match result: `0x27BA4` (0, 1, 2 or -1), `0x259CC` (-1) |
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
`0x39FF4` on that path). The claim never covers those (§W.10).

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
`0x286BC` with the same value. What it replaces: six won matches (§W.10).

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
