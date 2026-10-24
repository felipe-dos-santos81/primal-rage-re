# Endgame Truth — derivation record

Plan `docs/superpowers/plans/2026-10-23-endgame-truth.md`, spec
`docs/superpowers/specs/2026-10-23-endgame-truth-design.md`.

**§E1 is evidence only** (Task 1). It re-derives, from the raw bytes and the
existing captures, every value Tasks 2–11 use: the seven-character ending map
and the exact titles, the U16 finisher gate/entries/reactions and the S fields
that do and do not carry them, the U10 `b0c` proof, the U14 and U13 facts, the
5120 band and the INT 8 tick vector. It adds no code, no capture, no pin and no
allowance. **The record wins over the spec and the plan on any conflict**; every
correction found while re-deriving is stated in its section and repeated in
"What this settles".

## Sources and conventions

| Source | What it gives |
|---|---|
| `port/decomp/prage.c`, `prage.functions.csv` | the fixup-applied decompilation (data addresses are linear) |
| the fixup-applied flat image, `./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/opencode/e1img` | the loaded image at its own addresses (base `0x10000`); every byte quoted "image" below is from it |
| `data/game/C/PRAGE.EXE` | the raw file: code at file offset `VA + 0x52E54`, data at `VA + 0x46E54`; **its code immediates are pre-fixup** |
| `data/game/C/ENGLISH.TXT` | the text (6953 bytes) |
| `data/k11-captures/gp-u10-ending`, `gp-u13-ending2` | the U10 and U13 captures (read-only) |
| `/tmp/pr_gp_failed/gp-u14-win-live-run2` | the failed U14 retry artifact (kept from `2026-10-20-gameplay-truth-ii` §J6) |

Address model (`AGENTS.md`): Ghidra address == linear address == object base +
offset. `tools/le_info.py data/game/C/PRAGE.EXE` reports **object 0 base
`0x10000` (virt `0x63B15`), object 1 base `0x80000` (virt `0x8B0D0`)**. A raw
code immediate that names code (`0x108E4`, `0x5627E`) is that object's offset:
the loader adds `0x10000`. A raw immediate that names data (`0x84AD4`,
`0x8782A`, `0x28090`) is pre-fixup and the loader adds `0x80000`. The
fixup-applied image proves both: raw `0x5627E` → image `0x0006627E`
(`0x6651E`), raw `0x28090` → image `0xA8090` (`0x209F9`).

Instruction bytes below are the loaded (fixup-applied) bytes unless a line says
"raw"; a raw instruction differs from the loaded one only in its
code/data displacement, and the affected lines give both.

"Measured" is a byte or a capture field read at a cited address; "inferred" is
arithmetic or a standard constant on top of measured values. Nothing below is
fitted.

Re-check commands:

```bash
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/opencode/e1img
python3 tools/le_info.py data/game/C/PRAGE.EXE
# code bytes: disassemble  data/game/C/PRAGE.EXE[VA + 0x52E54 : ...] at VA
# data bytes: read        data/game/C/PRAGE.EXE[VA + 0x46E54 : ...] (pre-fixup)
make gp-replay scenario=gp-u13-ending2 GP_OPTIONAL=1   # -> /tmp/pr_gp_dump/gp-u13-ending2 (4940 .ipx)
```

---

## §E1.1 The seven-character ending map

### The code path (mode 0x1F)

The handler is `FUN_000208f8` (`0x208F8`, 785 bytes, one caller). Measured
instruction addresses (raw bytes disassembled at `VA + 0x52E54`):

```text
0x208F8  53 51 52 56 57 83 ec 04      push ebx/ecx/edx/esi/edi; sub esp,4
0x20900  8b 15 d4 4a 08 00            mov  edx,[0x84AD4]      -> DS_00104AD4 (the match-result side)
0x20906  8d 04 d5 00 00 00 00         lea  eax,[edx*8]
0x2090d  01 d0                        add  eax,edx            edx*9
0x2090f  c1 e0 02                     shl  eax,2              edx*36
0x20912  01 d0                        add  eax,edx            edx*37 = edx*0x94/4
0x20914  8a 04 85 2a 78 08 00         mov  al,[eax*4+0x0782A] -> linear 0x10782A
0x2091b  8a 15 25 4b 08 00            mov  dl,[0x84B25]       -> DS_00104B25 (state)
0x20921  88 04 24                     mov  [esp],al           the charid, kept for every state
0x20924  80 fa 04                     cmp  dl,4
0x20927  0f 87 d3 02 00 00            ja   0x20C00             state > 4 is a no-op
0x20931  2e ff 24 85 e4 08 01 00      jmp  dword cs:[eax*4+0x108E4]  -> jump table 0x208E4
```

So **the character id is `DS_0010782A[DS_00104AD4 * 0x94]`**, read by
`0x20900..0x2091A` (the load instruction is `0x20914..0x2091A`). The spec's
`0x20900..0x20914` and `tools/gp_win.py:53`'s same range are **short by the
load's own six bytes**; the behaviour is as they state.

The state sub-machine (`DS_00104B25`) indexes five tables by that char id.
Measured table reads (the instruction, its raw pre-fixup displacement, the
linear table):

| State | Instruction | Raw disp | Linear table | Meaning |
|---:|---|---|---|---|
| 0 | `0x2095A b8 80 7b 02 00` | `0x27B80` | `0xA7B80` | the fixed backdrop row (`0x38B18`) |
| 1 | `0x2099E b8 d8 7e 02 00` | `0x27ED8` | `0xA7ED8` | the fixed first actor |
| 1 | `0x209D4 8b 04 85 ac 80 02 00` | `0x280AC` | `0xA80AC` | character actor, sprite-layout branch (`DS_00104529` bit 1 set) |
| 1 | `0x209F9 8b 04 85 90 80 02 00` | `0x28090` | `0xA8090` | character actor, text-layout branch (bit 1 clear) |
| 3 | `0x20AAC 8b 04 b5 c8 80 02 00` | `0x280C8` | `0xA80C8` | character actor |
| 3 | `0x20AEA 8b 14 b5 bc 7e 02 00` | `0x27EBC` | `0xA7EBC` | character sprite stream |
| 4 | `0x20B94 8b 04 85 8c 81 02 00` | `0x2818C` | `0xA818C` | character title actor (`DS_00104529` bit 1 set) |
| 4 | `0x20BAC 8b 04 85 e4 80 02 00` | `0x280E4` | `0xA80E4` | character title **string id** (bit 1 clear) |

Supporting addresses: state 1's fixed actor spawn at `0x2099E` (call `0x2AE14`
at `0x209A3`), the sprite-flag test `0x209B3 f6 c3 02`; state 2's position gate
`0x20A3C/0x20A42` (`> 0x1E00`) and mode-0x15 handoff `0x20A5C..0x20A80`
(`0x4B0` frames, `0x3C`, mode `0x15`, return mode `0x1F`); state 3's fixed
fade-out stream `0x20AD5 ba 06 92 0e 00` (linear `0xE9206`; raw `06 92 06 00`)
and anim calls
`0x20AE0`/`0x20AF6`; state 4's countdowns `0x20B10..0x20B1A` (−0x80) and
`0x20B54..0x20B5A` (+0x40, clamped at `0x1000` at `0x20B6A`), the title string
draw `0x20BAC` (`game_string_get` = `0x1C500`), row `0x20BB3 ba 1a 00 00 00`,
col `0x20BBF b8 ff ff ff ff` (−1), `text_cursor_set` = `0x2F198` at `0x20BC4`
with mode `0x2000` (`0x20BA7 b9 00 20 00 00`), and the mode-0x15/`0x1E`
handoff `0x20BDD..0x20BFA` (`0x384`, `0x3C`, `0x1E`, `0x15`, state 0).

### The tables (measured, fixup-applied image; 7 dwords each)

| Table | [0] | [1] | [2] | [3] | [4] | [5] | [6] |
|---|---|---|---|---|---|---|---|
| `0xA8090` | `0A7F78` | `0A7F8C` | `0A7FA0` | `0A7FB4` | `0A7FC8` | `0A7FDC` | `0A7FF0` |
| `0xA80AC` | `0A7EEC` | `0A7F00` | `0A7F14` | `0A7F28` | `0A7F3C` | `0A7F50` | `0A7F64` |
| `0xA80C8` | `0A8004` | `0A8018` | `0A802C` | `0A8040` | `0A8054` | `0A8068` | `0A807C` |
| `0xA7EBC` | `0E9216` | `0E9266` | `0E9246` | `0E9256` | `0E9236` | `0E9226` | `0E9276` |
| `0xA818C` | `0A8100` | `0A8114` | `0A8128` | `0A813C` | `0A8150` | `0A8164` | `0A8178` |
| `0xA80E4` | `0xDE` | `0xDF` | `0xE0` | `0xE1` | `0xE2` | `0xE3` | `0xE4` |

The raw file (`VA + 0x46E54`) holds the same entries with `0x80000` subtracted
from the five pointer tables (`0x27F78`, `0x27EEC`, …); `0xA80E4`'s ids are
literal bytes (`de 00 00 00 df 00 00 00 …`).

### The exact titles

`0xA80E4[c]` is a string id. The text comes from `ENGLISH.TXT` through
`FUN_000474e4` (`0x474E4`): the id splits as `group = id >> 6`, `entry = id &
0x3F`; group blocks are chained by the u32 at block`+4` (the first block starts
at `base+8`, group 0; the u32 at `base+4` is group 1's block), entries begin at
block`+8`, each is one length byte then that many bytes **XORed with the
length**. Decoded (measured; sanity: id `0x01` = `TM   1994 ATARI GAMES CORP.`
(the same string `port/tests/test_game.c:87` asserts), id `0x1B` = `ENTER 3
LETTERS`, id `0x3B` = `YOU'RE UP, RIGHT PLAYER`):

| id | title |
|---:|---|
| `0xDE` | `THE FEAST OF SAURON` |
| `0xDF` | `BLIZZARD'S SOLITUDE` |
| `0xE0` | `TALON'S DREAM` |
| `0xE1` | `THE PALACE OF VERTIGO` |
| `0xE2` | `THE MEDITATION OF ARMADON` |
| `0xE3` | `DIABLO'S HELL` |
| `0xE4` | `CHAOS' REDEMPTION` |

All seven match the spec's table exactly (apostrophes included: `BLIZZARD'S`,
`TALON'S`, `DIABLO'S`, and `CHAOS'` without a second S).

### The character names and the cell map

- `0xC866C[c]` = the character-name string id: `0xB8`..`0xBE`, decoded
  `SAURON `, `BLIZZARD `, `TALON `, `VERTIGO `, `ARMADON `, `DIABLO `,
  `CHAOS ` (the file entries carry a trailing space).
- `0xC8880/0xC8881` = `00 05`: the initial character-select cursor cells
  (P1 cell 0, P2 cell 5). The copy is `0x445D8 8a 86 80 88 0c 00 mov
  al,[esi+0xC8880]` / `0x445DE 88 86 66 81 10 00 mov [esi+0x108166],al`
  (image bytes; `fight.c:1177` documents the same copy).
- `0xC8882` = `00 02 03 01 06 04 05` (measured, raw and image): the
  cell→character-id table. The write is `FUN_00043d60` at `0x43D60` (image
  bytes; the raw file's displacements are `0x88163`/`0x48882`/`0x8816A`):
  `0x43D65 8b 80 63 81 10 00 mov eax,[eax+0x108163]` (the dword at
  `0x108163+side`, whose high byte at `0x108166+side` is the cursor cell),
  `0x43D6B c1 f8 18 sar eax,0x18`, `0x43D6E 8a 80 82 88 0c 00 mov
  al,[eax+0xC8882]`, `0x43D74 88 83 6a 81 10 00 mov
  [ebx+0x10816A],al` (`DS_0010816A[side]` = the character id).

Inverse map (measured): cell 0→char 0, **cell 1→char 2**, **cell 2→char 3**,
**cell 3→char 1**, **cell 4→char 6**, **cell 5→char 4**, **cell 6→char 5**.

### The ending map

| char | name (`0xC866C`) | title id (`0xA80E4`) | title | cell (`0xC8882` inverse) |
|---:|---|---:|---|---:|
| 0 | SAURON | `0xDE` | THE FEAST OF SAURON | 0 |
| 1 | BLIZZARD | `0xDF` | BLIZZARD'S SOLITUDE | 3 |
| 2 | TALON | `0xE0` | TALON'S DREAM | 1 |
| 3 | VERTIGO | `0xE1` | THE PALACE OF VERTIGO | 2 |
| 4 | ARMADON | `0xE2` | THE MEDITATION OF ARMADON | 5 |
| 5 | DIABLO | `0xE3` | DIABLO'S HELL | 6 |
| 6 | CHAOS | `0xE4` | CHAOS' REDEMPTION | 4 |

The table is **character-id indexed**, not cell-indexed: the cell walk's only
job is to select the character (`0x43D60`), and the ending content follows
`0xC8882[cell]`. The U13 capture confirms char 1: its last `S` record has
`c0=1 c1=0` (`0x10782A`), and the U13 scenario walks to cell 3 (three
`p1.right` edges from cell 0). The U10 capture has `c0=0`.

The seven land marks are `0x108106..0x10810C`; the mark a won land writes is
`0x80 | side << 6 | char`, and the seventh-land tally counts a mark only when
`(mark & 0x7F) == DS_0010782A[side] | side << 6` (measured, `0x41C28` state 1):
`0x41D39 8a 83 06 81 10 00 mov al,[ebx+0x108106]`, `0x41D3F 24 7f and al,0x7F`,
`0x41D4A c0 e1 06 shl cl,6`, `0x41D4D 8a aa 2a 78 10 00 mov ch,[edx+0x10782A]`,
`0x41D53 08 cd or ch,cl`, `0x41D5C 3a 6c 24 04 cmp ch,[esp+4]`,
`0x41D62 fe 80 04 81 10 00 inc byte [eax+0x108104]`. For side 0 the five new
captures' marks are `0x82` (TALON), `0x83` (VERTIGO), `0x84` (ARMADON), `0x85`
(DIABLO), `0x86` (CHAOS); `0x80` is SAURON's and `0x81` BLIZZARD's.

The `gp_win.py` mode-0x1F row's label is built from the **character name**, not
the title: `_ENDING_LABEL` (`gp_win.py:53-55`) is `"%s's ending (mode 0x1F,
0x208F8): …"`, `_ending_tail(c0, name)` (`gp_win.py:120-126`) takes `(c0,
name)` and its predicate is `_all(ad4=0, c0=c0)`. The label's read range is
`0x20900..0x2091A` (the load is `0x20914..0x2091A`; Task 3 corrected it from
`0x20914`). The title is a `0xA80E4`
fact and the compared frames' content, not a WIN/S field.

---

## §E1.2 The U16 finisher path and fields

### The gate

All three reaction starters test the **other** slot's record:

| Starter | +0x42 gate | +0x43 gate | return |
|---|---|---|---|
| `0x37640` | `0x3766E f6 43 42 20  test byte [ebx+0x42],0x20` | `0x37678 f6 43 43 08  test byte [ebx+0x43],8` | `0x37672`/`0x3767C je 0x3776C` |
| `0x37774` | `0x377A0 f6 43 42 20` | `0x377AA f6 43 43 08` | `0x377A4`/`0x377AE je 0x37891` |
| `0x37898` | `0x378C5 f6 41 42 20  test byte [ecx+0x42],0x20` | `0x378CF f6 41 43 08  test byte [ecx+0x43],8` | `0x378C9`/`0x378D3 je 0x379BD` |

Measured; `EBX`/`ECX` is `DS_001077A8[(rec+0x51)^1]` (the defeated slot's
record), loaded at `0x37655..0x3765F`, `0x37787..0x37791`, `0x378B6`.

### The one write that sets both gate bits

`FUN_00036f10` (`0x36F10`, 477 bytes) arm A:

```text
0x36F17  80 3d 13 4b 10 00 00      cmp byte [0x104B13],0     (b13 must be non-zero)
0x36F71  80 3d 14 4b 10 00 00      cmp byte [0x104B14],0     (b14 must be 0)
0x36F78  0f 85 85 00 00 00         jne 0x37003               (arm B)
0x36F8E  39 d0                     cmp eax,edx               the other slot's side == DS_00104AD4
0x36F97  83 f8 01                  cmp eax,1                 +0x63 != 1 (not a CPU)
0x36FC0  66 8b 56 42               mov dx,[esi+0x42]
0x36FC8  81 ca 20 08 00 00         or  edx,0x820
0x36FD4  66 89 56 42               mov [esi+0x42],dx
```

The **word** store writes `+0x42 = 0x20` (bit 5) and `+0x43 = 0x08` (bit 3):
`0x820`'s low byte `0x20` lands in `+0x42`, its high byte `0x08` in `+0x43`.
Both gate bits come from this one store (measured).

### The `+0x42` bit `0x10`

`FUN_00036e78` (`0x36E78`, 152 bytes) second arm, when `+0x5B == 0`:

```text
0x36E9A  8b 50 40                  mov edx,[eax+0x40]
0x36E9D  81 ca 00 10 10 00         or  edx,0x101000          +0x41 bit 4, +0x42 bit 0x10
0x36EA9  81 e3 ff f7 ff fb         and ebx,0xFBFFF7FF        clears +0x41 bit 3, +0x43 bit 2
0x36EAF  89 58 40                  mov [eax+0x40],ebx
```

Reached from `FUN_000392a0`'s KO arm at **`0x3961E call 0x36E78`**, gated by

```text
0x39605  31 c0                     xor eax,eax
0x39607  8a 41 5a                  mov al,[ecx+0x5A]
0x3960A  83 f8 78                  cmp eax,0x78
0x3960D  7c 14                     jl 0x39623               (needs +0x5A >= 0x78)
0x39611  66 a1 00 4b 10 00         mov ax,[0x104B00]
0x39617  83 f8 03                  cmp eax,3
0x3961A  74 07                     je 0x39623               (skipped in mode 3)
0x3961C  89 c8                     mov eax,ecx
0x3961E  e8 55 d8 ff ff            call 0x36E78
```

The other call site is `0x33B00`'s restore: `0x33BDF lea eax,[0x1077B0+edx]`,
`0x33BE1 e8 92 32 00 00 call 0x36E78` (after `0x33BD5 cmp eax,3; je`). The
`+0x5A == 0x78` write in this arm is the KO: `0x395C2 c6 41 5a 78 mov byte
[ecx+0x5A],0x78` (measured).

### The mode-9 hold

`0x28012 bb 58 02 00 00  mov ebx,0x258` then `0x2801D 66 89 1d f8 4a 10 00
mov [0x104AF8],bx`. The mode decision follows: `0x28024 85 ff test edi,edi`,
`0x28026 7c 13 jl`, `0x28028 83 ff 02 cmp edi,2`, `0x2802b 7f 0e jg` (else
mode 8 at `0x2803B`), `0x2802D` mode 9 — i.e. mode 9 only for a result in
`0..2`, and the `0x258`-frame hold is armed on both. Measured.

### The three entries, reactions and callbacks

The keyboard/player action table is `0xC6B9C`; entry `(c, i)` is at
`0xC6B9C + (c*0x20 + i)*8`, laid out `{u32 desc; u16 reaction; u8 d; u8 e}`
(record gameplay-u6 §U6.10). Measured:

| character | entry | table address | reaction | callback (`0xA3528 + (c*0x40 + reaction)*0x14`, first dword) |
|---|---:|---|---:|---|
| TALON (c=2) | `0x15` | `0xC6E44` | `0x32` | `0xA3528 + 0xB2*0x14 = 0xA4310` → **`0x37640`** |
| TALON (c=2) | `0x16` | `0xC6E4C` | `0x33` | `0xA4324` → **`0x37774`** |
| TALON (c=2) | `0x17` | `0xC6E54` | `0x34` | `0xA4338` → **`0x37898`** |
| SAURON (c=0) | `0x17` | `0xC6C54` | `0x32` | `0xA3910` → `0x37640` |
| SAURON (c=0) | `0x18` | `0xC6C5C` | `0x33` | `0xA3924` → `0x37774` |
| SAURON (c=0) | `0x19` | `0xC6C64` | `0x34` | `0xA3938` → `0x37898` |

The reaction-table index is built by `FUN_0003afc4` (`0x3AFC4`):
`0x3AFDA..0x3AFF2` compute `char*0x40 + action` from
`0x3AFE8 8a 0c 85 2a 78 10 00 mov cl,[eax*4+0x10782A]` (the slot's character),
`0x3AFEF c1 e1 06 shl ecx,6`, `0x3AFF2 01 d1 add ecx,edx`; the three stores are
`0x3B010 [ebx] = 0xDE114 + idx*0xB`, `0x3B025 [ebx+4] = 0xA3528 + idx*0x14`
(with `0x3B014 ba 28 35 0a 00 mov edx,0xA3528`), `0x3B033 [ebx+8] = 0xA6728 +
idx*6`. `0x34E2C` then calls `[ebx+4]` (`0x35045`).

**Correction to the spec/plan.** "The three finisher entries are
`0x15/0x16/0x17`" is **TALON's (character 2) set only**. The entry indices are
per-character: SAURON's are `0x17/0x18/0x19` (and `gp_session.py:547-548`
already says so: "TALON's (char 2, P1 here) are 0x15/0x16/0x17"). Tasks 3–7
need the character's own indices; the U23 scenario (P1 = TALON) is the one
where `0x15/0x16/0x17` are right.

### The S fields

`tools/gp_session.py:35-52` (`SNAP_FIELDS`) carries, appended by plan
gameplay-u6b (record gameplay-u6 §U6.11):

```text
('r0', 0x1088A8, 1), ('r1', 0x1088A9, 1), ('c0', 0x10782A, 1), ('c1', 0x1078BE, 1),
('s0_43', 0x1077F3, 1),
```

- `r0`/`r1` are `DS_001088A8 + side`, the **last reaction each side applied**:
  `0x34EF6 88 8b a8 88 10 00  mov [ebx+0x1088A8],cl` inside `0x34E2C`
  (measured).
- `s0_43` is `0x1077F3` = **slot 0's `+0x43`** (the block bits `0x20/0x10`).
- **There is no `+0x42` field** in `SNAP_FIELDS`: `+0x42` is `0x1077F2`
  (slot 0) and `0x107886` (slot 1), and `+0x43` is `0x1077F3` (`s0_43`, slot 0)
  and `0x107887` (slot 1); only slot 0's `+0x43` is a field. The U16 gate's
  `+0x42` bit 5 is **not** a captured field. `s0_43` is slot 0's
  own `+0x43`; the U16 gate is the *other* (defeated) slot's `+0x43` bit 3, so
  in the U23 capture (P1 = TALON the winner, P2 defeated) `s0_43` does not
  carry the gate either. The gate is decomp evidence; the reaction is the
  capture's claim.

`tools/gp_twop.py:173-190` (`finisher()`): it requires a mode `0x09` record, a
mode `0x12` record and at least one of `r0`/`r1` in `0x32..0x34`;
`--allow-missing-reaction` waives **only** the reaction requirement. The
existing `gp-u16-twop2` capture needs the allowance (record gameplay-truth-ii
§J8: no reaction appeared, so mode 9 lasted one frame).

---

## §E1.3 The `b0c` evidence is U10's

`b0c` is `DS_00104B0C` (`0x104B0C`), a `WIN_EXTRA` field
(`tools/gp_session.py:401`).

### The only direct writer of 1 (the whole image)

Scanning the fixup-applied image for the displacement `0c 4b 10 00` finds
exactly seven instructions, five writes and two reads (measured):

| Site | Instruction | Value | Role |
|---|---|---|---|
| `0x2703D` | `88 15 0c 4b 10 00 mov [0x104B0C],dl` | `dl=0` (`0x27026 xor dl,dl`) | mode-0x24 handler `0x26F58` expiry (`0x27037` also sets `b14=1`) |
| `0x27200` | `88 25 0c 4b 10 00 mov [0x104B0C],ah` | `ah=0` (`0x271F0 xor ah,ah`) | join prompt `0x271E0` |
| `0x27582` | `88 35 0c 4b 10 00 mov [0x104B0C],dh` | `dh=0` | **mode-0xD wait**, after `0x27562 mov dl,[0x104B0C]` / `0x2756E test dl,dl; je` |
| `0x29744` | `88 35 0c 4b 10 00 mov [0x104B0C],dh` | `dh=0` | `0x296B8` (after `0x2971E` read / `0x2972A` test) |
| `0x37FF6` | `88 2d 0c 4b 10 00 mov [0x104B0C],ch` | `ch=1` (`0x37FF2 b5 01`) | **`0x37EA0`, the death-animation-end opcode `0x11`** |

Reads: `0x27562` (mode 0xD) and `0x2971E`. **`0x37FF6` is the only direct store of a
non-zero `b0c`** (no block clear/fill targets the byte: none of the 41
`0x61A70` memset call sites names a `0x104xxx` address in its statement), so
every `b0c=1` must be `0x37EA0` (or a harness poke).
`0x37EA0` is the `0xD100` opcode-`0x11` target in the seven character streams
(the dwords at `0xD2BEA`, `0xD486C`, `0xE119C`, `0xE4566`, `0xE7932`,
`0xEB1A6`, `0xED5AA`), ported as `fighter_37ea0` (record §D8.1;
`port/src/game/fighter.c:14353-14403`, whose 14401 store is the `b0c` write).

### The U10 trace (measured, `data/k11-captures/gp-u10-ending/poll.log`)

`gp_session.parse`-parsed `S` records (9972 of them):

- The **only `b0c` 0→1 transition** is at **game frame `f=0x1602` (5634)**, in
  **mode `0x0F`**, with `b14=1`, `b21=7`, `s1_5a=0x78`, `ad4=0`; after it
  `b0c` stays 1 to the capture end (`f=0x271D`), 4,364 `S` records with
  `b0c=1` and no 1→0 transition.
- The seventh final opponent's KO `S` record is at **`f=0x153A`** (mode
  `0x0D`); mode `0x0F` starts at **`f=0x1544`** (`b21=7`). `0x1602 - 0x1544 =
  0xBE = **190**`, and `0x1602 - 0x153A = 0xC8 = 200`.
  The spec's "190 frames after the seventh poked final KO" is therefore
  **190 after the mode-`0x0F` entry that follows the seventh KO** (200 after
  the KO's own `S` record). Both numbers are now stated.
- The seventeen `W` (poke) records: the round-1 pokes at capture frame
  `0x0482` (land marks + `0x10789E=0x78`), round 2 at `0x0850`, then the seven
  final KOs at `0x14BF, 0x14D5, 0x14E9, 0x14FD, 0x1511, 0x1525, 0x1539` and the
  seven `DEATH_DONE` pokes at `0x14CB, 0x14DF, 0x14F3, 0x1507, 0x151B, 0x152F,
  0x1543`. The last `DEATH_DONE` (`0x1543`) is consumed by the mode-`0x0F`
  entry at `f=0x1544`.
- **Every poked `b0c=1` is consumed inside its own frame**: e.g. the poke at
  capture `0x14CB` is followed by `f=0x14CC mode=0x0C b0c=0` (`0x27582` ran).
  No `S` record shows a poked 1 (each poke's value is consumed in the same
  frame); the only `b0c` 0→1 transition is the game's own `0x37FF6` write,
  190 frames into mode `0x0F`.

So a new `b0c` capture is **not needed**: U10 already pins the game-set write.
`GP_ENDING_WIN_MIN_FIRST = 9954` (`Makefile:1212`, comment at `:1139`) compares
it: the port's WIN fields agree at every frame through the replay's end
(`0 differing through 9953`, f=0x26E1). The old "before P2" value 5634 is
exactly `f=0x1602`.

### `b14` and why `0x36F10` is not the final's `b0c` path

- `b14` is `DS_00104B14` (`0x104B14`, `WIN_EXTRA`). Its writers in the image
  (displacement scan `14 4b 10 00`) are `0x25A08 mov [..],cl` / `0x25A51
  mov [..],dl` (value from the caller), `0x25AF8 88 25 14 4b 10 00 mov
  [0x104B14],ah` with `ah=0` (`0x25AF6 30 e4 xor ah,ah`), `0x27037 88 35 14 4b
  10 00 mov [0x104B14],dh` with `dh=1` (`0x27033`), and `0x271FA 88 15 14 4b
  10 00` with `dl=1` (`0x271E4 b2 01`). **The only unconditional setters of 1 are
  `0x27037` (the mode-0x24 handler `0x26F58`'s expiry) and `0x271FA` (the join
  prompt).** The U10 trace shows `b14 0→1` at `f=0x139E`, mode `0x17`, i.e.
  when the mode-`0x24` health-bonus screen expired — **not** inside the final's
  own mode. *Correction to the spec's "the final sets `b14=1`": the
  mode-0x24 handler that precedes the final sets it.*
- `0x36F10`'s arm A is gated on `b14 == 0` (`0x36F71`/`0x36F78`), so in the
  final (`b14=1`) arm A does not run and the `+0x42` bit 5 / `+0x43` bit 3
  gate bits are not set.
- **Stronger than the spec:** `0x36F10` has **no `b0c` write at all** (it
  writes `+0x42`/`+0x43`), and `0x37FF6` is the only direct store of a
  non-zero `b0c`. The final's `b0c=1` cannot be a finisher write for two
  independent reasons.

### The U14 fought-win rule

`gp_twop`'s / `gp_win`'s `live_death` rule (record gameplay-truth-ii §J6 and
spec §7.3) anchors on the **`(0x09,1)`** entry (mode 9, the match result), must
reject any `DEATH_DONE` `W` write (`0x104B0C`), and requires `b0c=1` after the
KO. That rule is unchanged by this phase; the U9/U10 poked wins are not a
substitute (their `DEATH_DONE` pokes prove the mode-0xD consumer, not a live
KO).

---

## §E1.4 U14 and U13

### The U14 scenario (measured, `tools/gp_session.py:504-516`)

```text
U14_PUNCH    = ('p1.b0', 'p1.b1')      entry 0x1A, reaction 0x10
U14_KICK     = ('p1.b2', 'p1.b3')      entry 0x1B, reaction 0x11
U14_GAP      = 16                      frames between attempts (harness)
U14_ATTEMPTS = 32                      attempts per round (harness)
SCENARIOS['gp-u14-win-live'] = dict(time_limit=200, extra=WIN_EXTRA, steps=WIN_MENU + (
    ('after_entry', 0x06, 1, 10, ('pad', U14_PUNCH, 4)),      round 1's first press
) + U14_PRESSES + (
    ('after_entry', 0x06, 2, 10, ('pad', U14_PUNCH, 4)),      round 2's first press
) + U14_PRESSES + (('until_mode', 0x12, 0),))                 the conquered-lands screen
```

`WIN_MENU` (`gp_session.py:423-428`) is boot/START MENU/LEFT PLAYER ARCADE and
the character-select confirm of cursor 0 (SAURON); `extra=WIN_EXTRA` only adds
snapshot fields — **no pokes**. `U14_PRESSES` is 31 more alternating
`1A`/`1B` presses, 16 frames apart, each held 4.

### The failed retry's `S` records (re-measured)

The artifact is `/tmp/pr_gp_failed/gp-u14-win-live-run2/` (7,287 frames; the
`X` record was never reached, so it is **not** a valid capture and all
`GP_U14_*` pins stay empty). 9,761 `S` records, `f=0x4..0x265D`:

| Fact | Measured |
|---|---|
| round 1 ends | mode `0x08` at `f=0x09B3`, `s0_5a=0x78`, `s1_5a=0x00` (P1 KO'd, P2 undamaged) |
| round 2 ends | mode `0x09` at `f=0x12DC`, `s0_5a=0x78`, `s1_5a=0x38`, `ad4=1` (P1 KO'd again, P2 at 0x38, P2 won) |
| after the result | mode `0x17` at `0x140D`, `0x15` at `0x14FC`, `0x13` at `0x1575`, `0x1E` at `0x1801`, … |
| back to attract | mode `0x03` at `f=0x1D68`, runs to the last `S` `f=0x265D` |

The two-attempt limit is spent (record §J6); a retake needs a new explicit
go-ahead and the revised script (more attempts, a `p1.right` re-approach,
`time_limit ~260`), and the claim is the no-poke fought KO with the game-set
`b0c` (`§E1.3`).

### The U13 tail (measured, `data/k11-captures/gp-u13-ending2`)

| Fact | Measured |
|---|---|
| `X` record | `ms=205231 f=271D step=24 end` (0x271D = 10013) |
| `S` count | 10021; last `f=0x2754` (10068), mode `0x03`, `cred=5`, `c0=1`, `c1=0` |
| `S` past the `X`'s `f` | 55 records (`f=0x271E..0x2754`) |
| capture frames | 5904 (`frame_00000..frame_05903`; `GP_U13_CAPTURE_FRAMES`, `Makefile:1277`) |
| the unexplained tail | the last 13: `5891..5903` (`GP_U13_MIN_FIRST = 5891`, `Makefile:1272`) |
| the port dump | 4940 frames, `0..4939` (`GP_U13_MIN_REACH = 4939`, `Makefile:1275`) |
| the guard | `gp_session.py:967-973`: `last = xrec['f']`; `if end > last: raise ScriptError('--end %d is past the capture end …')` |

The 13 tail frames are not identical (each has 54,288 non-black pixels and its
own sha256 — an animated screen); frame 5889 is all-black and 5890 is nearly
black (166 px). The port's dump stops at 4939 because the generated script's
`--end` is the `X`'s `f`, so those 13 states have no port frame. The caption
"the attract's `CREDITS: 5` screen" is the last `S` record's `mode=3, cred=5`;
the *pixels* of the tail are not claimed by this record.

*Correction to the spec's reading of the guard's lines*: the guard is
`gp_session.py:967-973` (the spec's `970-977` includes the `end < p27`
arm).

---

## §E1.5 The 5120 band (measured, re-derived)

Re-measured from `data/k11-captures/gp-u13-ending2/frame_05120.raw.gz` and
port frame `4317` of a fresh `make gp-replay scenario=gp-u13-ending2` dump
(`/tmp/pr_gp_dump/gp-u13-ending2`; 4940 `.ipx` frames). The port frames are
`.ipx` (64,000 indices + a 768-byte DAC) and are expanded with `expand_ipx`
(`rgb = dac[idx]`); the capture frames (`frame_05120.raw.gz`) are already the
192,000 RGB bytes (`320*200*3`), read directly.

**Capture 5120 differs from port 4317 in exactly 13 pixels**, rows 1..4,
x 21..298:

| pixels | capture RGB | port RGB | port index | port DAC entry |
|---|---|---|---|---|
| (1,23) (1,24) (1,295) (1,296) (2,22) (2,297) (3,21) (3,298) | `00001c` | `000000` | `0x32` | `000000` |
| (3,74) | `000000` | `100000` | `0x2E` | `100000` |
| (4,27) (4,28) (4,291) (4,292) | `000000` | `00000c` | `0x4D` | `00000c` |

- **8** pixels are `00001c` where the port's index is `0x32` (DAC `000000`),
  **1** is black where the port's index is `0x2E` (DAC `100000`), **4** are
  black where the port's index is `0x4D` (DAC `00000c`) — the spec's 8/1/4
  split, confirmed.
- **One capture frame only.** Of all 5904 capture frames, exactly one
  exhibits this 13-pixel state: `5120` (not just "all 8 `00001c`": also
  exactly one, `5120`).
- **The static-index intersections are empty.** In this same frame the
  capture shows two different colours at each index's own positions:
  `0x32`: 101 positions, 93 black + 8 × `00001c`; `0x2E`: 96 positions, 95 ×
  `100000` + 1 black; `0x4D`: 54 positions, 50 × `00000c` + 4 black. No single
  index→colour state (a DAC) can satisfy the band and the rest of the frame
  at once.
- **The frame-wide offset test** (all 64,000 pixels): `k=0` differs in **13**
  pixels; the best non-zero offset is `k=+109` at 11,282 (then `+81` 11,372,
  `+82` 11,497, `+80`/`+90` 11,594). The `{-29,+14,+29}` "covering" offsets
  are DAC aliases: both `00001c` and black recur at several DAC entries. No
  dumped DAC renders index `0x32` as `00001c` (0 of 4940 port frames).
- Neighbours (measured): `5119` matches port `4315` with **0** differing
  pixels; `5122` matches port `4318` with **0**; `5121` is a splice of
  `4317`/`4318` (it matches `4318` at 28 differing pixels and `4317` at 276,
  the lower rows). The exact splice bytes are not needed by this phase.

**The corner-ornament candidate `0x01047345` is not evidence.** The literal
does not occur anywhere in the 1,028,304-byte fixup-applied image nor in the
raw `PRAGE.EXE`: searched as the little-endian dwords `0x01047345` and
`0x00047345` and as the byte sequence `45 73 04 01`/`45 73 04 00`, at every
alignment — **zero hits**. A handle of that form would decode as
`res_handle(2, 0x47345)` (`port/src/platform/res.c:298`: `index << 23 | offset`), but no
writer, table entry or draw for it exists. The geometry is consistent with a
top-corner ornament (12 of the 13 pixels lie at x 21..28 and 291..298, the two
top corners), but the candidate explains neither the bytes nor `(3,74)`, which
is outside any such pair of rectangles. **The record withdraws the candidate
as unsupported and keeps the band a named gap**: either a sub-raster transient
of the original's top four rows or a present the replay never dumped (§P3.3's
class C), not a proven port defect.

The 4248/4320 recipes and pins are unchanged by this section (they are
`tools/gp_compare.py` data; §P3.4).

---

## §E1.6 The INT 8 tick vector

### The installer (measured)

Raw file bytes at `0x66519..0x6652D` (pre-fixup), and the fixup-applied image:

```text
raw   : b8 08 00 00 00  ba 7e 62 05 00  66 8c cb  b4 25  1e  66 8e db  cd 21
image : b8 08 00 00 00  ba 7e 62 06 00  66 8c cb  b4 25  1e  66 8e db  cd 21
addr  : 0x66519         0x6651E          0x66523     0x66526 0x66528 0x66529 0x6652C
```

| Address | Instruction | Meaning |
|---|---|---|
| `0x66519` | `b8 08 00 00 00` | `mov eax,8` (AL = vector 8) |
| `0x6651E` | `ba 7e 62 05 00` (raw) / `ba 7e 62 06 00` (image) | `mov edx,0x0005627E` raw / `0x0006627E` image |
| `0x66523` | `66 8c cb` | `mov bx,cs` |
| `0x66526` | `b4 25` | `mov ah,0x25` (DOS set-interrupt-vector) |
| `0x66528` | `1e` | `push ds` |
| `0x66529` | `66 8e db` | `mov ds,bx` |
| `0x6652C` | `cd 21` | `int 0x21` |

The immediate is a **code-object offset** (`0x5627E`); the object-0 fixup adds
`0x10000` and the loaded image holds `0x0006627E` — the linear address of the
handler. **Vector 8 points at `0x6627E`.** (`le_info.py`'s object 0 base
`0x10000`; image bytes at `0x66519` above.) The spec's parenthetical "(CS base
`0x10000`)" is the object base added by the fixup, not a runtime CS base: the
runtime addresses are flat (the fixup already produced the linear value).

After the `int 0x21`: `0x6652E 1f pop ds`, `0x6652F c7 05 fc fb 0e 00 02 00
00 00 mov dword [0xEFBFC],2`, `0x66539 68 8d d6 00 00 push 0xD68D`,
`0x6653E 6a 3c push 0x3C`, `0x66540 e8 23 75 ff ff call 0x5DA68` (AIL
`set_timer(rate=0x3C, max period=0xD68D)`).

### The handler `0x6627E` (measured)

```text
0x6627E  2e 83 3d 12 fd 0e 00 00  cmp dword cs:[0xEFD12],0   (re-entrancy guard)
0x66286  0f 87 11 01 00 00        ja  0x6639D
0x6628C  60 1e 06 0f a0 0f a8     pushal; push ds/es/fs/gs
0x66293  8b ec                  mov ebp,esp
0x66295  fc                     cld
0x66296  66 2e 8e 1d 1a fd 0e 00 mov ds, word cs:[0xEFD1A]   (es too at 0x6629E)
0x662A6  ff 05 12 fd 0e 00       inc dword [0xEFD12]
```

It is a runtime (≥ `0x5D000`) AIL routine, not a `prage.functions.csv` entry;
`0x66264` and `0x66271` are the two functions before it. It is **not** the
port's `0x2D62C`.

### The PIT routine `0x663A4` and the divisor converter `0x663D4`

`0x663A4` (`FUN_000663a4`, 48 bytes) is the divisor writer:

```text
0x663A4  55 8b ec 53 56 57 9c fa   push ebp; mov ebp,esp; push ebx/esi/edi; pushfd; cli
0x663AC  b0 36                     mov al,0x36
0x663AE  e6 43                     out 0x43,al          PIT mode command 0x36
0x663B0  8b 45 08                  mov eax,[ebp+8]      the divisor
0x663B3  a3 0a fd 0e 00            mov [0xEFD0A],eax
0x663BA  e6 40                     out 0x40,al          divisor low
0x663BC  8a c4                     mov al,ah
0x663C0  e6 40                     out 0x40,al          divisor high
```

`0x663D4` converts a period in µs to the divisor:

```text
0x663DA  b8 00 00 00 00            mov eax,0
0x663DF  81 7d 08 8d d6 00 00      cmp dword [ebp+8],0xD68D   (54925)
0x663E6  73 11                     jae 0x663F9                 (>= max: divisor 0)
0x663E8  8b 45 08                  mov eax,[ebp+8]             the period
0x663EB  bb bc 20 00 00            mov ebx,0x20BC              (8380)
0x663F0  b9 10 27 00 00            mov ecx,0x2710              (10000)
0x663F5  f7 e1                     mul ecx                     edx:eax = period*10000
0x663F7  f7 f3                     div ebx                     eax = period*10000/8380
0x663F9  50 / e8 a5 ff ff ff       push eax; call 0x663A4
```

**19,887:** for the AIL period **16,666 µs**, `16666 * 10000 = 166,660,000`;
`166,660,000 / 8380 = 19,887` remainder `6,940` (DIV truncates). Measured with
the two constants at `0x663EB`/`0x663F0`.

**16,666 µs is measured, not nominal.** AIL's rate setter
`0x66BB8` (`FUN_00066bb8`, 48 bytes) does

```text
0x66BC8  b8 40 42 0f 00         mov eax,0xF4240          1,000,000
0x66BCD  8b 5d 0c               mov ebx,[ebp+0xC]        the rate (60)
0x66BD0  f7 f3                  div ebx                  period = 1,000,000/60 = 16666 (remainder 40)
0x66BD2  50 / ff 75 08 / e8 8d 6e ff ff  push eax; push [ebp+8]; call 0x5DA68
```

`0x5DA68` → `0x66B88` stores the period (`0x66B99 89 83 40 fc 0e 00 mov
[ebx+0xEFC40],eax`, `0x66B9F` clears the slot flag) and calls `0x66407`, which
scans the 16 timer slots for the **minimum** period
(`0x6641D..0x66438`, `[edi+0xEFC40]`), compares it with the current divisor
`[0xEFD0E]` (`0x6643A`), and on a change `0x66448 push ecx; 0x66449 e8 86 ff ff
ff call 0x663D4`. The rate 60 comes from the installer's `0x6653E 6a 3c push
0x3C` (`0x66540` call) and the AIL registration path's `0x1CFFA 6a 3c push
0x3C` (`0x1CFFF call 0x5DA87` → `0x66BB8`).

**59.998 Hz (inferred):** the standard PIT input clock 1,193,182 Hz divided by
the measured divisor 19,887 is `59.99809 Hz`. The bytes give the divisor, not
the clock; the clock is the standard PC constant.

*Correction to the spec's "`0x1CFFF` sets `0x3C`": `0x1CFFA` is the `push 0x3C`
(60 Hz); `0x1CFFF` is the `call 0x5DA87`.* The period 16,666 µs is not merely
"the AIL 60 Hz period" but the measured `1,000,000/60` at `0x66BD0`.

### `0x2D62C` is the counter, not the ISR

`0x2D62C` (`FUN_0002d62c`, **9 bytes**, one caller):

```text
0x2D62C  ff 05 88 5d 10 00   inc dword [0x105D88]
0x2D632  8b c0               mov eax,eax
0x2D634  c3                  ret
```

It increments the run clock `DS_00105D88` (which `0x32970` reads). The 60 Hz
callback `0x1BDF4` calls it at **`0x1BE28 e8 ff 17 01 00`**; the same callback
calls the input sampler `0x1BBAC` at `0x1BE1C` and increments the word
`0xEF6DE` at `0x1BE21`. `0x1BDF4` is the registered AIL timer callback:
`0x1CFED 68 f4 bd 01 00 push 0x1BDF4`, `0x1CFF2 e8 1b 0a 04 00 call 0x5DA12`
(→ `0x66A26`, which stores the pointer at `[slot+0xEFB80]` and sets the slot
flag `[slot+0xEFBC0]=1`). Measured. **`0x2D62C` is not the ISR entry**; the
vector's target is `0x6627E` (§E1.6 above).

---

## What this settles

- **The ending map is one table and one code path.** Mode 0x1F (`0x208F8`)
  reads the winner's char id (`0x20900..0x2091A`) and indexes five
  character tables; the five new captures differ only by cell, char id, title
  string and the content. The exact titles (`0xDE..0xE4`) and the cell map
  (`0xC8882 = 00 02 03 01 06 04 05`) are confirmed byte-for-byte. Tasks 3–7
  may cite §E1.1.
- **The U16 entries are per-character.** `0x15/0x16/0x17` is TALON's (char 2)
  set; the reactions `0x32/0x33/0x34` and the callbacks
  `0x37640/0x37774/0x37898` are universal, and the reaction table is indexed
  `char*0x40 + action` (`0x3AFC4`). The gate bits (`+0x42` bit 5, `+0x43` bit
  3) are one word store at `0x36FD4`; `+0x42` bit `0x10` is `0x36E78`; the
  gate is decomp evidence and the `r0`/`r1` reaction is the capture's claim
  (`s0_43` exists, no `+0x42` field does).
- **`b0c` is already proven game-set by U10.** `0x37FF6` is the only direct
  store of a non-zero value to `b0c`; the trace's only `b0c` 0→1 transition is
  at `f=0x1602` in mode `0x0F`, after which `b0c` stays 1 to the capture end
  (4,364 `S` records), 190 frames after the mode-`0x0F` entry (`f=0x1544`) and
  200 after the seventh KO's `S` record, with every poked `DEATH_DONE` consumed
  by `0x27582` inside its own frame. No new `b0c` capture is needed; only the
  fought (no-poke) win (U14) is open, under the `live_death` rule.
- **U14's failed retry is measured** (round 1 P1 KO'd `s0_5a=0x78`, round 2
  P1 KO'd `s0_5a=0x78` with P2 `s1_5a=0x38`, `ad4=1`, mode 3 at
  `f=0x1D68`), so Task 8's retake has a baseline; it is still conditional.
- **The U13 tail is 13 capture frames** (`5891..5903`) past the `X`
  (`f=0x271D`); the last `S` is `f=0x2754`; the guard is `gp_session.py:967-973`.
  The tail's pixels are not claimed here.
- **The 5120 band is re-measured** exactly as the 8/1/4 split, occurs in one
  capture frame, and has empty static-index intersections; the spec's
  corner-ornament handle `0x01047345` has **no byte anywhere in the image** and
  is withdrawn — the cause stays a named gap.
- **The INT 8 vector is `0x6627E`** (raw offset `0x5627E` + object base
  `0x10000`), the PIT routine `0x663A4` writes divisor
  `period*10000/0x20BC` (`0x663D4`), the rate-60 period is the measured
  `16666` µs (`0x66BD0`), the divisor `19887` and (inferred) `59.998 Hz`, and
  `0x2D62C` is the 9-byte run-clock counter called by `0x1BDF4` at `0x1BE28`,
  not the ISR entry.

Corrections of record (raw beats the spec/plan): the charid read range ends at
`0x2091A`; the finisher entry indices are per-character; `b14=1` is set by the
mode-`0x24` handler's expiry (`0x27037`) in the U10 path, not by the final's
own mode; `0x36F10` has no `b0c` write at all; the "190 frames" is measured
from the mode-`0x0F` entry (`f=0x1544`); the corner-ornament handle is
unsupported; the `0x1CFFF`/`0x3C` citation is the call, the push is
`0x1CFFA`; and the `gp_session` guard's lines are `967-973`.

---
## §E2 The U23 finisher capture (Task 2)

### The live KO attempts and the labelled fallback

`SCENARIOS['gp-u23-u16-finisher']` (`tools/gp_session.py`) is gp-twop's two-human
start and a 32-attempt first round (`U16_FINISHER`'s prefix, unchanged), then the
plan's **labelled fallback** for the second round:

```text
after_entry 0x06,2,10  poke 0x10789E = 0x77      P2's +0x5A damage byte (record §W.3)
after 8,  ...          12 real 1A/1B attempts    16 frames apart, a p1.right every eight
after_entry 0x09,1,100 TALON's 0x15/0x16/0x17    reactions 0x32/0x33/0x34
until_mode 0x12,0                               the conquered-lands screen
```

The poked damage is not the claim: the game's own hit takes `0x77` to the KO value
`0x78`, and only the reaction in an S record's `r0`/`r1` is claimed.
`gp_twop.py check --finisher` runs **without** `--allow-missing-reaction`.

Six live-KO attempts were made on 2026-10-23; none was clean, and the fallback is
the remaining path. (`88` below is the round's attempt count; the KO attempt is a
separate, RNG-dependent number.)

| # | round 2 | anchor | what it showed | why it is not published |
|---:|---:|---:|---|---|
| 1 | 160 attempts | 10 | KO at attempt 88 (step 143, `f=0x19F0`), mode 9 at `f=0x19F8`; the 72 leftovers ran on the result; the finisher group fired at `f=0x1F24`, after mode 9, its 15 steps collapsed into one frame and the BIOS ring overflowed | `end frame reached` FAIL, `port script v2` FAIL (key 433 at `f=1F24` not queued, ring full), `unscripted input` FAIL (2 pad releases from `f=0x1F27`) |
| 2 | 88 attempts | 10 | KO at attempt 87 (`f=0x19D0`), mode 9 at `f=0x19E0`, `r0=0x33` | `unscripted input` FAIL: 1 pad release at `f=0x19F5`, the last attempt's `b2` release while the finisher's first `b0+b2` held it |
| 3 | 96 attempts | 200 | KO (`s1_5a=0x78`), mode 9 at `f=0x19F8`; the first entry applied `0x27`, the second no finisher reaction | no `0x33`; the pad change at `f=0x19F8` was unpinned (no S at `f=0x19F7`) |
| 4 | 88 attempts | 100 | no KO (`s1_5a=0x68` at the time-up), mode 9 at `f=0x20DE` | no reaction |
| 5 | 92 attempts | 100 | KO, mode 9 at `f=0x19E0`, `r0=0x33` first at `f=0x1AB7` (683 S records) | `unscripted input` FAIL: 1 pad release at `f=0x1A49`, the last leftover's `b2` release while the finisher's first `b0+b2` held it |
| 6 | 92 attempts | 160 | no end frame; the match returned to the attract (P1 KO'd later) | `end frame reached`/`stopped at the end` FAIL |

**The live KO is reached** (attempts 1, 2, 3 and 5), so the fallback is not used
because a KO could not be landed. The measured obstacle is the interaction of the
RNG-dependent KO attempt (85–88 across runs) with a narrow gate window: an anchor
past the leftovers (the 200 and 160 runs) misses the window (no `0x33`; attempt 6
also lost the match), while an anchor in the window (100) can collide the finisher's
first `b0+b2` with the last leftover's hold (attempts 2 and 5). The plan's fallback
keeps the 100-frame anchor, which is the measured window (the published capture's
second entry applies `0x33` at mode 9 + 213), and it is labelled here and in the
scenario, the Makefile and `docs/PROGRESS.md`.

**Why 12 attempts (fallback).** The published capture's first mode-6 S is `f=0x523`
and its first `s1_5a` change is `f=0x5A4` (0x81 into the round), so 12 attempts
starting 8 frames after the poke (then 16 apart, a `p1.right` walk every eight)
cover the first hit; the 100-frame anchor is past the fallback's own leftovers in
the published capture and lands the second entry in the measured window. The
leftover bound is 11 attempts x 16 = 176 frames (the poked 12-attempt round with a
first-hit KO); the anchor is a harness value tuned to the captured window, not a
claim that the bound is always met. The group spans 212 frames.

### The capture (2026-10-23)

`make gp-capture scenario=gp-u23-u16-finisher` under `xvfb-run -a` (this host has
no X display; the original renders to an AVI). Every session check is `ok`:

```text
snapshots 6087, f 5..17DC, 17 frames missed
CHECK base / steps fired 75/75 / end frame reached / mode 0x27 after the Enter /
      snapshots kb == raw (0 differ) / frames written 5464/5464 / port script v2 /
      no unscripted input / pokes written 1/1, 0 raced / stopped at the end
      (SIGTERM at f=17DD, rc=0): ok
wrote 5464 frames (raw 1371..8555), wall 122.8 s
```

Measured in its `poll.log` (6,087 S records):

| Fact | Value |
|---|---|
| the poke | `W f=13B8 addr=0010789E was=00 now=77 late=0 race=0` |
| round 2's mode-6 entry | `f=0x13AF` |
| the match result (mode 9) | `f=0x1486`, `s1_5a=0x78` (P2 KO'd) |
| the reaction | `r0=0x33` first at `f=0x155B`, 640 S records from there; `r0=0x27` at `f=0x14F7` immediately before |
| which entry | the second attempted entry, TALON's `0x16` (reaction `0x33`; the first, `0x15`/`0x32`, did not appear) |
| the result screen's exit | mode `0x17` at `f=0x16B0` |
| the conquered lands | mode `0x12` at `f=0x17A1` |
| the X record | `f=0x17A1` (the end frame); `f=0x17DD` is `signal_f`, the 60-frame stop tail |

`gp_twop.py check --finisher` (no allowance): two-human match ok, mode 9 reached,
mode 0x12 reached, reactions `['33', ...]`; exit 0.

### The pins and the port divergence

`gp_compare.py --report --max-unexplained 0` on the 4,419-frame port dump:

- **window** from capture 106 (raw 1749); **5,346 classified**: 2,944 clean,
  1,821 splice, 4 transition, 577 unexplained, and 12 all-black.
- **first unexplained** capture **4095** (raw 6783), nearest port 3442, rows
  0..2, x 0..319, 510 px. It is neither a loader overlay nor a scan-out (the
  loader's 166-px six-row strip is capture 5461, after the pin), so no allowance is
  added; the capture tail from 4095 is not claimed.
- **reach** 3439 port frames before the first unexplained.
- **trace** first difference `f=0x1549` (5449) in `rng` (capture `75B49A54`, port
  `BA99085D`); the first tick difference `f=0x147` is reported apart.
- **moves** first difference `f=0x155B` (5467) in `r0`: the port leaves `r0=0x27`
  where the capture advances to `0x33` (the capture also has `0x27` at `f=0x14F7`).
  **The port's replay does not advance to the finisher reaction**: the capture's
  claim is the game's reaction, and the moves ratchet stops below it. This is a
  measured port divergence, named here and not fixed by Task 2.

The `gp-u23-oracle` pins: `GP_U23_MIN_FIRST=4095`, `TRACE_MIN_FIRST=5449`,
`MOVES_MIN_FIRST=5467`, `MAX_START=106`, `MIN_REACH=3439`,
`CAPTURE_SHA256=6684d3cbb8cfabeed190b1139fa9cc93d69944c6d2b3ef9f9f56db2e0cb34446`,
`CAPTURE_FRAMES=5464`. Each one-step move was run and fails: N+1 (4096), F+1
(5450), M+1 (5468), start-1 (105), reach+1 (3440), one sha byte, and frames 5465.

The old `gp-u16-twop2` scenario, its pins and its `--allow-missing-reaction`
target are unchanged; `tools/gp_twop.py` is unchanged.

---

## §E3 The TALON ending capture (Task 3)

### The helpers and the scenario

`gp_session.ending_menu(cell)` returns `WIN_MENU`'s prefix, then `cell` `p1.right`
presses (the first at mode 0x10 + 60 frames, the rest 30 apart, hold 4 — `WIN_MENU1`'s
harness values) and the confirm. `cell == 0` is `WIN_MENU` itself and `cell == 3` is
`WIN_MENU1` step for step; both equalities are unit-tested, and `WIN_MENU1` and
`SCENARIOS['gp-u13-ending2']` are untouched. `gp_win.start_char(char, cell)` builds the
character-select and round-1 rows with the character and the cursor in the label and
`_all(c0=char, b1e=1)`; it is `_START` for `(0, 0)` and `_START1` for `(1, 3)` when
compared row-wise (each call builds a new lambda, so tuple identity is not comparable).

`SCENARIOS['gp-u18-talon-ending']` is `ending_menu(1)` (one `p1.right` from cell 0),
then 10 frames into the first mode-6 entry the poke `0x108106 = 0x82 x 7` and
`0x10789E = 0x78`, then `SCENARIOS['gp-u10-ending']['steps'][len(WIN_MENU) + 1:]` —
the round-2 KO, the final's 14 pokes and the `until_mode 0x03` end: the U13
arrangement with cell 1. It is in `STOP_AT_END`; `time_limit` 240, `WIN_EXTRA` and
`memsize=64` are u10's (record §W.13).

`MILESTONES['gp-u18-talon-ending']` is `start_char(2, 1)`, the round-1 KO row that
expects the seven `0x82` marks, `_MATCH1[1:]` and `_ending_tail(2, 'TALON')`. The
seventh-land row is the existing `0x41C28 state 3` row (`t104 & 0xFF == 7`), not the
mark row; the mode-0x1F row is `_all(ad4=0, c0=2)` under the label `TALON's ending`
(the character name). The title is a `0xA80E4` fact — id `0xE0`, TALON'S DREAM
(§E1.1) — and the compared frames' content, not a WIN/S field.

### The capture (2026-10-23)

`xvfb-run -a make gp-capture scenario=gp-u18-talon-ending` (this host has no X
display). Every session check is `ok`:

```text
snapshots 9972, f 4..271D, 38 frames missed
CHECK base / steps fired 21/21 / end frame reached / mode 0x27 after the Enter /
      snapshots kb == raw (0 differ) / frames written 5749/5749 / port script v2 /
      no unscripted input / pokes written 17/17, 0 raced / stopped at the end
      (SIGTERM at f=271D, rc=0): ok
wrote 5749 frames (raw 1389..14354), wall 205.2 s
```

`python3 tools/gp_capture.py check-input` prints `check=ok no unscripted input`. The
21 fired steps are the 5 menu steps plus the 16 cloned u10 steps (the trailing
`until_mode` is not an action).

Measured in its `poll.log` (9,972 S records):

| Fact | Value |
|---|---|
| the first poke | `W f=04A4 step=5 addr=00108106 was=..00 now=82828282828282 late=0 race=0` and `0x0010789E was=00 now=78` |
| round 1's mode-6 entry | `f=0x49B`, `c0=2 b1e=1` |
| round 1's KO (mode 8) | `f=0x4A7`, `m106=82828282 m10a=00828282` (TALON's marks) |
| the conquered lands | mode `0x12` at `f=0xBBE`; the seventh land (`t104=7`) at `f=0xC5F` |
| WORLD DOMINATION | `c82=1` at `f=0xCAB` |
| mode 0xF | `f=0x1544` |
| the ending | mode `0x1F` at `f=0x1816` with `ad4=0 c0=2`; state 3 at `f=0x1DB8` |
| the high-score entry | mode `0x1E` at `f=0x217E` |
| back in mode 3 / the X record | `f=0x26E6` |

`gp_win.py check` prints every row `ok` and `30/30 milestones ok`, including
`TALON's ending (mode 0x1F, 0x208F8): ... is character c0=2 at f=1816`.

### The report and the allowances

`gp_compare.py --report --max-unexplained 0` on the 4,865-frame port dump:

- **window** from capture 85 (raw 1748); 5,642 classified: 3,681 clean, 1,935
  splice, 9 transition, 2 scan3-allowed, 1 overlay-allowed, 14 unexplained,
  and 22 all-black.
- **first unexplained** capture **5735** (raw 14294), nearest port 0: 5735..5748
  are the post-script tail (the capture's last frame; the dump's last frame is
  `f=0x26E4` and the trace's and the capture's X `f=0x26E6`) and are NOT claimed.
- **reach** port 4864 (the dump's last frame).
- **trace / win / moves**: 9,610 frames compared from `f=0x137`, 38 without a
  capture snapshot, **0 differing through 9958**; the first tick difference is
  `f=0x138` (reported apart); the normalised `ent` 0 of 9,610 differ and `t508`
  7,713 of 9,610 (port = capture + 1), both reported.
- the mode-0x1F frames are inside the window: the ending's S records map to
  capture ≈5014 (`f=0x1816`) and ≈5245 (`f=0x1DB8`) by the capture's own clock
  (`ms * 70.09 / 1000`), both below 5735.

Three named per-frame allowances, each its own byte recipe (`tools/gp_compare.py`)
and none for a frame with a possible port divergence — the neighbours are clean at
the bound port frames in every case:

| capture (raw) | class | recipe (measured from the dump's `frames.txt`) |
|---|---|---|
| 2227 (4988) | overlay | port 1810's (`f=0BBC tick=C12 mode=17`) 166 glyph px, rows 192..197 x 0..85, over port 1811's (`f=0BBC tick=C18 mode=12`) mode-0x12 screen; the mask from port 1809 (`f=0BBC tick=C12 mode=17`, the stale-palette twin). The §G7 loader presentation at the mode-0x17 -> 0x12 entry. |
| 4099 (7759) | scan3 | rows 0..63 port 3430's (`f=14CC tick=155B mode=D`; 3428 `f=14CB` and 3429 identical), rows 64..167 port 3431's (`f=14CC tick=155F mode=C`), rows 168..199 port 3432's (`f=14CD tick=1560 mode=C`); pair (3431, 3432) unique. The final's mode-0x0D -> 0x0C transition (the U13 4248 class). |
| 4147 (7856) | scan3 | rows 0..45 port 3474's (`f=14F4 tick=15B3 mode=D`), rows 46..168 port 3475's (`f=14F4 tick=15B6 mode=C`), rows 169..199 port 3476's (`f=14F5 tick=15B7 mode=C`); pair (3475, 3476) unique; the split forced by the bytes. |

The port frame indices are the dump's (`frames.txt`), not the trace's records: the
two differ because the driver dumps one frame per present.

Mutation runs on the real dump (`gp_compare.overlay_explain` / `scan3_explain`
called on the loaded frames): changing S 1811's pixel (0,0) fails ("its pixel row 0
x 0 is 000000, not port 1811's 010203"); changing T1 1810's glyph pixel fails ("its
glyph pixels are b1f9319c813a.."); naming S as T2 fails ("its glyph pixels are
a2bc55c0b71a.."); changing the mid-bind frame's row (3431 row 64, 3475 row 46) or
the bottom frame's first row (3432 row 168, 3476 row 169) fails ("no unique adjacent
port pair binds ..."); changing every earlier top holder's row 0 fails ("no port
frame before 3431/3475 holds its named top rows"); a wrong top hash fails ("its top
rows 0..63 are 2fb5ec8f9b74.."). The only surviving edit is a T2 byte *inside* the
mask with the `!=` relation intact — the documented named limit of §G7 (T2 is only
the mask's indicator).

### The pins

`GP_U18_MIN_FIRST=5735`, `TRACE_MIN_FIRST=9959`, `MAX_START=85`, `MIN_REACH=4864`,
`CAPTURE_SHA256=b3279414514aee07d2d4cddada0b63c7ec90f68d905f6509deb3e5c955d2fcf1`,
`CAPTURE_FRAMES=5749`, `MILESTONES=30`, `WIN_MIN_FIRST=9959`. Each one-step move was
run through `make gp-u18-oracle` and fails:

| move | failure |
|---|---|
| MIN_FIRST 5736 | `FAIL: first unexplained 5735 < ratchet N 5736` |
| TRACE_MIN_FIRST 9960 | `FAIL: N 9960 > end 9959: N is unreachable` |
| WIN_MIN_FIRST 9960 | `FAIL: win: N 9960 > end 9959: N is unreachable` |
| MAX_START 84 | `FAIL: window starts at capture 85 (raw 1748) > pinned start 84` |
| MIN_REACH 4865 | `FAIL: window reach port 4864 < pinned reach 4865` |
| sha (last nibble) | `FAIL: poll.log sha256 ... != the pinned ...: re-measure, then re-pin` |
| CAPTURE_FRAMES 5750 | `FAIL: poll.log sha256 ... (5749 frames) != the pinned ... (5750 frames)` |
| MILESTONES 31 | `FAIL: path: N 31 > end 30: N is unreachable` |

`make gp-u18-oracle` passes at the pins (30/30 milestones, the three allowance lines,
first unexplained 5735, reach 4864, 0 differing through 9958) and is wired into
`make verify` after `gp-u23-oracle`. The `PR_GP_DUMP` miss set for the new scenario is
the same §G.24 pair `0x29D60`/`0x5D812` from `frontend_mode_1b_step` (hits 1 and 3),
pinned in `port/tests/test_platform.c`.

No conflict with §E1 was found: cell 1 is class 2 TALON, the mark is `0x82`, the
title id is `0xE0` (TALON'S DREAM) and the label is the character name. Review
correction (2026-10-23): `gp_win.py`'s shared `_ENDING_LABEL` said `0x20900..0x20914`
where §E1 measured the load at `0x20914..0x2091A`; it now names the whole read,
`0x20900..0x2091A`. The label is shared by every ending scenario, so each ending
oracle's printed label names the full range; no pin compares the label text.

---

## §E4 The VERTIGO ending capture (Task 4)

### The scenario and the milestones

`SCENARIOS['gp-u19-vertigo-ending']` (in `STOP_AT_END`) is P1's confirm of
character-select cell 2 — `ending_menu(2)`, Task 3's helper: two `p1.right` presses
(the first at mode 0x10 + 60, the second 30 later, hold 4) and the confirm — which
`0xC8882 = 00 02 03 01 06 04 05`, indexed by the cursor at `0x43D6E`/`0x43D74`,
maps to class 3 VERTIGO (§E1.1). The first poke is
`(0x108106, lands_marked(0, 3))` = `0x83 x 7` and `0x10789E = 0x78` at
`after_entry 0x06, 1, 10`; the rest is `SCENARIOS['gp-u10-ending']['steps']`
from index `len(WIN_MENU) + 1` (the round-2 KO, the final's 14 pokes and the
`until_mode 0x03` end). `time_limit=240`, `WIN_EXTRA` and `memsize=64` are u10's
(record §W.13); neither `WIN_MENU` nor u10's steps are mutated.

`MILESTONES['gp-u19-vertigo-ending']` is `start_char(3, 2)` (the label names
character 3 / cursor 2, the predicate `_all(c0=3, b1e=1)`), the round-1 KO row
whose seven-lands predicate is `[0x80 | r['c0']] * 7` (measured `0x83` on the
capture), `_MATCH1[1:]` and `_ending_tail(3, 'VERTIGO')`. The seventh-land row is
the inherited `0x41C28 state 3` row (`r['t104'] & 0xFF == 7`), not the mark row.
The mode-0x1F row is `_all(ad4=0, c0=3)` under the label `VERTIGO's ending`; the
title is a `0xA80E4` fact — id `0xE1`, THE PALACE OF VERTIGO (§E1.1) — and the
compared frames' content, not a WIN/S field.

### The capture (2026-10-23)

`xvfb-run -a make gp-capture scenario=gp-u19-vertigo-ending` (this host has no X
display). Every session check is `ok`:

```text
snapshots 10023, f 4..2753, 41 frames missed
CHECK base / steps fired 22/22 / end frame reached / mode 0x27 after the Enter /
      snapshots kb == raw (0 differ) / frames written 6004/6004 / port script v2 /
      no unscripted input / pokes written 17/17, 0 raced / stopped at the end
      (SIGTERM at f=2753, rc=0): ok
wrote 6004 frames (raw 1391..14418), wall 206.1 s
```

`python3 tools/gp_capture.py check-input` prints `check=ok no unscripted input`.
The 22 fired steps are the 7 menu actions (4 `WIN_MENU` steps, the two `p1.right`
presses and the confirm) plus the 15 cloned u10 actions (the trailing
`until_mode` is not an action).

Measured in its `poll.log` (10,023 S records):

| Fact | Value |
|---|---|
| the first poke | `W f=04C2 step=6 addr=00108106 was=..00 now=83838383838383 late=0 race=0` and `0x0010789E was=00 now=78` |
| round 1's mode-6 entry | `f=0x4B9`, `c0=3 b1e=1` |
| round 1's KO (mode 8) | `f=0x4C6`, the seven `0x83` marks |
| the conquered lands | mode `0x12` at `f=0xBDD`; the seventh land (`t104=7`) at `f=0xC7D` |
| WORLD DOMINATION | `c82=1` at `f=0xCC9` |
| mode 0xF | `f=0x157A` |
| the ending | mode `0x1F` at `f=0x184C` with `ad4=0 c0=3`; state 3 at `f=0x1DEE` |
| the high-score entry | mode `0x1E` at `f=0x21B4` |
| back in mode 3 | `f=0x271B` (the X record) |

`gp_win.py check` prints every row `ok` and `30/30 milestones ok`, including
`VERTIGO's ending (mode 0x1F, 0x208F8): ... is character c0=3 at f=184C`.

### The report and the allowances

`gp_compare.py --report --max-unexplained 0` on the 4,990-frame port dump:

- **window** from capture 85 (raw 1750); 5,897 classified: 3,384 clean, 2,487
  splice, 9 transition, 3 scan3-allowed, 1 overlay-allowed, 13 unexplained, and
  22 all-black (the enforced run stops at the first unexplained, hence its 5,885).
- **first unexplained** capture **5991** (raw 14362), nearest port 0: 5991..6003
  are the post-script tail (the replay's dump ends at port frame 4989, f=0x271A,
  and its trace at f=0x271B, where the capture's X record sits) and are NOT
  claimed.
- **reach** port 4989 (the dump's last frame).
- **trace / win / moves**: 9,660 frames compared from `f=0x137`, 41 without a
  capture snapshot, **0 differing through 10011**; the first tick difference is
  `f=0x138` (reported apart); the normalised `ent` 0 of 9,660 differ and `t508`
  6,849 of 9,660 (port = capture + 1), both reported.

Four named per-frame allowances, each its own byte recipe (`tools/gp_compare.py`):

| capture (raw) | class | recipe (measured from the dump's `frames.txt`) |
|---|---|---|
| 2312 (5021) | overlay | port 1872's (`f=0BDA tick=C2B mode=17`) 166 glyph px, rows 192..197 x 0..85, over port 1873's (`f=0BDA tick=C31 mode=12`) mode-0x12 screen; the mask from port 1871 (`f=0BDA tick=C2B mode=17`, the stale-palette twin). The §G7 loader presentation at the mode-0x17 -> 0x12 entry; 2311 is clean at 1872 and 2313 clean at 1873. T1 1872/T2 1871 is the only pair at mask 166 reproducing the frame's pixels. |
| 2478 (5662) | scan3 | rows 0..32 held by port 177 and 2020..2022 (not uniquely bound, the §G6 named limit), rows 33..147 port 2023's (`f=0DFC tick=E56 mode=23`), rows 148..199 port 2024's (`f=0DFD tick=E57 mode=23`); pair (2023, 2024) unique; neighbours 2477 clean at 2022 and 2479 clean at 2024. The health-bonus screen's three-present scan. |
| 4362 (7863) | scan3 | rows 0..52 port 3577's (`f=1516 tick=15B3 mode=D`, its only holder), rows 53..117 port 3578's (`f=1516 tick=15B7 mode=C`), rows 118..199 port 3579's (`f=1517 tick=15B8 mode=C`); pair (3578, 3579) unique; neighbours 4361 clean at 3577 and a (3579, ...) splice at 4363. The final's mode-0x0D -> 0x0C transition (the U13/U18 class). |
| 4430 (7961) | scan3 | rows 0..11 port 3640's (`f=1552 tick=160B mode=C`, its only holder), rows 12..145 port 3641's (`f=1552 tick=160D mode=C`), rows 146..199 port 3642's (`f=1553 tick=160E mode=C`); pair (3641, 3642) unique; neighbours 4429 clean at 3640 and a (3642, ...) splice at 4431. No mode change here: the replacement screen's next three-present scan. |

In every scan3 recipe the `r2` split is forced by the bytes (the first row below
it exists only in the bottom frame, and `r2 + 1` fails to bind); the top rows are
byte-pinned by the hash and required only to have an earlier holder (uniqueness is
not claimed — `2478` has four).

Mutation runs on the real dump (`gp_compare.overlay_explain` / `scan3_explain`
called on the loaded frames): changing S 1873's pixel (0,0) fails ("its pixel row
0 x 0 is 010203, not port 1873's 000000"); swapping T1/T2 fails ("its glyph
pixels are 43fead2f4cfc.."); naming S as the mask fails ("its glyph pixels are
a2bc55c0b71a.."); changing a T1 glyph pixel fails ("its pixel row 192 x 0 is
010203, not port 1872's 41b2d3"). For each scan3 recipe, changing the top row 0,
a middle row (`r1`), a bottom row (`r2`) or moving the `r2` split one row each
fails (the top by hash, the rest by "no unique adjacent port pair binds ...");
moving `r1` one row up fails by the top hash. All 19 mutations are caught.

### The pins

`GP_U19_MIN_FIRST=5991`, `GP_U19_TRACE_MIN_FIRST=10012`,
`GP_U19_MAX_START=85`, `GP_U19_MIN_REACH=4989`,
`GP_U19_CAPTURE_SHA256=31da347f1e637e3d384936e7b9473cfba7734f8a4216af00236e74d0c730fd69`,
`GP_U19_CAPTURE_FRAMES=6004`, `GP_U19_MILESTONES=30`,
`GP_U19_WIN_MIN_FIRST=10012`. Each one-step move fails:

| move | failure |
|---|---|
| MIN_FIRST 5992 | `FAIL: first unexplained 5991 < ratchet N 5992` |
| TRACE_MIN_FIRST 10013 | `FAIL: N 10013 > end 10012: N is unreachable` |
| WIN_MIN_FIRST 10013 | `FAIL: win: N 10013 > end 10012: N is unreachable` |
| MAX_START 84 | `FAIL: window starts at capture 85 (raw 1750) > pinned start 84` |
| MIN_REACH 4990 | `FAIL: window reach port 4989 < pinned reach 4990` |
| sha (last nibble) | `FAIL: poll.log sha256 ... != the pinned ...: re-measure, then re-pin` |
| CAPTURE_FRAMES 6005 | `FAIL: ... (6004 frames) != the pinned ... (6005 frames)` |
| MILESTONES 31 | `FAIL: path: N 31 > end 30: N is unreachable` |

`make gp-u19-oracle` passes at the pins (30/30 milestones, the four allowance
lines, first unexplained 5991, reach 4989, `0 differing through 10011`) and its
run without `GP_WIN_KEEP` removes the dump; a run with `GP_U19_MIN_FIRST=5992`
exits 2 at the frames FAIL. It is wired into `make verify` after
`gp-u18-oracle`. The `PR_GP_DUMP` miss set for the new scenario is the measured
§G.24 pair `0x29D60` (one hit) / `0x5D812` (three hits) from
`frontend_mode_1b_step`, pinned in `port/tests/test_platform.c`, and
`test_gp_capture`'s poked-final list gains the scenario (`memsize=64`).

No conflict with §E1 was found: cell 2 is class 3 VERTIGO, the mark is `0x83`,
the title id is `0xE1` (THE PALACE OF VERTIGO) and the label is the character
name. The old scenarios, `WIN_MENU1`, the U13/U18 recipes and every existing pin
are unchanged.

---

## §E5 The CHAOS ending capture (Task 5)

### The scenario and the milestones

`SCENARIOS['gp-u20-chaos-ending']` (in `STOP_AT_END`) is P1's confirm of
character-select cell 4 — `ending_menu(4)`, Task 3's helper: four `p1.right`
presses (the first at mode 0x10 + 60, the rest 30 apart, hold 4) and the
confirm — which `0xC8882 = 00 02 03 01 06 04 05`, indexed by the cursor at
`0x43D6E`/`0x43D74`, maps to class 6 CHAOS (§E1.1). The first poke is
`(0x108106, lands_marked(0, 6))` = `0x86 × 7` and `0x10789E = 0x78` at
`after_entry 0x06, 1, 10`; the rest is `SCENARIOS['gp-u10-ending']['steps']`
from index `len(WIN_MENU) + 1` (the round-2 KO, the final's 14 pokes and the
`until_mode 0x03` end). `time_limit=240`, `WIN_EXTRA` and `memsize=64` are u10's
(record §W.13); neither `WIN_MENU` nor u10's steps are mutated.

`MILESTONES['gp-u20-chaos-ending']` is `start_char(6, 4)` (the label names
character 6 / cursor 4; the predicate `_all(c0=6, b1e=1)`), the round-1 KO row
whose seven-lands predicate is `[0x80 | r['c0']] * 7` (measured `0x86` on the
capture), `_MATCH1[1:]` and `_ending_tail(6, 'CHAOS')`. The seventh-land row is
the inherited `0x41C28 state 3` row (`r['t104'] & 0xFF == 7`), not the mark row.
The mode-0x1F row is `_all(ad4=0, c0=6)` under the label `CHAOS's ending`; the
title is a `0xA80E4` fact — id `0xE4`, CHAOS' REDEMPTION (§E1.1) — and the
compared frames' content, not a WIN/S field.

### The capture (2026-10-23)

`xvfb-run -a make gp-capture scenario=gp-u20-chaos-ending` (this host has no X
display). Every session check is `ok`:

```text
snapshots 10078, f 4..278C, 43 frames missed
CHECK base / steps fired 24/24 / end frame reached / mode 0x27 after the Enter /
      snapshots kb == raw (0 differ) / frames written 5790/5790 / port script v2 /
      no unscripted input / pokes written 17/17, 0 raced / stopped at the end
      (SIGTERM at f=278C, rc=0): ok
wrote 5790 frames (raw 1396..14493), wall 207.2 s
```

`python3 tools/gp_capture.py check-input` prints `check=ok no unscripted input`.
The 24 fired steps are the 8 menu steps (the `boot`/`after`/`after_mode` prefix,
the four `p1.right` presses and the confirm), the scenario's own first poke and
the 15 cloned u10 steps (the round-2 KO and the 14 final pokes); the trailing
`until_mode` is the 25th step and is not counted as a fired step (the harness's
`total` excludes it).

Measured in its `poll.log` (10,078 S records):

| Fact | Value |
|---|---|
| the first poke | `W f=04FA step=8 addr=00108106 was=..00 now=86868686868686 late=0 race=0` and `0x0010789E was=00 now=78` |
| round 1's mode-6 entry | `f=0x4F1`, `c0=6 b1e=1` |
| round 1's KO (mode 8) | `f=0x4FE`, the seven `0x86` marks |
| the conquered lands | mode `0x12` at `f=0xC14`; the seventh land (`t104=7`) at `f=0xCB5` |
| WORLD DOMINATION | `c82=1` at `f=0xD01` |
| the health bonus | mode `0x23` at `f=0xE33`, mode `0x22` at `f=0xEAE`, mode `0x24` at `f=0x1358` |
| the final | mode `0xC` at `f=0x1524`; mode 0xF at `f=0x15B3` |
| the ending | mode `0x1F` at `f=0x1885` with `ad4=0 c0=6`; state 3 at `f=0x1E27` |
| the high-score entry | mode `0x1E` at `f=0x21ED` |
| back in mode 3 | `f=0x2755` (the X record) |

`gp_win.py check` prints every row `ok` and `30/30 milestones ok`, including
`CHAOS's ending (mode 0x1F, 0x208F8): ... is character c0=6 at f=1885`.

### The report and the allowances

`gp_compare.py --report --max-unexplained 0` on the 4,891-frame port dump:

- **window** from capture 83 (raw 1750); 5,684 classified: 3,399 clean, 2,259
  splice, 8 transition, 3 scan3-allowed, 1 overlay-allowed, 14 unexplained, and
  23 all-black (the enforced run stops at the first unexplained, hence its
  5,671).
- **first unexplained** capture **5776** (raw 14433), nearest port 0: 5776..5789
  are the post-script tail (the replay's dump ends at port frame 4890, f=0x2753,
  and its trace at f=0x2755, where the capture's X record sits) and are NOT
  claimed.
- **reach** port 4890 (the dump's last frame).
- **trace / win / moves**: 9,719 frames compared from `f=0x134`, 43 without a
  capture snapshot, **0 differing through 10069**; the first tick difference is
  `f=0x135` (reported apart); the normalised `ent` 0 of 9,719 differ and `t508`
  6,563 of 9,719 (port = capture + 1), both reported.

Four named per-frame allowances, each its own byte recipe (`tools/gp_compare.py`):

| capture (raw) | class | recipe (measured from the dump's `frames.txt`) |
|---|---|---|
| 381 (2971) | scan3 | row 0 held by port 260 (`f=04FB tick=551 mode=08`) alone among the earlier frames, rows 1..157 port 263's (`f=04FE tick=552 mode=08`), rows 158..199 port 264's (`f=04FF tick=553 mode=08`); pair (263, 264) unique; neighbours 380 clean at 260 and 382 a (264, …) splice. Round 1's mode-8 three-present scan — the first present bunching after the opening. |
| 2180 (5094) | overlay | port 1781's (`f=0C12 tick=C65 mode=17`) 166 glyph px, rows 192..197 x 0..85, over port 1782's (`f=0C12 tick=C6B mode=12`) mode-0x12 screen; the mask from port 1780 (`f=0C12 tick=C65 mode=17`, the same tick's second present, the stale-palette twin). The §G7 loader presentation at the mode-0x17 → 0x12 entry; 2179 is clean at 1781 and 2181 clean at 1782. T1 1781/T2 1780 is the only pair at mask 166 reproducing the frame's pixels. |
| 2350 (5736) | scan3 | rows 0..24 held by the earlier ports 193..196, 1039 and 1926..1931 (not uniquely bound, the §G6 named limit), rows 25..139 port 1932's (`f=0E34 tick=E90 mode=23`), rows 140..199 port 1933's (`f=0E35 tick=E91 mode=23`); pair (1932, 1933) unique; neighbours 2349 clean at 1931 and 2351 clean at 1933. The health-bonus screen's three-present scan (U19's 2478 class). |
| 4272 (8091) | scan3 | rows 0..18 port 3566's (`f=159F tick=1674 mode=D`), rows 19..90 port 3567's (`f=159F tick=1678 mode=C`), rows 91..199 port 3568's (`f=15A0 tick=1679 mode=C`); pair (3567, 3568) unique; neighbours 4271 clean at 3566 and 4273 a (3568, …) splice. The final's mode-0x0D → 0x0C transition (the U13/U18/U19 class). |

In every scan3 recipe the `r2` split is forced by the bytes (the first row below
it exists only in the bottom frame, and `r2 ± 1` fails to bind); the top rows are
byte-pinned by the hash and required only to have an earlier holder (uniqueness is
not claimed — `2350` has eleven).

Mutation runs on the real dump (`overlay_explain` / `scan3_explain` called on the
loaded frames): changing S 1782's pixel (0,0) fails ("its pixel row 0 x 0 is
010203, not port 1782's 000000"); swapping T1/T2 fails ("its glyph pixels are
43fead2f4cfc.."); naming S as the mask fails ("its glyph pixels are
a2bc55c0b71a.."); changing a T1 glyph pixel fails ("its pixel row 192 x 0 is
010203, not port 1781's 41b2d3"). For each scan3 recipe, changing the top row 0,
a middle row (`r1`), a bottom row (`r2`) or moving the `r2` split one row each
fails (the top by hash, the rest by "no unique adjacent port pair binds …"); for
381 moving `r1` to 2 fails by the top hash (row 1 is the middle frame's; the
`r1 - 1` move is degenerate at `r1 = 1` and is not used) and for 2350/4272 moving
`r1` up one row also fails by the top hash. All 22 mutations are caught.

### The pins

`GP_U20_MIN_FIRST=5776`, `GP_U20_TRACE_MIN_FIRST=10070`,
`GP_U20_MAX_START=83`, `GP_U20_MIN_REACH=4890`,
`GP_U20_CAPTURE_SHA256=73b5bb808dee603c1e5a359e2716c9654db945737a799cee30c00762699ca7cb`,
`GP_U20_CAPTURE_FRAMES=5790`, `GP_U20_MILESTONES=30`,
`GP_U20_WIN_MIN_FIRST=10070`. Each one-step move was run through the target's own
tools (with the dump kept) and fails:

| move | failure |
|---|---|
| MIN_FIRST 5777 | `FAIL: first unexplained 5776 < ratchet N 5777` (also through `make gp-u20-oracle GP_U20_MIN_FIRST=5777`, exit 2) |
| TRACE_MIN_FIRST 10071 | `FAIL: N 10071 > end 10070: N is unreachable` |
| WIN_MIN_FIRST 10071 | `FAIL: win: N 10071 > end 10070: N is unreachable` |
| MAX_START 82 | `FAIL: window starts at capture 83 (raw 1750) > pinned start 82` |
| MIN_REACH 4891 | `FAIL: window reach port 4890 < pinned reach 4891 (the port exhibited fewer port frames)` |
| sha (last nibble) | `FAIL: poll.log sha256 … != the pinned …: re-measure, then re-pin` |
| CAPTURE_FRAMES 5791 | `FAIL: … (5790 frames) != the pinned … (5791 frames)` |
| MILESTONES 31 | `FAIL: path: N 31 > end 30: N is unreachable` |

`make gp-u20-oracle` passes at the pins (30/30 milestones, the four allowance
lines, first unexplained 5776, reach 4890, `0 differing through 10069`) and is
wired into `make verify` after `gp-u19-oracle`. The `PR_GP_DUMP` miss set for the
new scenario is the measured §G.24 pair `0x29D60` (one hit) / `0x5D812` (three
hits) from `frontend_mode_1b_step`, pinned in `port/tests/test_platform.c`, and
`test_gp_capture`'s poked-final list gains the scenario (`memsize=64`).

No conflict with §E1 was found: cell 4 is class 6 CHAOS, the mark is `0x86`,
the title id is `0xE4` (CHAOS' REDEMPTION) and the label is the character name.
The old scenarios, `WIN_MENU1`, the U13/U18/U19 recipes and every existing pin
are unchanged.

---

## §E6 The ARMADON ending capture (Task 6)

### The scenario and the milestones

`SCENARIOS['gp-u21-armadon-ending']` (in `STOP_AT_END`) is P1's confirm of
character-select cell 5 — `ending_menu(5)`, Task 3's helper: five `p1.right` presses
(the first at mode 0x10 + 60, the rest 30 apart, hold 4) and the confirm — which
`0xC8882 = 00 02 03 01 06 04 05`, indexed by the cursor at `0x43D6E`/`0x43D74`, maps to
class 4 ARMADON (§E1.1). The first poke is `(0x108106, lands_marked(0, 4))` = `0x84 × 7`
and `0x10789E = 0x78` at `after_entry 0x06, 1, 10`; the rest is
`SCENARIOS['gp-u10-ending']['steps']` from index `len(WIN_MENU) + 1` (the round-2 KO, the
final's 14 pokes and the `until_mode 0x03` end). `time_limit=240`, `WIN_EXTRA` and
`memsize=64` are u10's (record §W.13); neither `WIN_MENU` nor u10's steps are mutated.

`MILESTONES['gp-u21-armadon-ending']` is `start_char(4, 5)` (the label names character
4 / cursor 5; the predicate `_all(c0=4, b1e=1)`), the round-1 KO row whose seven-lands
predicate is `[0x80 | r['c0']] * 7` (measured `0x84` on the capture), `_MATCH1[1:]` and
`_ending_tail(4, 'ARMADON')`. The seventh-land row is the inherited `0x41C28 state 3` row
(`r['t104'] & 0xFF == 7`), not the mark row. The mode-0x1F row is `_all(ad4=0, c0=4)`
under the label `ARMADON's ending`; the title is a `0xA80E4` fact — id `0xE2`,
THE MEDITATION OF ARMADON (§E1.1) — and the compared frames' content, not a WIN/S
field.

### The capture (2026-10-23)

`xvfb-run -a make gp-capture scenario=gp-u21-armadon-ending` (this host has no X
display). Every session check is `ok`:

```text
snapshots 9953, f 5..278C, 167 frames missed
CHECK base / steps fired 25/25 / end frame reached / mode 0x27 after the Enter /
      snapshots kb == raw (0 differ) / frames written 5946/5946 / port script v2 /
      no unscripted input / pokes written 17/17, 0 raced / stopped at the end
      (SIGTERM at f=278C, rc=0): ok
wrote 5946 frames (raw 1414..14516), wall 208.0s
```

`python3 tools/gp_capture.py check-input` prints `check=ok no unscripted input`. The 25
fired steps are the 9 menu steps (the `boot`/`after`/`after_mode` prefix, the five
`p1.right` presses and the confirm), the scenario's own first poke and the 15 cloned u10
steps; the trailing `until_mode` is the 26th step and is not counted as a fired step
(the harness's `total` excludes it — the X record says `step=26`).

Measured in its `poll.log` (9,953 S records):

| Fact | Value |
|---|---|
| the first poke | `W f=0504 step=9 addr=00108106 was=..00 now=84848484848484 late=0 race=0` and `0x0010789E was=00 now=78` |
| round 1's mode-6 entry | `f=0x4FB`, `c0=4 b1e=1` |
| round 1's KO (mode 8) | `f=0x508`, the seven `0x84` marks |
| the conquered lands | mode `0x12` at `f=0xC1E`; the seventh land (`t104=7`) at `f=0xCBF` |
| WORLD DOMINATION | `c82=1` at `f=0xD0B` |
| the health bonus | mode `0x23` at `f=0xE3D`, mode `0x22` at `f=0xEB8`, mode `0x24` at `f=0x1358` |
| the final | mode `0xC` at `f=0x1524`; mode 0xF at `f=0x15B3` |
| the ending | mode `0x1F` at `f=0x1885` with `ad4=0 c0=4`; state 3 at `f=0x1E27` |
| the high-score entry | mode `0x1E` at `f=0x21ED` |
| back in mode 3 / the X record | `f=0x2755` (`X ms=206774 f=2755 step=26 end`, `E reason=end rc=0`) |

`gp_win.py check` prints every row `ok` and `30/30 milestones ok`, including
`ARMADON's ending (mode 0x1F, 0x208F8): ... is character c0=4 at f=1885`.

### The report and the allowances

`gp_compare.py --report --max-unexplained 0` on the 4,974-frame port dump:

- **window** from capture 90 (raw 1743); 5,836 classified: 3,598 clean, 2,149
  splice, 59 transition, 15 band-allowed, 1 overlay-allowed, 14 unexplained, and 20
  all-black (the enforced run stops at the first unexplained, hence its 5,823 and 1).
- **first unexplained** capture **5932** (raw 14456), nearest port 0: 5932..5945 are
  the post-script tail (the replay's dump ends at port frame 4973, `f=0x2753`, and its
  trace at `f=0x2755`, where the capture's X record sits) and are NOT claimed.
- **reach** port 4973 (the dump's last frame).
- **trace / win / moves**: 9,615 frames compared from `f=0x120`, 167 without a capture
  snapshot, **0 differing through 10069**; the first tick difference is `f=0x121`
  (reported apart); the normalised `ent` 0 of 9,615 differ and `t508` 6,466 of 9,615
  (port = capture + 1), both reported.
- the mode-0x1F frames are inside the window: the ending's S records map to capture
  **5193** (`f=0x1885`, `ms=128531` -> raw 9009, `window.txt`'s `05193 9009`) and **5431**
  (`f=0x1E27`, `ms=152834` -> raw 10712, `05431 10712`) by the capture's own clock
  (`ms * 70.09 / 1000`), both below 5932.

One loader overlay and fifteen present-copy bands, each its own byte recipe
(`tools/gp_compare.py`, `OVERLAY_ALLOWED`/`BANDS_ALLOWED`):

| capture (raw) | class | recipe (measured from the dump's `frames.txt`) |
|---|---|---|
| 2217 (5125) | overlay | port 1799's (`f=0C1C tick=C6D mode=17`) 166 glyph px, rows 192..197 x 0..85, over port 1800's (`f=0C1C tick=C73 mode=12`) mode-0x12 screen; the mask from port 1798 (`f=0C1C tick=C6D mode=17`, the stale-palette twin). The §G7 loader presentation at the mode-0x17 -> 0x12 entry; 2216 is clean at port 29 (the still-black early screen) and 2218 clean at 1800. |
| 426 (3002) | band | rows 0..57 port 291's (`f=0505 tick=557 mode=08`), 191..199 port 292's (`f=0506 tick=558 mode=08`), band 58..190; pair (291, 292) unique; neighbours 425 clean at 291, 427 clean at 295. Round 1's mode-8 screen. |
| 756 (3333) | band | rows 0..150 port 577's (`f=0623 tick=675 mode=08`), 154..199 port 578's (`f=0624`), band 151..153; pair (577, 578) unique; 755 clean at 577, 757 a splice. The same mode-8 screen. |
| 4317 (8037) | band | rows 0..3 port 3610's (`f=1577 tick=1636 mode=0C`), 32..199 port 3611's (`f=1578`), band 4..31; pair (3610, 3611) unique; 4316 clean at 3609, 4318 a splice. The final's mode-0x0D -> 0x0C entry (U19's 4362 / U20's 4272 class). |
| 4470 (8216) | band | rows 47..199 port 3741's (`f=15F8 tick=16D1 mode=0F`), band 0..46; pair (3740, 3741) unique by the bottom rows; 4469 clean at 3739, 4471 a transition. The mode-0xF master screen. |
| 4477 (8223) | band | rows 29..199 port 3747's (`f=15FE tick=16D7 mode=0F`), band 0..28; pair (3746, 3747); 4476 clean at 3745, 4478 a transition. |
| 4486 (8232) | band | rows 0..194 port 3753's (`f=1604 tick=16DD mode=0F`), band 195..199; pair (3753, 3754); 4485 a splice, 4487 clean at 3755. |
| 4491 (8237) | band | rows 11..199 port 3759's (`f=160A tick=16E3 mode=0F`), band 0..10; pair (3758, 3759); 4490 clean at 3757, 4492 a transition. |
| 4497 (8243) | band | rows 82..199 port 3764's (`f=160F tick=16E8 mode=0F`), band 0..81; pair (3763, 3764); 4496 clean at 3762, 4498 clean at 3764. |
| 4502 (8248) | band | rows 13..199 port 3768's (`f=1613 tick=16EC mode=0F`), band 0..12; pair (3767, 3768); 4501 clean at 3766, 4503 a splice. |
| 4505 (8251) | band | rows 0..4 port 3770's (`f=1615 tick=16EE mode=0F`), 32..199 port 3771's (`f=1616`), band 5..31; pair (3770, 3771); 4504 clean at 3769, 4506 a splice. |
| 4511 (8257) | band | rows 65..199 port 3776's (`f=161B tick=16F4 mode=0F`), band 0..64; pair (3775, 3776); 4510 clean at 3774, 4512 clean at 3776. |
| 4527 (8273) | band | rows 0..134 port 3788's (`f=1627 tick=1700 mode=0F`), band 135..199; pair (3788, 3789); 4526 clean at 3788, 4528 clean at 3790. |
| 4554 (8300) | band | rows 0..102 port 3811's (`f=163E tick=1717 mode=0F`), band 103..199; pair (3811, 3812); 4553 clean at 3811, 4555 clean at 3813. |
| 4560 (8306) | band | rows 0..109 port 3816's (`f=1643 tick=171C mode=0F`), band 110..199; pair (3816, 3817); 4559 clean at 3816, 4561 clean at 3818. |
| 5119 (8872) | band | rows 2..199 port 4301's (`f=182A tick=1903 mode=15`), band 0..1; pair (4300, 4301); 5118 clean at 4300, 5120 clean at 4302. The ending's mode-0x15 fade (U13's 5120 class). |

**Why these sixteen and not a port divergence.** The game's present copies the composed
320×200 frame into the aperture top to bottom; at 70.09 Hz the scan-out runs
asynchronously to the 60.05 Hz presents, so a scan that lands inside that copy reads the
*previous* present's rows above the copy's front, the *new* present's rows below it and
the front row mixed. The captured frame is therefore a state the driver never dumps — it
dumps one composed frame per present — which is `BANDS_ALLOWED`'s named class (§G5).
Measured per frame on this dump: its two segments are byte-equal to one unique adjacent
dumped pair, its band (as a row range, its front row mixed) is held by no single dumped
present, every neighbour is clean, a splice or a transition,
and **fourteen of the fifteen have all 64,000 pixels equal to a pixel of some port frame
within ±4 of that pair** (0 unmatched of 64,000 for 426, 756, 4317, 4470, 4477, 4486,
4491, 4497, 4502, 4505, 4511, 4527, 4554 and 4560) — a pure tear of two dumped presents.
The exception is 5119, the mode-0x15 fade: its rows 2..199 are port 4301's and its 640
band pixels break down as 215 equal to port 4301's (31 of those also equal port 4300's,
where the two frames coincide) and 425 equal to no frame within ±4 (no constant
per-channel offset reproduces them) — the fade's own step between the two presents,
U13's 5120 class. Capture 4470 is the worked example of the tear: rows
0..45 are port 3739's, rows 47..199 are port 3741's and row 46 is a 216-of-320-pixel
mixture of the two — port 3740 (the present between them) is nowhere on the frame,
because its whole copy fell between two scans. The mode-0xF screen is redrawn almost
entirely every iteration (consecutive dumped frames differ in ~45,000 pixels), which is
why eleven of the sixteen sit there; the earlier ending captures' scans simply missed
the copy windows (U19/U20 had none).

Mutation runs on the real dump (`gp_compare.band_explain` / `overlay_explain` called on
the loaded frames), 79 edits over the fifteen band entries with the range's own hash
where the range moved:

| edit (per entry) | result |
|---|---|
| one band byte in the capture (15) | caught: `its band rows a..b are 45723eeaec59.., not the named 5d3d8e35e3f5..` |
| one byte of a bound port frame's segment (15) | caught: `no unique adjacent port pair binds its rows [0,58)/[191,200)` |
| the band shrunk by one row at the top (`top_row+1`, its own hash) (15) | caught: `no unique adjacent port pair binds its rows [0,59)/[191,200)` |
| the band shrunk by one row at the bottom (`band_hi-1`, its own hash) (15) | caught: `no unique adjacent port pair binds its rows [0,58)/[190,200)` |
| the band grown by one row (`top_row-1` / `band_hi+1`, its own hash) (19 applicable) | explained: a wider band is a valid description of the same frame — the §G5 shape limit U13's 5120 names (the bounds follow the unbindable run's ends, not the model) |

The overlay's five mutations all fail: a masked capture pixel (`its pixel row 192 x 0 is
010203, not port 1799's 41b2d3`), a T1 glyph byte (`its glyph pixels are
b1f9319c813a.., not the named de1c1bb2b51d..`), an S screen pixel off the mask (`its
pixel row 0 x 100 is 000000, not port 1800's 070707`), T1/T2 swapped
(`43fead2f4cfc..`) and S named as T2 (`a2bc55c0b71a..`). Named limit: the later
mode-0x12 pair (1802, 1801) carries the same glyph bytes at the same 166 indices, so
T1's identity is not unique — T2 is only the mask's indicator — and the capture's
pixels are the same under either.

### The pins

`GP_U21_MIN_FIRST=5932`, `GP_U21_TRACE_MIN_FIRST=10070`,
`GP_U21_MAX_START=90`, `GP_U21_MIN_REACH=4973`,
`GP_U21_CAPTURE_SHA256=41e384a94049fc5c1c74934c802153dcdee230631d44522ef03f27935db1b42f`,
`GP_U21_CAPTURE_FRAMES=5946`, `GP_U21_MILESTONES=30`,
`GP_U21_WIN_MIN_FIRST=10070`. Each one-step move was run through the target's own
tools (with the dump kept) and fails:

| move | failure |
|---|---|
| MIN_FIRST 5933 | `FAIL: first unexplained 5932 < ratchet N 5933` (also through `make gp-u21-oracle GP_U21_MIN_FIRST=5933`, `make: *** [Makefile:1698: gp-u21-oracle] Error 1`) |
| TRACE_MIN_FIRST 10071 | `FAIL: N 10071 > end 10070: N is unreachable` |
| WIN_MIN_FIRST 10071 | `FAIL: win: N 10071 > end 10070: N is unreachable` |
| MAX_START 89 | `FAIL: window starts at capture 90 (raw 1743) > pinned start 89` |
| MIN_REACH 4974 | `FAIL: window reach port 4973 < pinned reach 4974 (the port exhibited fewer port frames)` |
| sha (last nibble) | `FAIL: poll.log sha256 41e384a9..b1b42f (5946 frames) != the pinned 41e384a9..b1b42e (5946 frames): a re-capture invalidates the pinned N, F and window start; re-measure them from the new capture (record §G.24) before pinning` |
| CAPTURE_FRAMES 5947 | `FAIL: poll.log sha256 … (5946 frames) != the pinned … (5947 frames)` |
| MILESTONES 31 | `FAIL: path: N 31 > end 30: N is unreachable` |

`make gp-u21-oracle` passes at the pins (30/30 milestones, the sixteen allowance lines,
first unexplained 5932, reach 4973, `0 differing through 10069`) and is wired into
`make verify` after `gp-u20-oracle`. The `PR_GP_DUMP` miss set for the new scenario is
the measured §G.24 pair `0x29D60` (one hit) / `0x5D812` (three hits) from
`frontend_mode_1b_step`, pinned in `port/tests/test_platform.c` as
`k_miss_gp_u21_armadon_ending`, and `test_gp_capture`'s poked-final list gains the
scenario (`memsize=64`).

No conflict with §E1 was found: cell 5 is class 4 ARMADON, the mark is `0x84`, the title
id is `0xE2` (THE MEDITATION OF ARMADON) and the label is the character name. The old
scenarios, `WIN_MENU1`, the U13/U18/U19/U20 recipes and every existing pin (the
4248/4320/5120 pins included) are unchanged.

---

## §E7 The DIABLO ending capture (Task 7)

### The scenario and the milestones

`SCENARIOS['gp-u22-diablo-ending']` (in `STOP_AT_END`) is P1's confirm of
character-select cell 6 — `ending_menu(6)`, Task 3's helper: six `p1.right` presses
(the first at mode 0x10 + 60, the rest 30 apart, hold 4) and the confirm — which
`0xC8882 = 00 02 03 01 06 04 05`, indexed by the cursor at `0x43D6E`/`0x43D74`, maps to
class 5 DIABLO (§E1.1). The first poke is `(0x108106, lands_marked(0, 5))` = `0x85 × 7`
and `0x10789E = 0x78` at `after_entry 0x06, 1, 10`; the rest is
`SCENARIOS['gp-u10-ending']['steps']` from index `len(WIN_MENU) + 1` (the round-2 KO, the
final's 14 pokes and the `until_mode 0x03` end). `time_limit=240`, `WIN_EXTRA` and
`memsize=64` are u10's (record §W.13); neither `WIN_MENU` nor u10's steps are mutated.

`MILESTONES['gp-u22-diablo-ending']` is `start_char(5, 6)` (the label names character
5 / cursor 6; the predicate `_all(c0=5, b1e=1)`), the round-1 KO row whose seven-lands
predicate is `[0x80 | r['c0']] * 7` (measured `0x85` on the capture), `_MATCH1[1:]` and
`_ending_tail(5, 'DIABLO')`. The seventh-land row is the inherited `0x41C28 state 3` row
(`r['t104'] & 0xFF == 7`), not the mark row. The mode-0x1F row is `_all(ad4=0, c0=5)`
under the label `DIABLO's ending`; the title is a `0xA80E4` fact — id `0xE3`,
DIABLO'S HELL (§E1.1) — and the compared frames' content, not a WIN/S field.

### The capture (2026-10-23)

`xvfb-run -a make gp-capture scenario=gp-u22-diablo-ending` (this host has no X
display). Every session check is `ok`:

```text
snapshots 9824, f 5..27A1, 317 frames missed
CHECK base / steps fired 26/26 / end frame reached / mode 0x27 after the Enter /
      snapshots kb == raw (0 differ) / frames written 5811/5811 / port script v2 /
      no unscripted input / pokes written 17/17, 0 raced / stopped at the end
      (SIGTERM at f=27A1, rc=0): ok
wrote 5811 frames (raw 1450..14592), wall 210.6s
```

`python3 tools/gp_capture.py check-input` prints `check=ok no unscripted input`. The 26
fired steps are the 10 menu steps (the `boot`/`after`/`after_mode` prefix, the six
`p1.right` presses and the confirm), the scenario's own first poke and the 15 cloned u10
steps; the trailing `until_mode` is the 27th step and is not counted as a fired step
(the harness's `total` excludes it — the X record says `step=27`).

Two earlier runs of the same command failed one check each — `port script v2 (key 0
(enter) consumption at f=D0 unpinned)` and the same at `f=DC` — because the host was
under load (load average ~19) and the poller missed the `S` record at the frame before
the boot Enter's BIOS consumption (650 and then 317 missed frames of the run). Neither
failure was a scenario or port defect: the third run, with the same scenario and
`memsize=64`, has the S records the check needs and every check `ok`. The discarded
staging dirs were removed; no partial capture was published.

Measured in its `poll.log` (9,824 S records):

| Fact | Value |
|---|---|
| the first poke | `W f=04FA step=10 addr=00108106 was=..00 now=85858585858585 late=0 race=0` and `0x0010789E was=00 now=78` |
| round 1's mode-6 entry | `f=0x4F1`, `c0=5 b1e=1` |
| round 1's KO (mode 8) | `f=0x4FD`, the seven `0x85` marks |
| round 2 | mode 6 at `f=0x8BF`; the match won (mode 9) at `f=0x8CC` |
| the conquered lands | mode `0x12` at `f=0xC16`; the seventh land (`t104=7`) at `f=0xCB5` |
| WORLD DOMINATION | mode `0x12`, `c82=1` at `f=0xD01` |
| the health bonus | mode `0x23` at `f=0xE33`, mode `0x22` at `f=0xEAE`, mode `0x24` at `f=0x1358` |
| the final | mode `0xC` at `f=0x1524`; mode 0xF at `f=0x15C9` |
| the ending | mode `0x1F` at `f=0x189A` with `ad4=0 c0=5`; state 3 at `f=0x1E3C` |
| the high-score entry | mode `0x1E` at `f=0x2202` |
| back in mode 3 / the X record | `f=0x2769` (`X ms=209276 f=2769 step=27 end`, `E reason=end rc=0`) |

`gp_win.py check` prints every row `ok` and `30/30 milestones ok`, including
`DIABLO's ending (mode 0x1F, 0x208F8): ... is character c0=5 at f=189A`.

### The report and the allowances

`gp_compare.py --report --max-unexplained 0` on the 4,930-frame port dump (the dump's
frames 0..4929; its trace ends at `f=0x2769`):

- **window** from capture 74 (raw 1737); 5,715 classified: 3,561 clean, 2,064
  splice, 62 transition, 8 band-allowed, 6 scan3-allowed, 1 overlay-allowed, 13
  unexplained, and 22 all-black (the enforced run stops at the first unexplained, hence
  its 5,703 and 1).
- **first unexplained** capture **5798** (raw 14536), nearest port 0: 5798..5810 are
  the post-script tail (the replay's dump ends at port frame 4929 and its trace at
  `f=0x2769`, where the capture's X record sits) and are NOT claimed.
- **reach** port 4929 (the dump's last frame).
- **trace / win / moves**: 9,539 frames compared from `f=0xF8`, 303 without a capture
  snapshot, **0 differing through 10089**; the first tick difference is `f=0xFB`
  (reported apart); the normalised `ent` 0 of 9,539 differ and `t508` 7,338 of 9,539
  (port = capture + 1), both reported.
- the mode-0x1F frames are inside the window: the ending's S records map to capture
  **5063** (`f=0x189A`, `ms=129686` -> raw 9089) and **5344** (`f=0x1E3C`,
  `ms=154981` -> raw 10862) by the capture's own clock
  (`ms * 70.09 / 1000`), both below 5798; the loader overlay at 2191 is the
  mode-0x17 -> 0x12 entry.

Fifteen frames are named per-frame allowances, each its own byte recipe
(`tools/gp_compare.py`, `BANDS_ALLOWED` / `SCAN3_ALLOWED` / `OVERLAY_ALLOWED`). The
selection rule is measured, not fitted: where a three-frame recipe binds all 200 rows
(a unique adjacent pair plus an earlier holder of the top rows) it is used, because it
leaves nothing unbound; where no such pair is unique, the two-segment band recipe is
used (its band is capture-pinned bytes, the U13/U21 named limit).

| capture (raw) | class | recipe (measured from the dump's `frames.txt`) |
|---|---|---|
| 380 (3028) | scan3 | rows 0..10 port 284's (`f=04FB tick=551 mode=08`), 11..60 port 286's (`f=04FD tick=552 mode=08`), 61..199 port 287's (`f=04FE tick=553 mode=08`); pair (286, 287) unique; 379 clean at 284, 381 a (287, 288) splice. Round 1's mode-8 screen. |
| 1267 (4009) | scan3 | rows 0..24 held by the earlier ports 218..221 (mode 3, the still-black screens) and 1026 (`f=0844 tick=899 mode=05`), 25..178 port 1027's (`f=0845 tick=89A mode=05`), 179..199 port 1028's (`f=0846 tick=89B`); pair (1027, 1028) unique; 1266 clean at port 1, 1268 clean at 1028. |
| 1988 (4829) | band | rows 0..116 port 1638's (`f=0B02 tick=B57 mode=09`), 122..199 port 1639's (`f=0B03 tick=B58`), band 117..121; (1638, 1639) unique; 1987 clean at 1638, 1989 a (1639, 1640) splice. The match-result screen. |
| 2014 (4855) | band | rows 66..199 port 1661's (`f=0B19 tick=B6E mode=09`), band 0..65; (1660, 1661) unique; 2013 clean at 1659, 2015 a (1661, 1662) splice. |
| 2020 (4861) | band | rows 13..199 port 1666's (`f=0B1E tick=B73 mode=09`), band 0..12; (1665, 1666) unique; 2019 clean at 1664, 2021 a (1666, 1667) splice. |
| 2191 (5152) | overlay | port 1822's (`f=0C12 tick=C66 mode=17`) 166 glyph px, rows 192..197 x 0..85, over port 1823's (`f=0C12 tick=C6C mode=12`) mode-0x12 screen; the mask from port 1821 (`f=0C12 tick=C66 mode=17`, the stale-palette twin). The §G7 loader presentation at the mode-0x17 -> 0x12 entry; 2190 is clean at port 29 (the still-black early screen) and 2192 clean at 1823. |
| 4136 (7998) | scan3 | rows 0..23 port 3496's (`f=154D tick=15F1 mode=0C`), 24..149 port 3497's (`f=154E tick=15F2`), 150..199 port 3498's (`f=154F tick=15F3`); pair (3497, 3498) unique; 4135 clean at 3496, 4137 clean at 3498. The final. |
| 4228 (8129) | scan3 | rows 0..41 port 3579's (`f=159E tick=1662 mode=0C`), 42..144 port 3580's (`f=159F tick=1663`), 145..199 port 3581's (`f=15A0 tick=1664`); pair (3580, 3581) unique; 4227 clean at 3579, 4229 clean at 3581. |
| 5008 (8959) | band | rows 0..176 port 4270's (`f=184E tick=1929 mode=15`), 191..199 port 4271's (`f=184F tick=192A`), band 177..190; (4270, 4271) unique; 5007 clean at 4270, 5009 clean at 4272. The ending's mode-0x15 fade. |
| 5013 (8966) | band | rows 2..199 port 4277's (`f=1855 tick=1930 mode=15`), band 0..1; (4276, 4277) unique; 5012 clean at 4275, 5014 clean at 4277. |
| 5015 (8968) | band | rows 2..199 port 4279's (`f=1857 tick=1932 mode=15`), band 0..1; (4278, 4279) unique; 5014 clean at 4277. |
| 5016 (8969) | band | rows 0..93 port 4280's (`f=1858 tick=1933 mode=15`), 134..199 port 4281's (`f=1859 tick=1934`), band 94..133; (4280, 4281) unique; 5017 clean at 4281. |
| 5024 (8983) | band | rows 0..148 port 4291's (`f=1863 tick=193E mode=15`), 189..199 port 4292's (`f=1864 tick=193F`), band 149..188; (4291, 4292) unique; 5023 clean at 4291, 5025 clean at 4293. |
| 5027 (8987) | scan3 | rows 0..118 port 4294's (`f=1866 tick=1941 mode=15`), 119..121 port 4295's (`f=1867 tick=1942`), 122..199 port 4296's (`f=1868 tick=1943`); pair (4295, 4296) unique; 5026 clean at 4294, 5028 clean at 4296. |
| 5033 (8994) | scan3 | rows 0..129 port 4300's (`f=186C tick=1947 mode=15`), 130..197 port 4302's (`f=186E tick=1949`), 198..199 port 4303's (`f=186F tick=194A`); pair (4302, 4303) unique; 5032 clean at 4300, 5034 clean at 4302. Port 4301 is nowhere on the frame (its whole present fell between two scans). |

**Why these fifteen and not a port divergence.** The two classes are the ones §G5-§G7
already name, measured per frame on this dump: the scan3 frames are scan-outs caught
with three presents inside one 70.09 Hz scan (an adjacent pair binds the lower rows, an
earlier dumped frame holds the still-scanned top, and the bound pair is unique); the
band frames are presents caught inside the aperture copy (each segment byte-equal to one
unique adjacent dumped pair, the band a state no dumped present holds); the overlay is
the loader's own composition (T1's 166 glyph pixels over S's screen, T2 the stale-palette
twin whose `!=` mask is exactly those pixels). Every neighbour is clean, a splice or a
transition, so the port's own sequence is intact.

Mutation runs on the real dump (`gp_compare.band_explain` / `scan3_explain` /
`overlay_explain` on the loaded frames), 67 edits, all caught:

| edit (per entry) | result |
|---|---|
| one band byte in the capture (8) | caught: `its band rows .. are .., not the named ..` |
| one byte of a bound port frame's segment (8) | caught: `no unique adjacent port pair binds its rows ..` |
| the band shrunk one row at the top, its own hash (8) | caught: `no unique adjacent port pair binds its rows ..` |
| the band shrunk one row at the bottom, its own hash (8) | caught: `no unique adjacent port pair binds its rows ..` |
| a top-segment byte in the capture (6) | caught: `its top rows 0..N are .., not the named ..` |
| a byte of port B's middle segment (6) / of port B+1's bottom (6) | caught: `no unique adjacent port pair binds its rows ..` |
| the top hash of another frame (6) | caught: `its top rows ..` |
| r1 shrunk one row, its own top hash (6) | caught: `no unique adjacent port pair binds its rows ..` |
| a masked capture pixel, a T1 glyph byte, an S pixel off the mask, T1/T2 swapped, S named as T2 (5) | caught |
| the band grown one row / r1 grown one row, own hash (15 of 22 applicable) | explained: a wider description of the same frame — the §G5/§G6 shape limit (the bounds follow the unheld run's ends, not the model) |

Named limit: the overlay's T1 identity is not unique — the later mode-0x12 pair
(1825, 1824) carries the same 166 glyph bytes at the same indices — and T2 is only the
mask's indicator; the capture's pixels are the same under either.

### The pins

`GP_U22_MIN_FIRST=5798`, `GP_U22_TRACE_MIN_FIRST=10090`, `GP_U22_MAX_START=74`,
`GP_U22_MIN_REACH=4929`,
`GP_U22_CAPTURE_SHA256=e53bef4f6f4ef23e1c6046c453236d51b54575916cebd71259a3b9b8dbbe0355`,
`GP_U22_CAPTURE_FRAMES=5811`, `GP_U22_MILESTONES=30`, `GP_U22_WIN_MIN_FIRST=10090`.
Each one-step move was run through the target's own tools (with the dump kept) and fails:

| move | failure |
|---|---|
| MIN_FIRST 5799 | `FAIL: first unexplained 5798 < ratchet N 5799` |
| TRACE_MIN_FIRST 10091 | `FAIL: N 10091 > end 10090: N is unreachable` |
| WIN_MIN_FIRST 10091 | `FAIL: N 10091 > end 10090: N is unreachable` (the `win:` claim) |
| MAX_START 73 | `FAIL: window starts at capture 74 (raw 1737) > pinned start 73` |
| MIN_REACH 4930 | `FAIL: window reach port 4929 < pinned reach 4930 (the port exhibited fewer port frames)` |
| sha (last nibble) | `FAIL: poll.log sha256 e53bef4f..be0355 (5811 frames) != the pinned e53bef4f..be0356 (5811 frames): a re-capture invalidates the pinned N, F and window start; re-measure them from the new capture (record §G.24) before pinning` |
| CAPTURE_FRAMES 5812 | `FAIL: poll.log sha256 … (5811 frames) != the pinned … (5812 frames)` |
| MILESTONES 31 | `FAIL: path: N 31 > end 30: N is unreachable` |

`make gp-u22-oracle` passes at the pins (30/30 milestones, the fifteen allowance lines,
first unexplained 5798, reach 4929, `0 differing through 10089`) and is wired into
`make verify` after `gp-u21-oracle`. The `PR_GP_DUMP` miss set for the new scenario is
the measured §G.24 pair `0x29D60` (one hit) / `0x5D812` (three hits) from
`frontend_mode_1b_step`, pinned in `port/tests/test_platform.c` as
`k_miss_gp_u22_diablo_ending`, and `test_gp_capture`'s poked-final list gains the
scenario (`memsize=64`).

No conflict with §E1 was found: cell 6 is class 5 DIABLO, the mark is `0x85`, the title
id is `0xE3` (DIABLO'S HELL) and the label is the character name. The old scenarios,
`WIN_MENU1`, the U13/U18/U19/U20/U21 recipes and every existing pin (the 4248/4320/5120
pins included) are unchanged.

---

## §E8 The U14 fought live win stays a named gap (Task 8)

**Task 8 is skipped: the go-ahead was declined.** The retake was conditional
(spec §G3, §7.4, §11's "The U14 retake needs a new go-ahead"). The two permitted
attempts are spent (record `2026-10-20-gameplay-truth-ii-derivations.md` §J6;
this record's §E1.4 above) and no third attempt was authorized, so the plan's
Task 8 Step 1 "if declined" branch is the outcome. No capture was run and none
exists: there is no `data/k11-captures/gp-u14-win-live`, and no port dump was
produced for it.

### R1 is open

**R1 — the fought, no-poke live win — remains a named gap** (spec §10's first
item; §9 acceptance #3's second arm). What is missing is not the mode path but
the fight: the game must raise P2's `+0x5A` to `0x78` by landed `1A`/`1B` hits
and then set its own death-done byte `DS_00104B0C = 1` at `0x37FF6` (the
`0x37EA0` remains-stream `0xD100` target). No capture exhibits that sequence.

### The evidence is the preserved failed artifact, not a capture

§E1.4's U14 measurements rest on `/tmp/pr_gp_failed/gp-u14-win-live-run2/`, the
second attempt's failed artifact (record gameplay-truth-ii §J6), re-measured for
this section; it repeats §E1.4's rows and adds the artifact's identity rows, and
the repeated rows are identical to §E1.4's, re-measured on the same artifact, so
a correction to either copy must move both:

| Fact | Measured |
|---|---|
| frames | 7,287 `frame_*.raw.gz` (`frame_00000..frame_07286`) |
| `S` records | 9,761, `f=0x4` (4) .. `f=0x265D` (9821) |
| `X` / `W` records | 0 / 0 — the run never reached its end and wrote no poke |
| the end | `E ms=200522 reason=time-limit rc=0` (the scenario's own 200 s limit) |
| round 1 | mode `0x08` at `f=0x9B3`, `s0_5a=0x78`, `s1_5a=0x00` (P1 KO'd, P2 undamaged) |
| round 2 | mode `0x09` at `f=0x12DC`, `s0_5a=0x78`, `s1_5a=0x38`, `ad4=1` (P1 KO'd again; P2 at `0x38`, P2 won) |
| `b0c` | never non-zero in any of the 9,761 `S` records |
| after the result | mode `0x17` at `0x140D`, `0x15` at `0x14FC`, `0x13` at `0x1575`, `0x1E` at `0x1801` |
| back to attract | mode `0x03` at `f=0x1D68`, running to the last `S` `f=0x265D` |

With no `X` record and no stopped-at-end check the artifact was never published
as a capture, so there is nothing to pin: a capture is the `X` record plus the
checks. It records the failed attempt's shape only — this section claims no
frame, pin or pixel from it. The artifact lives under `/tmp` and is not durable:
if it is lost, §E1.4 above stands as the record's measurements and this table
cannot be re-measured.

### The pins stay empty and the scenario is unchanged

All eight `GP_U14_*` pins are empty (`Makefile:1322-1329`:
`GP_U14_MIN_FIRST`, `GP_U14_TRACE_MIN_FIRST`, `GP_U14_MAX_START`,
`GP_U14_MIN_REACH`, `GP_U14_CAPTURE_SHA256`, `GP_U14_CAPTURE_FRAMES`,
`GP_U14_MILESTONES`, `GP_U14_WIN_MIN_FIRST`), and `gp-u14-oracle`
(`Makefile:1330-1345`, still in `make verify` at `Makefile:1921-1922`) keeps its
`[ ! -d … ]` skip: it prints `no capture at data/k11-captures/gp-u14-win-live
(skipped)` and exits 0. A present capture with an empty pin would still fail
first in `gp_win.py check`'s sha comparison (the Makefile's own note), so the
empty pins are a declared gap, not a bypass.

The scenario is the measured one (`tools/gp_session.py:594-605`; §E1.4's
citation `504-516` predates Tasks 3-7's insertions, the bytes are the same):
`U14_PUNCH`/`U14_KICK` = `1A`/`1B`, `U14_GAP=16`, `U14_ATTEMPTS=32`, 31
`U14_PRESSES`, `time_limit=200`, `extra=WIN_EXTRA`, `WIN_MENU` + both rounds +
`until_mode 0x12`, and **no `poke` step** (69 steps, 0 pokes, measured from the
loaded module for this section). `git diff 4605e0f0..HEAD` (the phase base to
this commit) changes no `U14_*` definition, no `gp-u14-win-live` scenario line
and no `GP_U14_*` pin; the U14 scenario, the Makefile target and pins and
`gp_win.py`'s rule are byte-identical, so Task 8's own change set is these two
documentation files (the phase's other U14 mentions are new unit-case uses of
`live_death`, `U14_PUNCH`/`U14_KICK` in the U18-U22 tests and one reflowed
scenario-list assertion — not definitions, scenario lines or pins).

### `live_death` stays the acceptance test

The rule is unchanged (`tools/gp_win.py:299` `LIVE_WIN`, `:302-327`
`live_death`): it fails on any `W` record writing `DS_00104B0C` (the
`DEATH_DONE` poke U9/U10 use), fails when no `(0x09, 1)` round-2 KO row anchors
it, and fails unless some `S` at or after that frame has `b0c=1`. Its claim is
the game's own `0x37EA0 -> 0x37FF6` write after a real KO — the byte proves a
death animation ended by the match-end screen, and the frames/WIN trace carry
which one (`gp_win.py`'s comment; record gameplay-u9-u10 §W.5, §W.9). A third
attempt would be judged by this rule alone; it stays the acceptance test whether
or not the attempt happens.

### A third attempt needs a new explicit go-ahead

The revised script is known and unmeasured: `U14_ATTEMPTS` 160 (spec §7.4's
160-200), a `p1.right` re-approach every 8 attempts (the `_u16_round` pattern,
`gp_session.py:669-685`), `time_limit ~260`, no pokes (spec §7.4, §8's **U14
(G3)** paragraph, the plan's Task 8 Steps 2-4). Whether that script lands the
KO, the game-set `b0c` and the land **has not been measured** — the attempt
limit is spent, so the `GP_U14_*` pins stay empty until a new, explicit go-ahead
authorizes the capture and its measurement (spec §G3, §11; this record's §E1.4
above).

### What the phase does and does not claim

U14 R1 is a named gap and the phase does not claim it (spec §10). The phase's
other capture wins are different claims and do not close it: §E2's
`gp-u23-u16-finisher` is the labelled poked-damage fallback whose claim is the
finisher reaction, and §E3-§E7's five ending captures are U13-style poked wins
whose claim is the ending content — none is a fought win, and none is a U14
substitute (§E1.3 says the same of U9/U10). The game-set `b0c` is not itself a
gap: U10's `GP_ENDING_WIN_MIN_FIRST=9954` pins the game's write (spec §10's last
item); what stays open is the fought (no-poke) win that reaches it.
