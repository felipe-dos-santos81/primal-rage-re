# Spec: the volleyball sub-game (mode `0x21`)

Status: **derived, never exhibited.** Every statement below is read from the
ported code (which carries a differential row or a unit test), but **no capture
has ever shown the mode**; register row R05 stays `declared`. Addresses are
linear (Ghidra/LE), and the `DS_xxxx` labels are linear too: a Ghidra
`DAT_0008xxxx` is the linear `0x8xxxx`, i.e. DS offset `0x8xxxx - 0x80000`.

The mode's own functions are `NOT-HIT` in the coverage census
(`tools/coverage_baseline.txt`): `0x4BD98`, `0x4BF18`, `0x4C60C`, `0x4C784`,
`0x4CB18`, `0x4CC0C`, `0x4CD98`, `0x26540`, `0x13134`. The entry tail's
siblings are `HIT`: the measured `0x4B470` calls (`gp-idle-loss` twice, the
demo twice) are all first hits, and `0x4B69C`/`0x4B5A8` are the trample
prelude and the crowd-lie gate — see §10.

## 1. What it is

A hidden, two-human-only volleyball mini-game played with the fight engine. The
eighth counted trample of one crowd worshipper turns that worshipper into a
ball; both fighters then hit the ball back and forth over a midpoint for points,
with a 30-unit match clock and a score screen. It returns to the normal fight.

Entry and end are one-shot: the mode is reached from the fight's trample tail
(`0x4B470`) and leaves through `0x4CD98`, which sets the mode word back to 6 (the
fight frame).

## 2. Entry chain

The trigger is the *eighth* counted trample of one effects entry, in `0x4B470`'s
tail (`fight.c:3080-3091`). All of these must hold, in this order:

| # | gate | raw |
|---|---|---|
| 1 | `DS_00104B1D` is neither 2 nor 3 | `0x4B547`/`0x4B54C` |
| 2 | the stage word `DS_00104AFC == 0` | `0x4B559` |
| 3 | `DS_001088C1 == 0` (not already in the game) | `0x4B562` |
| 4 | `(s8)DS_001088F2 > 1` — the round clock's byte, read as the top byte of the dword `DS_001088EF` — must be at least 2 | `0x4B564`/`0x4B56F` |
| 5 | `entry+0x1F > 7` — the eighth counted trample of that entry (the byte counts hits from 1, so the eighth leaves 8) | `0x4B579` |
| 6 | `DS_001088C5 == 0` (not already ending) | `0x4B582` |
| 7 | `DS_00108864 == 0` (no ball already live) | `0x4B58C` |

Then `0x4BD98(entry)` (`0x4B590`) and, **unconditionally**, `0x4CB18(entry,
index, 0, entry+0x20)` (`0x4B59E`) — the launch happens even when `0x4BD98`
declines, which is what makes the entry shoot away when a later gate fails.

`0x4BD98` (`fight.c:2940`) has three gates of its own:

- `(s32)DS_00104AD8 <= 0` — the mode-`0xB` winner-pose timer is not running (`0x4BDA7`);
- `DS_00104ABC >= 2` — the fight-emulation selector, `(b1f == 3) + 1`, so the
  match must be **two-human** (`0x4BDB4`; writers `flow.c:1632`, `flow.c:2416`);
- `fight_13134() == 0` — both fighters' x are not outside the same `±0x3300`
  (the `0x13134` camera test, `0x4BDC1`).

Its stores (`0x4BDD3..0x4BEE5`):

| store | value | raw |
|---|---|---|
| `DS_00108864` | the entry (the ball) | `0x4BDD3` |
| `entry+0x1E` | 6 (the tumble/ball type) | `0x4BDD9` |
| `DS_001088C1` | 1 | `0x4BDE1` |
| **`DS_00104B00`** | **`0x21`** — the mode | `0x4BDE7` |
| `DS_001088C5` | 1 | `0x4BDEE` |
| `DS_001088AA` | `0x1E` (30 clock units) | `0x4BDF4` |
| `DS_001088A0`, `DS_00108898`, `DS_001088AC` | 0 | `0x4BDFB`, `0x4BE02`, `0x4BE31` |
| `DS_0010889C`, `DS_0010889D` | 0 (both scores) | `0x4BE19`, `0x4BE21` |
| `DS_00104AEC` bit 0 | cleared | `0x4BE27` |
| `entry+0x1C` bit 5 | set (the ball latch) | `0x4BE3C` |

Court setup (`0x4BE3F..0x4BEE5`):

- `DS_00108884` = midpoint of the two fighters' latched x
  (`DS_001077E4`, `DS_00107878`) minus `DS_00108854`. `DS_00108854` is read
  here and **written nowhere in the image** (Ghidra xref: the single read at
  `0x4BE8A`), so in every replay it is 0 and the midpoint is the plain average
  (the `jge`/`sar` pair halves the difference unsigned-safely).
- Two actors spawn from the descriptors at `0xBAB88` and `0xBAB9C`, both at
  `x = DS_00108884`, `y = DS_00108880 - 0x3140`, layer word `DS_000BD898`
  (`0x4BE90`, `0x4BEBC`); they are stored in `DS_00108868` / `DS_0010886C`.
  The second takes stream `0xEF66A` at 1.0 (`0x4BED0`). Both get
  `+0x36 = 0x1A4` (`0x4BEDA`, `0x4BEE5`) — the walk-on velocity the pass's
  preamble later snaps to 0.
- The ball entry is launched by `0x4CB18` (`fight.c:2987`): voice
  `0xD0`/`0xD1` by the actor's `+0x48` char range (`0x4CB1F..0x4CB3E`), stream
  `0xC9604[index]` at 3.0 (`0x4CB52`), horizontal speed
  `+0x34 = ±min(|x0 - x1|, 0x3F00) / 0x70` (`0x4CB63..0x4CBB5`; the sign is
  negated when `0x1A570(side)` holds, i.e. the striker faces right), vertical
  speed `+0x36 = (0x3BC0 - the actor's height) / 0x16` (`0x4CB84..0x4CBF0`),
  type 6 (`0x4CBFE`), and `entry+0x1C` bit 7 cleared (`0x4CC05`).
  `motion_step` (`0x2A4FC`) integrates `+0x34` into x and `+0x36` into the
  height, so these are the ball's horizontal and vertical launch speeds.

`0x4B470`'s trample that feeds the count is the normal fight's: `0x4B69C`
prelude (`0x4B6A6` tests the re-hit latch `entry+0x1C` bit 7, `0x4B716`
increments `+0x1F`). The full trigger geometry (the 71-motion-step flight,
`h(n) = 568n - 8n²`, the seven-pass re-hit window `n = 65..71`, why the count is
not limited to 1) is derived in the final-truth record §F6 and is not repeated
here.

## 3. The frame

`game_mode_21_step` (`0x26540`, `flow.c:3256`) is the mode's handler, dispatched
by `game_frame`'s switch (`flow.c:7807`). It runs the fight frame's preamble and
gated projection block byte-for-byte (`fight_slot_clear`, `camera_screen_base`
per side, the two position latches; only with `DS_001078FA == 2`:
`camera_project` per side, `camera_decay`, `fighter_pass_a`, `fighter_pass_b(0)`,
`camera_project`, `fighter_think`, `camera_project`), then:

```
fight_slot_pass     0x3CB68
fight_hud_pass(0)   0x35658
fight_hud_pass(1)   0x35658
fight_4bf18         0x4BF18   <- the game pass, in place of fight_effects_pass (0x49C78)
camera_scene_step   0x1282C + 0x12DA8
fight_hud_pulse     0x1DA08
flow_round_end_check 0x27FA8
DS_00104AEC |= 2    0x266A0
```

Unlike `0x26254` there is no `fight_mode25_enter` tail call (`0x4E11C`).

The frame has a second, shared tail: `0x2545C`'s `case 0x21` (`0x25509`) runs
`fighter_body_push`, `camera_pair_hold(1)`, latches each slot and syncs its pset
point, then `fight_health_bars`. So the fight's health bars stay visible over
the court.

Because `0x4BE27` clears `DS_00104AEC` bit 0, the round-timer render entry
`flow_round_timer_step` (`0x4F4E8`) is not called while the mode runs; nothing
decrements `DS_001088F2` (its only writer besides the mode-`0x27ED8`/`0x4F37C`
initialisers is that entry's `0x4F5A6`). `flow_round_end_check` (`0x27FA8`) is
still called every frame, and since the gate `DS_001088F2 >= 2` held at entry
(§2 #4) and the byte never falls, its round-end arms cannot fire during the game
unless a fighter's damage byte reaches `0x78` — which the mini-game does not
write. The mode therefore has exactly one exit: `0x4CD98` (§8).

## 4. The pass `0x4BF18`

`fight_4bf18` (`fight.c:4477`) is the mode's per-frame game driver.

**Preamble** (`0x4BF21..0x4BF53`): when `DS_00108868+0x36` (the walk-on
velocity) is non-zero and the distance to `DS_00108880` minus its `+0x34>>16`
has closed to within its own `+0x1C`, both court actors' `+0x36` snap to 0.

**List walk** (`0x4BF53..0x4C39C`): the *same* singly-linked effects list
`DS_0010884C` that `fight_effects_pass` walks (`entry+0 = next`, `+8 = the actor
record`, `+0x1E = the type`, `+0x21 = the owner side`, index =
`(u8)rec+0x48 - 0x20`). Per entry, the type byte dispatches through the 9-dword
jump table at `0x4BEF4` (AL 0..8; AL > 8 takes the unsigned `ja` default, which
is also table entries 0 and 7 — so types 0, 7 and >8 share one body).

| type | body | raw |
|---|---|---|
| 0, 7, >8 | if `DS_00108898 - 1 == entry+0x21` → `fight_4B3F0(entry,index,0)` (the `0xC955C` walk stream, type 8); else if `DS_00108898 != 0` → `fight_4B430(entry,index,0)` (the `0xC958C` stream, type 8) | `0x4BF86..0x4BFC3` |
| 1 | bit 5 clear: arrival test `|rec+0x18 - entry+0x14| <= sign(rec+0x34)-gated rec+0x32>>16` → `fight_4AC38(entry,index)`; bit 5 set: `fight_4A868(entry)` → voice `0xC8`, rec `+0x38/+0x34/+0x36` = 0, type 8, rec+0x55 = 1, `actors_anim_begin(rec, 0xC958C[index], 3.0)` | `0x4BFCD..0x4C02F` |
| 2 | `entry+0x18` word counts down; reaching < 1 → `fight_4AC38` | `0x4C07E` |
| 3, 4 | `rec+0x2C = fight_dust_clamp(rec+0x30>>16)`; `actors_anim_begin(rec, 0xC95EC[index], 3.0)`; `rec+0x38 = 0x40`; type 5; `entry+0x1C` bit 7 cleared | `0x4C0A1` |
| 5 | same clamp; when the signed word `rec+0x32 >= entry+0x1A`: `actors_anim_begin(rec, 0xC9544[index], 3.0)`, `rec+0x38 = rec+0x34 = 0`, type 0 | `0x4C0E7` |
| 6 | bit 5 clear only: `vy < 0` sets bit 7; a held actor gets rec `+0x18`/`+0x32`; landed (`vy + height <= 0`): free the held actor, take `0xC973C[index]` (type 8) or `0xC9544[index]` (type 4), zero rec `+0x1C/+0x36/+0x34` and `entry+0x1F`; else `rec+0x36 -= 0x10` | `0x4C135..0x4C1EE` |
| 8 | no body (the table entry *is* the shared tail) | `0x4C1F3` |

**Shared tail** (`0x4C1F3`, every type): if `entry+0x1C` bit 5 is set and the
type is not 1 → `fight_4C60C(entry, index)` (§6); then `vy < 0` sets bit 7; a
held actor gets rec `+0x18`/`+0x32`; then, when the type is 6 and
`vy + height <= 0` (the ball was struck into the ground this same frame), the
ball is landed (§7) — and a third point **returns from the whole function,
abandoning the rest of the entry list** (`0x4C35B jmp 0x4C5AF`).

**Post-loop** (`0x4C39C`, also the empty-list target):

- `DS_001088A0` non-zero counts down (unsigned `ja`); reaching 0 calls
  `fight_4CD98()` and returns (§8);
- `DS_00108874 = fight_midpoint()`;
- every `DS_001088D0` ticks (signed `idiv` of the zero-extended tick word
  `DS_00104AF4`): if `DS_001088F2 != 0` draw it at (0x13, 1), width 2, mode
  0x5000; if `DS_001088AA == 0` call `fight_4CC0C()` and return (the timeout
  end); else decrement it and draw it at (0x13, 4), and draw both scores at
  (1, 7) and (0x26, 7) (`0x4C3C9..0x4C4A3`);
- with no serve pending (`DS_00108898 == 0 && DS_001088AC == 0`) draw the idle
  prompt: string `0x55` centred on rows 6 and 9 and string `0x54` at columns 7
  and 0x16 on row 8 (`0x4C4A8..0x4C5AA`);
- otherwise count `DS_001088AC` down (clearing `DS_00108898` with it) and, while
  `DS_00108898` is live, draw string `0x53` "POINT" at column 0x16 when
  `0x1A570(DS_00108898 - 1)` holds (the scorer faces right), else at column 7,
  row 8 (`0x4C4C0..0x4C528`).

`DS_001088D0` is the fight's interval word, `(config[0x29] & 0xF) * 5 + 0x1E`
(`0x20CB0`), so the match clock advances one `DS_001088AA` unit per 30..105
ticks.

## 5. The ball

The ball is the effect entry in `DS_00108864`; its actor is the worshipper that
was trampled eight times. Fields used by this mode:

| field | meaning |
|---|---|
| `entry+0x1E` | type: 6 while flying/lying, 8 when settled/held, 1 while walking in |
| `entry+0x21` | the owner side (written by the eat path; the default type reads it) |
| `entry+0x20` | the side that last struck it (written by `0x4C60C`) |
| `entry+0x1F` | the strike/trample count (the strike path increments it at `0x4C6DE`; the landing and eat ends zero it; for a fight entry it is the trample count) |
| `entry+0x1C` bit 5 | "this is the mode-0x21 ball" (set at entry `0x4BE3C`; cleared/free via the type bodies) |
| `entry+0x1C` bit 7 | the flight latch (set while `vy < 0`, cleared on every strike/launch) |
| `entry+0x14` | the walk target x (set by the spawn and the walk-in) |
| `entry+0x10` | the held actor, if any |
| `rec+0x18` | the actor's x (the scoring comparison) |
| `rec+0x1C` | the height (dword) |
| `rec+0x34` | the horizontal speed (word) |
| `rec+0x36` | the vertical speed (word; `vy < 0` = falling) |

A new ball is created by the eat path (§7), never by the landing path;
the landing path turns the *landed* ball into type 8 and leaves it there until
the next score spawns a new one. The default type's two walk streams
(`0xC955C` vs `0xC958C`) are chosen by whether the just-scored side equals the
ball's owner (`DS_00108898 - 1 == entry+0x21`).

## 6. Striking and eating (`0x4C60C`, `0x4C784`)

`fight_4c60c` (`fight.c:4284`) tests the ball actor's pset point against both
fighters (`0x17D30`, BX = 0; both sides count as side 0 — `camera_point_hit`
returns 1 or 2, >2 folds to 1). Then the touched side's character (`slot+0x7A`,
above 6 = no action) and its state byte (`slot+0x5F`) decide:

| character | state `+0x5F` | outcome |
|---|---|---|
| 0, 3, 5 (SAURON, VERTIGO, DIABLO) | 0 | **eat** (`0x4C784`) |
| 2 (TALON) | 1 | **eat** |
| the same three | any other | strike |
| 2 | any other | strike |
| 1, 4, 6 (BLIZZARD, ARMADON, CHAOS) | any | strike |

The character→name mapping is the roster order measured by the ending captures
(TALON 2, VERTIGO 3, ARMADON 4, DIABLO 5); the state byte is the per-character
reaction index (`0xFF` = neutral, written by the reaction dispatchers), so
"state 0/1" is declared, not described as an animation.

**Strike** (`0x4C6B7..0x4C774`): `DS_00108898 = 0`; `entry+0x20 = the side`;
`entry+0x1C` bit 3 cleared; `entry+0x1F += 1`; a held actor (`rec+0x4A`) is
released (`+0x4A = 0`, the `+0x2A` bit 3 and `+0x29` bit 6 cleared,
`0x1088AE[`entry+0x21`] += 1`, `entry+0x1C` bit 6 cleared); then
`fight_4CB18(entry, index, flag, side)` with `flag = 1` and `entry+0x1C` bit 3
set when the striker's state is `0x0C..0x0F`, else `flag = 0`. The flag selects
the speed divisor (`0x38` vs `0x70`) and, at flag 1, `vy = 0`.

**Eat** (`0x4C784`, `fight.c:4186`):

1. spawn a `0xC976C` actor at the ball (`+0x18`, `+0x1C`, y =
   `DS_000BD898 - 0x100`, flags `0x4000` unless the fighter faces left) with
   stream `0xEF65A` at 3.0 — the spit;
2. kill the ball's shadow (`entry+0x10`) and the ball's actor, voices `0xD4`
   (or `0xD5` by the actor's `+0x48` character), `0xD6`, `0xCE`;
3. **the other side scores**: `DS_0010889C[side ^ 1] += 1`; the court actor
   `DS_0010886C` turns on `0xEF680` / `0xEF6AC` (whether the midpoint is right
   of the other fighter);
4. `DS_001088AC = 0x69` (105 ticks of point pause) and
   `DS_00108898 = other + 1` (the scorer, 1 or 2);
5. if the other side's score reaches 3: `entry+0x1F = 0`; call `0x4CC0C` when
   `DS_001088A0 == 0`; `DS_00108864 = 0`; return;
6. otherwise draw string `0x56` "BALL EATEN!" centred on row 6 and spawn a new
   ball: one node from the free list `DS_001083C4` to `DS_0010884C`
   (`effects_list_unlink`/`insert_after`), the dust descriptor
   `0xC9524[fight_dust_pick(other)]`, `+0x10 = 0x29CDC(side, slot+0x7A)`, an
   actor from that descriptor at `0x2580` beyond `DS_00108868` on the eater's
   side, walked (type 1, bit 5) to `0x1740` from it; the new entry becomes
   `DS_00108864`, owned by the *eaten* ball's side (`+0x21`, `+0x0C`), type 1,
   bit 5 set.

The eat is worth the point to the *opponent*: touching the ball in a non-strike
state is a fault. This is a distinct scoring path from the landing.

## 7. Scoring by landing

In the shared tail, when the type is 6 and `vy + height <= 0`
(`0x4C259..0x4C2B5`):

- free the held actor and clear `entry+0x1F`, `rec+0x38`, `rec+0x34`,
  `rec+0x36`;
- `side0 = 0x1A570(0)` (fighter 0 faces right); `mid = DS_00108884`;
  `idx = (s32)ballx < (s32)mid ? side0 ^ 1 : side0` — i.e. **the point goes to
  the player whose half the ball landed in** (the facing term maps the screen
  halves to the side indices; a fighter faces his opponent, so `side0` says
  whether side 0 is the left or the right player);
- `DS_0010889C[idx] += 1` (left column = `DS_0010889C`, right =
  `DS_0010889D`; the score screen's `a <= b ? "RIGHT PLAYER" : "LEFT PLAYER"`
  at `0x4CD3E` pins that);
- `DS_00108898 = idx + 1`; the court actor `DS_0010886C` turns on
  `0xEF6AC` when the ball fell left of the midpoint, else `0xEF680`; the ball
  actor turns on `0xC973C[index]` at 2.0; type 8; `DS_001088AC = 0x69`;
- if `DS_0010889C[idx] >= 3`: `entry+0x1F = 0`; call `0x4CC0C` when
  `DS_001088A0 == 0`; **return from the whole pass** (the rest of the frame's
  entries are skipped).

The landing comparison runs after the shared tail's strike test, so a ball
struck on the same frame is re-launched first and its new `vy` decides the
landing test: the strike wins the frame.

## 8. The end screen and the exit (`0x4CC0C`, `0x4CD98`)

`fight_4cc0c` (`fight.c:4144`) is both ends' screen:

- both court actors get `+0x36 = -0x1A4` (walk off) and `DS_001088A0 = 0x3C`
  (60 frames);
- the static board: string `0x57` at (10, 8), `0x58` at (0x13, 4), `0x59` at
  (1, 7), `0x5A` at (0x26, 7), `0x5B` at (0x13, 1), `0x5C` at (7, 8), `0x5D`
  at (0x16, 8) — all of them blanks in `ENGLISH.TXT` (the numbers are drawn by
  the post-loop) — then centred on rows 6 and 9: string `0x5E` "VOLLEYBALL
  GAME" and, when the scores are equal, `0x5F` "TIED"; otherwise row 6 is
  string `0x16` "RIGHT PLAYER" when `left <= right`, else `0x17` "LEFT PLAYER";
- `DS_001088A0`'s 60-frame countdown is then run by the pass's post-loop; at 0
  it calls `fight_4CD98` and returns.

`fight_4cd98` (`fight.c:4354`) is the teardown/exit:

- `DS_00104B00 = 6` — back to the normal fight frame;
- `DS_00104AEC |= 1` (the fight's render entry 0 back on, i.e. the round timer
  resumes);
- `DS_001088C5`, `DS_0010889D`, `DS_0010889C` = 0;
- a live ball entry (`DS_00108864` non-zero and type != 6) is reset:
  `+0x18 = 0`, `+0x1C` bit 5 cleared, type 4, `DS_00108864 = 0`; a type-6 ball
  is left alone (the normal effects pass will land it);
- string `0x60` (15 blanks) is drawn on rows 6 and 9 — the score screen erased
  — and both court actors are killed.

So both ends (score 3 and clock 0) run `0x4CC0C` + a 60-frame hold +
`0x4CD98`; the timeout end never spawns a new ball, and the **landing** end
leaves `DS_00108864` pointing at the settled ball, which `0x4CD98` then clears
(type 8 ≠ 6); the **eat** end clears it itself (`0x4C914`). The mode can
therefore re-trigger after the fight resumes (the seven gates of §2 are
clearable again, subject to the round clock and the two-human match).

## 9. The clock and the score screen numbers

- `DS_001088AA` = 30 at entry, decremented one per `DS_001088D0` ticks by the
  post-loop; drawn at (0x13, 4) width 2. At 0 the timeout end runs (`0x4C429`).
  It is the match clock, not the round clock.
- `DS_001088F2` (the round clock's byte) is *drawn* at (0x13, 1) but not
  decremented while the mode runs (§3); its value is whatever the fight left.
- The two scores are `DS_0010889C` (drawn at column 1) and `DS_0010889D`
  (column 0x26); they are drawn only on the interval frames.
- `DS_001088AC` = 0x69 after every point; the post-loop decrements it and
  clears `DS_00108898` with it, which is what separates the point text from the
  next serve.

## 10. Measured boundary

What is measured:

- The trigger tail is **reached**, but never past gate #5. `gp-idle-loss`
  (the only capture with a trample) hits `0x4B470` twice, both first counted
  hits (`+0x1F = 1`, stage 4, `via=1`, mode 6) in one frame, on distinct lying
  entries; the demo hits it twice at stage 0. `gp-twop`, `gp-twop-ko`,
  `gp-draw-timeout` and `gp-keys-fight` call it zero times (final-truth record
  §F6.7). The count is therefore measured only at 1; `+0x1F > 7` is a derived
  reachable state, not a measured one.
- The entry's stage gate is a 1-in-7 draw for a two-human match
  (`flow_stage_pick` `0x2587C`, `rng(7)`; `gp-twop` measured word 5; record
  §F6.6). No two-human capture has both fighters ~140 px apart on stage 0 with
  a crowd lie.
- Every function in the chain has a differential row (`0x4B470`, `0x4BD98`,
  `0x4CB18`, `0x4C60C`, `0x4C784`, `0x4BF18`, `0x4CC0C`, `0x4CD98`,
  `0x26540`), and the trample arithmetic, the flight, the latch and the landing
  are unit-tested (`test_fight.c` cases G/H/I/J; record §F6.4). The **mode's
  own pass, its score, its clock, its end screen and its strings have never
  been rendered** by any measured run.

What is not measured (named gaps, all declared):

- that any ball type other than 6 behaves as the code says in a live game
  (types 1/2/3/4/5/8 are derived from the dispatch, not from a frame);
- the eat path's new-ball spawn geometry in motion;
- the score screen's text composition (all-blank strings plus the drawn
  numbers) and the winner string selection;
- the mode's interaction with the fight HUD/health bars over the court;
- the exact per-character `+0x5F` values that make the eat arm fire.

## 11. The route to a capture (port-side search)

The only honest route left is a search in the deterministic port, then one
authorized capture replaying the found script (record §F6.8, final-truth plan
Task 6; register R05):

1. script a two-human join and a stage-0 match (retry until `DS_00104AFC == 0`;
   the pick is `rng(7)`, so ~7 attempts), then a combo that lies the crowd
   (`0x4B5A8` → type 3 → latch);
2. drive the two fighters to ~140 px apart (±0x80/0x70 x-speed swings), then
   spam the two strike windows at 71-motion-step intervals, reading `+0x1F`
   from the port, until it reaches 8;
3. the eighth hit's frame sets the mode; from there the port's own replay is
   the capture's expected frame path (the mode's pass is deterministic and its
   functions have differential rows, but no frame of it has been measured —
   §10 — so the capture is its first exhibit), and a capture that reaches it
   can be pinned like the other `gp-*` oracles;
4. the capture must show the court actors' walk-on, a point (landing or eat),
   the score board and the return to mode 6 to publish an oracle claim; the
   mode's *content* is what R05 would become.

Cost/risk: a new headless search driver plus one capture; the 1-in-7 stage is a
retry, not a blocker; the eight-window timing is the real risk (a miss lands the
entry and restarts the chain). The count is not limited to 1, so a script that
finds the phase can repeat it.

## 12. References

- `port/src/game/fight.c`: `0x4BD98` 2940, `0x4CB18` 2987, `0x4B470` 3045,
  `0x4CC0C` 4144, `0x4C784` 4186, `0x4C60C` 4284, `0x4CD98` 4354,
  `0x4BF18` 4477; `port/src/game/flow.c`: `0x26540` 3256, dispatch 7807,
  0x2545C tail 7982.
- `docs/superpowers/plans/2026-10-30-final-truth-derivations.md` §F6 (the
  trigger geometry, the gate chain, the measured tramples and the route).
- `docs/superpowers/specs/2026-10-30-final-claims.md` R05 (declared).
- `port/decomp/prage.c` for the raw bodies; `mem.h` for the address model.
