# Reverse completion P8: the remaining entries and the triage (record)

**Scope.** Track P's eighth and last batch (roadmap row P8 of record `2026-10-02-reverse-p1-derivations.md`
§P1.3, as corrected by P2 §P2.11, P3 §P3.11 and P7 §P7.11: the 16 members
`0x10604 0x29CFC 0x1BDF4 0x19AD4 0x19DD5 0x26163 0x26226 0x34962 0x45444 0x49078 0x2EE3C 0x37E40 0x3A3FC 0x3A820 0x4AEC4 0x1D2D0`,
the triage of `0x2D3FC..0x2D48C` and the 15 voice sites with no body), under spec
`2026-09-30-reverse-completion-design.md` §4 track P and §6. Plan: `2026-10-04-reverse-p8-remaining-entries.md`.
Recipe: E3 record §E3.10; lessons: the P-track review checklist and P3/P7's fix rounds.

**Status of the numbers.** Measured by the planner on 2026-10-04 in the worktree `.worktrees/reverse-p8`
(branch `reverse-p8`, `main` `4bf2209` = C1+C2+P4+P5+P6+P7 merged). The prototype is the plan's
Tasks 2-5 applied in order; every output quoted below is from that tree, and the full `make verify`
with the parallel-safe overrides ran once on it (`EXIT=0`). The base counters were re-measured on the
reverted tree. **The image** is `build/diffrun --exe data/game/C/PRAGE.EXE --image-out FILE`: sha1
`ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's, E3's, P1-P7's). Every address and instruction below
is capstone 5.0.7 over that image (fixups applied); `unicorn` 2.1.4 runs the original side.

Base (re-measured on `4bf2209`): `make diff-verify` reads `190/190 functions VERIFIED; 546/546 mutants
detected; 1 named gaps; 68/156 rows with callees closed (34 have none)`; E2 `targets 234 unported, 261
ported; supplement 131 (3 unported, 0 stale); untrusted entries 30`; `voice sites outside Ghidra 134: 0
in unported code, 115 in ported code, 19 nowhere`; `771 1203 64` / `731 731 100`.

---

## §P8.1 The member list from the raw: the 16 and the triage

Every member was read from the image with capstone 5.0.7: its containing function (the nearest
preceding boundary), every rel32 anywhere in the code object (`0x10000..0x73B14`), every occurrence of
its address as a 4-byte value anywhere in the image, and — for the untrusted entries — the referencing
instruction's context. The table is the verdict; the sections that follow carry the derivation.

| member | class | evidence (raw) | verdict |
|---|---|---|---|
| `0x29CFC` | direct entry | `jmp 0x13DF0`; the `call` at `0x4255D` its only reference; `0x13DF0` is the ported `effects_clear` | **port** (§P8.3) |
| `0x3A820` | stored callback | the immediate at `0x3A91E` (`mov [eax+0x10],0x3A820` in `0x3A8E8`) its only reference | **port** (§P8.2) |
| `0x19AD4` | interior block | `jbe 0x19AD4` at `0x19C09` inside the ported `0x19B90` (its char-4 arm); no dword/rel32 elsewhere | not an entry |
| `0x19DD5` | interior block | `ja 0x19DD5` at `0x19D37` (the `>0xF` default arm of the ported `0x19D34`); `xor eax,eax; ret` | not an entry |
| `0x26163` | interior block | `je 0x26163` at `0x260CC` (state 2 of the ported `0x260BC`); the port's `flow_bonus_card_a_step` implements it | not an entry |
| `0x26226` | interior block | `je 0x26226` at `0x261A4` (state 2 of the ported `0x26194`); the port's `flow_bonus_card_b_step` implements it | not an entry |
| `0x34962` | interior block | the `ja 0x34962` at `0x3486D`/`0x348CB` (the char > 6 arm) and the table `0x3479C`'s char-1 entry (`0x347A0`); the port inlines it as `FIGHTER_E4290` | not an entry |
| `0x45444` | interior block | `ja 0x45444` at `0x452FC` (the `+0x57 > 4` default of the ported `0x452E4`); `mov eax,1; call 0x62003` (the runtime error path) | not an entry |
| `0x49078` | interior block | `jbe 0x49078` at `0x48FB7` (the phase-1 arm of the ported `0x48F98`, `actor_type_2d_update`) | not an entry |
| `0x2EE3C` | after-table end | after the 9-dword table `0x2EE18`; prologue `push ebx/ecx/edx`; **no rel32 and no dword equal to it anywhere in the image**; not in Ghidra | dead code, not ported |
| `0x37E40` | after-table end | after the 7-dword table `0x37E24` (every entry `0x37E70`, the degenerate default); the body is `0x2A17C(rec,0,0x105FEBC)`; **no reference anywhere** | dead code, not ported |
| `0x3A3FC` | after-table end | after the 4-dword table `0x3A3EC` (every entry `0x3A423`, a `ret`); the body ends `jmp 0x188DC`; **no reference anywhere** | dead code, not ported |
| `0x4AEC4` | after-table end | after the 6-dword table `0x4AEAC`; the switch returns 0x10/0x0E/0x15/0x0C/0x0A/0x0D by AL; **no reference anywhere** | dead code, not ported |
| `0x1D2D0` | data | a struct at `0x1D2C0..0x1D2EF`: strings `01-01-01`/`/000`/`f ` and pointers (`+4 = 0xA2EB4`, `+8 = 0x1D2C9`, `+0xC = 0x1D2C0`); stored to `0x10740C` by `0x2FA02` in `0x2F9CC`, read at `+4` (`0x2CACC`) and `+0xC` (`0x2F98C`); the bytes at `0x1D2D0` are not an entry | data, not ported |
| `0x2D3FC 0x2D414 0x2D444 0x2D45C 0x2D474 0x2D48C` | data | six embedded table bases used as addends: `add ebx,0x2D3FC` at `0x2DB6F`/`0x2DBE3`/`0x2DCD4` (stride 8), `add ebx,0x2D414` at `0x2E94E` (stride 0x10), the base at `0x2D519`/`0x2E0DD`/`0x2E19A`/`0x2E050`, `mov ecx,0x2D45C` at `0x2DF98`, `mov edx,0x2D474` at `0x2DEA5`, `mov [esp+8],0x2D48C` at `0x2D4F7`; the bytes are word tables | data, not ported |
| `0x1BDF4` | host-owned | the AIL 60 Hz timer callback (`push 0x1BDF4` at `0x1CFED`, register `0x5DA12`); its callees `0x1BBAC`/`0x2D62C` are host-owned (`tools/port_classification.txt`); the port models the tick in `config.c`/`host.c` | not ported (§P8.6) |
| `0x10604` | deferred | the 250 Hz AIL timer callback (`push 0x10604` at `0x10648` in the deferred movie-audio starter `0x10610`); drives `0x102B8` (`2026-09-24-demo-pose-derivations.md`) | not ported (§P8.6) |

**The reach scans.** Every rel32 in `0x10000..0x73B14` (all `E8`/`E9`/`0F 8x` forms) and every 4-byte
occurrence in the whole image were scanned for each of the 16: `0x19AD4 0x19DD5 0x26163 0x26226 0x45444
0x49078 0x2EE3C 0x37E40 0x3A3FC 0x4AEC4` have **no rel32 and no dword occurrence at all**; `0x19AD4`'s
only rel32 is the untrusted `jbe` at `0x19C09` (and `0x19DD5`'s the `ja` at `0x19D37`), i.e. the interior
verdicts for those two come from the *sites*, not from occurrences of their own addresses. `0x29CFC`
has one `call` (`0x4255D`); `0x3A820` one dword immediate (`0x3A921`); `0x1BDF4` two immediates
(`0x1BF6E`, `0x1CFEE`); `0x10604` one immediate (`0x10648`); `0x34962` one dword (the jump-table entry
at `0x347A0`); `0x1D2D0` one immediate (`0x2FA07`); the six addends their `add`/`mov` sites above.
`0x10604`'s second apparent occurrence (`0xAA02D`) is misaligned (the bytes are the tail of the
adjacent `0x010604xx` data pointers), not a reference.

## §P8.2 Task 2: `0x3A820` (the 0x3A8E8 pose family's per-frame handler)

**The twin.** `0x3A820` is instruction-for-instruction `0x3A43C` with four substitutions:
the stream table `0xC8FE0` -> `0xC9058`; the B word `0x107D10` -> `0x107CFC` and the A word
`0x107D14` -> `0x107CF8` (both read as the high halves of `dword [side*2 + 0x107CFA]` /
`[side*2 + 0x107CF6]`, then `sar 0x10`); the jump table `0x3A42C` -> `0x3A810`; and the end store
`+0x90 = 1` -> `+0x90 = 4` (`0x3A8DA`). The tables are degenerate the same way: `0x3A42C`'s four
entries are all `0x3A4F2` and `0x3A810`'s four all `0x3A8D6`, so `+0x90` in 1..4 skips the snap and any
other value (`dec al > 3`) takes it. The stream table's seven dwords are `0xE7398 0xE401E 0xED104
0xD26F0 0xEAC4A 0xD4306 0xE0C30`. The function is `0x3531C` case 10's `slot+0x10` handler; `0x3A8E8`
stores the address at `0x3A91E` (`mov dword [eax+0x10], 0x3A820`, the dword at `0x3A921` its only
reference), and its port (`fighter_pose_3a8e8`, already ported) stores the raw address, which
`fn_resolve` must find — before this batch `0x3A820` was unregistered (a latent miss for any driver
that reaches the fourth pose family).

**Registers and mask.** `0x3531C` case 10 calls it with EAX = the slot, EDX = the slot's record,
EBX = the side (`mov eax,ecx; call [ecx+0x10]` at `0x354E0`, P3 §P3.2); the function's first
instructions `mov edx,ebx; mov eax,esp; call 0x33A10` overwrite EDX and EAX, so only EBX (the side) is
read; nothing reads its EAX: mask 0.

**Callee declarations** (args in the port's C order; clobbers = `E.callee_clobbers`; the three stubs'
own rows are VERIFIED in the full table):

| `E.Call` | callee (port C) | args | clobbers | mode |
|---|---|---|---|---|
| `0x33A10` | `fighter_ctx_swap(ctx, side)` | allow | - | allow (runs both sides; C1's row VERIFIED) |
| `2BC30` (E3's `ANIM_BEGIN`) | `actors_anim_begin(rec, stream, frame)` | `eax edx s0`, `pop 4` | `edx` | stub |
| `ANCHOR` `0x188AC` | `hit_anchor_set(side, x, y)` | `eax edx ebx` | `edx` | stub |
| `ANCHORX` `0x188DC` | `hit_anchor_x(side, x)` | `eax edx` | `edx` | stub |

**Cases (6).** `p0` (phase 0, side 0: only `+0x58 = 1`), `p2` (phase 2, side 1: returns), `s0`
(phase 1, side 0, char 0, `+0x90 = 0`: the snap path — `B[0] = 3`, `A[0] = 0x4321`, so `0x188DC`
gets `(0, 0x4321)`), `s1` (phase 1, side 1, char 1, `+0x90 = 4`: the table arm, no snap), `q0`
(`B[0] = 0`), `q5` (`B[0] = 5`). Every case seeds EDX = `0xDEAD` (the raw never reads it), the records'
`+0x18`/`+0x1C` and the slots' `+0x58`/`+0x7A`/`+0x90` neighbours, and the other slot's fields. The row
hits all 12 blocks of the 12-leader scan and the stream argument at call #0 pins `0xC9058[char]`
(`0xE7398`/`0xE401E`).

**Mutants (5), all detected** (the exact catching kinds are `P8_KINDS` in
`tools/tests/test_diff_verify.py`): `@side` (the side from EDX — caught in every case: the port writes
the wrong slot and the calls disappear), `@stream` (`0xC8FE0` — call #0 in `s0`/`s1`/`q0`/`q5`),
`@globs` (B/A swapped — call #2, and the extra/missing call when the tested word is 0/5), `@end`
(`+0x90 = 1` — byte in `s0`/`s1`/`q0`/`q5`), `@order` (`+0x58 = 2` before `0x2BC30` — call #0/#1
memory).

**The unit check** (`test_p8_3a820` in `test_fight.c`, through `u6b_run`): the phase 0/2 seeds, the
char-0 stream (`0xE7398`, frame 3.0, `+0x90 = 4`, the anchor y = 0, the other record's `+0x1C`
untouched), the snap (B = 3/A = 0x4321/`+0x90 = 0`; the record's `+0x18` takes `0x4321 - 0x1000` with
`0x1077A8[0] = 0` and `0x100AF0[0] = s0+0x20` making the record-x calls inert), the table arm
(`+0x90 = 4`), B = 5, B[1] = 3 not B[0], the side-1 mirror (record r1, `0xC9058[1] = 0xE401E`, the other
slot untouched), and the case-10 wiring through `fighter_state_3531c`. The seeds differ from every
post-condition (the phase gate, the stream, `+0x90`, the globs, the neighbours).

**Raw-over-plan corrections during prototyping:** the expected first stream word of `FIGHT_ACTORS` is
`0x1099` (not `0x1075`, the 0x3A43C family's value), and the side-1 block needs its own `+0x58 = 1`
phase seed (the first prototype read phase 0 there and the side-1 asserts failed). Both are in the
plan's scripts as measured.

## §P8.3 Task 3: `0x29CFC` (the effects-clear tail), the seam and the row

**The function.** `0x29CFC` is one instruction, `jmp 0x13DF0`, and `0x13DF0` is the ported
`effects_clear` (`effects.c`, header `/* 0x13DF0. */`). Its only reference is the `call` at `0x4255D`
in `game_mode_13_step`'s case 2 (`0x424E8`), whose port already called `effects_clear()` inline with the
comment `0x29CFC -> 0x13DF0`; P8 adds the wrapper `effects_29cfc` (`void`, `effects_clear()` as its
body), changes the call site to it, and registers `0x29CFC` so the call graph and `fn_resolve` are
faithful.

**The seam, and why it is safe.** The differential row's original side runs from `0x29CFC`; a tail
`jmp` into `0x13DF0` would otherwise run the whole effects subsystem on the original side while the
port runs its own `effects_clear` — a transitive comparison, not the row's narrow claim. Instead the
row declares `E.Call(0x13DF0)` stub and `effects_clear` opens with `PR_SEAM0(0x13DF0u)`; the harness's
hook returns 1 (stub) only when the address is in the case's call set (`mem.h`), so the seam is inert
in every existing row. Checked: no spec's call set holds `0x13DF0` (the self-check's exact-set
assertions and the stub table pass unchanged), and `E.callee_clobbers(image, 0x13DF0)` is `()` (the
stub table gains `0x13DF0: ()`). The row: entry `0x29CFC`, one case `c0` (no registers), mask 0
(`0x42562` overwrites EAX), one leader hit, `13DF0 stub unverified` (open until a C2b row).

**The mutant** `@mutant` (the clear skipped) is caught by `call #0` (the original records the arrival,
the port none). The unit check `test_p8_29cfc` (`test_game.c`) seeds the count bytes `0x5A` and the
pool head both built-empty (`0xFCCE0` self-linked: the clear zeroes `0x9AF3D`/`0x9AF3C`) and zero (the
guard returns, the `0x5A`s survive), and asserts the registration.

## §P8.4 Task 4: the triage verdicts

**The seven untrusted entries are blocks of ported functions** — each is reached by one rel32 from
inside a function the port already implements, and the port's C for that function contains the arm:

| member | site | the ported body |
|---|---|---|
| `0x19AD4` | `jbe` at `0x19C09` | `0x19B90` (`debris_update`; the port's arm comment `0xE8C38 (== 4, 0x19AD4)`) |
| `0x19DD5` | `ja` at `0x19D37` | `0x19D34` (svcmenu's getter; the port's `>0xF` default) |
| `0x26163` | `je` at `0x260CC` | `0x260BC` (`flow_bonus_card_a_step`; the port's comment `/* 0x26163 */`) |
| `0x26226` | `je` at `0x261A4` | `0x26194` (`flow_bonus_card_b_step`; `/* 0x26226 */`) |
| `0x34962` | `ja` at `0x3486D`/`0x348CB`, table `0x3479C` at `0x347A0` | the `0x348xx` health sync (`FIGHTER_E4290` for char > 6/char 1) |
| `0x45444` | `ja` at `0x452FC` | `0x452E4` (`fighter_452e4`; the `+0x57 > 4` default) |
| `0x49078` | `jbe` at `0x48FB7` | `0x48F98` (`actor_type_2d_update`; the phase-1 arm) |

They are not entries: none has a prologue the rel32 enters (each is jumped to with the caller's frame
and jumps/pops back through the caller's epilogue), and the E2 tool's "ported" column is per-address,
so it keeps reading `no`; the resolutions section says what they are instead.

**The four after-table ends are dead code.** `0x2EE3C` (after `0x2EE18`, 9), `0x37E40` (after
`0x37E24`, 7), `0x3A3FC` (after `0x3A3EC`, 4), `0x4AEC4` (after `0x4AEAC`, 6): each is a real
function-shaped body (prologue or a pure switch), but **no rel32 anywhere in the code object and no
4-byte occurrence anywhere in the image equals its entry** (and none is a Ghidra function). `0x37E40`
is `0x29C78`'s (P7's) degenerate twin (`0x2A17C(rec,0,0x105FEBC)` through an all-default table);
`0x4AEC4` is the AL-returning twin of `0x4B03C`'s inline type map; `0x2EE3C` maps a code 0x48..0x50
to OR-masks; `0x3A3FC` is a `0x188DC` anchor with an all-`ret` table. None is ported; a caller outside
the image (none exists in the DOS4GW load path the port models) is the named residual doubt.

**The two data groups.** `0x1D2D0` is the base of a struct embedded in the code object: `+4 =
0x0A2EB4`, `+8 = 0x1D2C9` (mid-string), `+0xC = 0x1D2C0`, and the bytes `0x1D2C0..` are the strings
`01-01-01`, `/000`, `f ` and zeros. `0x2F9CC` stores the base into `0x10740C` (`mov dword
[0x10740C],0x1D2D0` at `0x2FA02`); the readers load it and pass `[eax+4]` to `0x33578` (`0x2CACC`
`jmp`) and `[eax+0xC]` to `0x2F41C` (`0x2F98C`/`0x2F98F` `call`), i.e. EAX is a data pointer argument,
not a call target. The six `0x2D3xx` values are used only in `lea`/`add`/`mov` arithmetic as table
bases; read as 16-bit words at `+0`, `+2`, `+4`, `+8` with shifts into indices, they are word tables
(measured bytes: `0x2D3FC` starts `0a00 0a00 0400 0800 ...`). Both groups are **data**; E2's §E2.11
"8 unexamined immediates" therefore resolves: `3A820` ported, `1D2D0` data, the six data.

## §P8.5 Task 5: the 15 voice sites with no body

E2's §E2.7 lists 134 rel32 `call`/`jmp` sites to `0x2C3FC` outside the Ghidra functions; 19 had no
owning body, and P1 resolved four (they lie in P1's after-table finishers). The remaining 15 (each
site's nearest after-`ret` start in E2's table) resolve:

| site | body | verdict (evidence) |
|---|---|---|
| `0x228EF` | `0x2284C` (the port's P2 callback) | ported body: `fighter.c` `0x2284C..0x2285E` with the voice comment `0x228EA/0x228EF 0x2C3FC` |
| `0x2292B` | `0x228F9` | ported body: `0x22938`'s callback, comments `0x22926/0x2292B` |
| `0x39EDA` | `0x39E0A` | ported body: `fighter.c` voice `0x6C` at `0x39ED5/0x39EDA` |
| `0x45F8C` | `0x45F7D` | ported body: `fighter.c` voices `0xAB`/`0xAC` at `0x45E59`/`0x45F81` (the `jmp` at `0x45E5E`) |
| `0x48518`, `0x48548` | `0x484F0`, `0x48521` | ported body: `fighter_4844c`'s state-2 arm (`0x484F0..0x4855A`, P3) |
| `0x4B0B4`, `0x4B0BE` | `0x4B024` (the jump table) | ported body: `fighter_4b03c` (P7); the sites are inside its 88 instructions |
| `0x11A3D` | `0x11A30` | dead: the body has **no dword and no rel32 anywhere**; it is a duplicate of `game_state_step`'s case 5 (the live body `0x11DE8` has the table dword at `0x11CF0`, `0x11A30` none) |
| `0x11C38` | `0x11BF8` | dead: no reference anywhere; a duplicate of the case-7 exit (`0x11BCC` is the live one, after the tail `jmp 0x2C3FC` at `0x11BF0`) |
| `0x1599B` | `0x15960` | dead: no reference anywhere; a finisher-style hook (`+0x57 = 3`, voice `0xB1`); the live copies are P1's `0x14807`/`0x14B84`/`0x15A22` |
| `0x295FD` | `0x295C0` | dead: no reference anywhere; a round-start handler (`0x104B1E++`, `0x1078FA = 0`, voice `0x25`, `0x104AE4 = 0x5D812`) |
| `0x3D730` | `0x3D6E0` | dead: no reference anywhere; a `0x3D698`-style `+0x4B`/`+0x57` handler with the release stream `0xD4B50` and voice `0x4F` |
| `0x41880` | `0x41878` | dead: no reference anywhere; a hook storer (`0x418D5` stores `0x259CC` into `DS_00104AE4`, mode 0x17) |
| `0x475D9` | `0x475C0` | dead: no reference anywhere; a voice-table scan (`movsx word [edx]` vs ESI, voice at `[edx+2]`, EDX += 4) |

The 7 dead sites are named gaps: the port plays no voice there because no reachable path enters those
bodies (the residual doubt is a computed entry outside the image's literal scans, none known).

## §P8.6 Task 5: the host-owned and deferred resolutions

- **`0x1BDF4` is host-owned.** The AIL 60 Hz timer callback: `0x1CFED push 0x1bdf4; 0x1CFF2
  AIL_register_timer (0x5DA12); 0x1CFFA push 0x3c; 0x1CFFF 0x5DA87; 0x1D008 0x5DAA6` (host.c's
  comment). Its body: with `DSB(0x104B22) != 1`, `DS_00101508++`, `DS_00101500++` (`0x1BE02..0x1BE16`),
  `0x1BBAC()` (`0x1BE1C`), `DS_000EF6DE++` (`0x1BE21`), `0x2D62C()` (`0x1BE28`). `0x1BBAC` is
  host-owned (`tools/port_classification.txt`, record §50-C; it contains `in` at `0x1B899`, E3's
  NAMED_GAP row) and `0x2D62C` is host-owned (`inc dword [0x105D88]; ret`, record §K9.5; the port's
  `host.c` `g_tick` models it). The port already models the ISR's stores in `config.c`'s retrace block
  (`0x2EAF5`), and the ledger (`2026-09-29-all-gaps-ledger.md` :558) records "the ISR `0x1BDF4` is
  host-owned". P8 adds the classification row; a differential row would have to seam two host
  infrastructure functions the harness stubs anyway, i.e. verify nothing the model relies on.
- **`0x10604` is deferred.** `mov eax,[0x81E10]; inc dword [0x81E10]; ret` — the 250 Hz AIL timer
  callback (`push 0x10604` at `0x10648`; `AIL_register_timer`/`set_frequency`/`start` at `0x5DA12`/
  `0x5DA87`/`0x5DAA6`), registered by `0x10610` (103 bytes, `deferred record-§50-E`), which the
  movie-audio unit §49-W.2 defers ("the callback drives `0x102B8`",
  `2026-09-24-demo-pose-derivations.md`). Its counter `0x81E10` is read only by the same cluster
  (`0x100EA`, `0x10361`, the `<< 2` helper `0x10679`), all part of the deferred unit. P8 adds the
  classification row; it is not ported.

## §P8.7 Decisions, named gaps and limits

**No decision is left to the user.** The two ports follow the raw; the 14 non-ports are derived in
§P8.1/§P8.4-§P8.6; the E2 table's resolutions section and the classification rows record them.

Named gaps and limits:

- **Dead code is not ported** (§P8.4/§P8.5): the four after-table ends and the seven voice-site bodies
  have no literal reference in the image; the residual doubt (a computed entry, a caller outside the
  image) is named, not claimed.
- **The `0x29CFC` row's callee `0x13DF0`** is a stub without its own row (C2b's list, record §P8.3);
  `effects_clear`'s seam is inert outside the new row.
- **`0x1BDF4`/`0x10604`** are not exercised by any row (host-owned/deferred, §P8.6); their effects in
  play are the host's model, not the original's instructions.
- **The E2 table's untrusted/voice rows keep their mechanical `ported no`** for block addresses (the
  tool's rule); the resolutions section says what they are. E2's class rules and counts otherwise
  stay frozen as at `4bf2209` except the one ported target (`29CFC`).
- **Outside P8:** the `title_pin` unittest failure on this tree is pre-existing (P2 §P2.13) and outside
  `make verify`; a closeout item. C2's re-review batch (C2b) does not exist yet; the stubs this batch
  leaves (`0x13DF0`) join its list.
- E3's, P1's, P2's, P3's and P7's limits stand: seeds are hand pokes; the memory at a call is mem[]
  only; the callee column is one level deep.

## §P8.8 Results

The per-commit gates: the task's unit suite (`PR_ORACLE_REQUIRED=1 ./build/run_tests`, `all checks
passed` after every task), the harness self-check, and `make entry-triage` with the regenerated table
after Tasks 3-4. Counters (base re-measured; the prototype's are the planner's final state):

| after | diff-verify counter | E2 |
|---|---|---|
| base `4bf2209` | `190/190 functions VERIFIED; 546/546 mutants detected; 1 named gaps; 68/156 rows with callees closed (34 have none)` | `targets 234 unported, 261 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30` |
| Task 2 (0x3A820) | `191/191; 551/551; 1; 69/157 (34 have none)` | unchanged |
| Task 3 (0x29CFC + seam) | `192/192; 552/552; 1; 69/158 (34 have none)` | `233 unported, 262 ported; supplement 131 (3); untrusted 30` |
| Task 4 (resolutions section) | unchanged | unchanged (table text only) |
| Task 5 (classification rows) | unchanged | unchanged |

The two rows, as the final table prints them (cases, blocks hit/total, callees):

```
fighter_pose_3a820  6 12/12  188AC stub VERIFIED, 188DC stub VERIFIED, 2BC30 stub VERIFIED, 33A10 allow VERIFIED
effects_29cfc       1  1/1   13DF0 stub unverified
```

The 6 mutants and what alone catches each: `P8_KINDS` in `tools/tests/test_diff_verify.py` (measured:
`@side` `{byte, call #0, call #1, call #2}`, `@stream` `{call #0}`, `@globs` `{call #2}`, `@end`
`{byte}`, `@order` `{call #0 memory, call #1 memory}`, `effects_29cfc@mutant` `{call #0}`). Both rows
have every block hit and no outside-image read. `PR_ORACLE_REQUIRED=1 ./build/run_tests`: **all checks
passed** (`test_p8_3a820`, `test_p8_29cfc` included).

**The full gate (the planner's, on the final prototype state, `T=p8` with the parallel-safe
overrides):** `EXIT=0`; the 45 oracle lines equal to
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt` (`ORACLES-EQUAL`); the
`make audio-render` WAV byte-identical to `before-t2.wav` (`WAV-SAME`); `symbols.h` regenerated
byte-identically (1304 globals, 1206 functions); every `run_tests` pass `all checks passed`; the
Python diff-verify suite 103 tests OK and the E2 suite 44; and

```
diff-verify: 192/192 functions VERIFIED; 552/552 mutants detected; 1 named gaps; 69/158 rows with callees closed (34 have none).
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
```

with every gp ratchet at its pin unchanged from the base: gp-idle-loss 2064 / 8320, gp-u5-charsel
516 / 1513, gp-u6-moves-b 1005 / 2262 / moves 2949, gp-keys-fight 11, gp-twop 612 / 1506 / 1506, the
seven U8 scenarios (RA 1072 / 2274, LT 1076 / 2338, RT 1098 / 2402, TW 1107 / 2466, HC 1022 / 2274,
EN 278 / 1174, AS 1087 / 2018), gp-u9-win 346 / 3503 / path 8 / win 3503, gp-u10-ending
331 / 9954 / path 30 / win 9954. `python3 tools/port_progress.py` stays `771 1203 64` /
`731 731 100` (neither new function is a committed Ghidra `FN_`), so README does not move.

**Deviations from the plan's first draft (raw wins).** (1) The `0x3A820` unit check's stream word is
`0x1099`, not `0x1075` (the 0x3A43C family's); measured. (2) The side-1 block needs its own phase
seed. (3) The planner's first spec case `p0` carried 17 pokes; the harness caps a case at 16
(`diff_runner.c` `c.npoke < 16`), so two redundant record seeds were dropped. All three are in the
plan as measured.

## §P8.9 The roadmap after P8

P8 is the last porting batch: the P-track roadmap's rows P1-P8 are done (two rows are verified by E
without a C port: the span dispatchers under decision D2, and C2's callee rows). What remains outside
track P: C2b (the callee rows the P batches left open, `0x13DF0` among them), the closeout items
(`docs/PROGRESS.md` O10, the README title, the `title_pin` unittest), and the pre-existing named gaps
the records carry.
