# Named-gaps unit E — `fight_health_sync` case 0x12 (derivation record)

Branch `named-gaps-e` off main `af135ec`. Closes ledger
`2026-09-29-all-gaps-ledger.md` §H.3 #7 (first recorded in
`2026-09-29-k1-k9-derivations.md` §K1.1 and in PROGRESS.md's Task 3a
paragraph). Source of truth: the raw mirror `k11_img.bin` (the linear image
with the LE fixups applied; addresses are Ghidra/linear addresses) read with
capstone. Ghidra MCP was not available.

## §E.1 The arm (raw `0x34CC7..0x34D21`)

`0x34B6C` (`fight_health_sync`, EAX = side) keeps the side in EDX and the slot
record `DS_001077A8[side]` in ECX for the whole body:

```
34b71: 89c2              mov edx, eax                    ; EDX = side
34b73: 8b0c85a8771000    mov ecx, [eax*4 + 0x1077a8]     ; ECX = slot record
34b82: 8b31              mov esi, [ecx]                  ; ESI = fighter record
34bf4: 8a4152            mov al, [ecx + 0x52]
34bf7: 3c15              cmp al, 0x15
34bf9: 770d              ja 0x34c08
34c00: 2eff2485144b0300  jmp cs:[eax*4 + 0x34b14]
```

The table entry 18 is the dword at `0x34B14 + 18*4 = 0x34B5C` = `0x00034CC7`
(the only dword in the image with that value). The arm:

```
34cc7: 668b3455e0881000  mov si, [edx*2 + 0x1088e0]      ; si = DS_001088E0[side]
34ccf: 89f0              mov eax, esi
34cd1: 30c0              xor al, al
34cd3: 80e403            and ah, 3
34cd6: 25ffff0000        and eax, 0xffff                 ; eax = si & 0x300
34cdb: 7413              je 0x34cf0
34cdd: 81e6000c0000      and esi, 0xc00                  ; (clears ESI's high half)
34ce3: 31c0              xor eax, eax
34ce5: 6689f0            mov ax, si
34ce8: 85c0              test eax, eax                   ; si & 0xC00
34cea: 7404              je 0x34cf0
34cec: b001              mov al, 1
34cee: eb02              jmp 0x34cf2
34cf0: 30c0              xor al, al
34cf2: 84c0              test al, al
34cf4: 0f8589000000      jne 0x34d83                     ; both groups set -> exit
34cfa: 8a6154            mov ah, [ecx + 0x54]            ; the slot's +0x54 byte
34cfd: 84e4              test ah, ah
34cff: 7409              je 0x34d0a
34d01: 80fc01            cmp ah, 1
34d04: 0f8579000000      jne 0x34d83                     ; not 0 and not 1 -> exit
34d0a: 89d0              mov eax, edx                    ; EAX = side
34d0c: e8cb700000        call 0x3bddc
34d11: 84c0              test al, al
34d13: 746e              je 0x34d83                      ; AL == 0 -> exit
34d15: 89d0              mov eax, edx                    ; EAX = side
34d17: e8e83dfeff        call 0x18b04
34d1c: 5f 5e 5a 59 5b c3 pop edi/esi/edx/ecx/ebx; ret
```

The exit target `0x34D83` is the shared epilogue `pop edi; pop esi; pop edx;
pop ecx; pop ebx; ret` (`0x34D83..0x34D88`): it restores the five pushed
registers (`0x34B6C..0x34B70`) and does nothing else. So every exit of the arm
is a plain return with no store.

Operands:
- The gate word is the **side's** command word (`DS_001088E0 + side*2`, the
  word `0x3BDDC` also reads at `0x3BDFD`). The gate is an AND: it exits only
  when `(w & 0x300) != 0` **and** `(w & 0xC00) != 0` (the same predicate the
  port already writes as `bvar2` at `0x35201` and `bvar` at `0x35EE1`).
- `+0x54` is read from **ECX**, the slot record `DS_001077A8[side]` (the
  port's `rec` in `fight_health_sync`), as an unsigned byte: 0 and 1 pass.
- Both calls take **EAX = side** (EDX, kept since `0x34B71`; `0x36E2C` pushes
  only ECX and does not write EDX, and the port's case 0 already relies on the
  same EDX at `0x34C08`). `0x3BDDC` returns its result in AL (fix round 1,
  re-read from the raw). AL = 1 has three `mov al,1` sites. `0x3BE6A` is the
  `0x3CF38` hit: `0x3BE61 call 0x3cf38; test al,al; je 0x3be71`, then
  `0x3BE6A mov al,1; jmp 0x3bf64`. `0x3BF46` is the transition with command
  bit 0x2000 (`+0x4E = 0xFFFF` at `0x3BF40`). `0x3BF57` is the transition
  with bit 0x1000 (`0x3BF32 jmp 0x3bf57`) or neither bit (`+0x4E = 0` at
  `0x3BF51`). AL = 0 comes from `0x3BF62 xor al,al`, reached from the two
  rejects (`0x3BDF7`, the slot's `+0x40` bit 7; `0x3BE11`, command bit 15
  clear). The port's `fighter_attack_consume` returns the same 0/1. The
  transition stores `+0x52 = 3` (`0x3BF0A`), `+0x54 = 2` (`0x3BF10`) and
  `+0x53 = 4` (`0x3BF16`), and `DS_001078F8[side] = 1` (`0x3BF1C`).
- `0x18B04` takes EAX = side and its result is unused (the arm returns).

## §E.2 Reachability (the arm is live)

A raw rel32 `call` scan of the code object (`0x10000..0x73B14`):

- `0x36638` (`fighter_state_36638`, ported) has 13 call sites: `0x34A55`,
  `0x34BDE`, `0x358A4`, `0x358F9`, `0x36450`, `0x3651C`, `0x36A7A`, `0x36B41`,
  `0x373C6`, `0x37414`, `0x37456`, `0x375B5`, `0x3822D`. It stores
  `mov byte [ebx+0x52], 0x12` at `0x366B2` and `0x366D1`. These two are the
  only immediate-byte stores of 0x12 to a `+0x52` field (`c6 4x 52 12` /
  `c6 8x 52 00 00 00 12`) and there is no absolute store of 0x12 to
  `DS_00107802`/`DS_00107896` in the image.
- `0x3BDDC` has 7 call sites (`0x34A9F`, `0x34D0C`, `0x3520E`, `0x35F15`,
  `0x364A5`, `0x36571`, `0x3B28C`); `0x34D0C` is this arm's.
- `0x18B04` has 24 call sites; `0x34D17` is this arm's.
- `0x34B6C` has one caller, `0x357FC` in `0x35658` (`fight_hud_pass`).

So an out-of-range fighter in the `0x36E2C` position branch (`0x34BDE`), and
the default state `0x349C8` (`0x34A55`), put the slot in state 0x12, and the
next `fight_hud_pass` runs this arm. Before this unit the port's `break`
dropped the attack transition a command word with bit 15 would start there.

The port reaches it too: §E.4 gives the counts measured over `--check 8000`
and the front-end/demo-fight driver.

## §E.3 The port

`port/src/game/fight.c` case `18u` of `fight_health_sync` now does, in order:
the side's command word `DSW(DS_001088E0 + side*2)`; `break` when both the
`0x300` and `0xC00` groups are non-zero; `break` when `DSB(rec+0x54)` is
neither 0 nor 1; `fighter_attack_consume(side)` and, when it returns
non-zero, `hit_facing_flag(side)`. There is no `PORT:` note: nothing deviates.

`hit_facing_flag` (`0x18B04`) was `static` in `fighter.c` with three static
forward declarations. It is now exported through `fighter.h` (the forward
declarations are removed; the body is unchanged), in the way
`hit_reaction_pick` is exported.

## §E.4 Tests and gate

`check_state18` in `port/tests/test_fight.c` (run by `test_fight`, after
`check_hud_pass_machine`). It drives the real path, `fight_hud_pass(side)`,
with the side's slot at `+0x52 = 0x12` on the real `DS_001077B0` slot pair
(`0x3BDDC` and `0x18B04` index it by side). The slot's `+0x53 = 1` is a no-op
in `0x3531C` (`0x35803`, which runs after the sync) and makes `0x3BDDC` skip
its `0x3CF38` call, so the case-18 arm is the only writer of the observed
fields. Self's `+0x2C` (0x1000) is below the other's (0x2000), so `0x18B04`
sets the record's `+0x29` bit 0x40. Seeded sentinels: `+0x5F = 0xAA`,
`DS_001078F8[side] = 0xAA`, the record's `+0x29 = 0`, `+0x56 = 0x30`, and
`+0x54` = the case's value. Each differs from what the calls write (0xFF, 1,
0x40, 0x31, 2), and the `+0x52` seed 0x12 differs from `0x3BDDC`'s 3. The one
exception is case B2's `+0x54 = 2`, which equals the `0x3BF10` store. B2 is a
no-call case, and its `+0x52`/`+0x5F`/`DS_001078F8`/`+0x56` assertions tell
it apart. The transition's `+0x53 = 4` (`0x3BF16`) makes the same pass's
`0x3531C` (`0x35803`) take its case 4, `0x3540E inc byte [ecx+0x56]`, so
`+0x56` goes 0x30 -> 0x31. With no transition it stays 0x30, because
`+0x53 = 1` is a no-op there. This pins the one behaviour difference §E.4's
probe found (fix round 1). Six assertions per case (`s18_expect`), with the
same eight cases for side 0 and side 1:

| Case | cmd | `+0x54` | `+0x40` | 0x3BDDC | 0x18B04 |
|---|---|---|---|---|---|
| A both gate groups | 0x8500 | 0 | 0 | no | no |
| A2 0x300 group only | 0x8100 | 0 | 0 | yes (AL=1) | yes |
| A3 0xC00 group only | 0x8800 | 0 | 0 | yes | yes |
| B0 | 0x8000 | 0 | 0 | yes | yes |
| B1 | 0x8000 | 1 | 0 | yes | yes |
| B2 | 0x8000 | 2 | 0 | no | no |
| C bit 15 clear (AL=0) | 0x0000 | 0 | 0 | yes, returns 0 | no |
| C2 +0x40 bit 7 (AL=0) | 0x8000 | 0 | 0x80 | yes, returns 0 | no |

TDD: before the port, the suite failed with exactly the 40 assertions of the
eight calling cases (`FAILURES: 40`; e.g. `test_fight.c:9936: 18 != 3`,
`:9937: 0 != 2`, `:9938: 170 != 255`, `:9939: 170 != 1`, `:9940: 0 != 64`).
After: `all checks passed`. Assertion sites 13545 -> 13550 (+5); fix round
1 adds the `+0x56` assertion: 13550 -> 13551.

Mutations (each built and run with `PR_ORACLE_REQUIRED=1`; measured `FAIL`
lines, the closing `FAILURES: N` excluded):

| # | Mutation | FAIL lines | First line |
|---|---|---:|---|
| M1 | drop the both-groups exit | 10 | `test_fight.c:9936: 3 != 18` |
| M2 | gate OR instead of AND | 20 | `test_fight.c:9936: 18 != 3` |
| M3 | gate mask 0x200 instead of 0x300 | 10 | `test_fight.c:9936: 3 != 18` |
| M4 | gate mask 0x800 instead of 0xC00 | 10 | `test_fight.c:9936: 3 != 18` |
| M5 | drop the `+0x54` test | 8 | `test_fight.c:9936: 3 != 18` |
| M6 | `+0x54 == 0` only | 10 | `test_fight.c:9936: 18 != 3` |
| M7 | `+0x53` instead of `+0x54` | 8 | `test_fight.c:9936: 3 != 18` |
| M8 | `0x18B04` regardless of AL | 4 | `test_fight.c:9940: 64 != 0` |
| M9 | drop `0x18B04` | 8 | `test_fight.c:9940: 0 != 64` |
| M10 | drop both calls | 40 | `test_fight.c:9936: 18 != 3` |
| M11 | side 0's command word | 5 | `test_fight.c:9936: 3 != 18` |
| M12 | `0x3BDDC(0)` | 20 | `test_fight.c:9936: 18 != 3` |
| M13 | `0x18B04(1 - side)` | 8 | `test_fight.c:9940: 0 != 64` |
| M14 | case 18 back to a bare `break` | 40 | `test_fight.c:9936: 18 != 3` |

14 of 14 fail (measured before fix round 1's `+0x56` assertion, so the
counts are of the five-assertion `s18_expect`).

Fix round 1 mutations (six-assertion `s18_expect`; the `+0x56` assertion is
`test_fight.c:9947`):

| # | Mutation | FAIL lines | Of which `:9947: 48 != 49` |
|---|---|---:|---:|
| F1 | the arm does not call `0x3BDDC` (`if (0)`) | 48 | 8 |
| F2 | `0x3531C` case 4 drops its `+0x56` increment (`fighter.c`, `0x3540E`) | 9 | 8 (the ninth is `:9241: 17 != 18`, an existing check) |
| F3 | case 18 back to a bare `break` | 48 | 8 |

The probe evidence of §E.4 was re-run in fix round 1 and saved as
`.superpowers/sdd/2026-09-29-k7-k12/scratch/ne_probe_run.txt` (git-ignored).
New port: 56 arm runs, 56 `0x3BDDC` calls, AL = 1 twice (f = 2329 side 1,
f = 3773 side 0), 3 `0x350D0` transitions (f = 2061, 2112, 3921). Old port:
5 `0x350D0` transitions (the same three plus 2329 and 3773). The demo-fight
driver has 0 probe lines. The two `--check 8000` frame sets (24000 files
each) have 0 differences. The probe build was removed and the sources
restored (`git diff --quiet HEAD -- port/src`).

Gate (plain `make verify` with the `/tmp/pr_ne_*` overrides): EXIT=0, 0
compiler warnings; the oracle lines equal
`k7-k12/scratch/oracle-lines-base.txt` (empty `diff`); `dumps.sh ne` +
`dumpsha.sh ne` match `base.sha256` (24000 files in the dump, including the
16000 `--check 8000` frames); `make audio-render` is byte-identical to
`before-t2.wav`. `port_progress.py`: `767 1203 64` / `731 731 100`,
unchanged (no new function).

**Why a live arm moves no dump (measured).** Temporary probes (a `fprintf` in
the arm and at `0x350D0`'s `0x3520E` transition, built in a throw-away
`build-probe/`, the sources restored and `cmp`-checked afterwards):

- Over `--check 8000` the arm runs 56 times (25 for side 0, 31 for side 1),
  every time with `+0x54 = 0` and a word that passes the gate (none has both
  groups; `0x0303` has only the `0x300` group), so all 56 call `0x3BDDC`.
  54 return AL = 0 (command bit 15 clear: words `0x0000`, `0x1010`,
  `0x2020`, `0x4040`, `0x5050`, `0x6060`, `0x0303`). Two return AL = 1 and
  run `0x18B04`: frame counter `DS_000EF6DC` = 2329 (side 1) and 3773
  (side 0), both with the word `0x9090`.
- The front-end/demo-fight driver (`PR_FRONTEND_DET`, the oracle's window)
  never reaches the arm (0 hits).
- With the arm switched back to the old `break` in the same probe binary, the
  same two transitions happen in the same frames through the old route,
  `0x35803` `0x3531C` -> `0x350D0` -> `0x3520E` (`0x3BDDC`, then `0x18B04`
  at `0x3521A`), which has the same `0x300`/`0xC00` gate. With the arm live,
  `0x350D0`'s transition count over the run drops from 5 to 3 (the arm takes
  those two first). The two `--check 8000` frame sets (old vs new, 24000
  files) are byte-identical.

So on these runs the arm is reached but its effect is duplicated later in the
same `fight_hud_pass` by `0x3531C`'s default. One state difference remains and
the frames do not show it. After the arm's transition the slot's `+0x53` is
4, so the same pass's `0x3531C` takes case 4 and increments `+0x56`. The old
route left `+0x56` alone that frame. The new behaviour is the raw's, since
the raw runs `0x34B6C` (`0x357FC`) before `0x3531C` (`0x35803`). The arm is
not duplicated when `0x3531C` does not reach `0x350D0` (`+0x53` not 0, 0xD or
> 0xF) or `0x350D0` returns before `0x3520E` (`+0x78 > 0`, `+0x54 == 2`).
The test's `+0x53 = 1` is such a state.

## §E.5 Not tested

- `0x3BDDC`'s `0x3CF38`-hit return (`0x3BE6A`, after the `0x3BE61` call;
  AL = 1 without the attack state) reached through case 18: the fixture's `+0x53 = 1` skips that call on
  purpose. The callee's arms are covered by `check_attack_consume`; the arm's
  own contract (AL != 0 -> `0x18B04`) is covered by B0/B1.
- `0x18B04`'s mode-0x22 early return and its `+0x18` write
  (`0x18714`) through case 18: the callee's own tests cover them
  (`test_fight.c` "0x18B04 returns at once in mode 0x22", `hit_record_x`).
- A slot `+0x54` value other than 0, 1, 2 (e.g. 0x80, 0xFF): the raw compare
  is an unsigned byte equality to 0 and 1, and 2 exercises the reject.
