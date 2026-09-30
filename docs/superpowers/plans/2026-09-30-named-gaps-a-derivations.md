# Named gaps A — the K11 ground-truth harness: derivation record

Plan: `docs/superpowers/plans/2026-09-30-named-gaps-a-k11-harness.md`. Spec:
`docs/superpowers/specs/2026-09-30-named-gaps-design.md` §4 A. This record is
the authoritative source for sub-project B's G1/G2/G3 work. Every claim cites
a raw address, a `poll.log` line (`data/k11-captures/<scenario>/poll.log:<n>`)
or a capture frame (`<scenario> frame <i> (raw <r>)`). On any conflict the raw
wins; corrections are recorded here with their address.

## §A.0 Baseline (Task 0)

- **Where.** Branch `named-gaps-a`, a worktree `.worktrees/named-gaps-a` off
  `main` `af135ec` (merge: all-gaps Task 7 reconciliation), with `data` and
  `.superpowers` symlinked to the main checkout and the two git-ignored oracle
  fixtures (`port/tests/ghidra_data.bin`, `title_screen_ref.ppm`) copied in.
  **Deviation from the plan (recorded):** the plan says "branch in place from
  `named-gaps-spec` at `a169296`, no worktree"; the controller's
  `parallel-rules.md` (A runs in parallel with C/D/E/F) overrides it with a
  worktree off `main` `af135ec`, which contains the plan and spec. The worktree
  sees `data/` through the symlink, so the plan's reason for avoiding a
  worktree does not apply. Every `make` run uses the tag-`na` overrides
  (`SMK_DUMP=/tmp/pr_na_smk … TITLE_PIN_DIR=/tmp/pr_na_pin
  AUDIO_WAV=/tmp/pr_na_fm.wav`, plus `K11_DUMP=/tmp/pr_na_k11` from Task 8 on).
- **Oracle-line baseline.** The gate compares against
  `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt` (the
  all-gaps ledger §A lines: 45 lines, sha256
  `eaca80cf8d4a3bffe980472ea110ddf4bf038975003ed9ec6a76da6d5a92454a`, which include the `== demo-fight` lines),
  as `parallel-rules.md` prescribes, rather than a fresh `$S/a_or_base.txt`: a
  first baseline `make verify` on the untouched tree was killed (signal 15)
  when the previous session ended, in the demo-fight step. The Task 6 gate
  (§A.3) is the first full run on this branch.
- `python3 tools/port_progress.py`: `767 1203 64` and
  `731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)`.
- Fixed inputs (all as the plan expects):
  - `data/game/C/PRAGE.EXE` sha256 `eecba701576d36d1a217a271acd90e9aa4473121db8d51e8c8085c194ce0e91b`
  - `data/game/C/CMOS` sha256 `2c2bc349e0bcde11b38485c903432d1ddaaf0b4a89fa70444a3a3da1e85d5db0`, `2040 0` (2040 bytes, none non-zero)
  - `DOSBox-X version 2026.08.31 SDL2, copyright 2011-2026 The DOSBox-X Team.`
  - `ffmpeg version 9.0.2`, `ffprobe version 9.0.2`; `capstone 5.0.7`, `PIL 12.3.0`
  - The pinned copy's sha256 is recorded in §A.4 (it is built by `make
    title-pin TITLE_PIN_DIR=/tmp/pr_na_pin` before the first capture).

## §A.1 Raw facts (Task 1)

Scratch disassembler `/tmp/named-gaps-a/dx.py` (capstone on the raw file at
`VA + 0x52E54`). Conversion rule verified on `0x2FA10`: `0002FA10 b82a000000
mov eax, 0x2a`, `0002FA15 e85adfffff call 0x2d974`, `0002FA1A 24fc and al,
0xfc`, `0002FA1C a310740800 mov dword ptr [0x87410], eax` — the pre-fixup
displacement `0x87410` is `DS_00107410` (VA − `0x80000`).

### §A.1.1 G2 reachability (the DIAGS arm)

- `field 2A desc 0x1b80 width 4` (descriptor table at code VA `0x2D300`).
- Every code-object dword `0x87400..0x87413` (pre-fixup): `ref 0x10740c at
  0x2cacd`, `0x2f983`, `0x2fa03`; `ref 0x107410 at 0x2fa1d`, `0x32363`. No
  data-object dword holds `0x87410`.
- `0x2D974` (listed): with descriptor `0x1B80`, `eax = 0x6E + 1 = 0x6F` is odd,
  so `0x2D9AB..0x2D9B5` reads `byte & 0xF` and `edx` drops to 0: 4 bits.
- `0x32361 mov ebp,[0x87410]; 0x32367 and ebp,0x10; 0x3236A je 0x323d7` is the
  only reader; `0x2FA1A and al,0xfc; 0x2FA1C mov [0x87410],eax` the only writer.

**Conclusion.** Field `0x2A` holds 0..15; `DS_00107410 = field & ~3` ∈ {0, 4,
8, 0xC}; bit 4 is never set, so **the `0xFFE80003` read at `0x32573` is
unreachable in the stock game**. This corrects K11 record §K11.5 ("config
field `0x2A` bit 4") and spec §6 ("needs the diagnostic flag set"); spec §8
already carries the correction. Task 12's capture can reach the arm only by a
poke (a counterfactual).

### §A.1.2 G3 reachability (the `idiv` rows)

- Field widths: `06 0x243c7 16`, `07 0x24448 16`, `08 0x244c9 16`, `09 0x2454a
  16`, `12 0x34ad3 32`, `13 0x34c54 32`.
- `0x326C4` rows: `row 0x94 num 0xa den 0x8 + 0x0`, `row 0x95 num 0xc den 0xb +
  0x0`, `row 0x96 num 0x12 den 0x8 + 0x6`, `row 0x97 num 0x13 den 0x9 + 0x7`.
- `0x33458` listing: `0x334A6 test edx,edx; je 0x334c0` (no second field),
  else `0x334AC..0x334BC` `d = get(f1) + get(f2)`; `0x334C9 test eax,eax; je
  0x334e4` skips the divide only for `d == 0`; `0x334D7 and ebx,0xffff; 0x334DD
  sar edx,0x1f; 0x334E0 idiv ebx`.

**Fault condition.** `#DE` iff `d != 0 && (d & 0xFFFF) == 0` (the dividend is
sign-extended 32-bit and the divisor ≤ `0xFFFF`, so no quotient overflow).
Rows `0x94`/`0x95` divide by one 16-bit field and cannot fault. Rows
`0x96`/`0x97` add two 16-bit fields, so `d ≤ 0x1FFFE` and the only faulting
sum is **exactly `0x10000`**. **Correction to the plan:** its "a sum of
exactly `0x10000` or `0x20000 − …`" has no second case.

**The audit add `0x2DAE4`** (listed): `0x2DAE9 call 0x2d974` (get), `0x2DAF2
lea ecx,[eax+edx]`, then `0x2DB4D call 0x2da0c` (set) with the sum. `0x2DA0C`
writes only the descriptor's nibbles/byte, so a 16-bit field **wraps** modulo
`0x10000` (no saturation). Field 3 alone has the extra `0x2DAF5..0x2DB43` carry
into byte `0x105E2F`.

**Reachability in play.** The only `+1` callers of fields 8 and 6 are
`0x32A85` (`mov eax,8`, a one-player game that is not a continue: `0x32A77
test ecx,ecx; jne 0x32a91`) and `0x32A9B` (`mov eax,6`, a one-player
continue), both under `0x32A65 cmp edx,1` (one player). So `field 8 + field
6` counts one-player game ends, each modulo `0x10000`; row `0x96` faults when
the two counts sum to exactly `0x10000` — at least 65536 one-player game ends
persisted through the CMOS save (`0x1B084`) with no STATISTICS clear. Row
`0x97` is the same for two-player (`0x32ADE mov eax,7`, field 9's caller).

**Stimulus (spec §8 wins over the plan).** The plan picks fields 8 = 6 =
`0x8000`; spec §8 names field 8 = `0xFFFF`, field 6 = `1`, and says §8 wins on
conflicts. Both sum to `0x10000`; the `de` scenario uses §8's pair
(`tools/k11_session.py`), which is also the in-play shape (65535 new games and
one continue). It is a stimulus, not a fitted value.

### §A.1.3 G1: the setjmp/longjmp map

The jmp_buf `0x1044F4` (pre-fixup `0x844f4`) is loaded at `0x20c1a`,
`0x24aab`, `0x25206`, `0x2ebae` (every code-object occurrence).

- `0x20C1A mov eax,0x844f4; 0x20C1F call 0x653fc` — **the single setjmp**
  (`0x653FC` stores EBX/ECX/EDX/ESI/EDI/EBP, the return EIP, ESP and the six
  segment registers, returns 0). Its return value is not tested: the
  continuation `0x20C24..` runs the same way on the first pass and after a
  longjmp: `0x20C24 mov eax,-1; xor dl,dl; call 0x2c8f0`, `xor dh,dh; call
  0x29d60`, `mov [0x84b1d],dl`, `call 0x38b70`, `call 0x2f920`, `xor eax,eax;
  call 0x4f228`, `mov eax,1; mov ebx,0xabcd; call 0x2baf4` (RNG re-seed),
  `mov [0x6f6d8],ebx`, `mov eax,0x29; call 0x2d974; mov [0x84528],eax` (field
  `0x29`), `… call 0x47370` (language), `call 0x1e824`, `mov eax,[0x84528];
  and eax,0x100 …`.
- **Three longjmps** (`0x65431`, WATCOM `longjmp`: restores SS:ESP, pushes the
  saved EIP, `or edx,edx; jne; inc edx` so the value is ≥ 1, reloads the
  registers, `verr`-checks ES/FS/GS, `ret`), each `mov edx,1; mov
  eax,0x844f4; jmp 0x65431`:
  - `0x24AB0` — preceded by `0x24A9C call 0x1d270; 0x24AA1 call 0x1b084` (the
    CMOS save): the quit prompt's hard yes, `game_quit_prompt(1)`,
    `flow.c:6512`.
  - `0x2520B` — case `0x27`: `0x251F3 test eax,eax; je 0x25210; cmp eax,-5;
    je; cmp eax,-10; je; …jmp 0x65431` (any menu result other than 0/−5/−10,
    which includes the MAIN MENU Esc with flags 4, K11 record §K11.2
    mutation 5).
  - `0x2EBB3` — the idle timeout in `0x2EB80`: `0x2EB81 mov edx,[0x85f30];
    test edx,edx; je 0x2eb8f` (only with the latch `DS_00105F30` = 0), `0x2EB8F
    call 0x500bb; sub eax,[0x85f2c]; cmp eax,0x4b0; jbe 0x2ebb8` (unsigned
    `tick − DS_00105F2C > 0x4B0`), `0x2EBA1 xor ah,ah; 0x2EBA8 mov
    [0x87414],ah` (`DS_00107414 = 0`), then the longjmp.

This answers spec §3.4's question from the raw: the setjmp is single, the
longjmps are three. Task 11 captures `0x2EBB3` and `0x2520B`; `0x24AB0` is
reached from a game mode, not mode `0x27`, and is not captured.

### §A.1.4 Exception handlers

`int 31h sites 31`: the first at `0x625b4` (`mov ax,6; mov bx,ds` — get
segment base), the rest in the runtime from `0x664f2`. Where AX is set in
the four instructions before a site (some sites set it earlier), the values are `0x0006`, `0x0200` (×2, get real
mode vector), `0x0000`/`0x0001` (LDT descriptors), `0x0204`/`0x0205` (get/set
protected-mode interrupt vector) at `0x6682B`/`0x6684B`/`0x66886`, `0x0501`,
`0x0502`, `0x0503`, `0x0006`, `0x0A00`. The `0x0205` sites load BL from an
IRQ number (`0x6681A cmp ebx,8; jb; add ebx,0x60; add ebx,8`): vectors
`8..0xF`/`0x70..0x77`, i.e. hardware IRQs, never vector 0 or `0xE`. No `mov
ax/eax,0x203`, `0x212`, `0x213` or `0x2500` immediate exists in the code
object; the two `int 21h AH=25h` sites (`0x66526`, `0x6657B`) set vector 8
(`mov eax,8` before, the timer). **Prediction:** no game or runtime handler
for `#DE` or `#PF`; DOS/4GW's default exception handler runs. Tasks 12–13's
captures decide.

### §A.1.5 The menu input path

`dev1 0x0 ['0x1f73', '0x2d78', '0x2c7a', '0x2e63', '0x1675', '0x1769',
'0x316e', '0x326d']`, `dev2 0x0 ['0x4800', '0x5000', '0x4b00', '0x4d00',
'0x4700', '0x4900', '0x4f00', '0x5100']` (data `0x122C62`). The arrows are
player 2's keys and player 2's device word is 0 (keyboard), so in
`config_key_flags` (`0x2EBF0`) a latched arrow gives no direction bit
(`cfg_dir_bits`, `config.c:643`, `0x2EC85`: `k2 == code && [+0x2D6] == 0`
returns 0). Menu Up/Down therefore come only from the pad level: the key
bitmap `[DS_00101514]+0x2D8/+0x2D9`, filled by the host-owned ISR sampler
`0x1BBAC` and turned into `DS_000E1C34` by `0x500C4`. Enter and Esc come from
the latched key (`0x2EDA3..0x2EDC9`). Hence the poller logs `kb` and the port
driver holds the bitmap through a seam (§A.3).

### §A.1.6 Entry and exits of mode `0x27`

`0x24ECF xor eax,eax; mov ax,[0x84b00]; cmp eax,3; jne 0x24d08; 0x24EE0 mov
[0x84b00],di` — Enter (ascii `0x0D`) with `DS_00104B00 == 3` stores `0x27`
(`flow.c:6948..7010`). Esc in mode 3 opens the quit prompt; Esc in mode `0x27`
does nothing in the key loop. Exits (K11 record §K11.2): the START MENU Esc
gives −5 and re-initialises the MAIN MENU; the OPTIONS MENU Esc gives −1 inside
the Enter arm, so `0x2FFC4` returns 0; the MAIN MENU Esc with flags 4 gives −1
and case `0x27` longjmps at `0x2520B`. The `walk` key list never presses Esc
on the MAIN MENU.

## §A.2 DOSBox-X probe (Task 2)

Facts from `--help`/`-helpdebug`/the binary (the planner's, re-used): the
flags `-defaultconf -fastlaunch -nopromptfolder -nogui -nomenu -time-limit
-c -set -exit`; `AUTOTYPE [-list] [-w WAIT] [-p PACE] button…` ("," adds a
PACE); `DX-CAPTURE [/V] [/O] …`; `[dosbox] memory file`; `[dos] log console`.
The debugger's pty automation is not used (arena-backdrop §1.7, demo-pose §1.6).

Probe (`/tmp/named-gaps-a/probe`, the plan's exact command): `exit=0`;
`guest.mem` **16777216 bytes** (16 MiB, the default `memsize`); the log holds
`7:K11-LOGCON-PROBE` (and `LOG: K11-LOGCON-PROBE` on stdout), so
`-set "dos log console=quiet"` works and is `LOG_CON_ARGS`; `ATLIST.TXT` (1747
bytes) counts `enter: 1`, `esc: 1`, `up: 1`, `down: 1`, `left: 1`, `right: 1`;
no `unknown`/`invalid`/`not found` line. No Step 3 branch was needed. The
button names in `k11_session.KEYS` are exactly these six.

## §A.3 Tool and driver contracts (Tasks 3–8)

### §A.3.1 `tools/k11_fields.py` (Task 3)

The codec follows the `0x2D974` (get) and `0x2DA0C` (set) listings line by line
(each statement cites its address). Re-read here: `0x2DA29..0x2DA34` stores the
low byte in the byte table first (`[eax+0x85daf]`, DS `0x105DAF + idx`), then
`0x2DA59..0x2DAC0` writes the nibbles upward from `bitpos >> 1`: an odd
`bitpos` fills the high nibble of its byte (`0x2DA72..0x2DA90`), a lone last
nibble the low nibble (`0x2DA9F..0x2DAAE`), whole bytes otherwise
(`0x2DAB9`). The window `[0x105DAF, 0x105E30)` (129 bytes) covers every
descriptor: the highest byte-table index is 0x26 (`0x105DD5`), the nibble bytes
end at `0x105E2E` (exclusive `0x105E2F`), and `0x105E2F` is field 3's carry
byte (`0x2DB05`). `set_` writes neither the dirty byte `DS_00105DD8`
(`0x2DA3B`, `0x2DA4E`) nor calls `0x2D4EC`. `Ran 7 tests … OK`. Mutation (the
two nibble reads of `get` swapped): `FAILED (failures=5)` —
`test_field_2a_keeps_four_bits`, `test_round_trip_and_isolation_for_every_field`,
`test_byte_part_is_the_low_byte`, `test_get_reads_the_listing_order`,
`test_set_then_get_and_neighbour_nibbles_kept`; restored: `OK`.

### §A.3.2 `tools/k11_session.py` (Task 4)

As the plan, with one change: the `de` scenario pokes field 8 = `0xFFFF`,
field 6 = `1` (spec §8; §A.1.2). `Ran 6 tests … OK`. Mutation (`normalise`
returns the word unchanged): `ERROR: test_port_script_from_a_synthetic_poll`
(`ScriptError: observed keys ['1C0D', '50E0', '011B'] differ from scenario _t
['1C0D', '5000', '011B']`); restored: `OK`.

### §A.3.3 `tools/k11_capture.py` (Task 5)

As the plan. `Ran 6 tests … OK`. Mutation (`+ os.sep` dropped from the guard):
`FAIL: test_guard_refuses_everything_but_a_capture_subdir`; restored: `OK`.

### §A.3.4 The port side (Task 6)

- **Seam.** `host_set_key_bits_override` (`host.h`/`host.c`, a `PORT:` test
  seam, off by default). Two checks in `test_host`. Mutation (the override line
  deleted): `FAIL …/test_platform.c:2455: 0 != 4660`, `FAILURES: 1`; restored:
  `all checks passed`.
- **Driver** `test_k11_oracle` in `TEST_DRIVERS` under `PR_K11_DUMP` (runs
  alone; `game_init()` once).
- **Smoke** (`/tmp/named-gaps-a/k11smoke.script`: `enter_frame 300`, `key 60 1C
  0D`, `key 120 01 1B`, `end 240`). Red with `enter_state FFFF`: `FAIL
  …/test_game.c:11881: 0 != 65535`, `FAILURES: 1`. Green with `enter_state
  0000`: `all checks passed`, 9 frames `frame_0000..0008.raw`; `screens.txt` =
  `key 0 settled 2`, `key 1 settled 6`, `end settled 8`; `k11.log` = `key 0
  dt=60 tick=00000173 f=0165 mode=0027 st=0000 ent=000BCBEC`, `key 1 dt=120
  tick=000001AF f=019F mode=0027 st=0000 ent=000BCCDC` — identical to the
  planner's values.
- **Driver mutation** (the Enter's ascii `0x0D` → `0x0A`): `FAIL
  …/test_game.c:11879: 3 != 39`, `FAILURES: 1`, exit 1; restored: `all checks
  passed`, exit 0.
- **Gate** (`make verify` with the `na` overrides, then the dumps and the WAV):
  `verify-exit=0`, `ORACLES-EQUAL` (diff against the 45 base lines empty),
  `DUMPS-IDENTICAL` (`dumpsha.sh na` against `base.sha256`), `WAV-IDENTICAL`
  (`/tmp/pr_na_fm.wav` = `before-t2.wav`).
- Assertion sites (`rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc
  -l`): 13545 → 13557 (+2 seam, +10 driver).

### §A.3.5 `tools/k11_compare.py` (Task 7)

As the plan. `Ran 7 tests … OK`. Mutation (`missing = []`): `FAIL:
test_missing_settled_screen_fails`; restored: `OK`.

### §A.3.6 Make targets (Task 8)

`make k11-capture` (passes `--exe $(TITLE_PIN_DIR)/PRAGE.EXE`, so a tagged
`TITLE_PIN_DIR` override reaches the capture), `make k11-oracle` (in `make
verify`, before the unit tests) and `make k11-report`. Without a capture:
`k11-oracle: no capture at data/k11-captures/walk, frames not compared`,
`k11_compare: no capture at data/k11-captures/walk (skipped)`, exit 0; four
`k11-report: no capture at …` lines, exit 0. Gate t8: `verify-exit=0`,
`ORACLES-EQUAL`, `DUMPS-IDENTICAL`, `WAV-IDENTICAL`, the two skip lines and
`Ran 26 tests` (7 + 6 + 6 + 7).

## §A.4 The walk capture (Task 9)

**Two AUTOTYPE runs failed; the capture uses `--input inject` (§A.9).** With
the plan's `AUTOTYPE` line, run 1 received 11 of 38 keys (the last `K` at
`ms=35623`, the STATISTICS Enter; no `K`, no `kb` change afterwards, and the
idle timeout then fired: `ms=55736` `mode=0003`, `tick 0xCA3 − ktime 0x7F0 =
0x4B3`) and run 2 received 1 of 38 (`K ms=25043 … key=1C0D` only). Probes on
the MAIN MENU received 23/23 (`probekeys`) and 15/15 (`probestats`, which
entered STATISTICS), so the loss is in the host's typer, not a game state.
These runs are kept out of `data/` (`/tmp/named-gaps-a/caps/`).

`data/k11-captures/walk` (`make k11-capture scenario=walk`, input inject):

- `session.txt`: `exe=/tmp/pr_na_pin/PRAGE.EXE
  sha256=8120f1bd1df389ed94cb329c030f95d9e38caad193bbd9840717af557161a68d`
  (the pinned copy), `cmos=zero pokes=[] input=inject enter_wait=25 pace=1`,
  `time_limit=75 wall_s=75.5 rc=0`, `avis=['prage_000.avi', 'prage_001.avi']
  fps=70.0866 dro=['prage_000.dro', 'prage_001.dro']`.
- `window.txt`: `raw_window=1373..4345`, `twi5_last=962 twg_last=1322`; 171
  distinct frames.
- `poll.log:1`: `B ms=811 base=00266000` (the data object's runtime base, as
  earlier records measured). Runtime pointers are base-relative: `ent`
  `0x2A2BEC` is the port's `0xBCBEC` (`0x2A2BEC − 0x266000 + 0x80000`).
- `poll.log:1402..1404`: `I ms=25000 key=1C0D down=00010090 tab=FF ring=1`,
  `K ms=25003 tick=00000571 f=0143 key=1C0D`, `P ms=25012 f=0144 st=0000
  mode=0027 …` — the Enter, and mode `0x27` on the next poll at `f = 0x144`
  (the port script's `enter_frame 324`) in attract state 0.
- 38 `K` lines; the key sequence is exactly WALK (Enter/Down/Esc in order:
  `E E D D X D E E X D E X X X X X D E X D E X D E X D E X D E X D E X D E X
  X`). No `LEFT 0x27` line (mode stays `0x27` to the end). `E ms=75255
  reason=time-limit rc=0`.
- The DROs (`prage_000.dro` 532 bytes, `prage_001.dro` 910 bytes) are kept as
  the OPL artefact and not compared: the walk enters SOUND TEST and MUSIC TEST
  and leaves each with Esc, without playing anything.
- **Input path (§A.1.5).** A Down tap holds the key bitmap: `poll.log:1533`
  `kb=0040` from tick `0x5E8`, `poll.log:1538` `kb=0000 pad=00004000` at tick
  `0x5EC` (the pad level follows). The port script holds 11 `pad … 0040 …`
  lines (the 11 Downs). **Correction to the plan:** `DS_0010741C` (`ent`) is
  the current menu record, not the cursor row: it steps only when a menu
  changes (MAIN `0x2A2BEC` → START `0x2A2CDC` at `poll.log:1469`), so "each
  Down advances `ent` by `0x10`" does not hold; the Downs are proved by the
  `kb` runs and by the highlighted row moving in the frames below.

Screens (`walk frame <i> (raw <r>)`): 90 (1746) the title logo sweep before
the Enter; 92 (1753) MAIN MENU; 94..98 (1821..1961) START MENU and its cursor
rows; 100/101 (2035/2100) MAIN MENU, "GAME OPTIONS" highlighted; 105 (2173)
OPTIONS MENU; 107 (2241) CONFIG OPTIONS (titled "GAME OPTIONS"); 113 (2455)
STATISTICS page 1; 116 (2525) page 2 ("MORE STATISTICS"); 118/120/122
(2593/2660/2734) the three histograms; 127 (2941) SOUND TEST; 132 (3155) MUSIC
TEST; 139 (3364) MODIFY CONTROLS; 145 (3572) CONFIGURE KEYBOARD; 150..152
(3784..3787) TEST CONTROLS (no DIAGS rows); 157..160 (3993..4003) ADJUST
VOLUME, with `- LOADING -` at 158 (3996); 165/166 (4203/4207) 2 PLAYER
HANDICAP; 168 (4278) OPTIONS MENU; 170 (4345) the final MAIN MENU.

## §A.5 The walk oracle (Task 10)

First run (`/tmp/named-gaps-a/k11w`, the generated script: `enter_frame 324`,
`enter_state 0000`, 37 `key` lines, 11 `pad` lines): the driver passed (`all
checks passed`); the compare reported `window empty (start 90, end None)`,
`settled screens exhibited 28/38; missing [10, 19, 34, 39, 44, 55, 61, 69,
75, 77]` and three unexplained capture frames 138, 151, 158. Triage:

1. **The ten missing screens — a tool defect, fixed.** Each is byte-identical
   to a capture frame (`port 10 == capture 92`, `19 == 105`, `34 == 110`, `39
   == 125`, `44 == 130`, `55 == 143`, `61 == 148`, `69 == 155`, `75 == 163`,
   `77 == 92`), but also identical to an earlier port frame (the MAIN MENU and
   the OPTIONS MENU are redrawn), and `explain()` names only the first
   identical port frame. `k11_compare.canonical` credits identical port frames
   together, for the coverage claim and the window end. Test
   `test_repeated_screen_is_credited` (red before the fix: `FAIL:
   test_repeated_screen_is_credited`; green after).
2. **Capture 158 (raw 3996), rows 192..197, x 0..85 — a driver gap, fixed.**
   It is ADJUST VOLUME with the loader's `- LOADING -` text at (0, 192)
   (`res_load_present`, text 0x1E9 at `0x1B3EA`). The port draws it too, but
   between two pumps; the K11 driver now dumps it through
   `res_set_screen_hook`, as the front-end driver does. Mutation (the hook not
   set): `k11_compare: walk: FIRST UNEXPLAINED capture 158 (raw 3996): nearest
   port 64, differs in rows 192..197, x 0..85 (166 px)`.
3. **Captures 138 (raw 3363) and 151 (raw 3786) — allowed by name (C4).**
   Pixel analysis: capture 138 equals port 47 or 48 at 63060 of 64000 pixels
   and is black (`000000`) at the other 940 (rows 91..158), where port 48 has
   MODIFY CONTROLS' new glyphs; capture 151 equals port 58 or 59 at 63912 and
   is black at 88 (rows 120..152), exactly TEST CONTROLS' red markers of port
   59. Both are a glyph screen presented mid-draw with its new glyphs' palette
   not yet in the presented DAC; the port presents at the loop's wait points
   only (`0x2EA74`), so no port frame holds that state. The shape is the
   front-end oracle's allowed frame 833 (the presented DAC state,
   fidelity-gaps §7.11); it is not a service-menu drawing defect.
   `K11_ALLOWED_UNEXPLAINED['walk'] = {138, 151}` with these reasons; test
   `test_allowed_name_admits_only_its_index` (mutation `bad` ignoring the index:
   `FAIL: test_allowed_name_admits_only_its_index`).

No C1 (run clock) or C2 (input) divergence appeared: STATISTICS page 1's
cells match (the pinned copy's play-time fields are 0 at the Enter), and the
screens follow the capture key for key.

**The oracle can fail** (Step 5): frame 2 of the dump (the first settled
screen) with one byte flipped gives `settled screens exhibited 37/38; missing
[2]` and exit 1; the rerun restores exit 0.

**Baseline lines** (`make verify`, gate t10, 2026-09-30):

```
k11_compare: walk: window distinct [90..170] (raw 1746..4345)
k11_compare: walk: 81 frames in window: 47 clean, 2 splice, 0 transition, 2 unexplained, 30 all-black
k11_compare: walk: settled screens exhibited 38/38; missing []
k11_compare: walk: allowed by name: [138, 151]
k11_compare: walk: 0 unexplained in the window
```

Gate t10: `verify-exit=0`, `ORACLES-EQUAL`, `DUMPS-IDENTICAL`,
`WAV-IDENTICAL`.

