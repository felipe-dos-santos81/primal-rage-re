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
