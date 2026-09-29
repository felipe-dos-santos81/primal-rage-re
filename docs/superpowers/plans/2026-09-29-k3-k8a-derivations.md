# Clusters K3 (frame service) and K8a (update entry 6) — raw-byte derivation (Task 3c of `2026-09-29-all-gaps.md`)

**Scope.** Ledger `2026-09-29-all-gaps-ledger.md` §F rows 6 (K3 FRAME-SVC:
`0x38990`, called at `0x24CC3`/`0x24CC8`) and 7 (K8a UPD-06: update-table
entry 6, `0x25FAC`). Sections are `§K3.n` and `§K8a`; the port headers cite
them by that name.

**Tooling.** The Ghidra MCP bridge was not reachable (`ToolSearch` finds no
Ghidra tool). Every byte was read from the Python mirror of `mem_load_le` +
`mem_load_le_fixups` (`port/src/mem.c`), so each dword has the LE fixups
applied, and disassembled with capstone (32-bit). Caller scans cover the code
object `0x10000..0x73B14`: every `E8`/`E9` rel32 whose target is the entry,
plus every absolute dword equal to it. Data-reference scans find every
instruction in the code object whose operand is the absolute address. The
Ghidra decompilation in `port/decomp/prage.c` was read as a cross-check.

---

## K3 — `0x38990`, the projection inputs

### §K3.1 The body

Raw (52 B):

```
38990: push edx
38991: mov  ax, word [0xf0aec]
38997: and  al, 0xc0
38999: mov  word [0x107a3c], ax        ; DS_00107A3C = word(F0AEC) & 0xFFC0
3899f: mov  eax, dword [0xf0aec]
389a4: mov  edx, eax
389a6: sar  edx, 0x1f                  ; 0 or -1
389a9: shl  edx, 6                     ; 0 or -64, CF = 0 or 1
389ac: sbb  eax, edx                   ; +0 or +63
389ae: sar  eax, 6                     ; truncating signed / 64
389b1: xor  edx, edx
389b3: mov  dx, word [0x107a4e]        ; zero-extended
389ba: add  eax, edx
389bc: mov  word [0x107a4a], ax        ; DS_00107A4A = low 16
389c2: pop  edx
389c3: ret
```

Ghidra agrees: `_DAT_00107a3c = (ushort)DAT_000f0aec & 0xffc0;
DAT_00107a4a = (short)(... >> 6) + DAT_00107a4e;`.

- `and al,0xc0` masks the low byte only, so the stored word is
  `word & 0xFFC0` (the high byte is kept whole).
- The divide is the MSVC `sar`/`shl`/`sbb` idiom already documented for
  `0x389C4` (`render.c`): for a negative dword it adds 63 first, so it
  truncates toward zero. C `(s32)x / 64` is exactly that.
- `DS_00107A4E` is zero-extended (`xor edx,edx; mov dx,…`). Only the low 16
  bits of the sum are stored, so the extension does not change the result.
- It reads neither word it writes, so two calls in a row store the same
  values.

Test values (`check_scroll_track`, `test_platform.c`, all seeded, with
sentinels `0x7777` in both outputs):

| `F0AEC` | `4E` | `3C` | `4A` | what it pins |
|---|---|---|---|---|
| `0x00012345` | `0x0010` | `0x2340` | `0x049D` | the dword is divided, not the word (`0x2345/64+0x10 = 0x9D`) |
| `0xFFFFFF81` (-127) | `0x0010` | `0xFF80` | `0x000F` | truncation (a flooring `>> 6` gives `0x000E`) |
| `0x0000FFFF` | `0xFFF0` | `0xFFC0` | `0x03EF` | low-byte mask; 16-bit wrap of the sum |

The four neighbouring words `3A`/`3E`/`48`/`4C` are seeded
(`0x1111`/`0x2222`/`0x3333`/`0x4444`) and must survive.

### §K3.2 Callers and the call site

Rel32 scan: exactly two sites, `0x24CC3` and `0x24CC8`, both `call 0x38990`
in `0x24C5C` (`game_frame`). There is no absolute dword `0x00038990`.

```
24cba: cmp  byte [0x104b26], 0
24cc1: je   0x24cc8
24cc3: call 0x38990
24cc8: call 0x38990
24ccd: mov  di, word [0xef6dc]         ; the frame counter, then the update table
```

So it runs every frame, in every mode, after the `0x24C73` AI block
(`fighter_command_block`) and before the frame counter and the update table.
When `DS_00104B26 != 0` it runs twice. The data-reference scan finds no store
to `0x104B26` (only the four compares `0x24C73`, `0x24CBA`, `0x2A214`,
`0x2A32B`), so in practice it runs once. The port ports both calls anyway.

**Correction to the port's placement.** The old comment (`flow.c`, "0x24C5C's
second 0x38990 per-frame service call is deferred") sat *after*
`run_process_table`. The raw calls happen before the frame counter
(`0x24CCD`). The port now calls `render_scroll_track()` between
`fighter_command_block()` and the frame counter.

### §K3.3 Who reads the two words (and double-write check)

Scan of every absolute operand in the code object:

| global | writers | readers |
|---|---|---|
| `DS_00107A3C` | `0x38999` (this function) only | `0x389C6` (`cmp word [0x107a3c],0`) and `0x389D2` (`mov eax,[0x107a3a]`, `sar eax,0x10`: the high word is `3C`), both in `0x389C4` |
| `DS_00107A4A` | `0x3876F` (`0x38730`), `0x38925` (`0x38910`), `0x389BC` (this function) | `0x389FD` in `0x389C4` |

Both readers are `0x389C4` (`render_scroll_edge`). The master loop calls it at
`0x25601`, gated on `DS_00107A54` (`0x255F8`), *before* `0x24C5C` at
`0x2560B`. So a frame's `0x389C4` reads the values that the previous frame's
`0x38990` stored.

The port has no other writer of `DS_00107A3C`. Its only writers of
`DS_00107A4A` are `render_scroll_setup` (`0x3876F`) and `title_origin_reset`
(`0x38925`), the other two raw writers, at their raw positions. No port path
computes these values another way, so there is no double write. Before this
port, `DS_00107A3C` stayed 0 (its image value), so `render_scroll_edge` always
took its `< 1` arm.

**Effect on the port's runs (measured, not assumed).** A temporary counter in
`render_scroll_edge` over `--check 8000` saw `DS_000F0AEC` in `[0, 5888]` and
the `>= 1` arm taken on 725 of about 3500 calls, during the attract demo fights.
That arm changes `DS_00107A38` (`(48 - 3C)/64` instead of `48/64`). It moves
`DS_00107A4C` only when `4A <= 4C`. With `F0AEC >= 0`, `4A = 4E + F0AEC/64 >=
4E = 4C`, and equality needs `F0AEC < 64`, where `3C = 0`. So `4C` never moves
on these runs. `DS_00107A38`'s one raw reader is `0x1449F` (the layer-2 arm of
the sprite projection). The port's `render_list` reads it only for a layer-2
sprite with no preceding layer-1 sprite (`last_mode1_y == -1`). The 8000-frame
dump is byte-identical before and after (see the Task 3c report), so no layer-2
sprite on these runs reads it. **The oracles cannot see this state change:**
`DS_00107A38` now differs during the demo fights, and no pixel changes.

---

## K8a — update-table entry 6, `0x25FAC`

### §K8a The body, the table entry, the arms, the dispatcher

Table: `DS_000A8644` (32 dwords). Entry 6 is the dword at `0xA865C`, value
`0x00025FAC` (the fixed-up image). That is the only absolute reference to
`0x25FAC`, and there is no rel32 call or jump to it. Ghidra has no function
there, so the counter does not move for it.

Raw (46 B):

```
25fac: push edx
25fad: mov  eax, dword [0x104acc]      ; the card record
25fb2: inc  byte [eax+0x2d]            ; word +0x2C += 0x100, no carry out
25fb5: xor  edx, edx
25fb7: mov  dx, word [eax+0x2c]        ; zero-extended
25fbb: cmp  edx, 0x1000
25fc1: jl   0x25fd8                    ; signed on a zero-extended value: u16 < 0x1000
25fc3: mov  dh, byte [0x104ae8]
25fc9: and  dh, 0xbf
25fcc: mov  word [eax+0x2c], 0x1000
25fd2: mov  byte [0x104ae8], dh        ; bit 0x40 cleared, byte store
25fd8: pop  edx
25fd9: ret
```

(`0x25FDA mov eax,eax` is padding before `0x25FDC`.)

**The dispatcher** (`0x24CD5..0x24CFC`): `mov ebx,[0x104ae8]` is loaded once.
For each set bit it does `mov eax,edx; call [eax+0xa8644]`, with EDX = 4·i.
The body clobbers EAX, keeps EBX, and pushes and pops EDX, so `fn(void)` is
exact. Clearing bit 0x40 mid-walk does not affect the current frame's walk.
The port's `run_process_table(DS_000A8644, DSD(DS_00104AE8))` also passes the
mask by value. Before this task the entry was unregistered, and
`run_process_table` skipped it silently (`fn_resolve` returns NULL).

**The arms** (every store to `DS_00104ACC` next to an `AE8 |= 0x40`):

| site | owner (port) | record | bit |
|---|---|---|---|
| `0x25E18`/`0x25E1D` | `0x25C88` mode 5 case 2 (`game_mode_05_step`) | the fight card `0xA8884` or `0xBB6A0` | `or byte [0x104ae8],0x40` |
| `0x26B12`/`0x26B2C` | mode 0x23 (`game_mode_23_step`) | `0xA895C` or `0xBB6B4` | `mov dh,[AE8]; or dh,0x40; mov [AE8],dh` |
| `0x294B8`/`0x294C9` | `0x29328` mode 0x30 (`game_mode_30_step`) | `0xA8884` or `0xBB6A0` | `mov al,[AE8]; or al,0x40; mov [AE8],al` |

Each card is spawned by `0x2AE14(desc, 0x2A00, 0xFF, 0x1200, 0)`. `0x2AE14`
copies the descriptor's `+0x0C` word into record `+0x2C`, and all four
descriptors hold `0x0010` there. So the word runs `0x0010, 0x0110, …, 0x0F10`,
and on the 16th call it reaches `0x1010`, which is clamped to `0x1000`, and the
bit is cleared. `0x2A690` (`actor_pset_point`, `actors.c`) and its siblings copy record
`+0x2C` into pset `+0x0C`. The same three handlers kill the card after the `0x3C`-frame
`DS_00104AFE` countdown (`0x25E89`, `0x26B6F`, `0x2951E`), well after the
16-frame ramp. The raw never tests `DS_00104ACC` for zero, and neither does
the port. Other clearers of the bit: the whole-dword `AE8` stores `0x20E1C`,
`0x20EC3`, `0x28DC4`, `0x2BB13` (`actors_reset`) and `0x41435`.

**Reachability.** Modes 5, 0x23 and 0x30 are reached only by real input (an
accepted coin/start, ledger §D and record §47-B/§48-W). No oracle path reaches
them, so the entry runs in play but on no oracle frame.

Test values (`check_card_ramp`, `test_game.c`; the scratch record is at
`0x3E90000`; `+0x28`/`+0x30` are seeded with `0xA5A5A5A5`/`0x5A5A5A5A`
and `+0x2E` with `0xBEEF`, and all three must survive. `+0x2E` pins "no carry out
of the word" on the `0xFF10` case):

| `+0x2C` before | `AE8` before | `+0x2C` after | `AE8` after | pins |
|---|---|---|---|---|
| `0x0010` | `0xFFFFFFFF` | `0x0110` | `0xFFFFFFFF` | the byte increment; no clamp below `0x1000` |
| `0x0FFF` | `0xFFFFFFFF` | `0x1000` | `0xFFFFFFBF` | the clamp (`0x10FF` → `0x1000`); only bit 0x40 cleared |
| `0x7F10` | `0x40` | `0x1000` | `0` | the zero-extended compare (a signed s16 compare would not clamp `0x8010`) |
| `0xFF10` | `0x40` | `0x0010` | `0x40` | the byte wrap, with no carry out of the word |

Also: the table dword `DSD(0xA865C) == 0x25FAC`,
`fn_resolve(0x25FAC) == flow_card_ramp_step` after `actors_init`, and one
mode-1 `game_frame()` with only bit 0x40 armed takes `0x0210` to `0x0310`
(the dispatcher calls it). The demo's per-frame blocks (`DS_00104B1B` and
`DS_00104B15`) are held at 0 for that frame and then restored, so that the
frame touches no fighter state that later test areas rely on.

**Registration.** `fn_register(0x25FACu, flow_card_ramp_step)` in
`actors_init`, beside the §46-F hooks, as for entries 1/2/5/7/10 (§42-A,
§46-D, §50-E).
