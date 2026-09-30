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
`all checks passed`, `exit=0`; `gp.log` holds the 22 keys and 36 bits lines in
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
unsorted steps. Not tested: `CHECK(!gp_failed …)` (no scripted path faults; a
fault would need a capture that faults).
