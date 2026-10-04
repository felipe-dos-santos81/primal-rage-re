# Reverse completion P6: animation targets C (record)

**Scope.** Track P's sixth batch (roadmap row P6 of record `2026-10-02-reverse-p1-derivations.md` §P1.3, as
corrected by P2 §P2.11 and P3 §P3.11: "animation targets C", 18 functions, including P3's streams' targets
`0x47E04 0x47E30 0x482E4 0x48374`; `0x48254` stays P7), under spec
`2026-09-30-reverse-completion-design.md` §4 track P and §6. Plan: `2026-10-03-reverse-p6-animation-c.md`.
Recipe: E3 record §E3.10; lessons: P1 §P1.10-§P1.12 and the P3 reviews (ledger
`.superpowers/sdd/2026-10-03-reverse-p3-move-callbacks-b/progress.md`: the ctx offsets after a callee's
`ret 4`, the 16-poke diffrun limit, mutants that read an unseeded pointer, `fn_resolve` returning the
wrapper not the body).

**Status of the numbers.** Measured by the planner on 2026-10-04 on `main` **`f5b5556`** (P1-P3 merged), in
this worktree: the prototype is the plan's Tasks 2-9 applied in order; every output quoted below is from
that tree. The full `make verify` baseline ran on the untouched tree (25 min; the 45 oracle lines equal to
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`, the `make audio-render` WAV equal to
`before-t2.wav`, `78/78 functions VERIFIED; 158/158 mutants detected; 1 named gaps; 11/64 rows with callees
closed (14 have none)`, E2 `288/207`, `771 1203 64` / `731 731 100`). **Re-baseline note:** P4+P5 (and C1,
if it merges first) execute before this plan; if their merges move a counter or a pin, Task 1 records the
measured value in the ledger and every later expected counter adds this plan's increments to it (functions
+5,+3,+2,+2,+2,+1,+2,+1; mutants +15,+13,+7,+9,+11,+3,+12,+8; closed rows +1 then 0; rows with callees
+5,+3,+2,+2,+2,+1,+2,+1; E2 ported targets +5,+1,+1,+1,+1,+1,+2,+6). **The image** is
`build/diffrun --exe data/game/C/PRAGE.EXE --image-out FILE`: sha1
`ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's, E3's, P1-P3's). Every address and instruction below is
capstone 5.0.7 over that image (fixups applied); `unicorn` 2.1.4 runs the original side. Ghidra was not
consulted.

---

## §P6.1 The member list from the raw: the roadmap's 18

`diff_emu.static_scan(..., switches=True, resolved=E.RESOLVED_JUMPS)` from each roadmap member, every
direct callee, every code immediate, and the E2 evidence column (`docs/superpowers/plans/
2026-10-01-reverse-e2-triage.md`; all 18 are `animation-targets` rows):

| member | insns / blocks | direct callees | reachable as |
|---|---|---|---|
| `0x22338` | 56 / 7 | `2BC30 2C3FC 339AC 39280 39834 39F40` | D500 target, dword `0xE4E46` |
| `0x22494` | 23 / 1 | `2BC30 2C3FC 339AC 39834 3A95C` | D500 target, dword `0xE4E1A` |
| `0x22A40` | 41 / 5 | `13244 2AE14 2C3FC` | D100 target, dword `0xE154A` |
| `0x23F10` | 72 / 3 | `2A148 2AE14 2BC30 2C3FC` | D100 target, dword `0xE4FE0` |
| `0x2400C` | 33 / 3 | `2BC30 2BCF4 2C3FC` | D100 target, dword `0xE4FF4` |
| `0x241F4` | 13 / 1 | `2C3FC 339AC 3A95C` | D100 target, dword `0xE505E` |
| `0x24338` | 49 / 1 | `1890C 2BC30 2C3FC 339AC` | D100 target, dword `0xE50B8` |
| `0x24220` | 82 / 13 | `2BC30 2AE14 2C3FC 188DC` | immediate at `0x243A4` (the +0x10 handler `0x24338` stores) |
| `0x2BDA0` | 10 / 1 | `5D7DC` (rng_next) | D100 target, dword `0xE8B90` |
| `0x37DD4` | 32 / 4 | `29BC8 2A17C` | D100 target, dword `0xD2BCE` |
| `0x3E160` | 39 / 1 | `29C08 2AE14` | D100 target, dword `0xE86E0` |
| `0x40148` | 12 / 3 | `37D18` | D100 target, dword `0xE8716` |
| `0x40170` | 34 / 6 | `34D8C 3C208` | D100 target, dword `0xE86EE` |
| `0x45C98` | 37 / 4 | `13C70 2BC30 2C3FC` | D100 target, dword `0xEB80A` |
| `0x47E04` | 13 / 1 | `2C3FC 339AC 3A95C` | D000 target, dword `0xED9FC` |
| `0x47E30` | 27 / 4 | `2BC30 339AC 3AA54` | D500 target, dword `0xEDA26` |
| `0x482E4` | 46 / 4 | `2BC30 39834 3A95C` | D500 target, dword `0xED8A6` |
| `0x48374` | 52 / 4 | `2BC30 339AC 39834 39F40 3C190` | D500 target, dword `0xED8E8` |

**No correction to the membership.** Every member is an animation target except `0x24220`, the +0x10
handler `0x24338` stores at `0x243A4`; every callee the raw shows is in the seam list below (P3's streams'
targets `0x47E04 0x47E30 0x482E4 0x48374` are members as §P3.1 left them). The twelve callees that need a
seam (the raw's final list): `0x39280 0x39F40 0x13244 0x2A148 0x2BCF4 0x1890C 0x5D7DC 0x29BC8 0x37D18
0x13C70 0x3AA54 0x29C08` (the brief's eleven plus `0x29C08`, which `0x3E160` calls). `0x2BC30 0x2C3FC
0x2AE14 0x2A17C 0x3A95C 0x39834 0x34D8C 0x3C208 0x3C190 0x188DC 0x339AC` already had seams.

**The two shapes.** The 17 animation targets run as `anim_indirect` (0x2B2A0's opcodes 0x10/0x11/0x15,
and `0x2AE14`'s spawn table) calls them: EAX = rec, EDX = the opcode's operand; only `0x2BDA0` reads the
operand. `0x24220` is the `0x3531C` case-10 shape: EAX = slot, EDX = the slot's record (`mov edx,[ecx]` at
`0x35396`), EBX = side; the port's case-10 dispatch passes (slot, side), so it registers through an
adapter that supplies `DSD(slot)`, as `fighter_21458_case10` does.

## §P6.2 Callers, registers, masks and the callee declarations

- **Animation targets** (`0x22338 0x22494 0x22A40 0x23F10 0x2400C 0x241F4 0x24338 0x2BDA0 0x37DD4
  0x3E160 0x40148 0x40170 0x45C98 0x47E04 0x47E30 0x482E4 0x48374`): EAX = rec, EDX = the operand.
  Mask 0: `anim_indirect` and the spawn's `call [ebx+0xbb9dc]` do not read EAX after the call (`mov
  eax,ecx` at `0x2B575`/`0x2B59A`/`0x2B5F0`; the spawn's `test al,al` at `0x2B0EF` is on a target that
  returns AL — none of these does: their last store leaves EAX as scratch).
- **`0x24220`**: mask 0 (`0x354E5` pops and returns without reading EAX).
- **`0x2BDA0`**: EAX = rec, EDX = the operand; the operand is `mov ax,dx` (`0x2BDA5`) then
  `rng_next`'s range; the +0x53 store is `sete al`.

**Callee declarations** (args in the port's C order; clobbers = `E.callee_clobbers(image, addr)`; every one
a plain `ret` except `0x39F40`'s `ret 4`; new ones get their seam as their first statement):

| `E.Call` | callee (port C) | args | clobbers | seam |
|---|---|---|---|---|
| `P6_39280` | `0x39280` `fighter_state_39280(side)` | `eax` | none | Task 8 |
| `P6_39F40` | `0x39F40` `fighter_pose_start(side, edx, ebx, ecx, frame)` | `eax edx ebx ecx s0`, pop 4 | `ebx edx` | Task 8 |
| `P6_13244` | `0x13244` `fighter_13244()` | - | none | Task 4 |
| `P6_2A148` | `0x2A148` `actor_pset_flag_5f(rec, flag)` | `eax edx` | `edx` | Task 6 |
| `P6_2BCF4` | `0x2BCF4` `actors_anim_seek(rec, stream)` | `eax edx` | `edx` | Task 3 |
| `P6_1890C` | `0x1890C` `hit_anchor_y(side, y)` | `eax edx` | `edx` | Task 5 |
| `P6_RNG` | `0x5D7DC` `rng_next(range)` (real; its own row) | `eax` | - | Task 2 |
| `P6_29BC8` | `0x29BC8` `fighter_29bc8(side, rec, ch)` | `eax ebx edx` | `ebx edx` | Task 7 |
| `P6_37D18` | `0x37D18` `fighter_37d18(slot, rec)` | `eax edx` | `edx` | Task 2 |
| `P6_13C70` | `0x13C70` `effects_spawn(source_rec, byte_arg, handle)` | `eax edx ebx` | `ebx edx` | Task 6 |
| `P6_3AA54` | `0x3AA54` `fighter_3aa54(slot)` | `eax` | none | Task 4 |
| `P6_29C08` | `0x29C08` `fighter_29c08(side, ch)` | `eax edx` | `edx` | Task 5 |

`0x339AC` (`hit_anim_ctx`) runs on both sides (allow): its EAX is a stack buffer in every P6 caller, and
E3 verified it. The stubs' clobber sets are re-derived by
`test_each_stub_declares_the_registers_its_callee_clobbers`; `rng_next` runs real (its own row).

**Fixture conventions.** The animation targets take EAX = the record; the rows use E3_REC/E3_REC2 as the
records and E3_SLOT as a slot buffer where the raw reads `rec+0x14` or takes the slot directly. The
functions that read the slot-pointer table `0x1077A8` (rather than building the ctx) get `P6_PTRS =
{DS_SLOTS - 8: le32(DS_SLOTS) + le32(DS_SLOTS + 0x94)}` so the table names the real slot array; the
functions that build the ctx get `SLOT_PTRS`. The pokes stay inside diffrun's 16-poke and 64-byte limits
by composing the records as buffers (`_p6_fill`), with 0xA5 in the gaps so a too-wide read/write shows.

## §P6.3 Task 2: `0x2BDA0`, `0x241F4`, `0x47E04`, `0x40148`, `0x40170` (seams `0x5D7DC`, `0x37D18`)

- **`0x2BDA0`** (D100 target `0xE8B90`): `push ebx; mov ebx,eax; xor eax,eax; mov ax,dx; call 0x5D7DC;
  test eax,eax; sete al; mov [ebx+0x53],al`. The record's +0x53 = 1 when `rng_next((u16)arg)` returns 0.
  The call's EAX is the masked range (the raw's `mov ax,dx` zero-extends), so a port that passes EDX
  unmasked is a call-argument difference on r2.
- **`0x241F4`** (D100 `0xE505E`): ctx (`0x339AC`); `mov edx,0xA; mov eax,[esp+4]` = ctx[1] →
  `0x3A95C(ctx[1], 0xA)`; `voice 0x66`. EDX is pushed and popped: the operand is unread.
- **`0x47E04`** (D000 `0xED9FC`): ctx; `voice 0x66`; `mov edx,0xF; mov eax,[esp+4]` = ctx[1] →
  `0x3A95C(ctx[1], 0xF)`. The voice preserves EDX, so the stance takes 0xF.
- **`0x40148`** (D100 `0xE8716`): other = `DSD(0x1077A8 + ((rec+0x51) ^ 1) * 4)`; when 0 nothing; else
  `0x37D18(other, DSD(other))` and `0x104AE9 &= 0xFB`. The other slot's record pointer comes from the
  slot array; the row's o0 case makes the table's second dword 0.
- **`0x40170`** (D100 `0xE86EE`): other as above; with the record's +0x28 bit 14 the other record's +0x29
  bit 6 clears (else sets); `0x34D8C(rec+0x51)` (`hit_flash_pair`); `0x3C208(rec+0x51, word
  0xC759C[other slot's char])`. The unit check arms `DS_00104B00 = 0x22` so the real PLACE chain's
  `0x18AF8`/`hit_facing_flag` early-returns and the function's own +0x29 store is observable.

Cases: `r0`..`r3` (`0x2BDA0`, both rng outcomes, the 0x12340001 mask case); `s0`/`s1` per function (side
0/1); `o0`..`o2` (missing other, side 0, side 1); `p0`..`p3` (missing other, bit 14 with two chars, bit
14 clear, side 1). Mutants: `@mask` (the range unmasked) caught by r2's `call #0`; `@mutant` (inverted)
by r0's byte; `@side`/`@voice`/`@order`/`@bit`/`@set`/`@char` as `P6_KINDS` pins them.

**Fix round (planner's prototype).** `0x47E30`'s `0x3AA54` argument is ctx[3], not ctx[2] (the `[esp+0xC]`
after `0x2BC30`'s `ret 4` is ctx[3]); the diff row caught it, and the `@side` mutant swapped to ctx[2].
The `0x2BDA0` row's `rng_next` runs real, so the row needs `P6_RNG`'s `real` mode and rng_next's seam.

## §P6.4 Task 3: `0x22494`, `0x2400C`, `0x482E4` (seam `0x2BCF4`)

- **`0x22494`** (D500 `0xE4E1A`): ctx; `push [0x104738 + ctx[0]*4]`, `ANIM_BEGIN(ctx[4], 0xE4E1E, frame)`;
  `0x3A95C(ctx[1], 0xA)`; `0x39834(ctx[1], (u8)ctx[2].+0x5F)`; `voice 0x66`. Both sides' frames and both
  slots' +0x5F differ.
- **`0x2400C`** (D100 `0xE4FF4`): other; `orec.+0x1C -= (s16)word 0xA83FA[other slot's char]`;
  `ANIM_BEGIN(orec, DSD(0xA8408 + char*4), 3.0)`; `DSD(0x104748).+0x29 |= 8`;
  `actors_anim_seek(DSD(0x104748), 0x741)`; `voice 0x67`. The two tables are the effective word/stream
  bases: the raw loads a dword at `0xA83F8 + 2c` and takes its high half (`sar 16`), so the word is at
  `0xA83FA + 2c`; the row pokes both char entries.
- **`0x482E4`** (D500 `0xED8A8`): `ANIM_BEGIN(rec, 0xED8BC, 3.0)`; when `0x108392[own side]` is non-zero
  `ANIM_BEGIN(DSD(other slot), DSD(0xC8F40 + other char*4), 5.0)`, else `0x3A95C(other side, 0xF)` and
  `0x39834(other side, DSB(0x10780F + own*0x94))` (the own slot's +0x5F). Four cases cross the byte and
  the side; `@byte` reads the other side's byte.

## §P6.5 Task 4: `0x22A40`, `0x47E30` (seams `0x13244`, `0x3AA54`)

- **`0x22A40`** (D100 `0xE154A`): with the record's +0x14 slot set, `SPAWN(0xBB31C, rec+0x18, rec+0x30 >>
  16, rec+0x1C, rec+0x28 & 0x4000)`; the child into the slot's +8, `child.+0x59 = 2`, `child.+0x14 =
  slot`, `DSD(0x104730 + side*4) = child`; `slot+8 = 0` **before** `voice 0xA8` (the row's `@order`
  catches the other order at `call #1 memory`); `fighter_13244()`.
- **`0x47E30`** (D500 `0xEDA26`): ctx; when `0x108394[side]` non-zero `ANIM_BEGIN(ctx[4], 0xEDA2A,
  0x108378[side])` and `fighter_3aa54(ctx[3])`; else `ANIM_BEGIN(ctx[4], 0xED9FA, 0x108378[side])`.
  **Correction:** the raw's `mov eax,[esp+0xC]` after `0x2BC30`'s `ret 4` names ctx[3] (the other slot),
  not ctx[2]; the first prototype had ctx[2] and the diff row was MISMATCH until fixed.

## §P6.6 Task 5: `0x24338`, `0x3E160` (seams `0x1890C`, `0x29C08`)

- **`0x24338`** (D100 `0xE50B8`): ctx; `ANIM_BEGIN(ctx[5], DSD(0xBED60 + other char*4), 4.0)`;
  `hit_anchor_y(ctx[1], (s16)word 0xBD884[other char])`; the other slot +0x52/0x53/0x54 = 0x10/0xA/2,
  +0x10 = `0x24220`, +0x58 = 0; `ctx[5].+0x36 = 0x600`; `0xF0AFE = 0`, `0xF0AFF = rec+0x51`;
  `voice 0x73`; `voice DSW(0xBE008 + other char*2)`. `0xBD884` is the effective word base (dword at
  `0xBD882 + 2c`, `sar 16`).
- **`0x3E160`** (D100 `0xE86E0`): `fighter_29c08(side, own slot's char)` into the side's `0xC7614`
  descriptor's +0x10, then `SPAWN(that descriptor, 0, 0, 0, rec+0x56 | 0x400)`; `rec+0x4B = child+0x56`,
  `child+0x60 = 1`. It is `0x3E0F0`'s twin with the `0xC7614` table.

## §P6.7 Task 6: `0x23F10`, `0x45C98` (seams `0x2A148`, `0x13C70`)

- **`0x23F10`** (D100 `0xE4FE0`): other; `other.+0x41 |= 0x20`; `orec.+0x1C -= (s16)word 0xA83EC[char]`;
  `actor_pset_flag_5f(orec, 0)`; `ANIM_BEGIN(orec, DSD(0xA8408 + char*4), 3.0)`; `SPAWN(0xA84CC,
  orec+0x18, orec+0x30 >> 16, 0, 0)`; `child.+0x59 = 2`, `0x104748 = child`; three more spawns at x
  0/+0x14/-0x14, a4 = 0xF, a5 = `DSD(0x104748).+0x56 | 0x400`; `voice 0x64`.
- **`0x45C98`** (D100 `0xEB80A`): with `rec+0x14`'s slot, side = that slot's record's +0x51; other =
  `DSD(0x1077A8 + (side^1)*4)`; `ANIM_BEGIN(orec, DSD(0xC90F8 + other char*4), 2.0)`;
  `effects_spawn(DSD(pset(orec) + 0x18), 4, 0x105FC30)`; `voice DSW(0xC75AA + other char*2)`. The rows
  keep the argument rec different from `orec` and the slot's record's +0x51 different from the argument's,
  so `@stream`/`@side`/`@voice` are caught.

## §P6.8 Task 7: `0x37DD4` (seam `0x29BC8`) and gp-u10-ending

- **`0x37DD4`** (D100 `0xD2BCE`): `actor_pset_palette(rec, rec+0x51 != 0 ? 4 : 0, 0)`, then
  `fighter_29bc8(rec+0x51, rec, DSB(0x10782A + side*0x94))` (the side's slot's char). The palette word
  is 4 for **any** non-zero side (the raw's `test ah,ah`); s2 (side 2) pins it.
- **gp-u10-ending**: the set loses the `0x37DD4` row (it was `anim_indirect`, 33 hits at f=0x14FB in mode
  0xC; record §W.14). `make gp-ending-oracle` passes on P3's pins with the set at 3 distinct addresses
  (`fn-miss PR_GP_DUMP distinct=5 dropped=0`: `0x5D812` x2 sites, `0x29D60`, `0x29C78` P7) and every pin
  is exact and unchanged: 331 (`--min-first
  332` fails), trace/win 9954 (`9955` is unreachable), path 30 (`31` unreachable), MAX_START 83. The
  Makefile's `GP_ENDING_*` provenance and AGENTS.md's re-measure list name this batch.

## §P6.9 Task 8: `0x22338`, `0x48374` (seams `0x39280`, `0x39F40`)

- **`0x22338`** (D500 `0xE4E46`): ctx; `ANIM_BEGIN(ctx[4], 0xE4EAC, 3.0)`; `0x39834(ctx[1], 0x10)`;
  `voice 0x6B`; `0x39280(ctx[1])` (the other side); `ctx[3].+0x74 = 0`; by the own slot's +0x57: 6 →
  `0x39F40(ctx[1], -300, 0x8C, 0xF, 0xD)`, 7 → `(-70, 0x118, 0x13, 0x1E)`, every other state →
  `(-100, 0x78, 0xF, 0x10)`; then `voice DSW(0xBE008 + other char*2)` and the own +0x57 = 3. The state
  comes from the own slot (ctx[2]); the voice's char from the other slot (ctx[3]).
- **`0x48374`** (D500 `0xED8EA`): ctx; `0x3C190(ctx[0], -0x200)`; `ctx[4].+0x36 = 0x2EE`, +0x44 = 0x28;
  `ctx[2].+0x57 = 3`, +0x54 = 2; `ANIM_BEGIN(rec, 0xED8FE, 4.0)`; when `0x108392[side]` non-zero
  `ANIM_BEGIN(ctx[5], DSD(0xC8F40 + other char*4), 5.0)` and `ctx[3].+0x58 = 1`, else
  `0x39834(ctx[1], ctx[2].+0x5F)` and `0x39F40(ctx[1], -100, 0x64, 0xF, 0x14)`.

`0x39F40` is the batch's only `ret 4` callee; its `E.Call` carries `pop=4` and clobbers EBX/EDX (it
saves neither). `0x39280` clobbers nothing.

## §P6.10 Task 9: `0x24220` (the +0x10 handler)

- **`0x24220`** (the dword at `0x243A4`): EAX = slot, EDX = the slot's record, EBX = side. By the slot's
  +0x58: **0** — with the record's +0x1C >= 0x6400 (signed `jl`), `ANIM_BEGIN(rec, DSD(0xA8510 + char*4),
  2.0)`, `0x188DC(side, DSD(slot+0x2C))`, `rec.+0x2C = 0x200`, +0x36 = 0, +0x44 = 1, +0x32 =
  `word 0xA852C[DSW(0x104AFC)]`, `slot.+0x41 |= 0x20`, `slot.+0x58 = 1`, `0x104768 = 0x78`; every state-0
  path then `DSW(DSD(slot+4) + 0x2C) -= 0x80`. **1** — count `0x104768` down (word); at <= 0 signed,
  `voice 0x5A` and +0x58 = 2, then the state-2 block. **2** (and state 1 after the count) — once the
  record's +0x1C is 0: `SPAWN(0xA853C, rec+0x18, rec+0x30 >> 16, 0, 0)`, `ANIM_BEGIN(rec, 0xE87AC, 1.0)`,
  `voice 0x5B`, `0x1078FC = 1`, `slot.+0x53 = 3`, `slot.+0x52 = 9`, `0xF0AFE = 4`. Above 2 nothing.
- Registered as `fighter_24220_case10(slot, side)`, which supplies `rec = DSD(slot)` (the raw's `mov
  edx,[ecx]` at `0x35396`).

Cases `m0`..`m6` cover state 0 below/at the bound, state 1 with the count 2 and 1, state 2 with
`rec+0x1C` 0 and non-zero, state 3, and side 1. Mutants `@st`, `@bound`, `@anchor`, `@desc`, `@voice`,
`@plus4`, `@decr` are caught by the case sets `P6_KINDS` pins. The slot+4 record is seeded so the
`@plus4` mutant writes inside the image instead of crashing.

## §P6.11 Decisions, named gaps and limits

**No decision is left to the user.** The member list is the roadmap's (§P6.1); `0x24220` follows the
case-10 adapter precedent; `0x2BDA0`'s rng runs real because it has its own row; the 12 seams follow
AGENTS.md.

Named gaps and limits:
- **`0x24220`'s named limit:** none of its callees runs real in the rows; the unit check runs the real
  `0x2BC30`/`0x2C3FC` chain and observes the stores the stream walk does not touch.
- **The stream walks in unit checks:** `actors_anim_begin` runs its own bytes on the fixture, so the
  unit checks observe only stores written *after* the walk (e.g. `0x22494`'s stance, `0x24338`'s slot
  fields); the stream argument itself is the row's claim.
- **The 16-poke diffrun limit:** the composed record buffers (`_p6_fill`) put 0xA5 in the gaps; the
  functions' neighbours are seeded, so width mutants show.
- **The callee rows** (D3): the 12 new stubs join C2 with P1-P3's; `0x5D7DC` is real and has its own row.
  After P6 the counter reads `41/126 rows with callees closed (23 have none)` — only `anim_2bda0`'s row
  closes (rng_next is VERIFIED); every other P6 row stubs a callee without a row.
- E3's, P1's and P2's limits stand: seeds are hand pokes; the memory at a call is mem[] only; the callee
  column is one level deep.
- **Outside P6:** the `title_pin` unittest failure on this tree is pre-existing (P2 §P2.13) and outside
  `make verify`; not touched here.

## §P6.12 Results

Per-task gates (the harness self-check, the unit suite, `make entry-triage` with the regenerated table,
and the gp oracle a task touches). The counter after each task, measured on this branch (the plan's
`f5b5556` base is not the branch's: C1 and P4+P5 were merged before P6, so the plan's prototype table
`78/78; 158/158; 11/64` -> `96/96; 236/236; 12/82` is superseded; the lines below are the commits' own
measurements, Task 2 `31f4ed8` .. Task 9 `c7ce9d3`):

| after | diff-verify counter (measured on the branch) |
|---|---|
| Task 2 | `136/136 functions VERIFIED; 328/328 mutants detected; 1 named gaps; 41/113 rows with callees closed (23 have none)` |
| Task 3 | `139/139 ...; 341/341 ...; 41/116 ...` |
| Task 4 | `141/141 ...; 348/348 ...; 41/118 ...` |
| Task 5 | `143/143 ...; 357/357 ...; 41/120 ...` |
| Task 6 | `145/145 ...; 368/368 ...; 41/122 ...` |
| Task 7 | `146/146 ...; 371/371 ...; 41/123 ...` |
| Task 8 | `148/148 ...; 383/383 ...; 41/125 ...` |
| Task 9 | `149/149 functions VERIFIED; 391/391 mutants detected; 1 named gaps; 41/126 rows with callees closed (23 have none)` |

The final full gate on the branch (`make verify`, `EXIT=0`): the 45 oracle lines equal the k7-k12
baseline (`ORACLES-EQUAL`), the audio WAV byte-identical (`WAV-SAME`), `PR_ORACLE_REQUIRED=1
./build/run_tests` `all checks passed`, the diff-verify Python suite 165 tests OK and the E2 suite 44,
and `entry-triage: targets 240 unported, 255 ported; supplement 131 (7 unported, 0 stale); untrusted
entries 30` / `voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere`; the
E2 P-track classes `callbacks 0/71`, `finishers 0/9`, `voice 0/15`, `animation-targets 6/106`,
`span-writers 231/0`, `other 3/54` and the E1 readiness `leaf 228`, `stubs 11`, `allow-list 1`. The 18
rows, all VERIFIED, as the final table prints them:
`anim_2bda0` 4 1/1, `fighter_241f4` 2 1/1, `fighter_47e04` 2 1/1, `fighter_40148` 3 3/3, `fighter_40170`
4 6/6, `fighter_22494` 2 1/1, `fighter_2400c` 3 3/3, `fighter_482e4` 4 4/4, `fighter_22a40` 4 5/5,
`fighter_47e30` 3 4/4, `fighter_24338` 2 1/1, `fighter_3e160` 2 1/1, `fighter_23f10` 2 1/1, `fighter_45c98`
4 4/4, `fighter_37dd4` 3 4/4, `fighter_22338` 5 7/7, `fighter_48374` 4 4/4, `fighter_24220` 7 13/13
(the plan's `fighter_37dd4` `1/1` was the prototype's; the branch measures 4/4).
P6's 78 mutants: what alone catches each is pinned by `P6_KINDS` in `tools/tests/test_diff_verify.py`;
the five expectation tests the plan deferred are green (the exact sets, the stub-clobber table with the
nine new stubs, the counter). `port_progress.py` stays `771 1203 64` / `731 731 100` (none of the 18 is
a Ghidra `FN_`) and README does not move.

**Named limit (the row's "every store observable" claim):** `0x22A40`'s store of the child into the
slot's `+8` is overwritten by the following `slot+8 = 0` before the row's next recorded call, so the
memory-at-call comparison sees only the net value (`@order` still catches the two stores' order at
`call #1 memory`); no unit check runs the spawn (it needs the actor pool). Review fixes folded in: the four
Task 2 headers that cited the wrong record sections now cite §P6.3, and the two weak unit assertions
(`test_fight.c` 0x22494's own-slot `+0x5F` and 0x2400C's `+0x29 |= 8`) now seed a value that differs
from the post-condition and observe the store's effect — each fails under its mutation (0x9F vs 0x57;
0x21 vs 0x29).

**gp.** `make gp-ending-oracle` on the branch: `fn-miss PR_GP_DUMP distinct=5 dropped=0` — the set is
3 distinct addresses (`0x5D812` x2 sites, `0x29D60`, `0x29C78` P7), losing `0x37DD4` alone — frames
331 ok, trace 9954 ok, path 30 ok, win 9954 ok, window start 83; every pin +1 fails (332, 9955, 31,
9955). `make gp-win-oracle` loses `0x2BDA0` alone (`distinct=6 dropped=0`) and every pin is exact and
unchanged: frames 346, trace 2364, path 8, win 3162, window start 100 (the trace's first difference
f=0x93C precedes the target's first frame 0xD0C). No other gp scenario's miss set holds a P6 member
(checked from `k_gp_sets`; only gp-u9-win and gp-u10-ending did).

## §P6.13 The roadmap after P6

P6 **18** as listed; C2 gains the twelve stubs above; P7 keeps `0x48254` and the unported direct callees;
P8 unchanged. The U10 re-measure list (AGENTS.md, Makefile) loses `0x37DD4`.

**Fix rounds during the planner's prototype** (each caught by the gates, recorded here because the plan
bakes them in): `0x47E30`'s `0x3AA54` argument is ctx[3] (the diff row's MISMATCH); the composed pokes
keep inside the 16-poke/64-byte diffrun limits; `0x45C98`'s cases keep the argument rec, the slot's
record and the other record distinct so `@stream`/`@side`/`@voice` are caught; the `@plus4` mutant's
slot+8 is seeded so it cannot read an unseeded pointer; the effective word bases `0xA83FA`, `0xA83EC`,
`0xBD884` are the dword-load `sar 16` halves (the raw's tables), not the raw table addresses.
