# Gameplay U11 — in-match keys: derivation record

Unit U11 of the reverse-completion programme (spec
`docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track G;
gameplay spec `2026-09-30-gameplay-ground-truth-design.md` §2, §3.1–§3.2, §7
Q2/Q4/Q5). Plan: `docs/superpowers/plans/2026-10-01-gameplay-u11-in-match-keys.md`.
§K.0–§K.10 were written by the planner (2026-10-01) from the raw bytes, Ghidra
and the existing captures; the plan's tasks append §K.11 onward.

## §K.0 Method and sources

- **Image.** The fixed-up image (LE fixups applied) from the port's own loader:
  `build/diffrun --exe data/game/C/PRAGE.EXE --image-out $S/image.bin` (EXE sha256
  `eecba701576d36d1a217a271acd90e9aa4473121db8d51e8c8085c194ce0e91b`, image
  sha256 `0cfd6f481182f050d5897871068bb9f8fa4410108a56cba0b17be7d513943dec`,
  mapped at `0x10000`). Every listing below is capstone over that image, so its
  data operands are runtime addresses (`[0x105f30]` is `DS_00105F30`), unlike
  the raw-file listings of the earlier records. `dx.py ADDR N` (N hex) and
  `scan.py HEX…` (every 4-byte little-endian occurrence of an address in the
  image, split code `< 0x73B14` / data), both run from the directory holding
  `image.bin`:

```python
# dx.py
import sys, capstone
data = open('image.bin', 'rb').read(); BASE = 0x10000
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
a = int(sys.argv[1], 16); n = int(sys.argv[2], 16) if len(sys.argv) > 2 else 40
for k, i in enumerate(md.disasm(data[a - BASE:a - BASE + n * 16], a)):
    if k >= n: break
    print('%08X  %-20s %s %s' % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
```

```python
# scan.py
import sys, struct
d = open('image.bin', 'rb').read(); B = 0x10000
for t in sys.argv[1:]:
    p = struct.pack('<I', int(t, 16)); i = d.find(p); hits = []
    while i >= 0:
        hits.append(B + i); i = d.find(p, i + 1)
    print(t, ' '.join('%X' % h for h in hits if h < 0x73B14), '| data:', ' '.join('%X' % h for h in hits if h >= 0x73B14))
```
- **Ghidra** (live MCP, project `rage`, `/PRAGE.EXE`, read-only queries):
  `decompile_function 0x249F0 0x5004A 0x1D1B0 0x1D220 0x1D250 0x1D270 0x28CC8
  0x28DA4 0x2CA7C 0x2C060 0x4F644 0x50161 0x2D2F0 0x500C4`;
  `get_xrefs_to 0x105F30`; `get_function_callers 0x2EDE0`;
  `search_instructions operand=105c00`; `search_strings ABANDON|QUIT|PAUSE|Y/N`
  (no match: the strings are in a resource, see §K.3). The live project lists
  1504 functions, the committed `port/decomp/prage.functions.csv` 1352; each
  boundary cited below names its source. The raw bytes win where they differ
  (one case: §K.4's `0x2CA10`).

## §K.1 Where the game reads the keyboard

- **int 16h.** A scan of the code object for `cd 16` finds exactly these sites
  (plus `0x6BAF0`, the runtime's `int86` thunk table entry, which no rel32
  call targets):

  | site | AH | in | role |
  |---|---|---|---|
  | `0x24D0E` | 1 | `0x24C5C` (game_frame) | the key loop's "any key?" |
  | `0x24D2E` | 0 | `0x24C5C` | the key loop's read |
  | `0x24E54` | 0 | `0x24C5C` | the pause's blocking read |
  | `0x24A64` | 0 | `0x249F0` | the Y/N prompt's blocking read |
  | `0x2EB15`, `0x2EB2D` | 1, 0 | `0x2EA78` | the timed-screen drain (only for EAX ≥ 0) |

- **The IRQ1 key-state table** `[DS_00101514]+0x254+scan` (bit 7 clear = down)
  and the bitmap `+0x2D8/+0x2D9`. `scan.py 101514` finds 79 code references; by
  function (CSV boundaries): the ISR sampler `0x1BBAC` and its device helpers
  (`0x1B610 0x1B6A0 0x1B730 0x1B7C0 0x1B850 0x1B870`, and the joystick bodies
  `0x1B890..0x1BB73`, which the CSV lists in no function: game-port reads,
  `in al,dx` with `dx = 0x201` at `0x1B899`); key configuration `0x1AE20`,
  `0x1AF64`; boot `0x1BEC4`, `0x20C10` (controller checks); the loader
  `0x1B3AC` (writes `+0x253`); shutdown `0x1BE30`; the menus' `0x2EBF0`; the
  service menu (`0x31F56`, `0x32400`, outside CSV functions); device-word
  readers `0x1DE64`, `0x3C600`, `0x461DC` (`+0x2D4`/`+0x2D6` only); `0x500C4`.
  **In a match the only reader of the key-state table is the timer ISR's
  `0x1BBAC` (`0x1BE1C`), through the configured pad scans** — no other key
  is read from the table in play.
- So an in-match key acts either as a **pad bit** (its scan is one of the
  configured `+0x2DE..+0x2ED` bytes, or F1/F2) or through the **BIOS word**
  the key loop reads. There is no third path.

## §K.2 The key loop `0x24CFE..0x24EE7` arm by arm

`dx.py 24CCD B0` (verbatim excerpts):

```
00024CDB  66893ddcf60e00       mov word ptr [0xef6dc], di
00024CFE  beff000000           mov esi, 0xff
00024D03  bf27000000           mov edi, 0x27
00024D08  b401                 mov ah, 1
00024D0E  cd16                 int 0x16
00024D20  0f84c6010000         je 0x24eec
00024D2E  cd16                 int 0x16
00024D3E  a8ff                 test al, 0xff
00024D44  c1e808               shr eax, 8
00024D4B  21f0                 and eax, esi
00024D4D  a3305f1000           mov dword ptr [0x105f30], eax
00024D5A  83f81e               cmp eax, 0x1e
00024D67  e8f4baffff           call 0x20860
00024D6E  80fb0d               cmp bl, 0xd
00024D73  0f8656010000         jbe 0x24ecf
00024D79  80fb1b               cmp bl, 0x1b
00024D7E  0f861a010000         jbe 0x24e9e
00024D84  80fb20               cmp bl, 0x20
00024D87  7460                 je 0x24de9
00024D8E  84db                 test bl, bl
00024D96  c1eb08               shr ebx, 8
00024D99  80fb1f               cmp bl, 0x1f
00024DA0  80fb24               cmp bl, 0x24
00024DAB  80fb32               cmp bl, 0x32
00024DB5  80fb10               cmp bl, 0x10
00024DBF  e8ec83ffff           call 0x1d1b0
00024DC9  e85284ffff           call 0x1d220
00024DD3  e872b20200           call 0x5004a
00024DDF  e80cfcffff           call 0x249f0
00024DF2  83fb03               cmp ebx, 3
00024DFB  83fb27               cmp ebx, 0x27
00024E04  83fb17               cmp ebx, 0x17
00024E09  813de44a1000800e0100 cmp dword ptr [0x104ae4], 0x10e80
00024EA6  83f803               cmp eax, 3
00024EAD  e83efbffff           call 0x249f0
00024EB7  83f827               cmp eax, 0x27
00024EC5  e826fbffff           call 0x249f0
00024ED7  83f803               cmp eax, 3
00024EE0  66893d004b1000       mov word ptr [0x104b00], di
```

The loop runs in **every** iteration of the master loop, after the frame
counter (`0x24CDB`) and before the mode switch (`0x24EEC`), until the BIOS
queue is empty. Each word first sets the latch `DS_00105F30` = its ascii byte,
or its scan when the ascii byte is 0 (`0x24D3E..0x24D4D`). Then, the mode
`DS_00104B00` re-read per key:

| key (BIOS word) | arm | in a match (mode 6, `0x17`, …) |
|---|---|---|
| any non-zero ascii in mode `0x1E` | `0x24D63 call 0x20860` | name entry (not a match mode) |
| Enter `0x1C0D` | `0x24ECF` | only mode 3 → `0x27` (`0x24EE0`); **in a match: latch only** |
| ESC `0x011B` | `0x24E9E` | mode 3: `0x249F0(0)`; mode `0x27`: nothing; **any other mode: `0x249F0(1)`, ABANDON CONQUEST? Y/N** (`0x24EC5`) |
| space `0x3920` | `0x24DE9` | skipped in modes 3, `0x27`, and `0x17` with hook `DS_00104AE4 == 0x10E80`; **else the pause** (§K.3) |
| Alt-Q `0x1000` | `0x24DDD` | `0x249F0(0)`, QUIT TO DOS? Y/N, **in every mode** |
| Alt-S `0x1F00` | `0x24DC9` | `0x1D220`: sample pause toggle `DS_001028DB ^= 1` |
| Alt-J `0x2400` | `0x24DD3` | `0x5004A`: the joystick calibration (host-owned, record §55-A) |
| Alt-M `0x3200` | `0x24DBF` | `0x1D1B0`: music pause toggle (`DS_001028DA` 1 → 0, else → 1) |
| every other word | `0x24D7C`, `0x24D89`, `0x24D90`, `0x24DA3`, `0x24DB0`, `0x24DBA` (back to `0x24D08`) | latch only |

The ascii compares are unsigned (`jb`/`jbe`). The arm order and the strings
are those of record demo-pose §55-A (`0x1E8` `- PAUSED -`, `0x1EE` `QUIT TO
DOS? Y/N`, `0x1EF` `ABANDON CONQUEST? Y/N`, `0x1F0`/`0x1F1` `Y`/`N`; the
strings are in the string-table resource `0x47370` loads, which is why
Ghidra's string search finds none). **Alt itself (scan `0x38`) is read by
nothing:** it is no configured pad scan (`0x122C62` holds `1F 2D 2C 2E 16 17 31
32 48 50 4B 4D 47 49 4F 51`, record gameplay-ground-truth §G.1.2) and no
code compares `0x38`. A real Alt-letter therefore also presses the letter's
own scan in the key-state table: under the default binding **Alt-S also holds
P1 up (S), Alt-M P1 b3 (M), and the prompt's N answer P1 b2 (N)** for as
long as the key is down. The gp harness models that (it writes the letter's
scan), so those presses move P1 in the capture and in the replay alike.

## §K.3 The blocking readers, the latch, the ISR gate, the sound pair

- **The pause** `0x24E19..0x24E99`: `DS_00104B22 = 1` (`0x24E20`, the ISR's
  gate: `0x1BDF8 cmp al,1; je` skips the whole tick, so no clock, no key
  sampling), `0x1D250` (pause the music unless already paused, remembering
  that in `DS_001028D8`), the string `0x1E8` on row `0xF` (`0x2F198`),
  `0x2EA78(-1)` (one frame presented, **the latch cleared at `0x2EA85`**, no
  key drained for EAX = −1: `0x2EAE0`), then `0x24E50..0x24E6A` reads words
  until one has ascii `0x20`; then the string released (`0x2F280`),
  `0x1D270` (resume if `0x1D250` paused), `DS_00104B22 = 0`. The wait loop
  does not latch.
- **The prompt** `0x249F0` (`dx.py 249F0 70`): `0x24A01` gate on, `0x24A07
  0x1D250`, the question on row `0xA`, `0x24A3C 0x2EA78(-1)` (latch cleared),
  `0x24A64` reads a word; `0x24A7B 0x653ED` upper-cases it; `0x24A80 cmp
  eax,esi` (`Y`): gate off, then AL = 0 → `0x24A93 mov byte [0xa81a8],1` (the
  quit flag; `0x256DD cmp byte [0xa81a8],0; je 0x255ee` then leaves the master
  loop: QUIT TO DOS), AL ≠ 0 → `0x24A9C 0x1D270`, `0x24AA1 0x1B084`, `0x24AB0
  jmp 0x65431` (longjmp(0x1044F4, 1): the soft restart, record named-gaps-b
  §B.3); `0x24AB5 cmp eax,edi` (`N`): gate off, the question released; any
  other word loops (`0x24AEC`). The exit `0x24AF9 0x1D270`.
- **Consequences used by the rules (§K.6).** After a pause, an ESC–N or an
  Alt-Q–N in iteration `c`, the latch is **0** at the end of `c` (the opener
  latched its byte at `0x24D4D`, `0x2EA78` cleared it, the answer is read
  without latching) and the mode is unchanged. `DS_001028DA` (music) is
  unchanged by the pair `0x1D250`/`0x1D270` (0 → 1 → 0, or 1 kept with
  `DS_001028D8` untouched), and `DS_001028DB` (samples) is not touched.
- **No other in-match writer of the latch.** `scan.py 105F30` → `0x24D4E`,
  `0x2EA87`, `0x2EB68`, `0x2EB83`, `0x2EEEE`; Ghidra `get_xrefs_to 0x105F30`
  the same five (writes `0x24D4D 0x2EA85 0x2EB66 0x2EEEC`, read `0x2EB81`).
  `0x2EEC8`/`0x2EB80` are the menus' (`0x2EDE0`'s callers are `0x2CF00
  0x2FA40 0x33058 0x33230`), so in a fight the latch keeps its value from one
  key to the next: an event is visible as a **change** of the latch.
- **The snapshot fields this unit adds** (`gp_session.KEYS_EXTRA`, written by
  the capture for `gp-keys-fight` only, and by the port's `T` line):
  `lat` = `DS_00105F30` (4 bytes), `spz` = `DS_001028DB`, `mpz` =
  `DS_001028DA` (1 byte each).

## §K.4 Pads in a match, the join, and credits (no coin key)

- **F1/F2** are the start bits and share bit 0 of each bitmap byte with U/Home
  (`b0`): record gameplay-ground-truth §G.1.2, captured on the MAIN MENU in
  `gp-pads` (§G.7.2: `p1.start` kb `0100`, `p2.start` kb `0001`). The start
  masks `0x9ACBC` read from the image: `[0x1000000, 0x100]`.
- **Mode 6's case** `0x25256`: `cmp byte [0x104b1d],0; jne 0x25242` (only
  sub 0, the arcade), `0x2525F call 0x28CC8`, a non-zero result `dec eax;
  0x25269 call 0x28DA4`. **`0x28CC8`** (`dx.py 28CC8 30`): for each side not
  yet in (`0x28CD7 test eax,ebx` on `DS_00104B1F`): `0x28CDF 0x2C060` (a
  credit or free play) — none → `0x2C1C8` and return 0; else `0x28CED test
  [ecx*4+0x9acbc], [0x1088e4]`: the side's start mask newly pressed →
  `0x2C2B0`, `0x28D13 DS_00104B1F |= side + 1`, `0x28D19 0x2CA7C(1)`,
  return side + 1; else `0x2C178` and stop. **`0x28DA4`** (the join):
  `0x28E3A mov ecx,0x17 … 0x28E66 mov [0x104b00],cx` (mode `0x17`),
  `0x28E60` hook `0x28D80`, `DS_00104AFE = 0x78`.
- So **F1 in round 1** (side 0 already in, `b1f = 1`): side 0 is skipped,
  side 1's mask `0x100` is not pressed → no join; P1's `b0` is pressed:
  `DS_001088E4` bit `0x01000000` and `DS_001088E0 & 0x0101` (new and held,
  `0x4F644`: `e0 = new >> 24 | (held >> 16) & 0xFF00`) in the first level
  frame `F + 1`. **F2** joins side 1 at `F + 1`: mode `0x17`, `b1f` 1 → 3.
- **Credits on a mid-match join are not spent:** `0x2CA7C` (`dx.py 2CA7C`):
  `0x2CA7C cmp byte [0x105d60],0` (free play → 1), `0x2CA8B cmp eax,[0x105c00];
  ja` (too few → 0), `0x2CA93 cmp byte [0x104b1f],0; jne 0x2caa2` — the
  subtraction `0x2CA9C` runs only while no side is in, and `0x28D13` set
  `b1f` before the call. The join needs a credit to exist (`0x2C060`), not to
  pay one.
- **There is no coin key.** Writers of `DS_00105C00`: `scan.py 105C00` →
  code `0x2BF7F 0x2C318 0x2CA15 0x2CA31 0x2CA59 0x2CA6B 0x2CA8D 0x2CA9E`;
  Ghidra `search_instructions operand=105c00` → `0x2BF7D` read, **`0x2C317`
  store** (`0x2C304`, the init `((field 0x29 & 0xF0000) >> 16) + 1`),
  `0x2CA2F` read, `0x2CA57` read, **`0x2CA69 dec`**, `0x2CA8B` read,
  **`0x2CA9C sub`**. The raw hit `0x2CA15` is a read in `0x2CA10` (`push
  edx; xor eax,eax; mov edx,[0x105c00]; mov al,[0x105d60]; or; sete`), which
  is in no Ghidra function. **Nothing adds a credit**: "credits inserted
  during a fight" do not exist in this build; the count only falls (and is
  re-initialised by `0x2C304` at `game_state_init`, so at every restart and
  after game over, spec §3.4). The continue screen's credit test is the same
  `0x2C060`.

## §K.5 Spec §7 Q5 — keys under `game_frame`, and the blocking readers

The key loop is part of `game_frame` (`0x24C5C`) and runs once per master-loop
iteration in every mode (§K.2), so every in-match key is keyed by `f` like the
U1–U4 keys. The two blocking readers (`0x24E54`, `0x24A64`) are entered
inside that iteration and wait with `f` frozen and the ISR gated (§K.3). The
harness never needs `f` to advance inside them: **the answer is queued in the
same spin as its opener** (`('after', 0, …)`), so when the key loop reads the
opener the answer is already in the ring and the blocking read returns at
once, in the same iteration `c`. Evidence that same-spin words are consumed in
one iteration: the `gp-pads` chord (record §G.7.3: three words, one iteration
`0x3D5`). If the poller's two writes straddled the iteration, the blocking read
would still consume the answer with `f = c` (the poller's `H` record carries the
frozen `f`), `S(c)` would still show the last head, and `port_script` accepts
it (record §G.4 item 2); `gp_keys.py` checks that every word of an event has
the same consumption frame. **No tick-keyed step is added.** The pause's and
the prompt's waits with nothing queued (a real player's pause) are not
scripted: a named gap (§K.10).

## §K.6 The scenario `gp-keys-fight`, the events and their rules

The `gp-idle-loss` path to round 1 (its first three steps), then eleven events
in mode 6 / `0x17`, 10 frames apart, ordered so that **every event changes the
latch** (§K.3: an unchanged latch could not show the event):

| # | event | steps | latch before → after | other effect checked |
|---|---|---|---|---|
| 0 | enter | 3 | 0 → `0x0D` | mode unchanged (`0x24ECF` needs mode 3) |
| 1 | pause (space, space) | 4, 5 | `0x0D` → 0 | mode, `spz`, `mpz` unchanged |
| 2 | alt-s on | 6 | 0 → `0x1F` | `spz` 0 → 1, `mpz` unchanged |
| 3 | esc-n | 7, 8 | `0x1F` → 0 | mode, `spz`, `mpz` unchanged |
| 4 | alt-m on | 9 | 0 → `0x32` | `mpz` 0 → 1, `spz` unchanged |
| 5 | altq-n | 10, 11 | `0x32` → 0 | mode, `spz`, `mpz` (= 1, kept by `0x1D250`) unchanged |
| 6 | alt-s off | 12 | 0 → `0x1F` | `spz` 1 → 0 |
| 7 | alt-m off | 13 | `0x1F` → `0x32` | `mpz` 1 → 0 |
| 8 | f1 (`p1.start`, held 3) | 14 | `0x32` → `0x3B` | at `F + 1`: new `& 0x01000000`, `e0 & 0x0101 == 0x0101`; mode, `b1f` as at `F − 1` |
| 9 | f2 (`p2.start`, held 3) | 15 | `0x3B` → `0x3C` | first record in `F + 1 .. F + 9`: mode `0x17`, `b1f` = before `| 2`; `cred` unchanged |
| 10 | esc-y (in mode `0x17`) | 16, 17 | — | mode before ∉ {3, `0x27`}; **no record at `c`** (the iteration `0x24AB0` abandons never reaches its spin: `t50c − 1 == t508` needs `0x256C0`); the next record: mode 3, rng `0xABCD` (`0x20C53`/`0x20C62`), `cred` = the boot value, which differs from the value before |

`c` is each event's consumption frame (the `H` records paired FIFO with the
`I` presses, as `port_script` pairs them); `F` a pad event's first raw frame.
The `LOOKAHEAD` of 8 frames (harness) covers snapshot gaps at the join's mode
change (spec §3.7). Harness values: the 10-frame gaps (above the 1–4 iteration
BIOS latency of §G.7.3 plus `HOLD_FRAMES` 3), the 20 frames from F2 to the ESC
(inside the join's `0x78`-frame mode `0x17`, `0x28E75`), `time_limit = 90` s
(mode 6 at 55.1 s in `gp-idle-loss`, §G.18; the restart's boot movies 14.7 s
in `gp-pads`, §G.7.3: last mode-`0x27` `S` at `0x88A` ms 56161, first mode-3
`S` at `0x890` ms 70794).

**The restart and the replay driver.** A `0x65431` landing abandons its
iteration (record named-gaps-b §B.2): in the port the `game_loop_step` that
jumps raises the counter twice (`c` abandoned, `c + 1` run after the landing),
so the driver's "one `T` line per `f`" check must allow one missing `f` per
landing. The port gets a test seam `game_restart_landings()` (`flow.c`,
`PORT:`), counted in `game_loop()`'s landing branch; `test_restart_drive`
asserts it counts its one landing. Measured on the planner's dry run (§K.8):
without the seam the driver fails `1849 != 1850` (`test_game.c:12378`, the
unmodified tree; `test_game.c:12411` on `main` `8eaf25a`, re-baseline below); with it `test_gp_replay: 1 restart(s) landed` and all checks
pass; deleting the counter's increment fails both `test_restart_drive`
(`0 != 1`) and the replay (`1849 != 1850`).

## §K.7 The port against the raw

- `flow.c` `game_key_loop` (`0x24CFE..0x24EE7`), `game_quit_prompt`
  (`0x249F0`), `sound_music_pause_toggle` / `sound_sample_pause_toggle` /
  `sound_pause` / `sound_resume` (`0x1D1B0 0x1D220 0x1D250 0x1D270`) and the
  join `0x28CC8`/`0x28DA4` follow the listings above arm for arm (read
  side by side with §K.2–§K.4). Two deliberate omissions, both pre-existing
  and recorded: `0x5004A` (Alt-J, host-owned, `tools/port_classification.txt`
  row `5004A`) and `0x1B084` in ABANDON's yes (deferred, record §50-C).
- **The host binding is wrong (O10, `host.c:53-68`).** `host_key_bits()` sets
  kb bit `i` from `k_input_bind[i]` = `5 1 2 Up Down Left Right Space A S D F
  G H J K` and labels bit 0 "coin" and bits 1/2 the player starts. The raw
  layout (§G.1.2, every bit captured in `gp-pads` §G.7.2) is kb `0x0001` =
  P2 b0/start (Home, F2), `0x0002..0x0008` P2 b1..b3 (PgUp End PgDn),
  `0x0010..0x0080` P2 right/left/down/up (the arrows), `0x0100` P1 b0/start
  (U, F1), `0x0200..0x0800` P1 b1..b3 (I N M), `0x1000..0x8000` P1
  right/left/down/up (C Z X S). So in the windowed port Up presses P2's b3,
  '5' presses P2's start (the attract start `0x11F28` sees side 1's mask
  `0x100`), and no key is P1's up. There is no coin bit (§K.4). Only the
  windowed run reads it (every driver uses `host_set_key_bits_override`), so
  no oracle line can move. The plan's Task 7 (behind the user's decision)
  replaces the table with the default binding through a pure
  `host_kb_bit(scan)` (tested headless; the SDL scancode → set-1 scan table is
  the physical-keyboard named gap).
- `host.c translate_key` queues BIOS words for Esc, Enter, Space, Backspace,
  the arrows (`scan, 0`) and the letters (Alt-letters as `scan, 0`); not for
  F1/F2/Home/PgUp/End/PgDn. In a match those words only set the latch
  (§K.2), so the effect is a latch value the original would show and the port
  would not: named (§K.10), not fixed.

## §K.8 The planner's dry run (the port, not the original)

The port was driven with a synthetic script in the `gp-keys-fight` shape: the
`gp-idle-loss` script's three Enters (`port-script --end 2200`), then keys and
bits at hand-picked frames 10 apart from `2047` (mode 6 is first seen at
`0x7F5` = 2037 in `gp-idle-loss`), the pads held 3 frames, `end 2170`
(the generator `mk_dry.py` and the judge's poll.log builder `mk_drycap.py` are
in the plan's Task 3 Step 1). Build: the plan's Tasks 1–4
and 6–7 applied with `git apply` to a copy of `main` `e9271df` (the patch
pipeline equals the planner's scratch tree byte for byte). Results (`T` lines,
latch in hex):

| f | event | port state after the iteration |
|---|---|---|
| 2047 | enter | lat `0D`, mode 6 |
| 2057 | pause | lat 0 |
| 2067 | alt-s on | lat `1F`, spz 1; raw `0x80000000` from 2066, new/e0 `0x80000000`/`8080` at 2067 (P1 up) |
| 2077 | esc-n | lat 0, spz 1, mpz 0 |
| 2087 | alt-m on | lat `32`, mpz 1 |
| 2097 | altq-n | lat 0, mpz 1 |
| 2107 | alt-s off | lat `1F`, spz 0 |
| 2117 | alt-m off | lat `32`, mpz 0 |
| 2126–2127 | f1 | raw `0x01000000` at 2126; new `0x01000000`, e0 `0101` at 2127; lat `3B`; mode 6, b1f 1 |
| 2136–2137 | f2 | raw `0x100` at 2136; mode `0x17`, b1f 3, cred 4, lat `3C` at 2137 |
| 2157 | esc-y | no `T` line at 2157; 2158: mode 3, rng `0000ABCD`, cred 5, b1f 0 |

`gp_keys.py evidence` and `effects --min-effects 11` on a poll.log built from
that trace (the `T` records as `S`, `I`/`H` at the scripted frames): 11 of 11
events ok, `first not reproduced 11, ratchet N 11 ok`. Mutations (each
restored): the `T` line's `spz`/`mpz` swapped → `FAIL: first not reproduced 2
< ratchet N 11`; the pause's `config_screen_wait(-1)` deleted from
`game_key_loop` → `pause … FAIL: lat 20 at f=809, want 0`, `first not
reproduced 1`. The replay's `fn_resolve` miss set (scenario renamed
`gp-keys-fight`): the base pair plus `0x29D60 frontend_mode_1b_step`,
`0x5D812 frontend_mode_1b_step`, `0x3A588 fighter_state_3531c` (8 hits) — no
`0x23208` (the fight differs from `gp-idle-loss`'s after the keys); dropping
the `0x3A588` row fails the driver (`5 != 4`, `unexpected 0x3A588`). The
`gp-idle-loss` oracle with every change applied: `first unexplained 203,
ratchet N 203 ok`, `first differing 2088, ratchet N 2088 ok`, `0 restart(s)
landed`; the `gp-pads` replay `all checks passed`.

**Re-baseline on `main` `8eaf25a` (U5 and U6a merged, 2026-10-01).** The dry
run was repeated on a scratch copy of `8eaf25a` with the re-baselined plan's
patches (Tasks 1–4, 6, 7). Unchanged: `1849 != 1850` before the seam (now at
`test_game.c:12411`), `1 restart(s) landed` and all checks after it, the table
above row for row (lat, spz, mpz, b1f, cred, rng, new, e0 at every listed f),
`11 of 11` and `ratchet N 11 ok`, P2 (`first not reproduced 2`) and P3
(`pause f=809 FAIL: lat 20 at f=809, want 0`, `first not reproduced 1`).
Changed: the replay's miss set is the base pair plus `0x29D60
frontend_mode_1b_step` and `0x5D812 frontend_mode_1b_step` only (`distinct=4`):
U6a ported and registered `0x3A588` (record gameplay-u6 §U6.21), so Task 6's
table has two rows, and dropping the `0x29D60` row fails the driver
(`test_platform.c:204: 4 != 3`, `unexpected 0x29D60 from
frontend_mode_1b_step`). The `gp-idle-loss` oracle with every change applied:
`frames: first unexplained 2064, ratchet N 2064 ok`, `trace: 0 differing through
8319; ratchet N 8320 ok`, `0 restart(s) landed` — every `gp_compare` line equal
to the unmodified `8eaf25a`'s, and likewise `gp-u5-charsel`'s (516, 1513); the
`gp-pads` replay `all checks passed`.

**What the dry run does not show:** what the original does. The rules of
§K.6 are raw-derived; Task 5's capture is their evidence.

## §K.9 What the existing captures exercise

- `gp-pads` (`poll.log` 2520 lines): the mode-3 Enter, then the 18 pad names
  and one chord, each with its BIOS word (`1F73 … 3B00 … 3C00`), **all in
  mode `0x27`** (MAIN MENU): the key loop latched them and the menu's
  `0x2EEC8` cleared the latch. No ESC, space or Alt key; nothing in a match.
- `gp-idle-loss` (`poll.log` 9850 lines): three Enters (mode 3, `0x27`,
  `0x27`); nothing pressed from `0x26E` to the end (`I` records at `f = 13F,
  1D6, 26C` only). Nothing in a match.
- K11 `walk`, `menuesc`, `idle`, `diags`, `de`: the service menu (mode
  `0x27`), where the key loop's ESC, space and Enter arms do nothing (§K.2);
  `menuesc`'s ESC is the menu's own (`0x30430`, record named-gaps-b §B.3).
- So **no existing capture exercises any in-match key**; F1/F2 as pad bits
  are captured (Q2, MAIN MENU part), their in-match effect is not.

## §K.10 The claim and the named gaps

**Claim (narrow).** For the eleven events of §K.6, at the capture's own
frames: the original shows the raw-derived state change (evidence) and the
port shows the same one (effects, ratcheted). Judged fields only: `lat spz
mpz mode b1f cred rng new e0` and the presence of the `S`/`T` record at `c`.

Named gaps, each with its evidence:

1. **Physical keyboard path** (spec §7): claims start at the key-state table
   and the BIOS ring the harness writes. The SDL scancode → set-1 table of the
   windowed host is untested headless.
2. **The pause and prompt frames are not compared.** `0x2EA78(-1)` presents
   one frame with the text; this unit's ratchet is on state. When planned
   (`e9271df`) the path's frame claim stopped at the character select
   (`GP_IDLE_LOSS_MIN_FIRST` 203, divergence O1), long before the keys; on
   `8eaf25a` it reaches capture frame 2064 (mode 8 at `f = 0xC71`, after round
   1; record gameplay-u6 §U6.21), so the frames of the keys are now within
   reach of `gp_compare`. They stay unratcheted here: Task 6 Step 6 records
   the report-only lines, and a frame ratchet on `gp-keys-fight` is a
   follow-up.
3. **A pause or prompt left open** (nothing queued behind the opener) blocks
   with `f` frozen; v2 scripts cannot key it (§K.5).
4. **QUIT TO DOS's yes** (`0x24A93` → `0x256DD`): ends the process; not
   captured (it would end the capture before the restart event) and the
   replay driver has no model of the loop exit.
5. **Alt-J** (`0x5004A`): host-owned, not ported; not pressed (it times the
   game port).
6. **`0x1B084` in ABANDON's yes**: deferred (record §50-C); the config write is
   not observed.
7. **BIOS words for F1/F2/Home/PgUp/End/PgDn** are not queued by the windowed
   host (`translate_key`): latch-only in a match (§K.7).
8. **Configurable bindings**: the raw reads `+0x2DE..+0x2ED`, which the
   service menu can change; the host (after Task 7) follows the default
   binding only.
9. **Typematic repeat** is not modelled (spec §7 Q4); a held key queues one
   word.
10. **Joystick devices** (`+0x2D4`/`+0x2D6` ≠ 0, `0x1B890..0x1BB73`): not
    exercised.

## §K.11 Tool log (Tasks 1–4)

### Task 1: keys, extra fields, scenario
- `gp_session`: six keys (record §K.2), `format_s(..., fields)`, `KEYS_EXTRA` (record §K.3), `SCENARIOS['gp-keys-fight']` (record §K.6). `gp_capture`: `read_snap(..., fields)`, `Poller(fields=...)`, `main` passes `SNAP_FIELDS + extra`.
- Tests: before `KeyError: 'gp-keys-fight'`; after `Ran 44 tests … OK` (the plan's 41 + 3: U6b's Task 1, already on main, added three tests to `test_gp_session`). Mutations S1–S4 (PYTHONDONTWRITEBYTECODE=1, 44 tests each):
  - S1 `read_snap` over `gs.SNAP_FIELDS`: `ERROR: test_extra_fields_are_read_and_logged`, `FAILED (errors=1)`.
  - S2 `format_s` over `SNAP_FIELDS`: `ERROR: test_extra_fields_are_read_and_logged`, `FAILED (errors=1)`.
  - S3 `'alt-s': (0x1F, 0x1F73)`: `FAIL: test_keys_are_bios_make_words`, `FAILED (failures=1)`.
  - S4 step 8 `('after', 1, ...)`: `FAIL: test_an_answer_fires_with_its_opener`, `FAILED (failures=1)`.
- Raw check (fixed-up image, capstone base 0x10000): the latch store is `0x24D4D mov [0x105F30], eax`; the Alt-letter arms test `bl` (the scan) at `0x24D99`..`0x24DB8` (0x1F -> `call 0x1D220`, 0x32 -> `call 0x1D1B0`, 0x10 -> `call 0x249F0`); `0x1D1B0` toggles `[0x1028DA]` (not an xor: writes 0 at 0x1D1C2 / 1 at 0x1D213; see Task 2) (music pause, `mpz`), `0x1D220` toggles `[0x1028DB]` (samples pause, `spz`). No correction to the plan. The `KEYS_EXTRA` comment names the two functions, not the bytes.
- Merge note: U6b's five fields are already in `SNAP_FIELDS`; `KEYS_EXTRA` follows them in every `S` line of `gp-keys-fight`. This task does not touch `test_game.c`'s `T` line (Task 3).
- Compat: `make gp-oracle gp-charsel-oracle` after the change: every `gp_compare` line, the `landed` lines and `all checks passed` identical to the Task 0 baseline.

### Task 2: gp_keys.py
- `tools/gp_keys.py {evidence|effects} --capture DIR [--port DIR] [--min-effects N] [--capture-sha256 HEX]`: one row per event of `gp_keys.EVENTS`, judged by `judge(rule, c, F, rec, boot_cred)` from the same frames (`c` from the `I` presses paired FIFO with the `H` records, `F` a pad event's first raw frame with its kb bit) on the capture's `S` records (`evidence`) or the port's `T` records (`effects`, the ratchet: the events before the first one the port does not reproduce must number at least N). Absent capture: `skipped`, exit 0; unpinned `--min-effects`, N above the 11 events, an empty or another `--capture-sha256`: exit 1. Rules as record §K.6's table: latch rules (`latched`/`cleared`/`toggled`) need the latch to differ before the key, the mode unchanged and the pause bytes flipped or kept; `b0` reads `new`/`e0` at `F + 1`; `join` reads the first record in `F + 1 .. F + 9` (mode `0x17`, `b1f | 2`, `cred` kept); `restart` needs no record at `c` and the next record at mode 3, rng `0xABCD`, boot `cred`.
- Tests: before `ModuleNotFoundError: No module named 'gp_keys'`; after `Ran 83 tests ... OK` for `test_gp_keys` + `test_gp_session` + `test_gp_capture` + `test_gp_compare` (the plan's 80 + 3: U6b Task 1's tests, already on main; 13 in `test_gp_keys`). The Makefile's seven-module tool-test line is 110 tests and 123 with `test_gp_keys` (the Makefile gains `tools.tests.test_gp_keys` in Task 4). Mutations M1-M9 (`PYTHONDONTWRITEBYTECODE=1`, 13 tests each): M1 `FAIL: test_an_unchanged_latch_cannot_show_an_event`; M2 `FAIL: test_a_restart_must_abandon_its_iteration`; M3 `FAIL: test_the_ratchet_fails_below_n_and_unpinned`; M4 `FAIL: test_another_capture_fails_the_pin`; M5, M6, M7, M8 `FAIL: test_each_rule_can_fail`; M9 `FAIL: test_another_capture_fails_the_pin`; each `FAILED (failures=1)`, restored run `Ran 13 tests ... OK`.
- Fold-in (Task 1 review, k1): `test_an_answer_fires_with_its_opener` now asserts the whole ordered action list of steps 9-17 with their `after` gaps (10, 10, 0, 10, 10, 10, 10, 20, 0), the `until_mode` step 18 and the step count, and walks `Schedule` through them (frames `frame_of[8] + cumulative gap`). Mutations (both in `SCENARIOS['gp-keys-fight']`, each `FAIL: test_an_answer_fires_with_its_opener`, restored): step 12 `alt-s` -> `alt-m`; step 16's gap 20 -> 10.
- Fold-in (k2): the `KEYS_EXTRA` comment names the toggle sites. Raw check (fixed-up image, capstone base 0x10000, file offset = VA - 0x10000): `0x1D220 xor byte [0x1028DB],1` (then `0x1D229 mov al,[0x1028DB]`, `cmp eax,1`, `je 0x1CD9C`); the music byte has no xor: `0x1D1B0` reads `[0x1028DA]`, if 1 writes 0 at `0x1D1C2 mov [0x1028DA],dl` (dl = 0 from `0x1D1C0`), else writes 1 at `0x1D213 mov byte [0x1028DA],1`. Correction to Task 1's note: `0x1D1B0` is a conditional set/clear, not an `xor`; the flip the rule checks (`mpz` 0 -> 1 -> 0) is the same.

### Task 3: the port side
- Seam: `game_restart_landings()` (`flow.c`/`flow.h`, `PORT:`), incremented in `game_loop`'s `setjmp` landing branch; `test_gp_replay` counts the landings (`const u32 landings0`), prints `test_gp_replay: <n> restart(s) landed` and checks `gp_trace_lines + landed == end - enter_frame + 1`; `test_restart_drive` asserts the seam counts its one landing. The `T` line ends `... ent=%08X r0=%02X r1=%02X c0=%02X c1=%02X s0_43=%02X lat=%08X spz=%02X mpz=%02X` (U6b's five, then `KEYS_EXTRA`: `DS_00105F30` DSD, `DS_001028DB` DSB, `DS_001028DA` DSB). The brief's `test_game.c` hunks were re-anchored by content (U6b moved those lines; `12415` replaces `12411` for the old failing check, `12714`/`12426` replace P1's `12710`/`12422`).
- Red (Step 2, unmodified port): `FAIL test_game.c:12415: 1849 != 1850`, `FAILURES: 1`. Python: `test_the_port_t_line_writes_every_snap_field_in_order` failed on the longer T line before its change (expected names `SNAP_FIELDS` only).
- `tools/tests/test_gp_session.py` `test_the_port_t_line_writes_every_snap_field_in_order`: names now `SNAP_FIELDS + KEYS_EXTRA`; plus placeholder widths (2 hex digits per byte, all fields) and, for `KEYS_EXTRA`'s three, the accessor by size and `DS_%08X` address (the last three `(unsigned)DSx(DS_...)` arguments). The test on this branch checked names only; the width/address/accessor checks did not exist and are new here. Mutations (each `FAIL` of that test, restored `Ran 28 OK`): drop `mpz` from the format and the argument; swap `DS_001028DB`/`DS_001028DA`; `mpz=%04X`.
- Green (Step 4): `test_gp_replay: 1 restart(s) landed`, `all checks passed`; `PR_RESTART`: `all checks passed`; `evidence: 11 of 11 events ok`; `effects: first not reproduced 11, ratchet N 11 ok`; the §K.8 table (lat `D 0 1F 0 32 0 1F 32 3B 3C`, 2157 `-`, 2158 `mode=3 lat=0 ... cred=5 rng=0000ABCD`; spz/mpz flips 2067/2087/2107/2117) reproduced.
- Mutations: P1 (delete the increment): `PR_RESTART` `FAIL test_game.c:12714: 0 != 1`; replay `0 restart(s) landed`, `FAIL test_game.c:12426: 1849 != 1850`. P2 (swap the two byte arguments in `gp_trace_line`): replay still passes, `effects: FAIL: first not reproduced 2 < ratchet N 11`. P3 (comment out the pause's `config_screen_wait(-1);`, flow.c `0x24E46/0x24E4B`): replay passes, `effects: FAIL: first not reproduced 1 < ratchet N 11`. All restored.
- Gate (light, without `gp-keys-oracle`): `all checks passed` twice, `gp-exit=0`, `GP-IDLE-LOSS-EQUAL`, replays `0 restart(s) landed`, `gp-idle-loss frames 2064 / trace 8320` ok, `gp-u5-charsel 516 / 1513` ok, `diff-verify: 6/6 ... 7/7`, tool tests `Ran 123 tests OK` (the plan's 120 + 3 from U6b Task 1), `771 1203 64`, `731 731 100`.

### Task 4: make gp-keys-oracle
- `make gp-keys-oracle` (also under `PR_ORACLE_REQUIRED=1`) without `data/k11-captures/gp-keys-fight`: `gp-replay: no capture at ...` then two `gp_keys: gp-keys-fight: no capture at data/k11-captures/gp-keys-fight, skipped` (the gp oracles skip even when oracles are required, gameplay spec §4.3); `make help` shows `gp-keys-oracle  In-match keys: evidence + effects ratchet on gp-keys-fight (...)`. `.PHONY` was re-anchored by content (main's continuation line now ends `entry-triage`; `gp-keys-oracle` appended after it); the recipe follows `gp-charsel-oracle`'s, `verify` calls it after `gp-charsel-oracle`. `GP_KEYS_MIN_EFFECTS` and `GP_KEYS_CAPTURE_SHA256` are empty until Task 6 (an unpinned value fails with a capture present).
- Tool-test line (`verify`): `tools.tests.test_gp_keys` added; this branch has no `test_gp_moves` module on the line (U6b's later tasks add it at merge). Gate: `Ran 124 tests OK` = the plan's 120 + 3 (U6b Task 1 in `test_gp_session`) + 1 (the k5 test below).
- Fold-in k5 (review minor): `judge_all` took `boot_cred` from the first record of the side being judged, so the effects judge derived the restart's expected credits from the port's own trace. It now takes the capture's first `S` record for both judges. Test `test_the_port_cannot_supply_its_own_boot_cred`: the capture boots with 5 credits; a port record set whose first record and post-restart record say 7 fails the restart (`want 3/<seed>/5`); with the old derivation (`rec[min(rec)]`, mutated in) the same test fails (`[] != ['esc-y']`); restored, 14 tests OK. The real capture's first `S` record is the boot value only if the poller's first record precedes any credit change; Task 5/6 check this on the real capture.
- Fold-in k8 (review minor): `test_gp_replay` read `landings0` right after `game_init()`; it is now taken at the arm point (`enter_now`, with `gp_armed = 1`), so only landings inside the armed window excuse a frame, and a `CHECK` that the count was taken (sentinel `0xFFFFFFFF`) fails if the arm is never reached. Behaviour unchanged: the dry run prints `1 restart(s) landed`, `all checks passed`, evidence `11 of 11 events ok`, effects `first not reproduced 11, ratchet N 11 ok`; `PR_RESTART` `all checks passed`; the replays of gp-idle-loss and gp-u5-charsel print `0 restart(s) landed`. No mutation proves k8 itself: no pre-arm landing exists in any replay, so the change is a guard, not a measured behaviour.
- Light gate (with `gp-keys-oracle`): `all checks passed` twice, `gp-exit=0`, `GP-IDLE-LOSS-EQUAL`, ratchets 2064 / 8320 / 516 / 1513 ok, `diff-verify: 6/6 ... 7/7`, `771 1203 64`, `731 731 100`.

## §K.13 The host binding (Task 7)

**The raw truth (§K.7).** Fixed-up image (`build/diffrun --exe data/game/C/PRAGE.EXE --image-out`, capstone base `0x10000`, file offset = VA - `0x10000`), re-read for this task:
- The default config words are at linear `0xA2C62` (DS offset `0x22C62`; `0x1AE20` is `mov eax,0xA2C62` into `0x1AE28`): `00 1f73 2d78 2c7a 2e63 1675 1769 316e 326d 00 4800 5000 4b00 4d00 4700 4900 4f00 5100 64 64`. Their high bytes are the scans `+0x2DE..+0x2ED` hold: `1F 2D 2C 2E 16 17 31 32 | 48 50 4B 4D 47 49 4F 51`. **Correction:** the records (§K.2, gameplay-ground-truth §G.1.2 prose, named-gaps A) and this task's brief write the address as `0x122C62`, which is outside the image (data object ends `0x10B0CF`); it is `0xA2C62` (the `mov eax,0xA2C62` at `0x1AE20` and the §G.1.2 file offset `0xA2C62 + 0x46E54` agree). The `host.c` comment says `0xA2C62`.
- `0x1B610` reads `+0x2DE..+0x2E1` into `0x80 0x40 0x20 0x10` (`0x1B62A..0x1B691`); `0x1B730` `+0x2E2..+0x2E5` into `1 2 4 8`; `0x1B850` returns bit 0 from `[+0x28F] & 0x80` clear (`0x254 + 0x3B`, F1); `0x1B6A0` (`+0x2E6..+0x2E9`), `0x1B7C0` (`+0x2EA..+0x2ED`) and `0x1B870` (`+0x290`, F2) are the same for P2. `0x1BD0E call 0x1b610 .. 0x1BD43 call 0x1b730 .. 0x1BD4A call 0x1b850 .. 0x1BD5B mov [edx+0x2d8],al` and `0x1BD84 call 0x1b6a0 .. 0x1BDB4 call 0x1b870 .. 0x1BDC0 mov [eax+0x2d9],dl`: kb word = `(+0x2D8 << 8) | +0x2D9`.
- All 18 scan -> bit pairs of `host_kb_bit` match: S `1F` 0x8000, X `2D` 0x4000, Z `2C` 0x2000, C `2E` 0x1000, U `16` 0x0100, I `17` 0x0200, N `31` 0x0400, M `32` 0x0800, F1 `3B` 0x0100, Up `48` 0x0080, Down `50` 0x0040, Left `4B` 0x0020, Right `4D` 0x0010, Home `47` 0x0001, PgUp `49` 0x0002, End `4F` 0x0004, PgDn `51` 0x0008, F2 `3C` 0x0001. No other correction.

**The change.** `host.c`: `k_input_bind` (and its "coin" bit 0, which does not exist, §K.4) is gone; `host_kb_bit(u8 scan)` is the pure default binding (a `PORT:` host helper, no ported function: `port_progress.py` stays `771 1203 64` / `731 731 100`); `host_key_bits()` ORs `host_kb_bit(k_bios_letter[i])` over the pressed letters and `host_kb_bit(k_bios_pad[i].scan)` over the pressed Up/Down/Left/Right/Home/PgUp/End/PgDn/F1/F2. `host.h` states it. `port/spec/game_flow.md`'s one stale sentence ("host.c's `k_input_bind` table; bit 0 = coin") now names `host_kb_bit`.

**Tests (`test_platform.c`, top of `test_host`; the plan's hunk `@@ -2611` applied as written, no re-anchor needed).** 21 new `CHECK_EQ_INT`: the 18 pairs plus Q `0x10`, Alt `0x38` and `'5'` `0x06` giving 0. Red: with only the test applied the build fails (`call to undeclared function 'host_kb_bit'`, three errors). Green: `all checks passed`. Mutations (each restored, `all checks passed` after): F2 `0x0001u` -> `0x0100u`: `FAIL test_platform.c:2621: 256 != 1`, `FAILURES: 1`; `default: return 0u;` -> `return 1u;`: `FAIL test_platform.c:2622/2623/2624: 1 != 0`, `FAILURES: 3`.

**Gate (light, with `gp-keys-oracle`).** `all checks passed` twice (plain and `PR_RESTART=1`), `gp-exit=0`, `GP-IDLE-LOSS-EQUAL`, ratchets 2064 / 8320 / 516 / 1513 ok, `gp_keys` skips (no capture yet), `diff-verify: 6/6 ... 7/7`, `Ran 124 tests`, `771 1203 64`, `731 731 100`. No oracle line moves (every driver uses `host_set_key_bits_override`). The windowed binary was not run (headless tests only).

**Named gaps that stay.** (1) The SDL scancode -> set-1 scan table (letters through `k_bios_letter`, the ten pad keys through `k_bios_pad`) is untested headless; `host_key_bits()` reads `SDL_GetKeyboardState`, which no test seeds. (2) Configured bindings: only the default is followed (§K.10 item 8). (3) `translate_key` still queues no BIOS words for F1/F2/Home/PgUp/End/PgDn (§K.7, §K.10 item 7). (4) Space, Enter and the digits are no longer pad bits: the windowed attract/menus that relied on '1'/'2'/'5'/Space as start keys now start with U/F1 (P1) and Home/F2 (P2), as the original's default binding does.
