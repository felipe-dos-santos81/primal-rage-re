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

**Closure (final tree).** 13 of the 17 queue entries are rowed, all `VERIFIED`, with nine corrections
of the port to the raw (§3) and four entries not reached (§6). The one full self-check
(`PR_DIFF_JOBS=4 python3 tools/diff_verify.py --image /tmp/pr_d3c_img.bin --self-check`) ends `rc 0`
with `665/665 functions VERIFIED; 6081/6081 mutants detected; 1 named gaps; 437/498 rows with callees
closed (167 have none)` (652 + 13 rows, 5650 + 431 mutants), and the 13 rows' pins were measured again
on that tree, one row at a time (0 differences). The unittest, `make verify`, `make diff-verify` and
the oracles were not run.

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
| `0x3BB90` | `fighter_body_push` | 221 | `0xFF` | the function's own AL (`0x3BC61` `mov al,1`, `0x3BC67` `xor al,al`). Neither caller reads it (`0x2541D` call, `0x25422` `call 0x12d48`, which starts `mov al,[0xf0afe]`; `0x25509` call, `0x2550E` `mov eax,1`; record d2a §1 follows bits 8..31), so the mask is stricter than the callers need |
| `0x40608` | `fighter_40608` | 91 | 0 | the one caller writes EAX (`0x40585` call, `0x4058A` `mov eax,[0x108098]`) |
| `0x3D004` | `hit_3d004` (*static* before this batch) | 183 | `0xFF` | `mov al,1` (`0x3D087`) / `xor al,al` (`0x3D0B4`); the one caller reads AL (`0x384A5` call, `0x384AA` `test al,al`) |
| `0x470F8` | `ai_manage` (*static* before this batch) | 269 | 0 | the one caller writes EAX (`0x472DA` call, `0x472DF` `xor eax,eax`) |
| `0x46F4C` | `ai_pick` (*static* before this batch) | 427 | 0 | the one caller reads no EAX before it returns (`0x471CA` call, `0x471CF`..`0x47204` load EBP, EBX, CL, CH and store; in `0x470F8`), and that function's one caller writes it (`0x472DF`) |
| `0x469A8` | `ai_classify` (*static* before this batch) | 458 | 0 | the one caller writes EAX before reading it (`0x47291` call, `0x47296` `mov edx,[esp]`, `0x47299` `lea eax,[edx*8]`) |
| `0x3531C` | `fighter_state_3531c` | 467 | 0 | the one call site writes EAX before reading it (`0x35803` call, `0x35808` `mov dh,[ebx+0x28]`, `0x3580B` `or dh,1`, `0x3580E` `mov eax,ebx`) |

## §2 The rows (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `fighter_winner_body` | `0x193B0` | 9 | 19/19 | 52 | `0x0` | EAX = side. `0x33950` real; the `+0x1C` hook is the registered `0x3E4C4`, run real (it calls `0x33950` and the `0x3B714` stub). Stubs: `0x34D8C`, `0x1922C`, `0x3962C`, `0x396AC` (AL tested), `0x18B44`, `0x39A10`, `0x19164`, `0x3C148`, `0x3C16C`, `0x2BD44`, `0x3B714`, the `+0x14` hook `0x29D04` (EAX tested whole), `0x192DC` (§3.1) |
| `fighter_state_350d0` | `0x350D0` | 16 | 33/33 | 56 | `0x0` | EAX = side. `0x33950` real. Stubs: `0x29D04`, `0x36BC8`, `0x39280`, `0x3BDDC`, `0x34DDC`, `0x3CF38` (AL tested), `0x18B04`, `0x39040`, `0x1922C`, `0x1DE64` (AL kept), `0x34E2C` (§3.2) |
| `fighter_state_359e0` | `0x359E0` | 10 | 26/26 | 41 | `0x0` | EAX = slot, EDX = rec, EBX = side. Stubs: `0x365C8`, `0x1A570` (AL tested), `0x1A5D4`, `0x1A640` (EAX tested whole), `0x35B7C`, `0x367DC`, `0x35C1C` (rewrites the frame byte), `0x1883C` (§3.3) |
| `fighter_state_37464` | `0x37464` | 10 | 23/23 | 38 | `0x0` | EAX = side. Stubs: `0x1A570` (AL tested), `0x35B7C`, `0x36638`, `0x35C1C`, `0x1883C` (§3.4) |
| `fighter_382c4` | `0x382C4` | 16 | 29/29 | 32 | `0x0` | EAX = side. Stubs: `0x38154`, `0x35B7C`, `0x35C1C`, `0x1883C` (§3.5) |
| `fighter_36f10` | `0x36F10` | 8 | 20/20 | 44 | `0x0` | EAX = slot. Stubs: `0x35B7C`, `0x2BC30` (writes `DS_001078FD`), `0x2AE14`, `0x39A10`, `0x2C3FC`, `0x34038` (§3.6) |
| `fighter_body_push` | `0x3BB90` | 9 | 12/12 | 22 | `0xFF` | no argument. Stubs: `0x186D0`, `0x4FB20` (AX tested; rewrites the width sum), `0x3BAEC`. No difference found |
| `fighter_40608` | `0x40608` | 2 | 3/3 | 16 | `0x0` | no argument. `0x249C0` real and recorded (32 calls); `0x406B4` a stub. No difference found |
| `hit_3d004` | `0x3D004` | 6 | 10/10 | 23 | `0xFF` | EAX = side. Stubs: `0x3CD44` (EAX tested whole), `0x3CE58` (AL tested), `0x3C6A8`, `0x32BAC`. No difference found |
| `ai_manage` | `0x470F8` | 11 | 15/15 | 21 | `0x0` | EAX = side. `0x46F4C` a stub that changes the counter the raw reads after it. No difference found |
| `ai_pick` | `0x46F4C` | 8 | 22/22 | 28 | `0x0` | EAX = side. Stubs: `0x46C78`, `0x46DD4`, `0x5D7DC` (the draw). The seven targets of the table `0x46F30`. No difference found |
| `ai_classify` | `0x469A8` | 14 | 30/30 | 22 | `0x0` | EAX = param. `0x33950` real; ten stubs, each AL-tested (§3.7) |
| `fighter_state_3531c` | `0x3531C` | 28 | 25/25 | 36 | `0x0` | EAX = side. The sixteen entries of the table `0x352DC`. Stubs: `0x2C3FC`, `0x2B150`, `0x350D0`, `0x367DC`, `0x34E20` and `0x3CF38` (AL tested), the `+0x10` hook; the `+0x0C` hook `0x23130` real with `0x33950` and a `0x3C4CC` stub (§3.8, §3.9) |

13 rows, 147 cases, 431 mutants, every mutant detected, every block hit (no block is named unhit).

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

### §3.7 `0x469A8` `ai_classify`: AL after each of its ten predicates (commit `086d109d`)

| call | callee | the raw's test |
|---|---|---|
| `0x469BB` | `0x3C59C` | `0x469C0` `test al,al`, `jne` |
| `0x469D7` | `0x46958` | `0x469DC` |
| `0x46A07` | `0x46698` | `0x46A0C` |
| `0x46A20` | `0x466F4` | `0x46A25` |
| `0x46A40` | `0x4673C` | `0x46A45` |
| `0x46A60` | `0x46794` | `0x46A65` |
| `0x46A80` | `0x467DC` | `0x46A85` |
| `0x46AA0` | `0x4682C` | `0x46AA5` |
| `0x46AC0` | `0x468D8` | `0x46AC5` |
| `0x46B3F` | `0x46898` | `0x46B44` |

- **Old port.** `== 0` / `!= 0` on the whole `int` of each.
- **Measured before the fix** (the stubs return `0x100`): `c2: byte 0x108254: original 0x02, port
  unchanged` and `byte 0x108258: original 0x0D, port 0x01` (`0x3C59C`); `c3: byte 0x108218: original
  0x00, port 0x0D` (`0x46698`); `c4: call #2: original 0x466F4(0x0), port 0x46698(0x0)` (`0x46958`);
  `c5: call #3: original 0x4673C(0x1), port none` (`0x466F4`); `c6: call #4: original 0x46794(0x0),
  port none` (`0x4673C`); `c7: call #5: original 0x467DC(0x1), port none` (`0x46794`). The run's
  output was cut after `c7`; `c8`..`c13` (`0x467DC`, `0x4682C`, `0x468D8`, `0x46898`) are measured
  through the mutants, which are the old lines.
- **Mutants.** `@alflag` (`c2`), `@already` (`c4`), `@al698` (`c3`), `@alpred` (`c5`..`c10`: the six
  table predicates), `@al898` (`c13`).
- **Ordinary play.** Not reached: the C predicates return 0 or 1.

### §3.8 `0x3531C` `fighter_state_3531c`: state 8 works on the side's own slot (commit `8e6fd7b8`)

- **Raw.** State 8 (`0x3547A`) increments `[ecx+0x56]` and masks `[ecx+0x43]` through ECX, the
  `DS_001077A8[side]` pointer; then `0x35483` `mov eax,[esp+8]` (the slot `0x1077B0 + side*0x94`,
  built at `0x35345`..`0x3535F`) and `0x35491` `cmp dx,[eax+0x88]`; `0x3549A` `mov edx,[esp+8]`,
  `0x354A0` `mov al,[edx+0x5f]`; `0x354AC` `mov eax,[esp+8]`, `0x354B2` `mov dl,[eax+0x5f]`, `0x354B5`
  `mov byte [eax+0x53],0`; `0x354C5` `mov eax,[esp+8]`, `0x354C9` `mov byte [eax+0x53],8`, `0x354CD`
  `mov [eax+0x5f],dl`.
- **Old port.** Every one of them through the pointer.
- **Measured before the fix** (`d10`: the pointer names a scratch slot whose `+0x88` fails the
  threshold test the side's own slot passes): `call #0: original 0x34E20(0x11), port none`, `byte
  0x107803: original 0x08, port unchanged`.
- **Mutant.** `@s8ptr` (the old code), caught by `d10`.
- **Ordinary play.** None found: the one non-zero store to `DS_001077A8[side]` is the slot's own
  address (record d1a §3.2).
- **Not built alone.** Commit `8e6fd7b8` was not compiled in isolation (the tree was built with
  §3.9's change on top).

### §3.9 `0x3531C` `fighter_state_3531c`: AL after `0x34E20` and `0x3CF38` (commit `96b7b44a`)

- **Raw.** `0x354A3` `call 0x34e20`, `0x354A8` `test al,al`; `0x354BC` `call 0x3cf38`, `0x354C1`
  `test al,al`.
- **Old port.** `fighter_34e20(...) == 0` and `hit_chain_resolve(side) != 0` on the whole `int`.
- **Measured before the fix** (`d12`: `0x34E20` returns `0x100`; `d13`: `0x3CF38` returns `0x100`):
  `d12: call #1: original none, port 0x3CF38(0x0)`; `d13: byte 0x107897: original unchanged, port
  0x00` (the `+0x53 = 8` store not made).
- **Mutants.** `@al20` (`d12`), `@alh` (`d13`).
- **Ordinary play.** Not reached: the C callees return 0 or 1.

## §4 Named gaps, limits and unhit blocks

- **Unhit blocks.** None in the six rows of §2: every leader of each scan is executed.
- **Harness lines outside the blocks.** (1) `tools/tests/test_diff_verify.py`, inside the expected
  dictionary of `test_each_stub_declares_the_registers_its_callee_clobbers`, five inserted lines (no
  existing line changed), each ending `# track D3 batch d3c`: the stub addresses this batch adds with
  their `E.callee_clobbers` sets. (2) `tools/diff_emu.py`, after `_CALLEE_CLOBBER_FIXES`, two
  commented entries, `0x46C78` (below) and `_CALLEE_CLOBBER_FIXES[0x29D04] = ()`: `0x29D04` pushes EBX, ECX, EDX (`0x29D04`..`0x29D06`)
  and pops all three before each of its two `ret`s (`0x29D12`..`0x29D14`; `0x29D5B`..`0x29D5D`), but
  `mov eax,eax` (`0x29D5E`) stands between the second run of pops and the `ret` at `0x29D60`, which
  the scan's pops-directly-before-every-`ret` rule rejects, so it derived `(ebx, ecx, edx)`. `0x350D0`
  keeps the side in EBX across the hook call (`0x3514C`; read at `0x35227`), which that set would
  poison.
- **Seams added.** Thirty, listed at the end of this section. Every seamed callee was only run real
  (allowed) by earlier rows, never recorded, so no existing row's port output changes; the one full
  self-check of the final report is the proof.
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

- **`0x3BB90`: the penetration's upper bits.** `0x3BAEC` reads AX alone (`0x3BB37`/`0x3BB4C` `movsx
  ebx,ax`); the raw passes `EAX = (the sum word) - (0x4FB20's AX)` as 32 bits (`0x3BC4A`..`0x3BC5A`)
  and the port passes the low word sign-extended. The two differ in bits 16..31 when the difference
  leaves a signed word, which the cases do not do (the stub's sum is `0x0345`); the bits are not read.
- **`0x3BB90`: the `0x4FB20` stub writes the width sum** (`0x0345`), so the penetration proves the
  word is read again after the call (`0x3BC4C`); the sum the function stored is compared in the
  memory at that call.
- **`0x40608`: stale links.** The sentinels' pre-state dwords point into the scratch BSS at
  `0x10A000`.., so `@noefc`, which follows the stale `0x107EFC` link, stays inside mem[] (its first
  form, a link of `0x44440000`, was the batch's one `diffrun failed (-11)`).
- **`0x46F4C`: a character above 6.** `0x46F68` `cmp al,6`, `0x46F6A` `ja 0x46fe1` skips the seven
  table arms, leaving EBP and `[esp]` unset; the port takes character 6's tables there. Not
  exercised: the raw's values are whatever the caller left.
- **`0x46F4C`: `0x46C78`'s registers.** `tools/diff_emu.py` gains `_CALLEE_CLOBBER_FIXES[0x46C78] =
  ()`: the function saves EBX and EDX and its own instructions write only EAX, EBX and EDX
  (`_saved_and_written`), its callees `0x33950` and `0x46BBC` clobber EDX alone, and the scan's
  `(ecx, esi, edi, ebp)` comes only from its direct target `0x62003`, the runtime's fatal exit. Its
  raw caller `0x46F4C` loads EBP before the call (`0x46F82`..`0x46FD9`) and reads it after
  (`0x47084`, `0x470C4`).
- **Two C bodies of `0x3C59C`.** `fighter_pass_flag` (seamed) and the static `fighter_frame_flag`
  (`fighter.c`), which `ai_classify` and `fighter_think` (`0x3B464`) call. The second gained the same
  seam, so a row that stubs `0x3C59C` sees either; they were not merged (the two differ only in the
  shift mask, `& 0xff` against `& 0x1F`, on bits 0 and 1).
- **`0x3531C`: the hooks.** The `+0x0C` hook runs real and only as the registered `0x23130`; the
  `+0x10` hook's address `0x3D0BC` is no function and is not registered, so the port reports the
  address alone and its arguments (the port passes the slot and the side; the raw has EAX = the slot,
  EDX = the record, EBX = the side at `0x354E2`) are not compared. Its stub's clobber set `(edx)` is
  what the scan makes of the bytes there and means nothing.
- **`0x3531C`: the `0x3CF38` stub writes both slots' `+0x5F`**, so the byte saved before the call
  (`0x354B2`) and stored back after it (`0x354CD`) is a store the bytes at return can see.
- **Seams, the complete list** (each the callee's first statement): `0x3962C`, `0x396AC`, `0x19164`,
  `0x1922C`, `0x192DC`, `0x29D04`, `0x1DE64`, `0x34DDC`, `0x1A5D4`, `0x35B7C`, `0x35C1C`, `0x367DC`,
  `0x34038`, `0x4FB20`, `0x3BAEC`, `0x406B4`, `0x46F4C`, `0x46C78`, `0x46DD4`, `0x3C59C` (the static
  second body), `0x46958`, `0x46698`, `0x466F4`, `0x4673C`, `0x46794`, `0x467DC`, `0x4682C`,
  `0x46898`, `0x350D0`, `0x34E20`.
- **Linkage.** `fighter_382c4`, `hit_3d004`, `ai_classify`, `ai_pick` and `ai_manage` lost `static`
  and gained a declaration in one block of `fighter.h`.

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
| `fighter_body_push` | `DS_00107D30` (2F, 31), the four latched dwords `0xD3388`..`0xD3397` (sentinels), the width sum `0xD33A8` (A6, AA; compared at the `0x4FB20` call) | `@no30`, `@pswap`, `@xy`, `@no94`, `@nosum` |
| `fighter_40608` | the four sentinel dwords, every node's `+0`/`+4` (the `+8` dwords and the 12 bytes after the pool must survive), `0x1080B0` (AE, B2; pre `0xFF08` in `i1`) | `@noef4`, `@noef0`, `@noefc`, `@noef8`, `@xlink`, `@n31`, `@n33`, `@stride`, `@nocnt`, `@cntb`, `@cnt` |
| `hit_3d004` | `DS_001088A6` (A4, A8), `DS_00104B18` (17, 19), the slot's `+0x7C` (`+0x7B`, `+0x7D`; pre `0xFF` in `t2`), `+0x55` (`+0x54`, `+0x56`), `+0x5F` (`+0x5E`, `+0x60`) | `@noa6`, `@a6b`, `@no18`, `@noinc`, `@incw`, `@no55`, `@c53`, `@no55b`, `@slot`, `@order` |
| `ai_manage` | the block's `+0x04`, `+0x0C`, `+0x00`, `+0x30`, `+0x31` (`+0x32`; the other block seeded) | `@noclr0`, `@noclr7`, `@clrb`, `@no0c`, `@noact`, `@noinc`, `@inc2`, `@no30`, `@no31`, `@early` |
| `ai_pick` | the difficulty dword (C4, the other side's, D0), the block's `+0x20`, `+0x08`, `+0x2E` (`+0x2D`, `+0x2F`), `+0x1C`, `+0x3C` (`+0x38`) | `@noclampn`, `@noclamph`, `@clamp8`, `@gt8`, `@no20`, `@no08`, `@no2e`, `@no1c`, `@no3c` |
| `ai_classify` | block[1 - param]'s `+0x24` and `+0x28` (`+0x20`, `+0x2C`; block[param] seeded) | `@nocopy`, `@stb`, `@nost`, `@bown` |
| `fighter_state_3531c` | `DS_001078F6` (F4), the pointer slot's `+0x40` dword, `+0x56`, `+0x41`, `+0x43`; the side's own slot's `+0x53` and `+0x5F` (`+0x5E`, `+0x60`) | `@decb`, `@orS`, `@noor`, `@noinc4`, `@inc4S`, `@no41`, `@m43`, `@noinc8`, `@no43`, `@no53`, `@v8`, `@no5f`, `@s8ptr` |

**Findings.** `fighter_state_350d0@ald` was not detected by the first case set: `q8` returned at the
`0x1078F8[side] == 0` test on both sides of the AL question; `q8` now sets the byte.
`fighter_state_37464@cosext` and `fighter_382c4@nogate` were not detected either (the `0x80` opponent
character sat in a mode-3 case, where the table word decides nothing; no out-of-range case had the
gate bit clear); `f9` and `g15` separate them. `ai_manage@eq7`/`@eq9` needed the active flag set while the
state equals `+0x10` (`m4`, `m7`). `fighter_state_3531c@cbrec`, `@no22`, `@rS` and `@cb10S` needed the
hook and `0x367DC` cases on the scratch pointer slot (`d7`, `d15`, `d17`), whose `+0x5F` the first
fixture had given the wrong slot.

## §6 Queue entries not done

| addr | size | why |
|---|---|---|
| `0x3B464` | 608 | not reached in the session. Seen while reading, **not measured**: `fighter_think` tests the whole `int` of `fighter_frame_flag(1, ctx[1])` (`0x3B47F`); the bytes after that call were not read |
| `0x3C88C` | 730 | not reached in the session; nothing read |
| `0x1958C` | 464 | not reached in the session (level 1: it calls `0x193B0`, rowed here; `fighter_winner_body` has no seam yet). Its bytes were read for `0x193B0`'s mask only |
| `0x33B00` | 279 | not reached in the session (level 1: it calls the named non-row `0x2EA30`); nothing read |
