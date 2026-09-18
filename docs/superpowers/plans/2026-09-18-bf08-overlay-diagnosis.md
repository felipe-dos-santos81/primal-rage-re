# Diagnosis: `0x2BF08`'s per-frame title overlay (4a-iii Task 2)

Research deliverable for the title-residuals cycle. Branch `title-residuals`.
Task 1 removed the `0x2BF08` inert pin and measured **total** divergence between
the un-pinned original and the port (both captures 100% unexplained, 0/96 port
frames exhibited, no window; deterministic; pinned-backup control still green).
This document resolves the two competing explanations for that divergence and
identifies the missing content.

## Verdict

**Overlay, not alignment/timing shift.**

Removing the inert pin does not move the original in time. The un-pinned original
draws **one extra, deterministic, centred text line on every title frame** that
the port does not draw. In the captures that line is:

```
CREDITS:5
```

rendered at screen rows 7–12, columns 130–196 (166 pixels, 498 bytes). It is
composed by `0x2BF08`'s `DS_00105C00 != 0` message branch from the already-ported
string decoder and text renderer. The `0x2BF08` pin's removal changes nothing
else: for three sampled pairs the pinned control frame is byte-identical to a
port frame and the un-pinned frame differs from it **only** in that 166-pixel
block (§2).

The port's own comment at `flow.c:863-875` — "`0x2BF08` early-returns unless
`(DS_000EF6DC & 0x1F) == 0`, i.e. frames 32/64/96" — is **wrong**. That gate is
one arm of a four-way branch and is not reached when a message is active; the
real early-return is `DS_0009AD58 != 0` (the attract flag). §4 gives the full
branch map.

---

## 1. The two hypotheses and the test that separates them

| # | Hypothesis | Prediction |
|---|---|---|
| 1 | **Overlay.** `0x2BF08` composites a small time-varying element the port does not draw. | The capture matches one port frame uniquely, with a small residual that is *content*, not motion. |
| 2 | **Alignment/timing shift.** The pin's removal changed frame timing/draw order, so the capture is shifted ~a frame. | The capture matches a **neighbouring** port frame (N±1) as well as N, or an adjacent-frame splice exactly. |

Both were tested directly against `/tmp/pr_title_dump/title` (the 96-frame port
dump) and `data/title-captures/title` (587 frames, un-pinned).

### 1.1 Best match is a unique port frame; neighbours are far worse

For each sampled capture frame, the byte-difference count to every port frame
was computed; `N` is the argmin.

| capture frame | best N | residual vs `port[N]` | vs `port[N-1]` | vs `port[N+1]` |
|---|---|---|---|---|
| 226 | 10 | **498** | 21124 | 21167 |
| 242 | 24 | **3236** | 18458 | 33044 |
| 258 | 37 | **498** | 32111 | 855 |
| 274 | 51 | **498** | 21596 | 21604 |
| 290 | 65 | **1776** | 31409 | 23167 |
| 306 | 79 | **9055** | 40692 | 28315 |
| 322 | 92 | **497** | 98104 | 128587 |

A one-frame timing shift would make the capture match `port[N+1]` (or `N-1`)
exactly, i.e. residual ≈ 0 on the neighbour. Instead every neighbour residual
exceeds its frame's best-match residual: the smallest is 855 bytes (`j=258`,
`vs N+1`) and the largest 128587, while `port[N]` residuals are 497–9055
(0.26%–4.7% of the frame). **Hypothesis 2 is rejected.**

The best two-piece splice under the oracle's own model (split `port[N]` /
`port[N+1]` at the byte that minimises mismatch) reduces every sampled frame's
residual to **497–498 bytes** — the constant text block, not zero. A genuine
fractional-frame blend would splice to ≈0.

### 1.2 A/B against the pinned control isolates exactly the overlay

Task 1 left the pinned reference at
`$TMPDIR/opencode/pr-title-captures.pinned-backup/title` (582 frames;
git-ignored scratch). For a sampled un-pinned frame, the closest pinned frame
was found by content, and that pinned frame was then matched to a port frame:

| un-pinned frame | aligned pinned frame | pinned == port? | un-pinned vs pinned | diff bbox |
|---|---|---|---|---|
| 226 | 225 | `frame_0225 == port[10]` **exactly** | 498 bytes | rows 7–12, cols 130–196 (166 px) |
| 258 | 257 | `frame_0257 == port[37]` **exactly** | 498 bytes | rows 7–12, cols 130–196 (166 px) |
| 274 | 273 | `frame_0273 == port[51]` **exactly** | 498 bytes | rows 7–12, cols 130–196 (166 px) |

The pinned frame is the known-green original with `0x2BF08` inert. The un-pinned
run is the same run with `0x2BF08` live. The two differ **only** in the
`CREDITS:5` block, and the pinned frame is pixel-identical to the port. This
directly attributes the entire residual to `0x2BF08` and rules out a timing
shift: removing the pin did not move the frames relative to each other, it added
166 (498/3) pixels of text.

Content hashes (SHA-256 of the 192000-byte RGB24 frame) pin the three cited
matches; the port and pinned-frame hashes are identical, the un-pinned frames
differ. Prefixes (16 hex chars):

| frames (port/pinned/un-pinned) | port prefix | pinned prefix | un-pinned prefix |
|---|---|---|---|
| `0010` / `0225` / `0226` | `fd7f8475ed2c0ac6` | `fd7f8475ed2c0ac6` | `ce8f4ddc8a93fd8d` |
| `0037` / `0257` / `0258` | `4c0a76dd40e36011` | `4c0a76dd40e36011` | `2a5ecae378b86033` |
| `0051` / `0273` / `0274` | `f6e7d0566558fd96` | `f6e7d0566558fd96` | `b16bd2c8ca5c201d` |

Full values: port `frame_0010` =
`fd7f8475ed2c0ac67575f388b191791ef22f427e834274ad8be197e5f4764576`,
`frame_0037` = `4c0a76dd40e3601105bd18fc24c167d4626c83ddcc61945e2cc800d9b50e7eb5`,
`frame_0051` = `f6e7d0566558fd96215aaba1416afdb0e54191fab7bb119cc1828c7818871ab8`;
pinned `frame_0225`/`0257`/`0273` are byte-identical to those three; un-pinned
`frame_0226` = `ce8f4ddc8a93fd8d28bb63ef9f45b2c1ee6ecd982808202653fdb9c1bc114eba`,
`frame_0258` = `2a5ecae378b86033d795f34c147634ff8f60ce4583b2a3232e7a3b20e59c1420`,
`frame_0274` = `b16bd2c8ca5c201df1065fd0b423327985f51ffa78b63a928a471d3039745e24`.
The port frames are regenerable with `make title-oracle`; the un-pinned frames
are the current `data/title-captures/title`; only the pinned backup is scratch
(§7 gives the hash command), which is why its three hashes are recorded here.

(The aligned indices are one lower in the pinned run because the un-pinned
capture acquired one extra frame at the start; the oracle aligns by content, so
this is an index offset, not a per-frame timing shift.)

### 1.3 Determinism

Task 1 established, and the port oracle re-confirmed (`make title-oracle` reports
0/96 exhibited for both captures), that the two un-pinned captures are
byte-consistent at every sampled frame. The overlay is deterministic.

---

## 2. The overlay content

### 2.1 `0x2BF08`'s message branch

`0x2BF08` at `prage.c:16666` (VA `0x2BF08`, file `0x7ED5C`). The
`DS_00105D60 == 0` / `DS_00105C00 != 0` arm (raw `0x2BF7D`–`0x2BFCD`) is:

```
2bf7d  mov  ebx, [0x85c00]          ; ebx = DS_00105C00
2bf83  test ebx, ebx
2bf85  je   0x2bfce                  ; 0 -> the &0x1f / insert-coins arm
2bf87  push ebx                      ; sprintf arg 3 = DS_00105C00
2bf88  mov  eax, 0x46
2bf8d  call 0x1c500                  ; game_string_get(0x46) -> "CREDITS"
2bf92  push eax                      ; sprintf arg 2 = string
2bf95  push 0x9bc                    ; sprintf arg 1 = format pointer
2bf9a  lea  eax, [esp+0xc]
2bfa0  push eax                      ; sprintf arg 0 = dest
2bfa7  call 0x65546                  ; FUN_00065546 == sprintf
2bfaf  mov  eax, 0xffffffff          ; text_cursor_set col = -1 (centre)
2bfb4  mov  dl,  [0x85c05]           ; row = DS_00105C05
2bfba  call 0x2f198                  ; text_cursor_set
```

So the overlay is
`sprintf(buf, "%s:%d", game_string_get(0x46), DS_00105C00)`, followed by
`text_cursor_set(-1, DS_00105C05, buf, 0)`.

Register/string proof:

* **Format `%s:%d`.** The immediate `0x9BC` is pre-relocation; the LE fixup adds
  object 1's base `0x80000`, so the runtime pointer is `0x809BC`. Loading
  `data/game/C/PRAGE.EXE` through the same LE loader the port uses
  (`port/src/mem.c`) yields at VA `0x809BC` the bytes
  `25 73 3a 25 64 00` = `"%s:%d"`. (Also present verbatim at file offset
  `0xC7810`.)
* **String `0x46`.** `game_string_get` is the ported `0x1C500`+`0x474E4` decoder
  over `ENGLISH.TXT`. Decoding id `0x46` from
  `data/game/C/ENGLISH.TXT` gives `"CREDITS"`; id `0x45` = `"  - FREE PLAY -  "`,
  id `0x47` = `"- INSERT COINS -  "`, id `0x15` = `"THE FUTURE..."`.
* **Value.** `DS_00105C00` is both the non-zero gate and the `%d` argument.
* **Row.** `text_cursor_set`'s EDX is loaded from `DS_00105C05` (`8a 15 05 5c 08
  00`). The captured glyph occupies **screen** rows 7–12. That is text row
  `1`, not `7`: the renderer scales a text row by `20/3` px (`glyph y = row *
  0x200`, projected by `3414/4096`), so text row `7` would land at screen rows
  ~47–52. Seeded as `7` the overlay does not reproduce the capture; seeded as
  `1` it is byte-identical. See the erratum at the end of this document.

### 2.2 The captured pixels read `CREDITS:5`

Diff the un-pinned capture against the aligned point where the pinned control is
byte-identical to a port frame; the residual is a 6-row-tall bitmap. Rendering
the capture region (rows 7–12, cols 130–196) as ASCII (`#` = pixel > 40) gives:

```
cols 130..196 (67 wide), verbatim from the capture:
row  7  ..####..####....#####..####......##.....#####..###.....##.....#####
row  8  .###...##..##..###....###.##....##.....#####..##.......##....##....
row  9  ###....##..##..##.....##..##....##.......##..###.............#####.
row 10  ##.....##.##...#####..##..##....##.......#....####..............###
row 11  ##.....#####...##.....##.##.....##.......#......##.....##......###.
row 12  .####..##..##..####...####.....####......#...####......##....####..
```

Column segmentation gives **8 blobs covering 9 characters**, widths
`[6,6,6,6,4,11,2,6]`:

* `C R E D I T S` (the 11-px blob is `T` and `S` touching), then `:` (width 2),
  then a digit (width 6).
* The digit is a `5`: full top bar, left descender on the second row, full middle
  bar, right-only lower strokes, bottom-left hook. (Compare `3`, which has the
  second row on the *right*; our second row is on the left.)

The text is present, identically, at every sampled title frame
(`j = 220, 240, 260, 280, 300, 320, 420, 520` all read `CREDITS:5`); the
all-bright frames outside the title window are fade/other screens, not the
overlay.

### 2.3 Where the value comes from

`DS_00105C00` is a **live credit counter**, not an immutable config snapshot.
There are three stores to `0x85C00`; the first sets the initial value, the other
two decrement it:

```
; FUN_0002C304 (prage.c:17047) -- initial value
2c304  mov  eax, 0x29
2c309  call 0x2d974                  ; FUN_0002D974(0x29)
2c30e  and  eax, 0xf0000
2c313  sar  eax, 0x10
2c316  inc  eax
2c317  mov  [0x85c00], eax           ; DS_00105C00 = high_nibble + 1

; FUN_0002CA48 (prage.c:17368) -- decrement by 1
2ca57  cmp  dword [0x85c00], 0
2ca5e  je   0x2ca78
2ca60  cmp  byte [0x84b1f], 0        ; DS_00104B1F: input held -> skip
2ca67  jne  0x2ca6f
2ca69  dec  dword [0x85c00]

; FUN_0002CA7C (prage.c:17395) -- decrement by EAX
2ca8b  cmp  eax, [0x85c00]
2ca91  ja   0x2ca78
2ca93  cmp  byte [0x84b1f], 0        ; DS_00104B1F: input held -> skip
2ca9a  jne  0x2caa2
2ca9c  sub  dword [0x85c00], eax
```

`FUN_0002C304` is called from `FUN_00010E80` (`prage.c:721`), the game-state
init. `FUN_0002D974` is the save/config record reader: `FUN_0002D974(0x29)` is
the same read the port already pins to `0` for `DS_00104528`
(`flow.c:698-705`).

`FUN_0002CA7C` is reached from `FUN_00011F28` (`prage.c:1181`), the title-state
input handler, at raw `0x11F47` (`call 0x2ca7c`); `FUN_0002CA48` is the other
decrementer (called at `prage.c:28762`). `FUN_00011F28` is itself called from the
`0x11D04` tail before the state switch; the port stubs it
(`flow.c:827-829`: "PORT: 0x11F28 menu-input poll (menus, sub-project 4).").
So on the shipped no-input title path `DS_00105C00` holds its initial value, but
once input exists it counts down. **All three stores are unported writers**, not
just the initializer.

On the **static** shipped image `FUN_0002D974(0x29) = 0` (table entry at
VA `0x2D3A4` = `0x1D980`; the config bytes at `DS_00105DE0..` are zero), which
would give `DS_00105C00 = 1`. The captures show `5`, so the high nibble was `4`
at runtime: the save/config bytes at `DS_00105DE0` were populated by the game's
config subsystem in the DOSBox-X run (the "`0x2D974` save/config" residual in
the spec, §8). This is the one input a port of `0x2BF08` alone cannot derive
from shipped data — see §5.

`DS_00105C05` (the row) has two writers: `FUN_0002BF00` (VA `0x2BF00`:
`mov byte [0x85c05], 0x1d`, `prage.c:16660`) on the init path
(`prage.c:11436`), and `FUN_0002C06C` (VA `0x2C06C`: `mov [0x85c05], al`),
called from the attract/mode entries (`0x110ce`, `0x115c1`, `0x11a51`, `0x11aa3`,
`0x11e03`, `0x11fb4`). The port enters the title directly and calls neither, so
that row is a second unported input.

---

## 3. Step 1 — the aperture path: which callee reaches the composite

`0x2BF08`'s six callees and what they do:

| callee | port name | reaches the composite? |
|---|---|---|
| `FUN_0002CAA8` | new | No — reads `DS_00105D60 == 0`. |
| `FUN_0001C500` (`0x474E4`) | `game_string_get` | Indirect: decodes into `DS_00102760`. |
| `FUN_00065546` | new (`sprintf`) | Indirect: formats into a stack buffer. |
| `FUN_0002F198` | `text_cursor_set` | **Yes** — `text_render` → `text_glyph_emit` → `actor_spawn` into the text grid at `DS_00105F38`. |
| `FUN_0002F4BC` | `text_cursor_hold` | **Yes** — `text_cursor_set` with the grid cursor saved/restored. |
| `FUN_0002F280` | `text_cells_release` | **Yes** — releases the grid records for those cells. |

The text grid records are actor nodes; `game_frame` calls `actors_update` (the
original's `0x2A31C`) before the render table composites (`flow.c:822`), so a
spawn/release in `0x2BF08` changes the presented frame. That is why the overlay
shows up as a pixel diff.

### Which callees run on a `(&0x1F) != 0` frame vs a `== 0` frame

`0x2BF08` is called every frame from `0x11D04` (the state-machine tail, case 1
= title; `prage.c:1197-1201`). The only true early-return is the first test:

```
2bf0e  mov  ah, [0x1ad58]   ; DS_0009AD58 (attract sub-machine active)
2bf16  test ah, ah
2bf18  jne  0x2c056         ; return
```

In the no-input title capture `DS_0009AD58 == 0`, so the body runs. The body is
a four-way branch, and the two flags that select it are `DS_00105D60`
(`FUN_0002CAA8`) and `DS_00105C00`:

| flags (capture) | branch | callees | `&0x1F` gate? |
|---|---|---|---|
| `DS_00105D60 != 0` | FREE-PLAY | `game_string_get(0x45)`; `text_cursor_hold` (`&0x20`) or `text_cells_release` (`!&0x20`) | No |
| `DS_00105D60 == 0`, `DS_00105C00 != 0` | **message (observed)** | `game_string_get(0x46)`, `sprintf`, `text_cursor_set` | **No** |
| `DS_00105D60 == 0`, `DS_00105C00 == 0`, `(&0x1F) != 0`, latch `== 0` | none | clear latch, return | Yes |
| `DS_00105D60 == 0`, `DS_00105C00 == 0` otherwise | insert-coins | `game_string_get(0x47)`; `text_cursor_hold`/`text_cells_release` | Yes (`&0x20` picks hold vs release) |

In the capture the message branch is taken: `DS_00105C00 == 5`,
`DS_00105D60 == 0`, `DS_0009AD58 == 0`. That branch has **no `&0x1F` test**, so
`CREDITS:5` is drawn on every frame, `(&0x1F) != 0` and `== 0` alike. This
reconciles Task 1's "every frame differs" with the code.

The port comment's error is one of scope: it describes only the
`DS_00105C00 == 0` fallback (the `- INSERT COINS -` line) and omits the two
branches that precede it. The `&0x1F`/latch sequence at raw `0x2BFCE` is never
reached while `DS_00105C00 != 0`.

The decompiler also misstates this call: `prage.c:16694` shows
`FUN_0001c500(DAT_00105c00)`, but the raw bytes are `mov eax,0x46; call
0x1c500` — the string id is the constant `0x46` ("CREDITS") and
`DS_00105C00` is the `%d` argument. Raw bytes win.

---

## 4. Step 2 — overlay content (summary)

* **What is drawn:** a centred, single-line message, `sprintf("%s:%d",
  game_string_get(0x46), DS_00105C00)` → `CREDITS:5` in the captures.
* **Source record:** the string id `0x46` in `ENGLISH.TXT` (via `0x1C500` /
  `0x474E4`); the value `DS_00105C00` (writer `0x2C304` ← `0x2D974(0x29)`); the
  row `DS_00105C05` (`0x2C06C`).
* **Not** a `DS_00105C00` message-id lookup (the decompiler's reading), not a
  blinking cursor, and not text already in the grid — the grid has no such line
  before `0x2BF08` runs (the port frame is background there).

---

## 5. Step 3 — the port estimate

### Already ported (reused verbatim)

| Original | Port | Where |
|---|---|---|
| `0x1C500` + `0x474E4` string decode | `game_string_get` | `flow.c:254` |
| `0x2F198` | `text_cursor_set` | `actors.c:1476` |
| `0x2F4BC` | `text_cursor_hold` | `actors.c:1522` |
| `0x2F280` | `text_cells_release` | `actors.c:1495` |
| `0x2F830`, `0x2F0F0`, `0x2F5A0` | `text_render`, `text_width`, `text_glyph_emit` | `actors.c` |

So the whole rendering half of the overlay is already ported and exercised by
the title caption (`flow.c:370`).

### New work

| Item | Size | Notes |
|---|---|---|
| `0x2BF08` transcription | ~45 lines in `flow.c`, at the existing PORT marker `flow.c:863-875` | Four-branch control flow; reuse the four ported text functions; no new file. |
| `FUN_0002CAA8` | 1 line (`DS_00105D60 == 0`) | inline helper. |
| `FUN_00065546` | 1 line | `snprintf(buf, n, "%s:%d", s, DS_00105C00)`; no need to port the original formatter. |
| `DS_00105C00` data | pin or producer | **The blocking input** (§2.3). Three unported writers: `FUN_0002C304` (initial, `= high_nibble+1`), `FUN_0002CA48` (`dec`), `FUN_0002CA7C` (`sub`, reachable from the title input handler `0x11F28`). Static image gives `1`; capture is `5`. A `5` seed alone matches the no-input window only. |
| `DS_00105C05` data | pin or producer | Capture shows **screen** rows 7–12 = text row `1` (`20/3` px per row); unported writers `FUN_0002BF00` (`0x1d` at init) and `FUN_0002C06C` (attract/mode entries). |
| Unit test | one `test_flow` case | Cover the four branches: early return, FREE-PLAY hold/release, CREDITS (message), insert-coins `&0x1F`/`&0x20`; assert on globals, not rendering. |
| Negative control | 1 assertion | Stub `0x2BF08` back to a no-op and show the overlay frames drift. |

**Critical caveat for the re-spec:** porting `0x2BF08` alone does **not** turn
the un-pinned oracle green. With no producer ported, `DS_00105C00 == 0`, the
message branch is skipped, and the un-pinned capture still carries the
`CREDITS:5` text on every frame. If instead `FUN_0002C304` is ported on top of
the existing `FUN_0002D974(0x29) = 0` pin, it renders `CREDITS:1` — still wrong.
The *initial* value that yields `5` is runtime save/config state (the `0x2D974`
residual), so it is data, not code — but see the countdown caveat next.

**And the seed is a fidelity compromise, not the source.** `DS_00105C00` is a
live counter (§2.3): `FUN_0002C304` sets the initial value, then `FUN_0002CA48`
(`dec`) and `FUN_0002CA7C` (`sub`, reached from the title input handler
`FUN_00011F28` at `prage.c:1181` / raw `0x11F47`) count it down as credits are
consumed. Seeding `5` reproduces the **no-input** captured window exactly — the
oracle's window — but it does **not** reproduce credit countdown once input
exists. Porting the seed is therefore not equivalent to porting
`FUN_0002C304`/`FUN_0002CA48`/`FUN_0002CA7C`; it is a documented, coverage-limited
pin whose behavioural gap (input-driven credits) belongs to the input
sub-project (4b) that owns `0x11F28`.

---

## 6. Step 4 — recommendation

**Smaller intermediate: port `0x2BF08`'s control flow, seed `DS_00105C00 = 5`
and `DS_00105C05 = 1` for the no-input oracle window, and re-run the oracle. Do
not port the `0x2D974` config subsystem in this cycle; record the input-driven
credit countdown (`0x2CA48`/`0x2CA7C` via `0x11F28`) as a declared 4b gap.**

Rationale, in order of weight:

1. **Architectural fit.** The overlay's rendering composes entirely through
   already-ported code; the only new code is one 45-line transcription plus a
   one-line flag test and `snprintf`. That belongs in `flow.c` at the existing
   PORT marker. There is no new module.
2. **The blocking input is data, but the counter is not.** The *initial* value
   of `DS_00105C00` is a save/config read (`0x2D974`), not game logic; porting
   `FUN_0002D974` + the save loader to obtain one nibble is exactly the
   "machinery with no caller" anti-pattern the spec (§1.1) rejects. The
   *decrements* are input-driven and belong to 4b with `0x11F28`. So the right
   layer here is a two-value pin for the oracle window, alongside the existing
   `0x2D974` and RNG pins, with the countdown explicitly carved out.
3. **Falsifiable.** With the captured values seeded (`DS_00105C00 = 5`,
   `DS_00105C05 = 1`), the un-pinned oracle either goes green — proving
   `0x2BF08` moves the composite — or it does not, in which case `0x2BF08` is
   recorded as a declared gap with the unit proof (spec Decision 5). The pin is
   documented and narrow (two values), unlike the removed byte pin which hid the
   whole function.
4. **Fidelity cost is stated, not hidden.** The seed matches the no-input
   captured window only; it does not reproduce credit countdown once input
   exists (see §2.3/§5). The countdown is deferred to the input sub-project, so
   the oracle's claim is bounded: it proves `0x2BF08` against the no-input
   window, not against a credit-consuming run.
5. **Cost of the alternative.** "Keep the pin and document the carve-out" leaves
   the oracle proving only the pinned original, which is the state Task 1
   existed to end. Porting the overlay now without the data pin fails the oracle
   for the wrong reason (`DS_00105C00 == 0`), which is worse than the current
   honest red.

If the seeded port does make the oracle green, the cycle's residual is reduced
to the two documented data pins plus the 4b countdown gap. If it does not,
revert to the pin and record `0x2BF08` + `0x2D974` as the gap. Either way the
oracle's claim becomes true.

---

## 7. Reproduction

All commands are read-only; nothing under `data/` is modified. Capstone 5.x,
GNU syntax, 32-bit. `file_offset = va + 0x52E54` (object 0). Data-object
addresses (`DS_*`) are `raw + 0x80000` (object 1); `DS_00105C05` is written raw
at `0x85C05`.

```python
# disassemble 0x2BF08 and its callees
from capstone import *
d = open('data/game/C/PRAGE.EXE','rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_32); md.syntax = 0
for ins in md.disasm(d[0x2BF08+0x52E54:0x2BF08+0x52E54+341], 0x2BF08):
    print(hex(ins.address), ins.bytes.hex(), ins.mnemonic, ins.op_str)
```

```python
# decode ENGLISH.TXT string ids with the ported 0x474E4 algorithm
data = open('data/game/C/ENGLISH.TXT','rb').read()
def sd(base, i):
    off = 0
    for _ in range(i//0x40): off = int.from_bytes(data[base+off+4:base+off+8],'little')
    p = base + off + 8
    for _ in range(i % 0x40): p += data[p] + 1
    n = data[p]
    return bytes(data[p+1+k] ^ n for k in range(n))
print(sd(0, 0x46))   # b'CREDITS'
```

```python
# the format pointer: LE fixup makes 0x9BC -> 0x809BC in object 1
# reproduce with port/src/mem.c's loader, or read the static bytes:
raw = open('data/game/C/PRAGE.EXE','rb').read()
print(raw[0xC7810:0xC7816])   # b'%s:%d\x00'
```

```python
# A/B: isolate the overlay against the pinned control (numpy)
import numpy as np, os
W,H = 320,200; B=W*H*3
def L(d,i): return np.frombuffer(open(os.path.join(d,'frame_%04d.raw'%i),'rb').read(),np.uint8)
port = np.stack([L('/tmp/pr_title_dump/title',i) for i in range(96)])
# Task 1 placed the pinned backup under the scratch dir ($TMPDIR/opencode/ here)
pin  = np.stack([L(os.path.expandvars('$TMPDIR/opencode/pr-title-captures.pinned-backup/title'),i)
                 for i in range(582)])
c = L('data/title-captures/title', 226)
p = int((pin != c).sum(axis=1).argmin())          # closest pinned frame
print((pin[p] != port[10]).sum())                  # 0: pinned control == port[10]
diff = (c != pin[p]).reshape(H,W,3).any(axis=2)
rows = np.nonzero(diff.any(axis=1))[0]; cols = np.nonzero(diff.any(axis=0))[0]
print(rows.min(), rows.max(), cols.min(), cols.max(), diff.sum())  # 7 12 130 196 166
```

```python
# SHA-256 of the cited frames (port / pinned / un-pinned); §1.2 records them
import hashlib, os
def sha(d,i): return hashlib.sha256(
    open(os.path.join(d,'frame_%04d.raw'%i),'rb').read()).hexdigest()
pin_dir = os.path.expandvars('$TMPDIR/opencode/pr-title-captures.pinned-backup/title')
for port_i, pin_i, unp_i in ((10,225,226),(37,257,258),(51,273,274)):
    print(port_i, sha('/tmp/pr_title_dump/title', port_i))
    print('   ', pin_i, sha(pin_dir, pin_i))
    print('   ', unp_i, sha('data/title-captures/title', unp_i))
# port and pinned rows are equal; un-pinned differs
```

---

## 8. Files and invariants

* This document only. No port, oracle, pin, driver, `data/`, or capture change.
* `data/` untouched; captures git-ignored and not re-run (the pinned backup used
  for §1.2 already existed in scratch from Task 1).
* Every claim above is instruction-level (raw `0x2BF08` bytes, raw writes to
  `DS_00105C00`/`DS_00105C05`/`DS_00105D60`, the LE-loaded `"%s:%d"`) or
  byte-level (ASCII glyphs, A/B residuals).

---

## Erratum — `DS_00105C05` is `1`, not `7` (corrected after implementation)

§2.1 read the captured glyph's **screen** rows 7–12 as `DS_00105C05 == 7`. That
is wrong: the renderer scales a text row by `20/3` px (`glyph y = row * 0x200`
projected by `3414/4096`), so `7` would place the text at screen rows ~47–52.

The port seeded `DS_00105C05 = 1` (and `DS_00105C00 = 5`) and
`make title-oracle` passes against the un-pinned captures: 0 unexplained, 94/96
exhibited in both. Port dump frames `frame_{0010,0037,0051}.raw` are
byte-identical to un-pinned capture frames `{0226,0258,0274}.raw` (sha256
prefixes `ce8f4ddc…`, `2a5ecae3…`, `b16bd2c8…`), matching §1.2's un-pinned
hashes. Seeding `7` does not reproduce the block.

Corrected in place at §2.1, §5's table and §6's recommendation. The two-seed
pin and the 4b countdown carve-out are unchanged; only the row value was wrong.
