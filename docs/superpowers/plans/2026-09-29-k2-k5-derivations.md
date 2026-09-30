# Clusters K2 (trivial) and K5 (menu frame) — raw-byte derivation (Task 3b of `2026-09-29-all-gaps.md`)

**Scope.** Ledger `2026-09-29-all-gaps-ledger.md` §F rows 4 (K2 TRIV: `0x29B70`,
`0x32968`, `0x2D4B4`, `0x51F72`) and 5 (K5 MENU-FRAME: `0x2EA74` and its three
ported call sites in `menu.c`). Sections are `§K2.n` and `§K5.n`; the port
headers cite them by that name.

**Tooling.** The Ghidra MCP bridge was not reachable (`ToolSearch` finds no
Ghidra tool). Every byte was read from the Python mirror of `mem_load_le` +
`mem_load_le_fixups` (`port/src/mem.c`), so each dword has the LE fixups
applied, and disassembled with capstone (32-bit). Caller scans cover the code
object `0x10000..0x73B14`: every `E8`/`E9` rel32 and `0F 8x` rel32 whose target
is the function, each decoded to confirm it, plus every absolute dword equal to
the entry (none for any of the five). Owners come from
`port/decomp/prage.functions.csv` extents. The Ghidra decompilation in
`port/decomp/prage.c` was read for each function as a cross-check.

---

## K2 — trivial functions

### §K2.1 `0x29B70` — a bare `ret`

Raw: `0x29B70 c3` (1 B). Ghidra: `void FUN_00029b70(void) { return; }`.

Callers (raw = counter = 4): `0x20E0B` in `0x20DF4` (`game_fight_reset`, after
`0x20E06 mov ebx,7`), and `0x2521A`, `0x25224`, `0x2522E` in `0x24C5C`
(`game_frame`): the jump-table `0x24B8C` entries for modes 1, 2 and 0x20, each
`call 0x29B70; jmp 0x2540F`.

Port: the sites were an inline `break` / a comment with no function and no
header. Now one function `game_null_step()` (`flow.c`) with the header, called
from the four sites. Reach: live (every `game_fight_reset`; modes 1/2/0x20).

Test: the whole data object `0x80000..0x10B0CF` is seeded with a pattern and
must be byte-identical after the call (it writes nothing). Mutation: a store in
the body fails the check.

### §K2.2 `0x32968` — a bare `ret`

Raw: `0x32968 c3` (1 B), followed by 7 bytes of padding before `0x32970`.
Ghidra: `void FUN_00032968(void) { return; }`.

Caller (raw = counter = 1): `0x20CC7` in `0x20C10` (`game_init`), between
`0x20CC2 mov [0x10452c],al` and `0x20CCC call 0x2BF00`
(`config_set_credit_row_init`).

Port: `game_init_null()` (`flow.c`), called at that point in `game_init`.
Reach: live (`game_init`). Test: as §K2.1.

### §K2.3 `0x2D4B4` — the SEC-DED codeword length (raw correction)

Raw (17 B):

```
2d4b4: push edx
2d4b5: mov  edx, 1
2d4ba: inc  eax              <- the loop head (0x2D4C1 jmp 0x2D4BA)
2d4bb: cmp  edx, eax
2d4bd: jge  0x2d4c3          (signed)
2d4bf: add  edx, edx
2d4c1: jmp  0x2d4ba
2d4c3: pop  edx
2d4c4: ret
```

Ghidra agrees: `for (iVar1 = 1; param_1 = param_1 + 1, iVar1 < param_1;
iVar1 = iVar1 * 2) {}`. The result is **EAX**, not EDX: EDX is restored by the
`pop` at `0x2D4C3`, and every caller consumes EAX (`0x2D538`, `0x2D8CD`,
`0x2DB16` `imul eax,edx`; `0x2DB29` `add eax,esi`).

**Correction (raw wins).** Ledger §B.1 says "smallest power of two ≥ n+1".
That is wrong: the `jmp` goes back to the `inc eax`, so EAX grows by one on
every doubling, and the value returned is EAX, not the power of two. With
`k` passes the result is `n + k`, the first one where `2^(k-1) >= n + k`.
That is `n + r + 1` for the smallest `r` with `2^r >= n + r + 1`: the length
of a Hamming SEC-DED codeword over `n` data units (r check units plus the
overall parity unit). Values, stepped by hand from the bytes:

| n | passes (EAX, EDX at the `cmp`) | result |
|---:|---|---:|
| 0 | (1,1) | 1 |
| 1 | (2,1) (3,2) (4,4) | 4 |
| 3 | (4,1) (5,2) (6,4) (7,8) | 7 |
| 4 | (5,1) (6,2) (7,4) (8,8) | 8 |
| 0x26 | (0x27,1) … (0x2C,0x20) (0x2D,0x40) | 0x2D |

EDX preserved (`push`/`pop`), no memory touched.

Callers (raw = counter = 4), all with EAX = 0x26 (`0x2D528`, `0x2D8BD`,
`0x2DB0B`, `0x2DB1F`):
- `0x2D533` in `0x2D4EC` (the EEPROM image maintenance): `config_storage_touch`,
  a declared no-op (spec §7, record §26 of the TODO-verify derivations).
- `0x2D8C8` in `0x2D6F8`'s stored-image arm (`0x2D8BB`), which the port does not
  run (no stored image; `config.c` `PORT:` note, record §27).
- `0x2DB11`, `0x2DB24` in `0x2DAE4`, the audit add (deferred,
  `tools/port_classification.txt` record-§48-V).

No ported path computes the value today, so nothing is rewired (no deferred
layer changes without a raw reason). Reach: none on a ported live path; ported
as `config_codeword_len()` (`config.c`) for the persistence cycle that will
port those callers, and tested directly.

Test: the five rows above, with EDX-free C signature. Mutation: making it
return the power of two (the ledger's reading) gives 0x40 for 0x26 and fails.

### §K2.4 `0x51F72` — the 0xFA00-byte dword fill

Raw (404 B): `push ecx; mov cl,0xC8` (`0x51F73`), then `xchg ebx,ebx; nop`
(2 alignment bytes, no effect), then the loop at `0x51F78`: 80 stores
`mov [eax+0x00..0x13C],edx` (stride 4), `add eax,0x140` (`0x520F7`),
`dec cl; jne 0x51F78` (`0x520FC`), `pop ecx; ret`. So 200 passes of 320 bytes
= 0xFA00 bytes (one 320x200 screen), every dword = EDX. EAX is returned
advanced by 0xFA00; no caller reads it (`0x5211E` reloads EAX, `0x52156`
pops). Ghidra: `FUN_00051f72(undefined4 *param_1, undefined4 param_2)`, the same
unrolled loop with `cVar1 = -0x38` (0xC8).

Callers (raw = counter = 3), all in `0x52106` (`gfx_screen_reset`):
`0x52119` on `[0x1014E8]`, `0x52123` on `[0x1014E4]` (both `mem[]` buffers),
and `0x52151` on `mov eax,0xa0000` (the VGA aperture).

Port: `gfx_fill_screen(u32 addr, u32 value)` (`gfx.c`) over `mem[]`, used by
the two buffer fills. `PORT:` the aperture call (`0x52151`) cannot target
`mem[0xA0000]` (the aperture rule: it aliases data-object offset 0x20000); the
port fills `g_aperture`, the host's model of the screen, with the same dword
pattern. Reach: live (`0x2BAF4`'s non-zero arm and the movie player).

Test: a buffer with 4 sentinel bytes before and after, both sides; every dword
of the 0xFA00 bytes = the value, the sentinels untouched. Correction (all-gaps
Task 7): the test (`test_platform.c`, `SCRATCH + 0x30004`) uses one 4-aligned
address only; the unaligned case is not tested. Mutation: 0xC7 passes (one row short) fails the last-dword check.

---

## K5 — `0x2EA74` falls through into `0x2EA78` (raw conflict)

### §K5.1 The body

```
2ea68: mov eax,1 ; jmp 0x62003     (0x2EA68, the fatal error)
2ea72: mov eax,eax                 (padding)
2ea74: 31 c0     xor eax,eax
2ea76: 8b c0     mov eax,eax
2ea78: 53        push ebx          (0x2EA78, config_screen_wait)
```

There is no `ret` in `0x2EA74..0x2EA77`: execution continues into `0x2EA78`
with EAX = 0. Ghidra shows the same (its `FUN_0002ea74` decompiles the whole
`0x2EA78` body: `DAT_00105f30 = 0; FUN_0002a31c(); …`).

**Correction (raw wins).** `menu.c` said "`0x2EA74` is `xor eax,eax; mov
eax,eax`, a no-op" at its three call sites. It is `0x2EA78(0)`.

### §K5.2 What `0x2EA78(0)` does

From `0x2EA78` (ported as `config_screen_wait`, record §49-Y): ECX = EAX = 0;
the latch `DS_00105F30` = 0 (`0x2EA85`); one frame built and presented
(`0x2A31C`, `0x1C3FC`, `0x14328`, copy/blit, `0x50188` swap, `0x1C470`); then
`0x2EAE0 cmp ecx,-1` is not taken, and the loop `0x2EAF0..0x2EB74` runs for
ECX = 0 and ECX = -1 (`mov eax,ecx; dec ecx; cmp eax,-1; jg`): **two** passes,
each waiting for the frame word `DS_000EF6DE` to change and draining the BIOS
key queue. Return: EBX/ECX/EDX/ESI restored (`0x2EB7A..0x2EB7D`); EAX = -1 is
left, and no caller reads it (`0x2FA66 mov eax,0x100`, `0x2FE34 jmp 0x2FD11`
reads ESI and the stack only, `0x2FFF6 mov eax,0x100`).

Port: `config_screen_wait_zero()` (`config.c`) = `config_screen_wait(0)`.

### §K5.3 Call sites

18 raw sites (ledger §B.1). Three are in ported functions and are wired now:

| site | owner | order in the raw |
|---|---|---|
| `0x2FA61` | `0x2FA40` `menu_run` | after the locals (`0x2FA4E..0x2FA5D`), before the `0x2C3FC(0x100)` voice (`0x2FA6D`) and the title read (`0x2FA7A`) |
| `0x2FE2F` | `0x2FA40` `menu_run` | the end of every poll pass, after the `0x305FC` widget (`0x2FE19..0x2FE2A`), then `jmp 0x2FD11` |
| `0x2FFF1` | `0x2FFC4` `menu_step` | the init arm, after `DS_00105F2C = 0x500BB` (`0x2FFDF`) and `DS_0010741C = table + stride` (`0x2FFEC`), before the voice (`0x2FFFD`) |

The other 15 (`0x2D00C`, `0x3074B`, `0x30B20`, `0x30B45`, `0x30E5E`, `0x31275`,
`0x31290`, `0x313F5`, `0x324E1`, `0x32619`, `0x32E31`, `0x32F2D`, `0x33066`,
`0x33242`, `0x33290`) are in service-menu code outside any contiguous Ghidra
extent, none of it ported; they stay with that code.

### §K5.4 Reachability and oracle exposure

`menu_step` is live: `game_frame` mode 0x27 (`0x251DF`, the service menu, Enter
in mode 3). `menu_run` has no ported caller. No oracle capture enters mode 0x27
(the captures have no input), so the enforced oracles cannot see the change;
the 8000-frame headless dump must stay byte-identical, and does (report).

### §K5.5 Test values

- `config_screen_wait_zero()` directly: latch seeded 0x77 → 0; tick
  `DS_00101500` seeded 5000 → 5002 (two passes); a queued key `'a'` (scan 0x1E)
  latched as 0x61, the key word 0x1E61, stamped at 5001; the buffers swapped;
  the back buffer presented. Mutation: `config_screen_wait(-1)` (no wait) or a
  no-op fails the tick and latch checks.
- `menu_step` init (`0x2FFF1`): the stamp `DS_00105F2C` keeps the tick read
  before the wait (1000) and the tick advances by 2 (1000 → 1002). Mutation:
  dropping the call leaves 1000.
- `menu_run` (`0x2FA61` + `0x2FE2F`): one wait at entry plus one per completed
  poll pass (a pass that returns through the callback does not reach
  `0x2FE2F`). Mutation: dropping either call changes the tick count.

**Fixture consequence.** `0x2EA78` pumps the pad through `0x500C4` (`0x2EB0C`),
which recomputes the level word `DS_000E1C34` from the key bitmap
(`[DS_00101514]+0x2D8/0x2D9`) and the previous raw state `DS_000E1C30`. The
menu tests' `mt_press` used to set only the level and the latch, a state the
raw's pump would overwrite. It now also sets the bitmap bytes and
`DS_000E1C30` to the same bits, the state of a key held since the last pump, so
the pump leaves the level as pressed. No assertion changes.
