# E-wire, K8b and K8d — raw-byte derivation (Task 5a of `2026-09-29-all-gaps.md`)

**Scope.** Ledger `2026-09-29-all-gaps-ledger.md` §E rows 3, 4 and 19, §F
orders 13 (E-WIRE) and 15 (K8b UPD-BONUS), §B.2 entries 9/12/15/16/17, and the
Task 3e follow-ups: the animation-opcode setters `0x37EA0`/`0x24078`/`0x45D58`
(record `2026-09-29-k8c-derivations.md` §K8c.7, here "K8d"), the signed clamp
of `0x4F944` and four carried minors. Sections are `§W`, `§B8`, `§D8`, `§C` and
`§M`; the port headers cite them.

**Tooling.** The Ghidra MCP bridge was not reachable (`ToolSearch "ghidra"`
finds no tool). Every byte was read from the Python mirror of `mem_load_le` +
`mem_load_le_fixups` (`port/src/mem.c`), so each dword has the LE fixups
applied, and disassembled with capstone (32-bit). Caller scans cover every
`E8`/`E9` rel32 in `0x10000..0x73B14` and every absolute dword in both objects;
data-reference scans decode every instruction whose bytes contain the address.
Immediates were checked against the raw file (obj-0 file offset = VA +
`0x52E54`) to tell fixed-up data addresses from constants.

---

## §W E-3 / E-19: mode 0x33 calls `0x4DEF4`

```
29638: push ecx / push edx
2963a: mov eax,[0x1077e4] ; mov [0x1077e8],eax
29644: mov eax,[0x107878] ; mov [0x10787c],eax
2964e: xor eax,eax ; call 0x35658          ; fight_hud_pass(0)
29655: mov eax,1   ; call 0x35658          ; fight_hud_pass(1)
2965f: call 0x4def4                        ; fight_effects_idle_pass
29664: call 0x12da8                        ; camera_y_commit
29669: mov ah,[0x104aec] ; mov dx,[0x104afe] ; or ah,2 ; dec edx
2967a: mov [0x104aec],ah ; mov [0x104afe],dx ; test dx,dx ; jg 0x296b5
2968c: eax = 0x2b ; ecx = 0x17 ; xor dl,dl ; call 0x2c3fc
2969d: mov [0x104b25],dl ; edx = 0x25ae8 ; mov [0x104b00],cx ; mov [0x104ae4],edx
296b5: pop edx / pop ecx ; ret
```

The call at `0x2965F` takes no register argument: `0x4DEF4` pushes EBX..EDI
and reloads everything from memory (record §49-F; the port's
`fight_effects_idle_pass()` takes none). It sits between the second
`fight_hud_pass` and `camera_y_commit`, exactly as mode 0xF's `0x277E9`. The
`DS_00104AEC` read at `0x29669` follows it, so the tail order in the port is
already right. **Verdict: wire it** (`flow.c`, `game_mode_33_step`); the stale
named-gap comment is replaced.

**E-19.** `0x4A868`'s six sites (record §K4): `0x4BFDE` and the four `0x4DEF4`
states are wired (Task 3d). With `0x2965F` wired, both raw callers of
`0x4DEF4` (`0x277E9`, `0x2965F`) now call it, so the "not wired yet (ledger
§E-3)" notes in `fight.c`/`fight.h`/`flow.h` are rewritten. The one remaining
unwired site is `0x4A361` (the case-14 body `0x4A346..0x4A412`, `fight.c`
`0x49C78`), blocked on K13 (Task 5b).

Test (`check_mode_33` (d), `test_fight.c`): `DS_001088B0 = 0x0100`,
`DS_001088BB = 0` (no re-arm, no rng draw; `mz_seed`'s effect entry is type 8,
which the walker skips) → `0x00FF` after one `game_mode_33_step`, flag kept,
`DS_00104AFE` 5 → 4. Mutations: the call removed (`0x0100`) or doubled
(`0x00FE`) both fail. The order against `camera_y_commit` is taken from the
raw; the two touch disjoint state, so no test can see it.

Reachability: see §R (the probe).

## §R Reachability probe (not committed)

A temporary `fprintf` (applied and reverted by a script; `git status` clean
afterwards) printed: the update mask `DS_000A8644` on every change (positive
control: entry 7's 0x80), every `game_mode_33_step` call, every
`flow_round_bonus_a/_b` call, the first three `anim_indirect` targets
(positive control) and every `anim_indirect` whose `DS_00105BD4` is
`0x37EA0`/`0x24078`/`0x45D58` (registered or not).

- `prageport --check 8000`: masks `0` (frame 1), `0x80` (5061), `0` (5774),
  `0x80` (6811), `0` (7487); anim targets `0x10FA8` (341), `0x12720` (1740),
  `0x35938` (1960). No mode 0x33, no bonus call, no §D8 target.
- `PR_FRONTEND_DET` (the demo-fight / attract2 driver; `run1`/`run2`
  identical): masks `0` (887), `0x80` (4426), `0` (4572), `0x80` (4918); anim
  targets `0x12720` (1740), `0x39A34` (1972), `0x36870` (1986). No mode 0x33,
  no bonus call, no §D8 target.

So none of the four changes is reached on a no-input oracle path: the E-3 wiring
runs only in mode 0x33 (a match end), the bonus cards only after a decided round
outside `DS_00104B1D` 2/3 (card A when the winner's `+0x5A` is 0, card B when it
is not and the timer byte `DS_001088F2` is at least 0x32, or through card A's
chain),
and the §D8 targets only when their character streams run. The oracle lines,
the 8000-frame dump and the front-end dump are unchanged (report). Entries 8,
9, 11, 12, 15, 16, 17 are live in real play; their correctness rests on the
unit tests and the raw.

