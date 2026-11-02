# Derivation record — Volleyball truth (mode `0x21` route search and exhibit)

Phase: Volleyball truth (register R05).
Base commit: `edbc35de`.
Design: `docs/superpowers/specs/2026-11-01-volleyball-truth-design.md`.
Plan: `docs/superpowers/plans/2026-11-01-volleyball-truth.md`.
Status: **complete** (Tasks 1–5; R05 `closed`; the full gate passed
2026-11-01).

Goal: one of two measured outcomes for R05 — **closed** by one guarded capture
of a real-input two-human stage-0 match whose replay reaches `entry+0x1F == 8`
and the mode-`0x21` entry, exhibits the court, a point and the return to mode 6,
with a checker, oracle and pins; or **declared with a measured bound** — the
search's parameter space, the best state it reached and the reason no route was
found. A port-side dump alone is not a capture and publishes nothing.

Sources: the live Ghidra MCP instance (project `primal-rage`, program
`/PRAGE.EXE`), re-read for this record on 2026-11-01; `port/src/game/fight.c`
and `port/src/game/flow.c` are cross-checks only. On any plan/brief-vs-raw
conflict the raw wins; the raw-wins corrections are §1.4 (the `0x4B7DC`
equality and the `0x4B7A0` `> 1`) and §1.5 (`0x4CA59`). Addresses are linear
(Ghidra/LE); a `DAT_0008xxxx` in an un-fixed-up raw listing is the runtime
`0x1xxxxx` after the LE fixups (`mem_load_le_fixups`), and the port's
`DS_xxxx` labels are the runtime linear addresses.

## 1. The spec corrections

This section is the authoritative raw source for the corrections Task 1 made to
`port/spec/volleyball.md`; the record is not a copy of the report's prose.

### 1.1 `entry+0x10` is the shadow actor

`+0x10` is the entry's **second actor**, the shadow, not a held body.

- `0x4B470` spawns it once: `0x4B4B6 MOV EDX,[ESI+0x10]` / `0x4B4B9 TEST EDX,EDX`
  / `0x4B4BB JNZ 0x4B4DA`; when zero, `0x4B4BD..0x4B4D2` reads the actor's x
  (`0x18`) and y (`[0x30] >> 16`), spawns from descriptor `0xBB920[index]`
  (`0x4B4D2 CALL 0x2AE14`) and stores the result at `0x4B4D7 MOV [ESI+0x10],EAX`.
- It is killed at `0x4B84E` (the grab), `0x49F52` (the fight's type-6 landing)
  and `0x4C273` (the mode-`0x21` landing).
- The mode's type-6 body and shared tail copy the ball actor's x and y to it:
  `0x4C159..0x4C162` (`held+0x18 = rec+0x18`) and `0x4C165..0x4C16F`
  (`held+0x32 = rec+0x32`), and the shared tail repeats at `0x4C231..0x4C247`.
- The real held body is the *entry actor's* `+0x4A` link plus `entry+0x1C`
  bit 6: `0x4B840 OR AL,0x40` / `0x4B845 MOV [ECX+0x1C],AL`;
  `0x4B94A MOV [EAX+0x4A],DL`; `0x4B956 CALL 0x2BD20`; cleared by the strike
  release at `0x4C718`/`0x4C727` and by `0x4B69C`'s release at `0x4B751`/`0x4B760`.

Port cross-check: `fight_4b788` (`fight.c:2861`), `fight_4b69c`
(`fight.c:3101`), `fight_4c60c` (`fight.c:4284`), `fight_4bf18` (`fight.c:4477`).

### 1.2 `+0x34`/`+0x36` are the actor's vx/vy words

The effects entry is 0x24 bytes (`fight_list_init`'s pool loop,
`fight.c:56`); `entry+0x08` is its actor record (`0x4B478 MOV EAX,[EAX+8]`,
`0x4B52B MOV EAX,[ESI+8]`). `+0x34`/`+0x36` are the **actor's** vx/vy integer
words, not entry fields:

- `0x4BEDA`/`0x4BEE5` set `word [court+0x36] = 0x1A4` for the two court actors.
- `0x4BF26` tests `word [wa+0x36]`; the preamble's distance is
  `0x4BF2D MOV EDX,[EAX+0x34]` / `0x4BF30 SAR EDX,0x10`, i.e. the **word at
  `+0x36`** (the high half of the dword at `+0x34`), not the word at `+0x34`.
- `0x4CB63..0x4CBB5` sets the ball actor's `+0x34`; `0x4CB84..0x4CBF0` sets
  its `+0x36`.
- `motion_step` (`0x2A4FC`) adds `DSD(rec+0x32)>>16` to x and
  `DSD(rec+0x34)>>16` to the height; the high words are `+0x34` and `+0x36`.
  Port: `actors.c:2872-2873`.

The old spec's `+0x34>>16` in the preamble was therefore the wrong field; the
raw reads `+0x36` (the high half of the `+0x34` dword).

### 1.3 `0x4B5A8` is the walk-in spawner, not the lie

`0x4B5A8` is the type-0 crowd body's walk-in spawner, called from `0x4AAD0` at
`0x4AB45`. It does **not** write `entry+0x1C`.

- Gate: `0x4B5AC MOV EAX,[EAX+0xC]` / `0x4B5AF MOV ECX,[EAX]` /
  `0x4B5B1 MOV CL,[ECX+0x51]`; `0x4B5BA CMP byte [ECX+0x1088B6],0` /
  `0x4B5C1 JNZ 0x4B5D7`; else `0x4B5C3 CMP byte [EAX+0x52],7` and
  `0x4B5CD CMP byte [EAX+0x53],2`, both false → `0x4B694` (return 0).
- Body: `0x4B5D7` stream `0xC95BC[index]` at 3.0 (`0x4B5E6 CALL 0x2BC30`);
  `0x4B5F2 MOV [EBX+0x1A],AX` from `actor+0x32`; `0x4B5F9` `actor+0x38 =
  0xFFC0`; `0x4B614`/`0x4B62F` `actor+0x34 = ±0x40`; `0x4B627`/`0x4B642`
  the fighter's x ±0x1800; `0x4B650 MOV [EBX+0x14],EAX`; `0x4B656 MOV
  byte [EBX+0x1E],3`; `0x4B68B` decrements `DS_001088B6`.
- The budget is the per-side combo: `0x39056 CMP EDX,1` / `0x39059 JLE` and
  `0x39073 MOV AL,[EAX+0x107D2C]` / `0x3907B MOV [EBX+0x1088B6],AL`. The
  index at `0x4B5B1` is the slot actor's `+0x51` player-index byte.
- The dizzy alternative is `slot+0x52 == 7 && slot+0x53 == 2`, set by
  `0x36C8F MOV byte [ECX+0x52],7` and `0x36C96 MOV byte [ECX+0x53],2`.
- The **lie** is the effects pass's case-3 landing: `0x49E47 MOV byte
  [ECX+0x1E],4`; `0x49E3A CALL 0x5D7DC` / `0x49E3F ADD EAX,0x3C` /
  `0x49E4E MOV [ECX+0x18],AX` (the 60..119-frame timer); `0x49E4B OR DL,0x80` /
  `0x49E52 MOV [ECX+0x1C],DL` (the latch). It clears on the type-4 rise:
  `0x49EB5 AND AH,0x7F` / `0x49EB8 MOV [ECX+0x1C],AH`.
- On the mode's ball side the same bit is set by the type-6 body
  `0x4C14F` (`OR byte [entry+0x1C],0x80`) and the shared tail `0x4C227`
  (`OR byte [entry+0x1C],0x80`); bit 7 is therefore not only the flight latch.

Port cross-check: `fight_4b5a8` (`fight.c:2509`), `fighter_39040`
(`fighter.c:3411`), `fighter_state_36bc8` (`fighter.c:2703`).

### 1.4 `0x4B788`'s return-1 conditions (the brief's `!=` and `<=` were inverted)

`0x4B788` returns **1** (the caller tramples) in three arms, and **0** is the
grab arm:

- `0x4B796 XOR EAX,EAX`; `0x4B798 MOV AL,[0x00105b3a]`;
  `0x4B79D CMP EAX,0x1`; `0x4B7A0 JG 0x4B99E` — `DS_00105B3A > 1` returns 1.
- `0x4B7C0 XOR SI,0x1` (`SI = hit-1`, so this is the **other** side);
  `0x4B7DC CMP byte ptr [EAX+0x54],0x3`; `0x4B7E0 JZ 0x4B99E` — the other
  slot's `+0x54 == 3` returns 1.
- `0x4B7E6 MOV DL,[EBX+0x7A]`; `0x4B7EF CMP AL,byte ptr [ESI+0xC97F2]`;
  `0x4B7F5 JNZ 0x4B99E` — the fighter's `+0x5F` not equal to his character's
  grab-move id (`0x2D` for characters 0..6) returns 1.
- `0x4B994 XOR EAX,EAX ... RET` returns 0 (the grab); `0x4B99E MOV EAX,0x1 ...
  RET` returns 1.

The brief's item 4 wrote "other `+0x54 != 3`": the raw returns 1 on **equality**
with 3 (`0x4B7DC`/`0x4B7E0`). The brief's "`DS_00105B3A <= 1`" is not a
return-1 condition: `> 1` itself returns 1 (`0x4B7A0`), and `<= 1` is the path
that reaches the grab test. The spec states the raw conditions; the grab arm
still attaches the body (`0x4B845`/`0x4B94A`/`0x4B956`) and counts nothing.

### 1.5 The `+0x1F` zero list

`entry+0x1F` is zeroed at these sites:

| addr | instruction | context |
|---|---|---|
| `0x49622` | `MOV byte [ESI+0x1F],0` | `0x494A8`, the dust/effects builder |
| `0x4D061` | `MOV byte [ESI+0x1F],0` | `0x4CF20`, the mode-`0x23` six-entry sibling |
| `0x49FAF` | `MOV byte [ECX+0x1F],0` | `0x49C78`'s type-6 landing (the fight tumble) |
| `0x4C1E8` | `MOV byte [ECX+0x1F],0` | `0x4BF18`'s type-6 body landing |
| `0x4C27F` | `MOV byte [ECX+0x1F],0` | `0x4BF18`'s shared-tail ball landing |
| `0x4C349` | `MOV byte [ECX+0x1F],0` | `0x4BF18`'s 3-point landing end |
| `0x4C904` | `MOV byte [EAX+0x1F],0` | `0x4C784`, the eat path's 3-point end |
| `0x4CA59` | `MOV byte [ESI+0x1F],0` | `0x4C784`, the eat path's new-ball spawn |
| `0x4E4E5` | `MOV byte [ESI+0x1F],AH` | `0x4E350`, the ending audience spawn |

`0x4E4E8` is `MOV byte [ESI+0x21],AH` (the same spawn's `+0x21`), **not** a
`+0x1F` site. The brief's `0x4CAA9` is a transcription error: the live Ghidra
listing has `0x4CA59 MOV byte ptr [ESI+0x1f],0x0`, while `0x4CAA7 MOV byte ptr
[EAX+0x4e],0x1` is followed by `0x4CAAB MOV EAX,dword ptr [ESP]`; there is no
instruction boundary at `0x4CAA9`. The port cites `0x4CA59` at `fight.c:4248`.

### 1.6 Route B: the mid-round join keeps the 1P stage

- The 1P pick: `0x2588C MOV EAX,dword ptr [EAX+0x108166]` (EAX = `b1f`) /
  `0x25892 SAR EAX,0x18` reads the signed character byte at `0x108169 + b1f`
  (for `b1f == 1`, `DS_0010816A`). `0x25897` reads the base table
  `0xA87C4[c]`; the data bytes at `0xA87C4` are `00 04 02 03 06 01 05`, i.e.
  `{0,4,2,3,6,1,5}`. `0x258A2..0x258AA` stores
  `rng(6) + base + 1`, and `0x258B8..0x258C6` reduces it by 7 once when
  `>= 7`. Character 0 (base 0) can never give 0; each of characters 1..6 has
  one `rng(6)` value that does (the value 7 before the reduction).
- `flow_player_join` (`0x28DA4`) sets `DS_00104B17 = 2` at
  `0x28E6D MOV byte ptr [0x00104b17],DH` (DH = 2 at `0x28E58`), and
  `DS_00104ABC = (b1f == 3) + 1` at `0x28DE7 MOV [0x00104abc],EAX`
  (`0x28DD1 MOV AL,[0x00104b1f]`; `0x28DD6 CMP EAX,0x3`; `0x28DD9 SETZ AL`;
  `0x28DE1 INC EAX`).
- The join poll `0x28CC8` writes the joining side's bit into `DS_00104B1F` at
  `0x28D02..0x28D13`: `0x28D02 MOV AL,CL`; `0x28D04 MOV DL,[0x00104b1f]`;
  `0x28D0A INC AL`; `0x28D0C OR DL,AL`; `0x28D13 MOV [0x00104b1f],DL`.
- The pick returns on `m == 2`: `0x2585D CMP AL,0x2`; `0x2585F JZ 0x259C3`
  (the common return at `0x259C3`). The join's vs tail calls the pick at
  `0x430FE`, but with `m == 2` it writes nothing, so the 1P stage word
  survives. The fight then resumes with `b1f = 3` and `DS_00104ABC = 2`
  (the `0x4BD98` gate).

Port cross-check: `flow_stage_pick` (`flow.c:1343`), `flow_join_poll`
(`flow.c:1683`), `flow_player_join` (`flow.c:1624`).

### 1.7 The window convention: `T+37`, `T+38`, `T+65..T+71`

`T` is the trample pass. After `0x4B470` stores `vy = 0x240` (`0x4B52B`/
`0x4B52E`), type 6 (`0x4B537`) and clears the latch (`0x4B544`), the same-pass
case-6 body subtracts `0x10` (`0x49FB8`), and `motion_step` adds the result.

- The latch (`entry+0x1C` bit 7) is **set at `T+37`**, the first `vy < 0` pass:
  `0x49F11 CMP word [EBX+0x36],0`; `0x49F16 JGE`; `0x49F18 OR byte
  [ECX+0x1C],0x80`.
- The prelude `0x4B69C` runs at the top of the next effects pass (`0x49CFE`,
  before the type dispatch at `0x49D03`), so it **sees the latch from `T+38`**.
- The 35 `vy < 0` passes are `T+37..T+71`; the prelude only sees them from
  `T+38`.
- The **practical body-contact window** is `T+65..T+71` (7 passes) from the
  56 px (`0x38`) fighter box (`0x1790C`): `h(64) = 3,584` equals the box and
  `h(65) = 3,120` is the first height strictly inside it. **Read the label as
  the motion-step index**: the prelude at pass `T+k` runs before the type
  dispatch, so it evaluates the height left by the `k-1`th motion step; the
  same height-only window is therefore passes `T+66..T+72`, one pass later
  (the latch is already stated the same way: set at `T+37`, seen from
  `T+38`). The label is **approximate**, not a per-hit guarantee: the contact
  also depends on the fighters' horizontal boxes and the attack's active
  frame, and the eight measured hit frames' gaps from the previous hit are
  66, 72, 68, 70, 68, 78 and 63 (Volleyball truth Task 4, `gp-u30-volleyball`;
  record §2.3). The measured hit frames are the authority.
- The ~142 px separation is the zero-`+0x42` throw: 71 motion steps × `0x80`
  units = 2 px/frame × 71. `motion_step` adds the record's `+0x42` each step
  and clamps by `+0x43` (`actors.c:2874-2879`); a non-zero `+0x42` lengthens
  the throw (record §F6.4). The four measured hit actors read `+0x42 = 0`.

### 1.8 The measured boundary (`gp-idle-loss`, `f=0x8BE`, the demo)

- `gp-idle-loss` is **8,173 presented frames**
  (`tools/capture_manifest.json`'s `frame_count`); the counter's last `S`
  record is `f=0x2681` = 9,857. Record §F6.7's "2,565 frames" is superseded.
- Its two `0x4B470` calls are both first counted hits (`+0x1F = 1`, `via=1`,
  mode 6) at trace `f=0x8BE` = 2,238 decimal (the record's `f=2238` is
  decimal, not hex), on distinct lying entries `0x108414` and `0x108438`
  (type 4; `entry+0x1C = 0x80`; stage word 4).
- The demo's two calls are at stage 0. The demo's stage 0 is the boot default:
  `0x20C10`'s `0x20CDF MOV word ptr [0x84AFC],BX` with `BX = 0` (pre-fixup;
  the fixup makes it `DS_00104AFC`), not a `flow_stage_pick` result.
- The other measured fights (`gp-twop`, `gp-twop-ko`, `gp-draw-timeout`,
  `gp-keys-fight`) call `0x4B470` zero times and never sample an entry with
  bit 7 set (record §F6.7). The count is therefore measured only at 1;
  `+0x1F > 7` is derived, not measured.

### 1.9 `0x1A570`'s "faces right" is a reading

`0x1A570` tests only the pset word's bit `0x8000`:
`0x1A593 SHL EAX,0x5`; `0x1A596 MOV AX,[EDX+EAX]`; `0x1A59C AND AH,0x80`;
`0x1A5A4 SETE AL`. The spec reads `0x1A570 == 1` as "fighter 0 faces right"
(the usual pset-hflip convention) only as a reading; the port does not
independently measure which screen direction the bit means. §7 of the spec
therefore flags the "whose half" sentence as a reading of the raw's XOR, not
an exhibited claim.

### 1.10 Ghidra re-confirmation of the two corrected addresses

Re-read 2026-11-01 from the live Ghidra MCP instance (project `primal-rage`,
program `/PRAGE.EXE`) with `disassemble_function` on `0x4B788` and `0x4C784`,
`decompile_function` on both, and `disassemble_bytes` on `0x28D02..0x28D1E`.
The listing is the authority for §1.4 and §1.5:

```
0004b796 XOR EAX,EAX
0004b798 MOV AL,[0x00105b3a]
0004b79d CMP EAX,0x1
0004b7a0 JG 0x0004b99e              ; DS_00105B3A > 1 -> return 1
...
0004b7dc CMP byte ptr [EAX + 0x54],0x3
0004b7e0 JZ 0x0004b99e              ; other slot +0x54 == 3 -> return 1
...
0004b7ef CMP AL,byte ptr [ESI + 0xc97f2]
0004b7f5 JNZ 0x0004b99e             ; +0x5F != 0xC97F2[ch] -> return 1
...
0004b994 XOR EAX,EAX                ; the grab arm returns 0
...
0004b99e MOV EAX,0x1                ; the trample arm returns 1
```

```
0004ca59 MOV byte ptr [ESI + 0x1f],0x0   ; the eat path's new-ball spawn
0004ca5d ADD EDX,EAX
...
0004caa7 MOV byte ptr [EAX + 0x4e],0x1   ; 0x4CAA9 is inside this instruction
0004caab MOV EAX,dword ptr [ESP]
```

The decompiler agrees: `FUN_0004b788` returns 1 when `DAT_00105b3a >= 2` and
when the other slot's `+0x54 == 3`; `FUN_0004c784` has
`*(undefined1 *)((int)piVar10 + 0x1f) = 0;` at the new-ball spawn. No
instruction exists at `0x4CAA9`; the store the brief called `0x4CAA9` is
`0x4CA59`. The spec and the Task 1 report use `0x4CA59`; this record is the
raw source for the correction.

## 2. The route (2026-11-01, port-side search)

A deterministic port-script v2 route — deterministic at the pinned boot frame,
§2.5 — reaches `entry+0x1F == 8` and mode `0x21` on stage 0 with a two-human
match. The winning script is
`.superpowers/sdd/2026-11-01-volleyball-truth/route.script` (a copy of the
port's run; the mode's end at `f=0x15B9`, the search script's end at 7000).
The capture's generated script is the same route with two differences: it
omits the route's null `bits 3781 0020` / `bits 3781 0000` pair (two writes in
one port iteration, net 0; a capture cannot make a same-frame press+release)
and ends at the capture's X record, `f=0x160C` (5,644). Every frame below is
the port's `DS_000EF6DC`; the states are the `T`/`V`/`Vg` lines.

### 2.1 Stage 0 by the mid-round join (route B)

- The 1P character select: one `p1.right` (pad `0x1000`) at `f=716`, one
  `p1.b0` (`0x0100`) at `f=776`. The cursor moves from 0 to 1; the confirm
  stores `DS_0010816A = 0xC8882[1] = 2` (`0x43D6E/0x43D74`), character 2.
- The 1P stage pick at the vs: `0xA87C4[2] = 2`, `rng(6) = 4` →
  `2 + 4 + 1 = 7`, reduced by 7 → **0** (`0x25897..0x258C6`). The route's
  first 1P-fight `T` is at `f=0x444` — its mode word is **0x05** (the round
  start, `0x25C88`; the vs itself is mode 0x11 at `f=0x32E`, and the
  mode-0x06 `T` begins at `f=0x4BF`) — not the vs, and both read
  `afc=0000`. The `f=0x7F5` sample is the no-select base run's mode-0x06
  `S`, not this route's fight start.
- The join: `p2.start` (pad `0x0001`) at `f=2097` (`0x831`). It is **not**
  "60 frames into the 1P fight": from the port's first 1P-fight `T` at
  `f=0x444` the join is **1,005** frames (`0x831 - 0x444 = 0x3ED`); from the
  port's mode-0x06 `T` at `f=0x4BF` it is **882** (`0x831 - 0x4BF`); the
  capture's own `S` mode 0x06 at `f=0x4BF` reads the same 882 to the join's
  `S` at `f=0x831`. So the port timeline is 1,005 from its first fight `T`
  and 882 from mode 0x06, while the capture's `S` timeline is 882 — the two
  clocks start at different mode points, not at different game states.
  `flow_player_join` sets `DS_00104B17 = 2` (`0x28E6D`); the join's
  select/vs calls `flow_stage_pick` (`0x430FE`), which returns on `m == 2`
  (`0x2585D/0x2585F`) without writing. The fight resumes with `b1f=03`,
  `afc=0000`, `DS_00104ABC=2` (`0x28DCF..0x28DE7`).
- The join's character select (`mode 0x10` at `f=0x8CF`): `p1.b0` at `f=2372`,
  `p2.b0` at `f=2402`; the vs at `f=0x988`; the two-human fight resumes at the
  capture's `S` mode 0x06 at `f=0xB19` (the port's `T` mode 0x06 also enters
  at `f=0xB19`; the port's `f=0xC00` is a later mode-6 `T`, not the entry).

### 2.2 The crowd lie

The two-human fight runs from the capture's `S` mode 0x06 at `f=0xB19`
(alongside the port's `T` mode 0x06 at the same frame; the port's `T` at
`f=0xC00` is a later mode-6 frame). In it:

- The P1 walks in with the P2 (`p1.right | p2.left`, pads `0x1020`, 30 frames
  from `f=3197`) and lands two `p1.b0` hits on the P2 at `f=3257`/`f=3277`
  (the P2's `+0x5A` 7 then 10; the P2's reaction state `0x10`).
- At the P2's neutral tail the combo budget is copied
  (`0x3907B`: `0x107D2C[0] = 2` → `DS_001088B6[0] = 2`); the side-0 crowd
  (`0x4B5A8`) walks in at `f=3320` (entries `0x1083CC`/`0x1083F0`, type 3).
- The case-3 landing lies both at `f=3331`/`f=3333`: type 4, `+0x1C = 0x80`,
  the `rng(0x3C)+0x3C` lie timer. Entry `0x1083CC` is at `x=-0x109B`,
  `0x1083F0` at `x=-0xC3F`; the P1 at `x=+0xE0`.

### 2.3 The eight tramples

The first hit is on the lying body (pre-hit `h=0`); the later hits are on the
descending body with the fighters' boxes. The P1 (TALON, char 2) uses `b2`
(`0x0400`) for the lying body and `b3` (`0x0800`) for the airborne one; the P2
(ARMADON, char 4) uses `b0` (`0x0001`). The attacks' active frames are the
measured hit frames; the body's vx flips on each hit, so the two fighters
stand at the two ends (~142 px apart) and alternate. The `h` column is the
**V line's height at the hit frame** (post-launch; the same-pass case-6 body
subtracts `0x10` and `motion_step` adds the result, so hit 1's first step is
`0x230`, not the pre-hit 0), one instant for all eight rows; hit 8's V line is
the mode's `0x4CB18` launch (`fl=0x20`), so its vx is the mode's, not the
trample's.

| hit | side | input (frame, pad) | hit frame | V line at the hit frame (entry, n, vx, h) |
|---|---|---|---|---|
| 1 | P1 | `p1.left` from 3331 for 20, `p1.b2` 3351 | `0x0D1F` (3359) | `0x1083F0`, n=1, vx=+0x80, h=0x230 |
| 2 | P2 | `p2.left` from 3359 for 28, `p2.b0` 3418 | `0x0D61` (3425) | n=2, vx=-0x80, h=0xC80 |
| 3 | P1 | `p1.b3` 3484 | `0x0DA9` (3497) | n=3, vx=+0x80, h=0xA40 |
| 4 | P2 | `p2.b0` 3558 | `0x0DED` (3565) | n=4, vx=-0x80, h=0x10A0 |
| 5 | P1 | `p1.b3` 3620 | `0x0E33` (3635) | n=5, vx=+0x80, h=0x12D0 |
| 6 | P2 | `p2.b0` 3696 | `0x0E77` (3703) | n=6, vx=-0x80, h=0x1930 |
| 7 | P1 | `p1.left` from 3758 for 8, `p1.b3` 3766 | `0x0EC5` (3781) | n=7, vx=+0x80, h=0x820 |
| 8 | P2 | `p2.b0` 3836 | `0x0F04` (3844) | n=8, vx=FF92 (the mode launch), h=0x175A, mode 0x21 |

The P1's b3 and the P1's left steps compensate for the b3's forward step
(the P1 drifts +20 px over two b3s) and the body's height accumulation; the
7th hit's margin is one port frame, so the capture must reproduce the input
ticks exactly.

### 2.4 The mode and its measured content

At `f=0x0F04` the `0x4B5A8` tail calls `0x4BD98` and `0x4CB18`:
`T mode=0021`, `Vg ball=001083F0`, `abc=00000002`, `clk=001E`,
`c1=01`, `c5=01`; the entry becomes the ball (`ty=06`, `+0x1C = 0x20`), the
two court records spawn. The mode runs with the players idle:

- the clock `DS_001088AA` 30 → 0 over 1,717 frames (57 per unit);
- a point: the score `DS_0010889D` becomes 1 (`s1=01` by `f=0x1000`), the
  point pause `DS_001088AC = 0x69`, then the next serve;
- at `f=0x15B9` the clock reaches 0 and the mode returns to `mode=0006`
  (the `0x4C429` timeout end; `0x4CC0C`/`0x4CD98`); the score clears and the
  ball is gone.

The frame mapping (Task 4's replay, measured; report §5): the port's entry
frame 2,289 (`f=0x0F04`) is capture **2,734** and its return frame 3,854
(`f=0x15B9`) is capture **4,543**; both are clean matches (a capture frame
equals a port frame), 97 capture frames before the first unexplained 4,640.

Before this run the mode's own pass, its court pixels, its score text and its
strings had never been captured; the capture's pixels are the first exhibit.

### 2.5 The boot pin

The scenario's first step is `at_frame 320`, not the wall `boot`. The measured
gp-u15-join boot press is the spin of `f=319` (its `I` record `f=0x13F`; the
mode 0x27 appears at `f=321`); `at_frame 320` makes that press whatever the
guest's wall-clock boot time, and the generated port script still reads
`enter_frame 321` (`key 321 1C 0D`). The route is therefore **deterministic at
the pinned boot frame**: a port sweep shifting the whole record by ±1..±24
frames reached the mode only at shift 0 and at two coincidences, -5 and -24
(every other shift missed the seventh trample or never opened mode 0x21). The
capture's boot measure is `I ... f=0x13F press=enter ... late=0` with the
generated script's `enter_frame 321` — the pinned frame. The PinGuard fails a
run at once if S(319) is missed, so a shifted route cannot publish.

## 3. The close-out (Task 5)

R05 moves from `declared` to `closed`; the evidence is §2's capture and its
oracle. This section is the measured record of the counters the phase moved and
the gate it passed.

### 3.1 The register row

`docs/superpowers/specs/2026-10-30-final-claims.md` R05: status `closed`;
evidence `data/k11-captures/gp-u30-volleyball` (4,704 frames, every session
check ok, 22/22 steps, zero pokes, `poll.log` sha256 `42e131d1…`),
`gp-u30-volleyball-oracle` (N 4,640, trace/moves 5,645, start 104, reach 3,936,
13 missing classes), `tools/gp_volley.py check`, this record §2 and
`port/spec/volleyball.md`. Reason: the eight counted tramples, the mode entry at
`f=0x0F04`, the clock `0x1E → 0`, the point and the return at `f=0x15B9`.
Residual: the court pixels, one real point and the return; the port's
`+0x1F = 8` V line at `0x1083F0`; the S `ball` is a DOS4GW physical pointer
(non-zero/cleared only); real inputs, no poke writes
`DS_00104B1F`/`DS_00104ABC`; no audio claim and no widening of the mode's
unmeasured content. R30's residual now reads "the real-input routes only: R05
and R06 (both closed)"; R27/§3 carry 45 paths and 44/45 present; §4 gained the
superseded "`0x21` stays declared / no capture / future work" row.

### 3.2 The coverage census re-pin

`make coverage COV_REASON="volleyball truth: gp-u30-volleyball hits the
mode-0x21 chain (the 8-trample entry, the court/clock/score/return); re-pinned
from the measured run"` re-ran every driver (35 gp scenarios + the K11/check/
attract/title/frontend/restart tags) and re-pinned
`tools/coverage_baseline.txt`:

```
coverage: split HIT=684 NOT-HIT=71 UNINSTRUMENTED=4
coverage: baseline written (tools/coverage_baseline.txt)
```

The nine improved rows are exactly the functions the new replay exercises:
`0x13134`, `0x26540`, `0x43738` (`fight_char_screen_open`), `0x4BD98` (the mode
entry), `0x4BF18`, `0x4C60C`, `0x4CB18`, `0x4CC0C`, `0x4CD98`. `0x4C784` (the
eat path) and `0x4CE70` stay `NOT-HIT`, consistent with the narrow claim. A
second `coverage_census.py --check` with the same tag list reads
`coverage: no change (HIT=684 NOT-HIT=71 UNINSTRUMENTED=4)`.

### 3.3 The invariants

- `make gate-check`: `captures 44/45 present, 1 missing`, the declared
  `data/k11-captures/gp-u14-win-live`; `capture_manifest: 44 present checked,
  1 declared skipped`; `pin baseline ok (digest
  b8479dd3586c3306b50071d163f8055bf14d92535647c84afe07b22468bcfc12)`.
- `tools/capture_manifest.json` 45 entries; the new
  `data/k11-captures/gp-u30-volleyball` entry carries sha `42e131d1…`, 4,704
  frames, `prage_000.dro`, first `S` f=5, last `S` f=5704.
- `tools/pin_baseline.sha256` unchanged from Task 4's measured re-stamp
  (`b8479dd3…`): no covered marker moved in Task 5.
- `python3 tools/port_progress.py`: `798 1203 66` / `759 759 100` unchanged
  (the phase added test/tooling and Task 2's host-owned `host.c` seam, no `FN_`
  ported function); `README.md`'s title stays `— Reverse Engineering 66%`.
- `port/src/symbols.h` regenerates byte-identically (`make verify`).

### 3.4 The gate

`make verify` exits 0 on the Task 5 tree (the tree also carrying Tasks 1–4;
nothing committed). The measured lines:

```
gate: captures 44/45 present, 1 missing (strict=1, pins_exact=1)
gate: declared gap (absent): data/k11-captures/gp-u14-win-live
capture_manifest: 44 present checked, 1 declared skipped, 0 problems
capture_manifest: pin baseline ok (digest b8479dd3586c3306b50071d163f8055bf14d92535647c84afe07b22468bcfc12)
capture_manifest: verify preflight ok
gp_volley: gp-u30-volleyball: evidence: mode 0x21 at f=F04: b1f=3, afc=0 (stage 0), abc=2, ball=002EE3F0, clk=1E, scores 0-0, C1=1, C5=1, no poke, pad input on both sides ok
gp_volley: gp-u30-volleyball: evidence: the clock 1E -> 0 over 1717 mode-0x21 snapshots; the point (scores peak 0-1) and the return to mode 6 at f=15B9 (clk=0, ball=0, C5=0, scores 0-0) ok
gp_volley: port: trace: the type-6 entry 001083F0 at f=F04 carries +0x1F=8 ok
gp_volley: port: trace: the T/Vg entry f=F04 (...) and return f=15B9 (...) ok
gp_compare: gp-u30-volleyball: capture: poll.log sha256 42e131d1..b8307c, 4704 frames: matches the pin
diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps; 825/845 rows with callees closed (188 have none); 810/845 closed on full rows; 8 ported functions with no row (45 FN_ ported-unrowed: 8 portable [5 registry-visible + 3 header-shape exceptions] + 37 runtime; the scan-visible subset is 42 = 5 + 37). 
entry-triage: 736 candidates (732 by U0's rule); finisher=9 move-callback=71 span-writer=388 ...
PR_AUDIO_SONG_WAV: wrote /tmp/pr_selmo_fm.wav (12 s, 49716 Hz) peak 2790 sha256 530a94301f4be46f9a5c666c94516f8a1f7fca6f136d3be0562faed802c47cef
1304 globals, 1206 functions -> port/src/symbols.h
all checks passed
```

`make audio-render` then runs twice standalone, exit 0 both, with byte-identical
logs: title `/tmp/pr_title_fm.wav` 2,386,412 bytes sha256
`515b7c94a9d0cfb36afa183f9e6642c04ff8ac0150a1f324182cee6bb8d82f3a` (the
register's pinned title) and selmo `/tmp/pr_selmo_fm.wav` peak 2790 sha256
`530a9430…` (the PCM determinism pin). No PCM pin moved.

**Post-gate changes (docs, diagnostic inventory and test hygiene).** The gate
above was taken before the Task 5 fix rounds; the changes after it are:

- `README.md`: the audio counter `74 diff-allowed prefixes / 29,656 ticks` ->
  `86 / 44,489`, and `It has 34 rows` -> `It has 36 rows (R01–R36: 9 closed,
  1 gated, 19 terminal, 7 declared)`; the two old wordings are guarded by the
  new pairs in `tools/check_docs_claims.py`.
- `docs/PROGRESS.md`: the four `0x21` status clauses in the Task 4 /
  state-coverage / Task 9 / Task 14 paragraphs carry the superseded marker
  (R05 closed by this phase); no other historical counter was rewritten.
- `docs/superpowers/specs/2026-10-30-final-claims.md`: the header's "No other
  row changed." -> "No other row changed status.", the §3 provenance paragraph
  (Task 4's capture/pin-baseline re-pin vs Task 5's `make coverage` re-pin) and
  R19's phrase anchor; no row's status or evidence changed in the fix rounds.
- `tools/audio_compare.py`: `OTHER_K11_TRACES` gains `gp-u30-volleyball` and
  the two summary labels / three comments say "other k11 … outside the 29-trace
  gameplay inventory" (7 directories / 9 `.dro` files; the bucket affects only
  the summary line, never a comparison or exit code).
- `Makefile`: the audio-oracle comment ("outside the 29-trace gameplay
  inventory"); it is outside the PIN-BASELINE regions, so the digest is
  unchanged (`b8479dd3586c3306b50071d163f8055bf14d92535647c84afe07b22468bcfc12`).
- `tools/tests/test_gp_volley.py`: the Task 5 fix replaced the bare
  `open(...).read()` with a `with` block (no ResourceWarning; behaviour
  identical).
- `tools/check_docs_claims.py`: the two README guard pairs plus the docstring
  wording (the list is the task-8 pairs plus later measured reconciliations;
  the tool now prints `20 stale-phrase claim(s) absent`).
- this record §2.4: the measured entry/return capture-frame mapping and the
  "Before this run …" wording.

No game code, oracle, pin, allowance, capture byte or coverage row changed in
these rounds. The fix reports' re-runs: round 1
`python3 -m unittest tools.tests.test_final_claims
tools.tests.test_check_docs_claims` -> `Ran 16 tests … OK`;
`python3 tools/check_docs_claims.py` -> `20 stale-phrase claim(s) absent`;
`python3 -m unittest tools.tests.test_audio_compare
tools.tests.test_gp_volley` -> `Ran 72 tests … OK`. Round 2
`python3 -m unittest tools.tests.test_final_claims
tools.tests.test_check_docs_claims tools.tests.test_audio_compare` ->
`Ran 71 tests … OK`. Both rounds' `make gate-check` stayed green (44/45
present, the same pin digest) and `git diff --check` was clean. The Task 5 gate
result above stands on the measured tree; no full `make verify` or
`make audio-render` was re-run in the docs/test-hygiene rounds.
