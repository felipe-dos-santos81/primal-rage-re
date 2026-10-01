# Gameplay U6 (moves): derivation record

Raw-byte and measured evidence for the two U6 plans:
`2026-10-01-gameplay-u6a-idle-loss-divergences.md` (U6a: port what the idle-loss replay
proves missing, re-pin its ratchets) and `2026-10-01-gameplay-u6b-moves-capture.md` (U6b:
a scripted-moves capture, its ratchets and the callbacks it reaches). Everything here was
measured by the planner on `main` at `e9271df` in scratch trees under
`/private/tmp/claude-501/.../scratchpad/plans/u6/` (never committed); the executor re-measures
each value in the task that pins it. Raw wins on any conflict with a plan.

## §U6.0 Sources

The fixed-up image: `build/diffrun --exe data/game/C/PRAGE.EXE --image-out
image.bin` (1 028 304 bytes from `0x10000`, sha256 `0cfd6f481182f050d5897871068bb9f8fa4410108a56cba0b17be7d513943dec`),
disassembled with capstone 5.0.7 (`CS_MODE_32`); every address below is a linear address and
every data operand is fixed up. The live Ghidra MCP (program `PRAGE.EXE`, read-only queries) is
cited where used; the committed function list `port/decomp/prage.functions.csv` (1352 rows) is the
counter's source. The capture: `data/k11-captures/gp-idle-loss` (run 1, `poll.log` sha256
`773e2647…8c8447`, 8173 frames).

---

## §U6.1 The baseline replay (re-measured)

`gp_session.py port-script --scenario gp-idle-loss` writes `enter_frame 321`, `key 321/474/622
1C 0D`, `end 8319`. The replay of `e9271df` (`PR_GP_DUMP=… PR_GP_SCRIPT=… run_tests`) ends `all
checks passed` with the record §G.24 set, 7 pairs, 0 dropped:

```
fn-miss PR_GP_DUMP 0x5D812 actor_spawn hits=6442
fn-miss PR_GP_DUMP 0x5D812 set_dead hits=6096
fn-miss PR_GP_DUMP 0x29D60 frontend_mode_1b_step hits=1
fn-miss PR_GP_DUMP 0x5D812 frontend_mode_1b_step hits=1
fn-miss PR_GP_DUMP 0x23208 hit_reaction_apply hits=1
fn-miss PR_GP_DUMP 0x3A588 fighter_state_3531c hits=5353
fn-miss PR_GP_DUMP 0x3640C anim_indirect hits=1
```

`gp_compare.py --report`: `window from capture 90 (raw 1744)`, `FIRST UNEXPLAINED capture 203 (raw
2359)`, `trace: first difference f=828 (2088) in rng: capture 73A05D37, port CE92DD04` — the pinned
`GP_IDLE_LOSS_MIN_FIRST = 203`, `GP_IDLE_LOSS_TRACE_MIN_FIRST = 2088`, `GP_IDLE_LOSS_MAX_START = 90`.
`port_progress.py`: `771 1203 64`, `731 731 100`.

## §U6.2 `0x23208`: character 1's reaction-0x26 callback

References: one dword, `0xA3D20` = `0xA3528 + (1*64 + 0x26)*20` (the move table 0x3AFC4 indexes),
no `call`/`jmp rel32`. Live Ghidra lists `FUN_00023208` (body `0x23208..0x2324F`); the committed
csv does not, so `symbols.h` has no `FN_00023208` and the counters do not move.

```
023208 push ecx / push esi / sub esp,0x18
02320D mov ecx,eax          ; slot
02320F mov esi,edx          ; rec
023211 mov edx,ebx / mov eax,esp / call 0x33950   ; ctx_same(ctx, side): built, never read
02321A mov edx,0xe4900 / mov eax,esi / push 0x40400000 / call 0x3c4cc   ; RET 4 pops the push
02322B mov byte [ecx+0x52],9 / [ecx+0x53],7 / [ecx+0x54],0
023237 mov eax,0x79 / 02323C mov dword [ecx+0xc],0 / 023243 call 0x2c3fc   ; voice 0x79
023248 mov al,1 / add esp,0x18 / pop esi / pop ecx / ret
```

The caller `0x34E2C` calls it at `0x35045` (`call [esp+0x24]`, EAX = slot, EDX = rec, EBX = side)
and never reads AL (`0x35049 add esp,0x28`). Callees `0x33950` (`fighter_ctx_same`), `0x3C4CC`
(`hit_anim_start_b`), `0x2C3FC` (`sound_voice`) are ported. Fields written: slot `+0x52/+0x53/+0x54/
+0x0C`, the record through `0x3C4CC`. Size `0x48` bytes, 23 instructions.

## §U6.3 `0x3A588`: the `0x3A650` pose family's case-10 handler

Reference: the immediate at `0x3A689` inside `0x3A650` (`mov dword [eax+0x10],0x3a588` at
`0x3A686`). Not a Ghidra function (live or csv). Body `0x3A588..0x3A64C` (`0xC5` bytes):

```
03A588 sub esp,0x18 / mov edx,ebx / mov eax,esp / call 0x33a10        ; ctx_swap(ctx, side)
03A594 mov eax,[esp+0xc] / mov al,[eax+0x58] / cmp al,1 / jb 3a5a5 / jbe 3a5b9 / (else) ret
03A5A5 test al,al / jne 3a649 / mov byte [ctx3+0x58],1 / ret          ; phase 0 -> 1
03A5B9 push 3.0 / al = ctx3+0x7a / edx = [eax*4+0xc9030] / eax = [esp+0x18] (ctx5) / call 0x2bc30
03A5DA edx = ctx5 / ebx = 0 / eax = ctx1 / edx = [edx+0x18] / call 0x188ac   ; anchor x, y 0
03A5EC mov byte [ctx3+0x58],2
03A5F4 ebx = [ctx1*2+0x107d0a] >> 16 (A: word 0x107D0C+side*2); edx = [ctx1*2+0x107cfe] >> 16 (B: word 0x107D00+side*2)
03A60E B == 0 or B == 5 -> 3a63e; al = ctx3+0x90 - 1; cmp 3; ja 3a634 (snap); else jmp [eax*4+0x3a578]
03A634 edx = A / eax = ctx1 / call 0x188dc                            ; snap x
03A63E mov byte [ctx3+0x90],2 ; 03A649 add esp,0x18 / ret
```

The jump table `0x3A578` holds `0x3A63E` ×4 (+0x90 in 1..4 skips the snap). The setter `0x3A650`
stores `A = slot+0x2C` at `0x107D0C+side*2` and `B = BX` at `0x107D00+side*2` (`0x3A6AF/0x3A6B7`).
Callees `0x33A10`, `0x2BC30` (`actors_anim_begin`), `0x188AC` (`hit_anchor_set`), `0x188DC`
(`hit_anchor_x`) are ported. It is `0x3A6D4`'s body (ported, record §41-A) with the table
`0xC9030`, the globs `0x107D00/0x107D0C` and `+0x90 = 2`.

## §U6.4 `0x3640C`: an animation-opcode `0xD000` target

Seven dwords, each after a `0xD000` word: `0xD2156 0xD3E2A 0xE063E 0xE39F2 0xE6DF2 0xEA626 0xECBFA`.
Body (`0x21` bytes): `push edx; mov byte [eax+0x52],0; mov edx,0x14; mov dword [eax+0x24],3.0;
mov [eax+0x4d],dl; mov eax,[eax+0x14]; test; je; mov byte [eax+0x52],5; pop edx; ret`. A leaf. Its
callers, the three `call [0x105BD4]` sites of `0x2B2A0` (`0x2B56D`, `0x2B594`, `0x2B5EA`), overwrite
EAX at once (`0x2B573 xor ecx,ecx; mov eax,ecx`, `0x2B59A mov eax,ecx`, `0x2B5F0 mov eax,ecx`).

## §U6.5 `0x37DCC`: an animation-opcode `0xD100` target

Seventeen dwords after `0xD100` words: `0xD2B98 0xD329C 0xD4816 0xD4F30 0xE113C 0xE18DC 0xE4502
0xE502C 0xE78A4 0xE8676 0xE871E 0xEB154 0xEB724 0xEB7D8 0xEB8AE 0xED520 0xEDB4A`. Body:
`mov byte [0x1078fc],1; ret` (8 bytes). It is **reached only once `0x3A588` is ported** (§U6.7),
so U6a ports it ahead of `0x3A588`.

## §U6.6 What the differential harness can and cannot verify

`0x3640C` and `0x37DCC` are leaves: specs in `tools/diff_verify.py` (mask 0, the callers do not
read EAX) give `fighter_3640c 0x3640C 2 3/3 VERIFIED`, `fighter_37dcc 0x37DCC 2 1/1 VERIFIED`, and
`--self-check` `6/6 functions VERIFIED; 7/7 mutants detected`; `tools/tests/test_diff_verify.py` `Ran
44 … OK` with the lists extended. The records live in the image's zero BSS at `0x10A000/0x10A100`.

`0x23208` and `0x3A588` are not verifiable by E1 (record E1 §E.6 items 1 and 5): their direct-call
closures (`diff_emu.static_scan`, measured) are 21 and 17 functions and contain indirect transfers
the emulator stops on — `0x2C3FC` (`indirect@2C42F`), `0x2B2A0` (`call [0x105BD4]` at `0x2B2FC`),
`0x29F34` (3), `0x18350`, `0x18540`, `0x18460`, `0x18428`; `0x3A588` itself has the jump table at
`0x3A62C`. Named gap (track P stubs); they are verified by seeded unit tests and by the replay.

## §U6.7 Cumulative replays: the order of the four ports

Each variant is the fully patched tree with the others' `fn_register` lines removed (an
unregistered function behaves exactly like an unported one). Full `gp-idle-loss` replay, report mode:

| registered | misses (besides the 4 harmless pairs) | trace first diff | round 1 |
|---|---|---|---|
| none (base) | `23208` `3A588`(5353) `3640C`(1) | `0x828` (2088) | time-up `0x14D9`, P1 `+0x5A=0x38` |
| `3640C` | `23208` `3A588`(5356) | `0x828` (2088) | time-up `0x14D9`, `0x38` |
| `3640C 23208` | `3A588`(4626) | `0x871` (2161) | time-up `0x14D9`, `0x5C` |
| `23208` | `3A588`(4626) `3640C`(2) | `0x871` | time-up |
| `23208 3A588` | `37DCC`(1) — `3640C` no longer reached | `0x871` | **KO `0xCD0`**, `0x78` |
| `23208 3A588 3640C` | `37DCC`(1) | `0x871` | KO `0xCD0` |
| all four | none | `0x871` (2161) | KO `0xCD0` |

The 4 harmless pairs are `0x5D812` ×3 (the runtime stub) and the bare `ret` `0x29D60`. So the order
**`0x3640C` → `0x23208` → `0x37DCC` → `0x3A588`** makes the pinned set shrink by exactly the ported
function at every step (7 → 6 → 5 → 5 → 4 distinct pairs): `0x3640C` is reached on the time-up path
(f = `0x173A`, round 2) and disappears once `0x3A588` changes the path, and `0x37DCC` is reached
only after `0x3A588`. In every variant the frame claim is `FIRST UNEXPLAINED capture 203` and the
window starts at capture 90 (the character select precedes all four).

## §U6.8 The path after U6a

All four registered (`trace.txt` first `f` per mode; the capture's from its `S` records, the record
§G.22 table's `P`-record values in brackets):

| mode | capture | port before | port after U6a |
|---|---|---|---|
| 6 (round 1) | `0x7F5` | `0x7F5` | `0x7F5` |
| 8 | `0xC74` [`0xC71`], KO, P1 `+0x5A=0x78` | `0x154A` after time-up | **`0xCD0`**, KO, `0x78` |
| `0x16`, 5, 6 | `0xD2F`, `0xE1F`, `0xE99` | `0x154B`, `0x163C`, `0x16B7` | `0xD7D`, `0xE6E`, `0xEE9` |
| 7 (round 2 time-up) | `0x1B4E` [`0x1B4B`] | not reached | `0x1BB9` |
| 9, `0x17`, `0x15`, `0x13` | `0x1BBC`, `0x1BC0`, `0x1CAE`, `0x1D27` | not reached | `0x1C2A`, `0x1C2B`, `0x1D1C`, `0x1D95` |
| `0x1E`, `0x14`, `0x17` | `0x1FC1`, `0x1FC4`, `0x1FC7` | not reached | `0x2040`, `0x2044`, `0x2045` |
| 3 | `0x207F` [`0x207A`] | not reached | not reached (script ends at `0x207F`) |

Round 1 is now a CPU KO as in the original: **1243 frames** (`0x7F5 → 0xCD0`) against 1148. Round
2 is a time-up of **3280** frames (`0xEE9 → 0x1BB9`) against 3250. The run now reaches every mode
of the match end and game over except the return to mode 3 (the port is ~0x80 frames behind when
the script ends). Not explained, not guessed: the 95-frame longer round 1 and the 30-frame longer
time-up round (record §G.23 item 5 had 3300 vs 3250).

**The new first trace difference** is `f=0x871 (2161)` in `rng`: capture `6A6BD8FA`, port `AEE5C3BD`
(`T`/`S` equal through `0x870`; the port reaches `6A6BD8FA` at `0x872`, one frame later; the next step
`2B79ADE6` comes at `0x879` in the capture and `0x87B` in the port; `e2` first differs at `0x881`). A
throw-away `backtrace()` in `rng_next` (scratch only) names the port's draws: at iterations `0x86A`
and `0x872`/`0x87B`, `fight_4b144 ← fight_4aad0 ← fight_effects_pass ← game_mode_04_step` (ranges
`0x1200`, `2`, `0x1200`, `0x1200`), and at `0x877` the CPU's `ai_pick` (range `0x40`). So the same
type-0 crowd-effect walk record §G.20 named draws one frame late now. **Classification: a port
divergence in the fight-effect walk's pacing, not an unregistered callback (the miss log holds only
the harmless pairs), cause not isolated; owner: a fight-effects unit (not U6).**

**The frame claim** is unchanged (capture 203, divergence 1, owner U5). An observation outside any
claim: the port's frames from `f ≥ 0x77A` alone (a scratch symlink dump) are explained by capture
663..702 and capture 703 (raw 3784) is the first unexplained frame there, rows 88..192, x 0..133 (the
left fighter, in the round intro, mode 5) — reported for U5/U7, not ratcheted.

## §U6.9 What each post-port outcome means (U6a)

| observed after U6a Task 4 | meaning | action |
|---|---|---|
| trace F = 2161, N = 203, start 90, set = the 4 harmless pairs | the planner's measurement | re-pin F 2088 → 2161 |
| F > 2161 | the executor's base differs from `e9271df` in fight code | record the commit and the line; pin the measured F (it may only rise) |
| 2088 ≤ F < 2161 | a port differs from the record's code | stop: diff against the plan's code; do not pin |
| F < 2088 or N < 203 or start > 90 | a regression | stop; the ratchet fails as designed |
| round 1 not a KO at `0xCD0` | a port differs, or the base differs | record the path table; not a pin by itself |
| a new miss | a further unported target | name it in the record, pin it in `k_miss_gp_idle_loss` with its evidence (do not port it in U6a) |

## §U6.10 The move system (raw)

**Which table a side reads.** `0x3C600` (`hit_frame_desc`): with the side's slot `+0x63`
(`0x107813` for side 0) non-zero it uses `sel = 2`; else `sel` = the device word
`[DS_00101514]+0x2D4` (side 0) or `+0x2D6`, which is 0 for the keyboard (spec §3.2). The jump
table `0x3C5E4` = `0x3C64F, 0x3C6A0, 0x3C67A, 0x3C6A0, 0x3C64F, 0x3C6A0, 0x3C64F`: sel 0/4/6 read
`[eax*8+0xC6B9C]` (`0x3C66E`), sel 2 reads `0xC619C` (`0x3C699`). `fighter_command_generate` returns
at once when `+0x63 == 0` (it only writes the CPU's command), so **the keyboard player uses
`0xC6B9C` and the CPU `0xC619C`** (the port's names `HIT_TABLE_CPU`/`HIT_TABLE_PLAYER` are the other
way round; behaviour is right, the names are not). The reaction (`0x3CEAD`/`0x3CEEC`, the dword at
`0xC619E` >> 16) and the stance bytes (`0x3CD10`/`0x3CD29`, `0xC61A3`/`0xC61A2`) come from `0xC619C`
for both. Entry `(c, i)`: `(c*0x20 + i)*8`, `{u32 desc; u16 reaction; u8 d; u8 e}`.

**The phase machine** (`0x3C88C`, ported `hit_slot_step`): phases of `0x14` bytes, `m0 m1 m2` at
`+0/+4/+8`, the countdown word at `+0xC`. Phase 0 advances when one frame's command word covers
`m0`; phase 1 advances on the first frame `m0` is no longer whole; phases 2..7 accumulate `m0` over
frames within the countdown, reset on `m2`; the entry is armed (phase 8) when the next phase's `m0`
is 0; `hit_scan` (`0x3CD44`) takes the first armed entry whose stance byte allows the slot's `+0x54`
(`d`: stances 0/1, `e`: stance 2), and `0x3CE58` applies its reaction (0x10/0x11 become `0x3CBC4`/
`0x3CC58`'s variants 0x10/0x12/0x14/0x16 and 0x11/0x13/0x15/0x17).

**The command word** (`0x4F644`): `DS_001088E0` = newly-pressed P1 byte | held P1 byte << 8 (a new bit
is in both). P1 bits: `0x80` up, `0x40` down, `0x20` left, `0x10` right, `0x01..0x08` b0..b3.
`0x3C6E8` folds `0x10000/0x40000/0x20000/0x80000` into new-right/new-left/held-right/held-left with
facing 0 and the mirror with facing 1 (facing = `0x1A570`, the side's pset bit 15 clear,
`0x3CB89`). **Facing 0 for P1 at round 1 is measured** (§U6.12): a move whose last phase asks new
back succeeds with left pressed, never with right. A bit that changed this iteration keeps its old
level in `0x500C4` (spec §3.1), so a release must last 2 frames to reach the level. With a b0/b1 and
a b2/b3 bit held, `0x351DB..0x35201` skips the up-jump (`0x3BDDC`, ported
`fighter_attack_consume`), so a motion through up stays a motion.

**Character 0 (Sauron; the K11 option string at `0x80A60` lists `SAURON BLIZZARD TALON VERTIGO
ARMADON DIABLO CHAOS`; character 1 is "the white ape", record demo-pose §52-A)**, keyboard entries
(`gp_moves.py list --char 0`):

| i | reaction | d/e | callback | m0 per phase |
|---|---|---|---|---|
| `00` | `20` | 1/0 | `3D17C` | held back; held back; held fwd; new fwd + held b0 b2 |
| `01` | `24` | 1/0 | `3F0A8` (unported) | held down; held down; held up; new back + held b0 b2 |
| `02` | `26` | 1/0 | `3F3F4` | held up; held up; held down; new down + held b0 b2 |
| `03` | `28` | 1/0 | `3FB88` | held down; held down; new up; new down + held b0 b2 |
| `04` | `2A` | 1/0 | `3E3A8` | held fwd; held fwd; held back; new back + held b0 b2 |
| `05` | `2B` | 1/0 | `3E62C` | held down; held down; held up; new up + held b0 b2 |
| `06` | `2D` | 1/0 | `3D1EC` (unported) | held down; held down; new down; new up + held b0 b2 |
| `07` | `2E` | 0/1 (air) | `3F650` | as `02` |
| `08` | `2C` | 0/1 (air) | `3ECF8` | held b0 b2 |
| `13`–`16` | `3E 3F 3D 3D` | 1/0 | `3C0A4 3BF70 3C048 3C048` | back/fwd; down; up(-back/-fwd), any button resets |
| `17`–`19` | `32 33 34` | 1/0 | `37640 37774 37898` | the finisher starters |
| `1A`/`1C`/`1E` | `10` | 1/1 | — | new b0 b1 / held b0 new b1 / held b1 new b0 |
| `1B`/`1D`/`1F` | `11` | 1/1 | — | new b2 b3 / held b2 new b3 / held b3 new b2 |

`0x3BF70` (the 0x3F callback) starts the `0xC8B30` attack with state 3/4/2, the same state
`fighter_attack_consume` gives the up-jump: the `0x3D..0x3F` family is a down-up jump, not a throw.
**No throw entry is identified in character 0's table.** **The block** is not a table entry:
`0x1AB5C` (`fighter_input_mask`) arms `0x1A7CC` when the other side has a live reaction (`+0x5F !=
0xFF`) and the side holds away; it depends on the CPU attacking at that moment (§U6.12).

## §U6.11 The snapshot fields and the moves claim (U6b)

Five bytes appended to `SNAP_FIELDS` (and the port's `T` line, same order): `r0`/`r1` = `0x1088A8`/
`0x1088A9` (the last reaction each side applied: `0x34E2C`'s store at `0x34EF6`), `c0`/`c1` =
`0x10782A`/`0x1078BE` (slot `+0x7A`, the character), `s0_43` = `0x1077F3` (slot 0 `+0x43`, `0x1A6AC`'s
block bits `0x20/0x10`). `MOVE_FIELDS = (c0, c1, r0, r1, s0_43)` drive a third gp_compare claim,
`moves`, with its own ratchet (`--moves-min-first`), run only when asked or in report mode when the
capture records them; `TRACE_FIELDS` is unchanged, so `gp-idle-loss` prints identical lines
(measured: the report output of the base dump `diff`s empty against the old tool's).

## §U6.12 The moves list, the dry runs and the capture estimate

**The port dry run** (`gp_moves.py dry`): the `gp-idle-loss` capture's keys (the same menu path and
pick time-out) plus `bits` lines for the attempts, replayed by `PR_GP_DUMP` on a tree with the U6a
ports and the `T`-line fields. Measured facts: P1 `c0=00` (Sauron), P2 `c1=01` (Blizzard) at `f=0x7F5`;
facing 0 (the `0x24` attempt succeeds with left, never with right); a 1-frame release never reaches
the level (`e0` stays `0x1500` through it), hence `REPRESS = 2`; buttons held from offset 0 suppress
the jump (an up motion with buttons pressed late made P1 jump and fire the air entry `0x2C`).

The list: `1A, 00, 01, 1B, 06, 15` (reactions `10 20 24 11 2D 3D`) twice, 100 frames apart from
`f0 = mode-6 start + 10 = 0x7FF`, step 4 (harness values). Dry run on the U6a tree with every U6b port
(`gp_moves.check` on the port's `T` lines): `1A` performed at `0x800`, `00` not (P1 in hit-stun), `01`
`0x8D0`, `1B` not, `06` `0x998`, `15` `0x9FC`, `1A` not, `00` `0xAC4`, `01` `0xB28`, `1B` `0xB84`, `06`
not, `15` `0xC54`: every move at least once. On the tree U6b Task 5 runs (U6a plus the snapshot bytes
and the scenario's pinned set, no U6b port) the same script gives 7 of 12: `1A` `0x800`, `01` `0x8D0`, `1B`
`0x92D`, `06` `0x998`, `15` `0x9FC`, `06` `0xBF0`, `15` `0xC54`; `00` (`0x20`) is not shown in either window
there (the unregistered callbacks change the fight after `0x8D0`). The CPU interrupts others; in the original the CPU's
choices differ from the port's after the rng divergence, so the capture's own `check` decides.

**The misses the list reaches in the port** (U6a tree): `0x3F0A8`, `0x3D1EC`(2), `0x3C048`(2). With
those ported: `0x3F0F0` and `0x3F130` (`anim_indirect`), then (a list variant) `0x3A820`
(`fighter_state_3531c`, 330 hits); an earlier list variant also reached `0x231C0` (the CPU's
character-1 reaction 0x27). With all of §U6.13–§U6.18 ported the dry run's miss log is the 4
harmless pairs (`distinct=4`).

**The capture.** The scenario ends 1200 frames after the first attempt (`f = 0xCAF`). From the
`gp-idle-loss` capture's timing: 2136 capture frames up to `f = 0xCAF`, **103 MB** of
`frame_*.raw.gz` (plus `poll.log` ~1 MB), about 50 s of play after the MAIN MENU plus the 25 s boot
wait (time limit 130 s, a harness value). The port replay dumps 1742 frames (~113 MB of `.ipx`).

## §U6.13 `0x3F0A8` and the three callbacks it stores

`0x3F0A8` (references: the dwords `0xA37F8` and `0xA380C`, character 0 reactions `0x24` and `0x25`):
`ebx = slot; eax = rec; edx = 0xE7B78; push 3.0; call 0x3C4CC; call 0x2C3FC(0x8C); slot +0x52 = 9,
+0x53 = 7, +0x54 = 0, +0x0C = 0x3F054, +0x57 = 0, +0x18 = 0x3EFE0, +0x1C = 0x3F020; al = 1`.
`0x3EFE0` (`+0x18`, fn(side), EAX returned): `ctx_same`; `0x18BD4(flags)`; flags 1, 8 = 0, 0 = 1;
`0x18C14(ctx[0], flags, 0, 0)` — byte-for-byte `0x3E484`'s body. `0x3F020` (`+0x1C`, fn(side)):
`ctx_same`; `0x3B714(ctx[3], ctx[2])`; word `0x1080A4[ctx0] = 0`; `ctx[2]+0x57 = 1`. `0x3F054`
(`+0x0C`, case 7; reads EBX only): `ctx_same(side)`; unless `ctx[2]+0x57 == 1` return; `inc word
0x1080A4[ctx0]`; if the signed word > 10: `0x2BC30(ctx[4], 0xE7BBE, 2.0)`, `ctx[2]+0x57 = 2`. All
callees ported (`0x18BD4` static in `fighter.c`, `0x18C14`, `0x3B714` `fighter_reaction`, `0x2BC30`).

## §U6.14 `0x3D1EC`

Reference `0xA38AC` (character 0, `0x2D`). `ebx = slot; eax = rec; edx = [0xC8CC0]` (= `0xE70CC`);
`push 5.0; call 0x3C4CC; slot +0x52 = 9, +0x53 = 8, +0x54 = 1; al = 1`.

## §U6.15 `0x3C048`

References: the seven dwords `0xA39EC + c*0x500` (every character's `0x3D`). `ecx = slot, esi = rec;
ctx_same(side); r = 0x3BF70(slot, rec, side)`; if r: `ctx[2]+0x4E word = 0`; the side's record
(`[0x1077B0 + ctx0*0x94]`) `+0x34 word = 0, +0x43 = 0, +0x42 = 0`; returns r. `0x3BF70` already
clears the same record's three fields (`check_reaction_attack` block A in `port/tests/test_fight.c`
asserts them 0 after `fighter_3bf70` alone), so deleting those three
writes is an **equivalent mutant on every seed the tests use** (measured: mutation N11 survives);
the port keeps them because the raw makes them.

## §U6.16 `0x231C0`

Reference `0xA3D34` (character 1, `0x27`). `0x23130`'s shape: `ctx_same`; `+0x52 = 9`, `+0x53 = 7`,
`+0x54 = 0`, `+0x0C = 0` before `0x3C4CC(rec, 0xE48EE, 3.0)`; voice `0x7C`; `al = 1`.

## §U6.17 `0x3F0F0`, `0x3F130` and `0x2BEF4`

`0x3F0F0` (the dword at `0xE7B8C`, after a `0xD100` word, in `0x3F0A8`'s stream) and `0x3F130`
(`0xE7BC6`, in the `0xE7BBE` stream): with `rec+0x14` set, `0x2AE14(0xBB290, EDX = 8, ECX = 0,
EBX = -0x5A, a5 = rec+0x56 | 0x400)`, child `+0x14 = slot` (`0x3F130` also `+0x50 = 2` first), then
`0x2BEF4(child)`. `0x2BEF4` is `or byte [eax+0x2b],1; ret`, called only from `0x3F125` and `0x3F169`;
not a Ghidra function.

## §U6.18 `0x3A820`

The fourth case-10 sibling (reference: `0x3A921` in the setter `0x3A8E8`, ported as
`fighter_pose_3a8e8`). `0x3A588`'s body with the table `0xC9058` (`[3] = 0xD26F0`, first word
`0x17E8`), A = word `0x107CF8 + side*2` (`[edx*2+0x107cf6] >> 16`), B = word `0x107CFC + side*2`,
the jump table `0x3A810` (`0x3A8D6` ×4) and `+0x90 = 4`.

## §U6.19 Named gaps

1. **Divergence 2b** (§U6.8): the crowd-effect walk draws `rng` one frame late at `0x871` (owner: a
   fight-effects unit).
2. **Round lengths** after U6a: round 1 1243 vs 1148 frames, round 2 time-up 3280 vs 3250 (§U6.8).
3. **Divergence 1** (capture 203) stays U5's; the round-intro frame observation (capture 703) is
   reported for U5/U7.
4. **E1 cannot verify** `0x23208`, `0x3A588`, `0x3F0A8`, `0x3C048`, `0x231C0`, `0x3A820`, `0x3F054`,
   `0x3F020`, `0x3EFE0`, `0x3F0F0`, `0x3F130`, `0x3D1EC` (closures with indirect transfers or
   stack-argument callees; record E1 §E.6). `0x3640C`, `0x37DCC`, `0x2BEF4` are leaves (the first two
   are verified in U6a; `0x2BEF4` is static and has no binding).
5. **The block** (`0x1AB5C`/`0x1A7CC`) needs the opponent's attack at a known frame: owner U7 (two
   players: P2 scripted to attack while P1 holds back). **No throw** entry is identified (§U6.10).
6. **The equivalent mutant** of §U6.15.
7. **The port's table names** `HIT_TABLE_PLAYER`/`HIT_TABLE_CPU` are swapped relative to their use
   (§U6.10); not renamed here (another module's file, no behaviour change).
8. The counters do not move: none of the U6 functions is in `port/decomp/prage.functions.csv` /
   `symbols.h` (`port_progress.py` prints `771 1203 64` and `731 731 100` with all of them ported,
   measured in the scratch trees). Live Ghidra lists only `0x23208` among them.

## §U6.20 Verified by running (planner)

The image dump and its sha; every disassembly above (capstone over the image); the dword and
rel32 reference scans; the live Ghidra queries for `0x23208/0x3A588/0x3640C/0x37DCC`; the seven
replays of §U6.7 and their `gp_compare --report` lines; the backtrace of §U6.8; the U6a code and
tests (`run_tests` `all checks passed`; fifteen mutations each fail, record of the plan); the
diff_verify specs (`6/6 VERIFIED; 7/7 mutants`; `test_diff_verify` OK); the U6b code (eleven
functions, six test functions, twenty-one C mutations of which one equivalent); the tool changes
(`test_gp_session`, `test_gp_compare`, `test_gp_capture`, `test_gp_moves` `OK`, seven Python mutations
each fail, `PYTHONDONTWRITEBYTECODE=1`); the scenario schedule simulation (first press `0x7FF`, end
`0xCAF`); the dry runs and their checks; the capture-size estimate; `port_progress.py`.
