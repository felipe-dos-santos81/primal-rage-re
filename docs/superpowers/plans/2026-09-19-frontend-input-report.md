# Front-end input / credits / select state (4b-B) — cycle report

Plan: `docs/superpowers/plans/2026-09-19-frontend-input.md`.
Derivations: `docs/superpowers/plans/2026-09-19-frontend-input-derivations.md`.
Branch `frontend-input`, base `c33bbf7`. Tasks 1-3 committed as `64f285a`
(credit layer), `f7e6d84` (input bitfield), `7313704` (select state). Task 4
(this report) wires them and records the outcome.

## 1. The modules and their ownership

Three layers, each in its natural owner:

| module | owns | originals |
|---|---|---|
| `port/src/platform/input.c` | the host-key sampler and the input bitfield | `0x500C4` `input_pump`, `0x50161` `input_select_bits`, `0x4F644` `input_state_update` |
| `port/src/game/config.c` | the credit layer | `0x2CAA8`, `0x2CA2C`, `0x2C060`, `0x2CA48`, `0x2CA7C`, `0x2C06C`, `0x2BF00` |
| `port/src/game/flow.c` | the coin poll, the select state and the wiring | `0x11F28` `frontend_coin_poll`, `0x11F6C` `game_state_select`, `0x33904` `frontend_list_next`, `0x1C6D4` `frontend_resource_known` |

Wiring added this task:

* `game_loop` fills the key bitmap (BIOS scan byte at `DS_00101514`+`0x2d8`,
  ASCII byte at `+0x2d9`) from `host_key_bits()` immediately before
  `input_pump()` (`0x500C4`). `host.c`'s `k_input_bind` table is the binding;
  bit 0 is the coin input.
* `game_frame` calls `input_state_update()` at the `0x24C6E` site as its first
  action — before the frame counter and the process tables — so
  `DS_001088E4`/`DS_001088D8` are fresh for `game_state_step`; the raw guards
  that call with `cmp eax,0x27; je`, so the port calls it only when
  `DSW(DS_00104B00) != 0x27`.
* `game_state_step` calls `frontend_coin_poll(0)` and `(1)`.
* `game_init` calls `config_set_credit_row_init()` (`0x2BF00` → `DS_00105C05 =
  0x1D`) in the `0x20CCC` block.

## 2. Step 1 — the raw derivation of `0x11D04` / `0x11F28`

`file_offset = va + 0x52E54` for object 0. Disassembly run before editing:

```
=== 0x11D04 ===
0x11d07 8a251d4b0800      mov ah, byte ptr [0x84b1d]
0x11d0d 31d2              xor edx, edx
0x11d0f 84e4              test ah, ah
0x11d11 7537              jne 0x11d4a
0x11d13 31c0              xor eax, eax
0x11d15 e80e020000        call 0x11f28
0x11d1a 85c0              test eax, eax
0x11d1c 7405              je 0x11d23
0x11d1e ba01000000        mov edx, 1
0x11d23 b801000000        mov eax, 1
0x11d28 e8fb010000        call 0x11f28
0x11d2d 85c0              test eax, eax
0x11d2f 7403              je 0x11d34
0x11d31 80ca02            or dl, 2
0x11d34 85d2              test edx, edx
0x11d36 7412              je 0x11d4a
0x11d38 31c0              xor eax, eax
0x11d3a e8310c0200        call 0x32970
0x11d3f 89d0              mov eax, edx
0x11d41 e85e3a0100        call 0x257a4
0x11d46 5a                pop edx
0x11d47 59                pop ecx
0x11d48 5b                pop ebx
0x11d49 c3                ret
0x11d4a 66a1640a0700      mov ax, word ptr [0x70a64]
0x11d50 663d0900          cmp ax, 9
0x11d54 771a              ja 0x11d70
...
0x11d69 2effa2dc1c0000    jmp dword ptr cs:[edx + 0x1cdc]

=== 0x11F28 ===
0x11f28 52                push edx
0x11f29 89c2              mov edx, eax
0x11f2b e830a10100        call 0x2c060
0x11f30 85c0              test eax, eax
0x11f32 7421              je 0x11f55
0x11f34 a1e4880800        mov eax, dword ptr [0x888e4]
0x11f39 850495bcac0100    test dword ptr [edx*4 + 0x1acbc], eax
0x11f40 7411              je 0x11f53
0x11f42 b801000000        mov eax, 1
0x11f47 e830ab0100        call 0x2ca7c
0x11f4c b801000000        mov eax, 1
0x11f51 5a                pop edx
0x11f52 c3                ret
0x11f53 31c0              xor eax, eax
0x11f55 5a                pop edx
0x11f56 c3                ret
```

Register/data-flow answers:

* `0x11D07` reads `DS_00104B1D` (`0x84b1d` + obj-1 base `0x80000`); `edx = 0`.
* Gate: if `DSB(DS_00104B1D) != 0` the poll is skipped (`jne 0x11d4a`) and the
  state dispatch runs.
* Call 1: `eax = 0` at `0x11D13`, `call 0x11F28` at `0x11D15` (event code 0);
  a non-zero return sets `edx = 1` (`0x11D1E`).
* Call 2: `eax = 1` at `0x11D23`, `call 0x11F28` at `0x11D28` (event code 1);
  a non-zero return does `or dl, 2` (`0x11D31`, `edx |= 2`).
* Either accepted (`edx != 0`): `eax = 0`, `call 0x32970`; `eax = edx`,
  `call 0x257A4`; then `pop/pop/pop/ret` — **an early return from `0x11D04`**,
  before the `0x11D4A` state dispatch.
* `0x11F28`: `edx = code`; `call 0x2C060` (`config_credit_ready`), `0` when it
  returns 0; `eax = DSD(DS_001088E4)` (`0x888e4` fixed to `0x1088E4`); the test
  `[edx*4 + 0x1acbc] & eax` uses the event's mask in `DS_0009ACBC`
  (`0x1acbc` fixed to `0x9ACBC`); `0` when clear; `call 0x2CA7C`
  (`config_credit_spend(1)`) and return `1`.

## 3. Where the raw contradicted the design (the early-return errata)

The plan's Step 2 note said the state dispatch "still runs" after an accepted
credit. The raw does not: `0x11D38`-`0x11D49` calls `0x32970(0)` and
`0x257a4(accepted)` and returns from `0x11D04`. The port implements the early
return, so an accepted coin skips the state dispatch (and the `0x2BF08` overlay
tail) for that frame. Both divert handlers are unported; the code carries a
`/* PORT: */` note naming them.

Other raw-vs-plan corrections carried into Task 4:

* `0x11F28`'s table is `DS_0009ACBC`, not a code-segment `0x1ACBC` (the raw
  displacement fixes by the obj-1 base `0x80000`).
* `0x11D15`/`0x11D28` receive event codes **0 and 1**, not a single code; the
  accepted mask is `1` for the first and `2` for the second (`edx |= 1`,
  `edx |= 2`), and `edx` (not the code) is what `0x257A4` receives.
* `0x11F28` returns 1 (not the debit result), so `frontend_coin_poll` returns 1
  after the spend.
* The plan's Task 4 Step 3 said to call `input_state_update()` unconditionally.
  The raw `0x24C5C` is `mov ax,[0x84b00]` (`0x24C63`), `cmp eax,0x27`
  (`0x24C69`), `je 0x24C73` (`0x24C6C`), `call 0x4F644` (`0x24C6E`): the call is
  guarded and skipped when `DSW(DS_00104B00) == 0x27`. The port now matches the
  raw; the plan text is errata.

## 4. The derived bodies

* `frontend_coin_poll(code)` (`0x11F28`): `config_credit_ready()`; test
  `DSD(DS_0009ACBC + code*4) & DSD(DS_001088E4)`; `config_credit_spend(1)`.
* `game_state_step`: when `DSB(DS_00104B1D) == 0`, poll code 0 into `accepted`
  bit 1 and code 1 into bit 2; on any acceptance return before the dispatch.
* `game_frame`: `input_state_update()` before the `DS_00104B00` switch.
* `game_loop`: host bitmap fill before `input_pump()`.
* `game_init`: `config_set_credit_row_init()` then the documented `= 1` stand-in.

## 5. The determinism oracle

Task 3's gated `PR_FRONTEND_DUMP` driver, re-run after the Task 4 wiring:

```
$ mkdir -p /tmp/pr_s4_a /tmp/pr_s4_b
$ cmake --build build
$ PR_FRONTEND_DUMP=/tmp/pr_s4_a PR_GAME_DIR=data/game/C ./build/run_tests
all checks passed
A_exit=0
$ PR_FRONTEND_DUMP=/tmp/pr_s4_b PR_GAME_DIR=data/game/C ./build/run_tests
all checks passed
B_exit=0
$ diff /tmp/pr_s4_a/select.log /tmp/pr_s4_b/select.log && echo "state-2 frames deterministic"
state-2 frames deterministic
diff_exit=0
```

Silent diff; both runs green. The wiring did not change the log relative to
Task 3: headless input is all-zero, so `frontend_coin_poll` never reaches the
spend (the mask test fails) and the early return is never taken; the frame
sequence is unchanged. The log is 640 lines; the selector's first phase-3
transition still lands at iteration 588/589.

## 6. The surviving `DS_00105C05` stand-in

The raw's init chain writes the overlay row through `0x2BF00` (`mov byte
[0x85c05], 0x1d`) at the `0x20CCC` site — now ported as
`config_set_credit_row_init()`. The captured title renders row **1**, written by
`0x2C06C(1)` at `0x110CE` inside the attract machine `0x11000` (still unported,
sub-project 4d) and by `0x11F6C` phase 0 (live, but only reached once state 2
runs). Until `0x11000` lands the port keeps the documented stand-in
`DSB(DS_00105C05) = 1` immediately after the init write, with a
`TODO(verify): remove when 0x11000 lands`. The title oracle confirms it still
produces the captured row (`0 unexplained` on both captures).

## 7. Full ladder

`rm -rf build && make verify` → exit 0.

```
c_warnings=0
oracle C-vs-Python: 9340 writes byte-exact
capture oracle first difference at C write 24: C tick=68 reg=0xe1 val=0000 vs capture tick=68 reg=0xc1 val=0x34 (C 9340 writes, capture 6380 normalised)
all checks passed                        (headless / gated tests)
smk_compare: 120/120 frames match
smk_compare: 41/41 frames match
all checks passed                        (smacker oracle)
title_compare: capture 1: 111 frames in window: 54 clean, 55 splice, 2 transition, 0 unexplained
title_compare: capture 2: 111 frames in window: 54 clean, 57 splice, 0 transition, 0 unexplained
all checks passed                        (title oracle)
Ran 32 tests in 1.237s ... OK
1304 globals, 1206 functions -> port/src/symbols.h
all checks passed                        (symbols + tool tests)
```

`port/src/symbols.h` regenerates byte-identically (`git diff --quiet` clean).
The `capture oracle first difference at C write 24` line is the pre-existing,
informational capture-comparison line and is unchanged (this cycle does not
touch audio). The Python `ResourceWarning`s in the tool tests are pre-existing
unclosed-file warnings in `tools/gra_extract.py`, not compiler warnings.

## 8. Declared gaps

The DoD §2 coin-path unit coverage — the `DS_0009ACBC` mask table and
`0x11F28`'s accept and reject paths — was an undeclared gap in the original
report; the fix wave (§10) now covers it directly.

* **No pixel oracle for state 2.** The select state is validated by the
  determinism log and by unit tests for `frontend_list_next` /
  `frontend_resource_known`; there is no decoded-frame against a capture.
* **The attract machine `0x11000`** (4d): the title row-1 writer `0x2C06C(1)`
  and the rest of the attract sub-machine.
* **States 3-9**: the state-3 stub is reached (state 2's phase 3 sets
  `DSW(DS_000F0A64) = 3`); match-start and the fight states carry
  `/* PORT: */` markers.
* **`0x10DB0` / `0x10E18`** (the `0x11D04` tail's input-gated branches): deferred
  to 4b; with no input their bits stay zero.
* **The voice system** and **the fight engine** (sub-project 5).
* **`0x50146`** repeat-timer setter: inert, no port caller.
* The credit divert handlers `0x32970` / `0x257A4`: unported; the early return
  is faithfully taken but the handlers' effects are not.

## 9. Commit

```
flow: wire the coin poll and the input update; record the outcome
```

## 10. Fix wave (post-review)

Commit `flow: cover the coin path and align game_frame order with the raw`
applies the whole-branch review findings:

* **F1 — coin-path unit tests (DoD §2).** `test_frontend.c` now asserts the
  shipped mask table (`DSD(DS_0009ACBC) == 0x01000000`,
  `DSD(DS_0009ACBC + 4) == 0x00000100`) and drives `game_state_step()` through
  the reject path (no newly-pressed bit: credit held at 5, state 9's countdown
  `DS_000F0A6A` steps 2 -> 1) and both accept paths (`DS_001088E4` set to each
  mask: credit debited 5 -> 4 -> 3, countdown held at 2 because an accepted coin
  returns from `0x11D04` before the dispatch). The block runs before the
  `PR_FRONTEND_DUMP` guard; it maps `PRAGE.EXE` itself when `test_le()` has not,
  so the standalone `PR_FRONTEND_DUMP` invocation covers it too.
* **F2 — `game_frame` call order.** `input_state_update()` is now the first
  statement of `game_frame()` (`0x24C6E`), matching the raw where `0x4F644`
  precedes the frame counter and the process tables, not in the middle.
* **F3 — comment accuracy.** The `0x33904` iterator comment says `+4 dword`
  (the code and raw test a dword), in both `flow.c` and `flow.h`.
* **F4 — ephemeral task names.** Removed the "Task 2" and "Deferred to 4b"
  references from the shipping `flow.c` comments.
* **F5 — address tags.** The `0x9AEE0` / `0x9AEE4` / `0x9AEC8` raw literals in
  `game_state_select` now carry the address-tag comments the derivations doc
  claims.
* **F6 — iterator test hardened.** The `0x33904` test seeds a live dword at
  `tbl+4` as well as `tbl+0x14` and still requires `tbl+0x10`, proving the
  helper advances by `0x10` before its first test.
