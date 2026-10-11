# Track D1, batch d1b — derivations (game/fighter.c, second half)

Differential rows (the original's x86 bytes against the port's C function, `tools/diff_verify.py`) for
the already-ported functions of `.superpowers/sdd/2026-10-11-reverse-d1/d1b-queue.txt`. Method: the
brief of track D1 and record `2026-10-01-reverse-e3-derivations.md` §E3.10/§E3.12. Every number below is
measured on the branch `reverse-d1b` with the image `./build/diffrun --exe data/game/C/PRAGE.EXE
--image-out` dumps (fixups applied); every address is a linear address of that image.

The claim of each row is the harness's narrow one: equivalence on the exercised blocks and inputs only,
each function with its callees stubbed or run as stated.

**Result.** 29 rows VERIFIED with 341 mutants detected; 7 port corrections in 8 functions
(§3.1-§3.7, one commit each); 1 raw-over-port finding measured and NOT landed (0x47A00, §3.8); 3 queue
entries not done (§6).

## §1 The members and the callers that fix each EAX mask

A mask below is either full (the port's return value reproduces the original's whole EAX; no caller
analysis is needed), 0xFF (the original sets AL and every caller reads AL only), or 0 (no caller reads
EAX). The mask-0 and mask-0xFF rows rest on the instructions named here. They were found by following
EAX forward from every direct `call`/`jmp rel32` to the entry, through returns into the callers of the
containing function and into direct callees, until an instruction overwrites EAX (scratch tool; the
result per row is the list of first reads and of overwriting instructions). **Limit:** only direct
`call`/`jmp rel32` sites and occurrences of the entry's four address bytes in the image are followed;
a call through a computed pointer would not be seen.

| addr | port function | size | mask | the instructions that fix it |
|---|---|---|---|---|
| 0x36300 | `fighter_state_36300` | 226 | 0 | one caller 0x34CAD pops and returns (0x34CB2..0x34CB7) into 0x34B6C's one caller, `mov eax,esi` 0x35801 |
| 0x36710 | `fighter_state_36710` | 202 | 0 | 0x34CBC, then as 0x36300: `mov eax,esi` 0x35801 |
| 0x380C4 | `fighter_380c4` | 144 | 0 | `xor eax,eax` 0x48B7A |
| 0x38D24 | `fighter_38d24` | 108 | 0 | `mov eax,esi` 0x357FA; `mov eax,ecx` 0x38562 |
| 0x39278 | `fighter_39278` | 8 | 0 | `mov eax,2` 0x19367; the returns through 0x192DC (0x1936C, 0x193A5) and 0x193B0 (0x19580) into 0x1958C (0x1974D) end in `xor eax,eax` 0x262F3, 0x26482, 0x265DF, 0x2747D, 0x2882C, 0x29AE5; 0x197FF's in `mov eax,esp` 0x19774 or, out of 0x1975C, `xor eax,eax` 0x2635B, 0x264EA, 0x26647 and 0x3CB68's `mov eax,[0x107edc]` 0x3CB79 |
| 0x3962C | `fighter_3962c` | 126 | 0xFF | `mov al,1` 0x3969C / `xor al,al` 0x396A3; `test al,al` 0x193E9, 0x197A0, then EAX reloaded |
| 0x396AC | `fighter_396ac` | 139 | 0xFF | `mov al,1` 0x3972B / `xor al,al` 0x3972F; `test al,al` 0x14838, 0x14BB4, 0x1941E, 0x197C4 |
| 0x399CC | `fighter_state_399cc` | 65 | 0 | 0x34C82, then as 0x36300: `mov eax,esi` 0x35801 |
| 0x39AC8 | `fighter_39ac8` | 74 | full | (callers store AX: 0x39C16, 0x39DB6) |
| 0x39B14 | `fighter_39b14` | 4 | full | (caller stores AX: 0x39C3B) |
| 0x3A2A0 | `fighter_3a2a0` | 332 | full | EAX = ESI = 0 (0x3A375/0x3A37A) or 1 (0x3A38B/0x3A3E2) |
| 0x3B938 | `fighter_3b938` | 140 | 0 | `mov eax,[0x10784c]` 0x17C97; `mov eax,esi` 0x197FD |
| 0x3B9D8 | `fighter_3b9d8` | 273 | 0 | `xor eax,eax` 0x3BB48, 0x3BB64; `mov eax,1` 0x3BB58 |
| 0x3C1C8 | `fighter_3c1c8` | 61 | 0 | `mov eax,[esp+0x10]` 0x45415 |
| 0x3C6E8 | `fighter_cmd_facing` | 109 | full | `and edx,0xffff; mov eax,edx` 0x3C748/0x3C74E |
| 0x3D0C0 | `fighter_3d0c0` | 75 | 0 | `mov eax,[esp+0xc]` 0x3FE09 |
| 0x3F308 | `fighter_3f308` | 87 | 0 | `mov eax,[ecx]` 0x3E72E; `mov eax,[esp]` 0x3F1A3, 0x3F46F |
| 0x3F6F4 | `fighter_3f6f4` | 43 | full | callers read all of it: `mov edx,eax` 0x233EA, 0x2343E, 0x457D0, 0x45813, 0x4881A, 0x4886D; `cmp eax,1` 0x3FA42 |
| 0x3F720 | `fighter_3f720` | 92 | 0 | direct callers reload EAX: 0x146B7, 0x14A23, 0x233F4, 0x23448, 0x3FBE9, 0x457DA, 0x4581D, 0x48824, 0x48877. 0x3FA80 returns out of 0x3F9C8, a slot +0x0C callback (`mov dword [ebx+0xc],0x3f9c8` 0x3FBC1); the image's two `call [ecx+0xc]` sites drop EAX: `xor eax,eax` 0x35434, and 0x384D9 returns through 0x38434 to `mov eax,esi` 0x38570 |
| 0x408E8 | `fighter_408e8` | 87 | 0 | `mov eax,[esp+0xc]` 0x40A27 |
| 0x46698 | `ai_pred_46698` | 89 | 0xFF | `mov al,1` 0x466EA / `xor al,al`; `test al,al` 0x46A0C |
| 0x466F4 | `ai_pred_466f4` | 72 | 0xFF | `mov al,1` 0x46738 / `xor al,al`; `test al,al` 0x46A25 |
| 0x4673C | `ai_pred_4673c` | 85 | 0xFF | `mov al,1` 0x4678C / `xor al,al`; `test al,al` 0x46A45 |
| 0x46794 | `ai_pred_46794` | 72 | 0xFF | `mov al,1` 0x467D8 / `xor al,al`; `test al,al` 0x46A65 |
| 0x46898 | `ai_pred_46898` | 63 | 0xFF | `mov al,1` 0x468C9 / `xor al,al` 0x468D0; `test al,al` 0x46B44 |
| 0x46958 | `ai_ready` | 78 | 0xFF | `mov al,1` 0x4699C / `xor al,al` 0x469A1; `test al,al` 0x469DC |
| 0x46BBC | `ai_weight` | 130 | 0 | every call site (all in 0x46C78) reaches `mov eax,[ebx+0x108218]` 0x46DA4 |
| 0x46DD4 | `ai_band` | 291 | 0 | one caller 0x46FEC, then `lea eax,[edx*4]` 0x46FF1 |
| 0x4FB20 | `fighter_4fb20` | 117 | full | `mov eax,ecx` 0x4FB8F |

The queue's line for 0x46DD4 names the port's function only by its file: it is `ai_band`
(`ai_band_tables` is the helper its switch became).

## §2 The row table (measured)

`python3 tools/diff_verify.py --image <image> --function <row> --self-check`, every mutant's port side
run ten times with identical output before it was pinned.

| row | entry | cases | blocks | mutants | EAX mask | notes |
|---|---|---|---|---|---|---|
| `fighter_state_36300` | 0x36300 | 18 | 15/15 | 25 | 0x0 | EAX = slot, EDX = rec; stub 0x2BC30; arm table 0x362F0 resolved by hand (§4) |
| `fighter_state_36710` | 0x36710 | 11 | 11/11 | 19 | 0x0 | EAX = slot, EDX = rec; stubs 0x3C480, 0x2C3FC; correction §3.1 |
| `fighter_380c4` | 0x380C4 | 4 | 3/3 | 15 | 0x0 | EAX = side; stubs 0x2BC30, 0x2AE14 (its EAX is the +0x59 store's base, varied) |
| `fighter_38d24` | 0x38D24 | 8 | 4/4 | 12 | 0x0 | EAX = side; stubs 0x38C5C, 0x2F4BC; 0x1C500 and its tree (0x474E4, 0x1E75C, 0x1E808, 0x500BB) run on both sides, as C3g's 0x38D90 row |
| `fighter_39278` | 0x39278 | 4 | 1/1 | 4 | 0x0 | EAX = v; no callee |
| `fighter_3962c` | 0x3962C | 10 | 8/8 | 14 | 0xFF | EAX = side, DL = selector; 0x33950, 0x3AFC4 run on both sides; correction §3.2 |
| `fighter_396ac` | 0x396AC | 11 | 10/10 | 15 | 0xFF | EAX = side, DL = selector; 0x33950, 0x3AFC4 run on both sides; correction §3.2 |
| `fighter_state_399cc` | 0x399CC | 7 | 4/4 | 12 | 0x0 | EAX = side; 0x33A10 and 0x367DC run on both sides, 0x367DC's 0x2BC30 calls are the stubs; correction §3.3 |
| `fighter_39ac8` | 0x39AC8 | 14 | 3/3 | 8 | 0xFFFFFFFF | AX = a, DX = n; no callee; x87 compare (§4) |
| `fighter_39b14` | 0x39B14 | 5 | 1/1 | 4 | 0xFFFFFFFF | `imul eax,edx`; no callee |
| `fighter_3a2a0` | 0x3A2A0 | 7 | 7/7 | 27 | 0xFFFFFFFF | EAX = side, EDX/EBX/ECX/s0 pose words (`ret 4`); 0x33950, 0x3AFC4, 0x3A280 run on both sides; stubs 0x3B298 (AL read), 0x2C3FC, 0x3A0FC, 0x18B04, 0x39F40, 0x39834, 0x3B080, 0x3AD98 |
| `fighter_3b938` | 0x3B938 | 5 | 4/4 | 16 | 0x0 | EAX = slot; stubs 0x2BC30, 0x2C3FC |
| `fighter_3b9d8` | 0x3B9D8 | 14 | 16/16 | 25 | 0x0 | EAX = side, CX = pen; 0x33950 runs on both sides; stubs 0x1883C (writes side 0's +0x2C = 0x500), 0x3B8D8 (AL read); correction §3.4 |
| `fighter_3c1c8` | 0x3C1C8 | 5 | 3/3 | 6 | 0x0 | EAX = side, DX = v; 0x33950 runs on both sides |
| `fighter_3f6f4` | 0x3F6F4 | 6 | 3/3 | 6 | 0xFFFFFFFF | EAX = side, EDX = n; 0x33950 runs on both sides |
| `fighter_cmd_facing` | 0x3C6E8 | 11 | 12/12 | 9 | 0xFFFFFFFF | EAX = facing, EDX = input; no callee |
| `fighter_3d0c0` | 0x3D0C0 | 2 | 1/1 | 13 | 0x0 | EAX = side; no callee |
| `fighter_3f308` | 0x3F308 | 3 | 3/3 | 12 | 0x0 | EAX = slot; stubs 0x2C3FC, 0x2AE14, 0x13244 |
| `fighter_3f720` | 0x3F720 | 9 | 8/8 | 10 | 0x0 | EAX = side, EDX = n; 0x33950 runs on both sides; stubs 0x187FC (EAX read, called twice), 0x189FC (AL read), 0x3C190; correction §3.5 |
| `fighter_408e8` | 0x408E8 | 3 | 3/3 | 13 | 0x0 | EAX = slot; stubs 0x2C3FC, 0x2AE14 (its EAX is stored, varied) |
| `ai_pred_46698` | 0x46698 | 9 | 8/8 | 9 | 0xFF | EAX = side; 0x33950 runs on both sides |
| `ai_pred_466f4` | 0x466F4 | 7 | 9/9 | 5 | 0xFF | EAX = side; no callee |
| `ai_pred_4673c` | 0x4673C | 8 | 10/10 | 7 | 0xFF | EAX = side; no callee |
| `ai_pred_46794` | 0x46794 | 7 | 9/9 | 5 | 0xFF | EAX = side; no callee |
| `ai_pred_46898` | 0x46898 | 6 | 5/5 | 6 | 0xFF | EAX = side; 0x33A10 runs on both sides |
| `ai_ready` | 0x46958 | 7 | 6/6 | 5 | 0xFF | EAX = side; no callee |
| `fighter_4fb20` | 0x4FB20 | 12 | 12/12 | 13 | 0xFFFFFFFF | no argument; no callee |
| `ai_weight` | 0x46BBC | 4 | 1/1 | 9 | 0x0 | EAX = side, EDX = id; no callee; correction §3.6 |
| `ai_band` | 0x46DD4 | 11 | 21/21 | 17 | 0x0 | EAX = side; stub 0x187FC (EAX read, called twice); switch 0x46DB8 (bounded, followed by the scan); correction §3.7 |

Every block of every row is hit; no row names an unhit block. Each mutant's detecting kinds and exact
case set are `D1B_KINDS` / `D1B_CASES` in `tools/tests/test_diff_verify.py`.

Harness changes outside the D1b blocks (both additive): `diff_emu.RESOLVED_JUMPS[0x3631D] =
(0x36310, 0x362F0, 4)` and its window in `RealFunctionTests.WINDOWS` (§4). With the entry added,
`E.callee_clobbers` of each of the 165 stubs of `SPECS` is unchanged (re-derived over the image).

The callees stubbed here are all stubs of earlier rows (the hard-coded set of
`test_each_stub_declares_the_registers_its_callee_clobbers` is unchanged); the rowed callees that are
not in that set (0x3AFC4, 0x3A280, 0x33A10, 0x367DC, 0x1C500) run on both sides.

## §3 Corrections (address-proved) and one finding not landed

Each correction is its own commit, was first observed as a MISMATCH of the row against the unmodified
port, and has a mutant that restores the old behaviour. **None is proven against the capture oracles**
(`make verify` was not run on this branch, by the brief). `PR_ORACLE_REQUIRED=1 ./build/run_tests`
passes with all seven.

### §3.1 0x36710 `fighter_state_36710`: the 0x3C480 record and character

The `+0x58 == 1` arm calls 0x3C480 with EAX = the first dword of the slot of `rec+0x51`'s side:
`mov ebx,0x1077b0; shl eax,2; add ebx,eax` (0x3675A..0x36762), `mov eax,[ebx]` 0x3676E (kept at
`[esp+0x14]`, reloaded 0x36796), and the stream `0xC9080[byte [ebx+0x7a]]` (`mov dl,[ebx+0x7a]`
0x36793). Old port: the EDX record and `0xC9080[the EAX slot's +0x7A]`. Measured before the fix
(g0): `original 0x3C480(0x10A400, 0xED12A, 0x40400000), port 0x3C480(0x10A300, 0xD432C, 0x40400000)`.
Mutants `@rec`, `@char`. Equal while EAX is the side's slot and EDX its record (0x34B6C's call).

### §3.2 0x3962C `fighter_3962c` and 0x396AC `fighter_396ac`: the selector is DL

`mov bl,dl` (0x39630 / 0x396B1) and `test bl,bl` (0x3963B / 0x396BC) choose slot+0x64 over +0x5F.
Old port: `param_2 != 0u` on the whole argument. Measured before the fix with EDX = 0x100: q5
`eax (mask 0xFF): original 0x1, port 0x0`; r6 the same. Mutant `@dl`. Not reachable from the port's
callers (they pass 0 or 1).

### §3.3 0x399CC `fighter_state_399cc`: the 0x367DC arguments

`mov eax,[ebx*4+0x1077a8]` 0x399EE; `mov edx,[eax]` 0x399FC; `call 0x367dc` 0x39A02: EAX = s, the
0x1077A8[side] pointer, EDX = [s]. Old port: the 0x33A10 context's slot (0x1077B0 + side*0x94) and
that slot's first dword. Measured before the fix (p0, s = a slot that is not the side's): `call #0:
original 0x2BC30(0x10A300, 0xECBDA, 0x40400000), port 0x2BC30(0x10A400, 0xEA606, 0x40400000)` and
0x367DC's stores in the other slot. Mutants `@slot`, `@rec`. Equal while 0x1077A8[side] is the
side's own slot.

### §3.4 0x3B9D8 `fighter_3b9d8`: CX is sign-extended here, and AL of 0x3B8D8 is tested

`movsx ebx,cx` 0x3BA39 and 0x3BA6E; `test al,al` 0x3BA53 and 0x3BA8A. Old port: the whole 32-bit
argument halved; the whole int tested. Measured before the fix: u1 (EDX = 0xFFEF) `call #0: original
0x3B8D8(0x0, 0xFFFFFFF8), port 0x3B8D8(0x0, 0x7FF7)`; u2 (stub EAX = 0x100) `call #1: original
0x1883C(0x1, 0xFFFFFFF8, 0x0), port 0x1883C(0x0, 0x8, 0x0)`. Mutants `@pen16`, `@al`. Not reachable
from the port's code (0x3BAEC passes a sign-extended AX; the port's 0x3B8D8 returns 0 or 1).

### §3.5 0x3F720 `fighter_3f720`: AL of 0x189FC is tested

`test al,al` 0x3F766. Old port: `!fighter_189fc(..)` on the int. Measured before the fix (d5, stub
EAX = 0x100): `call #3: original 0x3C190(0x0, 0x64), port 0x3C190(0x0, 0x19)`. Mutant `@al`.

### §3.6 0x46BBC `ai_weight`: the other slot is 1 - side

`mov edx,1` 0x46BE4; `sub edx,ebx` 0x46BE9. Old port: `side ^ 1`. Measured before the fix (j3,
side 2: slot -1 against slot 3): the three stores differ (`byte 0x108284: original 0x07, port 0x0A`
...). Mutant `@oside`. Equal for sides 0 and 1.

### §3.7 0x46DD4 `ai_band`: the band is clamped and compared signed

`cmp edx,0xff; jle` 0x46E96/0x46E9C; `cmp edx,ecx; jl` 0x46EA8/0x46EAA; `jg` 0x46EB1/0x46EB3. Old
port: an unsigned clamp, so a negative band became 0xFF. Measured before the fix (k9, 0x187FC =
0x80000000, whose negation stays negative): `byte 0x10821D: original 0x10, port 0x23` (the raw
matches no band and falls back to entry 0; the port matched the last entry). Mutant `@uclamp`.
Reachable only with that distance. The port's `mag >> 6` was left as it is: for a band that is not
negative it equals the raw's truncating `sar/shl/sbb/sar` (0x46E79..0x46E83), and 0x80000000 is
divisible by 64, so no input tells the two apart.

### §3.8 FINDING, not landed: 0x47A00 `fighter_47a00` hands 0x3C480 the wrong record

`mov eax,[esp+0x14]` at 0x47A89, with ESP as `sub esp,0x18` 0x47A05 left it (the one `push` before it,
0x47A33, is the `ret 4` argument of 0x2AE14): `[esp+0x14]` is the sixth dword 0x33950 wrote, the
OTHER side's record (the same slot 0x47AB8 reloads for the second 0x39A10). The port passes `ctx[4]`,
the side's own record. Measured, every case (t0): `call #3: original 0x3C480(0x10A400, 0xE1111,
0x40400000), port 0x3C480(0x10A300, 0xE1111, 0x40400000)`, where 0x10A300 is the side's record and
0x10A400 the other's. With `ctx[5]` the row is VERIFIED: 4 cases, 3/3 blocks, 21/21 mutants (`@rec`
is the port as it stands).

This one is reachable: 0x47A00 is the throw setup 0x47B04's case 0 runs, so the port restarts the
thrower's record on the victim's stream where the original restarts the victim's.

**Why it is not landed.** `check_49z_47a00` (`port/tests/test_fight.c`, 38129, 38130, 38142 and
38224) asserts the port's behaviour: the SIDE's record `+8`/`+0x24` take the stream and 3.0, and the
two slots end `dist` apart. With the fix `PR_ORACLE_REQUIRED=1 ./build/run_tests` reports `FAILURES:
32`, all on those four lines (`11259375 != 66271744`: the side's `+8` keeps its 0xABCDEF seed;
`1200 != 80`: the distance). The first two are re-derivable from 0x47A89 (assert the other record,
and that the side's keeps its seed). The distance is not a one-line change: the test steers 0x3C208
with a tuned facing word in the opponent's actor (`DSW(FIGHT_ACTORS + ..) = 0x8F35 / 0x0F35`), which
the animation restart now rewrites; I did not derive the seed that restores the assertion and did not
fit 1200. The fix also changes gameplay, so it needs the capture oracles. The port is unchanged for
0x47A00 and the row is not in `D1B_SPECS`.

The fix is one token in `fighter_47a00`: `hit_anim_start_a(ctx[5], ...)`. The row as measured:

```python
D1B_47A00_CALLS = (SPAWN, E.Call(0x39834, ("eax", "edx"), clobbers=("edx", "ebp")), VOICE, HIT_A,
                   E.Call(0x3C358, ("eax",)), E.Call(0x39A10, ("eax", "edx"), clobbers=("edx",)),
                   E.Call(0x3C208, ("eax", "edx"), clobbers=("edx",)))


def d1b_47a00_case(side, gate, ch=2, och=4, b=0x15, w56=0x8123):
    """0x47A00: the gate byte 0x105B3A, both slots' record pointers and +0x57/+0x5F/+0x7A, both
    records' +0x4B and word +0x56, the stub actor's +0x56 and +0x60, and the two characters'
    0xC90F8 streams and 0xC9456 words."""
    ss, os_ = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    return {0x00105B39: bytes([0x39, gate, 0x3B]), ss: le32(D1B_REC), os_: le32(D1B_REC2),
            ss + 0x56: b"\x56\x57\x58", os_ + 0x56: b"\x66\x67\x68",
            ss + 0x5E: bytes([0x5E, b, 0x60]), os_ + 0x5E: bytes([0x6E, b ^ 0x3F, 0x70]),
            ss + 0x79: bytes([0x79, ch, 0x7B]), os_ + 0x79: bytes([0x79, och, 0x7B]),
            D1B_REC + 0x4A: b"\x4a\x4b\x4c", D1B_REC + 0x54: le16(0x5455) + le16(w56) + b"\x58\x59",
            D1B_REC2 + 0x4A: b"\x5a\x5b\x5c",
            D1B_REC2 + 0x54: le16(0x6465) + le16(w56 ^ 0x0FFF) + b"\x68\x69",
            D1B_CHILD + 0x55: b"\x55\xa6\xa7", D1B_CHILD + 0x5F: b"\x5f\x60\x61",
            0x000C90F8 + och * 4: le32(0x000E1111), 0x000C90F8 + ch * 4: le32(0x000E2222),
            0x000C9456 + och * 2: le16(0x8010), 0x000C9456 + ch * 2: le16(0x0020)}
```

```python
    # 0x47A00 fighter_47a00: EAX = side; the throw setup. With the byte 0x105B3A at most 1
    # (zero-extended, `jg` 0x47A21) 0xBB128 spawns with a5 = the side record's word +0x56 |
    # 0x400, its +0x60 = 1 and the record's +0x4B = its +0x56 (the raw does not test the spawn
    # for 0; the port's PORT guard is not exercised, a 0 stub EAX would write outside the
    # image). Then slot+0x57 = 1, 0x39834(other, slot+0x5F), voice 0x46, 0x3C480 on the OTHER
    # side's record (`mov eax,[esp+0x14]` 0x47A89) with 0xC90F8[the other's char] at 3.0,
    # 0x3C358(side), 0x39A10 on both records with 0x309 and 0x3C208(other, the s16 0xC9456[the
    # other's char]). 0x33950 runs on both sides. Mask 0 (record §1).
    Spec("fighter_47a00", 0x47A00, [
        Case("t0", {"eax": 0}, d1b_47a00_case(0, 0), {0x2AE14: D1B_CHILD}),
        Case("t1", {"eax": 1}, d1b_47a00_case(1, 1, ch=5, och=0), {0x2AE14: D1B_CHILD}),
        Case("t2", {"eax": 0}, d1b_47a00_case(0, 2), {0x2AE14: D1B_CHILD, 0x2C3FC: 0}),
        Case("t3", {"eax": 1}, d1b_47a00_case(1, 0xFF, ch=6, och=3), {0x2AE14: D1B_CHILD}),
    ], allow_calls=(0x33950,), calls=D1B_47A00_CALLS, eax_mask=0,
       mutants=("@gate", "@w56rec", "@or400", "@a60", "@r4b", "@a56", "@s57", "@s57side",
                "@pside", "@pb", "@voice", "@char", "@rec", "@frame", "@hold", "@t1", "@tval",
                "@t2", "@place", "@dzext", "@gs8")),
```

```c
/* 0x47A00's mutants. EAX = side. The 0x3C480 record is ctx[5], the other side's (`mov eax,
 * [esp+0x14]` 0x47A89). */
static void d1b_47a00_core(const u32 *r, u32 mut)
{
    u32 side = r[R_EAX], ctx[6], och;
    u8 gate = DSB(0x00105B3Au);
    fighter_ctx_same(ctx, side);
    if ((mut & 1u) ? (gate < 1u) : (mut & 0x100000u) ? ((s8)gate <= 1) : (gate <= 1u)) {   /* @gate, @gs8 */
        u32 w = (u32)DSW(ctx[(mut & 2u) ? 5 : 4] + 0x56u);              /* @w56rec */
        u32 a = actor_spawn((const u32 *)(mem + 0x000BB128u), 0u, 0u, 0u,
                            (mut & 4u) ? w : (w | 0x0400u));            /* @or400 */
        if (!(mut & 8u)) DSB(a + 0x60u) = 1u;                           /* @a60 */
        if (!(mut & 0x10u))                                             /* @r4b */
            DSB(ctx[4] + 0x4Bu) = DSB(a + ((mut & 0x20u) ? 0x57u : 0x56u));     /* @a56 */
    }
    if (!(mut & 0x40u))                                                 /* @s57 */
        DSB(0x001077B0u + ((mut & 0x80u) ? ctx[1] : side) * 0x94u + 0x57u) = 1u;   /* @s57side */
    fighter_39834((mut & 0x100u) ? ctx[0] : ctx[1],                     /* @pside */
                  (s32)(u32)DSB(ctx[(mut & 0x200u) ? 3 : 2] + 0x5Fu));  /* @pb */
    (void)sound_voice((mut & 0x400u) ? 0x47u : 0x46u);                  /* @voice */
    och = (u32)DSB(ctx[(mut & 0x800u) ? 2 : 3] + 0x7Au);                /* @char */
    hit_anim_start_a(ctx[(mut & 0x1000u) ? 4 : 5],                      /* @rec */
                     DSD(0x000C90F8u + och * 4u),
                     (mut & 0x2000u) ? 0x40000000u : 0x40400000u);      /* @frame */
    fighter_3c358((mut & 0x4000u) ? ctx[1] : side);                     /* @hold */
    fighter_39a10(ctx[(mut & 0x8000u) ? 5 : 4], (mut & 0x10000u) ? 0x308u : 0x309u);    /* @t1, @tval */
    if (!(mut & 0x20000u)) fighter_39a10(ctx[5], 0x309u);               /* @t2 */
    fighter_3c208((mut & 0x40000u) ? ctx[0] : ctx[1],                   /* @place */
                  (mut & 0x80000u) ? (s32)(u32)DSW(0x000C9456u + och * 2u)      /* @dzext */
                                   : (s32)(s16)DSW(0x000C9456u + och * 2u));
}
static void b_d1b_47a00(const u32 *r, u32 *eax) { fighter_47a00(r[R_EAX]); *eax = 0u; }
#define D1B_47A00_MUT(name, bit) \
    static void m_d1b_47a00_##name(const u32 *r, u32 *eax) { d1b_47a00_core(r, bit); *eax = 0u; }
D1B_47A00_MUT(gate, 1u)        D1B_47A00_MUT(w56rec, 2u)      D1B_47A00_MUT(or400, 4u)
D1B_47A00_MUT(a60, 8u)         D1B_47A00_MUT(r4b, 0x10u)      D1B_47A00_MUT(a56, 0x20u)
D1B_47A00_MUT(s57, 0x40u)      D1B_47A00_MUT(s57side, 0x80u)  D1B_47A00_MUT(pside, 0x100u)
D1B_47A00_MUT(pb, 0x200u)      D1B_47A00_MUT(voice, 0x400u)   D1B_47A00_MUT(char, 0x800u)
D1B_47A00_MUT(rec, 0x1000u)    D1B_47A00_MUT(frame, 0x2000u)  D1B_47A00_MUT(hold, 0x4000u)
D1B_47A00_MUT(t1, 0x8000u)     D1B_47A00_MUT(tval, 0x10000u)  D1B_47A00_MUT(t2, 0x20000u)
D1B_47A00_MUT(place, 0x40000u) D1B_47A00_MUT(dzext, 0x80000u) D1B_47A00_MUT(gs8, 0x100000u)
```

Table rows for `k_bindings`: `fighter_47a00` -> `b_d1b_47a00`, and `fighter_47a00@<m>` ->
`m_d1b_47a00_<m>` for the 21 mutants of the Spec, all mask 0. Its mask-0 argument: 0x47B85 returns out
of 0x47B04, a slot +0x0C callback (`mov dword [ecx+0xc],0x47b04` 0x47C53), whose two `call
[ecx+0xc]` sites drop EAX (§1, 0x3F720).

## §4 Named gaps, limits and unhit blocks

No unhit block and no named gap (no `gap=` spec). The limits of the rows, each from the bytes:

- **0x36300's arm table.** `cmp dl,3; ja 0x363da; xor ecx,ecx; mov cl,dl; jmp cs:[ecx*4+0x362f0]`
  (0x36310..0x3631D): the index is moved to another register, which `switch_cases` does not bound, so
  the jump is resolved by hand (`RESOLVED_JUMPS`, 4 entries: 0x363DA, 0x36325, 0x36372, 0x36389; the
  window is pinned in `test_each_resolved_jump_table_matches_the_bytes`). All four targets are hit.
- **0x36710, side.** `rec+0x51` takes 0, 1 and 2. 0x80 (the checklist's zero/sign-extension value)
  indexes the slot 0x1077B0 + 0x80*0x94 = 0x10C1B0, past the image's end 0x10B0D0: not exercised, so
  `xor eax,eax; mov al,[edx+0x51]` (0x36717) is not separated from a sign extension.
- **A spawn that returns 0.** 0x380C4 stores `[eax+0x59]` (0x3814C) with no test; a 0 stub EAX writes
  address 0x59, outside the image: not exercised (0x380C4; the same in 0x47A00's 0x47A40, where the
  port has a `PORT:` guard).
- **0x3AFC4's error path.** With the +0x5F/+0x64 byte at or above 0x40, 0x3AFC4 calls 0x62003, which
  the port does not model. 0x3962C and 0x3A2A0 keep the byte below 0x40; 0x396AC tests the bound
  itself (0x396DA) and its 0x40 and 0xFF cases return before the call.
- **0x39AC8.** n = 0 is a `#DE` in the raw (`idiv ebx` 0x39ADF): not exercised. The double at 0x80BFA
  (0.5) is read by the raw and inlined by the port; no case pokes it. The x87 compare is exact: both
  operands are integers below 2^31 or such an integer plus 0.5, in 53 bits.
- **x87 elsewhere.** 0x36300 (0x36359) and 0x3B938 (0x3B96B) `fild` a zero-extended byte and store a
  float: exact (0..255). Each has a `@stepsext` mutant and a byte >= 0x80.
- **0x46DD4, a character above 6.** `cmp al,6; ja 0x46e53` (0x46DF0) skips every `mov ebp/edi`: the
  raw then indexes with its caller's EBP and EDI. The port's default arm is character 6's tables. Not
  exercised (the port has no such input); the seven table targets 0x46E01..0x46E49 are each hit.
- **Unobservable boundaries** (stated in the specs): 0x3B9D8's `> 0` / `< 0` tests of the +0x34 word
  (a zero word makes the store a no-op); 0x4FB20's `|dy| > width` test at equality (the estimate is
  then at least the width, so 0 either way after the same stores: the `@gey` mutant was equivalent
  and was removed); 0x46DD4's truncating against flooring division (§3.7).

## §5 Store sweep

Every field a row's function writes is seeded with a value different from what it writes, with the
bytes around it seeded and slot-distinct; for each store there is a mutant that drops it (or moves
it across a call), and every one is detected. The dropped-store mutants: 0x36300 `@or @inc @sub2
@no52 @zero`; 0x36710 `@set1 @set2 @no43`; 0x380C4 `@no59`; 0x38D24 `@nodec`; 0x3962C `@noinc`;
0x396AC `@noinc`; 0x399CC `@or @w74`; 0x3A2A0 `@c8a @c18 @no41 @and42a @and42b`; 0x3B938 `@s64 @p48
@p34 @p36 @s08`; 0x3B9D8 `@or41a @or41b @d30`; 0x3D0C0 nine, one per store; 0x408E8 `@store`;
0x4FB20 `@sdx @sadx @sdy @sady`; 0x46BBC `@s2c @s14 @s34`; 0x46DD4 `@s2d @s18 @s38`. Too-wide stores:
0x36300 `@subwide`, 0x39278 `@wide`/`@byte`, 0x399CC `@w74wide`, 0x3A2A0 `@c18w`, 0x3B9D8
`@w34wide`, 0x3C1C8 `@wide`, 0x3D0C0 `@r44b`, 0x408E8 `@storew`, 0x46BBC `@idw`. Order across a call
(caught by the memory at the call alone): 0x36300 `@order`, 0x36710 `@order`/`@vorder`, 0x399CC
`@order`, 0x3A2A0 `@order`, 0x3B938 `@early`/`@vorder`, 0x3B9D8 `@d30order`.

Findings of the sweep:

- 0x3A2A0 clears slot+0x8A twice (0x3A2E5 and 0x3A3BE); only the first is separately observable
  (`@c8a`, through the memory at the calls between them), so no mutant drops the second.
- 0x3B938's two adjacent word stores (+0x34, +0x36) are each observable, but a single dword store
  would not differ from the pair.
- 0x3D0C0 and 0x4FB20 have no callee, so the order of their stores is not observable.
- 0x3B9D8 repeats its x test after 0x1883C (0x3BAB9); with a stub that writes nothing the repeat
  would be invisible, so the row's 0x1883C stub writes side 0's +0x2C and `@cache` (the first
  test's result reused) is detected on all five tail cases.
- One weak case was found and fixed before pinning: `fighter_cmd_facing`'s c9 first set both the
  0x10000 and 0x40000 gates, which OR the same two bits whatever the facing, so `@fbyte` was missed;
  c9 now sets one gate.

## §6 Queue entries not done

- **0x47A00** (233 bytes): measured, a raw-over-port finding, not landed (§3.8).
- **0x406B4** (309 bytes; callees 0x249B0, 0x249D0, 0x2A17C, 0x2AE14, 0x2C3FC, 0x5D7DC) and **0x45908**
  (197 bytes; callees 0x2AE14, 0x2C3FC, 0x339AC, 0x3C148, 0x3C16C, 0x3C480): not started beyond the
  disassembly; no row, no claim. (The EAX trace of 0x45908 stops at 0x4505C, whose address is taken
  at 0x45117; that caller was not analysed.)

Exports (linkage only, `static` dropped and a prototype added to the `PORT:` block of
`port/src/game/fighter.h`), so `port/tests/diff_runner.c` can bind them: `fighter_380c4`,
`fighter_39278`, `fighter_3962c`, `fighter_396ac`, `fighter_39ac8`, `fighter_39b14`, `fighter_3b9d8`,
`fighter_3c1c8`, `fighter_3f6f4`, `fighter_cmd_facing`, `fighter_3d0c0`, `fighter_3f720`,
`fighter_408e8`, `ai_pred_46698`, `ai_pred_466f4`, `ai_pred_4673c`, `ai_pred_46794`,
`ai_pred_46898`, `ai_ready`, `fighter_4fb20`, `ai_weight`, `ai_band` (22). No seam was added.
