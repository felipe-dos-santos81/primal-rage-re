# Effect producers (4a-iv) — cycle report

Sub-project 4a-iv ported the three missing `0x13xxx` effect producers so every
reachable effect type is constructible, and proved the
spawn → step → palette dirty list → `gfx_dac` chain end-to-end. Commit history:
`effects: port the type-4 producer (0x13D4C)`,
`effects: port the type-6 producer (0x13E28)`,
`effects: port the types-0/2 producer (0x13B3C)`, and this cycle's
`docs: record the effect-producer outcome`.

Spec: `../specs/2026-09-19-effects-producers-design.md`. Raw derivations:
`2026-09-19-effects-producers-derivations.md`.

## 1. The four live producers

The free-list head `DS_000FCCE8` is taken only by these four functions; each
writes the record type byte, so exactly these types are constructible.

| producer | type (`+0x0C`) | contract | port function |
|---|---|---|---|
| `0x13C70` | **3** | `+0x0F=0x80`, `+0x10` zeros, `+0x410` = resolved `[1+i]`, count `++` | `effects_spawn` (pre-existing) |
| `0x13D4C` | **4** | `+0x0F=0`, `+0x10` = resolved `[1+i]`, no `+0x410` fill, count `++` | `effects_spawn_darken` |
| `0x13E28` | **6** | `+0x0F=0x80`, `+0x10` white `0x00FFFFFF`, `+0x410` = resolved `[1+i]`, count `++` | `effects_spawn_pulse` |
| `0x13B3C` | **0** or **2** | flag 0 → type 0 / `+0x0E=0`; flag ≠ 0 → type 2 / `+0x0E=1`; `+0x14` **and** `+0x414` = resolved walked by signed offset; count **not** incremented | `effects_spawn_scroll` |

## 2. Types 1 and 5 are dead

Only a free-list pop can obtain a fresh record, and the four functions above are
the complete set that reference `DS_000FCCE8` and then write a type byte. No path
writes type `1` or `5`, so both `effects_step` bodies are unreachable in the
shipped EXE. The transcribed bodies stay (unreachable, not wrong). Proof per
function, with file offsets: `2026-09-19-effects-producers-derivations.md` §4
(`0x13B3C` writes `0`/`2` at `0x13C2F`/`0x13C25`; `0x13C70` type `3` at
`0x13CC4`; `0x13D4C` type `4` at `0x13D91`; `0x13E28` type `6` at `0x13E71`).
The out-of-line pop helper `0x133D0` has zero callers and produces nothing.

## 3. Raw evidence per producer

* `0x13D4C` (type 4): handle has **no incoming register** — `0x13D86 8b06 mov
  eax,[esi]` reads `DSD(source_rec)` immediately before `call 0x1b544`;
  `+0x0C=4` `0x13D91`, `+0x0F=0` `0x13D8D`, `+0x0E=1` `0x13DCB`; copy
  `resolved[1+i]` at `0x13DAB..0x13DAE`; no `+0x410` store; count `++`
  `0x13DD4..`. Derivation §1/§3.
* `0x13E28` (type 6): handle read at `0x13E66 8b01 mov eax,[ecx]`; `+0x0C=6`
  `0x13E71`, `+0x0F=0x80` `0x13E6D`; white fill `0x13E86`; resolved copy to
  `+0x410` `0x13EAB`; the two loops are strictly sequential, observable at
  `count == 257`. Derivation §6/§7.
* `0x13B3C` (types 0/2): source `EAX` (`0x13B41`), signed offset `DL`
  (`0x13B43`, arm test `0x13B93 test dl,dl`), flag `BL` (`0x13B47`, select
  `0x13C1D`), count `CL` (`0x13B4B`); handle read `0x13B85 mov eax,[edi]`.
  Derivation §8–§12.
* The dispatch that binds type 4 to its step body is the jump table at VA
  `0x134a8` (file `0x662fc`); entries 0..5 hold `target - 0x10000` for types
  1..6, and entry 3 is `0x374f` → VA `0x1374f`, the subtract-8-clamp-0 loop over
  `rec+0x10`. Recorded in the derivations doc §13.

## 4. The two `0x13B3C` open questions

1. **Does it increment `DS_0009AF3D`? No.** The body `0x13B3C..0x13C6F` writes
   only the lock `0x1AF3C` (`0x13B68`, `0x13B75`, `0x13C4D`, `0x13C62`); there is
   no access to `0x1AF3D`. A scroll record is inserted but invisible to
   `effects_active()`, faithfully.
2. **Does the zero-flag arm set `+0x0E = 0`? Yes.** `0x13C1D mov dh,[esp+4]` /
   `test dh,dh; je 0x13C2F`; zero path `0x13C2F 88760c` (type 0) and
   `0x13C32 88760e` (`+0x0E = 0`). `effects_step`'s type-0 branch skips while
   `+0x0E == 0` and never reloads it, so a type-0 record never retires.
   Reproduced as-is; named as original behaviour, not "fixed".

## 5. End-to-end result

`port/tests/test_effects.c` spawns via `effects_spawn_darken` (type 4), calls
`effects_step()` once, drains through `gfx_flush_palette()`, and asserts
`gfx_dac[0x40][0..2]` and `gfx_dac[0x41][0]` equal `0x38`.

The brief's expected `0x38` held. Derivation: each step's case-4 body subtracts 8
from every colour lane (`0x1374f`: `0x13767 83ea08 sub edx,8`, clamp to 0), so
`0x40 → 0x38`; `gfx_flush_palette` truncates each lane to 6 bits and expands,
`(0x38 >> 2) & 0x3F = 0x0E`, `(0x0E << 2) | (0x0E >> 4) = 0x38`. No constant was
fitted.

The test passed on first run (the producers and step wiring were already landed
by Tasks 1–3), so per the brief's Step 2 it was tightened to drive eight further
`effects_step()` calls — those produce `0x30, 0x28, …, 0x00` (the first call
supplied the `0x38`) — and nine total retire the record; `effects_active() == 0`
proves it cannot pass vacuously.

Type 6 (`effects_spawn_pulse`) and type 2 (`effects_spawn_scroll`, flag ≠ 0)
have matching end-to-end tests added to satisfy spec §6. Type 6 drains white
`0xFF`, then `0xF7` after one darken, and retires after its 24-step darken to
the target. Type 2 rotates `[A,B] → [B,A]` and back across two drains; its
non-vacuous proof is the drain reset, since a type-2 record has no retirement
arm. Derivations §15/§16.

## 6. Declared gaps

* **No palette capture oracle.** The chain is proven port-side only; that the
  port's rendered colours equal the original's is not claimed (spec §7).
* **No call-site wiring.** `0x29B74`, `0x41578` and `0x11F6C` remain unported, so
  no shipped game path spawns types 0/2/4/6 yet. The end-to-end test is the only
  live invocation.
* **Camera/scene functions untouched.** `0x1317C`, `0x1324C`, `0x13290`,
  `0x1333C` and their `DS_000F0AEC`/`DS_000F0AF0`/`DS_000F0AF4` state remain.
* The `0x13xxx` effect **render** path is still unported, so an effect mutates
  the palette but draws nothing.

## 7. Verification

Full ladder `rm -rf build && make verify`: exit 0, 0 warnings, `all checks
passed`; title oracle and `smk_compare` unchanged; `port/src/symbols.h`
byte-identical; no `data/` or `tools/` change.
