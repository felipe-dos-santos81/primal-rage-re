# Cluster K8c (the remaining update-table entries) — raw-byte derivation (Task 3e of `2026-09-29-all-gaps.md`)

**Scope.** Ledger `2026-09-29-all-gaps-ledger.md` §B.2 (the update process
table `DS_000A8644`) and §F order 11 (K8c UPD-REST): entries 4 `0x37C8C`,
8 `0x34648` (with the Ghidra function `0x29C20`), 9 `0x3800C`, 11 `0x4F890`,
12 `0x24150` and 17 `0x45D98`. Entries 15/16 are K8b (Task 5); entry 6 is
§K8a (Task 3c). Sections are `§K8c.n`; the port headers cite them by that name.

**Tooling.** The Ghidra MCP bridge was not reachable (`ToolSearch` finds no
Ghidra tool). Every byte was read from the Python mirror of `mem_load_le` +
`mem_load_le_fixups` (`port/src/mem.c`), so each dword has the LE fixups
applied, and disassembled with capstone (32-bit), by recursive descent from
each entry (jump tables followed by hand). Data-reference scans decode every
instruction in the code object `0x10000..0x5D000` whose bytes contain the
absolute address, at every alignment. Caller scans cover every `E8` rel32
and every absolute dword in both objects. `0x29C20` was cross-checked
against `port/decomp/prage.c` (`FUN_00029c20`).

---

## §K8c.0 The table, the dispatcher, the setters, reachability

Table dwords (fixed-up image): `A8644[4] = 0x37C8C` (`0xA8654`),
`[8] = 0x34648` (`0xA8664`), `[9] = 0x3800C` (`0xA8668`), `[11] = 0x4F890`
(`0xA8670`), `[12] = 0x24150` (`0xA8674`), `[17] = 0x45D98` (`0xA8688`).
Each is the only reference to its target (no rel32 call or jump); none is a
Ghidra function, so only `0x29C20` moves the counter.

The dispatcher `0x24CD5..0x24CFC` snapshots the mask in EBX once
(`mov ebx,[0x104ae8]`), then for each set bit calls `[eax+0xA8644]` with
EAX = EDX = index*4 and keeps EDX as its loop counter. Every entry below
pushes and pops EDX and none reads EAX, so `fn()` is exact and the port's
`run_process_table` (mask passed by value) is equivalent.

**Every writer of `0x104AE8..0x104AEA`** (all instructions whose operand is
one of those addresses, any alignment; no indexed or register-based access
reaches them: the nearest indexed stores `[edi+0x104A88]`/`[esi+0x104A98]`
(`0x2572F`/`0x2577F`) are bounded by the win counts `DS_00104AF2/AF3`; no
data dword holds the addresses; the image's initial mask is 0):

| site | store | owner | effect on the K8c bits |
|---|---|---|---|
| `0x20E1C`, `0x20EC3`, `0x28DC4`, `0x2BB13`, `0x41435` | `mov dword [0x104ae8], reg` | `0x20DF4`P, `0x20EB8`, `0x28DA4`P, `0x2BAF4`P, `0x4142C` | reg = 0 in each (`xor esi,esi` `0x20E10`; `xor edx,edx` `0x20EB9`, `0x28DB1`, `0x2BB0F`, `0x4142F`): clear all |
| `0x13275`, `0x198BD`, `0x22F05`, `0x230C7`, `0x25E1D`, `0x25FD2`, `0x26B2C`, `0x27E66`, `0x29010`, `0x290C5`, `0x294C9`, `0x48D2A`, `0x48D75` | byte `AE8` | — | bits 0x01/0x04/0x20/0x40/0x80/0x02 only; **none sets 0x10** |
| `0x3426E` | `AE9 \|= 1` | `0x34168` (ported `fighter_34168`, the stun start, called by `fighter_347b8` at `0x34863`) | **sets entry 8** |
| `0x37FFC` | `AE9 \|= 2` | `0x37EA0` (**unported**, no Ghidra fn) | **sets entry 9** |
| `0x4F973` | `AE9 \|= 8` (`or ah,8` `0x4F969`) | `0x4F944` (ported `fighter_4f944`; callers `0x1467F` `fighter_1461c`, `0x149EB` `fighter_14988`, `0x39195` `fighter_39040`) | **sets entry 11** |
| `0x24142` | `AE9 \|= 0x10` (`or dl,0x10` `0x2413B`) | `0x24078` (**unported**, no Ghidra fn) | **sets entry 12** |
| `0x45D7B` | `AEA \|= 2` (`or bl,2` `0x45D72`) | `0x45D58` (**unported**, no Ghidra fn) | **sets entry 17** |
| `0x2602B/0x26036`, `0x4013C`, `0x4052C`, `0x407D8`, `0x48B45`, `0x260A6` | `AE9`/`AEA` | — | bits 0x80, 0x04, 0x20, 0x40, 0x04, `AEA` 0x01: not K8c |
| `0x24175`, `0x340B0`, `0x36F05`, `0x38023`, `0x37C9E`, `0x4F907`, `0x45FD8` (+ `0x26186`, `0x26249`, `0x40165`, `0x405FB`, `0x408DA`, `0x48B89`) | `and` | — | clears only |

**Correction to ledger §B.2 (the raw wins).** §B.2 said entry 11's setter
"was not identified (the ported `0x4F944` stores `AE9` from AH)". `0x4F944`
*is* the setter: `0x4F95C mov ah,[0x104ae9]; 0x4F969 or ah,8; 0x4F973 mov
[0x104ae9],ah`. The port's `fighter_4f944` already carries it
(`fighter.c`, `DSB(DS_00104AE9) |= 8u`).

**The three unported setters are animation-opcode targets.** Each is a dword
after a `0xD100` word (opcode 0x11) in the character streams:
`0x37EA0` at `0xD2BEA`, `0xD486C`, `0xE119C`, `0xE4566`, `0xE7932`,
`0xEB1A6`, `0xED5AA` (one per character, words at those addresses − 2);
`0x24078` at `0xE5008`; `0x45D58` at `0xEB894`. None has a rel32 caller.
`anim_indirect` (`actors.c`) resolves such targets through `fn_resolve` and
skips an unregistered one, so in the port entries 9, 12 and 17 can never be
armed. Sizes: `0x37EA0..0x3800A` 362 B (callees `0x2AE14`, `0x2B150`,
`0x2BE5C`, `0x5D7DC`, all ported; a 7-way jump table at `0x37E84`),
`0x24078..0x2414D` 213 B (`0x2BC30`, `0x2C3FC`, `0x2AE14`, `0x1A570`),
`0x45D58..0x45D84` 44 B. They are named gaps for a later cycle (§K8c.7); they
are not update-table entries and are outside this task's closure.

**Reachability probe (not committed).** A temporary `fprintf` in
`run_process_table` printed the full `DS_000A8644` mask on every change.
Positive control: entry 7's bit 0x80 (armed by `0x290C5`/`0x29010`) shows up.

- `prageport --check 8000`: masks seen `0x00000000` and `0x00000080` only
  (frames 1, 5061, 5774, 6811, 7487; mode 3 throughout).
- `PR_FRONTEND_DET` driver (the demo-fight / attract2 run; `run1`, `run2`):
  `0x00000000` and `0x00000080` only (frames 887, 4426, 4572, 4918).

So no K8c bit is armed on either no-input path, including the two with a
ported setter (8, 11): the demos reach neither a stun nor a >= 4-hit combo /
`FD114` load. Registering the six entries cannot move a frame there.

**Closure (measured before porting).**

| entry | body | new callees | setter | reach in port | verdict |
|---:|---|---|---|---|---|
| 4 `0x37C8C` | 72 B | — (`0x2AE14` P) | **none in the image** | dead (no setter; `DS_001078E0` has no writer either) | port (§K8c.1) |
| 8 `0x34648` | 155 B | `0x29C20` 59 B (`0x2A17C` P) | `0x34168` P | live in fights (stun), not armed on the no-input paths | port (§K8c.2) |
| 9 `0x3800C` | 38 B | — | `0x37EA0` U (anim target) | unreachable until `0x37EA0` is ported | port (§K8c.3) |
| 11 `0x4F890` | 178 B | — (`0x2AE14` P) | `0x4F944` P | live in fights (combo), not armed on the no-input paths | port (§K8c.4) |
| 12 `0x24150` | 85 B | — | `0x24078` U (anim target) | unreachable until `0x24078` is ported | port (§K8c.5) |
| 17 `0x45D98` | 589 B | — (`0x5D7DC`, `0x29C08`, `0x2AE14`, `0x2BC30`, `0x2C3FC`, `0x37B54` all P) | `0x45D58` U (anim target) | unreachable until `0x45D58` is ported | port (§K8c.6) |

Total 1176 B, 7 functions, no new subsystem: every entry is under the
size gate, so none is deferred.

---

## §K8c.1 Entry 4, `0x37C8C`

```
37c8c: push ebx / push ecx / push edx
37c8f: mov  edx, [0x1078e0]          ; slot pointer (no writer anywhere)
37c95: mov  ecx, [edx]               ; slot +0 = record
37c97: cmp  word [ecx+0x34], 0
37c9c: jne  0x37ca9
37c9e: and  byte [0x104ae8], 0xef    ; clear own bit
37ca5: pop edx / ecx / ebx ; ret
37ca9: mov  ax, [0xef6dc]            ; frame word
37caf: xor ah, ah ; and al, 3 ; and eax, 0xffff
37cb8: jne  0x37cd0                  ; only frames with (w & 3) == 0
37cba: push eax                      ; a5 = 0
37cbb: mov  ecx, [ecx+0x30]
37cbe: mov  edx, [edx+0x2c]          ; a2 = slot +0x2C
37cc1: xor  ebx, ebx                 ; a4 = 0
37cc3: mov  eax, 0xbb1dc             ; the landing-dust descriptor
37cc8: sar  ecx, 0x10                ; a3 = rec +0x30 >> 16 (signed)
37ccb: call 0x2ae14
37cd0: pop edx / ecx / ebx ; ret
```

Dead in the shipped game (§K8c.0: no store sets `AE8` 0x10, `DS_001078E0` is
only read, at `0x37C8F`). Ported from the raw as `fighter_37c8c`, registered.

Tests (`check_update_k8c` (b), `test_game.c`): `+0x34 = 0`, mask
`0xFFFFFFFF` → `0xFFFFFFEF`, no spawn; `+0x34 = 5`, frame `0x00010001` → no
spawn, mask `0x10` kept; frame `0x00010004` → one record with `+0x18 =
0x12345` (slot `+0x2C`), `+0x32 = 0xFFF0` (`0xFFF00000 >> 16`; `0xBB1DC`'s
`+8` word `0x0080` has no `0x2000` bit, so a3 lands in `+0x32`), `+0x1C = 0`
(the pool was filled with `0xA5` first).

## §K8c.2 Entry 8, `0x34648`, and `0x29C20`

```
34648: push ebx / push edx
3464a: xor edx,edx ; xor eax,eax
3464e: mov dx, [0xef6dc]             ; frame word (zero-extended)
34655: mov ax, [0xbdbe4]             ; W (image word = 1)
3465b: test edx, eax ; jne 0x346e0   ; frame & W -> return
34663: inc eax                       ; W + 1 (32-bit)
34664: test edx, eax ; jne 0x3468e   ; flag = (frame & (W+1)) != 0
34668: ... dl = [0x1078ff]; al = [side*0x94 + 0x10782a]; edx = 0 ; jmp 0x346b5
3468e: ... same char;                                     edx = 1
346b5: call 0x29c20                  ; EAX = char, DL = flag
346ba: mov ebx, eax                  ; handle
346bc: ... eax = [side*0x94 + 0x1077b0]   ; the side's record
346d9: xor edx, edx                  ; word 0
346db: call 0x2a17c                  ; actor_pset_palette(rec, 0, handle)
346e0: pop edx / ebx ; ret
```

`0x29C20` (59 B; Ghidra `FUN_00029c20(int param_1, char param_2)` agrees):

```
29c20: mov eax, [eax*4 + 0xa8a98]    ; row = per-character palette row
29c27: test dl, dl ; je 0x29c43
29c2b: dl = [0x105b34 + [0x1078ff]] ; mov eax, [eax + edx*4] ; ret
29c43: cmp byte [0x105b34 + [0x1078ff]], 0 ; jne 0x29c58
29c54: mov eax, [eax+4] ; ret        ; index 0 -> row[1]
29c58: mov eax, [eax]   ; ret        ; else     -> row[0]
```

The flag is DL only (`test dl,dl`); the side index is `DS_001078FF`, which
`0x34168` stores (`0x34269`). With W = 1 the entry runs on even frames and
alternates the stunned side's palette between its own (`row[idx]`) and the
other of the row's two handles every two frames, until `0x34038`/`0x36E78`
clear the bit.

Tests (c)/(d): scratch row as character 7's (`0xA8AB4`, restored),
`DS_00105B34 = {1, 2}`, side 1: `29c20(7,1) = row[2]`; `29c20(7,0x100) =
row[0]` (DL = 0, index 2); index 0 → `29c20(7,0) = row[1]`,
`29c20(7,1) = row[0]`. Entry 8 on a pool record (`+0x5F = 1`, pset `+2 =
0x1234`, `+0x18 = 0`) with row `{0, 0, H}` (H = char 0's `row[0]`, nonzero):
frame 3 → nothing; frame 4 (no flag → `row[0] = 0`) → `+2 = 0x0800`, `+0x18`
still 0; frame 2 (flag → `row[2] = H`) → `+2 = 0x0800`, `+0x18 != 0`.
The image word `0xBDBE4 = 1` is asserted.

## §K8c.3 Entry 9, `0x3800C`

```
3800c: push edx
3800d: mov eax, [0x1078d8]           ; 0x37EA0's record
38012: mov edx, [eax+0x36] ; sar edx, 0x10   ; (s16) word +0x38
38018: cmp edx, 1 ; jg 0x3802c
3801d: mov word [eax+0x38], 0
38023: and byte [0x104ae9], 0xfd
3802a: pop edx ; ret
3802c: dec word [eax+0x38]
38030: pop edx ; ret
```

Setter `0x37EA0` sets the word to `rng(0x10) + 0x20` (`0x37FD4..0x37FE9`)
and `DS_00104B0C = 1`. Tests (e): `5 → 4`; `2 → 1` (mask kept); `1 → 0`,
mask `0xFFFFFFFF → 0xFFFFFDFF`; `0x8000` (signed −32768) `→ 0`, mask
`0x200 → 0`; the word `+0x36` sentinel `0xBEEF` kept.

## §K8c.4 Entry 11, `0x4F890`

```
4f890: push ebx / ecx / edx
4f893: mov dx, [0x1088e8] ; dec edx ; mov [0x1088e8], dx
4f8a2: test dx, dx ; jg 0x4f93e      ; signed countdown
4f8ab: dl = [0x1088f0] & 1 ; and edx, 0xff ; je 0x4f8dc
4f8bc: push 0 ; ecx = 0xd8 ; eax = 0xc98c8
4f8c8: ebx = [0x9acc6] sar 16 ; edx = [0x9acc4] sar 16 ; jmp 0x4f8f0
4f8dc: ecx = 0xd8 ; eax = 0xc98dc ; ebx = [0x9acc6] ; push edx (= 0) ; sar ebx,16
4f8f0: call 0x2ae14                  ; even arm: EDX = 0 (the `and`), x = 0
4f8f5: ah = [0x1088f0] - 1 ; mov [0x1088f0], ah
4f903: test ah, ah ; ja 0x4f912
4f907: and byte [0x104ae9], 0xf7 ; ret
4f912: dec word [0x1088ea]
4f919: edx = [0x1088e8] sar 16 ; cmp edx, 4 ; jge 0x4f930   ; (s16) 0x1088EA
4f927: mov word [0x1088ea], 4
4f930: mov dx, [0x1088ea] ; mov [0x1088e8], dx
4f93e: pop ... ; ret
```

`0x9ACC4` = `40 00 00 3A 00 00 00 00`: the dword at `0x9ACC4` is
`0x3A000040` (x = `0x3A00`), at `0x9ACC6` `0x00003A00` (y = 0); read at run
time (as `attract.c` does), asserted by the test. Both descriptors' `+8`
word is `0x2000`, so the layer `0xD8` goes to `+0x49`.

Tests (f): countdown 2 → 1, no spawn; → 0 with count 3: a record at
`+0x18 = 0x3A00`, `+0x1C = 0`, `+0x49 = 0xD8`, stream in `[0xE8950,
0xE8966)` (`DSD(0xC98C8)`..`DSD(0xC98DC)`), count 2, reload `0x10 → 0x0F`,
countdown `0x0F`, mask kept; countdown 0 (→ −1) with count 2: a second record
at x 0 on the `0xC98DC` stream, count 1, reload/countdown `0x0E`; reload 4
→ 3 → clamped 4; count 1 → 0: mask `→ 0xFFFFF7FF`, reload `0x77` and
countdown 0 untouched after the clear.

## §K8c.5 Entry 12, `0x24150`

```
24150: push ebx / ecx / edx
24153: mov dl, [0x104770] ; mov eax, [0x104744]
2415e: cmp dword [eax+0x1c], 0 ; jne 0x2419b
24164: inc dl ; xor ecx,ecx ; mov cl, dl ; cmp ecx, 4 ; jl 0x2417e
2416f: mov word [eax+0x34], 0
24175: and byte [0x104ae9], 0xef ; jmp 0x2419b
2417e: ebx = 0x100 sar cl ; mov [eax+0x36], bx
24189: bx = [eax+0x34] ; mov word [eax+0x44], 0xa ; sar bx, 2 ; mov [eax+0x34], bx
2419b: mov [0x104770], dl
241a1: pop ... ; ret
```

Setter `0x24078` spawns `0xA84FC` into `DS_00104744`, sets its `+0x34` to
`±0x80` by `0x1A570`, `+0x36 = 0`, `+0x44 = 0xA`, `+0x59 = 0xFE` and the
step byte `DS_00104770 = 0`. Tests (g): `+0x1C = 0x00010000` (a dword test;
a word test would proceed) keeps step 2 and `+0x34`; step `0 → 1`: `+0x36 =
0x80`, `+0x44 = 0xA`, `+0x34 0xFF80 → 0xFFE0` (signed), mask kept; step
`3 → 4`: `+0x34 = 0`, `+0x36` kept, mask `→ 0xFFFFEFFF`; step `0xFF → 0`
(byte wrap): `+0x36 = 0x100`, `+0x34 0x10 → 4`.

## §K8c.6 Entry 17, `0x45D98`

Twelve 8-byte entries at `0x108180` (`+0` record, `+4` state, `+5` timer),
ESI = 0..0x58 step 8; the state is switched through the 5-dword table at
`0x45D84` = `{0x45DBA, 0x45E63, 0x45E90, 0x45F34, 0x45F91}` after `cmp
al,4; ja 0x45F91`.

- **0** (`0x45DBA`): `rng(0xA)`; nonzero → next. Else `s = DS_00104AD4`;
  x = slot(s)`+0x2C − 0x400 + rng(0x800)` (the slot word read before the
  draw), y = slot(s)`+0x30 + rng(0x100)`; `DSD(0xC9384)` (the descriptor
  `0xC9374`'s `+0x10`) = `0x29C08(rec(s)+0x51, slot(s)+0x7A)`;
  `0x2AE14(0xC9374, x, 0, y, 0)` → entry record; `+0x36 = 0x200`; state 1;
  voice `0xAB` (`0x45E59` → `0x45F8C call 0x2C3FC`).
  `0x5D7DC` pushes/pops EBX/EDX (`0x5D7DC..0x5D805`), so the values held
  across the draws are exact.
- **1** (`0x45E63`): `cmp dword [rec+0x1c], 0x3a00; jl` (signed) → next;
  else `+0x36 = 0`, timer `0x1E`, state 2.
- **2** (`0x45E90`): `dec` timer; `test dh,dh; jg` (signed) → next; else
  `0x2BC30(rec, 0xEB8BE, 1.0)`; `o = DS_001078FD`; `+0x18 = slot(o)+0x2C −
  0x800 + rng(0x1000)`; `+0x32 = (rec(o)+0x30 >> 16) − 0x100 + rng(0x200)`
  (word); `+0x36 = 0xFC00`; state 3.
- **3** (`0x45F34`): `(s16)+0x36 + +0x1C` (`[eax+0x34] sar 16` + dword)
  `> 0` → next; else `0x2BC30(rec, 0xEB8C2, 3.0)`, `+0x1C = 0`, `+0x36 = 0`,
  state 4, `DS_001081EE += 1`, voice `0xAC`.
- **4** and above: next.

After the loop: `mov eax,[0x1081eb]; sar eax,0x18` (the signed byte
`0x1081EE`) `== 0xC` → `DS_00104AD4`'s record `+0x24 = 3.0`, `0x37B54(rec)`
(its other side's slot record `+0x53 = 1`), `AEA &= 0xFD`.

Setter `0x45D58`: zeroes the twelve state bytes (`0x45D5C..0x45D6A`, stores
at `0x108184 + 0..0x58`), `DS_001081EE = 0`, `AEA |= 2`.

The two voices follow `fighter.c`'s convention and stay `PORT:` notes (record
§45-A: the fighter-path `0x2C3FC` voices are not wired). `fighter_37b54`
(`actors.c`) is exported for the tail call.

Tests (h), entry index 5 (`+0x28`), the other eleven at state 4; slot 0 =
`DS_00104AD4` (record side byte 1, char 3, x `0x50000`, y `0x2000`), slot 1
= `DS_001078FD` (x `0x60000`, record y `0x00300000`); `DS_00105B34 = {0, 1}`.
State 5 is skipped (record sentinel kept). State 0 with a seed whose first
`rng(0xA)` is nonzero: state kept, no spawn, the RNG advanced by exactly one
draw. With a seed whose first draw is 0: the replayed draws give `+0x18 =
0x50000 − 0x400 + r1`, `+0x1C = 0x2000 + r2`, `+0x32 = 0`, `+0x36 = 0x200`,
state 1, record = the list head, `DSD(0xC9384)` = char 3's `row[1]`
(side byte 1, not slot index 0; the test asserts the two handles differ).
State 1: `0x80000000` and `0x39FF` wait; `0x3A00` → state 2, timer `0x1E`,
`+0x36 = 0`. State 2: timer 2 → 1 waits; timer 1 → state 3, `+0x24 =
0x3F800000`, `+0x18 = 0x60000 − 0x800 + r3`, `+0x32 = 0x30 − 0x100 + r4`,
`+0x36 = 0xFC00`. State 3: `−16 + 0x11 > 0` waits (count `0x0B` kept, mask
kept); `−16 + 0x10 = 0` lands: `+0x24 = 3.0`, `+0x1C = 0`, `+0x36 = 0`,
state 4, count `0x0C`, then the tail: slot 0's record `+0x24 = 0x40400000`,
`DS_001077A8[0]`'s record `+0x53 = 1`, mask `→ 0xFFFDFFFF`.

## §K8c.7 Named gaps left by this cycle

1. **Entry 4 is dead code in the shipped binary**: no store sets
   `DS_00104AE8` 0x10 and nothing writes `DS_001078E0` (§K8c.0). Ported and
   registered for completeness; no path can run it.
2. **The setters of entries 9, 12 and 17 are unported animation-opcode
   targets**: `0x37EA0` (362 B; 7 streams), `0x24078` (213 B; `0xE5008`),
   `0x45D58` (44 B; `0xEB894`). Until they are registered the three entries
   are ported but never armed in the port. A small follow-up cycle (≈ 620 B,
   all callees ported, the voices excepted) would close them; they change
   fight rendering when their streams run, so that cycle needs its own
   before/after dumps.
3. The voices `0x2C3FC(0xAB)` / `(0xAC)` in entry 17 (record §45-A).

## §K8c.8 Carried minors (brief items a–c)

- (a) `fighter.c`'s `0x467DC` header now reads `/* 0x467DC — record §K1.4
  (2026-09-29-k1-k9-derivations.md)`.
- (b) `test_game.c`'s §K3.2 block in `test_flow` moved above the unrelated
  "A mode other than 3 …" comment, assertions unchanged.
- (c) `flow.c`'s `SND_SLOT0_63` and `test_game.c`'s `VW_SLOT63` now use
  `DS_00107813` (`symbols.h:973`); values unchanged.
