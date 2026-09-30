# Fault and Control-Flow Gaps (named-gaps sub-project B) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Close named gaps G1 (the `jmp 0x65431` longjmp), G2 (the `0xFFE80003`
read) and G3 (the `0x33458` `idiv` #DE) by modelling what the original does:
G1 as a real C `setjmp`/`longjmp` soft restart (the raw's setjmp site is
single), G2/G3 as the termination DOS/4GW's default exception handler performs
(or the other outcome sub-project A's capture shows), each with a test that can
fail and a mutation proof.

**Architecture:** One restart point (`game_restart_arm` / `game_restart_longjmp`
in `port/src/game/flow.c`) armed by `game_loop()`, the port's single entry to
the master loop `0x255CC`. A longjmp lands there, re-runs the post-setjmp tail of
`0x20C10` (`game_init_resume()`, split out of `game_init()`), re-runs the loop
prologue and continues the loop, exactly as the raw resumes at `0x20C24` and
re-enters `0x255CC`. All three raw longjmp sites (`0x2EBB3`, `0x2520B`,
`0x24AB0`) call it. CPU faults end the run through one host function,
`host_cpu_fault()` in `port/src/host.c` (the only place allowed to print, shut
SDL down and exit), with a hook that tests use to catch the fault.

**Tech Stack:** C11 over flat `mem[]`, `<setjmp.h>`, CMake, the `run_tests`
assertion suite (`CHECK`/`CHECK_EQ_INT`), Python 3 + capstone over the
fixup-applied image mirror for raw checks, DOSBox-X evidence from sub-project A.

**Spec:** `docs/superpowers/specs/2026-09-30-named-gaps-design.md` (§2 G1-G3,
§3.1, §3.4, §4.B, §5, §6). Consumes sub-project A's record
`docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md`. Produces the
derivation record `docs/superpowers/plans/2026-09-30-named-gaps-b-derivations.md`.

## Global Constraints

- **Raw wins.** Every value below carries its address. A value that neither the
  raw nor A's record pins is a named gap with its evidence, never a plausible
  number. Any plan-vs-raw conflict found while implementing: stop, record the
  correction and the address in the B record, then continue from the raw.
- `port/src` comments: only `/* 0xADDR ... */` headers/annotations, `PORT:` and
  `TODO(verify):`. One C function per original function (the split
  `game_init_resume` is the post-setjmp half of `0x20C10`, whose other half is
  already inside `game_init`; it gets a `/* 0x20C24..0x20DE3 */` header).
- SDL, stdio termination and `exit()` for faults live **only** in
  `port/src/host.c` (the new `host_cpu_fault`). Game code calls it.
- Never write `mem[0xA0000]`; never hand-edit `port/src/symbols.h`.
- Tests: only `CHECK`/`CHECK_EQ_INT`; seeded sentinels that differ from every
  post-condition; never assert an unseeded BSS zero; each new assertion gets a
  mutation proof whose expected `FAIL` line is written in the step
  (`<line>` / `...` in those lines stands for the file and line of the
  assertion as written; the values after the colon are exact). Unit tests
  go in `port/tests/test_game.c` (`int test_restart(void)`, registered **last**
  in `TEST_CASES` in `port/tests/test.h`, because a soft restart clobbers the
  game globals the earlier cases seed). The host-fault test goes in
  `port/tests/test_platform.c`'s existing `test_host` (the area that owns
  `host.c`). `game_init()` runs once per process: the end-to-end restart test is
  an env-gated driver (`PR_RESTART`, `TEST_DRIVERS`), run alone.
- **The gate after every code task** (N = the task number):

  ```bash
  K=.superpowers/sdd/2026-09-30-named-gaps-b-faults-longjmp/scratch; mkdir -p $K
  BASE=/Users/felipe.dos.santos/code/mine/primal-rage-reverse/.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt
  make verify SMK_DUMP=/tmp/pr_bN_smk TITLE_DUMP=/tmp/pr_bN_title ATTRACT_DUMP=/tmp/pr_bN_attract \
    FRONTEND_DUMP=/tmp/pr_bN_frontend TITLE_PIN_DIR=/tmp/pr_bN_pin AUDIO_WAV=/tmp/pr_bN_fm.wav \
    2>&1 | tee $K/verify-tN.txt; echo EXIT=${PIPESTATUS[0]}
  grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' \
    $K/verify-tN.txt | diff $BASE - && echo ORACLES-EQUAL
  ```
  Expected: `EXIT=0` and `ORACLES-EQUAL`. If an oracle line moves, stop: do
  not adjust a ratchet or a baseline; record the moved line and report.
- Checks count: `rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l`
  before and after each task; the B record lists every change of the count and
  why (new tests, or an assertion changed because the raw wins).
- `python3 tools/port_progress.py` before and after each code task; if the
  ported count changes, update `README.md`'s title percentage and its "N% of the
  original's D real functions" line in the same task (AGENTS.md).
- Commits: `<area>: <what changed>`, named files only (never `git add -A`),
  message ending with the trailer
  `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Merge/push only
  when the user asks.
- Run binaries from the repo root. No `timeout` on macOS: every loop a test adds
  has an explicit iteration guard.

## Review Focus

Five failure modes, each tested by a named task:

1. **The longjmp lands in the wrong place or with the wrong value**, or with no
   armed restart point (a silent return into the caller, as the port does
   today). Tested by Task 1 (`rs_check_landing`) and Task 3 (`rs_check_case27`).
2. **The restart re-runs the wrong code**: the pre-setjmp `0x2F9CC` work run
   twice, the post-setjmp tail skipped, or the `0x255D4/0x255DA` loop prologue
   not re-run. Tested by Task 1 (`rs_check_resume`) and Task 4 (the `PR_RESTART`
   driver's post-state).
3. **The idle timeout never fires (or fires one tick early)**: `DS_00101500`
   not advancing in the master loop (F4), or `>` weakened to `>=`. Tested by
   Task 2 (`rs_check_idle`, boundary 0x4B0 vs 0x4B1) and Task 4 (the natural
   timeout inside the guard, with the exact-tick checks).
4. **A fault path returns into game code** (drawing continues after the #DE or
   the `0xFFE80003` read) or draws a fabricated value. Tested by Task 7
   (`sm_check_stats_fault`) and Task 8 (the converted TEST B in
   `sm_check_controls`), both asserting that the cells drawn after the faulting
   instruction keep their seeded sentinels.
5. **The quit prompt's `ABANDON CONQUEST` yes still quits to DOS** (the port's
   current conflation of `0x24AB0` with the quit flag) or the soft restart
   changes an oracle. Tested by Task 5 (`ch_check_quit_prompt` pass 2, the
   mode-4 `kl_check_esc` arm) and the gate's `ORACLES-EQUAL` in every task.

## Raw findings so far

Raw source: the fixup-applied image mirror `k11_img.bin` (indexed by linear
address), disassembled with capstone (`k11_dx.py <start> <len>`). Every address
below is linear (= Ghidra).

### F1 (G1). 0x65431 is WATCOM `longjmp`; 0x653FC is `setjmp`
- `0x65431` (`FN_00065431` in `symbols.h`): `push eax; push edx; mov dx,[eax+0x2A];
  mov eax,[eax+0x1C]; call [0xF0900]` (a runtime stack-switch notify hook), then
  `mov ss,[eax+0x2A]; mov esp,[eax+0x1C]; push [eax+0x18]` (the saved return
  address); `or edx,edx; jne; inc edx` (val 0 -> 1); restores `ebx=[eax]`,
  `ecx=[eax+4]`, `esi=[eax+0xC]`, `edi=[eax+0x10]`, `ebp=[eax+0x14]`,
  `es/fs/gs` from `+0x20/+0x26/+0x28` (each `verr`-checked, else 0),
  `edx=[eax+8]`, `ds=[eax+0x22]`; `pop eax` (= val); `ret` (0x6548D). This is
  the WATCOM `longjmp(jmp_buf *eax, int edx)`.
- `0x653FC` (`FN_000653FC`): stores `ebx,ecx,edx,esi,edi,ebp` at `+0..+0x14`,
  pops the return address into `+0x18`, `esp` into `+0x1C`, re-pushes it,
  saves `es,ds,cs,fs,gs,ss` at `+0x20..+0x2A`, `sub eax,eax` (returns 0). WATCOM
  `setjmp`. jmp_buf size 0x2C.
- Case 0x27 (`0x251C6..0x2520B`): `0x50146(eax=0xC000C000, edx=0x1E, ebx=0xF,
  ecx=4)`; `eax = 0x2FFC4(eax=0xBCBDC, edx=0x10, ebx=0xF000)`; if eax is 0, -5
  or -10 -> `0x25210: call 0x4F644; jmp 0x2540F` (the normal case exit);
  otherwise `0x25201: mov edx,1; mov eax,0x1044F4; jmp 0x65431` =
  `longjmp(DS 0x1044F4, 1)`.
- The jmp_buf `0x1044F4` (DS offset `0x844F4`) is referenced as an immediate at
  exactly four places (byte search for `f4 44 10 00` over the image):
  `0x20C1B` (setjmp), `0x24AAC` (`longjmp` at `0x24AB0`), `0x25207` (`longjmp`
  at `0x2520B`, the G1 site), `0x2EBAF` (`longjmp` at `0x2EBB3`).
- **The setjmp site is single.** The only `call`/`jmp` to `0x653FC` in the code
  object is `0x20C1F`, inside `0x20C10` (the init wrapper / `game_init`):
  `push ebx; push edx; sub esp,0x28; call 0x2F9CC; mov eax,0x1044F4;
  call 0x653FC; mov eax,-1; xor dl,dl; call 0x2C8F0; ...`. The setjmp return
  value is **discarded** (`eax` is overwritten at `0x20C24`), so a longjmp
  resumes at `0x20C24` and re-runs the rest of `0x20C10` exactly as the first
  pass did (from the `0x2C8F0(-1, 0)` call on), then returns to `0x20C10`'s
  caller with the frame `0x20C10` had at the setjmp (esp restored).
- Only three `jmp 0x65431` exist (`0x24AB0`, `0x2520B`, `0x2EBB3`), all with
  `eax = 0x1044F4`; no `call 0x65431`.

### F2 (G1). Where the longjmp resumes: a soft restart of 0x20C10
- `0x20C10` is called once, from `0x1C0BD` in game main `0x1BEC4`; after it
  returns, main runs `0x10D34` (CD-ROM restore), `0x1BE30` (teardown), `xor
  eax,eax; ret` (exit code 0).
- `0x20C10`'s body after the setjmp (`0x20C24..0x20DF2`): `0x2C8F0(-1,0)`,
  `0x29D60`, `mov [0x104B1D],dl`, `0x38B70`, `0x2F920`, `0x4F228(0)`,
  `0x2BAF4(eax=1, ebx=0xABCD)` (RNG seed; `mov [0xEF6D8],ebx`), `0x2D974(0x29)`
  -> `[0x104528]` and the derived `[0x105B3A]`, `[0x1088D0]`, `[0x10452C]`,
  `0x47370`, `0x1E824`, `0x32968`, `0x2BF00`, `0x32970(0,0)`, `0x13ADC`,
  `mov word [0x104AFC],bx`, `0x10E80` (game_state_init), `0x5D808`,
  `0x1AEE0(esp)`, `0x1AF64(esp)`, `[0x107468]=[0x10746C]=word [esp+0x24]`, the
  controller-type checks on `[0x101514]+0x2D4/+0x2D6` with `0x4FBBB(3)` /
  `0x4FBBB(0xC)`, then **`0x20DE8: call 0x255CC`** (the master loop), then
  `add esp,0x28; pop edx; pop ebx; ret`.
- So a longjmp to `0x1044F4` is a **soft restart**: execution resumes at
  `0x20C24` with the stack of `0x20C10`'s first activation, re-runs the whole
  post-setjmp init (including the RNG re-seed to `0xABCD` and the CMOS config
  re-read `0x2D974`) and re-enters the master loop `0x255CC` from its entry.
  `0x2F9CC` (before the setjmp) is **not** re-run. The master loop activation
  that executed the longjmp is discarded (its frames are below the restored
  esp). Nothing returns to the longjmp site.
- Consequence for the port: because the setjmp site is single and its return
  value is ignored, the faithful model is a real `setjmp`/`longjmp` pair (spec
  §3.4 "real setjmp/longjmp if single") around the post-setjmp body of the
  port's init wrapper + master-loop entry, or the equivalent explicit loop
  "re-run from 0x20C24" in the frame driver. See the Tasks for which applies
  given the port's frame-stepped (non-blocking) master loop.

### F3 (G1). All three longjmp sites are the same soft restart
- `0x2EB80` (`config_key_latched`, config.c:612): when no key is latched and
  `0x500BB() - DS[0x105F2C] > 0x4B0` (unsigned, `jbe` at `0x2EB9F`), it stores
  `byte [0x107414] = 0` (`0x2EBA8`) then `longjmp(0x1044F4, 1)` (`0x2EBAE..0x2EBB3`).
  **This is the service-menu idle timeout** (0x4B0 = 1200 ticks of the
  `0x500BB` counter; its rate is not re-derived here, see record §49-Y). The
  port keeps the store and returns 0 (`PORT:` at config.c:626).
- `0x2520B` (case 0x27): the menu result not in {0, -5, -10} -> longjmp.
  Port: flow.c:7184 skips `0x4F644` and continues (`PORT:`).
- `0x24AB0` (`0x249F0` quit prompt, `hard_quit` != 0, string `0x1EF`
  = `ABANDON CONQUEST? Y/N`, demo-pose record line ~25402): a yes runs
  `0x1D270`, `0x1B084`, then `longjmp(0x1044F4, 1)`. **Raw-vs-port conflict:**
  the port (flow.c:6451..6476) sets the quit-to-DOS flag `DS_000A81A8 = 1`
  there, so "abandon conquest" ends the process; the raw soft-restarts
  (F2). The raw wins: this site is folded into B's G1 task.
- All three share jmp_buf `0x1044F4` and the single setjmp `0x20C1F`, so one
  port mechanism closes all three.

### F4 (G1). The idle clock DS_00101500 does not advance in the port's master loop
- The timer ISR `0x1BDF4`: unless `byte [0x104B22] == 1` (`0x1BDF8..0x1BE00`,
  the pause/prompt flag), it increments **both** `[0x101508]` and `[0x101500]`
  (`0x1BE02..0x1BE16`), calls `0x1BBAC`, `inc word [0xEF6DE]` (`0x1BE21`),
  calls `0x2D62C`.
- `0x500BB` returns `[0x101500]` (config.c:512 `CFG_TICK_ISR`). The idle test
  in `0x2EB80` is `[0x101500] - [0x105F2C] > 0x4B0`.
- The port models the ISR in two places: `config_screen_wait` (config.c:571..578,
  increments `0x101508`, `0x101500` and the word `0xEF6DE` under the gate) and
  the master loop's `0x256C5` spin (flow.c `game_loop`, increments **only**
  `DS_00101508`). So while the service menu runs through `game_frame` case
  0x27 (one `0x2FFC4` step per master-loop frame), `0x101500` never advances in
  the port and the idle timeout can never fire. The G1 task must add the
  `0x101500` increment (and the `0x104B22` gate) to the master-loop spin, with
  `make verify` proving no oracle line moves (branch in Task 2 if one does).
  The `0xEF6DE` word and the ISR calls `0x1BBAC`/`0x2D62C` are **not** added by
  B (not needed by G1; flow.c:6964 already treats `0xEF6DE` separately);
  they stay as they are.

### F5 (G2). The 0xFFE80003 read and the diagnostic flag
- Site: `0x32573 mov ebx,0xFFE80003; 0x32578 mov bl,[ebx]` in TEST CONTROLS
  `0x32358` (`svc_test_controls`, svcmenu.c:992), inside `if (diag)`.
- `diag` = `DS_00107410 & 0x10` (`0x32361..0x32367`, `mov ebp,[0x107410]; and
  ebp,0x10`). `DS_00107410 = 0x2D974(0x2A) & ~3` (`0x2FA10..0x2FA1C`, inside
  `0x2F9CC`, before the setjmp). So the arm needs **config field 0x2A bit 4**.
- The only writers of field 0x2A in the code object (every `call 0x2DA0C`, 20
  callers, preceded by `mov eax,0x2A`): `0x2CB2C..0x2CB4B` (`(old & ~3) | 3`)
  and `0x30E8E..0x30EA3` (ADJUST VOLUME: `(old & ~3) | voice`). **Neither sets
  bit 4**; bit 4 survives from the persisted config only (the file/CMOS store
  `0x2D974` reads; the writer `0x1B084` is deferred in the port). So in the raw
  the arm is reachable only with a persisted config whose field 0x2A has bit 4
  set; no key sequence in the game sets it.
- Exception handlers the program installs (search of the code object
  `0x10000..0x74000`): `int 21h AX=2508h` at `0x66526` (IRQ0 timer) and its
  restore at `0x6657B`; DPMI `0205h` (set PM interrupt vector) at `0x66840` /
  `0x66875` for an IRQ (`bl<8 ? bl+8 : bl+0x68`); `0x729D4 -> 0x732A6`
  (int 21h `25h`/Phar-Lap `2504h`) for **vector 7** (the FPU emulator, after
  the DOS/4G vendor API `int 31h AX=0A00h` at `0x729BF`). There is **no DPMI
  `0203h`** (set exception handler, searched as `66 b8 03 02` and `b8 03 02 00
  00`), no `int 21h AX=2500h` (vector 0) and no hook of vector `0Eh` / `0Dh`.
  So a `#PF`/`#GP` at `0x32578`, or a `#DE` at the G3 `idiv`, is handled by
  **DOS/4GW's default exception handler**, not by game code.
- DOS/4GW's flat DS has base 0 and limit 4 GB, so `0xFFE80003` passes the
  segment check; whether it faults depends on DOS/4GW's page tables (unmapped
  high linear space -> `#PF` exception 0Eh). The raw cannot show DOS/4GW's
  page tables or its default handler's output: **A's capture decides** (see
  Task 3's branch table).

### F6 (G3). The 0x33458 idiv
- `0x33458` (`svc_stats_rows`, svcmenu.c:1177): per row, `v = 0x2D974(f1)`
  (+ `0x2D974(f2)` when `f2 != 0`); `0x334C9 test eax,eax; je 0x334E4`; else
  `0x334CD..0x334E0`: `eax = 0x2D974(num); edx = eax; ebx &= 0xFFFF;
  sar edx,31; idiv ebx` (`0x334E0`); `0x334E2 mov ebx,eax`.
- The divisor is `v & 0xFFFF` in `0..0xFFFF` and the dividend is a
  sign-extended 32-bit value, so the only fault is `ebx == 0` (**#DE,
  exception 00h, faulting EIP `0x334E0`**); quotient overflow is impossible
  (|quotient| <= |dividend| < 2^31 for a divisor >= 1). The fault condition is
  `v != 0 && (v & 0xFFFF) == 0`.
- No game or runtime code hooks vector 0 (F5), so the #DE goes to DOS/4GW's
  default handler. What that handler prints/does on this build (register dump
  and exit to DOS, or reflection to real-mode int 0 "Divide overflow") is not
  in the image: **A's capture decides**.
- Rows drawn before the faulting row are on screen; the faulting row's
  number (`0x334E4..`) and all later rows are never drawn in the raw.

### F7 (G1). The post-setjmp tail against the port's game_init
- `0x107414` is both the idle-timeout store of `0x2EBA8` and `menu.c`'s
  `MENU_ACTIVE` byte (`menu.c:16`). So the timeout marks the menu
  uninitialised before it restarts; the restarted run re-initialises any menu
  it enters (`0x2FFCD..0x2FFD4`).
- Two zero stores of the tail that the port's `game_init` omits, both
  restart-relevant (the menus set them): `0x20C29 xor dl,dl` ... `0x20C37 mov
  [0x104B1D],dl` = **`DS_00104B1D = 0`** (`0x2C8F0` pushes/pops EDX at
  `0x2C8F2`; `0x29D60` is a bare `ret`), and `0x20CD3 xor ebx,ebx` ...
  `0x20CDF mov [0x104AFC],bx` = **`DS_00104AFC = 0`** (`0x32970` and `0x13ADC`
  both open with `push ebx`). The image bytes at both addresses are 0, so on the
  first boot the stores are no-ops (oracle-neutral); on a restart they matter.
- The tail's other calls and their port status (the B record copies this table;
  B does not add the missing ones, which predate B and would move the first-boot
  state the oracles pin):

  | Raw (post-setjmp) | Port | Status |
  |---|---|---|
  | `0x20C2B 0x2C8F0(-1, dl=0)` | `attract_config_volumes_unscaled()` exists (attract.c:175) | not called by `game_init`: pre-existing omission, named in the B record |
  | `0x20C32 0x29D60` | bare `ret` | nothing to do |
  | `0x20C3D 0x38B70`, `0x20C42 0x2F920` | `actor_cursor_reset()`, `mem_fill(DS_00105F38,0,0x14D4)` | run inside `actors_reset()` (actors.c:731..732), which `game_state_init` calls at `0x10E9C` |
  | `0x20C49 0x4F228(0)` | `render_projection_reset(0u)` | in `game_init` |
  | `0x20C58 0x2BAF4(1)` + `0x20C62 [0xEF6D8]=0xABCD` | `rng_seed(0xABCDu)`; `actors_reset()` via `game_state_init` | seed in `game_init`; the `0x2BAF4` call itself is not repeated (pre-existing) |
  | `0x20C68 0x2D974(0x29)` .. `0x20CC2` | `config_field_get(0x29u)` and the three derived stores | in `game_init` |
  | `0x20C7F 0x47370` | `game_string_table_load(s_game_dir)` (idempotent: `s_string_table_loaded`) | in `game_init`, later in the order (pre-existing) |
  | `0x20C84 0x1E824` | `hiscore_init()` | in `game_init` |
  | `0x20CC7 0x32968` | `game_init_null()` | in `game_init` |
  | `0x20CCC 0x2BF00` | `config_set_credit_row_init()` | in `game_init` |
  | `0x20CD5 0x32970(0,0)` | none (`PORT:` run clock out of scope, attract.c:88) | pre-existing |
  | `0x20CDA 0x13ADC` | `effects_init()` inside `actors_init`/`actors_reset` | pre-existing placement |
  | `0x20CE6 0x10E80` | `game_state_init()` | in `game_init` |
  | `0x20CEB 0x5D808` | none (runtime, >= `0x5D000`) | pre-existing |
  | `0x20CF2 0x1AEE0`, `0x20CF9 0x1AF64` | `config_keys_pack`/`config_keys_load` | in `game_init` |
  | `0x20D05/0x20D0A [0x107468]=[0x10746C]=word [esp+0x24]` | none | pre-existing omission, named in the B record |
  | `0x20D0F..0x20DE3` controller checks, `0x4FBBB(3)`, `0x4FBBB(0xC)` | none (`PORT: 0x5004A joystick init`) | pre-existing. **Conflict:** `tools/port_classification.txt:70` says `4FBBB` is "called only by the ISR sampler 0x1BBAC"; the raw also calls it at `0x20DAC` and `0x20DD1`. Record the correction; do not edit the classification line in B beyond appending the two call sites to its evidence text. |

- The pre-setjmp work in the port's `game_init` that must **not** re-run:
  the image map, the `0x1BEC4` chain (`int10h_query`, `GAME_BIOS_BASE`,
  `game_audio_init`, `res_load_index`, `actors_init`, `surface_setup`,
  `palette_list_init`, `render_list_init`) and the `0x2F9CC` work
  (`DSD(DS_0010740C) = 0x1D2D0`, `config_validate()`,
  `DSD(DS_00107410) = config_field_get(0x2A) & ~3`, `svcmenu_register()`),
  which the port currently runs *after* `render_projection_reset`/`rng_seed`.
  In the raw `0x2F9CC` runs at `0x20C15`, before the setjmp, so Task 1 moves
  those four lines above the resume split (an order change on the first boot;
  the gate proves it oracle-neutral).

### F8. What the raw leaves to A's capture (B consumes A's record)
- **G1:** nothing is left open for the mechanism (F1-F3 decide it). A's capture
  corroborates the timing and target: in the service menu with no input, the
  menu is replaced by the attract (state 0, mode 3) at the first `0x2EB80` call
  where `[0x101500] - [0x105F2C] > 0x4B0`, and a dump taken after the switch
  shows word `0x104B00 = 3`, word `0xF0A64 = 0` (or the attract's next states),
  byte `0x107414 = 0`, and the tick pair `0x101508`/`0x10150C` restarted from 0.
  If A's record contradicts that (for example the game quits to DOS), stop and
  report: the raw reading F1-F3 is then wrong somewhere.
- **G2:** with a persisted config whose field `0x2A` has bit 4 set (A has to
  seed the config store; no key sequence sets it, F5), entering TEST CONTROLS:
  (a) the screen and console after row 6's pad word is drawn: the DOS/4GW
  message text verbatim (exception number, CS:EIP), whether the DOS prompt
  returns, and the errorlevel if A captures it; or (b) the RAW DATA row (row 7,
  column 0xE) showing a byte value, and the game continuing; or (c) A records
  the path as not reached.
- **G3:** with the statistics fields seeded so STATISTICS page 2 row 2's sum is
  `0x10000` (field 8 = `0xFFFF`, field 6 = `1`; rows at `0x326C4`: row 0
  `{id 0x94, num 0xA, f1 8, f2 0}`, row 1 `{0x95, 0xC, 0xB, 0}`, row 2
  `{0x96, 0x12, 8, 6}`, row 3 `{0x97, 0x13, 9, 7}`): the same observables as
  G2 (a)/(b)/(c), with exception 00h at the runtime address of `0x334E0`.
- The branch taken for G2 and G3 is selected in Task 0 and fixed for Tasks 6-8:

  | A's record shows | Branch | Port behaviour | Record wording |
  |---|---|---|---|
  | DOS/4GW default-handler message + return to DOS | **ABORT-PINNED** | `host_cpu_fault(exc, eip, <A's verbatim first line>, <A's errorlevel, or 1 with PORT if not captured>)` at the faulting instruction; nothing after it runs | gap closed with A's evidence; the port prints to stderr where the original prints over mode 13h (`PORT:`) |
  | the game continues and draws a value | **SILENT** | no fault; draw exactly what A's frame shows (G2: `text_hex_set(0xE, 7, <byte>, 8, 0u, 0x3000u)`; G3: the digits A's frame shows for row 2) | closed with A's frame number and bytes; `PORT:` naming DOS/4GW/DOSBox-X as the source of the value |
  | not reached | **ABORT-RAW** | `host_cpu_fault(exc, eip, NULL, 1)`: the host prints its own line naming the exception and address. G3 uses exc `0x00` (the CPU raises #DE on a zero divisor, F6); G2 uses exc `0x0E` with `TODO(verify): the page fault at 0xFFE80003 is inferred (no handler, F5; no memory behind the address, record K11 §K11.5), not captured` | restated as a smaller named gap: the message text and exit status (G3), plus the fault itself (G2) |

---

### Task 0: Baseline, the B record, and A's branch selection

**Files:**
- Create: `docs/superpowers/plans/2026-09-30-named-gaps-b-derivations.md`
- Read: `docs/superpowers/plans/2026-09-30-named-gaps-a-derivations.md` (A's record)

**Interfaces:** Consumes F1-F8 above and A's record. Produces the B record
(§B.1 longjmp/setjmp, §B.2 the resume tail and F7 table, §B.3 the three longjmp
sites, §B.4 exception handlers and the G2/G3 branch, §B.5 the idle clock, §B.6
evidence consumed from A, §B.7 Not tested, §B.8 closure) and
`$K/branch.txt` holding two lines `G2=<branch>` and `G3=<branch>`.

- [ ] **Step 1: Baseline.**

  ```bash
  K=.superpowers/sdd/2026-09-30-named-gaps-b-faults-longjmp/scratch; mkdir -p $K
  make build && make verify SMK_DUMP=/tmp/pr_b0_smk TITLE_DUMP=/tmp/pr_b0_title ATTRACT_DUMP=/tmp/pr_b0_attract \
    FRONTEND_DUMP=/tmp/pr_b0_frontend TITLE_PIN_DIR=/tmp/pr_b0_pin AUDIO_WAV=/tmp/pr_b0_fm.wav \
    2>&1 | tee $K/verify-t0.txt; echo EXIT=${PIPESTATUS[0]}
  grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $K/verify-t0.txt \
    | diff /Users/felipe.dos.santos/code/mine/primal-rage-reverse/.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt - && echo ORACLES-EQUAL
  rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l > $K/checks-base.txt
  python3 tools/port_progress.py | tee $K/progress-base.txt
  ```
  Expected: `EXIT=0`, `ORACLES-EQUAL`, progress first line `765 1203 64`
  (if it differs, the tree moved since this plan: record the new numbers and use
  them as the baseline).

- [ ] **Step 2: Re-verify the raw facts this plan builds on** (each is one short
  capstone run over a fixup-applied image; rebuild the image as
  `mem_load_le` + `mem_load_le_fixups` do if the scratchpad mirror
  `k11_img.bin` is gone, never from raw file offsets):
  `0x65431` (longjmp), `0x653FC` (setjmp), the single caller `0x20C1F` of
  `0x653FC`, the three `jmp 0x65431` (`0x24AB0`, `0x2520B`, `0x2EBB3`) with
  `eax = 0x1044F4`, `0x20DE8 call 0x255CC`, `0x1C0BD call 0x20C10`, the ISR
  `0x1BDF4..0x1BE16`, the idiv `0x334CD..0x334E2`, the read
  `0x32573..0x32596`, and the absence of DPMI `0203h`. Expected: each matches
  F1-F7. Any mismatch: stop and report (raw wins over this plan).

- [ ] **Step 3: Read A's record and select the branches.** Find its entries for
  `0xFFE80003`, `idiv`/`#DE`/`0x334E0` and the idle timeout. Apply F8's table;
  write `$K/branch.txt` (`G2=ABORT-PINNED|SILENT|ABORT-RAW`,
  `G3=...`). For ABORT-PINNED also copy into the B record: the verbatim first
  message line, the exception number, the EIP A saw and its relation to the
  Ghidra address (runtime relocation delta), the errorlevel or "not captured".
  For SILENT copy the frame number and the drawn bytes. If A's G1 entry
  contradicts F8's G1 expectation, stop and report.

- [ ] **Step 4: Write the B record** with §B.1-§B.8 as listed under
  Interfaces, copying F1-F8 with their addresses, the F7 table, and these
  corrections: (1) `0x24AB0` is a soft restart, not the quit-to-DOS the port
  models (flow.c `game_quit_prompt`); (2) the `0x4FBBB` classification line
  omits `0x20DAC`/`0x20DD1`; (3) the port's master loop does not advance
  `DS_00101500` (F4); (4) the spec's G1 row names only `0x2520B`, the idle
  timeout proper is `0x2EBB3` in `0x2EB80`; (5) the spec's citation "K7+K12
  record §2.6 (idle-timeout store)" does not resolve in
  `2026-09-29-k7-k12-derivations.md` (`rg -n 'idle|0x107414|0x2EBA8'` finds
  nothing); the store is recorded in config.c's `0x2EB80` header (records
  §49-X/§49-Y).

- [ ] **Step 5: Commit.**
  `git add docs/superpowers/plans/2026-09-30-named-gaps-b-derivations.md`;
  message `docs: named-gaps B record (longjmp, faults; A's branches)` + trailer.

---

### Task 1: The restart point and the resume tail

**Files:**
- Modify: `port/src/game/flow.h` (declarations), `port/src/game/flow.c`
  (`game_init` split, `game_init_resume`, `game_restart_arm`,
  `game_restart_longjmp`, `game_loop` landing)
- Modify: `port/tests/test_game.c` (new `int test_restart(void)` with
  `rs_check_landing`, `rs_check_resume`), `port/tests/test.h` (append
  `X(test_restart)` as the **last** `TEST_CASES` line, after `X(test_key_loop)`)

**Interfaces:**
- Consumes: F1, F2, F7.
- Produces (flow.h):
  ```c
  #include <setjmp.h>
  /* 0x65431 — record B §B.1. WATCOM longjmp(0x1044F4, 1): the soft restart.
   * Lands at the armed restart point (game_loop's, 0x20C1F's setjmp in the
   * raw); never returns. */
  _Noreturn void game_restart_longjmp(void);
  /* PORT: arms `jb` as the restart point and returns the previous one (NULL
   * when none). The raw's jmp_buf is the 0x2C-byte register save at DS
   * 0x1044F4 (0x653FC), which only 0x65431 reads. */
  jmp_buf *game_restart_arm(jmp_buf *jb);
  /* 0x20C24..0x20DE3 — record B §B.2. The post-setjmp tail of 0x20C10. */
  void game_init_resume(void);
  ```

- [ ] **Step 1: Write the failing tests** in `port/tests/test_game.c` (at the
  end of the file, with `#include <setjmp.h>` added to its includes if absent):

  ```c
  /* ---- named-gaps B: the 0x65431 soft restart (record B) ---------------- */

  static jmp_buf rs_jb;

  /* 0x65431 lands at the armed point with EAX = 1 (0x6544B..0x65450: val 0
   * becomes 1; the callers pass EDX = 1) and never returns to its caller. */
  static void rs_check_landing(void)
  {
      volatile int landed = -1, returned = 0;
      jmp_buf *const prev = game_restart_arm(&rs_jb);
      switch (setjmp(rs_jb)) {
      case 0:
          landed = 0;
          game_restart_longjmp();
          returned = 1;
          break;
      case 1:
          landed = 1;
          break;
      default:
          landed = 2;
          break;
      }
      CHECK_EQ_INT(landed, 1);
      CHECK_EQ_INT(returned, 0);
      CHECK(game_restart_arm(prev) == &rs_jb, "the armed point is handed back");
  }

  /* 0x20C24..0x20DE3: the resume tail's stores, each seeded to differ. */
  static void rs_check_resume(void)
  {
      const u32 v = config_field_get(0x29u);
      DSD(DS_000EF6D8) = 0x1234u;                 /* 0x20C62 seed */
      DSD(DS_00104528) = v ^ 0xA5A5A5A5u;         /* 0x20C6D */
      DSD(DS_001088D0) = 0xDEADu;                 /* 0x20CB0 */
      DSB(DS_00104B1D) = 0xA5u;                   /* 0x20C37 */
      DSW(DS_00104AFC) = 0x77u;                   /* 0x20CDF */
      DSB(DS_00107A54) = 1u;                      /* 0x4F228 */
      DSW(DS_00104B00) = 0x27u;                   /* 0x10EA1 (game_state_init) */
      DSW(DS_000F0A64) = 0x33u;                   /* 0x10EA8 */
      game_init_resume();
      CHECK_EQ_INT((long)DSD(DS_000EF6D8), 0xABCD);
      CHECK_EQ_INT((long)DSD(DS_00104528), (long)v);
      CHECK_EQ_INT((long)DSD(DS_001088D0), (long)((v & 0xFu) * 5u + 0x1Eu));
      CHECK_EQ_INT((int)DSB(DS_00104B1D), 0);
      CHECK_EQ_INT((int)DSW(DS_00104AFC), 0);
      CHECK_EQ_INT((int)DSB(DS_00107A54), 0);
      CHECK_EQ_INT((int)DSW(DS_00104B00), 3);
      CHECK_EQ_INT((int)DSW(DS_000F0A64), 0);
  }

  int test_restart(void)
  {
      int before = g_failures;
      rs_check_landing();
      rs_check_resume();
      return g_failures - before;
  }
  ```
  If `DS_00104AFC`, `DS_00104B1D`, `DS_00107A54`, `DS_001088D0`,
  `DS_00104528`, `DS_000EF6D8` are not all emitted by `symbols.h`, use a local
  `#define` with the raw address in the test file (never edit `symbols.h`).
  Match the file's existing `int test_X(void)` return convention (read the
  nearest `int test_...` and copy its `before`/return idiom exactly).

- [ ] **Step 2: Run and see it fail to build** (the symbols do not exist yet):
  `cmake --build build 2>&1 | grep -E 'error' | head -5`. Expected: undefined
  `game_restart_arm` / `game_restart_longjmp` / `game_init_resume`.

- [ ] **Step 3: Implement in flow.c.**
  - Add `#include <setjmp.h>` and, near `game_fatal`:
    ```c
    /* PORT: host control state, not original state: the raw keeps the
     * registers of 0x20C10's frame in the jmp_buf at DS 0x1044F4 (record B
     * §B.1), which only 0x65431 reads. */
    static jmp_buf *s_restart_point;

    jmp_buf *game_restart_arm(jmp_buf *jb)
    {
        jmp_buf *prev = s_restart_point;
        s_restart_point = jb;
        return prev;
    }

    /* 0x65431 — record B §B.1. WATCOM longjmp(0x1044F4, 1): restores the
     * registers and ESP saved by 0x653FC at 0x20C1F and returns there with
     * EAX = 1 (0x6544B: 0 becomes 1). Callers: 0x24AB0, 0x2520B, 0x2EBB3. */
    _Noreturn void game_restart_longjmp(void)
    {
        if (s_restart_point != NULL)
            longjmp(*s_restart_point, 1);                /* 0x65442..0x6548D */
        /* PORT: the raw's setjmp (0x20C1F) always runs before any caller can;
         * reaching here means a port path ran game code outside game_loop()
         * with no restart point armed, a port defect. */
        fprintf(stderr, "Primal Rage: restart longjmp with no restart point\n");
        exit(1);
    }
    ```
  - Split `game_init`: move the four `0x2F9CC` lines (`DSD(DS_0010740C) =
    0x0001D2D0u;`, `config_validate();`, `DSD(DS_00107410) = ...;`,
    `svcmenu_register();`) up to just after `render_list_init();` (they run at
    `0x20C15`, before the setjmp; keep their comments and fix the PORT comment
    block that described them as "before this block"). Then cut everything from
    `render_projection_reset(0u);` to the end of `game_init` (through
    `config_keys_load(...)`) into:
    ```c
    /* 0x20C24..0x20DE3 — record B §B.2. The post-setjmp tail of 0x20C10: the
     * first pass runs it from game_init, a 0x65431 longjmp re-runs it from
     * game_loop's restart point. The tail's calls the port omits are listed
     * in record B §B.2 (pre-existing). */
    void game_init_resume(void)
    {
        DSB(DS_00104B1D) = 0u;             /* 0x20C29 xor dl,dl; 0x20C37 (0x2C8F0 keeps EDX) */
        render_projection_reset(0u);       /* 0x20C47 `xor eax,eax`, 0x20C49 0x4F228 */
        ... the moved block, unchanged, in its current order ...
    }
    ```
    and insert `DSW(DS_00104AFC) = 0u;  /* 0x20CD3 xor ebx,ebx; 0x20CDF (0x32970, 0x13ADC keep EBX) */`
    immediately after `game_init_null();` (`0x20CC7`) and
    `config_set_credit_row_init();` (`0x20CCC`), i.e. before
    `game_string_table_load` / `game_state_init` (`0x20CE6`). `game_init` ends
    with `game_init_resume();`. The early `return`s after `game_fatal` stay in
    `game_init` (they are all before the split).
  - Land in `game_loop`:
    ```c
    void game_loop(void)
    {
        jmp_buf restart;
        jmp_buf *const prev = game_restart_arm(&restart);
        if (setjmp(restart) != 0) {
            /* 0x20C24: 0x65431 resumes after 0x20C1F's setjmp, whose result
             * is discarded (0x20C24 mov eax,-1), re-runs the tail and
             * re-enters 0x255CC at 0x20DE8. PORT: the landing is here, the
             * port's one entry to 0x255CC, because the drivers step
             * game_loop() one frame per call; the re-run is the raw's. */
            game_init_resume();            /* 0x20C24..0x20DE3 */
            game_loop_begin();             /* 0x20DE8 0x255CC: 0x255D4/0x255DA */
        }
        ... the existing PORT comment and do { ... } while (...); unchanged ...
        (void)game_restart_arm(prev);
    }
    ```
  - Update the file-top comment line `0x20C10 init wrapper` to mention the
    split. Add the three declarations to flow.h.

- [ ] **Step 4: Run the tests.**
  `cmake --build build && PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -3`.
  Expected: `all checks passed`.

- [ ] **Step 5: Mutation proofs** (apply, rebuild, run, restore; record each
  FAIL line in the B record §B.7's mutation table):
  1. `longjmp(*s_restart_point, 1)` -> `longjmp(*s_restart_point, 2)`:
     `FAIL port/tests/test_game.c:<line>: 2 != 1`.
  2. Delete `DSB(DS_00104B1D) = 0u;` in `game_init_resume`:
     `FAIL ...: 165 != 0`.
  3. Delete `DSW(DS_00104AFC) = 0u;`: `FAIL ...: 119 != 0`.
  4. Delete `game_state_init();` from `game_init_resume`:
     `FAIL ...: 39 != 3` and `FAIL ...: 51 != 0`.
  5. In `game_restart_arm`, `s_restart_point = jb;` -> `s_restart_point = prev;`
     (arming ignored): the longjmp takes the no-point path and `run_tests`
     exits 1 with `Primal Rage: restart longjmp with no restart point` on
     stderr; record that output as the proof (an exit ends the suite, so this
     proof is the process status, not a FAIL line).

- [ ] **Step 6: Gate** (N = 1). Expected `EXIT=0`, `ORACLES-EQUAL` (the
  `0x2F9CC` reorder and the two zero stores are first-boot no-ops by F7).
  If an oracle line moves, bisect by reverting the reorder first, then each
  store, and record which one moved it.

- [ ] **Step 7: Commit.** `git add port/src/game/flow.c port/src/game/flow.h
  port/tests/test_game.c port/tests/test.h`; message
  `game: model the 0x65431 soft restart point and split 0x20C10's resume tail` + trailer.

---

### Task 2: The idle timeout 0x2EBB3 and the idle clock

**Files:**
- Modify: `port/src/game/config.c` (`config_key_latched`), `port/src/game/config.h:150-151` (comment)
- Modify: `port/src/game/flow.c` (`game_loop`'s `0x256C5` spin)
- Modify: `port/tests/test_game.c` (`rs_check_idle`, called from `test_restart`)

**Interfaces:** Consumes Task 1's `game_restart_longjmp`/`game_restart_arm`,
F3, F4. Produces the idle path used by Task 4.

- [ ] **Step 1: Write the failing test.**

  ```c
  /* 0x2EB80: no latch and 0x500BB - DS_00105F2C > 0x4B0 (unsigned, 0x2EB9F
   * `jbe`) stores DS_00107414 = 0 (0x2EBA8) and longjmps (0x2EBB3). */
  static void rs_check_idle(void)
  {
      const u32 s_latch = DSD(0x00105F30u), s_tick = DSD(DS_00101500),
                s_time = DSD(0x00105F2Cu);
      const u8 s_flag = DSB(0x00107414u);
      jmp_buf *const prev = game_restart_arm(&rs_jb);
      volatile int landed = 0;
      volatile u32 got = 0xFEEDu;

      /* at the boundary: 0x4B0 is not over it */
      DSD(0x00105F30u) = 0u; DSD(DS_00101500) = 0x2000u;
      DSD(0x00105F2Cu) = 0x2000u - 0x4B0u; DSB(0x00107414u) = 0x5Au;
      if (setjmp(rs_jb) == 0) got = config_key_latched(); else landed = 1;
      CHECK_EQ_INT(landed, 0);
      CHECK_EQ_INT((long)got, 0);
      CHECK_EQ_INT((int)DSB(0x00107414u), 0x5A);

      /* one tick over: the store, then the restart */
      landed = 0; got = 0xFEEDu;
      DSD(0x00105F2Cu) = 0x2000u - 0x4B1u; DSB(0x00107414u) = 0x5Au;
      if (setjmp(rs_jb) == 0) got = config_key_latched(); else landed = 1;
      CHECK_EQ_INT(landed, 1);
      CHECK_EQ_INT((long)got, 0xFEED);
      CHECK_EQ_INT((int)DSB(0x00107414u), 0);

      /* a latched key wins over any idle time (0x2EB87) */
      landed = 0; got = 0xFEEDu;
      DSD(0x00105F30u) = 0x41u; DSB(0x00107414u) = 0x5Au;
      if (setjmp(rs_jb) == 0) got = config_key_latched(); else landed = 1;
      CHECK_EQ_INT(landed, 0);
      CHECK_EQ_INT((long)got, 0x41);
      CHECK_EQ_INT((int)DSB(0x00107414u), 0x5A);

      (void)game_restart_arm(prev);
      DSD(0x00105F30u) = s_latch; DSD(DS_00101500) = s_tick;
      DSD(0x00105F2Cu) = s_time; DSB(0x00107414u) = s_flag;
  }
  ```
  Add `rs_check_idle();` to `test_restart` after `rs_check_landing();` (before
  `rs_check_resume`, which clobbers game state). Use the `symbols.h` names
  (`DS_00105F30`, `DS_00105F2C`, `DS_00107414`) where the generator emits them;
  the literals above are the same addresses (config.c `CFG_KEY_LATCH`,
  `CFG_KEY_TIME`, `CFG_IDLE_FLAG`).

- [ ] **Step 2: Run; expect FAIL** on the one-tick-over block:
  `FAIL ...: 0 != 1` (landed) and `FAIL ...: 0 != 65261` (got; the port
  returns 0 today).

- [ ] **Step 3: Implement.** In `config_key_latched` replace the `PORT:`
  comment line with the call, and rewrite the header's last sentences:
  ```c
      if (DSD(CFG_TICK_ISR) - DSD(CFG_KEY_TIME) > 0x4B0u) {   /* 0x2EB94..0x2EB9F */
          DSB(CFG_IDLE_FLAG) = 0u;                            /* 0x2EBA1 xor ah,ah; 0x2EBA8 */
          game_restart_longjmp();                             /* 0x2EBA3/0x2EBAE..0x2EBB3 jmp 0x65431, longjmp(0x1044F4, 1) */
      }
  ```
  Header: "over, the idle timeout stores DS_00107414 = 0 (0x2EBA8, the menu's
  active byte) and soft-restarts through longjmp(0x1044F4, 1) (0x2EBAE..0x2EBB3,
  game_restart_longjmp, record B §B.3)." Update `config.h:150-151` the same way
  (drop "is not modelled"). In `game_loop`'s spin add the second ISR counter:
  ```c
          while (DSD(DS_0010150C) - 1u == DSD(DS_00101508)) {  /* 0x256C5 */
              DSD(DS_00101508)++;                               /* 0x1BE02..0x1BE10 */
              DSD(DS_00101500)++;                               /* 0x1BE08..0x1BE16: the 0x500BB clock (record B §B.5) */
              host_wait_vblank();
          }
  ```
  and extend the spin's existing `PORT:` comment with: "The ISR's gate on
  DS_00104B22 (0x1BDF8..0x1BE00), its calls 0x1BBAC/0x2D62C and the 0xEF6DE word
  are not modelled here (pre-existing; config_screen_wait models the gate)."

- [ ] **Step 4: Run.** Expected `all checks passed`.

- [ ] **Step 5: Mutation proofs.** (1) `> 0x4B0u` -> `>= 0x4B0u`:
  boundary block `FAIL ...: 1 != 0`. (2) delete `game_restart_longjmp();`:
  `FAIL ...: 0 != 1`. (3) delete the `DSB(CFG_IDLE_FLAG) = 0u;` store:
  `FAIL ...: 90 != 0`. (4) the spin increment is proved by Task 4 (without it
  the driver's guard trips: see Task 4 Step 5).

- [ ] **Step 6: Gate** (N = 2). The spin increment changes `DS_00101500` in
  every oracle run; its readers are the menu/svcmenu clocks, which no oracle
  run reaches. Expected `ORACLES-EQUAL`. If a line moves: revert only the
  spin line, re-run, and record the moved line with the reader that consumed
  the clock (`rg -n 'DS_00101500|CFG_TICK_ISR' port/src`); stop and report.

- [ ] **Step 7: Commit.** `git add port/src/game/config.c port/src/game/config.h
  port/src/game/flow.c port/tests/test_game.c`; message
  `game: idle timeout 0x2EBB3 soft-restarts; master loop advances the 0x500BB clock` + trailer.

---

### Task 3: Case 0x27's longjmp 0x2520B

**Files:**
- Modify: `port/src/game/flow.c` (`game_frame` case `0x27u`, its comment)
- Modify: `port/tests/test_game.c` (`rs_check_case27`, called from `test_restart`)

**Interfaces:** Consumes Task 1; `menu_step` (`0x2FFC4`) returns `-1` at
`0x30440` when the pad word has `0x2000000` and the menu flags have bit 2
(case 0x27 passes flags 4) — pinned by the existing START MENU test
(`test_game.c` ~7876..7895).

- [ ] **Step 1: Write the failing test.** Use the key-loop fixture
  (`kl_env`, `tf_menu_press`) exactly as the START MENU test does; seed the
  `0x4F644` output word as the witness that the normal exit did not run:

  ```c
  /* 0x251F3..0x2520B: 0x2FFC4's result not in {0, -5, -10} is
   * longjmp(0x1044F4, 1); 0x4F644 (input_state_update) is skipped. */
  static void rs_check_case27(void)
  {
      jmp_buf *const prev = game_restart_arm(&rs_jb);
      volatile int landed = 0;
      kl_env(0x27u);
      DSD(0x00105F30u) = 0u;                        /* no latched key */
      DSD(0x00105F2Cu) = DSD(DS_00101500);          /* not idle */
      mem_fill(DS_00107414, 0, 0x40u);              /* the menu uninitialised */
      /* MAIN MENU init: result 0, so 0x25210 runs 0x4F644, which writes
       * ((E4 & 0xFF000000) >> 24) | ((D8 & 0xFF000000) >> 16) = 0 for an
       * idle pad over the seed */
      DSW(DS_001088E0) = 0xBEEFu;
      tf_menu_press(0u);
      if (setjmp(rs_jb) == 0) game_frame(); else landed = 1;
      CHECK_EQ_INT(landed, 0);
      CHECK_EQ_INT((long)DSD(DS_0010741C), 0xBCBEC);     /* MAIN MENU, as ~7870 */
      CHECK_EQ_INT((int)DSW(DS_001088E0), 0);            /* result 0 runs 0x4F644 */
      /* Start (nested START MENU, flags 0), release, then Esc: -5 at
       * 0x30466 (record K11 "The Esc from START MENU returns -5"), which
       * is a normal exit: no restart */
      tf_menu_press(0x1000000u);
      if (setjmp(rs_jb) == 0) game_frame(); else landed = 1;
      tf_menu_press(0u);
      if (setjmp(rs_jb) == 0) game_frame(); else landed = 1;
      DSW(DS_001088E0) = 0xBEEFu;
      tf_menu_press(0x2000000u);
      if (setjmp(rs_jb) == 0) game_frame(); else landed = 1;
      CHECK_EQ_INT(landed, 0);
      CHECK_EQ_INT((int)DSW(DS_001088E0), 0);            /* -5 runs 0x4F644 */
      /* re-init MAIN MENU (flags 4), release, then Esc: -1 at 0x30440 */
      tf_menu_press(0u);
      if (setjmp(rs_jb) == 0) game_frame(); else landed = 1;
      tf_menu_press(0u);
      if (setjmp(rs_jb) == 0) game_frame(); else landed = 1;
      CHECK_EQ_INT(landed, 0);
      CHECK_EQ_INT((long)DSD(DS_0010741C), 0xBCBEC);
      DSW(DS_001088E0) = 0xBEEFu;
      tf_menu_press(0x2000000u);
      if (setjmp(rs_jb) == 0) game_frame(); else landed = 1;
      CHECK_EQ_INT(landed, 1);
      CHECK_EQ_INT((int)DSW(DS_001088E0), 0xBEEF);       /* 0x25210 not reached */
      (void)game_restart_arm(prev);
  }
  ```
  The mode word must stay 0x27 through these frames (no Enter on a START MENU
  item, whose callbacks store modes 0x28..0x2E); assert
  `CHECK_EQ_INT((long)DSD(DS_00104B00), (long)KL_MODE(0x27u));` before the
  last frame (the form the existing mode-0x27 key-loop check at ~10004 uses). Call `rs_check_case27()` from `test_restart` before
  `rs_check_resume`. The B block sits at the end of test_game.c, after
  `kl_env`/`tf_menu_press`/the START MENU test, so they are in scope. If a
  step's result differs from the comments (read `DSD(DS_0010741C)` and the
  result the way the START MENU test at ~7866..7895 does), stop and report:
  `game_frame`'s key loop (`0x24C5C`) runs before the case and may consume the
  pad; the START MENU test drives `menu_step` directly and cannot show that.

- [ ] **Step 2: Run; expect FAIL:** `FAIL ...: 0 != 1` (the port skips
  `0x4F644` and returns today).

- [ ] **Step 3: Implement** in case `0x27u`:
  ```c
          {
              u32 r = menu_step(0xBCBDCu, 0x10u, 4u);           /* 0x251DF..0x251EE 0x2FFC4 */
              if (r == 0u || r == (u32)-5 || r == (u32)-10)     /* 0x251F3..0x251FC */
                  input_state_update();                          /* 0x25210 0x4F644 */
              else
                  game_restart_longjmp();                        /* 0x25201..0x2520B jmp 0x65431, longjmp(0x1044F4, 1) */
          }
  ```
  and replace the comment's `PORT: 0x25206 0x65431 longjmp is out of scope ...
  continues.` with `Any other result soft-restarts (game_restart_longjmp,
  record B §B.3).`

- [ ] **Step 4: Run.** Expected `all checks passed`.

- [ ] **Step 5: Mutation proofs.** (1) delete the `else` arm:
  `FAIL ...: 0 != 1`. (2) drop `-5` from the normal set
  (`r == 0u || r == (u32)-10`): the Start-then-Esc frame lands,
  `FAIL ...: 1 != 0`. (3) drop `0` from the set: the init frame lands,
  `FAIL ...: 1 != 0`.

- [ ] **Step 6: Gate** (N = 3). Expected `ORACLES-EQUAL` (no oracle run
  enters mode 0x27: flow.c's own case comment).

- [ ] **Step 7: Commit.** `git add port/src/game/flow.c port/tests/test_game.c`;
  `game: case 0x27 menu result soft-restarts (0x2520B)` + trailer.

---

### Task 4: End-to-end driver: the service-menu idle timeout restarts the attract

**Files:**
- Modify: `port/tests/test_game.c` (new `int test_restart_drive(void)`)
- Modify: `port/tests/test.h` (`TEST_DRIVERS`: add `X(test_restart_drive, "PR_RESTART")`)
- Modify: `Makefile` (`verify` recipe: one driver line)

**Interfaces:** Consumes Tasks 1-3 and `game_init()` (once per process: this
is why it is a driver). Produces the spec §4.B test that "drives the idle
timeout and asserts the post-state".

- [ ] **Step 1: Write the driver** (copy the game-dir/`game_init()` prologue
  of the existing `PR_FRONTEND_DUMP` driver near `test_game.c:5060-5070`
  verbatim, then):

  ```c
  /* Record B §B.3/§B.5: in the service menu (mode 0x27) with no input the
   * master loop's 0x500BB clock runs until 0x2EB80 sees more than 0x4B0 ticks
   * since the menu's stamp (0x2FFDA), stores DS_00107414 = 0 and soft-restarts:
   * 0x20C24's tail, 0x255CC's prologue, then the attract from its start. */
  #define RD_BOOT   50        /* boot iterations compared after the restart */
  #define RD_GUARD  5000      /* > 0x4B0 + RD_BOOT: a missing clock trips it */
  int test_restart_drive(void)
  {
      int before = g_failures;
      /* ... the PR_FRONTEND_DUMP driver's game-dir + game_init() prologue ... */
      game_loop_begin();
      u16 boot_state[RD_BOOT];
      u32 r1 = 0;
      for (int i = 0; i < RD_BOOT; i++) {
          DSB(DS_000A81A8) = 1;
          game_loop();
          boot_state[i] = DSW(DS_000F0A64);
          if (i == 0) r1 = DSD(DS_000EF6D8);
      }
      /* the raw enters mode 0x27 from mode 3 on Enter (0x24EE0, record §55-A);
       * the driver stands that key in with the store it makes */
      DSW(DS_00104B00) = 0x27u;          /* the word the mode setters store (svcmenu.c:88) */
      DSB(DS_00107414) = 0u;
      DSB(DS_000A81A8) = 1; game_loop();                 /* menu init: stamps 0x105F2C */
      const u32 t0 = DSD(DS_00105F2C);
      u32 t_prev = 0, t_last = 0;
      int n = 0;
      while (DSW(DS_00104B00) == 0x27u && n < RD_GUARD) {
          t_prev = t_last;
          t_last = DSD(DS_00101500);
          DSB(DS_000A81A8) = 1;
          game_loop();
          n++;
      }
      CHECK(n < RD_GUARD, "the idle timeout restarts within the guard");
      CHECK(t_last - t0 > 0x4B0u, "0x2EB9F: over 0x4B0 ticks on the restart frame");
      CHECK(t_prev - t0 <= 0x4B0u, "0x2EB9F: not over on the frame before");
      CHECK_EQ_INT((int)DSW(DS_00104B00), 3);            /* 0x10EA1 */
      CHECK_EQ_INT((int)DSB(DS_00107414), 0);            /* 0x2EBA8 */
      CHECK_EQ_INT((long)DSD(DS_0010150C), 1);           /* 0x255DA, then 0x256C0 */
      CHECK_EQ_INT((long)DSD(DS_00101508), 1);           /* 0x255D4, then the spin */
      CHECK_EQ_INT((int)DSW(DS_000F0A64), (int)boot_state[0]);
      CHECK_EQ_INT((long)DSD(DS_000EF6D8), (long)r1);    /* reseeded 0xABCD, same first frame */
      for (int i = 1; i < RD_BOOT; i++) {
          DSB(DS_000A81A8) = 1;
          game_loop();
          CHECK_EQ_INT((int)DSW(DS_000F0A64), (int)boot_state[i]);
      }
      return g_failures - before;
  }
  ```
  (Use `symbols.h` names where emitted, local `#define`s with the raw address
  otherwise; copy the driver return idiom of the neighbouring drivers.) The
  `t_last`/`t_prev` checks are exact because the menu's non-init steps run no
  `0x2EA78` wait, so the `0x2EB80` call sees the tick value of the iteration's
  start; if a menu step does wait (read `menu_step`'s redraw path), stop and
  re-derive before weakening anything.

- [ ] **Step 2: Register and wire.** `test.h` `TEST_DRIVERS`:
  `X(test_restart_drive, "PR_RESTART")`. `Makefile` `verify`, right after the
  `PR_ORACLE_REQUIRED=1 ... run_tests` line:
  ```make
  	@echo "== restart driver (the 0x65431 soft restart, record B §B.3) =="
  	PR_RESTART=1 PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests
  ```

- [ ] **Step 3: Run it.** `cmake --build build && PR_RESTART=1 PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -5`.
  Expected `all checks passed`. If the `r1` or the state-sequence checks fail,
  do not delete them: the difference is a divergence between the first boot
  and the restart that the F7 omissions explain or do not; record the first
  differing iteration and the state in the B record, stop and report.

- [ ] **Step 4: The two ends of the time window are real.** Temporarily set
  `DSD(DS_00105F2C)` one tick later after the init iteration
  (`DSD(DS_00105F2C) = t0 + 1u;` and use that as `t0`): all checks still pass
  and `n` grows by exactly 1 (print `n` in a temporary `printf`, remove it).

- [ ] **Step 5: Mutation proofs.** (1) remove Task 2's
  `DSD(DS_00101500)++;` from the spin: `FAIL ...: the idle timeout restarts
  within the guard`. (2) remove `game_loop_begin();` from the landing:
  `FAIL ...: <ticks> != 1` twice (the tick pair keeps counting from the
  menu frames). (3) remove `game_init_resume();` from the landing: the mode
  stays 0x27, the menu re-initialises and re-stamps, so
  `FAIL ...: the idle timeout restarts within the guard` and
  `FAIL ...: 39 != 3`.

- [ ] **Step 6: Gate** (N = 4). Expected `EXIT=0`, `ORACLES-EQUAL`, and the
  new `== restart driver` block reports `all checks passed`.

- [ ] **Step 7: Commit.** `git add port/tests/test_game.c port/tests/test.h Makefile`;
  `tests: PR_RESTART driver for the service-menu idle soft restart` + trailer.

---

### Task 5: The quit prompt's ABANDON CONQUEST yes is the soft restart (0x24AB0)

**Files:**
- Modify: `port/src/game/flow.c` (`game_quit_prompt` and its header), `port/src/game/flow.h` (the comments at ~186 and ~446 that say "longjmp quit")
- Modify: `port/tests/test_game.c` (`ch_check_quit_prompt` pass 2; the mode-4 arm of `kl_check_esc`)

**Interfaces:** Consumes Task 1, F3. Raw: `0x24A91 jne 0x24A9C` (AL != 0);
`0x24A9C call 0x1D270`; `0x24AA1 call 0x1B084`; `0x24AA6 mov edx,1`;
`0x24AAB mov eax,0x1044F4`; `0x24AB0 jmp 0x65431`. String `0x1EF` =
`ABANDON CONQUEST? Y/N` (demo-pose record ~25402).

- [ ] **Step 1: Change the tests first (raw wins; record the change).** In
  `ch_check_quit_prompt`, declare `static jmp_buf qp_jb;` above the function,
  arm it for the loop (`jmp_buf *const prev = game_restart_arm(&qp_jb);`,
  unarm after the loop), and replace `game_quit_prompt(hard);` with
  ```c
          volatile int landed = 0;
          if (setjmp(qp_jb) == 0) game_quit_prompt(hard); else landed = 1;
          CHECK_EQ_INT(landed, pass == 2u ? 1 : 0);           /* 0x24AB0 */
  ```
  and `want_quit` to `(pass == 1u)` (pass 2 no longer sets `DS_000A81A8`: the
  seed `0x5A` stays). Every other assertion of the loop is written before
  `0x24AB0` in the raw (`0x24A89`, `0x1D270`, the question row, the presented
  frame) and stays. In `kl_check_esc`, find the mode-4 yes arm
  (`rg -n 'kl_expect_prompt\(0x1EFu' port/tests/test_game.c`), arm the same
  way around its `game_frame()`, assert `landed == 1`, and pass `quit = 0`.
  Any `kl_expect_prompt` assertion whose code runs after `0x24AB0` (after
  `game_quit_prompt` returns into `0x24C5C`) becomes a "sentinel unchanged"
  check; list each in the B record with its address.

- [ ] **Step 2: Run; expect FAIL:** pass 2 `FAIL ...: 0 != 1` (landed) and
  `FAIL ...: 1 != 90` (the quit flag); the mode-4 arm likewise.

- [ ] **Step 3: Implement.**
  ```c
              } else {
                  sound_resume();                            /* 0x24A9C 0x1D270 */
                  /* PORT: 0x24AA1 0x1B084 (the config writer) is deferred (record §50-C). */
                  game_restart_longjmp();                    /* 0x24AA6..0x24AB0 jmp 0x65431, longjmp(0x1044F4, 1) */
              }
  ```
  Header: replace "nonzero asks string 0x1EF and a yes leaves through the
  longjmp quit" with "nonzero asks string 0x1EF (ABANDON CONQUEST? Y/N) and a
  yes soft-restarts (record B §B.3)", and delete the old
  `PORT: 0x24A9C..0x24AB0 ... is out of scope ...` paragraph. Fix the
  flow.h comments that call it a quit (`rg -n 'longjmp quit' port/src`).

- [ ] **Step 4: Run.** Expected `all checks passed`.

- [ ] **Step 5: Mutation proof.** Put back `DSB(DS_000A81A8) = 1u; return;`
  in place of `game_restart_longjmp();`: `FAIL ...: 0 != 1` and
  `FAIL ...: 1 != 90`.

- [ ] **Step 6: Gate** (N = 5). Expected `ORACLES-EQUAL` (no oracle run
  presses ESC).

- [ ] **Step 7: Commit.** `git add port/src/game/flow.c port/src/game/flow.h port/tests/test_game.c`;
  `game: ABANDON CONQUEST yes soft-restarts (0x24AB0), not quit` + trailer.

---

### Task 6: The host termination path for CPU faults

**Files:**
- Modify: `port/src/host.h`, `port/src/host.c`
- Modify: `port/tests/test_platform.c` (`test_host`: `hf_check_hook`, `hf_check_exit`)

**Interfaces:** Produces
```c
/* PORT: the end of the run on a CPU exception the original leaves to DOS/4GW's
 * default handler (no program handler: record B §B.4). With a hook installed
 * (tests) the hook is called and must not return. Otherwise prints `msg`, or
 * when NULL a line naming `exc` and `eip`, to stderr, shuts the host down and
 * exits with `status`. */
_Noreturn void host_cpu_fault(u32 exc, u32 eip, const char *msg, int status);
typedef void (*host_fault_hook_fn)(u32 exc, u32 eip);
/* PORT: test seam; returns the previous hook. */
host_fault_hook_fn host_set_fault_hook(host_fault_hook_fn hook);
```

- [ ] **Step 1: Write the failing tests** in `test_platform.c` (add
  `#include <setjmp.h>`, `<string.h>`, `<sys/wait.h>`, `<unistd.h>` if absent):
  ```c
  static jmp_buf hf_jb;
  static volatile u32 hf_exc, hf_eip;
  static void hf_hook(u32 exc, u32 eip) { hf_exc = exc; hf_eip = eip; longjmp(hf_jb, 1); }

  static void hf_check_hook(void)
  {
      volatile int landed = 0;
      host_fault_hook_fn prev = host_set_fault_hook(hf_hook);
      hf_exc = 0x5Au; hf_eip = 0x5A5Au;
      if (setjmp(hf_jb) == 0) host_cpu_fault(0x0Eu, 0x32578u, "unused", 3); else landed = 1;
      CHECK_EQ_INT(landed, 1);
      CHECK_EQ_INT((long)hf_exc, 0x0E);
      CHECK_EQ_INT((long)hf_eip, 0x32578);
      CHECK(host_set_fault_hook(prev) == hf_hook, "the hook is handed back");
  }

  static void hf_check_exit(void)
  {
      int fds[2];
      CHECK(pipe(fds) == 0, "pipe");
      fflush(stdout); fflush(stderr);          /* the child must not re-flush our buffers */
      pid_t pid = fork();
      if (pid == 0) {
          dup2(fds[1], 2); close(fds[0]);
          host_cpu_fault(0x00u, 0x334E0u, NULL, 7);
      }
      close(fds[1]);
      char buf[256];
      ssize_t n = read(fds[0], buf, sizeof buf - 1);
      close(fds[0]);
      buf[n > 0 ? n : 0] = '\0';
      int st = 0;
      waitpid(pid, &st, 0);
      CHECK(WIFEXITED(st), "the fault ends the process");
      CHECK_EQ_INT(WEXITSTATUS(st), 7);
      CHECK(strstr(buf, "00h") != NULL && strstr(buf, "000334E0") != NULL,
            "the NULL-message line names the exception and the address");
  }
  ```
  Call both from `test_host`.

- [ ] **Step 2: Run; expect a build failure** (undefined `host_cpu_fault`).

- [ ] **Step 3: Implement** in host.c:
  ```c
  static host_fault_hook_fn s_fault_hook;

  host_fault_hook_fn host_set_fault_hook(host_fault_hook_fn hook)
  {
      host_fault_hook_fn prev = s_fault_hook;
      s_fault_hook = hook;
      return prev;
  }

  _Noreturn void host_cpu_fault(u32 exc, u32 eip, const char *msg, int status)
  {
      if (s_fault_hook != NULL) s_fault_hook(exc, eip);
      if (msg != NULL)
          fprintf(stderr, "%s\n", msg);
      else
          fprintf(stderr, "prageport: CPU exception %02Xh at %08X: the original "
                  "leaves it to DOS/4GW's default handler, which ends the run\n",
                  (unsigned)exc, (unsigned)eip);
      host_shutdown();
      exit(status);
  }
  ```
  (`host_shutdown` is safe without `host_init`: every release is guarded,
  host.c:194-203.)

- [ ] **Step 4: Run.** Expected `all checks passed`.

- [ ] **Step 5: Mutation proofs.** (1) drop the hook call: the in-process
  test exits the suite with status 3 and the stderr line
  `unused` (record it). (2) `exit(status)` -> `exit(1)`:
  `FAIL port/tests/test_platform.c:<line>: 1 != 7`. (3) `%08X` -> `%X`:
  `FAIL ...: the NULL-message line names the exception and the address`.

- [ ] **Step 6: Gate** (N = 6).

- [ ] **Step 7: Commit.** `git add port/src/host.c port/src/host.h port/tests/test_platform.c`;
  `host: CPU-fault termination path with a test hook` + trailer.

---

### Task 7: G3, the 0x334E0 idiv (per `$K/branch.txt` G3)

**Files:**
- Modify: `port/src/game/svcmenu.c` (`svc_stats_rows`, a fault-message block near the top defines)
- Modify: `port/tests/test_game.c` (new `sm_check_stats_fault`, called from `test_svcmenu`)

**Interfaces:** Consumes Task 6, F6, the G3 branch.

- [ ] **Step 1: Write the failing test** (ABORT-PINNED and ABORT-RAW):
  ```c
  static jmp_buf sf_jb;
  static volatile u32 sf_exc, sf_eip;
  static volatile int sf_hits;
  static void sf_hook(u32 exc, u32 eip) { sf_exc = exc; sf_eip = eip; sf_hits++; longjmp(sf_jb, 1); }

  /* 0x33458 row 2 {0x96, num 0x12, f1 8, f2 6}: field 8 + field 6 = 0x10000,
   * so EBX & 0xFFFF = 0 and 0x334E0's idiv raises #DE (record B §B.4). Rows 0
   * (field 8 = 0xFFFF: no fault) and 1 (field 0xB = 0: no idiv) draw first. */
  static void sm_check_stats_fault(void)
  {
      ch_text_setup();
      const u32 s8 = config_field_get(8u), s6 = config_field_get(6u), sb = config_field_get(0xBu);
      (void)config_field_set(8u, 0xFFFFu);
      (void)config_field_set(6u, 1u);
      (void)config_field_set(0xBu, 0u);
      CHECK_EQ_INT((long)config_field_get(8u), 0xFFFF);
      CHECK_EQ_INT((long)config_field_get(6u), 1);
      const s32 r = 5;
      text_cursor_set(0x24, r, (const u8 *)"Z", 0u);
      text_cursor_set(0x24, r + 2, (const u8 *)"Z", 0u);
      text_cursor_set(4, r + 3, (const u8 *)"Z", 0u);
      const u32 z0 = ch_cell(r, 0x24), z2 = ch_cell(r + 2, 0x24), z3 = ch_cell(r + 3, 4);
      host_fault_hook_fn prev = host_set_fault_hook(sf_hook);
      sf_hits = 0; sf_exc = 0x5Au; sf_eip = 0x5A5Au;
      if (setjmp(sf_jb) == 0) (void)svc_stats_rows(r);
      (void)host_set_fault_hook(prev);
      CHECK_EQ_INT(sf_hits, 1);
      CHECK_EQ_INT((long)sf_exc, 0);                        /* #DE */
      CHECK_EQ_INT((long)sf_eip, 0x334E0);
      CHECK(ch_cell(r, 0x24) != z0, "row 0 is drawn before the fault");
      CHECK_EQ_INT((long)ch_cell(r + 2, 0x24), (long)z2);   /* 0x334E4.. never runs */
      CHECK_EQ_INT((long)ch_cell(r + 3, 4), (long)z3);      /* row 3 never starts */
      (void)config_field_set(8u, s8);
      (void)config_field_set(6u, s6);
      (void)config_field_set(0xBu, sb);
  }
  ```
  If the two precondition checks fail (a field narrower than 16 bits), stop
  and re-derive the seed from the field descriptors at `0x2D300`; do not
  change the row. For **SILENT**, replace the hook/landing checks with
  `ch_expect` of the digits A's frame shows at row `r + 2`, columns
  `0x24..0x28`, and assert row 3's label is drawn. Call
  `sm_check_stats_fault()` from `test_svcmenu`.

- [ ] **Step 2: Run; expect FAIL:** `FAIL ...: 0 != 1` (hits) and the two
  sentinel checks (the port draws 0 today and goes on).

- [ ] **Step 3: Implement** (ABORT branches):
  ```c
          if (v != 0u) {                                      /* 0x334C9..0x334CB */
              const s32 n = (s32)config_field_get(num);       /* 0x334CD..0x334D5 0x2D974 */
              const s32 d = (s32)(v & 0xFFFFu);               /* 0x334D7 */
              if (d == 0)                                     /* 0x334E0 idiv, EBX = 0: #DE */
                  host_cpu_fault(0x00u, 0x334E0u, SVC_DE_MSG, SVC_FAULT_EXIT);
              v = (u32)(n / d);                               /* 0x334DD sar edx,31; 0x334E0 idiv ebx */
          }
  ```
  With, after the other `SVC_` defines:
  - ABORT-RAW:
    ```c
    /* PORT: record B §B.4: no program handler hooks vector 0 or 0Eh, so a
     * fault goes to DOS/4GW's default handler, which ends the run. Its text and
     * errorlevel are not captured (record B §B.6): the host prints its own
     * line and exits 1. */
    #define SVC_DE_MSG      NULL
    #define SVC_FAULT_EXIT  1
    ```
  - ABORT-PINNED: the same block with `SVC_DE_MSG` = A's verbatim first line
    as a string literal and `SVC_FAULT_EXIT` = A's errorlevel (or `1` with the
    PORT sentence kept for the exit status only), citing A's record entry.
  - SILENT: no `host_cpu_fault`; `v` gets the value whose digits A's frame
    shows, with a `PORT:` naming A's frame and DOS/4GW/DOSBox-X as the source.
  Remove the old `PORT: a non-zero sum with a zero low word ... draws 0`
  comment. `#include "../host.h"` in svcmenu.c if absent.

- [ ] **Step 4: Run.** Expected `all checks passed`.

- [ ] **Step 5: Mutation proof.** Restore `v = d != 0 ? (u32)(n / d) : 0u;`:
  `FAIL ...: 0 != 1` and the two sentinel FAIL lines.

- [ ] **Step 6: Gate** (N = 7). Expected `ORACLES-EQUAL` (no oracle reaches
  STATISTICS).

- [ ] **Step 7: Commit.** `git add port/src/game/svcmenu.c port/tests/test_game.c`;
  `game: STATISTICS 0x334E0 idiv #DE ends the run (G3, record B §B.4)` + trailer.

---

### Task 8: G2, the 0xFFE80003 read (per `$K/branch.txt` G2)

**Files:**
- Modify: `port/src/game/svcmenu.c` (`svc_test_controls`, the `diag` arm)
- Modify: `port/tests/test_game.c` (`sm_check_controls` TEST B, ~8609..8640)

**Interfaces:** Consumes Task 6, F5, the G2 branch, the `sf_hook` block of
Task 7 (reuse it; it is defined above `sm_check_controls` only if Task 7 put
it there — move the four `sf_` lines above `sm_check_controls` if needed,
body unchanged).

- [ ] **Step 1: Convert TEST B first** (ABORT branches). Before
  `(void)svc_test_controls(0xBCC8Cu);` seed `text_cursor_set(0xE, 7,
  (const u8 *)"Z", 0u)` and snapshot every cell TEST B asserts; install
  `sf_hook`, call under `setjmp(sf_jb)`, restore the hook, then assert
  `sf_hits == 1`, `sf_exc == 0x0E` (or A's exception number for
  ABORT-PINNED), `sf_eip == 0x32578`, and `ch_cell(7, 0xE)` equal to its
  snapshot (replacing `CHECK(ch_cell(7, 0xE) != 0u, "the raw data row is
  drawn")`). For each other TEST B assertion, classify it by the address of the
  instruction that draws it: drawn before `0x32573` (the first table loop
  `0x324F4..0x32518` strings, row 4 RAW DATA, the row-6 address, the pad word
  `0x32558..0x3256E`, device 8's names `0x3248A..0x32499`) stays; drawn at or
  after `0x3257A` (the sticks `0x325A4..0x325D6`, the second table loop
  `0x3260E..`, the bit-8 marker) becomes "cell equals its snapshot". Set
  `sm_end`'s expected count to the number of script steps consumed before the
  fault (the `0x2EA78` waits `svc_test_controls` runs before `0x32573`;
  derive it from the code, write the derivation in the B record). List every
  converted assertion with its address in the B record. For **SILENT**, keep
  TEST B as is and add `ch_expect` of A's byte in hex at row 7 columns
  `0xE..0x15`.

- [ ] **Step 2: Run; expect FAIL:** `FAIL ...: 0 != 1` (hits) and the
  row-7 sentinel.

- [ ] **Step 3: Implement** (ABORT branches):
  ```c
          if (diag != 0u) {                                   /* 0x32554..0x32556 */
              text_hex_set(0xE, 6, keys, 8, 0u, 0x3000u);     /* 0x32558..0x3256E 0x2F48C */
              /* 0x32573 mov ebx,0xFFE80003; 0x32578 mov bl,[ebx]: an arcade
               * address with no memory behind it in the DOS build (record K11
               * §K11.5). The fault ends the run (record B §B.4); the RAW DATA
               * draw 0x3257A..0x32596 is never reached. */
              host_cpu_fault(0x0Eu, 0x32578u, SVC_PF_MSG, SVC_FAULT_EXIT);
          }
  ```
  ABORT-RAW adds, above the call,
  `/* TODO(verify): the page fault is inferred (no program handler, record B §B.4), not captured (record B §B.6). */`
  and `#define SVC_PF_MSG NULL` next to Task 7's block. ABORT-PINNED:
  `SVC_PF_MSG` = A's verbatim line, the exception number A shows in place of
  `0x0Eu`. SILENT: `text_hex_set(0xE, 7, <A's byte>u, 8, 0u, 0x3000u);  /* 0x3257A..0x32596, EBX & 0xFF (0x32590) */`
  with a `PORT:` naming A's frame. Remove the old `PORT: 0x32573..0x32578 reads
  ... draws 0 in its place` comment.

- [ ] **Step 4: Run.** Expected `all checks passed`.

- [ ] **Step 5: Mutation proof.** Put back the old
  `text_hex_set(0xE, 7, 0u, 8, 0u, 0x3000u);` in place of the fault call:
  `FAIL ...: 0 != 1` and the row-7 sentinel FAIL.

- [ ] **Step 6: Gate** (N = 8).

- [ ] **Step 7: Commit.** `git add port/src/game/svcmenu.c port/tests/test_game.c`;
  `game: TEST CONTROLS 0xFFE80003 read ends the run (G2, record B §B.4)` + trailer.

---

### Task 9: Close the ledger rows and the record

**Files:**
- Modify: `docs/superpowers/plans/2026-09-29-all-gaps-ledger.md` (§D row `0x27`
  at ~336, the list item at ~354, §E rows 32/33 at ~402-403)
- Modify: `docs/superpowers/plans/2026-09-30-named-gaps-b-derivations.md` (§B.7, §B.8)
- Modify: `docs/PROGRESS.md` (one paragraph), `README.md` (only if Step 2 says so)
- Modify: `tools/port_classification.txt` (only the `4FBBB` evidence text, Task 0 correction 2)

- [ ] **Step 1: Ledger.** Row `0x27`: status `inline; the longjmp is
  game_restart_longjmp (record B §B.3), closed`; drop "the case-0x27 longjmp,
  a spec-§7 deviation that stays;" from the ~354 list. Row 32: **closed**
  (ABORT-PINNED / SILENT, with A's evidence) or **restated** (ABORT-RAW: "the
  port ends the run at 0x32578 through host_cpu_fault; the fault and DOS/4GW's
  text/exit status are not captured, record B §B.4/§B.6"). Row 33: likewise
  with `0x334E0` (ABORT-RAW restates only the message text and exit status:
  the #DE itself is certain, F6).

- [ ] **Step 2: Counts.** `python3 tools/port_progress.py`; if the ported
  number moved from the Task 0 baseline (it can, if a `/* 0x65431` header is
  counted), update `README.md`'s title and "N% of the original's D real
  functions" line. Record the checks count delta against `$K/checks-base.txt`
  with its reasons (new tests; the changed pass-2/mode-4/TEST B assertions).

- [ ] **Step 3: Record §B.7 Not tested** (at least): the windowed run's
  on-screen appearance of a fault (stderr only, `PORT:`); the ISR gate on
  `DS_00104B22` in the master spin; the F7 omissions; the restart while music
  plays (`0x5D808` unported); the SVC idle restart from inside a blocking
  svcmenu screen other than the menu (the path exists through
  `config_key_latched`'s callers at svcmenu.c 988/1213/1257/1558; only the
  unit and the menu-level driver paths are tested). §B.8 closure: the three
  rows' final status and the mutation table.

- [ ] **Step 4: PROGRESS.md** — append one paragraph: B closed G1 (soft
  restart at all three raw sites, idle clock), G2/G3 per branch, the
  `0x24AB0` correction, the tests and the driver.

- [ ] **Step 5: Final gate** (N = 9). Expected `EXIT=0`, `ORACLES-EQUAL`,
  `== restart driver` `all checks passed`.

- [ ] **Step 6: Commit.** `git add` the modified docs (and `README.md`,
  `tools/port_classification.txt` if changed); `docs: close named gaps G1-G3 (record B)` + trailer.

---

## Self-Review

- **Spec coverage.** §4.B G1 (`0x2520B`): Task 3, plus the same mechanism at
  `0x2EBB3` (Task 2) and `0x24AB0` (Task 5); "the return target, the state it
  leaves in mem[], the timing": F2/F7 and Tasks 1/4; "a test drives the idle
  timeout and asserts the post-state": Task 4. G2/G3 "as A establishes it":
  Task 0 Step 3's branch selection, Tasks 7/8 with a branch each; "a PORT note
  only if a host-level stand-in is unavoidable": `host_cpu_fault` (Task 6).
  Acceptance "three ledger rows closed or restated; make verify green": Task 9
  and the gate in every code task. §3.4 real setjmp/longjmp: the raw's site is
  single (F1), so Task 1 uses C `setjmp`/`longjmp`.
- **Spec conflicts (surfaced for the user).** (1) G1 names only `0x2520B`; the
  idle timeout proper is `0x2EBB3` in `0x2EB80`, and `0x24AB0` is the same
  restart: B closes all three. (2) `0x24AB0` corrects existing port behaviour
  (ABANDON CONQUEST yes quits to DOS today) and two existing test
  expectations. (3) F4: the port's master loop never advances `DS_00101500`,
  so the idle timeout needs a loop change the spec does not list. (4) The
  landing is in `game_loop`, not in `0x20C10`'s port equivalent (`PORT:`),
  because the drivers step the loop per call. (5) The spec's "K7+K12 record
  §2.6" citation does not resolve. (6) G2 needs a persisted config with field
  `0x2A` bit 4, which no in-game writer sets (F5): A must seed the config store
  or G2 takes ABORT-RAW.
- **Placeholder scan.** The only values not written literally are the ones A's
  record supplies (ABORT-PINNED message line, errorlevel, SILENT digits); each
  has its ABORT-RAW literal fallback and names the record entry it comes from.
- **Type consistency.** `game_restart_arm(jmp_buf *) -> jmp_buf *`,
  `_Noreturn void game_restart_longjmp(void)`, `void game_init_resume(void)`,
  `host_fault_hook_fn host_set_fault_hook(host_fault_hook_fn)`,
  `_Noreturn void host_cpu_fault(u32, u32, const char *, int)`, used with the
  same signatures in Tasks 1-8. `setjmp` is only used as a full controlling
  expression (`switch`, `if (... == 0)`, `if (... != 0)`), as C11 7.13.1.1
  requires; locals written after a `setjmp` and read after the landing are
  `volatile`.
