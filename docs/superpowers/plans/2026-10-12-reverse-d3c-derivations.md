# Reverse completion D, wave 3, batch d3c: differential rows for game/fighter.c (record)

Track D rows the ported functions the callee scan never reached. Wave 3 takes level 0 (every callee
rowed) and level 1 (calls a level-0 function of this wave). Batch d3c's queue is 17 functions of
`port/src/game/fighter.c` (`.superpowers/sdd/2026-10-11-reverse-d1/d3c-queue.txt`). §2 lists the rows
that landed, §6 the queue entries that did not.

**Status of the numbers.** Every figure below was measured on branch `reverse-d3c` (base `7a9dfe7`)
against the image `diffrun` dumps from `data/game/C/PRAGE.EXE` (sha1
`ff3b8cb14e00f1c282de7b7e15dcd7c230766947`), one row at a time with `python3 tools/diff_verify.py
--image /tmp/pr_d3c_img.bin --function <row> --self-check`. The claim is the harness's: equivalence on
the exercised blocks and inputs only, each function with its callees stubbed or run as stated. Every
correction of §3 is unproven against the capture oracles until the integrator's `make verify`.

Method notes that apply to every row:

- **Callees are stubs.** Wave 3's functions call more than the earlier waves', and what the earlier
  rows found most often was a `test al,al` the port made on the whole `int`. A real callee's port
  returns 0 or 1 and cannot separate the two, so each callee whose result the raw tests is a stub
  whose EAX the cases vary (`0x100`, `0xFFFFFF00`, `0xFFFFFF01`). The stub addresses this batch adds
  are in one line of the expected-clobber dictionary of
  `test_each_stub_declares_the_registers_its_callee_clobbers` (§4, "Harness lines outside the blocks").
- **No index leaves mem[].** Sides are 0 or 1; a character or side byte of `0x80` indexes inside the
  image or just above it, and what a mutant's wrong read finds is seeded with the opposite answer.
  Each row's binding and every mutant were run ten times on the port side (`diffrun` alone, identical
  output each time) before the pin.
- **Mutants.** Each row has one restatement of the function from the raw in
  `port/tests/diff_runner.c` (`d3c_<addr>_core`), with one choice flipped per mutant number; the
  binding itself calls the port function.

## §1 The members and what fixes each EAX mask

Call sites were found by scanning the image for `call`/`jmp rel32` to the entry and for the entry as a
data dword. None of the members below has a `jmp` tail call or a data pointer to it.

| addr | port function | size | mask | what fixes it |
|---|---|---|---|---|
| `0x193B0` | `fighter_winner_body` | 475 | 0 | the one caller returns at once (`0x1974D` call, `0x19752`..`0x1975B`, in `0x1958C`); the seven callers of `0x1958C` write EAX before reading it: `xor eax,eax` at `0x262F3`, `0x26482`, `0x265DF`, `0x2747D`, `0x29AE5`; `0x284F8` `call 0x3cb68`, whose first use of EAX is the load at `0x3CB79`; `0x28810` is followed by two `test byte` and `0x2882C` `xor eax,eax` or `0x28827` `call 0x3cb68` |
| `0x350D0` | `fighter_state_350d0` | 522 | 0 | the one caller returns at once (`0x353FF` call, `0x35404`..`0x3540D`, in `0x3531C`), whose one call site writes EAX first (`0x35803` call, `0x35808` `mov dh,[ebx+0x28]`, `0x3580B` `or dh,1`, `0x3580E` `mov eax,ebx`) |
| `0x359E0` | `fighter_state_359e0` | 412 | 0 | the one caller returns at once (`0x34C1B` call, `0x34C20`..`0x34C25`, the dispatcher `0x34B6C`), whose one call site is followed by `0x35801` `mov eax,esi` (`0x357FC` call) |
| `0x37464` | `fighter_state_37464` | 475 | 0 | `0x34C8F` call, `0x34C94`..`0x34C99` (the dispatcher), then `0x35801` |
| `0x382C4` | `fighter_382c4` (*static* before this batch) | 313 | 0 | the one call site writes EAX (`0x38564` call, `0x38569` `mov eax,ecx`) |
| `0x36F10` | `fighter_36f10` | 477 | 0 | the one caller jumps to the dispatcher's return (`0x34BEA` call, `0x34BEF` `jmp 0x34d83`, `0x34D83`..`0x34D88`), then `0x35801` |

## §2 The rows (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `fighter_winner_body` | `0x193B0` | 9 | 19/19 | 52 | `0x0` | EAX = side. `0x33950` real; the `+0x1C` hook is the registered `0x3E4C4`, run real (it calls `0x33950` and the `0x3B714` stub). Stubs: `0x34D8C`, `0x1922C`, `0x3962C`, `0x396AC` (AL tested), `0x18B44`, `0x39A10`, `0x19164`, `0x3C148`, `0x3C16C`, `0x2BD44`, `0x3B714`, the `+0x14` hook `0x29D04` (EAX tested whole), `0x192DC` (§3.1) |
| `fighter_state_350d0` | `0x350D0` | 16 | 33/33 | 56 | `0x0` | EAX = side. `0x33950` real. Stubs: `0x29D04`, `0x36BC8`, `0x39280`, `0x3BDDC`, `0x34DDC`, `0x3CF38` (AL tested), `0x18B04`, `0x39040`, `0x1922C`, `0x1DE64` (AL kept), `0x34E2C` (§3.2) |
| `fighter_state_359e0` | `0x359E0` | 10 | 26/26 | 41 | `0x0` | EAX = slot, EDX = rec, EBX = side. Stubs: `0x365C8`, `0x1A570` (AL tested), `0x1A5D4`, `0x1A640` (EAX tested whole), `0x35B7C`, `0x367DC`, `0x35C1C` (rewrites the frame byte), `0x1883C` (§3.3) |
| `fighter_state_37464` | `0x37464` | 10 | 23/23 | 38 | `0x0` | EAX = side. Stubs: `0x1A570` (AL tested), `0x35B7C`, `0x36638`, `0x35C1C`, `0x1883C` (§3.4) |
| `fighter_382c4` | `0x382C4` | 16 | 29/29 | 32 | `0x0` | EAX = side. Stubs: `0x38154`, `0x35B7C`, `0x35C1C`, `0x1883C` (§3.5) |
| `fighter_36f10` | `0x36F10` | 8 | 20/20 | 44 | `0x0` | EAX = slot. Stubs: `0x35B7C`, `0x2BC30` (writes `DS_001078FD`), `0x2AE14`, `0x39A10`, `0x2C3FC`, `0x34038` (§3.6) |

The exact kinds and case sets that catch each mutant are `D3C_KINDS` and `D3C_CASES` in
`tools/tests/test_diff_verify.py`.

## §3 Corrections (the raw over the port)

Each is its own commit, and each is unproven against the capture oracles until the integrator's
`make verify`. "Reached in ordinary play" is by reading the callees, not by a capture.

### §3.1 `0x193B0` `fighter_winner_body`: AL after `0x3962C` and `0x396AC` (commit `75e5e7a7`)

- **Raw.** `0x193E4` `call 0x3962c`, `0x193E9` `test al,al`; `0x19419` `call 0x396ac`, `0x1941E`
  `test al,al`.
- **Old port.** `fighter_3962c(ctx[0], 0u) != 0 || fighter_396ac(ctx[0], 0u) != 0` on the whole `int`.
- **Measured before the fix** (the stubs return `0x100` in `n1`, `0xFFFFFF00` and `0x100` in `n2`):
  `n1: call #3: original 0x396AC(0x1, 0x0), port 0x18B44(0x107844)`; `n2: call #3: original
  0x396AC(0x0, 0x0), port 0x18B44(0x1077B0)`, `n2: byte 0x10783A: original unchanged, port 0x00`.
- **Mutants.** `@al1` (caught by `n1`, `n2`), `@al2` (caught by `n2`).
- **Ordinary play.** Not reached: the C `fighter_3962c`/`fighter_396ac` return 0 or 1.

### §3.2 `0x350D0` `fighter_state_350d0`: AL after four callees (commit `3acd730c`)

| call | the raw's read | old port | mutant, caught by |
|---|---|---|---|
| `0x3520E` `call 0x3bddc` | `0x35213` `test al,al` | `fighter_attack_consume(side) != 0` | `@alc`: `q8`, `q10` |
| `0x3528E` `call 0x34ddc` | `0x35293` `test al,al` | `fighter_34ddc(side) == 0` | `@ald`: `q8` |
| `0x352A6` `call 0x3cf38` | `0x352AB` `test al,al` | `hit_chain_resolve(side) != 0` | `@alh`: `q11` |
| `0x352BA` `call 0x1de64` | `0x352BF` `xor edx,edx`, `0x352C1` `mov dl,al`, `0x352C3` `cmp edx,0xff`, and EDX is `0x34E2C`'s second argument (`0x352CD`) | `r = hit_reaction_pick(...)`, compared and passed whole | `@alr`: `q11`, `q12` |

- **Measured before the fix.** `q8`, `q10` (`0x3BDDC` returns `0x100`, `0xFFFFFF00`): `call #1:
  original 0x39040(0x1), port 0x18B04(0x0)`. `q11` (`0x3CF38` returns `0x100`): `call #3: original
  0x1DE64(0x1, 0x1), port none`. `q12` (`0x1DE64` returns `0x105`): `call #4: original 0x34E2C(0x0,
  0x5), port 0x34E2C(0x0, 0x105)`. The `0x34DDC` read was hidden behind the `0x3BDDC` one in `q8`;
  it is measured through its mutant `@ald` (the old line), which `q8` catches once `0x1078F8[side]` is
  set there.
- **Ordinary play.** Not reached: the C callees return 0 or 1, and `hit_reaction_pick` returns a byte
  value (its row's mask is `0xFF`, record d2a §1).

### §3.3 `0x359E0` `fighter_state_359e0`: AL after `0x365C8` and `0x1A570` (commit `6811d08f`)

- **Raw.** `0x359EA` `call 0x365c8`, `0x359EF` `test al,al`; `0x35ABA` `call 0x1a570`, `0x35ABF`
  `test al,al`; `0x35AF7` `call 0x1a570`, `0x35AFC` `test al,al`.
- **Old port.** `fighter_state_365c8(slot, rec, side) != 0` and `fighter_actor_bit15_clear(side) != 0`
  (twice) on the whole `int`.
- **Measured before the fix** (`e1`: `0x365C8` returns `0x100`; `e2`: `0xFFFFFF00`; `e7`: `0x1A570`
  returns `0x100`): `e1: call #1: original 0x367DC(0x10A000, 0x10A400), port 0x35B7C(0x10A000,
  0x10A400)`; `e2: call #1: original 0x1A5D4(0x0), port 0x35B7C(0x10A000, 0x10A400)`; `e7: byte
  0x10A418: original 0x08, port 0xE8` (the step added for subtracted).
- **Mutants.** `@al365`, `@al570`.
- **Ordinary play.** Not reached: the C callees return 0 or 1.

### §3.4 `0x37464` `fighter_state_37464`: AL after `0x1A570` (commit `b8749296`)

- **Raw.** `0x374C8` `call 0x1a570`, `0x374CD` `test al,al`, `0x374CF` `je 0x3750b`.
- **Old port.** `fighter_actor_bit15_clear(other) == 0` on the whole `int`.
- **Measured before the fix** (`f1`, the stub returns `0x100`): `byte 0x10A318: original unchanged,
  port 0xD0`, `byte 0x10A319: original 0x00, port 0xFF` (the record's `+0x18` came out `0xFFFFFFD0`
  for `0x00000010`).
- **Mutant.** `@al570`, caught by `f1`.
- **Ordinary play.** Not reached: the C callee returns 0 or 1.

### §3.5 `0x382C4` `fighter_382c4`: a phase above 2 returns (commit `f09ef84e`)

- **Raw.** `0x382E3` `mov dl,[ecx+0x52]`, `0x382E6` `cmp dl,2`, `0x382E9` `jb 0x382f6` (phases 0 and
  1), `0x382EB` `jbe 0x383ed` (phase 2: `0x383ED` `mov edx,ebx`, `mov eax,ecx`, `0x383F1` `call
  0x35b7c`), `0x382F1` `jmp 0x383f6` (the six pops and the `ret`).
- **Old port.** `if (phase >= 2u) { fighter_state_35b7c(slot, rec); return; }`, and a header that said
  "2 or above runs fighter_state_35b7c".
- **Measured before the fix** (`g2`: phase 3; `g3`: phase `0xFF`): `call #0: original none, port
  0x35B7C(0x10A000, 0x10A400)`.
- **New port.** Phase 2 runs `0x35B7C`; a phase above 2 returns. The header says so.
- **Mutant.** `@ge2` (the old code), caught by `g2` and `g3`.
- **Ordinary play.** Reached, by reading (not measured against a capture): `0x384F8` runs this for
  each side on every `game_mode_25_step` frame, and `0x35B7C`'s tail `0x35D20` stores `+0x52 = 9`
  (`0x35D53`) when the approach's frame run ends. From then on the raw does nothing here, while the
  old port called `0x35B7C` again every frame, which stores `+0x52 = 2` and starts the run again.

### §3.6 `0x36F10` `fighter_36f10`: `DS_001078FD` is read again for the second call (commit `cf119d0f`)

- **Raw.** `0x3708B` `xor eax,eax`, `0x3708D` `mov al,[0x1078fd]`, `0x37097` `mov eax,[eax*4 +
  0x1028f8]`, `0x370A3` `call 0x2bc30` (stream `0xE905A`); then `0x370A8` `xor eax,eax`, `0x370AA`
  `mov al,[0x1078fd]`, `0x370B4` `mov eax,[eax*4 + 0x102900]`, `0x370C0` `call 0x2bc30` (stream
  `0xE908A`).
- **Old port.** One `s = DSB(DS_001078FD)` for both table reads.
- **Measured before the fix** (`h7`, with a `0x2BC30` stub that writes the byte 1 while the record's
  side is 0): `call #4: original 0x2BC30(0x10C400, 0xE908A, 0x40800000), port 0x2BC30(0x10C300,
  0xE908A, 0x40800000)`.
- **Mutant.** `@cache` (the old code), caught by `h7`.
- **Ordinary play.** Not shown to be reached: the two agree unless the first `0x2BC30` call stores
  to `DS_001078FD`, which was not looked for in its animation-opcode targets.

## §4 Named gaps, limits and unhit blocks

- **Unhit blocks.** None in the six rows of §2: every leader of each scan is executed.
- **Harness lines outside the blocks.** (1) `tools/tests/test_diff_verify.py`, inside the expected
  dictionary of `test_each_stub_declares_the_registers_its_callee_clobbers`, two inserted lines (no
  existing line changed), each ending `# track D3 batch d3c`: the stub addresses this batch adds with
  their `E.callee_clobbers` sets. (2) `tools/diff_emu.py`, after `_CALLEE_CLOBBER_FIXES`, one comment
  and `_CALLEE_CLOBBER_FIXES[0x29D04] = ()`: `0x29D04` pushes EBX, ECX, EDX (`0x29D04`..`0x29D06`)
  and pops all three before each of its two `ret`s (`0x29D12`..`0x29D14`; `0x29D5B`..`0x29D5D`), but
  `mov eax,eax` (`0x29D5E`) stands between the second run of pops and the `ret` at `0x29D60`, which
  the scan's pops-directly-before-every-`ret` rule rejects, so it derived `(ebx, ecx, edx)`. `0x350D0`
  keeps the side in EBX across the hook call (`0x3514C`; read at `0x35227`), which that set would
  poison.
- **Seams added** (each the callee's first statement; §6 of the final report lists the exact lines):
  `0x3962C`, `0x396AC`, `0x19164`, `0x1922C`, `0x192DC`, `0x29D04`, `0x1DE64`, `0x34DDC`, `0x1A5D4`,
  `0x35B7C`, `0x35C1C`, `0x367DC`, `0x34038`. Every one of them was only run real (allowed) by
  earlier rows, never recorded, so no existing row's port output changes; the one full self-check of
  the final report is the proof.
- **`fighter_382c4` lost `static`** and gained a declaration in `fighter.h` (linkage only): a binding
  cannot call a `static` function.
- **`0x193B0`: what the hooks are.** The `+0x1C` hook runs real and only as `0x3E4C4`, one of the nine
  addresses the port stores there; the row shows the hook is called with EAX = the side and after the
  `+0x90` store, not what any other hook does. The `+0x14` hook is a stub of `0x29D04`, the one
  address the port stores there.
- **`0x193B0`: the stores the stub makes visible.** `0x3B714` is stubbed with a write of `0x77` to its
  second argument's `+0x8A`, so the last `mov byte [slot+0x8a],0` (`0x19576`) changes a byte at
  return (`@s8c`); it claims nothing about `0x3B714`.
- **`0x350D0`: `0x29D04`'s registers.** See "Harness lines" above; the row's stub declares no clobber.
- **`0x359E0`: the command word is read once by the port.** The raw reads `0x1088E0[side]` at
  `0x35A09` and again at `0x35A71`, after `0x365C8` and after `0x1A5D4`/`0x1A640`; the port reads it
  once at entry. No stub of the row writes the word, so the two are not separated; the three C
  callees do not store to it (read, not measured). The port was left as it is.
- **`0x359E0`/`0x37464`/`0x382C4`: the `0x35C1C` stub.** It writes the frame byte `rec+0x52 = 0xFE`,
  so every case's table entry is entry -2 and what varies is the byte there; `@fearly` (the frame
  read before the call) and `@fz` (the frame zero-extended) read other entries. The stub's EAX is not
  read by any of the three (`0x35B39`, `0x375DD`, `0x383A8` load EDX/EDI next).
- **`0x37464`: the pair table is read before the call by the port.** The raw loads `[0x1078dc]` after
  `0x1A570` (`0x374E3`, `0x3751D`); no stub writes it.
- **`0x382C4`: the distance `0x80000000`.** `neg eax` (`0x3837B`) leaves it negative, so `cmp
  eax,0x200; jg` (`0x3837D`) takes it as in range (`g13`). The port's `(v < 0) ? -v : v` is undefined
  in C there; the build under test gives the raw's answer. Not changed.
- **`0x36F10`: `1 - side` against `side ^ 1`.** The two agree for a side byte of 0 or 1; `h6` has
  `0x80`, where the raw takes entry `0x81` of `DS_001077A8` (`0x36F2B` `xor al,1`) and `@x1` reads
  `0x1075AC`, inside mem[].
- **`0x36F10`: the `0x2BC30` stub writes `DS_001078FD`.** A harness value (§3.6); the store at
  `0x37068` follows the arm's own `0x2BC30` call, so the byte the first table read sees is the
  record's side in every case.

## §5 Store sweep

For each field a member writes, the pre-state differs from the value written and the bytes beside it
are seeded; at least one dropped or misplaced store per field is a mutant.

| row | fields written (pre-state seeded) | dropped/narrowed/misplaced-store mutants |
|---|---|---|
| `fighter_winner_body` | both slots' `+0x43` (0xFF; `+0x42`, `+0x44`), the side slot's `+0x8A` (0x5A; `+0x89`, `+0x8B`), `0x100B50[side]` (the words before and after), the side slot's `+0x18`/`+0x1C` and the other slot's `+0x0C`/`+0x14`/`+0x18`/`+0x1C` (the five dwords `+0x0C..+0x1F` of both slots seeded), the other slot's `+0x90` (`+0x8F`, `+0x91`) | `@and2`, `@and3`, `@mask`, `@s8a`, `@s8b`, `@s8c`, `@nokeep`, `@keepd`, `@no90`, `@s90`, `@v90`, `@late90`, `@z18`, `@z1c`, `@keep14`, `@o18`, `@o1c`, `@o0c`, `@and8` |
| `fighter_state_350d0` | the slot's `+0x14`, `+0x40` dword (0xFF bytes), `+0x53` (`+0x52`, `+0x54`), `+0x74` word (`+0x72`, `+0x76`), `+0x56` (`+0x55`); `0x1078F2[side]` (F1, the other side's byte); the pointer slot's `+0x53`/`+0x56` when it is the scratch slot | `@keep14`, `@andb`, `@noand`, `@no53c`, `@v53c`, `@nof2`, `@latef2`, `@w74b`, `@no74`, `@s74`, `@S53`, `@no56`, `@S56`, `@late56` |
| `fighter_state_359e0` | slot `+0x43` (`+0x42`), slot `+0x41` (`+0x40`), rec `+0x18` (`+0x14`, `+0x1C`), slot `+0x4C` (`+0x4A`, `+0x4E`) | `@or43`, `@late43`, `@and41`, `@x18w`, `@sign`, `@no4c` |
| `fighter_state_37464` | slot `+0x52` (`+0x51`, `+0x53`), rec `+0x18` (a high word in the pre-state), slot `+0x43`, slot `+0x4C` | `@no52`, `@v52`, `@x18w`, `@x18z`, `@nox18`, `@order`, `@or43`, `@late43`, `@no4c` |
| `fighter_382c4` | slot `+0x52` (`+0x51`, `+0x53`), slot `+0x4C` | `@no52`, `@v52`, `@late52`, `@no4c` |
| `fighter_36f10` | slot `+0x52`, `+0x53`, `+0x54` (`+0x51`, `+0x55`), `+0x42` word (`+0x41`), `+0x8C` word (`+0x8A`, `+0x8E`); rec `+0x34`, `+0x36` (`+0x32`, `+0x38`), `+0x59` (`+0x58`, `+0x5A`); the other record's `+0x59`; the other slot's `+0x40` dword; `DS_001078F6` (F4), `DS_001078EC`, `DS_001078DC`, `DS_001078FD` (FC, FE) | `@no52`, `@no53`, `@b34`, `@no34`, `@no36`, `@no59`, `@noo59`, `@v59`, `@or16`, `@no42b`, `@nodc`, `@nof6`, `@no54`, `@or42b`, `@noec`, `@early`, `@nofd`, `@s8cb`, `@no8c`, `@late8c` |

**Findings.** `fighter_state_350d0@ald` was not detected by the first case set: `q8` returned at the
`0x1078F8[side] == 0` test on both sides of the AL question; `q8` now sets the byte.
`fighter_state_37464@cosext` and `fighter_382c4@nogate` were not detected either (the `0x80` opponent
character sat in a mode-3 case, where the table word decides nothing; no out-of-range case had the
gate bit clear); `f9` and `g15` separate them.

## §6 Queue entries not done

To be completed with the last row.
