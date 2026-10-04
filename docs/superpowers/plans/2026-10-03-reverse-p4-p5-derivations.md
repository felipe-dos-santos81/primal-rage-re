# Reverse completion P4+P5: the animation targets A and B (record)

**Scope.** Track P's combined batches 4 and 5 (roadmap rows P4 and P5 of record `2026-10-02-reverse-p1-derivations.md` §P1.3, as P2's §P2.11 and P3's §P3.11 left them: P4's 20 animation targets A and P5's 13 animation targets B, P2's `0x14FA8 0x14FF8 0x150AC` moved out), under spec `2026-09-30-reverse-completion-design.md` §4 track P and §6. Plan: `2026-10-03-reverse-p4-p5-animation.md`. Recipe: E3 record §E3.10; lessons: P1 §P1.10-§P1.12 and P2's/P3's reviews.

**Status of the numbers.** Measured by the planner on 2026-10-03 on `main` `f5b5556` in this worktree (a prototype of all tasks; the full `make verify` with the parallel-safe overrides ran on its final state). **The image** is `build/diffrun --exe data/game/C/PRAGE.EXE --image-out FILE`: sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947` (E2's, E3's, P1's, P2's and P3's). Every address and instruction below is capstone 5.0.7 over that image (fixups applied); `unicorn` 2.1.4 runs the original side. Ghidra was not consulted.

---

## §P4.1/§P5.1 The member list from the raw: the roadmap's 33

`diff_emu.static_scan(..., switches=True, resolved=E.RESOLVED_JUMPS)` from each member (the tail `jmp 0x2C3FC` not followed):

| member | insns / blocks | direct callees | reached by |
|---|---|---|---|
| `0x18BC8` | 3 / 1 | - | dword E8AE2 after the opcode word D000 at E8AE0 (leaf) |
| `0x21084` | 10 / 3 | - | dword E162C after the opcode word D100 at E162A (leaf) |
| `0x400E0` | 5 / 3 | - | dword D2816 after the opcode word D000 at D2814 (leaf) |
| `0x21044` | 20 / 5 | 0x1a570 | dword E1606 after the opcode word D100 at E1604 (allow-list) |
| `0x1549C` | 24 / 3 | 0x2bc30 0x2a17c 0x2c3fc | dword D3232 after the opcode word D100 at D3230 (stubs) |
| `0x154E8` | 11 / 3 | - | dword D327C after the opcode word D100 at D327A (stubs) |
| `0x229E8` | 7 / 1 | 0x2bc30 | dword E1562 after the opcode word D500 at E1560 (stubs) |
| `0x243F8` | 24 / 1 | 0x33950 0x2bc30 | dword E503C after the opcode word D100 at E503A (stubs) |
| `0x15510` | 34 / 1 | 0x29c08 0x2ae14 | outside E2; the dwords D29FA, D2B62, D31DC (record P1 §P1.2) |
| `0x241A8` | 28 / 3 | 0x2ae14 | dword D2C48 after the opcode word D100 at D2C46 (stubs) |
| `0x40358` | 27 / 3 | 0x2ae14 | dword D4EF6 after the opcode word D100 at D4EF4 (stubs) |
| `0x45C54` | 26 / 3 | 0x2ae14 0x2c3fc | dword EB7BE after the opcode word D100 at EB7BC (stubs) |
| `0x489DC` | 28 / 4 | 0x2ae14 | dword EDAF6 after the opcode word D000 at EDAF4 (stubs) |
| `0x400EC` | 25 / 3 | 0x2bc30 0x2c3fc 0x2c3fc | dword E86F6 after the opcode word D100 at E86F4 (stubs) |
| `0x3427C` | 40 / 1 | 0x2c3fc 0x2bc30 | dword E766E after the opcode word D500 at E766C (stubs) |
| `0x34308` | 38 / 1 | 0x2bc30 | dword E4326 after the opcode word D500 at E4324 (stubs) |
| `0x3438C` | 40 / 1 | 0x2bc30 0x2c3fc | dword ED332 after the opcode word D500 at ED330 (stubs) |
| `0x34418` | 40 / 1 | 0x2c3fc 0x2bc30 | dword EAF3E after the opcode word D500 at EAF3C (stubs) |
| `0x344A4` | 40 / 1 | 0x2c3fc 0x2bc30 | dword D45E8 after the opcode word D500 at D45E6 (stubs) |
| `0x34530` | 40 / 1 | 0x2c3fc 0x2bc30 | dword D2958 after the opcode word D500 at D2956 (stubs) |
| `0x345BC` | 40 / 1 | 0x2c3fc 0x2bc30 | dword E0F24 after the opcode word D500 at E0F22 (stubs) |
| `0x156E0` | 58 / 6 | 0x1a570 0x2bc30 0x2c3fc | dword D3310 after the opcode word D100 at D330E (stubs) |
| `0x22AB8` | 35 / 6 | 0x1a570 | dword E1850 after the opcode word D100 at E184E (allow-list) |
| `0x37B70` | 68 / 11 | 0x2ae14 0x2ae14 | dword D2B28 after the opcode word D000 at D2B26 (stubs) |
| `0x3D328` | 44 / 8 | 0x2ae14 0x2c3fc | dword D4B22 after the opcode word D100 at D4B20 (stubs) |
| `0x3DA50` | 45 / 8 | 0x2ae14 0x2c3fc | dword D4BEC after the opcode word D100 at D4BEA (stubs) |
| `0x3DB8C` | 59 / 9 | 0x2ae14 | dword D4AB8 after the opcode word D000 at D4AB6 (stubs) |
| `0x3DC3C` | 59 / 9 | 0x2ae14 | dword D4B00 after the opcode word D000 at D4AFE (stubs) |
| `0x403A0` | 48 / 6 | 0x2ae14 | dword D4F62 after the opcode word D100 at D4F60 (stubs) |
| `0x40FBC` | 37 / 6 | 0x1a570 0x2ae14 0x2a17c | dword D46A4 after the opcode word D100 at D46A2 (stubs) |
| `0x48A20` | 47 / 6 | 0x2ae14 | dword EDB34 after the opcode word D100 at EDB32 (stubs) |
| `0x24508` | 25 / 1 | 0x339ac 0x2bc30 0x2c3fc | dword E50E2 after the opcode word D100 at E50E0 (stubs) |
| `0x24454` | 57 / 9 | 0x2ae14 0x2ad40 0x2bc30 | the immediate at 0x24529 inside 0x24508, the +0x10 handler (code-immediate) |

**No correction to the membership.** Every member is reached by exactly the reference listed (`0x24454` by the immediate inside `0x24508`), nothing else is reached, and no member is a block of another function. `0x15510` is outside E2 (record P1 §P1.2); `0x24454` is E2's code-immediate row. None is a Ghidra `FN_`: `port_progress.py` stays `771 1203 64` / `731 731 100`.

**The stream targets of these streams stay open** (a scratch probe walked each stream the members start with the miss log armed; record §P4.7): the P6/P7 rows `0x47E04 0x47E30 0x482E4 0x48374` (P3's) and, for the U9 replay, `0x213F0 0x213F4 0x2BDA0` (reached after this batch, §P4.7).

## §P4.2/§P5.2 Callers, registers, masks and the callee declarations

- **Stream targets** (32): the dispatcher `0x2B2A0`'s opcodes 0x10 (0xD000), 0x11 (0xD100) and 0x15 (0xD500) call DS_00105BD4 with EAX = rec (`mov eax,esi` 0x2B56B / 0x2B590 / 0x2B5E3); the dispatcher overwrites EAX after (`xor ecx,ecx; mov eax,ecx` 0x2B573/0x2B59A, `mov eax,ecx` 0x2B5F0): **mask 0**. None reads the operand's EDX or the entry ECX (each overwrites them before any read): the `anim_code_*` wrappers drop the operand.
- **`0x24454`**: the +0x10 handler `0x24508` stores; `0x3531C` case 10 calls `[slot+0x10]` with EAX = the slot, EDX = the slot's record (`mov edx,[ecx]` 0x35396), EBX = side; then `add esp,0x18` and the pops: **mask 0**. The port's `fighter_state_3531c` passes (slot, side), so `0x24454` registers the adapter `fighter_24454_case10`.

**Callee declarations** (args in the port's C order; clobbers = `E.callee_clobbers(image, addr)`; every one a plain `ret`; new seams as their first statement):

| `E.Call` | callee (port C) | args | clobbers | seam |
|---|---|---|---|---|
| `ANIM_BEGIN`, `VOICE`, `PALETTE`, `SPAWN` | `0x2BC30`, `0x2C3FC`, `0x2A17C`, `0x2AE14` | as E3/P1 | as E3/P1 | E3/P1 |
| `BIT15` (P1) | `0x1A570` `fighter_actor_bit15_clear(side)` | `eax` | none | P1 |
| `CALL29C08` (new) | `0x29C08` `fighter_29c08(side, ch)` | `eax edx` | `edx` | Task 3 |
| `RELEASE` (new) | `0x2AD40` `release_record(rec, pset)` | `eax edx` | `edx edi ebp` | Task 7 |

`0x33950` (`fighter_ctx_same`) and `0x339AC` (`hit_anim_ctx`) run on both sides (allow).

---

### `0x18BC8` (task 2)

The byte 0x100C1D = 0; no callee, no branch.

Raw (3 instructions, 1 blocks; callees: none):

```
0x18BC8: xor        ah, ah
0x18BCA: mov        byte ptr [0x100c1d], ah
0x18BD0: ret        
CALLS:
```

Port (`fighter.c`):

```c
/* 0x18BC8 — record §P4.3. The byte 0x100C1D = 0. */
void fighter_18bc8(u32 rec)
{
    (void)rec;
    DSB(P4_100C1D) = 0u;                                    /* 0x18BCA */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_18bc8 | 0x18BC8 | 1 | 1/1 | VERIFIED | - |
| fighter_18bc8@mutant | 0x18BC8 | 1 | 1/1 | MISMATCH | - |

### `0x21084` (task 2)

rec+0x14 is the held record: rec+0x1C = 0, rec+0x34 word = 0, held+0x54 = 0, held+0x57 = 2. The `je` guards the whole body.

Raw (10 instructions, 3 blocks; callees: none):

```
0x21084: push       edx
0x21085: mov        edx, dword ptr [eax + 0x14]
0x21088: test       edx, edx
0x2108A: je         0x210a1
0x2108C: mov        dword ptr [eax + 0x1c], 0
0x21093: mov        word ptr [eax + 0x34], 0
0x21099: mov        byte ptr [edx + 0x54], 0
0x2109D: mov        byte ptr [edx + 0x57], 2
0x210A1: pop        edx
0x210A2: ret        
CALLS:
```

Port (`fighter.c`):

```c
/* 0x21084 — record §P4.3. The record's +0x14 pointer; when non-zero: the
 * record's +0x1C = 0, its word +0x34 = 0, and the pointed record's +0x54 = 0,
 * +0x57 = 2. */
void fighter_21084(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x21085 */
    if (held == 0u) return;                                 /* 0x21088/0x2108A */
    DSD(rec + 0x1Cu) = 0u;                                  /* 0x2108C */
    DSW(rec + 0x34u) = 0u;                                  /* 0x21093 */
    DSB(held + 0x54u) = 0u;                                 /* 0x21099 */
    DSB(held + 0x57u) = 2u;                                 /* 0x2109D */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_21084 | 0x21084 | 2 | 3/3 | VERIFIED | - |
| fighter_21084@mutant | 0x21084 | 2 | 3/3 | MISMATCH | - |
| fighter_21084@byte14 | 0x21084 | 2 | 3/3 | MISMATCH | - |

### `0x400E0` (task 2)

rec+0x14 non-zero: held+0x42 bit 2 cleared (`and 0xfb`).

Raw (5 instructions, 3 blocks; callees: none):

```
0x400E0: mov        eax, dword ptr [eax + 0x14]
0x400E3: test       eax, eax
0x400E5: je         0x400eb
0x400E7: and        byte ptr [eax + 0x42], 0xfb
0x400EB: ret        
CALLS:
```

Port (`fighter.c`):

```c
/* 0x400E0 — record §P4.3. The record's +0x14 pointer; when non-zero its +0x42
 * bit 2 is cleared. */
void fighter_400e0(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x400E0 */
    if (held == 0u) return;                                 /* 0x400E3/0x400E5 */
    DSB(held + 0x42u) &= (u8)~0x04u;                        /* 0x400E7 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_400e0 | 0x400E0 | 3 | 3/3 | VERIFIED | - |
| fighter_400e0@mutant | 0x400E0 | 3 | 3/3 | MISMATCH | - |

### `0x21044` (task 2)

held = rec+0x14; rec+0x34 = 0x258 (negated when 0x1A570(rec+0x51) sets AL), +0x36 = 0x96, +0x44 = 0xF, +0x43 = 0xF; held+0x57 = 1.

Raw (20 instructions, 5 blocks; callees: 0x1a570):

```
0x21044: push       ebx
0x21045: push       edx
0x21046: mov        edx, eax
0x21048: mov        ebx, dword ptr [eax + 0x14]
0x2104B: test       ebx, ebx
0x2104D: je         0x2107e
0x2104F: mov        al, byte ptr [eax + 0x51]
0x21052: mov        word ptr [edx + 0x34], 0x258
0x21058: mov        word ptr [edx + 0x36], 0x96
0x2105E: mov        word ptr [edx + 0x44], 0xf
0x21064: mov        byte ptr [edx + 0x43], 0xf
0x21068: and        eax, 0xff
0x2106D: call       0x1a570
0x21072: test       al, al
0x21074: je         0x2107a
0x21076: neg        word ptr [edx + 0x34]
0x2107A: mov        byte ptr [ebx + 0x57], 1
0x2107E: pop        edx
0x2107F: pop        ebx
0x21080: ret        
CALLS: 0x1a570
```

Port (`fighter.c`):

```c
/* 0x21044 — record §P5.3. The record's +0x14 pointer; when non-zero its word
 * +0x34 = 0x258 (negated when 0x1A570(rec+0x51) sets AL), word +0x36 = 0x96,
 * word +0x44 = 0x0F, byte +0x43 = 0x0F, and the pointed record's +0x57 = 1. */
void fighter_21044(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x21048 */
    if (held == 0u) return;                                 /* 0x2104B/0x2104D */
    DSW(rec + 0x34u) = 0x0258u;                             /* 0x21052 */
    DSW(rec + 0x36u) = 0x0096u;                             /* 0x21058 */
    DSW(rec + 0x44u) = 0x000Fu;                             /* 0x2105E */
    DSB(rec + 0x43u) = 0x0Fu;                               /* 0x21064 */
    if (fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0)   /* 0x2104F/0x21068, 0x2106D..0x21074 */
        DSW(rec + 0x34u) = (u16)(0u - DSW(rec + 0x34u));    /* 0x21076 */
    DSB(held + 0x57u) = 1u;                                 /* 0x2107A */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_21044 | 0x21044 | 3 | 5/5 | VERIFIED | 1A570 stub VERIFIED |
| fighter_21044@mutant | 0x21044 | 3 | 5/5 | MISMATCH | 1A570 stub VERIFIED |
| fighter_21044@neg | 0x21044 | 3 | 5/5 | MISMATCH | 1A570 stub VERIFIED |

### `0x1549C` (task 2)

other = (rec+0x51) ^ 1 (zero-extended); the other slot from DS_001077A8; its record on the 0x9B01C stream of its character at 1.0 (0x2BC30), the palette 0x1F874590 (0x2A17C, word 0), the voice 0x50.

Raw (24 instructions, 3 blocks; callees: 0x2bc30 0x2a17c 0x2c3fc):

```
0x1549C: push       ebx
0x1549D: push       edx
0x1549E: mov        al, byte ptr [eax + 0x51]
0x154A1: xor        ebx, ebx
0x154A3: xor        al, 1
0x154A5: mov        bl, al
0x154A7: mov        ebx, dword ptr [ebx*4 + 0x1077a8]
0x154AE: test       ebx, ebx
0x154B0: je         0x154e2
0x154B2: xor        edx, edx
0x154B4: mov        dl, byte ptr [ebx + 0x7a]
0x154B7: mov        eax, dword ptr [ebx]
0x154B9: mov        edx, dword ptr [edx*4 + 0x9b01c]
0x154C0: push       0x3f800000
0x154C5: call       0x2bc30
0x154CA: mov        eax, dword ptr [ebx]
0x154CC: xor        edx, edx
0x154CE: mov        ebx, 0x1f874590
0x154D3: call       0x2a17c
0x154D8: mov        eax, 0x50
0x154DD: call       0x2c3fc
0x154E2: pop        edx
0x154E3: pop        ebx
0x154E4: ret        
CALLS: 0x2bc30 0x2a17c 0x2c3fc
```

Port (`fighter.c`):

```c
/* 0x1549C — record §P4.4. The other side's slot (DS_001077A8 indexed by
 * (rec+0x51) ^ 1, zero-extended); when non-zero: its record on the 0x9B01C
 * stream of its character at 1.0 (0x2BC30), the palette 0x1F874590 (0x2A17C,
 * word 0), and the voice 0x50. */
void fighter_1549c(u32 rec)
{
    u32 other = (u32)DSB(rec + 0x51u) ^ 1u;                 /* 0x1549E..0x154A5 */
    u32 slot = DSD(DS_001077A8 + other * 4u);               /* 0x154A7 */
    if (slot == 0u) return;                                 /* 0x154AE/0x154B0 */
    actors_anim_begin(DSD(slot), DSD(P4_STREAMS_9B01C + (u32)DSB(slot + 0x7Au) * 4u),
                      0x3F800000u);                         /* 0x154B2..0x154C5 0x2BC30 */
    actor_pset_palette(DSD(slot), 0u, 0x1F874590u);         /* 0x154CA..0x154D3 0x2A17C */
    sound_voice(0x50u);                                     /* 0x154D8/0x154DD */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_1549c | 0x1549C | 3 | 3/3 | VERIFIED | 2A17C stub unverified, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_1549c@mutant | 0x1549C | 3 | 3/3 | MISMATCH | 2A17C stub unverified, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_1549c@side | 0x1549C | 3 | 3/3 | MISMATCH | 2A17C stub unverified, 2BC30 stub unverified, 2C3FC stub unverified |

### `0x154E8` (task 2)

the same other slot; its record's +0x24 = 0x40C00000 (6.0) and the voice 0xD2 (the raw's tail `jmp 0x2C3FC`).

Raw (11 instructions, 3 blocks; callees: none):

```
0x154E4: ret        
0x154E8: mov        al, byte ptr [eax + 0x51]
0x154EB: xor        al, 1
0x154ED: and        eax, 0xff
0x154F2: mov        eax, dword ptr [eax*4 + 0x1077a8]
0x154F9: test       eax, eax
0x154FB: je         0x154e4
0x154FD: mov        eax, dword ptr [eax]
0x154FF: mov        dword ptr [eax + 0x24], 0x40c00000
0x15506: mov        eax, 0xd2
0x1550B: jmp        0x2c3fc
CALLS:
```

Port (`fighter.c`):

```c
/* 0x154E8 — record §P4.4. The other side's slot as 0x1549C; when non-zero its
 * record's +0x24 = 0x40C00000 (6.0) and the voice 0xD2 (the raw's tail `jmp
 * 0x2C3FC`). */
void fighter_154e8(u32 rec)
{
    u32 other = (u32)DSB(rec + 0x51u) ^ 1u;                 /* 0x154E8..0x154ED */
    u32 slot = DSD(DS_001077A8 + other * 4u);               /* 0x154F2 */
    if (slot == 0u) return;                                 /* 0x154F9/0x154FB */
    DSD(DSD(slot) + 0x24u) = 0x40C00000u;                   /* 0x154FD..0x15506 */
    sound_voice(0xD2u);                                     /* 0x15506/0x1550B */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_154e8 | 0x154E8 | 4 | 3/3 | VERIFIED | 2C3FC stub unverified |
| fighter_154e8@mutant | 0x154E8 | 4 | 3/3 | MISMATCH | 2C3FC stub unverified |
| fighter_154e8@side | 0x154E8 | 4 | 3/3 | MISMATCH | 2C3FC stub unverified |

### `0x229E8` (task 2)

the record on 0xE1566 at 3.0; the `lea eax,[eax]` at 0x229F9 is a nop.

Raw (7 instructions, 1 blocks; callees: 0x2bc30):

```
0x229E8: push       edx
0x229E9: mov        edx, 0xe1566
0x229EE: push       0x40400000
0x229F3: call       0x2bc30
0x229F8: pop        edx
0x229F9: lea        eax, [eax]
0x229FC: ret        
CALLS: 0x2bc30
```

Port (`fighter.c`):

```c
/* 0x229E8 — record §P4.4. The record on 0xE1566 at 3.0 (0x2BC30); the raw's
 * `lea eax,[eax]` (0x229F9) is a nop. */
void fighter_229e8(u32 rec)
{
    actors_anim_begin(rec, P4_STREAM_229E8, 0x40400000u);   /* 0x229E9..0x229F3 0x2BC30 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_229e8 | 0x229E8 | 2 | 1/1 | VERIFIED | 2BC30 stub unverified |
| fighter_229e8@mutant | 0x229E8 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified |

### `0x243F8` (task 2)

ctx 0x33950(rec+0x51); the other slot's record on its character's 0xC90F8 stream at 2.0; the other slot 0x0A/9/0 with its +0x10 = 0.

Raw (24 instructions, 1 blocks; callees: 0x33950 0x2bc30):

```
0x243F8: push       edx
0x243F9: sub        esp, 0x18
0x243FC: xor        edx, edx
0x243FE: mov        dl, byte ptr [eax + 0x51]
0x24401: mov        eax, esp
0x24403: call       0x33950
0x24408: mov        eax, dword ptr [esp + 0xc]
0x2440C: push       0x40000000
0x24411: mov        al, byte ptr [eax + 0x7a]
0x24414: and        eax, 0xff
0x24419: mov        edx, dword ptr [eax*4 + 0xc90f8]
0x24420: mov        eax, dword ptr [esp + 0x18]
0x24424: call       0x2bc30
0x24429: mov        eax, dword ptr [esp + 0xc]
0x2442D: mov        byte ptr [eax + 0x53], 0xa
0x24431: mov        eax, dword ptr [esp + 0xc]
0x24435: mov        byte ptr [eax + 0x52], 9
0x24439: mov        eax, dword ptr [esp + 0xc]
0x2443D: mov        byte ptr [eax + 0x54], 0
0x24441: mov        eax, dword ptr [esp + 0xc]
0x24445: mov        dword ptr [eax + 0x10], 0
0x2444C: add        esp, 0x18
0x2444F: pop        edx
0x24450: ret        
CALLS: 0x33950 0x2bc30
```

Port (`fighter.c`):

```c
/* 0x243F8 — record §P4.4. The context 0x33950(rec+0x51); the other slot's
 * record on its character's 0xC90F8 stream at 2.0 (0x2BC30), then that slot
 * 0x0A/9/0 with its +0x10 = 0. */
void fighter_243f8(u32 rec)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, (u32)DSB(rec + 0x51u));           /* 0x243FC..0x24403 0x33950 */
    actors_anim_begin(ctx[5], DSD(P4_STREAMS_C90F8 + (u32)DSB(ctx[3] + 0x7Au) * 4u),
                      0x40000000u);                         /* 0x24408..0x24424 0x2BC30 */
    DSB(ctx[3] + 0x53u) = 0x0Au;                            /* 0x24429/0x2442D */
    DSB(ctx[3] + 0x52u) = 9u;                               /* 0x24431/0x24435 */
    DSB(ctx[3] + 0x54u) = 0u;                               /* 0x24439/0x2443D */
    DSD(ctx[3] + 0x10u) = 0u;                               /* 0x24441/0x24445 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_243f8 | 0x243F8 | 2 | 1/1 | VERIFIED | 2BC30 stub unverified, 33950 allow VERIFIED |
| fighter_243f8@mutant | 0x243F8 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 33950 allow VERIFIED |
| fighter_243f8@slot | 0x243F8 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 33950 allow VERIFIED |

---

### `0x15510` (task 3)

the side's own slot byte 0x10782A through 0x29C08 into 0x9B08C; a child from 0x9B07C, a5 = rec+0x56 | 0x400, its +0x59 = 2, rec+0x4B its pool index.

Raw (34 instructions, 1 blocks; callees: 0x29c08 0x2ae14):

```
0x15510: push       ebx
0x15511: push       ecx
0x15512: push       edx
0x15513: push       esi
0x15514: mov        esi, eax
0x15516: xor        ebx, ebx
0x15518: mov        bl, byte ptr [eax + 0x51]
0x1551B: mov        edx, ebx
0x1551D: lea        eax, [ebx*8]
0x15524: add        eax, ebx
0x15526: shl        eax, 2
0x15529: add        eax, ebx
0x1552B: xor        edx, ebx
0x1552D: mov        dl, byte ptr [eax*4 + 0x10782a]
0x15534: mov        eax, ebx
0x15536: call       0x29c08
0x1553B: mov        dword ptr [0x9b08c], eax
0x15540: mov        ax, word ptr [esi + 0x56]
0x15544: or         ah, 4
0x15547: and        eax, 0xffff
0x1554C: xor        ecx, ecx
0x1554E: xor        ebx, ebx
0x15550: push       eax
0x15551: xor        edx, edx
0x15553: mov        eax, 0x9b07c
0x15558: call       0x2ae14
0x1555D: mov        byte ptr [eax + 0x59], 2
0x15561: mov        al, byte ptr [eax + 0x56]
0x15564: mov        byte ptr [esi + 0x4b], al
0x15567: pop        esi
0x15568: pop        edx
0x15569: pop        ecx
0x1556A: pop        ebx
0x1556B: ret        
CALLS: 0x29c08 0x2ae14
```

Port (`fighter.c`):

```c
/* 0x15510 — record §P4.4. A stream dword outside E2 (0xD2B62 and the two
 * streams that jump to 0xD2B60, record P1 §P1.2). The side's own slot byte
 * 0x10782A (the character's palette row) through 0x29C08 into 0x9B08C; the
 * child from 0x9B07C with a5 = the record's +0x56 | 0x400, its +0x59 = 2 and
 * the record's +0x4B its pool index. */
void fighter_15510(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x15516/0x15518 */
    u32 child;
    DSD(P4_9B08C) = fighter_29c08(side, (u32)DSB(DS_0010782A + side * 0x94u));   /* 0x1551B..0x1553B 0x29C08 */
    child = actor_spawn((const u32 *)(mem + P4_DESC_9B07C), 0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x15540..0x15558 0x2AE14 */
    DSB(child + 0x59u) = 2u;                                /* 0x1555D */
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);                  /* 0x15561/0x15564 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_15510 | 0x15510 | 3 | 1/1 | VERIFIED | 29C08 stub unverified, 2AE14 stub unverified |
| fighter_15510@mutant | 0x15510 | 3 | 1/1 | MISMATCH | 29C08 stub unverified, 2AE14 stub unverified |
| fighter_15510@side | 0x15510 | 3 | 1/1 | MISMATCH | 29C08 stub unverified, 2AE14 stub unverified |
| fighter_15510@pal | 0x15510 | 3 | 1/1 | MISMATCH | 29C08 stub unverified, 2AE14 stub unverified |
| fighter_15510@a5 | 0x15510 | 3 | 1/1 | MISMATCH | 29C08 stub unverified, 2AE14 stub unverified |

### `0x241A8` (task 3)

the side's slot and its character's 0xA84E0 descriptor; a5 = rec+0x56 | 0x400; rec+0x4B the pool index.

Raw (28 instructions, 3 blocks; callees: 0x2ae14):

```
0x241A8: push       ebx
0x241A9: push       ecx
0x241AA: push       edx
0x241AB: push       esi
0x241AC: mov        esi, eax
0x241AE: xor        eax, eax
0x241B0: mov        al, byte ptr [esi + 0x51]
0x241B3: mov        eax, dword ptr [eax*4 + 0x1077a8]
0x241BA: test       eax, eax
0x241BC: je         0x241ec
0x241BE: mov        dx, word ptr [esi + 0x56]
0x241C2: or         dh, 4
0x241C5: and        edx, 0xffff
0x241CB: mov        al, byte ptr [eax + 0x7a]
0x241CE: push       edx
0x241CF: and        eax, 0xff
0x241D4: xor        ecx, ecx
0x241D6: xor        ebx, ebx
0x241D8: xor        edx, edx
0x241DA: mov        eax, dword ptr [eax*4 + 0xa84e0]
0x241E1: call       0x2ae14
0x241E6: mov        al, byte ptr [eax + 0x56]
0x241E9: mov        byte ptr [esi + 0x4b], al
0x241EC: pop        esi
0x241ED: pop        edx
0x241EE: pop        ecx
0x241EF: pop        ebx
0x241F0: ret        
CALLS: 0x2ae14
```

Port (`fighter.c`):

```c
/* 0x241A8 — record §P4.4. The side's slot (rec+0x51) and its character's
 * 0xA84E0 descriptor; the child's a5 = the record's +0x56 | 0x400 and the
 * record's +0x4B its pool index. */
void fighter_241a8(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x241AE/0x241B0 */
    u32 slot = DSD(DS_001077A8 + side * 4u);                /* 0x241B3 */
    u32 child;
    if (slot == 0u) return;                                 /* 0x241BA/0x241BC */
    child = actor_spawn((const u32 *)(mem + DSD(P4_DESC_A84E0 + (u32)DSB(slot + 0x7Au) * 4u)),
                        0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x241BE..0x241E1 0x2AE14 */
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);                  /* 0x241E6/0x241E9 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_241a8 | 0x241A8 | 4 | 3/3 | VERIFIED | 2AE14 stub unverified |
| fighter_241a8@mutant | 0x241A8 | 4 | 3/3 | MISMATCH | 2AE14 stub unverified |
| fighter_241a8@sext | 0x241A8 | 4 | 3/3 | MISMATCH | 2AE14 stub unverified |

### `0x40358` (task 3)

held = rec+0x14; a child from 0xC7758 with a2 = -4, a4 = -10, a5 = rec+0x56 | 0x400; the child's +0x14 = held, +0x51 = rec+0x51; rec+0x4B its index.

Raw (27 instructions, 3 blocks; callees: 0x2ae14):

```
0x40358: push       ebx
0x40359: push       ecx
0x4035A: push       edx
0x4035B: push       esi
0x4035C: mov        esi, eax
0x4035E: cmp        dword ptr [eax + 0x14], 0
0x40362: je         0x40399
0x40364: mov        ax, word ptr [eax + 0x56]
0x40368: or         ah, 4
0x4036B: mov        ebx, 0xfffffff6
0x40370: and        eax, 0xffff
0x40375: mov        edx, 0xfffffffc
0x4037A: push       eax
0x4037B: xor        ecx, ecx
0x4037D: mov        eax, 0xc7758
0x40382: call       0x2ae14
0x40387: mov        edx, dword ptr [esi + 0x14]
0x4038A: mov        dword ptr [eax + 0x14], edx
0x4038D: mov        dl, byte ptr [esi + 0x51]
0x40390: mov        byte ptr [eax + 0x51], dl
0x40393: mov        al, byte ptr [eax + 0x56]
0x40396: mov        byte ptr [esi + 0x4b], al
0x40399: pop        esi
0x4039A: pop        edx
0x4039B: pop        ecx
0x4039C: pop        ebx
0x4039D: ret        
CALLS: 0x2ae14
```

Port (`fighter.c`):

```c
/* 0x40358 — record §P4.4. The record's +0x14 pointer; when non-zero a child
 * from 0xC7758 with a2 = -4, a4 = -10, a5 = the record's +0x56 | 0x400, the
 * held record and the record's side copied into it, and the record's +0x4B
 * its pool index. */
void fighter_40358(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x4035E */
    u32 child;
    if (held == 0u) return;                                 /* 0x40362 */
    child = actor_spawn((const u32 *)(mem + P4_DESC_C7758), 0xFFFFFFFCu, 0u, 0xFFFFFFF6u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x40364..0x40382 0x2AE14 */
    DSD(child + 0x14u) = held;                              /* 0x40387/0x4038A */
    DSB(child + 0x51u) = DSB(rec + 0x51u);                  /* 0x4038D/0x40390 */
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);                  /* 0x40393/0x40396 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_40358 | 0x40358 | 2 | 3/3 | VERIFIED | 2AE14 stub unverified |
| fighter_40358@mutant | 0x40358 | 2 | 3/3 | MISMATCH | 2AE14 stub unverified |

### `0x45C54` (task 3)

held = rec+0x14; a child from 0xC9360, its +0x59 = 2, +0x14 = held, the voice 0x5F.

Raw (26 instructions, 3 blocks; callees: 0x2ae14 0x2c3fc):

```
0x45C54: push       ebx
0x45C55: push       ecx
0x45C56: push       edx
0x45C57: push       esi
0x45C58: mov        esi, eax
0x45C5A: cmp        dword ptr [eax + 0x14], 0
0x45C5E: je         0x45c91
0x45C60: mov        ax, word ptr [eax + 0x56]
0x45C64: or         ah, 4
0x45C67: and        eax, 0xffff
0x45C6C: xor        ecx, ecx
0x45C6E: xor        ebx, ebx
0x45C70: push       eax
0x45C71: xor        edx, edx
0x45C73: mov        eax, 0xc9360
0x45C78: call       0x2ae14
0x45C7D: mov        esi, dword ptr [esi + 0x14]
0x45C80: mov        byte ptr [eax + 0x59], 2
0x45C84: mov        dword ptr [eax + 0x14], esi
0x45C87: mov        eax, 0x5f
0x45C8C: call       0x2c3fc
0x45C91: pop        esi
0x45C92: pop        edx
0x45C93: pop        ecx
0x45C94: pop        ebx
0x45C95: ret        
CALLS: 0x2ae14 0x2c3fc
```

Port (`fighter.c`):

```c
/* 0x45C54 — record §P4.4. The record's +0x14 pointer; when non-zero a child
 * from 0xC9360 with a5 = the record's +0x56 | 0x400, its +0x59 = 2, the held
 * record, and the voice 0x5F. */
void fighter_45c54(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x45C5A */
    u32 child;
    if (held == 0u) return;                                 /* 0x45C5E */
    child = actor_spawn((const u32 *)(mem + P4_DESC_C9360), 0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x45C60..0x45C78 0x2AE14 */
    DSB(child + 0x59u) = 2u;                                /* 0x45C80 */
    DSD(child + 0x14u) = held;                              /* 0x45C84 */
    sound_voice(0x5Fu);                                     /* 0x45C87/0x45C8C */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_45c54 | 0x45C54 | 2 | 3/3 | VERIFIED | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_45c54@mutant | 0x45C54 | 2 | 3/3 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |

### `0x489DC` (task 3)

held = rec+0x14; the pointed record's +0x4B must be zero; a child from 0xBB13C, +0x59 = 1, the pointed record's +0x4B its index.

Raw (28 instructions, 4 blocks; callees: 0x2ae14):

```
0x489DC: push       ebx
0x489DD: push       ecx
0x489DE: push       edx
0x489DF: push       esi
0x489E0: mov        esi, dword ptr [eax + 0x14]
0x489E3: test       esi, esi
0x489E5: je         0x48a18
0x489E7: mov        edx, dword ptr [esi]
0x489E9: cmp        byte ptr [edx + 0x4b], 0
0x489ED: jne        0x48a18
0x489EF: mov        ax, word ptr [eax + 0x56]
0x489F3: or         ah, 4
0x489F6: and        eax, 0xffff
0x489FB: xor        ecx, ecx
0x489FD: xor        ebx, ebx
0x489FF: push       eax
0x48A00: xor        edx, edx
0x48A02: mov        eax, 0xbb13c
0x48A07: call       0x2ae14
0x48A0C: mov        byte ptr [eax + 0x59], 1
0x48A10: mov        edx, dword ptr [esi]
0x48A12: mov        al, byte ptr [eax + 0x56]
0x48A15: mov        byte ptr [edx + 0x4b], al
0x48A18: pop        esi
0x48A19: pop        edx
0x48A1A: pop        ecx
0x48A1B: pop        ebx
0x48A1C: ret        
CALLS: 0x2ae14
```

Port (`fighter.c`):

```c
/* 0x489DC — record §P4.4. The record's +0x14 pointer; when non-zero and the
 * pointed record's +0x4B is zero, a child from 0xBB13C with a5 = the record's
 * +0x56 | 0x400, its +0x59 = 1, and the pointed record's +0x4B its pool
 * index. */
void fighter_489dc(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x489E0 */
    u32 hrec;
    u32 child;
    if (held == 0u) return;                                 /* 0x489E3/0x489E5 */
    hrec = DSD(held);                                       /* 0x489E7 */
    if (DSB(hrec + 0x4Bu) != 0u) return;                    /* 0x489E9/0x489ED */
    child = actor_spawn((const u32 *)(mem + P4_DESC_BB13C), 0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x489EF..0x48A07 0x2AE14 */
    DSB(child + 0x59u) = 1u;                                /* 0x48A0C */
    DSB(hrec + 0x4Bu) = DSB(child + 0x56u);                 /* 0x48A10/0x48A15 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_489dc | 0x489DC | 3 | 4/4 | VERIFIED | 2AE14 stub unverified |
| fighter_489dc@mutant | 0x489DC | 3 | 4/4 | MISMATCH | 2AE14 stub unverified |

### `0x400EC` (task 3)

other = (rec+0x51) ^ 1; the other slot's record on its character's 0xC90F8 stream at 2.0, the voice 0xC75AA[char] (a zero-extended word), the voice 0x59, 0x104AE9 bit 2.

Raw (25 instructions, 3 blocks; callees: 0x2bc30 0x2c3fc 0x2c3fc):

```
0x400EC: push       ebx
0x400ED: push       edx
0x400EE: mov        al, byte ptr [eax + 0x51]
0x400F1: xor        al, 1
0x400F3: and        eax, 0xff
0x400F8: mov        ebx, dword ptr [eax*4 + 0x1077a8]
0x400FF: test       ebx, ebx
0x40101: je         0x40143
0x40103: xor        edx, edx
0x40105: mov        dl, byte ptr [ebx + 0x7a]
0x40108: mov        eax, dword ptr [ebx]
0x4010A: mov        edx, dword ptr [edx*4 + 0xc90f8]
0x40111: push       0x40000000
0x40116: call       0x2bc30
0x4011B: xor        eax, eax
0x4011D: mov        al, byte ptr [ebx + 0x7a]
0x40120: mov        ax, word ptr [eax*2 + 0xc75aa]
0x40128: and        eax, 0xffff
0x4012D: call       0x2c3fc
0x40132: mov        eax, 0x59
0x40137: call       0x2c3fc
0x4013C: or         byte ptr [0x104ae9], 4
0x40143: pop        edx
0x40144: pop        ebx
0x40145: ret        
CALLS: 0x2bc30 0x2c3fc 0x2c3fc
```

Port (`fighter.c`):

```c
/* 0x400EC — record §P4.4. The other side's slot (rec+0x51 ^ 1, zero-extended);
 * when non-zero: its record on its character's 0xC90F8 stream at 2.0 (0x2BC30),
 * the voice 0xC75AA[char] (a zero-extended word), the voice 0x59, and
 * 0x104AE9 bit 2 set. */
void fighter_400ec(u32 rec)
{
    u32 other = (u32)DSB(rec + 0x51u) ^ 1u;                 /* 0x400EE..0x400F3 */
    u32 slot = DSD(DS_001077A8 + other * 4u);               /* 0x400F8 */
    u32 ch;
    if (slot == 0u) return;                                 /* 0x400FF/0x40101 */
    ch = (u32)DSB(slot + 0x7Au);                            /* 0x40103/0x40105 */
    actors_anim_begin(DSD(slot), DSD(P4_STREAMS_C90F8 + ch * 4u), 0x40000000u);   /* 0x40108..0x40116 */
    sound_voice((u32)DSW(P4_VOICES_C75AA + ch * 2u));       /* 0x4011B..0x4012D */
    sound_voice(0x59u);                                     /* 0x40132/0x40137 */
    DSB(P4_104AE9) |= 0x04u;                                /* 0x4013C */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_400ec | 0x400EC | 3 | 3/3 | VERIFIED | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_400ec@mutant | 0x400EC | 3 | 3/3 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_400ec@side | 0x400EC | 3 | 3/3 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified |

---

### `0x3427C` (task 4)

the side's own slot record on 0xE76B2 at 3.0, after the voice 0x8C.

Raw (40 instructions, 1 blocks; callees: 0x2c3fc 0x2bc30):

```
0x3427C: push       edx
0x3427D: sub        esp, 0x18
0x34280: mov        al, byte ptr [eax + 0x51]
0x34283: and        eax, 0xff
0x34288: mov        dword ptr [esp], eax
0x3428B: mov        eax, 1
0x34290: mov        edx, dword ptr [esp]
0x34293: sub        eax, edx
0x34295: mov        dword ptr [esp + 4], eax
0x34299: lea        eax, [edx*8]
0x342A0: add        eax, edx
0x342A2: shl        eax, 2
0x342A5: add        eax, edx
0x342A7: mov        edx, 0x1077b0
0x342AC: shl        eax, 2
0x342AF: add        edx, eax
0x342B1: mov        dword ptr [esp + 8], edx
0x342B5: mov        edx, dword ptr [esp + 4]
0x342B9: lea        eax, [edx*8]
0x342C0: add        eax, edx
0x342C2: shl        eax, 2
0x342C5: add        eax, edx
0x342C7: mov        edx, 0x1077b0
0x342CC: shl        eax, 2
0x342CF: add        edx, eax
0x342D1: mov        eax, dword ptr [esp + 8]
0x342D5: mov        eax, dword ptr [eax]
0x342D7: mov        dword ptr [esp + 0x10], eax
0x342DB: mov        eax, dword ptr [edx]
0x342DD: mov        dword ptr [esp + 0xc], edx
0x342E1: mov        dword ptr [esp + 0x14], eax
0x342E5: mov        eax, 0x8c
0x342EA: mov        edx, 0xe76b2
0x342EF: call       0x2c3fc
0x342F4: mov        eax, dword ptr [esp + 0x10]
0x342F8: push       0x40400000
0x342FD: call       0x2bc30
0x34302: add        esp, 0x18
0x34305: pop        edx
0x34306: ret        
CALLS: 0x2c3fc 0x2bc30
```

Port (`fighter.c`):

```c
/* 0x3427C — record §P4.5. The side's own slot record (DS_001077B0 +
 * (rec+0x51) * 0x94, the byte zero-extended) on 0xE76B2 at 3.0, after the
 * voice 0x8C. The raw builds the two slot pointers inline (no 0x33950 call). */
void fighter_3427c(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x34280..0x34288 */
    u32 own = DS_001077B0 + side * 0x94u;                   /* 0x34299..0x342B1 */
    sound_voice(0x8Cu);                                     /* 0x342E5..0x342EF */
    actors_anim_begin(DSD(own), P4_STREAM_3427C, 0x40400000u);   /* 0x342F4..0x342FD */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_3427c | 0x3427C | 3 | 1/1 | VERIFIED; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_3427c@mutant | 0x3427C | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_3427c@side | 0x3427C | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_3427c@voice | 0x3427C | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |

### `0x34308` (task 4)

the same on 0xE436A at 3.0, no voice.

Raw (38 instructions, 1 blocks; callees: 0x2bc30):

```
0x34308: push       edx
0x34309: sub        esp, 0x18
0x3430C: mov        al, byte ptr [eax + 0x51]
0x3430F: and        eax, 0xff
0x34314: mov        dword ptr [esp], eax
0x34317: mov        eax, 1
0x3431C: mov        edx, dword ptr [esp]
0x3431F: sub        eax, edx
0x34321: mov        dword ptr [esp + 4], eax
0x34325: lea        eax, [edx*8]
0x3432C: add        eax, edx
0x3432E: shl        eax, 2
0x34331: add        eax, edx
0x34333: mov        edx, 0x1077b0
0x34338: shl        eax, 2
0x3433B: add        edx, eax
0x3433D: mov        dword ptr [esp + 8], edx
0x34341: mov        edx, dword ptr [esp + 4]
0x34345: lea        eax, [edx*8]
0x3434C: add        eax, edx
0x3434E: shl        eax, 2
0x34351: add        eax, edx
0x34353: mov        edx, 0x1077b0
0x34358: shl        eax, 2
0x3435B: add        edx, eax
0x3435D: mov        eax, dword ptr [esp + 8]
0x34361: mov        eax, dword ptr [eax]
0x34363: mov        dword ptr [esp + 0xc], edx
0x34367: mov        dword ptr [esp + 0x10], eax
0x3436B: mov        eax, dword ptr [edx]
0x3436D: mov        edx, 0xe436a
0x34372: mov        dword ptr [esp + 0x14], eax
0x34376: mov        eax, dword ptr [esp + 0x10]
0x3437A: push       0x40400000
0x3437F: call       0x2bc30
0x34384: add        esp, 0x18
0x34387: pop        edx
0x34388: ret        
CALLS: 0x2bc30
```

Port (`fighter.c`):

```c
/* 0x34308 — record §P4.5. The same slot record on 0xE436A at 3.0, no voice. */
void fighter_34308(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x3430C..0x34314 */
    u32 own = DS_001077B0 + side * 0x94u;                   /* 0x34325..0x3433D */
    actors_anim_begin(DSD(own), P4_STREAM_34308, 0x40400000u);   /* 0x34376..0x3437F */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_34308 | 0x34308 | 3 | 1/1 | VERIFIED; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified |
| fighter_34308@mutant | 0x34308 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified |
| fighter_34308@side | 0x34308 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified |

### `0x3438C` (task 4)

the same on 0xED354 at 1.0, the voice 0xA6 *after* the stream.

Raw (40 instructions, 1 blocks; callees: 0x2bc30 0x2c3fc):

```
0x3438C: push       edx
0x3438D: sub        esp, 0x18
0x34390: mov        al, byte ptr [eax + 0x51]
0x34393: and        eax, 0xff
0x34398: mov        dword ptr [esp], eax
0x3439B: mov        eax, 1
0x343A0: mov        edx, dword ptr [esp]
0x343A3: sub        eax, edx
0x343A5: mov        dword ptr [esp + 4], eax
0x343A9: lea        eax, [edx*8]
0x343B0: add        eax, edx
0x343B2: shl        eax, 2
0x343B5: add        eax, edx
0x343B7: mov        edx, 0x1077b0
0x343BC: shl        eax, 2
0x343BF: add        edx, eax
0x343C1: mov        dword ptr [esp + 8], edx
0x343C5: mov        edx, dword ptr [esp + 4]
0x343C9: lea        eax, [edx*8]
0x343D0: add        eax, edx
0x343D2: shl        eax, 2
0x343D5: add        eax, edx
0x343D7: mov        edx, 0x1077b0
0x343DC: shl        eax, 2
0x343DF: add        edx, eax
0x343E1: mov        eax, dword ptr [esp + 8]
0x343E5: mov        eax, dword ptr [eax]
0x343E7: mov        dword ptr [esp + 0xc], edx
0x343EB: mov        dword ptr [esp + 0x10], eax
0x343EF: mov        eax, dword ptr [edx]
0x343F1: mov        edx, 0xed354
0x343F6: mov        dword ptr [esp + 0x14], eax
0x343FA: mov        eax, dword ptr [esp + 0x10]
0x343FE: push       0x3f800000
0x34403: call       0x2bc30
0x34408: mov        eax, 0xa6
0x3440D: call       0x2c3fc
0x34412: add        esp, 0x18
0x34415: pop        edx
0x34416: ret        
CALLS: 0x2bc30 0x2c3fc
```

Port (`fighter.c`):

```c
/* 0x3438C — record §P4.5. The same slot record on 0xED354 at 1.0, after the voice 0xA6. */
void fighter_3438c(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x34390..0x34398 */
    u32 own = DS_001077B0 + side * 0x94u;                   /* 0x343A9..0x343C1 */
    actors_anim_begin(DSD(own), P4_STREAM_3438C, 0x3F800000u);   /* 0x343FE..0x34403 */
    sound_voice(0xA6u);                                     /* 0x34408..0x3440D */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_3438c | 0x3438C | 3 | 1/1 | VERIFIED; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_3438c@mutant | 0x3438C | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_3438c@side | 0x3438C | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |

### `0x34418` (task 4)

the same on 0xEAF66 at 3.0, the voice 0x84 before.

Raw (40 instructions, 1 blocks; callees: 0x2c3fc 0x2bc30):

```
0x34418: push       edx
0x34419: sub        esp, 0x18
0x3441C: mov        al, byte ptr [eax + 0x51]
0x3441F: and        eax, 0xff
0x34424: mov        dword ptr [esp], eax
0x34427: mov        eax, 1
0x3442C: mov        edx, dword ptr [esp]
0x3442F: sub        eax, edx
0x34431: mov        dword ptr [esp + 4], eax
0x34435: lea        eax, [edx*8]
0x3443C: add        eax, edx
0x3443E: shl        eax, 2
0x34441: add        eax, edx
0x34443: mov        edx, 0x1077b0
0x34448: shl        eax, 2
0x3444B: add        edx, eax
0x3444D: mov        dword ptr [esp + 8], edx
0x34451: mov        edx, dword ptr [esp + 4]
0x34455: lea        eax, [edx*8]
0x3445C: add        eax, edx
0x3445E: shl        eax, 2
0x34461: add        eax, edx
0x34463: mov        edx, 0x1077b0
0x34468: shl        eax, 2
0x3446B: add        edx, eax
0x3446D: mov        eax, dword ptr [esp + 8]
0x34471: mov        eax, dword ptr [eax]
0x34473: mov        dword ptr [esp + 0x10], eax
0x34477: mov        eax, dword ptr [edx]
0x34479: mov        dword ptr [esp + 0xc], edx
0x3447D: mov        dword ptr [esp + 0x14], eax
0x34481: mov        eax, 0x84
0x34486: mov        edx, 0xeaf66
0x3448B: call       0x2c3fc
0x34490: mov        eax, dword ptr [esp + 0x10]
0x34494: push       0x40400000
0x34499: call       0x2bc30
0x3449E: add        esp, 0x18
0x344A1: pop        edx
0x344A2: ret        
CALLS: 0x2c3fc 0x2bc30
```

Port (`fighter.c`):

```c
/* 0x34418 — record §P4.5. The same slot record on 0xEAF66 at 3.0, after the voice 0x84. */
void fighter_34418(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x3441C..0x34424 */
    u32 own = DS_001077B0 + side * 0x94u;                   /* 0x34435..0x3444D */
    sound_voice(0x84u);                                     /* 0x34481..0x3448B */
    actors_anim_begin(DSD(own), P4_STREAM_34418, 0x40400000u);   /* 0x34490..0x34499 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_34418 | 0x34418 | 3 | 1/1 | VERIFIED; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_34418@mutant | 0x34418 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_34418@side | 0x34418 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |

### `0x344A4` (task 4)

the same on 0xD461C at 3.0, the voice 0x86 before.

Raw (40 instructions, 1 blocks; callees: 0x2c3fc 0x2bc30):

```
0x344A4: push       edx
0x344A5: sub        esp, 0x18
0x344A8: mov        al, byte ptr [eax + 0x51]
0x344AB: and        eax, 0xff
0x344B0: mov        dword ptr [esp], eax
0x344B3: mov        eax, 1
0x344B8: mov        edx, dword ptr [esp]
0x344BB: sub        eax, edx
0x344BD: mov        dword ptr [esp + 4], eax
0x344C1: lea        eax, [edx*8]
0x344C8: add        eax, edx
0x344CA: shl        eax, 2
0x344CD: add        eax, edx
0x344CF: mov        edx, 0x1077b0
0x344D4: shl        eax, 2
0x344D7: add        edx, eax
0x344D9: mov        dword ptr [esp + 8], edx
0x344DD: mov        edx, dword ptr [esp + 4]
0x344E1: lea        eax, [edx*8]
0x344E8: add        eax, edx
0x344EA: shl        eax, 2
0x344ED: add        eax, edx
0x344EF: mov        edx, 0x1077b0
0x344F4: shl        eax, 2
0x344F7: add        edx, eax
0x344F9: mov        eax, dword ptr [esp + 8]
0x344FD: mov        eax, dword ptr [eax]
0x344FF: mov        dword ptr [esp + 0x10], eax
0x34503: mov        eax, dword ptr [edx]
0x34505: mov        dword ptr [esp + 0xc], edx
0x34509: mov        dword ptr [esp + 0x14], eax
0x3450D: mov        eax, 0x86
0x34512: mov        edx, 0xd461c
0x34517: call       0x2c3fc
0x3451C: mov        eax, dword ptr [esp + 0x10]
0x34520: push       0x40400000
0x34525: call       0x2bc30
0x3452A: add        esp, 0x18
0x3452D: pop        edx
0x3452E: ret        
CALLS: 0x2c3fc 0x2bc30
```

Port (`fighter.c`):

```c
/* 0x344A4 — record §P4.5. The same slot record on 0xD461C at 3.0, after the voice 0x86. */
void fighter_344a4(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x344A8..0x344B0 */
    u32 own = DS_001077B0 + side * 0x94u;                   /* 0x344C1..0x344D9 */
    sound_voice(0x86u);                                     /* 0x3450D..0x34517 */
    actors_anim_begin(DSD(own), P4_STREAM_344A4, 0x40400000u);   /* 0x3451C..0x34525 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_344a4 | 0x344A4 | 3 | 1/1 | VERIFIED; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_344a4@mutant | 0x344A4 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_344a4@side | 0x344A4 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |

### `0x34530` (task 4)

the same on 0xD299C at 3.0, the voice 0x9C before.

Raw (40 instructions, 1 blocks; callees: 0x2c3fc 0x2bc30):

```
0x34530: push       edx
0x34531: sub        esp, 0x18
0x34534: mov        al, byte ptr [eax + 0x51]
0x34537: and        eax, 0xff
0x3453C: mov        dword ptr [esp], eax
0x3453F: mov        eax, 1
0x34544: mov        edx, dword ptr [esp]
0x34547: sub        eax, edx
0x34549: mov        dword ptr [esp + 4], eax
0x3454D: lea        eax, [edx*8]
0x34554: add        eax, edx
0x34556: shl        eax, 2
0x34559: add        eax, edx
0x3455B: mov        edx, 0x1077b0
0x34560: shl        eax, 2
0x34563: add        edx, eax
0x34565: mov        dword ptr [esp + 8], edx
0x34569: mov        edx, dword ptr [esp + 4]
0x3456D: lea        eax, [edx*8]
0x34574: add        eax, edx
0x34576: shl        eax, 2
0x34579: add        eax, edx
0x3457B: mov        edx, 0x1077b0
0x34580: shl        eax, 2
0x34583: add        edx, eax
0x34585: mov        eax, dword ptr [esp + 8]
0x34589: mov        eax, dword ptr [eax]
0x3458B: mov        dword ptr [esp + 0x10], eax
0x3458F: mov        eax, dword ptr [edx]
0x34591: mov        dword ptr [esp + 0xc], edx
0x34595: mov        dword ptr [esp + 0x14], eax
0x34599: mov        eax, 0x9c
0x3459E: mov        edx, 0xd299c
0x345A3: call       0x2c3fc
0x345A8: mov        eax, dword ptr [esp + 0x10]
0x345AC: push       0x40400000
0x345B1: call       0x2bc30
0x345B6: add        esp, 0x18
0x345B9: pop        edx
0x345BA: ret        
CALLS: 0x2c3fc 0x2bc30
```

Port (`fighter.c`):

```c
/* 0x34530 — record §P4.5. The same slot record on 0xD299C at 3.0, after the voice 0x9C. */
void fighter_34530(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x34534..0x3453C */
    u32 own = DS_001077B0 + side * 0x94u;                   /* 0x3454D..0x34565 */
    sound_voice(0x9Cu);                                     /* 0x34599..0x345A3 */
    actors_anim_begin(DSD(own), P4_STREAM_34530, 0x40400000u);   /* 0x345A8..0x345B1 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_34530 | 0x34530 | 3 | 1/1 | VERIFIED; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_34530@mutant | 0x34530 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_34530@side | 0x34530 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |

### `0x345BC` (task 4)

the same on 0xE0F62 at 3.0, the voice 0x9B before.

Raw (40 instructions, 1 blocks; callees: 0x2c3fc 0x2bc30):

```
0x345BC: push       edx
0x345BD: sub        esp, 0x18
0x345C0: mov        al, byte ptr [eax + 0x51]
0x345C3: and        eax, 0xff
0x345C8: mov        dword ptr [esp], eax
0x345CB: mov        eax, 1
0x345D0: mov        edx, dword ptr [esp]
0x345D3: sub        eax, edx
0x345D5: mov        dword ptr [esp + 4], eax
0x345D9: lea        eax, [edx*8]
0x345E0: add        eax, edx
0x345E2: shl        eax, 2
0x345E5: add        eax, edx
0x345E7: mov        edx, 0x1077b0
0x345EC: shl        eax, 2
0x345EF: add        edx, eax
0x345F1: mov        dword ptr [esp + 8], edx
0x345F5: mov        edx, dword ptr [esp + 4]
0x345F9: lea        eax, [edx*8]
0x34600: add        eax, edx
0x34602: shl        eax, 2
0x34605: add        eax, edx
0x34607: mov        edx, 0x1077b0
0x3460C: shl        eax, 2
0x3460F: add        edx, eax
0x34611: mov        eax, dword ptr [esp + 8]
0x34615: mov        eax, dword ptr [eax]
0x34617: mov        dword ptr [esp + 0x10], eax
0x3461B: mov        eax, dword ptr [edx]
0x3461D: mov        dword ptr [esp + 0xc], edx
0x34621: mov        dword ptr [esp + 0x14], eax
0x34625: mov        eax, 0x9b
0x3462A: mov        edx, 0xe0f62
0x3462F: call       0x2c3fc
0x34634: mov        eax, dword ptr [esp + 0x10]
0x34638: push       0x40400000
0x3463D: call       0x2bc30
0x34642: add        esp, 0x18
0x34645: pop        edx
0x34646: ret        
CALLS: 0x2c3fc 0x2bc30
```

Port (`fighter.c`):

```c
/* 0x345BC — record §P4.5. The same slot record on 0xE0F62 at 3.0, after the voice 0x9B. */
void fighter_345bc(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x345C0..0x345C8 */
    u32 own = DS_001077B0 + side * 0x94u;                   /* 0x345D9..0x345F1 */
    sound_voice(0x9Bu);                                     /* 0x34625..0x3462F */
    actors_anim_begin(DSD(own), P4_STREAM_345BC, 0x40400000u);   /* 0x34634..0x3463D */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_345bc | 0x345BC | 3 | 1/1 | VERIFIED; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_345bc@mutant | 0x345BC | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_345bc@side | 0x345BC | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |

---

### `0x156E0` (task 5)

other = (rec+0x51) ^ 1; its record's +0x18 shifted by the signed byte 0xC9786 * 0x40 (0x1A570(rec+0x51) picks the sign) and its +0x1C by the signed byte 0xC9787 * 0x40; that record on its character's 0x9B038 stream at 0.0; rec+0x4B its pool index; 0xF0AFE = 0, 0xF0AFF = the side; the voice 0xD6.

Raw (58 instructions, 6 blocks; callees: 0x1a570 0x2bc30 0x2c3fc):

```
0x156E0: push       ebx
0x156E1: push       ecx
0x156E2: push       edx
0x156E3: push       esi
0x156E4: mov        ebx, eax
0x156E6: mov        al, byte ptr [eax + 0x51]
0x156E9: xor        al, 1
0x156EB: and        eax, 0xff
0x156F0: mov        ecx, dword ptr [eax*4 + 0x1077a8]
0x156F7: test       ecx, ecx
0x156F9: je         0x15785
0x156FF: xor        eax, eax
0x15701: mov        al, byte ptr [ebx + 0x51]
0x15704: call       0x1a570
0x15709: test       al, al
0x1570B: je         0x15724
0x1570D: mov        eax, dword ptr [0xc9783]
0x15712: sar        eax, 0x18
0x15715: mov        edx, dword ptr [ebx + 0x18]
0x15718: shl        eax, 6
0x1571B: sub        edx, eax
0x1571D: mov        eax, dword ptr [ecx]
0x1571F: mov        dword ptr [eax + 0x18], edx
0x15722: jmp        0x15739
0x15724: mov        eax, dword ptr [0xc9783]
0x15729: sar        eax, 0x18
0x1572C: mov        edx, dword ptr [ebx + 0x18]
0x1572F: shl        eax, 6
0x15732: add        eax, edx
0x15734: mov        edx, dword ptr [ecx]
0x15736: mov        dword ptr [edx + 0x18], eax
0x15739: mov        eax, dword ptr [0xc9784]
0x1573E: sar        eax, 0x18
0x15741: mov        esi, dword ptr [ebx + 0x1c]
0x15744: shl        eax, 6
0x15747: mov        edx, dword ptr [ecx]
0x15749: add        eax, esi
0x1574B: mov        dword ptr [edx + 0x1c], eax
0x1574E: xor        edx, edx
0x15750: mov        dl, byte ptr [ecx + 0x7a]
0x15753: mov        eax, dword ptr [ecx]
0x15755: mov        edx, dword ptr [edx*4 + 0x9b038]
0x1575C: push       0
0x1575E: call       0x2bc30
0x15763: mov        eax, dword ptr [ecx]
0x15765: mov        al, byte ptr [eax + 0x56]
0x15768: mov        byte ptr [ebx + 0x4b], al
0x1576B: xor        ah, ah
0x1576D: mov        byte ptr [0xf0afe], ah
0x15773: mov        al, byte ptr [ebx + 0x51]
0x15776: mov        byte ptr [0xf0aff], al
0x1577B: mov        eax, 0xd6
0x15780: call       0x2c3fc
0x15785: pop        esi
0x15786: pop        edx
0x15787: pop        ecx
0x15788: pop        ebx
0x15789: ret        
CALLS: 0x1a570 0x2bc30 0x2c3fc
```

Port (`fighter.c`):

```c
/* 0x156E0 — record §P5.3. The other side's slot (rec+0x51 ^ 1); when non-zero,
 * its record's +0x18 shifted by the signed byte 0xC9786 * 0x40 (the raw's
 * `mov eax,[0xC9783]; sar eax,0x18`, whose argument is the record's own side:
 * 0x1A570(rec+0x51)) and its +0x1C by the signed byte 0xC9787 * 0x40; then
 * that record on its character's 0x9B038 stream at 0.0 (0x2BC30), the record's
 * +0x4B its pool index, 0xF0AFE = 0, 0xF0AFF = the side, and the voice 0xD6. */
void fighter_156e0(u32 rec)
{
    u32 other = (u32)DSB(rec + 0x51u) ^ 1u;                 /* 0x156E6..0x156EB */
    u32 slot = DSD(DS_001077A8 + other * 4u);               /* 0x156F0 */
    u32 srec;
    if (slot == 0u) return;                                 /* 0x156F7/0x156F9 */
    srec = DSD(slot);                                       /* 0x156FF..0x15704 (0x1A570's rec) */
    if (fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0) {   /* 0x15704..0x1570B 0x1A570 */
        DSD(srec + 0x18u) = DSD(rec + 0x18u) - (u32)((s32)DSD(P5_C9783) >> 24) * 0x40u;   /* 0x1570D..0x1571F */
    } else {
        DSD(srec + 0x18u) = DSD(rec + 0x18u) + (u32)((s32)DSD(P5_C9783) >> 24) * 0x40u;   /* 0x15724..0x15736 */
    }
    DSD(srec + 0x1Cu) = DSD(rec + 0x1Cu) + (u32)((s32)DSD(P5_C9784) >> 24) * 0x40u;       /* 0x15739..0x1574B */
    actors_anim_begin(srec, DSD(P5_STREAMS_9B038 + (u32)DSB(slot + 0x7Au) * 4u), 0u);     /* 0x1574E..0x1575E */
    DSB(rec + 0x4Bu) = DSB(srec + 0x56u);                   /* 0x15763..0x15768 */
    DSB(0xF0AFEu) = 0u;                                     /* 0x1576B/0x1576D */
    DSB(0xF0AFFu) = DSB(rec + 0x51u);                       /* 0x15773/0x15776 */
    sound_voice(0xD6u);                                     /* 0x1577B/0x15780 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_156e0 | 0x156E0 | 3 | 6/6 | VERIFIED | 1A570 stub VERIFIED, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_156e0@mutant | 0x156E0 | 3 | 6/6 | MISMATCH | 1A570 stub VERIFIED, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_156e0@side | 0x156E0 | 3 | 6/6 | MISMATCH | 1A570 stub VERIFIED, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_156e0@signed | 0x156E0 | 3 | 6/6 | MISMATCH | 1A570 stub VERIFIED, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_156e0@plus | 0x156E0 | 3 | 6/6 | MISMATCH | 1A570 stub VERIFIED, 2BC30 stub unverified, 2C3FC stub unverified |

### `0x22AB8` (task 5)

held = rec+0x14; held+8 = the 0x104730 table of the pointed record's side; rec+0x1C += 0x1A00; by 0x1A570(that side): rec+0x34 = 0xFF6A with +0x18 -= 0x400, else 0x96 with +0x18 += 0x400; +0x36 = 0xFFF0.

Raw (35 instructions, 6 blocks; callees: 0x1a570):

```
0x22AB8: push       ebx
0x22AB9: push       ecx
0x22ABA: push       edx
0x22ABB: push       esi
0x22ABC: mov        edx, eax
0x22ABE: mov        eax, dword ptr [eax + 0x14]
0x22AC1: test       eax, eax
0x22AC3: je         0x22b20
0x22AC5: mov        ebx, dword ptr [eax]
0x22AC7: mov        bl, byte ptr [ebx + 0x51]
0x22ACA: and        ebx, 0xff
0x22AD0: mov        ebx, dword ptr [ebx*4 + 0x104730]
0x22AD7: mov        dword ptr [eax + 8], ebx
0x22ADA: add        dword ptr [edx + 0x1c], 0x1a00
0x22AE1: mov        eax, dword ptr [eax]
0x22AE3: mov        al, byte ptr [eax + 0x51]
0x22AE6: and        eax, 0xff
0x22AEB: call       0x1a570
0x22AF0: test       al, al
0x22AF2: je         0x22b08
0x22AF4: mov        esi, dword ptr [edx + 0x18]
0x22AF7: mov        word ptr [edx + 0x34], 0xff6a
0x22AFD: sub        esi, 0x400
0x22B03: mov        dword ptr [edx + 0x18], esi
0x22B06: jmp        0x22b1a
0x22B08: mov        ecx, dword ptr [edx + 0x18]
0x22B0B: mov        word ptr [edx + 0x34], 0x96
0x22B11: add        ecx, 0x400
0x22B17: mov        dword ptr [edx + 0x18], ecx
0x22B1A: mov        word ptr [edx + 0x36], 0xfff0
0x22B20: pop        esi
0x22B21: pop        edx
0x22B22: pop        ecx
0x22B23: pop        ebx
0x22B24: ret        
CALLS: 0x1a570
```

Port (`fighter.c`):

```c
/* 0x22AB8 — record §P5.3. The record's +0x14 pointer; when non-zero its +8
 * is the 0x104730 table entry of the pointed record's side, the record's
 * +0x1C += 0x1A00, and by 0x1A570(that side): the record's word +0x34 =
 * 0xFF6A with its +0x18 -= 0x400, else word +0x34 = 0x96 with +0x18 += 0x400;
 * either way word +0x36 = 0xFFF0. */
void fighter_22ab8(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x22ABE */
    u32 hrec;
    if (held == 0u) return;                                 /* 0x22AC1/0x22AC3 */
    hrec = DSD(held);                                       /* 0x22AC5 */
    DSD(held + 8u) = DSD(P5_104730 + (u32)DSB(hrec + 0x51u) * 4u);   /* 0x22AC7..0x22AD7 */
    DSD(rec + 0x1Cu) += 0x1A00u;                            /* 0x22ADA */
    if (fighter_actor_bit15_clear((u32)DSB(hrec + 0x51u)) != 0) {   /* 0x22AE1..0x22AF2 */
        DSW(rec + 0x34u) = 0xFF6Au;                         /* 0x22AF7 */
        DSD(rec + 0x18u) -= 0x400u;                         /* 0x22AF4..0x22B03 */
    } else {
        DSW(rec + 0x34u) = 0x0096u;                         /* 0x22B0B */
        DSD(rec + 0x18u) += 0x400u;                         /* 0x22B08..0x22B17 */
    }
    DSW(rec + 0x36u) = 0xFFF0u;                             /* 0x22B1A */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_22ab8 | 0x22AB8 | 4 | 6/6 | VERIFIED | 1A570 stub VERIFIED |
| fighter_22ab8@mutant | 0x22AB8 | 4 | 6/6 | MISMATCH | 1A570 stub VERIFIED |
| fighter_22ab8@side | 0x22AB8 | 4 | 6/6 | MISMATCH | 1A570 stub VERIFIED |
| fighter_22ab8@sext | 0x22AB8 | 4 | 6/6 | MISMATCH | 1A570 stub VERIFIED |
| fighter_22ab8@add | 0x22AB8 | 4 | 6/6 | MISMATCH | 1A570 stub VERIFIED |
| fighter_22ab8@table | 0x22AB8 | 4 | 6/6 | MISMATCH | 1A570 stub VERIFIED |

### `0x37B70` (task 5)

held = rec+0x14; the word 0xBDC64[its character] (16-bit negated when rec+0x28 bit 14 is set, a5 = 0x4000) and its character's 0xBDC48 descriptor; characters 0 and 5 take the word as a2, the others a2 = 0 and the pointed record's +0x4B takes the child's index; +0x59 = 1.

Raw (68 instructions, 11 blocks; callees: 0x2ae14 0x2ae14):

```
0x37B70: push       ebx
0x37B71: push       ecx
0x37B72: push       edx
0x37B73: push       esi
0x37B74: push       edi
0x37B75: mov        edx, eax
0x37B77: mov        esi, dword ptr [eax + 0x14]
0x37B7A: test       esi, esi
0x37B7C: je         0x37c1e
0x37B82: mov        ax, word ptr [eax + 0x28]
0x37B86: xor        al, al
0x37B88: and        ah, 0x40
0x37B8B: and        eax, 0xffff
0x37B90: je         0x37ba8
0x37B92: xor        eax, eax
0x37B94: mov        al, byte ptr [esi + 0x7a]
0x37B97: mov        di, word ptr [eax*2 + 0xbdc64]
0x37B9F: mov        ebx, 0x4000
0x37BA4: neg        edi
0x37BA6: jmp        0x37bb5
0x37BA8: mov        al, byte ptr [esi + 0x7a]
0x37BAB: xor        ebx, ebx
0x37BAD: mov        di, word ptr [eax*2 + 0xbdc64]
0x37BB5: xor        eax, eax
0x37BB7: mov        al, byte ptr [esi + 0x7a]
0x37BBA: mov        eax, dword ptr [eax*4 + 0xbdc48]
0x37BC1: test       eax, eax
0x37BC3: je         0x37c1e
0x37BC5: mov        cl, byte ptr [esi + 0x7a]
0x37BC8: test       cl, cl
0x37BCA: jbe        0x37bd1
0x37BCC: cmp        cl, 5
0x37BCF: jne        0x37bf3
0x37BD1: mov        dx, word ptr [edx + 0x56]
0x37BD5: xor        ecx, ecx
0x37BD7: or         dh, 4
0x37BDA: mov        cx, dx
0x37BDD: xor        edx, edx
0x37BDF: mov        dx, bx
0x37BE2: or         edx, ecx
0x37BE4: xor        ebx, ebx
0x37BE6: push       edx
0x37BE7: xor        ecx, ecx
0x37BE9: movsx      edx, di
0x37BEC: call       0x2ae14
0x37BF1: jmp        0x37c1a
0x37BF3: mov        dx, word ptr [edx + 0x56]
0x37BF7: xor        ecx, ecx
0x37BF9: or         dh, 4
0x37BFC: mov        cx, dx
0x37BFF: xor        edx, edx
0x37C01: mov        dx, bx
0x37C04: or         edx, ecx
0x37C06: xor        ebx, ebx
0x37C08: push       edx
0x37C09: xor        ecx, ecx
0x37C0B: xor        edx, edx
0x37C0D: call       0x2ae14
0x37C12: mov        edx, dword ptr [esi]
0x37C14: mov        bl, byte ptr [eax + 0x56]
0x37C17: mov        byte ptr [edx + 0x4b], bl
0x37C1A: mov        byte ptr [eax + 0x59], 1
0x37C1E: pop        edi
0x37C1F: pop        esi
0x37C20: pop        edx
0x37C21: pop        ecx
0x37C22: pop        ebx
0x37C23: ret        
CALLS: 0x2ae14 0x2ae14
```

Port (`fighter.c`):

```c
/* 0x37B70 — record §P5.3. The record's +0x14 pointer; when non-zero, the
 * word 0xBDC64[its character] (negated, 16-bit, when the record's +0x28 bit
 * 14 is set, and the spawn flag a5 = 0x4000) and its character's 0xBDC48
 * descriptor; when the descriptor is non-zero and the character is 0 or 5 the
 * child takes the word as a2, otherwise a2 = 0 and the pointed record's +0x4B
 * takes the child's pool index; either way a5 = the record's +0x56 | 0x400 and
 * the child's +0x59 = 1. */
void fighter_37b70(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x37B77 */
    u32 hrec, desc, child;
    u32 ch;
    s32 di;
    u32 a5;
    if (held == 0u) return;                                 /* 0x37B7A/0x37B7C */
    ch = (u32)DSB(held + 0x7Au);                            /* 0x37B82..0x37B8B */
    di = (s32)(s16)DSW(P5_BDC64 + ch * 2u);                 /* 0x37B92..0x37B97 */
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u) {               /* 0x37B82..0x37B90 */
        di = (s32)(s16)(u16)(0u - (u32)(u16)di);            /* 0x37BA4 */
        a5 = 0x4000u;                                       /* 0x37B9F */
    } else {
        a5 = 0u;                                            /* 0x37BAB */
    }
    desc = DSD(P5_DESC_BDC48 + ch * 4u);                    /* 0x37BB5..0x37BBA */
    if (desc == 0u) return;                                 /* 0x37BC1/0x37BC3 */
    if (ch == 0u || ch == 5u) {                             /* 0x37BC8..0x37BCF `jbe`/`jne` */
        child = actor_spawn((const u32 *)(mem + desc), (u32)di, 0u, 0u,
                            (u32)(u16)(DSW(rec + 0x56u) | 0x0400u) | a5);   /* 0x37BD1..0x37BEC */
    } else {
        child = actor_spawn((const u32 *)(mem + desc), 0u, 0u, 0u,
                            (u32)(u16)(DSW(rec + 0x56u) | 0x0400u) | a5);   /* 0x37BF3..0x37C0D */
        hrec = DSD(held);                                   /* 0x37C12 */
        DSB(hrec + 0x4Bu) = DSB(child + 0x56u);             /* 0x37C14/0x37C17 */
    }
    DSB(child + 0x59u) = 1u;                                /* 0x37C1A */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_37b70 | 0x37B70 | 7 | 11/11 | VERIFIED | 2AE14 stub unverified |
| fighter_37b70@mutant | 0x37B70 | 7 | 11/11 | MISMATCH | 2AE14 stub unverified |
| fighter_37b70@path | 0x37B70 | 7 | 11/11 | MISMATCH | 2AE14 stub unverified |
| fighter_37b70@neg | 0x37B70 | 7 | 11/11 | MISMATCH | 2AE14 stub unverified |
| fighter_37b70@hrec | 0x37B70 | 7 | 11/11 | MISMATCH | 2AE14 stub unverified |
| fighter_37b70@sext | 0x37B70 | 7 | 11/11 | MISMATCH | 2AE14 stub unverified |

---

### `0x3D328` (task 6)

held = rec+0x14; a child from 0xBB2B8 with a2 = 8 (bit 14) or -8, a4 = 4, a5 = rec+0x56 | 0x400; the child's +0x14 = held; on the side its +0x2E += 4 and +0x4E = 1; the voice 0x4E (always); rec+0x4B its index; the child's +0x60 = 1.

Raw (44 instructions, 8 blocks; callees: 0x2ae14 0x2c3fc):

```
0x3D328: push       ebx
0x3D329: push       ecx
0x3D32A: push       edx
0x3D32B: push       esi
0x3D32C: mov        esi, eax
0x3D32E: cmp        dword ptr [eax + 0x14], 0
0x3D332: je         0x3d3a4
0x3D334: mov        ax, word ptr [eax + 0x28]
0x3D338: xor        al, al
0x3D33A: and        ah, 0x40
0x3D33D: and        eax, 0xffff
0x3D342: je         0x3d34b
0x3D344: mov        eax, 8
0x3D349: jmp        0x3d350
0x3D34B: mov        eax, 0xfffffff8
0x3D350: mov        dx, word ptr [esi + 0x56]
0x3D354: or         dh, 4
0x3D357: mov        ebx, 4
0x3D35C: and        edx, 0xffff
0x3D362: xor        ecx, ecx
0x3D364: push       edx
0x3D365: movsx      edx, ax
0x3D368: mov        eax, 0xbb2b8
0x3D36D: call       0x2ae14
0x3D372: mov        ebx, dword ptr [esi + 0x14]
0x3D375: mov        dword ptr [eax + 0x14], ebx
0x3D378: mov        bl, byte ptr [esi + 0x51]
0x3D37B: mov        edx, eax
0x3D37D: test       bl, bl
0x3D37F: je         0x3d390
0x3D381: mov        bx, word ptr [eax + 0x2e]
0x3D385: mov        byte ptr [eax + 0x4e], 1
0x3D389: add        ebx, 4
0x3D38C: mov        word ptr [eax + 0x2e], bx
0x3D390: mov        eax, 0x4e
0x3D395: call       0x2c3fc
0x3D39A: mov        al, byte ptr [edx + 0x56]
0x3D39D: mov        byte ptr [esi + 0x4b], al
0x3D3A0: mov        byte ptr [edx + 0x60], 1
0x3D3A4: pop        esi
0x3D3A5: pop        edx
0x3D3A6: pop        ecx
0x3D3A7: pop        ebx
0x3D3A8: ret        
CALLS: 0x2ae14 0x2c3fc
```

Port (`fighter.c`):

```c
/* 0x3D328 — record §P5.4. The record's +0x14 pointer; when non-zero a child
 * from 0xBB2B8 with a2 = 8 (bit 14 of the record's +0x28 set) or -8, a4 = 4,
 * a5 = the record's +0x56 | 0x400; the child's +0x14 is the held record; on
 * the record's side: its +0x2E += 4, its +0x4E = 1 and the voice 0x4E; then
 * the record's +0x4B its pool index and the child's +0x60 = 1. */
void fighter_3d328(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x3D32E */
    u32 child;
    s32 a2;
    if (held == 0u) return;                                 /* 0x3D332 */
    a2 = (DSW(rec + 0x28u) & 0x4000u) != 0u ? 8 : -8;       /* 0x3D334..0x3D34B */
    child = actor_spawn((const u32 *)(mem + P5_DESC_BB2B8), (u32)a2, 0u, 4u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x3D350..0x3D36D */
    DSD(child + 0x14u) = held;                              /* 0x3D372/0x3D375 */
    if (DSB(rec + 0x51u) != 0u) {                           /* 0x3D378..0x3D37F */
        u16 w = DSW(child + 0x2Eu);                         /* 0x3D381 */
        DSB(child + 0x4Eu) = 1u;                            /* 0x3D385 */
        DSW(child + 0x2Eu) = (u16)(w + 4u);                 /* 0x3D389/0x3D38C */
    }
    sound_voice(0x4Eu);                                     /* 0x3D390/0x3D395 */
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);                  /* 0x3D39A/0x3D39D */
    DSB(child + 0x60u) = 1u;                                /* 0x3D3A0 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_3d328 | 0x3D328 | 4 | 8/8 | VERIFIED | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3d328@mutant | 0x3D328 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3d328@a2 | 0x3D328 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3d328@a4 | 0x3D328 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3d328@side | 0x3D328 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3d328@voice | 0x3D328 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |

### `0x3DA50` (task 6)

0x3D328's shape with a2 = 0x8C / -0x8C, the child from 0xBB2E0, a4 = 0x20, its +0x59 = 2.

Raw (45 instructions, 8 blocks; callees: 0x2ae14 0x2c3fc):

```
0x3DA50: push       ebx
0x3DA51: push       ecx
0x3DA52: push       edx
0x3DA53: push       esi
0x3DA54: mov        esi, eax
0x3DA56: cmp        dword ptr [eax + 0x14], 0
0x3DA5A: je         0x3dad4
0x3DA60: mov        ax, word ptr [eax + 0x28]
0x3DA64: xor        al, al
0x3DA66: and        ah, 0x40
0x3DA69: and        eax, 0xffff
0x3DA6E: je         0x3da77
0x3DA70: mov        eax, 0x8c
0x3DA75: jmp        0x3da7c
0x3DA77: mov        eax, 0xffffff74
0x3DA7C: mov        dx, word ptr [esi + 0x56]
0x3DA80: or         dh, 4
0x3DA83: mov        ebx, 0x20
0x3DA88: and        edx, 0xffff
0x3DA8E: xor        ecx, ecx
0x3DA90: push       edx
0x3DA91: movsx      edx, ax
0x3DA94: mov        eax, 0xbb2e0
0x3DA99: call       0x2ae14
0x3DA9E: mov        ebx, dword ptr [esi + 0x14]
0x3DAA1: mov        byte ptr [eax + 0x59], 2
0x3DAA5: mov        dword ptr [eax + 0x14], ebx
0x3DAA8: mov        bl, byte ptr [esi + 0x51]
0x3DAAB: mov        edx, eax
0x3DAAD: test       bl, bl
0x3DAAF: je         0x3dac0
0x3DAB1: mov        bx, word ptr [eax + 0x2e]
0x3DAB5: mov        byte ptr [eax + 0x4e], 1
0x3DAB9: add        ebx, 4
0x3DABC: mov        word ptr [eax + 0x2e], bx
0x3DAC0: mov        eax, 0x4e
0x3DAC5: call       0x2c3fc
0x3DACA: mov        al, byte ptr [edx + 0x56]
0x3DACD: mov        byte ptr [esi + 0x4b], al
0x3DAD0: mov        byte ptr [edx + 0x60], 1
0x3DAD4: pop        esi
0x3DAD5: pop        edx
0x3DAD6: pop        ecx
0x3DAD7: pop        ebx
0x3DAD8: ret        
CALLS: 0x2ae14 0x2c3fc
```

Port (`fighter.c`):

```c
/* 0x3DA50 — record §P5.4. 0x3D328's shape with a2 = 0x8C / -0x8C, the child
 * from 0xBB2E0 with a4 = 0x20 and its +0x59 = 2. */
void fighter_3da50(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x3DA56 */
    u32 child;
    s32 a2;
    if (held == 0u) return;                                 /* 0x3DA5A */
    a2 = (DSW(rec + 0x28u) & 0x4000u) != 0u ? 0x8C : -0x8C;   /* 0x3DA60..0x3DA77 */
    child = actor_spawn((const u32 *)(mem + P5_DESC_BB2E0), (u32)a2, 0u, 0x20u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x3DA7C..0x3DA99 */
    DSB(child + 0x59u) = 2u;                                /* 0x3DAA1 */
    DSD(child + 0x14u) = held;                              /* 0x3DA9E/0x3DAA5 */
    if (DSB(rec + 0x51u) != 0u) {                           /* 0x3DAA8..0x3DAAF */
        u16 w = DSW(child + 0x2Eu);                         /* 0x3DAB1 */
        DSB(child + 0x4Eu) = 1u;                            /* 0x3DAB5 */
        DSW(child + 0x2Eu) = (u16)(w + 4u);                 /* 0x3DAB9/0x3DABC */
    }
    sound_voice(0x4Eu);                                     /* 0x3DAC0/0x3DAC5 */
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);                  /* 0x3DACA/0x3DACD */
    DSB(child + 0x60u) = 1u;                                /* 0x3DAD0 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_3da50 | 0x3DA50 | 4 | 8/8 | VERIFIED | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3da50@mutant | 0x3DA50 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3da50@a2 | 0x3DA50 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3da50@a4 | 0x3DA50 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3da50@side | 0x3DA50 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |

### `0x3DB8C` (task 6)

held = rec+0x14; a child from 0xBB2CC with a2 = +/-0x1000 + rec+0x18, a3 = rec+0x30 >> 16, a4 = rec+0x1C + 0x1300, a5 = 0x4000/0; held+8 = the child, the child's +0x34 = 0xE0/0xFF20, +0x14 = held; on the side +0x2E += 4, +0x4E = 1.

Raw (59 instructions, 9 blocks; callees: 0x2ae14):

```
0x3DB8C: push       ebx
0x3DB8D: push       ecx
0x3DB8E: push       edx
0x3DB8F: push       esi
0x3DB90: push       edi
0x3DB91: push       ebp
0x3DB92: sub        esp, 4
0x3DB95: mov        esi, eax
0x3DB97: mov        edi, dword ptr [eax + 0x14]
0x3DB9A: test       edi, edi
0x3DB9C: je         0x3dc31
0x3DBA2: mov        ax, word ptr [eax + 0x28]
0x3DBA6: xor        al, al
0x3DBA8: and        ah, 0x40
0x3DBAB: and        eax, 0xffff
0x3DBB0: jne        0x3dbc1
0x3DBB2: mov        ebx, 0xffffff20
0x3DBB7: mov        eax, 0xfffff000
0x3DBBC: mov        dword ptr [esp], ebx
0x3DBBF: jmp        0x3dbce
0x3DBC1: mov        edx, 0xe0
0x3DBC6: mov        eax, 0x1000
0x3DBCB: mov        dword ptr [esp], edx
0x3DBCE: mov        dx, word ptr [esi + 0x28]
0x3DBD2: xor        dl, dl
0x3DBD4: and        dh, 0x40
0x3DBD7: and        edx, 0xffff
0x3DBDD: je         0x3dbe4
0x3DBDF: mov        edx, 0x4000
0x3DBE4: and        edx, 0xffff
0x3DBEA: mov        ecx, dword ptr [esi + 0x30]
0x3DBED: mov        ebx, dword ptr [esi + 0x1c]
0x3DBF0: mov        ebp, dword ptr [esi + 0x18]
0x3DBF3: push       edx
0x3DBF4: sar        ecx, 0x10
0x3DBF7: add        ebx, 0x1300
0x3DBFD: movsx      edx, ax
0x3DC00: mov        eax, 0xbb2cc
0x3DC05: add        edx, ebp
0x3DC07: call       0x2ae14
0x3DC0C: mov        edx, dword ptr [esp]
0x3DC0F: mov        dword ptr [edi + 8], eax
0x3DC12: mov        word ptr [eax + 0x34], dx
0x3DC16: mov        eax, dword ptr [edi + 8]
0x3DC19: mov        dword ptr [eax + 0x14], edi
0x3DC1C: cmp        byte ptr [esi + 0x51], 0
0x3DC20: je         0x3dc31
0x3DC22: mov        edx, dword ptr [edi + 8]
0x3DC25: add        word ptr [edx + 0x2e], 4
0x3DC2A: mov        eax, dword ptr [edi + 8]
0x3DC2D: mov        byte ptr [eax + 0x4e], 1
0x3DC31: add        esp, 4
0x3DC34: pop        ebp
0x3DC35: pop        edi
0x3DC36: pop        esi
0x3DC37: pop        edx
0x3DC38: pop        ecx
0x3DC39: pop        ebx
0x3DC3A: ret        
CALLS: 0x2ae14
```

Port (`fighter.c`):

```c
/* 0x3DB8C — record §P5.4. The record's +0x14 pointer; when non-zero a child
 * from 0xBB2CC with a2 = 0x1000 (bit 14 set) or -0x1000 plus the record's
 * +0x18, a3 = the record's +0x30 >> 16, a4 = its +0x1C + 0x1300, a5 = 0x4000
 * (bit 14) or 0; the held record's +8 the child, the child's word +0x34 =
 * 0x00E0 (bit 14) or 0xFF20, its +0x14 the held record; on the record's side
 * its +0x2E += 4 and +0x4E = 1. */
void fighter_3db8c(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x3DB97 */
    u32 child;
    s32 a2;
    u32 a5;
    u16 w34;
    if (held == 0u) return;                                 /* 0x3DB9A/0x3DB9C */
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u) {               /* 0x3DBA2..0x3DBB0 */
        a2 = 0x1000;                                        /* 0x3DBC6 */
        w34 = 0x00E0u;                                      /* 0x3DBC1 */
        a5 = 0x4000u;                                       /* 0x3DBDF */
    } else {
        a2 = -0x1000;                                       /* 0x3DBB7 */
        w34 = 0xFF20u;                                      /* 0x3DBB2 */
        a5 = 0u;                                            /* 0x3DBE4 */
    }
    child = actor_spawn((const u32 *)(mem + P5_DESC_BB2CC),
                        (u32)(a2 + (s32)DSD(rec + 0x18u)),
                        (u32)((s32)DSD(rec + 0x30u) >> 16),
                        DSD(rec + 0x1Cu) + 0x1300u, a5);    /* 0x3DBEA..0x3DC07 */
    DSD(held + 8u) = child;                                 /* 0x3DC0F */
    DSW(child + 0x34u) = w34;                               /* 0x3DC12 */
    DSD(child + 0x14u) = held;                              /* 0x3DC16/0x3DC19 */
    if (DSB(rec + 0x51u) != 0u) {                           /* 0x3DC1C/0x3DC20 */
        DSW(child + 0x2Eu) = (u16)(DSW(child + 0x2Eu) + 4u);   /* 0x3DC25 */
        DSB(child + 0x4Eu) = 1u;                            /* 0x3DC2D */
    }
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_3db8c | 0x3DB8C | 4 | 9/9 | VERIFIED | 2AE14 stub unverified |
| fighter_3db8c@mutant | 0x3DB8C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_3db8c@a2 | 0x3DB8C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_3db8c@w34 | 0x3DB8C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_3db8c@a4 | 0x3DB8C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_3db8c@side | 0x3DB8C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |

### `0x3DC3C` (task 6)

0x3DB8C's shape with the child's +0x34 = 0x1A0/0xFE60.

Raw (59 instructions, 9 blocks; callees: 0x2ae14):

```
0x3DC3C: push       ebx
0x3DC3D: push       ecx
0x3DC3E: push       edx
0x3DC3F: push       esi
0x3DC40: push       edi
0x3DC41: push       ebp
0x3DC42: sub        esp, 4
0x3DC45: mov        esi, eax
0x3DC47: mov        edi, dword ptr [eax + 0x14]
0x3DC4A: test       edi, edi
0x3DC4C: je         0x3dce1
0x3DC52: mov        ax, word ptr [eax + 0x28]
0x3DC56: xor        al, al
0x3DC58: and        ah, 0x40
0x3DC5B: and        eax, 0xffff
0x3DC60: jne        0x3dc71
0x3DC62: mov        ebx, 0xfffffe60
0x3DC67: mov        eax, 0xfffff000
0x3DC6C: mov        dword ptr [esp], ebx
0x3DC6F: jmp        0x3dc7e
0x3DC71: mov        edx, 0x1a0
0x3DC76: mov        eax, 0x1000
0x3DC7B: mov        dword ptr [esp], edx
0x3DC7E: mov        dx, word ptr [esi + 0x28]
0x3DC82: xor        dl, dl
0x3DC84: and        dh, 0x40
0x3DC87: and        edx, 0xffff
0x3DC8D: je         0x3dc94
0x3DC8F: mov        edx, 0x4000
0x3DC94: and        edx, 0xffff
0x3DC9A: mov        ecx, dword ptr [esi + 0x30]
0x3DC9D: mov        ebx, dword ptr [esi + 0x1c]
0x3DCA0: mov        ebp, dword ptr [esi + 0x18]
0x3DCA3: push       edx
0x3DCA4: sar        ecx, 0x10
0x3DCA7: add        ebx, 0x1300
0x3DCAD: movsx      edx, ax
0x3DCB0: mov        eax, 0xbb2cc
0x3DCB5: add        edx, ebp
0x3DCB7: call       0x2ae14
0x3DCBC: mov        edx, dword ptr [esp]
0x3DCBF: mov        dword ptr [edi + 8], eax
0x3DCC2: mov        word ptr [eax + 0x34], dx
0x3DCC6: mov        eax, dword ptr [edi + 8]
0x3DCC9: mov        dword ptr [eax + 0x14], edi
0x3DCCC: cmp        byte ptr [esi + 0x51], 0
0x3DCD0: je         0x3dce1
0x3DCD2: mov        edx, dword ptr [edi + 8]
0x3DCD5: add        word ptr [edx + 0x2e], 4
0x3DCDA: mov        eax, dword ptr [edi + 8]
0x3DCDD: mov        byte ptr [eax + 0x4e], 1
0x3DCE1: add        esp, 4
0x3DCE4: pop        ebp
0x3DCE5: pop        edi
0x3DCE6: pop        esi
0x3DCE7: pop        edx
0x3DCE8: pop        ecx
0x3DCE9: pop        ebx
0x3DCEA: ret        
CALLS: 0x2ae14
```

Port (`fighter.c`):

```c
/* 0x3DC3C — record §P5.4. 0x3DB8C's shape with the child's word +0x34 =
 * 0x01A0 (bit 14) or 0xFE60. */
void fighter_3dc3c(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x3DC47 */
    u32 child;
    s32 a2;
    u32 a5;
    u16 w34;
    if (held == 0u) return;                                 /* 0x3DC4A/0x3DC4C */
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u) {               /* 0x3DC52..0x3DC60 */
        a2 = 0x1000;                                        /* 0x3DC76 */
        w34 = 0x01A0u;                                      /* 0x3DC71 */
        a5 = 0x4000u;                                       /* 0x3DC8F */
    } else {
        a2 = -0x1000;                                       /* 0x3DC67 */
        w34 = 0xFE60u;                                      /* 0x3DC62 */
        a5 = 0u;                                            /* 0x3DC94 */
    }
    child = actor_spawn((const u32 *)(mem + P5_DESC_BB2CC),
                        (u32)(a2 + (s32)DSD(rec + 0x18u)),
                        (u32)((s32)DSD(rec + 0x30u) >> 16),
                        DSD(rec + 0x1Cu) + 0x1300u, a5);    /* 0x3DC9A..0x3DCB7 */
    DSD(held + 8u) = child;                                 /* 0x3DCBF */
    DSW(child + 0x34u) = w34;                               /* 0x3DCC2 */
    DSD(child + 0x14u) = held;                              /* 0x3DCC6/0x3DCC9 */
    if (DSB(rec + 0x51u) != 0u) {                           /* 0x3DCCC/0x3DCD0 */
        DSW(child + 0x2Eu) = (u16)(DSW(child + 0x2Eu) + 4u);   /* 0x3DCD5 */
        DSB(child + 0x4Eu) = 1u;                            /* 0x3DCDD */
    }
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_3dc3c | 0x3DC3C | 4 | 9/9 | VERIFIED | 2AE14 stub unverified |
| fighter_3dc3c@mutant | 0x3DC3C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_3dc3c@a2 | 0x3DC3C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_3dc3c@w34 | 0x3DC3C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_3dc3c@side | 0x3DC3C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |

### `0x403A0` (task 6)

held = rec+0x14; other = (rec+0x51) ^ 1; the word 0xC7780[its character] (negated when that record's +0x28 bit 14 is set) as a2, the signed word 0xC778C[its character] as a4, a5 = its +0x56 | 0x400; the child's +0x14 = held, +0x51 = rec+0x51, +0x59 = rec+0x59 - 1.

Raw (48 instructions, 6 blocks; callees: 0x2ae14):

```
0x403A0: push       ebx
0x403A1: push       ecx
0x403A2: push       edx
0x403A3: push       esi
0x403A4: mov        esi, eax
0x403A6: cmp        dword ptr [eax + 0x14], 0
0x403AA: je         0x4042c
0x403B0: mov        al, byte ptr [eax + 0x51]
0x403B3: xor        al, 1
0x403B5: and        eax, 0xff
0x403BA: mov        eax, dword ptr [eax*4 + 0x1077a8]
0x403C1: test       eax, eax
0x403C3: je         0x4042c
0x403C5: mov        ebx, dword ptr [eax]
0x403C7: xor        edx, edx
0x403C9: mov        bx, word ptr [ebx + 0x28]
0x403CD: mov        dl, byte ptr [eax + 0x7a]
0x403D0: xor        bl, bl
0x403D2: mov        dx, word ptr [edx*2 + 0xc7780]
0x403DA: and        bh, 0x40
0x403DD: and        ebx, 0xffff
0x403E3: je         0x403e7
0x403E5: neg        edx
0x403E7: mov        ebx, dword ptr [eax]
0x403E9: mov        bx, word ptr [ebx + 0x56]
0x403ED: or         bh, 4
0x403F0: and        ebx, 0xffff
0x403F6: mov        al, byte ptr [eax + 0x7a]
0x403F9: push       ebx
0x403FA: and        eax, 0xff
0x403FF: movsx      edx, dx
0x40402: xor        ecx, ecx
0x40404: mov        ebx, dword ptr [eax*2 + 0xc778c]
0x4040B: mov        eax, 0xc776c
0x40410: sar        ebx, 0x10
0x40413: call       0x2ae14
0x40418: mov        edx, dword ptr [esi + 0x14]
0x4041B: mov        dword ptr [eax + 0x14], edx
0x4041E: mov        dl, byte ptr [esi + 0x51]
0x40421: mov        byte ptr [eax + 0x51], dl
0x40424: mov        dl, byte ptr [esi + 0x59]
0x40427: dec        dl
0x40429: mov        byte ptr [eax + 0x59], dl
0x4042C: pop        esi
0x4042D: pop        edx
0x4042E: pop        ecx
0x4042F: pop        ebx
0x40430: ret        
CALLS: 0x2ae14
```

Port (`fighter.c`):

```c
/* 0x403A0 — record §P5.4. The record's +0x14 pointer; when non-zero and the
 * other side's slot (rec+0x51 ^ 1) non-zero: the word 0xC7780[its character]
 * (negated, 16-bit, when that slot's record's +0x28 bit 14 is set) as a2, the
 * signed word 0xC778C[its character] as a4, a5 = that record's +0x56 | 0x400,
 * a child from 0xC776C; the child's +0x14 the held record, its +0x51 the
 * record's side, its +0x59 the record's +0x59 - 1. */
void fighter_403a0(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x403A6 */
    u32 other, slot, srec, child;
    u32 ch;
    s32 a2;
    if (held == 0u) return;                                 /* 0x403AA */
    other = (u32)DSB(rec + 0x51u) ^ 1u;                     /* 0x403B0..0x403B5 */
    slot = DSD(DS_001077A8 + other * 4u);                   /* 0x403BA */
    if (slot == 0u) return;                                 /* 0x403C1/0x403C3 */
    srec = DSD(slot);                                       /* 0x403C5 */
    ch = (u32)DSB(slot + 0x7Au);                            /* 0x403CD */
    a2 = (s32)(s16)DSW(P5_C7780 + ch * 2u);                 /* 0x403D2 */
    if ((DSW(srec + 0x28u) & 0x4000u) != 0u)                /* 0x403C9..0x403E3 */
        a2 = (s32)(s16)(u16)(0u - (u32)(u16)a2);            /* 0x403E5 */
    child = actor_spawn((const u32 *)(mem + P5_DESC_C776C), (u32)a2, 0u,
                        (u32)(s32)(s16)DSW(P5_C778C + ch * 2u + 2u),
                        (u32)(u16)(DSW(srec + 0x56u) | 0x0400u));   /* 0x403E7..0x40413 */
    DSD(child + 0x14u) = held;                              /* 0x40418/0x4041B */
    DSB(child + 0x51u) = DSB(rec + 0x51u);                  /* 0x4041E/0x40421 */
    DSB(child + 0x59u) = (u8)(DSB(rec + 0x59u) - 1u);       /* 0x40424..0x40429 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_403a0 | 0x403A0 | 5 | 6/6 | VERIFIED | 2AE14 stub unverified |
| fighter_403a0@mutant | 0x403A0 | 5 | 6/6 | MISMATCH | 2AE14 stub unverified |
| fighter_403a0@side | 0x403A0 | 5 | 6/6 | MISMATCH | 2AE14 stub unverified |
| fighter_403a0@signed | 0x403A0 | 5 | 6/6 | MISMATCH | 2AE14 stub unverified |
| fighter_403a0@neg | 0x403A0 | 5 | 6/6 | MISMATCH | 2AE14 stub unverified |
| fighter_403a0@a5 | 0x403A0 | 5 | 6/6 | MISMATCH | 2AE14 stub unverified |
| fighter_403a0@r59 | 0x403A0 | 5 | 6/6 | MISMATCH | 2AE14 stub unverified |

### `0x40FBC` (task 6)

a child from 0xC77D8 with a2 = 8 when 0x1A570(rec+0x51) sets AL else -8; its +0x59 = 2, +0x24/+0x20 = rec's; on the side its palette (0x2A17C, word 0x0C, handle 0).

Raw (37 instructions, 6 blocks; callees: 0x1a570 0x2ae14 0x2a17c):

```
0x40FBC: push       ebx
0x40FBD: push       ecx
0x40FBE: push       edx
0x40FBF: push       esi
0x40FC0: mov        esi, eax
0x40FC2: xor        eax, eax
0x40FC4: mov        al, byte ptr [esi + 0x51]
0x40FC7: call       0x1a570
0x40FCC: test       al, al
0x40FCE: je         0x40fd7
0x40FD0: mov        eax, 8
0x40FD5: jmp        0x40fdc
0x40FD7: mov        eax, 0xfffffff8
0x40FDC: mov        dx, word ptr [esi + 0x56]
0x40FE0: or         dh, 4
0x40FE3: and        edx, 0xffff
0x40FE9: xor        ecx, ecx
0x40FEB: xor        ebx, ebx
0x40FED: push       edx
0x40FEE: movsx      edx, ax
0x40FF1: mov        eax, 0xc77d8
0x40FF6: call       0x2ae14
0x40FFB: mov        byte ptr [eax + 0x59], 2
0x40FFF: mov        edx, dword ptr [esi + 0x24]
0x41002: mov        dword ptr [eax + 0x24], edx
0x41005: mov        edx, dword ptr [esi + 0x20]
0x41008: mov        dword ptr [eax + 0x20], edx
0x4100B: cmp        byte ptr [esi + 0x51], 0
0x4100F: je         0x4101d
0x41011: mov        edx, 0xc
0x41016: xor        ebx, ebx
0x41018: call       0x2a17c
0x4101D: pop        esi
0x4101E: pop        edx
0x4101F: pop        ecx
0x41020: pop        ebx
0x41021: ret        
CALLS: 0x1a570 0x2ae14 0x2a17c
```

Port (`fighter.c`):

```c
/* 0x40FBC — record §P5.4. A child from 0xC77D8 with a2 = 8 when
 * 0x1A570(rec+0x51) sets AL else -8, a5 = the record's +0x56 | 0x400; its
 * +0x59 = 2, its +0x24 and +0x20 the record's, and on the record's side the
 * child's palette (0x2A17C, word 0x0C, handle 0). */
void fighter_40fbc(u32 rec)
{
    s32 a2 = fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0 ? 8 : -8;   /* 0x40FC2..0x40FDC */
    u32 child = actor_spawn((const u32 *)(mem + P5_DESC_C77D8), (u32)a2, 0u, 0u,
                            (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x40FE9..0x40FF6 */
    DSB(child + 0x59u) = 2u;                                /* 0x40FFB */
    DSD(child + 0x24u) = DSD(rec + 0x24u);                  /* 0x40FFF/0x41002 */
    DSD(child + 0x20u) = DSD(rec + 0x20u);                  /* 0x41005/0x41008 */
    if (DSB(rec + 0x51u) != 0u)                             /* 0x4100B/0x4100F */
        actor_pset_palette(child, 0x0Cu, 0u);               /* 0x41011..0x41018 0x2A17C */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_40fbc | 0x40FBC | 3 | 6/6 | VERIFIED | 1A570 stub VERIFIED, 2A17C stub unverified, 2AE14 stub unverified |
| fighter_40fbc@mutant | 0x40FBC | 3 | 6/6 | MISMATCH | 1A570 stub VERIFIED, 2A17C stub unverified, 2AE14 stub unverified |
| fighter_40fbc@pal | 0x40FBC | 3 | 6/6 | MISMATCH | 1A570 stub VERIFIED, 2A17C stub unverified, 2AE14 stub unverified |
| fighter_40fbc@a2 | 0x40FBC | 3 | 6/6 | MISMATCH | 1A570 stub VERIFIED, 2A17C stub unverified, 2AE14 stub unverified |

### `0x48A20` (task 6)

walks the 0x1014F4 chain while the record's +0x4B is non-zero; a child from 0xC94F8 with a2 = rec+0x18 +/- 0xC40, a3 = rec+0x30 >> 16, a4 = rec+0x1C + 0x800, a5 = 0x4000/0; the walked record's +0x4B its index; 0x1014F4 stored back (the same value).

Raw (47 instructions, 6 blocks; callees: 0x2ae14):

```
0x48A20: push       ebx
0x48A21: push       ecx
0x48A22: push       edx
0x48A23: push       esi
0x48A24: push       edi
0x48A25: mov        edi, dword ptr [0x1014f4]
0x48A2B: mov        edx, eax
0x48A2D: mov        esi, eax
0x48A2F: cmp        byte ptr [eax + 0x4b], 0
0x48A33: je         0x48a46
0x48A35: xor        eax, eax
0x48A37: mov        al, byte ptr [esi + 0x4b]
0x48A3A: imul       eax, eax, 0x68
0x48A3D: lea        esi, [edi + eax]
0x48A40: cmp        byte ptr [esi + 0x4b], 0
0x48A44: jne        0x48a35
0x48A46: mov        ax, word ptr [edx + 0x28]
0x48A4A: xor        al, al
0x48A4C: and        ah, 0x40
0x48A4F: and        eax, 0xffff
0x48A54: je         0x48a65
0x48A56: mov        eax, dword ptr [edx + 0x18]
0x48A59: mov        ebx, 0x4000
0x48A5E: add        eax, 0xc40
0x48A63: jmp        0x48a6f
0x48A65: mov        eax, dword ptr [edx + 0x18]
0x48A68: xor        ebx, ebx
0x48A6A: sub        eax, 0xc40
0x48A6F: and        ebx, 0xffff
0x48A75: mov        ecx, dword ptr [edx + 0x30]
0x48A78: push       ebx
0x48A79: mov        ebx, dword ptr [edx + 0x1c]
0x48A7C: sar        ecx, 0x10
0x48A7F: add        ebx, 0x800
0x48A85: mov        edx, eax
0x48A87: mov        eax, 0xc94f8
0x48A8C: mov        dword ptr [0x1014f4], edi
0x48A92: call       0x2ae14
0x48A97: mov        al, byte ptr [eax + 0x56]
0x48A9A: mov        edi, dword ptr [0x1014f4]
0x48AA0: mov        byte ptr [esi + 0x4b], al
0x48AA3: pop        edi
0x48AA4: pop        esi
0x48AA5: pop        edx
0x48AA6: pop        ecx
0x48AA7: pop        ebx
0x48AA8: ret        
CALLS: 0x2ae14
```

Port (`fighter.c`):

```c
/* 0x48A20 — record §P5.4. Walks the 0x1014F4 chain from the record while its
 * +0x4B is non-zero (esi = base + byte * 0x68), then spawns from 0xC94F8
 * with a2 = the record's +0x18 + 0xC40 (bit 14 of its +0x28 set) or - 0xC40,
 * a3 = its +0x30 >> 16, a4 = its +0x1C + 0x800, a5 = 0x4000 or 0; the walked
 * record's +0x4B takes the child's pool index. The raw stores 0x1014F4 back
 * (the same value) before the spawn and reloads it after (dead). */
void fighter_48a20(u32 rec)
{
    u32 base = DSD(P5_1014F4);                              /* 0x48A25 */
    u32 esi = rec;                                          /* 0x48A2D/0x48A2F */
    u32 a2, a5, child;
    while (DSB(esi + 0x4Bu) != 0u)                          /* 0x48A2F..0x48A44 */
        esi = base + (u32)DSB(esi + 0x4Bu) * 0x68u;
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u) {               /* 0x48A46..0x48A54 */
        a2 = DSD(rec + 0x18u) + 0xC40u;                     /* 0x48A56..0x48A5E */
        a5 = 0x4000u;                                       /* 0x48A59 */
    } else {
        a2 = DSD(rec + 0x18u) - 0xC40u;                     /* 0x48A65..0x48A6A */
        a5 = 0u;                                            /* 0x48A68 */
    }
    DSD(P5_1014F4) = base;                                  /* 0x48A8C */
    child = actor_spawn((const u32 *)(mem + P5_DESC_C94F8), a2,
                        (u32)((s32)DSD(rec + 0x30u) >> 16),
                        DSD(rec + 0x1Cu) + 0x800u, a5);     /* 0x48A75..0x48A92 */
    DSB(esi + 0x4Bu) = DSB(child + 0x56u);                  /* 0x48A9A/0x48AA0 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_48a20 | 0x48A20 | 3 | 6/6 | VERIFIED | 2AE14 stub unverified |
| fighter_48a20@mutant | 0x48A20 | 3 | 6/6 | MISMATCH | 2AE14 stub unverified |
| fighter_48a20@walk | 0x48A20 | 3 | 6/6 | MISMATCH | 2AE14 stub unverified |
| fighter_48a20@a2 | 0x48A20 | 3 | 6/6 | MISMATCH | 2AE14 stub unverified |

---

### `0x24508` (task 7)

ctx 0x339AC(rec); the other slot (ctx[3]) 0x10/0x0A/0 with its +0x10 = 0x24454 and +0x58 = 0; the other record on its character's 0xA85F8 stream at 1.0; the voice 0xEB.

Raw (25 instructions, 1 blocks; callees: 0x339ac 0x2bc30 0x2c3fc):

```
0x24508: push       edx
0x24509: sub        esp, 0x18
0x2450C: mov        edx, eax
0x2450E: mov        eax, esp
0x24510: call       0x339ac
0x24515: mov        eax, dword ptr [esp + 0xc]
0x24519: mov        byte ptr [eax + 0x52], 0x10
0x2451D: mov        eax, dword ptr [esp + 0xc]
0x24521: mov        byte ptr [eax + 0x53], 0xa
0x24525: mov        eax, dword ptr [esp + 0xc]
0x24529: mov        dword ptr [eax + 0x10], 0x24454
0x24530: mov        eax, dword ptr [esp + 0xc]
0x24534: mov        byte ptr [eax + 0x58], 0
0x24538: mov        eax, dword ptr [esp + 0xc]
0x2453C: push       0x3f800000
0x24541: mov        al, byte ptr [eax + 0x7a]
0x24544: and        eax, 0xff
0x24549: mov        edx, dword ptr [eax*4 + 0xa85f8]
0x24550: mov        eax, dword ptr [esp + 0x18]
0x24554: call       0x2bc30
0x24559: mov        eax, 0xeb
0x2455E: call       0x2c3fc
0x24563: add        esp, 0x18
0x24566: pop        edx
0x24567: ret        
CALLS: 0x339ac 0x2bc30 0x2c3fc
```

Port (`fighter.c`):

```c
/* 0x24508 — record §P5.5. The context 0x339AC(rec); the other slot (ctx[3])
 * 0x10/0x0A/0 with its +0x10 handler 0x24454 (0x3531C case 10) and +0x58 = 0;
 * the other record on its character's 0xA85F8 stream at 1.0 (0x2BC30); the
 * voice 0xEB. */
void fighter_24508(u32 rec)
{
    u32 ctx[6];
    hit_anim_ctx(ctx, rec);                                 /* 0x2450C..0x24510 0x339AC */
    DSB(ctx[3] + 0x52u) = 0x10u;                            /* 0x24515/0x24519 */
    DSB(ctx[3] + 0x53u) = 0x0Au;                            /* 0x2451D/0x24521 */
    DSD(ctx[3] + 0x10u) = 0x00024454u;                      /* 0x24525/0x24529 */
    DSB(ctx[3] + 0x58u) = 0u;                               /* 0x24530/0x24534 */
    actors_anim_begin(ctx[5], DSD(P5_STREAMS_A85F8 + (u32)DSB(ctx[3] + 0x7Au) * 4u),
                      0x3F800000u);                         /* 0x24538..0x24554 0x2BC30 */
    sound_voice(0xEBu);                                     /* 0x24559/0x2455E */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_24508 | 0x24508 | 2 | 1/1 | VERIFIED | 2BC30 stub unverified, 2C3FC stub unverified, 339AC allow VERIFIED |
| fighter_24508@mutant | 0x24508 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified, 339AC allow VERIFIED |
| fighter_24508@side | 0x24508 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified, 339AC allow VERIFIED |
| fighter_24508@stream | 0x24508 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified, 339AC allow VERIFIED |

### `0x24454` (task 7)

the +0x10 handler: by the slot's +0x58, 0 spawns from the slot's character's 0xA85DC descriptor (a2 = rec+0x18, a3 = rec+0x30 >> 16, a4 = rec+0x1C, a5 = bit 14 ? 0x4000 : 0) when rec+0x24 & 0x7FFFFFFF is zero, sets the child's +0x36 = 0x40 and 0x104740 = it; 1 releases 0x104740 (0x2AD40, its pset 0x1014EC + pool index * 0x20) once its +0x1C >= 0x3000 and starts the other side's record on 0xE5100 at 3.0, slot 3/9.

Raw (57 instructions, 9 blocks; callees: 0x2ae14 0x2ad40 0x2bc30):

```
0x24454: push       ecx
0x24455: push       esi
0x24456: mov        esi, eax
0x24458: mov        ecx, edx
0x2445A: mov        al, byte ptr [eax + 0x58]
0x2445D: test       al, al
0x2445F: jbe        0x24468
0x24461: cmp        al, 1
0x24463: je         0x244b2
0x24465: pop        esi
0x24466: pop        ecx
0x24467: ret        
0x24468: test       dword ptr [edx + 0x24], 0x7fffffff
0x2446F: jne        0x24504
0x24475: mov        ax, word ptr [edx + 0x28]
0x24479: xor        al, al
0x2447B: and        ah, 0x40
0x2447E: and        eax, 0xffff
0x24483: mov        ecx, dword ptr [edx + 0x30]
0x24486: push       eax
0x24487: xor        eax, eax
0x24489: mov        ebx, dword ptr [edx + 0x1c]
0x2448C: mov        al, byte ptr [esi + 0x7a]
0x2448F: sar        ecx, 0x10
0x24492: mov        edx, dword ptr [edx + 0x18]
0x24495: mov        eax, dword ptr [eax*4 + 0xa85dc]
0x2449C: call       0x2ae14
0x244A1: mov        word ptr [eax + 0x36], 0x40
0x244A7: mov        dword ptr [0x104740], eax
0x244AC: inc        byte ptr [esi + 0x58]
0x244AF: pop        esi
0x244B0: pop        ecx
0x244B1: ret        
0x244B2: mov        eax, dword ptr [0x104740]
0x244B7: cmp        dword ptr [eax + 0x1c], 0x3000
0x244BE: jl         0x24504
0x244C0: xor        ebx, ebx
0x244C2: mov        bx, word ptr [eax + 0x56]
0x244C6: mov        edx, dword ptr [0x1014ec]
0x244CC: shl        ebx, 5
0x244CF: add        edx, ebx
0x244D1: call       0x2ad40
0x244D6: mov        al, byte ptr [ecx + 0x51]
0x244D9: xor        al, 1
0x244DB: and        eax, 0xff
0x244E0: mov        eax, dword ptr [eax*4 + 0x1077a8]
0x244E7: test       eax, eax
0x244E9: je         0x24504
0x244EB: mov        edx, 0xe5100
0x244F0: mov        eax, dword ptr [eax]
0x244F2: push       0x40400000
0x244F7: call       0x2bc30
0x244FC: mov        byte ptr [esi + 0x53], 3
0x24500: mov        byte ptr [esi + 0x52], 9
0x24504: pop        esi
0x24505: pop        ecx
0x24506: ret        
CALLS: 0x2ae14 0x2ad40 0x2bc30
```

Port (`fighter.c`):

```c
/* 0x24454 — record §P5.5. The other slot's +0x10 handler 0x24508 stores
 * (0x3531C case 10: EAX = the slot, EDX = the slot's record, EBX = side; the
 * function reads EAX and EDX). By the slot's +0x58: above 1 nothing; 0 with
 * the record's +0x24 (mask 0x7FFFFFFF) non-zero nothing, else a child from the
 * slot's character's 0xA85DC descriptor with a2 = the record's +0x18, a3 = its
 * +0x30 >> 16, a4 = its +0x1C, a5 = 0x4000 (bit 14 of its +0x28) or 0, the
 * child's word +0x36 = 0x40, 0x104740 = the child and the slot's +0x58 = 1;
 * 1 with the child's +0x1C below 0x3000 nothing, else the child released
 * (0x2AD40, its pset 0x1014EC + pool index * 0x20), the other side's slot
 * (rec+0x51 ^ 1) when non-zero: its record on 0xE5100 at 3.0 and the slot
 * +0x53 = 3, +0x52 = 9. */
void fighter_24454(u32 slot, u32 rec)
{
    u8 st = DSB(slot + 0x58u);                              /* 0x2445A */
    u32 child;
    if (st > 1u) return;                                    /* 0x2445D..0x24467 */
    if (st == 0u) {
        if ((DSD(rec + 0x24u) & 0x7FFFFFFFu) != 0u) return; /* 0x24468/0x2446F */
        child = actor_spawn((const u32 *)(mem + DSD(P5_DESC_A85DC + (u32)DSB(slot + 0x7Au) * 4u)),
                            DSD(rec + 0x18u), (u32)((s32)DSD(rec + 0x30u) >> 16),
                            DSD(rec + 0x1Cu),
                            (DSW(rec + 0x28u) & 0x4000u) != 0u ? 0x4000u : 0u);   /* 0x24475..0x2449C */
        DSW(child + 0x36u) = 0x0040u;                       /* 0x244A1 */
        DSD(P5_104740) = child;                             /* 0x244A7 */
        DSB(slot + 0x58u) = (u8)(st + 1u);                  /* 0x244AC */
        return;
    }
    child = DSD(P5_104740);                                 /* 0x244B2 */
    if ((s32)DSD(child + 0x1Cu) < 0x3000) return;           /* 0x244B7..0x244BE */
    actor_release(child, DSD(DS_001014EC) + (u32)DSW(child + 0x56u) * 0x20u);   /* 0x244C0..0x244D1 0x2AD40 */
    {
        u32 other = (u32)DSB(rec + 0x51u) ^ 1u;             /* 0x244D6..0x244DB */
        u32 oslot = DSD(DS_001077A8 + other * 4u);          /* 0x244E0 */
        if (oslot == 0u) return;                            /* 0x244E7/0x244E9 */
        actors_anim_begin(DSD(oslot), P5_STREAM_24454, 0x40400000u);   /* 0x244EB..0x244F7 */
    }
    DSB(slot + 0x53u) = 3u;                                 /* 0x244FC */
    DSB(slot + 0x52u) = 9u;                                 /* 0x24500 */
}
```

Cases/mutants (from `P45_SPECS` and `P45_KINDS`; measured):

| fighter_24454 | 0x24454 | 9 | 9/9 | VERIFIED | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24454@mutant | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24454@guard | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24454@st1 | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24454@pset | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24454@release | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24454@side | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24454@desc | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24454@a5 | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |

---

## §P4.7 The gp scenarios: gp-u9-win re-measured; gp-u10-ending drops one row

**gp-u9-win.** After the batch ports `0x400E0`, `0x21044` and `0x21084`, the replay's miss set loses those three rows and gains the targets it then reaches (temporary frame instrumentation on the miss log; the frame word at 0xEF6DC): `0x213F0 anim_indirect` first=0x8ED (1 hit), `0x213F4 anim_indirect` first=0x91F (1 hit), `0x2BDA0 anim_indirect` first=0xD0C (10 hits). `distinct=7` (the base pair, `0x29D60`, `0x5D812` and the three). The re-measured pins: `frames: first unexplained 346` (unchanged), `trace: first differing 2364` (was 2150; the pin rises), `path: 0 not reproduced through 7; ratchet N 8`, `win: first differing 3162` (unchanged), MAX_START 100. Each pin + 1 fails (plan Task 2 Step 7).

**gp-u10-ending.** After `0x3DA50` is ported the set loses that row alone (`distinct=6`: the base pair, `0x29D60`, `0x5D812`, `0x37DD4`, `0x29C78`); the replay reaches no new target. Every pin is unchanged: `frames: first unexplained 331`, `trace: 0 differing through 9953; ratchet N 9954`, `path: 0 not reproduced through 29; ratchet N 30`, `win: 0 differing through 9953; ratchet N 9954`, MAX_START 83. Each pin + 1 fails (plan Task 6 Step 7).

No other gp scenario's set holds a P4/P5 member (every set was read before the batch: the base pair plus, for the U8 captures, the wipe hooks; `gp-u8-right-arcade`'s P2 rows are ported).

## §P4.8 Decisions, named gaps and limits

**No decision is left to the user.** The member list is the roadmap's; `0x24454` is the code-immediate handler with its case-10 adapter; the two new callees are seamed, their rows C1's.

Named gaps and limits:
- **Stream targets left open:** the P6/P7 rows the members' streams reach (`0x47E04 0x47E30 0x482E4 0x48374`, `0x48254`, and the U9 replay's `0x213F0 0x213F4 0x2BDA0`); unported, skipped by `anim_indirect`.
- **The callee rows** (D3): `0x29C08` and `0x2AD40` join C1; their seams are inert outside `build/diffrun`.
- **Unit-check limits:** `0x1549C`, `0x229E8` and `0x243F8` have no unit check (their calls' effects need a real stream/palette); their rows cover them. The unit tests' line numbers move with `test_fight.c`.
- **The side-0x80 cases read the slot at 0x10C1B0, just past the image** (both sides read the same zeros; `P45_OUTSIDE` names the seven rows).
- The `title_pin` unittest failure on this tree is pre-existing and outside `make verify`.

## §P4.9 Results

The per-commit gates: `make diff-verify entry-triage` with scratch paths and the per-task counters (plan Tasks 2-7); `PR_ORACLE_REQUIRED=1 ./build/run_tests` `all checks passed` after each; the gp oracles re-pinned in Tasks 2 and 6; the full `make verify` on the final state (Task 8): `ORACLES-EQUAL`, `WAV-SAME`, `771 1203 64` / `731 731 100`, `diff-verify: 111/111 functions VERIFIED; 252/252 mutants detected; 1 named gaps; 13/94 rows with callees closed (17 have none)`, `entry-triage: targets 257 unported, 238 ported; supplement 131 (8 unported, 0 stale)`, `voice sites outside Ghidra 134: 13 in unported code, 102 in ported code, 19 nowhere`, the diff-verify Python suite 163 tests, the E2 suite 44, every gp ratchet at its pin. The 33 members' rows and mutants are in the plan's tasks; the measured table lines are in each §P4/§P5 section above.
