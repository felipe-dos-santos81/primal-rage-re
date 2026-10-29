# State Coverage — derivation record

- **Plan:** `docs/superpowers/plans/2026-10-28-state-coverage.md`
- **Spec:** `docs/superpowers/specs/2026-10-28-state-coverage-design.md`
- **Branch / worktree:** `state-coverage` at
  `/home/felipe/.local/share/opencode/worktree/a7f742/state-coverage`
- **Base commit:** `3a2bc47a` (`Merge branch 'claim-coverage'`), on the closed
  claim-coverage tip.

This record is the raw-byte and tool-output source for the phase's tasks. Each
task appends its own section (§D2 Task 2, …); §D1 is Task 1's measured baseline
and every later task cites it. Nothing in §D1 changes port code or tool
behaviour: it is measurement and documentation only.

---

## §D1 The measured baseline (Task 1)

### §D1.1 Environment

| item | value |
|---|---|
| host | Linux, 20 CPUs (`nproc` = 20) |
| build | `build/` already present in this worktree; every `make` target rebuilt only what it needed |
| `data/` | symlink to the shared tree; `data/game` read-only, no capture written |
| `port/tests/ghidra_data.bin` | present (copied in per the AGENTS.md worktree rule) |
| dependencies | `unicorn` 2.1.4, `capstone` 5.0.7 present, so `diff-verify` and `entry-triage` run |
| `./build/run_tests` | green (per the controller's pre-flight; not re-run here — §D1 does not call `game_init`) |

`make verify` was **not** run in Task 1 (it is ~26 minutes and the mid-phase
gate owns it). The steps below are the gate's components, run individually on
the unmodified tree, each from the repo root.

### §D1.2 The counters (plan Step 1)

```text
$ python3 tools/port_progress.py
798 1203 66
759 759 100
ported 798 (759 portable + 39 runtime)  host-owned 41  deferred 12  replaced 37  runtime 315
```

`python3 tools/port_progress.py --unported | wc -l` prints **318** lines = 3
header lines + **315** unported rows, every one `runtime` (≥ `0x5D000`); the
portable half has nothing unported. This is the same shape the claim-coverage
record §C1.2 measured (the `0x62003` false positive was removed by that phase,
so the raw count reads 798, not 799).

```text
$ make diff-verify            # log /tmp/opencode/t1-diff-verify.log, EXIT:0
diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps;
825/845 rows with callees closed (188 have none); 809/845 closed on full rows; 8 ported
functions with no row (45 FN_ ported-unrowed: 8 portable [5 registry-visible + 3 header-shape
exceptions] + 37 runtime; the scan-visible subset is 42 = 5 + 37). The closure rule (record J
§J1): an allow-mode callee outside the portable set - runtime >= 0x5D000, host-owned/deferred
in tools/port_classification.txt, or an address the decomp index never lists as a function -
does not keep a row open, provided the port tree does not implement the address in any class
(no fn_register, no column-0 `/* 0xADDR` header), while a recorded stub or real callee and a
ported function without a row do. Claim: equivalence on the exercised blocks and inputs only,
each function with its callees stubbed or run as stated.
```

**Plan-vs-raw ruling (R10): the full-rows counter is `809/845`, not `807/845`.**
The tool's printed line is `809/845 closed on full rows`. Design §2
(`docs/superpowers/specs/2026-10-28-state-coverage-design.md:103`) reads `809/845`
and **agrees** with the tool. `AGENTS.md:594` (the 2026-10-24 proof-completion
paragraph) still reads `807/845` — that is the stale figure (claim-coverage Task
6 already moved it 807 → 809; record `2026-10-27-claim-coverage-derivations.md`
§C1.10). The other numbers agree everywhere: `1033/1033`, `14993/14993`,
`0 named gaps`, `825/845`, `8 ported functions with no row`.

The three differential lists, re-read from the modules (not retyped):

```text
$ python3 - <<'EOF'   # WEAKER / UNROWED from tools/diff_verify.py
import importlib.util
spec = importlib.util.spec_from_file_location('dv','tools/diff_verify.py')
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
print("WEAKER:", len(m.WEAKER), ' '.join('%05X'%a for a in sorted(m.WEAKER)))
print("UNROWED:", len(m.UNROWED), ' '.join('%05X'%a for a in sorted(m.UNROWED)))
print("UNROWED_HEADER_SHAPE:", len(m.UNROWED_HEADER_SHAPE), ' '.join('%05X'%a for a in sorted(m.UNROWED_HEADER_SHAPE)))
EOF
WEAKER: 13 1AFE8 1B084 1B544 1C740 1D0BC 249F0 24C5C 255CC 2CADC 2EA30 2EA68 2EA74 2EA78
UNROWED: 8 1B120 1B3AC 1BEC4 1C930 1CF40 20C10 47370 4FBA2
UNROWED_HEADER_SHAPE: 3 1B120 20C10 4FBA2
```

`WEAKER` reads **13** (design §2's figure; claim-coverage Task 6 moved it 15 →
13), `UNROWED` **8** (three of them `UNROWED_HEADER_SHAPE`). The 20 open rows
(`825/845` means 845 − 825 = 20 open) over **19 distinct addresses** (the
`TERMINAL_CENSUS` of record §C1.2.2, unchanged by claim-coverage Task 6) are the
counter's implied remainder; Task 13 (§D7) owns the per-row declaration, not
this baseline.

```text
$ make gate-check
gate: captures 34/35 present, 1 missing (strict=1, pins_exact=1)
gate: declared gap (absent): data/k11-captures/gp-u14-win-live
capture_manifest: 34 present checked, 1 declared skipped, 0 problems
capture_manifest: pin baseline ok (digest 069edcea8904b152b954dd4c81699399afcb7cc66fee7e452a9c990a527ae2b7)
capture_manifest: verify preflight ok
```

`34/35 present`, `gp-u14-win-live` the declared absence; the capture manifest,
pin baseline and verify preflight all pass.

```text
$ make entry-triage           # log /tmp/opencode/t1-entry-triage.log, EXIT:0
entry-triage: 736 candidates (732 by U0's rule); finisher=9 move-callback=71 span-writer=388
  call-table=7 anim-target=112 mid-instruction=3 data=75 code-immediate=15 direct=2 interior=6 data-pointer=48
entry-triage: targets 390 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
```

The committed table equals a fresh run (`--check` printed no `differs` line), so
`736` candidates (`732` by U0's rule), `span-writer=388` (the claim-coverage
Task 5 figure; record §C1.5.1) is the current reading.

### §D1.3 The mode census (plan Step 2)

The disk holds **40 `poll.log` files across 39 top-level capture directories**:
`data/k11-captures/save-reload/boot1/poll.log` is the *second boot* of the
`save-reload` scenario, nested one level down, so the glob
`data/k11-captures/*/poll.log` matches the **39** top-level scenario dirs and
the `find` count of `poll.log` files is **40**. The census below scans the 39
top-level `poll.log`s (the design's own `data/k11-captures/*/poll.log` glob;
the nested boot adds no `P` records not already in its sibling).

```text
$ python3 - <<'EOF'
import glob, re, collections
mode_records = collections.Counter(); mode_files = collections.defaultdict(set)
for f in sorted(glob.glob('data/k11-captures/*/poll.log')):
    scen = f.split('/')[2]
    for line in open(f, errors='ignore'):
        if line.startswith('P '):
            v = int(re.search(r'mode=([0-9A-Fa-f]{4})', line).group(1), 16)
            mode_records[v] += 1; mode_files[v].add(scen)
presented = sorted(mode_records)
never = sorted(set(range(0x34)) - set(presented))
print('union %d modes: %s' % (len(presented), ' '.join('%02X'%m for m in presented)))
print('never %d: %s' % (len(never), ' '.join('%02X'%m for m in never)))
EOF
union 35 modes: 00 03 05 06 07 08 09 0C 0D 0F 10 11 12 13 14 15 16 17 1A 1B 1E 1F 22 23 24 27 28 29 2A 2B 2C 2D 2E 30 31
never 17: 01 02 04 0A 0B 0E 18 19 1C 1D 20 21 25 26 2F 32 33
```

**35 of 52 table modes exhibited**; the never-presented set is exactly the
design's `01 02 04 0A 0B 0E 18 19 1C 1D 20 21 25 26 2F 32 33` (17 modes). The
mode values are 0x00..0x33 (52); the highest *presented* mode is `0x31`.

Per-mode `P`-record count and distinct-file count (the 35 presented modes):

| mode | P records | files | | mode | P records | files |
|---|---|---|---|---|---|---|
| 00 | 1075 | 39 | | 17 | 78 | 29 |
| 03 | 12086 | 39 | | 1A | 103 | 30 |
| 05 | 52 | 28 | | 1B | 103 | 30 |
| 06 | 45 | 28 | | 1E | 26 | 10 |
| 07 | 8 | 6 | | 1F | 14 | 7 |
| 08 | 18 | 18 | | 22 | 7 | 7 |
| 09 | 15 | 15 | | 23 | 7 | 7 |
| 0C | 49 | 7 | | 24 | 7 | 7 |
| 0D | 49 | 7 | | 27 | 7749 | 38 |
| 0F | 7 | 7 | | 28 | 1 | 1 |
| 10 | 30 | 30 | | 29 | 1 | 1 |
| 11 | 29 | 29 | | 2A | 1 | 1 |
| 12 | 12 | 12 | | 2B | 2 | 2 |
| 13 | 3 | 3 | | 2C | 1 | 1 |
| 14 | 10 | 10 | | 2D | 22 | 22 |
| 15 | 40 | 10 | | 2E | 1 | 1 |
| 16 | 17 | 17 | | 30 | 1 | 1 |
| | | | | 31 | 1 | 1 |

The two content-bearing never modes this phase's tasks cite are confirmed
absent from every capture: `0x21` (volleyball, in 0 files), `0x25` (camera
sequence, 0 files), `0x0E` (continue screen, 0 files), `0x32`/`0x33`
(ENDURANCE end, 0 files — `0x30`/`0x31` are present in `gp-u12-endurance`
only), and `0x0A`/`0x0B` (the drawn round, 0 files). `0x04` is the design's
"shares its body with exhibited modes" entry (the `0x26254` fight frame), and
`0x2F` the byte-duplicate of `0x2E`.

### §D1.4 The seam/registration split (plan Step 3)

```text
$ python3 - <<'EOF'
import glob, re, sys
sys.path.insert(0, 'tools'); import port_progress as pp
RUNTIME_BASE = 0x5D000
fns = pp.fn_addresses(); ported = pp.ported_set()
portable = sorted(a for a in fns if a < RUNTIME_BASE and a in ported)
reg = set(); seam = set()
for f in glob.glob('port/src/**/*.c', recursive=True) + glob.glob('port/src/**/*.h', recursive=True):
    if f.endswith('symbols.h'): continue
    t = open(f, errors='ignore').read()
    reg |= {int(m,16) for m in re.findall(r'fn_register\(\s*0x([0-9A-Fa-f]+)', t)}
for f in glob.glob('port/src/**/*.c', recursive=True):
    t = open(f, errors='ignore').read()
    for m in re.finditer(r'PR_SEAM(?:_RET)?0?\(\s*(0x[0-9A-Fa-f]+)', t):
        seam.add(int(m.group(1),16))
reg &= fns; seam &= fns
inst = seam | reg
print('portable', len(portable), 'fn_register', len([a for a in reg if a<RUNTIME_BASE]),
      'seamed', len([a for a in seam if a<RUNTIME_BASE]),
      'instrumented', len([a for a in portable if a in inst]),
      'unseamed', len([a for a in portable if a not in inst]))
EOF
portable 759 fn_register 32 seamed 610 instrumented 631 unseamed 128
```

**631 of 759 portable addresses are instrumented, 128 are not** — exactly the
design's recon. "Instrumented" is the union of (a) a `PR_SEAM`/`PR_SEAM0`/
`PR_SEAM_RET`/`PR_SEAM_RET0` invocation whose first argument is the address
(610 addresses) and (b) an `fn_register(0xADDR` call (32 addresses); 11
addresses are in both, so the union is 631. The 128 unseamed addresses carry
only a column-0 `/* 0xADDR` header (or a declared `PORT_PROGRESS_EXCEPTIONS`
entry — 7 of the 128; the other 121 carry a column-0 `/* 0xADDR` header in a
`.c` file (114) or a `.h` file (7)). The majority is in `game/` (105 of 128:
98 in `.c` files plus 7 declared in `fighter.h`/`flow.h`):

| file | count | | file | count |
|---|---|---|---|---|
| game/fighter.c | 33 | | game/svcmenu.c | 3 |
| game/flow.c | 20 | | platform/sprite.c | 2 |
| game/camera.c | 14 | | platform/res.c | 2 |
| game/config.c | 14 | | platform/input.c | 2 |
| platform/render.c | 6 | | game/menu.c | 2 |
| game/actors.c | 5 | | game/effects.c | 1 |
| game/fight.c | 5 | | game/nameentry.c | 1 |
| platform/gfx.c | 4 | | game/fighter.h | 4 |
| game/flow.h | 3 | | *(exception, no header)* | 7 |

The 128 addresses (Task 2 adds one `PR_HIT` line to each):

```text
11BCC 12C7C 12CD4 12DF0 12E3C 13290 1333C 13B3C 13F68 14080 14268 15B90 15F48 15FD4 16308 164C0 16734 17EEC 18460 18BD4 19958 1AB10 1AEE0 1AF64 1B120 1B3AC 1B544 1BE30 1BEC4 1C350 1C390 1C3A0 1C3D0 1C458 1C500 1C528 1C65C 1C930 1CAB8 1CEBC 1CED4 1CF40 1D0BC 1D4D0 1E824 20C10 21F30 22B28 24754 255CC 25FDC 2604C 27BA4 29C20 29D60 2BE5C 2BF00 2CA2C 2D2F0 2D498 2D4B4 2D62C 2DB58 2EA30 2EA64 2EB80 2EDE0 2F41C 2F464 2F9CC 2FA40 2FE40 30788 3157C 319B0 31A78 31E28 32968 32970 336C0 33714 33734 33904 33950 339AC 33A10 33A68 33ACC 35050 3605C 380C4 38BB0 39AC8 39B14 39B30 3A280 3A2A0 3AFC4 3B038 3B9D8 3C1C8 3C570 3C6E8 3C758 3C800 3D0C0 3F308 3F6F4 40608 408E8 413C8 4248C 4454C 45908 46504 46BBC 47370 48C4C 4DBB4 4F200 4F228 4F714 4FBA2 500BB 50161 50188 51F45 51F72
```

### §D1.5 The three open measurements (plan Step 4)

#### §D1.5.1 The U16 series (the `PR_GP_DUMP` trace vs the `.failed` poll.log)

Port side: `make gp-replay scenario=gp-u23-u16-finisher` (EXIT:0; script 257
lines; `fn-miss PR_GP_DUMP distinct=4 dropped=0`, the §F5.4 set), the port's
`trace.txt` `T` records carrying `s0_30`. Original side:
`data/k11-captures/gp-u24-u16-state.failed/poll.log` `S` records carrying
`s0_30`. Compared frame-by-frame over `0x14F0..0x1533`:

```text
0x14F0..0x14FD  port == original at all 14 frames (0x0B40, then 0x08C0, 0x0900, 0x0980, 0x0A80)
0x14FE..0x1500  original drops to 0xFFFFFD00; port stays 0x0A80
0x1501          port 0x0D40  vs  original 0xFFFFFBC0  (0x1180 apart, signed)
0x1509          port 0x0DC0  vs  original 0xFFFFFC40
0x150F          port 0x11C0  vs  original 0x00000040
0x1513..0x152B  port 0x19C0  vs  original 0x00000840
0x152C..0x1531  port 0x1B40  vs  original 0x000009C0  (0x1180 apart)
0x1532          original S record absent (poller gap)
0x1533          original 0x1180 (exit); port 0x22F4 at the same f
```

The original is exactly **`0x1180` below** the port at every sampled frame
`0x1501..0x1531` (the original's negative values read as signed: `0xFFFFFBC0`
= −`0x440`, and `0x0D40 − (−0x440)` = `0x1180`; `0x1B40 − 0x09C0` = `0x1180`).
The port's own `s0_30` reaches `0x1180` (its case-3 exit, `s0_54` clears) at
`f=0x155C` (and holds `0x155D`/`0x155E`); the original reaches `0x1180` at
`f=0x1533` (and holds `0x1534`/`0x1535`). The original's poller gaps are
`0x1504`, `0x151B`, `0x151C`, `0x151F`, `0x1532` — exactly the design §7.3
set. This is the fought-match record §F5.6's series, reproduced on the current
tree.

#### §D1.5.2 The 5120 band (U13 `--end-to-last-s`)

```text
$ make gp-replay scenario=gp-u13-ending2 GP_SCRIPT_ARGS=--end-to-last-s   # EXIT:0
```

The dump is **4,953** `.ipx` frames (`frame_00000.ipx`..`frame_04952.ipx`;
`frames.txt` 4,953 lines, `trace.txt` 9,765 lines; `fn-miss distinct=4`). The
capture holds 5,904 raw frames. Measured against capture 5120:

- The band is rows 1..4 (4 × 320 = 1280 pixels, 3840 bytes). **Rows 1..4 are the
  dump's only unheld rows** — no port frame holds any of them, while every other
  row 0..199 is held by at least one frame (row 0 by three, rows 5..199 in full
  by one).
- Exactly **13 pixels** differ from port 4317's rows, in the exact design split
  **8 / 1 / 4**:
  - 8 pixels are `00001c` where port 4317's index is `0x32` (black in that
    frame's DAC): row 1 `23, 24, 295, 296`; row 2 `22, 297`; row 3 `21, 298`;
  - 1 pixel is black where the index is `0x2E` (`100000`): row 3 `74`;
  - 4 pixels are black where the index is `0x4D` (`00000c`): row 4
    `27, 28, 291, 292`.
- `band_pair(5120, top_row=1, band_hi=5)` returns **4315** (the unique adjacent
  pair is port 4315 top / port 4316 bottom); `band_explain` confirms the pinned
  band sha `56b900b1…`.
- Neighbour classification: capture 5119 is **clean at port 4314**, capture 5121
  is a **splice of (4316, 4317)**.

**Plan-vs-raw correction (the port indices are off by one).** The `f` values
agree exactly with the design §1.4/§2 prose, but the *port indices* do not:
`frames.txt` maps `04314 f=1824`, `04315 f=1825`, `04316 f=1826`,
`04317 f=1827`, `04318 f=1828`. So "5119 clean at f=1824" is port **4314** (the
design's "4315"), the band sits at the f=1825/f=1826 step bound by **(4315,
4316)** (the design's "4316, 4317"), and 5121 splices **(4316, 4317)** (the
design's "4317, 4318"). The design's port numbers are one high; its `f` values
and its 13-offset / 8-1-4 measurement are exact. The likely cause is the dump
length the design §2 and the pinned comment straddle (`gp_compare.py`'s
`BANDS_ALLOWED` comment still says "4954 with `--end-to-last-s`"; the current
tree produces 4,953), so the historical one-frame-later numbering shifted by
one. The `band_pair` result is computed, not pinned, so no oracle pin is
affected; only the prose indices disagree.

#### §D1.5.3 The OPL facts

- **Opal 2.0.3**: `port/src/platform/audio/opl/opal/opal.h:3` ("opal 2.0.3,
  commit 7e829f3…") and `:42-46` (`OPAL_VERSION_MAJOR/MINOR/PATCH` = 2/0/3).
- **49,716 Hz**: `port/src/platform/audio/mixer.h:50` (`#define MIXER_OPL_RATE
  49716`); `opal/opal.h:49` (`OPAL_OPL3_SAMPLE_RATE 49716`); the two are tied by
  `port/src/platform/audio/opl/opl.c:20`
  (`_Static_assert(MIXER_OPL_RATE == OPAL_OPL3_SAMPLE_RATE, …)`).
- **The register stream is the claim** (music-truth record
  `2026-10-25-music-truth-derivations.md` §M1.6/§M1.7): the port replays the
  game's music bank 1:1 through the XMIDI sequencer → OPL register writes → Opal
  core, and `tools/audio_compare.py` compares the port's register stream
  tolerance-free against the capture's `.dro`; the PCM hash is determinism only,
  not audibility. The mixer is an invented model (`mixer.h:4-9`).
- **The dual-OPL2 / OPL3 write evidence**, re-measured over the 43 `.dro` files
  on disk (via `tools/opl_trace.parse`):

```text
$ python3 - <<'EOF'
import glob, collections, sys
sys.path.insert(0, 'tools')
import opl_trace

files = sorted(glob.glob('data/k11-captures/**/prage_*.dro', recursive=True))
hw = collections.Counter()
total = bank1 = am_set = vib_set = wave_nonzero = 0
bd = collections.Counter()
forbidden = {0x101: 0, 0x104: 0, 0x108: 0, 0x1BD: 0}
for f in files:
    header, events = opl_trace.parse(open(f, 'rb').read())
    hw[header['hardware']] += 1
    for _t, reg, val in events:
        total += 1
        if reg & 0x100:
            bank1 += 1
            lo = reg & 0xff
            if 0x20 <= lo <= 0x35:
                if val & 0x80: am_set += 1
                if val & 0x40: vib_set += 1
            if 0xe0 <= lo <= 0xf5 and val != 0:
                wave_nonzero += 1
        if reg == 0xBD:
            bd[val] += 1
        if reg in forbidden:
            forbidden[reg] += 1
print('files', len(files), 'hardware', dict(hw))
print('total writes', total, 'bank-1 (0x100+)', bank1)
print('bank-1 AM bit (0x80) set', am_set)
print('bank-1 VIB bit (0x40) set', vib_set)
print('bank-1 non-zero waveform (0x1E0..0x1F5)', wave_nonzero)
print('0xBD writes by value', dict(bd))
print('forbidden', {hex(k): v for k, v in forbidden.items()})
EOF
files 43 hardware {'OPL2': 9, 'dual-OPL2': 34}
total writes 574219 bank-1 (0x100+) 285474
bank-1 AM bit (0x80) set 2255
bank-1 VIB bit (0x40) set 4699
bank-1 non-zero waveform (0x1E0..0x1F5) 4589
0xBD writes by value {192: 43}
forbidden {'0x101': 0, '0x104': 0, '0x108': 0, '0x1bd': 0}
```

Every stream declares `hardware` dual-OPL2 (34) or OPL2 (9); every stream writes
the second register set (`0x100+`, about half its writes). The design's "4,699
bank-1 AM/VIB-enable" is, on re-measurement, the count of bank-1 per-operator
AM/VIB/EG/KSR/multiple registers (`0x120..0x135`) whose **VIB enable bit (0x40)**
is set (the AM enable bit `0x80` is set in a further 2,255 writes; the design's
"AM/VIB-enable" label is the VIB bit specifically). The only control-register
write is `0xBD = 0xC0`, 43 times (once per file); there is no `0x101`/`0x104`/
`0x108`/`0x1BD` write. The stream is therefore equally consistent with one OPL3
(the bank-1 per-operator AM/VIB and waveform behaviour is a topology fork the
oracle cannot see).

### §D1.6 The U14 rule rows (plan Step 5)

`tools/gp_win.py`'s `live_death` rule (`tools/gp_win.py:302-327`, the `LIVE_WIN`
set) requires a game-set `b0c = 1` at or after the round-2 KO row (mode 9,
k = 1) with no `DEATH_DONE` poke. Measured:

```text
$ python3 - <<'EOF'   # b0c across the two scenarios' S records
import re, collections
for path in ('data/k11-captures/gp-u14-win-live.failed/poll.log',
             'data/k11-captures/gp-u9-win/poll.log'):
    h = collections.Counter()
    for l in open(path):
        if l.startswith('S '):
            h[int(re.search(r'b0c=([0-9A-Fa-f]{2})', l).group(1), 16)] += 1
    print(path, dict(h))
EOF
.../gp-u14-win-live.failed/poll.log {0: 9471}
.../gp-u9-win/poll.log {0: 3541}
```

`b0c = 0` in **all 9,471** `S` records of U14's two fought KOs and in **all
3,541** of the poked match-1 win (`gp-u9-win`). The two fights' damage rows (the
game's own verdict, from `gp-u14-win-live.failed`'s first mode-8 and mode-9 S
records):

| row | frame | `s0_5a` | `s1_5a` | `w2` | `w3` | `ad4` | `b0c` | `c0` | `c1` | `afc` |
|---|---|---|---|---|---|---|---|---|---|---|
| round-1 result (mode 0x08) | `0x0898` | `78` | `19` | 0 | 1 | `FFFFFFFF` | 0 | 0 | 3 | 3 |
| match result (mode 0x09) | `0x0F9F` | `78` | `5C` | 0 | 2 | `1` | 0 | 0 | 3 | 3 |

P1 (`c0 = 0`) is KO'd in both rounds (P1's health `s0_5a = 0x78` never drops —
P1 lost by KO while P2's health went `0x19` → `0x5C`); P2 (`c1 = 3`, VERTIGO,
`afc = 3`) won 2-0 (`w3` 1 → 2, `ad4` `0xFFFFFFFF` → `1`). The only non-zero
`b0c` writes are on the ending path, and only for some characters' endings:

| capture | first `b0c = 1` frame |
|---|---|
| gp-u10-ending (char 0) | `0x1602` |
| gp-u18-talon-ending (char 2) | `0x15F7` |
| gp-u19-vertigo-ending (char 3) | `0x1641` |
| gp-u20-chaos-ending (char 6) | `0x1686` |
| gp-u22-diablo-ending (char 5) | `0x1665` |
| gp-u13-ending2 (char 1), gp-u21-armadon-ending (char 4) | never (all S records `b0c = 0`) |

The write itself is `DSB(DS_00104B0C) = 1u` at `0x37FF6`, inside `0x37EA0`
(`fighter_37ea0`, `port/src/game/fighter.c:14402`), the remains streams'
`0xD100` target (opcode 0x11) that runs when a KO'd fighter's death animation
ends — the seven characters' *final* remains stream, reached only after the
seventh final KO.

**The rule mismatch.** `live_death` anchors on the round-2 KO row of *match 1*
(mode 9, k = 1), but `b0c`'s only setter runs on the *ending* path (after the
seventh final KO). The U14 scenario stops at mode `0x12` in match 1, so no S
record at or after the round-2 KO can ever hold `b0c = 1`: whatever P1 does, a
further attempt cannot pass the rule. The rule, not the capture, needs
re-scoping. The two options the design §1.5/§4 record: **(a) drop the `b0c`
clause and claim "fought KOs, no pokes"**, or **(b) move the scenario to the
final**, where the remains stream itself sets the byte. The U14 declaration with
these options is owned by Task 13 (§D7), not this baseline.

### §D1.7 Plan-vs-raw corrections recorded by this task

1. **R10 — the full-rows counter.** `make diff-verify` prints `809/845 closed
   on full rows`. Design §2 (`…design.md:103`) agrees; `AGENTS.md:594` reads
   `807/845` and is stale (claim-coverage Task 6 moved it 807 → 809).
2. **The 5120 port indices are one high** in the design §1.4/§2 prose and the
   `BANDS_ALLOWED` comment: the current `--end-to-last-s` dump (4,953 frames)
   puts 5119 clean at **4314** (f=1824), the band pair at **(4315, 4316)**
   (f=1825/1826), and 5121 as a **(4316, 4317)** splice — the design writes
   4315 / (4316, 4317) / (4317, 4318). The `f` values and the 13-offset
   / 8-1-4 measurement are exact; the computed `band_pair` is unpinned, so no
   oracle moves.
3. **"4,699 bank-1 AM/VIB-enable"** re-measures as the bank-1 per-operator
   AM/VIB register (`0x120..0x135`) writes whose **VIB** enable bit (`0x40`) is
   set; the AM bit (`0x80`) is a separate 2,255 writes. The design's "AM/VIB"
   label is the VIB bit specifically.
4. **`b0c`'s ending-path write is not universal across characters.** Five of the
   seven ending captures set `b0c = 1` after the final (U10/U18/U19/U20/U22);
   BLIZZARD (U13) and ARMADON (U21) keep `b0c = 0` in every S record. The
   design's "every measured non-zero is after the seventh final KO" holds; its
   implication that the ending path always sets it does not.

### §D1.8 What §D1 does not claim

- **No full gate.** `make verify` was not run in Task 1; the mid-phase gate owns
  the two-run proof. The `diff-verify` counter above is the write-tracking
  (default) mode, not `PR_DIFF_FULLSCAN=1`.
- **The census is a hand reconstruction from `P` records**, not the future
  `make coverage` instrument: it proves which *modes* no capture exhibited, not
  which *functions* no driver entered (Task 2/3 build that).
- **The seam split is a source scan**, not a runtime hit count: "instrumented"
  means a seam or an `fn_register`, not that any driver reached it.
- **The U16 series** proves the port is `0x1180` above the original across
  `0x1501..0x1531`; it does not name the cause (record motion vs the `0x100AB4`
  anchor vs call order), which Task 9 measures.
- **The 5120 band's cause is not decided**; §D1.5.2 only re-measures the 13
  offsets, the unheld rows and the bind. Task 11 runs the aperture experiment.
- **The OPL write evidence** re-measures the topology fork's inputs; it does not
  decide dual-OPL2 vs one OPL3 (Task 12's pitch test is about pitch, not the
  topology).
