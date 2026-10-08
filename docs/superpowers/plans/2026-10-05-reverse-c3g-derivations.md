# Reverse completion C3g: the frontier rows, part 7 (record)

**Scope.** Track P's tenth verification-only batch (the C3f record §C3f.7's 31-address tail, the
closing batch): the **nineteen rows this session measured** — the nine wave-1 rows (the C3g
scratch's prototype, audited and re-measured here) and ten of wave 2 — one **raw-over-port
correction** (`hit_reaction_b`'s table base `0xA7ACC`), the new `0x3CD94`/`0x2F0F0` seams, the
`hit_gate` AL normalization, the pins for all nineteen, and the plan
`2026-10-05-reverse-c3g-frontier-rows-7.md` for the remaining twelve addresses (wave 2's last two —
`0x38ED0`, `0x34E2C` — wave 3's eight and wave 4's two), with the source record's evidence. Recipe:
E3 record §E3.10; checklist: `.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**Status of the numbers.** Measured by the planner on 2026-10-08 on `reverse-c3g` at the base
`main` `87443cf` (= C3f merged), in this worktree: the baseline in a detached worktree of the same
commit, and a full prototype of the nineteen rows (every gate run, then reverted). The image is
`build/diffrun --exe data/game/C/PRAGE.EXE --image-out FILE`, sha1
`ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's, E3's, P1-P8's, C1-C3f's). Every address below is
capstone 5.0.7 over that image (fixups applied); `unicorn` 2.1.4 runs the original side. Ghidra was
not consulted. The prototype diff is embedded in the plan; the prototype was reverted
(`git checkout -- port tools`), leaving only this record and the plan. The inherited scratch
(wave 1) was audited first: its self-checks were re-run, every pin re-measured, its four
`fighter.c` semantic edits re-checked against the raw, and its `_CALLEE_CLOBBER_FIXES` entry
re-derived (§C3g.6).

---

## §C3g.1 The members, re-derived from the raw

The C3f record §C3f.7's 31-address tail, in its four dependency waves. This session measured
nineteen (marked **M**); the other twelve carry the C3f record's sizes/callers and are the plan's
Tasks 3-4:

| addr | port function | size / insns | callers (rowed) | state |
|---|---|---|---|---|
| `0x1A7CC` | `fighter_block_start` | 296 / 84 | `fighter_input_mask` | **M** |
| `0x2A690` | `actor_pset_point` | 397 / 125 | `pset_write` | **M** |
| `0x37178` | `fighter_37178` | 746 / 243 | `fighter_379c4` | **M** |
| `0x38D90` | `fighter_38d90` | 318 / 87 | `fighter_39040` | **M** |
| `0x38FEC` | `fighter_38fec` | 81 / 32 | `fighter_39040` | **M** |
| `0x3BDDC` | `fighter_attack_consume` | 401 / 127 | `fight_command_map` | **M** |
| `0x3C6A8` | `hit_slot_seed` | 61 / 21 | `hit_chain_resolve` | **M** |
| `0x3CD44` | `hit_scan` | 66 / 33 | `hit_chain_resolve` | **M** |
| `0x3CE58` | `hit_reaction_drive` | 224 / 66 | `hit_chain_resolve` | **M** |
| `0x2F20C` | `text_vertical_set` | 115 insns | `fighter_38d90` | **M** |
| `0x2F4BC` | `text_cursor_hold` | 20 insns | `fighter_38d90` | **M** |
| `0x2F4D0` | `text_number_draw` | 61 insns | `fighter_38d90` | **M** |
| `0x38C5C` | `fighter_38c5c` | 125 B | `fighter_38d90`, `fighter_38d24` | **M** |
| `0x38ED0` | `fighter_38ed0` | 284 B | `fighter_38fec` | plan Task 3 |
| `0x34E2C` | `hit_reaction_apply` | 548 B | `hit_reaction_drive` | plan Task 3 |
| `0x3C600` | `hit_frame_desc` | 165 B | `hit_slot_seed` | **M** |
| `0x3CBC4` | `hit_reaction_a` | 146 B | `hit_reaction_drive` | **M** |
| `0x3CC58` | `hit_reaction_b` | 146 B | `hit_reaction_drive` | **M** |
| `0x3CCEC` | `hit_stance_ok` | 88 B | `hit_scan` | **M** |
| `0x3CE24` | `hit_gate` | 52 B | `hit_reaction_drive` | **M** |
| `0x4CE70` | `hit_reaction_allow` | 176 B | `hit_reaction_drive` | **M** |
| `0x2EFD4` | `text_number_format` | 283 B | `text_number_draw` | plan Task 4 |
| `0x2F0F0` | `text_width` | 105 insns | `text_vertical_set`, `text_cursor_set`, `text_cells_release` | plan Task 4 |
| `0x2F198` | `text_cursor_set` | 115 insns | `text_cursor_hold`, `text_number_draw` | plan Task 4 |
| `0x2F280` | `text_cells_release` | 146 B | `fighter_38c5c` | plan Task 4 |
| `0x2F314` | `text_cells_release_vertical` | 113 insns | `fighter_38c5c` | plan Task 4 |
| `0x2F830` | `text_render` | 239 B | `text_vertical_set` | plan Task 4 |
| `0x1922C` | `hit_stance_timer` | 147 B | `hit_reaction_apply` | plan Task 4 |
| `0x3CD94` | `hit_immunity` | 141 B | `hit_gate` | plan Task 4 |
| `0x2EF24` | `text_number_core` | 34 B | `text_number_format` | plan Task 4 |
| `0x2F5A0` | `text_glyph_emit` | 615 B | `text_render` | plan Task 4 |

The measured rows (cases/blocks/mutants measured; `EAX mask` from the Spec):

| row | entry | cases | blocks | mutants | EAX mask | named notes |
|---|---|---|---|---|---|---|
| `hit_slot_seed` | 0x3C6A8 | 4 | 1/1 | 6/6 | 0 | `0x3C600` stub |
| `hit_scan` | 0x3CD44 | 8 | 6/6 | 6/6 | 0xFFFFFFFF | `0x3CCEC` stub (AL tested) |
| `fighter_38fec` | 0x38FEC | 6 | 4/4 | 5/5 | 0 | `0x38ED0` stub, `0x33950` allow |
| `hit_reaction_drive` | 0x3CE58 | 12 | 16/16 | 11/11 | 0xFF | five stubs, `0x34D8C` allow |
| `fighter_38d90` | 0x38D90 | 3 | 8/8 | 7/7 | 0 | four text stubs, the string path allows |
| `fighter_block_start` | 0x1A7CC | 7 | 8/10 | 11/11 | 0 | two dead arms named |
| `actor_pset_point` | 0x2A690 | 8 | 20/20 | 11/11 | 0 | `0x2A620` stub |
| `fighter_attack_consume` | 0x3BDDC | 8 | 16/16 | 13/13 | 0xFF | three stubs, `0x33950` allow |
| `fighter_37178` | 0x37178 | 14 | 31/31 | 12/12 | 0 | three stubs, `0x1A570` allow |
| `hit_stance_ok` | 0x3CCEC | 9 | 8/8 | 7/7 | 0xFFFFFFFF | — |
| `hit_gate` | 0x3CE24 | 10 | 5/5 | 6/6 | 0xFF | `0x3CD94` stub (AL tested) |
| `hit_frame_desc` | 0x3C600 | 8 | 11/11 | 8/8 | 0xFFFFFFFF | the jump table's 11 blocks |
| `hit_reaction_a` | 0x3CBC4 | 8 | 8/8 | 7/7 | 0xFFFFFFFF | `0x1DDF4` (C1_GEOM) stub, `0x33950` allow |
| `hit_reaction_b` | 0x3CC58 | 7 | 8/8 | 7/7 | 0xFFFFFFFF | the correction's row |
| `hit_reaction_allow` | 0x4CE70 | 17 | 29/29 | 8/8 | 0xFFFFFFFF | the 0x4CE54 jump table |
| `fighter_38c5c` | 0x38C5C | 2 | 1/1 | 10/10 | 0 | four text stubs (call-list row) |
| `text_cursor_hold` | 0x2F4BC | 2 | 1/1 | 4/4 | 0 | `0x2F198` stub with a write |
| `text_number_draw` | 0x2F4D0 | 2 | 1/1 | 7/7 | 0 | `0x2EFD4`/`0x2F198` stubs, cursor save/restore |
| `text_vertical_set` | 0x2F20C | 6 | 6/6 | 9/9 | 0 | `0x2F0F0`/`0x2F830` stubs |

## §C3g.2 The rows: cases, seams, fixtures and the corrections

Each row is a `Spec` in `tools/diff_verify.py` (`C3G_SPECS` and its `c3g_*` fixture helpers), a
binding `b_*` and mutants `m_*` in `port/tests/diff_runner.c` (`k_bindings`), and the exact-set
extensions in `tools/tests/test_diff_verify.py` (`C3G_MASKS`, `C3G_KINDS`, the case-set test, the
clobber table, the counter line). The callee column of the full run's table:

| row | entry | cases/blocks/mutants | callees (each by its own check) |
|---|---|---|---|
| `hit_slot_seed` | 0x3C6A8 | 4 / 1/1 / 6 | `3C600` stub VERIFIED |
| `hit_scan` | 0x3CD44 | 8 / 6/6 / 6 | `3CCEC` stub VERIFIED |
| `fighter_38fec` | 0x38FEC | 6 / 4/4 / 5 | `38ED0` stub unverified (Task 3), `33950` allow VERIFIED |
| `hit_reaction_drive` | 0x3CE58 | 12 / 16/16 / 11 | `3CE24`/`4CE70`/`3CBC4`/`3CC58` stubs VERIFIED, `34E2C` stub unverified (Task 3), `34D8C` allow VERIFIED |
| `fighter_38d90` | 0x38D90 | 3 / 8/8 / 7 | `38C5C`/`2F4BC`/`2F4D0`/`2F20C` stubs VERIFIED, `500BB` allow unverified (named) |
| `fighter_block_start` | 0x1A7CC | 7 / 8/10 / 11 | `18B04`/`1A6AC` stubs VERIFIED, `33A10`/`3AFC4` allows VERIFIED |
| `actor_pset_point` | 0x2A690 | 8 / 20/20 / 11 | `2A620` stub VERIFIED |
| `fighter_attack_consume` | 0x3BDDC | 8 / 16/16 / 13 | `3CF38`/`4649C`/`3C480` stubs VERIFIED, `33950` allow VERIFIED |
| `fighter_37178` | 0x37178 | 14 / 31/31 / 12 | `36870`/`35838`/`36638` stubs VERIFIED, `1A570` allow VERIFIED |
| `hit_stance_ok` | 0x3CCEC | 9 / 8/8 / 7 | — |
| `hit_gate` | 0x3CE24 | 10 / 5/5 / 6 | `3CD94` stub unverified (Task 4) |
| `hit_frame_desc` | 0x3C600 | 8 / 11/11 / 8 | — |
| `hit_reaction_a` | 0x3CBC4 | 8 / 8/8 / 7 | `1DDF4` stub VERIFIED (C1), `33950` allow VERIFIED |
| `hit_reaction_b` | 0x3CC58 | 7 / 8/8 / 7 | `1DDF4` stub VERIFIED (C1), `33950` allow VERIFIED |
| `hit_reaction_allow` | 0x4CE70 | 17 / 29/29 / 8 | — |
| `fighter_38c5c` | 0x38C5C | 2 / 1/1 / 10 | `2F280`/`2F314` stubs unverified (Task 4) |
| `text_cursor_hold` | 0x2F4BC | 2 / 1/1 / 4 | `2F198` stub unverified (Task 4) |
| `text_number_draw` | 0x2F4D0 | 2 / 1/1 / 7 | `2EFD4`/`2F198` stubs unverified (Task 4) |
| `text_vertical_set` | 0x2F20C | 6 / 6/6 / 9 | `2F0F0`/`2F830` stubs unverified (Task 4) |

**Seams and exports** (each seam a first statement; `E.callee_clobbers` re-derives every declared
clobber; a stub's call is declared with those clobbers in `C3G_SPECS`):

| function | address | change | conformance |
|---|---|---|---|
| `fighter_input_scan` | `0x4649C` | `PR_SEAM_RET` (the C3f export gains its seam) | `ebx`, `edx` (stub in `fighter_attack_consume`'s row) |
| `hit_frame_desc` | `0x3C600` | `PR_SEAM_RET` | no clobbers (stub in `hit_slot_seed`'s row) |
| `hit_stance_ok` | `0x3CCEC` | `PR_SEAM_RET` | `edx` (stub in `hit_scan`'s row); the AL test |
| `hit_gate` | `0x3CE24` | `PR_SEAM_RET`; **returns `(u8)hit_immunity(..) != 0`** | `edx` (stub in `hit_reaction_drive`'s row) |
| `hit_immunity` | `0x3CD94` | `PR_SEAM_RET` (new; Task 4's row gains its seam) | `edx` (stub in `hit_gate`'s row) |
| `hit_reaction_allow` | `0x4CE70` | `PR_SEAM_RET` | no clobbers (stub in `hit_reaction_drive`'s row) |
| `hit_reaction_a` | `0x3CBC4` | `PR_SEAM_RET` | no clobbers (stub in `hit_reaction_drive`'s row) |
| `hit_reaction_b` | `0x3CC58` | `PR_SEAM_RET` | no clobbers (stub in `hit_reaction_drive`'s row) |
| `hit_reaction_apply` | `0x34E2C` | `PR_SEAM` | `edx`, `edi`, `ebp` (stub in `hit_reaction_drive`'s row) |
| `fighter_38c5c` | `0x38C5C` | `PR_SEAM` (the C3f seam; now a row) | — |
| `text_width` | `0x2F0F0` | `PR_SEAM_RET` (new; Task 4's row gains its seam) | `edx` (stub in `text_vertical_set`'s row) |

**Corrections and decisions during prototyping (each recorded here with its evidence):**

1. **`hit_reaction_b` was a raw-over-port correction.** The raw `0x3CC58`'s geometry call loads
   its table pointer from `0xA7ACC` (`0x3CCBD` `mov edx,[edx*4+0xa7acc]`), the middle per-character
   table; the port read `HIT_GEOM_TABLE2` (`0xA7B44`, variant A's table, `0x3CC29`). The row's
   `@table` mutant (which reads the other table) verified against the port's wrong base; the row
   caught it on the first run (`hit_reaction_b: b2: call #0: original 0x1DDF4(0x1, 0x10A300,
   0xA7A70), port 0x1DDF4(0x1, 0x10A400, 0xA7A70)`). The port now has
   `HIT_GEOM_TABLE_B 0x000A7ACCu` and `hit_reaction_b` uses it. The row then VERIFIED 7/7.
2. **The AL normalizations (wave 1, re-derived).** `hit_scan`'s `0x3CD68` `test al,al`,
   `hit_reaction_drive`'s `0x3CE63`/`0x3CEBE` `test al,al` and the `movsx edx,ax` at `0x3CF14` were
   the C3g scratch's edits; each is byte-backed (the scratch had left them uncommitted) and each
   row's `@al`/`@hi` mutant is caught only with it. `hit_gate` (this session) is the same pattern:
   the raw's `0x3CE4F` tests AL after `0x3CD94`, so `hit_gate` returns `(u8)hit_immunity(..) != 0`
   and its `@al` mutant (which returns the raw 32-bit value) is caught by case `g9` alone (stub
   `0x37`). The early reject's EAX carries the sign byte's high bits, so the gate's mask is `0xFF`.
3. **`hit_frame_desc`'s default.** The raw's `sel > 6` arm and the jump table's `1/3/5` entries
   return the **caller's ECX** (`0x3C6A0` `mov eax,ecx`); the port returns 0 (the C3f comment). The
   row's default cases hold ECX 0, so the deviation is invisible by construction — a named limit
   (§C3g.5), with the `@def` mutant targeting the `>6` arm's return.
4. **The `_CALLEE_CLOBBER_FIXES[0x3CF38]` entry (wave 1, re-derived).** `0x3CF38` pushes only
   `ebx/ecx/edx/esi` (`0x3CF38..0x3CF3B`) and pops them at both exits (`0x3CFD1..0x3CFD5`,
   `0x3CFFE..0x3D002`); it never writes EDI/EBP, but the byte-derived transitive scan
   over-approximates through its reaction callbacks. The raw caller `0x3BDDC` needs DI across the
   call (`0x3BDFD` `mov di,word[esi*2+0x1088E0]`; read at `0x3BF14` `mov eax,edi`), so the fix is
   required for `fighter_attack_consume`'s row and is exactly the caller-observed set.
5. **The text by-value seams (§P2.7) and the buffer seeding.** `text_cursor_set`'s seam passes the
   string's first 16 bytes as four little-endian dwords (`C3F_PTR_DW`, the `E.Call` names
   `[ebx]`/`[ebx+N]`); `text_number_format`/`text_number_core`'s seams do the same for their
   buffers. The port's `text_number_draw` zeroes its `buf[0x14]` and the stubbed `0x2EFD4` writes
   nothing on the original side, whose unicorn stack is zero: the recorded dwords agree by the
   symmetric stall (C3f §C3f.5's limit). The `0x2F198` stubs in `text_cursor_hold`/
   `text_number_draw` also write `0x105F34` (the cursor) through `E.Call(writes=...)`, so the save
   and restore are observable.
6. **`text_vertical_set`'s return.** The raw's EAX at the `ret` is `0x2F830`'s return (the extent;
   `0x2F26E add esi,eax` leaves EAX alone); its callers read nothing, so the row's mask is 0 (the
   same choice the wave-1 void rows make).
7. **`fighter_38c5c`'s row is the call list.** The function has no reads and no branches; its two
   cases (side 0/1) carry the four text calls' arguments (`@c`'s side-1 case is what separates the
   `0x25` stride from `0x94`).
8. **`0x38ED0`'s calls are `0x2F4BC`, not the plan's by-value `0x2F198` (Task 3).** The raw calls
   `text_cursor_hold` at 0x38F40/0x38F60/0x38F7E/0x38F9D with EAX=-1, EDX=row, EBX=the 0x1C500
   buffer, ECX=0x3000 (`mov eax,0xffffffff` at 0x38F3B; `call 0x2f4bc` at 0x38F40); `0x2F198` is
   inside `0x2F4BC`, outside this row, so no by-value buffer is involved (raw wins).
9. **`0x1922C` had no seam; its row runs it as an allow (Task 3).** It was `static` in `fighter.c`
   where the plan's "calls are the existing seams" expected one; the `0x34E2C` row runs it as an
   **allow** (real, unrecorded) and its mutant cores call it, so `hit_stance_timer` is now
   non-static with a `fighter.h` declaration — Task 3's one interface change, no `PR_SEAM` added
   (Task 4's work). `0x3AFC4` likewise has no seam by design and is an allow.
10. **`hit_immunity`'s `i7` poked the wrong slot (fix wave 2, Task 4 review).** The case runs
    `{"eax": 5, "edx": 2}` but `c3g_imm_case(0, 2, ...)` poked slot 0, so its pokes were dead: the
    raw and the port read slot 5's bytes (`r = [0x107AF3] = 0` by `0x3CDA9`, `ch = [0x107B0E] = 0`
    by `0x3CDC5`), the candidate word at the (0,2) entry (`0x26` at `0xC61B0`) and the char-0 row-0
    mask (`0xA182C` = `0xFF0000`, `0xA1830` = `0x7FFFFFFF`); `i7`'s `@hi` catch came from that
    image `c = 0x26` selecting the high arm (`0x3CDEE` `cmp ecx,0x20` / `jge 0x3CE03`), not the
    fixture. The call is now `c3g_imm_case(5, 2, 5, 5, 3, lo=8)`; the live reads are r=5, ch=5,
    entry (5,2) `c=3` and base `0xA2254` lo=8/hi=0 — the low arm, so `@hi` (which mutates the high
    arm's `[eax+4]` at `0x3CE0B` to `[eax]`) is caught by `i5` alone. The reviewer's "with the fix
    `i7` must keep catching `@hi` on the seeded bytes" is refuted by measurement; `i7` instead
    gains the `@entry` catch (the transposed word at `0xC63C8` zeroed vs the seeded `c=3` at
    `0xC66B0`), which is the liveness proof — the old wrong-slot seed shows no `i7` in `@entry`.
    Re-pinned: `@hi` `['i5']`, `@entry` `['i10','i3','i4','i5','i7','i8','i9']`; every other
    `hit_immunity` pin unmoved.
11. **`host_sprintf` is internal again (fix wave 2, Task 4 review).** Task 4's report correction 3
    exported both host wrappers "for the mutant cores"; only `host_memset` is called directly by a
    core (`diff_runner.c`'s `@fill`/`@m2`), the `0x2EF24` core calls `text_number_core` and never
    `host_sprintf`, so the export was dead surface. It is `static` in `actors.c` again, gone from
    `actors.h`; the `0x65546` seam and its stub row are untouched (the real `text_number_core` still
    reaches it).
12. **`text_render` sign-extends its char (fix wave 2, Task 4 review).** The raw's `movsx eax,bl`
    at `0x2F8F5` passes a sign-extended char to `0x2F5A0`; the port passed `(s32)ch`
    (zero-extended) — behaviourally identical (`0x2F5A0` masks) but a divergent stub-call EAX
    record for chars >= 0x80 (no case covers one). `actors.c`'s call is `(s8)ch` again, and the
    `c3g_2f830_core` mirror in `diff_runner.c` follows. The same wave fixed the `0x3CD94` `@bound`
    comment: the mutation is the bound `0x40 -> 0x80`, not a "dropped" `jl` — the raw's
    `test edx,edx` / `jl` at `0x3CDBC` is dead because EDX is a zero-extended byte.
13. **The `0x34E2C` core's callback arm omitted `sound_voice` (fix wave 1, `a454514`).** The raw's
    callback arm loads `anim[0]+7`, reads the voice word at `0xE9308[.]` and calls `0x2C3FC`
    *before* the `+0x5F` store and the callback (`0x35019`–`0x35032`; store `0x35042`
    `mov [eax+0x5f],cl`; callback `0x35045` `call [esp+0x24]`); the port (`fighter.c`) does the
    same. The mutant core went straight to the store, so every callback-arm case (h5/h6) carried a
    spurious `call #N: original 0x2C3FC(…), port none` difference. The call was inserted in the
    stream arm's exact form (voice, store, `fn_resolve`) and the row's pins re-measured: 14 of 16
    moved, the dropped `call #1`/`call #3` components and h5/h6 cases being that artifact
    (`@cbstore` is now `{'byte'}`/`['h5']` — caught by its own store byte `0x10780F` — `@ff`
    `{'byte'}`/`['h0']`, `@pair` `{'byte'}`/`['h2','h7']`, `@s52` `{'byte'}`/`['h4']`, `@start`
    `{'call #1'}`/`['h4']`; full table in the fix-wave report). The row stays VERIFIED 16/16 and
    the counter is unmoved; the reverted-fix probe restores the spurious calls and fails the
    committed pin.
14. **The Task 5 store sweep's two sentinels (`d4dcd04`).** (a) `hit_reaction_apply`'s
    `word [slot+0x88] = 0` (raw `0x34E9E`; port `fighter.c`) wrote its own pre-state: the image
    word is 0 and `c3g_apply_case` poked `+0x84` only. The fixture now seeds `slot+0x84..+0x8B`
    (`le16(s84) + 0xA5A5 + le16(0x1234) + 0x5A5A`), and dropping the store MISMATCHes `h8`
    (`0x107838: original 0x00, port unchanged`) and the slot-1 twin `0x1078CC` in `h7`.
    (b) `fighter_37178`'s `and dword [slot+0x40], 0xFFBFFFBF` (raw `0x371EC`) was invisible for the
    same reason — the fixture had poked `rec_s`/`rec_o`, the records, not the slots: the five pokes
    were relocated to the slots (`+0x40 = le32(0x00400011)`, `+0x43 = 0x15`, `+0x52 = 0x11`,
    `+0x54 = 0x22`, other `+0x42 = 0x73`; the first seed `+0x43 = 0x55` had bit 6 set and was
    corrected), and dropping the AND MISMATCHes `r11` (`0x1077F2: original 0x00, port unchanged`).
    `pins moved: 0` (measured against the committed pins) and the sweep's core-fidelity pass read
    all 30 `c3g_*_core`s against their port bodies call-for-call with no remaining divergence (the
    voice call of correction 13 included).
15. **Three commit messages understate their content (history not rewritten; the record carries
    the correction).** `8eb63f3`'s message names the nine wave-1 rows, but the commit lands the
    planner's full nineteen-row prototype, the `0xA7ACC` correction and every seam (Task 2 report
    correction 3; the commit's own counter pin is `310/310 … 217/241 (69)`). `065426e`'s
    plan-named message ("wave 2: the `0x3CF38` tree, the text cinq and the `hit_reaction_b`
    correction") describes content `8eb63f3` already held, so the truthful `the 0x38ED0 and
    0x34E2C rows` was used (Task 3 report correction 4). `d4dcd04`'s plan message ("the store
    sentinels and the case-set pins") named a re-pin the measurement refuted; the truthful
    `the +0x88 and +0x40 store sentinels` was used (Task 5 report).
16. **The plan's counters are stale where measured.** Task 2's expected `300/300; 1169/1169; 1;
    213/234 (66 have none)` is the wave-1-only mid-batch prototype; the embedded patch lands the
    nineteen-row prototype, whose measured counter is `310/310; 1242/1242; 1; 217/241 (69 have
    none)` (Task 2 report correction 1; the patch's own committed assertion pins it). The plan's
    re-baseline increments (`+247 mutants, rows with callees +24, no-callee +7`) measure `+266`
    (1087 -> 1353), `+26` (225 -> 251) and `+5` (66 -> 71). The executed states after that: Task 3
    `312/312; 1268/1268; 219/243 (69)` and Task 4 `322/322; 1353/1353; 229/251 (71)` (§C3g.8).
    The plan's Task 3 "`0x38ED0`'s `0x2F198` calls are by-value" is the correction 8 conflict.
17. **The baseline WAV's hashes.** The Task 1 report quotes `make audio-render`'s WAV "sha256" as
    `4194254de1155a2dda0ee0dd69f638d92cf9eea0`; that is the file's SHA-1 (the C3e record §C3e.8
    corrected the same report label). The WAV's sha256 is
    `df74acfb65d345fb72cb214102089f2a0ab8d4b271ddc17e2a5f5c4f1a380844`. The gate's authority is the
    byte compare (`cmp … before-t2.wav` -> `WAV-SAME`), not either hash; §C3g.8 quotes both
    correctly.

**Fixtures.** The rows reuse `c3d_slot_pokes` (C3d) and the slot records; the new helpers are
`c3g_stance_case` (the 0xC619C entry's `d`/`e` flags and the sentinel bytes), `c3g_gate_case` (the
slot's +0x55..+0x57), `c3g_desc_case` (the 0x101514 mode word and the 0xC6B9C/0xC619C entries),
`c3g_react_case`/`c3g_react_b_case` (the per-char table pointers 0xA7B44/0xA7ACC and the 0xA7A70
idx), `c3g_allow_case` (both slots' +0x7A/+0x7B chars) and the cursor seeds at 0x105F34. No fixture
pokes more than 64 bytes per region.

## §C3g.3 The mutants

155 mutants for the nineteen measured rows (82 wave 1 + 73 wave 2), all detected
(`python3 tools/diff_verify.py --self-check`); what alone catches each is pinned by
`test_each_c3g_mutant_is_caught_by_what_it_breaks` (`C3G_KINDS` plus the measured case-set table;
the exact dicts are the ones committed in `tools/tests/test_diff_verify.py`). The kinds (measured;
the record's copy of the test's `C3G_KINDS`):

```
C3G_KINDS = {
    "actor_pset_point@bdc": {'byte'},
    "actor_pset_point@bit": {'byte', 'call #0'},
    "actor_pset_point@cursor": {'call #0'},
    "actor_pset_point@div": {'byte'},
    "actor_pset_point@gte": {'byte'},
    "actor_pset_point@ramp": {'byte'},
    "actor_pset_point@rec3c": {'byte'},
    "actor_pset_point@x0": {'byte'},
    "actor_pset_point@x2": {'byte'},
    "actor_pset_point@x44": {'byte'},
    "actor_pset_point@ymask": {'byte'},
    "fighter_37178@base": {'byte', 'call #0', 'call #0 memory'},
    "fighter_37178@bound": {'byte', 'call #0', 'call #0 memory'},
    "fighter_37178@call": {'call #0'},
    "fighter_37178@dirs": {'call #0'},
    "fighter_37178@facing": {'byte', 'call #0', 'call #0 memory'},
    "fighter_37178@fe": {'byte', 'call #0 memory'},
    "fighter_37178@s18": {'byte', 'call #0 memory'},
    "fighter_37178@s43": {'byte', 'call #0 memory'},
    "fighter_37178@s52": {'byte', 'call #0 memory'},
    "fighter_37178@s54": {'byte', 'call #0 memory'},
    "fighter_37178@s57": {'byte'},
    "fighter_37178@x": {'byte', 'call #0', 'call #0 memory'},
    "fighter_38c5c@c": {'call #0', 'call #1', 'call #2', 'call #3'},
    "fighter_38c5c@col4": {'call #3'},
    "fighter_38c5c@mode": {'call #0', 'call #2', 'call #3'},
    "fighter_38c5c@n4": {'call #3'},
    "fighter_38c5c@r10": {'call #2'},
    "fighter_38c5c@r9": {'call #1'},
    "fighter_38c5c@s2": {'call #0'},
    "fighter_38c5c@s3": {'call #2'},
    "fighter_38c5c@s6": {'call #1'},
    "fighter_38c5c@two": {'call #0'},
    "fighter_38d90@c5c": {'call #0', 'call #0 memory', 'call #1', 'call #2', 'call #3', 'call #4', 'call #5', 'call #6'},
    "fighter_38d90@col": {'call #5'},
    "fighter_38d90@id": {'byte', 'call #2 memory', 'call #3 memory', 'call #4 memory', 'call #5 memory', 'call #6 memory'},
    "fighter_38d90@mode": {'call #4'},
    "fighter_38d90@pct": {'call #6'},
    "fighter_38d90@pctrow": {'call #5'},
    "fighter_38d90@row": {'call #2'},
    "fighter_38fec@call": {'call #0', 'call #1'},
    "fighter_38fec@ch": {'call #0', 'call #1', 'call #2', 'call #3', 'call #4', 'call #5', 'call #6'},
    "fighter_38fec@n": {'call #0', 'call #1', 'call #2', 'call #3', 'call #4', 'call #5'},
    "fighter_38fec@stride": {'call #1'},
    "fighter_38fec@table": {'call #0', 'call #1'},
    "fighter_attack_consume@anim": {'call #1', 'call #2'},
    "fighter_attack_consume@base": {'byte', 'call #1 memory', 'call #2 memory'},
    "fighter_attack_consume@chain": {'byte', 'call #1', 'call #2'},
    "fighter_attack_consume@clr": {'byte', 'call #0 memory', 'call #1 memory', 'call #2 memory'},
    "fighter_attack_consume@cmd": {'byte', 'call #0', 'call #1', 'call #2', 'eax'},
    "fighter_attack_consume@dir": {'byte'},
    "fighter_attack_consume@gate": {'call #0', 'call #1', 'call #1 memory', 'call #2'},
    "fighter_attack_consume@o40": {'byte', 'call #0', 'call #1', 'call #2', 'eax'},
    "fighter_attack_consume@ret": {'eax'},
    "fighter_attack_consume@row": {'byte', 'call #1 memory', 'call #2 memory'},
    "fighter_attack_consume@s5f": {'byte', 'call #0 memory', 'call #1 memory', 'call #2 memory'},
    "fighter_attack_consume@scan": {'call #0', 'call #1'},
    "fighter_attack_consume@state": {'byte'},
    "fighter_block_start@and": {'byte', 'call #0 memory', 'call #1 memory'},
    "fighter_block_start@bound": {'byte', 'call #1 memory'},
    "fighter_block_start@div": {'byte'},
    "fighter_block_start@k": {'byte'},
    "fighter_block_start@s52": {'byte'},
    "fighter_block_start@s53": {'byte'},
    "fighter_block_start@s60": {'call #1 memory'},
    "fighter_block_start@s61": {'byte', 'call #1 memory'},
    "fighter_block_start@s62": {'byte', 'call #1 memory'},
    "fighter_block_start@tbl": {'byte'},
    "fighter_block_start@tri": {'byte', 'call #1 memory'},
    "hit_frame_desc@char": {'eax'},
    "hit_frame_desc@def": {'eax'},
    "hit_frame_desc@entry": {'eax'},
    "hit_frame_desc@gt": {'eax'},
    "hit_frame_desc@mask": {'eax'},
    "hit_frame_desc@s63": {'eax'},
    "hit_frame_desc@side": {'eax'},
    "hit_frame_desc@table": {'eax'},
    "hit_gate@al": {'eax'},
    "hit_gate@bound": {'call #0', 'eax'},
    "hit_gate@call": {'call #0', 'eax'},
    "hit_gate@off": {'call #0', 'eax'},
    "hit_gate@ret": {'eax'},
    "hit_gate@signed": {'call #0', 'eax'},
    "hit_reaction_a@bit": {'call #0', 'eax'},
    "hit_reaction_a@geom": {'call #0', 'eax'},
    "hit_reaction_a@idx": {'call #0'},
    "hit_reaction_a@ret": {'eax'},
    "hit_reaction_a@side": {'call #0'},
    "hit_reaction_a@st": {'call #0', 'eax'},
    "hit_reaction_a@table": {'call #0'},
    "hit_reaction_allow@char": {'eax'},
    "hit_reaction_allow@d0": {'eax'},
    "hit_reaction_allow@d2": {'eax'},
    "hit_reaction_allow@inv": {'eax'},
    "hit_reaction_allow@off": {'eax'},
    "hit_reaction_allow@shift": {'eax'},
    "hit_reaction_allow@side": {'eax'},
    "hit_reaction_allow@z": {'eax'},
    "hit_reaction_b@bit": {'call #0', 'eax'},
    "hit_reaction_b@geom": {'call #0', 'eax'},
    "hit_reaction_b@idx": {'call #0'},
    "hit_reaction_b@ret": {'eax'},
    "hit_reaction_b@side": {'call #0'},
    "hit_reaction_b@st": {'call #0', 'eax'},
    "hit_reaction_b@table": {'call #0'},
    "hit_reaction_drive@allow": {'byte', 'call #2', 'eax'},
    "hit_reaction_drive@apply": {'call #1', 'call #2', 'call #3'},
    "hit_reaction_drive@bound": {'byte', 'call #1'},
    "hit_reaction_drive@flash": {'byte', 'call #1 memory'},
    "hit_reaction_drive@gate": {'byte', 'call #1', 'call #2', 'call #3', 'eax'},
    "hit_reaction_drive@gateal": {'byte', 'call #1', 'eax'},
    "hit_reaction_drive@hi": {'byte', 'call #1 memory', 'call #2 memory'},
    "hit_reaction_drive@mode": {'call #1', 'call #2', 'call #3'},
    "hit_reaction_drive@ret": {'eax'},
    "hit_reaction_drive@sel": {'byte', 'call #1', 'call #2', 'call #2 memory'},
    "hit_reaction_drive@store": {'byte', 'call #1 memory', 'call #2 memory'},
    "hit_scan@al": {'call #1', 'eax'},
    "hit_scan@bound": {'call #0', 'eax'},
    "hit_scan@miss": {'eax'},
    "hit_scan@phase": {'call #0', 'call #1', 'eax'},
    "hit_scan@side": {'call #0', 'eax'},
    "hit_scan@stride": {'call #0', 'call #1', 'eax'},
    "hit_slot_seed@bytes": {'byte'},
    "hit_slot_seed@clear": {'byte'},
    "hit_slot_seed@field": {'byte'},
    "hit_slot_seed@off": {'byte'},
    "hit_slot_seed@phase": {'byte'},
    "hit_slot_seed@scale": {'byte'},
    "hit_stance_ok@char": {'eax'},
    "hit_stance_ok@d": {'eax'},
    "hit_stance_ok@e": {'eax'},
    "hit_stance_ok@entry": {'eax'},
    "hit_stance_ok@lo": {'eax'},
    "hit_stance_ok@ret": {'eax'},
    "hit_stance_ok@st2": {'eax'},
    "text_cursor_hold@args": {'call #0'},
    "text_cursor_hold@mode": {'call #0'},
    "text_cursor_hold@save": {'byte'},
    "text_cursor_hold@str": {'call #0'},
    "text_number_draw@col": {'call #1'},
    "text_number_draw@fmt": {'call #0', 'call #1'},
    "text_number_draw@mode": {'call #1'},
    "text_number_draw@pad": {'call #0'},
    "text_number_draw@row": {'call #1'},
    "text_number_draw@save": {'byte'},
    "text_number_draw@val": {'call #0'},
    "text_vertical_set@ext": {'byte'},
    "text_vertical_set@mode": {'call #0', 'call #1'},
    "text_vertical_set@neg": {'byte', 'call #1'},
    "text_vertical_set@reload": {'byte', 'call #0'},
    "text_vertical_set@row": {'byte'},
    "text_vertical_set@sext": {'call #0'},
    "text_vertical_set@skip": {'byte', 'call #0', 'call #1'},
    "text_vertical_set@vert": {'call #0', 'call #1'},
    "text_vertical_set@wargs": {'call #0'},
}
```

The exact case set that alone catches each mutant (measured on the prototype; the record's copy of
the test's table) is in the plan's Task 5 and the committed `C3G_CASES`; the wave-2 additions:

```
        ("hit_stance_ok@e", ['k0', 'k1']),
        ("hit_stance_ok@st2", ['k0', 'k3', 'k4']),
        ("hit_stance_ok@d", ['k2', 'k5', 'k6']),
        ("hit_stance_ok@lo", ['k5']),
        ("hit_stance_ok@entry", ['k1', 'k8']),
        ("hit_stance_ok@char", ['k2', 'k3', 'k5']),
        ("hit_stance_ok@ret", ['k1', 'k4', 'k6', 'k7', 'k8']),
        ("hit_gate@bound", ['g1', 'g7']),
        ("hit_gate@signed", ['g3', 'g4']),
        ("hit_gate@off", ['g0', 'g2', 'g3', 'g4', 'g5', 'g6', 'g9']),
        ("hit_gate@ret", ['g1', 'g7', 'g8']),
        ("hit_gate@call", ['g0', 'g2', 'g3', 'g4', 'g5', 'g6', 'g9']),
        ("hit_gate@al", ['g9']),
        ("hit_frame_desc@s63", ['d0']),
        ("hit_frame_desc@side", ['d4']),
        ("hit_frame_desc@gt", ['d3']),
        ("hit_frame_desc@table", ['d0', 'd1', 'd2', 'd3', 'd4']),
        ("hit_frame_desc@entry", ['d0', 'd1', 'd2', 'd3', 'd4']),
        ("hit_frame_desc@char", ['d0', 'd1', 'd2', 'd3', 'd4']),
        ("hit_frame_desc@def", ['d5', 'd7']),
        ("hit_frame_desc@mask", ['d1', 'd2', 'd3', 'd4']),
        ("hit_reaction_a@st", ['a0', 'a5', 'a7']),
        ("hit_reaction_a@bit", ['a1', 'a6']),
        ("hit_reaction_a@side", ['a2', 'a3', 'a4']),
        ("hit_reaction_a@table", ['a2', 'a3', 'a4']),
        ("hit_reaction_a@idx", ['a2', 'a3', 'a4']),
        ("hit_reaction_a@ret", ['a2', 'a3', 'a4']),
        ("hit_reaction_a@geom", ['a2', 'a3', 'a4']),
        ("hit_reaction_b@st", ['b0', 'b5']),
        ("hit_reaction_b@bit", ['b1', 'b6']),
        ("hit_reaction_b@side", ['b2', 'b3', 'b4']),
        ("hit_reaction_b@table", ['b2', 'b3', 'b4']),
        ("hit_reaction_b@idx", ['b2', 'b3', 'b4']),
        ("hit_reaction_b@ret", ['b2', 'b3', 'b4']),
        ("hit_reaction_b@geom", ['b2', 'b3', 'b4']),
        ("hit_reaction_allow@side", ['r14', 'r7', 'r8']),
        ("hit_reaction_allow@char", ['r0', 'r1', 'r10', 'r11', 'r14', 'r15', 'r16', 'r2', 'r3', 'r5', 'r6', 'r7', 'r8', 'r9']),
        ("hit_reaction_allow@off", ['r10', 'r14', 'r16', 'r5', 'r6']),
        ("hit_reaction_allow@shift", ['r11', 'r15', 'r16', 'r2', 'r3', 'r4', 'r5', 'r6', 'r8', 'r9']),
        ("hit_reaction_allow@d0", ['r3']),
        ("hit_reaction_allow@d2", ['r7']),
        ("hit_reaction_allow@inv", ['r0', 'r1', 'r10', 'r11', 'r14', 'r15', 'r16', 'r2', 'r3', 'r4', 'r5', 'r6', 'r7', 'r8', 'r9']),
        ("hit_reaction_allow@z", ['r12', 'r13']),
        ("fighter_38c5c@c", ['s1']),
        ("fighter_38c5c@two", ['s0', 's1']),
        ("fighter_38c5c@r9", ['s0', 's1']),
        ("fighter_38c5c@r10", ['s0', 's1']),
        ("fighter_38c5c@s2", ['s0', 's1']),
        ("fighter_38c5c@s6", ['s0', 's1']),
        ("fighter_38c5c@s3", ['s0', 's1']),
        ("fighter_38c5c@mode", ['s0', 's1']),
        ("fighter_38c5c@col4", ['s0', 's1']),
        ("fighter_38c5c@n4", ['s0', 's1']),
        ("text_cursor_hold@save", ['h0', 'h1']),
        ("text_cursor_hold@str", ['h0', 'h1']),
        ("text_cursor_hold@args", ['h0']),
        ("text_cursor_hold@mode", ['h0']),
        ("text_number_draw@val", ['n0', 'n1']),
        ("text_number_draw@pad", ['n0', 'n1']),
        ("text_number_draw@mode", ['n1']),
        ("text_number_draw@save", ['n0', 'n1']),
        ("text_number_draw@fmt", ['n0', 'n1']),
        ("text_number_draw@col", ['n0']),
        ("text_number_draw@row", ['n0']),
        ("text_vertical_set@reload", ['v3', 'v4', 'v5']),
        ("text_vertical_set@neg", ['v2']),
        ("text_vertical_set@wargs", ['v1', 'v2']),
        ("text_vertical_set@skip", ['v1', 'v2']),
        ("text_vertical_set@vert", ['v0', 'v1', 'v2', 'v3', 'v4', 'v5']),
        ("text_vertical_set@mode", ['v1', 'v2', 'v3', 'v4', 'v5']),
        ("text_vertical_set@ext", ['v0', 'v1', 'v2', 'v3', 'v5']),
        ("text_vertical_set@row", ['v0', 'v1', 'v2', 'v3', 'v4', 'v5']),
        ("text_vertical_set@sext", ['v5']),
```

The mutants whose catch includes a call or call-memory kind: the whole 0x3CF38 tree (a/b's
`@side`/`@table`/`@idx` are call-kind; the `0x3C600` and `0x3CBC4`/`0x3CC58` predicates' arg
mutations are call-kind), `fighter_38c5c`'s ten (all call-kind), and the text rows'
`@args`/`@str`/`@mode`/`@val`/`@pad`/`@fmt`/`@col`/`@row`/`@wargs`/`@skip`/`@vert`/`@sext`
(call-kind). The wave-1 rows' mutants are as the C3f scratch measured them; every set was
re-measured here and matches the committed `C3G_KINDS`/`C3G_CASES` exactly (the scratch's own
dumps are stale only in that `hit_slot_seed`'s `0x3C600` and `hit_scan`'s `0x3CCEC` now have rows;
the mutants' sets themselves are unchanged).

## §C3g.4 Counters, measured

| state | diff-verify counter | E2 |
|---|---|---|
| base `87443cf` (detached worktree; this session) | `291/291 functions VERIFIED; 1087/1087 mutants detected; 1 named gaps; 205/225 rows with callees closed (66 have none)` | `233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted 30`; voice `0 / 115 / 19` |
| wave 1 only (nine rows, measured mid-batch) | `300/300 functions VERIFIED; 1169/1169 mutants detected; 1 named gaps; 213/234 rows with callees closed (66 have none)` | byte-identical |
| final prototype (nineteen rows) | `310/310 functions VERIFIED; 1242/1242 mutants detected; 1 named gaps; 217/241 rows with callees closed (69 have none)` | byte-identical |

The wave-1 arithmetic: +9 functions, +82 mutants (6+6+5+11+7+11+11+13+12), rows with callees
225 -> 234 (all nine), closed 205 -> 213 (+8: the four new rows that close — `actor_pset_point`,
`fighter_37178`, `fighter_attack_consume`, `fighter_block_start` — and the four old rows the new
leaves close: `fighter_input_mask` (on `0x1A7CC`), `pset_write` (on `0x2A690`), `fighter_39040`
(on `0x38D90`/`0x38FEC`), `fight_command_map` (on `0x3BDDC`); `fighter_379c4` stays open on the
named `0x5D812`, `hit_chain_resolve` on `0x32BAC`).

The wave-2 arithmetic: +10 functions, +73 mutants (7+6+8+7+7+8+10+4+7+9), rows with callees
234 -> 241 (seven: `hit_gate`, `hit_reaction_a`, `hit_reaction_b`, `fighter_38c5c`,
`text_cursor_hold`, `text_number_draw`, `text_vertical_set`), no-callee 66 -> 69 (`hit_stance_ok`,
`hit_frame_desc`, `hit_reaction_allow`), closed 213 -> 217 (+4: `hit_reaction_a`/
`hit_reaction_b` close on their `C1_GEOM` stub, and `hit_slot_seed`/`hit_scan` — open since wave 1
on `0x3C600`/`0x3CCEC` — close on the new rows; `fighter_38d90`'s four text stubs are now VERIFIED
but it stays open on the named `0x500BB` allow).

## §C3g.5 Named gaps and limits

- **`hit_frame_desc`'s default.** The raw returns the caller's ECX on the `sel > 6` arm and the
  jump table's 1/3/5 entries (`0x3C6A0`); the port returns 0. The row's default cases hold ECX 0,
  so the deviation is invisible by construction; the `@def` mutant only exercises the `>6` arm.
- **The stub stalls.** A stubbed callee writes nothing on either side, so `hit_gate`'s
  `0x3CD94` cannot be seen to set anything, the a/b rows' `0x1DDF4` cannot test real geometry, and
  the text rows' `0x2F198`/`0x2EFD4`/`0x2F830`/`0x2F0F0` cannot fill buffers or advance the
  cursor except where the stub writes are declared (`text_cursor_hold`/`text_number_draw`'s
  `0x105F34` write). The by-value dwords of `text_cursor_set`/`text_number_format` are the zeros
  both sides hold at arrival (the P2.7 limit).
- **`0x2F314` ignores its ECX** (the raw sets `0x2000` before the call); the seam has no mode
  argument and the row cannot claim it (`fighter_38c5c`).
- **`hit_reaction_a`/`hit_reaction_b`'s `0x33950` call is allowed, not recorded**: the port reads
  the slot's char directly instead of the callee's stack ctx; the raw's call is not part of the
  row's claim.
- **`fighter_38d90` stays open on the named `0x500BB` allow** even with all four text stubs rowed.
- The wave-1 rows' limits stand as C3f's §C3f.5 (the seeds are hand pokes; the memory at a call is
  `mem[]` only; the callee column is one level deep).

## §C3g.6 What the planner ran

- The **baseline** on a detached worktree of the clean `87443cf` (not the batch worktree, whose
  scratch was live): `python3 tools/diff_verify.py --self-check` -> the §C3g.4 base counter;
  `make entry-triage` -> `233/262`; `python3 tools/port_progress.py` -> `771 1203 64` /
  `731 731 100`.
- The **inherited-scratch audit.** The predecessor's uncommitted wave-1 prototype was first re-run:
  `--function NAME --self-check` for all nine (`hit_slot_seed` 6/6, `hit_scan` 6/6,
  `fighter_38fec` 5/5, `hit_reaction_drive` 11/11, `fighter_38d90` 7/7, `fighter_block_start`
  11/11, `actor_pset_point` 11/11, `fighter_attack_consume` 13/13, `fighter_37178` 12/12) and the
  full self-check (`300/300`, `1169/1169`, `213/234`). Every pin was re-measured against
  `C3G_KINDS`/`C3G_CASES` (they matched). The four semantic edits in `fighter.c` were re-checked
  against the raw listings (`0x3CD68`, `0x3CE63`, `0x3CEBE`, `0x3CF14`; §C3g.2 correction 2), the
  `_CALLEE_CLOBBER_FIXES[0x3CF38]` entry re-derived from `0x3CF38`'s pushes/pops and `0x3BDDC`'s
  DI use (§C3g.2 correction 4), and the stub-clobber table re-verified by the test
  (`test_each_stub_declares_the_registers_its_callee_clobbers`). Kept: all nine wave-1 rows, the
  four semantic edits and the clobber fix. Reverted: nothing of the scratch was found unjustified;
  the scratch scripts (`tools/_c3g_recon.py`, `tools/_c3g_listing.py`, `tools/_c3g_pin.py`) were
  deleted before the commit.
- The **wave-2 prototype**: ten rows, each measured with `python3 tools/diff_verify.py --function
  NAME --self-check --image ...` until VERIFIED with every mutant detected; the
  `hit_reaction_b` correction was found by its first run. Then the full `python3
  tools/diff_verify.py --self-check` (§C3g.4's final counter), `make entry-triage`
  (byte-identical), `python3 tools/port_progress.py` (unchanged) and the per-row pin dumps. The
  `python3 -m unittest tools.tests.test_diff_verify` run then failed once — the hardcoded
  stub-clobber dict lacked the three stubs the new text rows introduced (`0x2EFD4`, `0x2F0F0`,
  `0x2F830`) — and after the three entries were added the full suite is **111 tests, OK**, the C3g
  exact sets included.
- The **unmeasured twelve** (`0x38ED0`, `0x34E2C`, `0x2EFD4`, `0x2F0F0`, `0x2F198`, `0x2F280`,
  `0x2F314`, `0x2F830`, `0x1922C`, `0x3CD94`, `0x2EF24`, `0x2F5A0`) carry C3f §C3f.7's
  sizes/callers; `0x3CD94` and `0x2F0F0` gained their seams here (their rows would need them
  immediately), the rest of the seam notes stand.

## §C3g.7 The terminal verdict (required): the frontier after this prototype

**Measured on the prototype** (the full run's callee column and the §C3g.2 table; runtime
addresses `>= 0x5D000` and the named non-rows excluded per AGENTS.md's host-libc rule). After the
nineteen measured rows the frontier is **twelve ported addresses**, exactly the plan's remaining
tasks:

- **Wave 2's last two:** `0x38ED0` (`fighter_38ed0`, 284 B, stubbed by `fighter_38fec`) and
  `0x34E2C` (`hit_reaction_apply`, 548 B, stubbed by `hit_reaction_drive`).
- **Wave 3 (eight):** `0x2EFD4` (`text_number_format`, 283 B), `0x2F0F0` (`text_width`, 105 insns),
  `0x2F198` (`text_cursor_set`, 115 insns), `0x2F280` (`text_cells_release`, 146 B), `0x2F314`
  (`text_cells_release_vertical`, 113 insns), `0x2F830` (`text_render`, 239 B), `0x1922C`
  (`hit_stance_timer`, 147 B, called by `hit_reaction_apply`) and `0x3CD94` (`hit_immunity`, 141 B,
  stubbed by `hit_gate`).
- **Wave 4 (two):** `0x2EF24` (`text_number_core`, 34 B) and `0x2F5A0` (`text_glyph_emit`, 615 B).

Every one is ported (a `/* 0xADDR` header) and seams are in place for all of them except
`0x1922C` and `0x2F5A0`/`0x2EF24`'s final call forms (Task 4's own rows add them as needed); the
dependency order is `0x1922C` after `0x34E2C`, `0x2F5A0` after `0x2EF24`/`0x2F198`, and the text
tree bottom-up. **The named non-rows stay:** `0x2EA30` (the inert interrupt lock), `0x1B544`
(host `res_resolve`), `0x5D812`/`0x29D60`/`0x2EA64`/`0x32BAC` (bare stubs/rets), `0x500BB` (the
DPMI clock), `0x5DEED` and the AIL wrappers `0x5DC0F`/`0x5DC8B`/`0x5DD03`/`0x5DEAF`.
**The rows that stay open solely on them** after this prototype: `string_unlock` (`0x500BB`),
`fighter_38d90` (`0x500BB`), `fighter_379c4` (`0x5D812`), `hit_chain_resolve` (`0x32BAC`); every
other open row is open on one of the twelve addresses above.

**Verdict.** The frontier is **not** yet only the named non-rows: it is the twelve ported
addresses above, all rowwable (seam notes in §C3g.6), so **no C3h is provably needed** — C3g's
Tasks 3-4 close them and the residual after C3g is the named non-rows and the four rows that stay
open solely on them. If a later session finds any of the twelve not rowwable, that address — with
its evidence — is the C3h list's start, not a new standing gap.

**Re-measured on the final tree (Task 6 Step 2).** The final `tools/diff_verify.py`'s `SPECS` is
**323 entries** the C3f-final 292 (291 addresses plus the `0x468d8` alias) plus this batch's 31,
**322 distinct addresses** (the alias is the only duplicate). Every one of §C3g.1's 31 addresses
has a `Spec`; **none of the named non-rows has one** (`0x2EA30`, `0x1B544`, `0x5D812`, `0x29D60`,
`0x2EA64`, `0x32BAC`, `0x500BB`, `0x5DEED`, the AIL wrappers `0x5DC0F`/`0x5DC8B`/`0x5DD03`/
`0x5DEAF` — scanned by entry address). The final run's table has **20 rows that stay open solely
on a named non-row** (by missing callee): `0x500BB` — `string_unlock`, `fighter_38d90`,
`fighter_38ed0`, `snd_sample_queue`; `0x5D812` — `actor_spawn`, `set_dead`, `fighter_379c4`,
`text_glyph_emit`; `0x29D60` — `spawn_anim_opcode`, `hit_reaction_apply`; `0x2EA30` —
`release_record`, `actor_alloc`, `text_glyph_emit`; `0x2EA64` — `spawn_anim_opcode`; `0x1B544` —
`palette_acquire`, `effects_spawn`, `snd_sample_queue`; `0x32BAC` — `hit_chain_resolve`; `0x5DEED`
— `snd_music_playing`; `0x5DEAF` — `snd_music_stop`; `0x5DC0F`/`0x5DC8B`/`0x5DD03` —
`snd_samples_stop_all`, `snd_sample_stop`, `snd_sample_queue`, `snd_sample_playing`. Two further
rows stay open solely on the runtime host wrappers (`text_number_core` on `0x65546`,
`text_number_format` on `0x61A70`; both `>= 0x5D000`, host-libc per AGENTS.md, not porting
targets). No row is open on any address outside that set: **no genuine residual, no C3h**.

## §C3g.8 Results (the executed tree)

The plan's Tasks 2-5 were executed on `reverse-c3g` at the base `main` `87443cf` (the planner's
docs-only commits `89eda2c` plan+record, `ee94434` plan pin fix, `10889f6` record pins; Task 1
re-baselined on `10889f6`, image sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`): Task 2
`8eb63f3` (the embedded patch: the nineteen measured rows, the `0xA7ACC` correction and all its
seams), Task 3 `065426e` (the two remaining wave-2 rows `0x38ED0`/`0x34E2C`), Task 4 `f532e9d`
(waves 3+4's ten rows), the Task 3-review fix wave `a454514` (the `0x34E2C` core's voice call),
the Task 4-review fix wave `27f5c5c` (the `i7` seed and the three minors) and the review sweep
`d4dcd04`; this closure commit's sha is in the batch report named below. Every row was re-measured
in the tree; the planner's prototype values held.

| state | diff-verify counter | E2 |
|---|---|---|
| base `10889f6` (Task 1) | `291/291 functions VERIFIED; 1087/1087 mutants detected; 1 named gaps; 205/225 rows with callees closed (66 have none)` | `targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30`; voice `0 / 115 / 19` |
| Task 2 `8eb63f3` (the nineteen-row patch) | `310/310 functions VERIFIED; 1242/1242 mutants detected; 1 named gaps; 217/241 rows with callees closed (69 have none)` | byte-identical |
| Task 3 `065426e` (+2 rows) | `312/312 functions VERIFIED; 1268/1268 mutants detected; 1 named gaps; 219/243 rows with callees closed (69 have none)` | byte-identical |
| Task 4 `f532e9d` (+10 rows) | `322/322 functions VERIFIED; 1353/1353 mutants detected; 1 named gaps; 229/251 rows with callees closed (71 have none)` | byte-identical |
| final (`d4dcd04` + this closure commit) | the same `322/322 … 229/251 (71 have none)` (the sweep moved only fixtures) | byte-identical |

The task gates, measured on this tree (Task 1's baseline; Tasks 3/4's and the fix waves' and the
sweep's re-measures in the ledger directory):

```
diff-verify: 322/322 functions VERIFIED; 1353/1353 mutants detected; 1 named gaps; 229/251 rows with callees closed (71 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

`make entry-triage` is byte-identical (no ported function, no `fn_register`); `python3 -m unittest
tools.tests.test_diff_verify` 111 tests OK on the final tree (the `make diff-verify` phase-1
invocation runs 176 across `test_diff_emu` + `test_diff_verify`); `PR_ORACLE_REQUIRED=1
./build/run_tests` `all checks passed`; `symbols.h` regeneration is byte-identical; `python3
tools/port_progress.py` stays `771 1203 64` / `731 731 100`; README untouched; AGENTS.md and the
Makefile unchanged (no value moved).

**The three waves' content.** Task 3's two rows: `fighter_38ed0` (8 cases, 12/12 blocks, 10/10
mutants) and `hit_reaction_apply` (9, 22/22, 16/16), plus `hit_stance_timer`'s export (correction
9). Task 4's ten: `text_width` 6/10-10/10/6, `text_cursor_set` 6/6-6/6/7, `text_cells_release`
8/12-12/12/10, `text_cells_release_vertical` 6/7-7/7/7, `text_render` 11/24-24/24/10,
`text_number_core` 3/1-1/1/4, `text_number_format` 11/9-9/9/8, `text_glyph_emit` 30/54-54/59/16
(five dead arms named), `hit_immunity` 12/10-10/10/8, `hit_stance_timer` 13/7-7/7/9 — 110 cases,
85 mutants. **The Tasks 3-4 corrections carried here so the record is self-contained** (measured
in the task reports): `_CALLEE_CLOBBER_FIXES[0x2AD40] = ("edx",)` is raw-derived, not the plan's
set — `0x2AD40` pushes only ebx/ecx/esi while the raw callers `0x2F280` (uses EDI at `0x2F2ED`)
and `0x2F314` (EBP at `0x2F37A`) read them across the call, and the pre-existing `RELEASE`
constant was updated to the same caller-observed set; `text_number_core`'s planned `@fmt` mutant
is not expressible (the fmt pointer is a constant inside the callee) and became `@val`;
`text_number_format`'s `@copy` is the copy count (a source-offset mutation is unobservable through
the `0x2EFD4` stub's all-zero buffer, the P2.7 stall); `text_glyph_emit`'s spawn runs `0x2AE14`
real with its exercised tree allowed (`0x33754`/`0x2A820` stubbed), and its dead arms are five
(`0x2F647`/`0x2F660`/`0x2F684` by the `and eax,0xf000` at `0x2F5BB`; `0x2F6D8`/`0x2F6E0` by the
ESI mask at `0x2F5AC`); `hit_stance_timer` needs no seam — its only row caller runs it as an
allow and the mutant cores call the port (the plan's "Task 4 still adds its `PR_SEAM`" and §C3g.7's
"except `0x1922C`" are stale); `c3g_glyph_case`'s `mode` parameter is documentation only.

**The review sweep and the fix waves.** The sweep (Task 5, `d4dcd04`) found the two invisible
stores of correction 14 and proved each caught once seeded (drop-store probes: `0/1` VERIFIED,
then `1/1` restored); `pins moved: 0`. Its core-fidelity pass read all 30 `c3g_*_core`s against
their port bodies call-for-call: no divergence remained after the fix waves. The Task 3-review fix
wave (`a454514`) restored the `0x34E2C` core's `sound_voice` call (correction 13) and re-measured
14 of its 16 pins; the Task 4-review fix wave (`27f5c5c`) re-seeded `hit_immunity`'s `i7` to slot
5 (correction 10), made `host_sprintf` static again (correction 11) and fixed the `text_render`
cast and the `@bound` comment (correction 12).

**The closure-value accounting.** +31 functions (the 31 rows), +266 mutants (1087 -> 1353; 155 for
the nineteen-row patch, +26 for Task 3's two, +85 for Task 4's ten), rows with callees 225 -> 251
(+26: +16 the patch, +2 Task 3, +8 Task 4), no-callee 66 -> 71 (+5: +3 the patch, +2 Task 4),
closed 205 -> 229 (+24). Every wave-1/2 row that was open on a Task 3-4 address closes:
`fighter_38fec`/`hit_reaction_drive` (Task 3's two), `hit_gate`, `fighter_38c5c`,
`text_cursor_hold`, `text_number_draw`, `text_vertical_set` (Task 4's five); Task 4's new rows
that close are `text_cursor_set`, `text_cells_release`, `text_cells_release_vertical`,
`text_render` and `hit_stance_timer` (the other three stay open: `text_number_core`/
`text_number_format` on the runtime wrappers, `text_glyph_emit` on `0x2EA30`/`0x5D812`), and
Task 3's two rows stay open (`fighter_38ed0` on `0x500BB`, `hit_reaction_apply` on `0x29D60`).

**The terminal verdict re-measure** is §C3g.7's closing paragraph (this task): all 31 rowed, no
named non-row rowed, 20 rows open solely on named non-rows plus two on the runtime wrappers, no
genuine residual, **no C3h**.

**The full gate** on this closure commit (the plan's parallel-safe overrides, `T=c3g`; log
`/tmp/pr_c3g_final.log`): the run's exact lines (`EXIT=0`, `ORACLES-EQUAL`, `WAV-SAME`, the gp
ratchets, the counter above) are recorded in the batch report
`.superpowers/sdd/2026-10-05-reverse-c3g-frontier-rows-7/task-6-report.md`. The plan's expected
values: the 45 oracle lines equal to the k7-k12 baseline (`ORACLES-EQUAL`), the `make audio-render`
WAV `cmp`-equal to `before-t2.wav` (`WAV-SAME`; sha1
`4194254de1155a2dda0ee0dd69f638d92cf9eea0`, sha256
`df74acfb65d345fb72cb214102089f2a0ab8d4b271ddc17e2a5f5c4f1a380844`), every gp ratchet at its pin
(gp-idle-loss 2064/8320; gp-u5-charsel 516/1513; gp-u6-moves-b 2139/3248/3248; gp-keys 11 effects;
gp-twop 612/1506/1506; U8 RA 1072/2274, LT 1076/2338, RT 1098/2402, TW 1107/2466, HC 1022/2274,
EN 278/1174, AS 1087/2018; gp-u9-win 346/3503, path 8, win 3503; gp-u10-ending 331/9954, path 30,
win 9954) and `symbols.h` idempotent. The batch's behavioral changes — the `0xA7ACC` correction
and the AL normalizations — are the paths the ladder proves oracle-invisible.

**The C3 chain ends here**: after C3g the frontier is exactly the named non-rows and the rows
open solely on them; any later track-P work starts from this record's §C3g.7.
