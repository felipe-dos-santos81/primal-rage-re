# Spec: audio (sub-project 2a — AIL/Miles)

Task 1 investigation: **where the songs and samples physically live**, and the
one answer this cycle hinges on — *is the music a sequenced event stream, or Red
Book/CD audio?*

Every claim below is marked with the source of evidence:

* `verified (cmd: ...)` — the command was run and its output is the basis.
* `likely` — consistent with the evidence but not directly proven here.
* `TODO(verify): ...` — what would settle an open point.

Addresses are LE linear code-object addresses (`base 0x10000`), as elsewhere in
`port/spec`. File offsets are raw offsets into the files under `data/game/C/`,
`/Volumes/RAGECD` (mounted `RAGECD.ISO`) or `PRAGE.EXE`.

---

## Verdict (this is what Task 8's sequencer design depends on)

**The music is a sequenced event stream — XMIDI (Miles AIL). It is not Red
Book / CD audio. The FM path in `2026-09-16-audio-ail-design.md` is correct for
the shipped configuration.**

`verified (cmd: strings.csv + XMI parse + AIL call sites below)`. The decisive
evidence:

1. The shipped driver set selects an **MIDI** driver (`SBPRO2.MDI`) for music and
   a DIG driver for samples — see "MDI.INI / DIG.INI" below.
2. `PRAGE.EXE` contains the literal error strings `"Invalid XMIDI sequence\n"`
   (4 xrefs), `"XMIDI sound hardware not found\n"`, `".MDI driver required\n"`,
   `".DIG driver required\n"`, and a
   `"Warning: you need at least %i bytes more free memory for midi music.\n"`.
   `verified (cmd: port/decomp/prage.strings.csv)`
3. The music payloads are **XMIDI bundles** (`FORM ... XDIR INFO` +
   `CAT ... XMI` + `FORM ... XMID` + `FORM ... TIMB`) embedded in the level
   `.GRA` resources and in `PRAGE.EXE`. `verified (cmd below)`.
4. The AIL init/loader code parses exactly that XMIDI container and installs a
   **timer callback at `0x3c` (60 Hz)** to drive playback.
   `verified (cmd: port/decomp/prage.c FUN_0001cf40 / FUN_0005de48)`.

No Red Book path exists: there is no CD-audio / MSCDEX-track play call or `.CDA`
reference. The only CD string is the drive-presence error
`"%s cannot Initialise CD-Rom Drive.\n"`; the CD is used as a **file source**
(the game loads its `RAGE.S16` resource directory from it), not as an audio
track. `verified (cmd: grep -iE 'cd|red.?book|track|audio' prage.strings.csv)`.

---

## Data locations

### `INDEX` holds no audio

`verified`:

```sh
$ python3 tools/le_info.py --index data/game/C/INDEX | grep -viE "\.gra|name"
data/game/C/INDEX: 1380 bytes, 69 entries
```

The only line not matching `.gra` is the tool's own header; all 69 entries end
`*.gra`. `verified (cmd: le_info.py output above; entry count 69 / 69 .gra)`.
So audio is read **from the directory**, not through the resource manager.

### `S16*.GRA` are chunk containers; several carry a music bank

`S16SND2.GRA` and `S16RAD.GRA` (the two the design flagged as suspicious):

```sh
$ python3 tools/gra_chunks.py data/game/C/S16SND2.GRA data/game/C/S16RAD.GRA
== S16SND2.GRA (177851 bytes)
   [off 0x0] type=2 magic=b'43' next=0x0 body=177843 bytes
== S16RAD.GRA (18256 bytes)
   [off 0x0]    type=2 magic=b'43' next=0x4574 body=17772 bytes
   [off 0x4574] type=6 magic=b'43' next=0x0   body=468 bytes
```

`S16RAD.GRA` is a normal image set (type-2 pixels + type-6 descriptor table, 39
frames) — **not** audio. `S16SND2.GRA` is a single type-2 body.

But `S16SND2.GRA` is **not** an image set: it has no type-6 descriptor table
(`gra_render.py` refuses it), and its bytes are identical across all three
graphics variants:

```sh
$ md5 data/game/C/S16SND2.GRA /Volumes/RAGECD/RAGE.S08/S08SND2.GRA /Volumes/RAGECD/RAGE.S04/S04SND2.GRA
7bf181035f2d04814ab6880821193eb8  S16SND2.GRA
7bf181035f2d04814ab6880821193eb8  S08SND2.GRA
7bf181035f2d04814ab6880821193eb8  S04SND2.GRA
```

Graphics are palette-depth specific (S04/S08/S16 differ); `SND2` is
byte-identical across all three, so it is **payload data, not graphics**.
`verified (cmd: md5 above)`.

Inside `S16SND2.GRA` is an XMIDI bundle:

```sh
$ python3 - <<'EOF'
d=open('data/game/C/S16SND2.GRA','rb').read()
i=d.find(b'FORM'); print(hex(i), d[i:i+36])
EOF
0x21bfe b'FORM\x00\x00\x00\x0eXDIRINFO\x00\x00\x00\x02\x01\x00CAT \x00\x00\t6XMIDFORM...'
```

`FORM` len `0xE` type `XDIR`/`INFO`, then `CAT ` len `0x936` type `XMID`, then a
`FORM` len `0x92A` type `XMID`, then `TIMB`. That is the Miles **XMIDI bank**
container. `verified (cmd above + FORM scan below)`.

The same XMI bundle appears, at these file offsets, in the level and sound
resources:

```sh
$ python3 - <<'EOF'   # find FORM/XDIR/CAT/XMID in every C/*.GRA
...
EOF
S16BEACH.GRA  size=233128  FORM=12     XDIR=20     CAT=34     XMID=42
S16CAVES.GRA  size=293132  FORM=12     XDIR=20     CAT=34     XMID=42
S16CITYS.GRA  size=266532  FORM=12     XDIR=20     CAT=34     XMID=42
S16CONTI.GRA  size=45320   FORM=12     XDIR=20     CAT=34     XMID=42
S16DIASD.GRA  size=115658  FORM=-      XDIR=-      CAT=-      XMID=50486
S16GRAVE.GRA  size=237316  FORM=12     XDIR=20     CAT=34     XMID=42
S16HGHSC.GRA  size=663928  FORM=12     XDIR=20     CAT=34     XMID=42
S16HIMAL.GRA  size=184508  FORM=12     XDIR=20     CAT=34     XMID=42
S16JUNGL.GRA  size=279124  FORM=12     XDIR=20     CAT=34     XMID=42
S16KONSD.GRA  size=118520  FORM=117940 XDIR=117948 CAT=117962 XMID=117970
S16SELMO.GRA  size=973560  FORM=12     XDIR=20     CAT=34     XMID=42
S16SND2.GRA   size=177851  FORM=138238 XDIR=138246 CAT=138260 XMID=138268
S16SOUND.GRA  size=586942  FORM=12     XDIR=20     CAT=34     XMID=42
S16STONE.GRA  size=207492  FORM=12     XDIR=20     CAT=34     XMID=42
S16TITLE.GRA  size=1505988 FORM=221446 XDIR=221454 CAT=221468 XMID=221476
```

`verified (cmd: FORM/XDIR/XMID scan above)`. This is per-level music: each
arena GRA (`BEACH`, `CAVES`, `CITYS`, `JUNGL`, `HIMAL`, `STONE`, `HGHSC`,
`CONTI`, `GRAVE`, …) bundles its own XMIDI bank, plus a title bank in
`S16TITLE.GRA`, a select-screen bank in `S16SELMO.GRA`, and global banks in
`S16SOUND.GRA` / `S16SND2.GRA`. `S16SOUND.GRA` contains **nine** `XDIR`
directories (multiple sequence banks); most level GRAs contain one.
`verified (cmd: XDIR count scan)`.

**Container layout (level GRAs), `verified` for `S16BEACH`/`S16SOUND`:** the
type-2 body is `[u32 music_bank_size][XMIDI bank][graphics RLE...]`, i.e. the
first 4 body bytes (`file offset 8`) equal the XMI bank size that immediately
follows at offset 12.

```sh
$ python3 -c "import struct; d=open('data/game/C/S16BEACH.GRA','rb').read(); \
print('off8 u32 =',hex(struct.unpack_from('<I',d,8)[0])); \
print('CAT@38 len =',hex(struct.unpack_from('>I',d,38)[0]))"
off8 u32 = 0x4fb0
CAT@38 len = 0x4f92     # 0x4f92 + 0x1e = 0x4fb0
```

`S16SND2.GRA` and `S16TITLE.GRA` instead place their XMI bank **later** in the
body (at `0x21bfe` / `0x3618e`), not at offset 12.
`TODO(verify): the exact split of the SND2/TITLE type-2 body (sample/pixel data
vs XMI bank) — disassemble the consumer that indexes past the bank.`

### An XMI bank is also inside `PRAGE.EXE`

```sh
$ python3 - <<'EOF'
import re; d=open('data/game/C/PRAGE.EXE','rb').read()
for m in (b'FORM',b'CAT ',b'XMID'): print(m,[hex(x.start()) for x in re.finditer(m,d)][:6])
EOF
b'FORM' ['0xc87c4']
b'CAT ' ['0xc87cc']
b'XMID' ['0xc87d4', '0xc8904', '0xc89f4']
```

`verified (cmd above)`. So the **data object** also carries an XMI bank
(`0xC87C4`, one `FORM`/`CAT`/`XMID` bundle). This is the third location the
design's gating note hypothesised.

### Samples

PCM sample data is present as a **RIFF/WAVE** blob inside `S16SOUND.GRA`:

```sh
$ python3 - <<'EOF'
import struct
d=open('data/game/C/S16SOUND.GRA','rb').read(); i=d.find(b'RIFF')
print('RIFF@',hex(i),d[i:i+12])
fmt=struct.unpack_from('<HHIIHH',d,i+20)
print('audioformat=%d channels=%d rate=%d byterate=%d align=%d bits=%d'%fmt)
print('data size',struct.unpack_from('<I',d,i+40)[0])
EOF
RIFF@ 0x24b13 b'RIFF' ... b'WAVE'
audioformat=1 channels=1 rate=11025 byterate=11025 align=1 bits=8
data size 19327
```

`verified (cmd above)`: an 8-bit **unsigned mono PCM** WAV at **11025 Hz**, one
sample, 19327 bytes. The rate matches the AIL preference the game sets at init:
`FUN_0005d87e(1,0x2b11)` (`0x2b11 == 11025`) in `FUN_0001cf40`.
`verified (cmd: prage.c FUN_0001cf40)`.

A full scan of every installed file found `WAVE` in only two places:
`S16SOUND.GRA` (the sample above) and `SETSOUND.EXE` (the installer's own
preview strings). `verified (cmd: WAVE/RIFF/VOC scan over data/game/C/*)`.

`TODO(verify): the game's in-play sound-effect samples. Only one RIFF/WAVE blob
was found by magic; other sample voices are likely stored as raw AIL sample
blocks (no RIFF header) inside the level GRAs / `S16SND2.GRA`. Settle by
disassembling the AIL sample-load wrapper (`FUN_0005de48` family) and reading the
length/rate the game passes, then diffing the pointed-to bytes.`

### `RAGE.SND` on the CD is a driver directory — no audio payload

`RAGE.SND` on the disc is a **directory**, not a file:

```sh
$ stat -f '%N %HT' /Volumes/RAGECD/RAGE.SND
/Volumes/RAGECD/RAGE.SND Directory
```

It contains the AIL driver set (`ADLIB.MDI`, `OPL3.MDI`, `SBPRO2.MDI`, `SB16.DIG`,
`FAT.OPL`, `FAT.AD`, …) plus per-language sub-directories
(`DEUTSCH ENGLISH ESPANOL FRANCAIS ITALIANO PORTUGUE`), each holding only the
**installer's** sound-config tooling (`SETSOUND.EXE`, `AILDRVR.LST`, `CMOS`).
`verified (cmd: ISO9660 walk of RAGE.SND + iso walk of its language subdirs)`.

The CD root likewise holds only graphics variants plus the flattened driver
set: `RAGE.S04/` (4-bit graphics), `RAGE.S08/` (8-bit), `RAGE.S16/` (16-bit) and
`RAGE.SND/`. `verified (cmd: own ISO9660 root walk + stat -f over /Volumes/RAGECD/*)`.

**No music or sample payload lives in `RAGE.SND`.** The design's "music inside
the CD's `RAGE.SND`" hypothesis is **disproven**.

### What the game actually opens

The music/sample path is driven through the **Miles AIL API surface**, which the
game calls directly (it links the driver binaries, not a separate sound engine):

* `FUN_00010034` (called from `main`) initialises the DIG path and falls back
  `SB16.DIG → SBPRO.DIG → SBLASTER.DIG`: `verified (cmd: prage.c lines 23-35,
  string xrefs 0x1006d/0x10084/0x1009b)`.
* `FUN_0001cf40` is the AIL init: it sets preferences
  (`FUN_0005d87e(4,4)`, `(1,0x2b11)`, `(3,0x14)`, `(0xb,1)`), allocates **4**
  sample handles (`0x60 == 4 * 0x18` loop), allocates a sequence handle
  (`FUN_0005ddd0`/`FUN_0005de1f`), and installs a timer callback with period
  `0x3c` = **60 Hz** (`FUN_0005da12`/`FUN_0005da87(h,0x3c)`/`FUN_0005daa6`).
  `verified (cmd: prage.c FUN_0001cf40)`.
  * This is the likely sequencer pacing answer for Task 8: **60 Hz**.
    `TODO(verify): confirm `FUN_0005da87`'s second argument is a Hz period (vs ms)
    by reading the AIL timer wrapper at `0x5da87`.`
* `FUN_0005de48` → `FUN_0006a410` (878 B) is the **XMIDI loader**: it parses the
  `FORM`/`CAT`/`XMID` container, walks the `XMID` records, and on failure
  raises `"Invalid XMIDI sequence\n"`. `verified (cmd: prage.c 0x5de48 /
  0x6a410; strings.csv xrefs 0x6a450/0x6a54c/0x6a44b/0x6a545)`.
* `FUN_0005de1f` → `FUN_0006a3a0` allocates a sequence handle (raises
  `"Out of sequence handles\n"`). `verified (cmd: prage.c 0x6a3a0)`.
* MDI driver selection strings live at `0x6a35b` (`"MDI.INI"`) and the AIL
  banner string lists `"General MIDI OPL-2- OPL-3-based ..."`.
  `verified (cmd: prage.strings.csv)`.

The resource handle format (`(index << 23) | byte_offset`) resolves the GRA
resources by name; the game builds the `\RAGE.S16` path string
(`xrefs 0x10c98/0x10d25`) to find the S16 resource directory (installed
`data/game/C`, or the CD's `RAGE.S16/`).
`verified (cmd: prage.strings.csv + FORMATS.md resource-handle section)`.

### `MDI.INI` / `DIG.INI` (the shipped profile)

```sh
$ xxd data/game/C/DIG.INI   # DEVICE "Creative Labs Sound Blaster 16 or AWE32"
                            # DRIVER SB16.DIG
$ xxd data/game/C/MDI.INI   # DEVICE "Creative Labs Sound Blaster(TM) 16"
                            # DRIVER SBPRO2.MDI
```

`verified (cmd: xxd above)`. Both are `Miles Design Audio Interface Library
V3.02 of 18-Jan-95` config files. Music driver = **SBPRO2.MDI** (Sound Blaster
Pro 2 → OPL FM), sample driver = **SB16.DIG** (16-bit DMA).
`FAT.OPL` / `FAT.AD` are the `(value, offset)` patch banks for the FM drivers
(`verified (cmd: xxd -l 128 of FAT.OPL/FAT.AD — pairs stepping 0x0E)`).

---

## Consequence for the design document

* Task 8's **FM sequencer is the right design** — the music is a sequenced
  XMIDI event stream played through an `.MDI` FM/OPL driver. The spec needs
  **no revision** on this point.
* The design's "gating discovery" hypotheses resolve as: music is **inside the
  GRAs** (`S16SOUND`, `S16SND2`, `S16TITLE`, `S16SELMO`, and every arena GRA)
  **and** in the **data object** (`PRAGE.EXE @ 0xC87C4`) — **not** in
  `RAGE.SND`.
* The design's Task 7 (samples) should read WAV/PCM from the GRA resources;
  `S16SOUND.GRA` proves the format (8-bit unsigned mono 11025 Hz) and the rate
  matches the AIL preference.
* Task 8's pacing discovery is answered (likely **60 Hz**); confirm the timer
  argument semantics as noted.

## Open items

| Item | Status |
|---|---|
| Music is sequenced XMIDI, not Red Book | **verified** |
| XMI bank locations (GRA + EXE) | **verified** |
| `RAGE.SND` holds no payload | **verified** |
| WAV sample format/rate in `S16SOUND.GRA` | **verified** |
| Exact `S16SND2` / `S16TITLE` type-2 body split | `TODO(verify)` |
| Other (raw, headerless) sound-effect sample locations | `TODO(verify)` |
| AIL timer `0x3c` == 60 Hz (vs ms) | `likely`, `TODO(verify)` |

---

## OPL register-trace capture spike (Task 2)

**Verdict: the original's OPL register stream was NOT captured, so Task 9's
byte-exact Python fallback still governs the acceptance bar. The earlier
headline "NOT ACHIEVABLE" is withdrawn — the mechanism is not missing; the
capture is.** `verified (cmd: routes 1–3 below; DOSBox-X version 2026.08.31,
Homebrew/macOS)`.

What the three routes establish, and what they do not:

* Routes 1 (`[capture]`) and 2 (`-opencaptures`) have **no register-level
  surface** — unchanged from the first pass.
* Route 3 — the debugger's CPU instruction trace — **does** exist and **is**
  register-level for I/O: a trace line `out dx,al` carries the port in `EDX` and
  the value in `AL`. It **can** capture OPL writes, proven end to end on a
  synthetic guest probe (route 3c). The first pass's 0-byte `LOGCPU.TXT` was an
  **invocation artifact** — `LOG` with no count argument logs zero instructions
  — not a capability limit.
* What is missing is an **operational** capture of the *original's* music.
  Across **53,477,376 traced instructions** (17 windows) of the running game,
  with active VGA retrace polling and palette writes, there were **zero** writes
  to `0x388`/`0x389` or the SB FM mirrors `0x220`/`0x221`; the only `out dx,al`
  port seen was the VGA DAC port `0x3C9` (route 3d).

### Route 1 — the `[capture]` config section

The brief's command returns nothing:

```sh
$ dosbox-x -defaultconf -printconf 2>/dev/null | sed -n '/\[capture\]/,/^\[/p'
                       # (no output)
```

`-printconf` prints the config file's **path**, not its contents, so that
pipeline can never match. Resolving the file directly:

```sh
$ dosbox-x -defaultconf -printconf
/Users/<user>/Library/Preferences/DOSBox-X 2026.08.31 Preferences
```

That file has **no `[capture]` section at all**, and neither does
`dosbox-x.reference.full.conf` nor `dosbox-x.reference.conf`
(`grep -c '^\[capture\]'` → `0` in both). The only capture facility is the
`[dosbox]` key `captures = capture`, documented in the reference config as
*"Directory where things like **wave, midi, screenshot** get captured"*.

**Result: covers waveform / MIDI / screenshot output only — no OPL register
logging.**

### Route 2 — `-opencaptures`

```sh
$ dosbox-x --help 2>&1 | grep -A2 -i opencaptures
  -opencaptures <param>                   Launch captures
  -opensaves <param>                      Launch saves
```

The man page (`man 1 dosbox-x`) is explicit:

> **`-opencaptures`** *program* — Calls program with as first parameter the
> location of the captures folder and exit.

Verified by handing it a harmless program:

```sh
$ SDL_VIDEODRIVER=dummy dosbox-x -defaultconf -opencaptures /bin/echo
... (DOSBox-X startup log) ...
./capture
```

**Result: it hands the captures *folder path* to an external program and exits.
No register-level logging.**

### Route 3 — the debugger's CPU instruction trace

**3a. Piped / scripted session (non-TTY): refused.** The debugger does not
engage and DOS continues to boot. Exact log line:

```sh
$ printf 'HELP\nQUIT\n' | dosbox-x -defaultconf -break-start -noconsole
...
LOG: Debugger in Mac OS X not available unless you start DOSBox-X from terminal or from Terminal application
...
```

**3b. Under a pseudo-TTY it engages and is scriptable.** Driven with `expect` on
a pty, `HELP` is processed and `QUIT` exits the emulator (so the Task 13 note
"refuses a scriptable session" holds only for a non-TTY pipe). The command table
contains **no I/O-port logging and no I/O-port breakpoint**; the only logging is
a **CPU instruction** trace:

* `LOG [num]`, `LOGS`/`LOGL`/`LOGC [num]` — write a CPU log to `LOGCPU.TXT`
  (`num` = instruction count; `src/debug/debug.cpp:3050` `logcode`).
* `HEAVYLOG` — ring-buffer CPU log (`LOGCPU_INT_CD.TXT`) written on DOSBox-X exit.
* `IN[P|W|D] [port]` / `OUT[P|W|D] [port] [data]` — one-off manual port access.
* Breakpoints: `BP`, `BPINT`, `BPM`, `BPLM` — none port-based.

Two invocation facts explain the first pass's **0-byte** `LOGCPU.TXT` and are
required to drive the trace at all:

1. `LOG` with **no count logs nothing**: `cpuLogCounter` becomes `0` and the
   file is opened and immediately closed with zero instructions
   (`debug.cpp:3050-3066`, `6581-6593`).
2. A log started from the **reset vector is truncated after 3 instructions**:
   BIOS POST calls `DEBUG_StopLog()` (`debug.cpp:6622`). Observed directly —
   `LOG 200000` under `-break-start` produced a 747-byte `LOGCPU.TXT` holding
   exactly the 3 instructions before POST's stop message. To trace past POST,
   break in mid-run first: `BPINT 21` + `RUN` (reaches the first DOS call,
   i.e. after POST), then `BPDEL *` and `LOG <num>`.

**3c. The trace IS register-level for OPL ports — proven with a probe.** A
41-byte `.COM` guest (`OPLPROBE.COM`: `mov dx,0388h`/`0389h` + `out dx,al`) was
traced under the mechanism:

```sh
$ dosbox-x -defaultconf -fastlaunch -nopromptfolder -break-start \
    -c "MOUNT C <probe-dir>" -c "C:" -c "OPLPROBE.COM" -c "EXIT"
# expect on a pty:  BPINT 21 ; RUN ; BPDEL * ; LOG 80000
$ grep -E 'out +dx,al' LOGCPU.TXT | grep -E 'EDX:0000038(8|9)'
0814:00000105  out  dx,al   ... EDX:00000388 ... EAX:00000020
0814:0000010B  out  dx,al   ... EDX:00000389 ... EAX:00000001
0814:00000111  out  dx,al   ... EDX:00000388 ... EAX:00000040
0814:00000117  out  dx,al   ... EDX:00000389 ... EAX:00000010
0814:0000011D  out  dx,al   ... EDX:00000388 ... EAX:00000060
0814:00000123  out  dx,al   ... EDX:00000389 ... EAX:000000F0
```

All **6/6** writes were recovered, in order, with the exact register and value
(`AL`). So the mechanism is a viable register-level capture: ordered
`(tick, register, value)` with `tick` from the trace's own instruction order.

**3d. But no OPL writes from the *original* in bounded sweeps.** Same mechanism,
game instead of probe:

```sh
$ dosbox-x -defaultconf -fastlaunch -nopromptfolder -break-start \
    -c "MOUNT C <data/game/C>" \
    -c "IMGMOUNT D <data/game/CD/RAGECD.ISO> -t iso" -c "C:" -c "PRAGE.EXE -f"
# expect on a pty:  BPINT 21 ; RUN ; BPDEL * ; 17 × LOG 300000
```

17 windows × `0x300000` = **53,477,376 traced instructions**: `opl_hits = 0` in
every window (no `out dx,al` to `0x388`/`0x389`/`0x220`/`0x221`). The only
`out dx,al` port present was `0x3C9` (VGA DAC palette). VGA retrace polls
(`in al,dx`, `EDX=0x3DA`) ranged 0–20,110 per window, so the guest was executing
and rendering throughout. A separate earlier sweep with the emulator's own
`[log]` at all-debug also produced no `0x388`/`0x389`. `verified (cmd above)`.

**3e. Why this still is not an oracle for this cycle.** The trace capability is
real, but no music-playing window was captured, so there is no original stream
to diff against. Two explanations remain open and are **not** resolved here:
(a) the original had not yet entered its FM-note playback in the traced windows;
(b) the shipped configuration's playback does not drive the OPL ports. Settling
this is Task 8/9 work (e.g. anchor a trace on the AIL 60 Hz tick).
The emulator's own OPL device code has its register-write logging commented out
(`src/hardware/adlib.cpp:1049`), so no emulator-level route exists.

No `tools/opl_trace.py` is committed: the brief conditions it on an end-to-end
capture of the *original*, which was not obtained. The parser rule above
(`out dx,al` → port `EDX`, value `AL`, ordered) is sufficient to build it the
moment a music window is captured.

### Consequence

The primary oracle — a captured original OPL register stream — was **not
obtained** in this cycle, so Task 9's byte-exact Python fallback governs the
acceptance bar. This is now a **scoped** claim: the debugger instruction-trace
mechanism exists and can capture OPL register writes (3c), but no window of the
original's music playback was captured (3d), so the route is not proven
unavailable — only unproven-in-budget. No `tools/opl_trace.py` is written.
