# E-OPEN — the six open prose gaps — raw-byte derivation (Task 5c of `2026-09-29-all-gaps.md`)

**Scope.** Ledger `2026-09-29-all-gaps-ledger.md` §E rows 1, 14, 21, 24, 25
and 27 (§F order 16). Each item ends closed (derived from the raw, test
added), re-scoped (a named gap with the exact evidence and address), or stale
(the comment corrected). Sections §1..§6 follow the brief's order. The port
comments and the tests cite them.

**Tooling.** The Ghidra MCP bridge was not reachable (`ToolSearch "ghidra"`
finds no tool). Every byte was read from the Python mirror of `mem_load_le` +
`mem_load_le_fixups` (`port/src/mem.c`), so every dword has the LE fixups
applied. It was checked against the known table entry
`0x24B8C[0x1F] = 0x253D2`. Disassembly used capstone (32-bit). Writer scans
covered two ranges. The first is every Ghidra function below `0x5D000`
(`port/decomp/prage.functions.csv`). The second is a linear sweep, with
`skipdata`, of the 229 code gaps between them (80 029 bytes). Runtime
behaviour came from temporary probes: an `fprintf` in a separate scratch
build, with the source restored and `cmp`-checked afterwards.

| item | verdict | port change |
|---|---|---|
| §1 row 1, `res.c` tick drift | **re-scoped**: the stall is DOS I/O time, not in the raw; no frame observes it | comment only |
| §2 row 14, `0x36F10` in-range arm | **stale, closed**: the bit's one raw setter `0x36E78` is ported; the arm is reachable | comment; the now-live `0x353DD` retire ported; tests |
| §3 row 21, arena-frame audit | **stale**: the closure has no unported portable function | `fight.h` comment |
| §4 row 24, slot `+0x24` table | **stale, closed**: `DS_000BDA8C[char]`, stored by the ported spawn core | `fight.h` comment; test |
| §5 row 25, `DS_00100B54` | **closed**: 163 for `check_unfreeze`'s fixture, derived from the raw | test assertions and comment |
| §6 row 27, s16title at boot | **re-scoped**: a driver artefact, not a port divergence; the fix moves enforced oracle lines | test comment only |

---

## §1 Row 1: the loader's per-read tick model (`platform/res.c`)

**Question.** The port advances `DS_00101508` by `ceil(size / 132674)` per
first read. The original's re-syncs at the first demo's state-6 entry land at
ticks 3, 31, 32, 36, 54 and 55; the port's land at 2, 31, 32, 37, 56 and 58
(record §45-A.4). Can the raw pin a better model? And is the drift measurable
against the oracle captures?

**The raw.** `0x1B3AC` opens, reads and closes the file through the runtime:
`0x1B403 call 0x61C60`, `0x1B445 call 0x61CF6` and `0x1B45A call 0x61EC0`.
All three are at or above `0x5D000` (WATCOM libc over DOS `int 21h`). The
re-sync follows at `0x1B45F mov eax,[0x101508]` / `0x1B464 mov [0x10150C],eax`.
While the read blocks, only the timer ISR `0x1BDF4` runs. It increments
`DS_00101508` and `DS_00101500` (`0x1BE02..0x1BE16`), calls `0x1BBAC` (the
host-owned input sampler) and increments the word `DS_000EF6DE` (`0x1BE21`).
The read's duration is therefore the emulator's (or the drive's) I/O time. No
byte of PRAGE.EXE encodes it, so no raw-derived per-read model exists. The
132674 bytes/tick rate is already a measurement (§45-A.4), not a raw constant.

**Observability.** Every raw reference to the tick pair (a scan for the
displacements `0x101508` and `0x10150C`):

- `0x255D4`/`0x255DF`: zeroed and re-synced at the master loop's entry;
- `0x25643`: the present gate `cmp [0x10150C],[0x101508]`, a pure equality;
- `0x256C0..0x256D2`: the frame step and the spin, relative;
- `0x1B464`, `0x4FA0E`, `0x4FAB2` and `0x5210D`: re-syncs;
- `0x1BE02..0x1BE10`: the ISR increment;
- `0x1BF91`: `push 0x101508`, the DPMI lock of the page.

No reader uses the absolute value. The other counters the ISR advances
during a stall are also harmless:

- `DS_00101500` is read only through `0x500BB`, at `0x2FFDA` in the service
  menu, which no oracle path reaches.
- `DS_000EF6DE` is read at `0x2EAF2..0x2EB03`, `config_screen_wait`'s
  relative spin, and cleared at `0x5D808`.
- The 20 dword reads of `[0xEF6DC]`, whose high half is `0xEF6DE`, each mask
  the low byte (`xor ah,ah; and al,N`, or `test al,1`). The scan listing is
  in the report.

After `0x1B464` the gate passes on the load frame whatever the stall. So the
drift moves no presented frame, and no oracle line can see it. The captures
show only how many 70 Hz capture frames the loader screen persists for, and
the classifiers collapse that to one distinct frame.

**Verdict: re-scoped.** It stays a named gap. Closing it needs a per-read
DOSBox-X trace: a breakpoint at `0x1B45F` logging `DS_00101508` for each
read of the first demo's state-6 entry. Even that pins the emulator's disk
timing, not the game's. The `res.c` comment now carries this evidence.

## §2 Row 14: `0x36F10`'s in-range arm and slot `+0x42` bit `0x10`

**Question.** `fight.c` said the `0x34BE8` arm (`fighter_36f10`) was
unreachable, because "slot `+0x42` bit `0x10` is set by no ported writer".
The demo-fight record §7.10 said "no writer sets slot `+0x42` bit `0x10`".
The brief asked to find every raw writer.

**Scan.** The scan covered every instruction whose destination is a memory
operand covering byte `+0x42` of a slot. That means three forms. The first
is base-relative, `[reg+d]` with `d <= 0x42 < d + size`, which catches word
and dword stores at `+0x40`/`+0x41`. The second is absolute, `0x1077F2` or
`0x107886`. The third is indexed, `[idx*4 + 0x1077F0/0x1077F2]`. Instructions
that cannot set the bit were dropped: `and`/`btr`, and immediates without
`0x10` in that byte. The functions scan found 51 writes covering the byte, 17
of which could set it. The gap sweep found 37 more.

**Every candidate** (a register source was resolved by reading its
producer):

| site | owner | value into byte `+0x42` | sets `0x10`? |
|---|---|---|---|
| `0x36E9D`/`0x36EAF` | `0x36E78` (slot: `[eax]` is the record, whose `+0x51` names the side) | `or edx,0x101000` then `and 0xFBFFF7FF`: byte 2 gets `0x10` | **yes** |
| `0x36FC8`/`0x36FD4` | `0x36F10` | word `or 0x820`: bit `0x20` | no |
| `0x37003..0x37012` | `0x36F10` (other slot) | dword `or 0x801000`: bit `0x80` | no |
| `0x14C37`, `0x148BD`, `0x21A01`, `0x3E0A4`, `0x3FBC8` | various | dword `or 0x48000`: bit `0x04` | no |
| `0x15B66`, `0x40F96` | various | `or 0xC1000`: `0x0C` | no |
| `0x246B9`, `0x24947`, `0x40DF7`, `0x46112`, `0x49282` | various | `or 0x81000`: `0x08` | no |
| `0x37123`, `0x37D32`, `0x3C3A3`, `0x14E75`, `0x151FC`, `0x153B0`, `0x3EC84`, `0x458C8`, `0x4890A` | various | `or 4` | no |
| `0x156B8`, `0x15944`, `0x23C95`, `0x48C26` | various | `or 8` | no |
| `0x376B3` | `0x37698`'s body | `or 0x80` | no |
| `0x3ABC8`/`0x3ABD3` | `0x3AAFC` | `or 2` / `or 1` | no |
| `0x3BB6F`/`0x3BB7C` | `0x3BAEC` | `or` of a zero-extended word: high half 0 | no |
| `0x1D789`, `0x39FF4` (`and 0xFBF7EFFF`), `0x36823` (`and 0xCCF7BFFF`), `0x159E0`/`0x24730`/`0x24994`/`0x40C8A`/`0x40E49`/`0x46161`/`0x492D1` (`and 0xFFF7EFFF`), `0x21012`/`0x362B4`/`0x44DEE` (`and 0xFB`), `0x4A6DD` (`and 0xFC`) | various | clears only | no |
| `0x48630` (`mov [esi+0x42],0x1E`) and `0x3C104` | `0x48608`, `0x3C0EC` | ESI/EAX are the fighter **record** (`[edx+0x51]` is its side; `[slot]` is loaded into EAX), not the slot | not a slot |
| `0x37CE2` (`xor 8`) | `0x37CD4` | toggles bit 3 of `[[eax+0x14]]` | no |
| `0x507A4`, `0x515E6/9`, `0x51FA7` | the VGA blits (host-owned) | not slots | no |

Two bulk copies also move the byte. `0x33ACC` saves the 0x94-byte slot
(`rep movsd`, ECX `0x25`, at `0x33AE9`), and `0x33B00` restores it
(`0x33B42`). They carry a saved bit. They do not create one.

**Result (raw wins).** `0x36E78` is the only raw setter of slot `+0x42` bit
`0x10`, at `0x36E9D`, stored at `0x36EAF`. It is reached from `0x33B00`
(`0x33BE1`) and `0x392A0` (`0x3961E`), each when the slot's `+0x5A >= 0x78`
and the mode is not 3. It has been ported since record §48 as `fighter_36e78`
(`fighter.c`), with the same `(x | 0x101000) & 0xFBFFF7FF`. So §7.10's "no
writer" and `fight.c`'s "no ported writer" are both wrong. The gate
`0x36E2C` can pass, and `0x34BE8`'s `0x36F10` call is reachable.

**Consequence.** The same false premise sat in `fighter_state_3531c`
(`0x3531C`). It claimed that `DS_001078F6`'s countdown arm was inert because
"`0x36F10` (the only writer of `DS_001078F6`) is unreachable", and it skipped
`0x353CF..0x353DD`:

```
353cf: test byte ptr [0x104529], 2
353d6: je 0x353e2
353d8: mov eax, dword ptr [0x1078ec]
353dd: call 0x2b150              ; retire the actor 0x36FE1 spawned
353e2: or dword ptr [ecx + 0x40], 0x801000
```

That retire is now ported. The actor is `DS_001078EC`, which `0x36FE1`
spawns under the same config bit. The call uses the ported `0x2B150`
(`actor_set_dead`). The two voices `0x353C0`/`0x353CA` stay "not wired"
(§45-A). The `FSET_104529` define moved up in `fighter.c` so that both users
share it.

**Tests** (`test_fight.c`, each assertion able to fail):

- In `check_gap_handlers` block G (`0x33B00` into `0x36E78`), the copied `+0x42` is
  seeded 0. The test asserts the slot's `+0x42 & 0x10 == 0x10`.
- `check_state_dispatch` A2 is the same gate with the fighter in range
  (`x - 0x3000 = 0`), a second slot, and its record's `+0x51 == DS_00104AD4`
  (arm A). It asserts four things:
  - `DS_001078F6 == 0x1A3`: set to `0x1A4` at `0x36FB9`, then counted down
    once by `0x35803`'s `0x3531C` in the same pass;
  - the other record's `+0x59 == 0xFF` (`0x36F5E`);
  - `+0x42` bit `0x20` (`0x36FD4`);
  - `+0x43` bit `0x40` clear (the out-of-range arm sets it).
- `check_state_machine` B2 tests the countdown's expiry. For 1 → 0 with the
  config bit set, `DS_001078EC` is retired (`+0x28` bit 3) and `+0x40` gets
  `0x801000`. With the bit clear there is no retire, but `+0x40` is still
  set. For 2 → 1, nothing happens.

**Reachability in runs.** A probe (an `fprintf` at `0x36E78`'s setter
branch, at `0x34BE8`, and at the expiry `0x353E2`, in a scratch build) hit
none of the three in a headless `--check 8000` run. The no-input attract and
its demos never reach `0x36E78`'s second branch. The arm is reachable in
the code; no run here measures a path that takes it. The
headless 8000-frame dump (8000 frames, `.ppm`/`.pal`/`.idx`, SHA-1 per file)
is byte-identical at HEAD `18d19d2`, after the change, and in the probe
build.

## §3 Row 21: the arena frame `0x263F4` re-audited

The static call closure of `0x263F4` was built from Ghidra's call graph
(`port/decomp/prage.calls.csv`, callees below `0x5D000`). It holds 315
functions. The 12 not ported are all classified rows of
`tools/port_classification.txt`. All sit under `res_resolve`'s (`0x1B544`)
allocator, fatal and storage paths, reached from `0x170A0`:

- `0x10D0C`, `0x10D34`, `0x1ADE4`, `0x1C308`, `0x1D290`, `0x1E30C`,
  `0x1E458`, `0x1E62C`, `0x1E774` and `0x4FB98` are host-owned;
- `0x1B084` and `0x2D498` are deferred.

The partial arms inside ported closure functions (`PORT:` notes) are the
following:

- the `0x2C3FC` voices, 18 sites (K12, §45-A);
- the `0x1CC28` slot choice (K7);
- `0x3B762`'s `0x62003` error stub;
- the `0x1C528` renderer's stub slot;
- the ones this task closed: `0x34BE8` (§2) and `0x353DD` (§2).

Indirect calls (actor callbacks, anim targets) are outside a static closure.
Their tables are the `fn_register`/`fn_resolve` sets that earlier records
audited. **Verdict: stale.** The `fight.h` header now states the audit.

## §4 Row 24: the slot `+0x24` health-sprite table (`0x33F08`)

- **Writer.** The spawn core does `0x33D53 mov al,[esi+0x7a]; 0x33D56 mov
  eax,[eax*4 + 0xBDA8C]; 0x33D5D mov [esi+0x24],eax`, which is
  `fighter_spawn_slot` (ported). No other raw store to a slot's `+0x24` was
  found: the only disp-`0x24` stores to the slot pair are in the spawn core.
- **Base.** `DS_000BDA8C`: seven fixed-up dwords, one per character, which
  are `0xE6638, 0xE31A8, 0xEC5B8, 0xD1BD8, 0xEA088, 0xD368C, 0xDFDF4`. The
  eighth dword, `0x00490049`, is not a pointer.
- **Stride and index.** `0x33F87..0x33FC4` computes `s = (pset word &
  0x7FFF) - C[char]`. `C` comes from the jump table `0x33EEC`: the words at
  `0xE6DD0, 0xE39D0, 0xECBD8, 0xD2134, 0xEA604, 0xD3E08, 0xE061C`, which are
  `0xEE4, 0x12A2, 0xBD4, 0x16B5, 0x1F9E, 0x2F19, 0x32D7` for characters 0..6.
  `0x33F9B`/`0x33FA0` then gate with `test dx,dx; jl`, `movsx`, `cmp 0x4B0;
  jle`, so s is in `[0, 0x4B0]`. The word is `[table + s*2]` (`0x33FB7`),
  stored zero-extended to the secondary record's `+8` (`0x33FC4`).
  Otherwise `0x1E1` (`0x33FAB`).
- **Values** (words, fixed-up image):

  | char | s = 1 | s = 0x4B0 |
  |---|---|---|
  | 0 | `0x1BEB` | `0x000E` |
  | 1 | `0x27E2` | `0xFF20` |
  | 2 | `0x24F6` | `0x0CAA` |
  | 3 | `0x1964` | `0xD500` |
  | 4 | `0x225C` | `0xDC00` |
  | 5 | `0x36EB` | `0xB840` |
  | 6 | `0x3A9F` | `0xFF20` |

  Some tables are shorter than `0x4B1` words. For example, character 0's
  constant sits at `0xE6DD0`, `0x798` bytes past its table. So the gate's
  top indices read whatever follows, as the raw does.
- **Port check.** `fight_health_bars` already matched: the raw's 16-bit
  signed test equals the port's 32-bit one, because `(w & 0x7FFF) - C`
  always fits an s16. The only missing piece was the table's provenance,
  which is the ported spawn.

**Test** `check_health_table` (`test_fight.c`, run with the shipped INDEX in
place): for each character 0..6, `ce_seed` spawns side 0 through the real
spawn. The test asserts `DS_000BDA8C[ch]` and the slot's `+0x24` (seeded
`0xDEADBEEF`) against the raw pointers. It then runs `fight_health_bars` at
s = 1, `0x4B0` and `0x4B1` (the secondary's `+8` seeded `0xDEADBEEF`) and
asserts the raw words and `0x1E1`. **Verdict: stale, closed.** The
`fight.h` comment is rewritten.

## §5 Row 25: `DS_00100B54` in `check_unfreeze`

**Question.** The test asserted only `B54 != 0` and `AF8 == B54`, because
the pose-freeze record §7.3 called B54 a named gap: `0x16DA4` was not
decoded then. `0x16DA4` has since been ported as `camera_winner_height`
(pose-freeze Task 2, demo-pose §26), so the value can now be derived.

**Writers.** B54 is written only in `0x16DA4`: `0x16DC8` (zero), `0x16F79`
(the accumulator), `0x1706F` (the scaled result) and `0x17580` (`0x175E4`,
zero).

**Derivation for the fixture** (cases B, C, D and G). Both actors use the
fake 8 x 8 sprite with eight all-on rows. The screen boxes are raw
`(0, 36, 2, 3)`, and the 0xFF prefill covers `0x100BAE`, `0x100B64` and
`0x100BD3`.

1. **`0x15C30` (box clip).** `box[0] = 0 << 2 = 0`, `box[1] = 36 * 3 = 108`,
   `box[2] = 2 << 2 = 8` and `box[3] = 3 * 3 = 9` (`0x15DA3..0x15DB4`). The
   sprite rect is `(0, 107)..(8, 115)`, with dx = dy = 0 (read from the
   ported `0x15C30` in a probe; the arithmetic below is the raw's). The y
   pass gives `accy = 108 >= 107`, so `box[1] = 108 - 107 = 1`. The bottom
   is `1 + 107 + 9 = 117 > 115`, so `box[3] = 9 - 2 = 7` (`0x15E3F..0x15E85`).
   The x pass leaves `box[0] = 0` and `box[2] = 8`.
2. **`0x181D0` (rows).** With both boxes equal, the rows sync gets p1 = p2 =
   7 and p3 = 0, so B18 = 7. In case G (box_o = 0) the call is p1 = 7,
   p2 = the sprite height 8 and p3 = `0xD56 + 1 - 0xD56 = 1`, which gives
   `p2 - p1 - p3 = 0` and B18 = 7 again.
3. **`0x16DA4` per row.**
   - `0x16F48..0x16F73` ANDs `0x100BAE` with `0x100BD3` over the decoded
     width only (EDI = width_a = 1 byte). The mask's byte 0 is `0xFF`
     (`0x15F48` fills `box[2] = 8` pixels).
   - The prefilled `0xFF` in bytes 1..36 of `0x100BAE` and `0x100B64`
     survives, because the row decode writes the decoded width only.
   - B38 = `|B14 - B34|` = 0. So `0x1617C` copies `0x100B64` into
     `0x100BF8` (`0x16FDE`, flag 0), and `0x16FE3..0x16FFF` ANDs all `0x25`
     bytes into `0x100B89`. All 37 are `0xFF`.
   - `0x17001..0x17025` sums the `0xA163C` popcount table (verified as the
     popcount of every byte; `[0xFF] = 8`) over `0x25` bytes, giving 296.
4. **Sum.** Over the 7 rows (`0x17027..0x17038`, while row < B18) the sum is
   2072.
5. **Scale.** `0x1703E`: `(2072 << 12) idiv 0xF3D = 2175`. `0x17051`:
   `(2175 << 12) idiv 0xD56 = 2609`. `0x17062..0x1706A` divides by 16 with
   a sign fixup, giving **163**.

`0x1756F` then copies it to `AF8[side]`.

**Tests** (`check_unfreeze`): `B54 == 163` in B, C, D and G, and `B18 == 7`
in B and G. The pre-existing `AF8 == B54` invariants stay. **Verdict:
closed.**

## §6 Row 27: the s16title read at boot vs at the title state

**Question.** §45-A.3 found that the original reads entry 7 (`s16title`) at
boot (the poll's f 0), whereas "the port" reads it at cycle 2's loop 1973.
The front-end driver pins that loader screen as a known divergence.

**The raw.** The only code immediates naming entry 7 (a handle with
`>> 23 == 7`) are two groups:

- the attract phase 2's `0x110D3..0x1113D`: `palette_acquire(0x396ED28)` at
  `0x110D8`, then `0x396ECE8`, `0x396EAE8` and `0x396E9E8`;
- `0x2C7A5`/`0x2C7BA` (`0x383B6F4`/`0x3837440`).

The data handle `0x03836102` (the title music records `0x54`/`0x56` in
`0xBBDC8`) is used by `0x111FF`, which is phase 4. Entry 7's INDEX flags are
`0x02`, not the preload bit `0x01` that `0x1B210` tests. So its first read is
a lazy `0x1B544` resolve, and the first one reached is phase 2's `0x110D8`.

**The port's real boot.** A probe of the headless boot (`prageport --check
3000`, an `fprintf` in `res_resolve`'s lazy branch) shows the port reading
entry 7 at f 3, in state 0, attract phase 2, at tick 0. That is the raw's
own `0x110D8` path in the boot attract. The same probe shows entries 0 and 8
read at f 691 in state 1, which matches the original's f 691 in state 1.
§45-A.3's "f 887, state 2" for those is also the driver's.

**The divergence is the driver's.** `test_frontend`'s `PR_FRONTEND_DUMP`
driver runs `game_init()` and then enters state 2 directly ("this driver
skips it by entering at state 2"), re-seeding the RNG, the frame counter and
the attract-cycle counter to the boot attract's post-state. It does not
re-seed the INDEX read bits, so entry 7 is still unread when cycle 2's
phase 2 runs (loop 1973 = `c2_start` 1971 + 2). One residue remains. The
original poll's "f 0" against the port's f 3 is not resolved, because the
poll log is not in the repo. Settling it needs a DOSBox-X breakpoint at
`0x1B5E9` that logs `DS_000EF6DC`.

**Why no fix here.** The faithful driver fix is to seed entry 7's `+0xC`
read bit, as the boot attract leaves it. That removes the loop-1973 screen
from the cycle-2 dump. `fe_cyc2_n` would go from 2308 to 2307, and every
later cycle-2 index would shift down by one. That moves `make verify`'s
printed `attract2: ... cycle-2 dump 2308 frames` line and the name-allowed
splice `cycle-2 2192/2193/2194` (`tools/title_compare.py`), both enforced
oracle lines. Per the brief this is a HALT, so the change is not made.

**Verdict: re-scoped.** The `test_game.c` comment now states the true cause,
the fix, and its oracle impact:

| line | old | new, if fixed |
|---|---|---|
| `fe_cyc2_n` / attract2 dump count | 2308 | 2307 |
| attract2 named splice | cycle-2 2192/2193/2194 | 2191/2192/2193 |
| `ld_loop[0]`/`ld_frame[0]` | 1973 / 168 | removed |
