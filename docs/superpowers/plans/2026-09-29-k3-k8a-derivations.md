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
