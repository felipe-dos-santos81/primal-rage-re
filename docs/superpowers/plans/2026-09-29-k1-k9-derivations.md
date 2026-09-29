# Clusters K1 (header/split) and K9 (classification) — raw-byte derivation (Task 3a of `2026-09-29-all-gaps.md`)

**Scope.** Ledger `2026-09-29-all-gaps-ledger.md` §F rows 2 (K1 HDR) and 3 (K9
CLASSIFY). K1 is seven functions the port already implements but the counter
(`tools/port_progress.py`, regex `/\* 0x`) does not see. K9 is twelve functions
proposed host-owned or deferred (ledger §G). Sections are numbered `§K1.n` and
`§K9.n` so they cannot be confused with the Task 2 record's `§1..§31`; the port
headers and `tools/port_classification.txt` cite them by that name.

**Tooling.** The Ghidra MCP bridge was not reachable (`ToolSearch` finds no
Ghidra tool). Every address was read from the Python mirror of `mem_load_le` +
`mem_load_le_fixups` (`port/src/mem.c`), so each dword has the LE fixups applied,
and disassembled with capstone (32-bit). Caller scans cover the whole code
object `0x10000..0x73B14`: every `E8`/`E9` rel32 and `0F 8x` rel32 whose target
is the function, each decoded to confirm it; plus every absolute dword equal to
the entry anywhere in the image (code and data objects). Owners come from
`port/decomp/prage.functions.csv` extents.

**Verdict kinds.** K1: *header-only* (the port function is the raw body; add
the standard header), *shared-header* (the port body is shared with a
byte-identical twin; add the header to the shared function), *split* (the raw
differs from the shared body's other address, or the rule "one C function per
original function" needs its own function; split with the raw's body and a
mutation-proven test). K9: *supported* (the raw confirms the proposal; one
`tools/port_classification.txt` row) or *rejected* (stays unported, reported).

---

## K1 — header and split

### §K1.1 `0x34B6C` `fight_health_sync` — header-only

Raw `0x34B6C..0x34D88` (541 B). EAX = side. `ECX = [0x1077A8 + 4*side]`
(`0x34B73`), return when 0 (`0x34B7C`) or when `[ECX]` is 0 (`0x34B86`); the
position gate `0x36E2C` (`0x34B8E`) selects the `0x34B97` position branch
(`±0x3000` by the `+0x28` bit 0x4000, compared against `DS_000BE018`; in range
`0x36F10`, else `+0x43 |= 0x40` and `0x36638`), else the `+0x52` dispatch
through the 22-entry table `0x34B14` (`0x34BF7 cmp al,0x15; ja 0x34C08`). The
port (`fight.c` `fight_health_sync`) follows it arm by arm, including case 0x13
(`0x34D22..0x34D67`, both slots' `0x33B00` at `+0x52 == 0x13`) and the
out-of-range default `0x349C8`.

One arm is not reproduced and stays the existing `PORT:` note: case 0x12
(`0x34CC7..0x34D21`), the command gate `word[0x1088E0+2*side]` (`& 0x300` and
`& 0xC00` both non-zero ends the arm), `+0x54 ∈ {0,1}`, then `0x3BDDC` and on
success `0x18B04`. Both callees are ported now (`fighter_attack_consume`,
`hit_facing_flag`), so the arm is portable in a later cycle. It is **not**
ported here: this task is header-only with no behaviour change, and porting it
is a behaviour change on a live path (`0x357FC`). Recorded as a finding.

Caller: `0x357FC` in `0x35658` (ported, `fight_hud_pass`). Only the separator
comment `/* ---- 0x34B6C` named it, which the counter does not match.

### §K1.2 `0x3BDB0` `fight_attack_ready` — header-only

Raw (43 B): `0x33950` same-side context into a 0x18-byte frame (`0x3BDB8`),
`EAX = [esp+8]` (`ctx[2]`, the slot), `AL = 1` when `byte [EAX+0x53] == 0`
(`0x3BDC1`) and `byte [EAX+0x54] != 2` (`0x3BDC7`), else `AL = 0`. Only AL is
defined; the one caller `0x3B27F` (in `0x3B134`) tests `test al,al`
(`0x3B284`). The port returns 0/1 from exactly that test. Header added.

### §K1.3 `0x38910` `title_origin_reset` — header-only (plus the `0x4F1D0` call)

Raw (125 B): `0x4F1D0` (`0x3891A`; EAX = `byte [0x107A55]`, which `0x4F1D0`
ignores: `xor edx,edx; mov [0x107A3A],dx; mov [0x107A38],dx`), then
`DS_00107A4A = DS_00107A4C = DS_00107A4E` (`0x38925/0x3892B`),
`DS_00107A50 = word[0xBDE0C + 2*idx]` (`0x38938`),
`DS_00107A40 = word[0xBDDFC + 2*idx]` (`0x38948`),
`DS_00107A3A = DS_00107A50 / 32` and `DS_00107A38 = DS_00107A48 / 64` (the
`sar/shl/sbb/sar` idiom over a zero-extended word, so a plain shift;
`0x3895C..0x38985`). The port matches; it inlined `0x4F1D0`'s two stores, which
now become the call to `frontend_origin_zero()` (the `0x4F1D0` port) — the same
two stores in the same order, so no behaviour change. Its `/* PORT: 0x38910`
header becomes the standard header. Caller: `0x121F9` (`0x121A0`, ported).

### §K1.4 `0x4682C` — split from `0x467DC`

Raw `0x467DC` (78 B) and `0x4682C` (78 B) are the same code except one branch:
slot = `0x1077B0 + side*0x94` (`0x467E0..0x467ED`); `AL = 0` unless
`+0x54 == 0` (`0x467F2`), `+0x53 == 0` (`0x467FD`) and `CL = +0x52 == 1`
(`0x46808..0x4680B`); then `0x1A5D4(side)` (the facing/command test). `0x467DC`
returns `AL = CL = 1` when `0x1A5D4` returns 0 (`0x4681E je 0x46825`);
`0x4682C` returns `AL = CL = 1` when it returns non-zero (`0x4686E jne
0x46875`). Callers: `0x46A80` → `0x467DC` (state 6) and `0x46AA0` → `0x4682C`
(state 5), both in `0x469A8` (`ai_classify`).

The port shared one body, `ai_pred_cmd_sign(side, want_zero)`, headed
`/* 0x467DC / 0x4682C`. Its behaviour is correct for both; it breaks "one C
function per original function" and hid `0x4682C` from the counter. Split into
`ai_pred_467dc` and `ai_pred_4682c`, each with its raw body, exported for their
unit test (the precedent is `ai_pred_468d8`). Test values (§K1.4 test in
`test_fight.c`): side 1, `+0x54 = +0x53 = 0`, `+0x52 = 1`, the fighter record's
`+0x28` bit 0x4000 clear. Command word `0x2000` → `0x1A5D4` = `0x2000`:
`0x4682C` = 1, `0x467DC` = 0. Command word 0 → `0x1A5D4` = 0: `0x4682C` = 0,
`0x467DC` = 1. `+0x52 = 2` → both 0 (`0x4680E`/`0x4685E`).

### §K1.5 `0x1C528` — shared header on `sprite_node_build`

Raw `0x14268` and `0x1C528` (190 B each) decode to 65 instructions with
identical mnemonics and operands after relocation: the only differing bytes are
offsets `0x23..0x26`, the rel32 of the `call` at `+0x22`, which targets
`0x1B544` in both (`0x1428A`, `0x1C54A`); the five intra-function branches
(`+0x0A`, `+0x30`, `+0x4D`, `+0x88`, `+0x9A`) have the same encodings and land
at the same relative offsets. The body is genuinely the same, so the header is
added to the existing function (`sprite.c` `sprite_node_build`) as its own
comment line, and no split is needed. Caller: `0x1C60B` in `0x1C5E8`
(`text_blit_glyph`, which calls `sprite_node_build`). No new test: the body is
unchanged and already covered by `check_node_build`.

### §K1.6 `0x51ED8` — split from the shared span blit

Raw `0x51E5C` (121 B) and `0x51ED8` (109 B) differ in two ways, not one:

1. the destination base: `0x51EA5 add edi,[0xE87A4]` (the back buffer) versus
   `0x51F1B add edi,0xA0000` (the VGA aperture);
2. the node's `+0x14` (rows) and `+0x30`: `0x51E5C` pushes both
   (`0x51E74/0x51E77`) and pops them back after the renderer
   (`0x51EC8/0x51ECB`); `0x51ED8` does **not**. It stores
   `+0x14 = +0x14 - +0x34` (rows − clip_b, `0x51F23..0x51F2F`) before the
   renderer call (`0x51F35 call [edx*4+0x80C8C]`) and leaves it there.

Both return early without any write when `+0x18` (width) or `+0x14` (rows) is
0 (`0x51EDF`, `0x51EE8`); the `+0x14` store happens after the pixel-handle
resolve `0x1B544` regardless of its result.

The port's `sprite_blit_at(n, base)` is the shared body (it takes the base and
keeps the node intact, the `0x51E5C` behaviour). `0x51ED8` gets its own
function `sprite_blit_aperture(n)`: the same blit into `gfx_aperture()` (the
aperture rule), then the raw's non-restored `rows -= clip_b`. `+0x30` is a
renderer write-back the port's renderers do not model (they take the clip
values by value); that stays a `PORT:` note. Caller: `0x1C63D` in `0x1C5E8`,
whose node is a stack local (`sub esp,0x58`, `0x1C5EA`) that is dead after the
call and whose clip fields are zeroed first (`0x1C620..0x1C62C`, so
`clip_b = 0` there), so the game-visible behaviour does not change. Test values
(§K1.6 test in `test_platform.c`): a zero-width node with rows 5, clip_b 2 keeps
rows 5; the `0x2C11` node with rows 4, clip_b 1 ends with rows 3, while
`sprite_blit` on the same node keeps 4; the aperture receives the same bytes
`sprite_blit` writes to the back buffer, and the back buffer is untouched.

### §K1.7 `0x33714` — split from `palette_record`

Raw `0x33714` and `0x33734` (31 B each) are the same append — `ECX = EAX`
(first), head `= [0x107798] + 0x10`, `[head-0xC] = ECX`, `[head-8] = EDX`
(count), `[head-0x10] = EBX` (ptr), `[0x107798] = head` — except the flag byte:
`0x3371F mov byte [eax-4],1` versus `0x3373F mov byte [eax-4],0`. Only the
low byte of the flag dword is written (the upper three keep the slot's bytes).
`0x33714`'s only caller is `0x13490` (effect teardown `0x13420`, jump-table
`0x13408` entries 0/2/3/5 → `0x13485`: `EBX = [src]`, `EDX = [src+0xC]`,
`EAX = [src+8]`); no absolute reference exists.

The port called `palette_record(..., 1)` there. `0x33714` gets its own function
`palette_record_flagged(ptr, first, count)`. **PORT:** it delegates the append to
`palette_record` so the dirty list keeps one writer and one bound guard (the
guard is itself a `PORT:` safety net, see `gfx.c`); the flag argument is the
raw's constant 1. Test values (§K1.7 test in `test_platform.c`): head at the
list base, flag dword `0xAABBCC00`, the other three dwords `0xDEADBEEF`;
`palette_record_flagged(SCRATCH, 0x10, 3)` gives `{SCRATCH, 0x10, 3,
0xAABBCC01}` and the head advanced by 0x10.

---

## K9 — classification

Raw caller scan (this task, fixed-up image):

| addr | rel32 sites (site@owner) | absolute dwords |
|---|---|---|
| `0x501A3` | `256A5`@`255CC`, `2EAD1`@`2EA78` | none |
| `0x4FB98` | `1BEB0`@`1BE30`, `1BFEA`@`1BEC4` | none |
| `0x4FF8F` | `1BD15`, `1BD2E`, `1BD3C`, `1BD8B` @`1BBAC`; `1B976`, `1B9B6`, `1BA16`, `1BA96` in `0x1B934..0x1BB73` (no Ghidra function) | none |
| `0x4FFD8` | `1BDA4`, `1BDC9` @`1BBAC`; `1BAD6`, `1BB36` in `0x1B934..0x1BB73` | none |
| `0x2D62C` | `1BE28` (timer ISR `0x1BDF4`, no Ghidra function) | `0x1BF5C` (`0x1BF5B push 0x2d62c` → `0x109A0`) |
| `0x2E180` | `2E983`@`2E934` | none |
| `0x2E0A4` | `2E1EB`@`2E180` | none |
| `0x2E034` | `2E20B`@`2E180` | none |
| `0x2DF8C` | `2D919`@`2D6F8` | none |
| `0x2D498` | `2D612`@`2D4EC` | none |
| `0x32B94` | `2786B`@`277C0` | none |
| `0x32BB0` | `27E28`@`27DC8` | none |

The scan matches the ledger's §B.1 counts site by site.

### §K9.1 `0x501A3` — host-owned: supported

`0x501A3 pushad; 0x501A4 mov edi,[0xE87A0]; 0x501AA mov esi,[0xE87A4];
0x501B0 mov ebx,0xA0000; 0x501B5 mov ebp,0xC8` (200 rows), then an unrolled
compare of each dword of the two buffers (`cmp [edi+k],eax`, 159 sites) with a
store only on a difference. A whole-body operand scan (2944 B) finds 80 memory
stores, **every one `mov [ebx+k],…`** — the aperture — and no store through EDI
or ESI or to a global; the only memory reads are the two buffer pointers and
the buffers themselves. No `call`/`int`/`in`/`out`. So it is pure "copy the
changed dwords of the back buffer to the screen". **Port replacement:**
`gfx_present(mem + DSD(DS_000E87A4), 320, 200)` at both callers — the master
loop (`flow.c`, the `0x25680`/`0x256A5` site) and `config_screen_wait`
(`config.c`, `0x2EAC2/0x2EAD1`) — under the aperture rule.

### §K9.2 `0x4FB98` — host-owned: supported

`pushad; and eax,0xff; int 0x10; popad; ret` (10 B): the BIOS set-mode call
(AH = 0). Callers: `0x1BFEA` in the init `0x1BEC4` with `EAX = 0x13`
(`0x1BFE5`), and `0x1BEB0` in the teardown `0x1BE30` with
`EAX = DS_00101504` (`0x1BEAB`, the mode `0x4FBA2` saved at init). No memory
effect. **Port replacement:** the SDL host owns the window — `host_init`
(`SDL_CreateWindow`, `host.c:175`) for the init call (`flow.c`
`game_init`'s `PORT: 0x4FB98` note) and `host_shutdown` (`SDL_DestroyWindow`,
`host.c:196`) for the restore.

### §K9.3 `0x4FF8F` — host-owned: supported

Pure (73 B, no store): `AL = 0x10/0x20` from `word[0xE1C1E] - word[0xE1C22]`
against ±0x1E and `0x40/0x80` from `word[0xE1C20] - word[0xE1C24]`
(`0x4FF92..0x4FFD4`). An absolute-dword scan of the code object for
`0xE1C1C..0xE1C2D` finds its inputs written only by the game-port timers
`0x4FC05` (`0x4FC61`, `0x4FC84`, `0x4FCB8`, `0x4FCE2`), `0x4FCFD` (`0x4FD59`,
`0x4FD7C`, `0x4FDB0`, `0x4FDDA`), `0x4FDF5` (`0x4FE51..0x4FF74`) and the
joystick calibration `0x5004A` (`0x5005C..0x500B0`), all host-owned
(classification rows `4FC05`, `4FCFD`, `4FDF5`, `5004A`; records §49-V /
§55-A); its only other readers are `0x4FFD8` and those same four. Callers: four sites in the host-owned ISR sampler `0x1BBAC` (row
`1BBAC`, record §50-C) and four in the unreferenced routines of §K9.13.
**Port replacement:** the keyboard-only input path (`flow.c` `PORT: 0x5004A
joystick init — the port reads int 16h keyboard only`; `input_joystick_device`
is the shipped constant 0).

### §K9.4 `0x4FFD8` — host-owned: supported

Same as §K9.3 for joystick B: `word[0xE1C26] - word[0xE1C2A]` and
`word[0xE1C28] - word[0xE1C2C]` (`0x4FFDB..0x5001D`), pure. Callers: two sites
in `0x1BBAC` and two in §K9.13's routines. Same replacement.

### §K9.5 `0x2D62C` — host-owned: supported (with a correction)

`inc dword [0x105D88]; mov eax,eax; ret`. Its only call is `0x1BE28` in the
AIL timer callback `0x1BDF4` (registered at `0x1CFED`, record §1 of the Task 2
record), gated on `byte [0x104B22] != 1` (`0x1BDF8..0x1BE00`). The absolute
dword `0x1BF5C` is `0x1BF5B push 0x2d62c` feeding the `0x109A0` DPMI lock
(`0x1BF60`), not an install. **Correction to the ledger:** `0x1BFA4` is not a
read of `DS_00105D88`: it is `0x1BFA3 push 0x105d88` feeding another `0x109A0`
lock. The counter's one reader is `0x32981 mov eax,[0x105d88]` in the
host-owned run clock `0x32970` (row `32970`, record §48-V). **Port
replacement:** the host tick `g_tick` (`host.c`), the model of `DS_00105D88`.

### §K9.6 `0x2E180` — deferred: supported

Audit add (151 B): table index EAX < 3 selects descriptor
`0x2D444 + 8*(EAX+3)` (`0x2E194..0x2E19A`; entries 3..5 point at
`0x105ECD`/`0x105EE1`/`0x105EF5`, sizes 20/20/7), reads the counter byte, adds
EBX, and when the sum exceeds 0xFF halves the tables flagged in
`word[0x2D420 + 16*EDI]` through `0x2E0A4` (`0x2E1EB`) before storing through
`0x2E034` (`0x2E20B`). Only caller: `0x2E983` in `0x2E934`, which is deferred
(row `2E934`, record §48-V). **Port replacement:** none needed — the declared
no-op audit layer (`0x2E934` and `0x2DAE4` are no-ops; `fight.c`'s `0x2E934`
note).

### §K9.7 `0x2E0A4` — deferred: supported

For EAX < 3: sets the dirty bit `byte[0x105DD8 + (EAX+3)/8] |= 1 << ((EAX+3)&7)`
(`0x2E0D7..0x2E0E8`), halves each byte of the descriptor's counter table
(`0x2E0FD..0x2E108`), then `0x2D4EC(EAX+3)` (`0x2E10D`). Only caller
`0x2E1EB` in `0x2E180` (§K9.6). Same replacement.

### §K9.8 `0x2E034` — deferred: supported

For EAX < 3 and EDX below the descriptor's count: the same dirty bit
(`0x2E082`), `byte[table + EDX] = BL` (`0x2E091`), then `0x2D4EC(EAX+3)`
(`0x2E096`). Only caller `0x2E20B` in `0x2E180`. Same replacement.

### §K9.9 `0x2DF8C` — deferred: supported (with a correction)

Loop `ESI = 3..5` over descriptors `0x2D45C/0x2D464/0x2D46C` (`0x2DF98`,
`0x2DFE7`). With the argument non-zero, `EBP = -2` (`0x2DFAC`); with it zero,
`EBP = 0x2E990(id, ptr, size)` (`0x2DFC0`, the deferred storage read, row
`2E990`, record §49-Y.5). When `EBP < -1`: `0x61A70(ptr, 0, size)`
(`0x2DFD6`); when `EBP < 0`: `0x2D4EC(ESI)` (`0x2DFE1`).

**Correction to the ledger's proposal.** `0x61A70` is not inert: it is the
runtime memset (`0x61A72..0x61A80`: EDX's low byte replicated, ECX = EBX, then
`0x65490`), so the call writes `mem[]`. On the port's live path it runs:
`0x2D6F8` passes `EBP` (`0x2D917 mov eax,ebp`), and the defaults arm the port
always takes sets `EBP = 1` (`0x2D82D`), so the raw zeroes
`DS_00105ECD[20]`, `DS_00105EE1[20]` and `DS_00105EF5[7]` (47 bytes,
`0x105ECD..0x105EFB`) and calls the no-op `0x2D4EC(3/4/5)`. The verdict still
holds because the zeroing is inert in the port: those 47 bytes are zero in the
loaded image, and their only references in the whole image are the descriptor
entries `0x2D460/0x2D468/0x2D470`, read by `0x2D4EC` (a declared no-op in the
port, `config.c`), `0x2DF8C`, the deferred chain `0x2E034/0x2E0A4/0x2E180`, and
unported non-Ghidra code at `0x2E010`, `0x2E157`, `0x2E4D4`, `0x2E56B` and
`0x2E610`. No ported code writes them, so they stay zero and the memset
changes nothing. `DS_00105EFC` (the high-score read buffer, `config.c`) starts
one byte past the last zeroed byte. **Port replacement:** the declared no-op
storage layer; `config_validate` (`0x2D6F8`) omits the call (`config.c`
`0x2D912/0x2D919` note). If a later cycle ports a writer of those counters,
this row must be revisited.

### §K9.10 `0x2D498` — deferred: supported

Bounds-checked byte store (27 B): `cmp eax,0x100CE4; jb` / `cmp eax,0x1014DC;
jb` (`0x2D498..0x2D4A4`), in range `mov [eax],dl` (`0x2D4B0`), else
`mov eax,0x80AA8; jmp 0x2EA68` (`0x2D4A6`), and `0x2EA68` is
`mov eax,1; jmp 0x62003` (the runtime's exit path, into `>= 0x5D000`). Only caller
`0x2D612` in `0x2D4EC`, the EEPROM-image maintainer the port keeps as a
declared no-op (`config.c` `config_storage_touch`, counted as ported with an
empty body). **Port replacement:** that no-op layer; the image
`0x100CE4..0x1014DC` is not kept (spec §7).

### §K9.11 `0x32B94` — deferred: supported

`test al,1; je`; `0x2DAE4(0xE, 1)` (`0x32B99..0x32BA3`); EDX pushed/popped.
Only callee the deferred audit add `0x2DAE4` (row `2DAE4`, record §48-V). Only
caller `0x2786B` in `0x277C0` (ported, `flow.c` `PORT: 0x2786B 0x32B94` note).
Replacement: the no-op audit layer.

### §K9.12 `0x32BB0` — deferred: supported

`EAX <= 6` (unsigned, `0x32BB3`): `0x2DAE4(EAX + 0x1B, 1)` (`0x32BC0`); then
`EBX = EDX <= 6`: `0x2DAE4(EDX + 0x1B, 1)` (`0x32BD2`). Only callee `0x2DAE4`
(deferred). Only caller `0x27E28` in `0x27DC8` (ported, `flow.c`
`PORT: 0x27DF0..0x27E28 0x32BB0` note). Replacement: the no-op audit layer.

### §K9.13 The unreferenced sampler routines `0x1B934..0x1BB73` — no row

Eight routines with no Ghidra function: `0x1B934`, `0x1B974`, `0x1B9B4`,
`0x1BA14`, `0x1BA54`, `0x1BA94`, `0x1BAD4`, `0x1BB34` (each `push ebx … ret`).
Their callees are the host-owned `0x1B610/0x1B6A0/0x1B730/0x1B7C0/0x1B890`
(record §50-C) and `0x4FF8F`/`0x4FFD8`; their only stores are the key bitmap
bytes `[DS_00101514]+0x2D8/0x2D9`. No reference reaches them: no rel32
call/jmp/jcc from outside the range lands inside `0x1B934..0x1BB74`, no short
branch from within 0x200 bytes does, and none of the 252 absolute dwords in
the image whose value falls in the range equals a routine start (the dword
table at `0x1BB74`, `0x1BBAC`'s jump table, holds `0x1BD0E..0x1BDEE` only).
**No classification row is added**: `tools/port_progress.py` intersects the
classification with the Ghidra functions in `symbols.h` (`skip = {a: c … if a in
fns …}`), so a row for a non-Ghidra address would change nothing, and the
routines are dead code in the raw. They are recorded here so a later audit does
not mistake them for a gap.
