# Track D1 batch d1g — derivation record (2026-10-11)

Differential rows for the already-ported functions of the `game/actors.c` queue
(`.superpowers/sdd/2026-10-11-reverse-d1/d1g-queue.txt`, 29 entries). 23 rows landed, 6 are not done
(§6). Two rows found a port deviation from the raw; both are corrected in their own commits (§3).
Every value below is measured on this branch (`reverse-d1g`), image
`./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_d1g_img.bin`, fixups applied.

Narrow claim, as for every row: equivalence on the exercised blocks and inputs only, each function
with its callees stubbed or run as stated.

## §1 The members and the callers that fix each EAX mask

Masks: 0 = no caller reads EAX; 0xFF = AL; 0xFFFFFFFF = the whole register. Conventions taken from
the earlier records and re-read here: the slot `+0x0C` callbacks' caller `0x35431` is followed by
`xor eax,eax` (`0x35434`; record P1 §P1.4), the `0xD000/0xD100` dispatcher overwrites EAX after its
`call [0x105bd4]` (`mov eax,ecx` `0x2B575/0x2B59A/0x2B5F0`), the slot `+0x18` hook's caller tests the
whole EAX (`test eax,eax` `0x19048`; record P2), the update table's dispatcher `0x24CEF` reloads EAX
(`mov eax,edx` `0x24CED`) or leaves the loop for `mov ah,1` / `int 0x16` (`0x24D08/0x24D0E`).

| addr | port function (file) | size | mask | the caller instructions |
|---|---|---|---|---|
| `0x10D70` | `actor_pset_word_set` (actors.c) | 59 | 0 | `0x4FA7E`, `0x4FB19` `xor eax,eax`. `0x1D3F5/0x1D42C/0x1D458` return through `0x1D2F0`, whose callers load EAX: `0x1D5A8` (`mov al` `0x1D5AD`, `mov ah` `0x1D5B3`, `mov eax,[esp+4]` `0x1D5EC`), `0x1D797` (`mov eax,[ebx*4+0x1028f8]` `0x1D7A1`), `0x1D941` (`xor eax,eax` `0x1D948`), `0x1DD2C` (`0x1DD33`). `0x1D4C6` returns through `0x1D464`: `0x1D5E1` (`0x1D5EC`), `0x1D94A` (`0x1D953`), `0x1DD35` (`0x1DD3E`). `0x1DAE0` returns through `0x1DA84`: `0x1DB70` (`xor eax,eax` `0x1DB7E`), `0x1DC5C` (returns through `0x1DAE8` to `0x26AA1`: `mov al,1` `0x26AA6`, then `0x20EF8`, whose `mov eax,ecx` `0x20F05` precedes any read) |
| `0x2BD20` | `actors_link_held` (actors.c) | 36 | 0xFFFFFFFF | `xor eax,eax` at `0x2BD40`: EAX is 0 whole, the C returns 0 |
| `0x37B54` | `fighter_37b54` (actors.c) | 28 | 0 | the `0xD000` dispatcher; `0x4904F` then `mov eax,0xf0` (`0x4905B`); `0x45B43` and `0x45FD3` return through the `+0x0C` callbacks `0x4505C` and `0x45B50` |
| `0x38B70` | `actor_cursor_reset` (actors.c) | 57 | 0 | `0x20C3D` and `0x2BBDA` both call `0x2F920` next: `mov eax,0x105f38` (`0x2F927`) |
| `0x28E98` | `actor_type_0a19_list_init` (actors.c) | 77 | 0 | `0x20E3D`, `0x20EE4` call `0x34978` next: `mov eax,0x1077a8` (`0x3497F`) |
| `0x48C4C` | `actor_type_2d_list_init` (actors.c) | 107 | 0 | its one caller `0x48DBF` returns through the `+0x0C` callback `0x48D94` |
| `0x2BE5C` | `actor_mode1_pset` (actors.c) | 151 | 0 | `0x28FB5` (`mov eax,0x20` `0x28FBA`), `0x2906D` (`mov eax,0x80` `0x29072`), `0x37FCF` (`mov eax,0x10` `0x37FD4`) |
| `0x2A4FC` | `motion_step` (actors.c) | 292 | 0 | `0x2A2E2` in `0x2A1FC`: `mov eax,ebx` (`0x2A2F8`, `0x2A310`) or the `ret`, and `0x2A1FC`'s callers load EAX (`mov eax,edx` `0x2A38E`; `mov eax,[esp+4]` `0x35818`). `0x38572`: `mov eax,ecx` (`0x38582`, `0x3859B`) or `0x186C4`'s `xor eax,eax` |
| `0x2A39C` | `anim_id_path` (actors.c) | 107 | 0 | `0x2A272`: `mov al,[ebx+0x4e]` (`0x2A277`); on the `0x2A27E` arm `mov ax,[ebx+0x2e]` (`0x2A289`) precedes `or eax,esi; mov [edx+2],ax` (only AX is stored); then as `0x2A4FC`. `0x2ABE6`: `mov ax,[ecx+0x2a]; xor al,al; and ah,4; and eax,0xffff` (`0x2ABEB..0x2ABF4`) |
| `0x19958` | `debris_spawn` (actors.c) | 380 | 0 | `0x19BB3` in the update entry `0x19B90` (no Ghidra function), which falls into the walker `0x19BB8`: `mov eax,[ebx+8]` (`0x19BCC`) or the `ret` to the update dispatcher |
| `0x2372C` | `anim_code_2372C` → `fighter_2372c` (fighter.c) | 162 | 0 | the `0xD100` dispatcher (the dword at `0xE4FA4`); `0x2463E` then `mov eax,0x40` / `mov eax,0x80` (`0x2464C/0x24653`) |
| `0x370F0` | `anim_code_370F0` → `fighter_370f0` (fighter.c) | 118 | 0 | the `0xD000/0xD100` dispatcher (21 stream dwords); `0x48BC3` and `0x48F36` return through the `+0x0C` callbacks `0x48AAC` and `0x48D94` |
| `0x3FF08` | `fighter_3ff08` (fighter.c) | 134 | 0xFF | every exit sets AL (`xor al,al` `0x3FF23`, `0x3FF3A`; `mov al,1` `0x3FF85`), the rest of EAX is scratch (`shl eax,2` `0x3FF2D`, or `0x3C4CC`'s). Its one caller `0x40026` loads AL itself (`mov al,1` `0x4002B`) |
| `0x40BBC` | `fighter_40bbc` (fighter.c) | 120 | 0xFF | the `0x1078E8`-shaped callback (the dword at `0xBDB14`): `0x379E8`, then `test eax,eax` on the whole EAX (record P1 §P1.4). Main path: `mov al,1` (`0x40C25`), non-zero whatever the rest. Null path: `xor al,al` (`0x40BDE`) over `side * 4`, see §4 |
| `0x3FD30` | `fighter_3fd30` (fighter.c) | 166 | 0xFFFFFFFF | the `+0x18` hook (`0x3FF77` stores it): `0x1903F`, `test eax,eax` `0x19048` |
| `0x2F41C` | `text_cursor_next_line` (actors.c) | 21 | 0 | 6 rel32 sites; see the text scan below |
| `0x2F510` | `text_cursor_hold_font2` (actors.c) | 23 | 0 | 67 sites |
| `0x2F434` | `text_number_set` (actors.c) | 47 | 0 | 19 sites |
| `0x2F528` | `text_number_draw_font2` (actors.c) | 74 | 0 | 7 sites |
| `0x2F174` | `text_glyph_at` (actors.c) | 36 | 0 | 22 sites |
| `0x2F388` | `text_cells_release_count` (actors.c) | 145 | 0 | 22 sites |
| `0x1C350` | `render_list_init` (platform/render.c) | 63 | 0 | its one caller `0x2BBD5` calls `0x38B70` next: `mov eax,0x107a1c` (`0x38B79`) |
| `0x45B50` | `fighter_45b50` (fighter.c) | 191 | 0 | the `+0x0C` callback (`0x45C30` stores it) |

**The text wrappers' caller scan (tool-assisted; its limits).** The six wrappers never write EAX after
their last call (`0x2F42E`, `0x2F51F..0x2F526`, `0x2F45B..0x2F460`, `0x2F55F..0x2F56F`,
`0x2F193..0x2F197`; `0x2F388` leaves the cell's 0, its own `xor eax,eax` `0x2F3F0`, or the col when
the count is not positive): the raw EAX is `0x2F198`'s, `0x2F5A0`'s, 0 or the col, and the port's
functions are void, as `0x2F198`'s own row is (record
C3g §C3g.2 item 6, "the same choice the wave-1 void rows make"). The mask-0 claim was checked with a
scratch tracer, not by hand at each of the 143 sites: from every rel32 call site it walks forward
(both arms of a `jcc`, into direct callees until EAX is written) until EAX is read, overwritten, or
the caller returns, and at a `ret` recurses into the rel32 callers of the containing function
(Ghidra's function list plus every rel32 call target as starts), nine levels deep. Result: no read of
the returned EAX on any resolved path. What it flagged, each read by hand:

- `0x2F174`: `0x4ECCA` `mov edx,eax` and `0x4ED35` `add eax,0xa` read the upper half after `xor ah,ah;
  mov al,[..]`; only DX/AX is stored (`mov [esp+0xc],dx/ax` `0x4ECD7`, `0x4ED3A`) and both join at
  `0x4EDB2` `xor edx,edx` / `0x4EDBC` `xor eax,eax`. Dead reads.
- `0x2F510`: the site `0x461D2` is in the function at `0x461B4`, which has no rel32 caller and no
  dword reference in the image. Not resolved.
- `0x2F528`: the site `0x437FE` returns out of `0x43738`, the frame hook stored in `0x104AE4`
  (`0x28D69`, `0x28D86`) and called at `call [0x104ae4]` (`0x4F302` and five more); the sites after
  those calls were not read. `0x4F5BC` returns out of the update-table entry `0x4F4E8` (the dword at
  `0xA86C4`; the dispatcher reloads EAX).

Limits, named: the walk is not path-sensitive; an indirect call or jump, a function reached only
through a stored pointer, and a byte pattern that looks like a rel32 call inside another instruction
(one false call site, `0x2E6CF`, and one false dword reference, `0x43CA8`, were discarded by hand) are not resolved. **The text rows' mask 0 is therefore
weaker evidence than the other rows' and is flagged "tool-assisted" in the report.**

## §2 The rows (measured)

`row | entry | cases | blocks hit/total | mutants | EAX mask | notes`

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `actor_pset_word_set` | 0x10D70 | 4 | 4/4 | 8 | 0 | `ret 4`: EAX = word, `s0` = rec; no callee |
| `actors_link_held` | 0x2BD20 | 3 | 1/1 | 7 | 0xFFFFFFFF | no callee |
| `fighter_37b54` | 0x37B54 | 5 | 3/3 | 9 | 0 | no callee |
| `actor_cursor_reset` | 0x38B70 | 2 | 1/1 | 8 | 0 | `0x654C7` allow (runtime dword fill) |
| `actor_type_0a19_list_init` | 0x28E98 | 2 | 3/3 | 6 | 0 | `0x249C0` allow (its own C3b row) |
| `actor_type_2d_list_init` | 0x48C4C | 2 | 3/3 | 13 | 0 | `0x249C0` allow, `0x2C3FC` VOICE stub |
| `actor_mode1_pset` | 0x2BE5C | 5 | 4/4 | 13 | 0 | `0x2A620` real (recorded) |
| `motion_step` | 0x2A4FC | 10 | 24/24 | 21 | 0 | `0x2B150` stub |
| `anim_id_path` | 0x2A39C | 6 | 5/5 | 11 | 0 | `0x2A408` stub; correction §3.1 |
| `debris_spawn` | 0x19958 | 8 | 26/26 | 16 | 0 | `0x5D7DC` real (recorded), `0x2AE14` SPAWN stub |
| `anim_2372c` | 0x2372C | 5 | 9/9 | 13 | 0 | bound through `fn_resolve`; SPAWN stub |
| `anim_370f0` | 0x370F0 | 6 | 6/6 | 13 | 0 | bound through `fn_resolve`; `0x2BC30` ANIM_BEGIN stub |
| `fighter_3ff08` | 0x3FF08 | 6 | 11/11 | 16 | 0xFF | `0x3C4CC` HIT_B stub |
| `fighter_40bbc` | 0x40BBC | 5 | 10/10 | 15 | 0xFF | `0x339AC` allow, HIT_B stub; limit §4 |
| `fighter_3fd30` | 0x3FD30 | 6 | 8/8 | 17 | 0xFFFFFFFF | `0x33950`/`0x18BD4` allow, `0x18C14` CHECKS and `0x36870` ANIM54 stubs |
| `text_cursor_next_line` | 0x2F41C | 2 | 1/1 | 4 | 0 | `0x2F198` stub (by value, writes the cursor) |
| `text_cursor_hold_font2` | 0x2F510 | 3 | 1/1 | 5 | 0 | `0x2F198` stub |
| `text_number_set` | 0x2F434 | 2 | 1/1 | 7 | 0 | `ret 8`; `0x2EFD4`, `0x2F198` stubs |
| `text_number_draw_font2` | 0x2F528 | 2 | 1/1 | 7 | 0 | `ret 8`; `0x2EFD4`, `0x2F198` stubs |
| `text_glyph_at` | 0x2F174 | 2 | 1/1 | 4 | 0 | `0x2F5A0` stub |
| `text_cells_release_count` | 0x2F388 | 8 | 12/12 | 11 | 0 | `0x2AD40` RELEASE stub |
| `render_list_init` | 0x1C350 | 2 | 3/3 | 7 | 0 | no callee; correction §3.2 |
| `fighter_45b50` | 0x45B50 | 8 | 8/8 | 20 | 0 | `0x33950` allow; ANIM_BEGIN, PALETTE, VOICE stubs |

23 rows, 251 mutants, every block hit (no `unhit_named`). Every binding and mutant was run ten times
on the port side with identical output and no `diffrun` failure (274 bindings, 0 unstable): no
index-like sentinel leaves `mem[]`. No stub address is new: every stubbed callee was already in
`test_each_stub_declares_the_registers_its_callee_clobbers`'s table with the clobbers declared here.

**Exports for the bindings** (not corrections): `motion_step`, `anim_id_path` and `debris_spawn`
lose `static` (declared in `actors.h`). No seam was added.

## §3 Corrections (raw over port, address-proved)

Each is its own commit; neither changes a game path, and neither was run against the oracles.

1. **`anim_id_path` (`0x2A39C`): the pset index is DX, not EDX.** Raw: `xor ecx,ecx` (`0x2A3A0`),
   `mov cx,dx` (`0x2A3A2`), `shl ecx,5` (`0x2A3AA`), `add ecx,eax` (`0x2A3AD`). The port computed
   `DSD(DS_001014EC) + slot * PSET_SIZE` on the whole 32-bit argument. Found by case `a4` (EDX =
   `0x00030001`): `call #0: original 0x2A408(0x10A468, 0x10A220), port 0x2A408(0x10A468, 0x70A220)`
   and the pset word at the wrong address. New: `(slot & 0xffffu) * PSET_SIZE`. Both raw callers pass
   a zero-extended word (`0x2A26E` `mov edx,esi`, ESI from `mov si,[eax+0x56]` `0x2A204`;
   `0x2ABDF/0x2ABE3` `xor edx,edx; mov dx,si`), so no reachable state changes. Mutant
   `anim_id_path@idxw` is the port before the fix (caught by `a4` alone).
2. **`render_list_init` (`0x1C350`): it does not touch the list head.** Raw: the stores are
   `[0x10275c]` (`0x1C35C`), `[eax*8+0x10152c]` and `[eax*8+0x101530]` for EAX = 2..0x244 (`0x1C368`,
   `0x1C372`: 579 nodes `{next; 0}` from `0x10153C`), `[0x102754]` and `[0x102758]` (`0x1C380`,
   `0x1C386`: the 580th node, the list's end). Nothing writes `0x105B44`; the one raw caller
   `0x2BAF4` clears it itself (`mov [0x105b44],ecx` `0x2BBC9`) before `call 0x1c350` (`0x2BBD5`), as
   `actors_reset_al` does. The port's `render_list_init` also stored `DSD(DS_00105B44) = 0`. Found by
   reading the raw while seeding the row (the store sweep seeds `0x105B40..0x105B4B`); removed. The
   render unit tests used the function as a full reset and now clear the head in their own setup
   (`render_reset` in `test_platform.c`; no assertion changed). Mutant `render_list_init@head` is the
   port before the fix (caught on both cases). `render_count`, the port's own C counter, is
   untouched and invisible to the row.

Evidence for both: the 820-frame `--check` dump is byte-identical before and after each (sha256 of
the concatenated `frames/*` `a53064ba3444295e...`), `PR_ORACLE_REQUIRED=1 ./build/run_tests` green.

## §4 Named gaps, limits and unhit blocks

- **No unhit block** in any of the 23 rows.
- **`fighter_40bbc`'s null path under the whole-EAX test.** `0x379EE` tests the whole EAX. On the
  null path the raw returns `xor al,al` over `side * 4` (`0x40BCC..0x40BD1`, `0x40BDE`): zero for
  every side below `0x40`, `0x100` for side `0x40` (case `h4`). The port returns 0. The row's mask is
  `0xFF`, so `h4` agrees; a side of `0x40` or more is not a state the game has (`rec+0x51` is 0 or
  1). The main path's upper bits are `0x3C4CC`'s or the char's and cannot be compared at all.
- **The port's `actor_pset` pool guard.** `actor_mode1_pset` and `text_cells_release_count` take the
  pset through `actor_pset(rec)`, which returns 0 for a record outside the `0x1014F4` pool; the raw
  computes `[0x1014EC] + word[rec+0x56] * 0x20` unconditionally (`0x2BE62..0x2BE71`,
  `0x2F3D4..0x2F3E9`). The rows keep their records in the pool (as C3g's `actor_pset_point` and
  `text_cells_release` rows do); a record outside it is not exercised.
- **`actor_cursor_reset`**'s `0x654C7` (runtime, >= 0x5D000) runs on the original side only; the row
  compares the bytes it leaves. The row is not "closed" by the counter's rule (its callee has no row).
- **The text rows' stubs cannot fill buffers**: `0x2EFD4` writes nothing on either side, so the
  by-value string dwords `0x2F198` records are the zeroed buffer's on both sides (C3f §C3f.5's
  symmetric stall), and the text rows' mask 0 rests on the tool-assisted scan of §1.
- **`debris_spawn`**'s count is always 1 in the raw (`0x199C4`, `0x199DD`, `0x199F7`, `0x19A29`), so
  the loop's back edge `0x19AC4` is never taken; the block leaders are all hit.

## §5 Store sweep

For every field a row's function writes, the pre-state differs from the written value and the
neighbouring bytes are seeded; a dropped store is caught in every row that has one (the `@clear`,
`@s8`, `@v34`, `@r53`, `@afe`, `@s57`, `@b98`, `@l58`, `@c4`, `@x14`, `@x46`, `@g44`, `@land`, `@dec`
... mutants). Findings:

- `render_list_init`: the sweep's seeding of the list head is what exposes §3.2.
- `anim_id_path`: the width case (`a4`) exposes §3.1; `a5` (index `0x101`) separates the word index
  from a byte index.
- `actor_cursor_reset`: the dword above `0x107A1C + 28` and the dword below `0x107A00` survive
  (`@over`, `@short`); the fill is 7 dwords each, not 7 bytes (`@bytes`).
- `actor_type_2d_list_init`: `0x108395` (below the three cleared bytes) survives (`@b95`); the voice
  follows the clears (`@early`, caught by the call memory alone).
- `fighter_3fd30`: each of the six flag stores is a separate mutant, caught by the by-value flag
  dwords of the `0x18C14` call.
- One equivalent mutant was replaced: `text_cells_release_count@count` as `count < 0` cannot differ
  from `count <= 0` (a zero count runs no iteration either way); it is now the raw's do-while without
  its `jle` guard (`0x2F3B9`), caught by `e5`/`e6`.

## §6 Queue entries not done

| addr | size | why |
|---|---|---|
| `0x2F920` | 22 | no port function: the port inlines it as `mem_fill(DS_00105F38, 0, 0x14D4u)` in `actors_reset_al` (`actors.c`, the `0x2BAF4` body). A row would bind the harness's own `mem_fill` call and prove nothing of the port; it is covered when `0x2BAF4` is rowed. Not a correction: the bytes are `mov ebx,0x14d4; mov eax,0x105f38; xor edx,edx; call 0x61a70` (`0x2F922..0x2F92E`), which is that call |
| `0x4505C` | 138 | its switch is `mov dl,[esi+0x57]; cmp dl,4; ja; xor eax,eax; mov al,dl; jmp cs:[eax*4+0x45048]` (`0x4506E..0x4507A`): the index moves to another register, so `switch_cases` does not bound it (the scan reports 3 leaders and an indirect jump). A row needs a `RESOLVED_JUMPS` entry in `tools/diff_emu.py` and its pinned window in `test_each_resolved_jump_table_matches_the_bytes`: shared lines outside this batch's blocks. Its default arm calls the runtime fatal `0x62003`. Not attempted |
| `0x216EC` | 677 | not attempted (time) |
| `0x3EA24` | 508 | not attempted (time) |
| `0x3EE00` | 321 | not attempted (time); calls the runtime fatal `0x62003` |
| `0x3E524` | 263 | not attempted (time); calls the runtime fatal `0x62003` |
