# Reverse completion P7: the remaining unported direct callees and the targets outside E2 (record)

**Scope.** Track P's seventh batch (roadmap row P7 of record `2026-10-02-reverse-p1-derivations.md` §P1.3,
as corrected by P2 §P2.11, P3 §P3.11 and P6: "the unported direct callees with their callers, the targets
outside E2", 14 functions), under spec `2026-09-30-reverse-completion-design.md` §4 track P and §6.
Plan: `2026-10-04-reverse-p7-remaining-targets.md`. Recipe: E3 record §E3.10; lessons: P1 §P1.10-§P1.12,
P2-P3's reviews (ledger `.superpowers/sdd/2026-10-03-reverse-p3-move-callbacks-b/progress.md`) and P6's
findings (the `ret 4` ctx offsets, the 16-poke diffrun limit, effective word tables, mutants that read an
unseeded pointer).

**Status of the numbers.** Measured by the planner on 2026-10-04 on `main` **`e88eb44`** (P1-P6 merged), in
this worktree: the prototype is the plan's Tasks 2-7 applied in order; every output quoted below is from
that tree. The baseline gate ran on the untouched tree (`make diff-verify`: `149/149 functions VERIFIED;
391/391 mutants detected; 1 named gaps; 41/126 rows with callees closed (23 have none)`; E2 `targets 240
unported, 255 ported; supplement 131 (7 unported, 0 stale)`; `771 1203 64` / `731 731 100`). **The image**
is `build/diffrun --exe data/game/C/PRAGE.EXE --image-out FILE`: sha1
`ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's, E3's, P1-P6's). Every address and instruction below is
capstone 5.0.7 over that image (fixups applied); `unicorn` 2.1.4 runs the original side. Ghidra was not
consulted.

---

## §P7.1 The member list from the raw: the roadmap's 14

`diff_emu.static_scan(..., switches=True)` from each roadmap member, every direct callee, and the E2
evidence column (`docs/superpowers/plans/2026-10-01-reverse-e2-triage.md`):

| member | insns / blocks | direct callees | reached as | E2 |
|---|---|---|---|---|
| `0x2BDB8` | 17 / 1 | - | called at `0x21432` (0x213F4), `0x3E477` (0x3E424) | supplement, entry, leaf |
| `0x213F0` | 1 / 1 | - | a bare `ret`; stream dword (record §P1.2) | outside E2 |
| `0x213F4` | 28 / 3 | `2BDB8 2BC30` | D000 target, dword `0xE16BA` | anim-target |
| `0x3E424` | 32 / 3 | `2BDB8 2BC30` | D000 target, dword `0xE8454` | anim-target |
| `0x224EC` | 12 / 1 | `22404 339AC` | D500 target, dword `0xE4E30` | anim-target |
| `0x2BDE8` | 9 / 1 | - | called at `0x361A7`, `0x361AE` (0x36114) | supplement, entry, leaf |
| `0x36114` | 51 / 7 | `1883C 2BDE8` | D000 target, dword `0xD27E4` | anim-target |
| `0x23960` | 87 / 3 | `2AE14 5D7DC` | called at `0x23AB7` (0x23A7C) | supplement, entry |
| `0x23A7C` | 28 / 3 | `2AE14 23960` | D100 target, dword `0xE18C4` | anim-target |
| `0x3A9D8` | 35 / 1 | `33A10 188AC 2BC30` | called at `0x482B9` (0x48254) | supplement, entry |
| `0x48254` | 46 / 4 | `2BC30 3A9D8 39834` | D500 target, dword `0xED872` | anim-target |
| `0x23AE0` | 42 / 10 | `2BCF4 2A17C` | after the 7-dword table `0x23AC4`; dwords `0xD4FD4 0xE1948` | outside E2 |
| `0x29C78` | 23 / 3 | `2A17C` | after the 7-dword table `0x29C5C`; 14 stream dwords (`0xD2BDA` …) | outside E2 |
| `0x4B03C` | 88 / 22 | `2B150 2C3FC 41310 49444` | after the 6-dword table `0x4B024`; 30 stream dwords (`0xEE26E` …) | outside E2 |

**No correction to the membership.** The 14 are the roadmap's; the raw adds no member and reaches none
of them by a path the roadmap does not name (the after-table tables and the bare `ret` are the §P1.2
shapes). Six are E2 `animation-targets` rows (`0x213F4 0x3E424 0x224EC 0x36114 0x23A7C 0x48254`), four
are supplement rows (`0x2BDB8 0x23960 0x2BDE8 0x3A9D8`), four are outside E2 (`0x213F0 0x23AE0 0x29C78
0x4B03C`).

**The callees.** Five of the direct callees needed a decision (the brief's list):

- `0x2BDB8 0x23960 0x2BDE8 0x3A9D8` are members of this batch: each is ported with a `PR_SEAM`
  (`PR_SEAM(0x2BDB8u, rec, arg)`, `PR_SEAM(0x23960u, rec)`, `PR_SEAM(0x2BDE8u, rec)`,
  `PR_SEAM(0x3A9D8u, side, b)`) so the rows that call them record the arrivals; `0x2BDB8` and
  `0x2BDE8` run **real** in the rows that call them (`0x213F4`/`0x3E424` and `0x36114`), `0x23960`
  and `0x3A9D8` are **stubbed** there (their own rows cover them; `0x23960`'s stub returns a fixture
  child, `0x3A9D8`'s the caller's call-arg comparison).
- `0x2B150 0x41310 0x49444` were already ported (`set_dead`, `fighter_41310`, `actor_type_49444`)
  but had no seam: `0x4B03C`'s row stubs them, so each got `PR_SEAM(0x2B150u, rec)`,
  `PR_SEAM(0x41310u, side, delta)` and `PR_SEAM(0x49444u, rec)` as its first statement.
- `0x1883C` (`fighter_1883c`, C1's row) already had a seam and is stubbed by `0x36114`'s row;
  `0x33A10` (`fighter_ctx_swap`) runs on both sides (**allow**, as C1's `0x39FB0`/`0x3A95C` rows);
  `0x22404` (`fighter_22404`, P2's row) is stubbed by `0x224EC`'s row; `0x339AC` allow; `0x2BC30`,
  `0x2AE14`, `0x2BCF4`, `0x2A17C`, `0x2C3FC`, `0x39834`, `0x5D7DC` already had seams.

**The shapes.** Ten members run as animation targets (`anim_indirect`'s opcodes 0x10/0x11/0x15 and
`0x2AE14`'s spawn table): EAX = rec, EDX = the opcode's operand; only `0x2BDB8`'s callers pass a byte
argument and `0x3A9D8`'s caller (0x48254) passes the side in EAX and 0xF in EDX. Four are direct
entries (`0x2BDB8 0x23960 0x2BDE8 0x3A9D8`). `0x213F0` is the bare `ret` at the end of `0x21374`'s
body: its wrapper does nothing.

**The two twins.** `0x3A9D8` is `0x3A95C`'s twin (35 instructions each; the stream table 0xC9030
against 0xC8FE0) and `0x48254` is `0x482E4`'s twin (the stream 0xED87A and `0x3A9D8` against 0xED8BC
and `0x3A95C`). One C function per original function: four functions, four rows.

## §P7.2 Task 2: `0x2BDB8`, `0x213F0`, `0x213F4`, `0x3E424` (the D000 pair and their leaf)

- **`0x2BDB8`** (EAX = rec, EDX = the byte argument): `mov ah,[eax+0x2b]; or ah,2; mov [ebx+0x2b],ah`
  (a byte OR); `xor eax,eax; mov al,dl; mov [0x105BEE],dl`; `dec dl; mov [0x105BEC],dl`;
  `fild word [esp]` of the zero-extended DL then `fstp dword [ebx+0x24]`. So the record's +0x24 =
  `(float)(u8)arg`; the port stores the float's **bits** (`p2_set_f32`), not a float-to-int cast —
  the planner's first prototype wrote `(u32)(float)(arg & 0xFF)` and the row caught it (the original's
  0x42840000 against the port's 0x00000042 on b0).
- **`0x213F0`**: a bare `ret`; `fighter_213f0` is an empty function and the row asserts no byte changes.
- **`0x213F4`** (D000 target `0xE16BA`): `other = DSD(0x1077A8 + ((rec+0x51)^1)*4)`; when set,
  `ANIM_BEGIN(DSD(other), DSD(0xC9148 + other_slot_char*4), 3.0)`; `other_slot+0x58 = 0`;
  `other_slot+0x52 = 0xC`; `0x2BDB8(rec, 3)`; `0x2BDB8(DSD(other), 3)`.
- **`0x3E424`** (D000 target `0xE8454`): the twin with the table 0xC9120 and `other_slot+0x41 |= 0x80`
  between the +0x58 and +0x52 stores.

Cases: `b0` (arg 0x42), `b1` (0x100: the low byte against the whole argument for all three stores),
`b2` (0x1234FF: 255.0f) for `0x2BDB8`; `r0` for `0x213F0`; `s0`/`s1` (both sides) and `o0` (the other
slot null) for `0x213F4`/`0x3E424`. Mutants: `2bdb8@mutant` (the float from the whole argument),
`@and` (the store without the OR), `@dec` (0x105BEC = the argument), `@width` (a word store of
0x105BEE); `213f0@mutant` (a byte store); `213f4@mutant` (the own record's char for the stream),
`@stream` (0xC9120), `@side` (index `& 1`), `@order` (the stores before `0x2BC30`); `3e424@mutant`
(0xC9148), `@bit` (+0x41 = 0x80, not OR-ed), `@side`, `@order`. All 13 detected; the case sets are
`P7_KINDS` in `tools/tests/test_diff_verify.py`.

**Fix round 1 (the planner's prototype).** The row's first cases poked the other **record's** +0x7A
(and +0x58/+0x52/+0x41), but the raw reads the other **slot's** bytes (`[ebx+0x7a]` where EBX =
`DSD(0x1077A8 + …)`): both sides read an unseeded 0 and agreed. The cases now poke
`DS_SLOTS + oside*0x94 + 0x7A/0x58/0x52/0x41`; `@mutant`/`@stream` are caught by `s0`/`s1`.

## §P7.3 Task 3: `0x224EC`, `0x2BDE8`, `0x36114` (the D500 ctx target, the leaf, the anchor target)

- **`0x224EC`** (D500 target `0xE4E30`): ctx (`0x339AC`, allow); `0x22404(ctx[0])`; the own slot
  (ctx[2]) `+0x57 = 2`. The row stubs `0x22404` (P2's row) and seeds both slots' +0x57 differently.
- **`0x2BDE8`** (EAX = rec): `fld [eax+0x24]; fadd dword [0x809B8] (-1.0f); fstp [eax+0x20]`;
  `+0x2B &= ~2`. The row's cases use 3.0f, -2.0f and 1.0f and seed +0x2B 0xFF/0x02/0x03.
- **`0x36114`** (D000 target `0xD27E4`): `rec+0x24 = 2.0f`; with `rec+0x14` set: `slot+0x58 += 1`;
  `dx = ±word 0xBDA4C[slot_char]` (negated unless `rec+0x28` bit 14); `word rec+0x34 = dx`;
  `word rec+0x36 = word 0xBDA5A[slot_char]`; `0x1883C(side, (s16)+0x34, (s16)+0x36)`;
  `0x2BDE8(rec)` and, with the other slot set, `0x2BDE8(DSD(other))`.

Cases: `s0`/`s1` for `0x224EC`; `s0`/`s1`/`s2` for `0x2BDE8`; `s0` (bit 14 set: +0x34 = 0x180,
+0x36 = 0x300), `s1` (bit 14 clear: +0x34 = 0xFE80), `s2` (side 1, char 2: 0x1C0/0x100), `n0` (no
slot) and `o0` (no other slot) for `0x36114`. Mutants: `224ec@mutant` (value 3), `@side` (ctx[3]),
`@order` (the store before `0x22404`); `2bde8@mutant` (+1.0f), `@byte` (=0xFD); `36114@neg` (the
bit-14 branch inverted), `@side` (0x1883C on the other side), `@anchor` (EDX from +0x36), `@char`
(the own record's char), `@table` (0xBDA5A for +0x34), `@order` (the stores after the 0x1883C call),
`@second` (only the own 0x2BDE8). All 12 detected.

**Fix round 1.** `fighter_2bde8` had no seam and `0x36114`'s row runs it real, so the port's calls
were unrecorded ("original 0x2BDE8(0x10A300), port none"); the seam was added. The unit check's
`+0x2B` expectations were wrong (0x36114 does **not** set bit 1; 0x28 stays 0x28).

## §P7.4 Task 4: `0x23960`, `0x23A7C` (the spawn pair)

- **`0x23960`** (EAX = rec): `word 0x105B4C = 0`; with the other side's slot set, four spawns
  `0xA839C` at `x = other_slot+0x2C + {0x600, -0x600, 0x1000, -0x1000}`, `y = (s32)DSD(other_rec+0x30)
  >> 16`, z 0 and a5 0; each child's `+0x24 += (float)rng_next(6)` and `+0x20` takes the same value
  (the raw's `fst`/`fstp` pair).
- **`0x23A7C`** (D100 target `0xE18C4`): with `rec+0x14` set, one `0xA8388` spawn with
  `a5 = word rec+0x56 | 0x400`; the child's `+0x14` = the slot, `+0x51` = the side; `rec+0x4B` =
  `child+0x56`; then `0x23960(child)` (which spawns four more on the child's other slot).

Cases: `s0`/`s1` and `o0` for `0x23960`; `s0`/`s1`/`n0` for `0x23A7C`. The spawn stub returns
`P6_CHILD`; `rng_next` runs real. Mutants: `23960@off` (0x600 for all four), `@y` (y from the slot's
+0x2C), `@desc` (0xA8388), `@float` (the rng value without the add), `@x` (x from the own slot);
`23a7c@a5` (no 0x400), `@slot` (child+0x14 = the record), `@side` (1 - side), `@mutant` (rec+0x4B
from the record), `@order` (the stores after `0x23960`). All 10 detected.

**Fix round 1.** `fighter_23960` had no seam (its stub in `0x23A7C`'s row was not intercepting; the
row read "original 0x23960(0x10A900), port none"). The unit check's first rng-value expectation was
wrong because the real `actor_spawn` advances the rng before each add; it now asserts the
`+0x24 == +0x20` copy, the `[base, base+5]` domain and that some draw is non-zero.

## §P7.5 Task 5: `0x3A9D8`, `0x48254` (the two twins)

- **`0x3A9D8`** (EAX = side, EDX = b): `0x3A95C`'s twin. `fighter_ctx_swap` (allow); `0x188AC(ctx[1],
  DSD(ctx[5]+0x18), 0)`; the own slot (ctx[3]) `+0x52/+0x53/+0x54 = 0x10/0xA/0` and `+0x10 = 0`;
  `ANIM_BEGIN(ctx[5], DSD(0xC9030 + ctx[3].char*4), 3.0)`; `ctx[3]+0x7E = byte 0xBECF8 + (u8)b`.
- **`0x48254`** (D500 target `0xED872`): `0x482E4`'s twin. `ANIM_BEGIN(rec, 0xED87A, 3.0)`; when
  `0x108392[own side]` is non-zero `ANIM_BEGIN(other_rec, DSD(0xC8F40 + other_char*4), 5.0)`, else
  `0x3A9D8(other side, 0xF)` and `0x39834(other side, DSB(0x10780F + own*0x94))`.

Cases: `s0`/`s1` for `0x3A9D8` (side 0/1, the second with b = 0x80); `s0` (flag 1), `s1` (flag 0 on
side 1), `z0` (flag 0, other char 4) for `0x48254`. Mutants: `3a9d8@side` (0x188AC on the other
side), `@stream` (0xC8FE0), `@anchor` (ctx[5]+0x1C), `@slot` (the stores on ctx[2]), `@frame` (4.0),
`@byte` (b's high byte); `48254@byte` (the flag from the other side), `@side` (other = own), `@stream`
(0xED8BC), `@pose` (the other slot's +0x5F), `@char` (the own char), `@order` (the else branch's last
two calls swapped). All 12 detected.

**Fix round 1.** `fighter_3a9d8` had no seam (its stub in `0x48254`'s row was not intercepting, so
the port ran the real body and the row saw the nested `0x2BC30` call). The unit checks dropped the
`rec+8` stream-pointer assertions: the stream walk advances the pointer past the first opcodes
(0x213F4: 0xD27B2 -> 0xD27D6; 0x48254: 0xED87A -> 0xED89A), which the differential rows' call args
already pin.

## §P7.6 Task 6: `0x23AE0`, `0x29C78` (the after-table streams)

- **`0x23AE0`**: after the 7-dword table `0x23AC4`. With the other side's slot set: `orec+0x29 |= 8`,
  `orec+0x2E = 0x64`, `orec+0x4E = 1`; `ch = other_slot+0x7A`; `val = {0x46B9, 0x46BA, 0x46B8,
  0x46B6, 0x46B7, 0x46B9, 0x46BA}[ch]` (default 0x46B9); `0x2BCF4(orec, val)`;
  `0x2A17C(orec, 0, 0x105FEBC)`; `word 0x105B4C = 1`.
- **`0x29C78`**: after the 7-dword table `0x29C5C`, whose every entry is 0x29CA8: the own char's
  switch is degenerate, so the body is `0x2A17C(rec, 0, 0x105FEBC)` alone (the char load and the
  jump are dead; named below).

Cases: `c0`..`c6` and `c7` (every table entry and the default) plus `o0` for `0x23AE0`; `s0`/`s1`
and `d0` (own char 7, the degenerate switch) for `0x29C78`. Mutants: `23ae0@char` (the own record's
char), `@val` (the default for every char), `@side` (the own side's slot), `@pal` (handle 0), `@word`
(+0x2E = 0x63), `@bit` (+0x29 set), `@seek` (the seek on the record), `@order` (the stores after the
seek); `29c78@pal` (handle 0), `@rec` (the word 0xFFFF). All 10 detected.

**Fix rounds.** The palette handle is `0x105FEBC` (the raw's `mov ebx,0x105febc`); the planner's
first port wrote 0x105FECB (transposed) and the rows' call-arg comparison caught it in both
functions. `p7_23ae0_case` first poked the other record's +0x7A; the char is the other **slot's**
byte (`DS_SLOTS + 0x94 + 0x7A`), and the four case blocks were unhit until the poke moved.

## §P7.7 Task 7: `0x4B03C` (the 30-stream target) and its three seams

**`0x4B03C`**: after the 6-dword table `0x4B024`. With `rec+0x14` set: the byte
`DSB(DSD(slot+8) + 0x48) - 0x20` selects `dl` = 0x10/0x0E/0x15/0x0C/0x0A (0x0D default, above 5
unsigned); the slot's `+0x5A` (at `0x10780A + (u8)slot+0x20 * 0x94`) loses `dl` down to 0 (a
`jae`-to-0 when `dl >= v`); voice 0xD6; voice 0xCE; `actor_set_dead(rec)`; unless mode 0x104B00 is
0x22 or 0x24: `0x10889E[slot+0x21] = 1`, `0x41310(slot+0x21, -10000)`, then
`0x41310(slot+0x20, -30000)` when the two counters are equal else `+10000`; the
`0x1088A4[slot+0x20]` byte increments; when equal, `0x1088A2[slot+0x21]` increments; `0x49444(rec)`.

Cases (13): `t0`..`t5` (each type 0x20..0x25 with the slot's +0x5A above the delta), `t6` (type
0x26), `t7` (type 0x1F, wraps above 5), `z0` (delta above +0x5A -> 0), `m0`/`m1` (mode 0x22/0x24,
the 0x41310 block skipped), `eq0` (the counters equal: -30000 and the 0x1088A2 increment), `o0` (no
slot). Mutants: `@type` (the type from the slot's own +0x20), `@val` (0x0D for every type), `@sub`
(the +0x5A store as an add), `@mode` (the 0x24 check dropped), `@plus` (the +0x1088A4 index from
+0x21), `@eq` (the equality branch inverted), `@dead` (0x2B150 on the slot's +8 record), `@cam`
(0x41310's side from +0x20), `@tear` (0x49444 skipped), `@order` (the +0x1088A4 increment before the
voices), `@voice` (0xD5). All 11 detected.

**Fix round 1.** The unit check's 0x41310 assertions first used the records Z_R0/Z_R1; the raw's
target is `DSD(0x1077A8 + side*4)`, which the fixture fills with the **slots**, so the deltas land on
`Z_S0+0x3C`/`Z_S1+0x3C` (and, for side 2, the slot array's first dword). The equal-counters case's
-30000 clamps to 0.

## §P7.8 The gp scenarios: U9 drops the D000 pair, U10 drops 0x29C78

**Every gp scenario's miss set was read from `k_gp_sets` before the batch**: only `gp-u9-win` holds
`0x213F0`/`0x213F4` and only `gp-u10-ending` holds `0x29C78`; no other set holds a P7 member.

**gp-u9-win** (the rows dropped in Task 2; the capture sha256 `7dcea0f1…`, 2521 frames, pinned in the
Makefile). `make gp-win-oracle` on the prototype: `fn-miss PR_GP_DUMP distinct=4 dropped=0`
(`0x5D812` actor_spawn and set_dead, `0x29D60` and `0x5D812` from frontend_mode_1b_step), report
lines verbatim:

```
frames: window from capture 100 (raw 1744); 237 classified: 151 clean, 84 splice, 1 transition, 1 unexplained, 10 all-black
frames: first unexplained 346, ratchet N 346 ok
trace: 0 differing through 3502; ratchet N 2364 ok (every item is explained: N = 3503 is the exact pin)
path: 0 not reproduced through 7; ratchet N 8 ok
win: 3172 frames compared (f 13A..) ...; win: 0 differing through 3502; ratchet N 3162 ok (every item is explained: N = 3503 is the exact pin)
```

The pins were raised to the measured values: **TRACE_MIN_FIRST 2364 -> 3503** and **WIN_MIN_FIRST
3162 -> 3503**; MIN_FIRST 346 (347 fails), MAX_START 100 (99 fails) and MILESTONES 8 (9 is
unreachable) are unchanged and exact. Proven against the kept port dump: `--trace-min-first 3504`
fails ("N 3504 > end 3503: N is unreachable"), `--win-min-first 3504` fails the same way,
`--min-first 347` fails ("first unexplained 346 < ratchet N 347"), `--max-start 99` fails ("window
starts at capture 100 > pinned start 99"), `--min-milestones 9` fails ("N 9 > end 8").

**gp-u10-ending** (the row dropped in Task 6; the capture sha256 `a88de48a…`, 5704 frames, memsize
64). `make gp-ending-oracle` on the prototype: `fn-miss PR_GP_DUMP distinct=4 dropped=0`
(`0x5D812` actor_spawn and set_dead, `0x29D60` and `0x5D812` from frontend_mode_1b_step), and every
pin is exact and unchanged: frames 331 (`--min-first 332` fails), trace 9954 (`9955` is unreachable,
the replay's end), path 30 (`31` unreachable), win 9954 (`9955` unreachable), MAX_START 83.

**The Makefile's provenance and AGENTS.md** name both re-measures (Task 2's u9 block and Task 6's u10
block; the AGENTS.md paragraph's final clause).

## §P7.9 Decisions, named gaps and limits

**No decision is left to the user.** The member list is the roadmap's (§P7.1); the five callee
decisions follow the raw and the existing seams (§P7.1); the four seam additions follow AGENTS.md.

Named gaps and limits:

- **`0x29C78`'s dead switch** (§P7.6): every entry of the table `0x29C5C` is `0x29CA8`, so the own
  char's load and the indirect jump have no observable effect; the port omits them and the record
  says so. A mutant that reads the char differently is not observable (the row's `d0` case exercises
  char 7 through the same path).
- **The stubbed callees in the rows** (D3): `0x2BC30 0x2AE14 0x2BCF4 0x2A17C 0x2C3FC 0x39834 0x188AC
  0x22404 0x1883C` have their own C1/C2 rows (or, for `0x1883C`, C1's) and are stubbed here;
  `0x2B150 0x41310 0x49444` gain seams and join C2 (their rows are open). `0x33A10` and `0x339AC`
  run on both sides (allow). The closed-row count rises by four (`0x213F4`, `0x3E424`, `0x36114`,
  `0x224EC` — their callees have VERIFIED rows); `0x23A7C`, `0x3A9D8`, `0x48254`, `0x23AE0` and
  `0x4B03C` stay open on stubs without rows.
- **The stream walks in the unit checks** advance the records' `+8` pointers past the first opcodes
  (§P7.5); the unit checks do not assert the pointer and the differential rows' call args carry the
  stream claim.
- **`0x23960`'s unit check and the rng**: the real `actor_spawn` draws from the rng before each add,
  so the check asserts the `+0x24 == +0x20` copy, the `[base, base+5]` domain and a non-zero draw,
  not the exact values; the differential row (spawn stubbed, rng real) pins the rng call args.
- **Outside P7:** the `title_pin` unittest failure on this tree is pre-existing (P2 §P2.13) and
  outside `make verify`; not touched here.
- E3's, P1's, P2's, P3's and P6's limits stand: seeds are hand pokes; the memory at a call is mem[]
  only; the callee column is one level deep.

## §P7.10 Results

The per-task gates (the harness self-check, the unit suite, `make entry-triage` with the regenerated
table, and the gp oracle a task touches). The counter after each task is the measured base plus this
plan's increments (the planner prototyped the whole batch at once, so the per-task lines are the
final SPECS' arithmetic, as P6's record notes; the final line and every E2/gp line were measured):

| after | diff-verify counter | E2 |
|---|---|---|
| base `e88eb44` | `149/149 functions VERIFIED; 391/391 mutants detected; 1 named gaps; 41/126 rows with callees closed (23 have none)` | `targets 240 unported, 255 ported; supplement 131 (7 unported, 0 stale)` |
| Task 2 | `153/153; 404/404; 43/128 (25 have none)` | `238 / 257`, supplement (6 unported) |
| Task 3 | `156/156; 416/416; 44/130 (26 have none)` | `236 / 259`, supplement (5) |
| Task 4 | `158/158; 426/426; 44/132 (26)` | `235 / 260`, supplement (4) |
| Task 5 | `160/160; 438/438; 44/134 (26)` | `234 / 261`, supplement (4) |
| Task 6 | `162/162; 448/448; 45/136 (26)` | unchanged |
| Task 7 | `163/163 functions VERIFIED; 459/459 mutants detected; 1 named gaps; 45/137 rows with callees closed (26 have none)` | unchanged |

The 14 rows, all VERIFIED, as the final table prints them (cases, blocks hit/total, callees):

```
fighter_2bdb8  3  1/1   -
fighter_213f0  1  1/1   -
fighter_213f4  3  3/3   2BC30 stub VERIFIED, 2BDB8 real VERIFIED
fighter_3e424  3  3/3   2BC30 stub VERIFIED, 2BDB8 real VERIFIED
fighter_224ec  2  1/1   22404 stub VERIFIED, 339AC allow VERIFIED
fighter_2bde8  3  1/1   -
fighter_36114  5  7/7   1883C stub unverified, 2BDE8 real VERIFIED
fighter_23960  3  3/3   2AE14 stub unverified, 5D7DC real VERIFIED
fighter_23a7c  3  3/3   23960 stub VERIFIED, 2AE14 stub unverified
fighter_3a9d8  2  1/1   188AC stub VERIFIED, 2BC30 stub VERIFIED, 33A10 allow unverified
fighter_48254  3  4/4   2BC30 stub VERIFIED, 39834 stub unverified, 3A9D8 stub VERIFIED
fighter_23ae0  9 10/10  2A17C stub VERIFIED, 2BCF4 stub unverified
fighter_29c78  3  3/3   2A17C stub VERIFIED
fighter_4b03c 13 22/22  2B150 stub unverified, 2C3FC stub unverified, 41310 stub unverified, 49444 stub unverified
```

P7's 68 mutants: what alone catches each is pinned by `P7_KINDS` in
`tools/tests/test_diff_verify.py` and its case lists. No P7 row has an unhit block or an
outside-image read. `PR_ORACLE_REQUIRED=1 ./build/run_tests`: **all checks passed** (the six
`test_p7_*` checks included). `python3 tools/port_progress.py` stays `771 1203 64` / `731 731 100`
(none of the 14 is a Ghidra `FN_`) and README does not move.

**The full gate (the planner's, on the final prototype state, `T=p7` with the parallel-safe
overrides):** `EXIT=0`; the 45 oracle lines equal to
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt` (`ORACLES-EQUAL`); the
`make audio-render` WAV byte-identical to `before-t2.wav` (`WAV-SAME`); `symbols.h` regenerated
byte-identical; every `run_tests` pass `all checks passed`; the diff-verify Python suite 101 tests
OK and the E2 suite 44; and

```
diff-verify: 163/163 functions VERIFIED; 459/459 mutants detected; 1 named gaps; 45/137 rows with callees closed (26 have none).
entry-triage: targets 234 unported, 261 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
```

with every gp ratchet at its pin: gp-idle-loss 2064 / 8320, gp-u5-charsel 516 / 1513, gp-u6-moves-b
1005 / 2262 / moves 2949, gp-keys-fight 11, gp-twop 612 / 1506 / 1506, the seven U8 scenarios as at
`e88eb44` (RA 1072 / 2274, LT 1076 / 2338, RT 1098 / 2402, TW 1107 / 2466, HC 1022 / 2274, EN 278 /
1174, AS 1087 / 2018), **gp-u9-win 346 / 3503 / path 8 / win 3503** and **gp-u10-ending 331 / 9954 /
path 30 / win 9954**.

**The rows' store observability:** every field a row writes is seeded with a sentinel different from
the write and its neighbours are seeded nonzero and distinct (the `0x105BEC/BEE` bytes, the records'
`+0x24/+0x2B`, the slots' `+0x52..+0x58`, `+0x7A`, `+0x7E`, `+0x41`, the `0x10889E/0x1088A2/0x1088A4`
counters, the records' `+0x29/+0x2E/+0x4E`, the `+0x5A` byte and the `+0x3C` camera dwords), and the
`@width`/`@and`/`@sub`/`@bit`/`@eq`/`@mode`/`@plus` mutants each break one of those stores and are
caught by the cases `P7_KINDS` pins. `0x213F0`'s row asserts the absence of any write. No row has an
unhit block, so no `unhit_named` entry is needed.

## §P7.11 The roadmap after P7

P7 **14** as listed; C2 gains the three seam callees (`0x2B150 0x41310 0x49444`; `0x1883C` already had
its C1 row). P8 keeps its 16 + triage. The U9 re-measure list (AGENTS.md, Makefile) loses `0x213F0`
and `0x213F4`; the U10 list loses `0x29C78`. The four outside-E2 functions (`0x213F0 0x23AE0 0x29C78
0x4B03C`) move the supplement only where they are supplement rows; `0x213F0`, `0x23AE0`, `0x29C78`
and `0x4B03C` are outside E2's universe and move no E2 count.
