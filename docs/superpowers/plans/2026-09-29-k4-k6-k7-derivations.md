# Clusters K4 (fx gate), K6 (voice wrappers) and K7 (sample start) — raw-byte derivation (Task 3d of `2026-09-29-all-gaps.md`)

**Scope.** Ledger `2026-09-29-all-gaps-ledger.md` §F rows 8 (K4 FX-GATE:
`0x4A868`), 9 (K6 VOICE-WRAP: `0x4F714`, `0x4F728`) and 10 (K7 AUDIO-SMP:
`0x1CB18` plus `0x1CC28`'s slot choice). Sections are `§K4.n`, `§K6.n` and
`§K7.n`; the port headers cite them by that name.

**Tooling.** The Ghidra MCP bridge was not reachable (`ToolSearch` finds no
Ghidra tool). Every byte was read from the Python mirror of `mem_load_le` +
`mem_load_le_fixups` (`port/src/mem.c`), so each dword has the LE fixups
applied, and disassembled with capstone (32-bit). Caller scans cover the code
object `0x10000..0x73B14`: every `E8`/`E9` rel32 and `0F 8x` jcc whose target
is the entry, plus every absolute dword equal to it.

Raw caller scan (all five targets):

| target | rel32 sites | absolute dwords |
|---|---|---|
| `0x4A868` | `0x4A361` call, `0x4BFDE` call, `0x4DF8E` call, `0x4DFFA` call, `0x4E066` call, `0x4E0D0` call | none |
| `0x4F714` | `0x25C09` call | none |
| `0x4F728` | `0x27E4B` call | none |
| `0x1CB18` | `0x1CF26` call | none |
| `0x1CC28` | `0x2C4B6`, `0x2C4F9`, `0x2C505`, `0x2C52E`, `0x2C53A`, `0x2C563`, `0x2C56F`, `0x2C8D8` (all calls, all in `0x2C3FC`) | none |

This matches the ledger's §B.1 rows site for site.

---

## K4 — `0x4A868`, the effect entry's proximity gate

### §K4.1 The body

Raw (63 B, `0x4A868..0x4A8A6`):

```
4a868: push ebx / push ecx / push edx
4a86b: mov  edx, eax                 ; EDX = entry
4a86d: mov  eax, [eax+8]             ; R = entry+8
4a870: call 0x2be00                  ; EAX = [DS_001014EC + R.w56*0x20 + 4]
4a875: mov  ecx, eax                 ; ECX = x
4a877: mov  eax, [edx+8]
4a87a: mov  eax, [eax+0x32]          ; the DWORD at R+0x32
4a87d: sar  eax, 0x10                ; = the signed word R+0x34
4a880: add  eax, eax                 ; reach = 2 * that
4a882: test eax, eax / jge 4a88c
4a886: mov  ebx, eax / neg ebx / jmp 4a88e
4a88c: mov  ebx, eax                 ; EBX = |reach|
4a88e: mov  eax, ecx
4a890: sub  eax, [edx+0x14]          ; x - entry+0x14
4a893: test eax, eax / jge 4a899 / neg eax   ; |d|
4a899: cmp  eax, ebx
4a89b: setle al                      ; signed, inclusive
4a89e: and  eax, 0xff                ; AL zero-extended
4a8a3: pop edx / pop ecx / pop ebx / ret
```

`0x2BE00` is the port's `fight_2be00` (`push edx; mov ax,[eax+0x56];
... mov eax,[edx+eax+4]` over `DS_001014EC`, stride `0x20`).

- The ledger's form `|0x2BE00(rec) - entry+0x14| <= 2*|(rec+0x32)>>16|` is
  exact. The shift is arithmetic (`sar`) on the dword at `+0x32`, so the
  reach is the signed `+0x34` word; the low word `+0x32` is shifted out.
- Both absolute values use 32-bit `neg`, so `0x80000000` stays negative;
  the port keeps that with u32 negation and signed compares. The reach is at
  most `2 * 0x8000`, so its `add` never overflows.
- The comparison is `setle` (signed `<=`), so a distance equal to the reach
  passes. The result is exactly 0 or 1.

Port: `fight_4a868` (`fight.c`, exported in `fight.h`), right after
`fight_2be1c` so that the static `fight_2be00` is in scope.

Test values (`check_fx_gate` in `test_fight.c`, on `mz_seed`'s entry `E` /
actor `R`, `E+0x14 = 0x18000`, `R+0x32 = 0x0600` (the dword's low word)):

| `R+0x34` | x | want | what it pins |
|---|---|---|---|
| `0x0100` | `0x18200` | 1 | the edge is inclusive; the reach is doubled |
| `0x0100` | `0x18201` | 0 | one past; the dword's high half, not the word `+0x32` (`0x600*2` would pass) |
| `0x0100` | `0x17E00` | 1 | the distance's absolute value |
| `0x0100` | `0x17DFF` | 0 | without the distance's `neg`, `-0x201 <= 0x200` would pass |
| `0xFF00` | `0x18200` | 1 | the reach's absolute value (`-0x200`) |
| `0xFF00` | `0x18201` | 0 | the arithmetic shift (a logical one gives `0x1FE00`) |
| `0x0100` | `0x18180` | 1 | inside |
| `0x0100` | `0x18000` | 1 | zero distance |

### §K4.2 Site `0x4BFDE` in `0x4BF18` (`fight_4bf18`) — wired

Raw (case 1 of the table at `0x4BEF4`, entry `0x4BFCD`):

```
4bfcd: mov ax,[ecx+0x1c] / xor ah,ah / and al,0x20 / and eax,0xffff
4bfda: je  0x4c039                  ; bit 5 clear: the arrival test (already ported)
4bfdc: mov eax, ecx
4bfde: call 0x4a868
4bfe3: test eax,eax / je 0x4c1f3     ; shut: the shared tail
4bfeb: mov eax,0xc8 / call 0x2c3fc   ; voice 200
4bff5: R+0x38 = 0 (word)
4bffe: R+0x34 = 0 (word)
4c007: R+0x36 = 0 (word)
4c013: entry+0x1E = 8
4c017: R+0x55 = 1
4c01b: xor eax,eax / mov ax,di       ; the index (u16)
4c020: mov edx,[eax*4+0xc958c]
4c02a: push 0x40400000 / call 0x2bc30   ; actors_anim_begin(R, 0xC958C[index], 3.0)
4c034: jmp 0x4c1f3                   ; the shared tail
```

All the callees are ported (`actors_anim_begin` = `0x2BC30`). The voice
`0x2C3FC(0xC8)` is a case-2 record (`DS_000BBDC8[0xC8]`: case 2, handle
`0x0A804FBC`, loop byte 0): it would resolve a sound bank through `0x1B544`,
so it keeps this file's `PORT: ... not wired (record §45-A)` convention (K12
owns the voice sites). The port's shared tail reads the type after the
switch, so type 8 with bit 5 set takes `0x4C60C` exactly as the raw's
`jmp 0x4C1F3` does.

Wired: the port's case 1 is now `if (bit 5 clear) {arrival} else if
(fight_4a868(entry)) {body}`. The old `PORT:` note ("treated as always
false") is gone.

Test values (`check_fx_gate`): `mz_seed(1, 0x20)` plus `k4_seed`
(`DS_00108868` = `MZ_R2`, whose `+0x36` is 0, so the preamble idles;
`DS_001088A0 = 2`, so the post-loop counts to 1 and returns;
`DS_00108898 = 0x5555` proves the tail's `0x4C60C` hit test misses). Gate
open (x = `0x18200`): type 8, `R+0x38/+0x34/+0x36` = 0 (sentinels `0x3333`,
`0x0100`, `0x2222`), `R+0x55 = 1` (sentinel `0x77`), `R+8 = MZ_ST(9)` (the
scratch stream `0xC958C[3]` points at), `R+0x24 = 0x40400000`. Gate shut
(x = `0x18201`): all the sentinels survive, type 1, `R+8 = 0x0BAD`.

### §K4.3 Sites `0x4DF8E`/`0x4DFFA`/`0x4E066`/`0x4E0D0` in `0x4DEF4` (`fight_effects_idle_pass`) — wired

The jump table at `0x4DEE0` is `{0x4E108, 0x4DF8C, 0x4DFF8, 0x4E064,
0x4E0CE}` (states 0..4; `cmp al,4; ja 0x4E108`). Each state body:

| state | gate | `+0x34` | compare | low (`<`) table | high (`>=`) table | `+0x55` | frame |
|---|---|---|---|---|---|---|---|
| 1 | `0x4DF8E` | `0x4DFA5` | `0x4DFBD` `cmp ecx,edi; jge` | `0xC9664` | `0xC955C` | 1 on high (`0x4DFDA`) | 3.0 |
| 2 | `0x4DFFA` | `0x4E011` | `0x4E029` | `0xC964C` | `0xC9574` | 1 on high (`0x4E046`) | 3.0 |
| 3 | `0x4E066` | `0x4E076` | `0x4E096` | `0xC9634` | `0xC9574` | 1 on high (`0x4E0B3`) | 3.0 |
| 4 | `0x4E0D0` | `0x4E0E9` | none | `0xC9724` | — | none | 5.0 (`0x40A00000`) |

Each open gate also stores `DS_001088BB = 1`, and all four end at
`0x4E104 mov byte [ebx+0x1e],0`. The compare is `ECX = [R+0x30] sar 0x10`
(signed) against `EDI`/`ECX` = `xor; mov di/cx,[0xbd898]` (the word
zero-extended). The index is `(u16)(R+0x48 - 0x20)` (`0x4DF6A..0x4DF74`,
`and edx,0xffff` before each table read). None of the four bodies calls
`rng_next` (`0x5D7DC`).

**Correction (raw wins).** The old `fight.c` header said "the low branch
also setting the record's +0x55 = 1". The raw stores `+0x55` on the
`jge` (high, `>=`) branch. The header now says so.

Wired: the port's per-entry `PORT:` note is replaced by the four bodies,
folded into one arm with per-state tables.

Test values (`check_fx_gate`, `k4_seed(state, 0, x)`, `R+0x30 = 0x06000000`
so the high word is `0x600`; the three tables `mz_seed` does not point get
entry 3 at scratch streams 11..13):

| state | `DS_000BD898` | stream | `+0x55` | frame | what it pins |
|---|---|---|---|---|---|
| 1 | `0x0601` | `0xC9664` | `0x77` kept | 3.0 | low branch |
| 1 | `0x0600` | `0xC955C` | 1 | 3.0 | `>=` is high |
| 2 | `0x8000` | `0xC964C` | kept | 3.0 | the word is zero-extended (sign-extended it would be high) |
| 2 | `0x0600` | `0xC9574` | 1 | 3.0 | |
| 3 | `0x0601` | `0xC9634` | kept | 3.0 | |
| 3 | `0x0001` | `0xC9574` | 1 | 3.0 | |
| 4 | `0x0000` | `0xC9724` | kept | 5.0 | no compare, no `+0x55` |
| 1, `R+0x30 = 0xFFFF0000` | `0x0001` | `0xC9664` | kept | 3.0 | the high word is signed (`-1 < 1`) |

Every open case: `DS_001088BB` 0 -> 1, `R+0x34` `0x0100` -> 0, entry state
-> 0, and the preamble's `DS_001088B0` 0x100 -> 0xFF (no rearm, no rng).
Every shut case (x one past the reach): `DS_001088BB` stays 0, `R+0x34`
stays `0x0100`, `R+8` stays `0x0BAD`, `+0x55` stays `0x77`, the state is kept.

### §K4.4 Site `0x4A361` in `0x49C78` (`fight_effects_pass`, case 14) — not wired

`0x4A361` sits inside the case-14 body `0x4A346..0x4A412`, which the port
does not have (ledger K13, Task 5). Only the draw arm around it is ported. The
comment there now names `fight_4a868` as ported but not called, record
§K4.4. Wiring the gate without the body would call it and discard the result,
so it stays unwired until K13 ports the body.

### §K4.5 Per-site table

| site | owner | wired | why |
|---|---|---|---|
| `0x4A361` | `0x49C78` case 14 | no | the body `0x4A346..0x4A412` is K13 (Task 5) |
| `0x4BFDE` | `0x4BF18` case 1 | yes | the gated body `0x4BFEB..0x4C034` is 9 stores/calls of ported callees; ported here, voice kept as `PORT:` |
| `0x4DF8E` | `0x4DEF4` state 1 | yes | the state body's callees are all ported |
| `0x4DFFA` | `0x4DEF4` state 2 | yes | same |
| `0x4E066` | `0x4DEF4` state 3 | yes | same |
| `0x4E0D0` | `0x4DEF4` state 4 | yes | same |

### §K4.6 Reachability

`fight_4bf18` runs every frame of mode `0x21` (`game_mode_21_step`, the
attract volleyball). Its case 1 with bit 5 set needs an entry that
`0x4C60C`/`0x4CB18` launched. `fight_effects_idle_pass` runs in mode `0xF`
(reached only through the match-end stores) and would run in mode `0x33`
once §E-3 wires `0x2965F`. The 8000-frame check and the front-end/demo-fight/
attract2 dumps are compared before and after (see the report); the oracle
lines are the gate.

---

## K6 — `0x4F714` and `0x4F728`, the voice wrappers

### §K6.1 `0x4F714` (18 B)

```
4f714: mov ax, word [eax*2 + 0xc9888]
4f71c: and eax, 0xffff
4f721: jmp 0x2c3fc
```

A tail jump: AL is `0x2C3FC`'s. EAX is the stage word the caller loaded
(`0x25BFC xor eax,eax; 0x25BFE mov ax,[0x104afc]`), so the index is
zero-extended. The table `0xC9888` (words): `0x20, 0x21, 0x1B, 0x1C, 0x1E,
0x1D, 0x1F, 0x1F` for stages 0..7 (then `0x0000`, `0x0F00`). All eight are
case-1 records (`DS_000BBDC8`): `0x1B` `0x0D000008`, `0x1C` `0x0D800008`,
`0x1D` `0x0B800008`, `0x1E` `0x0C800008`, `0x1F` `0x0B000008`, `0x20`
`0x0A800008`, `0x21` `0x02803E64`, each with byte 1.

Port: `sound_voice_stage(stage)` (`flow.c`, next to `sound_voice`),
returning `sound_voice`'s AL.

Caller `0x25C09` (in `0x25BBC`, `game_hook_25bbc`): the `PORT:` note is
replaced by `(void)sound_voice_stage(DSW(DS_00104AFC))`. The EDX the caller
loads (`0x25C04 mov edx,0x5D812`) survives the tail (`0x2C3FC` pushes and
pops EDX), which the existing header already records.

### §K6.2 `0x4F728` (79 B)

```
4f728: push edx
4f729: mov  edx, [0x104ad4]          ; the match result (dword)
4f72f: cmp  edx, -1 / je 4f761
4f734: cmp  edx, 3  / je 4f761
4f739: lea eax,[edx*8]; add eax,edx; shl eax,2; add eax,edx   ; 37 * edx
4f747: cmp  byte [eax*4 + 0x107813], 0 / jne 4f761   ; slot[edx]+0x63 (0x94 stride)
4f751: cmp  byte [0x1088f2], 0 / jle 4f761           ; signed
4f75a: mov  eax, 0xdf / jmp 4f766
4f761: mov  eax, 0x23
4f766: call 0x2c3fc
4f76b: mov  eax, 0x22
4f770: call 0x2c3fc
4f775: pop edx / ret
```

EAX on entry (`0x27E40 mov ax,[0x104afc]`) is never read; the port drops it.
The records: `0x23` case 1 `0x02805B88` byte 0; `0xDF` case 1 `0x02806EC8`
byte 0; `0x22` case 5 (the music stop keyed on `DS_00105D5C` in
`0x1B..0x21`, `0x25`, `0x26`).

Port: `sound_voice_match_end()` (`flow.c`). Caller `0x27E4B` (in `0x27DC8`,
`flow_match_end`): the `PORT:` note is replaced by the call.

### §K6.3 Side effects (why wiring moves no pixel)

Every voice these two functions play is case 1 or case 5 in the shipped
table. Case 1 (`0x2C437`) stores `DS_00105D5C = handle`, then `0x1CA14`
stores `DS_001028D9`/`DS_001028D4` and, with no sequence handle
(`DS_001028C0 == 0`, which the port keeps; `game_audio_init`), returns
before `DS_001028CC`. Case 5 id `0x22` compares `DS_00105D5C` against
`0x1B..0x21`/`0x25`/`0x26`; after a case-1 voice it holds a handle, so it
keeps. Neither case reads a resource (`0x1B544`), draws, or calls
`rng_next`. The readers of `DS_00105D5C`/`DS_001028D4`/`DS_001028D9` are the
dispatcher's own case 5 and the pause toggles (`0x1D1B0`, key-driven), none
of which draws. So wiring changes those three words, as the raw does, and no
frame. The dumps before/after confirm it (report).

Test values (`check_voice_wrappers` in `test_game.c`, after
`check_sound_voice`, on `sv_seed`'s sentinels `DS_00105D5C = 0x5C5C5C5C`,
`DS_001028D4 = 0xD4D4D4D4`, `DS_001028D9 = 0x99`):

- `sound_voice_stage(0)`/`(2)`/`(7)`: `D5C = D4 =` `0x0A800008`,
  `0x0D000008`, `0x0B000008`; `D9 = 1`; AL 1.
- `sound_voice_match_end()` (`DS_001028D4`/`D5C` show which of `0xDF`
  (`0x02806EC8`) and `0x23` (`0x02805B88`) played; `D9 = 0`):
  result 0, slot 0 `+0x63 = 0`, `F2 = 1` -> `0xDF`; result 1 with slot 1
  `+0x63 = 0` and slot 0 `+0x63 = 1` -> `0xDF` (the slot is the result's);
  result 0 with slot 0 `+0x63 = 1` -> `0x23`; result -1 and 3 (their slot
  bytes seeded 0) -> `0x23`; result 2 -> `0xDF`; `F2 = 0` and `F2 = 0x80`
  (signed) -> `0x23`; `F2 = 0x7F` -> `0xDF`.
- The trailing `0x22` and its order: with `0x23`'s record handle retyped to
  `0x1B` for the test, the raw order stores `D5C = 0x1B` and then `0x22`
  stops the music (`D4 = 0`); a swapped order or a missing `0x22` leaves
  `D4 = 0x1B`.

---

## K7 — `0x1CB18` and `0x1CC28`'s slot choice: derived, not ported (design decisions needed)

### §K7.1 `0x1CB18` (271 B), the per-slot sample start

EAX = the slot index (0..3; `0x1CF20` calls it for each slot, `0x1CF21..0x1CF2E`,
before `0x1C930`). `ebp = slot * 0x18`.

```
1cb31: edx = slot+0x04 (queued handle); 0 -> return
1cb41: eax = 0x1B544(handle)          ; resolve: [eax] = size, eax+4 = bytes
1cb49: ecx = [eax]; edi = slot+0x10 (buffer); esi = eax+4
1cb5a: rep movsd / rep movsb          ; copy `size` bytes into the slot's buffer
1cb6b: 0x5DC0F(slot+0x00)             ; AIL_init_sample
1cb87: 0x5DC2A(h, slot+0x10, size)    ; AIL_set_sample_address
1cb9c: 0x5DCC5(h, DS_000A2CB4)        ; AIL_set_sample_volume
1cbb0: 0x5DCA6(h, 0x2B11)             ; AIL_set_sample_playback_rate 11025
1cbc3: 0x5DC4D(h, 0, 0)               ; AIL_set_sample_type (8-bit mono)
1cbca: slot+0x08 == 1 -> 0x5DCE4(h, 0) ; AIL_set_sample_loop_count 0
1cbfe: 0x5DC70(h)                     ; AIL_start_sample
1cc03: slot+0x0C = slot+0x04; slot+0x04 = 0
```

Every AIL call maps onto the port's `ail.h` one for one, and the port's
`AIL_set_sample_address`/`AIL_start_sample` take raw 8-bit bytes as the raw
does. The port already has a stand-in, `game_sample_play` (`flow.c`, header
`FUN_0001cb18`, not counted), that runs this call order on slot 0 for the
title announcer bytes `game_sample_request` locates.

### §K7.2 `0x1CC28`'s slot choice (`0x1CC62..0x1CD8D`)

After the DIG/pause gates, `ebp = 0x500BB()` (the time, `mov eax,[0x101500]`)
and `0x1B544(h)`:

- size (`[resolved]`) `> 0x6000` (`0x1CC68 jbe` unsigned): only slot 0.
  Slot 0 is free when its buffer `+0x10 != 0`, its `+0x04 == 0` and its
  status (`0x5DD03`) is not 4. Free: `+0x04 = h`, `+0x08 = loop`,
  `+0x14 = 0x500BB()`, AL = 1. Otherwise slot 0 is forced (below).
- size `<= 0x6000`: slots 3, 2, 1, 0 (`edi` 3..0, `esi` `0x48..0`). The first
  free one (same test) is queued and returns AL = 1. For a slot that is not
  free, `cmp ebp,[slot+0x14]; jbe` (unsigned): a slot whose queue time is
  below the running minimum becomes the candidate (`[esp+4] = edi`) and the
  minimum. The candidate starts at slot 0 (`0x1CC5B xor ebx,ebx`,
  `0x1CC64 mov [esp+4],ebx`) and the minimum at the current time.
- Forced (`0x1CD34`): the candidate slot is ended (`0x5DC8B`), re-inited
  (`0x5DC0F`) and queued (`+0x04`, `+0x08`, `+0x14` as above), AL = 1.

### §K7.3 Why this stops here (the controller's size/design gate)

The functions are small (two bodies, no new callees beyond `ail.h`), but a
faithful port needs three decisions that are beyond the AIL boundary and
belong to a re-plan:

1. **The slot buffers `+0x10`.** Their only writer is `0x1D0BC`
   (`0x1D163 mov [ebx+0x102870],eax`: slot 0 `0x8C00` bytes, slots 1..3
   `0x6000`, each from the paged allocator `0x1C308`; a failure on slot 0
   zeroes `DS_001028C8`). `0x1D0BC` and `0x1C308` are classified
   **host-owned** (`tools/port_classification.txt`, `record-§50-D`), with the
   replacement "samples.c allocates each handle's conversion buffer on
   `AIL_start_sample`". So in the port every `+0x10` is 0 while
   `DS_001028C8` is 1, a state the raw never has (with a DIG driver at least
   slot 0 has a buffer). Porting `0x1CC28`'s choice on that state makes every
   slot "not free", so every queue takes the forced path; porting `0x1CB18`
   on it copies the sample to `mem[0]`. Either a `+0x10` stand-in is written
   (as `DS_001028C8 = 1` is), or `0x1D0BC`'s classification is revisited, or
   `0x1CB18`'s copy is replaced by pointing the handle at the resolved bytes
   (`PORT:`). That is a scope decision on a user-approved classification.
2. **The time source `0x500BB`.** It reads `DS_00101500`, which only the timer
   ISR `0x1BDF4` advances (`0x1BE0E..0x1BE16`); the port has no ISR, and only
   `config.c`'s key-wait loop increments it. The "oldest slot" choice depends
   on it, so it needs a defined port clock (host tick, or `DS_00104AF4`,
   `PORT:`).
3. **The title announcer stand-in.** `game_state_title` calls
   `game_sample_request` and `game_audio_service` calls `game_sample_play` on
   `s_samples[0]` outside the slot records (a documented port choice for the
   deferred attract trigger `0x11000`). A faithful `0x1CF20` loop over four
   slots and this stand-in would drive the same handle from two paths. Either
   the stand-in becomes a real `sound_voice(0xCD)` queue (the announcer id,
   todo-verify record §23) at the raw's trigger, which changes when audio
   starts, or it stays and the slot loop must avoid slot 0.

### §K7.4 Reachability today

`sound_voice`'s case-2/3 queues run in the port: `actors.c`'s `0x4D` (case
3: `0x1201D606` and `0x2001513C`) at the character-3 stream target
(f = 4444 in the no-input run, record §45-A). K6 adds only case-1/5 voices.
So once `0x1CB18` is wired, the two `0x4D` samples would start in a windowed
run; `--check` has no audio device, so frames cannot move, but the audio
render and the "no port path writes slot `+0x04`" gap both change.

### §K7.5 Proposed re-plan (for the controller)

One task: (a) decide the `+0x10` model (recommended: a `PORT:` stand-in in
`game_audio_init`, the non-zero marker per slot as `0x1D0BC` would leave
them, with `0x1CB18` pointing the handle at the resolved bytes instead of
copying, since the port's AIL converts on start); (b) define `0x500BB` as a
`PORT:` read of a host-driven `DS_00101500` (or leave it constant and name
the degenerate "oldest" order); (c) port `0x1CC28`'s choice into
`snd_sample_queue` and `0x1CB18` as `snd_sample_start(slot)`, with
`game_audio_service` looping slots 0..3 before the music as `0x1CF20` does;
(d) retire or re-scope the announcer stand-in; (e) `make audio-render`
before/after, plus a windowed check that the `0x4D` pair starts at f = 4444.
The named gaps at `flow.c`'s sound-module comment and `snd_sample_queue`
stay, and now cite §K7.3.
