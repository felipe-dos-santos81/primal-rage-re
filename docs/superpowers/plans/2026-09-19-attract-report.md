# Attract Subsystem and State Tails (4d) — Cycle Report

Plan: `docs/superpowers/plans/2026-09-19-attract.md` (base `8d945cb`, branch
`attract`). This report covers Task 5 (the continuous-run oracle, the stand-in
retirement, docs and the ladder), and records the cycle-wide findings the plan
asked for.

## Status

**Complete.** Boot enters state 0 (attract); the row stand-in is gone; the
continuous attract+title oracle runs; the title window stays 0 unexplained on
both captures; the attract oracle reports its first divergence at capture frame
68 (raw 1626/1621), individually explained from the raw as the declared
unported `0x4F7F4`/`0x4F83C` scene-palette driver. `rm -rf build && make verify`
exits 0 with 0 C warnings, `all checks passed`, smk 120/120 + 41/41, the title
oracle green, and `symbols.h` byte-identical.

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
attract_compare: ... 68/216 capture frames explained; port attract frames exhibited 63/690
attract_compare: ... FIRST DIVERGENCE at capture frame 68 (raw 1626)
attract_compare: ...   best byte splice port214[0..0) ++ port215[0..192000) still differs at 920 byte(s) (first row 47 byte 45657)
```

| capture | first divergence | raw | capture frames explained | port frames exhibited |
|---|---|---|---|---|
| `title` | 68 | 1626 | 68/216 | 63/690 |
| `title2` | 68 | 1621 | 68/216 | 63/690 |

**The divergence is declared and raw-explained.** It is a ~62x24 red element
(rgb `243,8,8` / `203,8,8`) where the port renders the scene's normal background
gradient — a palette animation on the attract scene. Its producer chain is:

- `0x4F83C` (the starter, unported): sets `DS_00104AD0 |= 1`, zeroes
  `DS_001088F1`, and enqueues the first palette via `0x33874`.
- `0x4F7F4` (`DS_000A8744[0]`, unported): the per-frame driver `0x292AC` calls
  for mask bit 0. It advances `DS_001088F1` through the 10-entry table at
  `DS_000C98A0`, calls `0x33874` with each entry, and clears `DS_00104AD0` bit 0
  once the counter reaches 10.
- `0x33874` (unported): enqueues a palette record from the effect record
  `DS_000F0A48`'s handle/count.

Because neither the starter nor the driver is ported, the port never sets
`DS_00104AD0` bit 0 and the palette never animates. This is the `0x4F7F4` gap
carried from Task 3/4 (the brief names the table `0x4A8744`; the port symbol is
`DS_000A8744`, raw `0x28744` + data base `0x80000`, and its shipped entry 0 is
`0x4F7F4`). No frame is silently skipped: the first 68 capture frames are
explained, and the 69th is reported with its raw index.

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

- **Attract scene palette animation** — `0x4F83C` / `0x4F7F4` / `0x33874` are
  unported; `DS_00104AD0` bit 0 is never set. This is the attract oracle's
  explained first divergence (capture frame 68).
- **Voice `0x2C3FC`** — all attract voice calls remain declared no-op stubs; the
  RNG draws that must stay (the `attract_voice_tick` `rng_next` calls) are kept.
- **Fight states 5–9, front-end state 3/4, the `0x13xxx` effect render path,
  EEPROM storage I/O** — unchanged stubs.
- **`effects_spawn`'s raw `edi = 0xB4`** register is not modelled (3-arg API);
  the same value is stored to `DS_000F0A68` (Task 4).
- The attract's rendering matched all 68 distinct capture frames that precede
  the first palette-animation frame; the other ~620 port frames are holds that
  the capture collapsed, so "port frames exhibited 63/690" is expected, not a
  coverage failure.

## Ladder evidence

- `rm -rf build && make verify` → exit 0, `warnings=0` (C), `all checks passed`,
  `smk_compare: 120/120` + `41/41`, title oracle 0 unexplained on both captures,
  `symbols.h` regenerated byte-identically.
- `PR_ATTRACT_DUMP=/tmp/pr_attract … ./build/run_tests` → `all checks passed`,
  690 attract frames + 96 title frames dumped.
- `python3 tools/attract_compare.py --capture data/title-captures/title
  --capture data/title-captures/title2 --port /tmp/pr_attract` → exit 1 with the
  first divergence above (declared).
- Two `PR_ATTRACT_DUMP` runs: `diff` of the 690-line `attract.log` is silent.
