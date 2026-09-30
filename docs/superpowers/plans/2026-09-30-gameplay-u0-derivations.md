# Gameplay ground truth unit U0: the missing-function audit (raw-byte derivation)

**Scope.** Unit U0 of the gameplay ground-truth effort (scoping report
`.superpowers/sdd/2026-09-30-gameplay-scope/report.md`, finding 5): the 14
table-reached functions the all-gaps ledger names as unported
(`2026-09-29-all-gaps-ledger.md` §H.4: `0x37774 0x3D4DC 0x3D8AC 0x45C10
0x47BFC 0x20FE0 0x21EA4 0x21DA4 0x2208C 0x3F450 0x45B43 0x1DC5C 0x2EE41
0x4F638`) and the voice calls in code the port does not have (record k7-k12
§1.2). Branch `gameplay-ground-truth` (worktree), base `934992a`.

**Tooling.** The Ghidra MCP was not used. Every address was read from the
fixed-up LE image the port's own loader writes (`mem_load_le(exe,
object_bin_out)`, fixups applied, `0x10000..0x10B0D0`) with capstone 5.0.7
in 32-bit mode. Function extents: `port/decomp/prage.functions.csv`. Scans:
rel32 `call`/`jmp`/`jcc` over the code object below the runtime
(`0x10000..0x5D000`), dwords over the whole image, and capstone operands
(immediates and displacements) of every Ghidra function.

**Verdict.** The 14 are not 14 missing functions: 10 are members of 7
unported move clusters or render/animation entries, 4 are call-site
addresses inside unported code, and one of those (`0x2EE41`) is dead. U0
ports 40 functions from the raw (the 14 resolved: 13 ported, `0x2EE41` dead)
and adds a miss log that records every unresolved code pointer in real play.
The audit found the ledger's list incomplete: 27 of the 72 move-table
callbacks and 6 of the 10 finisher entries are still unregistered (§U0.12).

---

## §U0.1 The miss log (`mem.c`, `mem.h`, `PORT:`)

`fn_resolve(x)` is a macro for `fn_resolve_from(x, __func__)`: the plain
lookup, plus, while the log is armed, one record per distinct (non-zero
unresolved address, calling function) pair with a hit count (64 pairs; more
count as dropped). Address 0 is not a miss (every caller treats 0 as "no
callback"). Inert until `fn_misslog_arm(1)`: the unit suite and the windowed
game record nothing unless asked.

Armed by: every env-gated driver (`run_tests.c` arms before the driver and
reports after it: `PR_ATTRACT_DUMP`, `PR_TITLE_DUMP`, `PR_FRONTEND_DUMP` and
through it `PR_FRONTEND_DET`'s two children, `PR_K11_DUMP`, `PR_RESTART`),
the `--check` run (`main.c`, report on stdout and `frames/fn_miss.txt`), and
`PR_FN_MISSLOG` for the windowed run (report at exit). Report lines:
`fn-miss <tag> 0xADDR <caller> hits=N`, then `fn-miss <tag> distinct=N
dropped=N`.

The callers' behaviour is unchanged (the same NULL). The gate (§U0.13) shows
the oracle lines identical before and after.

## §U0.2 The pinned known-set (measured)

Every stock driver run records exactly:

| address | caller | what it is |
|---|---|---|
| `0x5D812` | `actor_spawn` | the runtime's `xor eax,eax; ret` stub (`0x5D812..0x5D814`), the type table's empty callback halves; deliberately unregistered (actors.c), its return discarded |
| `0x5D812` | `set_dead` | the same stub in the teardown half |

and `PR_FRONTEND_DUMP` adds `0x41578` from `test_frontend` (hits 1): the
driver's own unit probe `fn_resolve(FN_00041578) == NULL` ("direct-called
only, not registered", `test_game.c`), which runs in the same process before
`game_init()`.

Measured hit counts (`make verify` at `9b099f9`): `--check 820` 7504/7359;
`PR_RESTART` 891/846; `PR_TITLE_DUMP` 7198/7053; `PR_ATTRACT_DUMP` 8098/7954;
`PR_K11_DUMP` walk 10025/5257, menuesc 4548/4500; `PR_FRONTEND_DUMP` (the
demo fight) 37608/36840. Also measured and identical in kind: the K11
`idle`, `diags` and `de` scenarios and `--check 5`/`--check 60`.

**Consequence.** No stock driver ever resolves any function this unit ports:
the attract, title, front-end, demo fight, service menu and restart reach
none of them. Porting them cannot move an oracle line, and did not.

The check: `test_fn_misslog_driver(env)` (`test_platform.c`) after each
driver (the exact pair set, `dropped == 0`); `test_fn_misslog` reads
`frames/fn_miss.txt` (skips with a message when `--check` has not run).

## §U0.3 What the 14 are

| ledger | what the raw shows | reached through | U0 |
|---|---|---|---|
| `0x37774` | the reaction-0x33 callback of **every** character (dwords `0xA3924 + 0x500*c`, c = 0..6), not character 2's | 0x34E2C's move table | ported §U0.4 |
| `0x45C10` | character 4's `0xBDAE4` finisher entry (dword `0xBDAF4`) | `DS_001078E8`, set by `0x37774` | ported §U0.5 |
| `0x1DC5C` | a `call 0x1DA84` inside `0x1DC0C`, the render table's bit-3 entry (dword `0xA86D0`) | `DS_000A86C4[3]` | `0x1DC0C` ported §U0.6 |
| `0x4F638` | a `call 0x4DBB4` inside `0x4F5C8`, the render table's bit-4 entry (dword `0xA86D4`) | `DS_000A86C4[4]` | `0x4F5C8` ported §U0.6 |
| `0x45B43` | a `call 0x37B54` inside `0x45B18`, the 0xD100 target at `0xEB70E` | animation opcode 0x11 | `0x45B18` ported §U0.7 |
| `0x47BFC` | character 2's reaction-0x26 callback (dword `0xA4220`) | move table | ported with its hooks §U0.8 |
| `0x3D4DC` | the +0x1C callback of character 5's reaction-0x20 setter `0x3D73C` (code immediate `0x3D774`) | slot +0x1C | cluster ported §U0.9 |
| `0x3D8AC` | the +0x1C callback of character 5's reaction-0x26 setter `0x3DA10` (`0x3DA47`) | slot +0x1C | cluster ported §U0.9 |
| `0x20FE0` | the +0x1C callback of character 6's reaction-0x23 setter `0x210C4` (`0x21108`) | slot +0x1C | cluster ported §U0.10 |
| `0x21EA4` | the +0x1C callback of character 6's reaction-0x22 setter `0x2201C` (`0x22081`) | slot +0x1C | cluster ported §U0.10 |
| `0x21DA4` | direct-called only, at `0x21FF0` in `0x21F88` (that setter's +0x0C) | `call` | ported §U0.10 |
| `0x2208C` | direct-called only, at `0x22268` in `0x22200` (character 6's reaction-0x27 +0x0C) | `call` | ported §U0.10 |
| `0x3F450` | direct-called only, at `0x3F624` in `0x3F5BC` (character 0's reaction-0x2E +0x0C) | `call` | ported §U0.10 |
| `0x2EE41` | a `call 0x2EB80` inside `0x2EE3C` (scan codes 0x48..0x50/0x0D/0x1B to pad bits, jump table `0x2EE18`) | **nothing**: no rel32, dword or immediate names `0x2EE3C` in either object | dead; not ported (§U0.11) |

So the ledger's "14 table-reached" is corrected (raw wins): 7 are table- or
pointer-reached entries, 3 are +0x1C callbacks stored by unported setters, 3
are direct-called helpers of unported callbacks, 4 are call sites, one dead.
A port of a leaf alone would be dead code (its setter still missing), so U0
ports the whole cluster each belongs to.

Record k7-k12 §1.2 placed `3D643` at "`0x3D57D` (block start)" and `3D9D7`
at "`0x3D94D`": both are jump targets inside `0x3D4DC` and `0x3D8AC` (the
`r == 0` arms at `0x3D51C je 0x3D57D` and `0x3D8EC je 0x3D94D`).

## §U0.4 The finisher starters `0x37640`, `0x37774`, `0x37898`

The (c, 0x32), (c, 0x33), (c, 0x34) entries of `0xA3528` for every c
(`0xA3910/0xA3924/0xA3938 + 0x500*c`). EAX = slot, EDX = rec; the side in EBX
is not read. Listing `0x37640..0x379BC`. Common shape:

1. `cmp byte [0x105B3A],0; ja` return AL 0; other = `[0x1077A8 + ((rec+0x51)^1)*4]`,
   0 returns; other `+0x42 & 0x20` and `+0x43 & 8` must be set, else return.
2. voices `0xE4`, `0xE0` (`0x37687/0x37691`, `0x377B9/0x377C3`, `0x378DE/0x378E8`).
3. `DS 0x1078E4` = `[T1 + char*4]`, word `DS_001078F6` = 0, slot `+0x42 |= 0x80`,
   `DS_001078E8` = 0 (`0x37640`) / `[0xBDAE4 + char*4]` / `[0xBDB00 + char*4]`,
   and `0x2B150([0x1078EC])` when `DS_00104529 & 2`.
4. The arm by the slot's char: flags arm `DS_000F0AFE = 2`, slot `+0x40 |= 0x400200`;
   else `0x37D18(other, [other])` and slot `+0x41 |= 2`. `0x37640`: flags for 1
   and 3 (`0x376D8 cmp al,1; jb; jbe; cmp al,3; je`); `0x37774`: 0x37D18 for 6
   only (`0x3780B cmp al,6; je`); `0x37898`: flags for 3 and 5 (`0x37930..0x37938`).
5. `DS_00104B02[other's char] |= (rec+0x51 ? 0x40 : 0x80)`; slot `+0x53 = 3`,
   `+0x41 |= 0x10`; `DS_001078DC` = T2; `rec+0x59 = [T3 + char]`;
   word `DS_00104AF8 = 0x384`; `0x41310(rec+0x51, 50000)`; AL = 1.

| | T1 (`0x1078E4`) | T2 (`0x1078DC`) | T3 (`rec+0x59`) |
|---|---|---|---|
| `0x37640` | `0xC9288` | `0xBD8FE` | `0xBDA24` |
| `0x37774` | `0xC92B0` | `0xBD960` | `0xBDA2B` |
| `0x37898` | `0xC92D8` | `0xBD9C2` | `0xBDA32` |

The finisher entries (`0xBDAE4`, `0xBDB00`, per char): `0 0 48BE0 1567C 45C10 0 23BF8`
and `402FC 0 48F54 15908 45D14 40BBC 23EC0`.

## §U0.5 `0x45C10`

`0x45C10..0x45C53`: `0x2BC30(rec, 0xEB7A0, push 0x40400000)`, slot `+0x53 = 7`,
`+0x52 = 9`, `+0x54 = 0`, `+0x0C = 0x45B50`, `+0x57 = 0`, `+0x18 = +0x1C = +0x14 = 0`;
AL = 1. `0x379C4` tests the whole EAX (`0x379EE`), so the port returns 1 as it
does for `0x48BE0`.

## §U0.6 The render table's bit-3 and bit-4 entries

`DS_000A86C4` = `4F4E8 1D540 5D812 1DC0C 4F5C8 5D812...`. Bit 3 is set by
`0x1DBFF` (in `0x1DAE8`) and `or` stores at `0x26AC9`, `0x26B4C`, `0x26C54`,
`0x26C7F`, `0x26D40` (bits 3 and 4); no stock driver sets them (§U0.2).

`0x1DC0C..0x1DC6B`: side = `DS_00104B1A`; want = `[0x10780B + side*0x94]`
(the slot's +0x5B), shown = `[0x10290C + side]`; `want > shown` (unsigned `jbe`)
stores shown+1, `want < shown` (`jae`) stores shown-1, and either calls
`0x1DA84(new, side)` at `0x1DC5C`; then `and [0x104AEC],0xF7`.

`0x4F5C8..0x4F641`: `DSW(0x104AF4)` zero-extended, `cdq`-style `sar edx,31`,
`idiv [0x1088D0]`; a non-zero remainder or `DS_00104AC4 == 0` returns; else
`DS_00104AC4 -= 1` and `0x2F528(0x13, 1, count, 2, push 0, push 0x4000)`
(the same register/stack order as `0x437FE`), then
`0x4DBB4(0x1077B0 + DS_00104B1A*0x94, 1)` at `0x4F638`. A zero divisor would
fault the `idiv` (`0x4F5E0`): **`PORT:`** return, as `0x4F4E8` (record §49-Z).

## §U0.7 `0x45B18`

`0x45B18..0x45B4C` (the 0xD100 word at `0xEB70C`, its dword at `0xEB70E`, the
only reference): `0x2AE14(0xC934C, 0, 0, 0, (rec+0x56)|0x400)`, `rec+0x4B =`
the child's `+0x56` low byte, `0x37B54(rec)` at `0x45B43`.

## §U0.8 Character 2's reaction 0x26: `0x47BFC`, `0x478D4`, `0x47984`

`0x47BFC`: ctx = `0x339AC(rec)`; nothing unless the signed word at
`0x107D2C + side*2` (`[0x107D2A + side*2] sar 16`) is at least 1; then
`0x3C4CC(ctx[4], [0xC8950 + ctx[2].char*4], 2.0)`, slot 9/7/0, `+0x57 = 5`,
`+0x0C = 0x47B04` (ported, §49-Z), `+0x18 = 0x478D4` (`0x47C5D`),
`+0x1C = 0x47984` (`0x47C66`).
`0x478D4`: pass 1 flags 1/4/8/0xD/0xE = 0 and 2/5/9/0xA/0xC = 1 on boxes
`0xC9438`/`0xC944C` (EBX/ECX); a non-zero result is returned (`0x47930 jne`);
pass 2 from fresh flags: 1/4/5/8/0xD/0xE = 0 and 2/9/0xA/0xC = 1 (CH, which
`0x18BD4` keeps) on `0xC9442`/`0xC944C`.
`0x47984`: `0x3C480(ctx[4], 0xED944, 2.0)`, `0x34D8C(ctx[0])`, `0x18AF8`,
`0x3C148(0)`, `0x3C148(1)`, ctx[5] `+0x28 |= 0x20` and `+0x24 = 0`,
ctx[2] `+0x57 = 0`, `0x39A10(ctx[4], 0x29A)`, `0x39A10(ctx[5], 0x29A)`.
Box bytes: `0xC9438` A0x7, `0xC9442` 43x7, `0xC944C` 63x7.

## §U0.9 Character 5's reactions 0x20 and 0x26

Setters `0x3D73C` (dword `0xA50A8`) and `0x3DA10` (`0xA5120`): `0x3C4CC(rec,
0xD4B1E / 0xD4BD8, 3.0)`, slot 9/7/0, `+0x57 = 0`, callbacks `0x3D674/0x3D484/
0x3D4DC` and `0x3D9E4/0x3D858/0x3D8AC`; `0x3D73C` also `rec+0x4C = 0x78`.

- `0x3D674` (+0x0C): `+0x57 != 0` or no other slot returns; while
  `DSW(0x1088E0 + side*2) & 0x500 == 0x500` and the other slot lacks
  `+0x42 & 0x10`, `rec+0x4C -= 1` and `jg` returns; then `0x2BC30(rec, 0xD4B50,
  2.0)`, `0x3D3AC(rec)` when `rec+0x4B`, `+0x57 += 1`.
- `0x3D3AC`: `0x2BC30(DS_001014F4 + rec.4B*0x68, 0xD4E5C, push 0)`, voice `0x4F`.
- `0x3D484`/`0x3D858` (+0x18): flags 1/4/8 = 0 (and `0x3D484`'s 0xC = 0),
  5/9/0xB = 1; boxes `0xC75B8`/`0xC75C2` and `0xC75CC`/`0xC75D6`.
- `0x3D4DC`/`0x3D8AC` (+0x1C): `0x3AFC4` triple, `r = 0x3B298(ctx[1], +0x5F)`.
  `r != 0`: `0x3B080(ctx[1], b3, b2, 1)` unless ctx[3] `+0x54 == 2`, clears,
  `0x3AD98`. `r == 0`: `0x39834`, ctx[3] 0x10/0x0A, `+0x10 = 0x3D424` (`0x3D4DC`)
  or `0x3D790` with `+0x58 = 0` (`0x3D8AC`), `+0x14 = 0x3D3E4`; `0x3D4DC`
  then ctx[5] `+0x4C = 0x2D`, `0x188AC(ctx[1], ctx[5].x, 0)`, `0x3C148(ctx[1])`,
  `0x3C4CC(ctx[5], [0xC8A18 + ctx[3].char*4], 2.0)` (ECX = ctx[2] at `0x3D653`
  is read **after** the push, so `[esp+0xC]` there is ctx[2]); `0x3D8AC`
  ctx[4] `+0x61 = 0`; both `0x13D4C(ctx[5]'s pset +0x18, 1)` and voice `0xB7`;
  `0x3D4DC` ends `0x2BC30(ctx[4], 0xD4B50, 2.0)`, `0x3D3AC(ctx[4])`, ctx[2]
  `+0x57 += 1`.
- `0x3D790` (+0x10, case 10): ctx = `0x33A68(rec)`; slot `+0x58` 0 -> 1; 1:
  `0x3AA54(ctx[3])`, `0x3C480(ctx[5], [0xC9080 + char*4], 3.0)`, 2; 2: once
  `rec+0x36 == 0`, `0x13C70(pset+0x18, 4, 0x29C08(rec.51, char))`, `+0x14 = 0`,
  9/4, `rec+0x43 = [0xBD89A]` (8).
- `0x3D9E4` (+0x0C): `cmp dx,[ctx[2]+0x88]; jge` with `dx = [0xC75E0]` (0x28):
  ctx[2] `+0x8A = 0` when 0x28 is below the signed word.

`0x3D6E0` (voice site `3D730`) has no reference in either object: dead.

## §U0.10 Characters 6 and 0: the `0x3F3F4` twins and `0x210C4`

`0x2201C` (char 6, r22), `0x22294` (char 6, r27) and `0x3F650` (char 0, r2E)
are twins of the ported `0x3F3F4` cluster:

| | setter stream | hold word | +0x0C | landing | +0x18 | +0x1C pose |
|---|---|---|---|---|---|---|
| `0x2201C` | `0xE1724` (0x3C4CC), ctx[4] `+0x36/+0x44 = 0x280/0x20` | `0x104760` | `0x21F88` | `0x21DA4`: 0x21F30, anchor, `0xE1754`, `+0x61 = 1`, `+0x34/36/44 = 0` | `0x21E10` | `0x21EA4`: 0x18B04, `0x3C404(ctx[0], 0x14)`, `0x39F40(ctx[1], v, 0x78, 0x1A, 0x24)`, ctx[3] `+0x68 -= 1` |
| `0x22294` | `0xE17C0` (0x3C520), 0x3C16C, 0x3C148, `+0x36/+0x44 = 0x12C/0x37` or `0x21C/0x28` by the signed `0x107D2C` word | `0x10475C` | `0x22200` | `0x2208C`: 0x21F30, anchor, `0xE17DC`, `+0x61 = 1`, 0x3C148, 0x3C16C | `0x220F4` | `0x22188`: `0x39F40(ctx[1], 0x28, 0xA5, 0x17, 0x1D)`, `+0x68 -= 1` |
| `0x3F650` | `0xE7CEE`, as `0x22294` | `0x10809C` | `0x3F5BC` | `0x3F450`: 0x3F308, anchor, `0xE7D20`, as `0x2208C` | `0x3F4B8` | `0x3F54C`: `0x39F40(ctx[1], 0x14, 0xA5, 0x17, 0x1D)`, no `+0x68` |

Every +0x0C is `0x3F360`'s machine (0: a negative `+0x36` becomes 0xFFFF with
gravity 0x3C; 1: landing below `[0xBD882 + char*2] sar 16` while ctx[4] falls;
3: the hold word + 1; 2 and above 3: `0x62003(1)`, **`PORT:`** out of scope as
at `0x3F3E4`). Every +0x18 is `0x3F1F0`'s (1 while ctx[4]'s `+0x61` is clear;
flags 1/4/8 = 0 on the default boxes; 1 for character 1 in state 7 on
reaction 0x20/0x27, or the hold word above 3). Every +0x1C's `r != 0` arm is
`0x3B714(ctx[3], ctx[2])`.

`0x210C4` (char 6, r23): `0x339AC` (unread), `0x3C4CC(rec, 0xE15DC, 3.0)`, slot
9/7/0 `+0x57 = 0`, `0x210A4/0x20FA0/0x20FE0`. `0x210A4`: with `+0x57 == 1` and
`0x35E40(slot)` (landing word `>=` the slot's `+0x30`, signed `jl`, and the
record's `+0x36 <= 0`, signed `jg`), the record's `+0x36/+0x44 = 0`.
`0x20FA0`: flags 1/8 = 0 and 0 = 1 on the default boxes. `0x20FE0`: ctx[3]
`+0x68 -= 1`, ctx[2] `+0x18 = 0` and `+0x42 &= 0xFB`, `0x3A2A0(ctx[0], -100,
0x82, 0x12, push 0x14)`, `0x3C148(ctx[1])` when ctx[3] `+0x53 == 1`.

## §U0.11 Not ported, with evidence

- `0x2EE3C` (containing `0x2EE41`) and `0x3D6E0`: no rel32 `call`/`jmp`/`jcc`,
  no dword in either object and no code immediate names them. Dead code.
- `tools/port_classification.txt` is unchanged: it holds only Ghidra (`FN_`)
  rows (ledger §H.4), and none of U0's addresses is a Ghidra function, so the
  counter (`port_progress.py`: `771 1203 64` / `731 731 100`) is unchanged.

## §U0.12 What is still missing (named gaps with evidence)

- **27 of the 72 distinct move-table callbacks** (`0xA3528 + (c*64 + r)*20`)
  are unregistered, so those moves do nothing in real play (the log records
  them): `14EF8 14F50 15478 21114 21374 22938 22A00 231C0 23208 237D0 2381C
  3C048 3D10C 3D1EC 3DADC 3DB34 3DCEC 3F0A8 475EC 47608 47624 47720 47874
  47FCC 48608 48964 489A0` (before U0: 37 of 72).
- **6 of the 10 finisher entries**: `0x1567C`, `0x15908` (char 3), `0x23BF8`,
  `0x23EC0` (char 6), `0x402FC` (char 0), `0x45D14` (char 4). `0x379C4` falls
  to its `0xC9260` start when `fn_resolve` misses them (logged).
- **The wider non-Ghidra body.** A dword/immediate scan finds 575 plausible
  entries (after `ret`/padding) in no Ghidra function; besides the above,
  animation-opcode targets (dwords in the `0xD2xxx..0xEDxxx` streams, e.g.
  `0x241A8`, `0x37DCC`, `0x400E0`) and the sprite span writers `0x521DC..
  0x5CF00` (the table `0x80CB0..0x81308`, read by code at `0x57FE7..`), which
  no port code dispatches through `fn_resolve`.
- **Voice sites.** Of record k7-k12 §1.2's 75 outside sites, 9 are now in
  ported code and play: `37687 37691 377B9 377C3 378DE 378E8` (§U0.4), `3D3DB`,
  `3D643`, `3D9D7` (§U0.9). 66 stay outside (`3D730` is dead).
- **Unobservable in the unit fixtures:** `0x3D4DC`'s `0x188AC(ctx[1], x, 0)`
  (`0x3D5CE`): `0x39834`'s pose already leaves record 1 at y 0 and slot 1
  latched (mutation D11); `0x3F450`'s `0x188AC(ctx[0], rec.x, 0)`
  (`0x3F475`): its own `0x3C480` start zeroes the same y (mutation T23; the
  ported `0x3F184`'s test has the same limit). Both calls follow the raw; no
  test proves them.

## §U0.13 Tests, mutations and the gate

- `test_fn_misslog` (`test_platform.c`): seeded checks (disarmed, 0,
  registered, one miss, a second hit, a second caller, a second address, the
  64-pair cap and `dropped`, re-arm) plus the `--check` file. Mutations: no
  record, address-only dedupe, disarm ignored, 0 recorded, a pinned pair
  removed (driver and file) — all fail it.
- `test_table_reached` (`test_fight.c`): `check_u0_finishers`,
  `check_u0_45c10`, `check_u0_hud_bar_step`, `check_u0_bonus_count_step`,
  `check_u0_45b18`, `check_u0_47bfc`, `check_u0_c5`, `check_u0_twins`. The
  +0x18 hooks run against references that state the raw's flag bytes and
  boxes over 1000 fuzzed states (`u0_hook_diff`: return value, whole data
  object and fight scratch), with counters proving both results and the
  second pass occur. The +0x1C `r != 0` arms compare with a reference
  `0x3B298` + `0x3B714` run (as `0x3F284`'s H2).
- Mutations (scratch `mutbatch.py`, each applied alone, rebuilt, suite run):
  finishers 10/10, `0x45C10` 4/4, render entries 9/9, `0x45B18` 4/4,
  `0x47BFC` cluster 10/10, character 5 18/19 (D11 above), the twins and
  `0x210C4` 26/27 (T23 above); the seam 6/6. In all 87 of 89 fail.
- The existing `check_slot_hook` used `0x3D484` as its example of an
  unregistered +0x18 hook; U0 registers it, so the example is now
  `SH_UNREG_HOOK` (`0x00F0F010`, outside both LE objects). Same assertions,
  same count.
- Gate: `make verify` exits 0 with the oracle lines equal to
  `k7-k12/scratch/oracle-lines-base.txt` (diff empty) at `40c710e` (the seam),
  `9b099f9` and the final commit.
