# Track D, wave 5, batch d5g — derivation record

Differential rows for the queue `.superpowers/sdd/2026-10-11-reverse-d1/d5g-queue.txt`: seven functions of the
menus, the config validation and the name entry (`game/flow.c`, `config.c`, `menu.c`, `nameentry.c`). Branch
`reverse-d5g` off `reverse-d4int` (`3790f323`). Ground truth: the fixed-up image `build/diffrun --image-out`
dumps (linear addresses), disassembled with capstone 5.0.7. Every figure in §2 is measured with `python3
tools/diff_verify.py --image … --function <name> --self-check`; the pins in `tools/tests/test_diff_verify.py`
(`D5G_MASKS`/`D5G_KINDS`/`D5G_CASES`) come from `verify_all` on the batch's own specs. The claim of each row is
the track's: equivalence on the exercised blocks and inputs only, with the callees stubbed or run as stated.

Result: 5 of the 7 entries are rows (6 specs, 204 cases, 268 mutants), 2 are not rows (§4, §6). **Four
address-proved port corrections** (§3), all of one class: the port tested a callee's whole return where the raw
tests AL.

## §1 The members

| addr | port function | size | callers and what they read of EAX |
|---|---|---|---|
| `0x1E824` | `hiscore_init` (flow.c) | 242 | One caller (a rel32 scan), `0x20C84`, which continues `mov eax,[0x104528]` (`0x20C89`). No dword of the image holds `0x1E824`. Mask 0. |
| `0x2FFC4` | `menu_step` (menu.c) | 1577 | Two callers: `0x251EE`, returning to `test eax,eax; je 0x25210; cmp eax,-5` (`0x251F3..0x251F7`), and `0x2CB88` in `0x2CB74`, which returns it (`pop edx; pop ecx; pop ebx; ret` `0x2CB8D`). Every return path loads the whole register (`mov eax,0xfffffff6` `0x303F4`, `mov eax,0xffffffff` `0x30440`, `xor eax,esi` `0x30460`, `mov eax,0xfffffffb` `0x30466`, the callback's EAX at `0x30418`/`0x304AD`/`0x304DA`, `xor eax,eax` `0x305E7`). Mask `0xFFFFFFFF`. |
| `0x2FA40` | `menu_run` (menu.c) | 1024 | One caller, `0x2CBA8` in `0x2CB94`: `mov edx,eax` (`0x2CBAD`) … `mov eax,edx` (`0x2CBB9`), returned to the menu's own callback site (`test eax,eax` `0x2FD3C`, `0x30416`). The two returns are the callback's EAX (`0x2FD3E`) and `sete al; and eax,0xff; dec eax` (`0x2FD57..0x2FD5F`). Mask `0xFFFFFFFF`. |
| `0x1EEB0` | `game_mode_1e_step` (flow.c) | 1448 | One caller, `0x253CB` (in `0x24C5C`, case 0x1E), followed by `jmp 0x2540f`: the mode tail the d1c §1 and d1d §1 records trace dead and d2e §1 reuses for `0x41C28` (`0x253BD`, the same `jmp`). Mask 0. |
| `0x1F458` | `nameentry_step` (nameentry.c) | 2897 | Eight call sites, all in `0x1EEB0`: six return to `test eax,eax` (`0x1EF4B`, `0x1EFDD`, `0x1F03E`, `0x1F0C8`, `0x1F208`, `0x1F32B`), two to the epilogue (`0x1EFCD`, `0x1F0BB`). The function returns `xor eax,eax` (`0x1F4CB`) or `mov eax,1` (`0x1FE95`, `0x1FF9B`). Mask `0xFFFFFFFF`. |

## §2 The rows

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `hiscore_init` | `0x1E824` | 11 | 18/18 | 29/29 | 0 | No argument. The two 0x24-byte buffers `0x104367 + 0xA0k` and the one at `0x1042C7` are blanked (`0x20`) before any call (`@late`: the memory at every call). Then the byte `0x104529`: bit `0x40` (`0x1E875`), or bit `0x20` (`0x1E87A`) and `0x1E988`'s AL == 1 (`and eax,0xff; cmp eax,1` `0x1E884/0x1E889`: `r1` has stub EAX `0x101`, `r2` `0xFFFFFF01`, `n3` `0x100`, `n4` 2), takes the reset arm: `0x2DA0C(0x27, 0)`, `0x2DA0C(0x26, 0)`, `0x1E918(1)`, the byte read again (`0x1E8B0`); bit `0x40` clear returns (`r0..r2`), set clears it in the byte alone (`mov [0x104529],bl` `0x1E8C0`: `@clrw`), then `0x2DA0C(0x29, the dword 0x104528)`, `0x2DBC4(0, 1)` and `0x2DCA0(0, 0xA7D74, 1)` whatever the record holds. Else `0x1E918(0)`, `0x2DBC4(0, 1)` and the insert only when the record's first dword is 0 (whole: `n1` has `0x01000000`). The five callees are stubs. |
| `hiscore_init_rr` | `0x1E824` | 2 | 18/18 | 3/3 | 0 | The re-read: the `0x1E918` stub rewrites the byte `0x104529` to `0x5F`. `q0` enters through bit `0x20` and AL = 1, and the byte read at `0x1E8B0` has bit `0x40`: it is cleared to `0x1F` and the arm goes on (`@cache`, `@cachetest`, `@cachestore`). |
| `menu_step` | `0x2FFC4` | 41 | 65/67 | 69/69 | `0xFFFFFFFF` | EAX = the table, EDX = the stride, ECX = the flags (`mov [0x107418],ecx` `0x30089`; EBX is not read: `i4`). The state is `0x107414..0x10744F`, seeded whole. First call (the byte `0x107414` = 0 between non-zero bytes): the key time, `0x2EA74`, `0x2C3FC(0x100)`, cur 0, redraw 1, old -2, row0 5 plus the digit after a `0x0B` prefix of the FIRST ITEM's string (`mov eax,[0x10741c]; mov eax,[eax]` `0x30012/0x30022`; signed: `i5` has `0xB1`), the title 0 when nothing follows the prefixes (`i2`, `i3`, `i4`), `0x2FE84(title, 0x5000, 0x1000, flags)` and the active byte 1 after it. Redraw (the dword `0x10743C`, whole: `r4`): the callback(0), the walk while old != cur ('?' skipped but counted, `0x0A` a blank row, the `+0xC` row offset, the three highlighting arms with the second string 5 columns past the first), the terminator's old = cur. The poll `0x2EEC8(0xC300C000, 1)` is a stub (EAX = the keys) and `0x2EB80` runs (the latched key): Esc is -10 under flag bit 2 (the byte: `k3` has bit 10), else -5; the callback(sel) is returned when non-zero (whole: `c1`); the Esc bit; the Enter bit (`0x2FE84(sel, 0x5000, 0x1000, 0)`, then the item's callback with the selection read again); up `0x80008000` and down `0x40004000` through `0x2FE40` (run), wrapping once; the code row `0x305FC(0x11, 2)` by flag bit 0. The callback is `0x2CB74` (`svc_start_menu`), stubbed through its seam. |
| `menu_run` | `0x2FA40` | 26 | 53/56 | 46/46 | `0xFFFFFFFF` | The blocking twin: the same walk with its state in the frame (row0 keeps the `0x0B` digit: `q1`, `q5`). The loop leaves only on the callback's non-zero result (`0x2FD3E`) or the Esc bit without flag bit 2, so the stubs script the keys: `0x2EA74` writes the latched key dword `0x105F30` = `0x1B`, `0x2C3FC` (called once, after the first wait) writes 0, and `0x2EDE0` runs on both sides (`0x50161` reports each masked bit of the case's pad dword `0xE1C34` once; `0x2EBF0` turns the latched Esc into bit 25). The first poll sees the pad word; the poll after the loop's `0x2EA74` sees Esc. That reaches the up and down moves, the old item's release on the redraw they cause, Enter with its full redraw, and the code row before each wait. `x1` is the empty menu (cur == count: 0). |
| `game_mode_1e_step` | `0x1EEB0` | 64 | 50/50 | 57/57 | 0 | No argument. The state byte `0x104B25` above `0x10` returns; else the 17-entry table `0x1EE6C` (entry 1 is the shared `ret`; `switch_cases` follows it: `cmp al,0x10; ja; and eax,0xff; jmp cs:[eax*4+0x1ee6c]`). Nine stubs: `0x1ECC8` (AL: `test al,al` `0x1EEE6`), `0x1ED2C`, `0x2C3FC`, `0x1EC38` (AL: `0x1F1B0`, `0x1F2CE`; dropped in states 0, 0xF, 0x10), `0x1F458` (whole: six `test eax,eax`), `0x4F1E4`, `0x2BAF4(1)`, `0x1EA08`, `0x4F980(0x11)`. The state byte's store is placed against the calls as the raw has it (seven mutants are caught by the memory at a call alone): after `0x2C3FC` in states 0xF/0x10 (`0x1EFAE`, `0x1F09E`), after `0x2BAF4` in states 3 and 0xA (`0x1F186`, `0x1F3F1`), before `0x1EC38` in state 0 (`0x1EF12`, `0x1EF27`), before the voices in states 4 and 7 (`0x1F1DB`, `0x1F301`). State 0's score compare is unsigned (`jb` `0x1EF10`: `a6`), state 0xA's slot byte is indexed by the dword `0x104AD4` (0, 1; 2 is tested first). |
| `nameentry_step` | `0x1F458` | 60 | 102/102 | 64/64 | `0xFFFFFFFF` | AL = the side (`mov [esp+4],al` `0x1F460`: `f2` has EAX = `0x100`, `r3` `0xFFFFFF00`, `h1` the side 2). `0x1FFD0`; the timer word `0x10438C` stepped and drawn as its thirtieth; at 0 with the dword `0x1044AC` 0 (whole: `f2`), `0x20710(the side)` and EAX = 0. With the byte `0x1044D8` 0, one cursor move (right, left, up, down in that order, each by the side's pad bit or its autorepeat counter above `0x1E` and a multiple of 5) with its two voices and the cursor record's `+0x18`/`+0x1C`; the 28-cell grid; the score and rank lines; the prompt every 32nd frame (the frame word's low byte); the rank's two strings or, with option bit 1, the two banner actors once (the handle stored between the two `0x2AE14` calls: `@a8store` is the memory at the second). Then a letter from a face button of the side (7 * ((row - 6) / 3) + (col - 0xB) / 3) or the queue `0x104458` (the entry after the read index, masked `0xF`; signed `idiv` for the cursor: `q4` is -7): DEL (the count signed: `t5`), END, or a letter under the limit (signed bytes: `t8` has count `0x80`). `0x1C500` runs on both sides; nine stubs. |

Callee stubs and their clobber sets (`E.callee_clobbers` on the image, re-derived by `pins_gen`'s check for
every stub of the six specs): `0x1E988` `()`, `0x2DA0C` `(edx)`, `0x1E918` `()`, `0x2DBC4` `(edx)` (the existing
`D3D_2DBC4`), `0x2DCA0` `(ebx, edx)`; `0x2EA74` `()` by a fix (below), `0x2C3FC` `()`, `0x2FE84` `(ebx, ecx,
edx)`, `0x2F280` / `0x2F198` `(ebx, ecx, edx)` (existing), `0x2EEC8` `(edx, ebp)`, `0x305FC` `(edx)`, `0x2CB74`
`()`; `0x1ECC8`, `0x1ED2C`, `0x1EC38`, `0x4F1E4`, `0x2BAF4`, `0x1EA08`, `0x4F980` all `()`, `0x1F458` `(ebp)`;
`0x1FFD0` `()`, `0x20710` `(esi, edi, ebp)`, `0x2F434` / `0x2F4BC` / `0x2F4D0` / `0x2F510` / `0x2AE14` `(ebx,
ecx, edx)` and `0x2BCF4` `(edx)` (existing constants).

**`_CALLEE_CLOBBER_FIXES[0x2EA74] = ()`** (`tools/diff_emu.py`). `0x2EA74` is `xor eax,eax; mov eax,eax` with no
`ret`: it falls into `0x2EA78`, whose leading `push ebx; push ecx; push edx; push esi` (`0x2EA78..0x2EA7B`)
the scan cannot credit to an entry two instructions above it, so it reports every register. Its raw callers
keep all six live across it: `0x2FFC4` loads EBX before the call at `0x2FFF1` (`mov ebx,0xfffffffe`
`0x2FFE7`) and stores it after (`0x30017`), and reads EDI (`mov eax,[edi+8]` `0x30086`), ESI (`add edi,esi`
`0x30094`) and EBP (`mov eax,ebp` `0x300BB`) after it; `0x2FA40` loads EDX, EBX and ECX before the call at
`0x2FA61` (`0x2FA4E..0x2FA58`) and stores them after (`0x2FA72`, `0x2FA76`, `0x2FA7D`).

The callback stub. A menu callback is a code address in the table; the raw calls it indirectly (`call
[0x10744c]` `0x300E1`/`0x3040B`, `call [edx+8]` `0x304A2`; `call [esp+0x14]` `0x2FB11`/`0x2FD38`, `call
[edx+8]` `0x2FD9E`) and reads EDX (the keys) after `0x3040B` and `0x2FD38`. The rows use `0x2CB74`
(`svc_start_menu`, registered by `svcmenu_register`): it pushes EBX/ECX/EDX and its one callee `0x2FFC4` saves
ESI/EDI/EBP, so its clobber set is `()`. Its seam makes the port's `fn_resolve` path return the stub's EAX.

## §3 Corrections

All four are the wave's class 1: the raw tests AL, the port tested the whole return. None changes what the
port does today — its own `hiscore_audit_reset_due`, `hiscore_rank_pair` and `hiscore_rank_single` return
exactly 0 or 1 — but the raw's `0x1ECC8` and `0x1EC38` do return AL = 0 under non-zero upper bytes (`xor
al,al` at `0x1ECE5` after `and eax,0xffff` leaves the rank's high byte; `xor al,al` at `0x1EC9F` after a call's
EAX), so a port of those callees that kept the raw's register would have gone wrong here.

| # | function | address | old | new | the proving instructions |
|---|---|---|---|---|---|
| 1 | `hiscore_init` | `0x1E884` | `hiscore_audit_reset_due() != 1u` | `(hiscore_audit_reset_due() & 0xFFu) != 1u` | `0x1E87F call 0x1e988; 0x1E884 and eax,0xff; 0x1E889 cmp eax,1; 0x1E88C jne 0x1e8e9`. Row `hiscore_init` `r1`/`r2` (stub EAX `0x101`, `0xFFFFFF01`); mutant `@eax`. |
| 2 | `game_mode_1e_step` state 0 | `0x1EEE6` | `hiscore_rank_pair() != 0u` | `(hiscore_rank_pair() & 0xFFu) != 0u` | `0x1EEE1 call 0x1ecc8; 0x1EEE6 test al,al; 0x1EEE8 je 0x1ef37`. Case `a4` (stub EAX `0x100`); mutant `@s0eax`. |
| 3 | `game_mode_1e_step` state 4 | `0x1F1B0` | `hiscore_rank_single(DSD(0x1077EC)) != 0u` | `(… & 0xFFu) != 0u` | `0x1F1AB call 0x1ec38; 0x1F1B0 test al,al; 0x1F1B2 je 0x1f2aa`. Case `d3`; mutant `@s4eax`. |
| 4 | `game_mode_1e_step` state 7 | `0x1F2CE` | `hiscore_rank_single(DSD(0x107880)) != 0u` | `(… & 0xFFu) != 0u` | `0x1F2C9 call 0x1ec38; 0x1F2CE test al,al; 0x1F2D0 je 0x1f3cc`. Case `j3`; mutant `@s7eax`. |

The first run of `hiscore_init` and of `game_mode_1e_step` against the uncorrected port was a MISMATCH on
exactly those cases (`r1`, `r2`; `a4`, `d3`, `j3`) and on nothing else.

Seen, not changed:

- **`config_validate`'s defaults arm leaves out two fills.** `0x2D83A..0x2D875`: `0x61A70(0x105DE1, 0, 0x53)`
  and `0x61A70(0x105DB0, 0, 0x26)` — the stored-image buffer (which holds the byte `0x105E2F` and the magic
  `0x105E30`) and the config-field block — before `0x2CADC`. The port's comment calls them "the unported pset
  pool (no-op)". Its one caller is `game_init`, on the image's BSS (both ranges are zero in the image), so
  nothing differs today. Not a row (§4), so no mutant can pin a fix; left for the batch that rows `0x2D6F8`.
- **`menu_step`/`menu_run`: `sel` and `[sel+8]` are followed unchecked on Enter** in the raw (`call [edx+8]`
  `0x304A2`, `0x2FD9E`); the port's `menu_call` skips a zero or unregistered address. Enter with no selected
  item is not cased (the raw would call address 0).

## §4 Named gaps and unhit blocks

Unhit blocks, named in the specs:

- `menu_step` `0x3053E`, `0x305A5` and `menu_run` `0x2FDCB`, `0x2FE00`: the wrap's second pass (every item
  hidden) calls `0x2EA68` (`mov eax,1; jmp 0x62003`, the runtime's exit), which the port leaves out under
  `PORT:` and returns 0.
- `menu_run` `0x2FD65`: Esc under flag bit 2 becomes down (`or edx,0x40004000`), and the loop then leaves
  only on a menu-callback result that is 0 on one poll and non-zero on a later one. A stub's EAX is one value
  per case, so no terminating case reaches it. The flag's other use (the title draw's flags) is cased (`q3`).

What the rows do not cover:

- **`menu_run` makes one move per case.** The pad bits are edge-reported once and the second poll is the
  scripted Esc, so a down move can wrap only in a one-item or a hidden-tail menu (`d2`, `d5`), and two
  equivalent mutants were dropped: an initial `old` of -1 (nothing equals -1 or -2 before the first walk
  rewrites it) and a `sel` kept across a redraw (every walk that follows a move or an Enter sets it again).
- **`menu_step`'s second string pointer.** `0x1C500` returns the one buffer `0x102760`, so the store `mov
  [0x107420],eax` after the second string (`0x3023A`) writes the value the first one left: unobservable.
  The item's `count == old && count != cur` second test (`0x301B6`) cannot be false inside the walk (the walk
  runs only while old != cur).
- **`nameentry_step`'s autorepeat for up and down.** Up shares the counter `0x1044E0` with right and down
  `0x1044DC` with left, and right/left are tested first with the same threshold, so the up/down counter
  blocks run (`r6` has both counters at `0x24`: above `0x1E`, not a multiple of 5) but the fall-through of
  their `jne` (`0x1F6A7`, `0x1F754`: the autorepeat firing) cannot be reached without a pad bit. The port's comment says the counters have no writer in the image.
- **`game_mode_1e_step`: `0x104AD4` above 2 in state 0xA** indexes a third slot; not cased (0, 1 and 2 only).
- **The idle arm of `0x2EB80`** (the runtime longjmp) is not entered: every menu case has a key time `0x100`
  ticks old.

Functions that are not rows:

- **`0x2F9CC`: no port function.** It is inlined in `game_init` (flow.c: the `0x10740C` store, `config_validate()`,
  the `0x107410` store), like `0x2F920` in d4d. Its bytes: `call 0x1afe8; call 0x13adc; 0x4f1e4(0);
  0x2baf4(1); 0x4f1d0; 0x38b18(0x9ad84); mov [0x10740c],0x1d2d0; call 0x2d6f8; 0x2d974(0x2a); and al,0xfc;
  mov [0x107410],eax; 0x2c9b8(0); 0x2c8f0(-1); 0x2c8f0(-2)`. Nothing to bind.
- **`0x2D6F8` (`config_validate`): the harness cannot compare its main arm.** Measured (`/tmp/pr_d5g/cv_probe.py`,
  `E.run_original` with `0x2CADC`, `0x2D4EC`, `0x2D974` stubbed and the deferred callees run): on the image as
  it is, and with both stored copies made unreadable (a non-zero parity cell at `0x100CE4` and at `0x100CE4
  + 2 * 0x5B`), the raw takes the defaults arm and calls `0x2D4EC` nine times — `(1)`, `(2)`, `(0)` itself
  (`0x2D8AA`, `0x2D8B4`, `0x2D90B`) and `(6)`, `(7)`, `(8)`, `(3)`, `(4)`, `(5)` from inside `0x2DE98(1)` and
  `0x2DF8C(1)` (`0x2DF5C`, `0x2DFE1`: unconditional when their argument is non-zero) — where the port's
  `config_validate` calls its no-op `config_storage_touch` three times and has no function for the deferred
  `0x2DE98`/`0x2DF8C` (`tools/port_classification.txt`). Stubbed, the call lists differ (9 against 3); run,
  `0x2D4EC` rewrites the EEPROM image `0x100CE4..` (through `0x2D498`), which the port never does. Either way
  the only arm the port runs is a MISMATCH by construction, not by a defect. What could be compared — the early
  return (both reads failing and the byte `0x2D490` 0: the stores `0x105DA5`/`0x105DA4` only; the probe
  returns EAX = -1 with no call and no write on the image's zero bytes) and the magic-present arm with the RAM
  copy equal to the stored one — is about a third of its blocks, and needs the stored image encoded by hand
  (`0x2E990` is a Hamming decode over `[0x2D48C]` = `0x100CE4`, two bytes a cell). Not done. The raw's reads
  on the image's all-zero EEPROM return 1 (success), not the -1 the port's `PORT:` comment assumes; with
  `0x2D490` = 4 (the image's value) both take the same path.

## §5 Store-sweep findings

- `hiscore_init`: the three buffers are seeded with bytes that are not `0x20`, each between two sentinel
  bytes: `@len` (0x23 bytes), `@stride` (the second buffer at `+0x9F` hits the first's sentinel), `@fill`,
  `@no2c7`. The dword `0x104528` is `78 <flags> 34 12` between seeded dwords: `@noclr`, `@clrw` (a word store
  clears `0x10452A`), `@mask`. `@late` (the fill after the calls) shows only in the memory at each call.
- `menu_step`: all fourteen state dwords are seeded (`0x5A0000xx`; the two entry pointers with addresses of
  the zero BSS, so a mutant that follows one stays inside `mem[]` — the first mutant run crashed `diffrun` on
  exactly that, with `@actw` and the swapped-bit mutant following `0x5A000030`) and the active byte sits
  between `0x15 0x16 0x17`. Dropped stores: `@nostamp @nocur @flags @sel0 @cnt0 @ent @noact @rdw0 @nosel
  @term @nores @escrdw @escact @entres @entold @ent0 @res1 @nordw @nolen`; `@old`, `@row0`, `@zext`, `@cnt0`,
  `@ent`, `@actearly`, `@entlate` are caught by the memory at a call alone (the redraw of the same step
  rewrites those dwords).
- `menu_run`: no store of its own (the frame). The seeded state block `0x107410..0x10744F` is unchanged in
  every case. What `0x2EDE0` writes (the pad latch `0xE1C38`, the key time) is its row's; here it shows the
  poll's mask (`@mask`: the latch bits 14/15).
- `game_mode_1e_step`: the state byte between `0xB0 + i` bytes, the four words between seeded words (`@d8eew`
  and `@rb00w` are dword stores over the next word), the dword `0x104AE4` between seeded dwords. Every store
  has a dropping or misplacing mutant; the gate bytes `0x107813`/`0x1078A7` have their neighbours seeded
  (`@s4gatew`).
- `nameentry_step`: the timer word between seeded words (`@tw`: a byte test with the word `0x0100`), the
  cursor words after the seeded word `0x1044CE` (the raw reads the dword `0x1044CE` and shifts: a wrong
  offset reads `0xCECF`), six seeded letter cells (`@delcell`, `@cellstate`, `@cellletter`), the name buffer
  (`@endz`), the cursor record's `+0x10..+0x23` (`@rx`, `@uy`, `@fullxy`). `t8` (count `0x80`, signed below
  the limit) writes the cell at `0x104114 - 0x14 * 0x80` = `0x103714`, inside the image on both sides.

## §6 The queue

| entry | port function | result |
|---|---|---|
| `0x1E824` (L4, 242) | `hiscore_init` | rows `hiscore_init`, `hiscore_init_rr`; correction 1 |
| `0x2F9CC` (L5, 116) | none (inline in `game_init`) | not a row: no port function (§4) |
| `0x2D6F8` (L4, 633) | `config_validate` | not a row: the defaults arm cannot be compared (§4) |
| `0x2FA40` (L4, 1024) | `menu_run` | row `menu_run` |
| `0x2FFC4` (L4, 1577) | `menu_step` | row `menu_step` |
| `0x1EEB0` (L5, 1448) | `game_mode_1e_step` | row `game_mode_1e_step`; corrections 2-4 |
| `0x1F458` (L4, 2897) | `nameentry_step` | row `nameentry_step` |

## §7 Counters and gates (measured on the branch head)

- The one full self-check, `PR_DIFF_JOBS=4 python3 tools/diff_verify.py --image /tmp/pr_d5g_img.bin
  --self-check`, on the tree of the last commit (the files compared byte for byte with the measured ones):
  `diff-verify: 790/790 functions VERIFIED; 9488/9488 mutants detected; 1 named gaps; 499/623 rows with callees
  closed (167 have none)`, `rc 0`. The batch adds 6 specs and 268 mutants; its six rows are open (every one
  stubs a callee that has no row, or a named non-row). The branch point's own counter was not measured here.
- By script on the same tree: every stubbed address declares one clobber set across all 791 specs, each of the
  batch's equals `E.callee_clobbers`, and the expected-clobber dictionary equals the stubs of `SPECS` (370).
- Seams added (`port/src`, the first statement of each): `0x1E988` `PR_SEAM_RET0` (`hiscore_audit_reset_due`),
  `0x1E918` `PR_SEAM(…, force)` (`hiscore_fill_defaults`), `0x1ECC8` `PR_SEAM_RET0` (`hiscore_rank_pair`),
  `0x1EC38` `PR_SEAM_RET(…, score)` (`hiscore_rank_single`), `0x1EA08` `PR_SEAM0` (`frontend_match_start`),
  `0x4F1E4` `PR_SEAM0` (`frontend_input_reset`), `0x4F980` `PR_SEAM(…, ret_mode)` (`frontend_wipe_arm`), all
  flow.c; `0x2DA0C` `PR_SEAM_RET(…, field, value)` (`config_field_set`), `0x2DCA0` `PR_SEAM_RET(…, rec, src,
  table)` (`hiscore_insert`), `0x2EA74` `PR_SEAM0` (`config_screen_wait_zero`), `0x2EEC8` `PR_SEAM_RET(…, mask,
  (u32)flag)` (`config_input_poll_clear`), `0x305FC` `PR_SEAM(…, (u32)col, (u32)row)` (`config_code_row`), all
  config.c; `0x2FE84` `PR_SEAM(…, entry, mode_a, mode_b, flags)` (`menu_title_draw`, menu.c); `0x2CB74`
  `PR_SEAM_RET(…, entry)` (`svc_start_menu`, svcmenu.c); `0x2BAF4` `PR_SEAM(…, al)` (`actors_reset_al`,
  actors.c); `0x1ED2C` `PR_SEAM0` (`nameentry_reset`), `0x1F458` `PR_SEAM_RET(…, (u32)side)`
  (`nameentry_step`), `0x1FFD0` `PR_SEAM0` (`nameentry_cells_step`), `0x20710` `PR_SEAM(…, (u32)side)`
  (`nameentry_finish`), all nameentry.c. Nineteen.
- Outside the batch's blocks: one comment-delimited group of lines at the top of the expected-clobber
  dictionary of `test_each_stub_declares_the_registers_its_callee_clobbers` (the nineteen new stub addresses)
  and the `_CALLEE_CLOBBER_FIXES[0x2EA74]` entry in `tools/diff_emu.py`.
- Stability of the port side: `hiscore_init`, `hiscore_init_rr`, `menu_step` and `game_mode_1e_step` — every
  binding ten times in fresh processes, byte-identical output. `menu_run` and `nameentry_step` — four runs
  of every binding (one sequential pass, two pooled passes with identical verdicts, kinds and case sets, and
  the full self-check), not ten: the port side scans `mem[]` at every recorded call and ten passes of these
  two rows are hours. Every index-like value in their cases is inside the image by construction (§5).
- Not run by this batch: `make verify`, `make diff-verify`, any `*-oracle` target, the Python unittest. The
  four corrections are therefore unproven against the oracles.
