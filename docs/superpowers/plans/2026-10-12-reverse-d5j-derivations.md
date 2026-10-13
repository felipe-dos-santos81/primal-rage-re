# Track D, wave 5, batch d5j — derivation record

Differential rows for both queue entries (`.superpowers/sdd/2026-10-11-reverse-d1/d5j-queue.txt`):
`0x4BF18` and `0x49C78`, the two largest functions that had no row, both in `port/src/game/fight.c`.
Every value below comes from the image `./build/diffrun --exe data/game/C/PRAGE.EXE --image-out` writes
(fixups applied, linear addresses; sha1 `ff3b8cb1...`), disassembled with capstone, and from running the
original bytes in `tools/diff_emu.py`. Narrow claim, as for every row: equivalence on the exercised blocks
and inputs only, each function with its callees stubbed or run as stated.

## §1 The members

| addr | port function | size | callers and what they read of EAX |
|---|---|---|---|
| `0x4BF18` | `fight_4bf18` (fight.c) | 1697 | one caller, `0x26687`: `call 0x1282c` at `0x2668C`, whose first use of EAX is `mov ax,[0xef6dc]` `0x12834` .. `and eax,0xffff` `0x1283E`. Mask 0. |
| `0x49C78` | `fight_effects_pass` (fight.c) | 2492 | `0x2639B`, `0x2652A`: `call 0x1282c` (`0x263A0`, `0x2652F`), as above; `0x28302`, `0x28518`, `0x28847`, `0x28C04`: `call 0x12da8` (`0x28307`, `0x2851D`, `0x2884C`, `0x28C09`), which loads AL at `0x12DA9` and EAX at `0x12DBA` / `0x12DD2`; `0x2530D`: `jmp 0x2540f` (`0x25312`), `call 0x2a31c` and then `mov ax,[0x104b00]` `0x2545C` or `call 0x3bb90` (the chain the `input_state_update` row, d2d, and d5f's `0x4D2D0` row followed). Mask 0. |

Both functions have more callees than a case holds stubs (`diffrun`: 16 stubs and 40 pokes per case), so
each is several specs on the one entry. The specs of an entry share one binding (`b_d5j_4bf18`,
`b_d5j_49c78`) and one mutant model (`d5j_vb`, `d5j_fx` in `port/tests/diff_runner.c`: the function as the
port writes it, with one deviation per mutant); each spec names the blocks it leaves to the others in
`unhit_named`, and together the specs of an entry hit every block.

## §2 The rows (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `fight_4bf18` | `0x4BF18` | 23 | 62/84 | 111 | 0 | the list walk `0x4BF65..0x4C396`: the nine-entry type table `0x4BEF4` (types 0, 7 and above 8 share `0x4BF86`), the shared tail `0x4C1F3`, the score. Ten stubs reached (five more bound what a mutant can run); `0x496AC` runs on both sides. `0x4A868` whole EAX (`test eax,eax` `0x4BFE3`: a case returns 0x100); `0x1A570` AL (`test al,al` `0x4C29C`: 0x100 and 0xFFFFFF01). |
| `fight_4bf18_post` | `0x4BF18` | 20 | 27/84 | 44 | 0 | an empty list: the preamble `0x4BF21..0x4BF4D` and the post-loop `0x4C39C..0x4C5AA` (the `DS_001088A0` countdown, the frame gate's signed remainder, the counters, the serve prompt). The string path `0x1C500` runs on both sides (`D2B_STR_TREE`). |
| `fight_4bf18_rr` | `0x4BF18` | 8 | 43/84 | 11 | 0 | stubs relink the entry's +8 (`0x4A868`, `0x496AC` — a stub in this spec only —, `0x2B150`, `0x4C60C`), store `DS_00108898` and the entry's +0x1F (`0x2BC30`) and cut the node out of the list (`0x4C60C`). |
| `fight_effects_pass` | `0x49C78` | 33 | 91/139 | 122 | 0 | the midpoint store, types 0..8 and above 0xE (the table `0x49C2C`), the `DS_001088BF` tail (the table `0x49C68`). `0x4BD4C` whole EAX (`0x49D44`: 0x100); `0x4AF04` AL (`test al,al` `0x4A0A7`: 0x100, 0xFFFFFF80). |
| `fight_effects_pass_m9` | `0x49C78` | 31 | 76/139 | 103 | 0 | types 9..14 (and 3 in mode 9) and the mode-9 block `0x4A487..0x4A58F` with its frame locals. `0x4B430`, `0x4A7D4`, `0x4A868` whole EAX (`0x4A154`, `0x4A19B`, `0x4A366`: 0x100 each). |
| `fight_effects_pass_rr` | `0x49C78` | 14 | 91/139 | 17 | 0 | the prelude `0x4B69C` relinks the entry's +8 and +0xC; each stub an arm calls before a loaded-again site relinks +8 a second time; the stream stub repoints the +0xC cell. Sixteen stubs. |
| `fight_effects_pass_pre` | `0x49C78` | 2 | 36/139 | 3 | 0 | the prelude makes the node type 6 and cuts it out of the list; `0x4A634` stores the mode word. |

7 specs, 411 mutants, every one detected (`/tmp/pr_d5j/pins.py`: `pins: 7 rows, 411 mutants; BAD: []`).
The pins (`D5J_MASKS`, `D5J_KINDS`, `D5J_CASES`; `D5J_OUTSIDE` is empty: no row reads outside the image)
are the measured ones. The whole measurement (the seven rows and every mutant) was run ten times with
identical output. Each mutant's catching set is confined to the cases of its own arm (`D5J_CASES`), so
on every case some mutant's binding — the model with a deviation that case does not reach — agrees with
the raw: the two models are the port's functions on the cased paths.

Blocks: `0x4BF18` has 84 leaders, `0x49C78` 139 (`E.static_scan` with the two tables each resolved as
bounded switches). `/tmp/pr_d5j/unhit.py` re-derived every `unhit_named` list by running each case: none
named but hit, none unnamed, and no leader of either entry is left unhit by all of its specs.

Callee declarations (args from the callee's entry bytes, `pop` = its `ret N`, clobbers =
`E.callee_clobbers`; `/tmp/pr_d5j/stubcheck.py`: every stub of `D5J_SPECS` declares the derived set and
agrees with every other spec's declaration of the same address):

| callee | args | clobbers | how |
|---|---|---|---|
| `0x493F0` | none | none | stub (new seam, `PR_SEAM_RET0`) |
| `0x496AC` | eax (`cmp eax,0xb00` `0x496AD`) | none | allowed everywhere but `fight_4bf18_rr`, where it is a stub (new seam) |
| `0x4AC38`, `0x4C60C`, `0x4B69C`, `0x4AAD0`, `0x4B470`, `0x4B2AC` | eax, edx (`mov ebx,edx` `0x4AC39`; `mov esi,eax; mov edi,edx` `0x4C614`, `0x4B474`; `mov ecx,eax; mov esi,edx` `0x4B6A2`, `0x4AAD5`; `mov ebx,eax; mov esi,edx` `0x4B2B1`) | edx | stub (new seams) |
| `0x4CD98` | none | esi, edi, ebp | stub (new seam); the raw returns right after it (`0x4C3BF`) |
| `0x4A634`, `0x4A928` | none | none | stub (new seams) |
| `0x4AF04` | eax (AL: `mov dl,al` `0x4AF09`; the caller zero-extends, `0x4A09D`) | none | stub (new seam) |
| `0x4A7D4` | eax | none | stub (new seam) |
| `0x496DC` | eax, edx (`mov edi,eax; mov [esp+0xc],edx` `0x496E4`) | edx | stub (new seam) |
| `0x4987C` | eax, edx, ebx | ebx, edx | stub (new seam, the form `PR_SEAM(0x4987Cu, side, (u32)count, kind);`) |
| `0x4B3F0`, `0x4B430` | eax, edx, ebx | ebx, edx | stub (`D5J_4B3F0`/`D5J_4B430`: D2B's constants carry a write) |
| `0x1A570`, `0x2B150`, `0x2BC30`, `0x2C3FC`, `0x4A868`, `0x4CC0C`, `0x2F434`, `0x2F510`, `0x2F528`, `0x4BD4C`, `0x2BE1C`, `0x2BE00`, `0x5D7DC` | the existing constants (`BIT15`, `P7_2B150`, `ANIM_BEGIN`, `VOICE`, `D3B_4A868`, `D3B_GAMEOVER`, `D2C_NUMSET`, `D2B_FONT2`, `D2C_NUMDRAW2`, `D3B_4BD4C`, `D3B_2BE1C`, `D2B_2BE00_S`, `D2B_RNG`) | theirs | stub |
| `0x1C500`, `0x474E4`, `0x1E75C`, `0x1E808`, `0x500BB` | - | - | allowed (`D2B_STR_TREE`, in `fight_4bf18_post`) |

`0x4B69C` (`fight_4b69c`) is static in `fight.c` in this tree, so the model cannot call it: `d5j_prelude`
in `diff_runner.c` reaches its seam directly (`PR_SEAM(0x4B69Cu, entry, index)`), and every spec of
`0x49C78` stubs `0x4B69C`. The port side of the row runs the real `fight_effects_pass`, whose own call
reaches the seam inside `fight_4b69c`.

## §3 Corrections (address-proved; the raw wins)

1. **`fight_4bf18` tests AL of `0x1A570`** — `0x4C297 call 0x1a570; 0x4C29C test al,al; 0x4C29E setne al`
   (the score's side) and `0x4C4FF call 0x1a570; 0x4C504 test al,al; 0x4C506 jne 0x4c528` (the serve
   prompt's column). Old: `fighter_actor_bit15_clear(..) != 0`; new: `(u8)fighter_actor_bit15_clear(..) !=
   0u`. The port's `0x1A570` returns 0 or 1, so nothing changes in the game. Cases `g2` (`fight_4bf18`:
   the stub returns 0x100) and `n4` (`fight_4bf18_post`); mutants `@s1eax`, `@p1eax`.
2. **`fight_4bf18`'s frame gate is a signed remainder** — `0x4C3CE xor edx,edx; 0x4C3D0 mov
   ecx,[0x1088d0]; 0x4C3D6 mov dx,[0x104af4]; 0x4C3E2 mov eax,edx; 0x4C3E4 sar edx,0x1f; 0x4C3E7 idiv ecx;
   0x4C3E9 test edx,edx`. Old: `(u32)DSW(DS_00104AF4) % DSD(DS_001088D0)`; new: `(s32)(u32)DSW(..) %
   (s32)DSD(..)`. They differ only for a negative divisor (9 by -3: 0 against 9); `DS_001088D0`'s
   writers (`0x20CB0`, `0x24F4C`, `0x25007`, `0x25163`) were not traced, so whether the game stores a
   negative one is not claimed. Case `m4`; mutant `@remu`.
3. **`fight_4bf18` loads the entry's actor again after its calls** — the raw keeps it in ESI from the
   loop's top (`0x4BF65 mov esi,[ecx+8]`) and loads `[ecx+8]` again at type 1's gate arm after `0x4A868`
   and `0x2C3FC` (`0x4BFF5`, `0x4BFFE`, `0x4C007`, `0x4C010`, `0x4C027`), at the clamp's store and the
   stream call of types 3/4 and 5 (`0x4C0AF`, `0x4C0C2`; `0x4C0F2`, `0x4C113`), at type 6's settle stream
   after `0x2B150` (`0x4C1A9`, `0x4C1C2`), at the tail's held-actor copy after `0x4C60C` (`0x4C231`,
   `0x4C23D`) and at the score's stream (`0x4C30D`). Old: the actor read at the top everywhere. No in-game
   change (the port's callees do not write the entry's +8). `fight_4bf18_rr`'s `r1`, `r3`, `r4`, `r5`,
   `r6`, `rt`, `rs`; mutants `@c1cache`, `@t3cache`, `@t5cache`, `@t6cache`, `@tlcache`, `@scache`,
   `@allcached` (and `@allreread` for the sites that are ESI's).
4. **`fight_4bf18` loads `DS_00108898` again for the score test** — `0x4C321 mov ax,[0x108898]`, after the
   two stream calls (`0x4C2FC`, `0x4C315`), then `0x4C332 mov al,[eax+0x10889b]`; `cmp eax,3`. Old:
   `DSB(DS_0010889C + idx)` with the side computed before the calls; new: `DSB(DS_0010889B +
   (u32)DSW(DS_00108898))` (`DS_0010889B` is a local `#define` in `fight.c`: `symbols.h` has no name for
   it). No in-game change. Case `rs`; mutant `@s898cache`.
5. **`fight_effects_pass` tests AL of `0x4AF04`** — `0x4A09D xor eax,eax; 0x4A09F mov al,[ecx+0x20];
   0x4A0A2 call 0x4af04; 0x4A0A7 test al,al; 0x4A0A9 jne 0x4a468` (type 8's gate; `0x4AF04` sets AL alone
   on its "no" exits, `0x4AF56 xor al,al`). Old: `fight_4af04(..) != 0`; new: `(u8)fight_4af04(..) != 0u`.
   No in-game change (the port's function returns 0 or 1). Case `h2` (the stub returns 0x100); mutant
   `@af04eax`.
6. **`fight_effects_pass` loads the entry's actor again after the prelude and its arms' calls** — the raw
   loads the actor into EBX at the loop's top (`0x49CD8`), before the prelude `0x4B69C` (`0x49CFE`), and
   uses EBX only for type 3's x test, +0x28 bit and +0x38/+0x34 stores (`0x49DCA`, `0x49DEB`, `0x49E29`,
   `0x49E34`), type 4's +0x38/+0x34 stores (`0x49E85`, `0x49EA0`/`0x49EA8`), type 5's +0x32 test and
   stores (`0x49ED5`, `0x49EFC`, `0x49F02`) and type 6's +0x36 test, landing sum and stores (`0x49F11`,
   `0x49F3A`, `0x49F9A..0x49FAB`, `0x49FB8`). Everywhere else it loads `[ecx+8]`: type 1 (`0x49D4C`,
   `0x49D5D`); types 3, 4, 5 for the clamp's argument and store and the `0x2BE1C` / stream records
   (`0x49DB3`, `0x49DC1`, `0x49DE1`, `0x49E1C`; `0x49E71`; `0x49EC0`, `0x49ECE`, `0x49EE8`); type 6's
   held-actor copy and settle stream (`0x49F23`, `0x49F2C`, `0x49F70`, `0x49F89`); type 7 after the voice
   (`0x4A011`, `0x4A046`, `0x4A05E`, `0x4A07E`); types 11 to 14 throughout (`0x4A17F` .. `0x4A461`). Old:
   the actor read at the top at all of them, and type 7's from before the voice. No in-game change (the
   port's prelude and callees do not write the entry's +8). `fight_effects_pass_rr`'s fourteen cases;
   mutants `@c1cache`, `@t3cache`, `@t4cache`, `@t5cache`, `@t6cache`, `@t7cache`, `@t11cache`,
   `@t12cache`, `@t13cache`, `@t14cache`, `@armcache`, `@allcached` (and `@allreread` for the EBX sites).

All six are conformity corrections: with the port's own callees the old and the new code compute the same
thing (2 excepted, for a negative divisor that was not shown to occur). None has been run against the
capture oracles (the integrator's gate); `PR_ORACLE_REQUIRED=1 ./build/run_tests` passes on every
commit's final state. Each is its own commit.

No inverted gate, missing block or wrong table turned up in either function: on every case of the five
specs that do not rewrite state under the function (`fight_4bf18`, `_post`; `fight_effects_pass`, `_m9`,
`_pre`) the port agreed with the raw before any correction, but for the two AL tests and the remainder.

## §4 Named gaps, unhit blocks, limits

No named gap: both functions run in the harness. No `_CALLEE_CLOBBER_FIXES` addition (neither function
keeps a register live across `0x49444`, which it does not call; the d5f note does not apply here).

**Unhit blocks.** None across an entry's specs (§2). Each spec's `unhit_named` lists the leaders the
other specs of the entry run.

**Limits (not cased, with the reason).**

- `0x49C78`'s per-side counts are `[esp + side*2]` with the side byte unbounded (`0x49CDD`); the port
  counts sides 0..2 only (its `PORT:` comment). A side byte of 3 or more aliases the other frame locals
  in the raw (4 and 5 are the entry count `[esp+8]`). The rows use sides 0, 1, 2 in the counted cases; a
  side of 0xFF appears only where the count is not read back (`d3`: it lands at `[esp+0x1FE]`, above the
  frame).
- `[esp+4]` (side 2's count) is never initialised by the raw (`0x49CBC`/`0x49CC1` zero `[esp]` and
  `[esp+2]` only) — the port's own named gap (record §K13.3). The emulator's stack is zero-filled, which
  would hide it; no case reads `[esp+4]` (a `DS_00104B16` of 2 in mode 9, or a side-2 entry whose count
  is read) before a type-3 landing wrote it in the same call (`l0`, `l1`).
- The mode-9 block compares the entry and idle counts as words (`cmp ax,bx` `0x4A554`) and reads type
  14's count as a word (`0x4A499`); a list of 65536 entries is not cased.
- `0x496AC` is a pure leaf, so in the `0x49C78` specs (where it runs on both sides) a port that kept
  `[ecx+8]` between the clamp's argument and its store would not be seen; the prelude's relink still
  separates those sites from EBX. `fight_4bf18_rr` stubs it and does separate them.
- `0x4BF18`'s frame gate with a zero divisor faults on both sides (`idiv`); not cased.
- One stub EAX serves every call of an address in a case: the two `0x1A570` calls of `0x4BF18` are
  cased apart (the score in `fight_4bf18`, the prompt in `fight_4bf18_post`), and so are `0x49C78`'s rng
  draws (`0x3C`, `0xC00`, `0x14`, 2).
- The absolute differences of type 1 (`0x49D5B`/`0x4C043 neg`) and of the bounce (`0x4C378`) are cased
  at 0x80000000, where `neg` leaves the value negative; the port's `d = -d` on an `s32` agreed on this
  build, but signed negation of INT_MIN is undefined in C. Not changed (no mismatch); flagged in the
  report.
- `fight_4bf18` and `fight_effects_pass_pre` carry stubs no case reaches (`0x493F0`, `0x2F434`,
  `0x2F510`, `0x2F528`, `0x4CD98`; `0x2C3FC`, `0x4AAD0`): they bound what a mutant that falls out of its
  arm can run, so no mutant runs a real callee's tree.

**Equivalent mutants, not bound.** In both functions type 1's step read as the signed +0x34 word is the
raw's own `mov eax,[..+0x32]; sar eax,0x10` (the same bytes); the mutant bound instead is the
zero-extended word (`@stepz`). In `0x4BF18` the second `mov byte [ecx+0x1f],0` (`0x4C349`) repeats
`0x4C27F` unless a callee rewrites the byte in between: `@sno1f` is bound in `fight_4bf18_rr`, whose
stream stub does.

## §5 The store sweep

Every field a function writes is seeded with a value different from what it writes, between seeded
neighbours; one dropped store per field is a bound mutant.

- `0x4BF18`: the two records' +0x36 (`@pno36`, `@pnob`, `@pwb` for the width); type 1's +0x38/+0x34/+0x36,
  +0x55 and the type byte (`@no38`, `@no34`, `@no36`, `@no55`, `@no1e`, `@c1order` by the memory at the
  call); the timer (`@t2dec`); the clamp's +0x2C (`@noclamp`, `@t5clamp`), +0x38 (`@t3s38`, `@t3v38`,
  `@t5s38`), +0x34 (`@t5s34`), the type and +0x1C bit 7 (`@t3type`, `@t3f7f`, `@t5type`); type 6's +0x1C
  bit 7, the held actor's +0x18/+0x32 (`@t6copyw`, `@t6copyd` for the widths), the entry's +0x10, +0x1E,
  +0x1F and the record's +0x1C/+0x34/+0x36; the tail's twins (`@tl..`); the score bytes, `DS_00108898`,
  `DS_001088AC`, the type (`@sinc`, `@s898`, `@sac`, `@st8`); the bounce (`@bsub`, `@bnoneg`);
  `DS_001088A0` (`@a0dec`), `DS_00108874` (`@nomid`, `@midw`), `DS_001088AA` (`@aadec`, `@aaorder`),
  `DS_001088AC` and `DS_00108898` (`@acdec`, `@acz`).
- `0x49C78`: `DS_00108874` (`@nomid`, `@midw`); type 3's +0x2C, +0x28 bit, +0x38/+0x34, the timer, type
  and bit 7; type 4's timer, +0x38, +0x34, type and bit 7; types 5 and 6 as above; type 7's +0x1C bit 2
  (`@t7nobit`, `@t7late`), +0x34 and +0x28 bit 7 (`@t7no28`); type 8's table byte, +0x2A, +0x29, +0x4A,
  +0x1C bit 6 and the two per-side bytes (`@t8no4b`, `@t8no2a`, `@t8no29`, `@t8no4a`, `@t8nobf`,
  `@t8noae`, `@t8nob2`; `@t8late` by the memory at the call); the type bytes of 9, 10, 12, 13
  (`@t9type`, `@t10type`, `@t10early`, `@t12early`, `@t13type`, `@t13late`); type 11's +0x2C and
  +0x38/+0x34/+0x36; types 13/14's +0x29, +0x34, the entry's +0x14 (`@t13no14`, `@t14no14`),
  `DS_00108878` (`@t13no878`), +0x2C and +0x55; the mode-9 block's `DS_001088C3`, `DS_001088B0`,
  `DS_001088C9`, `DS_001088B4` and the type-12 pass; `DS_001088C2`, `DS_001088BF`, `DS_00104AEC`
  (`@noc2`, `@c2early`, `@nobf0`, `@bfearly`, `@noaec`).

Finding: none beyond §3 (no missing or extra store in the port on the exercised paths).

## §6 Queue entries not done

None: both entries are rowed.

## §7 Measurements that are not rows

- The existing rows on, or calling, a function this batch seamed were re-run with `--function ..
  --self-check`, each VERIFIED with every mutant detected: `fight_dust_clamp`, `fight_4ac38`,
  `fight_midpoint`, `fight_496dc`, `fight_4b788`, `fight_challenge_crowd`, `fight_4cd98`, `fight_4cf20`,
  `fight_4d898`, `fight_4dbec`, `fight_dust_build`, `fight_4987c`, `fight_4c784`, `fight_4a7d4`,
  `fight_4af04`, `fight_4a634`, `fight_4b2ac`, `fight_4a928`, `fight_4aad0`, `fight_4aad0_rr`,
  `fight_4b470`.
- Before any correction, against the port as it stood: `fight_4bf18` MISMATCH on `g2` alone (the AL
  test); `fight_4bf18_post` on `m4` (the remainder) and `n4` (the AL test); `fight_4bf18_rr` on every
  relinking case; `fight_effects_pass` on `h2` alone (the AL test); `fight_effects_pass_m9` and
  `fight_effects_pass_pre` agreed; `fight_effects_pass_rr` differed on its cases (107 problems).
- `/tmp/pr_d5j/stubcheck.py`: the 30 stub addresses of `D5J_SPECS` each declare `E.callee_clobbers`, with
  no disagreeing declaration elsewhere in `SPECS`; 15 are new stub addresses (`0x493F0`, `0x496AC`,
  `0x496DC`, `0x4987C`, `0x4A634`, `0x4A7D4`, `0x4A928`, `0x4AAD0`, `0x4AC38`, `0x4AF04`, `0x4B2AC`,
  `0x4B470`, `0x4B69C`, `0x4C60C`, `0x4CD98`), added to the expected dictionary of
  `test_each_stub_declares_the_registers_its_callee_clobbers`. A script evaluated that dictionary against
  the stubs of `SPECS` (equal, 366 addresses) and the pins block against the spec list (7 masks, 411
  kinds, 411 case sets); the harness suite itself was not run (the integrator's gate).
- The one full self-check of the batch, on the tree of its last row commit (`PR_DIFF_JOBS=4 python3
  tools/diff_verify.py --image /tmp/pr_d5j_img.bin --self-check`): `rc 0`, `diff-verify: 791/791 functions
  VERIFIED; 9631/9631 mutants detected; 1 named gaps; 499/624 rows with callees closed (167 have none)`.
  The base (`reverse-d4int` with wave 4 merged: 784 specs, 9220 mutants) plus this batch's 7 specs and
  411 mutants; the integrator re-pins the counter on the merged tree.
