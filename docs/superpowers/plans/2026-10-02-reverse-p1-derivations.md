# Reverse completion P1: the finisher entries and their +0x0C callbacks (record)

**Scope.** Track P's first batch (spec `2026-09-30-reverse-completion-design.md` §4 track P, §6 "P: each ported
function has a verification row; unhit blocks and non-emulable instructions named; counters and README percentage
updated") and the batch sequence of the whole track. Plan: `2026-10-02-reverse-p1-finishers.md`. Inputs: E2's record
and committed table (`2026-10-01-reverse-e2-derivations.md`, `2026-10-01-reverse-e2-triage.md`), E3's record
(`2026-10-01-reverse-e3-derivations.md`, §E3.10 is the recipe this batch follows item by item), U0's record
(`2026-09-30-gameplay-u0-derivations.md` §U0.4, §U0.12).

**Status of the numbers.** Measured by the planner on 2026-10-01/02 at `main` `834b703` in two scratch copies of the
tree: a prototype (every change at once; `make verify` run on it) and a replay (a fresh copy of `834b703`, the plan's
Tasks 2-5 applied in order with the plan's own scripts; every red and green output and every mutation result the plan
quotes is from the replay; `make verify` run again on its final state). **The image** is `build/diffrun --exe
data/game/C/PRAGE.EXE --image-out FILE` at `834b703`: 1 028 304 bytes from `0x10000`, sha1
`ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (the same as E2's and E3's). Every address and instruction below is capstone
5.0.7 over that image (fixups applied); `unicorn` 2.1.4 runs the original side. Ghidra was not consulted: the raw bytes
decide every fact here.

---

## §P1.1 The target list today

`python3 tools/entry_triage.py --image IMG --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt
--check docs/superpowers/plans/2026-10-01-reverse-e2-triage.md` at `834b703` (the committed table, equal to a fresh run):

```
entry-triage: targets 323 unported, 172 ported; supplement 131 (31 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 46 in unported code, 69 in ported code, 19 nowhere
```

By batch (the table's "P-track batches"): callbacks 22 unported, finishers 6, animation targets 61, other 3 (`0x10604`,
`0x1BDF4`, `0x29CFC`), span writers 231 (E2 decision D2: not ported one by one). So **92 non-span targets**, plus the
supplement's **31** unported entries (E2 §E2.5 counted 35 before U6a/U6b), the **8** unported untrusted entries, the
**8** unexamined immediates of §E2.11 (`0x1D2D0 0x2D3FC 0x2D414 0x2D444 0x2D45C 0x2D474 0x2D48C 0x3A820`), the six span
dispatchers of table `0x80C8C` (D2) and the four seamed callees without a row of their own (`0x2C3FC 0x2BC30 0x2AE14
0x3C480`, E3 §E3.6). `python3 tools/port_progress.py` prints `771 1203 64` / `731 731 100`.

## §P1.2 What E2's list does not hold (raw)

E2's universe needs an entry right after a `ret` (§E2.1(c)). Three shapes of real code fail that test.

**After a jump table.** A function whose bytes start right after a `jmp [idx*4+T]` table (E2 §E2.11 found one,
`0x3A588`). Scan (scratch `aftertables.py`): every `jmp dword ptr [idx*4 + T]` (no base) below `0x5D000`, the table
read while each dword is a code address, the address right after it: 98 tables, **27 ends outside the Ghidra
functions, 14 of them ported already and 13 not**:

| addr | after table (entries) | reached by (a dword equal to it anywhere in the image) | status |
|---|---|---|---|
| `0x15584` | `0x1556C` (6) | the immediate at `0x1569F` (`mov [ebx+0xc],0x15584` in `0x1567C`) | P1 |
| `0x1579C` | `0x1578C` (4) | `0x1592B` (in `0x15908`) | P1 |
| `0x23D38` | `0x23D20` (6) | `0x23EE3` (in `0x23EC0`) | P1 |
| `0x23AE0` | `0x23AC4` (7) | stream dwords `0xD4FD4 0xE1948` (animation target) | P7 |
| `0x29C78` | `0x29C5C` (7) | 14 stream dwords (`0xD2BDA` ...) | P7 |
| `0x2EE3C` | `0x2EE18` (9) | none | P8 (reach evidence first) |
| `0x37E40` | `0x37E24` (7) | none | P8 |
| `0x3A3FC` | `0x3A3EC` (4) | none | P8 |
| `0x3A820` | `0x3A810` (4) | `0x3A921` (§E2.11's immediate at `0x3A91E`) | P8 |
| `0x47E9C` | `0x47E8C` (4) | `0x48024` (in `0x47FCC`) | P3 |
| `0x4844C` | `0x48438` (5) | `0x48649` (in `0x48608`) | P3 |
| `0x4AEC4` | `0x4AEAC` (6) | none | P8 |
| `0x4B03C` | `0x4B024` (6) | 30 stream dwords (`0xEE26E` ...) | P7 |

**After a tail `jmp`.** `0x154E8` ends `jmp 0x2c3fc` (`0x1550B`), so `0x15510` (`push ebx`) is no candidate; three
animation-stream dwords hold it (`0xD29FA 0xD2B62 0xD31DC`): an animation target outside E2 (P4).

**A bare `ret` as a target.** The animation dwords (E2 rule 6 recomputed over the whole data object, scratch): 128
values, 8 outside E2's three lists and Ghidra: `0x15510`, `0x23AE0`, `0x29C78`, `0x4B03C` above, `0x347B8 0x37EA0
0x3E480` (ported), and `0x213F0`, whose first instruction is a `ret` (the end of the previous function; the same
shape as `0x29D60`, a pinned harmless miss, §G.24): P7 names it.

**Consequences.** (1) The untrusted entry `0x158ED` (E2 §E2.5) is a block of `0x1579C` (`jg 0x158ed` at `0x15850`,
`0x1579C`'s case 3), not an entry: P1 resolves it. (2) Four of E2's 19 "nowhere" voice sites lie in P1's after-table
functions: `0x155EA` (`0x15584`), `0x15802` and `0x158CD` (`0x1579C`), `0x23E9D` (`0x23D38`). The E2 table cannot
show either (its universe is unchanged), so the record carries them.

## §P1.3 The P-track roadmap

Each batch is one plan and one subagent-driven run, ordered smallest-dependency first: a batch ports only functions
whose direct callees are ported (seamed or allowed) or ported earlier in the same batch. Sizes are instructions and
basic blocks of `diff_emu.static_scan(..., switches=True)` from the entry (tail `jmp` to `0x2C3FC` not followed);
"calls" lists the direct callees (scratch `roadmap.py`).

| batch | functions | members (addr) | callees to seam first |
|---|---|---|---|
| **P1** (this plan) | 12 + the row of `0x1A570` | finishers `1567C 15908 23EC0 45D14 23BF8 402FC`; their +0x0C callbacks `15584 1579C 23D38 23B68 401D4`; `38034` | `1A570 2A17C 188AC` (and `38034` as a callee) |
| **P2** callbacks A | 19 | `14EF8 14F50 15478 21114 21374 2116C 211F0 212CC 22938 22510 22404 22588 22A00 237D0 2381C 3D10C 3DADC 3DB34 3DCEC` | `34D8C 18BD4 18C14 18AF8 39834 39A10 3C208 3C358` |
| **P3** callbacks B | 24 | `475EC 47608 47624 47720 476FC 47648 47688 47874 47798 477A8 477E8 47830 47FCC 47CB0 47D24 47E9C 48608 48054 48170 480B4 4811C 4844C 48964 489A0` | `3C190 35838 39FB0 3A95C 3B298 3B714 36870 188DC 36D98 3C148 468D8 3C16C` |
| **P4** animation A | 24 | leaves `156D4 18BC8 21084 23CA4 3F174 400E0`; `14FA8 1549C 154E8 15510 229E8 241A8 243F8 3427C 34308 3438C 34418 344A4 34530 345BC 400EC 40358 45C54 489DC` | `29C08` |
| **P5** animation B | 16 | `14FF8 150AC 156E0 21044 22AB8 23868 37B70 3D328 3DA50 3DB8C 3DC3C 403A0 40FBC 48A20 24508 24454` | `2AD40` |
| **P6** animation C | 18 | `22338 22494 22A40 23F10 2400C 241F4 24338 24220 2BDA0 37DD4 3E160 40148 40170 45C98 47E04 47E30 482E4 48374` | `39280 39F40 13244 2A148 2BCF4 1890C 5D7DC 29BC8 37D18 13C70 3AA54` |
| **P7** unported callees, outside-E2 targets | 14 | `2BDB8 213F4 3E424 224EC 23960 23A7C 2BDE8 36114 3A9D8 48254 23AE0 29C78 4B03C 213F0` | `1883C 33A10 2B150 41310 49444` |
| **P8** other | 16 + triage | `10604 29CFC 1BDF4 19AD4 19DD5 26163 26226 34962 45444 49078 2EE3C 37E40 3A3FC 3A820 4AEC4 1D2D0`; triage of `2D3FC..2D48C` (six addends of stride 0x18 whose scan runs into runtime code and an `int` at `0x6201B`/`0x62003`: data, not entries, until shown otherwise); the 15 voice sites still in no body | `13420 249B0 249D0 37B54 2EB80 18714 10D70`; `1BBAC`/`2D62C` are host-owned |
| **C1, C2** callee rows | about 45 rows, no port | the ported callees P rows stub: after P1 `2C3FC 2BC30 2AE14 3C480 188AC 2A17C`, then each batch's seamed list above | none (verification only) |
| **span** | 6 dispatchers + 231 writers | decision D2 of the plan (§P1.10) | - |

So **8 porting batches, 143 functions** (12 + 19 + 24 + 24 + 16 + 18 + 14 + 16), two verification-only batches of
callee rows, and the span decision. Notes: `22404` precedes `22588` and `224EC` (P2 before P7); `48170` precedes
`480B4` (both P3); `1BDF4` calls the host-owned ISR sampler `0x1BBAC` (`in` at `0x1B899`, E3's `NAMED_GAP` row) and
`0x2D62C` (tick): its row stubs both, which needs seamed host-side C functions or stays a named gap; `19DD5` is two
bytes inside the live Ghidra body of `0x19D34` (E2 §E2.5): reclassify, not port; `0x45444` calls the runtime error path
`0x62003`.

## §P1.4 Batch 1: the choice, the call conventions, the callee declarations

**Why the finisher cluster first.** (a) It is the one P item with a known in-play deviation (spec O4; U0 §U0.12):
since U0, `0x37774`/`0x37898` arm the finisher for characters 0, 3, 4 and 6 and `0x379C4` then misses the entry and
starts `0xC9260[char]` where the raw runs the entry. (b) Every direct callee is already seamed or allowed except three
small ones (`0x1A570`, `0x2A17C`, `0x188AC`) and `0x38034`, which the batch ports; nothing waits on an unported
function. (c) The finisher entries store slot +0x0C callbacks the slot dispatcher then runs every frame (`0x3531C`
case 7), so porting the entries without the callbacks would leave the port half way down the same path: the batch
takes the five callbacks (three of them outside E2, §P1.2) and `0x38034`. The alternatives are §P1.10's decision D1.

**The finisher entries' caller.** `0x379C4` at `0x379E4..0x379F0`: `mov eax,ebx; mov edx,[ebx]; call dword ptr
[0x1078e8]; test eax,eax; jne 0x37a54`. EAX = slot, EDX = rec, and the **whole EAX** is tested. A byte search for the
dword `0x1078E8` finds five instructions (`0x376C1`, `0x377F4`, `0x37919` the three stores of `0x37640/0x37774/0x37898`,
`0x379DC` the `cmp`, `0x379E9` the call): the call at `0x379E8` is the only reader. The tables hold (dwords at
`0xBDAE4 + 4c` / `0xBDB00 + 4c`): `0 0 48BE0 1567C 45C10 0 23BF8` and `402FC 0 48F54 15908 45D14 40BBC 23EC0`.
Mask: five entries end `mov al,1` after their last call (`0x156CF`, `0x1595B`, `0x23F0A`, `0x45D4D`, `0x40348`), so EAX
is non-zero whatever the upper bits: **mask `0xFF`**, the C returns 1. `0x23BF8`'s early return is `xor al,al` over the
zero-extended word `DS_00104AFC` (`0x23C00..0x23C11`), so EAX = word & 0xFF00, which the caller tests whole: **mask
`0xFFFFFFFF`** and the C returns that value; on its main path EAX = 0x2BC30's EAX with AL = 1 (named limit, §P1.10).

**The +0x0C callbacks' callers.** `0x3531C` case 7: `mov ebp,[ecx+0xc]; ...; mov eax,ecx; call dword ptr [ecx+0xc]`
(`0x35425..0x35431`) then `xor eax,eax` at `0x35434`; `0x38434`: `call dword ptr [ecx+0xc]` at `0x384D9`, then the pops
and `ret`; its only caller is `0x3856B`, followed by `mov eax,esi` (rel32 scan). EAX = slot, EDX = rec, EBX = side
(the port's `fighter_slot_cb (slot, DSD(slot), side)`). Both discard EAX: **mask 0**. `0x38034`'s only caller is
`0x402A7` in `0x401D4`, followed by `mov eax,0x46`: **mask 0**.

**The callee declarations** (args in the port's C order; `pop` = the callee's `ret N`; clobbers =
`E.callee_clobbers(image, addr)`):

| `E.Call` | callee (port C) | args | `ret` | clobbers, by the bytes |
|---|---|---|---|---|
| `VOICE` (E3) | `0x2C3FC` `sound_voice(id)` | `eax` | `ret` | none |
| `ANIM_BEGIN` (E3) | `0x2BC30` `actors_anim_begin(rec, stream, frame)` | `eax edx s0` | `ret 4` | `edx` |
| `HIT_B` (E3) | `0x3C4CC` `hit_anim_start_b(rec, stream, frame)` | `eax edx s0` | `ret 4` | `edx` |
| `SPAWN` (E3) | `0x2AE14` `actor_spawn(desc, a2, a3, a4, a5)` | `eax edx ecx ebx s0` | `ret 4` | `ebx ecx edx` |
| `BIT15` (new) | `0x1A570` `fighter_actor_bit15_clear(side)` | `eax` | `ret` (`0x1A5A8`) | none: pushes EDX (`0x1A570`), pops it (`0x1A5A7`) |
| `PALETTE` (new) | `0x2A17C` `actor_pset_palette(rec, word, handle)` | `eax edx ebx` (`mov ecx,eax` `0x2A17E`, `mov eax,edx` `0x2A192`, `test ebx,ebx` `0x2A1AA`) | `ret` | `edx`: saves ECX, ESI (`0x2A17C/0x2A17D`), writes EDX (`0x2A194`) |
| `ANCHOR` (new) | `0x188AC` `hit_anchor_set(side, x, y)` | `eax edx ebx` (`mov ecx,eax`, `mov esi,edx`, `mov [eax+0x1c],ebx`) | `ret` | `edx`: saves ECX, ESI; `mov edx,[eax*4+0x1077b0]` `0x188BC` |
| `F38034` (new) | `0x38034` `fighter_38034(side)` | `eax` | `ret` | none: pushes EBX, ECX, EDX, pops all three before its `ret` |

`0x33950` (`fighter_ctx_same`) and `0x339AC` (`hit_anim_ctx`) stay allow-mode (their EAX is a stack buffer in every
caller, E3 §E3.4) and have their own E3 rows.

**`0x1A570`'s row and mask.** `push edx; ... mov eax,[eax*4+0x1077b0]; mov ax,[eax+0x56]; ... mov edx,[0x1014ec];
shl eax,5; mov ax,[edx+eax]; xor al,al; and ah,0x80; and eax,0xffff; sete al; pop edx; ret`: AL = 1 when the side's
record's actor word (`DSD(DS_001014EC) + rec+0x56 * 0x20`) has bit 15 clear; EAX is then 1, else 0x8000. A rel32
scan finds **69 direct `call` sites and no `jmp`**; each first reads AL (`test al,al`, `mov dl,al`, `mov cl,al`,
`cmp dl,al`, `and eax,0xff`, `mov [..],al`) or overwrites AX (`0x3D289`): **mask `0xFF`**. As a stub its EAX is the C
predicate's 0 or 1 (`0x8000` would read as true in the port's `int`, though the raw's AL is 0: a harness value outside
the C function's codomain, so not used).

**Seeds.** Hand pokes into the image's zero BSS (`E3_SLOT 0x10A200`, `E3_REC 0x10A300`, `E3_REC2 0x10A400`, `E3_OUT
0x10A500`, `P1_PSET 0x10A600`) and into real globals, as E3 did (named deviation from spec §5.3, E1 §E.7). `diffrun`
reads at most 16 pokes per case (`c.npoke < 16` in `diff_runner.c`): the callback specs merge adjacent fields
(`+0x52..+0x57` as one poke) to stay under it.

## §P1.5 The four same-shape finishers

`0x1567C` (char 3, `0xBDAF0`), `0x15908` (char 3, `0xBDB0C`), `0x23EC0` (char 6, `0xBDB18`), `0x45D14` (char 4,
`0xBDB10`). Each: `push ebx; mov ebx,eax; mov eax,edx; mov edx,STREAM; push 0x40400000; call 0x2bc30`, then slot
stores, then (three of them) `mov eax,VOICE; call 0x2c3fc`, `mov al,1; pop ebx; ret`. One block each.

| entry | stream | +0x53 | +0x0C | +0x42 | voice |
|---|---|---|---|---|---|
| `0x1567C` | `0xD32A8` (`0x15681`) | 7 | `0x15584` (`0x1569C`) | `|= 8` (`mov ah,[ebx+0x42]; or ah,8`) | `0xAF` |
| `0x15908` | `0xD3334` (`0x1590D`) | 7 | `0x1579C` (`0x15928`) | `|= 8` | `0xAF` |
| `0x23EC0` | `0xE1B24` (`0x23EC5`) | 7 | `0x23D38` (`0x23EE0`) | untouched | `0xAA` |
| `0x45D14` | `0xEB876` (`0x45D19`) | 3 | 0 (`0x45D34`) | untouched | none |

All four also store +0x52 = 9, +0x54 = 0, +0x57 = 0, +0x18 = +0x1C = +0x14 = 0. Cases: +0x42 = 0x21 and 0xFF (the one
field read); every stored field seeded with a sentinel; the voice stub returns 1, then 0 (AL is overwritten).

## §P1.6 `0x23BF8` and `0x402FC`

**`0x23BF8`** (char 6's `0xBDAE4` entry, `0xBDAFC`), 8 blocks: `w = word [0x104AFC]`; `cmp byte [w+0xA83C4],0; jne`
else `xor al,al` and return (§P1.4); `call 0x1a570` with EAX = rec+0x51 (`0x23C1A`) and EDX = `0xE1A06` loaded before
the call (`0x23C1D`) and **preserved by `0x1A570`**; AL set: `cmp edi,[w*4+0xA83CC]` (edi = rec+0x18) `jge` keeps EDX,
else `0x23C55` loads `0xE19E6`; AL clear: `jle` keeps EDX, else `0xE19E6`. Signed compares. Then `0x2BC30(rec, EDX,
3.0)` and the slot 7/9/0, +0x0C = `0x23B68` (`0x23C72`), +0x57/+0x18/+0x1C/+0x14 = 0, +0x42 |= 8 (`mov dh,[ebx+0x42];
or dh,8`), `mov al,1`. The image's tables: flag bytes `01 00 01 00 00 01 00 00` at `0xA83C4`, thresholds `0x2600`,
0, `0x6000`, 0, 0, `0x3100` at `0xA83CC`. Cases (11): stage 1 (flag 0, EAX 0; `e0`), stage `0x105` with its flag byte
(`0xFF` in the image) poked 0 (EAX `0x100`; `e1`), stage `0x205` with its flag byte (`0x12`) poked 0 (EAX `0x200`;
`e2`), both AL values with x below, equal to and above the threshold (AL set, stage 0 `0x2600`: `a0` `0x2000`, `a1`
`0x2600`, `a6` `0x2601`; AL clear, stage 2 `0x6000`: `a7` `0x5FFF`, `a3` `0x6000`, `a2` `0x6001`), and x =
`0xFFFFF000` against `0x3100` for both AL values (`a4`, `a5`: a signed compare; an unsigned one flips both).
Correction (Task 3 review, 2026-10-02): the first spec had no AL-set case above the threshold, so a port testing
`x != thr` there stayed VERIFIED; `a6` and the mutant `fighter_23bf8@ne` (caught by `a6` alone) close it.

**`0x402FC`** (char 0's `0xBDB00` entry), 1 block: `sub esp,0x18; mov eax,esp; call 0x339ac` (EDX = rec), `mov
word [ctx[0]*2+0x1080a0],0`, `0x3C4CC(rec, 0xE7C40, 3.0)`, the slot 9/7/2, +0x57 = 0, +0x0C = `0x401D4` (`0x4033A`),
+0x18 = +0x1C = 0, `mov al,1`. +0x14 and +0x42 untouched. Cases: rec+0x51 = 0 and 1 (the word index), the 3C4CC stub's
EAX 0 and `0x1234` (AL overwritten); +0x42 seeded with the sentinel `0x42` in both and asserted kept (Task 3 review: a
port that writes it mismatches).

## §P1.7 The slot +0x0C callbacks: dispatch and shape

Each is called every frame while the slot's +0x53 is 7 (`0x3531C` case 7 and `0x38434`, §P1.4) and switches on the
slot's +0x57 through a bounded jump table (`cmp r8,N; ja; and r,0xff; jmp cs:[r*4+T]`, which `switch_cases` follows:
no hand resolution). `0x15584`, `0x1579C` and `0x401D4` build the context with `0x33950(side)` (`mov edx,ebx; mov
eax,esp; call 0x33950`): ctx = side, other, slot(side), slot(other), rec(side), rec(other) at `[esp]..[esp+0x14]`; a
push before a call shifts those offsets by 4 (`[esp+0x18]` after `push 0x3f800000` is ctx[5]). The cases end the
finisher by setting the slot's +0x53 = 3, +0x52 = 9, `DS_000F0AFE` = 4 and `DS_001078FC` = 1.

## §P1.8 `0x15584`, `0x1579C`, `0x23D38`

**`0x15584`** (9 blocks; table `0x1556C`: `15677 155AB 155F7 1561D 15677 1565F`): case 1 `0x2BC30(ctx[5],
[0xC90F8 + 4*ctx[3].char], 2.0)`, ctx[3]+0x42 |= 4, voice `word [0xC75AA + 2*char]` (zero-extended), +0x57 += 1;
case 2 `0x2BC30(ctx[5], [0x9B054 + 4*char], 1.0)`; case 3 ctx[5]'s word +0x2C -= 0x40 and, when the zero-extended word
is at most 0x400 (`cmp eax,0x400; jg` on `and eax,0xffff`), = 0x400 and +0x57 += 1; then `[ctx[3]+4]`'s word +0x2C =
ctx[5]'s; case 5 the end stores; 0, 4 and above 5 nothing. EDX (rec) is not read (`mov edx,ebx` overwrites it).
Cases (10): every +0x57 from 0 to 6, word +0x2C `0x500` (stays above), `0x420` (clamps), `0x10` (wraps to `0xFFD0`, above:
a signed 16-bit compare would clamp), and (Task 4 review, `c9`) `0x440`, which the -0x40 takes to exactly `0x400`: the
boundary of `jg` against `jge`.

**`0x1579C`** (11 blocks; table `0x1578C`: `15900 157C3 1580F 15835`): cases 1 and 2 as `0x15584`'s; case 3: word
+0x2C -= 0x40; above 0x400 copy it to `[ctx[3]+4]`+0x2C (`0x158ED`, E2's untrusted entry) and return; else ctx[5]+0x29
bit 6 cleared and +0x34 = 0x80 when ctx[5]'s word +0x28 has bit 14, else set and 0xFF80; `0x2BC30(ctx[5], 0xE8C82,
3.0)` with **EBX = `0x1187FAE0` loaded before it** (`0x1589E`), which `0x2BC30` preserves, so `0x2A17C(ctx[5], 0x18,
0x1187FAE0)`; word +0x2C = 0x1000, +0x28 |= 0x80, voice `0xEA`, the end stores. Case 0 and above 3 nothing. The run
confirms the EBX reading: the original records `0x2A17C(0x10A400, 0x18, 0x1187FAE0)`. Cases (8): the seven of the first
spec plus (Task 4 review, `c7`) the word `0x440` -> exactly `0x400`: the full path, where a `jge` would copy and return.

**`0x23D38`** (24 blocks; table `0x23D20`: `23D58 23DB4 23DE4 23E14 23EBD 23EA5`; EAX = slot, EDX = rec, EBX not
read): case 0 by the record's word +0x28 bit 14: set, `F0AF0 + 0x3000 >= rec+0x18` returns, else rec+0x18 =
`F0AF0 - 0x3000`; clear, `F0AF0 - 0x3000 <= rec+0x18` returns, else rec+0x18 = `F0AF0 + 0x3000` (the raw's own
pairing); +0x57 += 1. Case 1: `|F0AF0 - rec+0x18| > 0x2000` returns (`neg` then `jg`: `0x80000000` stays negative and
passes), else `0x2BC30(rec, 0xE1BAE, 3.0)`, +0x57 += 1. Case 2: the same against `[slot+8]`'s +0x18 within 0x1000,
`0xE1BBC`. Case 3: within 0xB00 the record at slot+8 takes rec's x and y, words +0x34 = 0, +0x2C = 0xE00, +0x29 bit 6
from rec's +0x28 bit 14; `0x2BC30(rec, 0xE1BD2, 3.0)`, `0x2BC30([slot+8], 0xE1C0C, 3.0)`, +0x57 += 1, voice `0xD6`.
Case 5 the end stores; 4 and above 5 nothing. Cases (21): the first 14 reach every block; Task 4's review added seven
boundary and sign cases, `gE`..`gK`: case 0 with x at the bound (`0x13000` with bit 14 set returns, `0xD000` with it
clear returns; `gE`, `gF`), x `0xFFFFF000` (negative: bit set returns, bit clear moves to `0x13000`; `gG`, `gH`), case 1
with `F0AF0 - x = 0x80000000` (`neg` leaves it negative, `jg` passes: it moves; `gI`), word +0x28 = `0xBFFF` with x
`0xC000` (bit 14 clear: moves; `gJ`) and case 1 with a difference of `0x80000001` (`neg` makes it `0x7FFFFFFF`:
returns; `gK`).

## §P1.9 `0x23B68`, `0x401D4`, `0x38034`

**`0x23B68`** (5 blocks; EAX = slot, EDX = rec): only +0x57 == 1 acts (`test al,al; jbe`; `cmp al,1; jne`); `mov
eax,0x400000; cdq-by-hand (mov edx,eax; sar edx,0x1f); idiv ebx` with EBX = rec+0x30 >> 16 (`sar`): the word quotient
into rec+0x2C and `[slot+4]`+0x2C; with rec+0x1C zero: word rec+0x38 = 0 (before the call, `0x23BAC`), `0x2BC30(rec,
0xE87AC, 1.0)`, `0x2AE14(0xA83B0, rec+0x18, rec+0x30 >> 16, rec+0x1C, 0)` (the fields re-read after `0x2BC30`;
`mov ecx,eax` overwrites the rec pointer, unused after), voice `0x5C` with DL = 1, then +0x53 = 3, `DS_001078FC` =
DL (= 1: `0x2C3FC` preserves EDX), +0x52 = 9, `DS_000F0AFE` = AH = 4. The SPAWN stub's EAX is not read (`mov eax,0x5c`
at `0x23BD1`). Cases (6): +0x57 = 0, 2, 1 with rec+0x1C set, 1 with it clear, and divisors 3 and -3 (`0x400000/3 =
0x155555`, word `0x5555`; truncation toward zero, C's); Task 5's review added `q5`, the dword rec+0x30 = `0xFFFD8000`:
the raw divides by `sar ebx,0x10` = -3 and stores the word `0xAAAB` (a signed `/0x10000` would divide by -2 and store
`0x0000`). The row has 5/5 blocks.

**`0x401D4`** (14 blocks; EAX = slot, EDX = rec, EBX = side): `0x33950(side)`; +0x57 0: word rec+0x36 negative
(`cmp word [ecx+0x36],0; jge`) sets it 0xFFFF, +0x44 = 0x3C, +0x57 = 1. 1: `mov eax,[char*2 + 0xbd882]; sar eax,16`
(a signed word, the high half of a dword at a 2-byte stride) against ctx[2]+0x30 (`jle` returns), then ctx[4]'s word
+0x36 negative, then `0x188AC(ctx[0], rec+0x18, 0)`, `0x2BC30(ctx[4], 0xE876A, 3.0)`, rec's words +0x34/+0x36/+0x44 = 0,
**ctx[2]'s** +0x54 = 0 and +0x57 = 3 (not the slot argument's). 3: `0x38034(byte [0x1078FD])`, voice `0x46`, with byte
`DS_00105B3A` below 2 (`cmp eax,2; jge` on the zero-extended byte) `0x2AE14(0xBB0EC, rec+0x18, rec+0x30 >> 16, 0, 0)`,
voice `0x6C`, +0x53 = 3, +0x52 = 9, `DS_001078FC` = 1. 2 and above 3 nothing. Thresholds in the image (`0xBD882 +
2c`, >> 16): 0x1600, 0x1400, 0x1400, 0x1600, 0x1800, 0x1800, 0x1180 for c = 0..6. Cases: 9; the EDX record is
`E3_OUT`, not the side's record `E3_REC` (ctx[4]), so a port that confuses them differs. Case 1's store rec+0x36 = 0
(`0x4027E`) was unobservable while `t4` seeded the word with 0 (Task 5 review): `t4` now seeds `0x3636`.

**`0x38034`** (3 blocks; EAX = side): slot = `0x1077B0 + side*0x94` (`shl 3; add; shl 2; add` = `*0x25`, then `*4`);
`0x2BC30(rec, [0xBDD00 + 4*char], 1.0)`; flag = rec's word +0x28 bit 14 ? 0x4000 : 0; `0x2AE14([0xBDD1C + 4*char], 0,
0, 0, flag | (rec+0x56 word | 0x400))` with the char byte re-read after `0x2BC30` (`0x3809E`); `mov byte [eax+0x59],1`
on the spawned record (the SPAWN stub's EAX, varied per case: `E3_REC2`, `E3_OUT`).

## §P1.10 Decisions, named gaps and limits

**Decisions for the user (the plan's top section): D1 the batch-1 choice; D2 the span code; D3 when the callee rows
come.** D2's evidence: E2's D2 asks P to "verify the six dispatchers the table `0x80C8C` holds ... with the writers
allow-listed". Allow-listed means both sides run the writer; the port has no C function per dispatcher or writer
(`sprite.c` blits a span as one routine; E2 §E2.4), so a dispatcher row has nothing to call on the port side.
`0x57FFB` alone is 556 instructions, 178 blocks and 18 indirect table calls (scratch survey).

**Named gaps and limits of P1:**
- **`0x23B68`'s `idiv` by zero.** The raw faults (#DE) when `rec+0x30 >> 16` is 0 (`0x23B82..0x23B8D`: `mov
  ebx,[edx+0x30]; sar ebx,0x10; idiv ebx`, no handler on this path). The divisor is the signed word rec+0x32, the high
  half of the dword +0x30, which `actor_spawn` writes from its a3 (`actors.c`: `DSW(rec + 0x32) = (u16)a3`, a
  depth-like value). Checked: every store to a `+0x32` word in `port/src` (a grep) is `actor_spawn`'s a3, a copy of
  another record's +0x32 (`fight.c`; `fighter.c` `0x37FA1`), a +-0x20 step (`0x454E0`, `0x45522`) or a plain value
  (`0x45584`, `0x45F1A`); none stores a constant 0, so no raw path is known to reach it. This is a search of the
  ported stores only, not a proof over the unported ones. The port skips both +0x2C stores on a zero divisor and
  continues with the spawn and the end stores (`/* PORT: */` in the C; Task 6 changed it from storing 0, to match
  `0x407EC`, whose port, on the same `idiv` of `+0x32`, leaves +0x2C unchanged, §52-A). No case divides by zero: the
  emulator would stop with a fault and the row would be `NOT_EXERCISABLE`; the rows were re-verified after the change
  (the zero path lies outside every case).
- **`0x23BF8`'s main-path EAX.** The raw returns `0x2BC30`'s EAX with AL = 1; the port returns 1. The only caller reads
  EAX != 0, which AL = 1 settles; with mask `0xFFFFFFFF` (needed for the early path) the cases keep the `0x2BC30` stub's
  EAX at 0, a harness value: the upper 24 bits on that path are not claimed (the raw's EAX there is whatever `0x2BC30`
  leaves with AL forced to 1; `0x379C4` tests it against 0 only, so in play only AL = 1 matters).
- **Callee rows.** After P1 the counter reads `1/17 rows with callees closed (9 have none)`: the stubbed `0x2BC30`,
  `0x2C3FC`, `0x2AE14`, `0x188AC` and `0x2A17C` have no row; every P1 row's claim holds "given each stub behaves as
  declared" (E3 §E3.4) until C1 discharges them.
- **No capture reaches the batch.** Every driver's pinned `fn_resolve` miss set (`test_platform.c`) holds only
  `0x5D812` and `0x29D60`: no scripted run arms a finisher, so registering the twelve changes no oracle line (the
  prototype's and the replay's `make verify` agree). The finisher path in play (O4) is therefore claimed by
  differential verification only; its screen stays unseen until a capture (U9's pokes, say) reaches it. The final
  `make verify` agrees: no oracle line moved.
- **Who advances +0x57 from 0 in `0x15584`/`0x1579C`.** Their case 0 does nothing; the animation streams the entries
  start presumably carry the step (an animation-opcode target), not examined here.
- E3's limits stand: seeds are hand pokes; the memory at a call is mem[] only; the callee column is one level deep.

## §P1.11 Results and facts verified by running

The replay's per-task gates (`make diff-verify entry-triage` with scratch image paths; `PR_ORACLE_REQUIRED=1
./build/run_tests` "all checks passed" after each):

| after | diff-verify counter | entry-triage |
|---|---|---|
| `834b703` | `13/13 functions VERIFIED; 17/17 mutants detected; 1 named gaps; 0/5 rows with callees closed (8 have none)` | `targets 323 unported, 172 ported; supplement 131 (31 unported, 0 stale)`; voice `46 / 69 / 19` |
| Task 2 | `17/17 ...; 21/21 ...; 0/9 ... (8 have none)` | `319 / 176`; finishers `2 / 7`; stubs 79; voice `43 / 72 / 19` |
| Task 3 | `20/20 ...; 26/26 ...; 1/11 ... (9 have none)` (25/25 in the replay; +1 the fix-round mutant `fighter_23bf8@ne`) | `317 / 178`; finishers `0 / 9`; stubs 77; voice `43 / 72 / 19` |
| Task 4 | `23/23 ...; 34/34 ...; 1/14 ... (9 have none)` (28/28 in the replay; +1 `@ne`, +5 the review's boundary mutants) | unchanged (the three are outside E2's lists) |
| Task 5 | `26/26 ...; 39/39 ...; 1/17 ... (9 have none)` (31/31 in the replay; +1, +5, and +2 from Task 5's review: `@no36`, `@noneg`) | `317 / 178`; supplement `28` unported; voice `40 / 75 / 19` |

The Task 4 and Task 5 rows quoted by the plan are the replay's; on `reverse-p1` the review rounds added eight mutants
and cases (Task 3: `fighter_23bf8@ne`; Task 4's fold-in: `fighter_23d38@ge/@unsigned/@bit`, `fighter_15584@ge`,
`fighter_1579c@ge`; Task 5's fix: `fighter_401d4@no36`, `fighter_23d38@noneg`), the other figures unchanged. The final
counter is the Task 5 row: `26/26 functions VERIFIED; 39/39 mutants detected; 1 named gaps; 1/17 rows with callees
closed (9 have none)`.

The batch's rows (the self-check table): `fighter_1567c` 2 cases 1/1, `fighter_15908` 2 1/1, `fighter_23ec0` 2 1/1,
`fighter_45d14` 2 1/1, `fighter_actor_bit15_clear` 4 1/1, `fighter_23bf8` 11 8/8, `fighter_402fc` 2 1/1,
`fighter_15584` 10 9/9, `fighter_1579c` 8 11/11, `fighter_23d38` 21 24/24, `fighter_38034` 2 3/3, `fighter_23b68` 6
5/5, `fighter_401d4` 9 14/14 (cases, then blocks hit/total): all `VERIFIED`, no unhit block. `fighter_402fc` is the one closed row (`339AC allow
VERIFIED, 3C4CC stub VERIFIED`).

**What catches each mutant** (`test_each_p1_mutant_is_caught_by_what_it_breaks`): `fighter_1567c@mutant` (0x15908's
stream) `call #0`; `fighter_15908@mutant` (voice 0xAA) `call #1`; `fighter_23ec0@mutant` (the stores after the voice)
`call #1 memory`; `fighter_45d14@mutant` (frame 2.0) `call #0`; `fighter_actor_bit15_clear@mutant` (bit 14) `eax`;
`fighter_23bf8@mutant` (the two compares swapped) `call #1`; `fighter_23bf8@zero` (early return 0) `eax` (cases `e1`,
`e2`); `fighter_23bf8@ne` (the AL-set compare as `!=`) `call #1` (case `a6` alone);
`fighter_402fc@mutant` (the word store after the call) `call #0 memory`; `fighter_15584@mutant` (the voice by the own
character) `call #1`; `fighter_1579c@mutant` (handle 0x1F874610) `call #1`; `fighter_23d38@mutant` (the held record
started first) `call #0`, `call #1`; `fighter_38034@mutant` (no bit 10) `call #1`; `fighter_23b68@mutant` (x and y
swapped) `call #1`; `fighter_401d4@mutant` (`0x38034` after the first voice) `call #0`, `call #1`. Every row with a
callee has a mutant only the call list or the memory at a call catches.

The review rounds' mutants, each pinned to the cases that alone catch it (`test_each_p1_mutant_is_caught_by_what_it_breaks`):
`fighter_23d38@ge` (case 0 `jg`/`jl` for `jge`/`jle`) byte, cases `gE gF`; `fighter_23d38@unsigned` (cases 0 and 1
compared unsigned) byte and `call #0`, cases `gG gH gI`; `fighter_23d38@bit` (case 0 tests the whole word +0x28, not
bit 14) byte, case `gJ`; `fighter_23d38@noneg` (case 1 without the `neg` at `0x23DC1`) byte and `call #0`, case `gK`;
`fighter_15584@ge` (case 3 clamps below 0x400 only) byte, case `c9`; `fighter_1579c@ge` (case 3 copies at 0x400 too)
`call #0 #1 #2` and byte, case `c7`; `fighter_401d4@no36` (case 1 without the store rec+0x36 = 0 at `0x4027E`) byte,
case `t4` (sentinel `0x3636`); `fighter_23bf8@ne` `call #1`, case `a6`; `fighter_23bf8@zero` `eax`, cases `e1 e2`.

**The seams are needed** (replay): without `PR_SEAM_RET(0x1A570u, side)` the row reads `a0: call #0: original
0x1A570(0x0), port 0x2BC30(0x10A300, 0xE19E6, 0x40400000)`; without `0x2A17C`'s, `c4: call #1: original
0x2A17C(0x10A400, 0x18, 0x1187FAE0), port 0x2C3FC(0xEA)`; without `0x188AC`'s, `t4: call #0: original 0x188AC(0x0,
0x5678, 0x0), port 0x2BC30(0x10A300, 0xE876A, 0x40400000)`.

**The gate.** The prototype (every change at once): `make verify` with the parallel-safe overrides ran every step to
the last (14 min 29 s), the 45 oracle lines equal to `oracle-lines-base.txt` (the gameplay ratchets ran on the captures
present: gp-idle-loss N 2064 / 8320, gp-u5-charsel 516 / 1513, gp-u6-moves-b and gp-keys-fight), `symbols.h`
regenerated byte-identical (the scratch copy has no git, so `make`'s `git diff` step was replaced by `cmp` against
`834b703`'s file), the `make audio-render` WAV identical to `before-t2.wav`, `771 1203 64` / `731 731 100`. The replay's
final state (Tasks 2-5 applied with the plan's scripts, then made a scratch git repository so `make`'s `git diff` step
runs): `make verify` EXIT=0 (49 min 46 s on a host shared with other runs), the 45 oracle lines equal, the gameplay
ratchets as at the baseline (gp-idle-loss 2064 / 8320, gp-u5-charsel 516 / 1513, gp-u6-moves-b 1005 / 2262 / 2949,
gp-keys-fight 11), `symbols.h` clean, the WAV identical, the counter and triage lines of Task 5 above, `771 1203 64` /
`731 731 100`, and `pr_seam = ` assigned only in `port/tests/diff_runner.c` and `port/tests/test_platform.c`. The plan's
scripts are byte-identical to the ones the replay ran, except that the replay's `m3` wrote its image to a scratch path
instead of `/tmp/pr_p1_m3.bin`.

Other facts run: the image sha1 and size; `unicorn` 2.1.4 / `capstone` 5.0.7; the five references to `0x1078E8`; the
69 `0x1A570` call sites; the three jump tables' entries; the 27 after-table ends and their references; the 128
animation dwords and the 8 outside E2; `callee_clobbers` for `0x1A570 0x2A17C 0x188AC 0x38034` (`() ("edx",) ("edx",)
()`); the image's stage, threshold, stream and voice tables quoted above; every unit-test mutation of the plan.

**Closure (Task 6, on `7bd60ac`, the Task 5 head `f60281d` plus the one-line `0x23B68` change and the `0x379C4` comment):** `make verify` EXIT=0 (14 min), the 45 oracle lines equal to `oracle-lines-base.txt`, the `make audio-render` WAV equal to `before-t2.wav`, the gameplay ratchets as at the baseline (gp-idle-loss N 2064 / 8320, gp-u5-charsel 516 / 1513, gp-u6-moves-b 1005 / 2262 / 2949, gp-twop 612 / 1506 / 1506, gp-keys-fight 11), `diff-verify: 26/26 functions VERIFIED; 39/39 mutants detected; 1 named gaps; 1/17 rows with callees closed (9 have none)` (the plan expected `31/31`: the review rounds added eight mutants, see the table above), `entry-triage: targets 317 unported, 178 ported; supplement 131 (28 unported, 0 stale)`, voice `40 / 75 / 19`, `771 1203 64` / `731 731 100`, `pr_seam = ` assigned only in `port/tests/diff_runner.c` and `port/tests/test_platform.c`.
