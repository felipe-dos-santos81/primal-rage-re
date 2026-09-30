# Named gaps B — faults and the soft restart: derivation record

Plan: `docs/superpowers/plans/2026-09-30-named-gaps-b-faults-longjmp.md` (as
corrected by `b272ccb`, cherry-picked onto this branch as `f364e04`: the port
already advances the idle clock). Spec: `docs/superpowers/specs/2026-09-30-named-gaps-design.md`
(§8 wins). Evidence consumed: sub-project A's record
`docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md` (§A.1, §A.6..§A.10).
Branch `named-gaps-b` off local `main` `47757c4`; tag `nb`.

Raw source: the fixup-applied image mirror `k11_img.bin` (indexed by linear
address = Ghidra address) in the session scratchpad, capstone through
`k11_dx.py`, and the scan `nb_raw.py` (every `E8`/`E9` rel32 in the code
object `0x10000..0x74000` and every byte pattern below).

## §B.0 Baseline (Task 0)

- Tree: `47757c4` + `f364e04` (docs only).
- `python3 tools/port_progress.py`: `769 1203 64` and `731 731 100` (the plan
  expected `765 1203 64`; the tree moved since the plan: units D/F/A merged.
  The new numbers are the baseline).
- Assertion sites (`rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l`): **13678**.
- Gate t0 (`make verify` with the `nb` overrides incl. `K11_DUMP=/tmp/pr_nb_k11`, then
  `dumps.sh nb` + `dumpsha.sh nb`, then `make audio-render` + `cmp` with
  `before-t2.wav`): `EXIT=0`, `ORACLES-EQUAL` (45 lines), the five `k11_compare:
  walk:` lines as A's §A.5 (window `[90..170]`, 38/38, allowed `[138, 151]`, 0
  unexplained), `DUMPS-IDENTICAL`, `WAV-IDENTICAL`.

## §B.1 WATCOM setjmp/longjmp (F1, re-verified)

- `0x65431` (`FN_00065431`) is `longjmp(jmp_buf *eax, int edx)`: `push eax;
  push edx; mov dx,[eax+0x2A]; mov eax,[eax+0x1C]; call [0xF0900]` (the
  runtime's stack-switch notify), `pop edx; pop eax`, then `0x65442 mov
  ss,[eax+0x2A]; 0x65445 mov esp,[eax+0x1C]; 0x65448 push [eax+0x18]` (the
  saved return EIP), `0x6544B or edx,edx; jne; 0x6544F inc edx` (a value of 0
  becomes 1), `push edx`, `ebx/ecx/esi/edi/ebp` from `+0/+4/+0xC/+0x10/+0x14`,
  `es/fs/gs` from `+0x20/+0x26/+0x28` (each `verr`-checked, else 0),
  `edx=[eax+8]`, `ds=[eax+0x22]`, `pop eax` (= the value), `0x6548D ret`.
- `0x653FC` (`FN_000653FC`) is `setjmp`: stores `ebx..ebp` at `+0..+0x14`,
  pops the return EIP into `+0x18`, `esp` into `+0x1C`, re-pushes it, saves
  `es, ds, cs, fs, gs, ss` at `+0x20..+0x2A`, `0x6542E sub eax,eax` (0),
  `ret`. The jmp_buf is `0x2C` bytes.
- Scan (`nb_raw.py`): the only rel32 `call`/`jmp` to `0x653FC` is `0x20C1F
  call`; the only ones to `0x65431` are `0x24AB0 jmp`, `0x2520B jmp`,
  `0x2EBB3 jmp` (no `call`). The immediate `0x1044F4` occurs at exactly
  `0x20C1A`, `0x24AAB`, `0x25206`, `0x2EBAE` (each `mov eax,0x1044f4`).
  **The setjmp site is single**, so spec §3.4 decision 4 applies: a real C
  `setjmp`/`longjmp` pair.

## §B.2 Where the longjmp resumes: the post-setjmp tail of 0x20C10 (F2, F7)

- `0x1C0BD call 0x20C10` is `0x20C10`'s one caller (in `0x1BEC4`); after it
  `0x1C0C2 call 0x10D34`, then the teardown.
- `0x20C10`: `push ebx; push edx; sub esp,0x28; 0x20C15 call 0x2F9CC;
  0x20C1A mov eax,0x1044F4; 0x20C1F call 0x653FC; 0x20C24 mov eax,-1` — the
  setjmp's value is discarded (EAX overwritten), so the first pass and every
  longjmp run the same tail `0x20C24..0x20DE3`, then `0x20DE8 call 0x255CC`
  (the master loop, re-entered from its entry, so its prologue
  `0x255D4/0x255DA` zeroes the tick pair `DS_00101508`/`DS_0010150C` again),
  then `0x20DED add esp,0x28; pop edx; pop ebx; ret`.
- `0x2F9CC` (before the setjmp) is **not** re-run by a longjmp. Neither is
  anything in `0x1BEC4` before `0x1C0BD`, in particular **`0x1C0B1 call
  0x1D0BC`** (the sound buffers) and `0x1C0B8 call 0x47370`.
- **Correction to the plan (raw wins, `0x1C0B1`).** Task 1 says to cut
  "everything from `render_projection_reset(0u);` to the end of `game_init`"
  into the resume tail "unchanged, in its current order". That block holds
  `sound_buffers_alloc()` (`0x1D0BC`), which the raw calls at `0x1C0B1` in
  `0x1BEC4`, *before* `0x20C10`, so a restart does not re-run it. (Re-running
  it would change nothing: `0x1D0BF cmp byte [0xA2CB0],0; jne 0x1D1A9`
  returns at once once `0x1D19D` has set the byte, so moving the call into the
  tail is an equivalent mutant; the placement follows the raw's order, review
  1.) It stays in `game_init`, moved above the split (first-boot order: it still runs before
  `game_state_init`'s movie loads, so the heap layout is unchanged; the gate
  proves it).
- Two zero stores of the tail the port lacked: `0x20C29 xor dl,dl ... 0x20C37
  mov [0x104B1D],dl` (`0x2C8F0` keeps EDX, `0x29D60` is a bare `ret`) =
  `DS_00104B1D = 0`; `0x20CD3 xor ebx,ebx ... 0x20CDF mov [0x104AFC],bx`
  (`0x32970` and `0x13ADC` keep EBX) = `DS_00104AFC = 0`. Both image bytes are
  0, so on the first boot the stores are no-ops.
- The tail against the port (F7; the omissions predate B and are not added by
  B, because they would move the first-boot state the oracles pin):

  | Raw (post-setjmp) | Port | Status |
  |---|---|---|
  | `0x20C2B 0x2C8F0(-1, dl=0)` | `attract_config_volumes_unscaled()` exists | not called by `game_init`: pre-existing omission |
  | `0x20C32 0x29D60` | bare `ret` | nothing to do |
  | `0x20C3D 0x38B70`, `0x20C42 0x2F920` | inside `actors_reset()` | run through `game_state_init` (`0x10E9C`) |
  | `0x20C49 0x4F228(0)` | `render_projection_reset(0u)` | in the tail |
  | `0x20C58 0x2BAF4(1)` + `0x20C62 [0xEF6D8]=0xABCD` | `rng_seed(0xABCDu)` | in the tail; the `0x2BAF4` call itself is not repeated (pre-existing) |
  | `0x20C68 0x2D974(0x29)` .. `0x20CC2` | `config_field_get(0x29u)` and the three derived stores | in the tail |
  | `0x20C7F 0x47370` | `game_string_table_load` (idempotent) | in the tail, later in the order (pre-existing) |
  | `0x20C84 0x1E824` | `hiscore_init()` | in the tail |
  | `0x20CC7 0x32968` | `game_init_null()` | in the tail |
  | `0x20CCC 0x2BF00` | `config_set_credit_row_init()` | in the tail |
  | `0x20CD5 0x32970(0,0)` | none (run clock out of scope, attract.c) | pre-existing |
  | `0x20CDA 0x13ADC` | `effects_init()` inside `actors_init`/`actors_reset` | pre-existing placement |
  | `0x20CE6 0x10E80` | `game_state_init()` | in the tail |
  | `0x20CEB 0x5D808` | none (runtime, >= `0x5D000`) | pre-existing |
  | `0x20CF2 0x1AEE0`, `0x20CF9 0x1AF64` | `config_keys_pack`/`config_keys_load` | in the tail |
  | `0x20D05/0x20D0A [0x107468]=[0x10746C]=word [esp+0x24]` | none | pre-existing omission |
  | `0x20D0F..0x20DE3` controller checks, `0x20DAC 0x4FBBB(3)`, `0x20DD1 0x4FBBB(0xC)` | none (`PORT: 0x5004A joystick init`) | pre-existing |

- The port's pre-setjmp part of `game_init` (not re-run): the image map, the
  `0x1BEC4` chain (`int10h_query`, `GAME_BIOS_BASE`, `game_audio_init`,
  `res_load_index`, `actors_init`, `surface_setup`, `palette_list_init`,
  `render_list_init`, `sound_buffers_alloc`), and the `0x2F9CC` work
  (`DSD(DS_0010740C) = 0x1D2D0`, `config_validate()`, `DSD(DS_00107410) =
  config_field_get(0x2A) & ~3`, `svcmenu_register()`), which the port ran
  after `render_projection_reset`/`rng_seed`; B moves it above the split (it
  runs at `0x20C15`, before the setjmp).
- **Landing (`PORT:`).** The port's drivers step `game_loop()` one iteration
  per call, so the restart point is armed in `game_loop()` (the port's one
  entry to `0x255CC`), not in `game_init`: a longjmp lands there, runs
  `game_init_resume()` (the tail) and `game_loop_begin()` (`0x255D4/0x255DA`),
  then runs the loop body from its top, as the raw re-enters `0x255CC` at
  `0x20DE8`. The iteration that jumped is abandoned, as in the raw (its frames
  lie below the restored ESP).

## §B.3 The three longjmp sites (F3, re-verified)

- `0x2EBB3` in `0x2EB80` (`config_key_latched`): `0x2EB81 mov edx,[0x105F30];
  test; je 0x2EB8F` (a latched key returns it), `0x2EB8F call 0x500BB; sub
  eax,[0x105F2C]; cmp eax,0x4B0; 0x2EB9F jbe 0x2EBB8` (unsigned), `0x2EBA1 xor
  ah,ah; 0x2EBA3 mov edx,1; 0x2EBA8 mov [0x107414],ah; 0x2EBAE mov
  eax,0x1044F4; 0x2EBB3 jmp 0x65431`. The idle timeout proper.
- `0x2520B` in `0x24C5C` case `0x27`: `0x251F3 test eax,eax; je 0x25210; cmp
  eax,-5; je; cmp eax,-10; je; 0x25201 mov edx,1; 0x25206 mov eax,0x1044F4;
  0x2520B jmp 0x65431`; `0x25210 call 0x4F644; jmp 0x2540F`.
- `0x24AB0` in `0x249F0` (the quit prompt): `0x24A89 mov [0x104B22],bh`
  (0), `0x24A8F test cl,cl; 0x24A91 jne 0x24A9C` (AL != 0), else `0x24A93 mov
  byte [0xA81A8],1; jmp 0x24AEC`; `0x24A9C call 0x1D270; 0x24AA1 call 0x1B084;
  0x24AA6 mov edx,1; 0x24AAB mov eax,0x1044F4; 0x24AB0 jmp 0x65431`.
  **Correction (raw wins):** the port set the quit-to-DOS flag here, so
  ABANDON CONQUEST's yes ended the process; the raw soft-restarts.
- **A's named gap 4 pinned from the raw** (A §A.6: "the instruction that
  clears `DS_00107414` on the MAIN MENU Esc is not pinned"): `0x2FFC4`'s Esc
  arm `0x30430 test byte [0x107418],4; je 0x30449; 0x30439 xor al,al;
  0x3043B mov [0x107414],al; 0x30440 mov eax,-1; ret` (menu.c `MENU_ACTIVE`).
  With the MAIN MENU's flags 4 the step clears the byte and returns -1, and
  case `0x27` then longjmps at `0x2520B`: the capture's `menu=00` one poll
  before `mode=0003` is this store (A §A.6, `menuesc/poll.log:1593..1596`).

## §B.4 Exception handlers and the G2/G3 branches

- No program handler for `#DE` or `#PF` (F5, A §A.1.4, re-scanned here): no
  `mov ax,0203h`/`mov eax,0203h` (DPMI set exception handler), no
  `0212h`/`0213h`, no `mov ax/eax,2500h` (vector 0) in the code object; the
  two `int 21h AH=25h` sites set vector 8. DOS/4GW's default handler runs.
- **G3 (`0x334E0 idiv ebx`, F6 re-verified `0x334C9..0x334E2`).** A's `de`
  capture (§A.8): with fields 8 = `0xFFFF`, 6 = `1` the game **aborts to DOS**
  on entering STATISTICS page 1. `de/dosbox.log:15`, verbatim:
  `DOS/4GW Professional error (2001): exception 00h (divide by zero) at 180:002244E0`,
  then the register dump (`dosbox.log:16..28`) ending `Crash address
  (unrelocated) = 1:000234E0`. Exception `00h`; runtime EIP `0x2244E0` =
  object-1 offset `0x234E0` + the capture's code-object base `0x201000`, i.e.
  Ghidra `0x334E0`. The errorlevel is **not captured** (DOS ran the script's
  `EXIT` next, `dosbox.log:29`). **Branch: ABORT-PINNED** — the port ends the
  run at `0x334E0` through `host_cpu_fault(0x00, 0x334E0, <that line>, 1)`,
  where the `1` is a `PORT:` (errorlevel not captured) and the message goes to
  stderr where the original prints over the text screen (`PORT:`). The
  register dump lines are not reproduced (they hold DOSBox-X run values).
- **G2 (`0x32573 mov ebx,0xFFE80003; 0x32578 mov bl,[ebx]`, F5 re-verified
  `0x32554..0x32596`).** Unreachable in the stock game (A §A.1.1: field
  `0x2A` is 4 bits, `DS_00107410 = field & ~3` never has bit 4). A's `diags`
  capture (§A.7) pokes `DS_00107410 |= 0x10`: the original **does not fault**
  (DOS/4GW runs with paging off, `CR0: PG:0` in the `de` dump, and a flat 4 GB
  DS) and draws `000000FF` on row 7 (`diags frame 111 (raw 2383)`, held to raw
  2663). **Branch: SILENT** — the port draws `text_hex_set(0xE, 7, 0xFF, 8, 0,
  0x3000)` with a `PORT:` naming DOSBox-X's answer for physical `0xFFE80003`
  (real hardware is not captured: A's named gap 3).
- `$K/branch.txt`: `G2=SILENT`, `G3=ABORT-PINNED`.

## §B.5 The idle clock and its reference as the port has them (F4)

- The ISR `0x1BDF4` (re-verified): `0x1BDF8 mov al,[0x104B22]; cmp eax,1; je
  0x1BE2D` (the gate), `0x1BE02..0x1BE16` increment `[0x101508]` and
  `[0x101500]`, `0x1BE1C call 0x1BBAC`, `0x1BE21 inc word [0xEF6DE]`, `0x1BE28
  call 0x2D62C`. `0x500BB` returns `[0x101500]`.
- The port already advances `DS_00101500`: `game_isr_ticks(n)` (flow.c) from
  the `0x256C5` spin (one tick per retrace) and res.c's read stall;
  `config_screen_wait` models the ISR itself. The reference `DS_00105F2C` is
  stored at all four raw sites; the compare is ported in `config_key_latched`.
  Only the `0x2EBB3` jump was missing: the port stored `DS_00107414 = 0` and
  returned 0, so the menu re-initialised every `0x4B1` idle ticks where the
  raw restarts. B changes nothing in the clock.

## §B.6 Evidence consumed from A

- **G1 (A §A.6, §A.10).** Idle: the Enter at `idle/poll.log:1405` stamps
  `ktime = 0x572`; `poll.log:2615` tick `0xA22` (difference `0x4B0`, not over)
  `menu=01`; `poll.log:2616` tick `0xA23` (`0x4B1`) `menu=00`; `poll.log:2618`
  tick `0xA25` `mode=0003 st=0000 f=05F4`. The frame word `DS_000EF6DC` is kept
  (`0x5F3` -> `0x5F4`), and so are the clock `DS_00101500` (`tick` runs on,
  `0xA23`, `0xA24`, `0xA25`), the stamp `DS_00105F2C` (`ktime=00000572`) and
  the menu entry `DS_0010741C` (`ent=002A2BEC`). Screen: one black frame
  (raw 3150..3153) then the TWI5 logo (`idle frame 108`, equal to
  `data/title-captures/frontend/frame_1887.raw`) and the whole boot sequence.
  MAIN MENU Esc: `menuesc/poll.log:1590..1596`, the same restart. This
  matches F8's expectation (mode 3, attract state 0, `DS_00107414 = 0`); the
  tick pair `DS_00101508/0x10150C` is not in the poll log (F8's "restarted
  from 0" is the raw's `0x255D4/0x255DA`, not A's evidence).
- **G2 (A §A.7), G3 (A §A.8):** §B.4.

## §B.6a Corrections to the plan and spec found in Task 0

1. `0x24AB0` is a soft restart, not the quit-to-DOS the port modelled
   (flow.c `game_quit_prompt`) — plan correction (1), confirmed.
2. `tools/port_classification.txt:69` says `4FBBB` is "called only by the ISR
   sampler 0x1BBAC"; the raw also calls it at `0x20DAC` and `0x20DD1`
   (`0x20C10`'s controller checks) — plan correction (2), confirmed.
3. Withdrawn by the plan revision `b272ccb`: the port's master loop does
   advance `DS_00101500` (§B.5).
4. The spec's G1 row names only `0x2520B`; the idle timeout proper is
   `0x2EBB3` in `0x2EB80`, and `0x24AB0` is the same restart — confirmed.
5. **Plan correction (5) is itself wrong on this tree:** the spec's citation
   "K7+K12 record §2.6 (idle-timeout store)" resolves:
   `2026-09-29-k7-k12-derivations.md` has `### §2.6 Correction to §0.7.2 (raw
   wins): the idle timeout is a master-loop reader` (line 1208) and the named
   gap text at line 1058 (`rg -n 'idle|0x107414|0x2EBA8'` finds both).
6. `sound_buffers_alloc()` (`0x1C0B1 0x1D0BC`) is pre-setjmp (§B.2); the
   plan's Task 1 cut would have moved it after the setjmp. A second call
   returns at `0x1D0BF` (the `DS_000A2CB0` gate, set at `0x1D19D`), so that
   would not have been observable; the placement is by the raw's order
   (review 1 corrected an earlier "allocates twice").
7. G3's `idiv` is reached from STATISTICS page 1 (`0x33058`), not page 2 (A
   §A.8; spec §2's "STATISTICS page 2" and the plan's F8 wording).
8. The instruction A left unpinned for the MAIN MENU Esc clear is `0x3043B`
   (§B.3).
9. **Found, out of B's scope (reported, not changed):** flow.c's mode-0x12
   step (`game_mode_12_step`, sub-state 7) carries `PORT: 0x42420
   longjmp(0x2DAE4, 0x10, 1) — the front-end quit path, out of scope (spec
   §7)` (and flow.h's "audit close + longjmp 0x2DAE4(0x10)"). The raw is
   `0x42420 mov eax,0x10; 0x42425 call 0x2DAE4` with EDX = 1 (`0x42415`):
   a plain call of the audit add `0x2DAE4` (A §A.1.2; field `0x10` += 1),
   then `0x4242A..0x42433` the function's epilogue. It is not `0x65431` (the
   scan in §B.1 finds only the three `jmp 0x65431`), so it is not a longjmp
   and not part of G1; it is an unwired audit-counter call to hand to the
   ledger owner.

## §B.6b Task log

### Task 1: the restart point and the resume tail

- flow.c: `game_restart_arm` (`PORT:`), `game_restart_longjmp` (`0x65431`),
  `game_init` split at the setjmp: the pre-setjmp part now ends with
  `sound_buffers_alloc()` (`0x1C0B1`, correction §B.6a 6) and the four
  `0x2F9CC` lines (moved up from after `rng_seed`), then calls
  `game_init_resume()` (`/* 0x20C24..0x20DE3 */`), which holds the rest in
  its old order plus `DSB(DS_00104B1D) = 0` (`0x20C37`) first and
  `DSW(DS_00104AFC) = 0` (`0x20CDF`) after `config_set_credit_row_init()`.
  The `PORT: 0x5004A joystick init` comment moved with the pre-setjmp part
  (`0x1C0AC`, in `0x1BEC4`). `game_loop()` arms a local jmp_buf; a landing
  runs `game_init_resume()` and `game_loop_begin()` and then the loop body.
  flow.h declares the three functions (and includes `<setjmp.h>`).
- Tests (`test_game.c`, `test_restart`, registered last): `rs_check_landing`
  (3 sites), `rs_check_resume` (8 sites). The implementation was written
  before the build was run, so the "fails to build" step was not observed;
  every assertion is instead proved by a mutation below.
- Mutations (scratch build, `PR_ORACLE_REQUIRED=1 run_tests`; restored after
  each; lines are `port/tests/test_game.c`):

  | # | mutation | measured |
  |---|---|---|
  | M1 | `longjmp(*s_restart_point, 1)` -> `2` | `FAIL ...:12024: 2 != 1` |
  | M2 | delete `DSB(DS_00104B1D) = 0u;` | `FAIL ...:12045: 165 != 0` |
  | M3 | delete `DSW(DS_00104AFC) = 0u;` | `FAIL ...:12046: 119 != 0` |
  | M4 | delete `game_state_init();` from the tail | `FAIL ...:12048: 39 != 3`, `FAIL ...:12049: 51 != 0` |
  | M5 | `s_restart_point = jb;` -> `= prev;` | run_tests exits 1, stderr `Primal Rage: restart longjmp with no restart point` |
  | M6 | delete `render_projection_reset(0u);` from the tail | **survives** (equivalent): `game_state_init`'s `actors_reset` (`0x10E9C 0x2BAF4`) re-runs `0x4F228` at `0x2BBC0`, so the tail's post-state is the same (the raw's own `0x20C58 0x2BAF4` does the same) |
  | M4+M6 | both | `FAIL ...:12047: 1 != 0` (the `DS_00107A54` check can fail), `12048: 39 != 3`, `12049: 51 != 0` |
  | M7 | delete `rng_seed(0xABCDu);` | `FAIL ...:12042: 4660 != 43981` |
  | M8 | delete the `DS_001088D0` store | `FAIL ...:12044: 57005 != 30` |
  | M9 | delete `DSD(DS_00104528) = v;` | `FAIL ...:12043: 2779096485 != 0` |

Gate t1 (full): `EXIT=0`, `ORACLES-EQUAL`, the five `k11_compare: walk:` lines
unchanged, `DUMPS-IDENTICAL`, `WAV-IDENTICAL` — the `0x2F9CC` reorder, the
`sound_buffers_alloc` move and the two zero stores are first-boot no-ops.

### Task 2: the idle timeout 0x2EBB3

- config.c `config_key_latched`: the `PORT:` line becomes
  `game_restart_longjmp()` after the `0x2EBA8` store; header and config.h
  rewritten. Nothing else (clock, reference, compare) changes (§B.5).
- New `rs_check_idle` (12 sites: the boundary, one over, the unsigned wrap,
  the latch).
- **Existing assertions changed because the raw wins** (each pinned the old
  "store and return 0"; each now arms a local restart point):
  - `test_game.c` `check_idle_timeout_clock` (record k7-k12 §2.6): the
    one-over `CHECK_EQ_INT(config_key_latched(), 0)` becomes
    `CHECK_EQ_INT(landed, 1)`; the boundary call is made under `setjmp` too
    (`CHECK_EQ_INT(got, 0)`), so a mutant that jumps there lands instead of
    longjmping into an unset jmp_buf.
  - `test_game.c` `ch_check_key_flags`: the same two changes for the one-over
    and the wrap (`5001`) calls, and the boundary call under `setjmp`.
  - `test_platform.c` `check_menu_step`: "Idle past 0x4B0 ticks clears the
    active flag (the longjmp's stand-in)" — `CHECK_EQ_INT(menu_step(..), 0)`
    becomes `CHECK_EQ_INT(landed, 1)`.
  - `test_game.c` `vs_menu_run`/`vs_menu_step` (voice-site rows 196/197): no
    assertion changes; they now stamp `DS_00105F2C` with the clock first
    (the suite's clock has run past the timeout; with the jump they would
    otherwise reach it with no restart point armed).
  The site count is unchanged by these conversions (one `CHECK` replaces one).
- Mutations (dev copy; `M` = measured):

  | # | mutation | measured |
  |---|---|---|
  | T2M1 | `> 0x4B0u` -> `>= 0x4B0u` | `FAIL ...test_game.c:1546: 65261 != 0`, `:1547: 0 != 90`, `:7537: 65261 != 0`, `:7538: 0 != 85`, then an unarmed call elsewhere ends the suite (`restart longjmp with no restart point`, exit 1) |
  | T2M2 | delete `game_restart_longjmp();` | `FAIL ...test_game.c:1548: 0 != 1`, `:7537`/`:7543: 0 != 1`, `test_platform.c:3062: 0 != 1`, `test_game.c:12093: 0 != 1`, `:12094: 0 != 65261`, `:12103: 0 != 1` |
  | T2M3 | delete `DSB(CFG_IDLE_FLAG) = 0u;` | `FAIL ...test_game.c:1549: 90 != 0`, `:7538`/`:7544: 85 != 0`, `test_platform.c:3064: 1 != 0` (and 10 later menu lines), `test_game.c:12095`/`:12104: 90 != 0` |
  | T2M4 | the compare made signed | `FAIL ...test_game.c:7543: 0 != 1`, `:7544: 85 != 0`, `:12103: 0 != 1`, `:12104: 90 != 0` |

  (Line numbers are those of the dev tree with Tasks 2-4 applied.)

### Task 3: case 0x27's longjmp 0x2520B

- flow.c case `0x27u`: `else game_restart_longjmp();` after the
  `0/-5/-10` test; the `PORT:` sentence replaced. The comment's `0x25206`
  (the `mov eax`) is now `0x2520B` (the jump).
- `rs_check_case27` (12 sites) drives `game_frame()` in mode `0x27`
  (`kl_env`, `ni_frame_env`, `sm_env_begin` for the pad layout and a fresh
  stamp), under `ra_save`/`ra_restore`. **Plan correction:** the plan expected
  `DS_001088E0 = 0` after the START MENU Esc; `0x4F644` writes the live pad
  there and the Esc is still held (`0x2000000` -> 512), so that check is
  "`!= 0xBEEF`" (the normal exit ran `0x25210`). Added: `DS_00107414 = 0`
  after the landing (seeded `0x5A`), the `0x3043B` store (§B.3).
- Mutations:

  | # | mutation | measured |
  |---|---|---|
  | T3M1 | delete the `else` arm | `FAIL ...test_game.c:12174: 0 != 1` |
  | T3M2 | drop `-5` from the normal set | `FAIL ...:12159: 1 != 0`, `:12161: the START MENU Esc (-5) runs 0x25210 0x4F644`, `:12167: 1 != 0` |
  | T3M3 | drop `0` from the normal set | `FAIL ...:12147: 1 != 0`, `:12149: 48879 != 0`, `:12159`, `:12167: 1 != 0` |
  | T3M4 | menu.c: delete `0x3043B`'s store | `FAIL ...test_game.c:12176: 90 != 0` (and 7 `check_menu_step` lines) |

### Task 4: the PR_RESTART driver

- `test_restart_drive` (`TEST_DRIVERS`, `PR_RESTART`; Makefile `verify` runs
  it after the unit suite). `game_init()`, `game_loop_begin()`, 50 boot
  iterations recording the attract state, the displayed frame and the RNG
  word after the first; then the raw's own entry to mode `0x27`, the Enter
  key (scan `0x1C`, ascii `0x0D`) queued in mode 3 (`0x24EE0`) rather than
  the plan's direct store; one iteration initialises MAIN MENU (`0x300C2`
  sets `DS_00107414 = 1`, `0x2FFDA` stamps); then idle iterations until the
  mode leaves `0x27` (guard 5000).
- Measured: `restart after 1198 menu iterations (stamp 3E, clock 4EF)`:
  `0x4EF - 0x3E = 0x4B1` on the restarting iteration and `0x4B0` on the one
  before (the two `0x2EB9F` checks), as A's capture (`0xA23 - 0x572 = 0x4B1`,
  A §A.6). With the stamp moved one tick later (temporary edit) the restart
  came one iteration later (`1199`, stamp `3F`, clock `4F0`) and every check
  still passed.
- Post-state checks: mode 3, `DS_00107414 = 0`, the tick pair 1/1, the clock
  `DS_00101500` not reset and the frame word `DS_000EF6DC` = the value before
  the restarting iteration + 2 (the abandoned iteration's `0x24CDB` and the
  new one's; A §A.6: kept), then attract state, RNG word and displayed frame
  equal to the first boot's for 50 iterations.
- **The displayed frame is compared as RGB** (each pixel's DAC colour, which
  is what the RGB captures hold). Measured on the first run: after the
  restart the same picture uses palette indices one lower than on the first
  boot (index 22 -> 21, identical DAC colours; 0 RGB bytes differ over frames
  1..49); comparing indices failed 48 frames. **Cause (review 1; an earlier
  version of this record blamed the palette list, wrongly: the tail's
  `game_state_init` runs `0x2BAF4(1)`, whose non-zero arm calls
  `palette_list_init` `0x336C0`, actors.c, as the raw's `0x20C58`/`0x10E9C`
  do).** Re-measured with an instrumented scratch build (the ownership table
  at `DS_00107618`, `{handle, refs, slot, len}`, after boot iteration 3, and a
  print in `res_load_present`): first boot `[80997C 14 1 1] [396ED28 2 2 3F]
  [396ECE8 1 41 F] [396EAE8 2 50 1F] ...`, with one loader draw (`index 7
  tick 2`); after the restart `[396ED28 2 1 3F] [396ECE8 1 40 F] [396EAE8 2
  4F 1F] ... [105FE30 1 CC F]` and no loader draw. On the first boot the
  attract's first acquire resolves a lazy entry not yet loaded, so
  `res_resolve` draws `- LOADING -` (`0x1B5E0`, `0x1C5E8`), whose font
  palette `0x80997C` takes slot 1 before the attract's palettes; after the
  restart the entry keeps its loaded mark (`0x1B47A`; the port's bump
  allocator never evicts), no loader draw runs, and every attract palette sits
  one slot lower. Whether the raw keeps that entry resident across a restart
  (its block allocator `0x1C308` can free) is not shown, and the RGB captures
  cannot see slots: a named gap (§B.7).
- **Plan correction (equivalent mutant):** the plan expected removing
  `game_loop_begin()` from the landing to fail the tick-pair checks. It does
  not: the tail's `game_state_init` runs `0x2BAF4` with EAX = 1
  (`0x10E9C`), whose `param_1 != 0` arm calls `0x52108` (`gfx_screen_reset(0)`,
  `0x2BBE8`), which stores 0 in both `DS_00101508` and `DS_0010150C`
  (`0x52108/0x5210D`). The raw's own tail does the same (`0x20C58 0x2BAF4(1)`
  and `0x10E9C`), so `0x255D4/0x255DA` are redundant after a restart there as
  well. The pair checks are proved by T4M3.
- Mutations (`PR_RESTART=1`):

  | # | mutation | measured |
  |---|---|---|
  | T4M1 | delete `DSD(DS_00101500) += n;` in `game_isr_ticks` | `FAIL ...:12255: the idle timeout restarts within the guard`, `:12256`, `:12258: 39 != 3`, `:12259: 1 != 0`, `:12260`/`:12261: 5002 != 1`, `:12262`, `:12264`, `:12266`, `:12267`, the per-frame checks |
  | T4M2 | delete `game_loop_begin();` from the landing | survives (equivalent, above) |
  | T4M3 | delete `game_init_resume();` from the landing | `FAIL ...:12255: the idle timeout restarts within the guard`, `:12257`, `:12258: 39 != 3`, `:12259: 1 != 0`, `:12260`/`:12261: 206 != 1`, `:12264: 5056 != 5057`, `:12266: 600282692 != 43981`, `:12267`, the per-frame checks |
  | T4M4 | the landing also zeroes `DS_000EF6DC` and `DS_00101500` | `FAIL ...:12262: the 0x500BB clock is not reset (A §A.6)`, `:12264: 1 != 1251` |

### Task 5: ABANDON CONQUEST's yes is the soft restart (0x24AB0)

- flow.c `game_quit_prompt`: the AL != 0 yes arm is `sound_resume()`
  (`0x24A9C`), a `PORT:` for the deferred `0x1B084` (`0x24AA1`, record §50-C),
  then `game_restart_longjmp()` (`0x24AA6..0x24AB0`); the quit flag is no
  longer set there. Header and flow.h rewritten ("longjmp quit" is gone from
  `port/src`).
- **Existing expectations changed (raw wins):** `ch_check_quit_prompt` pass 2
  (AL = 1, yes) now lands at an armed point (`CHECK_EQ_INT(landed, pass == 2u
  ? 1 : 0)`, one new site per pass loop) and expects the quit flag to keep its
  seed `0x5A` (`want_quit = (pass == 1u)`); `kl_check_esc`'s mode-4 arm
  (AL = 1) runs `game_key_loop()` under a restart point, asserts
  `landed == 1` (one new site) and `kl_expect_prompt(0x1EFu, 0)` (was `1`).
  Every other check of both is of a store made before `0x24AB0` (the
  `0x24A89` byte, the key consumption, `0x1D270`'s bytes, the question's row
  and column from `0x24A1C..0x24A32`, the frame `0x24A37`, the latch cleared
  by that frame at `0x2EA85`), so none became a "sentinel unchanged" check.
- Mutation T5M1 (the old `DSB(DS_000A81A8) = 1u; return;` in place of the
  jump): `FAIL ...test_game.c:8202: 0 != 1`, `:8204: 1 != 90`, `:10593: 0 !=
  1`, `:10499: 1 != 90` (the last is `kl_expect_prompt`'s quit line).

### Task 6: the host CPU-fault end of the run

- host.c/host.h: `host_cpu_fault(exc, eip, msg, status)` (`_Noreturn`,
  `PORT:`) and `host_set_fault_hook` (`PORT:` test seam). With no hook it
  prints `msg` (or its own line naming the exception and the 8-digit
  address) to stderr, runs `host_shutdown()` and `exit(status)`.
- `test_host`: `hf_check_hook` (4 sites) and `hf_check_exit` (4 sites; a
  forked child's exit status and stderr).
- Mutations: T6M1 (drop the hook call): `run_tests` exits 3 with `unused` on
  stderr (the suite ends there); T6M2 (`exit(1)`): `FAIL
  ...test_platform.c:2406: 1 != 7`; T6M3 (`%08X` -> `%X`): `FAIL
  ...test_platform.c:2408: the NULL-message line names the exception and the
  address`.

### Task 7: G3, the 0x334E0 idiv (ABORT-PINNED)

- svcmenu.c `svc_stats_rows`: the numerator is read first (`0x334CD..0x334D5`,
  as the raw orders it), then `d == 0` calls `host_cpu_fault(0x00, 0x334E0,
  SVC_DE_MSG, SVC_FAULT_EXIT)`; otherwise `v = n / d`. `SVC_DE_MSG` is A's
  verbatim first line (§B.4); `SVC_FAULT_EXIT` is 1 (`PORT:`, errorlevel not
  captured). The old `PORT: ... draws 0` is gone.
- `sm_check_stats_fault` (in `test_svcmenu`; 9 sites): fields 8 = `0xFFFF`,
  6 = 1, `0xB` = 0; sentinel `Z` glyphs at row 0 / row 2 column `0x25` and
  row 3 column 4; the hook catches exactly one fault, `00h` at `0x334E0`; row
  0's number is drawn, row 2's number and row 3's label keep the `Z` sprite.
  The witnesses compare **sprite ids**, not cell values: the text layer can
  hand the same record back to a redraw, so a cell value survived the old
  "draws 0" code (measured: with cell values the no-fault mutant failed only
  the hook checks).
- Mutations: T7M1 (the old `d != 0 ? n / d : 0`): `FAIL
  ...test_game.c:9383: 0 != 1`, `:9384: 90 != 0`, `:9385: 23130 != 210144`,
  `:9387: 16197 != 16239`, `:9388: 16214 != 16239`; T7M2 (exception `0x0E`
  at `0x334E4`): `:9384: 14 != 0`, `:9385: 210148 != 210144`; T7M3 (fault
  also on `d == 0xFFFF`, i.e. on row 0): `:9386: row 0 is drawn before the
  fault`. The three precondition checks (the two field reads and the
  sentinel glyphs) guard the stimulus; they have no mutant of the code under
  test.

### Task 8: G2, the 0xFFE80003 read (SILENT)

- svcmenu.c `svc_test_controls`: the DIAGS arm draws `0xFF` on row 7
  (`text_hex_set(0xE, 7, 0xFF, 8, 0, 0x3000)`) with the `PORT:` of §B.4 (A's
  `diags` frame 111, DOSBox-X's answer; real hardware not captured). No fault
  path; `host_cpu_fault` is not used here.
- TEST B keeps every assertion and adds three `ch_expect` on row 7: `'0'` at
  `0x13`, `'F'` at `0x14` and `0x15` (`000000FF`; `ch_expect` calls, so no
  new `CHECK` site in the count).
- Mutation T8M1 (the old `0u`): `FAIL ...test_game.c:7407: 16197 != 16219`
  twice (`ch_expect`'s sprite line, for the two `'F'` cells).

### The K11 driver: stepping and the fault end (commit `762bdfc`)

- **Found:** the K11 driver (A's `test_k11_oracle`) still stepped
  `game_loop()` by presetting the quit flag `DS_000A81A8`, the brake unit F
  replaced with `game_loop_step()` in the other drivers because the movie
  player's entry test `0x1C75F` reads that flag mid-iteration (record
  named-gaps-f §F.2). With B's restart that matters: after the `menuesc`
  Esc the port skipped the boot logo movies, and `make k11-report` showed
  `menuesc: 283 frames in window: 31 clean, ... 248 unexplained`, the first at
  capture 110 (raw 1964, the TWI5 logo). The driver now calls
  `game_loop_step()`, and installs a fault hook that ends the script at a CPU
  fault (`end settled`, a `fault` line in `k11.log`), where the original's
  run ends.
- Measured with the change (`make k11-report`-equivalent runs, report-only):
  - `menuesc`: `283 frames in window: 184 clean, 92 splice, 3 transition, 0
    unexplained, 4 all-black`, `settled screens exhibited 2/2`: the MAIN MENU
    Esc restart (`0x2520B`) matches the capture frame for frame, the black
    frame and the TWI5 logo included (A §A.6).
  - `idle` with the script's `end` moved from 180 to 1500 ticks (a hand edit,
    so the port runs past the timeout): `window distinct [1..438] (raw
    1374..4509)`, `264 clean, 159 splice, 2 transition, 9 unexplained, 4
    all-black`; the 9 are capture 95..103 (raw 1720..1742), the attract before
    the Enter that the port's dump (which starts at the Enter) does not hold;
    the window starts at 1 because the port's post-restart boot frames also
    match the capture's own first boot. From the Enter through the restart
    (raw 3150) and 19 s of the boot sequence, no capture frame is unexplained.
  - `diags`: `settled screens exhibited 12/12` (A had 11/12, missing the
    DIAGS screen the port drew with `00000000`); the one unexplained frame is
    A's mid-draw capture 110.
  - `de`: `11 frames in window: 7 clean, 0 splice, 0 transition, 0
    unexplained, 4 all-black`, `settled screens exhibited 5/5`, no "capture
    continues" line: the port stops where the original aborts (A had 4/5,
    missing the page-1 screen with row `0x96` drawn as 0).
  - `walk` (the enforced oracle): the five lines are unchanged (window
    `[90..170]`, 38/38, allowed `[138, 151]`, 0 unexplained).

Mutation line numbers in Tasks 2-8 are those measured on the scratch copy of
the tree at that task's stage (Tasks 2-4 were developed together, then 5-8),
so later insertions shift some of them in the committed files; each is
reproducible by applying the mutation to the commit of its task.

## §B.7 Not tested

- **Named gap (review 1): the raw's resource residency and palette slot order
  after a restart.** The port keeps every resolved entry loaded, so the
  restart's attract draws no `- LOADING -` and its palettes sit one slot
  lower than on the first boot (§B.6b, Task 4). The raw's block allocator
  `0x1C308` can free, so whether it re-draws the loader (and keeps the first
  boot's slot order) is not established; the captures are RGB and cannot show
  palette slots. The driver compares RGB.

- The windowed run's view of a fault: the original prints DOS/4GW's text
  over the screen and returns to the DOS prompt; the port prints the first
  line to stderr and exits (`PORT:`). The register dump is not reproduced.
- The G3 errorlevel (not captured) and G2 on real hardware (A's named gap 3).
- The ISR gate on `DS_00104B22` in the master spin (pre-existing,
  todo-verify §1), and the ISR calls `0x1BBAC`/`0x2D62C`.
- The F7 omissions of the resume tail (§B.2 table): `0x2C8F0(-1, 0)`, the
  `0x2BAF4` call at `0x20C58` (the port runs it through `game_state_init`
  only), `0x32970`, `0x5D808`, the `[0x107468]/[0x10746C]` stores and the
  controller checks. (Unit Z, §B.11, ported `0x2C8F0(-1)`, `0x5D808` and the
  two stores and gives the others their reasons.)
- **Corrected (final review, unit Z; raw wins).** An earlier version of this
  bullet said "`0x5D808` is unported, so the port does not stop what the first
  pass started". `0x5D808` is `mov word [0xEF6DE],0; ret` (the ISR's frame
  word; `port/spec/audio.md`'s `0x5d808` row agrees, naming it by its DS offset `0x6f6de`): it has nothing to do with
  the music. What a restart does to music that plays at the jump is
  **unanswered**: the tail's own calls (§B.11) do not stop the sequence (in
  Ghidra's call graph they reach `AIL_stop_sequence` `0x5DEAF` only through
  `0x47370`'s fatal-error path `0x1D290` -> `0x1BE30` -> `0x1D018`), and
  `0x2C8F0(-1)` at `0x20C2B` only re-sets the volumes (`0x1CAB8` -> `0x5DECA`,
  a 500 ms fade when the sequence plays). What the re-entered loop does to it
  (the attract's `0x2C3FC(0x100)`, the movie player) is not traced, and A's
  captures hold no audio.
- The capture's restart frames in `make verify`: the `PR_RESTART` driver
  compares the port's restart with the port's own first boot (state, RNG, RGB
  frames sampled after each iteration, so a movie played inside one
  iteration is not seen); the frame-for-frame comparison with A's captures
  (`menuesc`, and `idle` with a hand-extended script) is report-only
  (`make k11-report` is not in `make verify`, and the stock `idle` port
  script ends 180 ticks after the Enter, before the timeout). **Since unit Z
  (§B.12) the `menuesc` comparison is enforced in `make verify`** (`make
  k11-oracle`); `idle` stays report-only.
- The idle restart from inside a blocking service screen other than the
  menu (the path exists through `config_key_latched`'s callers at svcmenu.c
  `0x32542`, `0x32E36`, `0x3306B`, `0x33247`): only the unit calls and the
  menu-level driver path are run.
- `0x24AB0` end to end (from a game mode's Esc to the attract): the unit
  checks land at an armed point; no driver runs a game into the prompt, and A
  did not capture it.
- The case where a longjmp finds no armed point (`game_restart_longjmp`'s
  `PORT:` exit): only mutation M5 reaches it. The K11 driver leaves
  `game_loop()` by its own `longjmp` at the script's end, which leaves the
  restart point pointing at the dead `game_loop` frame; that driver runs no
  game code afterwards.
- Tasks 2, 3, 5, 6 and 7 were gated with `make verify` only (the oracle
  lines, the K11 walk and the unit/driver runs); the frame dumps and the WAV
  were compared at Tasks 1, 4, 8 and 9 (full gates), which bracket them.

## §B.8 Closure

| Gap | Ledger row | Status | Evidence |
|---|---|---|---|
| G1 (all three `jmp 0x65431`) | §H.3 #1, mode table row `0x27` | **closed** | raw §B.1-§B.3; A §A.6; `test_restart`, `PR_RESTART` driver; report-only: `menuesc` 0 unexplained in 283 frames (§B.6b) |
| G2 (`0xFFE80003`) | §H.3 #2, §E row 32 | **closed (SILENT)**, residue: real hardware | A §A.7; TEST B's row 7 |
| G3 (`0x334E0` `#DE`) | §H.3 #3, §E row 33 | **closed (ABORT-PINNED)**, residue: errorlevel, register dump | A §A.8; `sm_check_stats_fault`, `test_host` |

Counts: assertion sites 13678 -> 13749 before review 1, 13754 after it (review 1: +2 in `rs_check_resume`, +3 in `sm_check_stats_fault_exit`). Before review 1 (+71: Task 1 +11, Task 2 +12, Task 3
+12, Task 4 +17, Task 5 +2, Task 6 +8, Task 7 +9, Task 8 +0 (three
`ch_expect` calls)); the Task 2 conversions and the Task 5 expectation
changes replaced one site with one site. `python3 tools/port_progress.py`:
`769 1203 64` -> `770 1203 64` (the `/* 0x65431` header of
`game_restart_longjmp`, a runtime function, >= `0x5D000`); portable `731 731
100` unchanged; the README's 64% and its "64% of the original's 1203 real
functions" line are unchanged by the one function.

## §B.9 Gates

Every gate is `make verify` with the `nb` overrides (`K11_DUMP=/tmp/pr_nb_k11`
included); "full" adds `dumps.sh nb` + `dumpsha.sh nb` against `base.sha256`
and `make audio-render` + `cmp` against `before-t2.wav`.

| Gate | EXIT | oracle lines | K11 walk | restart driver | dumps | WAV |
|---|---|---|---|---|---|---|
| t0 (full) | 0 | ORACLES-EQUAL | unchanged | — | IDENTICAL | IDENTICAL |
| t1 (full) | 0 | ORACLES-EQUAL | unchanged | — | IDENTICAL | IDENTICAL |
| t2 | 0 | ORACLES-EQUAL | unchanged | — | — | — |
| t3 | 0 | ORACLES-EQUAL | unchanged | — | — | — |
| t4 (full) | 0 | ORACLES-EQUAL | unchanged | all checks passed (1198 iterations) | IDENTICAL | IDENTICAL |
| t5 | 0 | ORACLES-EQUAL | unchanged | all checks passed | — | — |
| t6 | 0 | ORACLES-EQUAL | unchanged | all checks passed | — | — |
| t7 | 0 | ORACLES-EQUAL | unchanged | all checks passed | — | — |
| t8 (full) | 0 | ORACLES-EQUAL | unchanged | all checks passed | IDENTICAL | IDENTICAL |
| t9 (full, after `762bdfc`, the last code commit) | 0 | ORACLES-EQUAL | unchanged | all checks passed (1198 iterations) | IDENTICAL | IDENTICAL |

## §B.10 Review 1 fixes

- **Important:** the palette-slot cause in §B.6b (Task 4) and the driver's
  `rd_frame_hash` comment were wrong; re-measured and rewritten (§B.6b), and
  the raw's post-restart residency/slot order is named in §B.7.
- `sound_buffers_alloc`'s reason corrected (§B.2, §B.6a item 6): a second
  call returns at `0x1D0BF`; the placement is the raw's order.
- `rs_check_resume` now pins that the `0x2F9CC` work is not re-run: it seeds
  `DS_00107410 |= 0x10` (A's diags poke) and `DS_0010740C = 0x5A5A5A5A`
  before `game_init_resume()` and asserts both survive (2 sites). Mutation
  R2M1 (the four `0x2F9CC` lines moved from `game_init` into the tail):
  `FAIL port/tests/test_game.c:12187: 1319061 != 0` (the re-run
  `config_validate` rewrites field `0x29`), `:12188: 55 != 30`, `:12194: 0 !=
  16`, `:12195: 119504 != 1515870810`.
- The `0x42425 call 0x2DAE4(0x10, 1)` comments (flow.c mode-0x12 sub-state
  7, flow.h `game_mode_12_step`) now say what the raw does: a deferred audit
  add, not a longjmp. Comment-only, proved by a Python comment stripper
  (block/line comments removed outside string and char literals, whitespace
  collapsed): both files `COMMENT-ONLY` against their pre-edit copies (the
  stripper reports `CODE-DIFFERS` on a control pair that changes one
  identifier). §B.6a item 9 stays.
- The K11 driver disarms the restart point (`game_restart_arm(NULL)`) after
  leaving `game_loop()` through its own `longjmp`, so `s_restart_point` never
  points into a dead frame.
- The verbatim DOS/4GW line is tested end to end: `sm_check_stats_fault_exit`
  forks a child that runs `svc_stats_rows` on the faulting fields with no
  hook; the child must exit 1 and its stderr must equal `DOS/4GW Professional
  error (2001): exception 00h (divide by zero) at 180:002244E0\n` exactly (3
  sites incl. the pipe). Mutations: R5M1 (last digit `E0` -> `E1` in
  `SVC_DE_MSG`): `FAIL port/tests/test_game.c:9400: the #DE prints A's
  DOS/4GW line verbatim`; R5M2 (`SVC_FAULT_EXIT` 2): `FAIL ...:9399: the #DE
  exits with status 1 (PORT: not captured)`; R5M3 (host.c drops the `\n`):
  `FAIL ...:9400: the #DE prints A's DOS/4GW line verbatim`.
- Gate r1 (full, after `a53b571`): `EXIT=0`, `ORACLES-EQUAL`, the five K11
  walk lines unchanged, restart driver `all checks passed` (1198 iterations),
  `DUMPS-IDENTICAL`, `make audio-render` + `cmp` with `before-t2.wav`:
  `cmp-exit=0 output=[]`.

## §B.11 Final-review follow-ups (unit Z, branch `named-gaps-z`)

Raw source as above (`k11_img.bin`, `k11_dx.py`), plus Ghidra's call graph
`port/decomp/prage.calls.csv` for reachability. Every omission of the §B.2
table was re-disassembled and decided; "first boot" values were measured with
a temporary probe at `game_init_resume`'s entry and exit on `--check 5`
(`A2CB8=7F A2CB4=7F f35=A0 f37=A0 b14D0=64 d107468=0 d10746C=0 wEF6DE=0
b107494=0`; after the tail: record word `+0x24` = `0x64`, devices `0`/`0`).

| Raw | Decision | Evidence |
|---|---|---|
| `0x20C24 mov eax,-1; 0x20C29 xor dl,dl; 0x20C2B call 0x2C8F0` | **ported** (`attract_config_volumes_unscaled()`, first in `game_init_resume`) | `0x2C8F3 cmp eax,-1; jne` takes the unscaled arm `0x2C8F8..0x2C934`, already ported (§42-F). **Not a state no-op on the first boot:** `DS_000A2CB8`/`DS_000A2CB4` go `0x7F` -> `0x50` (fields `0x35`/`0x37` = `0xA0`, halved) where the port used to keep `0x7F` until the attract's `0x110B0 0x2C8F0(-2)`, which for the stock field `0x2A & 3 = 3` stores the same `0x50`. The raw does this on the first pass too, so the port's first boot now follows it. No gated output reads the window between the two (the WAV renders the title bank through the sequencer directly, `test_audio.c` §9, not through `game_init`); gate below |
| `0x20C32 0x29D60` | nothing | bare `ret` |
| `0x20C3D 0x38B70`, `0x20C42 0x2F920`, `0x20C58 0x2BAF4(1)` | **not ported: unobservable** | all three are callees of `0x2BAF4` (Ghidra: `0x2BAF4` calls `0x38B70`, `0x2F920`, `0x4F228`, `0x13ADC`, `0x336C0`, `0x52106`, ...), which `0x10E80` runs again at `0x10E9C` with EAX = 1 (the port's `game_state_init` -> `actors_reset`). Between the two, the tail's calls (`0x2D974`, `0x47370`, `0x1E824`, `0x32968`, `0x2BF00`, `0x32970`, `0x13ADC`, `0x4F1E4`) read no state `0x2BAF4` resets (`hiscore_init` writes the name tables and config fields; `0x2BF00` one byte; `0x13ADC` is itself re-run by `0x2BAF4`), so the post-tail state is the same with or without the first call: an equivalent mutant, like §B.6b's M6 (`0x4F228`) |
| `0x20CD1/0x20CD3 0x32970(0, 0)` | **not ported: host-owned** | `tools/port_classification.txt` `32970 host-owned record-§48-V`. `0x32970` stores `[0x10747C] = [0x105D88]`, adds the elapsed ticks into `[0x107470 + 4k]` for each set bit k of `[0x107494]`, into `[0x107484]` and `[0x107488 + 4*[0x107494]]`, posts `0x2DAE4(3+i, q)` when `[0x107484] >= 0x3840`, and stores `[0x107494] = AL` (`0x32A28`). The port omits every call of it (`game_state_init`'s `0x10E84`, `config_play_time_close`'s `0x32A4A`), and no port code writes `DS_00107494`, `DS_0010747C`, `DS_00107484` or `DS_00107488`, so a restart has nothing of it to re-apply |
| `0x20CEB call 0x5D808` | **ported** (`game_isr_word_reset`, `/* 0x5D808`) | `0x5D808 mov word [0xEF6DE],0; 0x5D811 ret`; one caller (rel32 scan: `0x20CEB` only). The word is what the ISR increments (`0x1BE21`) and `0x2EA78`'s key wait compares (`0x2EAF2..0x2EB03`; config.c models the increment). First boot: `0` -> `0` (measured). It is in the runtime region (>= `0x5D000`) but is a game helper (`port/spec/audio.md`) |
| `0x20CFE..0x20D0A` `[0x107468] = [0x10746C] = word [esp+0x24]` | **ported** (after `config_keys_load`) | `0x1AEE0` packs `+0x24` from the byte `DS_001014D0` (`0x1AF4D..0x1AF52`, AH = 0) and `+0x26` from `DS_001014D2`; the tail copies `+0x24`, the player-1 handicap, into **both** sides' dwords (the HANDICAP screen `0x313CA..0x313D7` stores `v[0]`/`v[1]` separately; the tail does not). `0x394AC` reads `[0x107468 + side*4]` as a percentage (`fighter.c`). **Not a state no-op on the first boot:** `0` -> `0x64` (the image byte), so a first-boot fight in `DS_00104B1F == 3` now scales by 100 % where the port scaled by 0 %; the gate below shows no oracle reaches that arm |
| `0x20D0F..0x20DE3` the controller checks | **named gap** (a `PORT:` in `game_init_resume`) | with DL = DH = 0 on entry (`0x20C29`, `0x20C30`, assuming the calls between keep EDX under WATCOM's convention, as the ones checked do: `0x2C8F0` and `0x1AEE0`/`0x1AF64` push and pop it) a device word `2`/`4`/`6` at `[0x101514]+0x2D4`/`+0x2D6` sets DL/DH (`0x20D23..0x20D9C`, including `0x20D6F`'s `+0x2D4 = 0` when both are joysticks and `+0x2D6 == 2`), then `0x4FBBB(3)`/`0x4FBBB(0xC)` time the game port `0x201` (`0x4FBCA in al,dx`, `0x4FBDC out dx,al`, two `0x2FFFF` loops) and a 0 answer zeroes the device word. On the first boot both words are 0 (measured), so nothing runs; the CONTROLS screen (`0x32345 0x1AE28`, svcmenu.c) can set them. The answer of `0x4FBBB` is the hardware's (`4FBBB host-owned record-§49-V`); the port has no game port, and neither real hardware's nor DOSBox-X's answer is captured, so porting the branch would need a fitted answer |

- **`0x6A8D0`'s driver lock (review MINOR 4).** `0x6A8DE mov eax,[esi];
  0x6A8E0 inc dword [eax+0x14]` on entry, `0x6A8F5 dec` on the early exit and
  `0x6A940..0x6A949` on the normal one: a net-zero counter on the MDI driver.
  Its reader is the driver's timer callback `0x69370`: `0x6937B cmp dword
  [esi+0x14],0; jne 0x69A87`, which skips the service while an API call holds
  the lock (the timer interrupt can fire mid-call). The port's service
  (`seq_tick`) runs only from `game_audio_service` (flow.c) on the game
  thread, never inside `seq_fade_sequence_volume`, and the host audio stream
  has no callback (host.c `SDL_PutAudioStreamData`), so the counter is always 0
  when the service runs: not modelled, with a `PORT:` note in `sequencer.c`.
- **Tests** (`test_game.c` `rs_check_resume_tail`, in `test_restart`; 7 sites):
  fields `0x35 = 0x64`, `0x37 = 0x3C` and a scale of 1 (field `0x2A`), the
  volume globals seeded `0x11`/`0x22`, `DS_001014D0 = 0x37`, `DS_001014D2 =
  0x44`, the handicap dwords and the ISR word seeded; after
  `game_init_resume()`: volumes `0x32`/`0x1E` (the unscaled arm; the scaled one
  would give `0x10`/`0x0A`), both handicaps `0x37`, the word 0. Mutations
  (`PR_ORACLE_REQUIRED=1 run_tests`, restored after each; `test_game.c` lines):

  | # | mutation | measured |
  |---|---|---|
  | Z1 | delete the `0x2C8F0(-1)` call | `FAIL ...:12223: 17 != 50`, `:12224: 34 != 30` |
  | Z2 | delete the `0x5D808` call | `FAIL ...:12227: 4660 != 0` |
  | Z3 | the record's word `+0x26` for `+0x24` | `FAIL ...:12225: 68 != 55`, `:12226: 68 != 55` |
  | Z4 | delete the `0x20D05` store | `FAIL ...:12225: 2863289685 != 55` |
  | Z5 | delete the `0x20D0A` store | `FAIL ...:12226: 1431677610 != 55` |
  | Z6 | `0x5D808`'s body empty | `FAIL ...:12227: 4660 != 0` |
  | Z7 | the scaled arm `0x2C8F0(-2)` instead | `FAIL ...:12223: 16 != 50`, `:12224: 10 != 30` |

  A first version read the fields as the unit process left them: field
  `0x35` was 0 there, so Z7 survived; the test now sets the fields.
- **`tools/k11_compare.py` (review MINOR 5).** It read `PR_ORACLE_REQUIRED`
  from the environment, so a caller exporting `=1` made `k11-oracle` fail
  without the git-ignored capture, where the Makefile comment says the
  target skips. The tool now ignores the variable; `--required` (which no
  Makefile target passes) makes an absent capture fail. New
  `test_inherited_oracle_required_is_ignored` failed before (`1 != 0`, the
  tool printed `FAIL (required)`) and passes after; the existing
  absent-capture test now uses `--required`. `port/tests/test.h` notes that
  `test_restart` must stay last (its resume checks run `game_init_resume()`
  on the unit process).
- **Docs (review IMPORTANT 1, 2; MINOR 3, 4).** `port/spec/game_flow.md`:
  a new "The soft restart" section under the frame loop; ABANDON
  CONQUEST's yes is the restart, not the quit flag; `0x42425` is a plain call
  of the audit add; `rng_seed` and `config_validate` placed in
  `game_init_resume`/`game_init`. §B.7's music sentence corrected (above).
  Ledger §H.1/§H.5 made consistent with §H.3 (none of #1..#10 open; residues
  listed), §H.3 row 1 names the residency/palette-slot gap, rows Z1..Z5 added.
- **Counts.** Assertion sites 13754 -> 13761 (+7). `python3
  tools/port_progress.py`: `770 1203 64` -> `771 1203 64` (`0x5D808`, runtime
  region); portable `731 731 100` unchanged; 432 unported = 81 + 351. `named
  gap` sites (`rg -n -i 'named gap' port/src port/tests`): 11 -> 12 (the
  controller-check `PORT:` in `game_init_resume`).
- **Gates** (`make verify` with the `nz` overrides incl.
  `K11_DUMP=/tmp/pr_nz_k11`, `dumps.sh nz` + `dumpsha.sh nz`, `make
  audio-render AUDIO_WAV=/tmp/pr_nz_fm.wav` + `cmp` with `before-t2.wav`):

  | Gate | tree | EXIT | oracle lines | K11 walk | restart driver | dumps | WAV |
  |---|---|---|---|---|---|---|---|
  | t0 | `6bec424` (base) | 0 | ORACLES-EQUAL | unchanged | 1198 iterations, passed | IDENTICAL | `cmp` exit 0 |
  | t1 | + the tail code and test | 0 | ORACLES-EQUAL | unchanged | 1198 iterations, passed | IDENTICAL | `cmp` exit 0 |
  | t2 | + the `k11_compare` fix, the comment-only commits | 0 | ORACLES-EQUAL | unchanged | 1198 iterations, passed | IDENTICAL | `cmp` exit 0 |
  | t3 | + the enforced `menuesc` oracle (§B.12), the named-gap wording in `game_init_resume` (comment-only, stripper: `COMMENT-ONLY`) | 0 | ORACLES-EQUAL | the walk's five lines unchanged; `menuesc` 0 unexplained in 283, 2/2, END 388 >= 388 | 1198 iterations, passed | IDENTICAL | `cmp` exit 0 |

## §B.12 User decisions and the enforced `menuesc` oracle (unit Z)

Decisions of the user, 2026-09-30, relayed to unit Z by the controller (this
record holds no other evidence of them):

- **Accepted by the user, 2026-09-30:** the windowed behaviour changes that
  follow the raw. ABANDON CONQUEST? Y and the idle timeout / MAIN MENU Esc now
  soft-restart (this record, §B.3); ESC during the boot logos ends the logo,
  skips the next one at its entry (the key stays queued) and then raises QUIT
  TO DOS? Y/N (record named-gaps-f §F.2).
- **Confirmed by the user, 2026-09-30:** G3's abort line stays verbatim
  (`DOS/4GW Professional error (2001): exception 00h (divide by zero) at
  180:002244E0`, exit status 1, with the `PORT:` note that the errorlevel is
  not captured; §B.4).
- **Enforced at the user's request:** the `menuesc` comparison (the MAIN
  MENU Esc, the `0x2520B` longjmp, the restart's black frame and reboot,
  against A's capture `data/k11-captures/menuesc`) moves from `make
  k11-report` into `make k11-oracle`, which `make verify` runs, after the
  walk. It skips silently without the capture, like the walk.

**The `menuesc` oracle's claim (narrow, like the walk's, §A.10).** Its
window START comes from the port's own dump (the first capture frame that
exhibits a port frame at or before the MAIN MENU); its END is the last
capture frame that exhibits the port's final settled screen. Inside the
window every non-black capture frame must be explained byte-exact (clean, a
splice of two adjacent port frames, or one transition row; no frame is
allowed by name), and both settled screens must be exhibited. Measured: window
`[106..388]` (raw 1745..3109), 283 frames: 184 clean, 92 splice, 3
transition, 0 unexplained, 4 all-black, settled screens 2/2. The walk's claim
4 (no content after the END) cannot hold here: the original runs on after the
restart until the capture's 60 s limit (raw 4175) while the port's script
ends `END_TAIL_TICKS` = 180 ticks (a harness value, `k11_session.py`) after
the Esc, so the capture continues at 389 (raw 3190). `k11_compare.py`'s
`K11_OPEN_END` replaces that claim for `menuesc` only with a ratchet: the END
must reach capture frame 388, the value measured here (like the Makefile's
demo-fight N). Without the ratchet the oracle was toothless: with case
`0x27`'s `game_restart_longjmp()` replaced by a no-op the port never
restarts, the window shrinks to `[106..108]` and every other claim still
held (`cmp` exit 0, measured); with the ratchet that mutant fails (`END 108
must be >= 388: FAIL`, exit 1). What it does not prove: anything after raw
3109 (the rest of the reboot), the audio, and a port that under-renders
inside frames it never exhibits (the same limits as the walk).

- Tool tests (`tools/tests/test_k11_compare.py`, 17 tests): the pin for
  `menuesc` (and none for `walk`), an open end passing past the final screen,
  a short window failing, an unexplained frame and a missing screen still
  failing under an open end. Mutations of the tool (each restored): T1 (drop
  the short-window failure) fails `test_open_end_fails_a_short_window`; T2
  (open end for every scenario) fails `test_capture_past_the_final_screen_fails`;
  T3 (an open end skips the unexplained check) fails
  `test_open_end_still_fails_unexplained`.
- **`idle` stays report-only.** Its stock port script (from the capture's poll
  log, `end` = the last key + 180 ticks) ends 180 ticks after the Enter, before
  the `0x4B1`-tick timeout, so the restart is not in the port's dump. No cheap
  faithful extension exists: the hand extension to 1500 ticks (§B.6b, the K11
  driver) left 9 unexplained capture frames (95..103, the attract before the
  Enter, which the dump does not hold), so enforcing it would need a new
  harness rule for the script's end and a by-name allowance of 9 frames, and
  its END would meet the same open-end question with a longer run.
