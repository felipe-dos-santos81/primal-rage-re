# Reverse completion P3: the move callbacks 0x475EC..0x489A0 and the callbacks they store (record)

**Scope.** Track P's third batch (roadmap row P3 of record `2026-10-02-reverse-p1-derivations.md` §P1.3, as P2's §P2.11
left it: "move callbacks `0x475EC..0x489A0` and the callbacks they store", 24 functions, `0x48170` before `0x480B4`,
the after-table `0x47E9C` and `0x4844C`), under spec `2026-09-30-reverse-completion-design.md` §4 track P and §6.
Plan: `2026-10-03-reverse-p3-move-callbacks-b.md`. Recipe: E3 record §E3.10; lessons: P1 §P1.10-§P1.12 and P2's
reviews (ledger `.superpowers/sdd/2026-10-02-reverse-p2-move-callbacks/progress.md`: stores no case can observe; rows
that cannot tell rec+0x51 from side, the own from the other slot, a zero- from a sign-extension; unit assertions that
cannot fail; image-only checks presented as port tests; `fn_register` overflow; a stack buffer compared by value).

**Status of the numbers.** Measured by the planner on 2026-10-02/03 on `reverse-p2`'s tip **`8d4cdf4`** (P2 rebased
onto `main` `b16922d`: P2's 24 functions and its closure, including the gp-u10-ending re-pin of §P3.9), in scratch
trees: a prototype (each task developed in order, committed in a detached scratch worktree, the full `make verify`
with the parallel-safe overrides on its final state) and a replay (a fresh `8d4cdf4` worktree, Tasks 2-7 applied in
order by one driver with exactly the plan's scripts, every output kept; the plan quotes those). The replay's final tree is byte-identical to the prototype's (`git diff` empty), on which the final gate ran. Task 2's U10 script (one comment reworded after the replay) and Task 8's docs script were re-run on copies of the `8d4cdf4` files: the same result. The prototype was
started on `20e8f4a` (P2 before its closure) and rebased onto `8d4cdf4` when P2 closed; the code anchors are the
same (P2's closure touched only docs, the Makefile, `test_platform.c` and one comment in `fighter.c`). **Re-baseline
note:** P2 merges into `main` before P3 executes; if that merge (or anything merged after it) moves a counter or a
pin, re-measure it at Task 1 and add this plan's increments to the measured base. **The image** is `build/diffrun
--exe data/game/C/PRAGE.EXE --image-out FILE`: sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's, E3's, P1's
and P2's). Every address and instruction below is capstone 5.0.7 over that image (fixups applied); `unicorn` 2.1.4
runs the original side. Ghidra was not consulted.

---

## §P3.1 The member list from the raw: the roadmap's 24

`diff_emu.static_scan(..., switches=True, resolved=E.RESOLVED_JUMPS)` from each roadmap member (the tail of
`0x2C3FC` not followed), every immediate in the code range each one holds (all are slot-field stores), every dword
of the image equal to each address, and a rel32 scan of every `call`/`jmp` below `0x5D000`:

| member | insns / blocks | direct callees | stores (code immediates) | reached by |
|---|---|---|---|---|
| `0x475EC` | 9 / 3 | `1A570` | - | move-table dword `0xA4004` (character 2, reaction 0x0B) |
| `0x47608` | 9 / 3 | `1A570` | - | `0xA40CC` (2, 0x15) |
| `0x47624` | 12 / 1 | `2BC30` | - | `0xA42AC` (2, 0x2D) |
| `0x48964` | 23 / 6 | `1A570 35838` | - | `0xA41F8` (2, 0x24) |
| `0x489A0` | 23 / 6 | `1A570 35838` | - | `0xA420C` (2, 0x25) |
| `0x47720` | 20 / 1 | `339AC 3C4CC` | +0x0C `0x476FC` (`0x47751`), +0x18 `0x47648` (`0x4775C`), +0x1C `0x47688` (`0x47767`) | `0xA41A8` (2, 0x20) |
| `0x476FC` | 8 / 3 | - | +0x18 `0x47648` (`0x4770A`), +0x1C `0x47688` (`0x47711`) | slot +0x0C (`0x47720`) |
| `0x47648` | 24 / 1 | `33950 18BD4 18C14` | - | slot +0x18 (`0x47720`, `0x476FC`) |
| `0x47688` | 32 / 6 | `33950 3B298 39834 39FB0 3A95C 39A10` | - | slot +0x1C (`0x47720`, `0x476FC`) |
| `0x47874` | 25 / 1 | `3C4CC 3C190 2C3FC` | +0x0C `0x47830` (`0x47897`), +0x18 `0x477A8` (`0x4789E`), +0x1C `0x477E8` (`0x478A5`), **+0x14** `0x47798` (`0x478AC`) | `0xA41BC` (2, 0x21) |
| `0x47830` | 18 / 3 | `2BC30` | - | slot +0x0C (`0x47874`) |
| `0x47798` | 4 / 1 | `2C3FC` | - | slot +0x14 (`0x47874`) |
| `0x477A8` | 24 / 1 | `33950 18BD4 18C14` | - | slot +0x18 (`0x47874`) |
| `0x477E8` | 20 / 1 | `33950 3B714 2BC30` | - | slot +0x1C (`0x47874`) |
| `0x47FCC` | 33 / 1 | `33950 3C4CC` | +0x0C `0x47E9C` (`0x48021`), +0x18 `0x47CB0` (`0x4802C`), +0x1C `0x47D24` (`0x48037`) | `0xA41E4` (2, 0x23) |
| `0x47CB0` | 37 / 5 | `33950 18BD4 18C14` | - | slot +0x18 (`0x47FCC`) |
| `0x47D24` | 53 / 1 | `33950 34D8C 2BC30 3C480 3C208 18AF8 39834 3C358 39A10 2C3FC` | - | slot +0x1C (`0x47FCC`) |
| `0x47E9C` | 82 / 17 | `33950 2BC30` | - | slot +0x0C (`0x47FCC`) |
| `0x48608` | 27 / 1 | `3C4CC 3C190` | +0x0C `0x4844C` (`0x48646`), +0x18 `0x48054` (`0x48655`), +0x1C `0x480B4` (`0x4865E`) | `0xA4234` (2, 0x27) |
| `0x48054` | 31 / 3 | `33950 18BD4 18C14` | - | slot +0x18 (`0x48608`) |
| `0x480B4` | 28 / 1 | `33950 3B298 39A10 48170 3C208` | - | slot +0x1C (`0x48608`) |
| `0x48170` | 70 / 3 | `33950 3C148 3C480 468D8 36D98 188DC` | **+0x10** `0x4811C` (`0x48231`), in the other slot | `call` at `0x480F6` (its only reference) |
| `0x4811C` | 31 / 8 | `36870` | - | slot +0x10 (`0x48170`) |
| `0x4844C` | 118 / 23 | `33950 36870 2C3FC 3C148 3C16C 188AC 2BC30 188DC` | - | slot +0x0C (`0x48608`) |

**No correction to the membership.** Every code immediate a member stores is a member, every member is reached by
exactly the references listed (no other dword, no other rel32), and nothing else is reached: the 24 are closed
under "the callbacks they store". Every member is character 2's (the move-table dwords `0xA3528 + (2 * 64 + r) * 20`,
r = 0x0B, 0x15, 0x20, 0x21, 0x23, 0x24, 0x25, 0x27, 0x2D). Three facts the roadmap's one line does not carry,
recorded here:

1. **Two slot fields beyond P2's +0x0C/+0x18/+0x1C.** `0x47874` stores a **+0x14 callback** (`0x47798`), which
   `0x1952F` (`0x19537`), `0x350D0` (`0x3514C`) and `0x35050` (`0x350B8`) call with EAX = EDX = the slot and test
   whole (`test eax,eax`; non-zero zeroes the field), the port's `fighter_slot14_cb`. `0x48170` stores a **+0x10
   handler** (`0x4811C`) in the *other* slot, which `0x3531C` case 10 calls (§P3.2).
2. **The two after-table members need no hand resolution.** `0x47E9C` starts right after its 4-dword table
   `0x47E8C` (`cmp al,3; ja; and eax,0xff; jmp cs:[eax*4+0x47E8C]`, 0x47EC9..0x47ED9) and `0x4844C` after its
   5-dword table `0x48438` (`cmp dl,4; ja; and edx,0xff; jmp cs:[edx*4+0x48438]`, 0x48480..0x48492): both are the
   bounded form `switch_cases` follows (the scans list no indirect jump; 17/17 and 23/23 blocks are reached), unlike
   P2's `0x22638` (pre-scaled base, `RESOLVED_JUMPS[0x227BC]`). Neither address is in E2's universe (as `0x22638`):
   porting them moves no E2 count.
3. **`0x47648` and `0x477A8` have identical bodies** (24 instructions; only their rel32 displacements differ). One C
   function per original function: two functions, two rows.

**The stream targets of P3's streams stay open** (a scratch probe on `8d4cdf4`: each stream a P3 member starts
itself, begun on the fixture record `Z_R0` with `actors_anim_begin(rec, stream, 1.0)` and walked 400 frames by
`actor_sync` with the miss log armed; `0x5D812` from `actor_spawn` is the base pair every driver records):

| stream (started by) | misses |
|---|---|
| `0xED79A` (`0x47624`) | none beyond the base pair |
| `0xECE1C` (`0x47720`), `0xECBDA` (`0xC8950[2]`, `0x47FCC`), `0xEDA40` (`0x47E9C`), `0xED834` (`0x48608`), `0xECD42` (`0xC8B58[2]`, `0x4844C`) | none |
| `0xED974` (`0x47874`), `0xED9A4` (`0x477E8`, `0x47830`), `0xED9D0` (`0x47D24`) | `0x47E04` (2 hits), `0x47E30` (1) |
| `0xED850` (`0x48170`) | `0x48254`, `0x482E4`, `0x48374` (1 each) |

`0x47E04 0x47E30 0x482E4 0x48374` are P6 rows and `0x48254` a P7 row of §P1.3. No gp scenario reaches one (§P3.9),
so, unlike P2's `0x14FA8 0x14FF8 0x150AC`, none blocks a capture: they stay with their batches (a named gap,
§P3.10). The streams P3 members start for the *other* record (`0xC90F8[c]`, `0x47D24` and `0x48170`) are the other
character's, shared with P2's `0x211F0`/`0x22404`, and are not P3's.

## §P3.2 Callers, registers, masks and the callee declarations

- **Move callbacks** (`0x475EC 0x47608 0x47624 0x48964 0x489A0 0x47720 0x47874 0x47FCC 0x48608`): `0x34E2C` at
  `0x35045`, EAX = slot, EDX = rec, EBX = side; mask 0 (record P2 §P2.2).
- **Slot +0x0C callbacks** (`0x476FC 0x47830 0x47E9C 0x4844C`): `0x3531C` case 7 (`0x35431`), the same registers,
  then `xor eax,eax` (`0x35434`): mask 0.
- **Slot +0x18 hooks** (`0x47648 0x477A8 0x47CB0 0x48054`): `0x19020` at `0x1903F`, EAX = side, `test eax,eax`
  (`0x19048`): mask 0xFFFFFFFF.
- **Slot +0x1C callbacks** (`0x47688 0x477E8 0x47D24 0x480B4`): `0x193B0` at `0x19505`, EAX = side; `mov eax,[esp+8]`
  (`0x19508`): mask 0.
- **Slot +0x14 callback** (`0x47798`): `0x1952F`/`0x35144`/`0x350B0`: `mov edx,eax; call [edx+0x14]; test eax,eax`
  (`0x1953A`, `0x3514F`, `0x350BB`): EAX = EDX = the slot, mask 0xFFFFFFFF. `0x350BF` reads EDX after the call
  (`mov eax,edx`): `0x47798` leaves it (`0x2C3FC` preserves every register).
- **Slot +0x10 handler** (`0x4811C`): `0x3531C` case 10 at `0x354DA..0x354E2`: `cmp dword [ecx+0x10],0; je; mov
  eax,ecx; call [ecx+0x10]`: EAX = the slot, EDX = the slot's record (`mov edx,[ecx]` at `0x35396`, nothing writes
  EDX before the case switch), EBX = side; then `add esp,0x18` and the pops (`0x354E5`): mask 0. The port's
  `fighter_state_3531c` passes (slot, side) to a case-10 handler, so `0x4811C` registers an adapter that supplies
  `DSD(slot)`, as `fighter_21458_case10` does (record §49-V).
- **`0x48170`**: its only reference is `call 0x48170` at `0x480F6` (`0x480B4`), EAX = side; `0x480FB` reloads EAX
  (`mov eax,[esp+0xc]`): mask 0. It pushes EBX ECX EDX ESI EDI and pops them before its one `ret`:
  `E.callee_clobbers` is empty.

**Callee declarations** (args in the port's C order; clobbers = `E.callee_clobbers(image, addr)`; every one a plain
`ret`; new ones get their seam as their first statement):

| `E.Call` | callee (port C) | args | clobbers | seam |
|---|---|---|---|---|
| `BIT15` (P1) | `0x1A570` `fighter_actor_bit15_clear(side)` | `eax` | none | P1 |
| `ANIM_BEGIN`, `HIT_B`, `HIT_A`, `VOICE` (E3) | `0x2BC30`, `0x3C4CC`, `0x3C480`, `0x2C3FC` | as E3 | as E3 | E3 |
| `ANCHOR` (P1) | `0x188AC` `hit_anchor_set(side, x, y)` | `eax edx ebx` | `edx` | P1 |
| `FLASH CHECKS FACING POSE TIMER PLACE HOLD ANIM54` (P2) | `0x34D8C 0x18C14 0x18AF8 0x39834 0x39A10 0x3C208 0x3C358 0x36870` | as P2 | as P2 | P2 |
| `DIRS` (new) | `0x35838` `fighter_state_35838(slot, rec, dirbits)` | `eax edx ebx` (`mov edx,ebx` at `0x3583D`) | `ebx edx` | Task 2 |
| `DISPATCH` (new) | `0x3B298` `fighter_command_dispatch(side, edx_arg)` | `eax edx` (`mov ecx,edx; ...; mov edx,eax`), returns AL | `edx edi ebp` | Task 3 |
| `PIVOT` (new) | `0x39FB0` `fighter_39fb0(slot)` | `eax` | none | Task 3 |
| `STANCE` (new) | `0x3A95C` `fighter_3a95c(side, b)` | `eax edx` | `edx` | Task 3 |
| `SPEED` (new) | `0x3C190` `fighter_3c190(side, v)` | `eax edx` | `edx` | Task 4 |
| `REACT` (new) | `0x3B714` `fighter_reaction(param_1, param_2)` | `eax edx` (`mov esi,eax; mov ebp,edx`) | `edx` | Task 4 |
| `ARM170` (new) | `0x48170` `fighter_48170(side)` | `eax` | none | Task 6 (a member with its own row) |
| `CLEAR34` (new) | `0x3C148` `fighter_3c148(side)` | `eax` | none | Task 6 |
| `PRED` (new) | `0x468D8` `ai_pred_468d8(side)` | `eax`, returns AL | none | Task 6 |
| `RESET` (new) | `0x36D98` `fighter_36d98(slot)` | `eax` | none | Task 6 |
| `ANCHORX` (new) | `0x188DC` `hit_anchor_x(side, x)` | `eax edx` | `edx` | Task 6 |
| `CLEAR36` (new) | `0x3C16C` `fighter_3c16c(side)` | `eax` | none | Task 7 |

`0x33950` (`fighter_ctx_same`), `0x339AC` (`hit_anim_ctx`) and `0x18BD4` run on both sides (allow): their EAX is a
stack buffer in every P3 caller (§E3.10 item 6), and E3/P2 verified the first two. `0x33950` clobbers EDX only and
`0x18BD4` nothing, which `0x47648`/`0x477A8` (`xor ecx,ecx` before `0x33950`, `xor ebx,ebx` before `0x18BD4`) and
`0x48054` (`mov ecx,0xC949C` / `mov ebx,0xC9492` before them) rely on for `0x18C14`'s EBX/ECX: the rows verify with
those values compared at the call. `0x18C14`'s flags are compared by value (`CHECKS`, record P2 §P2.7). The stubbed
predicates whose AL a P3 function tests (`0x1A570`, `0x3B298`, `0x468D8`) return 0 or 1 in the C port; their stub
EAX varies per case (§E3.12 I4); `0x47688`'s h3 also returns `0x100` from `0x3B298` (AL 0, the high bits a scratch
value the raw ignores: `test al,al` at `0x476AB`), which the port's `(u8)` cast honours. Four callees lose `static`
so the harness's mutants can call them (`0x35838`, `0x3B298`, `0x3C190`, `0x36D98`).

## §P3.3 Task 2: the move callbacks with no context

- **`0x475EC`** (r 0x0B): `mov eax,ebx` (EBX = side is `0x1A570`'s argument, not rec+0x51); the EDX record's
  +0x43 = 0x28 (`0x475EE`) and word +0x34 = 0x258 (`0x475F2`), both before the call; `test al,al`; on AL the word is
  negated (`neg word [edx+0x34]`, `0x47601`; EDX survives `0x1A570`). AL = 1. **`0x47608`** (r 0x15): the same with
  0x22 and 0x2EE.
- **`0x47624`** (r 0x2D): `0x2BC30(rec, 0xED79A, 4.0)`, then the slot +0x52 = 9, +0x53 = 8, +0x54 = 0 (after `mov
  al,1`).
- **`0x48964`** (r 0x24): `test byte [eax+0x41],0x40; jne` refuses with AL = 0; else +0x41 |= 0x40 (`mov
  bl,[ecx+0x41]; or bl,0x40; mov [ecx+0x41],bl`: the entry EBX is overwritten, not read); `0x1A570(rec+0x51)`
  (`xor eax,eax; mov al,[edx+0x51]`); EBX = 0x2000 on AL, else 0x1000; `0x35838(slot, rec, EBX)`. **`0x489A0`**
  (r 0x25): the two values swapped.

Cases: `s0` (side 0, AL 0), `s1` (side 1, AL 1), `s2` (side 0, AL 1), every one with rec+0x51 = 1 so a port that
indexes `0x1A570` by the record differs on `s0`/`s2` (`fighter_475ec@side`); sentinels on +0x43 and +0x34. `n0`/`n1`
(`0x47624`, the slot's +0x52..+0x54 seeded). `q0` (+0x41 = 0x40 alone: refused), `q1` (0xBF, every other bit,
rec+0x51 = 0, AL 0), `q2` (0xBF, rec+0x51 = 1 with side 0, AL 1): `fighter_48964@side` (`0x1A570` on EBX) is caught by
`q2` alone. Mutants that only a call catches: `@mutant` of `0x475EC`/`0x47608` stores after the call (`call #0
memory`), `0x47624@mutant` the slot stores first (`call #0 memory`), `0x48964@mutant` the directions swapped (`call
#1`), `0x489A0@mutant` +0x41 set after the call (`call #0 memory`).

## §P3.4 Task 3: `0x47720` and the three callbacks it stores

**`0x47720`** (r 0x20): `mov eax,esp; call 0x339AC` (the context from the EDX record: ctx[0] = rec+0x51; the EAX
slot and EBX are not read); `0x3C4CC(ctx[4], 0xECE1C, 2.0)` (`[esp+0x10]` before the push); ctx[2] +0x52 = 9, +0x53 =
7, +0x0C = `0x476FC`, +0x18 = `0x47648`, +0x1C = `0x47688`. Cases `e0`/`e1`: EDX = `E3_OUT` with +0x51 = the side,
EBX = the other side; the started record is ctx[4] (`SLOT_PTRS`): `@mutant` (EDX's record) `call #0`, `@ebx` (the
context by EBX) `byte` and `call #0`.

**`0x476FC`** (+0x0C): `mov dl,[edx+0x63]; and edx,0xff; cmp edx,5; jl` (the record's byte, zero-extended); at least
5: the EAX slot's +0x18 = `0x47648`, +0x1C = `0x47688`, +0x0C = 0. Cases `c0` (4), `c1` (5), `c2` (0x80, which a
signed byte refuses: `@sext`, `c2` alone); the slot's own +0x63 is seeded 0.

**`0x47648`** (+0x18): ctx(side); flags from `0x18BD4`; flags 1 and 8 = 0 (`xor dl,dl`), 0 = 1 (`mov ah,1`, stored
at `0x47674`); `0x18C14(ctx[0], flags, EBX = 0, ECX = 0)`; returns its EAX. Cases `k0` (side 0, stub 0), `k1` (side
1, `0x12345678`); `@mutant` (flag 0 left at 2) `call #0`.

**`0x47688`** (+0x1C): ctx(side); `0x3B298(ctx[1], ctx[2].+0x5F)` (zero-extended byte); on AL nothing else; else
`0x39834(ctx[1], ctx[2].+0x5F)`; `0x39FB0(ctx[3])` when ctx[3]'s +0x54 is 2 (`mov eax,[esp+0xc]` survives the
compare into the call), else `0x3A95C(ctx[1], 0xF)`; `0x39A10(ctx[5], the signed word 0xBEDD8)` (`mov
edx,[0xbedd6]; sar edx,0x10`; 10 in the image). Cases `h0` (AL 1), `h1` (the other +0x54 = 2), `h2` (side 1), `h3`
(side 1, +0x54 = 2, stub EAX `0x100`), `h4` (the word poked `0xFFF0`); the slots' +0x52..+0x5F carry different
sentinels. `@mutant` (`0x3A95C` with 0xE) `call #2`; `@pivot` (`0x39FB0` on the own slot) `call #2`, `h1`/`h3`
alone; `@zext` `call #3`, `h4` alone.

**The unit run of `0x47688`** uses the real callees on the fixture: `0x3B298(1, slot 0's +0x5F)` returns 0 there
(measured), slot 1's +0x54 = 0x66 takes `0x3A95C` (slot 1 0x10/0xA/0, +0x10 = 0) and +0x54 = 2 takes `0x39FB0` (+0x10
= `0x39CC8`, +0x54 kept); both end with slot 1's +0x74 = 10. Its mutation (`ctx[2]` for `ctx[3]` in the +0x54 test)
fails two checks.

## §P3.5 Task 4: `0x47874` and the four callbacks it stores

**`0x47874`** (r 0x21; ECX = the slot, ESI = the record): `0x3C4CC(rec, 0xED974, 2.0)`; the slot +0x53 = 7, +0x54 = 0,
+0x52 = 9, +0x0C = `0x47830`, +0x18 = `0x477A8`, +0x1C = `0x477E8`, +0x14 = `0x47798`; `0x3C190(rec+0x51, 0x80)`
(`xor eax,eax; mov al,[esi+0x51]`); the voice 0x4B; AL = 1. Cases `v0`/`v1` (rec+0x51 0 and 1, EBX the other;
`v1`'s voice stub AL 0); `@mutant` (the four callbacks after `0x3C190`) `call #1 memory`, `@side` (`0x3C190` on EBX)
`call #1`.

**`0x47830`** (+0x0C; ECX = the slot, EAX = the record): `xor edx,edx; mov dl,[eax+0x51]; mov dx,[edx*2+0x1088E0];
xor dl,dl; and dh,9; and edx,0xffff; cmp edx,0x900; je` (both command bits 0x100 and 0x800: nothing); else
`0x2BC30(rec, 0xED9A4, 2.0)`, the slot's +0x0C = +0x14 = 0. Cases `z0` (0x0900), `z1` (0x0100), `z2` (0x0800, side 1),
`z3` (0xF6FF: every bit but the two), `z4` (0xFFFF), each with EBX = 1 - rec+0x51 and the other word taking the other
branch: `@side` is caught by all five; `@mutant` (either bit skips) by `z1`/`z2`; `@order` `call #0 memory`.

**`0x47798`** (+0x14): the voice 0x4C; `mov eax,1`. Cases `w0`, `w1` (voice stub 0): `@mutant` (0x4B) `call #0`,
`@eax` (the voice's EAX returned) `w1` alone.

**`0x477A8`** (+0x18): `0x47648`'s bytes. `@mutant` (flag 8 left at 2) `call #0`.

**`0x477E8`** (+0x1C): ctx; `0x3B714(ctx[3], ctx[2])` (`mov edx,[esp+8]; mov eax,[esp+0xc]`); `0x2BC30(ctx[4],
0xED9A4, 2.0)` with EBX = ctx[2] loaded before the call; ctx[2]'s +0x0C = +0x14 = 0. Cases `y0`/`y1`, both slots'
+0x0C..+0x1F seeded differently; `@mutant` (the slots swapped) `call #0`, `@order` `call #1 memory`.

## §P3.6 Task 5: `0x47FCC` and the three callbacks it stores (x87)

**`0x47FCC`** (r 0x23): `mov edx,ebx` (EBX alone), ctx; the side's dword `0x108370` = 0 (`0x47FDD`, before the call);
`0x3C4CC(ctx[4], [0xC8950 + 4 * ctx[2].+0x7A], 2.0)`; ctx[2] +0x53 = 7, +0x52 = 9, +0x54 = 0, +0x0C = `0x47E9C`, +0x18 =
`0x47CB0`, +0x1C = `0x47D24`, +0x57 = 0, +0x41 |= 0x80 (`0x21374`'s shape plus the dword). Cases P2's
`p2_ctx_case` with both sides' dwords seeded; `@mutant` (the other slot's character) `call #0`, `@order` (the dword
after the call) `call #0 memory`.

**`0x47CB0`** (+0x18): flags 1, 8, 4, 0xD, 0xE, 7 = 0, 5 = 1; w = `[ctx2+0x86] sar 16` (the signed word +0x88): above 3
(`jg`) or below 1 (`jge` to the call) returns 1; else `0x18C14(ctx[0], flags, EBX = 0xC946A, ECX = 0xC9474)`. P2's
`p2_2116c` cases with these bounds: `k0` (4), `k1` (3: the call, stub 0), `k2` (side 1, 1: `0x12345678`), `k3` (0),
`k4` (-1). `@mutant` (flag 0xD) `call #0`, `@ge` (`k1` alone), `@lo` (`k2` alone). A zero-extended word is not
observable here and not claimed: a negative word is below 1 signed and above 3 unsigned, both return 1.

**`0x47D24`** (+0x1C): ctx; `0x34D8C(ctx[0])`; ctx[2] +0x42 |= 4; `0x2BC30(ctx[4], 0xED9D0, the side's float
0x108378)` (`push dword [eax*4+0x108378]`); `0x3C480(ctx[5], [0xC90F8 + 4 * ctx[3].+0x7A], 3.0)`; `0x3C208(ctx[0], the
signed word 0xC947E[ctx[3].+0x7A])` (`mov edx,[eax*2+0xc947c]; sar edx,0x10`; image words 0x17C0 0x1540 0x1180 0x1080
0x1680 0x1680 0x1400); `0x18AF8()`; `0x39834(ctx[1], ctx[2].+0x5F)`; `0x3C358(ctx[0])`; ctx[2] +0x57 = 3, the float =
3.0, the byte `0x108394[side]` = 0 (`xor dh,dh; mov [eax+0x108394],dh`); `0x39A10(ctx[4], 0x309)`, `0x39A10(ctx[5],
0x309)`; the voice 0x66. Cases `b0`, `b1`, `b2` (character 3's word poked `0xF000`); `@mutant` (the own character's
stream) `call #2`, `@order` (+0x57, the float and the byte before `0x3C358`) `call #6 memory`, `@signed` `call #3`
(`b2` alone), `@frame` (3.0 for the float) `call #1`.

**`0x47E9C`** (+0x0C; ECX = the EAX slot): ctx; the side's dword `0x108370` + 1, above 0x3C (signed `jle`) setting
the byte `0x108394[side]` = 1; the state is **the EAX slot's +0x57** (`mov al,[ecx+0x57]`, 0x47EC9), the stores go to
ctx[2]; table `0x47E8C` = `0x47EE1 0x47F04 0x47FC4 0x47F2F`: 0: `[ctx2+0x86] sar 16` above 3 sets +0x57 = 1; 1:
`0x2BC30(ctx[4], 0xEDA40, 2.0)`, +0x57 = 2, +0x8A = 0; 2: nothing; 3: with the side's command word (`add ebx,ebx`
on the entry EBX) bit 0: `fld f; fadd qword [0x80C6C]` (-0.1); else bit 1: `fld f; fld st0; fadd qword [0x80C64]`
(0.1); `fstp st1`; either way `fstp dword f`; then the stored float `fcomp qword [0x80C74]` (1.1): below (`jae` not
taken) it becomes `0x3F8CCCCD` (1.1f) and returns; else `fcomp dword [0x80C7C]` (5.0): above (`jbe` not taken) it
becomes `0x40A00000`. Above 3 (`ja`) nothing. Cases `e0`..`eF` (16): the count 0x3B/0x3C/0x7FFFFFFF, each state, the
word +0x88 3/4/-1, the two steps (bit 0 first: `e7` cmd 3), `e8` (0xFFFC: neither bit), both clamps (`e9` 1.15 - 0.1,
`eA` 4.95 + 0.1, `eB` 1.0, `eC` 6.0), `eF` (cmd 0x0101 on 3.0). EAX is `E3_SLOT` (state `st`), ctx[2]'s +0x57 is 0x57:
`@slot` (the state from ctx[2]) `byte` and `call #0`; `@mutant` (+0.1 on bit 0) `byte`; `@signed` (`eD` alone);
`@order` (state 1's +0x57 before the call) `call #0 memory`.

**Named limit (x87), checked exhaustively.** The raw adds in x87 registers and rounds once into the float (`fstp
dword`); the port adds in double and rounds to the float, a double rounding. Every comparison reads the stored float
back (`fld dword` at `0x47F89`/`0x47FA9`), so only the store can differ. For f a float and d the double ±0.1, the exact
f + d has at most 59 significant bits (f's last bit 2^-23..2^-21, d's 2^-56), so the x87's 64-bit addition is exact
and its `fstp` rounds once. The double rounding differs from the single one only when the double sum lands exactly on
a midpoint between two adjacent floats (rounding is monotonic and every such midpoint is a double). A scratch numpy
check over **every float of [1.0, 6.0]** (20 971 521 values) for each addend (`0x80C6C` and `0x80C64` read from the
image) finds **0 double sums on a float midpoint**: for those floats the two stored values are equal. Not claimed
outside [1.0, 6.0]: the code stores 3.0 (`0x47D24`) and clamps to [1.1f, 5.0f], and the cases use 1.0, 1.15, 2.0,
3.0, 4.95 and 6.0.

## §P3.7 Task 6: `0x48608`, its +0x18/+0x1C callbacks, `0x48170` and the +0x10 handler `0x4811C`

**`0x48608`** (r 0x27; ECX = the slot, ESI = the record, EBX = rec+0x51 via `xor ebx,ebx; mov bl,[edx+0x51]`):
`0x3C4CC(rec, 0xED834, 2.0)`; `0x3C190(EBX, 0x78)`; the record's +0x42 = 0x1E; the slot +0x52 = 9, +0x53 = 7, +0x54 =
0, +0x57 = 0, +0x0C = `0x4844C`, the word `0x10838C[EBX]` = 0, +0x18 = `0x48054`, +0x1C = `0x480B4` (EBX survives both
calls). Cases `r0`/`r1` (rec+0x51 0/1, EBX the other); `@mutant` (0x80) `call #1`, `@order` (the stores before
`0x3C190`) `call #1 memory`, `@side` (the word by EBX) `byte`.

**`0x48054`** (+0x18; ECX = `0xC949C` and EBX = `0xC9492` before the allowed calls): flags 1, 8, 4, 0xD = 0, 5 and 9 =
1; r = `0x18C14(ctx[0], flags, 0xC9492, 0xC949C)`; with ctx[2]'s +0x57 non-zero, 1. Cases `n0`, `n1` (side 1,
`0x12345678`), `n2` (+0x57 = 3); `@mutant` (flag 9) `call #0`, `@eax` (`n2` alone).

**`0x480B4`** (+0x1C): ctx; `0x3B298(ctx[1], ctx[2].+0x5F)` (AL unread: the next instruction loads EDX); `0x39A10(ctx[4],
0x309)`; `0x39A10(ctx[5], 0x309)`; `0x48170(ctx[0])`; `0x3C208(ctx[1], the signed word 0xC94A6[ctx[3].+0x7A])` (image
words 0xFC0 0xD40 0x1180 0x1080 0x1680 0xE80 0xE80). Cases `x0`, `x1`, `x2` (character 3's word `0xF000`); `@mutant`
(`0x3C208` on ctx[0]), `@signed` (`x2` alone), `@char` (the own character), each `call #4`.

**`0x48170`** (EAX = side; ECX = side, ESI = 1 - side; EDI = the own slot, EBX = the other, computed as `0x1077B0 +
0x94 * n`): ctx; the word `0x108388[side]` = 0; `0x3C148(side)`, `0x3C148(1 - side)`; `0x3C480([EDI], 0xED850,
3.0)`; [EDI]+0x57 = 2; `0x468D8(1 - side)` and on its AL `0x36D98(EBX)`; ESI = ctx[3]'s +0x2C (`mov esi,[esp+0x10]`
after the push = ctx[3], then `mov esi,[esi+0x2c]`, **before** the next call); `0x3C480([EBX], [0xC90F8 + 4 * EBX's
+0x7A], 3.0)`; `0x188DC(ctx[1], ESI)`; EBX's +0x52 = 0x10, +0x53 = 0xA, +0x54 = 0, +0x10 = `0x4811C`, +0x58 = 0; the
byte `0x108392[side]` = `setne` of EBX's +0x43 & 0x30. Cases `g0` (AL 0, +0x43 0x10), `g1` (side 1, AL 1, 0x20),
`g2` (AL 0, 0xCF); `@mutant` (the own slot's +0x2C) `call #5`/`#6` (the index depends on whether `0x36D98` ran),
`@order` (+0x57 after `0x468D8`) `call #3/#4 memory`, `@reset` (`0x36D98` on the own slot) `g1` alone. The unit run
through `0x480B4` finds the byte 0: the real `0x3B298(1, ...)` has cleared slot 1's +0x43 by the time `0x48170` reads
it (measured), so the byte is checked on `0x48170` alone as well.

**`0x4811C`** (+0x10; ECX = the slot, ESI = EDX = the record, EBX = side): `mov dl,[eax+0x58]; cmp dl,1; jb` returns;
`jbe` (equal): the word `0x108380[side]` = 0 (`lea eax,[ebx*2]` between keeps the flags), +0x58 = 2; `cmp dl,2; je`:
`inc word [eax+0x108380]`, then `[eax+0x10837E] sar 16` (the signed word) above 0xF: +0x54 = 0, `0x36870(ESI)`;
above 2 nothing. Cases `i0`..`i5`; `@mutant` (`0x36870` on the slot) `call #0`, `@signed` (`i5`: 0x7FFF + 1) alone,
`@order` (+0x54 after the call) `call #0 memory`.

## §P3.8 Task 7: `0x4844C`

EBX = side, ctx; the side's words `0x10838C` and `0x108388` + 1 (`inc edx; inc ebx` over the words, stored at
`0x4846E` and `0x48479`); ctx[2]'s +0x57 against 4 (`ja`), table `0x48438` = `0x4849A 0x48601 0x484F0 0x4855A
0x48601`:

- **0**: v = the signed word ctx[4]+0x34 (`cmp word [eax+0x34],0; jge`; negative: `[eax+0x32] sar 16; neg`): above
  0x15E (`jle`) ctx[4]'s +0x42 = 0; the signed word `0x10838C[side]` (`[eax*2+0x10838A] sar 16`) above 0x1E: ctx[2]
  +0x54 = 0, `0x36870(ctx[4])`.
- **2**: ctx[3]'s +0x43 & 0x30 selects `0xC94CE` (set) or `0xC94BA`, five (key, voice) word pairs (`0xC94BA`: 2/0x78,
  4/0x68, 25/0x68, 51/0x79, 56/0x64; `0xC94CE`: 2/0x78, 4/0x70, 25/0x70, 51/0x79, 56/0x71); ECX = the signed word
  `0x108388[side]`; each pair whose `movsx` key equals it plays its zero-extended voice.
- **3**: ctx[2]'s word +0x74 = 0; ctx[4]'s +0x28 |= 0x20; the word `0x108384[side]` = ctx[2]'s word +0x2C; the signed
  word `0xBD884[ctx[2].+0x7A]` (0x1600 0x1400 0x1400 0x1600 0x1800 0x1800 0x1180) against ctx[2]'s dword +0x30
  (`jle`, signed): above: +0x54 = 0, `0x3C148(side)`, `0x3C16C(side)`, `0x188AC(side, ctx[4]'s +0x18, EBX = 0)`,
  `0x2BC30(ctx[4], [0xC8B58 + 4c], 3.0)`, `0x188DC(side, the signed word 0x108384[side])`, +0x57 = 4.
- 1, 4 and above 4: nothing.

Cases `a0`..`aD` (14); `@mutant` (the tables swapped) `call #0` (`a6`/`a7`: key 2 is 0x78 in both), `@signed` (the
count unsigned: `a3`'s 0x7FFF + 1 alone), `@abs` (no absolute value: `a2`'s -0x15F alone), `@order` (+0x54 after
`0x3C148`) `call #0 memory`, `@bound` (the dword +0x30 unsigned: `aB`'s -1 alone, where the raw goes on), `@zext`
(`aA`'s +0x2C word 0xF000 alone, `call #4`).

## §P3.9 The gp scenarios: only gp-u10-ending holds a P3 member

**The miss sets.** Every gp scenario was replayed in full on `20e8f4a` (P2's code, its closure not yet committed;
`make gp-replay scenario=... GP_OPTIONAL=1`, miss lines kept): `gp-idle-loss`, `gp-u5-charsel`, `gp-u6-moves-b`,
`gp-keys-fight`, `gp-twop` and the seven `gp-u8-*` record only the base pair and the two wipe hooks (`0x29D60`,
`0x5D812` from `frontend_mode_1b_step`; `gp-u8-endurance` the first alone); `gp-u9-win` adds `0x400E0 0x21044
0x21084` (P4/P5); **`gp-u10-ending`** adds `0x37DD4 0x29C78 0x3DA50` and **`0x475EC` (`hit_reaction_apply`, 11
hits)**. P2's closure (`7c5a483`, record U9/U10 §W.16) pinned exactly that set and re-pinned `GP_ENDING_TRACE_MIN_FIRST`
and `GP_ENDING_WIN_MIN_FIRST` from 2121/5634 to 9954 (the replay's end). First frames (lldb on the miss log's
new-entry store, `mem.c:77`, as §W.12 does; the frame word at `0xEF6DC`, the mode at `0x104B00`): `0x29D60` f=0x286
mode 0x1B, `0x5D812` f=0x3FE mode 0x1B, `0x37DD4` f=0x14FB mode 0xC, `0x29C78` f=0x14FE mode 0xD, `0x3DA50` f=0x155D
mode 0xF, **`0x475EC` f=0x1594 mode 0xF** (the final, where character 2 is fought).

**After Task 2** (`0x475EC` ported), the replay's set is P2's minus `0x475EC` (`distinct=7`): porting it reaches no
other P3 member and no new target (`0x475EC` starts no stream). `make gp-ending-oracle` with P2's pins:

```
gp_compare: gp-u10-ending: frames: first unexplained 331, ratchet N 331 ok
gp_compare: gp-u10-ending: trace: 0 differing through 9953; ratchet N 9954 ok
gp_compare: gp-u10-ending: path: 0 not reproduced through 29; ratchet N 30 ok
gp_compare: gp-u10-ending: win: 0 differing through 9953; ratchet N 9954 ok
```

**Every pin is exact and none moves**: 331 is the mode-8 long frame (§W.14, a comparison-model limit; `--min-first
332` fails), 9954 is the replay's end for the trace and the WIN fields (`9955` fails as unreachable), 30 is every
milestone (`31` fails); MAX_START 83 is unchanged. The row `{ 0x475ECu, "hit_reaction_apply" }` leaves
`k_miss_gp_u10_ending`, whose comment is rewritten (P2's wording replaced whole, its facts kept); the Makefile's
`GP_ENDING_*` provenance and AGENTS.md's re-measure list name P3's re-measure. Tasks 3-7 port no member any gp replay
reaches (no set held one), so no other set or pin moves; the final `make verify` (§P3.12) shows every gp ratchet at
its pin. **If P2's merge or a later merge changes the U10 set again** (the final's opponent order follows the rng),
re-measure it; never predict it (the controller's instruction, and §W.14's).

## §P3.10 Decisions, named gaps and limits

**No decision is left to the user.** The member list is the roadmap's (§P3.1); the +0x10 adapter follows
`fighter_21458_case10`'s precedent; `0x48170` is a member with its own row and a seam for `0x480B4`'s row (as P2's
`0x22404`). User decisions D2 (span code, a named gap) and D3 (callee rows in C1, which runs right after P3) stand.

Named gaps and limits:
- **The stream targets of P3's streams** (§P3.1): `0x47E04 0x47E30 0x482E4 0x48374` (P6) and `0x48254` (P7);
  unported, reached by no capture; in play those effects are missing (as before P3).
- **The callee rows** (D3): the new stubs `0x35838 0x3B298 0x39FB0 0x3A95C 0x3C190 0x3B714 0x3C148 0x468D8 0x36D98
  0x188DC 0x3C16C` join C1 with P2's eight and E3's four; `0x48170` has its own row here. After P3 the counter reads
  `11/64 rows with callees closed (14 have none)`.
- **x87** (§P3.6): equal stored floats proved for every float of [1.0, 6.0]; not claimed outside.
- **Not observable with the image's data and not claimed:** `0x47CB0`'s word sign (§P3.6); `0x47688`'s timer word is
  10 in the image (its sign is pinned by the poke of `h4`, not by game data).
- **Unit checks of the +0x18 hooks** compare with `0x18C14` run on the expected flags in one fixture state; a flag
  that state does not consult is covered by the row's by-value comparison only (the mutation proofs use a flag the
  state consults: flag 0 for `0x47648`/`0x477A8`, flag 5 for `0x47CB0`; for `0x48054` the +0x57 replacement, with
  the state chosen so that `0x18C14` returns 0).
- E3's, P1's and P2's limits stand: seeds are hand pokes; the memory at a call is mem[] only; the callee column is
  one level deep.

## §P3.11 The roadmap after P3

P3 **24** as listed (no change in membership); C1 gains the eleven stubs above; P4-P8 unchanged (P6 keeps `0x47E04
0x47E30 0x482E4 0x48374`, P7 `0x48254`). The re-measure list of U10 (AGENTS.md, Makefile) loses `0x475EC`.

## §P3.12 Results

The replay's per-task gates (`make diff-verify entry-triage` with scratch image paths; `PR_ORACLE_REQUIRED=1
./build/run_tests` "all checks passed" after each; the diff-verify Python suite 160 tests at the base, 161 from Task 2;
the E2 suite 44):

| after | diff-verify counter | entry-triage |
|---|---|---|
| `8d4cdf4` | `54/54 functions VERIFIED; 89/89 mutants detected; 1 named gaps; 7/41 rows with callees closed (13 have none)` | `297 / 198`; callbacks `9 / 62`; supplement 22 unported; stubs 60; voice `31 / 84 / 19` |
| Task 2 | `59/59 ...; 96/96 ...; 9/46 ... (13 have none)` | `292 / 203`; callbacks `4 / 67`; supplement 22; stubs 57; voice `31 / 84 / 19` |
| Task 3 | `63/63 ...; 104/104 ...; 10/49 ... (14 have none)` | `291 / 204`; callbacks `3 / 68`; supplement 19; stubs 56; voice `31 / 84 / 19` |
| Task 4 | `68/68 ...; 114/114 ...; 10/54 ... (14 have none)` | `290 / 205`; callbacks `2 / 69`; supplement 15; stubs 55; voice `29 / 86 / 19` |
| Task 5 | `72/72 ...; 127/127 ...; 11/58 ... (14 have none)` | `289 / 206`; callbacks `1 / 70`; supplement 13; stubs 54; voice `28 / 87 / 19` |
| Task 6 | `77/77 ...; 141/141 ...; 11/63 ... (14 have none)` | `288 / 207`; callbacks `0 / 71`; supplement 9; stubs 53; voice `28 / 87 / 19` |
| Task 7 | `78/78 functions VERIFIED; 147/147 mutants detected; 1 named gaps; 11/64 rows with callees closed (14 have none)` | unchanged (`0x4844C` is outside E2's universe) |

The closed rows added: `0x475EC`, `0x47608` (Task 2: `0x1A570` has P1's row), `0x47720` (Task 3: `0x3C4CC` and the
allowed `0x339AC`), `0x47FCC` (Task 5: `0x3C4CC` and `0x33950`). The rows (cases, blocks hit/total), all `VERIFIED`:
`475ec` 3 3/3, `47608` 3 3/3, `47624` 2 1/1, `48964` 3 6/6, `489a0` 3 6/6, `47720` 2 1/1, `476fc` 3 3/3, `47648` 2 1/1,
`47688` 5 6/6, `47874` 2 1/1, `47830` 5 3/3, `47798` 2 1/1, `477a8` 2 1/1, `477e8` 2 1/1, `47fcc` 2 1/1, `47cb0` 5 5/5,
`47d24` 3 1/1, `47e9c` 16 17/17, `48608` 2 1/1, `48054` 3 3/3, `480b4` 3 1/1, `48170` 3 3/3, `4811c` 6 8/8, `4844c` 14
23/23. What alone catches each mutant is pinned by `test_each_p3_mutant_is_caught_by_what_it_breaks` (`P3_KINDS`) and
its case lists. **A store sweep** (scratch: every non-stack store instruction of each row, `fstp` included, must be
the last writer of a byte that differs from the case's start at the end or at a recorded call, in some case) finds
every store of the 24 rows observable (81 store instructions; the hooks' stores are their stack flags, compared by
value). Every unit check's mutation proof (a deleted registration, a changed constant, index, bound or table) fails
the suite. `port_progress.py` stays `771 1203 64` / `731 731 100` (none of the 24 is a Ghidra `FN_` function) and
README does not move.

**The final gate** (the prototype's final state; `make verify` with the parallel-safe overrides): `EXIT=0` in 28 min 39 s on a host shared with
two other runs; the 45 oracle lines equal to `oracle-lines-base.txt`; `make audio-render` cmp-equal to `before-t2.wav`;
`symbols.h` regenerated byte-identical; `PR_ORACLE_REQUIRED=1 ./build/run_tests` all checks passed; `771 1203 64` /
`731 731 100`; the Task 7 counter and E2 lines above; no driver recorded an unexpected miss; and every gameplay ratchet
line identical to the baseline's at `8d4cdf4` (24 min 48 s): gp-idle-loss N 2064 / F 8320, gp-u5-charsel 516 / 1513,
gp-u6-moves-b 1005 / 2262 / moves 2949, gp-keys-fight 11, gp-twop 612 / 1506 / 1506, gp-u8-right-arcade 1072 / 2274,
gp-u8-left-training 1076 / 2338, gp-u8-right-training 1098 / 2402, gp-u8-tug-of-war 1107 / 2466, gp-u8-handicap 1022 /
2274, gp-u8-endurance 278 / 1174, gp-u8-attract-start 1087 / 2018, gp-u9-win 346 / 2150 / path 8 / win 3162,
gp-u10-ending 331 / 9954 / path 30 / win 9954. Facts also run: the image sha1; the rel32 and dword scans of §P3.1;
the callers of §P3.2 (`0x354DA..0x354E5`, `0x1952F`, `0x35144`, `0x350B0`); `callee_clobbers` of every stub (§P3.2's
table, re-derived by `test_each_stub_declares_the_registers_its_callee_clobbers`); the image tables quoted in
§P3.3-§P3.8; the stream probe of §P3.1; the lldb first frames of §P3.9; the x87 midpoint check of §P3.6.
