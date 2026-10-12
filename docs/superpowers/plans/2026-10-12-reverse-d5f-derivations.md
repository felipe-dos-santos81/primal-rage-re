# Track D, wave 5, batch d5f — derivation record

Differential rows for five of the seven queue entries (`.superpowers/sdd/2026-10-11-reverse-d1/d5f-queue.txt`):
`0x1975C`, `0x129FC`, `0x128D4`, `0x4E67C`, `0x4D2D0`. `0x4BF18` and `0x49C78` are not done (§6). Every
value below comes from the image `./build/diffrun --exe data/game/C/PRAGE.EXE --image-out` writes (fixups
applied, linear addresses), disassembled with capstone, and from running the original bytes in
`tools/diff_emu.py`. Narrow claim, as for every row: equivalence on the exercised blocks and inputs only,
each function with its callees stubbed or run as stated.

## §1 The members

| addr | port function | size | callers and what they read of EAX |
|---|---|---|---|
| `0x1975C` | `fighter_think` (fighter.c) | 186 | `0x2633D`, `0x264CC`, `0x26629`: `xor eax,eax` at `0x2635B`, `0x264EA`, `0x26647` before any read; `0x274C7`, `0x29B2F`: `call 0x3cb68`, whose first use of EAX is the load at `0x3CB79`. Mask 0. |
| `0x129FC` | `camera_dust_hit` (camera.c) | 443 | one caller, `0x129C1`: `test eax,eax` at `0x129C6`. The function sets the whole register (`mov eax,1`; `xor eax,eax` `0x12BB1`; the point test's own 0 through `je 0x12bb3` `0x12A34`). Mask `0xFFFFFFFF`. |
| `0x128D4` | `camera_impact_dust_spawn` (camera.c) | 265 | one caller, `0x26D36`: `call 0x26d4c` at `0x26D3B` (its `jg` arm `0x26D5B` returns with EAX untouched, the other loads it at `0x26D6B`), `0x26D40..0x26D4B` return to `0x25326 jmp 0x2540f`, and from there every path loads EAX before reading it (the chain the `input_state_update` row, d2d, followed: `0x2A31C`'s `mov eax,[0x105bcc]` `0x2A353` or its early `ret`, then `mov ax,[0x104b00]` `0x2545C` or `0x3BB90`'s `xor eax,eax` `0x3BB9B`). Mask 0. |
| `0x4E67C` | `fight_4e67c` (fight.c) | 799 | one caller, `0x2685A`: `call 0x1282c` at `0x2685F`, whose first use of EAX is `mov ax,[0xef6dc]` `0x12834` .. `and eax,0xffff` `0x1283E`. Mask 0. |
| `0x4D2D0` | `fight_4d2d0` (fight.c) | 1233 | `0x26D28`: `cmp [0x104ac4],1; jle`, then `call 0x128d4` (EAX loaded at `0x128DC`) or `call 0x26d4c` and the chain above; `0x26FF0`: `mov ah,[0x104aec]` `0x26FF5` (AH only, written before it is read), then `mov eax,4` `0x2701C` or `call 0x12da8` `0x270AE`, which loads AL at `0x12DA9` and EAX at `0x12DBA`/`0x12DD2`, and returns to `0x25330 jmp 0x2540f`. Mask 0. |

## §2 The rows (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `fighter_think` | `0x1975C` | 8 | 9/9 | 37 | 0 | eight call-set callees (`0x39278` recorded and run), `0x33950` allowed. The two gates' stubs return 0x100 / 0xFFFFFF00 for "no" and 0xFFFFFF01 for "yes" (`test al,al` `0x197A0`, `0x197C4`). The count test is signed (`cmp esi,[..]; jge`, ESI = 2). |
| `fighter_think_rr` | `0x1975C` | 2 | 7/9 | 2 | 0 | the step stub `0x17CB0` writes both overlap counts and the first pass's burst stub `0x3B938` rewrites the second, which the raw reads at the second pass's `0x19780`. Unhit, named: `0x197A4`, `0x197C8` (the gate arms). |
| `camera_dust_hit` | `0x129FC` | 116 | 43/43 | 38 | `0xFFFFFFFF` | characters 0, 1, 2, 4 (the four table targets of `0x129E0`) with reactions 0..0xF, 0x2C..0x2E, 0x81, 0x82, 0xFF; characters 3, 5, 6 with seven reactions; 7, 0x86, 0xFF refused (`cmp al,6; ja`); the point test's EAX 1, 2, 3, 0x100 (`test eax,eax` `0x12A32`), 0xFFFFFFFF (signed `jle` `0x12A3D`; the slot is `0x1077B0 - 2 * 0x94`, inside the image) and 0. |
| `camera_dust_hit_rr` | `0x129FC` | 4 | 28/43 | 1 | `0xFFFFFFFF` | the point-test stub relinks the node's +8. 15 blocks named (the reaction chains past their first members, the refusals). |
| `camera_impact_dust_spawn` | `0x128D4` | 10 | 15/15 | 34 | 0 | rng, SPAWN and the hit test stubbed. Case `s8` (side byte 0x80) reads the slot above the image: `D5F_OUTSIDE = [(0x10C202, 1)]`. |
| `camera_impact_dust_spawn_rr` | `0x128D4` | 2 | 9/15 | 1 | 0 | the hit-test stub rewrites the gate byte `0xF0AE8`, which the raw reads at every node (`0x129B1`). 6 blocks named (the spawn half). |
| `camera_impact_dust_spawn_rl` | `0x128D4` | 2 | 9/15 | 1 | 0 | the hit-test stub cuts its node out of the list; the raw loaded the link before the call (`0x129B7`). Same 6 blocks named. |
| `fight_4e67c` | `0x4E67C` | 18 | 47/47 | 75 | 0 | ten stubs. One stub EAX serves both `0x2BE00` calls of a case, so "big" (`> 0x1C00`) and the state-2 teardown (`< 0`) are cased apart. |
| `fight_4e67c_rr` | `0x4E67C` | 2 | 32/47 | 5 | 0 | stubs rewrite the side byte `0x1088BC`, the node's +8 (after `0x4A808`, and after the state-2 `0x2BE00`: that node sits at its own record's +0x58 so a write at the argument's +0x60 is its +8), the node's state byte (`0x4E5A4`) and the round byte (`0x4E99C`). 15 blocks named. |
| `fight_4d2d0` | `0x4D2D0` | 25 | 65/65 | 97 | 0 | nine call-set callees; the rng is recorded and run (a single stub EAX cannot walk type 4's draw tree `0x4D519`/`0x4D53F`/`0x4D567`), the leaf `0x496AC` allowed. Seeds are chosen by `d5f_seed` for a zero / non-zero pattern of the rng(2) draws. |
| `fight_4d2d0_rr` | `0x4D2D0` | 8 | 50/65 | 2 | 0 | mode 0x22 with a prelude stub `0x4D7A4` that relinks the node's +8: the raw uses the actor it kept in ECX (`0x4D2F8`) at some sites and `[ebx+8]` loaded again at the others. The port agreed at every site (no correction). 15 blocks named. |

11 specs, 293 mutants, every one detected (`/tmp/pr_d5f/d5pins.py`: `pins: 11 rows, 293 mutants; BAD: []`).
The pins (`D5F_MASKS`, `D5F_KINDS`, `D5F_CASES`, `D5F_OUTSIDE`) are the measured ones. Each row and its
mutants were run ten times with identical output.

Callee declarations (args from the callee's entry bytes, `pop` = its `ret N`, clobbers =
`E.callee_clobbers`):

| callee | args | clobbers | how |
|---|---|---|---|
| `0x17CB0` | none | edi, ebp | stub (new seam) |
| `0x3B464` | eax (`mov ecx,eax` `0x3B46B`) | edi, ebp | stub (new seam) |
| `0x3962C`, `0x396AC` | eax, edx (DL) | edx (§4) | stub |
| `0x17D30` | eax, edx, ebx (`movsx edx,ax` `0x17D64`; `mov esi,edx` `0x17D37`; `mov [esp+0x1c],ebx`) | ebx, edx | stub (new seam) |
| `0x12BB8` | eax, edx | edx, edi, ebp | stub (new seam) |
| `0x129FC` | eax | edi, ebp | stub (new seam) |
| `0x4A808`, `0x4A8A8` | eax | none | stub (new seams) |
| `0x4E5A4` | eax, edx (`mov esi,eax; mov ecx,edx` `0x4E5A7`) | edx | stub (new seam, static function) |
| `0x4E99C` | eax (`mov ebx,eax` `0x4E9A3`) | none | stub (new seam) |
| `0x49444` | eax | none (§4) | stub |
| `0x4D7A4`, `0x4AC38`, `0x4D224` | eax, edx | edx | stub (new seams) |
| `0x4987C` | eax, edx, ebx (`0x49883`, `0x49887`, `0x4988B`) | ebx, edx | stub (new seam) |
| `0x1922C`, `0x18B44`, `0x3B938`, `0x2AE14`, `0x2BC30`, `0x2BE00`, `0x2BE1C`, `0x4BD4C`, `0x4EBB8`, `0x4E350`, `0x2B150` | the existing constants | theirs | stub |
| `0x5D7DC` | eax | none | stub in `0x128D4` and `0x4E67C`; recorded and run in `0x4D2D0` |
| `0x39278` | eax | - | recorded and run (`D2A_39278`) |
| `0x33950`, `0x496AC` | - | - | allowed (leaves, run on both sides) |

## §3 Corrections (address-proved; the raw wins)

1. **`fighter_think` tests AL of its two gates** — `0x1979B call 0x3962c; 0x197A0 test al,al` and `0x197BF
   call 0x396ac; 0x197C4 test al,al`. Old: `fighter_3962c(..) != 0 || fighter_396ac(..) != 0`; new:
   `(u8)f(..) != 0u`. The port's own callees return 0 or 1, so nothing changes in the game. Cases `t2`,
   `t3`, `t4`; mutants `@eax1`, `@eax2`.
2. **`camera_dust_hit` loads the node's actor again for the burst** — `0x12A99 mov eax,[ecx+8]` (and
   `0x12AF5`, `0x12B47`, `0x12B98`), after the point test called at `0x12A2B`. Old: the record read at
   `0x12A01` was kept. No in-game change (the port's `camera_point_hit` does not write the node).
   `camera_dust_hit_rr`'s four cases; mutant `@cache`.
3. **`camera_impact_dust_spawn`'s a4 is the draw's low word, sign-extended** — `0x12941 mov [esp],di`,
   `0x12948 mov ebx,[esp-2]`, `0x12956 sar ebx,0x10`. Old: `rng_next(0x2000u)` whole; new:
   `(u32)(s32)(s16)rng_next(0x2000u)`. rng(0x2000) is below 0x2000, so nothing changes in the game; the
   row's stub returns 0x18004, 0xFFFF7FF8, 0x20004 (cases `s2`, `s6`, `s7`). Mutants `@a4full`, `@a4z`.
4. **`fight_4e67c`'s last gate was inverted** — `0x4E97A test edx,edx; 0x4E97C je 0x4e983` (even round:
   the call), `0x4E97E test si,si; 0x4E981 jne 0x4e992` (odd round with a non-zero processed count: no
   call). Old: `even || processed != 0u`; new: `even || (u16)processed == 0u`. **A gameplay defect of mode
   0x25**: on an odd round the port called `0x4E350` while entries were still being processed and did not
   when none was. Cases `m2`, `m3`, `ma` (no call) and `md` (the call); mutant `@spawninv`.
5. **`fight_4e67c`'s `0x108894` index comes from a 16-bit difference** — `0x4E6D5 mov ax,[eax+0x32]`,
   `0x4E6DE sub eax,0xe0`, `0x4E6E8 test ax,ax`, `0x4E6EB jle`, `0x4E6ED movsx edx,ax`. Old: `(s32)(s16)w -
   0xE0`; new: `(s32)(s16)(u16)(w - 0xE0u)`. A word in 0x8000..0x80DF wraps to a large positive
   difference (index 15) where the port had a negative one (index 0). Case `m7` (0x8010); mutant `@wrap`.
6. **`fight_4e67c` loads the fighter record and the node's actor again after its calls** — the side byte
   and its slot's record at `0x4E6B8..0x4E6CE` (after `0x2BE00`) and `0x4E799..0x4E7A6` (every odd-round
   node); the node's +8 at `0x4E758`/`0x4E761` (after `0x4A808`) and `0x4E8E4`/`0x4E8EB` (after the state-2
   `0x2BE00`). Old: the values read before those calls. No in-game change. `fight_4e67c_rr`'s `ra`, `rb`;
   mutants `@pcache`, `@rcache`, `@r2cache`.

Corrections 1, 2, 3 and 6 are conformity corrections (unobservable with the port's own callees);
4 and 5 change behaviour. None has been run against the capture oracles (the integrator's gate);
`PR_ORACLE_REQUIRED=1 ./build/run_tests` passes on every commit's final state.

## §4 Named gaps, unhit blocks, clobber fixes, limits

**`_CALLEE_CLOBBER_FIXES` additions (`tools/diff_emu.py`).**

- `0x3962C: ("edx",)`, `0x396AC: ("edx",)`. `0x3962C` saves EBX (`push ebx`; `pop ebx` `0x396A1`,
  `0x396A8`) and writes only EAX, EBX, EDX; `0x396AC` saves EBX and ECX and writes only EAX, EBX, ECX,
  EDX. Callees: `0x33950` (EDX) and `0x3AFC4`, which saves ECX and writes EAX, ECX, EDX. The scan's
  (esi, edi, ebp) comes only from `0x3AFC4`'s direct target `0x62003`, the runtime's fatal exit, which
  does not return (the `0x46C78` precedent). `0x1975C` keeps ESI = 2 across both calls (`0x1976D mov esi,2`;
  read at `0x197FD mov eax,esi` and at the second pass's `0x19780`). Measured: with the scan's set the
  original reaches `0x39278` with EAX = 0xC10BBE2D (the poison) on a case whose port call is `0x39278(2)`.
- `0x49444: ()`. It pushes EBX, ECX, EDX, ESI and pops them before its one `ret`
  (`0x49444..0x49447`, `0x494A0..0x494A3`); its own instructions write only those and EAX. Its callees
  `0x249B0`, `0x249D0` clobber nothing and `0x2B150` is `()` by the existing fix, which the fixpoint does
  not apply to an inner callee: the scan's (edi, ebp) is `0x2B150`'s uncorrected set. `0x4E67C` keeps the
  next node in EDI across the call (`0x4E730 mov edi,[ebx]`; call `0x4E8EE`; `0x4E8F3 mov ebx,edi`).
  Measured: with the scan's set case `md` of `fight_4e67c` faults (the loop follows the poison).

These change declared sets that exist outside this batch's blocks; the batch did **not** edit them. The
integrator has to change, with the fix:

- `tools/diff_verify.py`: `D3C_3962C = E.Call(0x3962C, ("eax", "edx"), clobbers=("edx", "esi", "edi", "ebp"))`
  and `D3C_396AC = ...` -> `clobbers=("edx",)`; `P7_49444 = E.Call(0x49444, ("eax",), clobbers=("edi", "ebp"))`
  -> no clobbers.
- `tools/tests/test_diff_verify.py`, the expected dictionary of
  `test_each_stub_declares_the_registers_its_callee_clobbers`: `0x3962C: ("edx", "esi", "edi", "ebp"),
  0x396AC: ("edx", "esi", "edi", "ebp")` -> `("edx",)` each; `0x49444: ("edi", "ebp")` -> `()`.

The rows that use those constants (`fighter_winner_body`; `fighter_4b03c`) were re-measured with the
corrected sets substituted (a script, not an edit): §7.

**Equivalent mutants, not bound.** In `0x129FC`, characters 0/3/5 (`0x12A7A`: 0x2D, or at most 0xD with
bit 1 clear) and characters 2/4 (`0x12B13`, `0x12B65`) accept the same nine reactions 0, 1, 4, 5, 8, 9,
0xC, 0xD, 0x2D, so moving a character between those two groups changes nothing. In `0x4E67C`, `v < 0`
for `v <= 0` at `0x4E6EB` is the same (0 / 100 = 0).

**Limits.**

- `0x4A808` and `0x4A8A8` set the whole EAX to 0 or 1 (`mov eax,1` `0x4A85A`/`0x4A8F9`, `xor eax,eax`
  `0x4A862`/`0x4A902`) and `0x4E67C` tests it whole (`test eax,eax` `0x4E750`, `0x4E876`). The port's
  functions return `u8`, so the stubs return 0 or 1 only; a stub EAX of 0x100 would make the raw take the
  arm and the `u8` return not. Not a divergence the real callees can produce; the return types were left.
- `0x4D2D0`: the port does not model a side byte `0x104B1A` above 1 (the raw counts into its saved
  registers at `0x4D313` and reads them at `0x4D77E`; the two `PORT:` comments). Not cased.
- `0x4D2D0`'s rng runs for real, so its `test eax,eax` after each rng(2) (`0x4D49E`, `0x4D51E`, `0x4D544`,
  `0x4D56C`) sees 0 or 1 only.
- `0x4E67C` passes `(u16)processed` to `0x4E99C` (`0x4E91C xor eax,eax; mov ax,si`) and the port the
  whole count; they differ only past 65535 list nodes. Not cased.
- `0x128D4`: the raw's `mov ebx,[esp-2]` reads two bytes below ESP; `sar ebx,0x10` discards them.

**Unhit blocks.** The five main rows hit every block. The six re-read specs name theirs in
`unhit_named`, each run by the main row of the same entry (`/tmp/pr_d5f/unhit.py` re-derived every
list: none named but hit, none unnamed).

## §5 The store sweep

Every field a function writes is seeded with a value different from what it writes, between seeded
neighbours; one dropped store per field is a bound mutant.

- `0x1975C`: slot +0x8A (seeded 0x5A/0x5B between 0x89 and 0x8B: `@no8a`, `@o8a`, `@v8a`, `@w8a`), slot
  +0x67 (`@no67`, `@o67`, `@v67`, `@w67`, `@late67` by the memory at the call).
- `0x129FC`: `0xF0AE8` (seeded 0xEE between 0xE7 and 0xE9: `@nostore`, `@storech`, `@storew`, `@early`).
- `0x128D4`: the spawned record's +0x34 word (`@nostore34`, `@o34`, `@w34d`, `@noguard`), `0xF0AE8`
  (`@noreset`, `@rv`).
- `0x4E67C`: `0x108894` (`@nozero`, `@b94`), the actor's +0x32/+0x34/+0x36/+0x38, the node's +0x1C bit 7
  and +0x1E, the partner's +0x18 and +0x32 (`@copyw`, `@copyd` for the widths), the actor's +0x28 bit 7,
  `0x1088BE` (`@nobe`, `@nobe0`), `0x1088B8` (`@nob8z`, `@b8first`).
- `0x4D2D0`: per type, the actor's +0x1C, +0x28, +0x29, +0x2C, +0x34, +0x36, +0x38, the node's +0x10,
  +0x18, +0x1C, +0x1E, +0x1F, the partner's +0x18/+0x32, `0x1088C2` (`@noc2z`); the list is in the
  spec's mutant tuple.

Finding: none beyond §3 (no missing or extra store in the port on the exercised paths).

## §6 Queue entries not done

- `0x4BF18` (1697 bytes, 17 callees: over diffrun's 16 stubs per case, so it needs specs per arm) and
  `0x49C78` (2492 bytes, 22 callees): not started beyond reading `0x4BF18`'s first half. Both walk the
  same list `0x10884C` with a nine-entry type table (`0x4BEF4` for `0x4BF18`) whose arms are close twins
  of `0x4D2D0`'s; the fixtures `d5f_x`, `d5f_x_rec`, `d5f_fx_case` and the cached-actor / loaded-again
  re-read spec (`fight_4d2d0_rr`) are the pattern to reuse. `0x4BF18` keeps the actor in ESI and the
  entry in ECX (`0x4BF65 mov esi,[ecx+8]`), so the same site-by-site question applies. Their unrowed
  callees `0x4C60C` and `0x4B69C` belong to other batches (stub them).
- No named gap in this batch: all five functions run in the harness.

## §7 Measurements that are not rows

- The existing rows that stub `0x3962C`/`0x396AC` (`fighter_winner_body`) and `0x49444`
  (`fighter_4b03c`), re-measured with the corrected clobber sets substituted (`/tmp/pr_d5f/altclob.py`, no
  file edited): `fighter_4b03c declared: ('VERIFIED', 22, 22) corrected: ('VERIFIED', 22, 22) mutants 11
  identical`; `fighter_winner_body declared: ('VERIFIED', 19, 19) corrected: ('VERIFIED', 19, 19) mutants
  52 identical` (the verdict, the blocks and every mutant's problem list are the same under both sets).
- The existing rows on a function this batch seamed (`camera_dust_burst`, `fight_4987c`, `fight_4a808`,
  `fight_4a8a8`, `fight_4ac38`, `fight_4d224`, `fight_4d224_rr`, `fight_4e99c`, `fighter_think_side`) were
  re-run with `--function .. --self-check`: each VERIFIED with every mutant detected.
- `/tmp/pr_d5f/stubcheck.py`: every stub of `D5F_SPECS` declares `E.callee_clobbers`; the only
  disagreeing declarations elsewhere in `SPECS` are the three of §4.
