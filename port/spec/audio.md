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

**Verdict: YES — the original's OPL register stream was captured. The route
exists and works, so the captured stream can now govern the Task 9 comparison
instead of the byte-exact Python fallback being the only bar.**
`verified (cmd: DX-CAPTURE /O capture + tools/opl_trace.py below; DOSBox-X
2026.08.31, Homebrew/macOS)`.

`TODO(verify): whether this bounded run's capture spans the title/attract music
that Task 8/9 must diff per-tick — the bounded run proves the original drives
OPL FM at register level, not which tracks it covered. A title/attract-targeted
re-capture (longer `-time-limit`, or anchored on the AIL 60 Hz tick) would
settle it.` **Task 9 resolved: yes.** The C sequencer's title bank halts (meta
`FF 2F`) at tick 3434, which under the 120 Hz note alignment of "Music tick
rate" is 28117 ms after the first note-on — equal to the capture's last write
at 28117 ms, so `prage_000.dro` spans the title/attract bank end to end.
`verified (cmd: ./build/run_tests + tools/opl_trace.py --info)`.

### The route that works: `DX-CAPTURE /O`

DOSBox-X's internal shell command `DX-CAPTURE` takes `/O` = "OPL FM (DROv2
format)": it starts raw OPL capture, runs the program, and finalises a
**DBRAWOPL `.dro`** file when the program exits.

```sh
$ strings "$(command -v dosbox-x)" | grep -i 'DROv2 format'
/O for OPL FM (DROv2 format) and /-D disabling post-exit delay.
```
`verified (cmd above; DOSBox-X src/shell/shell_cmds.cpp:4513 CMD_DXCAPTURE →
CAPTURE_StartOPL())`.

The recorder (`Adlib::Capture`, `src/hardware/adlib.cpp:806`) arms on
`CAPTURE_StartOPL()` but **only opens the file at the guest's first FM note-on**
(`Capture::DoWrite`, `adlib.cpp:996`: a write to reg `0xB0`–`0xB8` with bit
`0x20`, or percussion `0xBD`), then dumps the register cache and every
subsequent FM register change as ordered `(raw,value)` pairs with millisecond
delays. A gap over 30 s (`adlib.cpp:976`) ends the file; a later note-on starts
the next one. `verified (cmd: source read; reproduced by the probe below)`.

**Exact capture command** (repo root; `.dro` output is git-ignored):

```sh
G=$PWD/data/game
dosbox-x -defaultconf -fastlaunch -nopromptfolder -nogui -nomenu -time-limit 120 \
  -set "sdl fullscreen=false" -set "dosbox captures=/tmp/gamecap" \
  -c "MOUNT C $G/C" \
  -c "IMGMOUNT D $G/CD/RAGECD.ISO -t iso" \
  -c "C:" -c "DX-CAPTURE /O PRAGE.EXE -f" -c "EXIT"
```
`-time-limit` bounds the run; the game need not exit, because the `.dro` is
created (with a placeholder header) at the first note-on and finalised on
emulator shutdown. `verified`.

### Result: the original emits a full FM stream

```
prage_000.dro  7201 FM register writes  span 28117 ms  349 + 320 (2nd set) key-ons
prage_001.dro  1052 FM register writes  span  9433 ms   36 +  40 (2nd set) key-ons
```
`verified (cmd: tools/opl_trace.py --info <file>)`. The stream opens with the
AIL/MDI driver's cached dual-OPL2 state (the `.dro` header reports hardware type
`1` = dual-OPL2, second register set present): `0x01=0x20` waveform-select
enable, `0x105=0x01` (the OPL3-mode enable bit), second-set `0x120`/`0x121`/…
configuration, then operator/channel setup and key-ons on `0xB0`–`0xB8` and
`0x1B0`–`0x1B8`. That is the shipped `SBPRO2.MDI` FM path playing — captured,
not inferred.

Sample (`tools/opl_trace.py <file>`, columns `tick_ms reg value`; `tick_ms` is
accumulated from the capture's own delay commands, never wall-clock):

```
       0 0x0001 0x20
       0 0x0105 0x01
       0 0x0120 0x01
       0 0x0021 0x01
       0 0x0121 0x01
       0 0x0022 0x01
```

The `.dro` files themselves are **not committed** (git-ignored); only this
procedure and `tools/opl_trace.py` are. Traces are kept locally under
`data/audio-captures/` for comparison. `verified`.

### `tools/opl_trace.py`

Decodes DBRAWOPL v2 into a normalised `(tick_ms, register, value)` stream. It
rebuilds the raw-code→register table the same way the recorder does
(`Capture::MakeTables`, `adlib.cpp:834`), maps bit `0x80` to the second
register set (`0x100+`), and decodes both delay forms (`delay256`,
`delayShift8`). `--self-test` round-trips a synthetic file.
`verified (cmd: python3 tools/opl_trace.py --self-test)`.

A synthetic 16-bit `.COM` writing six OPL registers and one key-on, captured
through the same command, produced a `.dro` from which `opl_trace.py` recovered
all eight `(reg,value)` writes in order, exactly as written. `verified`.

The three routes below are the first pass's search for a register-level surface.
Routes 1–2 have no register-level surface; route 3 (the debugger instruction
trace) **is** register-level and was proven on a probe, but it is not what
produced the capture — the `DX-CAPTURE /O` route above did. They are kept here
as the record of what was tried.

* Routes 1 (`[capture]`) and 2 (`-opencaptures`) have **no register-level
  surface** — unchanged from the first pass.
* Route 3 — the debugger's CPU instruction trace — **does** exist and **is**
  register-level for I/O: a trace line `out dx,al` carries the port in `EDX` and
  the value in `AL`. It **can** capture OPL writes, proven end to end on a
  synthetic guest probe (route 3c). The first pass's 0-byte `LOGCPU.TXT` was an
  **invocation artifact** — `LOG` with no count argument logs zero instructions
  — not a capability limit.
* The first pass's operational sweep of the *original* saw **zero** OPL writes
  (`0x388`/`0x389` or the SB mirrors `0x220`/`0x221`) across **53,477,376 traced
  instructions** (17 windows), which 3e now explains as a window artifact.

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

**3d. First pass: no OPL writes seen from the *original* in a bounded sweep.**
(Window artifact — see 3e.) Same mechanism,
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

**3e. The first pass's "no OPL writes" was a window artifact.** The
17×`0x300000`-instruction sweep (3d) covered only the game's load/startup phase,
before FM playback; the raw capture arms at the **first FM note-on** and runs
until shutdown. Under the **same `-defaultconf` configuration** the sweep used,
`DX-CAPTURE /O` yields 7201 OPL register writes (8,253 across the two files
above). So the game's audio init **did** succeed and the driver **does** drive
OPL — this is neither an environmental Sound Blaster failure nor a driver that
avoids FM. The earlier "route unproven/unavailable" conclusion is withdrawn.
`likely (inferred: same config now produces the stream; capture begins at first
note-on)`.

No Sound Blaster misconfiguration explains 3d either: both runs used the
DOSBox-X defaults `sbtype=sb16`, `sbbase=220`, `irq=7`, `dma=1`,
`oplmode=auto` (whose capture reports dual-OPL2). `verified (cmd: grep '^sbtype' on the stored conf)`.

The **mapper route is genuinely unavailable in this build** — but it was never
the only route. The `caprawopl` mapper handler is commented out
(`src/hardware/adlib.cpp:1748`
`//MAPPER_AddHandler(OPL_SaveRawEvent,…, "caprawopl", "Cap OPL", …)`), so there
is no editable-mapper-file hotkey to bind. The working, non-interactive route is
the `DX-CAPTURE /O` shell command above, which needs no mapper.
`verified (cmd: source read + strings)`.

`tools/opl_trace.py` is now committed (see above) and the original's stream was
obtained, so nothing is deferred here.

### Consequence

The primary oracle — the original's OPL register stream — **was obtained**. The
Task 9 comparison now anchors on `tools/opl_trace.py` output from a `.dro`
captured with `DX-CAPTURE /O`; the byte-exact Python fallback is no longer the
sole acceptance bar.

---

## AIL surface (Task 3) — every call the game makes into the audio layer

**What this section is.** The call list Task 10 implements: each game→audio entry
point, its observed signature at the call sites, the behaviour to reproduce, its
reachability and a confidence mark.

**Evidence method (static only; no DOSBox-X).** Three passes, all reproducible:

1. `port/decomp/prage.calls.csv` filtered to `caller < 0x5d000` and
   `0x5d7dc <= callee < 0x5e000` — the 36 callees the game makes into the
   sound-library block (`awk -F','` on the csv; the same set by name below).
2. Each callee's body in `port/decomp/prage.c`, plus its internal callee
   (`FUN_00065b43`–`FUN_0006aca0`) — the behaviour column is read from the code
   that runs, not inferred from the name.
3. Callers/edges traced through `calls.csv` to classify reachability, and two
   hand-disassembled callbacks (`capstone`, LE pages resolved via `le_info.py`:
   object 0 `0x10000→file 0x62e54`, object 1 `0x80000→file 0xc6e54`) because
   Ghidra did not emit them as functions.

### Where the game stops and the audio layer begins (the boundary)

```
game code  (obj0, 0x10000 … ~0x5d000  and 0x63xxx)          <- "the game"
   │  calls
   ▼
AIL public-API thunks  0x5d851 … 0x5dfff                    <- the AIL surface
   │  36 callees in the 0x5d7dc–0x5dfff sound-library block: 33 AIL + 3 non-AIL (see below)
   ▼
AIL engine (statically linked)  0x65b43 … 0x6aca0           <- vendored, internal
   │  driver-call dispatcher 0x5d973(driver, fn#, in, out)
   ▼
external driver, loaded at runtime from RAGE.SND\*.DIG / *.MDI (low memory)
```

* The boundary the later tasks work against is **the thunk block
  `0x5d7dc–0x5dfff`**: everything at or below it is the game; every call that
  crosses it is listed below. The AIL engine and the loaded `.DIG`/`.MDI`
  binaries are internal — reachable only through the calls below — and are not
  part of the surface.
* `0x5d973` (`FUN_0005d973`, 4 args: driver, fn#, in, out) is the driver
  dispatcher; the engine calls it with driver API numbers **`0x300, 0x301,
  0x302, 0x303, 0x304, 0x305, 0x306, 0x401, 0x402, 0x501, 0x502, 0x503`**
  (`FUN_00065b7b`/`FUN_0006a410`/`FUN_00068070`/`FUN_0006a7c0` …). It is *not*
  game-called. It uses DPMI (`swi 0x31`) into the loaded driver.
  `verified (cmd: prage.c 0x66903 body + call sites; calls.csv)`
* Corrections to earlier text in this file, explicitly:
  * **`0x5D7DC` is not an audio entry point.** `FUN_0005d7dc` is the Watcom C
    runtime `rand()`: a 32-bit LCG (`seed = seed*0xB90D12B9 + 0x38CE051F;
    return (seed>>16)*(arg&0xffff)>>16`), 0 callees, called with no arguments by
    33 game functions (`0x10f28` spawn delay, `0x121a0` title, 0x4xxxx in-play,
    …). No AIL code calls it. `verified (cmd: prage.c 0x5d7dc body + all 33
    call sites in calls.csv; all callers < 0x5d000)`.
  * **The 60 Hz AIL timer drives the *game* tick, not the sequencer directly.**
    See "Timer rate" below.
  * `0x5d808` and `0x5d812` are also **not** AIL — game helpers that happen to
    sit in the block (see table).

### Game → AIL call list

`FUN_0005dXXX` names are the decompiler's; the AIL name is the closest AIL 3.02
API function and is marked in the confidence column. `called as` is the literal
argument expression at the call site. Reachability: **init** = game main
`FUN_0001bec4`/`main`; **movie** = Smacker/animation path
`0x11000`/`0x24c5c → 0x1c740 → 0x6345c → 0x63180`; **title** = state 1
`FUN_000121a0`; **in-play** = `FUN_0002c3fc` (206 callers) and the master loop
`0x255cc → FUN_0001cf20`; **teardown** = `FUN_0001d018`/`0x1be30`.

**Names are inferred labels; behaviour is what is verified — do not read a name
as evidence.** The AIL names in the third column are the closest Miles AIL 3.02
matches and are *inferred*. The `verified` marks cover the address, the call-site
signature/arguments (`called as`) and the behaviour to reproduce, **not** the
name. A row can therefore carry a `verified` behaviour and still have a
`likely`/`TODO(verify)` name: affected rows include **1–2, 14, 19, 21–24**. Plan
tokens: name `likely` = closest AIL match, unconfirmed; name `TODO(verify): …` =
no name assigned, and the stated check would settle it.

| # | address | AIL name (conf.) | called as (site) | behaviour to reproduce | reach | conf |
|---|---|---|---|---|---|---|
| 1 | `0x5d851` | `AIL_startup` (likely) | `FUN_0005d851()` — `FUN_0001cf40` | installs the AIL timer hook then loads the 18 default preferences via `FUN_0006603e` (pref 0..0x11 = 200,1,0x8000,100,0x10,100,0x28f,0,0,1,0x78,8,0x7f,1,0,2,1,1). No args, no return used. | init | verified body |
| 2 | `0x5d86a` | `AIL_shutdown` (likely) | `FUN_0005d86a()` — `FUN_0001d018` | releases every installed driver (`FUN_0005db61` each), releases all timers, unhooks. No return used. | teardown | verified body |
| 3 | `0x5d87e` | `AIL_set_preference` (verified) | `(7,1)` `FUN_00010034`; `(4,4)` `(1,0x2b11)` `(3,0x14)` `(0xb,1)` `FUN_0001cf40` | swaps `prefs[pref]=value`, returns the old value. Game sets sample rate **0x2b11 = 11025**, pref 3=20, pref 4=4, pref 0xb=1, pref 7=1. | movie, init | verified (body: `DAT_00108d64[pref]`) |
| 4 | `0x5da12` | `AIL_register_timer` (verified) | `(&LAB_00010604)` `FUN_00010610`; `(&LAB_0001bdf4)` `FUN_0001cf40` | allocates a timer slot, stores the callback, returns its handle (byte offset `0,4,…0x3c`) or `-1` ("Out of timer handles"). | init | verified body |
| 5 | `0x5da87` | `AIL_set_timer_frequency` (verified) | `(h,0xfa)` `FUN_00010610`; `(h,0x3c)` `FUN_0001cf40` | converts Hz→µs (`1000000/hz`) and stores the period; `0xfa`=**250 Hz**, `0x3c`=**60 Hz**. | init | verified (body `FUN_00066bb8` = `1000000/param_2`) |
| 6 | `0x5daa6` | `AIL_start_timer` (verified) | `(DAT_000f0a20)` and `(uVar1)` | marks the timer running (state 1→2). | init | verified body |
| 7 | `0x5dadc` | `AIL_release_timer_handle` (verified) | `(DAT_000f0a20)` `FUN_00010684` | clears the timer slot (state→0). Called on the refcount-0 shutdown. | teardown | verified body |
| 8 | `0x5db7c` | `AIL_install_DIG_INI` (verified) | `FUN_0005db7c()` — `FUN_0001cf40` | opens `DIG.INI`, parses `DRIVER=`/`IO_ADDR`/`DMA_*`, installs the named `.DIG`; returns the DIG driver handle (stored `DAT_001028c8`). | init | verified (body reads `"DIG.INI"`) |
| 9 | `0x5db9e` | `AIL_install_DIG_driver_file` (verified) | `(pcVar1,0)` / `("SBPRO.DIG",0)` / `("SBLASTER.DIG",0)` — `FUN_00010034` | installs the named driver file; `0` on failure ("Driver file not found"). Used as the `SB16.DIG → SBPRO.DIG → SBLASTER.DIG` fallback chain (handle `DAT_00081e08`). | movie | verified (body + 3 literal names) |
| 10 | `0x5dbcb` | `AIL_allocate_sample_handle` (verified) | `(DAT_00081e08)` `FUN_0001013c`; `(DAT_001028c8)` ×4 `FUN_0001cf40` | finds a free sample slot on the driver, inits it, returns the handle (`0` + "Out of sample handles" when full). Init allocates exactly **4** sample handles (loop `iVar2+0x18` until `0x60`). | movie, init | verified body |
| 11 | `0x5dbf4` | `AIL_release_sample_handle` (verified) | `(*(param_1+0x23c))` `FUN_00010570` | marks the sample free (state=1). | movie | verified body |
| 12 | `0x5dc0f` | `AIL_init_sample` (verified) | sample arg, 7 sites (`0x1013c`,`0x1cb18`,`0x1cc28`,`0x1cd9c`,`0x1ce04`,`0x1cf40`,`0x1d220`) | resets the sample (state=2, position/loop/volume defaults, **default rate `0x2b11`=11025**). | all | verified body |
| 13 | `0x5dc2a` | `AIL_set_sample_address` (verified) | `(handle, buf, len)` `FUN_0001cb18` | sets `sample.addr=buf`, `sample.len=*buf` (XMI block), resets position fields. | in-play/title | verified body |
| 14 | `0x5dc4d` | `AIL_set_sample_type` (likely) | `(h,uVar3,cVar1!='\0')` `FUN_0001013c`; `(h,0,0)` `FUN_0001cb18` | writes `sample+0x34=fmt`, `sample+0x38=flag` and re-commits; `fmt` is the 0..3 code derived from the record's two flags. Exact field names open. | movie, in-play | behaviour verified; name likely (inferred) |
| 15 | `0x5dc70` | `AIL_start_sample` (verified) | `(handle)` `FUN_0001cb18` | marks the sample playing (state=4) and issues driver call `0x401` (DMA start). | in-play/title | verified body |
| 16 | `0x5dc8b` | `AIL_stop_sample` (verified) | `(handle)`, 7 sites (`0x10510`,`0x10570`,`0x1cc28`,`0x1cd9c`,`0x1ce04`,`0x1d018`,`0x1d220`) | marks the sample stopped (state=2) and fires its registered callbacks. | movie, teardown | verified body |
| 17 | `0x5dca6` | `AIL_set_sample_rate` (verified) | `(h,*(param_1+0x27c))` `FUN_0001013c`; `(h,0x2b11)` `FUN_0001cb18` | sets `sample.rate` (+0x3c). Init/streaming set 11025. | movie, in-play | verified body |
| 18 | `0x5dcc5` | `AIL_set_sample_volume` (verified) | `(h,0x7f)` `FUN_0001013c`; `(h,DAT_000a2cb4)` `FUN_0001cb18`/`FUN_0001ced4` | sets `sample.volume` (+0x40), clamped 0..`0x7f`; `DAT_000a2cb4` is the game volume global. | movie, in-play | verified body |
| 19 | `0x5dce4` | `AIL_set_sample_loop_count` (likely) | `(handle,0)` `FUN_0001cb18` | writes `sample+0x30` (loop count; init default 1, game forces 0 = no loop). | in-play/title | behaviour verified; name likely (inferred) |
| 20 | `0x5dd03` | `AIL_sample_status` (verified) | `(handle)` 6 sites | returns `sample+4`: `2`=stopped, `4`=playing (the game gates on `!= 4` and `== 4`). | movie, in-play | verified body |
| 21 | `0x5dd2c` | sample buffer-size helper (no AIL name) | `(DAT_00081e08,*(param_1+0x27c),uVar3)` `FUN_0001013c` | computes the byte size of a streaming buffer from format/rate/len; result `(n+3)&~3` is allocated. Likely game/AIL streaming glue. | movie | behaviour verified; name TODO(verify) |
| 22 | `0x5dd5d` | streaming buffer index | `(puVar1[0x8f])` in `FUN_000102b8` | returns which of the two streaming buffers needs refilling (`0`/`1`, or `-1` when idle). | movie | behaviour verified; name TODO(verify) |
| 23 | `0x5dd86` | feed streaming buffer | `(handle,uVar2,buf,len)` `FUN_000102b8` | installs `(buf,len)` into streaming half `uVar2`, resets its consumed count, issues driver call `0x401`. | movie | behaviour verified; name TODO(verify) |
| 24 | `0x5ddad` | `AIL_register_sample_callback` (likely) | `(handle,0,param_1)` `FUN_0001013c` | stores a function pointer into the sample's callback table (`+0x854`, indexed); `FUN_00068070`/`FUN_000680f0` invoke `+0x84c`/`+0x850` on start/stop. | movie | behaviour verified; name likely (inferred) |
| 25 | `0x5ddd0` | `AIL_install_MDI_INI` (verified) | `FUN_0005ddd0()` — `FUN_0001cf40` | opens `MDI.INI`, installs the named `.MDI`; returns the MDI driver handle (`DAT_001028c4`). | init | verified (body reads `"MDI.INI"`) |
| 26 | `0x5de1f` | `AIL_allocate_sequence_handle` (verified) | `(DAT_001028c4)` — `FUN_0001cf40` | allocates the single sequence handle from the MDI driver (`DAT_001028c0`; `0` + "Out of sequence handles"). | init | verified body |
| 27 | `0x5de48` | `AIL_init_sequence` (verified) | `(DAT_001028c0,DAT_001028d0,0)` ×2 `FUN_0001c930` | parses the `FORM/CAT/XMID` bank, walks the XMID records, sets up banks/tempo; `0` + "Invalid XMIDI sequence" on bad data. **This is the music load.** | title, in-play | verified body |
| 28 | `0x5de79` | `AIL_start_sequence` (verified) | `(DAT_001028c0)` — `FUN_0001c930` | silences all channels, resets the track to start, marks playing (state=4). **This is the music start.** | title, in-play | verified body |
| 29 | `0x5deaf` | `AIL_stop_sequence` (verified) | `(DAT_001028c0)` 5 game call sites (`0x1c930` ×2, `0x1ca6c`, `0x1d018`, `0x1d1b0`; 6th site `0x69b62` is engine-internal) | sends all-notes-off (cc `0xb0/0x40`), marks stopped (state=2). **This is the music stop.** | all | verified body |
| 30 | `0x5deca` | `AIL_set_sequence_volume` (verified) | `(DAT_001028c0,DAT_000a2cb8,500)` | sets the sequence target volume and a fade time in ms (500 ms here; computes a per-tick delta). | title, in-play | verified body |
| 31 | `0x5deed` | `AIL_sequence_status` (verified) | `(DAT_001028c0,param_2,param_3)` `prage.c:8381` (`FUN_0001ca40`); `(DAT_001028c0)` `prage.c:8424` (`FUN_0001cab8`); `(DAT_001028c0)` `prage.c:8745` (`FUN_0001d018`) — all 3 call sites (exhaustive) | returns `sequence+4`; the game treats `4` as *playing*. | in-play, teardown | verified body |
| 32 | `0x5dfdc` | no-op (purpose unknown) | `FUN_0005dfdc()` — `FUN_00010034`, `FUN_00010610` | empty body (prologue/epilogue only, no stack). Runs once when the sound-driver refcount goes 0→1. Behaviour to reproduce: **nothing**. | init/first use | verified (disasm: only `push/mov/pop/ret`) |
| 33 | `0x5dfeb` | no-op (purpose unknown) | `FUN_0005dfeb()` — `FUN_000100c4`, `FUN_00010684` | empty body. Runs once when the refcount goes 1→0. Behaviour: **nothing**. | teardown | verified (disasm) |
| — | `0x5d7dc` | **not AIL**: Watcom `rand()` | 0-arg, 33 game sites | 32-bit LCG returning `(seed>>16)*(arg&0xffff)>>16`. Not audio; must exist for game determinism. | all | verified |
| — | `0x5d808` | **not AIL**: game helper | `FUN_0005d808()` — master loop `0x20c10` | `word[0x6f6de]=0` — resets the counter the 60 Hz callback increments. | in-play | verified (disasm) |
| — | `0x5d812` | **not AIL**: stub | `0x292ac` (calls.csv; not rendered in `prage.c`) | `return 0`. | in-play | verified (disasm) |

### Internal-only (reached only through the calls above)

Game code never calls these; they are listed so no later task mistakes them for
the surface. `FUN_0005d8ab`/`FUN_0005d8bf` (lock/unlock around all engine entry
points; `FUN_00066264`/`FUN_00066271` refcount), `FUN_0005d8d3`, `FUN_0005d8fc`,
`FUN_0005d91b`/`0x5d936`/`0x5d958`/`0x5d9a8` (driver
enable/query), `FUN_0005d9c3`/`0x5d9e5`/`0x5da3b`/`0x5da68` (INI parse, timer
period), `FUN_0005daa6` is surface but `FUN_0005dac3` (stop) is internal,
`FUN_0005daf7` (release all timers), `FUN_0005db0b`/`0x5db34`/`0x5db61`
(driver get/install/uninstall internals), `FUN_0005ddf2` (install MDI from file),
`FUN_0005de94` (all-notes-off reset), `FUN_0005df16`/`0x5df35`/`0x5df5e`/`0x5df7d`
(sequence event helpers, note stealing, callback registration), and the whole
engine body `FUN_00065b43 … FUN_0006aca0`.
`verified (cmd: prage.calls.csv — every caller of these is >= 0x5d000)`.

### Timer rate (Step 3)

**Answer: the game installs two AIL timers, 60 Hz and 250 Hz; neither is proven
to be the XMIDI sequencer tick. The sequencer is advanced by AIL's per-driver
timer, whose rate is declared by the loaded `.MDI` driver, not by `PRAGE.EXE`.**

What is verified statically:

* The AIL timer subsystem stores an independent **period in µs** per timer and
  reprograms PIT channel 0 to the soonest due timer:
  `FUN_0005da87(hz)` → `FUN_0005da68(1000000/hz)` → `DAT_000efc40[t] = period`;
  `FUN_000663d4(us)` writes `out 0x43,0x36; out 0x40,lo; out 0x40,hi` with
  `divisor = us*10000/0x20bc` (≈ `us*1.1932`). So a 60 Hz timer fires every
  `1000000/60 = 16666 µs` (PIT divisor ≈ 19887) exactly.
  `verified (cmd: prage.c 0x66bb8/0x66b88/0x663d4/0x66407)`.
* The game's `FUN_0001cf40` (called from game main `FUN_0001bec4`) sets the
  **60 Hz** timer; its callback `LAB_0001bdf4` (hand-disassembled) does
  `DAT_00081508++; DAT_00081500++; FUN_0001bbac(); word[0x6f6de]++; FUN_0002d62c()`
  — i.e. it calls **`0x2D62C` = the game's 60 Hz tick** (`DAT_00105D88++`,
  per `game_flow.md`). The 60 Hz value is the **game frame/music-service tick**.
  `verified (cmd: prage.c 0x1cf40; capstone @0x1bdf4; game_flow.md "Tick")`.
* `main` also calls `FUN_00010610`, which sets a **250 Hz** timer; its callback
  `LAB_00010604` is `mov eax,[0x81e10]; inc [0x81e10]; ret` — a free-running
  4 ms counter (`DAT_00081e10`) used as the **streaming-audio timeline**
  (`FUN_000100dc`/`FUN_000102b8`/`FUN_00010678` multiply it by 4 µs).
  `verified (cmd: prage.c 0x10610; capstone @0x10604)`.
* The XMIDI sequencer is advanced by AIL's **per-driver** timer: `FUN_00065b7b`
  (the engine's generic driver install) registers a timer with callback
  `FUN_000656e2` and sets its frequency to `*(s16*)(driver+0x2e)`, a field the
  **driver itself writes during its init** (driver call `0x300`, issued just
  above). That rate lives in `SBPRO2.MDI`, not in `PRAGE.EXE`.
  `verified (cmd: prage.c 0x65b7b lines "FUN_0005da12(&DAT_000656e0)" /
  "FUN_0005da87(timer, *(s16*)(driver+0x2e))")`.
* AIL also keeps an 18.2 Hz housekeeping timer: `FUN_0006646c` calls
  `FUN_0005da68(0x3c, 0xd68d)` = 54925 µs.
  `verified (cmd: prage.c 0x6646c)`.

So the earlier "60 Hz to drive playback" phrasing is **correct for the game tick
and the music-service timer, not proven for the sequencer's OPL write cadence**.

`TODO(verify): the MDI driver's declared timer rate (and therefore the
sequencer's OPL write cadence) — Task 8/9 settle it from the capture: bucket the
`.dro` writes by inter-write delay from `tools/opl_trace.py` and test whether
writes land on 1/60 s (16666 µs) boundaries (⇒60 Hz) or on another period; a
driver rate ≠ 60 Hz also shows as the game-timer callbacks (0x2d62c /
DAT_00081e10) and the FM writes advancing at different multiples. If the
capture cannot separate them, read the rate the loaded `SBPRO2.MDI` writes to
driver offset +0x2e at its `0x300` init.`

### Notes for the implementing tasks

* **Init chain Task 4 wires (music-relevant):** `FUN_0001cf40` with `param_1 != 0,
  param_2 != 0` → `AIL_startup`; prefs `(4,4)(1,11025)(3,20)(0xb,1)`;
  `AIL_install_DIG_INI` → 4× `AIL_allocate_sample_handle` + `AIL_init_sample`;
  `AIL_install_MDI_INI` → `AIL_allocate_sequence_handle`; `AIL_register_timer`
  with the 60 Hz callback. Then per song `AIL_init_sequence` +
  `AIL_set_sequence_volume` + `AIL_start_sequence` (from `FUN_0001c930`).
* **Sample play Task 5/6 prove:** the shortest path is
  `FUN_0001cf20` (master loop) → for each slot `FUN_0001cb18` → `AIL_init_sample`
  → `AIL_set_sample_address` → `AIL_set_sample_type` → `AIL_set_sample_volume`
  → `AIL_set_sample_rate(11025)` → `AIL_set_sample_loop_count(0)` →
  `AIL_start_sample`.
* `DAT_000a2cb4` is the master SFX volume; `DAT_000a2cb8` the master music
  volume (both fed to `AIL_set_sample_volume` / `AIL_set_sequence_volume`).
  `verified (cmd: prage.c 0x1ced4 / 0x1cab8)`.
* Nothing in the surface is deferred except the internal-only engine body and
  the `.MDI` driver rate above.

### Task-3 open items

| Item | Status |
|---|---|
| Game→AIL surface = 33 AIL + 3 non-AIL callees in `0x5d7dc–0x5dfff` | **verified** |
| `0x5d7dc` is `rand()`, not audio | **verified** |
| Game music/frame AIL timer = 60 Hz | **verified** |
| Streaming AIL timer = 250 Hz (4 ms counter) | **verified** |
| Sequencer tick = MDI-driver timer (offset +0x2e), rate not in EXE | **verified**; value `TODO(verify)` |
| AIL names for rows 14/19/24 and rows 21–23 | **likely** / `TODO(verify)` by AIL 3.02 header match |

---

## Music event grammar (Task 8)

The XMIDI `EVNT` stream that the sequencer decodes. Read from the bytes and the
decompilation (`FUN_00068750` = the VLQ reader, `FUN_00069372` = the per-tick
event loop, `FUN_0006a410` = the container parser in `port/decomp/prage.c`), and
confirmed against the capture.

* **Delta times are single bytes** (`< 0x80`) that precede the event group they
  apply to. A delta of `0x00` is **omitted**: the next byte is a status byte
  directly, which is why `EVNT` starts with `FF 58 …`. `FUN_00069372`'s event
  loop breaks when the byte at the read pointer is `< 0x80` and then reads that
  one byte into the tick countdown (`[0xb] = *ptr`), so no multi-byte delta and
  no running status occur. `verified (cmd: prage.c FUN_00069372 0x6960a/0x698c0;
  FUN_00068750)`.
* Status bytes (channel events carry a 4-bit channel; channel 9 is percussion):

  | status | bytes | meaning |
  |---|---|---|
  | `0x8n` | note, vel | note off |
  | `0x9n` | note, vel, **VLQ duration** | note on; `vel == 0` = note off. The trailing duration is a Miles extension (note length in ticks); the engine stores it (`[0x185]`) and releases when it reaches 0. |
  | `0xAn` / `0xEn` | two data bytes | aftertouch / pitch bend (decoded as parameter pairs) |
  | `0xBn` | controller, value | ctrl 0 = bank select (`B0 00 00` in every song header), 7 volume, 10 pan, 91/93 effects, 64 sustain |
  | `0xCn` | program | program change |
  | `0xDn` | value | channel pressure |
  | `0xFF` | type, VLQ length, data | meta: `0x51` tempo (3 bytes, µs/quarter), `0x58` time signature, `0x2F` XMIDI loop/end; `0xF0`/`0xF7` are sysex + VLQ length |

  `verified (cmd: prage.c FUN_00069372 branches; EVNT bytes below)`.

* **Patch key.** A program change selects a FAT.OPL entry by
  `(bank << 8) | program`; bank comes from controller 0. MIDI channel 9 is
  percussion and selects `0x7F00 | note`. `verified` against the capture (the
  first four notes are channel 9 and their patches are the `0x7F` drum bank
  entries).

Reproduce the parse (from the repo root; prints the first note-ons and their
durations):

```sh
python3 - <<'EOF'
import struct
d=open('data/game/C/S16TITLE.GRA','rb').read()
i=221480; size=int.from_bytes(d[i+4:i+8],'big')
assert d[i:i+12]==b'FORM\x00\x00\x13\x0eXMID' and size==0x130e
p=i+12; ev=None
while p+8<=i+8+size:
    cid=d[p:p+4]; sz=int.from_bytes(d[p+4:p+8],'big')
    if cid==b'EVNT': ev=d[p+8:p+8+sz]
    p+=8+sz+(sz&1)
q=0; tick=0; n=0
while q<len(ev) and n<6:
    b=ev[q]
    if b<0x80: tick+=b; q+=1; continue
    q+=1
    if (b&0xf0)==0x90:
        note,vel=ev[q],ev[q+1]; q+=2
        dur=0
        while ev[q]&0x80: dur=(dur<<7)|(ev[q]&0x7f); q+=1
        dur=(dur<<7)|ev[q]; q+=1
        if vel: print('tick=%d note=%d vel=%d dur=%d'%(tick,note,vel,dur)); n+=1
    elif b&0xf0 in (0x80,0xa0,0xb0,0xe0): q+=2
    elif b&0xf0 in (0xc0,0xd0): q+=1
    elif b==0xff:
        q+=1; ln=0
        while ev[q]&0x80: ln=(ln<<7)|(ev[q]&0x7f); q+=1
        ln=(ln<<7)|ev[q]; q+=1+ln
    else: break
EOF
```

## Music tick rate (Task 8)

**Answer: one XMIDI delta tick is 8.333 ms (120 Hz).** This is the driver's OPL
write cadence in the shipped configuration, measured from the capture rather
than inferred from the EXE (the spec's earlier `TODO(verify)`).

Evidence — align `S16TITLE.GRA`'s first note-ons against the captured key-on
times in `data/audio-captures/prage_000.dro`:

```
note#  tick  note  cap_ms  pred_ms  err
  0     59   47       0      0.0    0.0
  1     67   45      64     66.7   -2.7
  2     74   43     124    125.0   -1.0
  3     82   36     192    191.7    0.3
  4    254   84    1624   1625.0   -1.0
  5    551   79    4100   4100.0    0.0
  6    568   74    4240   4241.7   -1.7
  7    578   72    4324   4325.0   -1.0
  8    594   68    4456   4458.3   -2.3
  9    733   43    5616   5616.7   -0.7
 16    753   61    5784   5783.3    0.7
```

`pred_ms = (tick - 59) * 1000 / 120`. The first 17 notes fit within **3 ms over
5.6 s**; fitting the same points at 60 Hz gives errors growing past 1 s and at
250 Hz the residuals scatter. `verified (cmd: tools/opl_trace.py --json on
prage_000.dro + the scan above)`. The engine's own event timing agrees: its
tempo accumulator (`FUN_00069372`, `[0x15] += [0x11]`, `[0x11] = 100`) advances
one musical tick per driver service with no scaling, so the delta unit is the
service period. The AIL default-preference table the game installs contains
`0x78 = 120` (spec row 1), consistent with the measured rate.
`TODO(verify): the value the loaded SBPRO2.MDI writes to driver offset +0x2e
during driver call 0x300 (FUN_00065b7b) — the behavioural 120 Hz is measured,
that field is not.`

The port keeps this as `SEQ_TICK_MS = 1000/120` (`sequencer.h`).
`TODO(verify): the capture's note-on alignment drifts by ~0.5 s after tick 594;
that is an un-modelled loop/branch (meta `FF 2F` / `RBRN`), not a rate change —
left to Task 9.`

## FAT.OPL patch bank (Task 8)

Container and payload layout are in `FORMATS.md` ("`FAT.OPL` / `FAT.AD`"). The
payload decode is **verified** against the capture:

* `[3..7]` → modulator registers `0x20, 0x40, 0x60, 0x80, 0xE0` and `[9..13]` →
  carrier, with `[8]` → `0xC0`. For program `0x7A` the capture writes
  modulator `0x20=0x0E, 0x40=0x00, 0x60=0xF6, 0x80=0x00` and carrier
  `0x20=0xC0, 0x60=0x1F, 0x80=0x02, 0xE0=0x03`, `0xC0=0x3E`; the payload
  (`0e 00 00 0e 00 f6 00 00 0e c0 00 1f 02 03`) matches except the two driver
  transforms below. `verified (cmd: prage_000.dro + FAT.OPL)`.
* The driver ORs `0x30` into `0xC0` (the OPL3 left/right output bits):
  `0x0E -> 0x3E`, `0x04 -> 0x34`. `verified (cmd: capture)`.
* The driver attenuates the **carrier TL** (`[10]`) by note velocity. The
  attenuation is added to the raw payload byte (KSL bits included), so it can
  carry into bits 6-7: patch `0x49`, `[10] = 0x00`, is written `0x16` at
  velocity 127, `0x17` at 113 and `0x18` at 104. That is a dominant
  `+ 0x16 + ((127 - velocity) >> 3)` term. `verified (cmd: prage_000.dro +
  FAT.OPL)`.
  `TODO(verify): the exact velocity→TL function.` The captured offset is **not**
  a pure function of velocity: patch `0x34` (`[10] = 0x83`, KSL 2) is written
  `0x9a` at velocity 127 (offset 23) and patch `0x74` (`[10] = 0x03`) is written
  `0x18` at velocities 116-127 (offset 21), while the `[10] = 0x00` / `0x40`
  patches sit at offset 22. Cases checked (velocity → written carrier TL):
  `0x49` 127/122→`0x16`, 113→`0x17`, 104→`0x18`; `0x1e`/`0x58` 127→`0x16`;
  `0x24` 120/127→`0x56`; `0x34` 127→`0x9a`; `0x74` 115→`0x19`, 126/127→`0x18`.
  The title capture and `FAT.OPL` alone do not pin the per-patch residual;
  **what would settle it**: disassemble `SBPRO2.MDI`'s velocity→TL path, or
  capture a velocity sweep for one program. The port writes `[10]` verbatim.
* `[0] = 0x0E` and `[1] = 0x00` are constant across all 181 entries; `[2]` is
  the percussion base note for the `0x7F` bank (`likely`).

## Known capture divergences (Task 9)

Places where the port's register stream deliberately differs from
`data/audio-captures/prage_000.dro`. Task 9 must either match each one or
explicitly exclude it; none is silent. Entries marked *withdrawn* no longer
differ — they are kept as the correction record for the earlier claim.

1. **Carrier TL velocity attenuation omitted.** The driver adds a velocity term
   to the carrier TL (`[10]`) — dominant form `+ 0x16 + ((127 - velocity) >> 3)`,
   with an unexplained ±1 per-patch residual (see "FAT.OPL patch bank" above).
   The port writes `[10]` verbatim, so its carrier TL is up to ~0x16 brighter
   than the capture. `TODO(verify): the exact function` (disassemble
   `SBPRO2.MDI`, or a single-program velocity sweep).

2. **`0x105 = 0x01` (OPL3-mode enable): withdrawn, port now matches.** The
   capture's next write after `0x01 = 0x20` is `0x105 = 0x01`. Earlier text here
   claimed writing it to the vendored opal core **silences** the output. That is
   an artifact of the probe behind it omitting the driver's `0xC0 = patch | 0x30`
   output-enable write. In OPL2 mode `channelMix` forces every channel's enable
   on, masking a missing `0xC0`; in OPL3 mode the `0xC0` bits gate the mix, so
   the `0xC0`-less probe rendered 0. With the `0xC0` enable present — as every
   real note setup has — `0x105 = 0x01` is output-neutral and byte-identical.
   The port now writes `0x105 = 0x01` after `0x01 = 0x20`, matching the capture;
   the core is unmodified. `verified (cmd: ./build/run_tests covers
   port/tests/test_opl.c; a 1024-frame key-on probe against
   build/libprage_core.a measures 1003/1003 non-zero samples with and without
   the write when 0xC0 is written, memcmp 0; without 0xC0 the old probe's
   1003/0 reproduces)`.

3. **Sequencer parser and XMIDI running status.** The reviewer flagged possible
   running status / `0x80` note-off / `0x9n vel 0` in the non-title banks. No
   such encoding is present: every `EVNT` chunk in every shipped `S16*.GRA`
   musical bank (26 chunks, `S16TITLE`, `S16SND2`, `S16SOUND`, `S16SELMO`, the
   arena GRAs, …) parses to its exact end under the port's single-byte-delta,
   status-per-event, mandatory-`0x9n`-duration grammar, yielding zero note-ons
   with velocity > 127 and zero `0x80`/`0x9n vel 0` events. A running-status
   parse of the same bytes produces invalid velocities (e.g. `S16CONTI` at byte
   offset 448 yields velocity 149), so running status is not the correct
   interpretation. `verified (cmd: parse of every EVNT chunk; see the offset 448
   case)`. No parser change is warranted on this evidence.

4. **Driver cached-state init block omitted.** After `0x01 = 0x20` and
   `0x105 = 0x01` the capture writes a full reset sweep (`0x20..0x35` and
   `0x120..0x135`, values `0x01`/`0x3F`/`0xFF`/`0x0F`) before its first key-on.
   The port opens with only the two enable writes. The comparison excludes this
   by normalising the capture from its first key-on and dropping the port's
   tick-0 writes. `verified (cmd: ./build/run_tests capture-oracle line)`.
5. **Per-note patch re-application and channel reuse.** The port re-applies the
   whole 14-byte patch on every key-on and allocates the lowest free voice; the
   driver applies operators when a patch is selected and schedules channels on
   its own state. First difference after the item-4 normalisation is the port's
   first note operator write (tick 60, `0x20 = 0x00`) against the capture's
   first key-on (tick 60, `0xB0 = 0x2B`): the capture's first note still uses
   the init-block operator state. The port emits 9340 writes, the capture 6380
   after normalisation. Not matchable without reproducing the driver's
   channel/patch state machine. `verified (cmd: ./build/run_tests, "capture
   oracle first difference")`.
6. **Percussion note frequency.** The port maps MIDI percussion through the
   melodic `NOTE_TAB`; the capture's first drum note (MIDI 47) is block 2 fnum
   `0x3CF` while the melodic table gives `0x28B`. `TODO(verify): the driver's
   percussion note -> fnum mapping.` `verified (cmd: capture A0/B0 vs
   sequencer.c NOTE_TAB)`.
7. **OPL rhythm register `0xBD` not written.** The capture's only `0xBD` write
   is its tick-0 init value `0xC0` (rhythm-mode enable, all percussion off),
   which normalisation drops with the init block (item 4). The port never
   writes `0xBD` — it routes percussion through the melodic voice pool (items
   5-6) — so `documented_excluded` also excludes `0xBD` defensively in case a
   later capture writes it after its first key-on. `verified (cmd:
   tools/opl_trace.py data/audio-captures/prage_000.dro | awk
   '$2=="0x00bd"' -> one write, tick 0, value 0xC0)`.

Task 9 result: `tools/opl_seq.py` and the C sequencer agree **byte-for-byte**
(9340 writes: tick, register, value and order) — the tolerance-free governing
oracle comparison. Against the capture the port is **not** byte-exact for items
1 and 4-7; each is excluded or reported, never tuned away.

**Asset gate on the governing comparison.** The byte-exact C-vs-Python gate and
the capture comparison both need the untracked `data/game/C` assets
(`S16TITLE.GRA`, `FAT.OPL`) and the untracked `.dro` capture. In a checkout
without them the test prints `SKIP sequencer real-data checks — including the
governing C-vs-Python byte gate` and the suite still passes, so a green
`./build/run_tests` there does **not** exercise the oracle. Run
`PR_ORACLE_REQUIRED=1 ./build/run_tests` with the data present to make a missing
asset fail and the gate run. `verified (cmd: run_tests SKIP line)`.
