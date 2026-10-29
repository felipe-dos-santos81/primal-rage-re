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

### §D1.9 The `PR_COVERAGE` hit counter (Task 2)

The instrument Task 2 added (plan Step 2's counter; see
`.superpowers/sdd/2026-10-28-state-coverage/task-2-report.md`). `PR_HIT(addr)`
is a `mem.h` macro compiled only under `PR_COVERAGE` (a no-op otherwise); it
notes one entry into a 1,300-slot `{addr, hits}` table in `mem.c`, never in
`mem[]`. The four seam macros gain the `PR_HIT` line before `if (pr_seam)`, so
the 610 seamed addresses are covered without a second edit, and `fn_resolve`
notes a hit for a found (`fn_register`-registered) function at its
found-return site. `main.c` and
`run_tests.c` write `coverage/<tag>.hits` when `PR_COVERAGE` is on and
`PR_HIT_TAG` names a tag; no output otherwise.

**Re-measured site count — 124 distinct addresses, 125 `PR_HIT` lines, not the
design's "128".** §D1.4's 128 unseamed addresses split into: 114 with a column-0
`/* 0xADDR` header in a `.c` file (one line each, except `0x50188` which carries
that header on two `.c` functions — `flow.c:swap_buffers` and
`config.c:cfg_swap_buffers` — so 115 `.c`-header lines); 7 with a column-0 header
only in a `.h` file (`0x33A10`, `0x33950`, `0x3AFC4`, `0x1AB10`, `0x255CC`,
`0x1BEC4`, `0x33904`); 3 of the 7 `PORT_PROGRESS_EXCEPTIONS` addresses with a real
`.c` body (`0x1B120` `res_load_index`, `0x20C10` `game_init`, `0x4FBA2`
`int10h_query`); and 4 exceptions with no function body that stay uninstrumented
— `0x29D60` (bare `ret`), `0x2EA30` (inert interrupt-lock counter), `0x2EA64`
(bare `ret`), `0x500BB` (inlined DPMI clock read) — Task 3 declares these
`UNINSTRUMENTED`. `tools/port_progress.py` is unchanged (`798 1203 66`,
`759 759 100`).

**Byte-identity** (plan Step 4): `build/` (PR_COVERAGE off) and `build-cov/`
(`-DPR_COVERAGE=ON`, `PR_HIT_TAG` unset), both from the repo root. The outputs
are byte-identical:

| artifact | OFF | ON (tag unset) |
|---|---|---|
| `frames/fn_miss.txt` sha256 | `26c999cee30d7557d17d392639eaadcde0378f84288bc14f2d4a5be169d8eefe` | `26c999cee30d7557d17d392639eaadcde0378f84288bc14f2d4a5be169d8eefe` |
| `--check 820` frames (sha256 of the sorted per-file sha list, 2460 files) | `100e5bbdb239036396acf60ce3f324efd7d18b25f92b109903940d9fea4beecf` | `100e5bbdb239036396acf60ce3f324efd7d18b25f92b109903940d9fea4beecf` |
| `PR_TITLE_DUMP` driver frames (sha256 of the sorted per-file sha list, 96 files) | `ca293a7fa0fd0745f95afce846592b1f6fd757870d24a3b4439e9803369d3566` | `ca293a7fa0fd0745f95afce846592b1f6fd757870d24a3b4439e9803369d3566` |

The `--check 820` run is 2,461 files (820 frames × 3 + `fn_miss.txt`). A smoke
dump with `PR_HIT_TAG=smoke` wrote `coverage/smoke.hits` (`fn-hit smoke 0x20C10
n=1`, … 144 lines), proving the dump path; the unset-tag run stays inert. The
unit test (`test_fn_hit`) fails when the note increment is dropped and passes
otherwise; no `PR_SEAM` was added. `diffrun` links the plain `prage_core`, so its
bytes do not change.

---

## §D2 The out-of-scope modes declared (Task 4)

The three content-bearing never-presented modes that this phase **declares,
not captures** (design §3 G2, §7.2 Step 4): the volleyball easter egg `0x21`,
the both-characters-4 camera sequence `0x25`, and the continue/toll screen
`0x0E`. No capture, no pin, no code change: this is the measured gate
derivation and the declared reason each is out of scope. The census's measured
NOT-HIT rows (Task 3's `tools/coverage_baseline.txt`, pinned, not touched here)
already hold the three entry handlers, so the declaration moves no baseline
line.

### §D2.1 The `DS_00104ABC` derivation (why `0x21`/`0x25` are two-human-only)

`DS_00104ABC = (DS_00104B1F == 3) + 1`, written at exactly two sites (the raw
`setne al; inc eax`), so `DS_00104ABC` is `2` **iff** `DS_00104B1F == 3`:

| site | address | store |
|---|---|---|
| `flow_player_join` | `0x28DA4` | `DSD(DS_00104ABC) = (DSB(DS_00104B1F) == 3u ? 1u : 0u) + 1u;` at `flow.c:1632` (`0x28DCF..0x28DE7`) |
| the mode-5/0xC round-end case-3 arm | (update-table entry 6) | the same store at `flow.c:2416` (`0x25EB2..0x25EC5`) |

`DS_00104B1F` is the join bitmask (bits `1`/`2` = side 0/1 joined), so `3` =
both players joined = a two-human match. Every in-match gate below that reads
`DS_00104ABC >= 2` therefore requires `b1f == 3`; a 1P or `b1f == 2` scenario
cannot open it however much damage is poked (design §9's stop rule; the port
never stores `DS_00104ABC` anywhere else — §D2.5).

**Plan-vs-raw line correction.** The plan/brief cites `flow.c:1621`/`2405`; the
measured stores are `flow.c:1632` and `flow.c:2416`. `flow.c:1616` (in the
`flow_player_join` header comment) and `flow.c:2405` (a `break;`) are the lines
the brief's numbers point at; the store instructions themselves are at 1632 and
2416.

### §D2.2 Mode `0x21` — the volleyball easter egg (entry `0x4BD98`)

The mode store is `fight_4bd98` (`0x4BD98`), called once, from `fight_4b470`
(`0x4B470`)'s eighth-hit tail (`fight.c:3090`, `0x4B590`). Two gate layers:

**Caller (`fight_4b470`, `fight.c:3080-3086`) — the eighth worshipper trample:**

| gate | condition | address | file:line |
|---|---|---|---|
| `DS_00104B1D` not 2/3 | `== 3u || == 2u` returns | `0x4B547`/`0x4B54C` | `fight.c:3080` |
| `DS_00104AFC == 0` | `!= 0u` returns | `0x4B559` | `fight.c:3081` |
| `DS_001088C1 == 0` | `!= 0u` returns | `0x4B562` | `fight.c:3082` |
| `(DS_001088EF >> 24) > 1` | `<= 1` returns | `0x4B56F` | `fight.c:3083` |
| the caller's `+0x1F > 7` | `(u32)DSB(entry + 0x1Fu) <= 7u` returns | `0x4B579` | `fight.c:3084` |
| `DS_001088C5 == 0` | `!= 0u` returns | `0x4B582` | `fight.c:3085` |
| `DS_00108864 == 0` | `!= 0u` returns | `0x4B58C` | `fight.c:3086` |

`+0x1F` is the effects entry's hit counter, so `+0x1F > 7` is the **eighth**
worshipper trample of that one entry (record §42-C; the trample `0x4B470`
itself is `HIT` in the baseline — the demo reaches it — but its mode-`0x21`
tail is not).

**Callee (`fight_4bd98`, `fight.c:2943-2945`) — the mode-`0x21` store:**

| gate | condition | address | file:line |
|---|---|---|---|
| `DS_00104AD8 <= 0` | `(s32)DSD(DS_00104AD8) > 0` returns | `0x4BDA7` | `fight.c:2943` |
| `DS_00104ABC >= 2` | `DSD(DS_00104ABC) < 2u` returns | `0x4BDB4` | `fight.c:2944` |
| `fight_13134` false | `fight_13134() != 0` returns | `0x4BDC1` | `fight.c:2945` |

`fight_13134` (`0x13134`, `fight.c:2921`) is 1 when **both** fighter records' x
(`+0x18` of `DS_001077B0`'s and `DS_00107844`'s record) are below `-0x3300`, or
both above `0x3300` — i.e. the two fighters are far off-screen in the same
direction. The gate passes only when they are not.

On a pass the store body runs (`fight.c:2946-2977`): `DS_00108864 = entry`
(type 6), `DS_001088C1 = 1` and `DS_001088C5 = 1`, the **mode word
`DS_00104B00 = 0x21`** (`fight.c:2949`, `0x4BDE7`), the **`0x1E` timer
`DSW(DS_001088AA) = 0x1Eu`** (`fight.c:2951`, `0x4BDF4`), and the **two actor
spawns** `0xBAB88` (`fight.c:2968`) / `0xBAB9C` (`fight.c:2971`) at the
fighters' midpoint, both with `+0x36 = 0x1A4`.

### §D2.3 Mode `0x25` — the both-characters-4 camera sequence (entry `0x4E11C`)

The mode store is `fight_mode25_enter` (`0x4E11C`), called once, from
`game_mode_04_step` (`0x26254`, `flow.c:3231`, `0x263EA`). Two gate layers:

**Caller (`game_mode_04_step`, `flow.c:3228-3230`):**

| gate | condition | address | file:line |
|---|---|---|---|
| `DS_001078FA == 2` | `== 2u` (re-read after `0x27FA8`) | `0x263BA` | `flow.c:3228` |
| `DS_00108892 >= 6` | `(u32)DSW(DS_00108892) >= 6u` | `0x263D0` | `flow.c:3229` |
| slot 0's `+0x53 == 0` | `DSB(DS_00107803) == 0u` | `0x263DE` | `flow.c:3230` |

(`DS_00107803` = `0x1077B0 + 0x53`, slot 0's `+0x53`.)

**Callee (`fight_mode25_enter`, `fight.c:5584-5597`):**

| gate | condition | address | file:line |
|---|---|---|---|
| `DS_00104B1D` not 2/3 | `b1d == 3u || b1d == 2u` → decrement, return | `0x4E127..0x4E12F` | `fight.c:5584` |
| camera within `±0x3300` | `cx < -0x3300 || cx > 0x3300` (`cx = (s32)DSD(DS_000F0AF0)`) | `0x4E13D..0x4E151` | `fight.c:5588-5589` |
| `DS_00104AD8 <= 0` | `(s32)DSD(DS_00104AD8) > 0` | `0x4E153`/`0x4E15A` | `fight.c:5590` |
| `DS_00104ABC >= 2` | `DSD(DS_00104ABC) < 2u` | `0x4E15C`/`0x4E163` | `fight.c:5591` |
| both characters 4 | `DSB(DS_0010782A) != 4u` (slot 0 `+0x7A`); `DSB(DS_001078BE) != 4u` (slot 1 `+0x7A`) | `0x4E165..0x4E170`; `0x4E172..0x4E17D` | `fight.c:5592`; `fight.c:5593` |
| `DS_001088C4 == 0` | `DSB(DS_001088C4) != 0u` | `0x4E17F..0x4E187` | `fight.c:5594` |

Character `4` is ARMADON (record §E4's `start_char(4, …)`), so the gate is a
**mirror-ARMADON** match; the camera (`DS_000F0AF0`) must be within the
`±0x3300` band. The brief's `0x25` list names the six it does; the raw also
gates on `DS_00104B1D` not 2/3 and `DS_001088C4 == 0`, recorded here as the
complete gate. The mode's own frame step is `game_mode_25_step` (`0x266AC`,
`flow.c:5901`), also `NOT-HIT`.

### §D2.4 Mode `0x0E` — the continue/toll screen (handler `0x27A2C`)

Entry `flow_continue_open` (`0x278B0`, `flow.c:1789`) opens the screen from
`flow_arena_ko_check` (`0x272DC`, `flow.c:1810`) when the winner's slot `+0x5A
>= 0x78` — a lost match. The handler `game_mode_0e_step` (`0x27A2C`,
`flow.c:1978`) runs:

| piece | description | address | file:line |
|---|---|---|---|
| the continue poll | `flow_continue_poll` (`0x42F60`, `flow.c:1898`): 0 with no credit (`config_credit_ready == 0`, `flow.c:1901` `0x2C060`) or the side's start mask `0x9ACBC[side]` not newly pressed in `DS_001088E4`; else spend a credit and set `DS_00105C04 = 1` | `0x42F60` | `flow.c:1898` |
| the countdown byte `DS_00108110` | starts **`0xF` (15)** at `flow_continue_open` (`flow.c:1793`, `0x278C2`); decremented on the frame word `DS_000EF6DC & 0x3F == 0` (every 64 master ticks) or a forced tick; the forced-tick arm **resets it to `0xA`** (`flow.c:1990`, `0x27A88`) and clears `DS_00105C04`; below 0 (signed) → `0x29B74` darken, mode `0x15` | `0x278C2`, `0x27A88`, `0x27ACD..0x27ADF` | `flow.c:1793`, `flow.c:1990`, `flow.c:2000-2003` |
| the credit | `config_credit_ready` (`0x2C060`, `config.c:622`): with a credit the "PRESS START" blink `0x2C0F4`, else the "INSERT 1 COIN" blink `0x2C1D4` | `0x27B5E`/`0x27B83` | `flow.c:2017`/`flow.c:2021` |

**Plan-vs-raw correction — the countdown does not start at `0x0A`.** The
brief's Step 1 says "starts `0x0A`". The raw initialises `DS_00108110 = 0xF`
(15) when the screen opens (`flow.c:1793`), and `0x0A` is only the
forced-tick **reset** value after a credit-spent start press (`flow.c:1990`).
(The challenge screen `0x42CB4` also resets it to `0xA` at `flow.c:3677`, but
that is mode `0x13`, not `0x0E`.) The 15 is drawn on entry (`flow.c:1797`), so
the continue countdown is 15→0, not 10→0.

### §D2.5 Why each is out of scope (and why no poked 1P scenario)

- **`0x21` and `0x25` are two-human-only.** Both store the mode only through
  `DS_00104ABC >= 2`, and `DS_00104ABC = (b1f == 3) + 1` (§D2.1), so both need
  a two-human match. `0x21` additionally needs **eight tramples of one specific
  effects entry** (`+0x1F > 7`, §D2.2) and `0x25` a **mirror-ARMADON** match
  whose camera sits within `±0x3300` (§D2.3). **A poked 1P scenario cannot open
  either gate** — no damage poke writes `DS_00104B1F` or `DS_00104ABC`, and the
  port's only `DS_00104ABC` stores are the two join sites above. This phase
  does not build one; the modes stay declared.
- **`0x0E` needs a lost match plus a credit.** The screen opens only on a KO
  (`flow_arena_ko_check`'s `+0x5A >= 0x78`), and its continue poll needs a
  credit ready and a start press; the phase takes no toll-screen scenario.

### §D2.6 The census rows (Task 3's baseline, cited, not moved)

| mode | entry/handler | census row | related NOT-HIT rows (co-entered, not the entry) |
|---|---|---|---|
| `0x21` | `0x4BD98` `fight_4bd98` | `tools/coverage_baseline.txt` `0x4BD98 NOT-HIT` (line 696) | `0x13134` (l.31, `fight_13134`), `0x26540` (l.240, `game_mode_21_step`), `0x4CB18` (l.700), `0x4BF18` (l.697), `0x4C60C` (l.698) |
| `0x25` | `0x4E11C` `fight_mode25_enter` | `…` `0x4E11C NOT-HIT` (line 714) | `0x266AC` (l.241, `game_mode_25_step`), `0x4E350`, `0x4E27C`, `0x4E67C`, `0x4EF8C`, `0x38154`, `0x384F8` |
| `0x0E` | `0x27A2C` `game_mode_0e_step` | `…` `0x27A2C NOT-HIT` (line 255) | `0x278B0` (l.253, `flow_continue_open`), `0x2791C` (l.254, `flow_continue_take`), `0x2C1D4` (l.323), `0x2C1C8` (l.322) |

The trample `0x4B470` is `HIT` (line 690) — the demo reaches it — which is why
`0x4BD98`'s own `NOT-HIT` is meaningful, not an unreached-caller artifact. Task
5's `0x296B8`/`0x29638` (`NOT-HIT`, lines 275/274) are mode `0x32`/`0x33`,
that task's rows, not this task's. The baseline is Task 3's pin; this
declaration adds no line, moves none, and reclassifies none.

### §D2.7 The `0x0B` correction (the drawn winner pose)

The plan's Task 4 Step 3 says to note that `0x0B` (the drawn winner pose) is
covered by claim-coverage Task 11's `gp-draw` *if that capture published*. It
did **not**: there is no `data/k11-captures/gp-draw` directory, no `gp-draw`
entry in the Makefile's `CAPTURE_DIRS`, no oracle and no miss set (measured
2026-10-29). The drawn-winner-pose content (`0x0B`) therefore **remains
unexhibited**, and this task does not claim otherwise. A timeout-draw capture
is Task 7's job (§G5, a poked timeout draw, not the KO draw); it will exhibit
`0x0A`/`0x0B` when it publishes.

### §D2.8 What §D2 does not claim

The three modes' *content* is not reproduced: no capture, no pin, no oracle
(design §8 "Will not claim"). §D2 records the gates and the out-of-scope
reason only, and leaves the three modes in the never-presented set
(§D1.3's `0B 0E 21 25`).

---

## §D3 The U16 discriminating dump (Task 9)

**Measured class: record motion (`rec+0x1C`), not the anchor term and not
`0x186D0` call order.** At every required boundary frame, the original and the
port have the same `0x100AB4` addend and the same anchor index. The original's
record-y is `0x1180` below the port's; equivalently, the port's record-y and
latched `slot+0x30` are `0x1180` above the original's. No port behaviour or pin
changed.

### §D3.1 The appended fields and the pointer correction

`SNAP_FIELDS` and the `PR_GP_DUMP` T record append, in order:

```text
ab4    [0x100AB4]      dword    rec_y  [[0x1077B0]+0x1C]  dword
anchor [0x100AF0]      dword    s0_57  [0x107807]         byte
rec8   [[0x1077B0]+0x08] dword  rec34  [[0x1077B0]+0x34]  word
rec36  [[0x1077B0]+0x36] word   rec44  [[0x1077B0]+0x44]  word
```

The slot-1 record pointer is `0x107844`; this measurement uses slot 0. In the
port, the record pointer is an original linear address. In DOSBox-X's memory
file it is a **physical address**: the diagnostic S records measured
`[0x1077B0] = 0x0030AF18` and `[0x1077A8] = 0x002ED7B0`. `read_snap` therefore
uses `record_physical + field_offset`, exactly as it already does for the
key-config pointer, not `base + record - 0x80000 + field_offset`. The first
version of this task's reader used the latter and returned zeroes; the r3/r4
runs made with it are rejected as component evidence. The pointer diagnostic
is `gp-u24-u16-state-t9-r4`; the accepted component run is r5 below.

`test_the_u16_component_fields_are_appended_with_their_widths` and
`test_read_snap_resolves_the_u16_fields_through_the_slot_record` failed before
the append/dereference (missing fields and `None` values), passed after it,
then failed again when the test was changed to the measured physical-pointer
layout (`rec_y` and the other record fields read zero) and passed after the
one-line correction. `format_s` needs no pointer arithmetic of its own: its
input is the resolved value dictionary, and the test round-trips all eight new
values. The port T-line test checks field order, widths, every fixed address,
and every `[DS_001077B0]` record offset. The combined
`test_gp_session`/`test_gp_capture` run is 135 tests, OK.

### §D3.2 Port arm

```text
$ make gp-replay scenario=gp-u23-u16-finisher
...
fn-miss PR_GP_DUMP distinct=4 dropped=0
all checks passed
```

The U23 T records confirm the baseline series: `s0_30 = 0x0D40` at `0x1501`,
rising to `0x1B40` at `0x152C..0x1531`; `ab4` equals `s0_30` and `rec_y` is zero
throughout that interval. At the U23 reaction set (`0x1532`), `ab4 = 0x1D40`
and `rec_y = 0x2EE`. The complete extraction is in this task's report; every
row satisfies `(ab4 + rec_y) mod 2^32 == s0_30`.

For the same-state original comparison below, the accepted r5 capture was also
replayed with U23's scenario definition (U24 clones its steps object):

```text
$ python3 tools/gp_session.py port-script --scenario gp-u23-u16-finisher \
    --capture data/k11-captures/gp-u24-u16-state-t9-r5 \
    --out /tmp/pr_gp_dump/gp-u24-u16-state-t9-r5-u23.script
$ PR_GP_DUMP=/tmp/pr_gp_dump/gp-u24-u16-state-t9-r5-u23 \
  PR_GP_SCRIPT=/tmp/pr_gp_dump/gp-u24-u16-state-t9-r5-u23.script \
  PR_GAME_DIR=data/game/C ./build/run_tests
...
fn-miss PR_GP_DUMP distinct=4 dropped=0
all checks passed
```

### §D3.3 Original arm and run validity

Immediately before the original runs the 1-minute load was 0.83, 1.01, 1.13,
0.97 and 0.91 (all below 8); no build or replay ran concurrently. Two ordinary
BIOS-pad guarded attempts (`t9`, `t9-r2`) both stopped at `f=0x1485` before the
window because the guard requires an S record the mode-9 transition never
produces, and retained no log. The installed DOSBox-X 2026.08.31 has no usable
scripted debugger path here (`DEBUGBOX` is `Bad command or filename`, and
Alt+Pause does not open one), so the BPX fallback could not be used.

The accepted run kept the PinGuard but disabled pad-BIOS synthesis, a harness
input-path change that is explicit in its `session.txt` (`pad_bios=0`):

```text
$ DISPLAY=:1 timeout 600 python3 tools/gp_capture.py \
    --scenario gp-u24-u16-state \
    --out data/k11-captures/gp-u24-u16-state-t9-r5 \
    --exe /tmp/pr_title_pin/PRAGE.EXE --no-pad-bios
gp_capture: snapshots 6089, f 5..17DC, 15 frames missed (spec §3.7)
gp_capture: CHECK steps fired 75/75 (0 skipped); stopped on mode 0x12: ok
gp_capture: CHECK port script v2: ok
gp_capture: CHECK no unscripted input: ok
gp_capture: CHECK pokes written 1/1, 0 raced: ok
gp_capture: CHECK capture pins: 0 BIOS words skipped (backlog over the 14-entry cap): ok
gp_capture: CHECK stopped at the end (SIGTERM at f=17DC, rc=0): ok
gp_capture: wrote 5563 frames ... wall 123.3s
```

All eleven checks pass (the five not quoted in full are `base`,
`end frame reached`, `mode 0x27 after the Enter`, `snapshots kb == raw`, and
`frames written 5563/5563`). `poll.log` is 6367 lines, sha256
`d0ba4382b2fb56f3e9463051ee21214e214aa6e9e4768088c23db357c33919e5`. The
run is a new diagnostic directory, not a published capture, not in
`CAPTURE_DIRS`, and has no oracle or pin.

The no-BIOS-pad arm advances some reaction stream steps one frame earlier than
the ordinary U23/U24 runs. The aligned port replay advances at exactly the
same frames, so the comparison below is same-state throughout. As independent
controls, the new original's `s0_30` equals the existing
`gp-u24-u16-state.failed` original at `0x1501` (`0xFFFFFBC0`) and at every
frame `0x152C..0x1530` (`0x09C0`). At `0x1531` the old run still holds
`0x09C0`, while the accepted run and its aligned port replay both enter the
next state (`s0_57 = 3`, `s0_54 = 2`); the old and new `0x1531` records are not
interchanged. The aligned replay's `trace.txt` sha256 is
`6e2c6fd666ee7234273bba1fae1ceb715d25d1f97e01df3784e9ab48ec2dba40`.

### §D3.4 The two complete boundary series

These are the accepted original S records and the aligned port T records at
**every** frame `0x1501..0x1531`. `O` is original, `P` is port; all values are
hex. `rec_y` is the record component before the latch, not `s0_30`.

| frame(s) | O `s0_30` | O `ab4` | O `rec_y` | P `s0_30` | P `ab4` | P `rec_y` |
|---|---:|---:|---:|---:|---:|---:|
| `1501..1502` | `FFFFFBC0` | `00000D40` | `FFFFEE80` | `00000D40` | `00000D40` | `00000000` |
| `1503..1507` | `FFFFFCC0` | `00000E40` | `FFFFEE80` | `00000E40` | `00000E40` | `00000000` |
| `1508..150D` | `FFFFFC40` | `00000DC0` | `FFFFEE80` | `00000DC0` | `00000DC0` | `00000000` |
| `150E..150F` | `00000040` | `000011C0` | `FFFFEE80` | `000011C0` | `000011C0` | `00000000` |
| `1510..1511` | `000002C0` | `00001440` | `FFFFEE80` | `00001440` | `00001440` | `00000000` |
| `1512..152A` | `00000840` | `000019C0` | `FFFFEE80` | `000019C0` | `000019C0` | `00000000` |
| `152B..1530` | `000009C0` | `00001B40` | `FFFFEE80` | `00001B40` | `00001B40` | `00000000` |
| `1531` | `00000EAE` | `00001D40` | `FFFFF16E` | `0000202E` | `00001D40` | `000002EE` |

Every row also has identical `anchor`, `s0_57`, `rec34`, `rec36` and `rec44`.
`rec8` is the same stream address after removing the measured DOS4GW physical
relocation (`original = port + 0x1E6000` in this run). Every row satisfies the
latch sum modulo `2^32`. In particular:

- at `0x1501`, `0xD40 + 0xFFFFEE80 = 0xFFFFFBC0` on the original and
  `0xD40 + 0 = 0xD40` on the port;
- at `0x152C..0x1530`, `0x1B40 + 0xFFFFEE80 = 0x09C0` on the original and
  `0x1B40 + 0 = 0x1B40` on the port;
- at the aligned `0x1531` state, `0x1D40 + 0xFFFFF16E = 0x0EAE` on the original
  and `0x1D40 + 0x2EE = 0x202E` on the port.

Signed subtraction gives the same direction at every row: **P minus O is
`+0x1180`; O minus P is `-0x1180`**, for both `rec_y` and `s0_30`. The earlier
`0x14FE` transition shows the record moving before the plateau: the accepted
original has `rec_y = 0xFFFFF280` while the aligned port still has zero, with
`ab4 = 0x0A80` on both. This is not a constant fabricated from the plateau;
it is the measured record component at each frame.

### §D3.5 The class and the follow-up addresses

The classification rule is direct: `ab4` agrees at both boundaries and at every
intervening frame, while `rec_y` differs. Therefore the defect is **record
motion**. The `0x100AB4` producer (`0x18350`, the `0xCFC32` TALON table and the
`0x18540` anchor index path) is not the differing term, and the two-addend
`0x186D0` latch is not a call-order mismatch.

A read-only `PR_GP_DUMP` opcode print (reverted after the measurement) shows
that the port reaches the reaction stream's y opcode with a zero operand:

```text
f=14FD p=000ED858 cw=FF21 op=21 ax=0000 y=00000000 r52=00
f=1500 p=000ED858 cw=FF21 op=21 ax=0000 y=00000000 r52=01
```

At those same stream states the original holds `rec_y = 0xFFFFF280` and then
`0xFFFFEE80`. The follow-up fix therefore belongs to the **record-motion
path**: stream `0xED850`, command word `0xFF21` at **`0xED858`**, opcode `0x21`'s
`rec+0x1C += (s16)operand * 64` writer at **`0x2B7B8`**, and its `rec+0x52`
operand source in **`0x29F34` case `0x40`**. The raw stream bytes and that
operand's seed are Task 10's next inspection; this task does not guess which of
those record-side instructions is mistranscribed. The fix is not in
`0x18350`, `0xCFC32`, `0x186D0`, or the `0x48599` gate.

### §D3.6 Limits and measured corrections

- The two ordinary guarded attempts could not pass the pre-window `0x1485` pin
  gap; the accepted original uses `--no-pad-bios`. Its aligned port replay is
  therefore part of the evidence, not an optional convenience. No claim is made
  that a differently timed `0x1531` record can be substituted for another.
- The physical-pointer correction is raw-derived from r4's explicit pointer
  fields. The zero-valued r3/r4 component fields are not used in the class.
- The ordinary U23 port baseline and the existing `.failed` original baseline
  still read exactly as §D1.5.1 states: the original is `0x1180` below the port
  at the sampled frames, or the port is `0x1180` above the original. The new
  one-frame-earlier stream steps do not rewrite that baseline.
- This is a classification only. No offset, dwell, gate or anchor value was
  fitted; no game C function, oracle ratchet, miss set or Makefile pin changed.

### §D3.7 The fix (Task 10): the reaction reads the IVT, not a fitted offset

Task 10 inspected the raw stream at the dumped cursor and found the record-motion
gap is a **null table base**, not a mistranscribed instruction. The reaction
stream `0xED850` begins with the two mode-0x6000 opcodes `0xFF20` (x) and `0xFF21`
(y); `actors_anim_begin` (`0x2BC30`) sets `rec+0x0C = 0` before its initial walk,
and neither opcode loads a base first, so each does a table lookup against linear
address **0**. In the original, address 0 is the DOS/4GW conventional memory (the
IVT's INT 0 vector); in the port it is BSS zero. The port's opcode-0x21 handler
(`0x2B7B8`), its operand source (`0x29F34` case 0x40) and the mode-0x6000 command
decode were all re-checked against the raw assembly and are byte-faithful — the
gap is the *data* at address 0, not the code.

The y-opcode's operand is `(s8)DSB(rec+0x52*2 + 1)`, i.e. `mem[1]` at `rec+0x52 = 0`
and `mem[3]` at 1, each applied once (the reaction enters once, `actors_anim_begin`
is called once, and rec+0x1C accumulates across the two loop passes). The original's
measured series (§D3.4/§D3.5) is `rec_y = 0xFFFFF280` (`-3456`) at `0x14FE` then
`0xFFFFEE80` (`-4480`) at `0x1501`, so:

- `y[0] = -54 = 0xCA` → `mem[1] = 0xCA` (`-3456 = -54 * 64`);
- `y[1] = -16 = 0xF0` → `mem[3] = 0xF0` (`-4480 - (-3456) = -1024 = -16 * 64`).

These are the INT 0 vector's offset-high and segment-high bytes (the port has no
real mode, so it cannot read the real IVT). The fix is the host stand-in
`bios_ivt_stand_in()` (`port/src/game/flow.c`, called by `game_init`), which writes
those two bytes. **The x bytes `mem[0]`/`mem[2]` (the INT 0 vector's offset-low and
segment-low, read by the x-opcode) are the adjacent record-motion x defect, recorded
not fixed**: they were not captured, and the acceptance (the dwell, the `0x4844C`
gate, `r0`) reads only the y. No offset, dwell or amplitude was fitted; the two bytes
are derived from the measured series with the addresses that prove them.

The boundary test `test_u16_reaction_motion` (registered in `test.h`) seeds
`bios_ivt_stand_in`, runs the reaction stream's two passes, and asserts the dwell
`0xFFFFF280` (`-3456`) then `0xFFFFEE80` (`-4480`); it fails before the fix
(`mem[1] = 0`) and under a `mem[1] = 0xCB` mutation (`-0xD40`, `-0x1140`). The gate's `0x13FF/0x1400/0x1401`
boundary is `test_p3_4844c`'s state-3 claim (already committed), not re-added.

Measured on the ordinary U23 replay after the fix: the port's `rec_y`/`s0_30` now
match the original's series at every frame `0x14FC..0x1531` (the port is no longer
`0x1180` above it), `s0_54` clears at `0x1533` (the capture's exit frame, not the
old `0x155C`), and `r0` reaches `0x33` at `f=0x155B`. The U23 oracle re-pins
**only** `GP_U23_TRACE_MIN_FIRST` `5449 → 6050` and `GP_U23_MOVES_MIN_FIRST`
`5467 → 6050` (both are the replay's end + 1: `0 differing through 6049`, exact at
±1 — 6051 is unreachable, 5449/5467 fail under `PR_PINS_EXACT`). `MIN_FIRST 4095`,
`MAX_START 106`, `MIN_REACH 3439`, `MAX_MISSING 9`, the capture sha and the frame
count are unchanged. `make diff-verify` stays `1033/1033 functions VERIFIED;
14993/14993 mutants detected; 0 named gaps; 825/845 rows with callees closed (188
have none); 809/845 closed on full rows; 8 ported functions with no row`
(`0x20C10` is one of the 8, so the new `bios_ivt_stand_in` call in `game_init` moves
no row), and `port_progress.py` stays `798 1203 66` / `759 759 100`.

---

## §D4 The 5120 aperture read (Task 11)

The experiment the plan's §G7(a)/Task 11 asked for: read the 13 band offsets
plus one control at every S record of a fresh diagnostic capture and decide
whether the 5120 band's 13 differing pixels are a capture-pipeline artifact
(PASS-A), a port divergence with a nameable writer (PASS-B), or both
contradicted (FAIL: the band stays a named gap). The answer is **INCONCLUSIVE**,
a fourth outcome outside the design's three: the diagnostic read is invalid —
the DOSBox-X memory file does not hold the VGA aperture at all — so the 14
bytes are not a measurement of the aperture and neither support PASS-A/PASS-B
nor contradict them. The band stays a named gap for that reason.

### §D4.1 The diagnostic scenario and the read

- `SCENARIOS['gp-u13-band']` (`tools/gp_session.py`) is `gp-u13-ending2`'s
  steps/timelimit/memsize unchanged, with `extra = WIN_EXTRA + APERTURE_FIELDS`.
  It is in `STOP_AT_END` like U13.
- `BAND_APERTURE` (14 offsets, 13 band + the control `(1,100)`) is the brief's
  measured x list: `(1,23) (1,24) (1,295) (1,296) (2,22) (2,297) (3,21) (3,74)
  (3,298) (4,27) (4,28) (4,291) (4,292)`, control `(1,100)`. The 13 are exactly
  §D1.5.2's 8 index-`0x32` + 1 `0x2E` + 4 `0x4D` set.
- Each byte is read at **file offset** `0xA0000 + 320*row + x`, a literal
  offset, not `base + ds - DATA_BASE_VA`. The VGA aperture is at linear
  `0xA0000` (mode 13h, one byte per pixel index); DOS/4GW identity-maps linear
  `0xA0000` to the physical VGA aperture, and the DOSBox-X memory file is a
  physical dump, so the aperture's file offset is its linear address. The data
  object is instead relocated to `base = 0x266000` (`find_base`; the `B` record
  reads `base=00266000`, and `RAGE.S16` sits at file offset `0x26602d` alone),
  so a `base + ds - DATA_BASE_VA` read of `0xA0000` would land on the data
  object's shadowed `0xA0000` hole, not the aperture. `read_snap` gained the
  `APERTURE_FIELD_NAMES` branch for this literal read (`tools/gp_capture.py`),
  and `tools/tests/test_gp_capture.py::test_the_aperture_fields_are_read_at_the_literal_offset`
  pins it with a decoy at the base-relative address.

### §D4.2 The capture

```
$ make gp-capture scenario=gp-u13-band    # EXIT:0, wall 206.5 s, load 0.93 (uptime 11:50:12)
```

`data/k11-captures/gp-u13-band`: 5,870 frames (raw 1384..14412), every CHECK ok
(steps 23/23, end reached, port script v2, no unscripted input, pokes 17/17
unraced, stopped at f=2755), `memsize=64`, `exe` sha
`8120f1bd1df389ed94cb329c030f95d9e38caad193bbd9840717af557161a68d`. The published
`gp-u13-ending2` directory and its pins are untouched.

### §D4.3 The f=0x1826 record and the 14 bytes

The S record at `f=1826` (`mode=0015`, the fade; `frames.txt` maps `04316
f=1826 mode=0015`, confirming §D1.5.2's correction) reads, for all 14 offsets:

```
band (1,23)..(3,298) ap=FF   (8 offsets, port index 0x32, capture 00001c)
band (3,74)         ap=FF   (port index 0x2E -> 100000, capture black)
band (4,27)..(4,292) ap=FF  (4 offsets, port index 0x4D -> 00000c, capture black)
control (1,100)     ap=FF   (port index 0x65 -> 000000, capture 000000)
```

i.e. **all 14 bytes are `0xFF`**. This is not a single-frame read: across all
10,020 S records (140,280 reads) every aperture byte is `0xFF` (0 non-`0xFF`
reads), from the boot (`f=5`, mode 3) to the ending (`f=0x1827`, mode 0x15).

The comparison sides (unchanged from §D1.5.2, read from the existing
`gp-u13-ending2` replay dump and capture): the port frame at `f=1826`
(`frame_04316.ipx`) holds indices `0x32`×8 / `0x2E`×1 / `0x4D`×4 and control
`0x65`, with DAC `0x32->000000`, `0x2E->100000`, `0x4D->00000c`, `0x65->000000`
and `0xFF->000000`; capture 5120 holds `00001c`×8 and black×5, control black.

### §D4.4 The decision and what it means

`tools/gp_band.py` applies `gp_compare.decide_aperture` (the tested
PASS-A/PASS-B/FAIL/INCONCLUSIVE rule) to those three sides and returns
**INCONCLUSIVE**. The aperture read maps, through the frame's DAC, to a single
colour (`0xFF -> 000000`, black) at every offset; that cannot be PASS-A (the
port's 13 band colours are black+`100000`+`00000c`, 8/1/4) or PASS-B (the
capture's are `00001c`+black, 8/5). But it is not FAIL either: FAIL is the
design's "both contradicted", a *valid* read whose 13 pixels match neither side,
whereas this read is a single uniform byte (`0xFF` ×14) — the band's 13 pixels
have at least two distinct colours on both sides, so a uniform read cannot be a
measurement of them. The control pixel is not the discriminator here
(`DAC[0xFF]` happens to equal the control's agreed black, so the control check
passes and the uniform-byte check is what rejects the read).

The cause is the memory file, not the game: DOSBox-X's `dosbox memory file`
maps the guest's main RAM (`MemBase == memory_file_base`), exactly `memsize`
bytes (the capture's `guest.mem` is 67,108,864 = 64 MB). The VGA aperture is a
separate emulated region (`vgamem`), and DOS/4GW routes the game's `0xA0000`
writes to it, not to `MemBase`, so `MemBase[0xA0000..0xAFFFF]` is never written
and stays `0xFF` (measured pre-capture on a 30 s boot: the region is 65,536/65,536
`0xFF` bytes while the data object at `0x266000` already holds `SB16.DIG` etc.).
The poller therefore cannot read the real aperture through the memory-file map —
the "unmeasured surface" of design §1.4 is, on this host, unmeasurable by that
map, so the read is not a measurement of the aperture.

**Outcome and limits.** The band stays a named gap because the read is invalid,
not because the aperture contradicts either hypothesis; no pin moves and the
band's bounds (`(5120, 1, 5, 56b900b1…)`) are unchanged, and no writer is named.
The experiment rules out the memory-file read as a way to distinguish a
capture-pipeline artifact from a port divergence; deciding the 13 pixels' cause
would need a read of `vgamem` itself (DOSBox-X's debugger or a capture-side
hook), which is out of scope here. `make diff-verify` and `port_progress.py`
are unchanged (`1033/1033` … `825/845` … `809/845`; `798 1203 66` / `759 759
100`); the new scenario is not in `CAPTURE_DIRS`, the capture manifest or the
audio traces (it is a diagnostic, like `gp-u24-u16-state`).

---

## §D5 The capture exhibits (Tasks 5, 6, 7, 8)

The four capture tasks' measured outcomes, incorporated here (ledger R15: the
capture tasks record their corrections in their task reports, and the closing
task incorporates them; §D7.6 restates the corrections). Each capture is a new
`CAPTURE_DIRS` entry with a new `gp-u*-oracle` in `make verify` (each skips
without its capture) — except the name-entry exhibit, which has no oracle.

### §D5.1 ENDURANCE's team advance and match end (Task 5)

`gp-u25-endurance-end` clones `gp-u12-endurance` and adds four poked KOs of one
side (the U10 `KO_P2`/`DEATH_DONE` recipe keyed `after_entry 0x31 k`), with
`memsize=64` (the first capture OOM'd at the 16 MB default, record §W.13).
**Measured corrections:** the four-KO roster walk is confirmed (the loser's
roster byte `c1` walks `04 → 06 → 01 → 03`, as predicted), the counter that
reaches 4 is **`af1` (`0x104AF1`)** walking `0 → 4` (not the plan's `af0`, which
is the other side's byte), and the stop is mode **`0x17`** (not the plan's
`0x14`; ledger R6). The capture is **1,360
frames** and `gp-u25-oracle` is green (its `gp_endurance.py check` reads
mode/`c1`/`af0`/`af1`/`b25`/`w10d`); the mode-`0x33` 600-frame wait and the
results screen are the new pixel claims.

### §D5.2 The side-1 tour and ARMADON ending (Task 6)

`gp-u26-side1-ending` builds the "RIGHT PLAYER ARCADE" route (`u8` row 1 +
`p2.right`/`p2.start`) and drives seven poked wins for side 1. **Measured:** the
capture reaches mode `0x1F` with `b1f=2`, `ad4=1`, `c1=4` (ARMADON), and the
falsifiable cross-check — the ending content is side-independent — passes:
**268 mode-`0x1F` frames are byte-identical to `gp-u21`** (ARMADON's P1 ending).
The cross-check is `tools/gp_side1.py` with a mutation proof (11 `MAX_MISSING`
classes itemized). The capture's mode-`0x1E` records all carry `st=0000` (756 S
records; 49 replay frames), so states 7/8 never appear — which is why Task 8
was taken.

### §D5.3 The timeout/health drawn round (Task 7)

`gp-draw-timeout` pokes `0x10780A = 0x10789E = 0x30` and the clock top byte
`0x1088F2 = 0` in round 3 after two poked KOs. **Measured correction:** the
`clock = 0` predicate is read from the **`W` record's clock poke** — the round-3
arm re-reads `+0x5A` after `0x39FF4`, so the health values are bound to the
clock-poke frame and asserted `< 0x78`, not the earliest KO poke. `gp_draw.py
--timeout` requires `ad4=0xFFFFFFFF`, `clock=0`, `|A-B| <= 3`, `b1e==adc==3`,
modes `0x0A`/`0x0B`; `b16` is **not** pinned (`0x280BA` calls `flow_round_over`
without `0x27C48`, so `DS_00104B16` keeps round 2's value); the KO draw
(`ad4=2`) is asserted not to match.

### §D5.4 The name-entry letters exhibit (Task 8)

`gp-u27-name-entry` clones the U10 script and injects letter keys at mode `0x1E`
so the entry screen is driven, not just displayed. **Measured:** the exhibit
drives sub-states `b25 00 → 04 → 05 → 06 → 07 → 0A`, three letters and the
finaliser committing "ABD". It is an **exhibit, not a gated oracle**: its replay
has a pinned miss set (`k_miss_gp_u27_name_entry`, the two §G.24 wipe hooks
`0x29D60`/`0x5D812` from `frontend_mode_1b_step`), but there is no
`gp-u27-oracle` and `gp-u27-name-entry` is **not in `make verify`** (the exhibit
is not a regression gate, unlike the other gp captures).

---

## §D6 The OPL single-voice pitch test (Task 12)

The offline experiment the plan's §G7(b)/Task 12 asked for: does the vendored
Opal 2.0.3 core render one voice at the pitch its register bytes specify? The
test is `test_opl_pitch` in `port/tests/test_audio.c` (registered after
`test_opl` in `test.h`); it drives the core directly through
`opl_reset`/`opl_write`/`opl_render`, no mixer, no capture, no game state.

### §D6.1 The pitch derivation

The core's phase accumulator is `opal.c:472-508`: `channelWriteB0` (opal.c:753)
stores `chan->freq = (freq & 0xFF) | ((B0 & 3) << 8)` (the 10-bit fnum from the
A0 low byte and B0 bits 1-0) and `chan->octave = (B0 >> 2) & 7` (block); then
`operatorPhaseGenerate` computes `baseFreq = (fNum << octave) >> 1` and
`op->phase += (baseFreq * freqMultTimes2) >> 1`. With MULT = 1,
`freqMultTimes2 = mulTimes2[1] = 2` (opal.c:46/1452), so the per-sample
increment is `(fnum << block) >> 1`. One sine cycle is 1024 phase steps and
`phaseOut = phase >> 9` (opal.c:501), so a cycle is `2^19` phase-accumulator
units; samples per cycle = `2^19 / ((fnum << block) >> 1)` = `2^20 / (fnum *
2^block)`, and therefore

```
f = fnum * 49716 / 2^(20 - block)         (MIXER_OPL_RATE = 49716 = OPAL_OPL3_SAMPLE_RATE)
```

**Choice.** Target A4 = 440 Hz. `block = 4`, `fnum = 580`:
`f = 580 * 49716 / 65536 = 439.9915 Hz` (the ideal 440 Hz needs fnum 580.011, so
580 rounds it down by 0.0085 Hz). The fnum is a 10-bit integer, so its
**quantization step** is one fnum step = `49716 / 2^(20 - block)` =
`49716 / 65536 = 0.7586 Hz`.

**Window and tolerance.** One second at the native rate is `N = 49716` frames, so
a rising-zero-crossing count resolves the fundamental to `49716 / N = 1 Hz`. The
tolerance is the measurement resolution plus the fnum quantization (both
derived, not fitted):

```
tolerance = 49716 / N + 49716 / 2^(20 - block) = 1 + 0.7586 = 1.7586 Hz
```

### §D6.2 The measurement

The measured voice is channel 0, left-panned (`0xC0 = 0x10`), a muted modulator
(TL 63) and a sounding carrier (TL 0), algorithm 0 (no feedback), MULT 1, sine
— a pure carrier sine at f0. A second channel (right-panned `0xC0 = 0x20`,
fnum 288 block 6 = 873.914 Hz) exists only so the channel-swap mutation is a
pure pitch change. OPL3 mode (`0x105 = 0x01`) is enabled so the C0 output-enable
bits pan the two channels apart.

```
$ ./build/run_tests | grep 'OPL pitch'
OPL pitch: fnum=580 block=4 f=439.9915 Hz, N=49716, quantization=0.7586 Hz,
tolerance=1.7586 Hz; measured 439 rising zero crossings = 439.0000 Hz
```

`|439.0000 - 439.9915| = 0.9915 <= 1.7586`: the core renders the voice at the
derived pitch. The count is 439, not 440, because the phase starts at 0 (the
key-on reset) and the rising crossing at the start of the first cycle is not
counted; both 439 and 440 (and 441) fall inside the tolerance, so the assertion
is not boundary-fragile.

### §D6.3 The pitch mutations (both fail, then restore)

The test is mutation-proved two ways, each applied temporarily and reverted:

| mutation | edit | measured | verdict |
|---|---|---|---|
| channel swap | channel 0 takes `fnum2/block2`, channel 1 takes `fnum/block` (pan kept) | 873 rising crossings = 873.00 Hz | FAIL (433 Hz off) |
| block+1 | channel 0 block 4 → 5 | 879 rising crossings = 879.00 Hz | FAIL (one octave up) |
| (restored) | — | 439 crossings = 439.00 Hz | pass |

The channel-swap failure is a *pitch* failure, not a key-off: both channels stay
keyed (both B registers keep the key bit) and the pan is untouched, so the
measured left channel simply plays the other channel's note. The block+1 failure
doubles the fundamental (2^(20-(block+1)) halves the denominator), far outside
the 1.7586 Hz tolerance.

### §D6.4 The amplitude sanity (carrier TL)

The carrier TL field (register `0x43`) sets `outputLevel = (val & 0x3F) << 2`
(opal.c:1465) in 0.1875 dB units (opal.h:98), i.e. 0.75 dB per TL step. It adds
into `egOut` (opal.c:298), which feeds the exponential as
`operatorExpCalc(out + (envelope << 3))` (opal.c:292); `operatorExpCalc`
(opal.c:162) shifts `expTable[level & 0xFF]` by `level >> 8`, halving per 256
argument units. A +8 TL step therefore shifts the argument by `8 * 32 = 256` and
scales the amplitude by `2^(-8/8) = 0.5`; the RMS moves by the same factor
(mean square by `0.5^2 = 0.25`).

```
$ ./build/run_tests | grep 'OPL amplitude'
OPL amplitude: carrier TL 0 mean-square=8297402.98, TL 8 mean-square=2074930.29,
ratio=0.2501 (derived RMS factor 0.5 -> mean-square 0.25)
```

Measured mean-square ratio **0.2501** (RMS ratio 0.5001) against the derived
0.25 — exact to the exp ROM's integer-shift rounding. The amplitude assertion
fails under a no-op TL mutation (both renders at TL 0 → ratio 1.0000), so it is
not vacuous.

### §D6.5 What this claims, and the named limits

- **Claimed: the fundamental pitch** of one voice, derived from the register
  bytes and measured over a deterministic 1 s render. Not timbre: the test does
  not verify the FM harmonic content or the waveform shape beyond its zero
  crossings.
- **The dual-OPL2/OPL3 topology fork stays open.** The test keys a single 2-op
  voice in OPL3 mode (0x105 = 1, only so the C0 output-enable bits pan the two
  mutation channels apart); it says nothing about whether the game's
  dual-OPL2 register stream (§D1.5.3) is one OPL3 or two OPL2 chips, and does
  not decide that fork.
- **Opal's YMF262 accuracy is unmeasured.** The vendored core's synthesis
  arithmetic is upstream's; this test confirms the phase-accumulator pitch law
  only, not bit-accuracy against a real YMF262.
- **The mixer stays invented** (mixer.h:4-9); the test bypasses it and drives
  `opl_render` directly.

---

## §D7 The proof-debt declaration (Task 13)

The phase's closing record: what stays open after Tasks 1–12, each item with a
measured reason and an address. This task changes no code, no capture and no
pin; it consumes §D1's baseline, §D2's modes, §D3's U16 fix, §D4's 5120
INCONCLUSIVE and §D6's pitch test, and re-measures the counters it cites. No
row, class or universe is widened.

### §D7.1 The counters (re-measured for this task, not copied)

```text
$ make diff-verify            # this task's build, EXIT:0
diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps;
825/845 rows with callees closed (188 have none); 809/845 closed on full rows; 8 ported
functions with no row (45 FN_ ported-unrowed: 8 portable [5 registry-visible + 3 header-shape
exceptions] + 37 runtime; the scan-visible subset is 42 = 5 + 37).
```

```text
$ python3 - <<'EOF'   # WEAKER / UNROWED re-imported from tools/diff_verify.py
import importlib.util
spec = importlib.util.spec_from_file_location('dv','tools/diff_verify.py')
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
print("WEAKER:", len(m.WEAKER), ' '.join('%05X'%a for a in sorted(m.WEAKER)))
print("UNROWED:", len(m.UNROWED), ' '.join('%05X'%a for a in sorted(m.UNROWED)))
EOF
WEAKER: 13 1AFE8 1B084 1B544 1C740 1D0BC 249F0 24C5C 255CC 2CADC 2EA30 2EA68 2EA74 2EA78
UNROWED: 8 1B120 1B3AC 1BEC4 1C930 1CF40 20C10 47370 4FBA2
```

The tool's printed line is authoritative and confirms §D1.2: the full-rows
counter is **`809/845`** (not the stale `807/845` that AGENTS.md and
`port/RE_GUIDE.md` still carry), `825/845` with callees (so **20 open rows**),
and **8** ported functions with no row. The imported `WEAKER` frozenset holds
**13** addresses — `0x51ED8` and `0x1BE30` are absent (they left `WEAKER` when
claim-coverage Task 6 closed them; record
`2026-10-27-claim-coverage-derivations.md` §C1.10: full 807 → 809, WEAKER 15 →
13, `0x255CC` stays for its `0x256D4` spin). AGENTS.md's "15" and its `0x51ED8`
entry are stale and corrected in this task's docs pass. `UNROWED` reads **8**
(three of them `UNROWED_HEADER_SHAPE`: `0x1B120`, `0x20C10`, `0x4FBA2`).
`python3 tools/port_progress.py` is unmoved (`798 1203 66` / `759 759 100`; the
§D1.2 class breakdown). The three lists below are the harness's own tables
(`tools/tests/test_diff_verify.py`: `TERMINAL_CENSUS`, `WEAKER_CENSUS`,
`UNROWED_CENSUS`), not a second hand-written list, and their test derives the
open set from `closed_rows` and compares every blocker, so the declaration and
the counter cannot drift apart.

### §D7.2 The differential table

**20 open rows over 19 distinct addresses.** `attract_config_volumes_unscaled`
and `attract_config_volumes` are two specs at `0x2C8F0`, so the open set is
keyed `(name, entry)`. The 20 are 15 AIL runtime thunks, `sound_buffers_alloc`,
`input_sampler`, and the three runtime exit-flush rows; each is terminal because
its measured blocker is a runtime/host boundary a faithful row would have to
fabricate.

| row | entry | blocker(s) | class |
|---|---|---|---|
| `snd_music_stop` | `0x1CA6C` | `0x5DEAF` | AIL runtime stub |
| `snd_sample_playing` | `0x1CE70` | `0x5DD03` | AIL runtime stub |
| `snd_samples_stop_all` | `0x1CD9C` | `0x5DC0F`, `0x5DC8B`, `0x5DD03` | AIL runtime stubs |
| `snd_sample_stop` | `0x1CE04` | `0x5DC0F`, `0x5DC8B`, `0x5DD03` | AIL runtime stubs |
| `snd_sample_queue` | `0x1CC28` | `0x5DC0F`, `0x5DC8B`, `0x5DD03` | AIL runtime stubs |
| `snd_music_playing` | `0x1CA40` | `0x5DEED` | AIL runtime stub |
| `input_sampler` | `0x1BBAC` | `0x4FBBB`, `0x4FC05`, `0x4FCFD`, `0x4FDF5` | host-owned PIT/game-port stubs |
| `sound_music_pause_toggle` | `0x1D1B0` | `0x5DEAF` | AIL runtime stub |
| `sound_music_volume` | `0x1CAB8` | `0x5DECA`, `0x5DEED` | AIL runtime stubs |
| `sound_sfx_volume` | `0x1CED4` | `0x5DCC5`, `0x5DD03` | AIL runtime stubs |
| `sound_buffers_alloc` | `0x1D0BC` | `0x1C308` | host-owned audio/allocation stub |
| `attract_config_volumes_unscaled` | `0x2C8F0` | `0x5DCC5`, `0x5DD03`, `0x5DECA`, `0x5DEED` | AIL runtime stubs |
| `attract_config_volumes` | `0x2C8F0` | `0x5DCC5`, `0x5DD03`, `0x5DECA`, `0x5DEED` | AIL runtime stubs |
| `sound_sample_start` | `0x1CB18` | `0x5DC0F`, `0x5DC2A`, `0x5DC4D`, `0x5DC70`, `0x5DCA6`, `0x5DCC5`, `0x5DCE4` | AIL runtime stubs |
| `fight_char_screen_open` | `0x43738` | `0x5DCC5`, `0x5DD03`, `0x5DECA`, `0x5DEED` | AIL runtime stubs |
| `fight_char_screen_open_both` | `0x444C8` | `0x5DCC5`, `0x5DD03`, `0x5DECA`, `0x5DEED` | AIL runtime stubs |
| `config_file_load` | `0x1AFE8` | `0x6C4CF` | runtime exit-flush stub |
| `config_file_save` | `0x1B084` | `0x6C4CF` | runtime exit-flush stub |
| `menu_fatal_error` | `0x2EA68` | `0x6C4CF` | runtime exit-flush stub |
| `sound_shutdown` | `0x1D018` | `0x5D86A`, `0x5DC8B`, `0x5DD03`, `0x5DEAF`, `0x5DEED` | AIL runtime stubs |

Why each class is terminal (record §P8.1; the `_AIL_REASON` text): the AIL
implementation is host-owned (NG4) and the runtime thunk has no portable C
binding, so a callee row would validate a harness-authored audio model, not game
code; `sound_buffers_alloc`'s driver/allocation boundary (`0x1C308`) is
host-owned with no portable original-C binding; `input_sampler`'s four probes
(`0x4FBBB`/`0x4FC05`/`0x4FCFD`/`0x4FDF5`) are harness-owned hardware models; and
the three exit-flush rows all block on the unrowed runtime flush `0x6C4CF`
(`config_file_save` reopened by the short-write case, §P7.3). No row blocks on a
ported function in the portable set by accident — the 8 `UNROWED` functions are
callees of none.

**13 `WEAKER` rows** (the rowed functions whose row is not full), with the
one-line reason from `WEAKER_CENSUS`:

| entry | why it cannot be full | evidence |
|---|---|---|
| `0x1AFE8` | l6 declares the short-read exit-versus-defaults difference; that case compares no port side. | reverse-g §G2 |
| `0x1B084` | s5 declares the short-write exit-versus-failed-write-return difference; its port side is not compared. | proof-completion §P7.2 |
| `0x1B544` | The row covers only the resident arm; the slow tail-jump arm is named unhit (2/11 blocks). | reverse-g §G3 |
| `0x1C740` | The movie row pokes runtime/Smacker entry points and covers only the port's prefix (4/23 blocks). | reverse-n §N.3 |
| `0x1D0BC` | The two buffer-failure arms `0x1D10E`/`0x1D183`, ending in runtime printf, remain unhit. | proof-completion §P7.4 |
| `0x249F0` | The reachable restart arm `0x24A9C` remains unexercised after the longjmp became scan-only. | proof-completion §P6.3 |
| `0x24C5C` | The key rows omit the Alt-J/calibration path through `0x5004A` and the restart arm by design. | reverse-h §H.2 |
| `0x255CC` | The loop row leaves the spin block `0x256D4` unhit (19/20); the full copy `0x25680` runs in k4. | reverse-g §G4; claim-coverage Task 6 |
| `0x2CADC` | Three raw calls the port deliberately omits are nop pokes; only the seven calls the port makes are compared. | reverse-d6a §4 |
| `0x2EA30` | The SET-lock clear block `0x2EA49` is named unhit (7/8 blocks). | reverse-g §G3 |
| `0x2EA68` | Every case declares exit-versus-return, so the row proves the original but compares no port side. | reverse-g §G2 |
| `0x2EA74` | The key wait leaves the `0x5004A` calibration path and two other blocks named unhit (20/23). | reverse-h §H.2 |
| `0x2EA78` | The key wait leaves the `0x5004A` calibration path and two other blocks named unhit (20/23). | reverse-h §H.2 |

The two addresses that *left* `WEAKER` in claim-coverage Task 6 are `0x51ED8`
(the `sprite_blit_aperture` node-scratch byte difference, closed by the
`+0x14`/`+0x38` write-back) and `0x1BE30` (the teardown row's one code poke,
closed); they are full now and absent from the set. `0x255CC` stays weaker
because its `0x256D4` spin block is named unhit, even though its `0x25680` full
copy now runs in case k4.

**8 `UNROWED` functions** (ported functions with no differential row; none
open), with the port replacement and why a row would mask the boundary:

| entry | replacement | why a row would mask the boundary | evidence |
|---|---|---|---|
| `0x1B120` | res_load_index plus res_load_present and the host INDEX loader | The withdrawn poked row assigned `0x1B3AC`'s read/mark work and an invented frame effect to the port; it compared a composition, not the C function. | reverse-n §N.3.1/§N.4 |
| `0x1B3AC` | res_load_present's head, res_load_index's host reads/marks, and res_resolve's mark | No single C function spans the raw body; the port adds the ISR read-stall model and omits the raw driver stores, so a row would bind a synthetic composition. | reverse-n §N.3/§N.3.5/§N.4 |
| `0x1BEC4` | game_main, game_init, the host boot loaders, and game_shutdown | The boot chain has no one C counterpart and immediately reaches the BIOS query; a row would compare a harness-assembled boot against host startup. | reverse-n §N.1 |
| `0x1C930` | the host music-request/sequencer path (title_music_start) | The raw's AIL handle gates, copies, stops and clear have no port counterparts; a row would replace them with fabricated handle state. | reverse-n §N.2.3 |
| `0x1CF40` | game_audio_init's host audio objects, mixer reset and FAT.OPL load | The raw returns and stores ten AIL handles while the port deliberately stores different sentinels and does extra host work; a row would mask those differences. | reverse-n §N.2.2 |
| `0x20C10` | game_init, game_init_resume, and game_main's game_loop/restart call | The raw setjmp frame is split across those functions; entering only the resume tail lacks its stack frame, so no faithful single binding exists. | reverse-n §N.1 |
| `0x47370` | game_string_table_load's host file loader and fixed string handle | The port does not reproduce the raw allocator/pager, language index, free/reload or size stores; a row would validate an invented loader model. | reverse-n §N.3/§N.3.5 |
| `0x4FBA2` | int10h_query's host-window mode answer 0x13 | The raw returns BIOS AL from int 0x10; poking it would compare a harness-chosen constant with the port's constant and prove nothing about the BIOS. | reverse-n §N.1; reverse-d6a §4 |

**The span census and the E2 triage** (proof-completion §P2/§P4, claim-coverage
§C1.5.1; unchanged since, no code moved). The span blit's five tables hold **417
slots** and **392 distinct targets** (six renderers, one bare `ret`, **385
writer slots**). Of the 385 writer slots, **384 are reachable** and one —
`0x80E0C[128] = 0x57E3C`, the mirrored RLE's `lit_bwd/chunk128` entry — is
**dead** (a literal control byte is 7-bit, so index 128 is unreachable there).
The out-of-table `0x81110[128] = 0x81310[0] = 0x5D110` is reachable (the raw and
shear renderers call it for a span `>= 128` pixels) and is the **386th writer
target, outside the 385 slots**. The minimal fixture proves **26/26 code-shape
classes** and the exhaustive fixture **385/386 distinct targets (all 385
reachable)**, including both-edges clipping `0x5D580`/`0x582F4` and the clipped
14/15 word tails. **E2 triage:** the committed table lists **388 `span-writer`
rows** (the **384** writer-slot targets plus the four renderers `0x5215C`,
`0x57F80`, `0x57FFB`, `0x58CBD`); the one writer slot without an E2 row is the
residual **`0x5D004`** — slot 127 of the literal-forward table `0x81110` (dword
`0x8130C`, read by the `call` at `0x521AE`) — which lies at `0x5D004 >=
RUNTIME_BASE` (`0x5D000`) so universe rule (b) excludes it, and it is named in
the table's supplement row, not as a candidate. `make entry-triage` still reads
`736 candidates (732 by U0's rule); … span-writer=388 … supplement 131 (3
unported, 0 stale)` (§D1.2), byte-identical to a fresh run.

**The `sprite_blit_aperture` note.** One `make diff-verify` run during the phase
reported a pool mismatch in the `sprite_blit_aperture` (`0x51ED8`) row; three
immediate reruns of the same row were green and it was not reproduced, so it is
noted as a one-off harness observation, not a divergence — no row, case or pin
was changed on account of it.

### §D7.3 The storage table

The storage layer's open items (record `2026-10-13-reverse-s-derivations.md`
§S.8's PLAUSIBLE items and §S.10's slot-13 defect) are declared terminal with a
measured reason each:

| # | open item | declaration | measured reason |
|---|---|---|---|
| (a) | whether the original can itself save a slot byte of 13+ (the runtime's exit-routine walk `0x6C4CF` "not traced") | **terminal — the exit chain is save-free** | `0x2EA68` is `mov eax,1` then a tail-jump to `0x62003` (the runtime `_exit`; `port/src/game/menu.c:56`), and `0x6C4CF`'s closure reaches **only runtime call targets** (`>= 0x5D000`, WATCOM libc/DOS-4GW glue — never `0x1B084` `config_file_save`), so the original never writes a slot-13..35 save and no boot loads one. |
| (b) | — (the method behind (a)) | **the static trace** | the closure of `0x6C4CF` is read statically from its bytes/call targets (every target runtime), closing §S.8's "not traced" limb without a dynamic exit-chain run through the differential harness. |
| (c) | the two "nothing else stores it" absolute-operand scans (the byte `0x2D490`; `0x105DA4`/`0x105DA5`) | **already implicitly covered** | the differential rows compare every changed byte on both sides, so any other writer of either byte would already be a byte difference; the explicit static watch is optional and **not taken**. |
| (d) | which statistics screens a capture compares (under the audit's F2) | **already enforced** | the save-reload boot-2 comparison pins the five storage ranges (§J3.3: the `config` image, the kind-0 counters and the three high-score tables), which is the statistics screens the gate decides. |
| (e) | the 39 `config_field_get` consumers of a checksum-valid save, none audited for an index leaving `mem[]` | **bounded and audited** | the field indices the 39 consumers pass are bounded and the audit's walk found no index that leaves `mem[]`. |

### §D7.4 U17 and U14 declared (neither run)

**U17** (host-input-truth record §H6.9) is declared, not run: the
`gp-u17-device2` walk is **wall-paced** — `menu_run` (`0x2FA40`) blocks inside one
`game_loop_step`, so the master loop's frame counter freezes and the poller
writes no `S` record to key a frame-keyed script on; `port_script` therefore
refuses a wall scenario with a `ScriptError`, and the capture's frame/trace
ratchets (`MIN_FIRST`/`TRACE_MIN_FIRST`/`MAX_START`/`MIN_REACH`) stay empty.
Its screens (the MODIFY CONTROLS menu and the device-2 selection) are **already
covered** by the K11 walk/menuesc captures and the capture-side screen proof
(`make gp-u17-oracle` runs the evidence check and the identity pins only). The
tick-keyed replay is a named gap for the controller, not a claim; no capture is
re-run.

**U14** (the fought live win, R1) is declared, not run. §D1.6 measured the rule
mismatch: `tools/gp_win.py`'s `live_death` rule anchors on the round-2 KO row of
*match 1* (mode 9, k = 1) and requires a game-set `b0c = 1`, but `b0c`'s only
setter is `DSB(DS_00104B0C) = 1` at `0x37FF6` inside `0x37EA0`
(`fighter_37ea0`, the remains streams' `0xD100` target), which runs only on the
*ending* path after the seventh final KO — while the U14 scenario stops at mode
`0x12` in match 1, so no `S` record at or after the round-2 KO can ever hold
`b0c = 1`. The rule, not the capture, needs re-scoping. The **two options** are
recorded (design §1.5/§4): **(a)** drop the `b0c` clause and claim "fought KOs,
no pokes"; or **(b)** move the scenario to the final, where the remains stream
itself sets the byte. Both stay unmeasured; the nine `GP_U14_*` pins stay empty
and `gp-u14-oracle` keeps its absent-capture skip.

**The `b0c` non-universality correction (carried from §D1.7 item 4).** The
"seventh final KO" rule is a property of the captures that measured it, not of
every ending: five of the seven ending captures set `b0c = 1` after the final
(U10 char 0 `0x1602`, U18 char 2 `0x15F7`, U19 char 3 `0x1641`, U20 char 6
`0x1686`, U22 char 5 `0x1665`), but **BLIZZARD (U13) and ARMADON (U21) keep
`b0c = 0` in every `S` record**. So re-scope option (b) — move to the final and
let the remains stream set the byte — would still fail for those two characters,
whose endings never set `b0c`. The declaration records this; it does not weaken
or widen the rule.

### §D7.5 The B2–B8 replacement and the census baseline's provenance

The claim-coverage design's non-goal §4 names "the remaining
front-end/ENDURANCE/high-score/finisher/service-menu blind spots (B2, B3, B4,
B6, B7, B8)" and its §G repeats "spots B2/B3/B4/B6/B7/B8", but those labels are
**never defined anywhere in the repo** (no table expands B2..B8). The measured
replacement is the census's NOT-HIT set plus the declared modes, both owned by
this phase:

- **The census's measured NOT-HIT set** (`tools/coverage_baseline.txt`): **102**
  portable addresses no driver entered, plus **4** `UNINSTRUMENTED` (`0x29D60`,
  `0x2EA30`, `0x2EA64`, `0x500BB`) against **653** `HIT`. (Task 3's original pin
  was `HIT 645 / NOT-HIT 110 / UNINSTRUMENTED 4`; Task 14 re-pinned it — §D8.3 —
  after the phase's captures reached eight of the NOT-HIT addresses.) The
  NOT-HIT set (in baseline order) is:

```text
12BB8 12FD8 13134 1324C 13B3C 14988 14A5C 14B90 17BC8 18950 18B44 1B890 1B8DC
1B908 1BE30 1BEC4 1ECC8 20860 21458 216EC 26540 266AC 271E0 27254 278B0 2791C
27A2C 29C20 2A148 2BD20 2BD44 2C1C8 2C1D4 2CA48 2E0A4 2EA68 2F940 305FC 319B0
33B00 36300 37464 37B54 380C4 38154 382C4 38434 384F8 385B0 3A8E8 3B90C 3C1C8
3D004 3E6A8 3EA24 3EE00 3F9C8 3FF08 40608 406B4 408E8 40954 40BBC 41578 416D4
42FE0 43738 43928 44054 4505C 45908 45B50 48AAC 48C4C 48D94 4987C 4A808 4A8A8
4AF04 4BD98 4BF18 4C60C 4C784 4CB18 4CC0C 4CD98 4CE70 4D898 4E11C 4E27C 4E350
4E5A4 4E67C 4E99C 4EBB8 4EF8C 4F0FC 4F6E8 4F704 4F944 4FF8F 4FFD8
```

  The content-bearing subset is the declared modes and their co-entered rows
  (§D2.6): `0x4BD98`/`0x4E11C`/`0x27A2C` are the `0x21`/`0x25`/`0x0E` entries.
  The drawn round's `0x0A`/`0x0B` handlers (`0x28BD4`/`0x28C38`) and the
  mode-`0x32`/`0x33` team/match-end entries (`0x292D4`/`0x29638`/`0x296B8`), plus
  the round-end debris spawner (`0x19820`/`0x19958`) and the mode-`0x0A` setter
  (`0x27ED8`), left the NOT-HIT set in Task 14's re-pin — Task 7's
  `gp-draw-timeout` and Task 5's `gp-u25-endurance-end` exhibit them (§D8.3);
  `0x0B`'s KO-draw winner pose stays unexhibited (§D2.7, `gp-draw` never
  published). The `front-end/ENDURANCE/high-score/finisher/service-menu` blind
  spots the B-labels meant are exactly the remaining NOT-HIT addresses, none of
  which is a regression.

- **The declared modes** `0x21` (volleyball), `0x25` (mirror-ARMADON camera) and
  `0x0E` (continue screen) — §D2 — whose two-human/credit gates a poked 1P
  scenario cannot open, so they stay declared, not captured.

**The census baseline's provenance — a separate pin, not the pin-baseline
tripwire.** `tools/coverage_baseline.txt` is a pin: its header records the
measured provenance (`measured 2026-10-29, make coverage runs 1 and 2`, the
portable HIT union sha256 `e93d7415f1…`, `HIT 645 / NOT-HIT 110 /
UNINSTRUMENTED 4` — re-pinned by Task 14 §D8.3 to `HIT 653 / NOT-HIT 102 /
UNINSTRUMENTED 4`, reason in the header), and `coverage_census.py --check` fails
on a new NOT-HIT. It
is **not** part of `tools/pin_baseline.sha256`: that file is the
capture/oracle pin-region tripwire over the marker-delimited regions of
`Makefile`, `tools/audio_compare.py`, `tools/k11_compare.py`,
`tools/gp_compare.py` and `tools/title_compare.py` (record `…-claim-coverage`
§C1.7), a different mechanism. The census baseline therefore belongs in its own
file (as it already is), not in the pin-baseline digest, and this task changes
neither.

### §D7.6 The phase corrections carried into the declaration (R15)

The measurement tasks' corrections, restated so the declaration is self-contained:

- **ENDURANCE (Task 5):** the roster counter is `af1` (`0x104AF1`), walking
  `0 → 4`; the stop is mode `0x17`, not the plan's predicted `0x14`. The
  capture `gp-u25-endurance-end` is pinned by `gp-u25-oracle`; `af0` is the other
  side's byte.
- **The timeout draw (Task 7):** the `clock = 0` predicate is read from the `W`
  record's clock poke (the round-3 arm re-reads `+0x5A` after `0x39FF4`, so the
  health values are bound to the clock-poke frame and asserted `< 0x78`, not the
  earliest KO poke).
- **The U16 fix (Task 10, §D3.7):** the reaction's null table base — it reads
  the IVT at linear 0 — is fixed by the host stand-in `bios_ivt_stand_in()`
  seeding `mem[1] = 0xCA`/`mem[3] = 0xF0` (the inverted dwell `-3456`/`-4480` =
  `y * 64`); the x bytes `mem[0]`/`mem[2]` are the adjacent record-motion defect,
  recorded not fixed. Only `GP_U23_TRACE_MIN_FIRST` and
  `GP_U23_MOVES_MIN_FIRST` re-pinned (`5449/5467 → 6050`); every other pin is
  unchanged.
- **The 5120 experiment (Task 11, §D4):** **INCONCLUSIVE** — the DOSBox-X memory
  file holds the guest's main RAM only, not the VGA aperture (`vgamem`), so the
  "real aperture at the S record" reads `0xFF` at every offset and the
  capture-pipeline-artifact vs port-divergence question stays undecided; the
  band's bounds `(5120, 1, 5, 56b900b1…)` and every pin are unchanged and the gap
  stays declared.
- **`0x0B` stays unexhibited** (§D2.7): `gp-draw` never published, so the KO
  drawn winner pose has no capture, no oracle and no miss set.

### §D7.7 What this declaration claims

The declaration proves the *measured shape* of the remaining gap — 20 open / 13
`WEAKER` / 8 `UNROWED` differential rows, the 385-slot span census and the
388-row E2 triage, the storage table's five terminal items, and U17/U14 as
declared with the `b0c` non-universality noted — each with an address and a
reason. It does **not** close any of them: no row, class or universe is widened,
no capture or pin changes, and `make diff-verify`, `port_progress.py`,
`entry-triage` and the coverage/pin baselines are byte-identical to their
pre-declaration readings. The B-labels are replaced by the census's NOT-HIT set
and the declared modes, which is what those labels always meant but never
expanded to.

---

## §D8 The gate and the docs (Task 14)

The phase's closing record: the two full gates, the two `audio-render` runs, and
every counter re-verified. Documentation only — no code, no capture, no pin.

### §D8.1 The two full gates

Both runs are `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify`,
run sequentially (never concurrently; the oracles are load-sensitive) from the
repo root. The full gate is **~88 minutes** with `PR_DIFF_FULLSCAN=1` (the
write-tracking-only form is ~26; the full-scan `diff-verify` alone runs 234
harness tests in ~591 s plus the whole-memory scans). Load was 0.78 at start and
peaked ~12–13 during the diff-verify pool run.

```text
$ PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify   # run 1, log /tmp/opencode/task14-gate1.log
gate: captures 37/38 present, 1 missing (strict=1, pins_exact=1)
gate: declared gap (absent): data/k11-captures/gp-u14-win-live
capture_manifest: 37 present checked, 1 declared skipped, 0 problems
capture_manifest: pin baseline ok (digest eed6eff69cedef33d57c590862ad3c4a3a0fd1ce1ec59a6d32f844cbca2f3579)
capture_manifest: verify preflight ok
...
diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps;
825/845 rows with callees closed (188 have none); 809/845 closed on full rows; 8 ported
functions with no row (45 FN_ ported-unrowed: 8 portable [5 registry-visible + 3 header-shape
exceptions] + 37 runtime; the scan-visible subset is 42 = 5 + 37).
entry-triage: 736 candidates (732 by U0's rule); finisher=9 move-callback=71 span-writer=388
  call-table=7 anim-target=112 mid-instruction=3 data=75 code-immediate=15 direct=2 interior=6 data-pointer=48
entry-triage: targets 390 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
...
== symbols.h must regenerate byte-identically ==
python3 tools/gen_symbols.py port/decomp port/src/symbols.h
1304 globals, 1206 functions -> port/src/symbols.h
  dropped 11 globals, 1 functions outside the LE objects
all checks passed
=== gate run 1 exit: 0 at Thu Oct 29 02:21:18 PM -03 2026 ===
```

Run 2 (log `/tmp/opencode/task14-gate2.log`, start 14:23:37, exit 0 at 15:50:53)
prints the same key lines — `gate: captures 37/38 present, 1 missing`, the
identical `diff-verify` counter, the identical `entry-triage` line, and the
identical `PR_AUDIO_SONG_WAV` peak/sha — and exits 0. No `improved:`/`re-pin`
line appears in either run, so no pin drifted.

**Plan-vs-raw correction (R16): the gate inventory is `37/38 present`, not the
brief's `36 present / 1 declared`.** The phase added three `CAPTURE_DIRS` entries
(`gp-u25-endurance-end`, `gp-u26-side1-ending`, `gp-draw-timeout`) to the design
§2 baseline's `34/35`, so the count is 38 paths, 37 present, `gp-u14-win-live`
the one declared absence. The brief's "36 present / 1 declared" (37 total) is
off by one.

The three new oracles' key result lines (run 1):

```text
gp_endurance: gp-u25-endurance-end: evidence: KO 4: af1=4 ends the match -> mode 0x33 (the 600-frame wait) at f=716 ok
gp_endurance: gp-u25-endurance-end: evidence: 7/7 milestones ok
gp_compare: gp-u25-endurance-end: frames: first unexplained 598, ratchet N 598 ok
gp_side1: 268 mode-0x1F frames compared, all byte-identical in rendered RGB
gp_side1: concat sha256 975684936cf55071cca1932c2a3c0bdeb80910c313fd1213209d97d14bd8f427
gp_draw: gp-draw-timeout: timeout: the drawn round: ad4 = 0xFFFFFFFF, b1e = adc = 3, |A - B| <= 3, clock 0x1088F2 poked 0, mode 0x0A at f=B02, mode 0x0B at f=B26 ok
gp_compare: gp-draw-timeout: frames: first unexplained 346, ratchet N 346 ok
```

### §D8.2 The two `audio-render` runs

`make audio-render` runs standalone twice after the gates; both are byte-identical
(the determinism pin; the claim is the port's own deterministic non-silence, not
audibility — record §M5):

```text
$ make audio-render   # run 1 and run 2, identical
PR_AUDIO_WAV: wrote /tmp/pr_title_fm.wav (12 s, 49716 Hz)
PR_AUDIO_SONG_WAV: wrote /tmp/pr_selmo_fm.wav (12 s, 49716 Hz) peak 2790 sha256 530a94301f4be46f9a5c666c94516f8a1f7fca6f136d3be0562faed802c47cef
all checks passed
```

The selmo (s16selmo `0x0980025C`, volume `0x50`) renders at **peak 2790**, sha256
`530a94301f4be46f9a5c666c94516f8a1f7fca6f136d3be0562faed802c47cef`, identical
across both runs (the pinned `SELMO_WAV_PEAK`/`SELMO_WAV_SHA256` in
`test_audio.c`).

### §D8.3 The counters (each command and its line)

```text
$ python3 tools/port_progress.py
798 1203 66
759 759 100
ported 798 (759 portable + 39 runtime)  host-owned 41  deferred 12  replaced 37  runtime 315

$ make gate-check
gate: captures 37/38 present, 1 missing (strict=1, pins_exact=1)
gate: declared gap (absent): data/k11-captures/gp-u14-win-live
capture_manifest: 37 present checked, 1 declared skipped, 0 problems
capture_manifest: pin baseline ok (digest eed6eff69cedef33d57c590862ad3c4a3a0fd1ce1ec59a6d32f844cbca2f3579)
capture_manifest: verify preflight ok

$ make diff-verify   # (also the gate's step; the line is the FULLSCAN reading above)
diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps; 825/845 rows with callees closed (188 have none); 809/845 closed on full rows; 8 ported functions with no row

$ make entry-triage
entry-triage: 736 candidates (732 by U0's rule); finisher=9 move-callback=71 span-writer=388 call-table=7 anim-target=112 mid-instruction=3 data=75 code-immediate=15 direct=2 interior=6 data-pointer=48
entry-triage: targets 390 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30

$ python3 tools/capture_manifest.py --check-pins
capture_manifest: pin baseline ok (digest eed6eff69cedef33d57c590862ad3c4a3a0fd1ce1ec59a6d32f844cbca2f3579)

$ python3 tools/coverage_census.py --check-instrumented
check-instrumented: every .c column-0 header is instrumented

$ python3 tools/coverage_census.py --check    # needs the coverage/ dumps (make coverage, outside verify)
```

`symbols.h` idempotence is the gate's closing step (it regenerates
byte-identically and the `git diff --quiet` guard passes). The coverage census's
`--check` compares the `coverage/<tag>.hits` dumps against the committed
`tools/coverage_baseline.txt`; the dumps are produced by `make coverage` (the
38-minute PR_COVERAGE build, **outside `verify`**). The four `UNINSTRUMENTED` are
`0x29D60`, `0x2EA30`, `0x2EA64`, `0x500BB`.

**R17 — the coverage baseline was re-pinned to eight `improved:` addresses.** This
task re-ran `make coverage` (log `/tmp/opencode/task14-coverage.log`) to
regenerate the dumps and `--check` them against the committed baseline; the
census printed, instead of a clean `baseline ok`:

```text
missing gp-u14-win-live
coverage: split HIT=653 NOT-HIT=102 UNINSTRUMENTED=4
improved: 0x19820 0x19958 0x27ED8 0x28BD4 0x28C38 0x292D4 0x29638 0x296B8
make: *** [Makefile:2539: coverage] Error 2
```

The committed baseline (Task 3, measured 2026-10-29 before the capture tasks
landed) read **HIT 645 / NOT-HIT 110 / UNINSTRUMENTED 4**; the re-run read
**HIT 653 / NOT-HIT 102 / UNINSTRUMENTED 4** — exactly eight addresses moved
NOT-HIT → HIT. Measured per-tag attribution (from the dumps, not the report's
prose): `gp-u25-endurance-end` reaches `0x292D4` (the mode-`0x32` team list),
`0x29638`/`0x296B8` (`game_mode_33_step`/`game_mode_32_step`), and
`gp-draw-timeout` reaches `0x19820`/`0x19958` (the round-end debris spawner),
`0x27ED8` (the mode-`0x0A` setter), `0x28BD4` (`game_mode_0a_step`) and
`0x28C38` (`flow_winner_pose_step`, mode `0x0B`) — the earlier report prose
wrongly credited `0x19820`/`0x19958` to ENDURANCE. **Re-pinned** with the
measured reason (design §7.1 step 3's "a new hit is an `improved:` re-pin with a
recorded reason"):

```text
$ python3 tools/coverage_census.py --update-baseline --reason "state coverage Task 14: eight addresses improved by the phase's captures (gp-draw-timeout: 0x19820 0x19958 0x27ED8 0x28BD4 0x28C38; gp-u25-endurance-end: 0x292D4 0x29638 0x296B8)"
coverage: baseline written (…/tools/coverage_baseline.txt)
```

After the re-pin, `--check` exits 0 (`coverage: split HIT=653 NOT-HIT=102
UNINSTRUMENTED=4`, no `improved:`/`new NOT-HIT` line) and `--check-instrumented`
passes (`every .c column-0 header is instrumented`). The baseline header now
records the reason above.
