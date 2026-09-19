# Front-End Input, Credits and the Select State (4b-B) Design

**Base:** `main` at `583df28` (4b-A merged). Sub-project 4b, slice B, per the
user's decomposition: slice A built the config/EEPROM data layer; this slice
makes the front-end interactive.

## 1. Goal

Port the front-end's **input bitfield**, its **credit layer**, the **coin poll**,
and the **state-2 selector** (`0x11F6C`), so the port's front-end becomes
interactive and advances: the title (state 1) can end into the state-2 carousel,
and a coin input moves the live credit counter the title overlay already renders.

Deliberately **not** in this cycle: the attract sub-machine `0x11000` (4d), states
3–9 and their handlers, the voice system `0x2C3FC`, the fight engine, and the
config-module functions 4b-A deferred (`0x2DAE4`, `0x2DB58`, `0x2DBC4`,
`0x2DCA0`, `0x2DDE4`, `0x2DE98`, `0x2DF8C`) plus storage I/O.

## 2. Decisions

1. **Slice B = input + credits + state 2.** The user chose the boundary that
   includes the character/entry selector `0x11F6C`, not just the credit layer and
   not the attract machine.
2. **Extend existing owners; no new files.** `platform/input.c` gains the input
   state, `game/config.c` extends its theme from "EEPROM/config data layer" to
   "config and credit state", and `flow.c` gains the coin poll and state 2. The
   general table utilities stay with the tables they walk.
3. **Proof contract: raw-derived unit tests + a determinism oracle.** There is no
   capture covering the select screen, so state 2 is proven by (a) byte-exact
   unit tests derived from the raw for every table, mask and phase transition,
   (b) the title oracle not regressing, and (c) a new headless determinism oracle
   that drives state 2 for a fixed frame count and requires a stable frame hash,
   the expected phase progression and no crash. The absence of a pixel oracle for
   state 2 is a **declared gap**, not a silent omission. (User-approved.)
4. **Faithful bits, documented SDL binding.** The bit layout, the mask table
   `0x9ACBC`, the event codes, the latch semantics of `0x50161` and the
   level/latch split in `0x4F644` are all ported from the raw. Only the choice of
   *which physical SDL key* drives each input bit is a port decision, recorded in
   one table in `host.c`. The original's mapping lives in a hardware keyboard
   handler and BIOS scancode space that SDL does not have. (User-approved.)
5. **`0x10DB0`/`0x10E18` stay declared no-ops this cycle.** Their only effect is
   to force `DS_000F0A64` to states 4 or 5, whose handlers are out of scope;
   enabling them would strand the front-end in a state with no handler while only
   the overlay still ran. They are ported with states 4/5 in a later cycle.
   (User-approved.)
6. **`DS_00105C05` remains a documented stand-in.** See §5.

## 3. Evidence gathered during design

All addresses are original linear VAs; obj-0 code is at `file_offset = VA +
0x52E54`. Function bodies below were read from the raw, not the decompiler.

### 3.1 Credit layer (raw-verified)

| function | raw behaviour |
|---|---|
| `0x2C06C(al)` | `DSB(0x105C05) = al` (`mov byte [0x85c05], al`). The overlay's text-row writer. |
| `0x2BF00` | `DSB(0x105C05) = 0x1D`. One caller: `0x20CCC`, inside the already-ported `0x20C10` init wrapper. |
| `0x2CAA8` | `return DSB(0x105D60) == 0` — "not free play". |
| `0x2CA2C` | `return (DSB(0x105D60) \| DSD(0x105C00)) != 0` — "has credit or free play". |
| `0x2CA48` | free play → `1`; credits `== 0` → `0`; `DSB(0x104B1F) == 0` → `DSD(0x105C00)--`; return `1`. |
| `0x2CA7C(n)` | free play → `1`; `n > credits` → `0`; `DSB(0x104B1F) == 0` → `DSD(0x105C00) -= n`; return `1`. |
| `0x2C060` | `call 0x2CAA8; jmp 0x2CA2C` — returns `0x2CA2C`'s value; the `0x2CAA8` result is discarded. |
| `0x2D2F0` | `return 0` (3 bytes). The "tiny accessor" of the config cluster. |

Globals: `DS_00105C00` credits (4d), `DS_00105D60` FREE PLAY (byte),
`DS_00105C05` overlay row (byte), `DS_00104B1F` a "suppress the debit" flag (byte).

### 3.2 Coin poll `0x11F28` (raw-verified, 47 bytes)

```
edx = eax                       ; the event code argument
call 0x2C060                    ; has credit or free play?
if (eax == 0) return 0
eax = DSD(DS_001088E4)                       ; current input level mask
if ((DSD(edx*4 + DS_0009ACBC) & eax) == 0) return 0
eax = 1; call 0x2CA7C           ; debit exactly one credit
return 1
```

`DS_0009ACBC` is a 4-byte-entry mask table indexed by event code; `DS_001088E4`
is the level mask. Both are obj-1 data; the raw displacements are `0x1ACBC` and
`0x888E4`.

Call sites: `0x11D15` and `0x11D28` (inside `0x11D04`), plus `0x43939`/`0x43B4E`
(fight engine, out of scope).

### 3.3 Input state (raw-verified call graph)

- `0x500C4` — the per-frame sampler. Reads the key bitmap at
  `DAT_00101514 + 0x2d8` / `+0x2d9` and maintains `DAT_000E1C30` (previous),
  `DAT_000E1C34` (level), `DAT_000E1C38` (latched) and the `_DAT_000E1C40` repeat
  timer. Callers: `0x255EE` (the master loop `0x255CC`) and `0x2EB0C`.
- `0x50161(mask)` — the bit selector: takes the level word, computes the newly
  latched bits as `(latch ^ level) & mask`, ORs them into the latch, and returns
  `(level & ~mask) | edge`. Callers include `0x4F64A`/`0x4F659` (inside `0x4F644`)
  and `0x1C84D`, `0x2EDEA`, `0x2EED2`.
- `0x4F644` — assembles the two public masks:
  `DS_001088E4 = 0x50161(arg1)`, `DS_001088D8 = 0x50161() & 0xFF00FF00`,
  `DS_001088DC = 0x2D2F0()`, `DS_001088D4 = 0x2D2F0() & 0xFF`, and
  `DS_001088E4 |= DS_001088D8` only when `(0x2D2F0() & 2) != 0`. Because
  `0x2D2F0()` returns 0, the joystick half is inert and the merge never fires:
  `DS_001088E4` is the level mask and `DS_001088D8` the latched mask. Callers:
  `0x24C6E` (inside `0x24C5C`) and `0x25210`.
- `0x4F644` is the per-frame input builder `0x24C5C` calls; `flow.c` currently
  records it as unported.

### 3.4 State 2, the selector `0x11F6C` (raw-verified call graph)

`0x11F6C` (562 bytes) is a phase machine on `DS_000F0A6F`:

- phase 0 — setup: `0x29D60` (a `ret`-only no-op), `0x2BAF4` (actors reset),
  `0x4F1D0` (input reset), `0x38B18` (spawn rows), `0x2C06C` (row write);
  `DS_000F0A6E = 0`, `DS_000F0A44 = 0`; when `DS_00104528` bit 1 of byte 1 is set,
  `_DS_000F0A40 = 0`. Falls through to phase 1.
- phase 1 — per entry: `0x2BAF4`, `0x4F1D0`, `0x38B18`,
  `DS_000F0A44 = 0x2AE14(0)` (spawn), then iterate with `0x33904` → `0x1C6D4` →
  `0x13E28`; then the text branch draws via `0x1C500` + `0x2F198`, gated on
  `DSD(DS_0009AEE8 + DS_000F0A6E * 0xC) != 0`, or the sprite branch spawns
  `_DS_000F0A40`. Ends with `DS_000F0A70 = 2; DS_000F0A6F = 4; DS_000F0A68 = 0x5A`.
- phase 2 — advance: `DS_000F0A6E++`; at `6` set `DS_000F0A70 = 3;
  DS_000F0A6F = 4; DS_000F0A68 = 0x1E`, otherwise `DS_000F0A6F = 1`.
- phase 3 — leave: `0x2B150` (actor set dead), `DS_000F0A64 = <register handoff>`,
  `DS_000F0A6F = 0`. The handoff is the selected next state and **must be derived
  from the raw** (the decompiler drops it as `extraout_DX`).
- phase 4 — pause: decrement `DS_000F0A68`; when it expires,
  `DS_000F0A6F = DS_000F0A70` (2 or 3).

Its only caller is `0x11DA0` (inside `0x11D04` case 2).

Reused helpers:

- `0x33904` is **already ported** as `title_retire_next` (`flow.c:289`), the
  fixed-stride iterator over the table at `DS_00107608` bounded by `0x107797`
  (stride `0x10`, stop at a nonzero dword at `+0x14`). 15 call sites, including
  `0x1202B`/`0x1204F` in state 2 and `0x123CB`/`0x123F1` in the title.
- `0x1C6D4` (94 bytes) is a table scan; its three callers are `0x12038` (state 2),
  `0x126EC` and `0x42594`. Its body must be derived from the raw.
- `0x29D60` is a `ret`-only no-op with 15 callers across the front-end and fight
  code.
- `DS_0009AEE8` is the per-entry table, indexed by `DS_000F0A6E * 0xC`.

### 3.5 The master loop's input position

`0x500C4` is called from `0x255EE`, inside the master loop `0x255CC`; `0x4F644`
is called from `0x24C6E`, inside `0x24C5C` (`game_frame`). The port's loop already
calls `input_pump()` (tagged `0x500C4`) at the loop head and `game_frame()`
immediately after; the plan must confirm the exact relative position.

## 4. Components

| unit | original | contract |
|---|---|---|
| `platform/input.c` | `0x500C4` | `input_pump()` (existing name, currently only `host_pump()`) gains the real sampler: host key bitmap → `DAT_000E1C30`/`E1C34`/`E1C38`/`E1C40`. |
| `platform/input.c` | `0x50161` | The level/latch bit selector, ported exactly. |
| `platform/input.c` | `0x4F644` | `input_state_update()`: builds `DS_001088E4` (level) and `DS_001088D8` (latched). With `0x2D2F0() == 0` the joystick merge never fires. |
| `game/config.c` | `0x2CAA8`/`0x2CA2C`/`0x2CA48`/`0x2CA7C`/`0x2C060` | The credit predicates and debit. `config.h`'s scope statement extends to "config and credit state". |
| `game/config.c` | `0x2C06C`/`0x2BF00` | The overlay text-row writers. |
| `game/config.c` | `0x2D2F0` | `return 0`; the joystick accessor, in the config cluster. |
| `flow.c` | `0x11F28` | The coin poll, replacing the current no-op in `game_state_step()`. |
| — | `0x29D60` | A named no-op (1-byte `ret`) so its 15 call sites read as the raw does. |
| `flow.c` | `0x11F6C` | `game_state_select()`: the six-entry, four-phase selector. |
| `flow.c` | `0x33904` | `title_retire_next` generalized to its real identity (a plain list iterator); state 2 reuses it. |
| `actors.c` or the table's owner | `0x1C6D4` | The table scan state 2 uses, placed with the table it walks. |

`host.c` owns the SDL binding table and fills the key bitmap; no other file gains
host calls.

## 5. Consumers and pins

- `game_state_step()` (`0x11D04`) replaces its `/* PORT: 0x11F28 menu-input poll */`
  no-op with the real poll. The raw calls it twice (`0x11D15`, `0x11D28`) and
  diverts the state machine when either reports an accepted credit; the exact
  register flow comes from the raw.
- `game_frame()` (`0x24C5C`) calls `input_state_update()` at `0x24C6E`.
- The `0x2BF00` row writer is ported into `config.c` with its sibling `0x2C06C`,
  and `game_init` calls it where `0x20CCC` does, so the port writes row `0x1D`
  exactly as the raw does at that point. The **title state itself never writes the
  row** (`0x2C06C`'s callers are the attract `0x110CE`, state 4's `0x115C1`,
  state 6's `0x11A51`/`0x11AA3`, and `0x11D04`'s tail at `0x11E03`), so the
  captured row `1` comes from the attract machine `0x11000` (4d). The port's
  existing stand-in therefore stays, immediately after the `0x2BF00` call, citing
  `0x110CE`; it is a **declared 4d dependency**, not a fitted constant.
- `0x10DB0`/`0x10E18` remain no-ops with a comment naming their targets (states
  4/5) and the reason (§2.5).

## 6. Definition of done

1. Build has 0 warnings; `port/src/symbols.h` regenerates byte-identically.
2. Unit tests prove, against the raw: the credit predicates and debit including
   free play, the no-debit flag and the `n > credits` guard; `0x50161`'s
   level/latch semantics; `0x4F644`'s mask assembly with `0x2D2F0() == 0`; the
   `DS_0009ACBC` mask table and `0x11F28`'s accept and reject paths; the `0x33904`
   iterator's stride and bound; and `0x11F6C`'s four phases and its
   `DS_0009AEE8` table.
3. The title oracle does not regress (state 1 stays byte-exact on both captures).
4. A new headless determinism oracle drives state 2 for a fixed frame count and
   requires a stable frame hash, the expected phase progression and no crash.
5. The full ladder passes: `make verify` exit 0, 0 warnings, `all checks passed`,
   the capture comparison unchanged (this cycle does not touch audio), `symbols.h`
   byte-identical.

## 7. Declared gaps and non-goals

- **No pixel oracle for state 2.** §2.3; the determinism oracle is the substitute.
- **The attract machine `0x11000` is out of scope**, so the `DS_00105C05` row
  stand-in survives this cycle with its raw site cited (§5).
- **States 3–9 are out of scope**, and with them `0x12484`, `0x11578`, `0x11A8C`,
  `0x11BCC`, `0x263F4`, `0x33F08`, `0x257A4` and the `0x10DB0`/`0x10E18`
  transitions that lead to states 4/5.
- **The voice system `0x2C3FC` is out of scope**; state 2's and the state machine's
  voice calls are declared no-ops.
- **The fight engine is out of scope** (`0x11F28`'s other two callers).
- The config-module functions and storage I/O 4b-A deferred remain deferred.

## 8. Risks

- **Register-handoff derivations.** `0x11F6C` phase 3's `DS_000F0A64 = extraout_DX`
  and `0x11D04`'s two-poll divert are decompiler-hostile; the plan must derive
  them from the raw bytes and prove them, not transcribe the decompile.
- **Latch semantics.** `0x50161` returns `(level & ~mask) | edge` and mutates the
  latch; `DS_001088E4` and `DS_001088D8` differ in exactly which bits are edges.
  A naive port that treats both as plain levels would pass a weak test and break
  the coin poll; the tests must distinguish the two masks.
- **Key bitmap layout.** The two key-state bytes at `DAT_00101514 + 0x2d8`/`0x2d9`
  and their bit meaning must be derived; the SDL binding is a port choice and must
  be documented as such, not presented as the original's mapping.
- **`0x9AEE8`'s entries** are resource handles; the plan must confirm they resolve
  through the port's resource layer before state 2's text branch can be proven.
- **`DS_00104B1F`** is not yet identified; its writers and meaning must be derived
  before the debit tests can assert the suppression path.
- **State 2 may need inputs to advance.** If the selector turns out to be
  input-driven rather than purely timer-driven, the plan must say so and cover it;
  the design's reading is timer-driven with the coin poll and the transitions
  as the only input consumers.
