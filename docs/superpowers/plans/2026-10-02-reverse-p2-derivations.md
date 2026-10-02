# Reverse completion P2: the move callbacks 0x14EF8..0x3DCEC and the callbacks they store (record)

**Scope.** Track P's second batch (roadmap row P2 of record `2026-10-02-reverse-p1-derivations.md` §P1.3: "move
callbacks `0x14EF8..0x3DCEC` and the callbacks they store (`0x2116C 0x211F0 0x212CC 0x22510 0x22588`, with
`0x22404`)"), under spec `2026-09-30-reverse-completion-design.md` §4 track P and §6. Plan:
`2026-10-02-reverse-p2-move-callbacks.md`. Recipe: E3 record §E3.10; lessons: P1 record §P1.10-§P1.12 (stores no case
can observe, stream targets left behind).

**Status of the numbers.** Measured by the planner on 2026-10-02 at `main` `1085402` in two scratch copies of the
tree: a prototype (the tasks developed in order, each committed in a scratch git repository; the full `make verify`
ran on its final state) and a replay (a fresh copy of `1085402`, Tasks 2-8 applied in order with exactly the plan's
scripts by one driver, every red and green output and mutation result kept per step; the plan quotes those).
**The image** is `build/diffrun --exe data/game/C/PRAGE.EXE --image-out FILE`: 1 028 304 bytes from `0x10000`, sha1
`ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's, E3's and P1's). Every address and instruction below is capstone
5.0.7 over that image (fixups applied); `unicorn` 2.1.4 runs the original side. Ghidra was not consulted.

---

## §P2.1 The member list from the raw: 24, not 19

`diff_emu.static_scan(..., switches=True)` from each roadmap member (the tail call to `0x2C3FC` not followed) and a
scan of every immediate each one stores into a slot field:

| member | insns / blocks | direct callees | stores (code immediates) | reached by |
|---|---|---|---|---|
| `0x14EF8` | 25 / 3 | `3C4CC 2C3FC` | - | move-table dword `0xA46A8` (char 3, reaction 0x20) |
| `0x14F50` | 25 / 3 | `3C4CC 2C3FC` | - | `0xA46BC` (3, 0x21) |
| `0x15478` | 12 / 1 | `3C4CC` | - | `0xA47AC` (3, 0x2D) |
| `0x21114` | 33 / 10 | `3C4CC` | - | `0xA3DAC` (1, 0x2D), `0xA56AC` (6, 0x2D) |
| `0x21374` | 31 / 1 | `33950 3C4CC` | +0x0C `0x212CC` (`0x213BD`), +0x18 `0x2116C` (`0x213C8`), +0x1C `0x211F0` (`0x213D3`) | `0xA55BC` (6, 0x21) |
| `0x22938` | 43 / 3 | `33950 3C4CC 34D8C` | +0x0C **`0x22638`** (`0x22960`), +0x18 `0x22510` (`0x2296B`), +0x1C `0x22588` (`0x22976`) | `0xA3CD0` (1, 0x22) |
| `0x22A00` | 21 / 3 | `3C4CC` | +0x0C **`0x229FC`** (`0x22A2A`) | `0xA55A8` (6, 0x20) |
| `0x237D0` | 23 / 3 | `3C4CC 2C3FC` | - | `0xA5620` (6, 0x26) |
| `0x2381C` | 23 / 3 | `3C4CC 2C3FC` | - | `0xA560C` (6, 0x25) |
| `0x3D10C` | 35 / 3 | `2C3FC 3C4CC` | - | `0xA37BC` (0, 0x21) |
| `0x3DADC` | 25 / 3 | `3C4CC 2C3FC` | - | `0xA50F8` (5, 0x24) |
| `0x3DB34` | 25 / 3 | `3C4CC 2C3FC` | - | `0xA510C` (5, 0x25) |
| `0x3DCEC` | 12 / 1 | `3C4CC` | - | `0xA51AC` (5, 0x2D) |
| `0x2116C` | 38 / 5 | `33950 18BD4 18C14` | - | slot +0x18 (`0x21374`) |
| `0x22510` | 38 / 5 | `33950 18BD4 18C14` | - | slot +0x18 (`0x22938`) |
| `0x211F0` | 52 / 1 | `33950 34D8C 3C4CC 3C480 18AF8 3C208 39834 3C358 39A10 2C3FC` | - | slot +0x1C (`0x21374`) |
| `0x22588` | 36 / 1 | `33950 18AF8 34D8C 22404 3C358 3C208 39A10 2C3FC` | - | slot +0x1C (`0x22938`) |
| `0x22404` | 36 / 1 | `33950 2BC30 3C480 3C208` | - | called by `0x22588` (`0x225A6`) and `0x224EC` (`0x224FC`, P7) |
| `0x212CC` | 53 / 16 | `33950 3C4CC` | - | slot +0x0C (`0x21374`) |
| **`0x22638`** | 197 / 36 (with its table resolved) | `33950 2BC30 36870 2C3FC` | - | slot +0x0C (`0x22938`) |
| **`0x229FC`** | 1 / 1 (`c3`) | - | - | slot +0x0C (`0x22A00`) |
| **`0x14FA8`** | 31 / 3 | `2AE14` | - | `0xD000` dwords `0xD2E2E` (stream `0xD2E26`) and `0xD2E5E` (`0xD2E56`) |
| **`0x14FF8`** | 60 / 9 | `1A570 2AE14` | slot +8 (the held record) | `0xD000` dword `0xD2E34` (`0xD2E26`) |
| **`0x150AC`** | 60 / 9 | `1A570 2AE14` | slot +8 | `0xD000` dword `0xD2E64` (`0xD2E56`) |

**The raw wins on two counts (recorded corrections to the roadmap):**

1. **Two stored callbacks the roadmap missed.** `0x22938` stores `0x22638` as the slot's +0x0C (`0x22960 c7400c38260200`)
   and `0x22A00` stores `0x229FC` (`0x22A2A c7410cfc290200`). Neither is in E2's universe or Ghidra's list:
   `0x22638` starts right after its own jump table (`0x22618`, 8 dwords: `227C3 227E2 2280D 22930 x5`), reached
   through `jmp cs:[edx+0x22618]` with the index pre-scaled into a base (`lea edx,[eax*4]`), the form P1's
   after-table scan (§P1.2, `jmp [idx*4+T]` with no base) did not look for; `0x229FC` is the `ret` that ends
   `0x229E8` (`0x229FC: c3`; P4's animation target), the same shape as `0x213F0` and `0x29D60`. Both are called every
   frame by `0x3531C` case 7 while their slot is in state 7, so they are "callbacks they store".
2. **Three stream targets the replay reaches** (the decision below). `0x14EF8` and `0x14F50` start the streams
   `0xD2E26` and `0xD2E56`, whose `0xD000` words (opcode 0x10, mode 0x4000) name `0x14FA8` (both), `0x14FF8`
   (`0xD2E26`) and `0x150AC` (`0xD2E56`); E2 lists them as animation targets in P4 (`0x14FA8`) and P5 (`0x14FF8`,
   `0x150AC`). With only the two callbacks ported, `gp-u8-right-arcade`'s replay misses `0x14FA8` and `0x14FF8`
   (`fn-miss PR_GP_DUMP 0x14FA8 anim_indirect`, `0x14FF8 anim_indirect`), and with those two, `0x150AC`. `0x14FF8`
   and `0x150AC` put the held record in the slot's +8, which is the guard of `0x14EF8`/`0x14F50` themselves
   (`cmp dword [slot+8],0`), so the callbacks without their targets would let the move repeat where the raw blocks
   it. This is P1's I4 lesson (§P1.12: the finisher streams' targets moved into P1); P2 takes them so the U8 replay
   runs to its end, and P4/P5 lose them (§P2.11).

So P2 ports **24 functions**: the 19 of the roadmap, `0x22638`, `0x229FC`, `0x14FA8`, `0x14FF8`, `0x150AC`. None
is a Ghidra `FN_` function, so `port_progress.py` stays `771 1203 64` / `731 731 100`.

**The other stream targets of P2's streams stay open** (a scratch probe: each P2 stream started on a fixture record
with `actors_anim_begin(rec, stream, 1.0)` and walked 400 frames by `actor_sync` with the miss log armed; the
fixture's own data-address misses discarded): `0x213F0`, `0x213F4` (`0xE1672`, `0x211F0`'s), `0x22338`, `0x22494`,
`0x224EC` (`0xE4E08`..`0xE4E72`, `0x22638`'s), `0x3DB8C` (`0xD4AB2`, `0x3DADC`'s), `0x3DC3C` (`0xD4AFA`,
`0x3DB34`'s), `0x22A40` and `0x229E8` (`0xE1534`, `0x22A00`'s). No capture reaches them (no gp miss set holds one);
their owners are P4-P7 (§P1.3). Named in §P2.10.

## §P2.2 Callers, registers and EAX masks

- **Move callbacks** (the +0 dword of a `0xA3528 + (c*64 + r)*20` entry): `0x34E2C` at `0x35045`: `mov eax,[esp+8]`
  (slot), `mov ebx,[esp]` (side), `mov edx,[esp+0x10]` (rec), `call [esp+0x24]`. **Mask 0**: `0x34E2C` returns the
  callback's EAX (`0x35049..0x3504F`) to `0x352CD` (then `0x350D0` returns to `0x3531C`, whose only caller
  `0x35803` loads `mov eax,ebx`) and to `0x3CF2E` (`mov al,1` at `0x3CF33`; `0x3CE58`'s two callers `0x3CF7F` and
  `0x3D035` read AL alone, `mov dl,al`, then `lea eax`). A rel32 scan finds no other caller of `0x34E2C`; a scan of
  every `call [r/m]` below `0x5D000` finds no other reader of the move table's +0 dword.
- **Slot +0x0C callbacks**: `0x3531C` case 7 (`0x35431`, then `xor eax,eax`) and `0x38434` (`0x384D9`): (slot, rec,
  side), mask 0 (§P1.4, §P1.12 I3).
- **Slot +0x18 hooks**: `0x19020` at `0x1903F` (`mov eax,edx` = side, `call [ebx+0x1077c8]`), then `test eax,eax`
  (`0x19048`): **mask 0xFFFFFFFF**. The only `call [..+0x1077c8]` in the image.
- **Slot +0x1C callbacks**: `0x193B0` at `0x19505` (`mov eax,[esp]` = ctx[0], `call [edx+0x1c]`), then `mov
  eax,[esp+8]` (`0x19508`): mask 0. The only `call [..+0x1c]` reader of the slot field.
- **`0x22404`**: called at `0x225A6` (`0x22588`; then `mov eax,[esp]`) and `0x224FC` (`0x224EC`; then `mov
  eax,[esp+8]`): mask 0.
- **`0xD000` targets**: `0x2B2A0` at `0x2B56B..0x2B56D` (`mov eax,esi`; `call [0x105bd4]`; then `xor ecx,ecx; mov
  eax,ecx`): EAX = rec, mask 0. `0x14FA8` pushes EDX and zeroes it (`0x14FBF`); `0x14FF8`/`0x150AC` load EDX
  (`0x15021`/`0x15032`, `0x150D5`/`0x150E6`) before any read: the port's `anim_code_*` wrappers drop the operand.

**Callee declarations** (args in the port's C order; clobbers = `E.callee_clobbers(image, addr)`; all plain `ret`
unless shown):

| `E.Call` | callee (port C) | args | clobbers |
|---|---|---|---|
| `HIT_B` (E3) | `0x3C4CC` `hit_anim_start_b(rec, stream, frame)` | `eax edx s0`, `ret 4` | `edx` |
| `HIT_A` (E3) | `0x3C480` `hit_anim_start_a(...)` | `eax edx s0`, `ret 4` | `edx` |
| `ANIM_BEGIN` (E3) | `0x2BC30` `actors_anim_begin(...)` | `eax edx s0`, `ret 4` | `edx` |
| `VOICE` (E3) | `0x2C3FC` `sound_voice(id)` | `eax` | none |
| `SPAWN` (E3) | `0x2AE14` `actor_spawn(desc, a2, a3, a4, a5)` | `eax edx ecx ebx s0`, `ret 4` | `ebx ecx edx` |
| `BIT15` (P1) | `0x1A570` `fighter_actor_bit15_clear(side)` | `eax` | none |
| `FLASH` (new) | `0x34D8C` `hit_flash_pair(side)` | `eax` (`mov ebx,eax`; pushes EBX/EDX, pops both) | none |
| `CHECKS` (new) | `0x18C14` `fighter_18c14(side, flags, box_a, box_b)` | `eax [edx] [edx+4] [edx+8] [edx+12] ebx ecx` (§P2.7) | `ebx edx ebp` |
| `FACING` (new) | `0x18AF8` `fighter_18af8()` | none (`xor eax,eax; call 0x18b04`) | `ebx ecx edx` |
| `POSE` (new) | `0x39834` `fighter_39834(side, b)` | `eax edx` | `edx ebp` |
| `TIMER` (new) | `0x39A10` `fighter_39a10(rec, value)` | `eax edx` (`movsx ebx,dx`) | `edx` |
| `PLACE` (new) | `0x3C208` `fighter_3c208(side, dist)` | `eax edx` | `edx` |
| `HOLD` (new) | `0x3C358` `fighter_3c358(side)` | `eax` (pushes EDX, `mov edx,eax`) | none |
| `ARM404` (new) | `0x22404` `fighter_22404(side)` | `eax` (pushes EDX) | none |
| `ANIM54` (new) | `0x36870` `fighter_36870(rec)` | `eax` | `esi edi ebp` |

`0x33950` (`fighter_ctx_same`) and `0x18BD4` (`fighter_18bd4`, 16 bytes of 2 at EAX; a leaf) run on both sides
(allow): their EAX is a stack buffer in every P2 caller (§E3.10 item 6). The registers each P2 function reads after
a stubbed call were checked against these sets by running (the rows verify with the poison): `0x211F0` keeps EDX =
`0xE1672` through `0x34D8C` into `0x3C4CC` and EDX = `0x29A` through `0x3C358` into `0x39A10`; `0x3D10C` keeps EDX
= `0xE84C8` through the voice into `0x3C4CC` (`0x3D17C`'s idiom).

## §P2.3 The guard-shaped callbacks (Task 2)

`0x237D0`, `0x2381C`, `0x3DADC`, `0x3DB34`, `0x3D10C`, `0x22A00` (and in Task 3 `0x14EF8`, `0x14F50`): `cmp dword
[slot+8],0; je` else `xor al,al` and return with nothing written. Then `0x3C4CC(rec, STREAM, FRAME)` and the slot:

| entry | stream (load) | frame | +0x52/+0x53/+0x54 | +0x0C | +0x18/+0x1C | +0x64/+0x5F | other | voice |
|---|---|---|---|---|---|---|---|---|
| `0x237D0` | `0xE14D8` (`0x237DF`) | 3.0 | 0xB/6/0 | 0 | kept | `+0x64 = +0x5F; +0x5F = 0xFF` | | `0xAA` |
| `0x2381C` | `0xE1506` (`0x2382B`) | 3.0 | 0xB/6/0 | 0 | kept | same | | `0xAA` |
| `0x3DADC` | `0xD4AB2` (`0x3DAEB`) | 3.0 | 0xB/6/0 | 0 | 0/0 | same | | `0xB8` |
| `0x3DB34` | `0xD4AFA` (`0x3DB43`) | 3.0 | 0xB/6/0 | 0 | 0/0 | same | | `0xB8` |
| `0x3D10C` | `0xE84C8` (`0x3D128`, before the voice) | 3.0 | 0xB/6/0 | 0 | 0/0 | same | word `0x1080AC[rec+0x51]` = 0x80 (`0x3D170`; ESI = `movzx` rec+0x51 at `0x3D113`, before the guard) | `0x91` **first** |
| `0x22A00` | `0xE1534` (`0x22A0F`) | 3.0 | 9/7/0 | `0x229FC` | kept | `+0x64 = +0x5F` only | +0x57 = 0 | none |
| `0x14EF8` | `0xD2E26` (`0x14F07`) | **2.0** | 0xB/6/0 | 0 | 0/0 | same as `0x237D0` | | `0xB2` |
| `0x14F50` | `0xD2E56` (`0x14F5F`) | 2.0 | 0xB/6/0 | 0 | 0/0 | same | | `0xB2` |

Every store precedes the voice (the memory at the voice call holds them). `0x229FC` (`c3`) does nothing.

**Cases** (`p2_guarded`): `g0` slot+8 = `0x01000000` (non-zero in its high byte only: a port testing a byte runs the
body; `fighter_237d0@guard` is caught by `g0` alone); `g1` the body, +0x5F = 0x22; `g2` the body, +0x5F = 0x80, side
1, the voice stub's AL = 0 (`mov al,1` overwrites it; mask 0 anyway). Sentinels on +0x0C, +0x18, +0x1C, +0x52..+0x57,
+0x5F, +0x64 (`P2_SEED`); `0x3D10C` also seeds both words `0x1080AC` and takes rec+0x51 = 0, 0, 1.

## §P2.4 Character 3's reactions 0x20/0x21, their stream targets, and U8's right-arcade (Task 3)

`0x14EF8`/`0x14F50` as §P2.3. **`0x14FA8`** (EAX = rec): `0x2AE14(0xBB36C, 0, 0, 0, rec's word +0x56 | 0x400)`
(`or ah,4` over the zero-extended word); the spawned record's +0x59 = 2; for side 1 (`test bl,bl` on rec+0x51) its
+0x4E = 1 and word +0x2E += 4 (`0x14FD8..0x14FE3`, the +0x4E store first); rec+0x4B = its byte +0x56; its +0x60 = 1.
**`0x14FF8`** (EAX = rec): with the owner slot rec+0x14 (none: return), `0x1A570(rec+0x51)` (AL set: the word w =
`0xFEB6` = -0x14A and the x offset `0xFFFFEE00`; clear: 0x14A and 0x1200); `0x2AE14(0xBB380, rec+0x18 + (s16)off,
rec+0x30 >> 16 (sar), rec+0x1C + 0x1600, rec's word +0x28 bit 14 ? 0x4000 : 0)` into slot+8; its +0x59 = 2; word
+0x34 = w and +0x14 = slot through a re-read of slot+8; for side 1 word +0x2E += 4 and +0x4E = 1. **`0x150AC`**: the
same with 0x226 (`0xFDDA`). The image: `0xD2E2C: d000 a84f0100` and `0xD2E32: d000 f84f0100` in `0xD2E26`;
`0xD2E5C: d000 a84f0100` and `0xD2E62: d000 ac500100` in `0xD2E56`.

Cases: `0x14FA8` f0 (side 0, the spawn stub returns `E3_OUT`), f1 (side 1, `E3_REC2`; its word +0x2E = `0xFFFE`
wraps to 2); `0x14FF8`/`0x150AC` h0 (no owner), h1 (side 0, AL 0), h2 (side 1, AL 1, bit 14 set, x `0xFFFFF000`, z
`0xFFFD8000`, rec+0x30 `0xFFFD0000`: the `sar` gives -3), h3 (the word +0x28 `0xBFFF`: bit 14 clear, every other
bit set), h4 (side 0, AL 1, bit 14 set). The 0x1A570 stub's AL varies per case (§E3.12 I4).

**The unit run on the real streams** (`test_p2_reactions_3`): slot 0 (character 3) runs the callback through its
registration, then `actor_sync` walks its record's stream; the held record lands in slot+8 at frame 6 for both
streams (the probe printed `f=6`), with +0x14 = the slot, +0x59 = 2, word +0x34 = `0xFEB6` (`0xFDDA` for `0x14F50`;
this fixture's `0x1A570(0)` is 1), rec+0x4B = the spawned record's index (`0x14FA8` runs on both streams). A second
call of `0x14EF8` then returns at the guard.

**`gp-u8-right-arcade`** (U8 record §U8.16, pinned `GP_MODES_RA_MIN_FIRST = 726`, `GP_MODES_RA_TRACE_MIN_FIRST =
1978`: both stopped at f=0x7B7, character 3's reaction 0x20, on the unported `0x14EF8`). Measured with `make
gp-modes-one GP_MODES_ID=RA scenario=gp-u8-right-arcade`:

| port state | the replay's `fn-miss` lines beyond the base pair | first unexplained frame | trace |
|---|---|---|---|
| U8's pin (`bc51fd0`) | `0x14EF8`, `0x14F50` (`hit_reaction_apply`) | 726 (nearest port 526, f=0x7B6) | first difference f=0x7BA (1978) |
| `0x14EF8`/`0x14F50` only | `0x14FA8`, `0x14FF8` (`anim_indirect`) | (the driver fails its miss set) | |
| + `0x14FA8`, `0x14FF8` | `0x150AC` (`anim_indirect`) | (fails) | |
| + `0x150AC` (P2) | none: `0x29D60`, `0x5D812` (frontend_mode_1b_step) and the base pair only | **1072** (raw 4116), nearest port 824 | **0 differing through 2273** |

Capture frame 1071 equals port frame 825 (f=0x8E1, the port's last frame, the script's X record) with 0 differing
pixels, and 1072 differs from it by 5 045 (from port 824 by 5 108): 1072 is the next game frame, which the port never
ran: how far the port got, not a divergence. `--min-first 1073` fails (`first unexplained 1072 < ratchet N 1073`)
and `--trace-min-first 2275` fails (`N 2275 > end 2274: N is unreachable`): **N = 1072 and F = 2274 are the exact
pins** (both only rise: 726 -> 1072, 1978 -> 2274); MAX_START stays 88. The miss rows `{0x14EF8,
"hit_reaction_apply"}` and `{0x14F50, "hit_reaction_apply"}` leave `k_miss_gp_u8_right_arcade`.

## §P2.5 The unconditional callbacks (Task 4)

`0x15478`: `0x3C4CC(rec, 0xD2DD2, 4.0)`, the slot 9/8/0 (+0x54 = 0 after `mov al,1`). `0x3DCEC`: `mov edx,[0xc8cd4]`
(the stream read from the data word, `0xD40F2` in the image), 4.0, the slot 9/8/1. `0x21114`: p =
`DS_001077A8[rec+0x51]` (`xor edx,edx; mov dl,[eax+0x51]; shl edx,2`: the whole byte, no `& 1`); none: AL = 0,
nothing; by p's +0x7A (`cmp dl,1; jb; jbe; cmp dl,6; je; jmp`): 1 -> `0xE481C`, 6 -> `0xE1702`, at 5.0; any other
character starts nothing; then the slot 9/8/1 in every non-zero case.

Cases: `0x3DCEC` u1 pokes `0xC8CD4` = `0xE1234` (a port that hard-codes `0xD40F2` differs on u1 alone,
`fighter_3dcec@mutant`); `0x21114` w0 (zero pointer), w1 (char 1), w2 (side 1, char 6), w3 (char 0), w4 (char 2),
w5 (side 2 reads `0x1077B0`: char 7), w6 (char 5). `fighter_21114@side` (the other side's pointer) is caught on all
seven.

## §P2.6 The callbacks that arm the slot (Task 5)

Both read EBX alone: `mov edx,ebx; mov eax,esp; call 0x33950` (EAX and EDX are not read). **`0x21374`**:
`0x3C4CC(ctx[4], [0xC8950 + 4 * ctx[2].+0x7A], 2.0)`. **Correction found by running:** the record is `ctx[4]`, the
side's own: `mov eax,[esp+0x14]` (`0x21398`) follows `push 0x40000000` (`0x21384`), so `[esp+0x14]` is the context's
`+0x10` slot; the first draft read it as ctx[5] and the row printed `s0: call #0: original 0x3C4CC(0x10A300, ...),
port 0x3C4CC(0x10A400, ...)`. Then ctx[2]: +0x53 = 7, +0x52 = 9, +0x54 = 0, +0x0C = `0x212CC`, +0x18 = `0x2116C`,
+0x1C = `0x211F0`, +0x57 = 0, +0x41 |= 0x80. **`0x22938`**: ctx[3]'s +0x42 bit 4 (`test byte [eax+0x42],0x10`, the
other slot) refuses with AL = 0; else ctx[2] +0x57 = 0, +0x0C = `0x22638`, +0x18 = `0x22510`, +0x1C = `0x22588`,
+0x52 = 9, +0x54 = 0, +0x53 = 7; the word `0x104758[ctx[0]]` = 0 (`0x2299A`, before the call); `0x3C4CC(ctx[4],
0xE4DB4, 3.0)`; `0x34D8C(ctx[1])`; ctx[2] +0x42 |= 4; the word `0x104754[ctx[0]]` = 0 and the dword
`0x104738[ctx[0]]` = `0x40400000` (`0x229D0`, `0x229D8`, after both calls). `0x34D8C` gets its seam.

Cases (`p2_ctx_case`): both slots' characters 5 and 3, sentinels on the own slot's +0x0C..+0x1F, +0x41/+0x42,
+0x52..+0x57; `0x22938` t0 (the other +0x42 = 0x10, refused), t1/t2 (0xEF: every other bit) on both sides, the
per-side words and floats seeded. Mutants: `fighter_21374@mutant` (the other slot's character) `call #0`;
`fighter_22938@mutant` (`0x34D8C` on the own side) `call #1`; `fighter_22938@order` (the word `0x104754` and the
float stored before the calls) `call #0 memory`, `call #1 memory`.

## §P2.7 The +0x18 hooks, the flag bytes, and the registration table (Task 6)

**`0x2116C`**: ctx(side); `0x18BD4(flags)` (16 bytes of 2 at `[esp+0x18]`); flags 1, 8, 4, 0xE, 7, 0xD = 0, 5 = 1
(`0x21188..0x211A4`); `cmp dx,[ctx2+0x88]` with `dx = word [0xA81AE]` (3): `jl` returns 1 when 3 < w88; `cmp
bx,[..]` with the word `0xA81AC` (1): `jle` reaches `0x18C14(ctx[0], flags, 0xA81BE, 0xA81C8)` when 1 <= w88, else 1.
Signed words. **`0x22510`**: flags 1, 8, 4, 0xE, 7, 0xD = 0, 5, 9 = 1; `mov eax,[ctx0*2 + 0x104756]; sar eax,16`
(the signed word `0x104758[side]`): above 0x14 (`jg`) or below 0xD (`jge` to the call) returns 1; else
`0x18C14(ctx[0], flags, 0xA82C4, 0xA82CE)`. Both return EAX whole (mask 0xFFFFFFFF).

**The flag bytes cannot be compared as E3 §E3.10 item 6 says.** They live on the caller's stack, so `0x18C14` has no
mem[] offset to report, and item 6's alternative, running the callee on both sides (allow), is not available:
`0x18C14`'s call tree is 263 functions and 13 205 instructions with 16 indirect-call functions (`0x2B2A0`, `0x2AE14`,
the runtime). Without the flags the rows would verify ports that set any of them wrong (the hooks' only work). The
harness therefore compares them **by value**: an `E.Call` argument may be `[reg]` or `[reg+N]` (`diff_emu.deref_arg`),
the dword at that address when the callee is reached, and the port's seam passes the 16 bytes as four little-endian
dwords (`P2_FLAGS_DW` in `fighter.c`, a `/* PORT: */` beside the seam). `DerefArgTests` pins the read (`[edx]`,
`[edx+4]` of a synthetic caller: `0x44332211`, `0x88776655`) and the refusal of `[esp]`, `[edx+x]`, `[edx-4]`,
`edx+4`, `[edx`; mutating the offset away or accepting `esp` fails them. This extends §E3.10 item 6 (a stack buffer
handed to a stubbed callee is compared by its bytes when allow-mode is not available); it is additive (no existing
`Call` names such an argument).

Cases: `0x2116C` k0 (w88 4: 1), k1 (3: the call, stub EAX 0), k2 (side 1, 1: stub `0x12345678`), k3 (0: 1), k4 (the
bounds poked to -16 and 16, w88 -1: the call, stub 7; an unsigned compare returns 1). `0x22510` j0 (0x15), j1
(0x14: the call), j2 (side 1, 0xD: the call, `0x9ABCDEF0`), j3 (0xC), j4 (-1). Mutants: `@mutant` (flag 0xD / 9
wrong) `call #0`; `fighter_2116c@unsigned` (k4 alone), `@eax` (1 instead of the result: k1 k2 k4);
`fighter_22510@ge` (j1 alone).

**The registration table.** With this task's two hooks `run_tests` aborted: `fn_register: table full (limit 1300),
cannot register original address 0xF1A10`. `actors_init` runs four times in `run_tests` (`test_game.c` 2900 and
10064, `test_fight.c` 11055, `test_platform.c` 3871; `test_game.c` already says "every actors_init() registers its
handlers again and the registration table has a fixed limit"), and each run appended every pair again: 1 307 entries
for 351 distinct addresses (a scratch count). `fn_register` now skips a pair (address, function) already in the table;
`fn_resolve` returns the first entry for an address, so a repeated identical pair could never be returned: no
behaviour changes. One address has two functions (`0x255CC`); a second function for an address is still appended
(`test_fn_register_repeats` pins both, and without the skip it aborts on its 1 301st registration).

## §P2.8 The +0x1C callbacks and 0x22404 (Task 7)

**`0x22404`** (EAX = side): ctx; `0x2BC30(ctx[4], 0xE4DEA, 2.0)` (`[esp+0x10]` before the push); ctx[2] +0x57 = 2
(before the next call); `0x3C480(ctx[5], [0xC90F8 + 4 * ctx[3].+0x7A], 2.0)` (`[esp+0x18]` after the push);
`0x3C208(ctx[0], [0xA82D6 + 2c] sar 16)` (the signed word `0xA82D8[c]`); ctx[3] +0x53 = 0xA, +0x52 = 9, +0x54 = 0,
+0x10 = 0. **`0x211F0`**: ctx; `0x34D8C(ctx[0])` (EDX = `0xE1672` loaded first); `0x3C4CC(ctx[4], 0xE1672, 2.0)`;
`0x3C480(ctx[5], 0xC90F8[c], 2.0)`; `0x18AF8()`; `0x3C208(ctx[0], word 0xA81B0[c])` (`xor edx,edx; mov dx`:
zero-extended); `0x39834(ctx[1], ctx[2].+0x5F)`; `0x3C358(ctx[0])` (EDX = 0x29A); `0x39A10(ctx[4], 0x29A)`;
`0x39A10(ctx[5], 0x29A)`; the voice `0xC75AA[c]`; ctx[2] +0x57 = 2; ctx[3] +0x53 = 0xF. **`0x22588`**: ctx;
`0x18AF8()`; `0x34D8C(ctx[1])`; `0x22404(ctx[0])`; `0x3C358(ctx[0])`; ctx[3] +0x5D = 0x44; `0x3C208(ctx[0], the
signed word 0xA82D8[c])`; `0x39A10` twice; the voice `0xC75AA[c]`. c is ctx[3]'s +0x7A, re-read at each use. Image
words: `0xA82D8` = 0x17C0 0x1140 0x1180 0x1900 0x1900 0x1680 0x1100 (c = 0..6, all positive: `a2`/`d2` poke
character 3's to `0xF000` so a zero-extended read differs); `0xA81B0` = 0x1300 0xF00 0x1280 0x1500 0x1400 0x1300 0xF00;
`0xC75AA` = 0x91 0x96 0xA6 0xA0 0x84 0x8A 0x9B.

New seams: `0x18AF8` (no argument: `PR_SEAM0`, a new `mem.h` macro, since `PR_SEAM`'s initializer list cannot be
empty in C11), `0x39834`, `0x39A10`, `0x3C208`, `0x3C358`, `0x22404`. `hit_anim_start_a`, `fighter_18af8` and
`fighter_39834` lose `static` (declared in `fighter.h`) so the harness's mutants can call them. Mutants:
`fighter_22404@mutant` (+0x57 after the `0x3C480` call) `call #1 memory`; `@signed` (a2 alone) `call #2`;
`fighter_211f0@mutant` (the own character's stream) `call #2`; `@order` (+0x57 before the voice) `call #9 memory`;
`fighter_22588@mutant` (`0x34D8C` on the own side) `call #1`; `@order` (+0x5D after `0x3C208`) `call #4 memory`.

## §P2.9 The +0x0C callbacks (Task 8)

**`0x212CC`** (EBX = side; ECX = the EDX record, `mov ecx,edx` at `0x212D0`): by ctx[2]'s +0x57: 0: `cmp dx,[..+0x88]`
with the word `0xA81AE`; `jge` returns, else +0x57 = 1; 1: p = `DS_001077A8[rec+0x51]` (the EDX record's side, not
ctx[0]); none: return; p's +0x7A 1 -> `0xE4A18`, 6 -> `0xE16E6` at 3.0 on ctx[4]; then +0x52 = 9, +0x57 = 2, +0x8A =
0; above 1: return (the `jne 0x2136c` at `0x212EF` is never taken: AL is 0 there). Cases m0..m9 (m2: w88 -1, where an
unsigned compare steps; m9: rec+0x51 = 1 with the pointer only in `DS_001077AC`).

**`0x22638`** (EBX = side): the table `0x22618` is resolved by hand (`RESOLVED_JUMPS[0x227BC] = (0x227A3, 0x22618,
8)`; `verify_spec`/`verify_gap` now pass `RESOLVED_JUMPS` to their scans, which changes no existing row: no row's own
body holds one of the seven earlier resolved jumps) and the row covers 36/36 blocks. Per frame: t = the other side's
command word `DS_001088E0[ctx[1]] & 0xF0`; the side's count `0x104758` + 1 (`inc ebx` over the word); when t or
ctx[3]'s +0x63 is non-zero: with c = ctx[3]'s character, +0x5D (zero-extended byte) at most the signed word
`0xA82EC[c]` (4 for every character) becomes 0, else drops by that word's low byte; then while the signed word
`0xA8300[c]` (0x64 0x64 0x6E 0x78 0x6E 0x6E 0x78) exceeds the signed count, +0x5D is at least 1. The side's float
`0x104738`: on the command's bit 0, `fld; fadd qword [0x8098C]` (-0.7), `fstp`, then `fld1; fcomp dword` (1.0
against the stored float): below 1.0 it becomes 1.0; else `fld; fld st0; fadd qword [0x80980]` (0.1), `fstp st1;
fst dword`, `fcomp dword [0x80988]` (3.0) **against the unrounded sum**: above 3.0 it becomes 3.0. A command with
bits 1..3 is copied to `0x104754`. The switch on ctx[2].+0x57: 0 waits for the count above 0x14 (+0x57 = 1); 1
`0x2BC30(ctx[4], 0xE4DCE, 3.0)`, +0x57 = 3, +0x8A = 0; 2: ctx[3].+0x5D zero -> `0x36870(ctx[5])`, +0x57 = 1;
ctx[3].+0x53 not 0xA -> +0x57 = 1; else the own command bit 0 -> `0x2BC30(ctx[4], 0xE4E08, the float's bits)` (`push
dword [eax*4+0x104738]`), +0x57 = 4, voice 0x7D; the latched bit 2 -> `0xE4E34`, 5; bit 1 -> `0xE4E4A`, 6; bit 3 ->
`0xE4E72`, 7 (each at 3.0, voice 0x78); 3..7 and above 7 nothing. The 16 cases p0..pF reach every block; pE (the
count `0x8000` + 1, negative) and pF (+0x5D = 0x80, 128 against 4) pin the two signedness choices
(`fighter_22638@signed`, `@byte5d`, each caught by its case alone).

**Named limit (x87).** The raw adds in x87 registers (precision control as the runtime leaves it; unicorn starts with
FCW `0x37F`, extended) and compares the +0.1 sum before it is rounded to the float; the port adds in double
(`/* PORT: */` in the C). For a float x in 1.0..3.0 (the only values this code stores: `0x22938` sets 3.0 and the
clamps keep it there) the two agree. The comparison: the double sum and the extended sum can fall on different sides
of 3.0 only when the exact sum x + 0.1 lies within one double ulp (2^-51) of 3.0, that is x within about 2^-51 of
2.9 - 5.5·10^-18; the floats of [2, 4) are 2^-22 apart and the nearest to 2.9 is 9.5·10^-8 from it. The stored float:
a double rounding (exact -> 53 or 64 bits -> 24 bits) differs from a single one only when the first rounding lands
on a midpoint between two floats, which needs the bits of the exact sum below the float's 24 to be 1000…0 within
2^-29 of it; for x a float of [1, 3] those bits are 0.1's own repeating `1100` pattern (0.1·2^23 has the fraction
0.8), or -0.7's `0110` for the other arm (whose results 0.3..2.3 keep x's bits above the float's last), never near a
midpoint. Not claimed for x outside 1.0..3.0 (no path stores one); the cases use 1.2, 1.5, 2.0,
2.5 and 2.95.

`0x36870` gets its seam. Mutants: `fighter_212cc@mutant` (streams swapped) `call #0`, `@signed` (m2) `byte`,
`@side` (ctx[0]'s pointer) `byte`/`call #0`; `fighter_22638@mutant` (voice 0x78 for the bit-0 start) `call #1`.

## §P2.10 Decisions, named gaps and limits

**No decision is left to the user.** Two choices were taken from the raw and recorded as corrections: the 24
members (§P2.1, with P1's I4 precedent for the stream targets) and the by-value comparison of `0x18C14`'s flags
(§P2.7, forced: allow-mode is not available). Both are reversible by deleting rows; neither changes game behaviour.

Named gaps and limits:
- **The other stream targets of P2's streams** (§P2.1 list): unported, owned by P4-P7; no capture reaches them, so no
  oracle moves; in play those effects are missing (as before P2).
- **The callee rows** (decision D3): the new stubs `0x34D8C 0x18C14 0x18AF8 0x39834 0x39A10 0x3C208 0x3C358 0x36870`
  and the earlier unverified `0x2BC30 0x2C3FC 0x2AE14 0x3C480` get rows in C1; `0x22404` has its own row here.
  After P2 the counter reads `7/41 rows with callees closed (13 have none)`.
- **x87** (§P2.9).
- **One stub EAX per case** (§P1.12): `0x14FF8`/`0x150AC` call `0x2AE14` once, so not affected.
- **No capture reaches** `0x2116C 0x22510 0x211F0 0x22588 0x22404 0x212CC 0x22638 0x21374 0x22938` or the Task 2/4
  callbacks: no gp miss set held one (§P2.11); the U8 right-arcade replay is the one capture that exercises a P2
  member (`0x14EF8`, its stream targets).
- E3's and P1's limits stand: seeds are hand pokes; the memory at a call is mem[] only; the callee column is one level
  deep.

## §P2.11 The roadmap after P2

P2 **24** (19 + `0x22638 0x229FC` + `0x14FA8 0x14FF8 0x150AC`); P4 **20** (less `0x14FA8`); P5 **13** (less
`0x14FF8 0x150AC`); the rest unchanged. Total **145 functions** in 8 porting batches (143 + the two stored callbacks
outside E2). `0x22404` precedes `0x224EC` (P7) and is now ported.

## §P2.12 The U8 and U9/U10 interactions

- **U8:** only `gp-u8-right-arcade`'s miss set held P2 members (§P2.4). The other gp sets (`gp-idle-loss`,
  `gp-u5-charsel`, `gp-u6-moves`, `gp-keys-fight`, `gp-twop`, the six other `gp-u8-*`) hold only `0x29D60` and
  `0x5D812`: every call to a P2 member goes through `fn_resolve`, which would have logged it, so none of those replays
  reaches one and their pins do not move (the final `make verify`, §P2.13).
- **U9/U10** (branch `gameplay-u9-u10`, record `2026-10-02-gameplay-u9-u10-derivations.md` §W.7) predicts misses on
  `0x400E0` (P4), `0x21044` (P5), `0x21084` (P4) and `0x3DA50` (P5): **none is a P2 member**. Its scenarios fight
  with characters whose reactions may reach P2 callbacks; if they do, the port now runs them where it skipped them,
  and U9/U10's measured pins (taken on its own base) must be re-measured after P2 merges, never predicted.

## §P2.13 Results

The replay's per-task gates (`make diff-verify entry-triage` with scratch image paths; `PR_ORACLE_REQUIRED=1
./build/run_tests` "all checks passed" after each; the Python suite 156 tests at the base, 157 from Task 2, 159 from
Task 6):

| after | diff-verify counter | entry-triage |
|---|---|---|
| `1085402` | `30/30 functions VERIFIED; 47/47 mutants detected; 1 named gaps; 1/18 rows with callees closed (12 have none)` | `313 / 182`; supplement 28 unported; voice `40 / 75 / 19` |
| Task 2 | `37/37 ...; 55/55 ...; 2/24 ... (13 have none)` | `307 / 188`; callbacks `16 / 55`; stubs 70; voice `35 / 80 / 19` |
| Task 3 | `42/42 ...; 60/60 ...; 2/29 ... (13 have none)` | `302 / 193`; callbacks `14 / 57`; animation targets `54 / 58`; stubs 65; voice `33 / 82 / 19` |
| Task 4 | `45/45 ...; 64/64 ...; 5/32 ... (13 have none)` | `299 / 196`; callbacks `11 / 60`; stubs 62 |
| Task 5 | `47/47 ...; 67/67 ...; 6/34 ... (13 have none)` | `297 / 198`; callbacks `9 / 62`; stubs 60 |
| Task 6 | `49/49 ...; 72/72 ...; 6/36 ... (13 have none)` | supplement 26 unported |
| Task 7 | `52/52 ...; 78/78 ...; 6/39 ... (13 have none)` | supplement 23 unported; voice `31 / 84 / 19` |
| Task 8 | `54/54 functions VERIFIED; 84/84 mutants detected; 1 named gaps; 7/41 rows with callees closed (13 have none)` | `297 / 198`; supplement 22 unported; voice `31 / 84 / 19` |

The closed rows: `0x22A00` (Task 2: its one callee `0x3C4CC` has an E3 row), `0x15478`, `0x3DCEC`, `0x21114` (Task
4, the same), `0x21374` (Task 5: `0x3C4CC` and the allowed `0x33950`, which has its own E3 row), `0x212CC` (Task 8,
the same). The rows (cases, blocks hit/total), all `VERIFIED`, none with an unhit block: `237d0` 3 3/3, `2381c` 3
3/3, `3dadc` 3 3/3, `3db34` 3 3/3, `3d10c` 3 3/3, `22a00` 3 3/3, `229fc` 1 1/1, `14ef8` 3 3/3, `14f50` 3 3/3,
`14fa8` 2 3/3, `14ff8` 5 9/9, `150ac` 5 9/9, `15478` 2 1/1, `3dcec` 2 1/1, `21114` 7 10/10, `21374` 2 1/1, `22938` 3
3/3, `2116c` 5 5/5, `22510` 5 5/5, `22404` 3 1/1, `211f0` 2 1/1, `22588` 3 1/1, `212cc` 10 16/16, `22638` 16 36/36.
What alone catches each mutant is pinned by `test_each_p2_mutant_is_caught_by_what_it_breaks` (`P2_KINDS`), and the
cases that alone catch the boundary mutants by its case lists (`g0`, `u1`, `w0..w6`, `k4`, `j1`, `k1 k2 k4`, `a2`,
`m2`, `pE`, `pF`). Every unit check's mutation proof (deleting a registration, changing a stored constant or a
bound) fails the suite; the harness mutations (the `[reg+N]` offset dropped, `esp` accepted, the `0x227BC`
resolution removed) fail their Python tests; the `fn_register` skip removed makes the run abort (`exit -6`, `table
full`).

**The final gate** (the prototype's final state, a scratch git repository so `make`'s `git diff` step runs; `make
verify` with the parallel-safe overrides, 35 min 8 s on a host shared with the replay): `EXIT=0`, the 45 oracle lines
equal to `oracle-lines-base.txt`, `make audio-render` cmp-equal to `before-t2.wav`, `symbols.h` regenerated
byte-identical, `PR_ORACLE_REQUIRED=1 ./build/run_tests` all checks passed, `771 1203 64` / `731 731 100`, the
counter and triage lines of Task 8 above, and every gameplay ratchet at its pin: gp-idle-loss N 2064 / F 8320,
gp-u5-charsel 516 / 1513, gp-u6-moves-b 1005 / 2262 / moves 2949, gp-keys-fight 11, gp-twop 612 / 1506 / 1506,
**gp-u8-right-arcade 1072 / 2274** (the new pins), gp-u8-left-training 1076 / 2338, gp-u8-right-training 1098 /
2402, gp-u8-tug-of-war 1107 / 2466, gp-u8-handicap 1022 / 2274, gp-u8-endurance 278 / 1174, gp-u8-attract-start 1087
/ 2018. Facts also run: the image sha1; the three scans of §P2.2 (rel32 callers of `0x34E2C`, `0x22404`, `0x19020`,
`0x193B0`; every `call [r/m]` reading +0x0C/+0x18/+0x1C or `0x1077C8`); `callee_clobbers` of every stub (§P2.2's
table, re-derived by `test_each_stub_declares_the_registers_its_callee_clobbers`); the image tables quoted in
§P2.4-§P2.9; the `0x18C14` call-tree size; the stream probe of §P2.1.
