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
### §G.5.2 The poller, the run, the frames and the checks (Task 6)

`guard_gp` (`k11_capture.guard_out` plus a `gp-` basename), `write_frames`
(streams the chosen AVI frames to `frame_%05d.raw.gz`, gzip level 1, and
`window.txt`; stops decoding after the last wanted frame), `dosbox_cmd` (as
K11's, `DX-CAPTURE /V /O`, the memory file, `dos log console=quiet`),
`Poller`, `main`. Capture layout: `poll.log`, `frame_%05d.raw.gz`,
`window.txt`, `session.txt`, `dosbox.log`, `*.dro`, `last_frame.png`. Nothing
holds all frames: pass 1 keeps only an md5 per AVI frame.

Changes to the plan's code, each with its reason:

1. **The snapshot is closed on both sides (Review Focus 1).** The plan logged
   `v2` when `v` was spinning and `v`, `v2` agreed on `f` and `t508`. `v2`'s
   fields are read one after the other, and those read after its `t508` read
   (`raw` … `ent`, then the bitmap `kb` and the BDA head/tail) could still land
   after a tick that ends the spin (the ISR samples keys at `0x1BE1C` and the
   next iteration's `0x500C4` rewrites `raw`). `accept(v, v2, v3)` adds a third
   read `v3` after `kb`/head/tail and requires `v2`, `v3` to agree as well. Test
   `test_accept_needs_the_spin_across_both_reads`.
2. **One `H` record per consumed word** (§G.4), through `ring_steps`.
3. A self-check line: `kb != raw` counts the `S` records whose bitmap word
   (read in the spin) differs from `raw_to_kb(raw)`. In the spin no tick has
   happened since the one that fed this iteration's `0x500C4`, so the two must
   agree; a count above 0 would show a torn snapshot or a second sampler.
   Reported, not a CHECK (a blocking pump in a menu may legitimately differ).
4. `session.txt` also records `avi_frames` (the pass-1 count); the poller
   sleeps 5 ms while `[DS_00101514]` is still 0 instead of spinning.

Tests: `python3 -m unittest tools.tests.test_gp_capture tools.tests.test_gp_session`
→ `Ran 27 tests … OK` (9 gp_capture + 18 gp_session; the plan's 24 plus the
chord, ring and accept tests; before: `AttributeError: module 'gp_capture' has
no attribute 'guard_gp'` / `'accept'` / `'FRAME_BYTES'`). Mutations (each
restored → `OK`): `guard_gp` without the `gp-` check →
`FAIL: test_guard_requires_gp_prefix`; `accept` without `consistent(v2, v3)` →
`FAIL: test_accept_needs_the_spin_across_both_reads`; frames written raw
instead of gzip → `ERROR: test_frames_stream_to_gzip`.

## §G.6 Make targets (U1 Task 7)

`make gp-capture scenario=<gp-…> [GP_ARGS=…]` (after `k11-report`; in
`.PHONY`; depends on `title-pin`; `--exe $(TITLE_PIN_DIR)/PRAGE.EXE`, so a
`TITLE_PIN_DIR` override reaches it). `make help | grep gp-capture` prints
`gp-capture  Capture a gameplay scenario (scenario=gp-pads|gp-idle-loss; writes
data/k11-captures/)` (`gp-idle-loss` is U4's scenario; U1 defines only
`gp-pads`). `make verify`'s tool-test line now also runs
`tools.tests.test_gp_session tools.tests.test_gp_capture`.

Gate (`make verify` with the §G.0 overrides, `/tmp/gameplay-u1/t7_verify.txt`):
`verify-exit=0`; the oracle lines `diff` against `or_base.txt` → no output
(`ORACLES-EQUAL`); the 12 `k11_compare:` lines equal `k11_base.txt`
(`K11-EQUAL`); the tool-test line `Ran 67 tests` = the baseline's 40 + 27
(18 gp_session + 9 gp_capture).
## §G.7 The gp-pads capture (U1 Task 8)

Two runs of `make gp-capture scenario=gp-pads TITLE_PIN_DIR=/tmp/pr_u1_pin`
(the pinned exe `8120f1bd…a68d`, zero CMOS, `pad_bios=1`, DOSBox-X 2026.08.31,
`time_limit=75`, wall 75.8 s each, 5 256 AVI frames at 70.0866 fps). Run 1's
`poll.log` / `session.txt` / `window.txt` / console are kept in the ledger
(`.superpowers/sdd/2026-09-30-gameplay-u1-capture-harness/gp-pads-run1/`,
`poll.log` sha256 `78ad7a17…a462`); run 2 is `data/k11-captures/gp-pads/`
(`poll.log` 2 521 lines, sha256
`c43f4462a519adbff68a3c83acbb73db062f453338a470c2e67aba804b33649e`; 422 distinct
frames, raw 1372..5251; 11 MB). The analysis script is
`/tmp/gameplay-u1/analyse.py`; its run-2 output `/tmp/gameplay-u1/t8_run2_analysis.txt`.
(After review 1, `data/k11-captures/gp-pads` holds run 3, §G.8a; run 2's
`poll.log`, `session.txt` and `window.txt` are kept in the ledger's
`gp-pads-run2/`, and the script and its output in the ledger directory.)

### §G.7.1 Run 1 and the two corrections it forced

Run 1 (the plan's scenario: every pad held 2, the chord `p1.up + p1.b1 +
p2.left` held 5): every CHECK `ok` (`steps fired 20/20`, `port script v2: ok`),
`snapshots 2374, f 5..963, 25 frames missed; kb != raw in 0`. Its measurements:

1. **A hold of 2 was sampled by one iteration only.** Every pad showed `raw`
   in exactly one frame (e.g. `p1.up` pressed in the spin of `0x190`:
   `191:raw=8000`, `192:raw=0000`). The plan's `Injector` released at the spin
   of `f + hold − 1`, which contradicts spec §4.1 ("a hold of `n` releases at
   the spin of `F − 1 + n`", `F = f + 1`) and its own test comment ("held for
   iterations 0x101, 0x102"). **Correction (capture wins):** release at the
   spin of `f + hold`; the test now asserts the key still down after
   `release_due(0x101)` and up after `release_due(0x102)`; the old rule fails
   it (mutation `f + hold − 1` → `FAIL: test_press_holds_and_queues_once`).
   Commit `36ea83c`.
2. **The chord ended the MAIN MENU.** Its `raw = 8220` from `0x3AD`, the three
   words consumed together at `0x3AE` (§G.4's case), and mode 3 at `f = 0x3AF`
   (`P` record) after a snapshot gap `0x3AE..0x3B1`. Cause: `0x2FFC4`
   (`menu.c menu_step`) polls the pad **level** through `0x2EEC8` with the mask
   `0xC300C000`; `keys & 0x2000000` (P1 `b1`, kb `0x0200`) at `0x3041E` returns
   −1 for the MAIN MENU (flags 4), and case `0x27` (`0x251C6..0x25215`) turns any
   result other than 0/−5/−10 into the `0x2520B` longjmp restart. Because the
   release happens only at a spin snapshot, the gap also made the chord's
   release late (`release` logged at `f = 0x3B2`, so `raw = 8220` still at
   `0x3B2`). The single presses never reached the level (below), so the menu
   ignored them.

Harness change for run 2 (`SCENARIOS['gp-pads']`, harness values, commit
`36ea83c`): single pads held **1** (one sampled frame: the level
`DS_000E1C34` never takes it, since `0x500C4` keeps a changed bit's old level,
so the MAIN MENU does not act on `b0`/`b1`/`start`/up/down); the chord is
`p1.left + p1.b2 + p2.right` held 5 — bits outside `0xC300C000`, so it reaches
the level without a menu action.

### §G.7.2 Run 2: the pad map (spec §7 Q2 — closed)

`make gp-capture …` → `snapshots 2448, f 4..998, 5 frames missed (spec §3.7);
kb != raw in 0`; `CHECK base: ok`, `steps fired 20/20: ok`, `end frame reached:
ok`, `mode 0x27 after the Enter: ok`, `port script v2: ok`. The map (verbatim):

```
L445   p1.up     press f=1B8 late=0 first raw f=1B9 kb=8000 want=8000  S(f-1) raw=0
L478   p1.down   press f=1D6 late=0 first raw f=1D7 kb=4000 want=4000  S(f-1) raw=0
L511   p1.left   press f=1F4 late=0 first raw f=1F5 kb=2000 want=2000  S(f-1) raw=0
L544   p1.right  press f=212 late=0 first raw f=213 kb=1000 want=1000  S(f-1) raw=0
L577   p1.b0     press f=230 late=0 first raw f=231 kb=0100 want=0100  S(f-1) raw=0
L610   p1.b1     press f=24E late=0 first raw f=24F kb=0200 want=0200  S(f-1) raw=0
L643   p1.b2     press f=26C late=0 first raw f=26D kb=0400 want=0400  S(f-1) raw=0
L676   p1.b3     press f=28A late=0 first raw f=28B kb=0800 want=0800  S(f-1) raw=0
L709   p1.start  press f=2A8 late=0 first raw f=2A9 kb=0100 want=0100  S(f-1) raw=0
L742   p2.up     press f=2C6 late=0 first raw f=2C7 kb=0080 want=0080  S(f-1) raw=0
L775   p2.down   press f=2E4 late=0 first raw f=2E5 kb=0040 want=0040  S(f-1) raw=0
L808   p2.left   press f=302 late=0 first raw f=303 kb=0020 want=0020  S(f-1) raw=0
L841   p2.right  press f=320 late=0 first raw f=321 kb=0010 want=0010  S(f-1) raw=0
L874   p2.b0     press f=33E late=0 first raw f=33F kb=0001 want=0001  S(f-1) raw=0
L907   p2.b1     press f=35C late=0 first raw f=35D kb=0002 want=0002  S(f-1) raw=0
L940   p2.b2     press f=37A late=0 first raw f=37B kb=0004 want=0004  S(f-1) raw=0
L973   p2.b3     press f=398 late=0 first raw f=399 kb=0008 want=0008  S(f-1) raw=0
L1006  p2.start  press f=3B6 late=0 first raw f=3B7 kb=0001 want=0001  S(f-1) raw=0
L1039  p1.left   press f=3D4 late=0 first raw f=3D5 kb=2410 want=2000  S(f-1) raw=0
L1040  p1.b2     press f=3D4 late=0 first raw f=3D5 kb=2410 want=0400  S(f-1) raw=0
L1041  p2.right  press f=3D4 late=0 first raw f=3D5 kb=2410 want=0010  S(f-1) raw=0
```

Every one of the 18 names gives exactly its §G.1.2 kb bit, in the frame
`press f + 1`, with `S(f − 1) raw = 0`; the chord gives the OR `2410`. No
injection was late (0 of 21). Run 1 gave the same 18 bits (kb `8000 4000 2000
1000 0100 0200 0400 0800 0100 0080 0040 0020 0010 0001 0002 0004 0008 0001`,
all at `press f + 1`) and the chord `8220`. **F1/F2 are the start bits and
share bit 0 of their byte with U/Home (`b0`)** — raw-derived in §G.1.2, now
captured twice. Spec §3.2's table stands; no `PAD` change.

Timing (Review Focus 1–2), run 2: one-frame presses show `raw` in exactly one
frame and the level `pad` never (`1B9:8000/0000 1BA:0000/0000`, `raw/pad`
kb words); the chord held 5 shows `raw` in the five frames `0x3D5..0x3D9` and
the level in `0x3D6..0x3DA` (one iteration behind, including one frame after
the release: `3DA:0000/2410`), as `0x500C4`'s rule predicts (§G.1.1). The
consistency self-check `kb != raw` is 0 in both runs (2 374 and 2 448
snapshots): no torn snapshot.

### §G.7.3 BIOS consumption (spec §3.6, §7 Q3) and the menu

Every queued word was consumed and pinned (`presses with bios 22 H 22`, each
`S(H.f − 1).head` the old head and `S(H.f).head` the new one). The chord's
three words were consumed in the one iteration `0x3D5` (`H` records
`0026 0028 002A`, `S(3D5).head = 002A`), as §G.4 predicted from the key loop.
`H.f − I.f` (iterations from the queuing spin to the consuming iteration):
run 2 `{1: 8, 2: 7, 3: 5, 4: 2}`, run 1 `{1: 6, 2: 8, 3: 5, 4: 3}` — one to
**four** iterations (the planner's probe saw one to three), even on the MAIN
MENU where the key loop runs every iteration, while a key-state write is
always sampled by the next iteration. The cause stays open (Q3); the port
script keys each word at its observed consumption frame, so nothing depends
on it.

Menu effects: in run 2 the MAIN MENU entry `ent` stayed `0x2A2BEC` (the
"Start" row `0xBCBEC`) from `0x141` to the timeout, so neither the one-frame
pads nor their BIOS words (including `48E0`/`50E0`, whose ascii byte `E0` is
what the key loop latches at `0x24D4D`, not a scan `0x48`/`0x50`) moved the
cursor or selected anything. Mode 3 came back at `f = 0x88C` (tick `0xCBD`):
the idle timeout `0x2EBB3` (`config.c`, `tick − key_time > 0x4B0`, record
named-gaps-a §A.6) counted from the chord's last level frame `0x3DA` (tick
`0x80B`; `0xCBD − 0x80B = 0x4B2`), after which the RNG reads `0x0000ABCD`
(`f = 0x890`, the restart's seed, `prage.c:11428`). So the chord bits outside
the menu mask still refresh the key time (`0x2EEF2`: `input_select_bits`
returns the unmasked level bits) — consistent with the code, noted, not
further tested. Run 1's mode change is §G.7.1 item 2.

`python3 tools/gp_session.py port-script --scenario gp-pads --capture
data/k11-captures/gp-pads --out /tmp/gameplay-u1/gp-pads.script` → 64 lines,
`enter_frame 321`, `enter_state 0000`, `key 321 1C 0D`, `key 441 1F 73`,
`bits 441 8000`, `bits 442 0000`, … , `key 981 2C 7A`, `key 981 31 6E`,
`key 981 4D E0`, `bits 981 2410`, `bits 986 0000`, `end 1041` (the chord's three
keys in press order, §G.4 item 3).

## §G.8 U1 closure (U1 Task 9)

**Delivered.** `tools/gp_session.py` (constants, the pad table, the
frame-keyed `Schedule`, the `poll.log` v2 format, `port_script` v2,
`trace_diff`, the CLI `port-script` / `trace-diff`) and `tools/gp_capture.py`
(the DOSBox-X run under `DX-CAPTURE /V /O`, the per-frame spin-state snapshot
closed on both sides, frame-exact key and pad injection with P1/P2 names,
chords and holds, one `H` record per consumed BIOS word, the gzip frame
stream, the self-checks), with 27 unit tests (`tools/tests/test_gp_session.py`
18, `tools/tests/test_gp_capture.py` 9) in `make verify`; `make gp-capture
scenario=gp-pads`; the evidence capture `data/k11-captures/gp-pads` (run 2).
No K11 tool, no `title_capture.py`/`smk_capture.py`/`title_compare.py` and no
`port/` file changed (`git diff --stat 934992a -- tools/k11_capture.py
tools/k11_session.py tools/k11_fields.py tools/k11_compare.py
tools/title_compare.py tools/title_capture.py tools/smk_capture.py port/` →
empty).

**Corrections to the plan (raw/capture wins), each with its evidence:**
§G.4 (the key loop `0x24D08..0x24EE7` drains a chord's words in one
iteration: one `H` per word, `S(c).head` against the frame's last `H`, FIFO
order within a frame; captured in §G.7.3); §G.7.1 (a hold of `n` releases at
the spin of `f + n`, not `f + n − 1`; the `gp-pads` scenario's one-frame
presses and menu-inert chord); §G.0 (the gate's grep includes `== demo-fight`
and keeps the K11 lines apart). §G.5.2's snapshot re-read `v3` is a design
hardening from reasoning about the read order, not a raw/capture correction
(review 1).

**Open questions.** Q2 closed (§G.7.2: all 18 names and a chord, captured
twice, each at `press f + 1`, no late injection). Q3 still open with more
data (§G.7.3: one to four iterations, `{1: 8, 2: 7, 3: 5, 4: 2}` and `{1: 6,
2: 8, 3: 5, 4: 3}`; the design replays the observed consumption frame). Q4
ruled YES by the user (§G.2) and exercised: 22 words queued and consumed per
run, none acted on by the key loop or the MAIN MENU in run 2. Q5, Q6, Q1: not
U1's (Q1 and Q6 need U4's long captures).

**Not tested.** The real keyboard controller and IRQ1 handler (the injection
writes the IRQ1 key-state table `[DS_00101514]+0x254+scan` and the BDA ring
directly, as K11's `--input inject`); typematic repeat; keys inside blocking
loops (Q5: `f` is frozen there, the generator rejects such a key); a press or
release that falls in a snapshot gap is executed at the first snapshot after
it (logged; §G.7.1 item 2 shows one), so its frame is the observed one, never
the intended one.

**Gate** (`/tmp/gameplay-u1/t9_verify.txt`, the §G.0 overrides):
`verify-exit=0`; the 45 oracle lines equal
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`
(`ORACLES-EQUAL`); the 12 `k11_compare:` walk/menuesc lines equal the Task 0
ones (`K11-EQUAL`); tool tests `Ran 67 tests`; `port_progress.py` `771 1203 64`
and `731 731 100` (unchanged: U1 ports no function).

## §G.8a Review 1 fixes (U1)

Review `.superpowers/sdd/2026-09-30-gameplay-scope/u1-review1.md` ("Needs
fixes"; spec verdict ✅). New commits on `gameplay-u1`, no history rewritten:

1. **`late` per step (Important).** `Poller` computed `late` from
   `Schedule.prev_frame` after `due(f)` returned every step, so an earlier step
   fired in the same call was judged against the last step's frame (repro:
   `after_mode 0x27 10`, `after 3`, `due(0x10C)`: step 1 is 3 frames late and
   was logged `late=0`). `Schedule.frame_of[step]` now keeps each step's `F`
   and `gp_capture.fire(sched, f)` returns `(step, action, late)` per step.
   Test `test_late_is_judged_per_step`; mutation (the old expression inside
   `fire`) → `FAIL … [(0, ('key', 'enter'), 0), …] != [(0, ('key', 'enter'), 1), …]`.
   `gp-pads` could not hit it (30-frame spacing), so §G.7's `late=0` stands.
2. **`guard_gp`** requires the parent to be exactly `data/k11-captures`
   (`data/k11-captures/walk/gp-x` is rejected); mutation → `FAIL:
   test_guard_requires_gp_prefix`.
3. **Staged publish.** A run writes into `data/k11-captures/.gp-<name>.partial`
   and `publish` moves it to `gp-<name>` only when every CHECK passed; a
   failing run goes to `gp-<name>.failed` and leaves a good capture untouched.
   `session.txt` records one `check=ok|FAIL <label>` line per CHECK. Mutation
   (always publish to `out`) → `FAIL: test_publish_keeps_a_good_capture_from_a_failing_rerun`.
4. **CHECK `frames written n/n`**; `test_frames_stream_to_gzip` now asserts the
   exact file set and that decoding stops after the last wanted frame (mutation:
   the early `break` removed → FAIL).
5. **CHECK `mode 0x27 after the Enter`** tests order: the first `S`/`P` in mode
   `0x27` comes after the Enter's `I` record, at `f` ≥ its `f` (mutation →
   FAIL).
6. **CHECK `snapshots kb == raw`** (was only printed; mutation → FAIL). All
   CHECKs are the pure `run_checks`, unit-tested.
7. `docs/PROGRESS.md`: the `v3` re-read is described as hardening, and the pad
   map as confirmed from injected key-state bytes, not the keyboard controller
   or the IRQ1 handler.
8. Makefile: the verify banner reads `== k11 and gp tool unit tests ==`; the
   `gp-capture` help names only `gp-pads` (`gp-idle-loss` "planned for U4");
   the poller's 60 s memory-file wait is the named harness value `MEM_WAIT_S`.
9. Hardening (optional in the review): `port_script` requires `S(c − 1).head`
   to equal the head before frame `c`'s first word (the previous `H` record's
   head), not merely differ from `H.head` (test
   `test_consumption_needs_the_old_head_before`, mutation → FAIL), and names a
   press whose word the full ring dropped (`ring=0`) instead of counting it as
   queued (test `test_a_press_the_full_ring_dropped_is_named`, mutation → FAIL).
   The run-1 and run-2 logs still generate their scripts (run 2's is
   byte-identical to §G.7.3's). Not done: the review's minor 1 (a `race=` flag
   for a tick between `v3` and the key-state write); the replay uses the
   observed `S.raw`/`H` frames, so only the evidence field is affected.

Run 3 (the new staging path, over run 2's good capture): `make gp-capture
scenario=gp-pads TITLE_PIN_DIR=/tmp/pr_u1_pin` → exit 0, `snapshots 2447, f
5..998, 5 frames missed`, every CHECK `ok` (`base`, `steps fired 20/20`, `end
frame reached`, `mode 0x27 after the Enter`, `snapshots kb == raw (0 differ)`,
`frames written 446/446`, `port script v2`), the same seven `check=ok` lines in
`session.txt`, no `.partial`/`.failed` left in `data/k11-captures`. `poll.log`
sha256 `9f8860340b77eaefe12a3c480638000609882b752791d6edf33d20067609fa0c`.
The map is identical to run 2's (all 18 names and the chord `2410` at
`press f + 1`, `S(f − 1) raw = 0`; `late=1` nowhere in the log). The two
counts measure different things: **21** is the pad presses (18 single names
+ the chord's 3), each `late=0`, i.e. written in the spin of its `F − 1`
(the scheduled `after`/`after_mode` steps); **22** is the BIOS words, those 21
plus the boot Enter (wall-timed, logged `late=0` by definition), and all 22
were consumed and pinned by `port_script` (`presses with bios 22 H 22`).
BIOS consumption `{1: 7, 2: 8, 3: 5, 4: 2}` over those 22.

## §G.9 The replay driver: script v2, frame-keyed keys and bits (U2 Task 1)

Worktree `.worktrees/gameplay-u2`, branch `gameplay-u2` from `gameplay-u1`
(`302ecc8`); `data` and `.superpowers` symlinked to the main checkout's, the two
git-ignored fixtures copied from it. Every `make verify` in U2 runs with the
per-agent overrides `SMK_DUMP=/tmp/pr_u2_smk TITLE_DUMP=/tmp/pr_u2_title
ATTRACT_DUMP=/tmp/pr_u2_att FRONTEND_DUMP=/tmp/pr_u2_fe TITLE_PIN_DIR=/tmp/pr_u2_pin
AUDIO_WAV=/tmp/pr_u2.wav K11_DUMP=/tmp/pr_u2_k11`; scratch `S=/tmp/gameplay-u2`.

**Baseline (Step 1).** `make verify` → `verify-exit=0` (`$S/t1_base.txt`). The
45 oracle lines (`grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)'`,
the §G.0 pattern, not the plan's) `diff` against
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt` → no output
(`ORACLES-EQUAL`); the 12 `k11_compare:` lines equal `/tmp/gameplay-u1/k11_base.txt`
(`K11-EQUAL`). The K11 dumps are present (79 walk, 187 menuesc `frame_*.raw`);
their sha256 lists (`$S/k11_{walk,menuesc}.sha256`) equal those of U1's gate
dumps `/tmp/pr_u1_k11/{walk,menuesc}` (U1 changed no `port/` file, so those are
the pristine port's). Assertion sites at `302ecc8`
(`rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' | wc -l`): **13761**.
(Note: the verify run's own mid-run `cmake --build` recompiled `test_game.c`
after the Task 1 driver had been written into it; the driver runs only under
`PR_GP_DUMP`, and the oracle lines and K11 dumps equal the pristine
references above, so the baseline stands. The assertion count was taken on the
stashed tree.)

**The driver** (`port/tests/test_game.c`, after `test_k11_oracle`; registered
once in `TEST_DRIVERS` as `X(test_gp_replay, "PR_GP_DUMP")`, before
`test_restart_drive`, which stays last). As the plan's Step 3, with two
deviations: the Task 2 statics (`gp_dumped`, `gp_hash_last`, `gp_frames`,
`gp_trace`, `GP_DS_0010810D`) are added in Task 2, where they are first used
(`-Wall -Wextra` would flag them unused here); and the `calloc` comment drops the
`PORT:` tag (a test file, not a port deviation). Keys of one frame are queued in
script order, which `port_script` keeps as press (FIFO) order (§G.4 item 3), so
a chord's words reach the port's key loop together as they reached the
original's (§G.4, §G.7.3). No `port/src` change; `run_tests.c` untouched (its
`k_drivers` table is built from `TEST_DRIVERS`).

**Red (Step 4)** — `$S/smoke.script` with `enter_state FFFF`:
`FAIL …/port/tests/test_game.c:12297: 0 != 65535`, `FAILURES: 1`, `exit=1` (as
the plan). **Green (Step 5)** — `enter_state 0000` (the value the red run
printed, = the K11 smoke's): `all checks passed`, `exit=0`; `gp.log` exactly

```
key 0 f=300 scan=1C ascii=0D mode=0003
key 1 f=450 scan=1C ascii=0D mode=0027
key 2 f=600 scan=1C ascii=0D mode=0027
```

with no `left-queued` line (as the plan).

**The gp-pads script** (`python3 tools/gp_session.py port-script --scenario
gp-pads --capture data/k11-captures/gp-pads --out $S/gp-pads.script` → 64
lines, byte-identical to `/tmp/gameplay-u1/gp-pads.script` of §G.7.3):
`all checks passed`, `exit=0`; `gp.log` holds the 22 keys and 38 bits lines in
script order, every key logged with `mode=0027` after the first (`key 0 f=321
… mode=0003`), the chord `key 19/20/21 f=981` (`2C 7A`, `31 6E`, `4D E0`) then
`bits f=981 kb=2410`, `bits f=986 kb=0000`, and **no `left-queued` line**: the
port's key loop consumed every word in the iteration the original consumed it
(the script's key frame is the capture's `H` frame).

**Mutations (Step 6),** each restored → `all checks passed`:

- (a) `gp_step[gp_next].f <= f + 1u` → `<= f` (keys one iteration late):
  `FAIL …:12295: 3 != 39` (`mode_after`) **and** `FAIL …:12299: 3 != 0`
  (`gp_missed`), `FAILURES: 2`, `exit=1`.
- (b) `key 299 1C 0D` inserted after `key 300 1C 0D`
  (`$S/smoke_unsorted.script`): `FAIL …:12244: PR_GP_SCRIPT names a parsable gp
  port script v2`, `FAILURES: 1`.
- (c, added) `enter_frame 301` (the first key is not the Enter at
  `enter_frame`, Review Focus 2): the same parse `FAIL …:12244`, `FAILURES: 1`.
- (d, added) `gp_keys_sent++` removed: `FAIL …:12298: 0 != 3`.
- (e, added) the end test `>= gp_end` → `> gp_end + GP_LOOP_SLACK` (the end
  never reached): `FAIL …:12299: 600 != 0` (the `e` step counted missed at each
  iteration past it) and `FAIL …:12300: the gp script ran to its end frame`.
- (f, added) a script whose Enter frame the loop never reaches (`enter_frame
  0`, `key 0 1C 0D`, `end 10`; `f + 1 == 0` never holds): all seven sentinel
  checks fail (`the loop reached the script's Enter frame`, `65535 != 3`,
  `65535 != 39`, `1048575 != 0` twice, `0 != 1`, `the gp script ran to its end
  frame`), `FAILURES: 7`.

**Correction to the plan (Step 6):** the plan records `gp_missed` as never
tripped and lists it under "Not tested". It is shown failing under mutation (a)
(`3 != 0`: a step applied one iteration late is exactly a miss) and (e); what
no *well-formed script* can trip on the unmutated driver is a miss, because
each `game_loop_step()` raises the counter by one and the parser rejects
unsorted steps. (`CHECK(!gp_failed …)` is proven in §H.2 item 2.)

## §G.10 Every displayed frame and the per-iteration trace (U2 Task 2)

`gp_dump_if_new` (the K11 driver's hash of `gfx_display()`, else
`DS_000E87A0`, plus `gfx_dac`; a new hash writes `frame_%05u.ipx` = 64 000
index bytes then the 768-byte DAC, and a `frames.txt` line `%05u f=%04X
tick=%08X mode=%04X`), called from the pump hook, the loader-screen hook
(`res_set_screen_hook(gp_loader)`) and after every armed iteration;
`gp_trace_line` (one `T` line per armed iteration, `SNAP_FIELDS`' names and
widths; `w10d` through the local `GP_DS_0010810D 0x0010810Du`, which
`symbols.h` does not name). Every argument is cast to `unsigned`; the build has
no warning.

**Red (Step 1):** the checks and the two files, with nothing dumping yet →
`FAIL …/port/tests/test_game.c:12311: the gp frames were written`,
`FAILURES: 1`, `exit=1` (as the plan).

**Green (Step 3), the smoke:** `all checks passed`; 145 `.ipx` files of 64 768
bytes; `trace.txt` 601 lines, `T f=012C` … `T f=0384`; the trace's mode runs
(`awk '{print $3}' trace.txt | uniq -c`), verbatim:

```
 300 mode=0027
   1 mode=002D
  18 mode=001A
  18 mode=001B
 264 mode=0010
```

— all as the plan's scratch run (the capture's `0x1A` also lasts 18 frames,
`0x248..0x259`, spec §3.5). `frames.txt` by mode: `7 0027, 2 002D, 21 001A,
18 001B, 97 0010`; first line `00000 f=012C tick=00000138 mode=0027`, last
`00144 f=0384 tick=0000039E mode=0010`. Every mode of the trace has frames.

**Step 4, the format:** the plan's `translate` expansion of `frame_00000.ipx`
→ `192000 cb92e3aa69d3 64000` (as the plan). Cross-check against the RGB
writer itself (added): the expansions of the smoke's `frame_00001..00005.ipx`
are byte-identical to the K11 walk dump's `fe_write_frame` files
(`/tmp/pr_u2_k11/walk/frame_0001.raw` … `0005`, the same MAIN MENU fade;
`frame_00000` differs from the walk's `0000`, whose Enter came at `f = 324`
instead of 300). So `.ipx` through its DAC is `fe_write_frame`'s RGB24.

**Step 5, the parse:** `gp_session.parse` on the smoke's `trace.txt` →
`T 601 [] True` (no `SNAP_FIELDS` name missing, every `TRACE_FIELDS` name
present).

**Mutations (Step 6),** each restored → `all checks passed`:

- (a) the pump hook's and the after-iteration `gp_dump_if_new()` removed:
  **the plan's check passed** (`all checks passed`): the loader hook alone still
  wrote two frames (`00000 f=025A … mode=001A`, `00001 f=026B … mode=001A`), so
  `gp_dumped > 1u` held. **Correction (measured):** a check added, the first
  dumped frame's `f` equals `enter_frame` (sentinel `gp_first_f = 0xFFFFF`):
  in the smoke and gp-pads the first frame comes from the pump hook
  (`f=012C` tick `138`); the after-iteration call would write it only if the
  pump hook's dump were gone. Under (a) → `FAIL …:12373: 602 != 300`,
  `FAILURES: 1`.
- (b) `gp_trace_line()` only when `(gp_iters & 1u) == 0u` →
  `FAIL …:12370: 300 != 601` (the trace-count check, then
  `gp_trace_lines == gp_iters_armed`; replaced in §H.2 item 1 by `== end −
  enter_frame + 1`, which fails the same way), `FAILURES: 1`.

Which call dumps what (measured on the smoke, each call removed alone): without
the after-iteration call the dump is unchanged (145 frames); without the pump
hook's it has 137: the 8 frames missing are the in-iteration presents of the
blocking fades (`f=012C` ticks `138`/`139`, `f=01C2` ticks `1CF..1D1`,
`f=0258` tick `267`, `f=025A` tick `269`, `f=026B` tick `27C`). So in the
smoke every iteration pumps (the spin `0x256C6..0x256DB` pumps), and the
after-iteration call is the defensive path of Review Focus 3 (an iteration
whose spin never pumps). It never writes a frame in the smoke or gp-pads; it
stands in for the pump hook's dump only under mutation; **no assertion
protects it**, and Review Focus 3's catch-up present has no check.

**The gp-pads replay** (`$S/gp-pads.script`, 17.6 s wall): `all checks
passed`; 3 `.ipx` (`f=0141` ticks `14D..14F`: the title screen under the Enter's yellow
wipe, an all-black frame, the settled MAIN MENU; corrected in §I item 7a), 721 `T`
lines `f=0141..0411`, all `mode=0027`, no `left-queued`. Against the capture's
`S` records (`gp_session.snapshots(poll.log)`, 721 common `f`, `0x141..0x411`),
field by field over all `SNAP_FIELDS`: **every field is equal at every `f`
except three, each explained by the sampling point or the address space, none
in `TRACE_FIELDS`:**

1. `tick` (721 of 721; first `0x141`: capture `0x56F`, port `0x14F`): host-timed
   (spec §7 Q6; the boot movies run in real time in DOSBox).
2. `t508` (721 of 721; capture `0`, port `1` at `0x141`, `t50c` equal): the
   capture reads in the spin (`[t50c] − 1 == [t508]`, `0x256C6..0x256CC`); the
   port's `T` is taken after `game_loop_step()` returns, i.e. after the tick
   that released the spin (`0x1BE10` raised `DS_00101508`), so the port's
   `t508 = t50c` where the capture's is `t50c − 1`. (Its `tick` is likewise one
   ISR tick later than a spin read.)
3. `ent` (721 of 721; capture `0x2A2BEC`, port `0x0BCBEC`): a pointer into the
   data object; the capture holds DOSBox's linear address (object base
   `0x266000`, `poll.log`'s `B` record), the port its Ghidra linear address:
   `0x2A2BEC − 0x266000 + 0x80000 = 0xBCBEC` (the "Start" row, §G.7.3).

So the port replays the 22 BIOS words and the 38 bitmap changes of `gp-pads`
with the capture's `mode st raw pad new held e0 e2 rng cred fp b1d b1f b25 w10d
cnt s0_* s1_*` at every one of the 721 frames. **What that shows is narrow:**
over `0x141..0x411` only the pad words vary in the capture (`raw` 18 distinct
values, `e0`/`e2` 3, `pad`/`new`/`held` 2); `mode` (`0x27`), `st`, `rng`,
`cred`, `fp`, `b1d`, `b1f`, `b25`, `w10d`, `cnt` and the six slot bytes hold one
value each (measured on run 3, §H.2 item 6), so their equality in this MAIN MENU
window says little. (Informational: U3 owns the
comparison and its ratchets; items 2 and 3 are recorded for it in §H.)

## §G.11 `make gp-replay` and the gate (U2 Task 3)

`Makefile`: `GP_DUMP = /tmp/pr_gp_dump` beside `K11_DUMP`; `gp-replay` after
`gp-capture` (in `.PHONY`; `make help` lists it), as the plan's Step 1 with one
addition: an absent capture prints `gp-replay: no capture at
data/k11-captures/<scenario>` and exits 0, **or exits 1 when
`PR_ORACLE_REQUIRED` is set (even empty, the `make test` convention) and
`GP_OPTIONAL` is empty**. And `make verify` runs it on the U1 capture after the
K11 oracles: `@$(MAKE) --no-print-directory gp-replay scenario=gp-pads
GP_OPTIONAL=1` (the controller's instruction: the real `gp-pads` capture is
the driver's test input). **It skips without the capture, like the K11
oracles (spec §4.3), whatever `PR_ORACLE_REQUIRED` says** (review 1,
Important 1, ruling (b); §H.2 item 3): a checkout without
`data/k11-captures/gp-pads` still passes `make verify`. U3's `gp-oracle` must
not inherit `PR_ORACLE_REQUIRED` either (pass `GP_OPTIONAL=1` to its
`gp-replay`). The plan put nothing in `make verify` (U3's `gp-oracle` adds the
idle-loss run); U3's edit of the `verify` recipe goes after this line.

**Step 2** (`make gp-replay scenario=gp-pads GP_DUMP=/tmp/pr_u2_gp`):
`gp_session: port-script: gp-pads: wrote /tmp/pr_u2_gp/gp-pads.script (64
lines)`, `all checks passed`, exit 0; 721 `T` lines, 3 `.ipx`; `trace.txt`
byte-identical (`cmp`) to the direct run of §G.10. Branches: `scenario=gp-nonesuch`
→ `gp-replay: no capture at data/k11-captures/gp-nonesuch`, exit 0; the same
under `PR_ORACLE_REQUIRED=1` → `… (PR_ORACLE_REQUIRED)`, `make: *** [gp-replay]
Error 1`. Failure propagation (mutation): a scratch copy of the capture
(`K11_CAPTURES=$S/caps`, `poll.log` line 322 `P … f=0141 mode=0027 st=0000`
edited to `st=0001`, so the script says `enter_state 0001`) →
`FAIL …/port/tests/test_game.c:12366: 0 != 1`, `FAILURES: 1`,
`make: *** [gp-replay] Error 1` (exit 2). The scratch copy was deleted; nothing
under `data/` was written.

**Step 3, the gate** (`make verify` with the §G.9 overrides plus
`GP_DUMP=/tmp/pr_u2_gp`; `$S/t3_verify.txt`): `verify-exit=0`, last line `all
checks passed`. The 45 oracle lines (the §G.0 pattern) `diff` against
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt` → no output
(`ORACLES-EQUAL`); the 12 `k11_compare:` lines equal `/tmp/gameplay-u1/k11_base.txt`
(`K11-EQUAL`); the K11 dumps' sha256 lists equal Task 1's
(`K11-walk-IDENTICAL`, `K11-menuesc-IDENTICAL`); the verify's gp-replay
section: `wrote /tmp/pr_u2_gp/gp-pads.script (64 lines)`, `all checks passed`,
its `trace.txt` identical to Step 2's. `git diff --stat 934992a -- port/src` →
empty. Assertion sites **13776** = 13761 + 15 (11 from Task 1 as the plan; 4
from Task 2, the plan's 3 plus §G.10's first-frame check). Tool tests `Ran 67
tests` (unchanged). `port_progress.py`: `771 1203 64`, `731 731 100`
(unchanged: U2 ports no function).

## §G.12 U2 closure (U2 Task 4)

**Delivered.** The env-gated driver `test_gp_replay` (`port/tests/test_game.c`,
`TEST_DRIVERS` `X(test_gp_replay, "PR_GP_DUMP")`, before `test_restart_drive`)
and `make gp-replay scenario=<gp-…>` (in `make verify` on `gp-pads`). No
`port/src` file, no K11 driver line and no tool changed (`git diff --stat
302ecc8 -- port/src tools` → empty).

**The driver's contract** (what U3 relies on):

- Input: `PR_GP_SCRIPT`, a port script v2 (`gp_session.py port-script`). The
  parser sizes its step array from the script's line count (`calloc`, no fixed
  cap) and rejects a script that is unsorted, lacks `enter_frame`/`enter_state`,
  does not end in `end`, or whose first step is not the Enter `key <enter_frame>
  1C 0D`.
- The frame rule: before the iteration that raises `DS_000EF6DC` from `f` to
  `f + 1`, every step with frame `≤ f + 1` is applied in script order — `key`
  through `input_push` (a chord's words together, §G.4), `bits` through
  `k11_key_bits` (the override seam plus the bitmap bytes
  `[DS_00101514]+0x2D8/+0x2D9`). The Enter is armed when `f + 1 ==
  enter_frame`; nothing is applied before it.
- Stop: after the iteration that raises the counter to `end`; the loop bound
  `end + GP_LOOP_SLACK` iterations from boot, **`GP_LOOP_SLACK = 600` a harness
  bound, not a game value** (every `game_loop_step()` raises the counter by one,
  so a well-formed script ends at iteration `end`; the slack only bounds a port
  that never arms or never reaches `end`); `GP_STALL_PUMPS = 200000` host pumps
  in one iteration exit 1 (the K11 driver's guard); a CPU fault ends the run
  through `host_set_fault_hook` and fails `CHECK(!gp_failed …)`.
- Output in `PR_GP_DUMP`: `gp.log` (`key N f=… scan=… ascii=… mode=…`, `bits
  f=… kb=…`, `left-queued after f=…` for a key the port left in the queue,
  `missed …`, `fault …`); `frame_%05u.ipx` (64 000 indices then the 768-byte
  DAC; RGB = `dac[idx]`, byte-identical to `fe_write_frame`, §G.10) for every
  new displayed image from the Enter's iteration on, dumped at each host pump,
  loader screen and iteration end; `frames.txt` (`%05u f=%04X tick=%08X
  mode=%04X`); `trace.txt`, one `T` line per armed iteration with the
  `SNAP_FIELDS` names and widths, read after `game_loop_step()` returns.
- Checks (15 sites): the parse (including the first key being `1C 0D`, §H.2
  item 4); the log and dump files open; armed; mode 3
  before and `0x27` after the Enter's iteration, the counter at `enter_frame`
  and `DS_000F0A64 = enter_state` after it (sentinels `0xFFFF`/`0xFFFFF`); keys
  queued = keys in the script; no missed step; end reached; no CPU fault and no
  frame-write failure; more than one frame; the first frame at `enter_frame`;
  `end − enter_frame + 1` `T` lines (one per `f`, §H.2 item 1).

**The smoke's mode runs** (§G.10): `300 × 0x27, 1 × 0x2D, 18 × 0x1A, 18 × 0x1B,
264 × 0x10` over `f = 0x12C..0x384` — the port's first view of spec §3.5's
path (MAIN MENU, START MENU, LEFT PLAYER ARCADE, the wipe, character select).
**gp-pads** (§G.10): 721 iterations `0x141..0x411`, all mode `0x27`, no
`left-queued`; every `SNAP_FIELDS` field equal to the capture's `S` records at
every `f` except `tick` (host-timed), `t508` (sampling point) and `ent`
(address space), §H items 1–2.

**Not tested.** A step missed behind a blocking loop (a script cannot express
one: the generator rejects a key consumed while `f` is frozen, §4.1/Q5, and
the parser rejects unsorted steps; `gp_missed` is shown failing only under the
driver mutations of §G.9); a CPU fault during replay (no scripted path faults;
the same check is proven through the frame-write path, §H.2 item 2); a
script beyond `0xFFFF` frames (the counter is a word; the idle run ends near
`0x22BA`); a present in an iteration whose spin never pumps (the smoke's and
gp-pads' iterations all pump, §G.10: the after-iteration dump never writes a
frame there, and no assertion protects it); `left-queued` (neither script
leaves a key).

**Gate:** §G.11 Step 3 (the closure commit changes only `docs/`, this record and `AGENTS.md`'s driver list; `make verify` re-run on it, same results, U2 report).

## §H U2 corrections and notes (raw/measured wins over the plan)

1. **The trace's sampling point differs from the capture's by the releasing
   tick (for U3).** The capture's `S` is read in the spin (`[t50c] − 1 ==
   [t508]`, `0x256C6..0x256CC`); the port's `T` after `game_loop_step()`
   returns, which is after the port's spin loop (`flow.c` `while (DS_0010150C −
   1 == DS_00101508) { game_isr_ticks(1); host_wait_vblank(); }`) ran its tick.
   So the port's `t508` equals its `t50c` where the capture's is `t50c − 1` (721
   of 721 gp-pads frames), and its `tick` is one ISR tick past a spin read.
   Neither is in `TRACE_FIELDS`; `tick` is host-timed anyway. Kept as the
   plan's sampling point (no port change in U2); a comparison of `t508` must
   subtract that tick.
2. **`ent` is a pointer, compared across address spaces (for U3).** The
   capture's `ent` is DOSBox's linear address (data object base `0x266000`,
   the `B` record), the port's the Ghidra linear address:
   `0x2A2BEC − 0x266000 + 0x80000 = 0xBCBEC` at every gp-pads frame. Not in
   `TRACE_FIELDS`; a comparison must rebase it.
3. **The plan's frame check could not fail under its own mutation (a)**
   (§G.10): the loader-screen hook alone wrote two frames. Added
   `CHECK_EQ_INT(gp_first_f, gp_enter_frame)`; (a) now fails `602 != 300`.
4. **`gp_missed` can fail** (§G.9): under the keys-one-late mutation (`3 !=
   0`); the plan listed it as untestable. Only a well-formed script on the
   unmutated driver cannot trip it.
5. **`make verify` replays gp-pads** (§G.11), skipping without the capture
   (§H.2 item 3); standalone `gp-replay` fails on an absent capture under
   `PR_ORACLE_REQUIRED` — both beyond the plan, per the controller's
   instruction.
6. The plan's baseline grep (`… k11_compare`) was replaced by §G.0's
   (`… == demo-fight`, K11 lines apart), as U1 did.

### §H.2 U2 review 1 fixes

Review `.superpowers/sdd/2026-09-30-gameplay-scope/u2-review1.md` ("Needs
fixes"; spec verdict compliant). `gameplay-u2` was first rebased onto
`gameplay-u1` `dfde805` (U1's review fixes). U1's review-fix section, which
U1 had numbered §G.9, is now **§G.8a**, so U2 keeps §G.9–§G.12 (U3's plan
uses §G.13+); its references (§G.7's "run 3, §G.8a", `docs/PROGRESS.md`'s U1
paragraph) were updated. The gp-pads capture is now U1's run 3 (`poll.log`
sha256 `9f886034…fa0c`); its port script is byte-identical to run 2's
(`diff` of the two `port-script` outputs, 64 lines, `enter_frame 321`,
`enter_state 0000`, 22 `key` and 38 `bits` lines), so every §G.9–§G.11 number
stands. Each fix, with its mutation (each restored → `all checks passed`):

1. **One `T` line per `f` (Minor 1).** `CHECK_EQ_INT(gp_trace_lines,
   gp_iters_armed)` counted two variables incremented side by side (a harness
   tautology). Replaced by `gp_trace_lines == gp_end − gp_enter_frame + 1`:
   each armed iteration raises the counter by exactly one (`0x24CDB`), so a
   skipped or repeated `f` fails it. Smoke 601, gp-pads 721 (pass). Mutations:
   trace only on even iterations → `FAIL …:12378: 300 != 601`; a second
   `gp_trace_line()` in the Enter's iteration (a repeated `f`) → `FAIL
   …:12379: 602 != 601`. `gp_iters_armed` is gone.
2. **`CHECK(!gp_failed …)` proven (Minor 2);** its message now reads "no CPU
   fault or frame-write failure ended the gp replay". Mutation `ok = 0;` after
   the frame write in `gp_dump_if_new` → `FAILURES: 6`, among them `FAIL
   …:12371: no CPU fault or frame-write failure ended the gp replay` (also `1
   != 3` keys, the end, the frames, `1048575 != 300` first frame, `1 != 601`
   T lines: the write failure stops the loop at the Enter).
3. **`make verify` skips gp-pads without the capture (Important 1, ruling
   (b)).** Spec §4.3: a gp capture skips like the K11 oracles. The verify line
   is `$(MAKE) gp-replay scenario=gp-pads GP_OPTIONAL=1`; `GP_OPTIONAL=1`
   turns the `PR_ORACLE_REQUIRED` failure off (a make variable, not an
   inherited env var, so `PR_ORACLE_REQUIRED=1 make verify` skips too).
   Measured: `PR_ORACLE_REQUIRED=1 make gp-replay scenario=gp-nonesuch` →
   `… (PR_ORACLE_REQUIRED)`, exit 2; the same with `GP_OPTIONAL=1` →
   `gp-replay: no capture at data/k11-captures/gp-nonesuch`, exit 0;
   `PR_ORACLE_REQUIRED=1 make gp-replay scenario=gp-pads GP_OPTIONAL=1` → `all
   checks passed`. U3's `gp-oracle` must not inherit `PR_ORACLE_REQUIRED`
   either.
4. **The first key must be the Enter (Minor 7).** `gp_parse` also requires
   `gp_step[0]` to be scan `0x1C` ascii `0x0D` (the mode-3 Enter the arm needs,
   `0x24ECF`). Mutation: the smoke with `key 300 39 20` → `FAIL …:12298:
   PR_GP_SCRIPT names a parsable gp port script v2`.
5. **`make gp-replay` without `scenario=` (Minor 6)** fell back to the global
   `scenario ?= walk` and fed the K11 v1 capture to `port-script`. The recipe
   now requires `gp-?*`: `make gp-replay` and `make gp-replay scenario=walk` →
   `usage: make gp-replay scenario=gp-<name> (got scenario=walk)`, exit 2.
6. **Wording (Minors 3–5):** the bits count is 38 (18 one-frame pads × 2 + the
   chord's press and release), not 36; the after-iteration dump is stated as
   unprotected (§G.10, §G.12); the gp-pads match is stated as narrow. Measured
   on run 3 (721 common `f`): distinct capture values `raw` 18, `e0` 3, `e2` 3,
   `pad`/`new`/`held` 2 each, and **1** for `mode`, `st`, `rng`, `cred`, `fp`,
   `b1d`, `b1f`, `b25`, `w10d`, `cnt`, `s0_52/54/5a`, `s1_52/54/5a`, `ent`
   (correction to the review, which counted `mode` among the varying fields:
   it is `0x27` at all 721 frames). Differences port vs capture: `tick`,
   `t508`, `ent` only, 721 each, as before.

Assertion sites: 13776 → **13776** (the trace check replaced one-for-one; the
Enter test lives in the existing parse check).

## §G.13 The loaders (U3 Task 1)

Worktree `.worktrees/gameplay-u3`, branch `gameplay-u3` from `gameplay-u2`
(`300eef7`); `data` and `.superpowers` symlinked to the main checkout's, the
two git-ignored fixtures copied. Every `make verify` in U3 runs with the
per-agent overrides `SMK_DUMP=/tmp/pr_u3_smk TITLE_DUMP=/tmp/pr_u3_title
ATTRACT_DUMP=/tmp/pr_u3_att FRONTEND_DUMP=/tmp/pr_u3_fe TITLE_PIN_DIR=/tmp/pr_u3_pin
AUDIO_WAV=/tmp/pr_u3.wav K11_DUMP=/tmp/pr_u3_k11 GP_DUMP=/tmp/pr_u3_gp`; scratch
`S=/tmp/gameplay-u3`.

`tools/gp_compare.py`: `expand_ipx` (64 000 indices and the 768-byte DAC to
RGB24 by `bytes.translate` per channel), `_paths`, `load_capture_frame` (gzip,
length-checked), `load_port_frame`, `Lazy` (LRU, `cache = 160` frames: a harness
value, memory only), `View`. Tests: `Ran 2 tests … OK` (before: `ModuleNotFoundError`).
Mutation: every channel read from `dac[3 * i + 0]` → `FAIL:
test_expand_is_dac_of_index … b'\x01\x01\x01' != b'\x01\x02\x03'`; restored →
`OK`.

Cross-check against the RGB writer itself (as U2 §G.10): the expansions of
the smoke dump's `frame_00001..00005.ipx` (`/tmp/gameplay-u2/smoke`) are
byte-identical to `fe_write_frame`'s `/tmp/pr_u2_k11/walk/frame_0001.raw` …
`0005.raw` (`True` ×5). So `expand_ipx` is `fe_write_frame`'s `rgb = dac[idx]`
and the port frames U3 compares are the same bytes the K11 and front-end
oracles read.

## §G.14 The frame claim and its ratchet (U3 Task 2)

`gp_compare.shift`, `exhibited` (`k11_compare.exhibited`'s rule, read:
`tools/k11_compare.py:63`), `classify`, `frame_claim`, `nearest`, `diff_box`,
`ratchet`; `title_compare.explain` is called as is (no tolerance, mask, crop or
allowance; `git diff 300eef7 -- tools/title_compare.py` empty). **Harness
values, named as such (spec §5):** `BACK = 2`, `AHEAD = 64` (the `[p − 2, p + 64)`
search order: the window only decides which port frames are tried first; a
frame the window does not explain is tried against the whole dump, so whether
a frame is explained never depends on it, only possibly which explanation
(clean, splice or transition) is reported first when two exist), `REPORT_MAX = 5`,
`Lazy`'s `cache = 160`.

Tests: `Ran 8 tests … OK` (the plan's; before: 6 × `AttributeError: … 'frame_claim'`),
then 3 added, `Ran 11`: `test_an_unreachable_n_fails` (N above the capture's
end must fail: a ratchet that could never be met), `test_an_improved_first_unexplained_is_said`
(the "raise N" hint), `test_report_goes_past_the_first_unexplained` (report mode
lists two, the enforced run stops at the first), and `test_unpinned_n_fails` now
asserts the `not pinned` message and `first is None`, so it cannot pass on some
other failure.

Mutations (each restored → `Ran 11 … OK`, `git diff` of the restored file empty
against the pre-mutation copy):

- (a) no full-dump fallback in `classify` → `FAIL: test_a_match_beyond_the_window_is_found`.
- (b) the all-black `continue` removed → `FAIL: test_black_frames_are_skipped`.
- (c) `first < n` → `first < n − 1` → `FAIL: test_first_unexplained_against_the_ratchet`.
- (d) the start search taking the first non-black capture frame (`if True:` for
  `if 0 in exhibited(…)`) → `FAIL: test_window_start_is_the_port_first_frame`.
  (The first attempt, `start = 0` before the search, did not fail: the search
  overwrites it — a bad mutation, not a weak test; (d) is the real one.)
- (e) `if n > end:` → `if False:` → `FAIL: test_an_unreachable_n_fails`.
- (f) the enforced run continuing past the first unexplained frame →
  `FAIL: test_report_goes_past_the_first_unexplained`.

## §G.15 The trace claim (U3 Task 3)

`gp_compare.trace_claim`: the port's first `T` line per `f` against the
capture's first `S` line per `f` (`gp_session.snapshots`), over
`gp_session.TRACE_FIELDS` (`mode st raw pad e0 e2 rng cred s0_5a s1_5a`); an
`f` the capture did not snapshot is skipped and counted, never compared with a
default (spec §3.7); the first differing `f` is the ratcheted value; `tick` is
reported on its own (host-timed, spec §7 Q6), never ratcheted.

**Additions to the plan, from the U2 facts (§H items 1–2; the plan's claim
covered only `TRACE_FIELDS`, which leaves out `t508`, `ent` and `tick`):**

1. `normalised`: `t508` and `ent` are compared **after** the conversion §H
   derived, and reported (one line, counts and first `f`), **never ratcheted**.
   `t508`: port = capture + 1 (the capture reads in the spin, the port after the
   releasing tick). `ent`: port = capture − `B.base` + `0x80000` (capture linear
   address to Ghidra linear address; `B` is the capture's first `B` record; a
   capture `ent` of 0 must be 0 in the port). Neither is compared raw. They are
   reported rather than ratcheted because `t508`/`t50c` restart at some
   transitions (spec §3.7), where a difference would say nothing about play.
2. **A vacuous trace fails**: no port `T` record with a capture snapshot is an
   exit 1 (`nothing compared`), so a wrong `poll.log` or an empty overlap cannot
   read as "0 unexplained".

Tests: the plan's count was `Ran 10` (2 loader + 6 frame + 2 trace); with the
U3 additions the file runs **15** at this point (2 loader, 9 frame, 4 trace: the
plan's two, the conversion test, the vacuous test), `OK` (before the trace
code: `AttributeError … 'trace_claim'`); the CLI class of §G.16 makes it 19.
Mutations (each restored → `Ran 15 … OK`):

- `TRACE_FIELDS + ('tick',)` → `FAIL: test_tick_is_reported_not_ratcheted`.
- `t508` without the `+ 1` → `FAIL: test_normalised_fields_are_converted_not_compared_raw`.
- `ent` without the rebase → the same test fails.
- the `compared == 0` guard removed → `FAIL: test_nothing_compared_fails`.
- an unsnapshotted `f` compared anyway (the skip removed) → `ERROR:
  test_first_difference_skips_unsnapshotted_frames` (a `KeyError`, where the plan
  would have to default the missing record) and `test_nothing_compared_fails`.

## §G.16 The CLI, `make gp-oracle` / `gp-report`, `make verify` (U3 Task 4)

`gp_compare.py --scenario N --capture DIR --port DIR [--min-first N]
[--trace-min-first F] [--report]`. Beyond the plan: a capture directory without
a `poll.log` fails (report mode: exit 0 and says so); report mode prints
`report only: no ratchet applied, exit 0`; the ratchet's wording is per claim
(`first unexplained` for frames, `first differing` for the trace). Tests (the
CLI class, 4): an absent capture skips (exit 0, `(skipped)`), a present capture
with unpinned values fails (twice) and passes once pinned, a capture without
`poll.log` fails, `--report` exits 0 where the enforced run exits 1. Mutations
(each restored → `OK`): unpinned N treated as 0 → `FAIL:
test_a_present_capture_needs_pinned_values` and `test_unpinned_n_fails`; the
absent-capture skip returning 1 → `FAIL: test_an_absent_capture_skips`; the trace
N ignored (`n_trace = 0`) → `FAIL: test_a_present_capture_needs_pinned_values`.
(**Corrected in review 1, §I item 10:** the two mutations of `main`'s final
`return 0 if a.report else …` that survived here were not redundant. In report mode
`frame_claim` returns 1 for "window empty" and `trace_claim` for "nothing
compared", so the guard is what makes `make gp-report` exit 0 exactly when the
comparison is most broken; it is now tested and each mutation fails.)
Suite: `Ran 19 tests` in `test_gp_compare` at this commit; 28 after review 1.

**Self-comparison through the CLI** (scratch, not committed: U2's smoke dump
`/tmp/gameplay-u2/smoke`, 145 `.ipx`, expanded into a fake capture
`/tmp/gameplay-u3/gp-fake`, its `trace.txt` rewritten as `S` records), verbatim:

```
gp_compare: gp-fake: frames: window from capture 0 (raw 0); 138 classified: 138 clean, 0 splice, 0 transition, 0 unexplained, 7 all-black
gp_compare: gp-fake: frames: 0 unexplained through 144; ratchet N 145 ok
gp_compare: gp-fake: trace: 601 frames compared (f 12C..), 0 without a capture snapshot; first tick difference none (reported, not ratcheted)
gp_compare: gp-fake: trace: normalised (reported, not ratcheted): t508 601 of 601 differ (first f=12C); ent not compared (the capture has no B record)
gp_compare: gp-fake: trace: 0 differing through 900; ratchet N 901 ok
```

(The plan's "139 clean" is 145 − 7 = 138 here; its counts were loose. The
`t508` line is the self-comparison's own artefact: the fake capture copies the
port's `t508`, so the `+ 1` conversion of real captures does not hold.) Without
`--min-first`/`--trace-min-first`: `FAIL: the ratchet N is not pinned` for both,
rc 1; against `/tmp/gameplay-u3/nothing`: `no capture at … (skipped)`, rc 0.

**Makefile.** `GP_IDLE_LOSS_MIN_FIRST` and `GP_IDLE_LOSS_TRACE_MIN_FIRST`
(empty until U4 Task 5), `gp-oracle`, `gp-report`, both in `.PHONY` and `make
help`; `verify` runs `gp-oracle` after U2's `gp-replay scenario=gp-pads
GP_OPTIONAL=1` line, and `tools.tests.test_gp_compare` joined the tool-test
line. **Both gp-oracle and gp-report pass `GP_OPTIONAL=1` to `gp-replay`**
(record §H.2 item 3): a missing gp capture skips even under `PR_ORACLE_REQUIRED`
(spec §4.3), where the plan's bare `gp-replay` would have failed there.

**The report on U1's gp-pads** (`make gp-report scenario=gp-pads
GP_DUMP=/tmp/pr_u3_gp`, `all checks passed`; U1's run 3, 446 distinct frames),
verbatim (re-run after review 1, so the coverage line and the missing ratchet lines are the current output):

```
gp_compare: gp-pads: report only: no ratchet applied, exit 0
gp_compare: gp-pads: frames: window from capture 104 (raw 1744); 7 classified: 2 clean, 0 splice, 0 transition, 5 unexplained, 2 all-black
gp_compare: gp-pads: frames: FIRST UNEXPLAINED capture 108 (raw 3930): nearest port 0, rows 0..199, x 0..319 (63947 px)
gp_compare: gp-pads: frames: UNEXPLAINED capture 109 (raw 3935): nearest port 0, rows 0..199, x 0..319 (63952 px)
gp_compare: gp-pads: frames: UNEXPLAINED capture 110 (raw 3936): nearest port 0, rows 0..199, x 0..319 (63952 px)
gp_compare: gp-pads: frames: UNEXPLAINED capture 111 (raw 3940): nearest port 0, rows 0..199, x 0..319 (63955 px)
gp_compare: gp-pads: frames: UNEXPLAINED capture 112 (raw 3941): nearest port 0, rows 0..199, x 0..319 (63955 px)
gp_compare: gp-pads: frames: coverage (reported, not ratcheted): 0 non-black port frame(s) up to port 2 not exhibited by any classified capture frame
gp_compare: gp-pads: trace: 721 frames compared (f 141..), 0 without a capture snapshot; first tick difference f=141 (reported, not ratcheted)
gp_compare: gp-pads: trace: normalised (reported, not ratcheted): ent 0 of 721 differ; t508 0 of 721 differ
gp_compare: gp-pads: trace: 0 differing through 1041
```

What it shows (read from the capture and the port dump, not asserted by the
tool): the port's dump is 3 `.ipx`. Capture 104 (raw 1744) is clean port 0 (the
title screen under the Enter's yellow wipe: the title-to-menu transition), 105
(raw 1746) all-black, 106 (raw 1747) clean port 2 (the settled MAIN MENU), which
the capture then holds as one distinct frame for 721 game iterations. (The
earlier text called port 0 the "MAIN MENU fade-in", inherited from §G.10; the
reviewer rendered both.) Capture 107 (raw 3926) is
all-black and 108 (raw 3930) the first screen after it: raw 1744 → 3930 is
31.19 s at 70.0866 fps, and `f = 0x141 → 0x88C` (the idle-timeout restart's mode 3,
`poll.log` `P … f=088C mode=0003`, §G.7.3) is 31.09 s at 60.05 Hz. So **the
first unexplained capture frame is the first screen after the original's
idle-timeout restart, which the port never reaches because its script ends at
`f = 0x411` (`end 1041`, 60 frames after the chord)** — exactly the spec §4.3
case "when the port's script ends before the capture … `j` bounds how far the
port got". It is not a rendering difference. The trace over the 721 common
frames `0x141..0x411`: no `TRACE_FIELDS` difference; `t508` equals the capture's
`+ 1` and `ent` equals the capture's rebased by the `B` record's `0x266000`, at
**all 721** (§H items 1–2 confirmed by the tool: 0 of 721 differ); `tick`
differs from the first frame (host-timed). As in §G.10 this is narrow: over
those frames only the pad words vary (mode is `0x27` throughout).

An enforced run of the same dump on gp-pads (real data; not a gate, gp-pads has
no pinned values): `--min-first 108 --trace-min-first 1042` → `first unexplained
108, ratchet N 108 ok` and `0 differing through 1041; ratchet N 1042 ok`, rc 0;
`--min-first 109` → `FAIL: first unexplained 108 < ratchet N 109`;
`--min-first 107` → `ratchet N 107 ok (improved: raise N)`; `--trace-min-first
1043` → `FAIL: N 1043 > end 1042: N is unreachable`. (For a trace with no
difference, N is the end of the port's trace, `last f + 1`: the exact pin.) After
review 1 an enforced run also needs `--max-start`: `--max-start 104` → rc 0; `103`
→ `FAIL: window starts at capture 104 (raw 1744) > pinned start 103`; `108` →
`window start 104 < pinned start 108 (improved: lower the pin)`; unset → `FAIL: the
window-start pin is not set (GP_IDLE_LOSS_MAX_START)`. On `gp-pads` the exact pins
would be start 104, N 108, F 1042.

**The gate** (`make verify` with the §G.13 overrides, `/tmp/gameplay-u3/t4_verify.txt`):
`verify-exit=0`, last line `all checks passed`. The 45 oracle lines (the §G.0
pattern) `diff` against
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt` → no output
(`ORACLES-EQUAL`); the 12 `k11_compare:` lines equal `/tmp/gameplay-u1/k11_base.txt`
(`K11-EQUAL`); the K11 dumps' content hashes (sha256 over the sorted per-file
sha256, walk and menuesc) equal U2's gate dumps'; the gameplay sections read
`gp-replay: no capture at data/k11-captures/gp-idle-loss` and `gp_compare: no
capture at data/k11-captures/gp-idle-loss (skipped)` (the gp-pads replay line ran
and passed before it); tool tests `Ran 92 tests` `OK` (`test_gp_compare`'s 19 included);
`port_progress.py` `771 1203 64` / `731 731 100` (unchanged: U3 ports no
function); no `port/` file changed.

**The claim, stated narrowly (AGENTS.md's language for the front-end and K11
oracles).** The frame claim proves that no content-bearing capture frame from
the window start up to the first unexplained one is unexplained by the port's
frames under `title_compare`'s model (clean, a byte splice of adjacent port
frames, one transition row); it does not prove the frames after it, nor that
the port draws what the capture draws where both are black (all-black capture
frames are skipped), and **it cannot detect a port that under-renders**: its
window start is where the capture first shows the port's own first frame, and
every port frame the dump holds is a candidate explanation. A port that stops
early only moves the first unexplained frame earlier (so the ratchet catches
that), and the claim never says how much of the capture the port was supposed
to reach. A green frame claim is not "the frame is correct". The trace
claim proves the `TRACE_FIELDS` equal at every snapshotted `f` below its first
difference; unsnapshotted frames (spec §3.7) are not compared, `tick` is not
claimed (host-timed), `t508` and `ent` are reported after their conversions and
not ratcheted, and **over a window where little varies it says little**: the
only measured trace evidence so far is `gp-pads` (mode `0x27` throughout, only
the pad words vary, §G.10, §H.2 item 6). The values `GP_IDLE_LOSS_MIN_FIRST` and
`GP_IDLE_LOSS_TRACE_MIN_FIRST` and `GP_IDLE_LOSS_MAX_START` are not pinned by U3
(no `gp-idle-loss` capture exists yet); U4 Task 5 pins them from a measured first
unexplained frame, a measured first differing `f` and the measured window start,
each with its provenance, and proves each can fail.

**What the frame claim does not claim (review 1, §I items 8–9).** The window
START is ratcheted (it must not be later than the pin and must lie below N), so
a regressed port cannot slide the window past the frame that set N; the claim
holds from the window start, not from capture frame 0 (the frames before it, the
title before the Enter, are not checked). **Order and port-frame coverage are not
claimed by the ratchet.** The full-dump fallback lets any port frame explain any
capture frame, and port frames the capture never shows are not required: a
garbage port frame inserted between two good ones (port `[1, 99, 2]` against
capture `[1, 2]`) and a capture that goes back in time (capture `[1, 3, 2, 1]`
against port `[1, 2, 3]`) both pass. A reported coverage line counts the
non-black port frames up to the last one the capture exhibits that no classified
capture frame exhibits (it flags the first example, not the second); it is
reported and not ratcheted in this unit. **Named gap:** a coverage and an order
ratchet (every non-black port frame up to the last exhibited one must be
exhibited, in order), sound at 70.09 Hz capture against 60.05 Hz game under the
same splice model; the tests `test_named_gap_order_is_not_claimed` and
`test_coverage_is_reported_…` pin the current behaviour so a later change must
update this paragraph.
The `[p − 2, p + 64)` search order, `REPORT_MAX` and the LRU size are harness
values, not game values.

## §I U3 corrections and notes (raw/measured wins over the plan)

1. **`make gp-oracle` and `gp-report` pass `GP_OPTIONAL=1`** to `gp-replay`
   (the plan's recipe passed nothing): spec §4.3 says a gp capture skips like
   the K11 oracles, and §H.2 item 3 made a bare `gp-replay` fail on an absent
   capture under `PR_ORACLE_REQUIRED` (which `make verify`'s callers may set).
2. **The trace claim also reports `t508` and `ent` after the §H conversions**
   (plan: `TRACE_FIELDS` only). Measured on `gp-pads`: 0 of 721 differ for both
   (§G.16), which confirms §H items 1 and 2 with the tool. Reported, not
   ratcheted (they restart/convert at transitions, spec §3.7).
3. **A vacuous trace (nothing compared) fails**; a capture directory without
   `poll.log` fails; the ratchet words its noun per claim. None is in the plan.
4. **The plan's expected self-comparison counts were loose** (`139 clean`):
   145 frames − 7 all-black = **138** classified (§G.16).
5. **The windowed search is a search order, not a guarantee of the same kind**
   (the plan: "the classification equals the unwindowed one"): whether a frame is
   explained never depends on the window (the full dump is the fallback), but
   when a frame has both a clean and a splice explanation the window may report
   the one nearer `p` (§G.14). The ratchet reads only explained/unexplained.
6. **`AGENTS.md`'s command block names `make gp-capture scenario=gp-pads`**,
   not the plan's `gp-idle-loss`: that scenario does not exist before U4
   (`gp_session.SCENARIOS` holds `gp-pads` only; U1 says "planned for U4").
7. **The first measured frame ratchet value is the port's script end, not a
   rendering difference** (§G.16: `gp-pads`, first unexplained capture frame
   108 = the first screen after the original's idle-timeout restart, which the
   port's script ends 60 frames after its chord and never reaches). For
   `gp-idle-loss` U4 should expect the same shape if its replay stops early:
   N then bounds how far the port got, as spec §4.3 says.
7a. (item 7 wording) **Port frames 0..2 of `gp-pads`** are the title screen under
   the Enter's yellow wipe, an all-black frame and the settled MAIN MENU, not "the
   MAIN MENU fade-in" (§G.10 and §G.16 said so; review 1 rendered them). Capture 104
   is clean port 0 and 106 clean port 2. Evidence unchanged, wording corrected.
8. **The window START is ratcheted (review 1, Important 1).** The start is where the
   capture first shows the port's first frame, so a port whose first frame regressed
   to a screen that recurs later in the capture slid it forward past the frame that
   set N, and the claim went green (reviewer's fixture: port `[9,2,3]` against
   capture `[9,2,3,7,1,2]` with N=3 gave `(0, 3)`, the regressed port `[1,2,3]`
   gave `(0, None)`). New pin `GP_IDLE_LOSS_MAX_START` (Makefile, `--max-start`,
   empty until U4 pins it; an unset pin fails once the capture exists, like the
   other pins), and the start must also lie below N. Test
   `test_the_window_start_cannot_slide_forward`; mutations (`start > max_start` and
   `start >= N` checks removed, unset-pin check removed) each fail a named test.
9. **Order and port-frame coverage are not claimed (review 1, Important 3), a named
   gap.** See §G.16's "What the frame claim does not claim". A reported coverage
   line was added; no ratchet. `AGENTS.md` and `docs/PROGRESS.md` say the same.
10. **Report mode (review 1, Important 2 and Minor 4).** `make gp-report` always
    exits 0, including when `frame_claim` finds no window and `trace_claim` compares
    nothing (the condition is printed; `main`'s guard is load-bearing, §G.16 corrected,
    test `test_report_mode_exits_zero_when_the_claims_cannot_run`, mutation of the
    guard fails it). It prints no ratchet verdict and checks no pin after "no ratchet
    applied" (tests for both claims; mutations fail).
11. **Minors 5 and 9.** A fully explained enforced run with N below the end prints
    `every item is explained: N = <end> is the exact pin` (as `title_compare`
    prints "window end + 1 is the exact pin"); the empty-trace header no longer
    prints `(f []..)`.
12. `docs/superpowers/plans/2026-09-30-gameplay-u3-frame-compare.md` ended with
    stray planner-artifact text after its last Self-Review bullet (closing tags and
    a line telling the reader to run `build_u3.py` from a scratchpad); removed.


## §G.17 The scenario and the script truncation (U4 Task 1)

Worktree `.worktrees/gameplay-u4`, branch `gameplay-u4` from `gameplay-u3`
(`b78fd51`); `data` symlinked to the main checkout's, the two git-ignored
fixtures (`port/tests/ghidra_data.bin`, `title_screen_ref.ppm`) copied. Every
`make verify` in U4 runs with the per-agent overrides `SMK_DUMP=/tmp/pr_u4_smk
TITLE_DUMP=/tmp/pr_u4_title ATTRACT_DUMP=/tmp/pr_u4_att FRONTEND_DUMP=/tmp/pr_u4_fe
TITLE_PIN_DIR=/tmp/pr_u4_pin AUDIO_WAV=/tmp/pr_u4.wav K11_DUMP=/tmp/pr_u4_k11
GP_DUMP=/tmp/pr_u4_gp`; scratch `S=/tmp/gameplay-u4`. The plan says "main checkout";
U4 runs in the worktree (the controller's instruction), so `S`-relative paths and
`data/k11-captures/gp-idle-loss*` resolve through the symlink to the one shared
`data/`.

`SCENARIOS['gp-idle-loss']` (the plan's steps: boot Enter, `after_mode 0x27 150`
Enter, `after 150` Enter, `until_mode 3 0`; `time_limit = 200`) and
`SCENARIOS['gp-idle-loss-run2']` (a copy); `port_script(name, lines, end=None)`
(the local `X` record renamed `xrec`; `--end F` keeps the keys and bits at
`f <= F` and writes `end F`, a header comment `(cut at F)`; an `--end` past the
capture's `X` frame is a `ScriptError`); `gp_session.py port-script … --end F`;
`make gp-replay … GP_SCRIPT_ARGS="--end F"` (`GP_SCRIPT_ARGS ?=`, appended to the
`port-script` command); the `make help` line for `gp-capture` now names
`gp-pads|gp-idle-loss`. The 150-frame gaps and the 200 s limit are harness
values (spec §5), the probe's 2.5 s and 169.9 s.

Tests: `python3 -m unittest tools.tests.test_gp_session` → `Ran 22 tests … OK`
(U1's 20 after review 1 plus the 2 of `TestIdleLoss`; before the code:
`KeyError: 'gp-idle-loss'` and `TypeError: port_script() got an unexpected
keyword argument 'end'`, `FAILED (errors=2)`).

**Correction to the plan's mutation proof (measured).** The plan says dropping the
`if c <= last` filter on keys fails `test_end_truncates_the_script`. It does not:
the plan's `end=295` lies after every key (`288`, `294`), so only the `bits`
filter (`bits 296`) is exercised; with the key filter dropped the test stayed `OK`.
The test gained a second cut, `end=290`, which keeps only the Enter at `288` and
drops the key at `294` (and its bits). Mutations (each restored → `OK`): key
filter dropped → `FAIL: test_end_truncates_the_script`; bits filter dropped
(`for f, kb in bits`) → the same `FAIL`; the `--end` past the `X` frame
(`end=400`) raises `ScriptError` (asserted).

## §G.18 Capture run 1 and the mode path (U4 Task 2)

**Ruling Q7 (storage; logged here as the controller's instruction requires):**
spec §7 Q7 is answered YES by the user's delegate: a gp-idle-loss capture of
0.3–0.6 GB after gzip is acceptable in the git-ignored `data/k11-captures/`.
Measured: `data/k11-captures/gp-idle-loss` **376 MB** (`du -sh`; 8 173 distinct
frames of 14 017 AVI frames), `gp-idle-loss-run2` **374 MB** (8 134 frames):
inside the range. Both are git-ignored (`data/`), `data/` still holds nothing
but the `gp-*` directories this unit wrote through `make gp-capture`.

**Run 1:** `make gp-capture scenario=gp-idle-loss TITLE_PIN_DIR=/tmp/pr_u4_pin`
(the pinned exe sha256 `8120f1bd…a68d`, zero CMOS, `pad_bios=1`, DOSBox-X
2026.08.31, `time_limit=200`), exit 0, console `/tmp/gameplay-u4/cap1.txt`:
`snapshots 9807, f 4..0x2681 (the console prints f in hex), 47 frames missed
(spec §3.7)` (99.5 % of the 9 854 iterations); `CHECK base`, `steps fired 3/3`,
`end frame reached`, `mode 0x27 after the Enter`, `snapshots kb == raw (0 differ)`,
`frames written 8173/8173`, `port script v2`: all `ok`; `wall 200.8 s` (the time
limit: DOSBox-X was stopped by `-time-limit`, the scenario's end was reached at
`ms=174412`, then the attract ran on); `raw 1371..14016`. `poll.log` 9 850 lines,
sha256 `773e2647…8c8447`. `session.txt` records the seven `check=ok` lines. The
first rerun with `--time-limit 260` that the plan allows was not needed.

**The end record.** `X f=207F` (`ms=174412`) is not the frame of mode 3's first
observation: `P f=207A mode=0003` came at `ms=159739`; then `f` froze for the boot
movies (15 s of wall time) and the next spin snapshot is `f=207F`, where the
scenario's `until_mode` end was logged. So `end 8319` is the frame the master loop
resumed at, five iterations after mode 3 began. (Both runs: `X f=207F`.)

**The mode path** (script of the plan, `/tmp/gameplay-u4/modepath1.txt`; `f` hex,
`poll.log:<n>` is the first `S`/`P` record of the mode; credits, slot `+0x5A`
from the first `S` of the mode):

| poll.log | f | mode | frames | what the record shows |
|---|---|---|---|---|
| 3 | 1 | 3 | 320 | attract; the Enter pressed at `f=0x13F` (wall 25.0 s), consumed `0x141` |
| 323 | 0x141 | 0x27 | 301 | MAIN MENU, credits 5; Enter 2 consumed `0x1DA`, Enter 3 `0x26E` |
| 631 | 0x26E | 0x2D | 1 | LEFT PLAYER ARCADE; **credits 4** from here (5 → 4) |
| 632, 651 | 0x26F, 0x281 | 0x1A, 0x1B | 18, 18 | the wipe |
| 670 | 0x293 | 0x10 | 941 | character select, P1 idle |
| 1612, 1631 | 0x640, 0x652 | 0x1A, 0x1B | 18, 18 | the pick timed out |
| 1648, 1650 | 0x664, 0x665 | 0x11, 0x17 | 1, 241 | |
| 1892, 1911 | 0x756, 0x768 | 0x1A, 0x1B | 18, 18 | |
| 1930 | 0x77A | 5 | 123 | round start |
| 2054 | 0x7F5 | 6 | 1148 | round 1 (P2 `e2 = 0x2020` at its first frame, P1 `e0 = 0`) |
| 3203 | 0xC71 | 8 | 188 | round end: P1 `+0x5A` rose `0x10, 0x17, 0x27, 0x37, 0x3F, 0x51, 0x59, 0x6B, 0x72, 0x78`; P2's stayed 0 |
| 3389 | 0xD2D | 0x16 | 241 | results (P1 `+0x5A = 0x78`) |
| 3629 | 0xE1E | 5 | 123 | round start |
| 3752 | 0xE99 | 6 | 3250 | round 2 |
| 7003 | 0x1B4B | 7 | 113 | match end |
| 7114, 7116 | 0x1BBC, 0x1BBD | 9, 0x17 | 1, 241 | P1 `+0x5A = 0x4F`, P2 0 |
| 7355 | 0x1CAE | 0x15 | 121 | countdown |
| 7477 | 0x1D27 | 0x13 | 665 | the challenge screen |
| 8141 | 0x1FC0 | 0x1E | 4 | game over; `cnt = 0xFF` (the count below 0, §3.4) |
| 8145, 8147 | 0x1FC4, 0x1FC5 | 0x14, 0x17 | 1, 181 | |
| 8327 | 0x207A | 3 | — | back in mode 3 (credits 5 again at the `X` record) |

The path equals spec §3.5's, mode for mode (`0x27, 0x2D, 0x1A, 0x1B, 0x10, 0x1A,
0x1B, 0x11, 0x17, 0x1A, 0x1B, 5, 6, 8, 0x16, 5, 6, 7, 9, 0x17, 0x15, 0x13, 0x1E,
0x14, 0x17, 3`), the credits 5 → 4 at `0x2D` (`f=0x26E`), P1's `+0x5A`
reaching `0x78` in round 1 and P2's staying 0 throughout the match, and no mode
`0xE` or `0x1F`. **The idle loss scenario reproduces §3.5's path; the deltas
differ in one place:**

| segment | spec §3.5 (probe) | run 1 |
|---|---|---|
| mode `0x10` (P1 idle) | 916 (`0x26C → 0x600`) | **941** |
| round 1, mode 6 | **1 831** (`0x7B5 → 0xEDC`) | **1 148** |
| round 2, mode 6 | 3 249 (`0x10F9 → 0x1DAA`) | 3 250 |
| 7 → 9 | 111 (`0x1DAA → 0x1E19`) | 113 |
| `0x13` | 636 (`0x1F84 → 0x2200`) | 665 (`0x1D27 → 0x1FC0`) |
| P1 `+0x5A` at match end | `0x44` | **`0x4F`** |

Each difference is a **measured difference between the probe and the harness
capture, not a correction of the path**: the Enters were consumed at other frames
(probe `0x120/0x1B3/0x247` against run 1's `0x141/0x1DA/0x26E`, and a different
wall-timed first Enter), the character-select pick time-out fell at another
frame, and the CPU's round 1 was a different fight from the probe's (a KO after
1 148 frames, where the probe's lasted 1 831: the same winner, P1's `+0x5A`
reaching `0x78`, P2's 0). The cause of the round 1 difference is **not
established**: it begins at a different pick/round start and so a different RNG
stream (`rng` at `0x77A` is `0xBA9703D6`), but the probe's `rng` was not logged
at those frames. The spec's "916 frames in mode `0x10`" (probe) and the 941 here are both
measured; run 2 (§G.19) gives 947 for the same input, so the quantity moves by
several frames between runs of the harness and the probe's value is one more
sample of it (the pick time-out's anchor is open, §G.19). The probe's 1 831 against
1 148 is the one difference the evidence does not close. (The plan's wording "the
CPU wins round 1 by KO and round 2 on time" holds here: P1's `+0x5A` reaches `0x78`
in round 1, mode 8, and the match ends by the clock in round 2.)

The shape of the match, which §G.20 compares with the port: P1 never receives
input (`e0 = 0`, `raw`/`pad` 0 from `0x2D` to the end, the three Enters aside).

## §G.19 Capture run 2 and the determinism answer (U4 Task 3; spec §7 Q1)

**Run 2:** `make gp-capture scenario=gp-idle-loss-run2 …` → exit 0, `snapshots 9793,
f 5..0x2676, 49 frames missed`, the seven CHECKs `ok`, `frames written 8134/8134`,
`wall 200.7 s`, `raw 1383..14016`; `poll.log` 9 836 lines, sha256
`5ce62b39…30c7`; 374 MB. Path identical to run 1's (§G.18); `X f=207F` as run 1.

**The runs are not the same input.** The boot Enter is wall-timed (25.0 s), so it
lands on another frame, and each queued BIOS word is consumed 1–4 iterations
later (spec §3.6; cause open):

```
run 1: enter_frame 321, keys 321 474 622   (press f 0x13F 0x1D6 0x26C; consumed 0x141 0x1DA 0x26E)
run 2: enter_frame 314, keys 314 464 616   (press f 0x137 0x1CF 0x265; consumed 0x13A 0x1D0 0x268)
```
(`gp_session.py port-script` of each; `end 8319` in both; the key gaps are 153/148
against 150/152 frames.) So `trace-diff`'s absolute-`f` answer
(`gp_session: trace-diff: 9779 frames compared; first difference f=13B (mode);
first tick difference f=5`) compares different input frames and does not answer Q1.

**Rebased, as the plan's Step 2 (scratch `/tmp/gameplay-u4/rebase.py`: `S` records
with `f -= <key frame>`, `gs.format_s`, `trace_diff`; the logs `a<k>.log`/`b<k>.log`).**
Rebasing by the Enter alone (key 0) differs at `f−321 = 0x140` (mode `0x1A` vs
`0x1B`) because key 1 and 2 sit at different offsets from key 0 (153 vs 150, 148
vs 152); by key 1 at `+149`, by key 2 (the last input; after it P1 never acts) at
**`+978`**, mode `0x1A` (run 1) against `0x10` (run 2). Up to `+977` after key 2,
all `TRACE_FIELDS` (mode, st, raw, pad, e0, e2, rng, cred, s0_5a, s1_5a) are equal
in the two runs (`rng` included, `0xD9C00F95` at `+0x3CF`): **deterministic over the
978 frames after the last input, including the character-select stretch.**

**The pick time-out is not anchored to the key.** Rebased by key 2, run 2's time-out
comes **6 frames later** (`+984`, run 1 `+978`; mode `0x10` lasts 941 frames in run 1,
947 in run 2), and from there every later transition is the same 6 frames
later with the same durations (the mode table: `0x1A +978/+984`, round 1 `+1415/+1421`
(1 148 frames both), `0x13 +6841/+6847`, `0x1E +7506/+7512`, mode 3 `+7692/+7698`).
The whole of run 2 shifted by 6 and compared from `+978` on run 1's key 2 frame:
**`trace_diff` → `first None`, 8 189 frames compared, no `TRACE_FIELDS` difference**
(the shifted comparison, for the record).

**The same measurement in absolute `f` is cleaner:** over the 9 779 common `f`
the `TRACE_FIELDS` differ at **85 frames, all inside `0x13B..0x624`** (fourteen runs
of 4–6 frames, each at an input or a mode change: `0x13B..0x140` the Enter,
`0x269..0x26D`, `0x27B..0x282`, `0x28D..0x292`, then the character-select
frames `0x2DA..0x624` every `0x5D` = 93 frames: `rng` 66 frames, `mode` 23, `cred`
5), and **from `0x625` to the end of the capture (`0x207F`) every `TRACE_FIELDS` value is
equal in the two runs**: the pick time-out fell at the same absolute
**`f = 0x640`** in both, though key 2 was consumed 6 frames apart (`0x26E`,
`0x268`). The probe's pick time-out (`0x600`) is a third value (its Enter `0x120`).
The anchor is **not** the keys, **not** the tick (at `0x640`: tick `0xA82` run 1,
`0xA84` run 2; `tick − f` `1090` vs `1092`), and not a constant absolute `f`
(the probe's `0x600`): it behaves like a clock counted from the boot, a host-timed
(or boot-anchored) timer, and **its source is an open question** (named in §G.23,
not guessed; a follow-up reads the mode `0x10` countdown's source, `0x424E8`'s
sibling for the pick). The slack is 6 frames at most in the two runs.

**Q1 (answer).** Over a run where every input is held to the same frames the
pinned original is **deterministic in every `TRACE_FIELDS` value**: 978 frames after
the last input are equal (rebased by it), and from the time-out (`f = 0x625`) to the
end of the capture (7 000+ frames, absolute `f`) the two runs agree
in every `TRACE_FIELDS` value, `rng` included. What varies between runs is
**when the pick time-out comes relative to the keys** (a 6-frame spread between two
runs, 25 against the probe), which moves a whole later run by that shift.
The `tick` differs everywhere (`first f=5` absolute; host-timed boot offset),
reported not ratcheted (Q6, §G.20).

**What the 85 absolute-`f` differences are.** They are the input-timing shift,
not nondeterminism: the three keys were consumed 7, 10 and 6 frames apart
between the runs (`0x141/0x13A`, `0x1DA/0x1D0`, `0x26E/0x268`), so every
game event after a key lands that many frames apart (the fourteen runs of 4–6
frames are exactly the events in the table of §G.18: the Enter, the wipe, the
character select's 93-frame `rng` draws), until the pick time-out re-aligns the
runs at the absolute `f = 0x640`. With the shift removed (rebased by key 2) the
first 978 frames after the last input are identical. So **there is no run-to-run
difference that the input timing does not explain**, and for Task 5 the
run-to-run bound on the trace ratchet is *none*: `F = the port's first differing
f` (plan: `min(port, run-to-run)`, run-to-run absent). The inputs the port
replays are run 1's frames (`port-script` of `gp-idle-loss`), which are the ones
run 1's `S` records were taken under, so the comparison port-against-run 1 is
a comparison under the same inputs.

**Not established:** why the pick time-out is anchored at the absolute
`f = 0x640` in two runs and `0x600` in the probe (a clock from the boot, not the
keys and not `tick`); with only two runs the 6-frame figure is a lower bound of
the spread.

## §G.20 The port replay of gp-idle-loss and the first divergences (U4 Task 4)

`make gp-report scenario=gp-idle-loss GP_DUMP=/tmp/pr_u4_gp TITLE_PIN_DIR=/tmp/pr_u4_pin`
(`/tmp/gameplay-u4/report1.txt`; the build ran in the worktree's `build/`). **No
stall and no fault:** the driver's `all checks passed` (the Enter arm, the three
keys queued, no `left-queued`/`missed`/`fault` line in `gp.log`, `end 8319`
reached); the script is `enter_frame 321`, `key 321/474/622 1C 0D`, `end 8319` (7
lines: no pad bits); 6 497 `.ipx` (409 MB; `frames.txt` ends `06496 f=207F
tick=000020EA mode=0006`), 7 999 `T` lines. The plan's `GP_IDLE_LOSS_END` branch
is **not needed** (the Makefile is unchanged by this task). The two claims, verbatim:

```
gp_compare: gp-idle-loss: report only: no ratchet applied, exit 0
gp_compare: gp-idle-loss: frames: window from capture 90 (raw 1744); 112 classified: 87 clean, 20 splice, 0 transition, 5 unexplained, 6 all-black
gp_compare: gp-idle-loss: frames: FIRST UNEXPLAINED capture 203 (raw 2359): nearest port 106, rows 9..19, x 160..174 (102 px)
gp_compare: gp-idle-loss: frames: UNEXPLAINED capture 204 (raw 2360): nearest port 106, rows 7..19, x 160..174 (122 px)
gp_compare: gp-idle-loss: frames: UNEXPLAINED capture 205 (raw 2362): nearest port 72, rows 7..19, x 160..174 (156 px)
gp_compare: gp-idle-loss: frames: UNEXPLAINED capture 206 (raw 2366): nearest port 71, rows 7..19, x 160..174 (156 px)
gp_compare: gp-idle-loss: frames: UNEXPLAINED capture 207 (raw 2369): nearest port 70, rows 7..19, x 160..174 (156 px)
gp_compare: gp-idle-loss: frames: first unexplained 203, ratchet N 0 ok (improved: raise N)
gp_compare: gp-idle-loss: trace: 1764 frames compared up to the first difference (f 141..), 4 without a capture snapshot; first tick difference f=141 (reported, not ratcheted)
gp_compare: gp-idle-loss: trace: first difference f=828 (2088) in rng: capture 73A05D37, port CE92DD04
gp_compare: gp-idle-loss: trace: normalised (reported, not ratcheted): ent 0 of 7973 differ; t508 7655 of 7973 differ (first f=281)
gp_compare: gp-idle-loss: trace: first differing 2088, ratchet N 0 ok (improved: raise N)
```

(`window from capture 90` is the first capture frame the port's first dump
exhibits; 7 973 frames compared = the common `f` of the port's 7 999 and the
capture's 9 807 snapshots; the report-mode "ratchet N 0" is the unpinned value,
not a result.)

### The first unexplained capture frame (Step 2)

Capture 203 (raw 2359) against the nearest port frame 106 (`frames.txt`: `00106
f=0334 tick=0000034E mode=0010`): the character select (mode `0x10`), Sauron on
the left, the pick-countdown number at the top. The differing 102 px lie in rows
9..19, x 160..174: **the countdown digits** — the capture shows **12**, the port
frame **13** (PNGs `/tmp/gameplay-u4/cap_203.png`, `port_106.png`, crops `*_crop.png`
read by eye). The rest of the picture is identical to port frame 106.

What the neighbouring frames show (digit region masked, scene hashes matched against
every mode-`0x10` port frame, `/tmp/gameplay-u4`): capture 185..202 follow port
frames 93..108/110 in order (the scene, with the 13 digit, as the port draws it);
capture **203** is the first frame whose digit is the next one (12; the mid-flip
frame, 204+ show it whole) **and** from 203 the scene runs *backwards* through
port frames 106, 105, … 100, 99, … 80 (capture 203..238), one scene step per
capture frame, where the port's own scene, after the same digit change at port
frame 111 (`f = 0x340`, a multiple of 64: `0x43CF0..0x43CFF` run `0x43AAC` when
`f & 0x3F == 0`, `port/src/game/fight.c` `fight_char_select_pass`), keeps
going forward (frames 111..146) and reverses at port frame 147 (`f = 0x3A0`,
**96 game frames later**). So the divergence is **in the character select's actor
animation, not in the digit**: Sauron's idle animation turns around at the
`f = 0x340` countdown step in the original and at `f ≈ 0x3A0` in the port; the
digit appears different only because the nearest port frame is the one with the
same scene. The trace is equal at those frames (mode, st, rng, e0/e2, cred
all as the original through `f = 0x827`), so nothing in `TRACE_FIELDS` sees it.

**Owner and classification: a port divergence in the character-select actor's
animation (an actors/animation-timing or a `0x43AAC` side effect the port omits);
not host-timed (the original is deterministic over this window: both captures
carry the same scene at the same `f` from `0x625`, §G.19), not an unported
function reached (the replay ran to the end with no fault), not a harness
artefact (`f` and the inputs are those of the capture; the trace is equal).**
**Not isolated to a function:** which of the actor/animation routines differ is
a named gap (§G.23, owner U5, the character-select walk). The countdown itself
(`DS_0010816C`, the `f & 0x3F` anchor) is the same in both: the pick time-out comes
at `f = 0x640` in the port and in both captures, which also resolves §G.19's open
"anchor": it is the absolute frame counter (`f & 0x3F == 0` steps), not a clock.

(The later unexplained frames 204..207 are the same scene reversal, `rows 7..19
x 160..174` plus the reversed scene; the report stops at its `REPORT_MAX = 5`.)

### The first trace difference (Step 3)

`first difference f=828 (2088) in rng: capture 73A05D37, port CE92DD04`.
```
poll.log  S f=0827 mode=0006 … e0=0000 e2=0000 rng=73A05D37 … s0_5a=10 s1_5a=00     (tick 00000CA8)
trace.txt T f=0827 mode=0006 … e0=0000 e2=0000 rng=73A05D37 … s0_5a=10 s1_5a=00     (tick 00000892)
poll.log  S f=0828 mode=0006 … rng=73A05D37 …                                       (tick 00000CA9)
trace.txt T f=0828 mode=0006 … rng=CE92DD04 …                                       (tick 00000893)
```
(`f = 0x825, 0x826, 0x827`, the three frames before: identical in all
`TRACE_FIELDS`, `rng = 0x73A05D37`; the mode at `0x828` is 6, round 1, 51 frames
after the round start `0x7F5`.) The two runs agree here (§G.19: no difference
from `0x625`), so this is not run-to-run noise. The port's draw at `0x828` is the
original's draw 5 frames later: the sequence of `rng` values after `0x822` is the
same in both (`0x73A05D37` → `0xCE92DD04` → …: the capture changes it at `0x82D`,
the port at `0x828`), consumed in the same order, so no extra or missing draw —
a **timing** difference of an effect entry. A throw-away `backtrace` in `rng_next`
(not committed; `rng.c` restored) names the caller of the draws at `f = 0x822`
and `0x828`: `fight_4b144 ← fight_4aad0 ← fight_effects_pass ← game_mode_04_step`
(`rng(2)`, `rng(0x1200)`, `rng(0x1200)`), i.e. the **type-0 fight-effect entry
(the crowd/worshipper walk, `fight.c` 0x4B144/0x4AAD0)** choosing its next target.
The original makes the same three draws at `0x822` (the first walk) and the next
walk's draws at **`0x82D`**; the port at **`0x828`**: its walk to the target ends
**5 frames earlier**. The state around it, from the `S`/`T` records (equal up to
`0x827`): at `0x82E` the original's P1 slot changes (`+0x52` 0x11 → 0x10,
`+0x5A` 0x10 → 0x17: P2's attack connects, `e2` having been `0x4C4C`, `0x1010`,
`0x2F20` from `0x81A..0x822`), in the port P1 is not hit there (`+0x52` goes to 9
at `0x837`, `+0x5A` stays `0x10`). From there the fights are different fights.

**Classification: port bug (probable) in the actors' animation/walk timing;
the first trace difference and the first frame difference share one signature**:
an actor animation whose phase differs by a few frames (Sauron's turn-around
96 frames, the worshipper's walk 5 frames, the opposite sign), deterministically.
The *cause is not isolated*: not host-timed (the two captures agree from `0x625`
at every `f`, and `tick` advances 1 per frame around `0x820`: `0xCA1` at `0x820`,
`0xCA6` at `0x825`), not an unported function (no fault, the same draws in the
same order), not the harness (the inputs are run 1's own; `f` is the capture's
frame counter). It is a named gap (§G.23), pinned by the ratchet.

### The tick line and spec Q6 (Step 3)

`first tick difference f=141` (the first compared frame): the capture's tick
counts from DOSBox's start (`0x56F`-ish plus the boot movies) and the port's from
its Enter; `tick − f` is constant in the port (`0x1A`) and grows in the capture
(1075 at `0x200`, 1090 at `0x640`, 1153 at `0x1000`, 1156 at `0x2000` in run
1; run 2 runs 2–3 ticks above it at the same `f`): host-timed, as spec §7 Q6
guessed. **No `TRACE_FIELDS` difference follows within 60 frames of the first tick
difference** (`f = 0x141` → the first difference is at `0x828`, 1 764 frames
later): in the 1 764 frames compared before the divergence the port agrees with the
original in every `TRACE_FIELDS` value while the tick offset drifts by 15, so
**play in that window does not read the host-timed tick in any way the trace
shows** (the pick countdown is `f`-anchored, above). Q6 stays open for the
rest of the run (the divergence at `0x828` is not correlated with a tick event:
the capture's tick advances 1 per frame there). `t508` (reported, not ratcheted):
7 655 of 7 973 frames differ from the `+1` rule; port − capture takes the
values `{1: 318, 0: 929, 2: 276 …}` and jumps at every load (`0x281`, `0x622`, `0x640`,
`0x654`, `0x7C1`, `0xE1F` (+1 793), `0x163C`, `0x1D29`, `0x1FC1` …): the loaders'
tick model (`RES_READ_BYTES_PER_TICK`, `res.c`), host-timed in the original. `ent`:
0 of 7 973 differ after the `B` rebase.

### The path against the capture's

```
f     capture            port
141   0x27               0x27        26E 0x2D  26F 0x1A  281 0x1B  293 0x10
640   0x1A  652 0x1B  664 0x11  665 0x17  756 0x1A  768 0x1B  77A 5  7F5 6   — equal, mode for mode and frame for frame
C71   8 (round 1: KO)    14D9 7 (time up), 154A 8, 154B 0x16, 163C 5, 16B7 6
D2D   0x16  E1E 5  E99 6  1B4B 7  1BBC 9  1BBD 0x17  1CAE 0x15  1D27 0x13  1FC0 0x1E  1FC4 0x14  1FC5 0x17  207A 3   (capture only)
```

(From `trace.txt` and the `S`/`P` records: `/tmp/gameplay-u4/pathtable.txt`.) The
port's round 1 lasts 3 300 frames (`0x7F5 → 0x14D9`, ending by the clock, P1's
`+0x5A = 0x38`, P2's 0) where the original's lasts 1 148 (KO, `+0x5A = 0x78`);
its round 2 starts at `0x16B7` and is in mode 6 at the script's end `0x207F`
(`+0x5A = 7`). **Nothing past round 1 is compared with the original's
`0x1B4B`..`0x207A`**: the port never reaches match end, mode 9, the countdown,
the challenge, game over or the return to mode 3 before its script ends.
