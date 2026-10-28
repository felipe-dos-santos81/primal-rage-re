# Claim Coverage — derivation record

- **Plan:** `docs/superpowers/plans/2026-10-27-claim-coverage.md`
- **Spec:** `docs/superpowers/specs/2026-10-27-claim-coverage-design.md`
- **Branch / worktree:** `claim-coverage` at
  `/home/felipe/.local/share/opencode/worktree/a7f742/claim-coverage`
- **Base commit:** `35274191` (`docs: add the claim coverage design and plan`),
  on `07894c7d` (`audio: close the register-stream oracle final review`).

This record is the raw-byte and tool-output source for the phase's tasks. Each
task appends its own section (§C2 Task 2, …); §C1 is Task 1's measured baseline
and every later task cites it. Nothing in §C1 changes port code or tool
behaviour: it is measurement and documentation only.

---

## §C1 The measured baseline (Task 1)

### §C1.1 Environment

| item | value |
|---|---|
| host | Linux, 20 CPUs (`nproc` = 20) |
| build | `make build` from scratch in this worktree (no `build/` existed); succeeded |
| `data/` | symlink to the shared 7.0 GB tree; `data/game` read-only, no capture written |
| `port/tests/ghidra_data.bin` | present (copied in per the AGENTS.md worktree rule) |
| dependencies | `unicorn` 2.1.4, `capstone` 5.0.7 present, so `diff-verify` and `entry-triage` run |
| U14 | **not attempted**: no `gp-u14-win-live` capture was made (design approval; plan Task 12 skipped) |

`make verify` was **not** run to completion in Task 1 (it is ~26 minutes and the
mid-phase gate owns it). The steps below are the gate's components, run
individually on the unmodified tree.

### §C1.2 The counters (plan Step 1)

```text
$ python3 tools/port_progress.py
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)
```

```text
$ python3 tools/port_progress.py --unported | head -3
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)
6FB28  4979  28  29  runtime
...
$ python3 tools/port_progress.py --unported | wc -l
353                  # 2 header lines + 351 rows, every row `runtime`
```

`--unported` lists **351 rows and every one is `runtime`** (≥ `0x5D000`); the
portable half has nothing unported.

```text
$ make gate-check
gate: captures 34/35 present, 1 missing (strict=1, pins_exact=1)
gate: declared gap (absent): data/k11-captures/gp-u14-win-live
EXIT:0
```

```text
$ make diff-verify            # log /tmp/opencode/t1-diff-verify.log, EXIT:0
diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps;
825/845 rows with callees closed (188 have none); 807/845 closed on full rows; 8 ported
functions with no row (45 FN_ ported-unrowed: 8 portable [5 registry-visible + 3 header-shape
exceptions] + 37 runtime; the scan-visible subset is 42 = 5 + 37). The closure rule (record J
§J1): an allow-mode callee outside the portable set - runtime >= 0x5D000, host-owned/deferred
in tools/port_classification.txt, or an address the decomp index never lists as a function -
does not keep a row open, provided the port tree does not implement the address in any class
(no fn_register, no column-0 `/* 0xADDR` header), while a recorded stub or real callee and a
ported function without a row do. Claim: equivalence on the exercised blocks and inputs only,
each function with its callees stubbed or run as stated.
```

The table it printed holds **1033 `VERIFIED` function rows** and **14993
`MISMATCH` mutant rows** (the mutant verdict's desired value) — the two numbers
in the counter line, measured row by row:

```text
$ awk -F'|' '/^\| / {gsub(/^ +| +$/,"",$6); if ($6 ~ /^VERIFIED/) v++; else if ($6 ~
  /^MISMATCH/) m++; else o++} END{print "VERIFIED:",v," MISMATCH:",m," other:",o}' \
  /tmp/opencode/t1-diff-verify.log
VERIFIED: 1033  MISMATCH: 14993  other: 1     # `other` is the table header
```

#### §C1.2.1 The three differential lists, extracted from the modules

The plan's Step 1 names `TERMINAL_CENSUS`, `WEAKER` and `UNROWED` as members of
`tools/diff_verify.py`. **Plan-vs-raw correction:** only the last two are; the
census is data in the harness suite, `tools/tests/test_diff_verify.py:30829`.
The lists below are read out of those modules, not retyped from AGENTS.md:

```text
$ python3 - <<'EOF'   # WEAKER / UNROWED from tools/diff_verify.py
import importlib.util
spec = importlib.util.spec_from_file_location('dv','tools/diff_verify.py')
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
print("WEAKER:", len(m.WEAKER), ' '.join('%05X'%a for a in sorted(m.WEAKER)))
print("UNROWED:", len(m.UNROWED), ' '.join('%05X'%a for a in sorted(m.UNROWED)))
print("UNROWED_HEADER_SHAPE:", len(m.UNROWED_HEADER_SHAPE), ' '.join('%05X'%a for a in sorted(m.UNROWED_HEADER_SHAPE)))
EOF
WEAKER: 15 1AFE8 1B084 1B544 1BE30 1C740 1D0BC 249F0 24C5C 255CC 2CADC 2EA30 2EA68 2EA74 2EA78 51ED8
UNROWED: 8 1B120 1B3AC 1BEC4 1C930 1CF40 20C10 47370 4FBA2
UNROWED_HEADER_SHAPE: 3 1B120 20C10 4FBA2
RUNTIME_UNROWED: 37
```

The **15 `WEAKER` entries** are `0x1AFE8`, `0x1B084`, `0x1B544`, `0x1BE30`,
`0x1C740`, `0x1D0BC`, `0x249F0`, `0x24C5C`, `0x255CC`, `0x2CADC`, `0x2EA30`,
`0x2EA68`, `0x2EA74`, `0x2EA78`, `0x51ED8`. The **8 `UNROWED`** functions are
`0x1B120`, `0x1B3AC`, `0x1BEC4`, `0x1C930`, `0x1CF40`, `0x20C10`, `0x47370`,
`0x4FBA2`; three of them (`0x1B120`, `0x20C10`, `0x4FBA2`) are
`UNROWED_HEADER_SHAPE` (implemented but named only mid-comment). `WEAKER`'s 15
entries cover **24 differential rows**: `0x24C5C` alone has ten, the other
fourteen one each (14 + 10 = 24 — the design's §2 figure).

#### §C1.2.2 The 20 open rows (`TERMINAL_CENSUS`)

Extracted from `tools/tests/test_diff_verify.py`
(`len(TERMINAL_CENSUS)` = 20 rows over **19 distinct addresses**: `0x2C8F0`
carries two specs, `attract_config_volumes_unscaled` and
`attract_config_volumes`):

| entry | row name | class of reason |
|---|---|---|
| `0x1CA40` | `snd_music_playing` | AIL runtime thunk (NG4) |
| `0x1CA6C` | `snd_music_stop` | AIL runtime thunk |
| `0x1CAB8` | `sound_music_volume` | AIL runtime thunk |
| `0x1CB18` | `sound_sample_start` | AIL runtime thunk |
| `0x1CC28` | `snd_sample_queue` | AIL runtime thunk |
| `0x1CD9C` | `snd_samples_stop_all` | AIL runtime thunk |
| `0x1CE04` | `snd_sample_stop` | AIL runtime thunk |
| `0x1CE70` | `snd_sample_playing` | AIL runtime thunk |
| `0x1CED4` | `sound_sfx_volume` | AIL runtime thunk |
| `0x1D018` | `sound_shutdown` | AIL runtime thunk |
| `0x1D1B0` | `sound_music_pause_toggle` | AIL runtime thunk |
| `0x2C8F0` | `attract_config_volumes_unscaled` | AIL runtime thunk |
| `0x2C8F0` | `attract_config_volumes` | AIL runtime thunk |
| `0x43738` | `fight_char_screen_open` | AIL runtime thunk |
| `0x444C8` | `fight_char_screen_open_both` | AIL runtime thunk |
| `0x1BBAC` | `input_sampler` | the four host-owned PIT/game-port probes |
| `0x1D0BC` | `sound_buffers_alloc` | host allocation boundary (blocked by `0x1C308`) |
| `0x1AFE8` | `config_file_load` | the runtime exit flush `0x6C4CF` is unrowed |
| `0x1B084` | `config_file_save` | the same, reopened by the short-write case |
| `0x2EA68` | `menu_fatal_error` | the same (exit-vs-return terminal difference) |

The 15 AIL-class rows' reason is one string: *"The AIL implementation is
host-owned (NG4); the runtime thunk has no portable C binding, so a callee row
would validate a harness-authored audio model, not game code."* Note the two
rows named `fight_char_screen_open*` (`0x43738`, `0x444C8`) carry the AIL
reason, not a menu one; the name is the spec's, the reason is the census's.
`input_sampler`'s reason: *"All 48 sampler blocks are proved with modeled host
answers; rows for the four probes would verify harness-owned hardware models,
which NG4 excludes."*

### §C1.3 The audio oracle under load (plan Step 2)

**The flake reproduces.** Two consecutive `make audio-oracle` runs under
generated CPU load both **exit 2**, on the same failing trace with the same
signature, and a second trace fails in only one of the two.

**Conditions.** 20 CPU-bound `python3 -c 'while True: pass'` processes on 20
CPUs (one per core), started 10 s before run 1 and killed after run 2; nothing
else was run by this task during the pair. `uptime` at the three points:
`load average: 20.21, 16.41, 11.83` (before run 1), `31.71, 23.45, 15.38`
(before run 2), `30.69, 26.90, 18.13` (after run 2). Each run took `real` 175.5 s
and 175.7 s (`AUDIO_JOBS` = 4). Logs: `/tmp/opencode/t1-audio-run1.log`,
`/tmp/opencode/t1-audio-run2.log`; the dumps are
`/tmp/opencode/t1-audio-dump-run1-copy` (run 1's, copied before run 2
overwrote the path) and `/tmp/opencode/t1-audio-dump-run2`.

```text
$ make audio-oracle AUDIO_DUMP=/tmp/opencode/t1-audio-dump-run1     # RUN1_EXIT:2
audio-compare: gp-u18-talon-ending: FAIL
audio-compare: gp-u18-talon-ending: FAIL: section s16beach.gra+0x8 first difference (tick groups differ) at relative tick 253
audio-compare: gp-u18-talon-ending: FAIL: section 3 bank 87E7C2DE: expected matched, got diff
audio-compare: gp-u18-talon-ending: FAIL: claimed 718 ticks, pinned minimum 964
audio-compare: gp-u20-chaos-ending: FAIL
audio-compare: gp-u20-chaos-ending: FAIL: section s16caves.gra+0x8 first difference (tick groups differ) at relative tick 228
audio-compare: gp-u20-chaos-ending: FAIL: section 3 bank 86946EB7: expected a named gap, got diff
make: *** [Makefile:2162: audio-oracle] Error 123
# 25 traces ok, 1 skip (gp-u17-device2, the declared wall-paced scenario)

$ make audio-oracle AUDIO_DUMP=/tmp/opencode/t1-audio-dump-run2     # RUN2_EXIT:2
audio-compare: gp-u18-talon-ending: FAIL
audio-compare: gp-u18-talon-ending: FAIL: section s16beach.gra+0x8 first difference (tick groups differ) at relative tick 253
audio-compare: gp-u18-talon-ending: FAIL: section 3 bank 87E7C2DE: expected matched, got diff
audio-compare: gp-u18-talon-ending: FAIL: claimed 718 ticks, pinned minimum 964
make: *** [Makefile:2162: audio-oracle] Error 123
# 26 traces ok, 1 skip
```

`gp-u18-talon-ending` fails **both** runs with the identical claim: the
`s16beach+0x8` (87E7C2DE) section's first difference at its relative tick 253
(port `[<none>]` vs capture `0xb7=0x0f 0xb8=0x0f`), and the trace claims **718**
ticks against its pinned minimum of **964**. `gp-u20-chaos-ending` fails only in
run 1: its `s16caves+0x8` (86946EB7) section *anchored* (898 claimed ticks over 3
sections) and differed at relative tick 228, where the pin expects that bank to
be a named gap; run 2 leaves it unanchored and claims 416 ticks over 2 sections —
`416` is the trace's pin (`TRACE_PINS`), so run 2 is green on it.

#### §C1.3.1 The label divergence, measured field by field

Same-trace `audio.txt` comparison, run 1's dump vs run 2's:

| trace | lines (run1 / run2) | differing lines | fields that differ |
|---|---|---|---|
| `gp-u18-talon-ending` | 3012 / 3012 | 779 | **772 `W` frame labels**, **2 `W` tick labels**, **5 `S` `ticks` labels**; every register/value/attr field identical |
| `gp-u5-charsel` (passes both runs) | 3218 / 3218 | 2621 | 2608 `W` frame labels, 8 `W` tick labels, 5 `S` `ticks` labels; register/value identical |
| `gp-u20-chaos-ending` | 3247 / 3191 | not line-aligned | the streams themselves differ (56 more lines in run 1): `W` tick, write index, register, value and attr fields all move, and the section inventory changes (898 claimed ticks over 3 sections vs 416 over 2) |

`gp-u18-talon-ending`'s exact label pairs:

- 772 `W` frame labels move: **592 are one lower in run 2, 180 are two lower**
  (e.g. `W 655 50 60 0029 31 04` in run 1 is `W 654 50 60 0029 31 04` in run 2);
- two `W` tick labels: writes `1488`/`1489` read tick `310` in run 1 and `314`
  in run 2;
- five `S` `ticks` labels: frame 216 `496 → 498`, 631 `1334 → 1336`, 759
  `1592 → 1594`, 1037 `2148 → 2150`, 1188 `2458 → 2464`.

The section anchors move with them: run 1 anchors `s16selmo+0x25C` at capture
frame 655, run 2 at frame 654 (song tick 50 in both); `gp-u20-chaos-ending`'s
anchor moves 650 → 651 and the two later sections anchor in one run and not the
other. This is the same class of divergence the fought-match record §F6.3
measured (`gp-u18-talon-ending` there: 183 differing label lines, 180 `W` frames
one lower, two `W` ticks `308 → 310`, one `S` `ticks` `2454 → 2456`) — the
magnitude differs per run pair, the mechanism (a label, not the music, moving)
does not. Note that `gp-u5-charsel` **passes** with 2,608 moved frame labels:
frame labels alone do not fail a trace; the tick labels and the section-splitting
do.

**What §C1.3 establishes for Task 2.** The oracle is not reproducible under
load on the current tree: one trace fails in both runs of the pair, a second in
one of two, and a passing trace's dump still differs in 2,621 label lines.
Task 2 owns the clock fix and the re-pin; §C1 records the reproduction, the two
exact failing claims, the pin (`964`) and the label deltas above, and it claims
nothing about a fix.

### §C1.4 The classification audit (plan Step 3)

#### §C1.4.1 `tools/port_classification.txt` — 55 entries, 53 effective

```text
$ wc -l tools/port_classification.txt
65 tools/port_classification.txt
$ grep -v '^#' tools/port_classification.txt | awk '{print $2}' | sort | uniq -c
     13 deferred
     42 host-owned
$ grep -vc '^#' tools/port_classification.txt
55
```

55 entries = **42 `host-owned` + 13 `deferred`**. Two of them are not `FN_`
addresses in `symbols.h` and therefore affect nothing (the design's "2 inert"):

```text
$ python3 - <<'EOF'
import re
fns={int(m,16) for m in re.findall(r'^#define FN_([0-9A-F]{8}) ', open('port/src/symbols.h').read(), re.M)}
e=[(int(l.split()[0],16), l.split()[1]) for l in open('tools/port_classification.txt') if l.strip() and not l.startswith('#')]
print("entries:", len(e), "not in symbols.h:", [(hex(a),c) for a,c in e if a not in fns])
for cls in ('host-owned','deferred'):
    print(cls, sum(1 for a,c in e if c==cls), "effective", sum(1 for a,c in e if c==cls and a in fns))
print("effective total:", sum(1 for a,c in e if a in fns))
EOF
entries: 55 not in symbols.h: [('0x10604', 'deferred'), ('0x1bdf4', 'host-owned')]
host-owned 42 effective 41
deferred 13 effective 12
effective total: 53
```

So the effective split is **41 host-owned + 12 deferred = 53**, which is exactly
`port_progress.py`'s "excludes 53". `0x10604` is the deferred 250 Hz callback's
register site; `0x1BDF4` is the host-owned 60 Hz AIL callback.

#### §C1.4.2 The `0x62003` false positive

`0x62003` **is** an `FN_` address (`symbols.h:2235`,
`#define FN_00062003 0x00062003u`) and `port_progress.py` credits it as ported
from one comment only, which says the address is out of scope:

```text
$ grep -rn '/\* 0x62003' port/src/
port/src/game/fighter.c:1175:    default: break;   /* 0x62003 out of scope; state is 0..0xD by construction */
```

`0x62003` is the runtime's error exit (the `mov eax,1; jmp 0x62003` target; see
`menu.h:24-26`, `menu.c:56/264/277/416/433`, `config.c:114/140`,
`actors.c:1253`). It is above `RUNTIME_BASE` (`0x5D000`), so it sits in the
runtime class: removing the credit makes the **raw** count `799 → 798` and
leaves the **portable** population (`759/759`) untouched. Task 4 owns the fix
(a `/* PORT: 0x62003 … */` note must not match the `/* 0xADDR` rule).

#### §C1.4.3 The ten inline credits

Rule: the same credit rule `tools/port_progress.py` applies — the address is in
`symbols.h` (`FN_`), a `/\* 0x([0-9A-Fa-f]{4,6})\b` occurrence exists anywhere
on a line, and there is **no** `fn_register(0xADDR` call and **no** *column-0*
`/* 0xADDR` header. The scan must be case-insensitive: `0x4160c` has a real
lowercase-c column-0 header (`port/src/game/flow.c:523`), so it is **not** an
inline credit — a case-sensitive scan reports eleven.

```text
$ python3 - <<'EOF'      # comment-only credits (no fn_register, no column-0 header)
import glob, re
RUNTIME_BASE = 0x5D000
fns = {int(m,16) for m in re.findall(r'^#define FN_([0-9A-F]{8}) ', open('port/src/symbols.h').read(), re.M)}
files = [f for ext in ('c','h') for f in glob.glob('port/src/**/*.'+ext, recursive=True) if not f.endswith('symbols.h')]
texts = {f: open(f, errors='ignore').read().split('\n') for f in files}
reg = set()
for f in files:
    reg |= {int(m.group(1),16) for m in re.finditer(r'fn_register\(\s*0x([0-9A-Fa-f]+)', open(f, errors='ignore').read())}
rows = []
for a in sorted(fns):
    pat = re.compile(r'0x%05x\b' % a, re.I); hdr0 = False; inline = []
    for f in files:
        for i, ln in enumerate(texts[f], 1):
            if not pat.search(ln): continue
            if re.match(r'/\* 0x[0-9A-Fa-f]{4,6}\b', ln) and int(re.match(r'/\* 0x([0-9A-Fa-f]{4,6})\b', ln).group(1),16) == a:
                hdr0 = True
            if re.search(r'/\* 0x%05x\b' % a, ln, re.I): inline.append((f,i,ln.strip()))
    if (hdr0 or inline or a in reg) and not hdr0 and a not in reg: rows.append((a, inline))
print(len(rows))
for a, occ in rows: print('%05X %-8s %s' % (a, 'portable' if a < RUNTIME_BASE else 'runtime', occ[0][0]+':'+str(occ[0][1])))
EOF
15
12484 portable   port/src/game/flow.c:7972
1B120 portable   port/src/platform/res.c:246
1B544 portable   port/src/game/actors.c:1258
1C350 portable   port/src/game/actors.c:1151
20C10 portable   port/src/game/flow.c:7064
29D60 portable   port/src/game/flow.c:4386
2BF08 portable   port/src/game/flow.c:8033
2EA30 portable   port/src/game/actors.c:1124
2EA64 portable   port/src/game/actors.c:2369
389C4 portable   port/src/game/flow.c:7274
38A38 portable   port/src/game/flow.c:7275
4FBA2 portable   port/src/game/flow.c:6960
500BB portable   port/src/game/config.c:860
61A70 runtime    port/src/game/actors.c:1134
62003 runtime    port/src/game/fighter.c:1175
```

**13 portable comment-only credits**, of which three are the
`UNROWED_HEADER_SHAPE` addresses (`0x1B120`, `0x20C10`, `0x4FBA2`), leaving the
**ten** the design names:

| address | the only credit | site |
|---|---|---|
| `0x12484` | `game_state_3();   /* 0x12484 */` | `flow.c:7972` |
| `0x1B544` | `const u32 *res = res_resolve(handle);   /* 0x1B544 */` | `actors.c:1258` |
| `0x1C350` | `render_list_init();   /* 0x1C350 */` | `actors.c:1151` |
| `0x29D60` | `/* 0x29D60 is a ret-only no-op. */` | `flow.c:4386` |
| `0x2BF08` | `game_overlay_step();   /* 0x2BF08 */` | `flow.c:8033` |
| `0x2EA30` | `/* 0x2EA30(0x2700)/0x2EA30(): … */` | `actors.c:1124` |
| `0x2EA64` | ``(void)flag;   /* 0x2EA64 is a `ret` */`` | `actors.c:2369` |
| `0x389C4` | `render_scroll_edge();   /* 0x389C4 */` | `flow.c:7274` |
| `0x38A38` | `render_scroll_fill();   /* 0x38A38 */` | `flow.c:7275` |
| `0x500BB` | ``#define CFG_TICK_ISR 0x00101500u   /* 0x500BB `mov eax,[0x101500]` */`` | `config.c:860` |

The two runtime comment-only credits are `0x61A70` (`actors.c:1134`, the pset
pool fill) and `0x62003` (§C1.4.2). `0x2EA64` is the bare `ret` the design
names; `0x500BB` is inlined in `string_unlock` (`flow.c:250-257`, the
`DS_00101500` clock shadow) and in `config.c:860`'s `CFG_TICK_ISR` — its credit
is the `#define` comment, not a function.

#### §C1.4.4 The caller-closure reading of the 351 runtime rows

**The rule as measured.** Universe: the 351 unported `FN_` functions at or above
`RUNTIME_BASE` (`0x5D000`), total size **69,098 bytes** from
`port/decomp/prage.functions.csv`. Graph: the `caller,callee` edges of
`port/decomp/prage.calls.csv` restricted to Ghidra functions (rows whose
`entry` has no `::`). For each `r` in the universe, **A(r)** is the transitive
closure of its callers (ancestors), and **E(r) = {a ∈ A(r) : a < 0x5D000}** is
its *game-side* ancestor set — the "exclusive ancestor" reading: `r` is
exclusively reachable only from `E(r)`.

```text
$ python3 - <<'EOF'
import csv, glob, re, collections
RUNTIME_BASE = 0x5D000
fns = {int(m,16) for m in re.findall(r'^#define FN_([0-9A-F]{8}) ', open('port/src/symbols.h').read(), re.M)}
txt = ''.join(open(f, errors='ignore').read() for ext in ('c','h')
              for f in glob.glob('port/src/**/*.'+ext, recursive=True) if not f.endswith('symbols.h'))
ported = {int(m,16) for m in re.findall(r'/\* 0x([0-9A-Fa-f]{4,6})\b', txt)}
ported |= {int(m,16) for m in re.findall(r'fn_register\(\s*0x([0-9A-Fa-f]+)', txt)}
ported &= fns
cls = {int(l.split()[0],16) for l in open('tools/port_classification.txt') if l.strip() and not l.startswith('#')}
funcs = {int(r['entry'],16): int(r['size']) for r in csv.DictReader(open('port/decomp/prage.functions.csv')) if '::' not in r['entry']}
rev = collections.defaultdict(set)
for r in csv.DictReader(open('port/decomp/prage.calls.csv')):
    a, b = int(r['caller'],16), int(r['callee'],16)
    if b != a: rev[b].add(a)
unp = sorted(a for a in fns if a not in ported and a not in cls and a >= RUNTIME_BASE)
def ancestors(x):
    seen, stack = set(), list(rev[x])
    while stack:
        n = stack.pop()
        if n in seen: continue
        seen.add(n); stack.extend(rev[n])
    return seen
E = {a: frozenset(x for x in ancestors(a) if x < RUNTIME_BASE) for a in unp}
g = collections.defaultdict(list)
for a in unp: g[E[a]].append(a)
print("rows", len(unp), "bytes", sum(funcs.get(a,0) for a in unp), "distinct E sets", len(g))
for k, v in sorted(g.items(), key=lambda kv: -len(kv[1]))[:5]:
    print("count=%-3d bytes=%-6d ext=%s" % (len(v), sum(funcs[x] for x in v), sorted('%05X'%x for x in k) or 'EMPTY'))
EOF
rows 351 bytes 69098 distinct E sets 66
count=99 bytes=19742 ext=EMPTY
count=37 bytes=19261 ext=['11000', '11D04', '1BEC4', '1C740', '20C10', '24C5C', '255CC']
count=20 bytes=4889  ext=['1BEC4', '1CF40']
count=19 bytes=4912  ext=['10034', '11000', '11D04', '1BEC4', '1C740', '1CF40', '20C10', '24C5C', '255CC']
count=18 bytes=1417  ext=['1013C', '10C30', '10DB0', '10E18', '10F28', '11000', …]
```

**What reproduces.** `E(r)` equal to the seven-address set
`{0x11000, 0x11D04, 0x1BEC4, 0x1C740, 0x20C10, 0x24C5C, 0x255CC}` is exactly
**37 functions / 19,261 bytes** — the design's "37 movie-only (19,261 bytes)".
The seven are the boot/movie/attract spine: `0x11000` the attract sub-machine,
`0x11D04` the state dispatch, `0x1BEC4` the init chain, `0x1C740` the boot-logos
movie player (`movie.c:81`), `0x20C10` `game_state_init`, `0x24C5C` the per-frame
update and `0x255CC` the master frame loop. The 37 members themselves are
`0x5E000..0x6C881` (from `0x5E000`, `0x5E09B`, `0x5E783`, `0x5F758`, `0x60A90`,
`0x6279C` … to `0x6C760`, `0x6C7B9`, `0x6C851`, `0x6C875`, `0x6C881`). So the
figure reproduces, but the label is the recon's shorthand: the closure is the
*boot/movie/attract* spine's runtime helpers, not the movie player's alone.

**What does not reproduce.** None of the four mechanical rules tried yields **56
functions / 10,824 bytes**:

- transitive `E(r)` equal to any single one of the 66 measured `E` sets: the
  only large readings are the 37/19,261 B and 99/19,742 B groups above;
- transitive `E(r) ⊆ S` for each of the 66 sets (the rule that would admit a
  union of groups): every count falls outside 56 — the readings between 40 and
  79 are 40/19,494 B, 41/19,659 B, 42/20,145 B, 47/19,931 B, 48/19,963 B,
  55/21,385 B, 59/21,585 B, 60/22,525 B, 61/21,632 B, 61/21,677 B, 62/24,654 B,
  63/23,006 B, 63/23,034 B, 65/22,103 B, 69/25,102 B, 71/25,578 B, 77/25,294 B,
  78/25,309 B, 78/29,276 B, 79/26,712 B, 79/29,551 B;
- transitive `E(r) ⊆` the AIL wrapper set (the 15 AIL-class rows of §C1.2.2 plus
  `0x1C930`, `0x1CF20`, `0x2C8F0`, …) gives **0** members: the AIL thunks' own
  ancestors include the frame loop, so no runtime function is reachable *only*
  from them in this graph;
- the **direct**-caller boundary `B(r) = {callers of r that are < 0x5D000}`:
  31 distinct sets; the largest is 308 functions with `B(r) = ∅`; the only
  movie-exclusive reading is **9 functions / 3,484 bytes** whose sole direct
  caller is `0x1C740` (the movie player), and `B(r) ⊆ AIL` gives 0.

These are the rules tried, not an exhaustive search over subsets of the 66 sets.
The design's remainder (258 = 351 − 37 − 56) therefore also does not reproduce;
under the transitive rule the remainder is 314 (351 − 37). Two candidate causes,
both for Task 4 to settle: the recon's root set for the AIL closure was never
recorded, and `prage.calls.csv` carries only Ghidra's direct-call edges (no
indirect calls through the data tables, which the span-writer and reaction
censuses do see) — a richer graph would change both closures. **Task 4 owns the
final set and rule; §C1 records only what these four mechanical rules measure.**

**The earlier 43/76 reading.** The design also records an **earlier recon
reading of 43/76** (design §2 item 2, §7.1 item 4) and states it does not
reproduce, without defining what the two numbers count — no committed source
gives the pair a meaning (a function count, a byte total, or a bank count), so it
cannot be attributed to a set. Against the same closure measurement it does
**not** reproduce either:

- the transitive partition's group sizes are exactly
  `{1, 2, 3, 4, 5, 6, 7, 12, 15, 18, 19, 20, 37, 99}` — no group has 43 or 76
  members;
- the `E(r) ⊆ S` readings skip both: the counts bracketing 43 are 42/20,145 B
  and 47/19,931 B, and 76 is absent (77/25,294 B and 78/25,309 B are the
  neighbours);
- the direct-boundary partition's group sizes are `{308, 9, 2, 1}` — no 43 or
  76.

This record replaces it with **nothing**: the measured 37/19,261 B is the one
reproducible closure and the 56/10,824 B reading is the one named unreproduced;
43/76 is recorded as **unreproducible and undefined**, not as a third measured
set.

### §C1.5 The E2 measurement (plan Step 4, first half)

The committed table is byte-identical to a fresh run (`--check`'s claim), so the
tool's current reading is the committed one:

```text
$ ./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/opencode/e2img
$ python3 tools/entry_triage.py --image /tmp/opencode/e2img \
    --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt --out /tmp/opencode/e2-fresh.md
entry-triage: 579 candidates (575 by U0's rule); finisher=9 move-callback=71 span-writer=231
  call-table=7 anim-target=112 mid-instruction=3 data=75 code-immediate=15 direct=2 interior=6 data-pointer=48
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
$ diff -q /tmp/opencode/e2-fresh.md docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
                                    # no output: identical
```

**227 / 158** is the committed census's split (record
`2026-10-24-proof-completion-derivations.md` §P2.4, AGENTS.md): the E2 table
lists **231 `span-writer` rows**; intersecting them with the **385** writer
slots gives **227 triaged** and **158 untriaged** (385 − 227 = 158). The four
`span-writer` rows that are not writer slots are the renderers whose only slot
is a span table: `0x5215C` (`0x80C8C[4]`), `0x57F80` (`[9]`), `0x57FFB` (`[25]`)
and `0x58CBD` (`[2]`/`[18]`); 231 − 4 = 227.

The filler bug, at `tools/entry_triage.py:40`:

```text
$ sed -n '38,41p' tools/entry_triage.py
# The fillers between functions (§E2.1): nop, `mov eax,eax` (E1 record §E.5, 0x19066),
# `lea eax,[eax+0]`, and zero fill (k7-k12 §1.2, 0x475C0). Order matters: 8D 40 00 before 00.
FILLERS = (b"\x90", b"\x8b\xc0", b"\x8d\x40\x00", b"\x00")
MAX_FILL = 16
```

`87 db` (`xchg ebx,ebx`) is absent from `FILLERS`; it is the 2-byte filler that
follows `ret` before 157 of the 158 untriaged span writers.

**Design recon, to be re-measured in Task 5 — not a Task 1 measurement:** the
design's recon measured the admission as **157 candidates, all `span-writer`,
universe 579 → 736 and U0 575 → 732**, with the 158th writer `0x5D004` above
`RUNTIME_BASE` (design §2 item 3, plan Task 5). This record does **not** add `87
db` and does not re-measure those three numbers; Task 5 owns them.
`tools/entry_triage.py` was not edited.

### §C1.6 The audio bank and rotation measurement (plan Step 4, second half)

The record's measurement (music truth §M4.4 item 7, over the 26-bank
code-reachable set of §M1.6.7) is: the replayed port sections **request 10
distinct banks**, of which **5 are capture-matched** — `s16title+0x36102`
(FD94B4E3), `s16selmo+0x25C` (286ED662), `s16selmo+0x08` (F1774E70),
`s16beach+0x08` (87E7C2DE), `s16jungl+0x08` (BC44EF50) — **5 are requested-only**
(`s16citys+0x08` 20DA829D, `s16himal+0x08` BF1C78E2, `s16caves+0x08` 86946EB7,
`s16sound+0x5B88` 3BA4C376, `s16sound+0x3E64` 3D536DDE: they appear only as named
gaps), and the other **16 banks of the 26** are not requested or attributed in
the measured cut windows. `AUDIO_END` is 1400 (`Makefile:2119`).

The exclusion's code, `tools/audio_compare.py:219-230` (the plan's cited lines):

```python
def documented_excluded(reg, attr):
    """The port deliberately does not reproduce these against a capture.

    `0xBD` unconditionally, and the carrier-TL registers on MIDI channels 1 and
    4 only (the engine's per-channel volume residual, `port/spec/audio.md`).
    """
    lo = reg & 0xFF
    if lo == 0xBD:
        return True
    if lo in (0x43, 0x44, 0x45, 0x4B, 0x4C, 0x4D, 0x53, 0x54, 0x55):
        return attr in (1, 4)
    return False
```

**The measured second-bank write.** `make audio-one scenario=gp-u23-u16-finisher`
produced `audio.txt`; parsing it with the oracle's own `parse_port` and
`documented_excluded` gives, for the `s16selmo+0x25C` (286ED662) section
(anchor = its first key-on, `0xB4=0x2B` at song tick 50, frame 671):

| write | frame | song tick | relative tick | value | attr | `documented_excluded` |
|---|---|---|---|---|---|---|
| 1 | 684 | 75 | **25** | `0x20` | `0x01` | **True (excluded)** |
| 2 | 720 | 148 | 98 | `0x98` | `0x04` | True (excluded) |
| 3 | 834 | 376 | 326 | `0x16` | `0x09` | False (compared) |
| 4 | 865 | 437 | 387 | `0x16` | `0x03` | False (compared) |

```text
$ grep -n ' 0145 ' /tmp/opencode/t1-audio-u23/gp-u23-u16-finisher/audio.txt
172:W 684 75 168 0145 20 01
427:W 720 148 423 0145 98 04
1068:W 834 376 1064 0145 16 09
1274:W 865 437 1270 0145 16 03
1465:W 924 60 1460 0145 98 07      # a later section (song tick restarts at 60)
2121:W 1198 58 2115 0145 16 03     # a later section
```

The last two lines are the same register in the following songs and are not
part of the table. The section's other carrier-TL writes are `W 672 52 128 0143
16 09` (`f=672`), `W 674 56 149 0143 19 09` (`f=674`) and `W 684 75 155 0144 19
02` (`f=684`).

So the register/attr pair the design cites, `0x145=0x20` at the section's
relative tick 25 (frame 684, song tick 75, write index 168), reproduces exactly.
**Plan-vs-raw correction:** the design says the second bank's carrier-TL writes
"carry attrs outside `(1, 4)`, so their carrier-TL writes are not excluded"
(design §2). The measured write's attr is **`0x01`, inside `(1, 4)`**, and the
current predicate **excludes** it; the same register's rel-326 and rel-387
writes carry attrs `0x09`/`0x03` and are **not** excluded. Both readings cannot
hold; the raw is the table above. Task 7 owns the predicate fix and must derive
it from these four rows, not from the design's sentence.

### §C1.7 The capture artefacts (plan Step 5)

The gate's inventory is **34 of 35** paths present with `gp-u14-win-live` the
declared absence (§C1.2). In addition, `data/k11-captures/` holds **four failed
artefacts**, none of them in `CAPTURE_DIRS`, none with a pin, oracle or miss set:

| directory | frames | `S` / `X` / `W` records | `poll.log` |
|---|---|---|---|
| `gp-u14-win-live.failed` | 8928 (`frame_00000`..`frame_08927`) | 9471 / **0** / 0 | present |
| `gp-twop-ko.failed` | 2565 (`frame_00000`..`frame_02564`) | 3087 / 1 / 0 | present |
| `gp-u24-u16-state.failed` | 5443 (`frame_00000`..`frame_05442`) | 5758 / 1 / 1 | present |
| `gp-u24-u16-state.failed.run1` | 8133 (`frame_00000`..`frame_08132`) | 8243 / 1 / 1 | present |

Every one is preserved by name (`.failed` / `.failed.run1`); there is no other
`.failed.N` variant. The frames, `S` and `X` counts above are this task's own
`ls`/`grep -c` measurement and agree with the fought-match record's counts.

#### `gp-u14-win-live.failed` — the one authorized U14 attempt (2026-10-26)

Failing checks (`session.txt`): `port script v2` (key 70 `p1.b3` at `f=69D` was
not queued: the BIOS ring was full), `unscripted input` (2 pad presses, 6 pad
releases, first at `f=71A`), `end frame reached` (no `X` record: mode 0x12 never
opened; the run ended on the 260 s limit) and `stopped at the end`
(`SIGTERM at f=-, rc=0`). The other six checks pass (`steps fired 137/362 (225
skipped)`, `mode 0x27`, `kb == raw`, `frames written`).

**What it still proves** (record §F2.6): the game's own verdict rows, from its
`S` fields — round 1 result (mode 0x08, `f=0x898`) `s0_5a=0x78`, `s1_5a=0x19`,
`w2=0`, `w3=1`, `ad4=0xFFFFFFFF`; match result (mode 0x09, `f=0xF9F`)
`s0_5a=0x78`, `s1_5a=0x5C`, `w2=0`, `w3=2`, `ad4=1`; P1 (`c0=0`) KO'd in both
rounds, P2 (`c1=3` VERTIGO, `afc=3`) won 2-0; `b0c` is 0 in all 9,471 `S`
records. It proves **nothing** on the port side: no port script, so no replay,
dump, compare, miss set or pin exists for it.

#### `gp-twop-ko.failed` — the two-human KO (recovery run, 2026-10-26)

Failing check: `port script v2` (`key 16 (p1.b2)` consumption at `f=596`
unpinned — `S(0x595)` does not exist; two attempts burst-fired at `f=0x593`
after a 23-frame poller gap). The other nine checks pass (`steps fired 107/166
(0 skipped)`, `end frame reached`, `frames written 2565/2565`, `no unscripted
input`, `SIGTERM at f=0CB9`, `X ms=75275 f=0C7D step=107 end`).

**What it still proves** (record §F4.7): a complete **hand-dealt two-human KO**
— `gp_twop.py check --ko` reports `ko f=B91; round result f=B91; match result
f=-; death-done f=-`; the round-result row is `s0_5a=0`, `s1_5a=0x78`, `w2=1`,
`w3=0`, `ad4=0xFFFFFFFF`, `b0c=0`; the join is `f=2AB` (`cred 4 -> 4`) with 2378
`S` records from the join to the `X`; `s1_5a` walks `0x12…0x75` then `0x78`.
It is **not replayable** (no port script), so it carries no oracle, no
`GP_TWOP_KO_*` pin, no `CAPTURE_DIRS` entry and no audio trace. This is the only
surviving two-human artefact: the first authorized run was replaced by this
recovery (record §F4.7).

#### `gp-u24-u16-state.failed.run1` — the first U16 diagnostic (2026-10-26)

Failing check: `port script v2` (`pad change at f=57B unpinned (no S record at
f=57A)`). Nine checks pass (`steps fired 65/75 (0 skipped)`, `end frame
reached`, `frames written 8133/8133`, `no unscripted input`, `pokes written
1/1, 0 raced`, `X ms=165863 f=215D step=65 end`).

**What it still proves** (record §F5.3): a *non-KO* run — round 1 timed out
(mode 7 at `f=0x11A3`, `s1_5a=0x2A`), round 2's 12 real 1A/1B attempts all
whiffed (`s1_5a` stayed at the poked `0x77`), the already-decided match went
mode 9 at `f=0x20A7`, mode 0x17 at `f=0x20AB`, mode 0x12 at `f=0x215D`. It
proves the poke write and the press schedule (all logged at their frames) and
that this draw does **not** reach the reaction; it proves nothing about the
U16 gate operand.

#### `gp-u24-u16-state.failed` — the U16 recovery (2026-10-26)

Failing checks: `port script v2` (`key 16 (p1.b2)` consumption at `f=5CF`
unpinned) and `unscripted input` (2 pad releases at `f=0x745`, which record
§F5.6 shows are the script's own steps 33/34/35 burst-fired; no stray input).
Eight checks pass, including `steps fired 75/75 (0 skipped)`, `frames written
5443/5443`, `pokes written 1/1, 0 raced`, `X ms=122961 f=17A1 step=75 end`.

**What it still proves** (record §F5.6): the KO path *was* reached — round 2's
mode 9 opens at `f=0x1486` with `s1_5a=0x78` (the `0x77` poke plus one real
hit), mode 0x17 at `f=0x16B0`, mode 0x12 at `f=0x17A1`. The exit state at the
old marker's exit frame is measured (`f=0x1533`: `s0_30=0x1180`,
`s0_42=0x8000`, `s0_54=0`, from this task's own `S` row). The **deciding gate
input `y(0x1532)` is not measured**: the `S` records jump `0x1531 → 0x1533`,
so the `s0_54=2` set record is absent. R2 stays a named gap; the four measured
rows and the port's same-frame values are in §F5.6, not restated here.

#### The absent U14 target and its empty pin block

`data/k11-captures/gp-u14-win-live` does not exist (only the `.failed`
artefact); it is the gate's one declared gap (`GATE_NAMED_GAPS`). Its nine pins
are **all empty** in the `Makefile` (`Makefile:1504-1512`):

```text
GP_U14_MIN_FIRST =
GP_U14_TRACE_MIN_FIRST =
GP_U14_MAX_START =
GP_U14_MIN_REACH =
GP_U14_MAX_MISSING =
GP_U14_CAPTURE_SHA256 =
GP_U14_CAPTURE_FRAMES =
GP_U14_MILESTONES =
GP_U14_WIN_MIN_FIRST =
```

`make gp-u14-oracle` therefore takes its absent-directory skip. Task 12 is not
executed this phase (the approval does not name U14); U14 R1 stays a named gap.

### §C1.8 Plan-vs-raw corrections recorded by this task

1. **`TERMINAL_CENSUS`'s home.** The plan's Step 1 lists it among
   `tools/diff_verify.py`'s members; it is in
   `tools/tests/test_diff_verify.py:30829` (a tuple of dicts). `WEAKER` and
   `UNROWED` are in `tools/diff_verify.py:407` and `:417` as the plan says.
2. **The `0x145=0x20` write's attr.** Measured `0x01` (§C1.6), so the current
   `attr in (1, 4)` predicate already excludes it. Design §2's summary sentence
   ("carry attrs outside `(1, 4)`, so their carrier-TL writes are not excluded")
   is imprecise — the measured attrs straddle the rule: `0x01`/`0x04` are inside
   and excluded, `0x09`/`0x03` are outside and compared — and the design's own
   measured paragraph directly after it says exactly that.
3. **The AIL closure.** 56 functions / 10,824 bytes does not reproduce under any
   of the four mechanical rules in §C1.4.4; the 37/19,261 half does, as the
   boot/movie/attract spine's set. Task 4 measures the final set and rule.
4. **The "ten inline credits" need a case-insensitive scan.** With a
   case-sensitive one, `0x4160c`'s real lowercase column-0 header
   (`flow.c:523`) is missed and the count reads eleven.
5. **The 43/76 reading.** The earlier recon pair (design §2 item 2, §7.1 item 4)
   is undefined in every committed source — nothing says whether 43 and 76 count
   functions, bytes or banks — and it does not reproduce under the closure
   measurement (§C1.4.4): no rule tried yields a 43-member or a 76-member set.
   It is recorded as unreproducible and undefined; no replacement number is
   fitted.

### §C1.9 What §C1 does not claim

- **No full gate**: `make verify` was not completed in Task 1; the mid-phase
  gate owns the two-run proof. The `diff-verify` counter above is the
  write-tracking (default) mode, not `PR_DIFF_FULLSCAN=1`.
- **The audio pair is a reproduction of the flake, not determinism and not a
  fix** (Task 2 owns both): both runs fail on `gp-u18-talon-ending` (718 claimed
  ticks vs the pinned 964) and run 1 also fails `gp-u20-chaos-ending`; §C1.3
  records the labels, the anchors and the two failing claims.
- **No closure set or rule is final.** §C1.4.4's 37/19,261 reading is one rule's
  output; the design's 56/10,824, its 258 remainder and its earlier 43/76 pair
  are not reproduced and are not claimed (43/76 is also undefined in every
  committed source).
- **No E2 admission is measured here**; §C1.5 records the tool's current split
  and labels the design's 157/736/732 as recon for Task 5.
- **The failed captures prove only what §C1.7 states** — the U14 one no port
  behaviour at all, the two-human KO a game-side KO with no replay, the U16 ones
  their own rows with the gate operand absent.

---

## §C2 The deterministic sequencer clock (Task 2)

### §C2.1 The determinism check

A new `make audio-determinism` target (Makefile, after `audio-oracle`) runs two
`make audio-one scenario=gp-u18-talon-ending` replays into two dump roots under
the same `AUDIO_DET_DUMP` and `cmp`s the two `.../audio.txt` dumps. The
`audio-one` comparison status is deliberately ignored (under load it fails
before the fix while the dumps are still written); `cmp` is the verdict. It is
a standalone check, not wired into `verify` (the brief's Step 1 — a driver
cannot spawn itself), and is load-agnostic: it must be run under the load the
RED/GREEN pair used or the two dumps are byte-identical on an idle host for the
wrong reason.

### §C2.2 RED: the wall-clock sequencer is non-deterministic under load

On the unmodified tree, `make audio-determinism` under 20 CPU-bound
`python3 -c 'while True: pass'` processes (`taskset -c N`, one per core,
started 10 s before, killed after) **fails** — the two dumps differ:

```text
$ make audio-determinism          # AUDIO_DET_EXIT:2 (make Error 1)
audio-determinism: FAIL (the two audio.txt dumps differ)
1c1
< S 0 cc=00000000 dc=00000000 vol=50 bank=00000000 inits=0 ticks=56
---
> S 0 cc=00000000 dc=00000000 vol=50 bank=00000000 inits=0 ticks=60
4c4
< S 216 cc=00000000 dc=03836102 vol=50 bank=FD94B4E3 inits=1 ticks=488
---
> S 216 cc=00000000 dc=03836102 vol=50 bank=FD94B4E3 inits=1 ticks=496
63c63
< S 631 cc=00000000 dc=0980025C vol=50 bank=286ED662 inits=2 ticks=1326
---
> S 631 cc=00000000 dc=0980025C vol=50 bank=286ED662 inits=2 ticks=1334
...
```

`wc -l`: 3012 vs 3028 lines; `diff` counts **1704 `<` lines** (6 `S` + 1698
`W`). The `S` `ticks` labels shift (56→60, 488→496, 1326→1334 …) and the `W`
frame labels shift, exactly the §C1.3 mechanism (a label, not the music,
moving). Load averages: `17.65, 10.10, 9.13` before, `22.74, 12.64, 10.05`
after.

### §C2.3 Plan-vs-raw correction: the ISR tick breaks the oracle; the frame counter is right

The brief/design prescribe `host_audio_rate() == 0 ? DSD(DS_00101500) :
host_tick_count()` for the headless sequencer clock (the "same source the mixer
uses"). **The raw contradicts this.** Implementing it makes the dump
deterministic (two runs byte-identical) but **breaks the register-stream
oracle**: every trace's `s16selmo+0x25C` section diverges from the capture and
the downstream sections fail to anchor, collapsing the claims to the 30-tick
title section only (e.g. `gp-u18-talon-ending` "FIRST DIFFERENCE … at relative
tick 215: port [\<none\>] vs capture […]"; claimed 30 ticks vs pinned 964).
This is beyond the brief's "re-pin the minimums": the sections would have to be
re-classified from matched to named gaps, gutting the oracle's claim.

**The cause.** `DS_00101500` (the ISR tick) is advanced by the spin (1/frame)
**and** by the res.c read-stall model `res_load_present`
(`port/src/platform/res.c:93`, `game_isr_ticks(bytes / RES_READ_BYTES_PER_TICK)`,
132 674 bytes/tick — a *derived* model whose named gap, record §45-A, is a
drift of up to 4 ticks from the original's wall-time disk reads). Those model
ticks land inside the song windows and shift every later note, while the
capture's sequencer is the original's 60 Hz AIL timer (wall time). The mixer's
use of `DS_00101500` is the *render frame count* (tolerant); the sequencer's
tick count is compared exactly, so the two are not interchangeable.

**The correct clock** is the master-loop **frame counter `DS_000EF6DC`**
(advanced once per frame at `flow.c:7595`, `0x24CDB`), which the res.c
read-stall model does **not** touch: one uniform tick per frame = 2 sequencer
ticks = 120 Hz, matching the capture, with no load drift and no wall time. It
is the deterministic equivalent of the old wall-clock pacing on an idle host.
The section anchor absorbs the pre-section (loader-screen) drift, so the
within-section per-tick multisets match. Measured `S`-line ticks for
`gp-u18-talon-ending` under the frame counter:

```text
S 0 ticks=24   S 216 ticks=456   S 631 ticks=1314   S 759 ticks=1580
S 1037 ticks=2194   S 1188 ticks=2508        # song-start frames are unchanged
```

The only change from the idle wall-clock run is a small uniform shortening
(the wall clock's accumulated ±1 jitter is gone): `gp-u18-talon-ending` claims
**962** (was 964) and `gp-u21-armadon-ending` **1198** (was 1200); every other
trace's claim is unchanged and the full 29-trace oracle is green (twice).

### §C2.4 GREEN: the frame counter is deterministic and faithful

`make audio-determinism` under the same load now **passes** (byte-identical):

```text
$ make audio-determinism          # AUDIO_DET_EXIT:0
audio-determinism: PASS (the two audio.txt dumps are byte-identical)
$ sha256sum /tmp/pr_audio_determinism/run{1,2}/gp-u18-talon-ending/audio.txt
04401d83ef920c3454893d76cc1e3c5382b5874a07b5b60516d262b45ee1f5cc  …/run1/…
04401d83ef920c3454893d76cc1e3c5382b5874a07b5b60516d262b45ee1f5cc  …/run2/…
```

`wc -l` both 3122. `make audio-oracle` twice (`AUDIO_DUMP` per lane) exits 0
both times, `summary ok`, 0 `FAIL`s.

### §C2.5 The re-pins, proved at ±1

Only the two moved minima are re-pinned in `tools/audio_compare.py` `TRACE_PINS`;
each proved by the manual `N`/`N+1` procedure (set N+1, confirm the claim
fails shorter; restore N):

| trace | old pin | new pin (measured) | N+1 fails? |
|---|---|---|---|
| `gp-u18-talon-ending` | 964 | **962** | yes — "claimed 962 ticks, pinned minimum 963" |
| `gp-u21-armadon-ending` | 1200 | **1198** | yes — "claimed 1198 ticks, pinned minimum 1199" |

### §C2.6 The pins that did not move

- **`audio-render` PCM pin**: `make audio-render` still writes the selmo WAV
  `peak 2790 sha256 530a94301f4be46f9a5c666c94516f8a1f7fca6f136d3be0562faed802c47cef`
  (the test's `SELMO_WAV_PEAK`/`SELMO_WAV_SHA256`, unchanged), and the title WAV
  is unchanged at `515b7c94a9d0cfb36afa183f9e6642c04ff8ac0150a1f324182cee6bb8d82f3a`
  peak 4090 (documented; not asserted). The render blocks pace the sequencer
  from rendered audio, not `game_audio_service`, so this is expected — and it
  is confirmed, not re-fitted.
- **Title/attract path**: `make title-oracle` passes (pixel-exact), and the
  9866-write title oracle still reads "oracle C-vs-Python: 9866 writes
  byte-exact" with "capture oracle first difference at C write 430" unchanged.
  `./build/prageport --check 820` still passes (the sequencer does not outrun
  the host clock, because `g_tick` includes the init chain's wall time).

### §C2.7 `audio-render` in `verify`

`verify` now runs `@$(MAKE) --no-print-directory audio-render` after
`audio-oracle` (the PCM determinism pin; a parallel lane overrides
`AUDIO_WAV`/`AUDIO_SONG_WAV`). Added run time ≈ 13 s (the render is a second
`run_tests` pass).
