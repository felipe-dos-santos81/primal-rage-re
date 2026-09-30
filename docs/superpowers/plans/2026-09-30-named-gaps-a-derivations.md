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

## §A.6 G1 evidence: the idle timeout and the MAIN MENU Esc (Task 11)

Captures `data/k11-captures/idle` (keys `1/1`, `time_limit=90 wall_s=90.7`)
and `data/k11-captures/menuesc` (keys `2/2`, `time_limit=60 wall_s=60.6`),
both `--input inject`, every `CHECK ok`.

> **G1, idle timeout (`0x2EBB3`).** The Enter is `idle/poll.log:1405` (`K
> ms=25002 tick=00000571 … key=1C0D`); mode `0x27` from `poll.log:1406`. The
> stamp `DS_00105F2C` is `0x572` from then on. At `poll.log:2615` (tick
> `0xA22`) `tick − DS_00105F2C = 0x4B0`, not above `0x4B0` (the `0x2EB9F`
> `jbe`), and `menu=01`. At `poll.log:2616` (tick `0xA23`, the difference
> `0x4B1` > `0x4B0`, latch `DS_00105F30 = 0`) the original has cleared
> `DS_00107414` (`menu=00`, the `0x2EBA8` store) and longjmped to the setjmp at
> `0x20C1F`: the next changed poll, `poll.log:2618` (tick `0xA25`, `ms=45072`),
> reads `mode=0003 st=0000 f=05F4` — mode 3, attract state 0; the master frame
> counter `DS_000EF6DC` is not reset (`0x5F3` → `0x5F4`). The attract then runs
> from state 0 (`st=0001` at `poll.log:4177`, `ms=71072`). The screen holds the
> MAIN MENU (`idle frame 106 (raw 1750)`) until `idle frame 107 (raw 3150)`,
> all black, then `idle frame 108 (raw 3154)`, the TWI5 logo movie, **equal to
> `data/title-captures/frontend/frame_1887.raw`** — the frame the front-end
> capture shows when the attract restarts after the first demo (its first
> all-black frame is 1885). The whole boot sequence follows: TWI5, TWG, the
> title, the intro ("THE FUTURE…", "WHO WILL RULE THE NEW URTH?"), `idle frames
> 108..1180`; 528 of the distinct frames from 108 on are byte-equal to
> frames of `data/title-captures/{title,frontend}`.

Timing: from the Enter's stamp (tick `0x572`) to the store (tick `0xA23`) is
`0x4B1` ticks = 1201 ticks, 20.0 s at 60 Hz; the wall clock agrees
(`ms=25012` → `ms=45036`, 20.02 s).

> **G1, MAIN MENU Esc (`0x2520B`).** The Esc is `menuesc/poll.log:1590` (`K
> ms=28003 tick=00000628 f=01F8 key=011B`). At `poll.log:1593` (tick `0x62B`,
> `f=01FB`) `menu=00` and the stamp `DS_00105F2C = 0x62B` (the menu step took
> the key: `0x2EE04..0x2EE0B` stamps on a non-zero input); at
> `poll.log:1596` (tick `0x62C`, `f=01FC`) `mode=0003 st=0000`. **Correction to
> the plan:** it expected "no `menu` store" on this path; the capture shows
> `DS_00107414` cleared one frame before the mode change. `0x251F3..0x2520B`
> itself stores nothing, so the clear comes from inside the menu step before
> it returns; the instruction is not pinned here (named for B). The screen: `menuesc frame 108 (raw 1750)` the MAIN MENU until
> `frame 109 (raw 1960)` all black, then `frame 110 (raw 1964)` the TWI5 logo,
> equal to `data/title-captures/frontend/frame_1887.raw` — the same soft
> restart as the idle timeout.

`make k11-report` (report-only; the port's script ends 180 ticks after the last
key, so the port never reaches the restart): `k11_compare: idle: window
distinct [104..106] (raw 1743..1750)`, `3 frames … 0 unexplained, 1
all-black`, `settled screens exhibited 1/1`; `k11_compare: menuesc: window
distinct [106..108] (raw 1745..1750)`, `settled screens exhibited 2/2`, `0
unexplained`. The port keeps the store and not the longjmp (`config.c`), and
its master spin does not advance `DS_00101500` (spec §8), so the timeout does
not fire there: B's G1 target is the restart shown above (black, then the
boot logos from TWI5 on, attract state 0, the frame counter kept).

## §A.7 G2 evidence: the 0xFFE80003 read (Task 12)

`data/k11-captures/diags` (keys `12/12`, `time_limit=55 wall_s=55.6`,
`E … reason=time-limit`): `diags/poll.log:1417` `W ms=25076 ds=00107410
linear=002ED410 old=00 new=10`, and `diag=00000010` from `poll.log:1418` to
the end. **The memory-file poke is visible to the guest**: the TEST CONTROLS
screen draws the DIAGS rows (`0x3236C..0x323D2`), so plan fallback F1 is not
needed. No DOS/4GW line in `dosbox.log`.

> **G2 (`0x32573..0x32578`).** Unreachable in the stock game (§A.1.1: field
> `0x2A` is 4 bits, descriptor `0x1B80`; `DS_00107410 = field & ~3`,
> `0x2FA1C`). With `DS_00107410 |= 0x10` poked (`diags/poll.log:1417`), the
> original under DOS/4GW + DOSBox-X **does not fault**: it draws `000000FF`
> for the byte at linear `0xFFE80003` (`diags frame 111 (raw 2383)`, held to
> raw 2663; against the port's `00000000` the report box is `rows 47..52, x
> 152..166 (42 px)`, the last two digits). The row above (`0x32558`, the pad
> word) reads `00000000` in both. DOS/4GW runs with paging off (the `de`
> capture's register dump: `CR0: PG:0 … PE:1`) and a flat 4 GB data
> segment, so the read reaches physical `0xFFE80003`, which DOSBox-X returns as
> `0xFF` (no device there). On real hardware the byte is whatever the chipset
> maps at that address (commonly a BIOS-flash alias); this capture pins
> DOSBox-X's answer only.

The report also shows `diags frame 110 (raw 2382)` unexplained: TEST CONTROLS
mid-draw, 353 black glyph pixels (rows 40..152), otherwise equal to port 15/16
— the §A.5 C4 shape. `k11_compare: diags: settled screens exhibited 11/12;
missing [16]`: port 16 is the DIAGS screen with `00000000`.

**Recommendation for B:** the arm stays unreachable without a poke; if B
models it, the evidence supports "the read returns `0xFF`, no fault" for the
DOSBox-X target, not an abort.

## §A.8 G3 evidence: the 0x33458 #DE (Task 13)

`data/k11-captures/de`: the pokes `de/poll.log:1402..1405` (`W … ds=00105DB6
old=00 new=01`, `ds=00105DB8 new=FF`, `ds=00105DEA new=F0`, `ds=00105DEB
new=0F`), all inside `0x105DAF..0x105E2F`. Decoded from the first `F` record
after them: `{'0x6': '0x1', '0x8': '0xffff', '0xa': '0x0', '0x12': '0x0'}`
(fields 6 = 1, 8 = `0xFFFF`; the dividends `0xA` and `0x12` are 0). The C
cross-check: `k11_session: check-fields: de: tools/k11_fields.set_ matches
config_field_set over 129 bytes`, exit 0.

> **G3 (`0x334CD..0x334E2`).** With fields 8 = `0xFFFF`, 6 = `1`
> (`de/poll.log:1402..1405`, decoded above; the codec matches
> `config_field_set`), entering STATISTICS (the Enter at `poll.log:1657`,
> tick `0x65A`) **aborts to DOS**. `de/dosbox.log:15` `DOS/4GW Professional
> error (2001): exception 00h (divide by zero) at 180:002244E0`, then lines
> 16..28: `TSF32: prev_tsf32 6B24`; `SS 188 DS 188 ES 188 FS 0 GS 20`; `EAX 0
> EBX 0 ECX 30DD00 EDX 0`; `ESI A EDI 18 EBP 8 ESP 2F0F84`; `CS:IP
> 180:002244E0 ID 00 COD 0 FLG 246`; the CS/SS/DS/ES/FS/GS descriptors; `CR0:
> PG:0 ET:1 TS:0 EM:0 MP:0 PE:1 CR2: 0 CR3: 0`; `Crash address (unrelocated)
> = 1:000234E0`. Object 1 offset `0x234E0` is VA `0x334E0`, the `idiv ebx`;
> EBX = 0 (the divisor's low word), EAX = 0 (field `0x12`), EDI = `0x18`
> (row 2 of `0x326C4`, id `0x96`), ESI = `0xA` (the row). The code object's
> runtime base is `0x2244E0 − 0x234E0 = 0x201000`. DOS then runs the
> script's `EXIT` (`dosbox.log:29`), so the run ends early: `E ms=32319
> reason=exit` (`poll.log:1668`), `wall_s=32.6` of 55, keys `5/11` (the
> capture's `CHECK keys` fails by design). The last game frame is `de frame 99
> (raw 2033)`: the STATISTICS title over the backdrop, no row drawn yet; the
> ISR tick stops after `poll.log:1667` (tick `0x662`). `last_frame.png` is
> black (640×400; the message is in `dosbox.log`, so F3 was not needed).

This matches §A.1.4's prediction: no handler; DOS/4GW's default handler
prints the register dump and returns to DOS. **Correction to the spec:**
`0x33458` is called from STATISTICS page 1 (`0x331A6`, `svc_stats_rows`), not
page 2. Reachability in play: §A.1.2 (65536 one-player game ends with no
clear).

`make k11-report` for `de` (port script: the 5 received keys, the field
pokes at the Enter): `window distinct [88..99] (raw 1747..2033)`, `0
unexplained`, `settled screens exhibited 4/5; missing [12]` — port 12 is page
1 with row `0x96` drawn as 0, a screen the original never shows.

## §A.9 Fallbacks taken (Task 14)

- **F2, variant (input).** AUTOTYPE typed 11/38 and then 1/38 keys of the
  walk (§A.4) while the menu probes typed all theirs; the loss is on the host
  side and not repeatable. Instead of shortening the walk (F2 as written),
  `k11_capture.py --input inject` (the default) types the scenario through the
  memory file at AUTOTYPE's timing (`schedule`: the first key at `-w`, one
  `-p` per item): it clears bit 7 of the IRQ1 key-state byte
  `[DS_00101514]+0x254+scan` for `HOLD_S = 0.05` s (the hold AUTOTYPE's taps
  showed, `walk.run1 poll.log:1538..1541`, 3 ticks), and appends the BIOS word
  to the int 16h ring (`1C0D`, `011B`, `50E0`, `48E0` as the AUTOTYPE runs
  left them). Every scenario then received all its keys (walk 38/38, idle
  1/1, menuesc 2/2, diags 12/12; de 5/11 only because the game aborted).
  Tests: `test_schedule_follows_autotype_timing`,
  `test_bios_insert_appends_wraps_and_refuses_when_full`,
  `test_dosbox_cmd_without_autotype` (mutations: the ring wrap removed →
  `FAIL: test_bios_insert…`; `t += pace` → `t += 0` → `FAIL:
  test_schedule…`). What this does not exercise: the real keyboard
  controller and the game's IRQ1 handler, which AUTOTYPE's partial runs did
  (their keys produced the same `K` words and `kb` runs).
- **F1** not taken: the memory-file poke is visible (§A.7). **F3** not taken
  (the text is in `dosbox.log`). **F4** not taken (the timeout fired at 20 s).
  **F5** not taken (the base was found in every run).
- Two tool fixes found by the evidence runs (not fallbacks): `port_script`
  accepts a key prefix when the log ends `reason=exit` (the `de` abort;
  `test_port_script_accepts_a_key_prefix_only_after_an_exit`, mutation →
  `ERROR`), and the driver leaves game_loop() at the script's `end` through a
  `longjmp` to its own frame, because the `de` script ends inside STATISTICS
  page 1, a blocking loop no scripted key leaves (the first `de` report run
  spun there; the walk and the smoke dumps are unchanged by it).

## §A.10 Closure (Task 15)

### The paths

| path | reached by | evidence | oracle |
|---|---|---|---|
| MAIN MENU | walk | walk frames 92 (raw 1753), 170 (raw 4345) | k11-oracle |
| START MENU | walk | walk frames 94..98 (raw 1821..1961) | k11-oracle |
| CONFIG OPTIONS ("GAME OPTIONS") | walk | walk frame 107 (raw 2241) | k11-oracle |
| STATISTICS p1 / p2 / histograms 0..2 | walk | walk frames 113 (2455), 116 (2525), 118/120/122 (2593/2660/2734) | k11-oracle |
| SOUND TEST / MUSIC TEST (entered and left) | walk | walk frames 127 (2941), 132 (3155) | k11-oracle |
| MODIFY CONTROLS | walk | walk frame 139 (3364); 138 allowed by name (§A.5) | k11-oracle |
| CONFIGURE KEYBOARD | walk | walk frame 145 (3572) | k11-oracle |
| TEST CONTROLS (no DIAGS) | walk | walk frames 150/152 (3784/3787); 151 allowed by name (§A.5) | k11-oracle |
| ADJUST VOLUME, with `- LOADING -` | walk | walk frames 157..160 (3993..4003), LOADING at 158 (3996) | k11-oracle |
| 2 PLAYER HANDICAP | walk | walk frames 165/166 (4203/4207) | k11-oracle |
| TEST CONTROLS DIAGS `0xFFE80003` | diags (memory poke) | §A.7: `000000FF`, no fault, diags frame 111 (raw 2383) | report only (B) |
| idle timeout `0x2EBB3` | idle | §A.6: `idle/poll.log:2616..2618`, idle frames 106..108 | report only (B) |
| MAIN MENU Esc `0x2520B` | menuesc | §A.6: `menuesc/poll.log:1590..1596`, menuesc frames 108..110 | report only (B) |
| `0x33458` `#DE` | de (field poke) | §A.8: `de/dosbox.log:15..28`, de frame 99 (raw 2033) | report only (B) |
| quit prompt longjmp `0x24AB0` | not captured | reached from a game mode, not mode 0x27 (§A.1.3) | — |
| STATISTICS page 2 clear (Esc + Enter held) | not captured | the input (AUTOTYPE or the injector) types single taps, not chords (§A.2, §A.9) | — |
| SOUND/MUSIC TEST playback | not captured | no Enter is typed there; the DROs are kept, not compared (§A.4) | — |

No walk screen was dropped (F2 as written was not needed; §A.9).

### The answers (for sub-project B)

- **G1.** Both captured longjmps (the idle timeout `0x2EBB3` after `0x4B1`
  ticks, 20.0 s, and the MAIN MENU Esc `0x2520B`) restart the game the same
  way: mode 3, attract state 0, `DS_000EF6DC` kept, one black frame, then the
  boot sequence from the TWI5 logo (`idle frame 108` = `menuesc frame 110` =
  `data/title-captures/frontend/frame_1887.raw`). §A.6.
- **G2.** Unreachable in the stock game (§A.1.1). Under a poke, DOS/4GW +
  DOSBox-X read `0xFF` at linear `0xFFE80003` with no fault (paging off) and
  the screen shows `000000FF`. §A.7.
- **G3.** A zero low word in row `0x96`'s divisor (fields 8 + 6 = `0x10000`)
  aborts to DOS with `DOS/4GW Professional error (2001): exception 00h (divide
  by zero)` and the register dump; no handler is installed. §A.8.

### The oracle's claim

Every non-black capture frame between the first capture frame that exhibits
the port's first MAIN MENU and the final settled MAIN MENU is explained by the
port (clean, a splice of two adjacent port frames, or a transition row),
except the two frames allowed by name (§A.5, the presented-DAC gap of
fidelity-gaps §7.11), and every settled screen the port draws is present in
the capture (38/38). The claim does not cover the timing between keys, the
input path through the keyboard controller and the game's IRQ1 handler (the
capture injects keys, §A.9), the SOUND/MUSIC TEST audio, or anything after the
walk.

### Named gaps left by A

1. The mid-draw presented-DAC frames (walk 138, 151; diags 110): owner
   fidelity-gaps §7.11.
2. The walk's keys are injected through the memory file, not typed through
   the keyboard controller (§A.9); AUTOTYPE's key loss on this host is not
   explained.
3. `0xFFE80003` reads `0xFF` on DOSBox-X; real hardware is not captured.
4. The instruction that clears `DS_00107414` on the MAIN MENU Esc is not
   pinned (§A.6).

### Final gate (gate t15, after the last code commit `1ecc23b`)

`make verify` with the `na` overrides: `verify-exit=0`; `ORACLES-EQUAL` (the
45 base lines); `DUMPS-IDENTICAL`; `WAV-IDENTICAL`; the five `k11_compare:
walk:` lines equal §A.5's; `Ran 32 tests` for the k11 tools (fields 7, session
7, capture 9, compare 9). `python3 tools/port_progress.py`: `767 1203 64` and
`731 731 100 …`, unchanged (A ports no function; the README stays at 64%).
Assertion sites 13545 → 13557. The captures (`data/k11-captures/{walk, idle,
menuesc, diags, de}`, git-ignored) are 32, 218, 141, 22 and 19 MB.
