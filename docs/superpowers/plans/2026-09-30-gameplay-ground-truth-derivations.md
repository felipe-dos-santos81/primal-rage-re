# Gameplay ground truth (U1–U4): derivation record

Spec: `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md`.
Plans: `docs/superpowers/plans/2026-09-30-gameplay-u{1,2,3,4}-*.md`. Every
claim cites a raw address, a `poll.log` line
(`data/k11-captures/<scenario>/poll.log:<n>`) or a capture frame
(`<scenario> frame <i> (raw <r>)`). On any conflict the raw wins; corrections
are recorded here with their address.

## §G.0 Baseline (U1 Task 0)

Commands (the plan's Task 0, run in the worktree `.worktrees/gameplay-u1` on
branch `gameplay-u1` from `gameplay-ground-truth`; `data` and `.superpowers`
are symlinks to the main checkout's, the two git-ignored test fixtures
`port/tests/ghidra_data.bin` and `port/tests/title_screen_ref.ppm` copied from
it). `make verify` ran with the per-agent overrides `SMK_DUMP=/tmp/pr_u1_smk
TITLE_DUMP=/tmp/pr_u1_title ATTRACT_DUMP=/tmp/pr_u1_att FRONTEND_DUMP=/tmp/pr_u1_fe
TITLE_PIN_DIR=/tmp/pr_u1_pin AUDIO_WAV=/tmp/pr_u1.wav K11_DUMP=/tmp/pr_u1_k11`
(other agents run gates concurrently; the defaults are shared /tmp paths).

- HEAD `ff686d6dc13460cdefa7440ef0f59c692781cd38` (docs: gameplay ground-truth plans U1-U4).
- `verify-exit=0`.
- Oracle lines: `grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)'`
  → `/tmp/gameplay-u1/or_base.txt`, 45 lines, sha256
  `eaca80cf8d4a3bffe980472ea110ddf4bf038975003ed9ec6a76da6d5a92454a`; identical
  (`diff`, no output) to the controller's baseline
  `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`.
  (Correction to the plan's Step 2 grep: the gate's pattern includes
  `== demo-fight` and keeps the K11 lines apart; the K11 lines are
  `/tmp/gameplay-u1/k11_base.txt`, 12 `k11_compare:` lines: walk `81 frames in
  window: 47 clean, 2 splice, 0 transition, 2 unexplained, 30 all-black`,
  `0 unexplained in the window`; menuesc `283 frames in window: 184 clean, 92
  splice, 3 transition, 0 unexplained, 4 all-black`, `END 388 must be >= 388: ok`.)
- K11 tool unit tests: `Ran 40 tests` (the verify line for
  `tools.tests.test_k11_{fields,session,capture,compare}`).
- `python3 tools/port_progress.py`: `771 1203 64` and
  `731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)`.
- `shasum -a 256 data/game/C/PRAGE.EXE`:
  `eecba701576d36d1a217a271acd90e9aa4473121db8d51e8c8085c194ce0e91b`.
- CMOS: `2040 0` (2040 bytes, none non-zero: the defaults path).
- `make title-pin TITLE_PIN_DIR=/tmp/pr_u1_pin`: the pinned copy's sha256
  `8120f1bd1df389ed94cb329c030f95d9e38caad193bbd9840717af557161a68d` (as the plan expects).
- `dosbox-x -version`: `DOSBox-X version 2026.08.31 SDL2, copyright 2011-2026 The DOSBox-X Team.`

## §G.1 Raw facts (U1 Task 1)

Method: the scratch disassembler `/tmp/gameplay-u1/dx.py` (the plan's Task 1
Step 1, capstone on `data/game/C/PRAGE.EXE`, obj-0 file offset = VA +
`0x52E54`). Its data displacements are **pre-fixup** (VA − `0x80000`):
`[0x6f6dc]` is `DS_000EF6DC`, `[0x81508]` is `DS_00101508`. Data addresses
were cross-checked in `port/decomp/prage.c` (Ghidra, fixups applied).

### §G.1.1 The master loop, the frame counter and the pad sampler

`python3 dx.py 255CC 120` (verbatim excerpts):

```
000255D4  891508150800           mov dword ptr [0x81508], edx
000255DA  a108150800             mov eax, dword ptr [0x81508]
000255DF  a30c150800             mov dword ptr [0x8150c], eax
000255EE  e8d1aa0200             call 0x500c4
0002560B  e84cf6ffff             call 0x24c5c
00025632  66ff05f44a0800         inc word ptr [0x84af4]
00025639  e882defeff             call 0x134c0
0002563E  a10c150800             mov eax, dword ptr [0x8150c]
00025643  3b0508150800           cmp eax, dword ptr [0x81508]
000256B1  e826810300             call 0x5d7dc
000256B6  e86578ffff             call 0x1cf20
000256C0  ff050c150800           inc dword ptr [0x8150c]
000256C6  a10c150800             mov eax, dword ptr [0x8150c]
000256CB  48                     dec eax
000256CC  3b0508150800           cmp eax, dword ptr [0x81508]
000256D2  7509                   jne 0x256dd
000256D4  89e8                   mov eax, ebp
000256D6  e801810300             call 0x5d7dc
000256DB  ebe9                   jmp 0x256c6
000256DD  803da881020000         cmp byte ptr [0x281a8], 0
000256E4  0f8404ffffff           je 0x255ee
```

`python3 dx.py 24C5C 90`:

```
00024C63  66a1004b0800           mov ax, word ptr [0x84b00]
00024C69  83f827                 cmp eax, 0x27
00024C6C  7405                   je 0x24c73
00024C6E  e8d1a90200             call 0x4f644
00024CCD  668b3ddcf60600         mov di, word ptr [0x6f6dc]
00024CD4  47                     inc edi
00024CDB  66893ddcf60600         mov word ptr [0x6f6dc], di
```

`python3 dx.py 1BDF4 3C` (the timer ISR body):

```
0001BDF8  a0224b0800             mov al, byte ptr [0x84b22]
0001BDFD  83f801                 cmp eax, 1
0001BE00  742b                   je 0x1be2d
0001BE0E  42                     inc edx
0001BE0F  43                     inc ebx
0001BE10  891508150800           mov dword ptr [0x81508], edx
0001BE16  891d00150800           mov dword ptr [0x81500], ebx
0001BE1C  e88bfdffff             call 0x1bbac
0001BE21  66ff05def60600         inc word ptr [0x6f6de]
0001BE28  e8ff170100             call 0x2d62c
```

All as the plan expected. Consequence (spec §3.1): in one iteration the pad
sampler `0x500C4` (`0x255EE`) runs **before** the frame counter
`DS_000EF6DC` is raised (`0x24CDB`, inside `0x24C5C` called at `0x2560B`).
The ISR raises `DS_00101508` (`0x1BE10`) and then samples the IRQ1 key-state
table into the bitmap (`0x1BE1C call 0x1bbac`) in the same interrupt, so the
tick that releases the spin `0x256C6..0x256DB` also leaves the bitmap the next
iteration's `0x500C4` reads. While the loop spins
(`[DS_0010150C] − 1 == [DS_00101508]`, `0x256C6..0x256CC`) iteration `f` is
complete and the next has not begun: the consistent snapshot point. A key-state
write made in the spin of `f` is sampled by the next tick and read by
iteration `f + 1` (the one that raises the counter to `f + 1`).

`0x500C4` (`prage.c:36695`, fixups applied): `DAT_000e1c30 =
(+0x2d8 << 24) | (+0x2d9 << 8)`; `DAT_000e1c34 = ~(old ^ new) & new | (old ^
new) & DAT_000e1c34` (a bit that changed keeps its previous level: the level
lags the raw word by one iteration). `0x4F644` (`prage.c:35951`) writes
`DAT_001088e4` (new), `DAT_001088d8` (held `& 0xff00ff00`), `DAT_001088e0` (P1)
and `DAT_001088e2` (P2). These are the `raw`, `pad`, `new`, `held`, `e0`, `e2`
fields of `gp_session.SNAP_FIELDS`.

### §G.1.2 The pad bits

`port/decomp/prage.c:7310..7420` (Ghidra): `FUN_0001b610` tests
`[DAT_00101514 + 0x254 + [+0x2de..+0x2e1]]` bit 7 clear into `0x80 0x40 0x20
0x10`; `FUN_0001b730` `+0x2e2..+0x2e5` into `1 2 4 8`; `FUN_0001b850` returns
`([+0x28f] & 0x80) == 0` (bit 0; `+0x28F = 0x254 + 0x3B`, F1). `FUN_0001b6a0`,
`FUN_0001b7c0`, `FUN_0001b870` are the same with `+0x2e6..+0x2ed` and `+0x290`
(`0x254 + 0x3C`, F2). Ghidra lost the register flow of the combine in
`FUN_0001bbac`, so it was re-read raw (`dx.py 1BCF0 F0`): device 0 jumps to
`0x1BD0E call 0x1b610`, then `0x1BD41 mov dl, al; call 0x1b730; mov dh, al;
call 0x1b850; mov ah, dl; or ah, dh; … or al, ah; 0x1BD5B mov byte ptr [edx +
0x2d8], al`, and for P2 `0x1BD84 call 0x1b6a0 … 0x1BDD0 call 0x1b7c0 … call
0x1b870 … [eax + 0x2d9]`. So `+0x2D8 = b610 | b730 | b850` and `+0x2D9 = b6a0
| b7c0 | b870`, as spec §3.2.

The config words (`python3 -c …` on file offset `0xA2C62 + 0x46E54`):
`['0x0', '0x1f73', '0x2d78', '0x2c7a', '0x2e63', '0x1675', '0x1769', '0x316e',
'0x326d', '0x0', '0x4800', '0x5000', '0x4b00', '0x4d00', '0x4700', '0x4900',
'0x4f00', '0x5100', '0x64', '0x64']` — the high bytes are the scan codes the
planner's probe read at `+0x2DE..+0x2ED` (`probe.log` line 3:
`2d4..2ed=…1f2d2c2e1617313248504b4d47494f51`).

The table (spec §3.2, verbatim; kb word = `(+0x2D8 << 8) | +0x2D9`):

| name | P1 key (scan) | P2 key (scan) | P1 kb bit | P2 kb bit |
|---|---|---|---|---|
| up | S (`0x1F`) | Up (`0x48`) | `0x8000` | `0x0080` |
| down | X (`0x2D`) | Down (`0x50`) | `0x4000` | `0x0040` |
| left | Z (`0x2C`) | Left (`0x4B`) | `0x2000` | `0x0020` |
| right | C (`0x2E`) | Right (`0x4D`) | `0x1000` | `0x0010` |
| b0 | U (`0x16`) | Home (`0x47`) | `0x0100` | `0x0001` |
| b1 | I (`0x17`) | PgUp (`0x49`) | `0x0200` | `0x0002` |
| b2 | N (`0x31`) | End (`0x4F`) | `0x0400` | `0x0004` |
| b3 | M (`0x32`) | PgDn (`0x51`) | `0x0800` | `0x0008` |
| start | F1 (`0x3B`) | F2 (`0x3C`) | `0x0100` | `0x0001` |

Status: raw-derived. The only capture evidence before U1 is the K11 walk's
Down tap, `kb=0040` (record named-gaps-a §A.4, `walk/poll.log:1533`). U1 Task 8
(§G.7) measures the rest.

### §G.1.3 START MENU

`rg` on `port/src/game/svcmenu.c`: the seven setters `0x2CBC4` (LEFT PLAYER
ARCADE, line 96), `0x2CBDC` (104), `0x2CBF4` (112), `0x2CC0C` (120), `0x2CC24`
(128), `0x2CC3C` (136), `0x2CC54` (144), registered at lines 1631..1637. The
K11 record §0.2 (`2026-09-29-k11-service-menu-derivations.md:60..72`) gives the
table `0xBCCCC` rows `BCCDC … BCD3C` in that order with the modes `0x2D 0x2E
0x28 0x29 0x2A 0x2B 0x2C` and subs `0 0 1 1 2 3 4`: spec §3.3 holds. The
cursor starts on row 0 (K11 record §K11.2). So Enter (mode 3 → `0x27`, MAIN
MENU on "Start"), Enter (START MENU), Enter (LEFT PLAYER ARCADE) stores mode
`0x2D`.

### §G.1.4 Credits

`dx.py 25071 5D` (mode `0x2D`'s case): `0x25071 mov eax, 0x29; call 0x2d974`
… `0x250AF mov eax, 1; … 0x250BA call 0x2ca7c; 0x250BF mov eax, 1; 0x250C4
call 0x257a4; 0x250C9 jmp 0x2540f` — one credit spent (`0x2CA7C(1)`) and side
0 diverted (`0x257A4(1)`). `dx.py 2C304 18`: `mov eax, 0x29; call 0x2d974; and
eax, 0xf0000; sar eax, 0x10; inc eax; mov dword ptr [0x85c00], eax` —
`DS_00105C00 = ((field 0x29 & 0xF0000) >> 16) + 1` (`prage.c:17055` the same).
`prage.c:17361`: the credit test is `DS_00105D60 != 0 || DS_00105C00 != 0`
(FREE PLAY or a credit). Spec §3.4 holds.

### §G.1.5 The planner's probe (not reproduced; U4 Task 2 re-measures it with the harness)

Kept log: `.superpowers/sdd/2026-09-30-gameplay-scope/planner-probe/probe.log`,
14 661 lines, sha256
`37785064bbe9b1c8dec31ff532f555e4bec1f86401d7417a28e1380f42258e2c` (checked
2026-09-30, equal to the spec's), with its script `gp_probe.py`. Copied from
spec §3.5–§3.7:

- Probe: the pinned copy (`title_pin.py`, sha256 `8120f1bd…a68d`), DOSBox-X
  2026.08.31, no video, `-time-limit 300`, zero CMOS; a poller thread on the
  memory file (base `0x266000`) logging one snapshot per `f` in the spin state
  and every mode change; injected Enters at 25 s (mode 3), mode `0x27` + 2.5 s,
  + 5.0 s; nothing else.
- First `f` per mode: `0x11D` mode 3 (the Enter queued); `0x120` `0x27` MAIN
  MENU (`ent = 0x2A2BEC`); `0x1B3` Enter queued, START MENU from `0x1B6`
  (`ent = 0x2A2CDC`); `0x246` Enter queued; `0x247` `0x2D` (credits 5 → 4 at
  `0x248`); `0x248`, `0x25C` `0x1A`, `0x1B` (the wipe); `0x26C` `0x10`
  character select (P1 idle for 916 frames); `0x600..0x728` `0x1A 0x1B 0x11
  0x17 0x1A 0x1B`; `0x73A` 5 round start; `0x7B5` 6 round 1 (P2 command word
  `0x2020`, P1 `0`); `0xEDC` 8 round end (P1 `+0x5A = 0x78`); `0xF8E`, `0x107F`
  `0x16`, 5; `0x10F9` 6 round 2 (3 249 frames); `0x1DAA` 7 match end (P1
  `+0x5A = 0x44`, P2 `0`); `0x1E19`, `0x1E1C` 9, `0x17`; `0x1F0B` `0x15`;
  `0x1F84` `0x13` the challenge screen; `0x2200`, `0x2204`, `0x2207` `0x1E`,
  `0x14`, `0x17` game over; `0x22BA` 3 attract (boot movies follow).
- Mode 3 at `f = 0x22BA` came 169.9 s after the poller started; the match runs
  at 60.0 frames/s (`0x7B5 → 0xED9`: 1 828 frames in 30.45 s). From the first
  Enter to mode 3 is 8 605 iterations.
- BIOS consumption latency (§3.6): a key queued in the spin of `f` was
  consumed one to three iterations later (the mode-3 Enter queued at `0x11D`
  took effect at `0x120`; the START MENU Enter queued at `0x246` at `0x247`).
  Cause not established (spec §7 Q3).
- Snapshot coverage (§3.7): 14 602 distinct `f` in `0x4..0x3946`; 57 frames
  missed in 25 gaps, every gap at a load or a mode change. `DS_00101508` /
  `DS_0010150C` restart at 1/2 at some transitions (`0x121`, `0x248`); the spin
  predicate is relative and unaffected.
## §G.2 gp_session: constants, pads, log format (U1 Task 2)

`tools/gp_session.py` (stdlib): `DATA_BASE_VA`, `KB_PTR_DS = 0x101514`,
`KEYTAB_OFF = 0x254`, `BDA_HEAD/TAIL = 0x41A/0x41C`, `SNAP_FIELDS` (the
spec §4.1 `S` fields; addresses re-checked against `prage.c` `0x500C4` /
`0x4F644` / `0x2C304` in §G.1), `TRACE_FIELDS`, `KEYS`, `PAD`, `raw_to_kb`,
`format_s`, `parse`. Harness values, named as such: `ENTER_WAIT = 25.0` s (the
K11 boot wait, `k11_session.ENTER_WAIT`) and `HOLD_FRAMES = 3` (AUTOTYPE's
3-tick press, record named-gaps-a §A.9); neither is a game value.

`PAD`'s scans and kb bits are §G.1.2's table. Its BIOS words: the letters and
P2's Home/PgUp/End/PgDn are the config words at `0x122C62` (§G.1.2: `0x1F73
0x2D78 0x2C7A 0x2E63 0x1675 0x1769 0x316E 0x326D`, `0x4700 0x4900 0x4F00
0x5100`); P2's arrows the grey-key `E0` form AUTOTYPE's taps left in the ring
(record named-gaps-a §A.9: `48E0`, `50E0`; left/right the same form); F1/F2 the
standard make words `0x3B00`/`0x3C00`. None of these words is acted on by the
int 16h key loop `0x24D08..0x24EE7` (`flow.c game_key_loop`): it acts on ascii
`0x0D`, `0x1B`, `0x20`, and with ascii 0 only on scans `0x10`, `0x1F`, `0x24`,
`0x32`; every letter word here has a non-zero ascii byte, and the ascii-0 words
(`0x3B00 0x3C00 0x4700 0x4900 0x4F00 0x5100`) hit its `default`. Each still
latches `DS_00105F30` (`0x24D4D`).

**Spec §7 Q4 — ruled YES (user decision, delegated through the controller,
2026-09-30):** a pad press also queues its key's BIOS word once, like a real
keyboard's make code; typematic repeat is not modelled; the
`gp_capture --no-pad-bios` switch is kept (it turns the pad words off, the
BIOS keys `enter`/`esc` still queue).

Tests: `python3 -m unittest tools.tests.test_gp_session -v` → `Ran 6 tests … OK`
(before the module existed: `ModuleNotFoundError: No module named 'gp_session'`).
Mutation: `p1.b0`/`p1.b1` kb bits swapped in `PAD` →
`FAIL: test_pad_bits_follow_the_isr_sampler … AssertionError: 512 != 256 : b0`;
restored → `OK`. (A stale `tools/__pycache__` hid the restore once — same size,
same second; the proofs after this one run with `PYTHONDONTWRITEBYTECODE=1`.)
## §G.3 The scheduler (U1 Task 3)

`gp_session.Schedule`, `expand`, `SCENARIOS['gp-pads']`. An action for frame
`F` fires at the first spin snapshot with `f >= F − 1`: the key-state write
made in the spin of `F − 1` is sampled by the next ISR tick (`0x1BE1C`) and
read by iteration `F`'s `0x500C4` (§G.1.1). Modes seen before the `boot` step
fires are not recorded, so the attract's mode 3 cannot satisfy a later
`until_mode 3`. The `gp-pads` scenario's gaps (120 frames after mode `0x27`,
then 30, the chord held 5, the end 60 after it) and its `time_limit = 75` are
harness values (a stimulus spacing, not game values).

Tests: `Ran 11 tests … OK` (before: `AttributeError: module 'gp_session' has no
attribute 'Schedule'`). Mutation: `f < F - 1` → `f < F` in `due` →
`FAIL: test_after_fires_at_the_spin_before_its_frame` and
`FAIL: test_until_mode_counts_only_after_the_last_action` (`FAILED
(failures=2)`); restored → `OK`.
## §G.4 Port script v2 and trace diff (U1 Task 4)

`gp_session.port_script`, `snapshots`, `trace_diff`, `ScriptError`, the CLI
`port-script` / `trace-diff`, as spec §4.1.

**Correction to the plan (raw wins): a chord's BIOS words are consumed in one
iteration.** The int 16h key loop `0x24D08..0x24EE7` (`dx.py 24CFE 60`:
`0x24D08 mov ah, 1 … 0x24D0E int 0x16 … 0x24D20 je 0x24eec` exits only when no
key is queued; otherwise `0x24D2E int 0x16` (AH = 0) reads one and the loop
repeats; `flow.c game_key_loop`) drains every queued word before the mode
switch. With spec §7 Q4 ruled YES a chord of `n` pads queues `n` words at once,
so they are all consumed in the same iteration and the head moves by `2n` in
one step. The plan's generator would have seen one head change for `n`
presses ("n BIOS words queued, 1 consumed"). Two changes, both pinned by the
new test `test_a_chord_is_consumed_in_one_iteration`:

1. `gp_capture`'s poller writes **one `H` record per consumed word** (walking
   the ring from the old head to the new one; each record carries the head
   after that word) — §G.5.
2. `port_script` requires `S(c − 1).head ≠ H.head` for each key (as planned) and
   `S(c).head` = the head of the **last** `H` record of frame `c` (instead of
   the key's own `H.head`; identical for a single key). A word consumed after
   `S(c)` was taken (a blocking loop with `f` frozen at `c`) still fails.
3. Keys of one frame keep their press (FIFO) order in the script: the plan's
   `sorted(ev)` would have ordered same-frame keys by their text
   (`key 301 17 69` before `key 301 1F 73`); the sort key now carries the
   press index.

Tests: `Ran 18 tests … OK` (the plan's 17 plus the chord test; before:
`AttributeError: module 'gp_session' has no attribute 'port_script'`).
Mutations (each restored → `OK`):
(a) drop `prev is None or` → `ERROR: test_unpinned_key_is_an_error`;
(b) drop the `f - 1 not in snap` check → `FAIL: test_bits_need_the_previous_snapshot`;
(c) compare `S(c).head` with the key's own `H.head` →
`ERROR: test_a_chord_is_consumed_in_one_iteration` (ScriptError);
(d) the plan's sort without the press index →
`FAIL: test_a_chord_is_consumed_in_one_iteration`.
## §G.5 gp_capture (U1 Tasks 5–6)

### §G.5.1 Snapshot, spin predicate, injector (Task 5)

`tools/gp_capture.py`: `read_snap` (the `SNAP_FIELDS` at `base + ds −
0x80000`), `spinning` (`([DS_0010150C] − 1) & 0xFFFFFFFF == [DS_00101508]`,
the `0x256C6..0x256CC` predicate), `consistent` (two reads agree on `f` and
`t508`: no ISR tick and no iteration between them), `Injector` (the key-state
byte `[ptr + 0x254 + scan]` bit 7 cleared, released at the spin of
`f + hold − 1` so iterations `f + 1 .. f + hold` sample it; the BIOS word
queued once through `k11_capture.bios_insert` for a key, and for a pad unless
`pad_bios=False`), and `ring_steps` (new, §G.4: the head after each word
consumed between two head reads, wrapping at the BDA ring end).

Tests: `python3 -m unittest tools.tests.test_gp_capture` → `Ran 6 tests … OK`
(the plan's 5 plus `test_ring_steps_one_head_per_consumed_word`; before:
`ImportError: Failed to import test module: test_gp_capture`). Mutations (each
restored → `OK`): `release_due` with `f > h[0]` →
`FAIL: test_press_holds_and_queues_once`; `consistent` without the `t508`
compare → `FAIL: test_consistent_rejects_a_moving_counter`; `ring_steps` with
`h > end` → `FAIL: test_ring_steps_one_head_per_consumed_word`.
## §G.6 Make targets (U1 Task 7)
## §G.7 The gp-pads capture (U1 Task 8)
## §G.8 U1 closure (U1 Task 9)
