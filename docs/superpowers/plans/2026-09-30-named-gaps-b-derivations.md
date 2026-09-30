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
  `0x1BEC4`, *before* `0x20C10`; re-running it on a restart would allocate the
  four sample buffers a second time from the bump allocator. It stays in
  `game_init`, moved above the split (first-boot order: it still runs before
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
   plan's Task 1 cut would have re-run it on every restart.
7. G3's `idiv` is reached from STATISTICS page 1 (`0x33058`), not page 2 (A
   §A.8; spec §2's "STATISTICS page 2" and the plan's F8 wording).
8. The instruction A left unpinned for the MAIN MENU Esc clear is `0x3043B`
   (§B.3).

## §B.7 Not tested

(Task 9.)

## §B.8 Closure

(Task 9.)

## §B.9 Gates

(Per task.)
