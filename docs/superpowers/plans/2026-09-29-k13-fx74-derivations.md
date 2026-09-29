# K13 FX-7.4 — the inline blocks of `0x49C78` — raw-byte derivation (Task 5b of `2026-09-29-all-gaps.md`)

**Scope.** Ledger `2026-09-29-all-gaps-ledger.md` §E rows 15, 16, 17, 18, 22
(and the stale row 23), §F order 14 (K13). These are the parts of the effects
pass `0x49C78` (`fight_effects_pass`, `fight.c`) that the port skipped: the
case-13 body, the case-14 body (with the `0x4A361` site of `0x4A868`), the
mode-9 block and the frame locals it reads, and the "type-8 held body" that
`fight.h` still listed. Sections are `§K13.n`; the port comments cite them.
None of them is a function: they are spans of `0x49C78`, so they are ported
inline in `fight_effects_pass` with the address range in the block comment,
as cases 1..12 already were. The function counter does not move.

**Tooling.** The Ghidra MCP bridge was not reachable (`ToolSearch "ghidra
decompile"` finds no tool). Every byte was read from the Python mirror of
`mem_load_le` + `mem_load_le_fixups` (`port/src/mem.c`), so each dword has
the LE fixups applied, and disassembled with capstone (32-bit). The case jump
table at `0x49C2C` (15 dwords, types 0..0xE) reads `0x49D1E, 0x49D2F,
0x49D90, 0x49DB3, 0x49E5A, 0x49EC0, 0x49F11, 0x49FC2, 0x4A08A, 0x4A115,
0x4A131, 0x4A17A, 0x4A1EB, 0x4A24A, 0x4A346`.

**Corrections to the brief (raw wins).**

| brief | raw | evidence |
|---|---|---|
| case-13 body `0x4A24A..0x4A2F4` (170 B) | `0x4A24A..0x4A345` (252 B) plus the shared tail `0x4A45C..0x4A467`. `0x4A2F4` is only the end of the draw-gate setup: the `0x2BE00` call is at `0x4A2F5`, the draws at `0x4A305`/`0x4A315`, `0x496DC` at `0x4A32B`. | §K13.1 listing |
| case-14 body `0x4A346..0x4A412` (204 B) | `0x4A346..0x4A45B` (278 B) plus the same shared tail. `0x4A413` starts its draw gate. | §K13.2 listing |
| mode-9 block `0x4A487..0x4A58F` (264 B) | `0x4A487..0x4A590` (266 B; the last instruction `jne 0x4A581` is at `0x4A58F`, 2 bytes), gated at `0x4A476..0x4A481`. | §K13.3 listing |
| "type-8 held body" unported | **Already ported** (`fight.c` case 8, record §42-C): `0x4A08A..0x4A114` (139 B). The comment in `fight.h` was stale. | §K13.4 |

---

## §K13.1 Case 13 `0x4A24A..0x4A345` (+ tail `0x4A45C..0x4A467`)

Registers on entry (from the walk, `0x49CD1..0x49D1C`): ECX = entry, EBP =
next, EBX = rec (`[ecx+8]`), ESI = EDI = si = `(u16)(rec+0x48 - 0x20)`, EDX =
si * 4 (`0x49D0F lea edx,[edi*4]`).

```
4a24a: mov edx,[ecx+8] ; mov eax,[edx+0x3c]      ; x = rec+0x3C (dword)
4a250: mov dx,[edx+0x2a] ; xor dl,dl ; and dh,0x10 ; and edx,0xffff
4a25f: je 0x4a273                                ; +0x2A bit 0x1000 clear -> on
4a261: cmp eax,-0x300 ; jl 0x4a273               ; signed
4a268: cmp eax,0x5700 ; jle 0x4a468              ; on screen: next entry
4a273: mov eax,[0x1088c6] ; xor edx,edx ; sar eax,0x18   ; (s8)DS_001088C9
4a27d: mov dl,[ecx+0x21] ; cmp edx,eax ; jne 0x4a29a     ; zero-ext side vs it
4a284: cmp byte [0x1088c7],0 ; jne 0x4a29a
4a28d: mov eax,[ecx+8] ; call 0x2b150 ; jmp 0x4a468      ; kill the actor
4a29a: mov edx,[ecx+8] ; mov eax,[edx+0x32] ; sar eax,0x10 ; cmp eax,0x80 ; jne 4a2b9
4a2aa: or byte [edx+0x29],0x40 ; mov word [rec+0x34],0xff80 ; jmp 4a2c6
4a2b9: and byte [edx+0x29],0xbf ; mov word [rec+0x34],0x80
4a2c6: xor eax,eax ; mov ax,si ; mov edx,[eax*4+0xc95d4]
4a2d2: mov eax,[ecx+8] ; push 0x40400000 ; call 0x2bc30
4a2df: mov eax,[0x1088c6] ; sar eax,0x18 ; imul eax,eax,0x94 ; mov eax,[eax+0x1077b0]
4a2f3: mov ebx,ecx ; call 0x2be00 ; mov edx,eax ; test eax,eax ; jle 4a310
4a300: mov eax,0xc00 ; call 0x5d7dc ; sub edx,eax ; mov eax,edx ; jmp 4a31c
4a310: mov eax,0xc00 ; call 0x5d7dc ; add eax,edx
4a31c: mov edx,[0x108878] ; mov [ebx+0x14],eax
4a325: mov eax,ecx ; mov byte [ecx+0x1e],0xe ; call 0x496dc   ; EAX = entry, EDX = count
4a330: mov eax,[ecx+8] ; mov eax,[eax+0x30] ; xor edx,edx ; sar eax,0x10
4a33b: mov [0x108878],edx ; jmp 0x4a45c
4a45c: call 0x496ac ; mov edx,[ecx+8] ; mov [edx+0x2c],ax
4a468: mov ecx,ebp ; cmp ebp,0x10884c ; jne 0x49cd1
```

Semantics (ported verbatim):

1. **On-screen wait.** With the actor's `+0x2A` word bit `0x1000` set and its
   `+0x3C` dword in `[-0x300, 0x5700]` (both compares signed) nothing happens.
2. **Kill.** Else, when the entry's side byte `+0x21` (zero-extended) equals
   the sign-extended `DS_001088C9` and `DS_001088C7` is 0, `0x2B150`
   (`actor_set_dead`) kills the actor and nothing else changes.
3. **Turn.** Else a speed of exactly `0x80` (`[+0x32] sar 16`, the signed
   `+0x34` word) becomes `-0x80` with `+0x29 |= 0x40` (hflip); any other
   speed becomes `0x80` with `+0x29 &= 0xBF`. The actor takes the
   `0xC95D4[si]` stream at 3.0 (`0x2BC30`, `actors_anim_begin`).
4. **Target.** The case record is `slot[(s8)DS_001088C9]` (`fight_case_rec`,
   `0x1077B0 + idx * 0x94`); with its `0x2BE00` term `r`, `+0x14 = r -
   rng(0xC00)` when `r > 0` (signed `jle`), else `rng(0xC00) + r`.
5. **Spawn.** The entry becomes type `0x0E` and `0x496DC(entry,
   DS_00108878)` spawns that many more (`fight_496dc`, record §49-Z, its only
   caller); `DS_00108878` (the cap `0x4B2AC` computes) is then zeroed. The y
   (`+0x30 sar 16`) is read after the call.
6. **Tail.** `+0x2C = 0x496AC(y)` (`fight_dust_clamp`).

The draw order is the target's `rng(0xC00)`, then `0x496DC`'s three per
spawned entry (`rng(0x64)` pick, `rng(0x180)`, `rng(0xC00)`). The port's old
case 13 drew `rng(0xC00)` unconditionally; the raw draws it only on the turn
path.

## §K13.2 Case 14 `0x4A346..0x4A45B` (+ tail)

```
4a346: mov ebx,[esp+0x10] ; mov eax,[ecx+8] ; inc ebx ; mov di,[eax+0x34]
4a352: mov [esp+0x10],ebx                     ; case 14's count, every visit
4a356: test di,di ; je 0x4a468                ; stopped: next
4a35f: mov eax,ecx ; call 0x4a868 ; test eax,eax ; je 0x4a3a4
4a36a: rec+0x38 = 0 ; rec+0x34 = 0 ; rec+0x36 = 0 ; byte rec+0x55 = 1
4a38c: mov edx,[edx+0xc955c]                  ; EDX = si*4 (0x4A868 pushes/pops EDX)
4a392: mov eax,[ecx+8] ; push 0x40400000 ; call 0x2bc30 ; jmp 0x4a468
4a3a4: the on-screen wait as 0x4A24A (0x4a3a4..0x4a3c8, jle 0x4a468)
4a3ce: the turn as 0x4A29A (0x4a3ce..0x4a3f4)
4a3fa: the 0xc95d4[si] stream at 3.0 (0x4a3fa..0x4a40e)
4a413: the target as 0x4A2DF (0x4a413..0x4a450: mov [ebx+0x14],eax)
4a453: mov eax,[ecx+8] ; mov eax,[eax+0x30] ; sar eax,0x10 ; (falls into 0x4a45c)
```

`0x4A868` (`fight_4a868`, record §K4.1) starts with `push ebx / push ecx /
push edx` and pops them before `ret` (`0x4A868..0x4A8A6`), so EDX is still
`si * 4` at `0x4A38C`: the stop stream is `0xC955C[si]`. Case 14 has no kill,
no type change and no `0x496DC`; `DS_00108878` is not touched. `0x4A361` is
now wired: all six `0x4A868` sites are called (record §K4.5).

## §K13.3 The frame locals and the mode-9 block `0x4A476..0x4A590`

**Frame.** `sub esp,0x1c` (`0x49C7E`). The locals, by writer and reader:

| local | written | read |
|---|---|---|
| `[ESP]`, `[ESP+2]` words | zeroed every call (`0x49CC1`, `0x49CBC`); `[ESP + side*2]++` per entry (`0x49CDD..0x49CEA`, side = `+0x21`) | `0x4A48E` (`[ESP + DS_00104B16*2]`) |
| `[ESP+4]` dword | case 3's landing: `0x4000` when `0x2BE1C > 0` (`0x49DF2`), else 0 (`0x49DFE`), read back at `0x49E04` and OR-ed into `+0x28` (fix round 1); `[ESP + 2*2]++` for a side byte of 2 | its low word at `0x4A48E` when `DS_00104B16 == 2`; `0x49E04` |
| `[ESP+8]` dword | mode 9: 0 (`0x49C9C`); `inc` per entry (`0x49CEE..0x49CF8`) | `0x4A538` (as BX) |
| `[ESP+0xC]` dword | mode 9: EDX with DL = DH = 0 (`0x49CA6`); type 9 `inc` (`0x4A128`) | `0x4A549` (as AX) |
| `[ESP+0x10]` dword | mode 9: 0 (`0x49CA2`); case 14 `inc` (`0x4A352`) | `0x4A499` (as DX) |
| `[ESP+0x14]` byte | mode 9: 0 (`0x49C98`); type 11: 1 (`0x4A17A`) | `0x4A56E` |
| `[ESP+0x18]` byte | mode 9: 1 (`0x49C94`); type 11 moving: 0 (`0x4A1E2`) | `0x4A567` |

Apart from case 3's read-back of `[ESP+4]` right after its own store (`0x49E04`), every reader is in the mode-9 block, so outside mode 9 the unset values are
never read (the port starts them at 0).

**The block.**

```
4a476: xor eax,eax ; mov ax,[0x104b00] ; cmp eax,9 ; jne 0x4a591
4a487: xor eax,eax ; mov al,[0x104b16] ; mov ax,[esp+eax*2]   ; side count
4a492: xor edx,edx ; and eax,0xffff ; mov dx,[esp+0x10] ; sub eax,2
4a4a1: cmp edx,eax ; jl 0x4a4d5                              ; signed
4a4a5: cmp byte [0x1088c3],0 ; jne 0x4a4d5
4a4ae: mov eax,0xcb ; mov bl,1 ; call 0x2c3fc               ; voice (EBX pushed/popped by 0x2C3FC)
4a4ba: mov eax,0x14 ; mov [0x1088c3],bl ; call 0x5d7dc ; add eax,0x78 ; mov [0x1088b0],ax
4a4d5: cmp byte [0x1088c3],0 ; je 0x4a507
4a4de: cmp word [0x1088b0],0 ; jne 0x4a507
4a4e8: mov eax,0xcb ; call 0x2c3fc ; mov eax,0x14 ; call 0x5d7dc ; add eax,0x78 ; mov [0x1088b0],ax
4a507: cmp byte [0x1088c3],0 ; je 0x4a531
4a510: mov ax,[0x1088b0] ; xor edx,edx ; mov dx,ax ; dec eax ; mov [0x1088b0],ax
4a522: cmp edx,0x3c ; jne 0x4a531 ; mov eax,0xdc ; call 0x2c3fc
4a531: xor eax,eax ; mov al,[0x10780a] ; mov ebx,[esp+8] ; cmp eax,0x78 ; setl al
4a542: xor edx,edx ; mov [0x1088c9],al
4a549: mov eax,[esp+0xc] ; mov [0x1088b4],dx ; cmp ax,bx ; jne 0x4a567
4a559: mov word [0x1088b4],1 ; call 0x4a928
4a567: cmp byte [esp+0x18],0 ; je 0x4a591
4a56e: cmp byte [esp+0x14],0 ; je 0x4a591
4a575: mov eax,[0x10884c] ; cmp eax,0x10884c ; je 0x4a591
4a581: mov edx,[eax] ; mov byte [eax+0x1e],0xc ; mov eax,edx ; cmp edx,0x10884c ; jne 0x4a581
4a591: call 0x4a634 ...                                       ; the tail (ported)
```

Semantics (ported verbatim):

1. **Voice latch.** When case 14's count (a word, zero-extended) is at least
   the `DS_00104B16` side's entry count minus 2 (signed) and `DS_001088C3` is
   0: voice `0xCB`, `DS_001088C3 = 1`, `DS_001088B0 = rng(0x14) + 0x78`.
2. **Re-arm.** Latched with `DS_001088B0 == 0`: voice `0xCB`, `DS_001088B0 =
   rng(0x14) + 0x78`.
3. **Countdown.** Latched: `DS_001088B0 -= 1`; the pre-decrement value `0x3C`
   plays voice `0xDC`.
4. `DS_001088C9 = (slot 0's +0x5A, DS_0010780A) < 0x78`.
5. `DS_001088B4 = 0`; when type 9's count equals the entry count (as words)
   `DS_001088B4 = 1` and the survey `0x4A928` (`fight_4a928`, record §49-Z,
   its only caller) runs.
6. **Scatter.** With `[ESP+0x18]` and `[ESP+0x14]` both set (a type-11
   walker ran and none is still moving) every list entry becomes type 12.

The three voices stay `PORT:` notes (record §45-A), like every effects-path
voice; the `DS_001088C3 = 1` store after the first one is kept (0x2C3FC
pushes and pops EBX, `0x2C3FC push ebx`, so BL is still 1).

**Named gap (the drawn round's side count).** `DS_00104B16` is the round
winner: 0 or 1 a side, **2 a draw** (`0x27DB3`, `flow.c`). With 2 the block
reads the word `[ESP+4]`. **Correction (fix round 1, raw wins):** the first
version of this record said `0x49C78` never writes it. That was wrong. Case 3's
landing uses the dword as its hflip temporary:

```
49de6: call 0x2be1c ; lea edx,[ebx+0x28] ; test eax,eax ; jle 0x49dfc
49df2: mov dword [esp+4],0x4000 ; jmp 0x49e02
49dfc: xor edi,edi ; mov dword [esp+4],edi
49e02: xor eax,eax ; mov edi,[esp+4] ; mov ax,[edx] ; or eax,edi ; mov [edx],ax
```

A scan of every ESP-relative operand in `0x49C78..0x4A633` finds no other
store to `[esp+4]`/`[esp+6]`. So once a type-3 entry lands in a call, the
word is `0x4000` (the drawn round's limit `0x3FFE` keeps the latch shut) or
0 (limit -2 opens it). Type 3 is common; its writers are `0x4B656` and
`0x4D143`. The port's case 3 now stores `loc_side[2]` the same way and ORs
it into `+0x28`. **The named gap is only the value before the first case-3
store in a call** (and before any side-2 increment). That is uninitialised
memory of `0x49C78`'s own `sub esp,0x1c` frame (`0x49C7E`), holding whatever
an earlier call at that stack depth left. Pinning it needs a runtime capture
at `0x4A48E`. The port starts it at 0. It decides only whether the voice
latch opens on a drawn round with no type-3 landing that frame (one
`rng(0x14)` draw). The dword store also zeroes the word `[ESP+6]`, which
only a side byte of 3 would reach (the `PORT:` bound below).

**`PORT:` bound.** The raw indexes `[ESP + side*2]` with the entry's
`+0x21` byte unbounded. Every `+0x21` writer stores a side argument or a
copy of one: `0x49626` (`[esp+8]`, `0x494A8`), `0x497BB` (the parent entry's,
`0x496DC`), `0x49B3C` (`[esp+0x10]`, `0x4987C`), `0x4BCD9`, `0x4CA63`,
`0x4D065` (the side arguments of their builders), `0x4DE42`
(`DS_0010810D`) and `0x4E4E8` (0). A byte of 3 or more would alias the
other frame locals (or the caller's frame); the port counts sides 0..2
only.

## §K13.4 The "type-8 held body" — already ported

`fight.h`'s header listed "the type-8 held body" as a gap. The raw case 8 is
`0x4A08A..0x4A114` (139 B): the `+0x1C` bit-6 gate (`0x4A097`), `0x4AF04`
on the `+0x20` holder (`0x4A0A2`), the `+0x4A` link (`0x4A0B2`), the
release (`0x4A0CD..0x4A100`: the holder record's `+0x4B`, `+0x2A` bit 3,
`+0x29` bit 6, `+0x4A`, the entry's bit 6, `DS_001088AE[side]++`,
`DS_001088B2[side] = 1`) and `0x4B470(entry, si)` (`0x4A107 mov edx,edi`).
The port's case 8 (record §42-C) matches it instruction for instruction.
Only the `fight.h` wording was stale; it is corrected.

## §K13.5 Reachability (probe, not committed)

Types 12/13/14 have only these raw writers (a scan for `C6 4x 1E imm`):
type 12 `0x4A583` (the mode-9 block), 13/14 `0x4A241`/`0x4A238` (case 12,
set only by that scatter), 14 `0x4A327` (case 13) and `0x49807`
(`0x496DC`, called only from case 13). Type 9 only `0x4AAE6` (`0x4AAD0` in
mode 9). So all three blocks live in mode 9 (the post-match results,
`0x28788`).

A temporary `fprintf` in `fight_effects_pass` (applied and reverted by a
script; `git status` clean afterwards) printed each change of
`DS_00104B00` and the first three dispatches of each type 8..14:

- `prageport --check 8000`: mode 3 only (frame 1958, the first call); type 8
  at frames 1976/1977/1978 (the positive control); no type 9..14; no mode 9.
- `PR_FRONTEND_DET` (the front-end/demo-fight/attract2 driver, `run1.log` and
  `run2.log` identical): mode 3 only (frame 1958); type 8 at 2008/2009/2010;
  no type 9..14; no mode 9.

No oracle path reaches any ported block, so the enforced lines cannot move
(confirmed by `make verify` and the frame-dump comparison in the report).

## §K13.6 Tests (`test_fight.c`, called from `test_fight`)

Fixture `k13_base` (`z_nodes`: the free list, mode 3, rng seeded; slot 0's
record on pset 14 with the `0x2BE00` term `0x2000`; slot 1's on pset 15; the
three stream tables' entry 3 on plain words `K13_SW`/`K13_SH`/`K13_SI`;
`DS_00108878 = 5`) and `k13_ent` (si 3, `+0x1C = 0x05` so the trample and
case 8 stay inert, pset `k+1` term `0x1000`, x `0x1000`, y word `0x800`,
`+0x3C 0x100`, sentinels `+0x2C 0x9999`, `+0x36 0x2222`, `+0x38 0x1111`,
`+0x55 0x77`, stream `0xEEFD4`). Each check runs between `mz_save` and
`mz_restore`.

- `check_k13_case13`: the turn (`+0x29 0x41`, `+0x34 0xFF80`, `K13_SW` at
  3.0, `+0x14 = 0x2000 - rng(0xC00)`, type `0x0E`, `DS_00108878 = 0`,
  `+0x2C = 0xD80`, one draw); the other turn with a term `-0x100`; a zero
  term (`jle`: `rng(0xC00) + 0`); the kill
  (`+0x28` `0x11 -> 0x19`, nothing else, no draw); `DS_001088C7` set; `C9 =
  0xFF` against side `0xFF` (no kill: sign-extension); the on-screen wait at
  x `0x100`, `-0x300`, `0x5700` and not at `-0x301`, `0x5701`; `0x496DC` with
  count 1 on one free node (four draws, the new entry at the head, type
  `0x0E`, side 1, `+0x14 = target - rng(0xC00)`).
- `check_k13_case14`: speed 0 (nothing); `0x4A868` true at the inclusive
  edge (the stop, `+0x55 = 1`, `K13_SH`, no draw); false off screen (the
  turn, target, `+0x2C`; `DS_00108878` kept); the other turn with a zero
  term (`+0x14 = rng`); false on screen (nothing).
- `check_k13_mode9`: the latch open (2 >= 4 - 2) and shut (1 < 2); the count
  of the `DS_00104B16` side, not the entries'; the re-arm at 0; the
  countdown at `0x3C`; on a drawn round, case 3's `[ESP+4]` store (fix round
  1): `0x4000` keeps the latch shut (one draw), 0 opens it (two draws);
  `DS_001088C9 = 1` for `+0x5A 0x10`; `DS_001088B4`
  cleared; the survey on all-idle (`DS_0010885C 0x1001`, `DS_00108858
  0x3000`, `C6 0`, `C8 1`) and not on one idle of two; the scatter after an
  arrived walker, not with one still walking, not without a walker; mode 3
  runs none of it.

## §K13.7 Named gaps left

1. The drawn round's side count `[ESP+4]` (§K13.3), but only before the
   first case-3 store in the call: uninitialised own-frame memory, 0 in the
   port. Pinning it needs a runtime capture at `0x4A48E`.
2. The three voices `0x2C3FC(0xCB)` (`0x4A4B5`, `0x4A4ED`) and
   `0x2C3FC(0xDC)` (`0x4A52C`) stay `PORT:` notes (record §45-A).
3. Side bytes of 3 or more in the per-side count (§K13.3 `PORT:` bound).
