# EEPROM/config core (4b-A) — cycle report

Plan: `docs/superpowers/plans/2026-09-19-eeprom-config.md`.
Derivations: `docs/superpowers/plans/2026-09-19-eeprom-config-derivations.md`.
Branch `eeprom-config`, merge base `bfac11e`.

## 1. The module and its no-op declarations

`port/src/game/config.{c,h}` owns the packed config data layer. It reads the
descriptor table (obj-0 VA `0x2D300`) and the config byte region (`DS_00105D88`,
backed by `DS_00105DE1`/`DS_00105DAF`) out of the loaded image; no table or value
is transcribed.

| symbol | original | interface |
|---|---|---|
| `config_field_get` | `0x2D974` | `u32 config_field_get(u32 field)` |
| `config_field_set` | `0x2DA0C` | `u32 config_field_set(u32 field, u32 value)` |
| `config_menu_default_bits` | `0x2CCD0` | `u32 config_menu_default_bits(u32 table)` |
| `config_set_defaults` | `0x2CADC` | `void config_set_defaults(void)` |
| `config_validate` | `0x2D6F8` | `void config_validate(void)` |

Declared no-ops this cycle (spec §7): the storage layer — the load `0x2D638`,
the two `0x2E990` reads, the `0x80CE4` image and its maintenance `0x2D4EC`
(`config_storage_touch` is the setter's call-site placeholder) — the defaults
writer's screen setup `0x1AE20` and storage write `0x2EA78` (with the message
draw `0x2F198` and cursor restore `0x2F280`), the pset-pool tail calls `0x61A70`,
and the high-score validators `0x2DE98`/`0x2DF8C`/`0x2DAE4`. With no storage the
magic never matches, so `config_validate` always takes the fresh-machine defaults
path.

## 2. The derived getter/setter walks

Both walks are transcribed from the raw bytes (full instruction-level derivation
with file offsets in `2026-09-19-eeprom-config-derivations.md`):

* `0x2D974` reads the descriptor's width `((d>>14)&7)+1` and bit position
  `(d>>6)&0xFF`, assembles from `DS_00105DE1 + ((bitpos+width)>>1)` walking
  downward (low nibble seed when `bitpos+width` is odd, high-nibble tail when the
  width lands odd), then ORs an optional trailing byte from `DS_00105DAF + (d&0x3F)`.
  Returns `0xFFFFFFFF` for `field > 0x3E`.
* `0x2DA0C` is its inverse: writes the trailing byte first, raises
  `DS_00105DD8` (`|1` for a trailing byte, `|6` always), preserves the nibbles
  the field does not own, and returns `0` / `0xFFFFFFFF`.
* `0x2CCD0` walks the obj-1 menu table at `0xA2EB4` as a **contiguous** array of
  stride `0x14` (presence, shift, count, entries pointer); entries stride `8`,
  `[0]` a `char *`; `bits |= i << shift` for the first entry whose string starts
  with `'*'`.

Two raw facts corrected the controller's summary and were kept (raw wins): the
menu-table immediate is **obj-1** `0xA2EB4` (fixup at source `0x2CAF9` targets
object 2), and the record linkage is `rec += 0x14`, not a pointer chase.

## 3. The defaults path and the validate result

`config_set_defaults` (`0x2CADC`) calls `config_menu_default_bits(0xA2EB4)`, then
`set(0x29, bits)`, `set(0x35, 0xA0)`, `set(0x37, 0xA0)`,
`set(0x2A, (get(0x2A) & 0xFC) | 3)`. On the shipped table
`config_menu_default_bits(0xA2EB4) = 0x00142095`.

`config_validate` (`0x2D6F8`) on the no-storage path (`read1 = read2 = -1`)
writes `DS_00105DA5 = 0`, `DS_00105DA4 = 0`, `DS_00105DD8 |= 6` then `|= 1`,
`DS_00105E2F = 0`, runs the defaults writer, rewrites the magic
`DS_00105E30 = 0x9C94D2C4`, then `DS_00105DA7 = 1` and discards
`config_field_get(0x24)`. Result on the fresh image, confirmed by the unit test
and the oracle:

```
field 0x29 = 0x142095    field 0x35 = 0xA0    field 0x37 = 0xA0
field 0x2A & 3 = 3       DS_00105E30 = 0x9C94D2C4    DS_00105DD8 = 0x7
```

## 4. Step 1 derivations (Task 4)

Disassembled from `data/game/C/PRAGE.EXE` with capstone (obj-0
`file = va + 0x52E54`):

**The three globals from `v` (`0x20C5D`–`0x20CC2`).** After
`0x2D974(0x29)` stores `v` at `[0x84528]` (`0x20C6D`):

```
0x20c89  mov eax,[0x84528]      0x20c93  mov ebx,[0x84528]
0x20c8e  and eax,0x100          0x20c9c  and ebx,0xf
0x20c99  shr eax,4              0x20ca4  lea eax,[ebx*4]
0x20c9f  mov [0x85b3a],al       0x20cab  add eax,ebx
                                0x20cad  add eax,0x1e
                                0x20cb0  mov [0x888d0],eax
0x20cb5  mov eax,[0x84528]
0x20cba  and eax,0xf0
0x20cbf  shr eax,4
0x20cc2  mov [0x8452c],al
```

so, exactly:

```
DS_00105B3A = (u8)((v & 0x100) >> 4)
DS_001088D0 = (v & 0xF) * 5 + 0x1E
DS_0010452C = (u8)((v & 0xF0) >> 4)
```

With `v = 0x142095`: `DS_00104528 = 0x142095`, `DS_00105B3A = 0x0`,
`DS_001088D0 = 55`, `DS_0010452C = 9`.

**`0x2C304` and its call site.** `0x2C304` is called from `0x10E80` at
`0x10ECC` — partway through, after the `DS_000F0A5C`/`DS_000F0A6F` writes and
before the `DS_00104AB8`/`DS_00104B1F` tail:

```
0x2c304  mov eax,0x29
0x2c309  call 0x2d974
0x2c30e  and eax,0xf0000
0x2c313  sar eax,0x10
0x2c316  inc eax
0x2c317  mov [0x85c00],eax
```

`DS_00105C00 = ((0x2D974(0x29) & 0xF0000) >> 16) + 1`; with `v = 0x142095`
that is `4 + 1 = 5`, the capture's credit count. `0x10E80` is called from
`0x20C10` at `0x20CE6`, i.e. after the whole `0x20C5D` derivation.

**`0x2F9CC` ordering.** `0x2F9CC` is called first thing in the `0x20C10` wrapper
(`0x20C15`) and runs `0x1AFE8`, then **`0x13ADC` (effects_init) at `0x2F9D4`**,
then **`0x2D6F8` (config_validate) at `0x2FA0B`**, then `0x2D974(0x2A)` into
`DS_00107410` via `and al,0xFC` (`DS_00107410 = config_field_get(0x2A) & 0xFC`). So
`config_validate` precedes the `0x20C5D` config derivation and
follows effects_init. In the port, effects_init already runs inside
`actors_init()` (`actors.c:120`), which `game_init` calls before the config
block, so placing `config_validate()` at the head of that block reproduces the
raw order. The outer `0x20C10` wrapper is not transcribed as one function; its
`0x2F9CC` and `0x10E80` calls fold into `game_init`/`game_state_init`.

## 5. What changed in `flow.c`

* `#include "game/config.h"`.
* The pinned block at the old `flow.c:698-708` is replaced by the real read:
  `config_validate()`, `v = config_field_get(0x29)`, the three `0x20C9F`–
  `0x20CC2` globals, and `0x2C304`'s `DS_00105C00` derivation. The stale "table
  entry returns 0" comment is gone.
* The overlay block's `DSD(DS_00105C00) = 5;` seed is deleted (now derived);
  `DSB(DS_00105C05) = 1;` remains, with the comment rewritten to name the
  unported writers `0x2BF00`/`0x2C06C` and to preserve the capture provenance and
  the `20/3`-px text-row derivation.

## 6. The unseed outcome

Attempted exactly as the brief prescribes.

1. **Both seeds deleted** (credit and row): `make title-oracle` went red —
   capture 1: `587 frames, all unexplained; port frames exhibited 0/96`;
   capture 2: `590 frames, all unexplained; port frames exhibited 0/96`.
2. **`DS_00105C00` unseed held, `DS_00105C05 = 1` restored**: `make title-oracle`
   green, byte-identical to the pre-change baseline —

```
capture 1: window distinct [216..326] (raw 2198..2308)
capture 1: 111 frames in window: 54 clean, 55 splice, 2 transition, 0 unexplained
capture 1: port frames exhibited 95/96; missing [0]; endpoints OK
capture 2: window distinct [216..326] (raw 2193..2303)
capture 2: 111 frames in window: 54 clean, 57 splice, 0 transition, 0 unexplained
capture 2: port frames exhibited 95/96; missing [0]; endpoints OK
determinism: clean samples of 54 port frame(s) agree, 0 disagree
```

**Conclusion.** The cycle's falsifiable question is answered: the shipped image's
defaults path (`config_validate` → `0x142095`) reproduces the captured credit
count `5` via `0x2C304`, so `DS_00105C00` is no longer pinned. The
`DS_00105C05 = 1` row pin **survives** and is required, but it is not a fitted
value and not config-derived: `0x2BF00` (the init writer the port's path would
reach) writes `0x1D = 29`, while the captured row `1` is written by `0x2C06C`
from the attract entry `0x110CE` (`mov eax,1; call 0x2c06c`), which the port does
not run — it enters title state 1 directly and defers the attract sub-machine
(4d). The raw therefore cannot reproduce the captured row on the ported path;
the port keeps the captured `1`. No value was fitted.

The brief's Step-3 fallback — restoring the `DS_00105C00 = 5` credit seed if a
CREDITS-line residual had appeared — was **not needed**: the unseed succeeded and
the only surviving writer-pin is `DS_00105C05`, whose captured value comes from the
unported attract routine `0x2C06C` at `0x110CE`.

## 7. Declared gaps

* **No storage I/O.** The save/load path (`0x2D638`, `0x2E990`), the `0x80CE4`
  image and its maintenance `0x2D4EC` are no-ops; validate always takes the
  fresh-machine defaults path.
* **Deferred module taps.** `0x1AE20` (screen setup) and `0x2EA78` (storage
  write), plus the high-score validators `0x2DE98`/`0x2DF8C` and `0x2DAE4`.
* **Credit countdown.** `0x2CA48`/`0x2CA7C` and their caller `0x11F28` (the title
  input handler) are unported; the port reproduces the no-input window only.
* **`DS_00105C05`.** Init writer `0x2BF00` (`= 0x1D`) and attract writer `0x2C06C`
  (`= 1` at `0x110CE`) unported, so the captured `1` stays a seed.

## 8. Verification

* Focused: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests` —
  `all checks passed`, exit 0, 0 warnings.
* Full ladder: `rm -rf build && make verify` — **exit 0, 0 warnings,
  `all checks passed`**; `smk_compare` **120/120** and **41/41**; title oracle
  **0 unexplained** in both captures, `95/96` exhibited; `symbols.h` regenerated
  byte-identically (the `git diff --quiet` gate passed); the `gra_extract` test
  module ran 32 tests `OK` (the pre-existing `ResourceWarning: unclosed file`
  noise only). The audio capture-comparison line is unchanged (this cycle does
  not touch audio).
