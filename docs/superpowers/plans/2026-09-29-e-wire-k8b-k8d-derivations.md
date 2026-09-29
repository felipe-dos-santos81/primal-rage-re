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

