# Track D, wave 5, batch d5c — derivation record

Differential rows for eleven of the fourteen queue entries of `d5c` (`fight.c`, `flow.c`). Branch
`reverse-d5c` off `3790f323` (`reverse-d4int`, waves 1 to 4 merged). Ground truth: the fixed-up image
`build/diffrun --image-out /tmp/pr_d5c_img.bin` dumps (linear addresses), disassembled with capstone.
Every figure in §2 is measured with `python3 tools/diff_verify.py --image /tmp/pr_d5c_img.bin --function
<row> --self-check`; the pins (`D5C_MASKS`/`D5C_KINDS`/`D5C_CASES` in `tools/tests/test_diff_verify.py`)
come from `verify_all` on the row's own spec and were re-measured whole after the last row (14 rows, 452
mutants, no difference). Each row and each of its mutant bindings was run ten times in a row through
`build/diffrun` (identical output, exit 0 every time). The claim of each row is the track's: equivalence
on the exercised blocks and inputs only, with the callees stubbed or run as stated. Every correction of
§3 is unproven against the capture oracles until the integrator's gate.

Each mutant set is a restatement of the function in `port/tests/diff_runner.c` (`d5c_<addr>_core`) with
one choice flipped per mutant number. The restatements were not bound unflipped and measured on their own:
what shows a restatement is the function is that each of its mutants differs from the original on the
cases §2 pins and on no others.

The full self-check on the batch's last row commit (`PR_DIFF_JOBS=4 python3 tools/diff_verify.py --image
/tmp/pr_d5c_img.bin --self-check`) ends `rc 0` with `798/798 functions VERIFIED; 9672/9672 mutants detected;
1 named gaps; 508/631 rows with callees closed (167 have none)`.

## §1 The members

The mask is the part of EAX a caller reads. Every row is mask 0 ("void"), with the instructions that write
EAX before any read after each call site.

| addr | port function | size | callers and what they read of EAX |
|---|---|---|---|
| `0x43818` | `fight_char_screen_setup` (fight.c) | 154 | Two sites: `0x43778` is followed by `0x4377D xor ecx,ecx` / `0x4377F lea eax,[edx+1]`; `0x44508` by `0x4450D mov cl,1` / `0x4450F mov eax,edx`. |
| `0x43738` | `fight_char_screen_open` (fight.c) | 222 | One call site, `0x4372E`, which returns through `0x43733..0x43736` into `0x4367C`, a function no instruction calls: the `DS_00104AE4` frame hook. The function is itself a hook value (`0x28D6A`, `0x28D87`). The walk from the return site of each of the six `call [0x104ae4]` (`0x4F302`, `0x4F373`, `0x4F6F1`, `0x4F70D`, `0x4F9AA`, `0x4F9D1`) is record 2026-10-11-reverse-d2b §1 (the `0x43A08` row): dead on every path. Re-used, not re-walked. |
| `0x444C8` | `fight_char_screen_open_both` (fight.c) | 132 | One site, `0x4462B`, returning through `0x44630..0x44636` and `0x4368D..0x43690` into the hook's callers (the same walk). |
| `0x4454C` | `fight_4454c` (fight.c, *static*) | 235 | One site, `0x43688`, returning through `0x4368D..0x43690` into the hook's callers (the same walk). |
| `0x43B24` | `fight_char_select_pass` (fight.c) | 486 | One site, `0x438F3`, returning through `0x438F8 pop edx; ret` to `0x2538A jmp 0x2540f`, whose `0x2A31C` writes EAX before reading it (the note of the existing `0x25210` row in `tools/diff_verify.py`; re-used, not re-walked). |
| `0x438B4` | `fight_mode_10_step` (fight.c) | 116 | One site, `0x25385`, then `0x2538A jmp 0x2540f` (as above). |
| `0x34B6C` | `fight_health_sync` (fight.c; *static* before this batch) | 541 | One site, `0x357FC`, followed by `0x35801 mov eax,esi`. |
| `0x35658` | `fight_hud_pass` (fight.c) | 478 | 28 call sites: 26 overwrite EAX and two (`0x277E4`, `0x2965A`) call `0x4DEF4`, which reads EAX only after loading AX (record 2026-10-11-reverse-d1a §1, the `0x186C4` row; re-used, not re-walked). |
| `0x29638` | `game_mode_33_step` (flow.c) | 128 | One site, `0x251BC`, then `0x251C1 jmp 0x2540f`. |
| `0x277C0` | `game_mode_0f_step` (flow.c) | 239 | One site, `0x2537B`, then `0x25380 jmp 0x2540f`. |
| `0x42CB4` | `flow_challenge_poll` (flow.c) | 681 | One site, `0x425DE`, followed by `0x425E3 jmp 0x425ea` and `0x425EA call 0x33f08`, whose first access to EAX is the load `0x33F15 mov eax,[ebx+0x1077a8]` (`0x33F08..0x33F13` push and load EDI and EBX). |
| `0x424E8`, `0x274FC`, `0x296B8` | (flow.c) | 570, 708, 696 | not rowed (§6). |

## §2 The row table (measured)

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `fight_char_screen_setup` | 0x43818 | 2 | 1/1 | 30/30 | 0 | 0x2BAF4, 0x2AE14, 0x33754 stubs; 0x4F228 runs on both sides; the bare `ret` 0x29D60 runs on the original side (the port has no call). |
| `fight_char_screen_open` | 0x43738 | 7 | 9/9 | 33/33 | 0 | 0x2C3FC, 0x13DF0, 0x1D810, 0x43818, 0x43964, 0x43A08, 0x2F528 stubs; 0x2C8F0 runs on both sides under its own rows' call set (0x2D974 recorded, the AIL wrappers stubs). |
| `fight_char_screen_open_both` | 0x444C8 | 2 | 3/3 | 26/26 | 0 | the same call set without 0x2F528. |
| `fight_4454c` | 0x4454C | 7 | 11/11 | 34/34 | 0 | 0x2C3FC, 0x444C8 stubs. Bound through `fight_hook_4367c` with the byte 0x104B1D = 3 (§4). |
| `fight_char_select_pass` | 0x43B24 | 17 | 43/44 | 55/55 | 0 | 0x11F28, 0x43964, 0x43A08, 0x432A0, 0x435AC, 0x43464, 0x43EA0, 0x43FBC, 0x43D60, 0x43AAC stubs; 0x4F980 runs on both sides. One dead block named (§4). |
| `fight_mode_10_step` | 0x438B4 | 12 | 15/15 | 16/16 | 0 | 0x44798, 0x43B24, 0x43928, 0x4F790 stubs. |
| `fight_health_sync` | 0x34B6C | 23 | 23/43 | 42/42 | 0 | the position branch, the states 0..8 and above 0x15; 13 stubs. 20 blocks named: the `_t9` row's. |
| `fight_health_sync_t9` | 0x34B6C | 25 | 26/43 | 36/36 | 0 | the states 9..21; 10 stubs. 17 blocks named: the row above's. The two rows hit all 43 blocks between them. |
| `fight_health_sync_rr` | 0x34B6C | 3 | 14/43 | 2/2 | 0 | the gate stub stores another record in the slot (a harness write). 29 blocks named. |
| `fight_hud_pass` | 0x35658 | 9 | 22/22 | 55/55 | 0 | 0x3C59C, 0x33C78, 0x38D24, 0x34B6C, 0x3531C, 0x2A1FC, 0x354F0, 0x186C4 stubs; 0x34038 recorded and run (`mode="real"`, §4). |
| `fight_hud_pass_rr` | 0x35658 | 2 | 12/22 | 1/1 | 0 | the 0x34B6C stub stores another record in the pointed slot. 10 blocks named. |
| `game_mode_33_step` | 0x29638 | 5 | 3/3 | 23/23 | 0 | 0x35658, 0x4DEF4, 0x12DA8, 0x2C3FC stubs. |
| `game_mode_0f_step` | 0x277C0 | 6 | 3/3 | 39/39 | 0 | the same and 0x41310, 0x32A3C stubs; 0x32B94 and 0x32970 run on the original side only (§4). |
| `flow_challenge_poll` | 0x42CB4 | 17 | 46/46 | 60/60 | 0 | 0x42F60, 0x42FE0, 0x4F778, 0x2BAF4, 0x2C3FC stubs; 0x4F1E4 and its 0x2EA30 run on both sides. |

14 rows on 11 entries; 452 mutants.

## §3 Corrections (each address-proved, each in its own commit)

| # | function | raw | old | new |
|---|---|---|---|---|
| 1 | `fight_health_sync` (0x34B6C) | `0x34B8E call 0x36e2c` / `0x34B93 test al,al` / `0x34B95 je 0x34bf4` | the whole C int of `fight_position_gate` tested | `& 0xFF`. Measured with a stub EAX of 0x100: `p8: call #1: original 0x349C8(0x0), port 0x36F10(0x1077B0)`. Mutant `fight_health_sync@gateeax` (p8 alone). |
| 2 | `fight_health_sync` (0x34B6C) | `0x34D0C call 0x3bddc` / `0x34D11 test al,al` / `0x34D13 je 0x34d83` | the whole C int of `fighter_attack_consume` tested | `& 0xFF`. Measured: `k6: call #2: original none, port 0x18B04(0x0)`. Mutant `fight_health_sync_t9@consumeeax` (k6 alone). |
| 3 | `fight_health_sync` (0x34B6C) | `0x34B82 mov esi,[ecx]` before the gate; ESI at `0x34B97` (the +0x28 flag), `0x34BD4 mov edx,esi` (0x36638) and the `mov edx,esi` of every table callee (`0x34C19`, `0x34C26`, `0x34C37`, `0x34C53`, `0x34C66`, `0x34C9A`, `0x34CA9`, `0x34CB8`, `0x34D69`, `0x34D7C`); the slot read again only at `0x34BA7 mov edx,[ecx]` / `0x34BB4 mov eax,[ecx]` (the +0x18 dword) | the slot's record read after the gate for all of them | read once before the gate; `DSD(DSD(rec) + 0x18)` for the x alone. Measured on the `_rr` row: `y0: call #1: original 0x36638(0x1077B0, 0x10A300), port 0x36F10(0x1077B0)`; `y1: call #1: original 0x36F10(0x107844), port 0x36638(0x107844, 0x10A500)`; `y2: call #1: original 0x359E0(0x1077B0, 0x10A300, 0x0), port 0x359E0(0x1077B0, 0x10A500, 0x0)`. Mutants `fight_health_sync_rr@post` (the old port) and `@pre`. |
| 4 | `fight_hud_pass` (0x35658) | `0x356BE call 0x3c59c` / `0x356C3 test al,al` / `0x356C5 jne 0x35722` | the whole C int of `fighter_pass_flag` tested | `& 0xFF`. Measured: `a2: byte 0x107838: original 0x11, port unchanged`. Mutant `fight_hud_pass@flageax` (a2 alone). |

All four are fidelity to the raw with no change on the states the port's own callees produce: the three
predicates return 0 or 1 in the port, and 0x36E2C stores nothing (it is a predicate over the slot). None
changes what the shipped game does.

The first form of correction 3's commit was built from zero-context hunks and placed one line wrongly
(`git apply --unidiff-zero` takes a pure insertion's new line number); it was amended before any later
commit, and the amended commit `a235fdce` is HEAD's file plus that one edit.

## §4 Named gaps and unhit blocks, and what the rows do not cover

- **`fight_health_sync`, two rows.** `build/diffrun` takes 16 stubs a case (`stub_t stub[16]`,
  `port/tests/diff_runner.c`) and the function has 21 callees, so the table is split at state 9. Each row
  names the other's blocks; no block of the function is unhit by both. A mutant of one row can only reach
  the callees of that row's call set (`@idx1` maps 8 to 9 in the first row and 21 to 0 in the second, whose
  set includes 0x349C8 for that).
- **`fight_health_sync`, the jump table.** `0x34C00 jmp cs:[eax*4 + 0x34b14]`: the 22 entries read from the
  image are 0x34C08, 0x34C15, 0x34C26, 0x34C46, 0x34C53, 0x34C62, 0x34C73, 0x34C80, 0x34C8D, 0x34D83 (x3),
  0x34C9A, 0x34CA9, 0x34D83 (x3), 0x34CB8, 0x34CC7, 0x34D22, 0x34D69, 0x34D78; every one has a case.
- **`fight_health_sync`, 0x35D7C.** The raw loads EAX = the slot (`0x34C46 mov eax,ecx`) but 0x35D7C reads
  EDX alone (`0x35D85 sub eax,edx`, `0x35D87 mov [esp],edx`), which still holds the side from `0x34B71`.
  The `E.Call` names `edx`; the port passes the side.
- **`fight_health_sync`, a null slot pointer.** The mutant set has no "skips the `test ecx,ecx`" member: a
  port that skipped it would read `mem[0]`, which is zero, and return at the record test anyway.
- **`fight_4454c`.** The port function is `static`; the binding calls its one caller `fight_hook_4367c`
  and every case pokes the byte 0x104B1D = 3 (`0x4367F cmp byte [0x104b1d],3`), which the original's
  0x4454C does not read. The row therefore also depends on the hook's first test.
- **`fight_char_select_pass`**: block `0x43C2D` (`jmp 0x43c9f` after `0x43C27 cmp ax,0x10; je`) is dead: a
  non-zero value & 0xF0 below 0x20 is 0x10.
- **`fight_char_screen_setup`, the two handles.** One stub EAX serves the three spawns of a case, so
  which spawn's result each handle holds is shown by the memory at the calls only (the first handle is
  stored before the third spawn, `0x43872`): `@storelate` is caught by `call #3 memory` alone.
- **`fight_char_screen_open`.** The word drawn was just stored (5 or 0xF), so its sign extension (`sar
  ebx,0x10` 0x437F6) is not observable. `1 << side` and `side + 1` agree for both sides, so that choice has
  no mutant.
- **`fight_hud_pass`, 0x34038.** The caller keeps the side in ESI across it (`0x357F3 mov eax,esi`) and
  `E.callee_clobbers` lists ESI for it (an over-approximation), so the row runs it on both sides and
  records it, as d3e's rows do. Every case and every mutant leaves the side's +0x8C word other than 1 when
  it is reached, so it returns at `0x34054`: the row does not exercise its body.
- **`fight_hud_pass`, the respawn.** The port calls `fighter_spawn` (0x33EB4's C) where the raw sets
  0x33C78's arguments inline (0x357AB..0x357D6); the row compares the 0x33C78 call both make. The local
  word the raw stores at `0x357DD` is on its stack and is not compared.
- **`fight_hud_pass`, a null slot pointer**: no mutant, for the reason given for `fight_health_sync`.
- **`game_mode_33_step` / `game_mode_0f_step`, the countdown's width.** A dword decrement of the word
  0x104AFE would borrow into the mode word 0x104B00 only when the word was 0, and that is the expiry arm,
  which rewrites 0x104B00: not observable, no mutant.
- **`game_mode_0f_step`, 0x32B94 and 0x32970.** The raw calls the deferred audit post 0x32B94(the byte
  0x104B1F) at `0x2786B` and the host-owned run clock 0x32970(0) at `0x27874`; the port omits both under
  `/* PORT: */` comments. They run on the original side only: 0x32B94 with bit 0 of its argument clear in
  every case (it returns without its 0x2DAE4 post), the clock from its no-elapsed-time state (d1i's
  fixture; it changes no byte). The row compares neither call nor its argument, and no case has bit 0 of
  0x104B1F set.
- **`flow_challenge_poll`.** The count at the actors arm is at least 0, so its sign extension (`sar
  eax,0x18` 0x42F14/0x42F3A) is not observable. The result dword is read again at `0x42E3D` after the
  stubs; no stub rewrites it, so a port that cached it is not separated (no `_rr` row).

No row is a named gap (`gap=`), and no function of the queue was found unrunnable.

## §5 Store-sweep findings

Every field each function writes is seeded with a value different from the one written, with seeded
neighbours; one dropped store per field is a mutant.

| row | stores | seeded with | dropped-store and width mutants |
|---|---|---|---|
| `fight_char_screen_setup` | 0x4F228's two bytes and two words (run real), DS_0010814C, DS_00108150 | each between two seeded neighbours | `@noproj`, `@no14c`, `@no150`; `@14cw`, `@150w`; `@14cslot`, `@storelate` |
| `fight_char_screen_open` / `_both` | 0x104B1B, 0x104B24, the two states, DS_00108144[2], the countdown word, the hook, 0x108174 | the states 0x70/0x71 (neither 0 nor 1), every other field between neighbours | `@no1b`, `@no24`, `@no70a`, `@no70b`, `@no144`, `@nohook`, `@no74`; `@144w`, `@cdb`; `@144idx`, `@storelate`, `@cdlate`, `@hookearly` |
| `fight_4454c` | both arms' per-side bytes, the two start cursors, the eight pick bytes, the eight dwords, 0x108164/65 | the select bytes whole, the dwords 0x108110..0x108133 each distinct | `@no6e`, `@nocopy`, `@no34`, `@noclr9`, `@noclr8`, `@no6e2`, `@no34b`, `@nopick`, `@nozero`, `@no64`; `@zerow`; `@pick3`, `@zero3`, `@pickstride`, `@zerostride` |
| `fight_char_select_pass` | 0x104B1F, the state, the cursors, the hook, the countdown word, 0x105C04 | the select bytes whole; the countdown's high byte non-zero | `@no1f`, `@no70`, `@nohook`, `@nocd`, `@noc04clr`, `@noclamp`; `@cdb`; `@1fearly` |
| `fight_mode_10_step` | the countdown word, 0x108174 | 0x108174 = the sub-state (1), the stored byte 0x5C | `@nodec`, `@nostore`; `@decd` (g11: a word of 0) |
| `fight_health_sync` (three rows) | the slot's +0x43 bit 6, the +0x8E word | +0x41..+0x44 and +0x8C..+0x93 seeded on both slots | `@no43`, `@nodec`; `@decd`; `@43bit`, `@43late` |
| `fight_hud_pass` | the slot's +0x88, +0x8C, +0x92, +0x78 words, 0x1078FB, the pointed slot's +0x28 dword, the record's +0x28 byte | both slots' +0x76..+0x7B and +0x86..+0x93, the other side's words different | `@no88`, `@no8cdec`, `@no8cclr`, `@no92`, `@nocapclr`, `@no78dec`, `@nofb`, `@no28`, `@noor`; `@88d`; `@88other`, `@28late`, `@orlate` |
| `game_mode_33_step` / `_0f_step` | the two latches, 0x104AEC bit 1, the countdown, 0x104B25, 0x104B1B, the slot's +0x41 bit 4, 0x104AFA, 0x104B00, the hook | each between neighbours; 0x104AEC is 0x5D, 0xFF and 0x00 | `@nolatch0`, `@nolatch1`, `@noaec`, `@nodec`, `@no25`, `@nomode`, `@nohook`, `@no1b`, `@no41`, `@noafa`; `@latchlate`, `@aecearly` |
| `flow_challenge_poll` | the hook, 0x1088EE, 0x104AFE, 0x104B25, the count byte, 0x105C04, the seven bytes 0x104B02, 0x104B00, each actor's +8 and +0x28 | the seven bytes mix bits 6 and 7; each actor's +0x04..+0x0F distinct | `@nohook`, `@no8ee`, `@noafe`, `@no25`, `@c04noset`, `@noc04clr`, `@nodec`, `@nohook2`, `@no25b`, `@nomode`, `@e0skip`, `@dcskip`, `@no28or`; `@loop6`, `@loopoff`, `@8off`; `@storeearly` |

The call memory at each recorded call caught the order mutants (`@order`, `@storelate`, `@latchlate`,
`@aecearly`, `@cdlate`, `@hookearly`, `@1fearly`, `@43late`, `@28late`, `@orlate`, `@storeearly`).

## §6 Not done

- **`0x424E8`** (`game_mode_13_step`, 570 bytes, 19 callees), **`0x274FC`** (`game_mode_0d_step`, 708
  bytes, 19 callees) and **`0x296B8`** (`game_mode_32_step`, 696 bytes, 16 callees): not started. No row,
  no claim. The first two have more callees than `diffrun`'s 16 stubs a case, so each needs the split of
  `fight_health_sync` (or one spec per state, as d2e's `0x41C28`). Their callee `0x35658` is rowed and
  seamed by this batch; `0x42CB4` (a callee of `0x424E8`) is rowed but has no seam yet (its row needed
  none: `PR_SEAM0(0x42CB4u)` in `flow_challenge_poll`), and `0x20E90` (another callee of `0x424E8`) has no
  row.

## §7 Port changes other than corrections

Seams (each the callee's first statement):

| addr | function | line |
|---|---|---|
| `0x2BAF4` | `actors_reset_al` (actors.c) | `PR_SEAM(0x2BAF4u, al);` |
| `0x12DA8` | `camera_y_commit` (camera.c) | `PR_SEAM0(0x12DA8u);` |
| `0x43818` | `fight_char_screen_setup` (fight.c) | `PR_SEAM0(0x43818u);` |
| `0x43964` | `fight_char_entry_spawn` (fight.c) | `PR_SEAM(0x43964u, side);` |
| `0x43A08` | `fight_char_select_actor` (fight.c) | `PR_SEAM(0x43A08u, side);` |
| `0x444C8` | `fight_char_screen_open_both` (fight.c) | `PR_SEAM0(0x444C8u);` |
| `0x43928` | `fight_char_join` (fight.c) | `PR_SEAM0(0x43928u);` |
| `0x432A0` | `fight_char_prompt` (fight.c) | `PR_SEAM(0x432A0u, side);` |
| `0x43464` | `fight_char_text_blink` (fight.c) | `PR_SEAM(0x43464u, side);` |
| `0x435AC` | `fight_char_opp_text_blink` (fight.c) | `PR_SEAM(0x435ACu, side);` |
| `0x43EA0` | `fight_char_portrait` (fight.c) | `PR_SEAM(0x43EA0u, side);` |
| `0x43FBC` | `fight_char_fighter` (fight.c) | `PR_SEAM(0x43FBCu, side);` |
| `0x43AAC` | `fight_char_countdown` (fight.c) | `PR_SEAM_RET0(0x43AACu);` |
| `0x43B24` | `fight_char_select_pass` (fight.c) | `PR_SEAM0(0x43B24u);` |
| `0x44798` | `fight_char_team_pass` (fight.c) | `PR_SEAM0(0x44798u);` |
| `0x1A978` | `fight_stance_pass` (fight.c) | `PR_SEAM(0x1A978u, side);` |
| `0x36E2C` | `fight_position_gate` (fight.c) | `PR_SEAM_RET(0x36E2Cu, rec);` |
| `0x34B6C` | `fight_health_sync` (fight.c) | `PR_SEAM(0x34B6Cu, side);` |
| `0x35658` | `fight_hud_pass` (fight.c) | `PR_SEAM(0x35658u, side);` |
| `0x4DEF4` | `fight_effects_idle_pass` (fight.c) | `PR_SEAM0(0x4DEF4u);` |
| `0x349C8` | `fighter_state_default` (fighter.c) | `PR_SEAM(0x349C8u, side);` |
| `0x35D7C` | `fighter_state_35d7c` (fighter.c) | `PR_SEAM(0x35D7Cu, side);` |
| `0x35F84` | `fighter_state_35f84` (fighter.c) | `PR_SEAM(0x35F84u, slot, rec);` |
| `0x361C8` | `fighter_state_361c8` (fighter.c) | `PR_SEAM(0x361C8u, slot, rec);` |
| `0x36300` | `fighter_state_36300` (fighter.c) | `PR_SEAM(0x36300u, slot, rec);` |
| `0x36710` | `fighter_state_36710` (fighter.c) | `PR_SEAM(0x36710u, slot, rec);` |
| `0x399CC` | `fighter_state_399cc` (fighter.c) | `PR_SEAM(0x399CCu, side);` |
| `0x36430` | `fighter_state_36430` (fighter.c) | `PR_SEAM(0x36430u, slot, rec, side);` |
| `0x364FC` | `fighter_state_364fc` (fighter.c) | `PR_SEAM(0x364FCu, slot, rec, side);` |
| `0x35D20` | `fighter_state_35d20` (fighter.c) | `PR_SEAM(0x35D20u, slot, rec);` |
| `0x359E0` | `fighter_state_359e0` (fighter.c) | `PR_SEAM(0x359E0u, slot, rec, side);` |
| `0x37464` | `fighter_state_37464` (fighter.c) | `PR_SEAM(0x37464u, side);` |
| `0x35E6C` | `fighter_state_35e6c` (fighter.c) | `PR_SEAM(0x35E6Cu, slot, rec);` |
| `0x36F10` | `fighter_36f10` (fighter.c) | `PR_SEAM(0x36F10u, slot);` |
| `0x3531C` | `fighter_state_3531c` (fighter.c) | `PR_SEAM(0x3531Cu, side);` |
| `0x354F0` | `fighter_wall_clamp` (fighter.c) | `PR_SEAM(0x354F0u, side);` |
| `0x42F60` | `flow_continue_poll` (flow.c) | `PR_SEAM_RET(0x42F60u, side);` |
| `0x42FE0` | `flow_challenge_join` (flow.c) | `PR_SEAM(0x42FE0u, side);` |

38 seams. The existing rows of the seamed callees were re-run after each seam (real and mutants) and are
unchanged: `actors_reset_al`, `camera_y_commit`, `fight_char_entry_spawn`, `fight_char_select_actor`,
`fight_char_join`, `fight_char_prompt`, `fight_char_text_blink` (+ `_rr`), `fight_char_opp_text_blink`
(+ `_rr`), `fight_char_portrait`, `fight_char_fighter`, `fight_char_countdown`, `fight_char_team_pass`,
`fight_stance_pass`, `fight_position_gate`, `fight_effects_idle_pass`, `fighter_state_default`,
`fighter_state_35d7c`, `fighter_state_35f84`, `fighter_state_361c8`, `fighter_state_36300`,
`fighter_state_36710`, `fighter_state_399cc`, `fighter_state_36430`, `fighter_state_364fc`,
`fighter_state_35d20`, `fighter_state_35b7c`, `fighter_state_359e0`, `fighter_state_37464`,
`fighter_state_35e6c`, `fighter_36f10`, `fighter_state_3531c`, `fighter_wall_clamp`, `flow_continue_poll`,
`flow_challenge_join`. The full self-check above covers every other row.

Export (linkage only): `fight_health_sync` loses `static` and is declared in a `/* PORT: */` block of
`port/src/game/fight.h`.

Harness lines outside the batch's own blocks: one group of lines added at the top of the expected
dictionary of `test_each_stub_declares_the_registers_its_callee_clobbers`
(`tools/tests/test_diff_verify.py`), under `# D5 d5c`, for the 38 stub addresses no earlier row stubs:
`0x12DA8 ()`, `0x1A978 ("edi","ebp")`, `0x2BAF4 ()`, `0x349C8 ("ebp",)`, `0x34B6C ("ebp",)`, `0x3531C ()`,
`0x354F0 ()`, `0x35658 ("edi","ebp")`, `0x359E0 ("ebx","edx")`, `0x35D20 ("edx",)`, `0x35D7C
("edi","ebp")`, `0x35E6C ("edx","ebp")`, `0x35F84 ("edx",)`, `0x361C8 ("edx",)`, `0x36300 ("edx",)`,
`0x36430 ("ebx","edx","ebp")`, `0x364FC ("ebx","edx","ebp")`, `0x36710 ("edx",)`, `0x36E2C ()`, `0x36F10
("ebp",)`, `0x37464 ()`, `0x399CC ()`, `0x42F60 ()`, `0x42FE0 ("ebp",)`, `0x432A0 ("esi","edi","ebp")`,
`0x43464 ()`, `0x435AC ("ebp",)`, `0x43818 ()`, `0x43928 ()`, `0x43964 ()`, `0x43A08 ()`, `0x43AAC
("esi","edi","ebp")`, `0x43B24 ("esi","edi","ebp")`, `0x43EA0 ()`, `0x43FBC ()`, `0x444C8 ("edi","ebp")`,
`0x44798 ()`, `0x4DEF4 ()`, each `E.callee_clobbers` of the image. No existing line of the three harness
files is changed or removed, and `tools/diff_emu.py` is untouched.
