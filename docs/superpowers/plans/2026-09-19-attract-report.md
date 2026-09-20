# Attract Subsystem and State Tails (4d) — Cycle Report

Plan: `docs/superpowers/plans/2026-09-19-attract.md` (base `8d945cb`, branch
`attract`). This report covers Task 5 (the continuous-run oracle, the stand-in
retirement, docs and the ladder), and records the cycle-wide findings the plan
asked for.

## Status

**Complete.** Boot enters state 0 (attract); the row stand-in is gone; the
continuous attract+title oracle runs; the title window stays 0 unexplained on
both captures; the attract oracle reports its first divergence at capture frame
100 (raw 1770 / 1765), the animation-triggered actor spawn `0x10FA8`
(descriptor `0x9AD08`, layer `0xE4`) that the port skips because the animation
opcode-0x11 indirect call is unported. `rm -rf build && make verify` exits 0
with 0 C warnings, `all checks passed`, smk 120/120 + 41/41, the title oracle
green, and `symbols.h` byte-identical.

> Errata: the first divergence was capture frame **68** (raw 1626 / 1621) before
> the Finding-1 spawn-slot fix (`e29f849`). Frame 68 was *that bug's* symptom,
> not a palette gap — see "Frame 68 was the spawn-slot bug" below. The boundary
> and its cause below are the post-fix truth.

## Modules and ownership

| file | owns |
|---|---|
| `port/src/game/attract.{c,h}` | `attract_step` (`0x11000`), `attract_state_reset` (`0x10EE4`), `attract_voice_tick` (`0x10F28`), `attract_scene_tick` (`0x292AC`), `frontend_pause_tail` (`0x10DB0`), `frontend_continue_tail` (`0x10E18`), `attract_config_volumes` (`0x2C8F0(-2)`) |
| `port/src/platform/render.{c,h}` | `render_scroll_edge` (`0x389C4`), `render_scroll_fill` (`0x38A38`) — they write compositor projection state (`DS_00107A3E/3A/38`, the `DS_00107900` shear table) |
| `port/src/game/flow.c` | state dispatch only: state 0/>9 → `attract_step`; the per-case tails; `game_state_init` enters state 0; the `DS_00107A54`-gated scroll calls; the two frame-dump hooks |
| `port/src/platform/gfx.c` | `palette_list_init` (`0x336C0`) — see the ownership-table fix below |
| `port/tests/test_attract.c` | the attract unit tests + the `PR_ATTRACT_DUMP` continuous driver |
| `port/tests/test_title.c` | `test_title_window`, the shared post-attract title window (driven by `test_title` alone or by `test_attract`'s continuous run) |
| `tools/attract_compare.py` (new) | the read-only attract-prefix comparator |

## The derived `0x11000` register arguments

Full disassembly and mapping: `docs/superpowers/plans/2026-09-19-attract-derivations.md`.
Summary (all recovered from the loaded image, not fitted):

- Phase 2 `0x38B18` at `0x11138`: `(eax = mem + 0x9AC08, edx = 0, ebx = 0)`.
  The raw's `mov esi, 0x3C` is not an argument — `0x38B18` overwrites `esi`, and
  the `0x3C` is the `DS_000F0A62` store at `0x11178`. The helper was not widened;
  it was renamed `frontend_spawn_row` and exported.
- Phase 2 `DS_000F0A60 = 0x2D`: `ecx` is live across the stubbed `0x2C3FC(0x40)`
  / `0x2C3FC(0x42)` calls (case 2 preserves `ecx`), so the value survives to the
  `0x11171` store.
- Phase 9 (`0x1133F`): `width = text_width(game_string_get(0x1EB), 0x2000)`,
  `col = 0x15 - width/2`, then `(col, 0x17, str(0x1EB), 0x2000)` and
  `(col+5, 0x17, mem + 0x9ACC4, 0x2000)`.
- Phase 9 `0x113ED` `ebx = 0x4C`: an LE fixup to `mem + 0x8004C`, the fixed
  string `"16 Meg Release"`.
- The `0x11550` tail sets no argument registers; `0x10F28` reads only
  `DS_000F0A60`/`DS_000F0A62`.

## The attract oracle: window derivation and result

`tools/attract_compare.py` derives the attract window from the capture/title
alignment rather than hardcoding it: it loads the port's `title/` frames from
the same continuous run, finds the first capture frame explained by any of them
(the title window), and treats every earlier capture frame as the attract
prefix. Both captures derive **attract window [0..215], title window starting at
capture 216** (raw 2198 / 2193).

```
attract_compare: ... attract window [0..215]; title window starts at capture 216 (raw 2198)
attract_compare: ... 100/216 capture frames explained; port attract frames exhibited 96/690
attract_compare: ... FIRST DIVERGENCE at capture frame 100 (raw 1770)
attract_compare: ...   best byte splice port339[0..0) ++ port340[0..192000) still differs at 6207 byte(s) (first row 85 byte 81850)
```

| capture | first divergence | raw | capture frames explained | port frames exhibited |
|---|---|---|---|---|
| `title` | 100 | 1770 | 100/216 | 96/690 |
| `title2` | 100 | 1765 | 100/216 | 96/690 |

**The divergence is declared and raw-explained.** Capture frame 100 is where
the attract's animated logo hand-off fires. The logo actors run the sprite
streams at `0x0E88AE` / `0x0E88BC`; after sprite `0x01FD` the stream carries the
command word `0xD100` at `0x0E88D2`, whose inline dword is the code pointer
`0x00010FA8`. Animation opcode 0x11 calls that pointer through `DS_00105BD4`.
The original's `0x10FA8` is:

```
0x10fa8 53                 push ebx
0x10fa9 51                 push ecx
0x10faa 52                 push edx
0x10fab 6a00               push 0
0x10fad b9e4000000         mov  ecx, 0xe4
0x10fb2 b808ad0100         mov  eax, 0x1ad08      ; descriptor, loads as mem + 0x9AD08
0x10fb7 31db               xor  ebx, ebx
0x10fb9 31d2               xor  edx, edx
0x10fbb e8549e0100         call 0x2ae14          ; actor_spawn(0x9AD08, 0, 0xE4, 0, 0)
0x10fc0 5a                 pop  edx
0x10fc1 59                 pop  ecx
0x10fc2 5b                 pop  ebx
0x10fc3 c3                 ret
```

So the original spawns the RAGE continuation actor — descriptor `0x9AD08`
(animation stream `0x0E88E6`, flags `0x2000`, frame 4), layer `0xE4` — exactly
when the phase-5/6 actor finishes sprite `0x01FD`. The port reaches `0x01FD`
(dump 339 / `--check` 340), advances to `0x01FE` (dump 340 / `--check` 341),
and because `anim_indirect` (`port/src/game/actors.c:457`) resolves
`fn_resolve(0x10FA8)` to NULL it never spawns `0x9AD08`. The capture keeps the
letters (they are the hand-off actor); the port's letters vanish. An actor dump
confirms no actor carrying a `0x02xx` sprite (the `0x9AD08` stream) ever exists
in the port.

The same streams also dispatch opcode 0x11 to `0x4F83C` (the palette-animation
starter: `DS_00104AD0 |= 1`, `DS_001088F1 = 0`, then `0x33874`) and to
`0x10FC4` (a `mov [eax+0x18],0; ret` helper). Those calls sit later in the same
animation, so they are further declared gaps, not the frame-100 cause.

### Frame 68 was the spawn-slot bug, not the palette driver

Before the Finding-1 fix (`e29f849`, "attract: fix the spawn-phase argument
slots and add the attract gates") the first divergence was capture frame 68
(raw 1626 / 1621). That was the spawn-slot bug itself: phases 5/6/7 passed the
voice id (`0xE2`/`0xE6`/`0xE8`) in EBX (a4) instead of ECX (a3), so the spawned
logo actors took layer byte `rec+0x49 = 0` — drawn behind the background — and
put the id into `rec+0x1C`. Reverting 5/6/7 reproduces the frame-68 divergence;
fixing the slots extends the matched prefix to frames 0..99 and exposes the real
unported gap at 100. The palette driver is still unported after the fix, so it
cannot explain a divergence that the fix removed.

## The title-window result

The title window is unchanged: `make title-oracle` reports
`0 unexplained` on both captures, port frames 95/96 exhibited (endpoint frame 0
disclosed), and the determinism proof agrees on 54 clean samples. The window is
still `[216..326]` because `title_compare.py` derives it by content; no
re-derivation was needed.

The continuous run required one driver change, not a window shift: the capture
was made from the **pinned** original (`tools/title_pin.py`), whose three title
entry draws are hardcoded to the values the port's LCG produces from seed
`0xABCD` (12, 111, 0). Pre-4d the port happened to take those as the first three
draws; now the attract consumes the shared RNG stream first, so the port's draws
would differ. `test_title_window` therefore re-seeds `0xABCD` immediately before
the title entry, reproducing the pin exactly (verified: removing the re-seed
drops the oracle to 1/96 exhibited).

## Raw-vs-plan contradictions and integration findings

1. **Master loop (`0x255CC`).** The plan's sketch gated `attract_scene_tick` and
   reversed the scroll calls. The raw (`0x255F3`/`0x255F8`/`0x25601`/`0x25606`)
   runs `attract_scene_tick` **unconditionally** and gates only
   `render_scroll_edge` then `render_scroll_fill` on `DS_00107A54 != 0`. The
   port follows the raw (Task 4).
2. **`0x38B18` has no `esi` argument** (Task 4 item 1).
3. **Phase 0xA discards `actor_spawn`'s return value** (no store to
   `DS_000F0A50`); phase 0xA is also **skipped on the boot cycle** — phase 2
   wraps `DS_000F0A5C` 4 → 0, so phase 9 takes its `== 0` arm and sets
   `DS_000F0A70 = 0xB`, jumping straight to phase 0xB. The driver therefore
   asserts phases 0..9, `0xC`, `0xB` (mask `0x1BFF`), not `0xA`.
4. **State dispatch.** State 0 and every state `> 9` both target `0x11D70`
   (`attract_step`); every case's tail is `0x10DB0; 0x10E18; 0x2BF08`.
5. **`palette_list_init` (`0x336C0`) omitted the raw's ownership-table clear.**
   The raw's first action at `0x336C3` is `mem_fill(0x87618, 0, 0x180)` —
   clearing the 24-entry palette ownership table at `DS_00107618` that
   `palette_acquire` (`0x33754`) records into. The port skipped it. Pre-4d the
   table was empty at the title so the omission was invisible; the attract's
   earlier `palette_acquire` calls populate it, so the title's palette acquires
   found the handles already present and never re-enqueued them, leaving the DAC
   cleared (black) for those entries. Adding the clear restored the title
   (`0 unexplained`). This is a real raw-conformance bug, not a fitted fix.
6. **`--check` assumed the title at frame 0.** With boot on state 0, a fixed
   frame-5 announcer probe, a `frames >= 60` music probe and a per-frame
   blank check are all invalid (the attract legitimately starts on a blank
   buffer). `run_check` now asserts the title facts relative to the title entry
   and only blank-checks drawn (non-attract) frames; `make verify` runs
   `--check 820` to cross the 690-frame attract into the title.

## 4b-B correction: `0x4F1D0` vs `0x4F1E4`

`0x4F1D0` (`xor edx,edx; mov [0x87A3A],dx; mov [0x87A38],dx`) and `0x4F1E4`
(write `DS_00104B15`) are distinct raw functions that 4b-B conflated. Task 1
added `frontend_origin_zero` (`0x4F1D0`) and rewired `game_state_select` phase 0
from the mislabelled `title_input_reset()` to `frontend_origin_zero()`;
`title_input_reset` keeps its `0x4F1E4` tag. Because the select state's phase-0
behaviour changed, its determinism log was legitimately re-baselined: the new
`select.log` first line is `0 4 952198597`, run-to-run stable across two runs.
The attract phases call the true `0x4F1D0` via `frontend_origin_zero` too.

## Declared gaps (unchanged or newly named)

- **Attract logo animation indirect calls (new).** Animation opcode 0x11 in the
  logo streams (`0x0E88AE`/`0x0E88BC`/`0x0E88E6`/`0x0E8900`/`0x0E890A`) carries
  an inline code pointer and calls it through `DS_00105BD4`; the port's
  `anim_indirect` (`port/src/game/actors.c:457`) skips it because `fn_resolve`
  has no registration for the target. The targets reached on the boot cycle:
  - `0x10FA8` — spawns descriptor `0x9AD08` (layer `0xE4`); **this is the attract
    oracle's first divergence, capture frame 100 (raw 1770 / 1765).**
  - `0x4F83C` — the palette-animation starter (`DS_00104AD0 |= 1`,
    `DS_001088F1 = 0`, `0x33874`); reached later in the same animation.
  - `0x10FC4` — `mov [eax+0x18],0; ret`.
- **Attract scene palette animation** — `0x4F83C` / `0x4F7F4` / `0x33874` are
  unported; `DS_00104AD0` bit 0 is never set by the port. (The old report named
  this as the frame-68 cause; frame 68 was actually the spawn-slot bug. The gap
  is real but reachable only through the `0x4F83C` animation call above, so it
  would show after frame 100.)
- **Voice `0x2C3FC`** — all attract voice calls remain declared no-op stubs; the
  RNG draws that must stay (the `attract_voice_tick` `rng_next` calls) are kept.
- **Fight states 5–9, front-end state 3/4, the `0x13xxx` effect render path,
  EEPROM storage I/O** — unchanged stubs.
- **`effects_spawn`'s raw `edi = 0xB4`** register is not modelled (3-arg API);
  the same value is stored to `DS_000F0A68` (Task 4).
- The attract's rendering matched all 100 distinct capture frames that precede
  the first unported-producer frame; the other ~590 port frames are holds that
  the capture collapsed, so "port frames exhibited 96/690" is expected, not a
  coverage failure.

## Ladder evidence

- `rm -rf build && make verify` → exit 0, `warnings=0` (C), `all checks passed`,
  `smk_compare: 120/120` + `41/41`, title oracle 0 unexplained on both captures,
  attract oracle expected divergence at capture frame 100 on both captures,
  `symbols.h` regenerated byte-identically.
- `PR_ATTRACT_DUMP=/tmp/pr_attract … ./build/run_tests` → `all checks passed`,
  690 attract frames + 96 title frames dumped.
- `python3 tools/attract_compare.py --capture data/title-captures/title
  --capture data/title-captures/title2 --port /tmp/pr_attract` → exit 1 with the
  first divergence above (declared); `--expect-first 100` makes it the passing
  `make attract-oracle` gate.
- Two `PR_ATTRACT_DUMP` runs: `diff` of the 690-line `attract.log` is silent.
