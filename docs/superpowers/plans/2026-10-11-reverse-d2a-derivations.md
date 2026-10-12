# Reverse completion D, wave 2, batch d2a: differential rows for game/fighter.c (record)

Track D rows the ported functions the callee scan never reached. Wave 2 takes the functions whose callees
are all rowed. Batch d2a's queue is 25 functions of `port/src/game/fighter.c`
(`.superpowers/sdd/2026-10-11-reverse-d1/d2a-queue.txt`), the last nine of them wave-1 leftovers.

**Status of the numbers.** Every figure below was measured on branch `reverse-d2a` (base `ad0c70d`) against
the image `diffrun` dumps from `data/game/C/PRAGE.EXE` (sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`),
one row at a time with `python3 tools/diff_verify.py --image … --function <row> --self-check`. The full
`--self-check` and the unittest were not run (the integrator runs them). The claim is the harness's:
equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.

Method notes that apply to every row:

- **No new stub address.** `test_each_stub_declares_the_registers_its_callee_clobbers` pins the stub set in
  one literal that parallel batches would all edit, so a callee that is no stub of any row runs real on both
  sides: `allow_calls` where its effect is visible in memory or in its own stubs' calls, or a recorded real
  call (`E.Call(..., mode="real")`, which needs a seam) where only the call list can tell (`0x39278`, §1).
  The cost is named per row in §4: a real callee's port returns 0 or 1, so the raw's `test al,al` after it
  is not separated from a whole-EAX test.
- **No index leaves mem[].** Sides are 0 or 1; every wrong-side or sign-extended read a mutant makes lands
  inside the image and is seeded with the opposite answer (the `d2a_*` fixtures of `tools/diff_verify.py`).
  Each row's binding and every mutant were run ten times on the port side (`diffrun` alone, identical
  output each time) before the pin.
- **Static functions.** A binding cannot call a `static` function: the members marked *static* in §1 lost
  the keyword and gained a declaration in one block of `fighter.h` (linkage only, no behaviour).

## §1 The members and what fixes each EAX mask

The mask is the part of EAX a caller reads. Call sites were found by scanning the image for `call`/`jmp
rel32` to the entry and for the entry as a data dword; none of the members below has a `jmp` tail call or a
data pointer to it.

| addr | port function | size | mask | what fixes it |
|---|---|---|---|---|
| `0x146F0` | `fighter_146f0` (*static*) | 291 | 0 | the one caller overwrites EAX at once (`0x146DB` call, `0x146E0` `mov eax,[esp+8]`) |
| `0x14A5C` | `fighter_14a5c` (*static*) | 308 | 0 | `0x14A47` call, `0x14A4C` `mov eax,[esp+8]` |
| `0x192DC` | `fighter_192dc` (*static*) | 212 | 0 | the one caller returns at once (`0x19580` call, `0x19585`..`0x1958A`, in `0x193B0`); record d1b §1 (`0x39278`) follows that return: every path ends in `xor eax,eax`, `mov eax,esp` (`0x19774`) or `mov eax,[0x107edc]` (`0x3CB79`) with no read of EAX |
| `0x235C4` | `fighter_235c4` | 151 | 0 | `0x3B65E` call, `0x3B663` `jmp 0x3b6ac`, `0x3B6AC` `mov eax,[esp+0xc]` |
| `0x35B7C` | `fighter_state_35b7c` (*static*) | 158 | 0 | six sites: `0x359FF`, `0x35A40`, `0x35A5F` return (`pop`×4, `ret`) from `0x359E0`, whose one caller returns (`0x34C20`..`0x34C25`, the dispatcher `0x34B6C`), whose one call site is followed by `0x35801` `mov eax,esi`; `0x36F6C` is followed by `0x36F7E` `mov eax,[ebx]` or, through `0x36F78` `jne 0x37003`, by `0x37019` `xor eax,eax`; `0x3758D` `jmp 0x37636` returns from `0x37464` to `0x34C94` (the dispatcher again); `0x383F1` returns from `0x382C4` to `0x38569` `mov eax,ecx` |
| `0x36430` | `fighter_state_36430` | 203 | 0 | the one caller returns at once (`0x34C68` call, `0x34C6D`..`0x34C72`, the dispatcher), then `0x35801` |
| `0x364FC` | `fighter_state_364fc` | 201 | 0 | `0x34D7E` call, `0x34D83`..`0x34D88` (the dispatcher), then `0x35801` |

## §2 The rows (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `fighter_146f0` | `0x146F0` | 6 | 12/12 | 26 | `0x0` | EDX = rec. `0x339AC`, `0x14590`, `0x33950` real; `0x3C148`, `0x3C520`, `0x3C480`, `0x1890C`, `0x188DC`, `0x18B04`, `0x2C3FC` stubs |
| `fighter_14a5c` | `0x14A5C` | 6 | 12/12 | 28 | `0x0` | the twin (`0xFD100`/`0xFD11A`/`0x9AFEA`, streams `0xD2FCC`/`0xD2F8A`); its `0x3C480` arm stores +0x54 = 0 and +0x52 = 9 first |
| `fighter_192dc` | `0x192DC` | 13 | 19/19 | 12 | `0x0` | `0x33950` real; `0x39278` real and recorded (new seam). The compares at `0x19307`..`0x19343` and `0x19377`..`0x1939E` are dead (every path reaches the one call at `0x193A0`); `h5`..`h12` walk them |
| `fighter_235c4` | `0x235C4` | 4 | 1/1 | 16 | `0x0` | `0x33A10`, `0x33ACC`, `0x22B28` real; `0x39834` and `0x22B28`'s `0x13C70`, `0x3C16C`, `0x3C148`, `0x2C3FC` stubs |
| `fighter_state_35b7c` | `0x35B7C` | 9 | 9/9 | 15 | `0x0` | `0x35C1C`, `0x35D20` real; their `0x2BCF4`, `0x2BC30` stubs (§3.1) |
| `fighter_state_36430` | `0x36430` | 11 | 12/12 | 23 | `0x0` | `0x1A640` real; `0x365C8`, `0x36638` (AL tested), `0x3BDDC`, `0x2BC30` stubs (§3.2, §3.3) |
| `fighter_state_364fc` | `0x364FC` | 11 | 12/12 | 23 | `0x0` | the twin (`0x1A640` zero: +0x52 = 5, `0xC8FB8` at 2.0) |

The exact kinds and case sets that catch each mutant are `D2A_KINDS` and `D2A_CASES` in
`tools/tests/test_diff_verify.py`; the mutants are the `m_d2a_*` bindings over the `d2a_*_core` functions of
`port/tests/diff_runner.c`, each named by the choice it flips. Mutants caught by the call list or the memory
at a call alone: every index/sign mutant of `fighter_146f0`/`fighter_14a5c` that only changes an argument
(`@gate`, `@oside`, `@cmd`, `@m30`, `@fd`, `@ysh`, `@sign`, `@x2c`, `@dxsh`, `@xw`, `@a`, `@hold`, `@voice`),
their `@order`/`@early`/`@late`; every mutant of `fighter_192dc` but `@arg`; `fighter_235c4@react`,
`@pside`, `@order`.

## §3 Corrections (the raw over the port)

Each is its own commit, and each is unproven against the capture oracles until the integrator's
`make verify`.

### §3.1 `0x35B7C` `fighter_state_35b7c`: the step divides the 16-bit difference (commit `b6d299b`)

- **Raw.** On the `frame >= max/2` arm: `movsx ax,[ebx+0x52]` (`0x35BB7`), `sub edi,eax` (`0x35BBC`; EDI
  holds the `0xBDA3E` word), `mov eax,edi`, `mov esi,3`, `cwd` (`0x35BC5`), `idiv si` (`0x35BC7`), and the
  quotient's low byte is stored at rec+0x58 (`0x35BD5`): a 16-bit signed division of the 16-bit difference.
- **Old port.** `(u8)((max - (s32)(s8)frame) / 3)` on the 32-bit difference.
- **Measured before the fix** (case `n7`: word `0x8000`, frame `0x7F`): `byte 0x10A158: original 0x80, port
  0x2B` (`0x8000 - 0x007F = 0x7F81`, / 3 = `0x2A80`; the port divided -32895).
- **New port.** `(u8)((s16)(u16)(max - (s32)(s8)frame) / 3)`.
- **Mutant.** `@w32` (the old code) is caught by `n7` alone.
- **Scope.** The two agree whenever `max - frame` fits a signed word; the image's seven table words are
  `0x18 0x16 0x10 0x1E 0x14 0x18 0x16`, so no game state differs.

### §3.2 `0x36430`/`0x364FC`: the bit-2 test reads the side's own slot (commit `3d33349`)

- **Raw.** `lea ecx,[esi*8]; add ecx,esi; shl ecx,2; add ecx,esi` (`0x3648B`..`0x36497`; `0x36557`..`0x36563`
  in the twin) overwrite the slot pointer with side × 0x25, and `test byte [ecx*4+0x1077f3],4` (`0x36499`;
  `0x36565`) reads `DS_001077B0 + side*0x94 + 0x43`.
- **Old port.** `DSB(slot + 0x43u) & 4u` on the EAX slot.
- **Measured before the fix** (a scratch slot in EAX; cases `k6`, `k7` of both rows): `call #2: original
  0x3BDDC(0x1), port none` and `call #2: original none, port 0x3BDDC(0x0)`.
- **Mutant.** `@slot43` (the old code) is caught by `k6` and `k7`.
- **Game effect.** None found: the dispatcher `0x34B6C` passes the side's own slot (`DS_001077A8[side]`,
  whose one non-zero store is the slot's own address; record d1a §3.2).

### §3.3 AL-width alignments (behaviour-neutral in the game; commit `0e42ea3`)

The raw tests AL after these calls; the port compared the whole `int`. The C callees return 0 or 1, so no
run changes; the rows stub the callee with EAX = `0x100` (AL = 0). Each now reads `(u8)callee(...)`.

| function | call | the raw's test | mutant, caught by |
|---|---|---|---|
| `fighter_state_36430` | `0x365C8` | `0x3643E` `test al,al` | `@al365`, `k2` (measured before: `byte 0x1077F3: original 0xBB, port unchanged`) |
| `fighter_state_36430` | `0x36638` | `0x36455` `test al,al` | `@al366`, `k2` (measured before: `call #2: original 0x3BDDC(0x0), port none`) |
| `fighter_state_364fc` | `0x365C8` | `0x3650A` `test al,al` | `@al365`, `k2` |
| `fighter_state_364fc` | `0x36638` | `0x36521` `test al,al` | `@al366`, `k2` |

## §4 Named gaps, limits and unhit blocks

- **Unhit blocks: none** in the rows above.
- **Seam added.** `fighter_39278` opens with `PR_SEAM(0x39278u, v)`: `0x192DC` calls it once or twice with
  the same argument, and both leave the same `DS_001088EC` word, so only the call list tells them apart.
  `0x39278`'s own row was re-run (VERIFIED, 4/4); no other spec names `0x39278`.
- **AL against EAX not separable where the callee runs real.** `0x146F0`/`0x14A5C` test AL after `0x14590`
  (`0x14707`, `0x14A73`), whose port returns 0 or 1. `0x35B7C` tests the whole EAX after `0x35C1C`
  (`0x35C05`), which returns 0 or 1 (`mov eax,ecx` `0x35CAE`), so a port testing AL would agree.
  `0x36430`/`0x364FC` test the whole EAX after `0x1A640` (`0x364D7`, `0x365A3`), whose values are 0,
  `0x1000` and `0x2000`: AL is 0 for all three, so there the width is separated (`@al1a`, caught by `k4`
  and `k9`).
- **`0x146F0`/`0x14A5C`: the per-side dword's width.** `or ecx,[eax*4+0xfd108]` reads a dword, of which only
  bit 13 is read afterwards (`test ch,0x20`): a word read is equivalent and no mutant separates it.
- **`0x192DC`: what the dead compares read.** `0x107D2A + side*2` (dword, `sar 16`) and the bytes
  `0x107804`/`0x107898` are read by the raw and decide nothing; the port does not read them, and no
  difference can follow.
- **`0x235C4`: one block.** The row's four cases differ in the side, the pset index and the other slot's
  +0x53 (what the real `0x22B28` compares); the function itself has no branch.

## §5 Store sweep

For each field a member writes, the pre-state differs from the value written and the bytes beside it are
seeded; at least one dropped or misplaced store per field is a mutant.

| row | fields written (pre-state seeded) | dropped/narrowed/misplaced-store mutants |
|---|---|---|
| `fighter_146f0`, `fighter_14a5c` | ctx[2]+0x54 (+0x53, +0x55), ctx[4]+0x36 (+0x34, +0x38), ctx[4]+0x44 (+0x42, +0x46), `0x1078F8[side]` (F7..FA), ctx[2]+0x42 (+0x41, +0x43; pre `0xFF`, `0x04`, `0xFB`), ctx[2]+0x57 (+0x56, +0x58); the twin's +0x52 | `@s54`, `@w36`, `@wide`, `@w44`, `@rec`, `@f8`, `@and`, `@s57`, `@twin`, `@s54z`, `@s52` |
| `fighter_192dc` | none of its own (`0x39278` writes `DS_001088EC`; EA..EF seeded) | `@arg` |
| `fighter_235c4` | ctx[3]+0x52, +0x53, +0x10, +0x18, +0x1C (the whole slot is slot-distinct bytes); the two snapshot areas `0x104530`..`0x104727` (0xEE) through the real `0x33ACC` | `@s52`, `@s53`, `@cb`, `@x18`, `@x1c`, `@slot`, `@nosnap`, `@dst`, `@scale`, `@late` |
| `fighter_state_35b7c` | slot+0x52 (+0x51), slot+0x53 (+0x54), rec+0x58 (+0x57, +0x59) | `@s52`, `@s53a`, `@s53k`, `@neg`, `@w32`, `@div` |
| `fighter_state_36430`, `fighter_state_364fc` | slot+0x43 (+0x42, +0x44), slot+0x52 (+0x51, +0x53), slot+0x54 (+0x55) | `@or`, `@s52a`, `@s54`, `@s52b` |

**Finding.** None beyond §3: no row's first case set missed a store.

## §6 Queue entries not done

| addr | size | why |
|---|---|---|
| `0x47A00` | 233 | **integrator**: the coordinator took it out of this batch (record d1b §3.8's finding changes reachable gameplay and an existing unit test; it lands separately with the full oracle gate). Nothing was measured here |
