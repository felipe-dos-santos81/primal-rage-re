# Reverse completion D1, batch d1a: differential rows for game/fighter.c, first half (record)

Track D1 rows the ported functions the callee scan never reached. Batch d1a's queue is 31 functions of
`port/src/game/fighter.c` (`.superpowers/sdd/2026-10-11-reverse-d1/d1a-queue.txt`). **25 are rowed here, all
`VERIFIED` with every block hit; 6 are not done (§6).** Two rows found the port's behaviour differing from
the raw (§3.1, §3.2) and three needed an AL-width alignment (§3.3).

**Status of the numbers.** Every figure below was measured on branch `reverse-d1a` (base `b4d0ad4`) against
the image `diffrun` dumps from `data/game/C/PRAGE.EXE` (sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`),
with `python3 tools/diff_verify.py --image … --function <row> --self-check`. The claim is the harness's:
equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.

Method notes that apply to every row:

- **No new stub address.** `test_each_stub_declares_the_registers_its_callee_clobbers` pins the stub set, a
  shared literal nine batches would all edit; so a callee that is not yet a stub anywhere runs real
  (`allow_calls`) on both sides: `0x3C570`, `0x1922C`, `0x249C0`, `0x33A68`, the runtime `0x61A4C` and
  `0x654C7`, and the two registered callbacks `0x3E484` and `0x29D04`. No seam was added.
- **No index leaves mem[].** Sides are 0 or 1; every `@…sext` mutant's negative index and every wrong-side
  read lands inside the image and is seeded with the opposite answer (fixtures `d1a_*` in
  `tools/diff_verify.py`).
- **Static functions.** Seven members were `static`; the binding cannot call a static function, so each
  lost the keyword and gained a declaration in one block of `fighter.h` (linkage only, no behaviour):
  `fighter_14590`, `fighter_19164`, `ai_facing_cmd`, `fighter_spawn_audio_gate`, `fighter_21f30`,
  `fighter_22b28`, `fighter_35050`.

## §1 The members and what fixes each EAX mask

The mask is the part of EAX a caller reads. A value-returning function's mask comes from its callers'
next instruction; a void function is mask 0 when every call site overwrites EAX before reading it, or
returns into a parent whose own call sites do. Where the chain of returns was not followed to its end the
limit is named. Call sites were found by scanning the image for `call`/`jmp rel32` to the entry; none of the
25 has a `jmp` tail call or a data pointer to it.

| addr | port function | size | mask | what fixes it |
|---|---|---|---|---|
| `0x14590` | `fighter_14590` | 60 | `0xFF` | `mov al,1` `0x145BE` / `xor al,al` `0x145C5`; all six callers `test al,al` (`0x14669`, `0x14707`, `0x14872`, `0x149D5`, `0x14A73`, `0x14BEC`) |
| `0x186C4` | `fighter_slot_latch_both` | 12 | 0 | both callers return at once (`0x3582E`..`0x35835` in `0x35658`, `0x385AA`..`0x385AE` in `0x384F8`); of `0x35658`'s 28 call sites 26 overwrite EAX (`mov eax,1`, or a callee that writes it before reading it on every path the scan follows: `0x49C78`, `0x4BF18`, `0x12DA8`, `0x4D2D0`) and two (`0x277E4`, `0x2965A`) call `0x4DEF4`, which reads EAX only in `dec eax` at `0x4DF36` after `mov ax,[0x1088b0]` and stores AX alone; `0x384F8`'s two sites overwrite it |
| `0x18950` | `fighter_connect_query` | 152 | `0xFF` | `mov al,1` / `xor al,al`; `0x19658` `mov bl,al`, `0x19696` and `0x196A7` `test al,al` |
| `0x19020` | `fighter_19020` | 70 | 0 | the one caller reloads EAX on both arms (`0x195CC` `mov eax,[esp+8]`) |
| `0x19068` | `fighter_pass_b` | 250 | 0 | 12 call sites: five push and load other registers then write EAX, seven call `0x12DA8` or `0x49C78`, which write EAX before reading it (`0x12DA9` `mov al,[..]`, `0x49C81` `xor eax,eax`) |
| `0x19164` | `fighter_19164` | 198 | 0 | `0x19481` `mov eax,[esp+8]` |
| `0x19820` | `fighter_19820` | 168 | 0 | the caller writes AX at once (`0x27F3F` `mov ah,0x1e`, `0x27F4D` `mov al,[0x10780a]`) and never reads the upper half; **limit:** that half then leaves through `0x27FA5` and the returns of `0x27FA8`'s three sites (`0x2800C`, `0x280C3`, `0x280DB`), not followed further |
| `0x1A5D4` | `ai_facing_cmd` | 107 | full | the result is `mov eax,0x2000` / `0x1000` / `xor eax,eax`; six callers `test eax,eax` (`0x234C2`, `0x35A32`, `0x35AB4`, `0x4681C`, `0x4686C`, `0x4874B`) |
| `0x1A640` | `fighter_1a640` | 107 | full | five callers `test eax,eax` (`0x1AA98`, `0x35A51`, `0x364D7`, `0x365A3`, `0x4875E`) |
| `0x1A8F4` | `fighter_block_end` | 129 | 0 | the caller returns (`0x1AB09`..`0x1AB0F`, in `0x1A978`), its caller returns (`0x34C7A`..`0x34C7F`, the state dispatcher `0x34B6C`), and the dispatcher's one call site `0x357FC` is followed by a write of EAX |
| `0x1CEBC` | `fighter_spawn_audio_gate` | 24 | `0xFF` | `mov al,1` / `xor al,al`; `0x33E4D` `test al,al` |
| `0x21F30` | `fighter_21f30` | 87 | 0 | `0x214DE`, `0x21DC3`, `0x220AB` `mov eax,[..]` |
| `0x22B28` | `fighter_22b28` | 179 | 0 | both callers return (`0x22D8A` in `0x22CE4`, `0x2365A` in `0x235C4`); their one call site each (`0x22E64`, `0x3B65E`) is followed by a write of EAX |
| `0x24754` | `fighter_24754` | 174 | 0 | `0x24917` `lea eax,[esi*8]` |
| `0x29C20` | `fighter_29c20` | 59 | full | `0x346BA` `mov ebx,eax` (the palette handle) |
| `0x33ACC` | `fighter_33acc` | 51 | 0 | the three callers load EAX before reading it (`0x22D39`, `0x23614`, `0x27277`) |
| `0x34038` | `fighter_34038` | 131 | 0 | `0x357F3`, `0x3855B`, `0x3A093` `mov eax,r`; the fourth caller (`0x370E2`, in `0x36F10`) returns into the dispatcher (`0x34BEA`), as `0x1A8F4`'s chain |
| `0x34978` | `fighter_slots_reset` | 39 | 0 | a void reset; both callers go on to `call 0x2c074` (`0x20E47`, `0x20EEE`), which never touches EAX; **limit:** the value then leaves through `0x20E8E`/`0x20EF4`, not followed further |
| `0x349C8` | `fighter_state_default` | 329 | 0 | the caller returns (`0x34C0F`..`0x34C14`, the dispatcher), then `0x357FC` |
| `0x34DDC` | `fighter_34ddc` | 68 | `0xFF` | `xor al,al` / `mov al,1`; `0x35293` `test al,al` |
| `0x34E20` | `fighter_34e20` | 11 | `0xFF` | `xor al,al` / `mov al,1`; `0x354A8` `test al,al` |
| `0x35050` | `fighter_35050` | 125 | 0 | `0x22C65`, `0x39CDE` `mov eax,[esp+0xc]` |
| `0x35C1C` | `fighter_state_35c1c` | 154 | full | `mov eax,ecx` `0x35CAE` (0 or 1); three callers `test eax,eax` (`0x34C2F`, `0x35C05`, `0x35D0F`), the other three overwrite it |
| `0x35D20` | `fighter_state_35d20` | 89 | 0 | the three callers return at once (`0x34C40`..`0x34C45` the dispatcher; `0x35C12`..`0x35C19` in `0x35B7C`; `0x35D1C`..`0x35D1E` in `0x35CB8`); **limit:** `0x35B7C`'s and `0x35CB8`'s own callers are tail returns again (five of `0x35B7C`'s six), not followed further |
| `0x35D7C` | `fighter_state_35d7c` | 134 | 0 | the caller returns (`0x34C4D`..`0x34C52`, the dispatcher), then `0x357FC` |

## §2 The rows (measured)

`row | entry | cases | blocks | mutants | EAX mask | notes`

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `fighter_14590` | `0x14590` | 8 | 5/5 | 7 | `0xFF` | `0x33950` real |
| `fighter_slot_latch_both` | `0x186C4` | 4 | 8/8 | 4 | `0x0` | `0x186D0` real (the second pass is a fall-through, so it cannot be a stub: the scan's 8 blocks are the wrapper's and `0x186D0`'s); `0x18540`/`0x18350` stubs |
| `fighter_connect_query` | `0x18950` | 9 | 6/6 | 6 | `0xFF` | the pair-table dword and its row are poked; states 0x80, 0x1F, 0x20, 0x3F, 0x40 and a character 0x80 |
| `fighter_19020` | `0x19020` | 5 | 5/5 | 6 | `0x0` | the hook is the registered `0x3E484` run real (with `0x33950`, `0x18BD4`); its `0x18C14` is the stub whose EAX comes back (0, 1, 0x100) |
| `fighter_pass_b` | `0x19068` | 9 | 19/19 | 18 | `0x0` | `0x3C570`, `0x1922C`, `0x33950` real; `0x3CF38` stub |
| `fighter_19164` | `0x19164` | 14 | 8/8 | 11 | `0x0` | `0x61A4C` real; x87, with NaN and overflow cases (§3.1) |
| `fighter_19820` | `0x19820` | 5 | 9/9 | 10 | `0x0` | `0x249C0` real |
| `ai_facing_cmd` | `0x1A5D4` | 6 | 6/6 | 5 | `0xFFFFFFFF` | |
| `fighter_1a640` | `0x1A640` | 6 | 6/6 | 5 | `0xFFFFFFFF` | |
| `fighter_block_end` | `0x1A8F4` | 5 | 5/5 | 12 | `0x0` | `0x33A68` real; `0x2BC30` stub |
| `fighter_spawn_audio_gate` | `0x1CEBC` | 5 | 4/4 | 4 | `0xFF` | |
| `fighter_21f30` | `0x21F30` | 3 | 3/3 | 7 | `0x0` | `0x2C3FC`, `0x2AE14`, `0x13244` stubs |
| `fighter_22b28` | `0x22B28` | 4 | 3/3 | 12 | `0x0` | EAX = the context in mem[]; `0x13C70`, `0x3C16C`, `0x3C148`, `0x2C3FC` stubs |
| `fighter_24754` | `0x24754` | 4 | 4/4 | 11 | `0x0` | `0x33950` real; `0x1A570` (AL tested), `0x2AE14`, `0x2C3FC` stubs (§3.3) |
| `fighter_29c20` | `0x29C20` | 8 | 5/5 | 6 | `0xFFFFFFFF` | side and index bytes 0, 1 and 0x80; DL = 0 with EDX = 0x100 |
| `fighter_33acc` | `0x33ACC` | 2 | 1/1 | 7 | `0x0` | one block (`rep movsd` twice) |
| `fighter_34038` | `0x34038` | 7 | 5/5 | 9 | `0x0` | the `0x2B150` and `0x29BC8` stubs each write DS_001078FF, so the raw's re-reads (`0x34073`, `0x340A2`) are observable |
| `fighter_slots_reset` | `0x34978` | 2 | 1/1 | 6 | `0x0` | `0x654C7` real (two dwords) |
| `fighter_34ddc` | `0x34DDC` | 8 | 4/4 | 7 | `0xFF` | |
| `fighter_34e20` | `0x34E20` | 6 | 3/3 | 3 | `0xFF` | |
| `fighter_35050` | `0x35050` | 4 | 4/4 | 5 | `0x0` | the callback is the registered `0x29D04` run real; `0x2A17C` stub |
| `fighter_state_default` | `0x349C8` | 16 | 24/24 | 15 | `0x0` | seven stubs; `0x3BDDC`'s zeroes the command words (§3.3, §4) |
| `fighter_state_35c1c` | `0x35C1C` | 8 | 8/8 | 12 | `0xFFFFFFFF` | `0x2BCF4` stub |
| `fighter_state_35d20` | `0x35D20` | 4 | 8/8 | 8 | `0x0` | `0x2BC30` stub |
| `fighter_state_35d7c` | `0x35D7C` | 5 | 3/3 | 7 | `0x0` | EDX = side; `0x3CF38` stub (§3.2) |

25 rows, 157 cases, 203 mutants, every mutant detected. The exact kinds and case sets that catch each
mutant are `D1A_KINDS` and `D1A_CASES` in `tools/tests/test_diff_verify.py`; the mutants are the `m_d1a_*`
bindings over the `d1a_*_core` functions of `port/tests/diff_runner.c`, each named by the choice it flips
(`@side`/`@slot`/`@rec` an index, `@…sext` an extension, `@al` AL against EAX, `@order` a store across a
call, a bare field name a dropped or narrowed store). Mutants caught by the call list or the memory at a
call alone: `fighter_pass_b@order` and `@call`, `fighter_block_end@order`, `fighter_22b28@voice`,
`fighter_19020@arg`, `fighter_state_35c1c@order`, `fighter_state_35d20@order`, and every mutant of
`fighter_21f30`.

Two equivalent mutations were tried and are not bound: a dword store of `0x24754`'s speed at +0x34 (the
+0x36 word store at `0x247EA` rewrites its upper half), and `0x349C8` without the other-slot null test
(mem[0x40] is zero, so the `0x8200000` mask test fails the same way).

## §3 Corrections (the raw over the port)

Each is its own commit, and each is unproven against the capture oracles until the integrator's
`make verify`.

### §3.1 `0x19164` `fighter_19164`: NaN is kept, and `fistp` stores the indefinite (commit `0a75e09`)

- **Raw.** `fld dword [0x80584]` (5.0); `fcomp dword [ebx+0x20]`; `fnstsw ax`; `sahf`; `jbe 0x19193`
  (`0x1917C`..`0x19188`): the branch is taken on CF or ZF, which an unordered compare sets, so a NaN at
  rec+0x20 is kept rather than replaced by 5.0. Then `call 0x61a4c` (`fnstcw`; RC := chop; `frndint`;
  restore) and `fistp dword [esp+4]` (`0x191A1`; the same pair at `0x191DF`/`0x191E4`): for NaN, or a
  truncation outside the dword, the x87 stores the integer indefinite `0x80000000`, whose low byte 0 is
  what `0x191A9` writes to `DS_00100B5A[side]`.
- **Old port.** `(u8)(s32)((f >= 5.0f) ? f : 5.0f)`: 5 for NaN, and a C cast that is undefined out of
  range (0xFF on this arm64 host).
- **Measured before the fix** (unpinned probe, the cases now `n0`, `n1`, `n3`): `byte 0x100B5A: original
  0x00, port 0x05` (NaN); `original 0x00, port 0xFF` (3e9 and +inf).
- **New port.** `!(5.0f > f) ? f : 5.0f`, and both conversions through `fighter_x87_fistp` (the indefinite
  when `!(f >= -2147483648.0f && f < 2147483648.0f)`).
- **Mutants.** `@nan` (the ordered select) is caught by `n0` alone; `@ovf` (the saturating cast) by `n1`
  and `n3`.
- **Scope.** rec+0x20 is an animation rate; no capture is known to hold NaN or 2^31 there. Other ported
  callers of `0x61A4C` cast the same way and are outside this queue.

### §3.2 `0x35D7C` `fighter_state_35d7c`: the slot is computed from the side (commit `e61b5a5`)

- **Raw.** EDX = side; `lea eax,[edx*8]; add eax,edx; shl eax,2; add eax,edx; mov ebx,0x1077b0; shl eax,2;
  add ebx,eax; mov [esp+8],ebx` (`0x35D8E`..`0x35DA6`): the slot is `DS_001077B0 + side * 0x94`, written
  through at `0x35DDA`/`0x35DDE` with no null test. EAX (the dispatcher's slot pointer) is overwritten at
  `0x35D80` unread.
- **Old port.** `slot = DSD(DS_001077A8 + side * 4)` with `if (slot == 0) return;`.
- **Measured before the fix.** `a0: byte 0x107803: original 0x04, port unchanged` and `byte 0x10A253:
  original unchanged, port 0x04` (the table entry pointed at a scratch block); `a2: call #0: original
  0x3CF38(0x0), port none` (the table entry 0).
- **Game effect.** None found: the table's one non-zero store is the slot's own address (`0x33CA0`
  `mov [eax+0x1077a8],esi`, ESI = `0x1077B0 + side*0x94`), the reset stores 0 (`0x34986`), and the
  dispatcher tests the entry before dispatching (`0x34B7A`).
- **Unit test.** `test_fight.c` case D drove the dispatcher with a scratch slot in the table and asserted
  the re-arm there; it now asserts it on the real slot 0 (same three checks). Under the old port both
  fail: `test_fight.c:8884: 170 != 4`, `:8885: 170 != 2`.
- **Mutant.** `@table` (the old code) is caught by `a0`..`a3`.

### §3.3 AL-width alignments (behaviour-neutral in the game)

The raw tests AL after these calls; the port compared the whole `int`. The C callees return 0 or 1, so no
run changes; the rows stub the callee with EAX = `0x100` (AL = 0), where the old compare takes the other
arm. Each now reads `(u8)callee(...)`, as the C3 rows did (`hit_scan`'s `0x3CD68`).

| function | call | the raw's test | commit | mutant, caught by |
|---|---|---|---|---|
| `fighter_24754` | `0x1A570` | `0x2476C` `test al,al` | `3f4b3bc` | `@al`, `j2` (measured before: `call #1` x `0xFFFEFF00` for the original's `0xFFFF5300`; speed bytes `0x96 0x00` for `0x6A 0xFF`) |
| `fighter_state_default` | `0x365C8` | `0x34A43` `test al,al` | `0eeef06` | `@al365`, `w8` |
| `fighter_state_default` | `0x36638` | `0x34A5A` `test al,al` | `0eeef06` | `@al366`, `w11` |
| `fighter_state_default` | `0x3BDDC` | `0x34AA7` `test al,al` | `0eeef06` | `@al3b`, `w9` |
| `fighter_state_35d7c` | `0x3CF38` | `0x35DED` `test al,al` | `e61b5a5` | `@al`, `a2`/`a3` |

## §4 Named gaps, limits and unhit blocks

- **Unhit blocks: none.** Every row hits every block of its static scan.
- **`0x19164`'s x87 domain.** The row covers finite values, -0.0, NaN, +inf and both overflow signs at
  rec+0x20 and NaN at rec+0x24. Not covered: signalling NaN, denormals, and -inf.
- **`0x35050`: AL against EAX is not separable.** `0x350BB` is `test eax,eax`, but the only registered
  callback, `0x29D04`, returns 0 or 1 (`xor eax,eax` `0x29D10`, `mov eax,1` `0x29D56`), so a port testing
  AL would agree. `0x19020`'s `test eax,eax` (`0x19048`) is separated (`h3`, stub EAX `0x100`).
- **`0x19020` and `0x35050`: an unregistered callback.** The port skips a code pointer `fn_resolve` does
  not know (`/* PORT: */`); the rows only use registered ones.
- **`0x19068`'s `0x3C570` result.** The raw tests AL (`0x19096`); `0x3C570` runs real on both sides and its
  port returns 0 or 1, so the width is not exercised.
- **Stubs that write memory.** `D1A_DEAD`, `D1A_PAL` and `D1A_CONSUME` write a byte or a word the real
  callee may not; they exist only to make a re-read (or its absence) observable and claim nothing about
  the callees.
- **EAX-mask chains not closed:** `0x19820`, `0x34978` and `0x35D20` (§1, each marked **limit**).
- **Runs per mutant.** Each row and mutant was run at least five times with identical results (the
  `--function` self-check, the pin measurement, the full self-check and the unittest class on the final
  tree, and repeat loops); the report gives the final count.

## §5 Store sweep

For each field a member writes, the pre-state differs from the value written and the bytes beside it are
seeded; at least one dropped store per row is a mutant.

| row | fields written (pre-state seeded) | dropped/narrowed-store mutants |
|---|---|---|
| `fighter_19020` | `DS_00100AF8[side]` dword (both `0x5A5A5A5A`) | `@byte`, `@idx` |
| `fighter_pass_b` | B5A/B5C/B5E bytes (all eight B58..B5F), slot+0x56 (with +0x55, +0x57), rec+0x20/+0x24, `0x107EE0` | `@s56`, `@clr`, `@x20`, `@a`, `@e` |
| `fighter_19164` | B5A, B5C, B58 (B58..B5F seeded `0x91`..`0x98`), rec+0x24 | `@x24`, `@b58`, `@b5a` |
| `fighter_19820` | the two sentinels and ten nodes (`0x100C20`..`0x100CA7` filled), CA8, CA9, `0x104AE8` | `@s28`, `@cnt`, `@ca8`, `@zero`, `@or` |
| `fighter_block_end` | slot+0x43, +0x52, +0x53, +0x60, +0x62 (+0x54, +0x61 beside them) | `@s53`, `@s62`, `@s60`, `@and` |
| `fighter_22b28` | ctx[3]+0x14, +0x58 (+0x57, +0x59), +0x43; `0x10474C` word (both words); ctx[5]+0x24 (+0x20, +0x28); `0x10476C` (both bytes) | `@cb`, `@tickb`, `@x24`, `@s58` |
| `fighter_24754` | slot+8; actor +0x14, +0x34, +0x36 (+0x38), +0x59 (+0x58, +0x5A) | `@slot8`, `@e36`, `@e59` |
| `fighter_33acc` | 0x94 + 0x68 bytes over `0xEE`, four guard bytes after each | `@len1`, `@len2`, `@over`, `@over2` |
| `fighter_34038` | `DS_001077A0[s]` (the dword before and the slot pointers after), `0x104AE9` | `@clr`, `@ae9` |
| `fighter_slots_reset` | `0x1077A8`/`AC` (A4 and B0 beside), `0x1078F6` word, `0x1078FA` byte (F4..FB seeded) | `@n1`, `@n3`, `@f6b`, `@f6`, `@fa`, `@faw` |
| `fighter_35050` | slot+0x14 (+0x10, +0x18) | `@keep`, `@field` |
| `fighter_state_default` | slot+0x43, +0x52, +0x53, +0x54 | `@s53`, `@s52`, `@s54` |
| `fighter_state_35c1c` | rec+0x52 (+0x4F..+0x53), +0x28, +0x29 (+0x26..+0x2B) | `@step`, `@b29`, `@b28` |
| `fighter_state_35d20` | slot+0x43 (+0x42, +0x44), +0x52, +0x53, `0x1078FE` (FD, FF) | `@s52`, `@and`, `@keep` |
| `fighter_state_35d7c` | slot+0x53, +0x54 (+0x52, +0x55; both slots and the table's block) | `@c54`, `@c53` |

**Finding.** `fighter_pass_b`'s first case set did not see the `0x190EE` clear of B5E (`@clr`): every case
that reached it had B5A = 1, whose `0x19145` store rewrites the byte. `p7` now reaches it with B5A = 0x7F.
The other rows write nothing (`0x14590`, `0x18950`, `0x1A5D4`, `0x1A640`, `0x1CEBC`, `0x29C20`, `0x34DDC`,
`0x34E20`) or only through their callees (`0x186C4`, `0x21F30`).

Not separable by a width mutant, by construction: a word-wide `and`/`or` of `0x1A8F4`'s +0x43,
`0x34038`'s `0x104AE9` or `0x19820`'s `0x104AE8` leaves the neighbouring byte unchanged.

## §6 Queue entries not done

| addr | size | why |
|---|---|---|
| `0x1DE64` | 1078 | 69 blocks, four stubbed callees; deferred behind the smaller entries and not reached |
| `0x354F0` | 360 | not reached |
| `0x35E6C` | 279 | not reached |
| `0x35F84` | 213 | not reached |
| `0x3605C` | 184 | not reached |
| `0x361C8` | 183 | not reached |

Nothing was measured for these six; they have no row. The queue was worked top to bottom except for
`0x1DE64`, skipped once for its size.
