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
| `0x39B30` | `fighter_39b30` (*static*) | 388 | 0 | the one caller overwrites EAX (`0x39D0B` call, `0x39D10` `mov eax,[esp+0xc]`) |
| `0x39FF4` | `fighter_39ff4` | 245 | 0 | `0x27FFE` is followed by `0x28003` `call 0x27ed8`, whose first use of EAX is a write (`0x27EE6` `mov eax,edx`); `0x28064` by `0x28069` `xor eax,eax` |
| `0x3BAEC` | `fighter_3baec` (*static*) | 163 | 0 | the one caller writes AL and returns (`0x3BC5C` call, `0x3BC61` `mov al,1`, `0x3BC63`..`0x3BC66`, in `0x3BB90`), so bits 8..31 leave through `0x3BB90`'s two sites: `0x2550E` `mov eax,1`; and `0x25422` `call 0x12d48`, which writes AL (`0x12D48` `mov al,[0xf0afe]`), then either masks EAX (`0x12D51` `and eax,0xff`) or, on `0x12D4F` `ja 0x12d78`, returns without reading it (`0x12D78`..`0x12DA4`), after which `0x25427`..`0x25455` only write EAX (`0x25436` `mov eax,ebx`) and `0x25457` `call 0x33f08` writes it first (`0x33F15` `mov eax,[ebx+0x1077a8]`) |
| `0x3C758` | `hit_connect` (*static*) | 168 | full | `mov eax,2` / `xor eax,eax` / `mov eax,3` / `mov eax,1`; the three callers read it whole (`0x3C956`, `0x3C9D3` `test eax,eax`; `0x3CAD7` `cmp eax,3`) |
| `0x3C800` | `hit_displace` (*static*) | 121 | `0xFF` | `mov al,1` `0x3C86B` / `xor al,al` `0x3C872`; `0x3CA99` `test al,al` |
| `0x467DC` | `ai_pred_467dc` | 78 | `0xFF` | `mov al,cl` `0x46825` / `xor al,al`; `0x46A85` `test al,al` |
| `0x4682C` | `ai_pred_4682c` | 78 | `0xFF` | `mov al,cl` `0x46875` / `xor al,al`; `0x46AA5` `test al,al` |
| `0x461DC` | `fighter_input_ring_update` | 642 | 0 | the one caller reaches `call 0x38990` on both arms (`0x24CB5` call, `0x24CBA` `cmp byte [0x104b26],0`, `0x24CC3`/`0x24CC8`), whose first uses of EAX are writes (`0x38991` `mov ax,[0xf0aec]`, `0x3899F` `mov eax,[0xf0aec]`) |
| `0x46C78` | `ai_state_setup` (*static*) | 318 | 0 | the one caller overwrites EAX (`0x46FE3` call, `0x46FE8` `mov eax,edx`) |
| `0x354F0` | `fighter_wall_clamp` | 360 | 0 | `0x3581C` is followed by `0x35821` `mov eax,[esp]`; `0x35824` by `0x35829` `call 0x186c4`, whose first instruction is `xor eax,eax` |
| `0x35E6C` | `fighter_state_35e6c` | 279 | 0 | the one caller returns at once (`0x34D6D` call, `0x34D72`..`0x34D77`, the dispatcher), then `0x35801` |
| `0x35F84` | `fighter_state_35f84` | 213 | 0 | `0x34C57` call, `0x34C5C`..`0x34C61` (the dispatcher), then `0x35801` |

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
| `fighter_39b30` | `0x39B30` | 6 | 8/8 | 31 | `0x0` | `0x33A10`, `0x39AC8`, `0x39B14` real; `0x3C148`, `0x3C16C`, `0x3C520`, `0x3C480`, `0x1890C`, `0x3C190` stubs; the x87 hold is the `0x3C520`/`0x3C480` stack word (§4) |
| `fighter_39ff4` | `0x39FF4` | 4 | 11/11 | 31 | `0x0` | both sides per case. `0x34038` and `0x1922C` (with `0x33950`) real; `0x2BD44`, `0x39F40` and `0x34038`'s `0x2B150`, `0x29BC8` stubs |
| `fighter_3baec` | `0x3BAEC` | 8 | 8/8 | 14 | `0x0` | AX = pen. `0x3B9D8` (with `0x33950`) real; its `0x3B8D8` a stub, its `0x1883C` a stub that clears both slots' +0x41 (§4) |
| `hit_connect` | `0x3C758` | 10 | 9/9 | 14 | `0xFFFFFFFF` | `ret 4`; `0x3C600` a stub returning a scratch descriptor, `0x3C6E8` real (§3.4, §3.5) |
| `hit_displace` | `0x3C800` | 6 | 3/3 | 13 | `0xFF` | `ret 8` (the second stack argument is the one read) |
| `ai_pred_467dc` | `0x467DC` | 10 | 9/9 | 8 | `0xFF` | `0x1A5D4` real |
| `ai_pred_4682c` | `0x4682C` | 10 | 9/9 | 8 | `0xFF` | the twin |
| `fighter_input_ring_update` | `0x461DC` | 9 | 49/50 | 23 | `0x0` | both sides per case. `0x1A570` a stub, `0x3C6E8` real; `0x463FB` named unhit (§4, §3.6) |
| `ai_state_setup` | `0x46C78` | 18 | 18/19 | 11 | `0x0` | all 13 live targets of the table `0x46C40`; `0x33950`, `0x46BBC` real; `0x46D97` named unhit (§4) |
| `fighter_wall_clamp` | `0x354F0` | 16 | 22/22 | 21 | `0x0` | `0x188DC`, `0x187FC`, `0x1883C`, `0x3C148` stubs; `0x187FC`'s EAX is the distance (§3.7) |
| `fighter_state_35e6c` | `0x35E6C` | 8 | 8/8 | 23 | `0x0` | `0x2C3FC`, `0x3BDDC` (AL tested), `0x18B04`, `0x3C480`, `0x188AC` stubs (§3.3, §3.8) |
| `fighter_state_35f84` | `0x35F84` | 9 | 7/7 | 22 | `0x0` | `0x1922C` (with `0x33950`) real; `0x3C148`, `0x3C16C` stubs (§3.9) |

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

### §3.3 AL-width alignments (behaviour-neutral in the game; commits `0e42ea3`, `ac25a9f`)

The raw tests AL after these calls; the port compared the whole `int`. The C callees return 0 or 1, so no
run changes; the rows stub the callee with EAX = `0x100` (AL = 0). Each now reads `(u8)callee(...)`.

| function | call | the raw's test | mutant, caught by |
|---|---|---|---|
| `fighter_state_36430` | `0x365C8` | `0x3643E` `test al,al` | `@al365`, `k2` (measured before: `byte 0x1077F3: original 0xBB, port unchanged`) |
| `fighter_state_36430` | `0x36638` | `0x36455` `test al,al` | `@al366`, `k2` (measured before: `call #2: original 0x3BDDC(0x0), port none`) |
| `fighter_state_364fc` | `0x365C8` | `0x3650A` `test al,al` | `@al365`, `k2` |
| `fighter_state_364fc` | `0x36638` | `0x36521` `test al,al` | `@al366`, `k2` |
| `fighter_state_35e6c` | `0x3BDDC` | `0x35F1A` `test al,al` | `@al`, `b3` (measured before: `call #2: original 0x3C480(0x10A300, 0xE9002, 0x40000000), port 0x18B04(0x1)`) |

### §3.4 `0x3C758`/`0x3C800`: the phase word is sign-extended (commit `d2e7fab`)

- **Raw.** `mov ebx,[eax+ebx*2+0x107d56]; sar ebx,0x10` (`0x3C76D`, `0x3C774`) and `mov edx,[ebx+0x107d56];
  sar edx,0x10` (`0x3C818`, `0x3C81E`; again `0x3C84C`, `0x3C852`): the phase that scales the 0x14-byte
  descriptor entry is the signed word at `0x107D58 + side*0x40 + i*2`.
- **Old port.** `(u32)DSW(DS_00107D58 + off)`.
- **Measured before the fix** (phase `0xFFFF`; `hit_connect` `v8`, `hit_displace` `w3`): `eax (mask
  0xFFFFFFFF): original 0x2, port 0x0`; `eax (mask 0xFF): original 0x1, port 0x0` with `byte 0x10A740:
  original 0x56, port unchanged`.
- **Mutant.** `@pz` (the old code) is caught by `v8` / `w3`.
- **Game effect.** None: the one caller `0x3C88C` reaches them only with 0 <= phase <= 7 (`0x3C8DC`..`0x3C8E3`
  reject a negative phase and one above 8; `0x3C8F7` takes 8).

### §3.5 `0x3C758`/`0x3C800`: the stack arguments point at dwords (commit `ea83dc3`)

- **Raw.** `0x3C758`: `mov ecx,[esp+0x10]; and eax,ebx; mov edx,[ecx]; or edx,eax; mov [ecx],edx; cmp ebx,edx`
  (`0x3C7CE`..`0x3C7DA`): the accumulator is read, written and compared as a dword. `0x3C800`: `mov
  eax,[ecx+eax*4+0xa]; mov edx,[esp+0x10]; sar eax,0x10; mov [edx],eax` (`0x3C85E`..`0x3C869`): the
  displacement is stored sign-extended, as a dword.
- **Old port.** Both out-parameters were `u16 *`: a 16-bit accumulate and compare, a word store.
- **Measured before the fix** (through a binding that copied the low word in and out; `hit_connect` `v7`,
  the accumulator `0x00010020`; `hit_displace` `w0`, `w1`): `eax (mask 0xFFFFFFFF): original 0x3, port 0x0`;
  `byte 0x10A742: original 0x00, port unchanged`, `byte 0x10A742: original 0xFF, port unchanged`.
- **New port.** `u32 *out` in both; `hit_slot_step`'s three locals are `u32` (it zero-extends the
  `0x107E58` word as `0x3CA5F` `xor eax,eax; mov ax,..` does and stores the low words as `0x3CAB3` and
  `0x3CB56` do).
- **Mutants.** `hit_connect@ow` (caught by `v7`), `hit_displace@dw` (caught by `w0`, `w1`, `w3`, `w4`).
- **Game effect.** None: each caller's dword has a zero upper half (`0x3C94D` stores ESI = 0, `0x3C9BF`
  stores EDX = 0, `0x3CA5F`..`0x3CA68` zero-extend the word) and only its low word is read back.
- **Not built alone.** Commits `d2e7fab` and `ea83dc3` were not compiled in isolation (the tree was built
  after both and the export).

### §3.6 `0x461DC` `fighter_input_ring_update`: the ring index is the signed word (commit `5a7e4e9`)

- **Raw.** `mov eax,[0x1082d2]; sar eax,0x10; inc eax; cmp eax,0x14; jl` (`0x46420`..`0x4642C`): the index
  is the sign-extended word DS_001082D4 plus one, wrapped to 0 at 0x14 and above, signed.
- **Old port.** `(DSD(DS_001082D2) >> 16) + 1u`, `if (pos >= 0x14u) pos = 0u`: a negative index wrapped to 0.
- **Measured before the fix** (case `r4`, the word `0xFFFE`): `byte 0x10826E: original 0x30, port
  unchanged`, `byte 0x1082D4: original 0xFF, port 0x00` (the raw stores at index -1).
- **Mutant.** `@pz` (the old code) is caught by `r4`.
- **Game effect.** None: `0x46447` is the only instruction that names the word as a store, and it keeps
  it in 0..0x13.

### §3.7 `0x354F0` `fighter_wall_clamp`: `0x187FC` is called twice on the push path (commit `18eb091`)

- **Raw.** `call 0x187fc; test eax,eax; jge` (`0x3558E`..`0x35595`), then `call 0x187fc; neg eax`
  (`0x35597`) or `call 0x187fc` (`0x355A0`); the left wall the same (`0x3560C`, `0x35615`, `0x3561E`).
- **Old port.** One call and its absolute value, under a `/* PORT: */` note that called `0x187FC`
  side-effect-free (it re-latches both slots through `0x186D0`).
- **Measured before the fix** (with `0x187FC` a stub; case `w1`): `call #2: original 0x187FC(), port
  0x1883C(0x1, 0xFFFFFF00, 0x0)`.
- **Mutant.** `@one` (the old code) is caught by every case that reaches the push test.
- **Game effect.** None expected: the port's `0x186D0` recomputes the same slot fields on a second pass
  with nothing changed in between (not measured against a capture).

### §3.8 `0x35E6C` `fighter_state_35e6c`: the side's own slot and record (commit `0d2fd6e`)

- **Raw.** The side is `[edx+0x51]` (`0x35E74`); `0x35E88`..`0x35EA0` put `DS_001077B0 + side*0x94` in
  `[esp+8]` and `0x35EC0`..`0x35EC6` its record in `[esp+0x10]`. `0x35ED0` `or byte [ebx+0x41],0x80` is the
  only use of the EAX slot. From `0x35F2B` `mov eax,[esp+8]` the stores of +0x54, +0x52, +0x53, +0x84 and
  +0x92 and the +0x7A read go through the side's slot; `0x35F58` hands `0x3C480` the side's record and
  `0x35F6D`/`0x35F76` read its +0x18 for `0x188AC`.
- **Old port.** The EAX slot and the EDX record throughout.
- **Measured before the fix** (a scratch slot and a third record; case `b5`): `call #1: original
  0x3C480(0x10A300, 0xE9002, 0x40000000), port 0x3C480(0x10A400, 0xE9005, 0x40000000)`, `byte 0x107896:
  original 0x09, port unchanged`, `byte 0x10A052: original unchanged, port 0x09`.
- **Mutants.** `@slot`, `@rec`, `@x18` (the old code), each caught by `b5`.
- **Game effect.** None found: the dispatcher passes the side's own slot and its record.
- The AL test after `0x3BDDC` (`0x35F1A`) is commit `ac25a9f`, in §3.3's table.

### §3.9 `0x35F84` `fighter_state_35f84`: the gate's record and the stores' slot (commit `dcd9ce4`)

- **Raw.** The gate reads the EAX slot's +0x7A and +0x30 and `mov eax,[ebx]; cmp word [eax+0x36],0`
  (`0x36007`, `0x36009`): that slot's own record. The side is `[edx+0x51]` (`0x35F8D`); `[esp+8]` is the
  side's slot (`0x35FA1`..`0x35FB9`) and `[esp+0x10]` its record (`0x35FD9`..`0x35FDF`); `0x36022`/`0x3602A`
  zero that record's +0x24 and `0x36026`..`0x36042` store +0x52, +0x78 and +0x53 through that slot.
- **Old port.** `rec + 0x36` and `rec + 0x24` on the EDX record, the three stores on the EAX slot.
- **Measured before the fix** (cases `l4`, `l5`, `l6`): `call #0: original 0x3C148(0x1), port none`;
  `call #0: original none, port 0x3C148(0x0)`; `byte 0x107802: original 0x14, port unchanged` with `byte
  0x10A052: original unchanged, port 0x14`.
- **Unit test.** `test_fight.c` case E drove the dispatcher with a scratch slot in the table and asserted
  +0x52 there; it asserts it on the real slot 0 now (the same one check). Under the old port:
  `test_fight.c:8910: 170 != 20`.
- **Mutants.** `@r36`, `@r24`, `@slot` (the old code): `l4`/`l5`, `l4`/`l6`, `l4`/`l6`.
- **Game effect.** None found, as §3.8.

## §4 Named gaps, limits and unhit blocks

- **Unhit blocks, named.** `0x463FB` in `0x461DC`: the fourth toggle reads the low byte's bit 5 again
  (`0x463EC` `mov ax,[esp+ebx]`, `0x463F2` `and al,0x20`) right after `0x463E8` cleared it whenever it was
  set, so `xor byte [esp+ebx+1],0x20` cannot run; the port keeps the same dead test, and `@t4` (testing
  the bit it flips) is caught by `r2` and `r5`. `0x46D97` in `0x46C78`: state 0xA or above 0xD calls the
  runtime fatal `0x62003`; the port skips the weight store there. Every other block of every row is hit.
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
- **`0x39B30`'s x87 domain.** The hold is `(float)(n / d)` of two dwords under the harness's 53-bit control
  word; the port divides in `double` and rounds once more to `float`, as `fstp dword` does. The row keeps
  the divisor dword (`0xBED10[ch]`) non-zero: 0/0 is the x87's negative default NaN and the host's C
  division need not agree, and the image's seven divisors are 5, 6, 7, 7, 8, 4, 4. n = 0 (`0x39AC8`'s
  `idiv` by n*n) and (s16)(n + m) = 0 (`0x39C6D` `idiv ebx`) are #DE in the raw and are not exercised.
- **`0x39B30`: equivalent mutation.** `0x39B14` taking the whole `0x39AC8` result for the word re-read
  from rec+0x44 (`0x39C29` `mov eax,[eax+0x42]; sar eax,0x10`): the product's low word, the only part
  stored (`0x39C3B`), depends on the low word alone. Not bound.
- **`0x39FF4`: equivalent mutations, not bound.** A logical for the arithmetic `sar eax,6` (`0x3A0BE`): the
  `movsx edx,ax` at `0x3A0CE` drops the bits that differ. Keeping bit 12 in the `0x3A04D` mask: `0x3A06B`
  sets it again. `x - cam` for `cam - x` (`0x3A0A5`): the absolute value follows.
- **`0x3BAEC`: stubs that write memory.** `D2A_1883C_CLR` clears both slots' +0x41 where the real `0x1883C`
  re-latches the slots; it exists only to make the bit-15 OR-back (`0x3BB64`..`0x3BB84`) observable and
  claims nothing about `0x1883C`.
- **`0x3BAEC`: the pen's extension.** The raw sign-extends AX before each `0x3B9D8` call (`0x3BB37`,
  `0x3BB4C`); the port passes its `s32` on, and `0x3B9D8` (real here) truncates it again itself (`0x3BA39`),
  so a pen passed unextended is not separated in this row.
- **`0x3C758`/`0x3C800`: `0x3C600`'s result.** The stub returns a scratch descriptor; the real one's value
  (and its default arm, a `/* PORT: */` in the port) is its own row's.
- **`0x461DC`: what is not observable.** `0x3C6E8` gets the zero-extended command word (`xor edx,edx; mov
  dx,[ebx+0x1088e0]` `0x46242`..`0x46246`), on which it is an identity (its gates are bits 16..19), so
  neither its call nor the `cmp eax,1` on `0x1A570`'s AL that guards it (`0x4623D`) changes any byte. The
  A-arm skip (bit 0 with no 0x3030 bit) dropped is equivalent too: the word then matches no later arm.
- **`0x354F0`: the sign call and the value call.** The raw takes the sign from the first `0x187FC` and the
  value from the second; the stub returns the same EAX to both, so a port using the first for both is
  only separated by the call count.
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
| `fighter_39b30` | rec+0x44 (+0x42, +0x46), rec+0x36 (+0x34, +0x38), rec+0x63 (+0x62, +0x64), slot+0x41 (+0x40, +0x42), slot+0x68 (+0x67, +0x69), slot+0x74 (+0x76); the other slot and record seeded differently | `@g44`, `@w36`, `@r63`, `@or41`, `@inc`, `@w74`, `@rec`, `@early` |
| `fighter_39ff4` | rec+0x4B (+0x4A, +0x4C), rec+0x2A (+0x29, +0x2B), slot+0x0C, +0x18, +0x1C (the nine dwords +0x00..+0x23 seeded), slot+0x5D (+0x5C, +0x5E), slot+0x40 dword, slot+0x8C (+0x8A, +0x8E) | `@c4b`, `@f2a`, `@x0c`, `@x18`, `@x1c`, `@s5d`, `@mask1`, `@mask2`, `@or41`, `@t1`, `@tbyte` |
| `fighter_3baec` | both slots' +0x40 dword (the kept bit 15, with other +0x41 bits set) | `@keep0`, `@keep1`, `@kswap`, `@klate`, `@kmask` |
| `hit_connect` | the accumulator dword (the dwords before and after it seeded) | `@nostore`, `@ow`, `@noacc` |
| `hit_displace` | the displacement dword (pre `0x5A5A5A5A`, neighbours seeded) | `@nostore`, `@dw`, `@dz`, `@dlo`, `@s0` |
| `fighter_input_ring_update` | the two ring words (both rings seeded whole, with the word before and after), `0x1082D4` (D2, D6 beside) | `@d4`, `@swap`, `@pz`, `@wrap` |
| `ai_state_setup` | the side block's +0x10 dword (+0x14, +0x18 beside; the other block seeded), and +0x2C/+0x14/+0x34 through the real `0x46BBC` | `@nostate`, `@stbyte`, `@wside` |
| `fighter_wall_clamp` | none of its own | - |
| `fighter_state_35e6c` | the EAX slot's +0x41 (+0x40, +0x42); the side slot's +0x54, +0x52, +0x53 (+0x51, +0x55), +0x84 (+0x82, +0x86), +0x92 (+0x90) | `@or41`, `@s54`, `@s52`, `@s53`, `@inc`, `@incb`, `@w92`, `@slot`, `@order` |
| `fighter_state_35f84` | the EAX slot's +0x41; the side record's +0x24 (+0x20, +0x28); the side slot's +0x52, +0x53 (+0x51, +0x54), +0x78 (+0x76, +0x7A) | `@or41`, `@s24`, `@r24`, `@s52`, `@s53`, `@s78`, `@b78`, `@slot`, `@t19`, `@order` |

The two predicates (`0x467DC`, `0x4682C`) write nothing.

**Findings.** `fighter_39b30`'s first fixture put a stray word before the h pair, so the h, m and a words
were not the ones the cases named: the row verified (both sides read the same bytes) but `@hz` and `@q32`
were not detected, which is what exposed it. The same fixture, and `fighter_state_35f84`'s first one,
poked each character's ground as a dword at `0xBD882 + ch*2`: two neighbouring characters' pokes
overlapped, so `q2`'s ground was `0x8282` for the `0x8000` the case named, and `l0`'s gate failed where
the case meant it to pass (seen as `@ycmp` and `@csext` undetected). Both now poke the ground word alone;
`fighter_39b30`'s pins did not move. `ai_pred_467dc@s54` and `ai_pred_4682c@s53` were not detected
by the first eight cases (each gate failed only where the last test failed too); `e8` and `e9` separate
them.

## §6 Queue entries not done

| addr | size | why |
|---|---|---|
| `0x47A00` | 233 | **integrator**: the coordinator took it out of this batch (record d1b §3.8's finding changes reachable gameplay and an existing unit test; it lands separately with the full oracle gate). Nothing was measured here |
