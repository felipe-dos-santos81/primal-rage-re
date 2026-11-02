# Spec: the volleyball sub-game (mode `0x21`)

Status: **derived and exhibited.** Every statement below is read from the
ported code (which carries a differential row or a unit test), and the mode was
published by Volleyball truth Task 4: `data/k11-captures/gp-u30-volleyball`
reaches `entry+0x1F = 8`, the mode entry `0x4BD98` at `f=0x0F04`, a real point
and the return to mode 6 at `f=0x15B9`, with `gp-u30-volleyball-oracle` and
`tools/gp_volley.py check` pinning it; register row R05 is `closed` (record
`../docs/superpowers/plans/2026-11-01-volleyball-truth-derivations.md` §2).
Addresses are
linear (Ghidra/LE), and the `DS_xxxx` labels are linear too: a Ghidra
`DAT_0008xxxx` is the linear `0x8xxxx`, i.e. DS offset `0x8xxxx - 0x80000`.

The mode's own functions were `NOT-HIT` in the coverage census before the
capture (`tools/coverage_baseline.txt`): `0x4BD98`, `0x4BF18`, `0x4C60C`, `0x4C784`,
`0x4CB18`, `0x4CC0C`, `0x4CD98`, `0x26540`, `0x13134`; Volleyball truth Task 5
re-pinned the baseline from the measured run and the nine rows the capture's
replay exercises are now `HIT` (`0x13134`, `0x26540`, `0x43738`, `0x4BD98`,
`0x4BF18`, `0x4C60C`, `0x4CB18`, `0x4CC0C`, `0x4CD98`; HIT 684 / NOT-HIT 71 /
UNINSTRUMENTED 4, register R18). `0x4C784` (the eat path) stays `NOT-HIT`. The
entry tail's
siblings are `HIT`: the measured `0x4B470` calls (`gp-idle-loss` twice, the
demo twice) are all first hits, and `0x4B69C`/`0x4B5A8` are the trample
prelude and the crowd walk-in spawner (`0x4B5A8`; the lie itself is the effects
pass's case-3 landing, `0x49E47`/`0x49E52`, §2) — see §10.

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
  The second takes stream `0xEF66A` at 1.0 (`0x4BED0`). Both actors get
  `actor+0x36 = 0x1A4` (`0x4BEDA`, `0x4BEE5`) — the walk-on velocity the pass's
  preamble later snaps to 0.
- The ball entry is launched by `0x4CB18` (`fight.c:2987`): voice
  `0xD0`/`0xD1` by the ball actor's `+0x48` char range (`0x4CB1F..0x4CB3E`),
  stream `0xC9604[index]` at 3.0 (`0x4CB52`), the ball actor's horizontal speed
  `+0x34 = ±min(|x0 - x1|, 0x3F00) / 0x70` (`0x4CB63..0x4CBB5`; the sign is
  negated when `0x1A570(side)` holds — the §7 pset-hflip reading), the ball
  actor's vertical speed `+0x36 = (0x3BC0 - the actor's height) / 0x16`
  (`0x4CB84..0x4CBF0`), type 6 (`0x4CBFE`), and `entry+0x1C` bit 7 cleared
  (`0x4CC05`). `motion_step` (`0x2A4FC`) integrates the ball actor's `+0x34`
  into x and `+0x36` into the height, so these are the ball's horizontal and
  vertical launch speeds.

`0x4B470`'s trample that feeds the count is the normal fight's. `0x4B69C` is
called once per effects entry at the top of `0x49C78`'s loop (`0x49CFE`),
before the type dispatch. It tests only the re-hit latch (`0x4B6A6`,
`entry+0x1C` bit 7), finds the fighter whose point box contains the entry
actor's pset point (`0x17D30`; both sides fold to side 0), and calls `0x4B788`
(`0x4B705`). Only when `0x4B788` returns 1 (`0x4B70A`/`0x4B70C`) does it store
`entry+0x20 = hit - 1` (`0x4B713`), increment `+0x1F` (`0x4B716`) and call
`0x4B470` (`0x4B779`). `0x4B788` returns 1 when `DS_00105B3A > 1` (`0x4B7A0`),
when the other side's slot `+0x54 == 3` (`0x4B7DC`), or when the fighter's
`+0x5F` is not his character's grab-move id `0xC97F2[ch]` (`0x2D` for characters
0..6, `0x4B7EF`/`0x4B7F5`); its return-0 arm is the grab (bit 6 set at
`0x4B845`, the body linked at `actor+0x4A` by `0x4B94A`/`0x4B956`) and does not
count. This is why a touch can do nothing.

The crowd supply is a two-step event, not a latch written by `0x4B5A8`. A side's
2+ hit combo makes `0x39040` copy the consecutive-hit byte `0x107D2C[side]` into
the per-side budget `DS_001088B6[side]` (`0x39056`/`0x3907B`). The type-0 crowd
body `0x4AAD0` calls `0x4B5A8` (`0x4AB45`); when `DS_001088B6[actor+0x51] != 0`
(the slot actor's `+0x51`) or the slot is in the dizzy state `slot+0x52 == 7 &&
slot+0x53 == 2` (set by `0x36BC8` at `0x36C8F`/`0x36C96`), it turns one crowd
entry into a walk-in: stream `0xC95BC[index]` at 3.0 (`0x4B5D7`/`0x4B5E6`),
`entry+0x1A = actor+0x32` (`0x4B5F2`), `actor+0x38 = -0x40` (`0x4B5F9`),
`actor+0x34 = ±0x40` (`0x4B614`/`0x4B62F`), `entry+0x14` = the fighter's x
±0x1800 (`0x4B627`/`0x4B642`, stored at `0x4B650`), type 3 (`0x4B656`), and
`DS_001088B6` decremented (`0x4B68B`). `0x4B5A8` writes nothing to
`entry+0x1C`. The lie arrives on the next frame from the effects pass's case-3
landing (`0x49E47`/`0x49E52`): type 4, `entry+0x1C |= 0x80`,
`entry+0x18 = rng(0x3C) + 0x3C` (60..119 frames, `0x49E3A`/`0x49E4E`); the
latch clears when the type-4 timer rises (`0x49EB5`/`0x49EB8`).

Derived vs measured: the raw latch (`entry+0x1C` bit 7) is set at `T+37`
(the first `vy < 0` pass, `0x49F18`); the prelude runs before the type
dispatch, so it sees the latch from `T+38`. The 35 `vy < 0` passes are
`T+37..T+71`. The practical body-contact window is `T+65..T+71` (7 passes):
the entry's pset point must fall in a fighter's 56 px (`0x38`) box (`0x1790C`),
and `h(65) = 3,120 < 3,584`. **The label is the motion-step index and is
approximate**: the prelude at pass `T+k` evaluates the height left by the
`k-1`th motion step, so the height-only window is passes `T+66..T+72`; the
contact also depends on the fighters' horizontal boxes and the attack's active
frame, and the eight measured hit-frame gaps from the previous hit are 66, 72,
68, 70, 68, 78 and 63 (Volleyball truth Task 4, `gp-u30-volleyball`). The
measured hit frames are the authority. The ~142 px separation of the two
fighters is the zero-`+0x42` throw: 71 motion steps × `0x80` units = 2 px/frame
× 71; `+0x42`/`+0x43` can lengthen it (record §F6.4). The flight
`h(n) = 568n - 8n²`, the same-pass `0x230` first step and why the count is not
limited to 1 are derived in the final-truth record §F6 and are not repeated
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

**Preamble** (`0x4BF21..0x4BF53`): when the first court actor's `+0x36` word
(the walk-on velocity, `0x4BF26`) is non-zero and the distance to
`DS_00108880` minus that actor's `+0x36` word (the high half of the dword at
`+0x34`, `0x4BF2D`/`0x4BF30`) has closed to within its own `+0x1C` height, both
court actors' `+0x36` snap to 0 (`0x4BF42`/`0x4BF48`).

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
| 6 | bit 5 clear only: `vy < 0` sets bit 7; the shadow actor (`entry+0x10`) follows rec `+0x18`/`+0x32`; landed (`vy + height <= 0`): kill the shadow actor, take `0xC973C[index]` (type 8) or `0xC9544[index]` (type 4), zero rec `+0x1C/+0x36/+0x34` and `entry+0x1F`; else `rec+0x36 -= 0x10` | `0x4C135..0x4C1EE` |
| 8 | no body (the table entry *is* the shared tail) | `0x4C1F3` |

**Shared tail** (`0x4C1F3`, every type): if `entry+0x1C` bit 5 is set and the
type is not 1 → `fight_4C60C(entry, index)` (§6); then `vy < 0` sets bit 7; the
shadow actor (`entry+0x10`) follows the ball actor's rec `+0x18`/`+0x32`; then,
when the type is 6 and `vy + height <= 0` (the ball was struck into the ground
this same frame), the ball is landed (§7) — and a third point **returns from
the whole function, abandoning the rest of the entry list** (`0x4C35B jmp
0x4C5AF`).

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
  `0x1A570(DS_00108898 - 1)` holds (the scorer faces right, the §7 reading),
  else at column 7, row 8 (`0x4C4C0..0x4C528`).

`DS_001088D0` is the fight's interval word, `(config[0x29] & 0xF) * 5 + 0x1E`
(`0x20CB0`), so the match clock advances one `DS_001088AA` unit per 30..105
ticks.

## 5. The ball

The ball is the effect entry in `DS_00108864`; its actor is the worshipper that
was trampled eight times. The entry is 0x24 bytes (the pool stride,
`fight.c:56`); `entry+0x08` is its actor record (read at `0x4B478`/`0x4B52B`),
and `rec` below is that actor. The `+0x34`/`+0x36` words are the actor's vx/vy
integer words (`motion_step` `0x2A4FC` reads the dword at `+0x32`/`+0x34` and
takes the high word), not entry fields. Fields used by this mode:

| field | meaning |
|---|---|
| `entry+0x1E` | type: 6 while flying/lying, 8 when settled/held, 1 while walking in |
| `entry+0x21` | the owner side (written by the eat path; the default type reads it) |
| `entry+0x20` | the side that last struck it (written by `0x4C60C`) |
| `entry+0x1F` | the strike/trample count (incremented at `0x4B716`/`0x4C6DE`; zeroed at the sites listed below; for a fight entry it is the trample count) |
| `entry+0x1C` bit 5 | "this is the mode-0x21 ball" (set at entry `0x4BE3C`; cleared/free via the type bodies) |
| `entry+0x1C` bit 6 | "grabbed/held": set by `0x4B788` (`0x4B845`) with the body linked at `actor+0x4A`; cleared by the strike release (`0x4C727`) and by `0x4B69C`'s release (`0x4B760`) |
| `actor+0x4A` | the held body's pset link to the fighter, set by `0x4B94A`/`0x4B956`, cleared on release (`0x4C718`, `0x4B751`) |
| `entry+0x1C` bit 7 | the flight latch (set by the case-3 landing `0x49E52`, the type-6 body `0x4C14F`, the shared tail `0x4C227`, or while `vy < 0` at `0x49F18`; cleared on every strike/launch and by the type-4 rise `0x49EB8`) |
| `entry+0x14` | the walk target x (set by the spawn and the walk-in) |
| `entry+0x10` | the shadow actor: spawned at `0x4B4B6..0x4B4D7` (store `0x4B4D7`), follows the ball actor's x/y (`0x4C159..0x4C16F`), killed at `0x4B84E`/`0x49F52`/`0x4C273`; it is not the held body |
| `rec+0x18` | the actor's x (the scoring comparison) |
| `rec+0x1C` | the height (dword) |
| `rec+0x34` | the actor's horizontal speed (word) |
| `rec+0x36` | the actor's vertical speed (word; `vy < 0` = falling) |

`entry+0x1F` is zeroed at `0x49622` (the dust/effects builder), `0x4D061` (the
mode-0x23 six-entry sibling), `0x49FAF` (the fight's type-6 landing), `0x4C1E8`
(the mode-0x21 type-6 body landing), `0x4C27F` (the shared-tail ball landing),
`0x4C349` (the 3-point landing end), `0x4C904` (the eat path's 3-point end),
`0x4CA59` (the eat path's new-ball spawn) and `0x4E4E5` (the ending audience
spawn). `0x4E4E8` is that spawn's `+0x21`, not a `+0x1F` site.

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
`entry+0x1C` bit 3 cleared; `entry+0x1F += 1`; the held body (`actor+0x4A`,
`entry+0x1C` bit 6) is released (`+0x4A = 0`, the `+0x2A` bit 3 and `+0x29` bit
6 cleared, `0x1088AE[`entry+0x21`] += 1`, `entry+0x1C` bit 6 cleared); then
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

- kill the shadow actor (`entry+0x10`, `0x4C273`) and clear `entry+0x1F`
  (`0x4C27F`), `rec+0x38`, `rec+0x34`, `rec+0x36`;
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

The `0x1A570(0) == 1` step ("fighter 0 faces right") is the usual pset-hflip
reading: the raw tests only bit `0x8000` of the pset word (`0x1A59C`), and the
port does not independently measure which screen direction that bit means, so
"whose half" is a reading of the raw's XOR, not an exhibited claim.

The landing comparison runs after the shared tail's strike test, so a ball
struck on the same frame is re-launched first and its new `vy` decides the
landing test: the strike wins the frame.

## 8. The end screen and the exit (`0x4CC0C`, `0x4CD98`)

`fight_4cc0c` (`fight.c:4144`) is both ends' screen:

- both court actors' `+0x36 = -0x1A4` (walk off) and `DS_001088A0 = 0x3C`
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

- The trigger tail was **reached, but never past gate #5** before Volleyball
  truth (that boundary is historical): `gp-idle-loss`
  (the only capture with a trample; 8,173 presented frames,
  `tools/capture_manifest.json`'s `frame_count`; the counter's last `S` is
  9,857; record §F6.7's 2,565 is superseded) hits `0x4B470` twice, both first
  counted hits (`+0x1F = 1`, `via=1`, mode 6) in one frame, at trace `f=0x8BE`
  (2,238 decimal; the record's `f=2238` is decimal, not hex), on distinct lying
  entries `0x108414`/`0x108438` (type 4, `entry+0x1C = 0x80`), stage word 4.
  The demo hits it twice at stage 0; the demo's stage 0 is the boot default
  (`0x20C10`'s `0x20CDF` store clears `DS_00104AFC`), not a `flow_stage_pick`
  result. `gp-twop`, `gp-twop-ko`, `gp-draw-timeout` and `gp-keys-fight` call
  it zero times (final-truth record §F6.7). The count was therefore measured
  only at 1 at that phase.
- **The eight-count and the mode are now measured** (Volleyball truth Task 4,
  record `../docs/superpowers/plans/2026-11-01-volleyball-truth-derivations.md`
  §2): `gp-u30-volleyball` reaches `entry+0x1F` 1..8 by real two-human input —
  the first counted trample at `f=0x0D1F` (3359), the eighth at `f=0x0F04`
  (3844) — and the eighth enters mode `0x21` at `0x4BD98`. Its `S` records at
  entry (`f=0x0F04`) read `b1f=3`, `afc=0` (stage 0), `abc=2`,
  `clk=0x1E`, `C1=C5=1`, scores 0-0; the clock runs `0x1E → 0` over the mode's
  1,717 records; the side-1 score takes one real point; and the return is
  `f=0x15B9` (mode 6, `clk=0`, ball gone, scores 0-0). The port's own `V` line
  carries the `+0x1F = 8` at entry `0x1083F0` (the 8th trample of the
  eight-trample chain). The capture's S `ball` is a DOS4GW physical pointer
  (`0x2EE3F0`), so the checker claims only non-zero at the entry and zero at
  the return.
- The entry's stage gate is a 1-in-7 draw for a two-human match
  (`flow_stage_pick` `0x2587C`, `rng(7)`; `gp-twop` measured word 5; record
  §F6.6). Route B keeps a 1P stage instead (§11): `gp-u30-volleyball`'s 1P
  select confirms character 2 and `0xA87C4[2] = 2` plus `rng(6) = 4` gives
  `7 → 0` at the vs.
- Every function in the chain has a differential row (`0x4B5A8`, `0x4B69C`,
  `0x4B788`, `0x4B470`, `0x4BD98`, `0x4CB18`, `0x4C60C`, `0x4C784`, `0x4BF18`,
  `0x4CC0C`, `0x4CD98`, `0x26540`), and the trample arithmetic, the flight, the
  latch and the landing are unit-tested (`test_fight.c` cases G/H/I/J; record
  §F6.4). The **mode's own pass was rendered for the first time** by
  `gp-u30-volleyball` (its court frames, clock and score); the capture's
  pixels are the first exhibit, and the first unexplained capture frame (4,640)
  is the replay's post-script tail, so nothing past capture 4,639 is claimed.

What the published capture does not claim (the mode's content is not widened by
the exhibit):

- that any ball type other than 6 behaves as the code says in a live game
  (types 1/2/3/4/5/8 are derived from the dispatch, not from a frame);
- the eat path's new-ball spawn geometry in motion;
- the score screen's text composition (all-blank strings plus the drawn
  numbers) and the winner string selection;
- the mode's interaction with the fight HUD/health bars over the court;
- the exact per-character `+0x5F` values that make the eat arm fire.

## 11. The route to a capture (taken: Volleyball truth Task 4)

The trigger's mechanism is derived (§2-§8) and its four measured first-hit
calls are §10's earlier boundary. This section's search was run in the
deterministic port (Final truth record §F6.8, final-truth plan Task 6; register
R05) and its route B produced the published `gp-u30-volleyball` capture
(record `../docs/superpowers/plans/2026-11-01-volleyball-truth-derivations.md`
§2). Route B is primary because the join
keeps a stage chosen before it; route A is the fallback.

### Route B (primary): a mid-round join on the 1P stage

1. **Stage 0 from the 1P pick.** In the 1P character select choose a character
   index `c` whose pick gives 0. The pick is
   `DS_00104AFC = 0xA87C4[c] + rng(6) + 1`, reduced by 7 when the word is
   `>= 7` (`0x25897..0x258C6`); `c` is the signed character byte at
   `0x108169 + b1f` (`0x2588C`/`0x25892`; for the 1P/standby `b1f == 1` this
   is `DS_0010816A`). The base table at `0xA87C4` is `{0,4,2,3,6,1,5}`:
   character 0 can never give 0, and each of characters 1..6 can (for one
   `rng(6)` value). The character cursor and the select timing are the knobs;
   read `DS_00104AFC` at the vs/fight.
2. **The join keeps it.** At mode 6 press P2 start (`0x28CC8`'s join poll);
   its `0x28D02..0x28D13` arm ORs the joining side's bit into `DS_00104B1F`
   (`b1f`), and `flow_player_join` (`0x28DA4`) sets `DS_00104B17 = 2`
   (`0x28E6D`) and `DS_00104ABC = (b1f == 3) + 1` (`0x28DE7`), re-entering the
   select/vs. `flow_stage_pick` returns on `m == 2` (`0x2585D`/`0x2585F`), so
   the 1P stage word survives; the join's vs tail calls the pick (`0x430FE`)
   but it writes nothing. The fight resumes on the same stage with `b1f = 3`
   and `DS_00104ABC = 2` (the `0x4BD98` gate).
3. **The lie.** Land a 2+ hit combo (P1 on P2) to set `DS_001088B6[0]` through
   `0x3907B`; the side-0 crowd's type-0 body calls `0x4B5A8`, which turns one
   worshipper into a type-3 walk-in (target the fighter ±0x1800,
   `0x4B627`/`0x4B642`; `actor+0x34 = ±0x40`, `0x4B614`/`0x4B62F`), and its
   case-3 landing (`0x49E47`/`0x49E52`) lies it: type 4, `entry+0x1C` bit 7,
   `rng(0x3C)+0x3C` (60..119) frames. The dizzy state
   `slot+0x52 == 7 && slot+0x53 == 2` (`0x36BC8`) is the alternative supply.
4. **The eight tramples.** Put a fighter over the lying body: `0x4B69C` counts
   the contact only when `0x4B788` returns 1 (not grabbing), and the first
   trample launches the body (`vy = 0x240`, `0x4B52E`; `vx = ±0x80`,
   `0x4B507..0x4B525`). The second fighter stands at the far end of the flight;
   the entry's `+0x1F` advances on each landing-window contact until
   `+0x1F > 7` enters `0x4BD98` (mode `0x21`). The trigger is positional, not a
   strike. The practical window is `T+65..T+71` (the motion-step label; the
   prelude's pass index is one later, `T+66..T+72`, and the eight measured hit
   gaps 66, 72, 68, 70, 68, 78 and 63 make the label approximate; the 56 px
   box, `0x1790C`); the raw latch is set at `T+37` (the prelude sees it from
   `T+38`). A miss
   lands the body and zeroes `+0x1F` (`0x49FAF`). The ~142 px separation is the
   zero-`+0x42` 2 px/frame × 71 flight.

### Route A (fallback): the front-end two-human `rng(7)` draw

1. Script a two-human join and a stage-0 match (retry until `DS_00104AFC == 0`;
   the pick is `rng(7)` at `0x2587C`, so ~7 attempts), then the 2+ hit combo
   above: `0x4B5A8` turns a crowd entry into a walk-in and
   `0x49E47`/`0x49E52` lies it.
2. Drive the two fighters to the flight's ends (~142 px apart with `+0x42 = 0`,
   ±0x80 x-speed swings, `0x4B507..0x4B525`), stand over the lying body, and
   read `+0x1F` from the port; the trample is body contact, not a strike.
   Repeat until it reaches 8.
3. The eighth hit's frame sets the mode; from there the port's own replay is
   the capture's expected frame path (measured by `gp-u30-volleyball`, §10).

**The capture (either route).** A capture that reaches the mode can be pinned
like the other `gp-*` oracles. It must show the court actors' walk-on, a point
(landing or eat), the score board and the return to mode 6 to publish an oracle
claim; the mode's *content* is what R05 would become. **Volleyball truth Task 4
published it:** `data/k11-captures/gp-u30-volleyball`, `gp-u30-volleyball-oracle`
and `tools/gp_volley.py check` (record
`../docs/superpowers/plans/2026-11-01-volleyball-truth-derivations.md` §2).

Cost/risk (as planned; the measured run's actual risk was the boot frame — the
route's fight phase is RNG-seeded by the frames before the boot, so the scenario
pins `at_frame 320`, the measured `gp-u15-join` press spin, and a missed `S(319)`
fails the run): a new headless search driver plus one capture. Route B's stage is a
character/timing search (each of characters 1..6 has one `rng(6)` value that
gives 0); route A's 1-in-7 is a retry, not a blocker. The eight-window timing is
the real risk (a miss lands the entry and restarts the chain). The count is not
limited to 1, so a script that finds the phase can repeat it.

## 12. References

- `port/src/game/fight.c`: `0x4B5A8` 2509, `0x4B788` 2861, `0x4BD98` 2940,
  `0x4CB18` 2987, `0x4B470` 3045, `0x4B69C` 3101, `0x4CC0C` 4144,
  `0x4C784` 4186, `0x4C60C` 4284, `0x4CD98` 4354, `0x4BF18` 4477;
  `port/src/game/flow.c`: `flow_stage_pick` 1343, `flow_player_join` 1624,
  `0x26540` 3256, dispatch 7807, 0x2545C tail 7982.
- `docs/superpowers/plans/2026-10-30-final-truth-derivations.md` §F6 (the
  trigger geometry, the gate chain, the measured tramples and the route).
- `docs/superpowers/specs/2026-10-30-final-claims.md` R05 (closed).
- `docs/superpowers/plans/2026-11-01-volleyball-truth-derivations.md` §2 (the
  measured route, the capture, the checker and the pins).
- `port/decomp/prage.c` for the raw bodies; `mem.h` for the address model.
