# EEPROM/Config Core (4b-A) Design

**Base:** `main` at `ad51dcc`. Sub-project 4b, slice A, per the user's decomposition:
the config/EEPROM data layer first, alone, with its existing consumers wired.

## 1. Goal

Port the game's EEPROM/config **core** — the packed-field getter and setter, their
bounded-write helpers, the descriptor table, the config byte region, and the
magic/validate/defaults path — into a new `port/src/game/config.{c,h}`, and wire
the consumers that already exist in the port so the module has real callers rather
than being machinery with no caller.

Deliberately **not** in this cycle: the menu/attract state machine and the input
poll (slice B), and the module functions no ported consumer needs yet.

## 2. Decisions

1. **Slice A alone** (user's decomposition): the config/EEPROM data layer, not the
   menus that read it.
2. **Wire its existing consumers.** The module is ported together with its current
   port callers: the `DS_00104528` pin (`flow.c:698-705`) and the credit
   initializer `0x2C304`. This is what keeps the cycle off the "machinery with no
   caller" anti-pattern the previous cycle's diagnosis rejected
   (`2026-09-18-bf08-overlay-diagnosis.md` §5).
3. **Core + defaults, consumer fields only.** The getter/setter, bounded writes,
   descriptor table, config region, magic/validate, and defaults are in scope. The
   later module functions (`0x2DAE4`, `0x2DB58`, `0x2DBC4`, `0x2DCA0`, `0x2DDE4`,
   `0x2DE98` high-score, `0x2DF8C`) and the load/save I/O are deferred until a
   ported consumer needs them.
4. **Proof contract.** Falsifiable: unit tests must prove the getter/setter/
   defaults/descriptor semantics byte-exactly against the raw, and the title oracle
   must not regress. Removing the `DS_00105C00`/`DS_00105C05` seeds is *attempted*;
   if the real defaults path does not reproduce the captured `CREDITS:5`, that is a
   declared, evidence-backed gap (the capture's runtime config is not present in the
   shipped image) and the seed stays for that value only. The unseed is never forced
   by fitting a constant.

## 3. Evidence gathered during design

All addresses are original linear VAs; `file_offset = va + 0x52E54` for obj-0 code.

**Module cluster** `0x2D2F0`..`0x2DF8C` (function sizes/callers from the decompiler):

| function | size | callers | role |
|---|---|---|---|
| `0x2D2F0` | 3 | 2 | tiny accessor |
| `0x2D498` | 27 | 1 | bounded byte write into the EEPROM **storage image**: `addr < 0x80CE4 || addr >= 0x814DC` → `"attempt to write outside eeprom"`; else `*addr = dl`. The image is `0x80CE4..0x814DC` = `0x7F8` bytes. |
| `0x2D4B4` | 17 | 4 | `eax = ceil_pow2(eax + 1)` (round a size up to a power of two) |
| `0x2D4EC` | 319 | 17 | `(kind 0..8, value)`: maps a kind through the obj0 8-byte-entry table at `0x2D444` to a byte range in the storage image, enforces the `0x7F8` bound (`"Write extends past end of EEPROM"`), writes the value, and maintains `DS_00105E2F`/checksum bytes |
| `0x2D444` | — | obj0 | 9-entry × 8-byte descriptor table for `0x2D4EC` (u16 limit@0, u16 @2, u32 base@4) |
| `0x2D638` | 191 | 1 | storage load/validate: reads the stored image through `0x2E990` and checks the magic at its tail, copying a valid image into the working template |
| `0x2E990` | — | many | storage primitive (read/write a byte run at a storage offset); the original's EEPROM access |
| `0x2D6F8` | 633 | 1 | validate: magic check, defaults, load path |
| `0x2D974` | 149 | 24 | **field getter** |
| `0x2DA0C` | 213 | 14 | **field setter** |
| `0x2DAE4` | 116 | 18 | deferred |
| `0x2DB58` | 105 | 4 | deferred |
| `0x2DBC4` | 219 | 7 | deferred |
| `0x2DCA0` | 321 | 5 | deferred |
| `0x2DDE4` | 177 | 3 | deferred |
| `0x2DE98` | 241 | 1 | high-score validate — `"Bad High-score-table"` (VA `0x80B08`); deferred |
| `0x2DF8C` | 109 | 1 | deferred |

**Descriptor table** is in **obj-0** at VA `0x2D300` (file `0x80154`), 63 × 4-byte
entries. Decoded fields: bit position `(d >> 6) & 0xff`, bit width
`((d >> 14) & 7) + 1`, trailing-byte index `d & 0x3f`. Verified entries: field
`0x00` = `0x00024001`, `0x01` = `0x00024082`, `0x02` = `0x00024103`, … (bitpos
increasing, trailing index incrementing), field `0x29` = `0x0001D980` (width 8,
bitpos 102, no trailing byte), `0x2A` = `0x00001B80`, `0x35` = `0x0000E0C0`,
`0x37` = `0x000062C0`.

**Derived walk (getter `0x2D974`, verified instruction-by-instruction).**
`width = ((d >> 14) & 7) + 1`, `bitpos = (d >> 6) & 0xff`. `eax = bitpos + width`;
`ecx = eax >> 1`; if `eax & 1` (odd) the seed value is the **low nibble** read at
`DS_00105DE1 + ecx`, `ebx = ecx`, `width -= 1`; else the seed is `0` and
`ebx = ecx`. Then loop with **`ebx` decremented before each read**: while `width != 0`,
if `width == 1` take the **high nibble** at `DS_00105DE1 + ebx` and finish, else
`value = (value << 8) | DSB(0x00105DE1 + ebx)`, `width -= 2`. Finally, if
`(d & 0x3f) != 0`, `value = (value << 8) | DSB(0x00105DAF + (d & 0x3f))`.
Field `0x29` (bitpos 102, width 8) reads bytes at indices 54, 53, 52, 51, so
`value = b54<<24 | b53<<16 | b52<<8 | b51`, and `0x2C304`'s `& 0xF0000` extracts the
low nibble of `b53` (`DS_00105DE1 + 53`).

**Derived walk (setter `0x2DA0C`).** `field > 0x3E` → `0xFFFFFFFF`. Trailing byte
first: if `(d & 0x3f) != 0`, `DSB(0x00105DAF + (d & 0x3f)) = value`, `value >>= 8`,
and `DS_00105DD8 |= 1`. Then `DS_00105DD8 |= 6`; `ebx = (s32)bitpos >> 1`;
if `bitpos & 1`: write `(DSB(0x00105DE1 + ebx) & 0x0f) | ((value & 0x0f) << 4)` back
to `DS_00105DE0 + ebx + 1` (= `DS_00105DE1 + ebx`), `width -= 1`, `value >>= 4`.
Then while `width != 0`: if `width == 1`, `DSB(0x00105DE1 + ebx)` gets its high nibble
replaced by `value & 0x0f` and the loop ends, else `DSB(0x00105DE1 + ebx) = value`,
`ebx++`, `value >>= 8`, `width -= 2`. Finally two `0x2D4EC` calls (kinds 1 and 2),
which are no-ops in this cycle.

**Config byte region** `DS_00105D88 + 0x1000` (registered for the original's
memory-dump system by `0x109A0(&DAT_00105d88, 0x1000)` from the master init). The
getter indexes the region from `DS_00105DE1` backwards; a trailing byte comes from
`DS_00105DAF + (d & 0x3f)`. Validate scratch: `DS_00105DA4`/`DS_00105DA5`,
`DS_00105DA7`, flag byte `DS_00105DD8`.

**Magic / validate / defaults.** `0x2D6F8` compares the four bytes at
`DS_00105E30` against `0x9C94D2C4`; on mismatch it sets `DS_00105DD8 |= 6`, calls
`0x61A70`, `|= 1`, calls `0x61A70`, clears `DS_00105E2F`, calls the defaults
writer, then rewrites the magic. The defaults writer is `0x2CADC`
(`"SETTING CONFIGURATION DEFAULT VALUES"`, VA `0x809C8`), which draws the message
through the ported `0x2F198` and writes:

```
0x2cb04  eax = 0x29  edx = 0x2CCD0(0x22EB4)  -> setter   ; field 0x29
0x2cb0e  eax = 0x35  edx = 0xA0             -> setter
0x2cb1d  eax = 0x37  edx = 0xA0             -> setter
0x2cb2c  eax = 0x2A  edx = (get(0x2A) & 0xFC) | 3 -> setter
```

`0x2CCD0` parses an obj-0 menu-descriptor list at `0x22EB4` (runtime `0x32EB4`),
returning the bitfield of the entries flagged with `'*'` — i.e. field `0x29`'s
default is the default-selected menu option.

**Immediate bases differ by site — a real trap.** Inside this module, `0x9C8`
(the defaults message) is an obj-1 data pointer (`+0x80000` → `0x809C8`), while
`0x1D300` (descriptor table) and `0x22EB4` (menu table) are obj-0 pointers
(`+0x10000` → `0x2D300` / `0x32EB4`). Reading `0x1D300` as obj-1 yields the
meaningless alternating pattern `0xAAAA/0/0xFF0000/0/0x5555/…`; reading it as obj-0
yields the sensible table above. Every immediate in this cycle must have its base
determined per site, and the plan must say so.

**Boot site.** `0x2F9CC` (116 bytes, 10 callees) is the master init:
`0x13ADC` (`effects_init`) → `0x2D6F8` (config validate) → `_DAT_00107410 =
0x2D974() & 0xFFFFFFFC` → `0x2C9B8` → `0x2C8F0` ×2. The port has no `0x2F9CC`
equivalent; it enters the title directly and calls `effects_init` from
`actors.c:120` (`0x2BBB8`).

**Credit consumer.** `0x2C304` is
`DS_00105C00 = ((0x2D974(0x29) & 0xF0000) >> 16) + 1`, called from `0x10E80`
(game-state init). The getter returns a multi-byte value built from the config
bytes covering the field's bit range, which is why bits 16-19 are meaningful even
though the descriptor's width is 8.

## 4. Architecture

A new `port/src/game/config.{c,h}` owns the whole data layer. It reads the
descriptor table and the config bytes directly out of `mem[]` (both objects are
already loaded with fixups applied by `mem_load_le`), so no table or default value
is transcribed into C — every value comes from the shipped image or the raw bytes.

`config_validate()` is called once from the port's boot path immediately after
`effects_init`, mirroring `0x2F9CC`'s ordering, before the first title frame. The
plan pins the exact call site against the port's current entry.

Component list, with the exact semantics to derive in the plan:

| Port symbol | Original | Contract |
|---|---|---|
| `config_field_get(u32 field)` | `0x2D974` | `field > 0x3E` → `0xFFFFFFFF`. Descriptor `DSD(0x2D300 + field*4)`; width `((d>>14)&7)+1`; walk the byte/nibble array ending at `DS_00105DE1 + ((bitpos+width)>>1)`; if `(d & 0x3f) != 0`, shift the accumulated value left 8 and OR the byte at `DS_00105DAF + (d & 0x3f)`. The exact walk (nibble vs byte steps, shift order, backwards direction) is taken from the raw bytes, not the decompiler. |
| `config_field_set(u32 field, u32 value)` | `0x2DA0C` | The inverse walk, writing through the bounded helper. `field > 0x3E` → `0xFFFFFFFF`. |
| bounded-write helpers | `0x2D498`, `0x2D4B4`, `0x2D4EC` | Own the 2040-byte EEPROM storage image at `0x80CE4` and its `0x7F8` bound. **Because the port has no storage I/O, this image is inert and these three are declared no-ops**; the setter's three `0x2D4EC` calls become no-ops with it. Evidence for the bound is recorded in §3 so a later persistence cycle can port them. |
| `config_validate(void)` | `0x2D6F8` | Magic `0x9C94D2C4` at `DS_00105E30`; mismatch → flag bits, defaults, rewrite magic; else the load path. The storage calls (`0x2D638`'s read and `0x2D6F8`'s own `0x2E990` reads) become declared no-ops reporting "no stored image", so validate routes to defaults, and the deferred high-score call `0x2DE98` is a declared no-op. |
| `config_set_defaults(void)` | `0x2CADC` | Draw the defaults message through the ported text calls; write fields `0x29`/`0x35`/`0x37`/`0x2A` as above, with field `0x29`'s value from `0x2CCD0(0x32EB4)`. |
| `0x2CCD0` parser | `0x2CCD0` | The obj-0 menu-descriptor parser the defaults path needs (asterisk-flagged default selection). Ported with the defaults path. |

## 5. Consumers and pins

- `flow.c:698-705`: `DS_00104528 = 0` becomes `DSD(DS_00104528) = config_field_get(0x29)`, matching `0x20C5D-0x20CC2`.
- `0x2C304` is ported and called where `0x10E80` calls it, so `DS_00105C00` is derived from config rather than seeded.
- The 4a-iii title-overlay seeds `DS_00105C00 = 5` and `DS_00105C05 = 1` (`flow.c`) are attempted for removal. The credit countdown `0x2CA48`/`0x2CA7C` and the input handler `0x11F28` stay deferred with slice B.

## 6. Definition of done

1. `port/src/game/config.{c,h}` owns the module; build has 0 warnings.
2. Unit tests prove, against the raw bytes: setter→getter round-trip across widths
   1-8 and the trailing-byte case; `field > 0x3E` → `0xFFFFFFFF` for both getter and
   setter; the `DS_00105DD8` dirty flags the setter raises; and a zeroed config
   region produces the expected `DS_00105DD8` flags, the `0x9C94D2C4` magic, and
   consumer-field values that match the obj-0 default table. (The storage-image
   bound is not tested: that layer is a declared no-op, §7.)
3. `DS_00104528` is read from config in `flow.c`, not a literal.
4. The title oracle does not regress. Whether the seeds could be removed is
   recorded with evidence: green if the defaults path reproduces the captured
   values, otherwise a named gap with the derived value stated.
5. The full ladder passes: `make verify` exit 0, `all checks passed`, the capture
   comparison unchanged (this cycle does not touch audio), `symbols.h` byte-identical.

## 7. Declared gaps and non-goals

- **No save/load I/O, hence no storage image.** The storage primitive `0x2E990`,
  its `0x2D638` caller, the 2040-byte EEPROM image at `0x80CE4` and its writers
  (`0x2D498`, `0x2D4EC`, the `0x2D444` kind table, `0x2D4B4`) are declared no-ops,
  so the port behaves as a fresh EEPROM every run and `config_validate` always
  routes to the defaults path; the original persists config across runs and keeps a
  checksummed image. A host-side save file is a later cycle if wanted. (`0x2E990`
  also appears in the decompiler as `FUN_0002e990`, called from `0x2D6F8`'s read
  path and from `0x2D638`.)
- **Deferred module functions:** `0x2DAE4`, `0x2DB58`, `0x2DBC4`, `0x2DCA0`,
  `0x2DDE4`, `0x2DE98` (high-score), `0x2DF8C`. The high-score call inside
  `config_validate` is therefore a declared no-op.
- **Slice B stays out:** the input poll `0x11F28`, the `0x11D04` state machine, the
  `0x11000` attract sub-machine, and the credit countdown `0x2CA48`/`0x2CA7C`.
- **No fitted constants.** If the captured `CREDITS:5` is not reproducible from the
  shipped image plus the defaults path, the port keeps a documented seed for that
  value only and records the derived value and the reason.

## 8. Risks

- The getter's bit walk is intricate (backwards nibble/byte stepping with a
  trailing-byte shift) and the decompiler renders it poorly; the plan must derive it
  from the raw bytes and prove it with a round-trip test, not by inspection.
- Whether the defaults path reproduces the captured credit value is **unknown**;
  the design treats it as the cycle's central falsifiable question rather than a
  promised outcome.
- The `0x2CADC` defaults path draws text; wiring it at boot could add a first-frame
  visual the port did not previously have, which the title oracle may or may not
  observe. The plan must place the call to mirror `0x2F9CC` and check the oracle.
