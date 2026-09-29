# E-wire, K8b and K8d — raw-byte derivation (Task 5a of `2026-09-29-all-gaps.md`)

**Scope.** Ledger `2026-09-29-all-gaps-ledger.md` §E rows 3, 4 and 19, §F
orders 13 (E-WIRE) and 15 (K8b UPD-BONUS), §B.2 entries 9/12/15/16/17, and the
Task 3e follow-ups: the animation-opcode setters `0x37EA0`/`0x24078`/`0x45D58`
(record `2026-09-29-k8c-derivations.md` §K8c.7, here "K8d"), the signed clamp
of `0x4F944` and four carried minors. Sections are `§W`, `§B8`, `§D8`, `§C` and
`§M`; the port headers cite them.

**Tooling.** The Ghidra MCP bridge was not reachable (`ToolSearch "ghidra"`
finds no tool). Every byte was read from the Python mirror of `mem_load_le` +
`mem_load_le_fixups` (`port/src/mem.c`), so each dword has the LE fixups
applied, and disassembled with capstone (32-bit). Caller scans cover every
`E8`/`E9` rel32 in `0x10000..0x73B14` and every absolute dword in both objects;
data-reference scans decode every instruction whose bytes contain the address.
Immediates were checked against the raw file (obj-0 file offset = VA +
`0x52E54`) to tell fixed-up data addresses from constants.

---

## §W E-3 / E-19: mode 0x33 calls `0x4DEF4`

```
29638: push ecx / push edx
2963a: mov eax,[0x1077e4] ; mov [0x1077e8],eax
29644: mov eax,[0x107878] ; mov [0x10787c],eax
2964e: xor eax,eax ; call 0x35658          ; fight_hud_pass(0)
29655: mov eax,1   ; call 0x35658          ; fight_hud_pass(1)
2965f: call 0x4def4                        ; fight_effects_idle_pass
29664: call 0x12da8                        ; camera_y_commit
29669: mov ah,[0x104aec] ; mov dx,[0x104afe] ; or ah,2 ; dec edx
2967a: mov [0x104aec],ah ; mov [0x104afe],dx ; test dx,dx ; jg 0x296b5
2968c: eax = 0x2b ; ecx = 0x17 ; xor dl,dl ; call 0x2c3fc
2969d: mov [0x104b25],dl ; edx = 0x25ae8 ; mov [0x104b00],cx ; mov [0x104ae4],edx
296b5: pop edx / pop ecx ; ret
```

The call at `0x2965F` takes no register argument: `0x4DEF4` pushes EBX..EDI
and reloads everything from memory (record §49-F; the port's
`fight_effects_idle_pass()` takes none). It sits between the second
`fight_hud_pass` and `camera_y_commit`, exactly as mode 0xF's `0x277E9`. The
`DS_00104AEC` read at `0x29669` follows it, so the tail order in the port is
already right. **Verdict: wire it** (`flow.c`, `game_mode_33_step`); the stale
named-gap comment is replaced.

**E-19.** `0x4A868`'s six sites (record §K4): `0x4BFDE` and the four `0x4DEF4`
states are wired (Task 3d). With `0x2965F` wired, both raw callers of
`0x4DEF4` (`0x277E9`, `0x2965F`) now call it, so the "not wired yet (ledger
§E-3)" notes in `fight.c`/`fight.h`/`flow.h` are rewritten. The one remaining
unwired site is `0x4A361` (the case-14 body `0x4A346..0x4A412`, `fight.c`
`0x49C78`), blocked on K13 (Task 5b).

Test (`check_mode_33` (d), `test_fight.c`): `DS_001088B0 = 0x0100`,
`DS_001088BB = 0` (no re-arm, no rng draw; `mz_seed`'s effect entry is type 8,
which the walker skips) → `0x00FF` after one `game_mode_33_step`, flag kept,
`DS_00104AFE` 5 → 4. Mutations: the call removed (`0x0100`) or doubled
(`0x00FE`) both fail. The order against `camera_y_commit` is taken from the
raw; the two touch disjoint state, so no test can see it.

Reachability: see §R (the probe).

## §B8 K8b: update entries 15 `0x260BC` and 16 `0x26194` (the bonus cards)

Table dwords: `A8644[15] = 0x260BC` (`0xA8680`), `[16] = 0x26194`
(`0xA8684`). Each is the only reference to its target (no rel32 caller); no
Ghidra function (not in `symbols.h`), so the counter does not move.

Setters (every writer of the bits, record §K8c.0's scan): `AE9 |= 0x80` only at
`0x26036` in `0x25FDC` (`flow_round_bonus_a`); `AEA |= 1` only at `0x260A6` in
`0x2604C` (`flow_round_bonus_b`). Clears: `0x26186` (entry 15 itself), `0x26249`
(entry 16 itself) and the whole-mask zero stores. `0x25FDC` is called from
`0x27CC7`/`0x27D67`, `0x2604C` from `0x27CFD`/`0x27DA0` (all in `0x27C48`,
`flow_round_winner`) and from `0x2613E` (entry 15, below).

State bytes (every access, data-reference scan): `DS_00104B0D` (entry 15's
state) is written only at `0x26025` (`0x25FDC`: 0), `0x26111` (1), `0x26154`
(2) and read at `0x260BF`; `DS_00104B0E` (its timer) only at `0x26102`,
`0x2611B`, `0x26123`; `DS_00104B0F`/`DS_00104B10` likewise only by `0x2604C`
and entry 16. `DS_00104AB4`/`DS_00104AB0` (the card records) only by the two
spawns and the two entries. `DS_00104B1C` is written by `0x27C48` (`0x27C55`
= 0xFF, `0x27CDF`, `0x27D79`) and read only at `0x2612D`.

### §B8.1 Entry 15, `0x260BC`

```
260bc: push ebx / ecx / edx
260bf: mov al,[0x104b0d] ; cmp al,1 ; jb 0x260d6 ; jbe 0x2611b
260ca: cmp al,2 ; je 0x26163 ; (else) pop ; ret
260d6: test al,al ; jne 0x2618d           ; (never taken: al == 0 here)
260de: eax = [0x104ab4] ; mov cx,[eax+0x2c] ; xor edx,edx ; add ecx,0x80
260ef: mov dx,cx ; mov [eax+0x2c],cx ; cmp edx,0x1000 ; jl 0x2618d
26102: mov byte [0x104b0e],0x3c ; ch = 1 ; mov word [eax+0x2c],0x1000
26111: mov [0x104b0d],ch ; ret
2611b: dl = [0x104b0e] - 1 ; mov [0x104b0e],dl ; test dl,dl ; jg 0x2618d
2612d: cmp byte [0x104b1c],0 ; jl 0x26143
26136: mov eax,[0x104b19] ; sar eax,0x18 ; call 0x2604c
26143: push 0x3f800000 ; bh = 2 ; edx = 0xe91a4 ; eax = [0x104ab4]
26154: mov [0x104b0d],bh ; call 0x2bc30 ; ret
26163: eax = [0x104ab4] ; mov dx,[eax+0x2c] ; sub edx,0x200 ; mov [eax+0x2c],dx
26176: test dx,dx ; ja 0x2618d             ; (test clears CF: ja == nonzero)
2617b: mov word [eax+0x2c],0 ; call 0x2b150 ; and byte [0x104ae9],0x7f
2618d: pop edx / ecx / ebx ; ret
```

- State 0: the card record's word `+0x2C` gains 0x80 (16-bit store; the
  `movzx`-style `mov dx,cx` after `xor edx,edx` compares the zero-extended
  word, so 0xFFC0 wraps to 0x0040 and does not clamp). At or above 0x1000 it is
  clamped to 0x1000, the timer `DS_00104B0E` = 0x3C and the state = 1.
- State 1: the timer byte decrements; while it is still positive (signed
  `jg`) nothing else. At or below 0: when the signed byte `DS_00104B1C` is
  not negative, `0x2604C((s8)DS_00104B1C)` (the top byte of the dword at
  `0x104B19`, `sar 0x18`) spawns the second card; then the state = 2 and
  `0x2BC30(DS_00104AB4 (re-read), 0xE91A4, 1.0)`.
- State 2: the word drops by 0x200; while non-zero nothing else. At zero:
  the word = 0, `0x2B150(rec)` (actor_set_dead) and `AE9 &= 0x7F`.
- Any state above 2: nothing.

`0xE91A4` is a fixed-up data address (raw file bytes `a4 91 06 00`, image
`a4 91 0e 00`). `0x2BC30` is `actors_anim_begin`, `0x2B150` `actor_set_dead`.
EAX (the dispatch index) is overwritten, EBX/ECX/EDX pushed and popped:
`fn()` is exact.

### §B8.2 Entry 16, `0x26194`

The same machine on `DS_00104B0F` (state), `DS_00104B10` (timer),
`DS_00104AB0` (record) and `AEA` bit 0x01; state 1 has no `DS_00104B1C` chain,
its stream is `0xE91D4` (fixed up, raw `d4 91 06 00`) at 1.0; state 2 ends with
`AEA &= 0xFE` (`0x26249`). Addresses: `0x26197` read, `0x261B6..0x261F3`
state 0, `0x261F4..0x26225` state 1, `0x26226..0x2624F` state 2.

### §B8.3 Tests (`check_bonus_cards`, `test_game.c`)

Pool records (`+0x56` = their index) serve as the two cards and as a
reference record: the expected stream cursor after `0x2BC30` is the one the
reference reaches when begun on the same stream (both streams open with
command words the begin walks). `DS_001077A8[0/1]` point at scratch camera
targets (`+0x3C` 0x2000 / 0x1000) and the mode word is 0x30, so
`0x41310(side)` shows which side the chain passed.

- Registration: `A8644[15] = 0x260BC`, `[16] = 0x26194`, both resolve.
- Entry 15, state 0: `0x0010 → 0x0090` (state 0 and timer 0x77 kept, mask
  kept); `0xFFC0 → 0x0040` (wraps, no clamp); `0x7FC0 → 0x8040` clamps to
  0x1000 with state 1 (a signed compare would not); `0x0F81 → 0x1001` clamps:
  word 0x1000, timer 0x3C, state 1.
- State 1: timer 2 → 1 waits (card stream sentinel kept). Timer 1 → 0 with
  `DS_00104B1C = 0xFF`: state 2, stream = the reference's, `+0x24 =
  0x3F800000`, `DS_00104AB0` sentinel kept. Timer 0x81 (signed −127, fires)
  with `DS_00104B1C = 1`: `0x2604C(1)` runs (`DS_00104B0F` 0x77 → 0,
  `AEA` 0 → 1, `DS_00104AB0` = the new list head, side 1's `+0x3C` 0x1000 →
  0x5E20, side 0's kept at 0x2000), state 2, stream set.
- State 2: `0x0400 → 0x0200` (alive, mask kept); `0x0100 → 0xFF00`
  (non-zero, alive); `0x0200 → 0`: dead bit set, mask `0xFFFFFFFF →
  0xFFFF7FFF`, state still 2.
- State 3: word, timer and state kept.
- Entry 16: the same state-0 cases; state 1 waits at 2 → 1, fires at 0x81
  with `DS_00104B1C = 1` but no chain (`DS_00104AB4` sentinel and
  `DS_00104B0D` 0x77 kept), stream = the `0xE91D4` reference, 1.0; state 2
  `0x0100 → 0xFF00` alive, `0x0200 → 0` dead, mask `→ 0xFFFEFFFF`; state 3
  kept.

## §D8 K8d: the animation-opcode setters

Each is the dword after a `0xD100` word (opcode 0x11) in a character stream;
none has a rel32 caller or another reference (§K8c.0). `anim_indirect`
(`actors.c`) calls a registered target as `fn(rec, arg)`; each raw target
takes EAX = the record and pushes/overwrites or ignores EDX, so the port's
wrappers drop `arg`.

| target | stream words (the `0xD100` word at − 2) | size | callees (all ported) |
|---|---|---:|---|
| `0x37EA0` | `0xD2BEA`, `0xD486C`, `0xE119C`, `0xE4566`, `0xE7932`, `0xEB1A6`, `0xED5AA` | 362 B | `0x2AE14`, `0x2B150`, `0x2BE5C`, `0x5D7DC` |
| `0x24078` | `0xE5008` | 213 B | `0x2BC30`, `0x2C3FC` (×2), `0x2AE14`, `0x1A570` |
| `0x45D58` | `0xEB894` | 44 B | — |

### §D8.1 `0x37EA0`

```
37ea0: push ebx/ecx/edx/esi/edi ; sub esp,0x14 ; mov esi,eax
37eaa: dl = [eax+0x51] ; al = [side*0x94 + 0x10782a] ; cmp al,6 ; ja 0x37ef1
37ecd: jmp [eax*4 + 0x37e84]    ; {0x37EF1,0x37ED5,0x37EDC,0x37EE3,0x37EEA,0x37EF1,0x37ED5}
       37ed5: 0x46BA  37edc: 0x46B8  37ee3: 0x46B6  37eea: 0x46B7  37ef1: 0x46B9
37ef6: stack descriptor: [0] = id & 0xFFFF, [4] = [5] = 0 (AH), [6] = 0 (BX),
       [8] = 0x2A00, [0xA] = 0x80, [0xC] = 0x1000, [0x10] = 0x0105FEBC
37f38: edi = [0x1014ec] + (u16)[esi+0x56] * 0x20        ; the record's pset
37f49: a5 = ([edi] & 0x8000) ? 0x4000 : 0
37f62: a4 = [edi+8] ; a2 = [edi+4] ; a3 = (u16)[edi+0xe] ; call 0x2ae14
37f78: new pset = [0x1014ec] + (u16)[new+0x56]*0x20 ; its +4/+8 = [edi+4]/[edi+8]
37f95: new +0x18/+0x1c = rec's ; word +0x32 = rec's ; word +0x28 = rec's
37fb1: new byte +0x29 |= 0x28 ; DS_001078D8 = new
37fbf: call 0x2b150 (EAX = rec)                         ; the caller dies
37fc6: [0x1078d8] byte +0x29 |= 0x10 ; call 0x2be5c (EAX = it)
37fd4: call 0x5d7dc(0x10) ; word [[0x1078d8]+0x38] = r + 0x20
37fed: DS_00104B0C = 1 ; AE9 |= 2 ; ret
```

Character → sprite: 0 `0x46B9`, 1 `0x46BA`, 2 `0x46B8`, 3 `0x46B6`, 4
`0x46B7`, 5 `0x46B9`, 6 `0x46BA`, above 6 `0x46B9` (`ja`). `0x0105FEBC` is a
constant (raw file bytes `bc fe 05 01` equal the image: no fixup), a resource
handle `index << 23 | offset` (index 2, offset `0x5FEBC`), the same form as
`0xA84FC`'s `0x0105FF3C` and record §31's `0x1F874610`. The descriptor word
`+0xE` is never written (stack garbage) and never read by `0x2AE14` (the same
note as `fight_char_team_tag_add`, record §48-S). The raw does not test the
spawn's return (neither does the port). `DS_00104B0C` is read by `0x274FC`
(mode 0xD, `0x27562`) and `0x296B8` (mode 0x32, `0x2971E`): arming it changes
those modes' flow when the stream runs, which is why §R probes it.

### §D8.2 `0x24078`

```
24078: push ebx/ecx/edx/esi ; mov esi,eax ; mov byte [eax+0x59],0xfd
24082: al = [eax+0x51] ^ 1 ; ebx = [DS_001077A8 + al*4] ; test ; je 0x24148
2409b: dl = [ebx+0x7a] ; eax = [ebx] ; edx = [0xa8424 + dl*4] ; push 3.0 ; call 0x2bc30
240b3: call 0x2c3fc(0x6b) ; call 0x2c3fc((u16)[0xbe008 + [ebx+0x7a]*2])
240d4: push 0 ; ecx = [ebx] ; eax = 0xa84fc ; edx = [ebx+0x2c]
240e0: ecx = [ecx+0x30] ; ebx = 0x1000 ; sar ecx,0x10 ; call 0x2ae14 ; [0x104744] = eax
240f5: al = [esi+0x51] ; call 0x1a570 ; nonzero: word +0x34 = 0xFF80, else 0x0080
2411b: +0x36 = 0 ; DS_00104770 = 0 ; +0x44 = 0xA ; +0x59 = 0xFE ; AE9 |= 0x10
```

`0xA84FC` is fixed up (raw `fc 84 02 00`). `0xA8424[c]` is the other side's
character stream table (`0xE798E, 0xE45C2, 0xED606, 0xD2C46, 0xEB202, 0xD48C8,
0xE11F8`). The two voices follow `fighter.c`'s convention (record §45-A: the
fighter-path `0x2C3FC` voices are not wired); `0x2C3FC` keeps EBX, which the
raw reuses at `0x240BF`.

### §D8.3 `0x45D58`

```
45d58: push ebx/edx ; xor eax,eax
45d5c: add eax,8 ; xor dl,dl ; mov [eax+0x10817c],dl ; cmp eax,0x60 ; jne 45d5c
45d6c: bl = [0x104aea] | 2 ; mov [0x1081ee],dl ; mov [0x104aea],bl ; ret
```

The twelve state bytes `0x108184 + 8i` (i = 0..11) and `DS_001081EE` become 0;
`AEA |= 2`. EAX is not read.

### §D8.4 Tests (`check_anim_setters`, `test_game.c`)

The palette table (`DS_00107618`, 0x180 bytes), the fighter slots
(`0x1077A0`, 0x160), `0x108180` (0x70) and `DS_001088E8` (4) are saved and
restored around the function.

- Registration: `fn_resolve(0x37EA0/0x24078/0x45D58)` is non-null.
- `0x45D58`: `0x108180..0x1081EF` = 0x55 with the twelve state bytes 0x77 →
  the twelve are 0, the `+0`/`+5` bytes and `0x1081E4` stay 0x55,
  `DS_001081EE` 0x33 → 0, mask `0 → 0x00020000`.
- `0x24078`, the other side's `DS_001077A8` entry 0: only the caller's
  `+0x59 = 0xFD`; `DS_00104744`, the step byte and the mask are kept.
- `0x24078` with the entry pointing at a scratch slot (record `orec`, char 3,
  `+0x2C = 0x12340`, the record's `+0x30 = 0x00450000`), run twice with the
  caller's pset word 0 bit 15 clear then set (slot 1's record is `orec`,
  whose begin leaves bit 15 clear, so the other-side predicate is caught):
  the spawn is `DS_00104744` = the list head, `+0x18 = 0x12340`, `+0x32 =
  0x45`, `+0x1C = 0x1000`, `+0x34 = 0xFF80` then `0x0080`, `+0x36 = 0`,
  `+0x44 = 0xA`, `+0x59 = 0xFE`, the caller's `+0x59 = 0xFD`, step 0, mask
  `0x1000`; `orec` begins `0xA8424[3] = 0xD2C46` (asserted) at 3.0 (the
  cursor equals a reference begun on it).
- `0x37EA0`, side 1, char 3, the caller's pset word 0 `0x8123` (bit 15), pset
  `+4 = 0x11111`, `+8 = 0x22222`, `+0xE = 0xD0`; record `+0x28 = 0x2102`,
  `+0x32 = 0x1357`; `DS_000F0AF0 = 0x700000`, `DS_000F0AEC = 0x1000`; seed
  0x4321. The spawn is `DS_001078D8` = the list head: pset word 0 `0xC6B6`,
  pset `+4/+8` copied, layer `+0x49 = 0xD0`, the palette entry's handle
  `0x0105FEBC`, `+0x32 = 0x1357`, `+0x28 = 0x02`, `+0x29 = 0x19` (`0x21 |
  0x28 | 0x10`, then `0x2BE5C` clears 0x20), `+0x18` by `0x2BE5C`'s bit-12
  arm (`pset+4 + ((s32)+0x44 >> 16) * 2 − 0x2A00`, not the `DS_000F0AF0`
  form), `+0x1C = 0x1000 + 0x3BC0 − 0x22222 − ((s32)+0x30 >> 16)`, `+0x38 =
  rng(0x10) + 0x20` (replayed), the caller's dead bit, `DS_00104B0C` 0x77 →
  1, mask `0x200`.
- `0x37EA0` for chars 0..7 with a5 = 0: pset word 0 = `0x46B9, 0x46BA,
  0x46B8, 0x46B6, 0x46B7, 0x46B9, 0x46BA, 0x46B9`.

## §C `0x4F944`'s clamp is signed

```
4f944: push ebx / push edx
4f946: cmp eax,0x14 ; jle 0x4f950 ; mov eax,0x14
4f950: ebx = 0x10 ; mov [0x1088f0],al (0x4F955) ; xor edx,edx
4f95c: mov ah,[0x104ae9] ; mov [0x1088e8],dx (0x4F962) ; or ah,8 (0x4F969)
4f96c: mov [0x1088ea],bx ; mov [0x104ae9],ah (0x4F973) ; pop ; ret
```

`jle` is a signed compare: a value at or below 0x14 as an s32 (every negative
value included) is stored as is; the port's `v > 0x14u` clamped negatives to
0x14. The raw callers pass 1 (`0x1467F`, `0x149EB`) or a round count >= 4
(`0x39195`), so no shipped path passes a negative; the fix is for fidelity.
The store addresses in the port's comments are corrected (`0x4F955`,
`0x4F962`, `0x4F96C`, `0x4F969`/`0x4F973`). Test: `v = 0xFFFFFFFF` → the
byte 0xFF (unsigned would give 0x14); `0x80000000` → 0x00; `0x15` → 0x14;
`0x14` → 0x14; each with the countdown 0, the reload 0x10 and mask `0x800`.

## §M Carried minors (Task 3e review)

- `fighter.c`: `UPD11_ROUND_HI` duplicated `FIGHT_ROUND_HI` (both
  `0x001088EA`); the entry-11 port now uses `FIGHT_ROUND_HI`.
- `fighter_45d98` state 3: the raw adds in 32 bits (`0x45F43 add edx,ebx`,
  wrapping) and tests the sign of the result (`test edx,edx; jg`); the port now
  adds as `u32` and casts, removing the C signed-overflow case (the existing
  §K8c.6 assertions cover it; two mutations of the compare fail them).
- `fighter_29c20` already carries `/* 0x29C20 — record §K8c.2`; no change.
- `test_game.c` `check_update_k8c` (d): entry 8 acquires char 0's palette into
  the palette table `DS_00107618` and never released it; the table is now
  saved before (d) and restored after it. No assertion changes.
- The headers of entries 9, 12 and 17 (`fighter_3800c`, `fighter_24150`,
  `fighter_45d98`) said "unported setter … the port never sets it"; they now
  name the §D8 ports. `check_update_k8c`'s comment likewise.

## §R Reachability probe (not committed)

A temporary `fprintf` (applied and reverted by a script; `git status` clean
afterwards) printed: the update mask `DS_000A8644` on every change (positive
control: entry 7's 0x80), every `game_mode_33_step` call, every
`flow_round_bonus_a/_b` call, the first three `anim_indirect` targets
(positive control) and every `anim_indirect` whose `DS_00105BD4` is
`0x37EA0`/`0x24078`/`0x45D58` (registered or not).

- `prageport --check 8000`: masks `0` (frame 1), `0x80` (5061), `0` (5774),
  `0x80` (6811), `0` (7487); anim targets `0x10FA8` (341), `0x12720` (1740),
  `0x35938` (1960). No mode 0x33, no bonus call, no §D8 target.
- `PR_FRONTEND_DET` (the demo-fight / attract2 driver; `run1`/`run2`
  identical): masks `0` (887), `0x80` (4426), `0` (4572), `0x80` (4918); anim
  targets `0x12720` (1740), `0x39A34` (1972), `0x36870` (1986). No mode 0x33,
  no bonus call, no §D8 target.

So none of the four changes is reached on a no-input oracle path: the E-3 wiring
runs only in mode 0x33 (a match end), the bonus cards only after a decided round
outside `DS_00104B1D` 2/3 (card A when the winner's `+0x5A` is 0, card B when it
is not and the timer byte `DS_001088F2` is at least 0x32, or through card A's
chain),
and the §D8 targets only when their character streams run. The oracle lines,
the 8000-frame dump and the front-end dump are unchanged (report). Entries 8,
9, 11, 12, 15, 16, 17 are live in real play; their correctness rests on the
unit tests and the raw.

## §N Named gaps left

1. The voices of `0x24078` (`0x2C3FC(0x6B)`, `0x2C3FC(word[0xBE008 +
   char*2])`) stay `PORT:` notes (record §45-A), like every fighter-path voice.
2. `0x4A361` (case-14 gate) stays unwired until K13 ports the case-14 body
   (Task 5b).
3. Equivalent mutant: dropping `0x37EA0`'s copy of the pset `+8` (`0x37F8F`)
   is unobservable, because the spawn's layer-mode `pset_write` (`0x2A820`)
   already wrote pset `+8` = a4 = the same old pset `+8`. The copy is kept for
   fidelity.
