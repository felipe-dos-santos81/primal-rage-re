# All-gaps ledger and baseline (Task 1 of `2026-09-29-all-gaps.md`)

Measured on branch `all-gaps` at `535596f` (2026-09-29). This ledger is the
work list for Tasks 2–6: they take their items from §B–§F, not from the
plan's snapshot table. Every row below was measured with the command named in
its section, or cites the raw address that proves it.

**Tooling note.** The Ghidra MCP bridge was not reachable in this session (no
tool exposed, `127.0.0.1:8080` refused). Data and code addresses were read from
a Python mirror of `mem_load_le` + `mem_load_le_fixups` (`port/src/mem.c`), so
every dword below has the LE fixups applied, which is what Ghidra shows. The
mirror reproduces the known table entries (`0x24B8C[0x1F] = 0x253D2`,
`[0x28] = 0x24F09`, `[0x2B] = 0x25187`; PROGRESS.md §49-J/§49-Q). Disassembly
used capstone on that image. Callers and callees come from
`port/decomp/prage.calls.csv` (Ghidra), and raw `call rel32` scans of the image.

## Corrections to the plan's snapshot (raw wins)

| Snapshot claim | Measured | Evidence |
|---|---|---|
| `0x34B6C` (541 B, 21 callees) is a large unported cluster | Its body is already ported as `fight_health_sync` (`fight.c:2066`). All 21 callees are ported. The counter misses it because the header is `/* ---- 0x34B6C the health sync` (`fight.c:2048`), which `port_progress.py`'s `/\* 0x` regex does not match. | `port_progress.py --unported` + `prage.calls.csv` (§B); `fight.c:2048/2066` |
| `0x501A3` (2944 B) is a "large audio/game-port candidate" | It is the VGA dirty-dword blit. `0x501B0 mov ebx,0xa0000`, and its only stores go through EBX, the aperture. Its callers are the master loop `0x255CC` and `0x2EA78`. | §B row, §F K9 |
| `0x50D23` (4405 B) is an audio/game-port candidate | It is the movie dirty-rectangle blit. `0x50D3E add edi,[0xe87a0]`, `0x50D44 add esi,[0xe87a4]` and `0x50D4A add ebx,0xa0000`. It stores through **both** EBX (the aperture) **and** EDI (the `DS_000E87A0` buffer in `mem[]`). Its only caller is the movie player `0x1C740`. | §B row |
| `game_frame`: "§47-B listed 39 unported, many since ported" | **All 52 table entries are wired** in `flow.c` `game_frame`'s switch. See §D. | §D |
| Prose named gaps: 28 comment sites in `port/src` | 32 in `port/src` + 3 in `port/tests` = 35, folded into 27 rows; 12 rows are wholly or partly stale. | §E |
| `menu.c`: "`0x2EA74` is `xor eax,eax; mov eax,eax`, a no-op" (`menu.c:172/282/294`) | **Wrong.** `0x2EA74` has no `ret`. Bytes `31 C0 8B C0` fall through into `0x2EA78` (`push ebx` …), so `0x2EA74` is `config_screen_wait(0)`: one presented frame, then two tick waits with the BIOS key drain. It has 18 raw `call` sites: `0x2D00C`, `0x2FA61`, `0x2FE2F`, `0x2FFF1`, `0x3074B`, `0x30B20`, `0x30B45`, `0x30E5E`, `0x31275`, `0x31290`, `0x313F5`, `0x324E1`, `0x32619`, `0x32E31`, `0x32F2D`, `0x33066`, `0x33242`, `0x33290`. | capstone at `0x2EA70..0x2EAB0` |

---

## §A Baseline oracle lines

The baseline is green. `make build && make verify` exited 0, and the full log is
`<scratchpad>/verify-baseline.txt`. `make verify` sets `PR_ORACLE_REQUIRED=1`
for the tests step. These lines are verbatim, and every later cycle must
reproduce them.

```
PR_ORACLE_REQUIRED=1 PR_GAME_DIR=data/game/C ./build/run_tests
oracle C-vs-Python: 9866 writes byte-exact
capture oracle first difference at C write 430: C tick=1010 reg=0xa0 val=0xd3 vs capture tick=1010 reg=0x1a8 val=0x68 (C 9866 writes, capture 6903 normalised)
all checks passed

smk_compare: 120/120 frames match
smk_compare: 41/41 frames match

title_compare: capture 1: 111 frames in window: 54 clean, 55 splice, 2 transition, 0 unexplained
title_compare: capture 1: port frames exhibited 95/96; missing [0]; endpoints OK
title_compare: capture 2: 111 frames in window: 54 clean, 57 splice, 0 transition, 0 unexplained
title_compare: capture 2: port frames exhibited 95/96; missing [0]; endpoints OK
title_compare: determinism: clean samples of 54 port frame(s) agree, 0 disagree

title_compare: frontend: window distinct [560..1884] (raw 3108..4791)
title_compare: frontend: 1325 frames in window: 517 clean, 801 splice, 3 transition, 2 unexplained
title_compare: frontend: 2 unexplained captured frame(s) allowed by name: [(832, 3671), (833, 3740)]; no other unexplained frame in the window.

== demo-fight oracle (ratchet on the first unexplained frame, N=1886) ==
title_compare: demo-fight: front-end window distinct [560..1884]; fight window empty: the front-end window reaches the first all-black capture frame 1885 (raw 4793)
title_compare: demo-fight: 0 unexplained in the fight window
title_compare: demo-fight: fully explained; the window claim is now exact

title_compare: attract2: front-end window distinct [560..1884]; cycle-2 region [1885..3616] (raw 4793..8409); cycle-2 dump 2308 frames
title_compare: attract2: 1732 frames in region: 1078 clean, 630 splice, 17 transition, 1 unexplained, 6 all-black
title_compare: attract2: captured frame 3545 (raw 8338) allowed by name as a three-frame splice: cycle-2 2192/2193/2194 at bytes 120000/172800 (rows 125/180)
title_compare: attract2: 0 unexplained in the region

attract_compare: data/title-captures/title: 215/216 capture frames explained; port attract frames exhibited 173/690
attract_compare: data/title-captures/title: expected divergence at capture frame 215
attract_compare: data/title-captures/title2: 215/216 capture frames explained; port attract frames exhibited 173/690
attract_compare: data/title-captures/title2: expected divergence at capture frame 215

python3 -m unittest tools.tests.test_title_compare   -> Ran 10 tests ... OK
python3 -m unittest tools.tests.test_gra_extract -v  -> Ran 33 tests ... OK
python3 tools/gen_symbols.py port/decomp port/src/symbols.h
1304 globals, 1206 functions -> port/src/symbols.h
all checks passed   (symbols.h byte-identical)
EXIT=0
```

| Gate | Value |
|---|---|
| demo-fight ratchet N | 1886 (Makefile). The fight window is empty and fully explained. |
| attract2 ratchet | 0 unexplained in region [1885..3616]. 3545 is allowed by name. |
| `CHECK`/`CHECK_EQ_INT` suite total | **12532** assertion sites: `rg -o '\bCHECK(_EQ_INT)?\(' port/tests -g '!test.h' \| wc -l`. Per file: audio 185, fight 10035, game 1477, platform 783, video 52. `run_tests` prints no executed-count line, only `all checks passed`, so the static site count is the gate. |
| `python3 tools/port_progress.py` | `745 1203 62` / `709 743 95 (portable: excludes 69 host-owned/deferred and runtime >= 5D000)` |

---

## §B Unported functions

`python3 tools/port_progress.py --unported` lists 34 non-runtime rows (plus 356
`runtime` rows ≥ `0x5D000`, which are not targets). Callers and callees below
come from `prage.calls.csv`. P = ported, U = unported, D = deferred, H =
host-owned. "Reach" says whether a ported live path calls the function.

**Caller evidence (fix round 1).** The callers column gives the counter's
`n_callers` (Ghidra's reference count, from `prage.functions.csv`) and then the
raw `call rel32` / `jmp rel32` / `jcc rel32` site count. The raw count comes
from a whole-code-object scan of the fixed-up image (`0x10000..0x73B14`); each
hit was confirmed by decoding it. An absolute-dword scan of the code and data
objects was also run. Sites are listed as `site@owner`. The owner is the Ghidra
function whose contiguous extent (`entry + size`) holds the site. **Callers not
reachable via Ghidra are raw-scan-derived**; a † marks those rows. They sit in
code Ghidra has no function for, or outside a function's contiguous extent. The
counter counts call sites, not caller functions. Where raw equals the counter,
every site is inside a Ghidra function. Where raw is larger, the extra sites
are the † ones. `0x2EA74` is the one row that does not reconcile site by site:
Ghidra attributes 5 references, and only 3 of the 18 raw sites fall inside
contiguous Ghidra extents. The other 2 Ghidra references are presumably in
non-contiguous function bodies, and the Ghidra MCP was not reachable to name
them.

### §B.1 Ghidra functions (the counter's denominator)

| addr | size | callers: counter / raw sites (site@owner) | callees | reach | class | cluster | what it is (raw) |
|---|---:|---|---|---|---|---|---|
| `0x50D23` | 4405 | 1 / 1: `1C82D`@`1C740`P | — | yes (movie player) | port (partial) / host-owned (aperture half): **own plan** | K10 | Movie dirty-rect blit. It stores to the aperture (EBX = `0xA0000`+off) and to the `DS_000E87A0` buffer (EDI), so the EDI half is `mem[]` state. |
| `0x501A3` | 2944 | 2 / 2: `256A5`@`255CC`P, `2EAD1`@`2EA78`P | — | yes | **closed: host-owned**, §K9.1 | K9 | Dirty-dword blit `E87A4` vs `E87A0` into `0xA0000` (`0x501B0`). All its stores go through EBX. `gfx_present` replaces it (`gfx.c:152`, `config.c:510`). |
| `0x34B6C` | 541 | 1 / 1: `357FC`@`35658`P | 21, all P | yes | **closed**: header, §K1.1 | K1 | Body is `fight_health_sync` (`fight.c:2066`). The header `/* ---- 0x34B6C` is not counted. |
| `0x51F72` | 404 | 3 / 3: `52119`, `52123`, `52151`, all @`52106`P | — | yes (boot/movie) | **closed**: `gfx_fill_screen`, §K2.4 | K2 | Unrolled 0xFA00-byte dword fill of `[EAX]` with EDX (`0x51F78..`). `0x52106` uses it on the two `mem[]` buffers and on the aperture; the aperture call stays `g_aperture` (`PORT:`, aperture rule). |
| `0x1CB18` | 271 | 1 / 1: `1CF26`@`1CF20`P | `1B544`P + AIL `5DC0F/2A/4D/70/A6/C5/E4` | no (no port path writes slot `+0x04`) | port — **derived, not ported** (Task 3d, §K7: needs the §K7.3 decisions; re-plan) | K7 | Sample start: copies the queued resource into the slot's `0x1D0BC` buffer, then AIL init/set/start. Named gap at `flow.c:5962`. |
| `0x1C528` | 190 | 1 / 1: `1C60B`@`1C5E8`P | `1B544`P | yes | **closed**: shared header, §K1.5 | K1 | A byte-identical copy of `0x14268`. It shares `sprite_node_build` (`sprite.c:55-58`), and the header names it mid-comment. |
| `0x2E180` | 151 | 1 / 1: `2E983`@`2E934`D | `2E034`U, `2E0A4`U | no (only via deferred `0x2E934`) | **closed: deferred**, §K9.6 | K9 | Audit counter add into the EEPROM image tables `0x2D420/0x2D45E/0x2D460`. |
| `0x31A78` | 137 | 0 / 4†: `321F2`, `32230`, `324A9`, `324DC` (non-Ghidra code after `31E28`P) | `3157C`P | via non-Ghidra code only | port | K11 | Jump-table dispatcher at `0x31A68`. Raw `call`s from `0x321F2`, `0x32230`, `0x324A9`, `0x324DC` (service-menu callback code). |
| `0x38910` | 125 | 1 / 1: `121F9`@`121A0`P | `4F1D0`P | yes | **closed**: header, §K1.3 | K1 | Ported as `title_origin_reset` under `/* PORT: 0x38910` (`flow.c:183`). |
| `0x2E0A4` | 117 | 1 / 1: `2E1EB`@`2E180`U | `2D4EC`P | no | **closed: deferred**, §K9.7 | K9 | Halves an EEPROM-image counter table and sets the dirty bit `DS_00105DD8`. Only via `0x2E180`. |
| `0x2E034` | 110 | 1 / 1: `2E20B`@`2E180`U | `2D4EC`P | no | **closed: deferred**, §K9.8 | K9 | Stores BL into an EEPROM-image counter and sets the dirty bit. Only via `0x2E180`. |
| `0x51ED8` | 109 | 1 / 1: `1C63D`@`1C5E8`P | `1B544`P | yes | **closed**: split `sprite_blit_aperture`, §K1.6 | K1 | Ported as `sprite_blit_at(n, base)` under `/* PORT: 0x51ED8` (`sprite.h:58`, `sprite.c:285`). |
| `0x2DF8C` | 109 | 1 / 1: `2D919`@`2D6F8`P | `2D4EC`P, `2E990`D, `61A70` rt | yes, but inert | **closed: deferred**, §K9.9 | K9 | Loops sides 3..5. Each effect goes through the deferred storage read `0x2E990` (§49-Y.5), the no-op `0x2D4EC` (`config.c:58`) or the runtime `0x61A70`. **Correction (§K9.9):** `0x61A70` is memset; on the port's path it zeroes `0x105ECD..0x105EFB`, which are zero in the image and have no ported writer, so it stays inert. `config.c:205` already calls it deferred. |
| `0x319B0` | 91 | 0 / 2†: `32207`, `32248` (non-Ghidra code after `31E28`P) | — | via non-Ghidra code only | port | K11 | Jump-table dispatcher at `0x319A0`. Raw `call`s from `0x32207` and `0x32248`. |
| `0x4F728` | 79 | 1 / 1: `27E4B`@`27DC8`P | `2C3FC`P | yes | **closed**: `sound_voice_match_end`, §K6 (wired) | K6 | Two voices: `0xDF` or `0x23` (gated on `DS_00104AD4`, `0x107813+rec`, `DS_001088F2`), then `0x22`. `flow.c:2762` PORT. |
| `0x4682C` | 78 | 1 / 1: `46AA0`@`469A8`P | `1A5D4`P | yes | **closed**: split `ai_pred_4682c`, §K1.4 | K1 | Shares `ai_pred_cmd_sign` with `0x467DC` (`fighter.c:1295`, header `/* 0x467DC / 0x4682C`). It needs its own function. |
| `0x4FF8F` | 73 | 4 / 8†: `1BD15`, `1BD2E`, `1BD3C`, `1BD8B` @`1BBAC`H; `1B976`, `1B9B6`, `1BA16`, `1BA96` in the unreferenced sampler routines `0x1B934..0x1BB73` | — | no | **closed: host-owned**, §K9.3 | K9 | Joystick A axis bits from `DS_000E1C1E/20/22/24`, ±0x1E. Every caller is in the host-owned sampler region `0x1B908..0x1BDCE` (see §G). |
| `0x4FFD8` | 73 | 2 / 4†: `1BDA4`, `1BDC9` @`1BBAC`H; `1BAD6`, `1BB36` in the unreferenced sampler routines `0x1B934..0x1BB73` | — | no | **closed: host-owned**, §K9.4 | K9 | Joystick B axis bits from `DS_000E1C26/28/2A/2C`. Callers as for `0x4FF8F` (see §G). |
| `0x4A868` | 63 | 6 / 6: `4A361`@`49C78`P, `4BFDE`@`4BF18`P, `4DF8E`, `4DFFA`, `4E066`, `4E0D0` @`4DEF4`P | `2BE00`P | yes (case-13/14 bodies, `0x4DEF4` states 1..4) | **closed**: `fight_4a868`, §K4; all 6 sites wired (`0x4A361` by Task 5b, §K13.2 of `2026-09-29-k13-fx74-derivations.md`) | K4 | Proximity gate `\|0x2BE00(rec) - entry+0x14\| <= 2*\|(rec+0x32)>>16\|`. Its absence is the named gap at `fight.c:4910`, `fight.h:75`. |
| `0x29C20` | 59 | 1 / 1†: `346B5` (update-table entry 8 `0x34648`, non-Ghidra) | — | via update entry 8 only | **closed: ported** (Task 3e, §K8c.2, `fighter_29c20`) | K8c | Reads `0xA8A98[i]` by `DS_00105B34[DS_001078FF]`. The raw `call` at `0x346B5` sits inside update-table entry 8, `0x34648` (non-Ghidra; registered in Task 3e as `fighter_34648`). |
| `0x38990` | 52 | 2 / 2: `24CC3`, `24CC8` @`24C5C`P | — | **yes, every frame** | **closed**: `render_scroll_track`, §K3 | K3 | `DS_00107A3C = word[0xF0AEC] & 0xFFC0`, `DS_00107A4A = dword[0xF0AEC]/64 + DS_00107A4E`. `game_frame` calls it at `0x24CC8`, and also at `0x24CC3` when `DS_00104B26 != 0`. `flow.c:6784` says "deferred". |
| `0x3BDB0` | 43 | 1 / 1: `3B27F`@`3B134`P | `33950`P | yes | **closed**: header, §K1.2 | K1 | Body is `fight_attack_ready` (`fight.c:1987`), header `/* ---- 0x3BDB0`. |
| `0x32BB0` | 41 | 1 / 1: `27E28`@`27DC8`P | `2DAE4`D | yes, inert | **closed: deferred**, §K9.12 | K9 | Two `0x2DAE4(0x1B+c, 1)` audit adds (`0x32BC0`, `0x32BD2`). Its only callee is deferred (record §48-V). |
| `0x2F464` | 38 | 1 / 5†: `30A12`, `30D59` (after `30788`P), `328E5`, `32913` (after `31E28`P), `33020` (after `32BB0`U) | `2EFD4`P, `2F198`P | via non-Ghidra code only | port | K11 | Raw `call`s from `0x30A12`, `0x30D59`, `0x328E5`, `0x32913` and `0x33020` (service-menu callbacks). |
| `0x33714` | 31 | 1 / 1: `13490`@`13420`P | — | yes | **closed**: split `palette_record_flagged`, §K1.7 | K1 | `palette_record` with flag 1 (`effects.c:63-64` PORT; `0x3371F mov byte [eax-4],1`). |
| `0x2D498` | 27 | 1 / 1: `2D612`@`2D4EC`P | `2EA68`P | no (the port's `0x2D4EC` is a declared no-op) | **closed: deferred**, §K9.10 | K9 | Bounds-checked byte store into the EEPROM image `0x100CE4..0x1014DC`, else `0x2EA68(0x80AA8)`. |
| `0x32B94` | 24 | 1 / 1: `2786B`@`277C0`P | `2DAE4`D | yes, inert | **closed: deferred**, §K9.11 | K9 | `test al,1` → `0x2DAE4(0xE, 1)` audit add. Its only callee is deferred (record §48-V). |
| `0x4F714` | 18 | 1 / 1†: `25C09` (non-Ghidra code after `25AE8`P; ported site `flow.c:1325`) | `2C3FC`P | yes (raw `call` `0x25C09`, ported site) | **closed**: `sound_voice_stage`, §K6 (wired) | K6 | `sound_voice(word[0xC9888 + 2*stage])` (a `jmp 0x2C3FC` tail). `flow.c:1325` PORT. |
| `0x2D4B4` | 17 | 4 / 4: `2D533`@`2D4EC`P, `2D8C8`@`2D6F8`P, `2DB11`, `2DB24` @`2DAE4`D | — | no (all three callers are no-op/deferred/unrun) | **closed**: `config_codeword_len`, §K2.3 | K2 | **Correction (§K2.3):** not a power of two. The loop jumps back to `inc eax` and returns EAX, so it is n + r + 1 for the smallest r with 2^r ≥ n + r + 1 (0x26 → 0x2D), pure. |
| `0x4FB98` | 10 | 2 / 2: `1BEB0`@`1BE30`P, `1BFEA`@`1BEC4`P | — | yes | **closed: host-owned**, §K9.2 | K9 | `pushad; and eax,0xff; int 10h; popad; ret`: the BIOS set-mode call. `flow.c:6371` PORT: SDL owns the window. |
| `0x2D62C` | 9 | 1 / 1†: `1BE28` (timer ISR `0x1BDF4`, non-Ghidra); abs dword at `0x1BF5C` (`push 0x2d62c`) | — | ISR only | **closed: host-owned**, §K9.5 | K9 | `inc dword [0x105D88]; ret`. **Correction (§K9.5):** `0x1BFA4` is a `push 0x105d88` for the `0x109A0` lock, not a read; the one reader is `0x32981` (`0x32970`, host-owned record §48-V, the run clock). `0x1BEC4` hands `0x2D62C` to `0x109A0` at `0x1BF5B` (`push 0x1000; push 0x2d62c`). |
| `0x2EA74` | 4 | 5 / 18†: `2FA61`, `2FE2F` @`2FA40`P, `2FFF1`@`2FFC4`P; `2D00C`, `3074B`, `30B20`, `30B45`, `30E5E`, `31275`, `31290`, `313F5`, `324E1`, `32619`, `32E31`, `32F2D`, `33066`, `33242`, `33290` outside contiguous Ghidra extents | — | yes (service menu) | **closed**: `config_screen_wait_zero`, §K5 | K5 | `xor eax,eax; mov eax,eax`, then falls into `0x2EA78`, so it is `config_screen_wait(0)`. The three ported sites (`menu.c`) now call it; the 15 others stay with the unported service-menu code. |
| `0x32968` | 1 | 1 / 1: `20CC7`@`20C10`P | — | yes (`game_init`) | **closed**: `game_init_null`, §K2.2 | K2 | A bare `ret`. |
| `0x29B70` | 1 | 4 / 4: `20E0B`@`20DF4`P, `2521A`, `25224`, `2522E` @`24C5C`P | — | yes | **closed**: `game_null_step`, §K2.1 | K2 | A bare `ret`: `game_frame` cases 1/2/0x20 and `0x20E0B`, all four sites now call it. |

Totals: 34 functions, 10,445 bytes. Classes: 7 header-only (K1), 15 port
(K2–K8c, K10, K11), 7 deferred and 5 host-owned proposed (K9, see §G).

**Task 3a (2026-09-29).** K1 and K9 are closed (record `2026-09-29-k1-k9-derivations.md`).
The 7 K1 rows carry one standard header each (3 of them split). All 12 K9
proposals were re-scanned against the raw and are supported; each has a
`tools/port_classification.txt` row. `port_progress.py`: `752 1203 63` /
`716 731 98`. The 15 remaining non-runtime rows are K2–K8c, K10 and K11.

**Task 5a (2026-09-29).** Record `2026-09-29-e-wire-k8b-k8d-derivations.md`. §E-3 wired (`0x2965F`, §W); K8b
entries 15/16 ported and registered (§B8); the K8d anim setters `0x37EA0`/
`0x24078`/`0x45D58` ported and registered (§D8), so entries 9/12/17 can now be
armed; `0x4F944`'s clamp is signed (§C). None is reached on a no-input path (probe,
§R); oracle lines, the 8000-frame and the front-end dumps are unchanged. All five
new addresses are non-Ghidra: `port_progress.py` stays `762 1203 63` / `726 731 99`.

**Task 3e (2026-09-29).** K8c is closed (record `2026-09-29-k8c-derivations.md`).
Update entries 4, 8, 9, 11, 12 and 17 and `0x29C20` are ported and registered
(`fighter.c`). Entry 4 has no setter in the image. Entries 8 and 11 have ported
setters that no no-input path reaches (probe, §K8c.0). The setters of 9, 12 and
17 are the unported animation targets `0x37EA0`/`0x24078`/`0x45D58` (§K8c.7).
`port_progress.py`: `762 1203 63` / `726 731 99`. The 8000-frame and front-end
dumps are byte-identical.

**Task 3d (2026-09-29).** K4 is closed (record `2026-09-29-k4-k6-k7-derivations.md`).
`0x4A868` is `fight_4a868`. Five of its six sites are wired: `0x4BFDE` (with the
gated body `0x4BFEB..0x4C034`, voice kept as `PORT:`) and the four `0x4DEF4` states.
`0x4A361` waits for K13. The 8000-frame and front-end dumps are byte-identical.

**Task 5b (2026-09-29).** K13 is closed (record `2026-09-29-k13-fx74-derivations.md`). The case-13 body
(`0x4A24A..0x4A345`, calling `0x496DC`), the case-14 body (`0x4A346..0x4A45B`,
wiring `0x4A361`), the frame locals and the mode-9 block (`0x4A476..0x4A590`,
calling `0x4A928`) are ported inline in `fight_effects_pass`. The type-8 held
body was already ported. A probe found no mode 9 and no type 9..14 on either
oracle path. `port_progress.py`: `762 1203 63` / `726 731 99` (no function
added). The 8000-frame and front-end dumps are byte-identical.
K6 is closed: `0x4F714`/`0x4F728` are `sound_voice_stage`/`sound_voice_match_end`,
wired at `0x25C09`/`0x27E4B` (case-1/5 voices only: no resource read, no draw).
K7 is derived but not ported: §K7.3 names three decisions beyond the AIL
boundary, and §K7.5 proposes the re-plan.

**Task 3c (2026-09-29).** K3 and K8a are closed (record `2026-09-29-k3-k8a-derivations.md`).
`0x38990` is `render_scroll_track`, called at both `game_frame` sites before the
frame counter. Update entry 6 `0x25FAC` is `flow_card_ramp_step`, registered in
`actors_init`. `port_progress.py`: `758 1203 63` / `722 731 99`. The 8000-frame
and front-end dumps are byte-identical. §K3.3 names the state change the oracles
cannot see: `DS_00107A38` during the demo fights.

### §B.2 Code the counter cannot see (non-Ghidra entry points reached through tables)

These are not in `symbols.h`, so closing them does not move the percentage.
They are still live gaps.

**Update process table `DS_000A8644`** (32 dwords, gated by the mask
`DS_00104AE8`, dispatched every frame by `run_process_table`, `flow.c:149`;
entries 3 and 18..29 are `0x5D812`). Unregistered entries are skipped silently.

| entry | bit | target | registered | reachability evidence |
|---:|---|---|---|---|
| 4 | `AE8` 0x10 | `0x37C8C` | **yes** (Task 3e, §K8c.1: `fighter_37c8c`) | **Dead.** No store in the image sets `AE8` 0x10 (every whole-mask store writes 0) and `DS_001078E0` has no writer. Cleared by itself at `0x37C9E`. |
| **6** | `AE8` 0x40 | **`0x25FAC`** | **yes** (Task 3c, §K8a: `flow_card_ramp_step`) | **Live.** Set by the ported `game_mode_05_step` (`0x25E1D`, `flow.c:2277`), `game_mode_30_step` (`0x294C9`, `flow.c:2398`) and `game_mode_23_step` (`0x26B2C`, `flow.c:5471`). It is skipped with no comment. |
| 8 | `AE9` 0x01 | `0x34648` (+ `0x29C20`) | **yes** (Task 3e, §K8c.2: `fighter_34648`, `fighter_29c20`) | Set by the ported stun start `0x34168` (`0x3426E`, `fighter_34168`). Cleared by ported `0x34038` (`0x340B0`) and `0x36E78` (`0x36F05`). Not armed on the no-input paths (probe, §K8c.0). |
| 9 | `AE9` 0x02 | `0x3800C` | **yes** (Task 3e, §K8c.3: `fighter_3800c`) | Set only by the animation target `0x37EA0` (`0x37FFC`; `0xD100` words in the 7 character streams), **ported and registered in Task 5a** (record §D8.1, `fighter_37ea0`). Not reached on the no-input paths (probe, §R). |
| 11 | `AE9` 0x08 | `0x4F890` | **yes** (Task 3e, §K8c.4: `fighter_4f890`) | **Correction:** set by the ported `0x4F944` (`or ah,8` `0x4F969`, store `0x4F973`), called by `fighter_1461c`, `fighter_14988`, `fighter_39040`. Not armed on the no-input paths (probe, §K8c.0). |
| 12 | `AE9` 0x10 | `0x24150` | **yes** (Task 3e, §K8c.5: `fighter_24150`) | Set only by the animation target `0x24078` (`0x24142`; the `0xD100` word at `0xE5006`), **ported and registered in Task 5a** (record §D8.2, `fighter_24078`). Not reached on the no-input paths (§R). |
| 15 | `AE9` 0x80 | `0x260BC` | **yes** (Task 5a, record §B8.1: `flow_bonus_card_a_step`) | Set by the ported `0x25FDC` (`0x26036`, `flow_round_bonus_a`). Not armed on the no-input paths (probe, §R). |
| 16 | `AEA` 0x01 | `0x26194` | **yes** (Task 5a, record §B8.2: `flow_bonus_card_b_step`) | Set by the ported `0x2604C` (`0x260A6`, `flow_round_bonus_b`; also from entry 15 at `0x2613E`). Not armed on the no-input paths (§R). |
| 17 | `AEA` 0x02 | `0x45D98` | **yes** (Task 3e, §K8c.6: `fighter_45d98`) | Set only by the animation target `0x45D58` (`0x45D7B`; the `0xD100` word at `0xEB892`), **ported and registered in Task 5a** (record §D8.3, `fighter_45d58`). Not reached on the no-input paths (§R). |

Registered (for completeness): entries 0 `0x1324C`, 1 `0x48F98`, 2 `0x19B90`,
5 `0x22FE8`, 7 `0x2910C`, 10 `0x28F08`, 13 `0x40554`, 14 `0x407EC`. The
per-character entrance table `DS_000A8628` (8 dwords) is fully registered.

**Service menu `0xBCBDC`/`0xBCC1C`** (stride 0x10, callback at `+8`):
`0x2CB74`, `0x2CB94`, `0x2CACC`, `0x2CAC0`, `0x30EB4`, `0x30F54`, `0x31F24`,
`0x19DF0`, `0x32358`, `0x30864` and `0x31138`. None is registered or has a header
(`menu.c:41-43` states the first four are skipped). The nested sub-menus they
open were not walked here.

---

## §C `TODO(verify)` list

`grep -rn 'TODO(verify)' port/src` finds **31** sites (they match the snapshot).

**Task 2 (2026-09-29).** All 31 sites are closed or re-scoped; `grep -rn
'TODO(verify)' port/src` now finds 0. The verdict and raw evidence for each row
are in `2026-09-29-todo-verify-derivations.md` §1–§31 (16 resolved as-is, 6
fixed, 2 named gaps, 7 `PORT:` reclassifications).

| # | file:line | question | evidence needed | note | Task 2 |
|---:|---|---|---|---|---|
| 1 | `host.c:26` | Which interrupt vector installs `0x2D62C`? | Already in hand: none. `0x2D62C` is called directly by the timer ISR `0x1BDF4` at `0x1BE28`, and `0x1BEC4` passes it to `0x109A0` at `0x1BF5B` with size `0x1000`. | Rewrite the comment. Pairs with the K9 host-owned row. | closed: resolved-as-is, record §1 |
| 2 | `host.c:282` | SDL audio-open failure branches are untested | A host-seam test that injects a NULL stream | Host only, with no raw counterpart | re-scoped: PORT (host), record §2 |
| 3 | `gfx.c:83` | Raw clamps with `+0xC` (flag word) and do-while runs a zero count | Raw `0x1C48B..0x1C4D6`, plus a scan of the shipped palette records for count 0 or first+`[+0xC]` > 256 | | closed: fixed, record §3 |
| 4 | `gra.c:93` | RLE sentinel meaning is documented, not proven | Raw decoder disassembly (the `0x51E5C` span path) | | closed: resolved-as-is, record §4 |
| 5 | `gra.c:104` | Which DAC bank or record a sprite selects | Raw `0x14268` / palette-acquire path | | closed: resolved-as-is, record §5 |
| 6 | `gra.c:151` | Meaning of the s16 x/y anchor | Raw `0x14268` xorg/yorg use | | closed: resolved-as-is, record §6 |
| 7 | `platform/audio/mixer.c:15` | Voice-exhaustion policy | DIG driver "Out of sample handles" path, or a game path that queues >4 | Same question as #9 | closed: resolved-as-is, record §7 |
| 8 | `platform/audio/ail.h:151` | Name of AIL row 21 (`0x5DD2C`) | AIL export or ordinal naming in the runtime | Deferred stub (movie audio) | re-scoped: PORT (name), record §8 |
| 9 | `platform/audio/mixer.h:15` | Same as #7 | same | | closed: resolved-as-is, record §9 |
| 10 | `platform/audio/ail.h:156` | Name of AIL row 22 (`0x5DD5D`) | as #8 | | re-scoped: PORT (name), record §10 |
| 11 | `platform/audio/ail.h:161` | Name of AIL row 23 (`0x5DD86`) | as #8 | | re-scoped: PORT (name), record §11 |
| 12 | `platform/audio/mixer.h:46` | `MIXER_OPL_RATE` is tied to the vendored core | Vendored core constant, not raw. A static assert would close it. | | closed: fixed (static assert), record §12 |
| 13 | `platform/audio/sequencer.c:264` | ctrl 64 → MDI `0x3b1e` sustain release | SBPRO2.MDI driver disassembly at `+0x3B1E` | | re-scoped: PORT named gap, record §13 |
| 14 | `platform/audio/sequencer.c:427` | RBRN loop range not reproduced | XMIDI RBRN semantics from the AIL XMIDI code, plus a capture past the loop | | closed: resolved-as-is (+ FOR/NEXT named gap), record §14 |
| 15 | `platform/audio/sequencer.h:22` | Tick rate vs the SBPRO2.MDI `+0x2E` field | Read the driver's `+0x2E` during init (`FUN_00065B7B`) | | closed: resolved-as-is, record §15 |
| 16 | `platform/audio/ail.c:367` | as #8 (definition) | as #8 | | re-scoped: PORT (name), record §16 |
| 17 | `platform/audio/ail.c:376` | as #10 (definition) | as #8 | | re-scoped: PORT (name), record §17 |
| 18 | `platform/audio/ail.c:383` | as #11 (definition) | as #8 | | re-scoped: PORT (name), record §18 |
| 19 | `platform/audio/ail.c:431` | Failed re-init of a playing sequence | AIL init-sequence decompilation (`0x5Dxxx`) | | closed: fixed, record §19 |
| 20 | `game/flow.c:1002` | Could an unregistered `DS_00104AE4` hook value reach `0x4F9A0`? | Enumerate every raw store to `DS_00104AE4` and check each one's registration | | closed: resolved-as-is, record §20 |
| 21 | `game/flow.c:5772` | `0x1CF40`'s `param_1`/`param_2` gates | The raw call site's arguments (shipped init) | Resolvable from raw | closed: resolved-as-is, record §21 |
| 22 | `game/flow.c:5828` | Sound-table id → handle mapping | Read `DS_000BBDC8` (stride 12) with fixups | Resolvable from raw (the mirror image) | closed: resolved-as-is, record §22 |
| 23 | `game/flow.c:5900` | Source of the loop flag | `0x1CC28`'s `+0x08` byte from `BBDC8` rec `+8` | Belongs to K7 | closed: fixed, record §23 |
| 24 | `game/flow.c:6417` | Credit countdown under input | A capture with coin input (none exists) | Likely stays a named gap | re-scoped: PORT named gap (capture), record §24 |
| 25 | `game/fight.c:1586` | Meanings of slot `+0x5A/+0x5D/+0x5E/+0x63` | Raw xrefs of the writers | | closed: resolved-as-is, record §25 |
| 26 | `game/config.c:112` | Raw EDX at `0x2DACA`/`0x2DAD4` | Disassembly of `0x2DA0C`'s loop | Moot while `0x2D4EC` is a no-op | closed: resolved-as-is (signature), record §26 |
| 27 | `game/config.c:200` | Three `0x2D4EC` calls omitted (`0x2D8A5`, `0x2D8AF`, `0x2D909`) | Raw sites | Add the no-op calls for fidelity | closed: resolved-as-is (call sites), record §27 |
| 28 | `game/fighter.c:155` | Character > 6 reads caller registers | Maximum character index reachable in `DS_0010816A` | Unreachability proof | closed: resolved-as-is, record §28 |
| 29 | `game/fighter.c:2297` | `DSD(0x1078DC)` indirection at `0x374E3` | Raw `0x374E3`. `0x36F10`'s caller is now wired (`fight.c:2082`). | The fix moves the `0x37464` test seeds | closed: fixed, record §29 |
| 30 | `game/fighter.c:3883` | Load order of slot `+0x2C` vs `0x2BC30` | `0x3C4B0` disassembly, and whether `0x2BC30` writes slot `+0x2C` | | closed: fixed, record §30 |
| 31 | `game/fighter.c:11005` | Immediate `0x1F874610` is not a handle | Raw `0x45B95`, plus the palette-acquire behaviour on an unresolved handle | | closed: resolved-as-is, record §31 |

---

## §D `game_frame` case table (jump table `0x24B8C`, 52 entries)

The table was read from the fixed-up image. Dispatch is at `0x24EF2 cmp ax,0x33;
ja 0x2540F; … jmp [eax*4+0x24B8C]` (`0x24F01`). Every case arm in `flow.c`
`game_frame` (`flow.c:6767`) was compared with its entry's first instructions.
**Result: 52/52 wired.** Every called target has a `/* 0xADDR` port header
except `0x29B70` (a bare `ret`, K2).

| mode | entry | raw body | ported | port owner |
|---|---|---|---|---|
| 0x00 | `0x2540F` | (tail) | Y | `case 0x00u:` (no-op) |
| 0x01 | `0x2521A` | `call 0x29B70` | Y (no `0x29B70` header) | inline `break` |
| 0x02 | `0x25224` | `call 0x29B70` | Y (no header) | inline `break` |
| 0x03 | `0x25238` | `call 0x11D04` | Y | `game_state_step` |
| 0x04 | `0x25242` | `call 0x26254` | Y | `game_mode_04_step` |
| 0x05 | `0x2524C` | `call 0x25C88` | Y | `game_mode_05_step` |
| 0x06 | `0x25256` | `B1D` gate, `0x28CC8`/`0x28DA4`, else case 4 | Y | `flow_join_poll`/`flow_player_join`/`game_mode_04_step` |
| 0x07 | `0x25273` | `call 0x282C4` | Y | `game_mode_07_step` |
| 0x08 | `0x25335` | `call 0x28468` | Y | `game_mode_08_step` |
| 0x09 | `0x2533F` | `call 0x28788` | Y | `game_mode_09_step` |
| 0x0A | `0x2527D` | `call 0x28BD4` | Y | `game_mode_0a_step` |
| 0x0B | `0x25287` | `call 0x26254; call 0x28C38` | Y | `game_mode_04_step` + `flow_winner_pose_step` |
| 0x0C | `0x25349` | `0x28CC8`/`0x28DA4`, else `0x27380` | Y | `flow_join_poll`/`flow_player_join`/`game_mode_0c_step` |
| 0x0D | `0x25367` | `call 0x274FC` | Y | `game_mode_0d_step` |
| 0x0E | `0x25371` | `call 0x27A2C` | Y | `game_mode_0e_step` |
| 0x0F | `0x2537B` | `call 0x277C0` | Y | `game_mode_0f_step` |
| 0x10 | `0x25385` | `call 0x438B4` | Y | `fight_mode_10_step` |
| 0x11 | `0x2538F` | inline: `AFE=0xF0`, `88EE=0`, hook `0x259CC`, mode 0x17 | Y | inline |
| 0x12 | `0x253BD` | `call 0x41C28` | Y | `game_mode_12_step` |
| 0x13 | `0x253C4` | `call 0x424E8` | Y | `game_mode_13_step` |
| 0x14 | `0x253D9` | `call 0x25AE8` | Y | `game_hook_25ae8` |
| 0x15 | `0x253E0` | `call 0x4F24C` | Y | `frontend_mode_15_step` |
| 0x16 | `0x253E7` | `call 0x4F2B0` | Y | `frontend_mode_16_step` |
| 0x17 | `0x253EE` | `call 0x4F318` | Y | `frontend_mode_17_step` |
| 0x18 | `0x253F5` | `call 0x4F6E8` | Y | `frontend_mode_18_step` |
| 0x19 | `0x253FC` | `call 0x4F704` | Y | `frontend_mode_19_step` |
| 0x1A | `0x25403` | `call 0x4F9A0` | Y | `frontend_mode_1a_step` |
| 0x1B | `0x2540A` | `call 0x4F9C8` (falls into the tail) | Y | `frontend_mode_1b_step` |
| 0x1C | `0x2540F` | (tail) | Y | no-op arm |
| 0x1D | `0x2540F` | (tail) | Y | no-op arm |
| 0x1E | `0x253CB` | `call 0x1EEB0` | Y | `game_mode_1e_step` |
| 0x1F | `0x253D2` | `call 0x208F8` | Y | `game_mode_1f_step` |
| 0x20 | `0x2522E` | `call 0x29B70` | Y (no header) | inline `break` |
| 0x21 | `0x25296` | `call 0x26540` | Y | `game_mode_21_step` |
| 0x22 | `0x25321` | `call 0x26C8C` | Y | `game_mode_22_step` |
| 0x23 | `0x25317` | `call 0x26A50` | Y | `game_mode_23_step` |
| 0x24 | `0x2532B` | `call 0x26F58` | Y | `game_mode_24_step` |
| 0x25 | `0x252A0` | inline `DS_00104B25` sub-state over `0x266AC`/`0x4EF8C`/`0x49C78`/`0x4F0FC` | Y | inline + `game_mode_25_*` |
| 0x26 | `0x2540F` | (tail) | Y | no-op arm |
| 0x27 | `0x251C6` | `0x50146`, `0x2FFC4(0xBCBDC)`, `0x4F644`, else `jmp 0x65431` (longjmp) | Y | inline. The longjmp is `PORT:` out of scope (spec §7). |
| 0x28 | `0x24F09` | inline game start | Y | `game_mode_28_step` |
| 0x29 | `0x24F66` | inline | Y | `game_mode_29_step` |
| 0x2A | `0x24FC4` | inline | Y | `game_mode_2a_step` |
| 0x2B | `0x25187` | inline | Y | `game_mode_2b_step` |
| 0x2C | `0x2501E` | inline | Y | `game_mode_2c_step` |
| 0x2D | `0x25071` | inline | Y | `game_mode_2d_step` |
| 0x2E | `0x250CE` | inline | Y | `game_mode_2e_step` |
| 0x2F | `0x2512B` | inline | Y | `game_mode_2f_step` |
| 0x30 | `0x2519E` | `call 0x29328` | Y | `game_mode_30_step` |
| 0x31 | `0x251A8` | `call 0x299E8` | Y | `game_mode_31_step` |
| 0x32 | `0x251B2` | `call 0x296B8` | Y | `game_mode_32_step` |
| 0x33 | `0x251BC` | `call 0x29638` | Y | `game_mode_33_step` |

Residue for Task 4 (none of these is a missing case):
- the `0x29B70` header (K2) — **closed** (Task 3b, §K2.1: `game_null_step`);
- the `0x38990` preamble calls at `0x24CC3`/`0x24CC8` (K3) — **closed** (Task 3c, §K3.2: both wired in `game_frame`);
- the `game_mode_33_step` → `0x4DEF4` call (§E-3);
- the case-0x27 longjmp, a spec-§7 deviation that stays;
- the stale "other cases are named gaps" comment at `flow.h:42` (§E-8).

No headless reachability run was done. Per the §47-B/§48-W notes, only mode 3
runs on the no-input oracle paths.

---

## §E Prose named gaps

`grep -rni 'named gap' port/src port/tests` finds 35 sites (32 in `port/src`, 3 in
`port/tests`). Each was cross-checked against the current tree and
`docs/PROGRESS.md`. **Stale** means the blocker is now ported: the comment is
wrong, and Task 6 fixes it.

| # | file:line | gap | blocker | status |
|---:|---|---|---|---|
| 1 | `platform/res.c:42` | Per-read tick model drifts by up to 4 ticks | Boot resource-read timing (record §45-A) | **re-scoped** (Task 5c, §1 of `2026-09-29-e-open-derivations.md`): the stall is the runtime's DOS I/O (`0x61C60`/`0x61CF6`/`0x61EC0`), with no duration in the raw; no reader uses the absolute tick (the gate `0x25643` is an equality, re-synced at `0x1B464`), so no frame or oracle line sees it. Closure needs a DOSBox-X per-read trace at `0x1B45F`. |
| 2 | `game/flow.c:2313` | Only registered character entrances run | `DS_000A8628` table | **stale**: all 8 entries are registered |
| 3 | `game/flow.c:2520`, `:2535` | `game_mode_33_step` does not call `0x4DEF4` | `0x4DEF4` | **closed** (Task 5a, record §W): `0x2965F` now calls `fight_effects_idle_pass` between the second `fight_hud_pass` and `camera_y_commit`; the named-gap comment is gone. No oracle path reaches mode 0x33 (§R). |
| 4 | `game/flow.c:2622`, `:2623` | Update entry 15 `0x260BC` (bonus card) | `0x260BC` (non-Ghidra) | **closed** (Task 5a, record §B8): entries 15 and 16 ported and registered; both `PORT:` notes in `flow_round_bonus_a/_b` rewritten. |
| 5 | `game/flow.c:5962` | `0x1CC28` slot choice and `0x1CB18` start | K7 | open; derived in §K7 (Task 3d), the comment cites §K7.3 |
| 6 | `game/flow.c:6168` | same as 5 (`snd_sample_queue`) | K7 | open; derived in §K7.2 (Task 3d) |
| 7 | `game/fighter.h:73` | `0x18540`/`0x18350` screen-anchor path | ported (`fighter.c:183/214`, called at `fighter.c:253/257`) | **stale** |
| 8 | `game/flow.h:42` | "the other cases are named gaps" | — | **stale** (§D: 52/52) |
| 9 | `game/fighter.h:89` | `0x494A8` dust entry and `res_resolve` tail | `0x494A8` is ported and called (`fighter.c:371`). The res_resolve tail was not re-measured. | **partly stale** |
| 10 | `game/flow.h:292` | Game-start modes `0x28..0x2F` are named gaps | ported (§49-Q) | **stale** |
| 11 | `game/flow.h:641` | "one of the fallthrough list's named gaps" (history) | — | **stale wording** |
| 12 | `game/fight.c:41` | "The port skipped `0x20DF4` as a named gap" | `0x20DF4` ported (`flow.c:4713`, §46-B) | **stale** |
| 13 | `game/fight.c:694` | `0x2E934` character-pick audit | K9 deferred (`0x2E180` cluster) | **closed** (Task 3a): the chain is classified deferred (§K9.6–§K9.8) and the comment now says deferred |
| 14 | `game/fight.c:2081` | `0x36F10` in-range arm unreachable: slot `+0x42` bit 0x10 has no ported writer | A writer of bit 0x10 (§7.10) | **stale, closed** (Task 5c, §2): the one raw setter is `0x36E78` (`0x36E9D or edx,0x101000`), which is ported, so the arm is reachable. The now-live `0x3531C` countdown retire `0x353CF..0x353DD` (`0x2B150` on `DS_001078EC`) is ported, and the comments in `fight.c` and `fighter.c` are rewritten. |
| 15 | `game/fight.c:2933`, `fight.h:241` | `0x496DC` has no call site | case-13 body `0x4A24A..0x4A2F4` (§7.4) | **closed** (Task 5b, §K13.1 of `2026-09-29-k13-fx74-derivations.md`): the case-13 body `0x4A24A..0x4A345` is ported and calls `fight_496dc` at `0x4A32B`; both headers rewritten. |
| 16 | `game/fight.c:3013`, `fight.h:245` | `0x4A928` has no call site | mode-9 block `0x4A487..0x4A58F` (§7.4) | **closed** (Task 5b, §K13.3): the mode-9 block `0x4A476..0x4A590` is ported and calls `fight_4a928` at `0x4A562`; both headers rewritten. |
| 17 | `game/fight.c:4490`, `:4510`, `:4825` | mode-9 frame locals and block | §7.4 | **closed** (Task 5b, §K13.3): all seven frame locals are modelled and the block reads them. One residue stays a named gap in the code. A drawn round (`DS_00104B16 == 2`) reads the word `[ESP+4]`, which case 3's landing writes (`0x49DF2`/`0x49DFE`, ported in fix round 1). Only its value before the first such write in a call is unpinned: it is uninitialised own-frame memory (0 in the port; pinning needs a runtime capture at `0x4A48E`). |
| 18 | `game/fight.c:4795` | case-13 body | §7.4 + `0x4A868` (K4) | **closed** (Task 5b, §K13.1/§K13.2): both bodies are ported, and `0x4A361` now calls `fight_4a868`. |
| 19 | `game/fight.c:4910`, `fight.h:75`, `fight.h:80` | `0x4DEF4` states 1..4 gated on `0x4A868`, treated as false | `0x4A868` (K4) | **closed** (Task 3d, §K4.3: the four state bodies are ported and wired). Task 5a (record §W): both raw callers of `0x4DEF4` (`0x277E9`, `0x2965F`) now call it, and the "not wired yet (ledger §E-3)" notes in `fight.c`/`fight.h`/`flow.h` are rewritten. The last unwired `0x4A868` site, `0x4A361`, was wired by Task 5b (§K13.2). |
| 20 | `game/fight.c:4931` | "`0x29638` (mode 0x33, still a named gap)" | `0x29638` ported | **stale** |
| 21 | `game/fight.h:17` | "everything else ported or a named gap" (arena frame) | not re-measured | **stale** (Task 5c, §3): 315 functions in `0x263F4`'s static closure; the 12 unported are classified host-owned/deferred rows under `0x1B544`. The header states the audit. |
| 22 | `game/fight.h:55` | type-8 held body, case-13/14 bodies, mode-9 block | §7.4 | **closed** (Task 5b): the bodies and the block are ported (§K13.1–§K13.3). The type-8 held body `0x4A08A..0x4A114` (139 B) was already ported (record §42-C, §K13.4); the header was stale. |
| 23 | `game/fight.h:234` | "type-0 processing (`0x4AAD0`) is a named gap" | `0x4AAD0` ported (`fight.c:2551`) | **closed** (Task 5b): the `fight.h` and `fight.c` (`fight_dust_build`) comments now name `fight_4aad0`. |
| 24 | `game/fight.h:299` | slot `+0x24` health-sprite table (§7.9) | not re-measured | **stale, closed** (Task 5c, §4): `DS_000BDA8C[char]` (7 pointers), stored by the ported spawn core at `0x33D5D`, indexed s in `[0, 0x4B0]`, 2 bytes per s. Pinned by `check_health_table`. |
| 25 | `tests/test_fight.c:539` | `DS_00100B54`'s value (§7.3) | not re-measured | **closed** (Task 5c, §5): B54 = 163 (7 rows x 37 x 8 = 2072, then `0x1703E..0x1706F`), with B18 = 7; asserted in `check_unfreeze` B/C/D/G. |
| 26 | `tests/test_game.c:1062` | "0x16 is a still-unported named gap" | `0x4F2B0` ported (§49-G) | **stale** (test comment) |
| 27 | `tests/test_game.c:4602` | s16title read at boot vs at the title state (§45-A) | boot resource order | **re-scoped** (Task 5c, §6): the port's real boot reads entry 7 at f 3 through the raw's `0x110D8` (attract phase 2). The loop-1973 screen is the front-end driver's, because it enters at state 2. The fix (seeding entry 7's read bit) moves the attract2 dump count 2308 -> 2307 and its named splice, both enforced, so it is not made. |

Rows 3/7/8/10/12/20/23/26 are stale, and so are parts of 2/9/11/19 (12 rows).
The 35 sites fold into 27 rows. The other gap prose that does not use the
phrase "named gap" is summarised here, and the §F cycles own it:

- `0x38990` "deferred" at `flow.c:6784` (K3) — **closed** (Task 3c, §K3: the comment is gone and the calls are wired);
- about 180 sites carry "not wired (record §45-A)"
  (`rg -c 'not wired' port/src`: fighter.c 64, flow.c 47, fight.c 32,
  nameentry.c 18, attract.c 8, actors.c 6, others 5). 124 of them have the
  exact form `PORT: 0xADDR 0x2C3FC` (K12);
- the menu `0x2EA74` "no-op" claim (K5) — **closed** (Task 3b, §K5: the three `menu.c` sites call `config_screen_wait_zero`).

---

## §F Cycle list (ordered)

Order: (a) unblocks other gaps, then (b) smallest raw-evidence risk first. The
size gate is ≥ ~4 KB or ≥ ~20 new functions, and such a cluster is marked
**own plan**.

| order | id | addresses | bytes | kind | owner |
|---:|---|---|---:|---|---|
| 1 | T2 | the 31 `TODO(verify)` sites (§C) | — | resolve or re-scope | Task 2 |
| 2 | K1 HDR | `0x34B6C`, `0x3BDB0`, `0x1C528`, `0x4682C`, `0x38910`, `0x33714`, `0x51ED8` | 1117 | port (header / one-function split only, no behaviour change; +7 on the counter) | Task 3 — **closed** (Task 3a, §K1) |
| 3 | K9 CLASSIFY | host-owned: `0x501A3`, `0x4FB98`, `0x4FF8F`, `0x4FFD8`, `0x2D62C`. Deferred: `0x2E180`, `0x2E0A4`, `0x2E034`, `0x2DF8C`, `0x2D498`, `0x32B94`, `0x32BB0`. | 3688 | host-owned/deferred rows in `tools/port_classification.txt` plus derivation rows. **Needs the user's approval (§G).** | Task 3 — **closed** (Task 3a, §K9; 12/12 supported) |
| 4 | K2 TRIV | `0x29B70`, `0x32968`, `0x2D4B4`, `0x51F72` | 423 | port | Task 3 — **closed** (Task 3b, §K2 of `2026-09-29-k2-k5-derivations.md`) |
| 5 | K5 MENU-FRAME | `0x2EA74` (and its 3 ported call sites in `menu.c`) | 4 | port (raw conflict fix) | Task 3 — **closed** (Task 3b, §K5 of `2026-09-29-k2-k5-derivations.md`) |
| 6 | K3 FRAME-SVC | `0x38990` (called at `0x24CC3`/`0x24CC8`) | 52 | port, **live every frame**. Re-run `demo-fight-oracle attract2-oracle` and diff the dumps. | Task 3 — **closed** (Task 3c, §K3 of `2026-09-29-k3-k8a-derivations.md`) |
| 7 | K8a UPD-06 | update entry 6 `0x25FAC` | n/a (non-Ghidra) | port (live: armed by modes 5/0x23/0x30) | Task 3 — **closed** (Task 3c, §K8a) |
| 8 | K4 FX-GATE | `0x4A868` | 63 | port (unblocks §E-18/19 and K13) | Task 3 — **closed** (Task 3d, §K4 of `2026-09-29-k4-k6-k7-derivations.md`) |
| 9 | K6 VOICE-WRAP | `0x4F714`, `0x4F728` | 97 | port (wrappers over the ported `sound_voice`) | Task 3 — **closed** (Task 3d, §K6) |
| 10 | K7 AUDIO-SMP | `0x1CB18` + `0x1CC28`'s slot choice (`0x1CC62..0x1CD8D`) | 271+ | port | Task 3 — **derived, re-plan needed** (Task 3d, §K7.3/§K7.5: the `+0x10` buffers of the host-owned `0x1D0BC`, the `0x500BB` clock, the announcer stand-in) |
| 11 | K8c UPD-REST | update entries 4 `0x37C8C`, 8 `0x34648` (+`0x29C20`, 59 B), 9 `0x3800C`, 11 `0x4F890`, 12 `0x24150`, 17 `0x45D98` | 59 + n/a | port. Reachability first: find each bit's setter. | Task 3 — **closed** (Task 3e, §K8c; setters `0x37EA0`/`0x24078`/`0x45D58` left as named gaps §K8c.7) |
| 12 | T4 | §D residue (the `0x29B70` header, the `0x38990` calls) | — | confirmation cycle only. All 52 cases are wired. | Task 4 |
| 13 | E-WIRE | §E-3 (`0x2965F` → `fight_effects_idle_pass`) and §E-19 (the `0x4DEF4` gate once K4 lands; **done** in Task 3d, §K4.3) | — | wiring | Task 5 — **closed** (Task 5a, record §W) |
| 14 | K13 FX-7.4 | case-13 body `0x4A24A..0x4A2F4` (170 B), case-14 body `0x4A346..0x4A412` (204 B), mode-9 block `0x4A487..0x4A58F` (264 B), type-8 held body (not measured) | ≥638 | port (inline blocks of `0x49C78`) | Task 5 — **closed** (Task 5b, `2026-09-29-k13-fx74-derivations.md`). Measured: case 13 `0x4A24A..0x4A345` (252 B), case 14 `0x4A346..0x4A45B` (278 B), shared tail `0x4A45C..0x4A467`, mode-9 block `0x4A476..0x4A590` (266 B from `0x4A487`), type 8 `0x4A08A..0x4A114` (139 B, already ported). |
| 15 | K8b UPD-BONUS | update entries 15 `0x260BC` and 16 `0x26194` | n/a | port (§E-4) | Task 5 — **closed** (Task 5a, record §B8; plus the K8d setters `0x37EA0`/`0x24078`/`0x45D58`, record §D8) |
| 16 | E-OPEN | §E-1, 14, 21, 24, 25, 27 | — | derive or re-scope | Task 5 |
| 17 | K10 MOVIE-BLIT | `0x50D23` | 4405 | **own plan**: the EDI `mem[]` half is port, the aperture half is host-owned | Task 6 |
| 18 | K11 MENU-CB | `0x2F464`, `0x319B0`, `0x31A78` + 11 non-Ghidra service-menu callbacks (§B.2) and their sub-menus | 266 + n/a | **own plan** (the callback code spans `0x2D00C..0x33290` in the raw `call` sites) | Task 6 |
| 19 | K12 VOICE-WIRE | about 180 "not wired (record §45-A)" sites | — | **own plan** (≥ 20 sites). Needs proof of no RNG/render effect per site. | Task 6 |
| 20 | STALE | §E rows 2, 3, 7, 8, 9, 10, 11, 12, 19, 20, 23, 26 | — | comment fixes | Task 6 |

Every one of the 34 §B.1 functions is in exactly one cluster: K1 7, K2 4, K3 1,
K4 1, K5 1, K6 2, K7 1, K8c 1, K9 12, K10 1, K11 3.

---

## §G Classifications for the user (one-way scope decisions)

**Task 3a (2026-09-29): done.** The scan was redone and matches site by site; all 12 rows are supported and written with tags `record-§K9.1..§K9.12` (record `2026-09-29-k1-k9-derivations.md`). Two evidence texts below were corrected there: `0x2DF8C` (`0x61A70` is memset, which is inert on the port's path, §K9.9) and `0x2D62C` (`0x1BFA4` is a lock push, §K9.5).

**Task 3 must redo the raw caller scan for each row before writing any `tools/port_classification.txt` line.** Scan the fixed-up image for rel32 call/jmp/jcc sites and absolute-dword references. The evidence lines below carry the placeholder tag `record-§TBD-K9`, to be replaced by the derivation-record section that Task 3 writes.

**For the controller to relay.** These are proposals. Each is closed only by a
`tools/port_classification.txt` row plus a derivation-record row naming the
port's replacement (plan Global Constraints). Until approved they stay in §B as
unported.

**host-owned (5)**

| addr | evidence line (proposed) |
|---|---|
| `0x501A3` | `record-§TBD-K9`: Dirty-dword blit to the VGA aperture: `0x501B0 mov ebx,0xa0000`, and every store goes through EBX. Its callers `0x255CC`/`0x2EA78` present through `gfx_present` (aperture rule). |
| `0x4FB98` | `record-§TBD-K9`: `int 10h` set-mode (`pushad; and eax,0xff; int 0x10; popad; ret`). SDL owns the window (`flow.c:6371`). |
| `0x4FF8F` | `record-§TBD-K9`: Joystick A axis bits from `DS_000E1C1E..24`. Every caller is in the ISR/game-port sampler region `0x1B908..0x1BDCE`. There are 8 raw sites. `0x1BD15`, `0x1BD2E`, `0x1BD3C` and `0x1BD8B` are in the host-owned ISR sampler `0x1BBAC` (reached through its jump table at `0x1BB74`, dispatched at `0x1BD06`). `0x1B976`, `0x1B9B6`, `0x1BA16` and `0x1BA96` are in the sampler routines `0x1B934..0x1BB73`. Those routines have no raw reference (no rel32 call/jmp/jcc into the range, no absolute dword equal to a routine start). Their only callees are the host-owned `0x1B610`/`0x1B6A0`/`0x1B730`/`0x1B7C0`/`0x1B890` (§50-C) and `0x4FF8F`/`0x4FFD8`, and they only store the key bitmap `[DS_00101514]+0x2D8/0x2D9`. |
| `0x4FFD8` | `record-§TBD-K9`: Joystick B axis bits from `DS_000E1C26..2C`. Every caller is in the ISR/game-port sampler region `0x1B908..0x1BDCE`. There are 4 raw sites. `0x1BDA4` and `0x1BDC9` are in `0x1BBAC` (host-owned). `0x1BAD6` and `0x1BB36` are in the unreferenced sampler routines `0x1B934..0x1BB73` (same evidence as `0x4FF8F`). |
| `0x2D62C` | `record-§TBD-K9`: ISR tick `inc dword [0x105D88]`. It is called only from the timer ISR `0x1BDF4` (`0x1BE28`). Its counter is read only by the `0x1BEC4` init (`0x1BFA4`) and the host-owned `0x32970` (`0x32982`, record §48-V). |

**deferred (7)** (EEPROM/audit storage, the same layer as record §48-V/§49-Y.5)

| addr | evidence line (proposed) |
|---|---|
| `0x2E180` | `record-§TBD-K9`: Audit add into the EEPROM-image counters. Its only caller is the deferred `0x2E934` (record §48-V), at the single raw site `0x2E983`. |
| `0x2E0A4` | `record-§TBD-K9`: Counter-table halve plus dirty bit `DS_00105DD8`. Its only caller is `0x2E180`, at the single raw site `0x2E1EB`. |
| `0x2E034` | `record-§TBD-K9`: Counter store plus dirty bit. Its only caller is `0x2E180`, at the single raw site `0x2E20B`. |
| `0x2DF8C` | `record-§TBD-K9`: High-score storage validate. Every effect goes through the deferred `0x2E990` (record §49-Y.5), the declared no-op `0x2D4EC` or the runtime `0x61A70`. |
| `0x2D498` | `record-§TBD-K9`: Bounds-checked byte store into the EEPROM image `0x100CE4..0x1014DC`. Its only caller is `0x2D4EC` (single raw site `0x2D612`), a declared no-op (`config.c:58`). |
| `0x32B94` | `record-§TBD-K9`: `0x2DAE4(0xE,1)` when AL bit 0 is set. Its only callee is the deferred `0x2DAE4` (record §48-V). |
| `0x32BB0` | `record-§TBD-K9`: Two `0x2DAE4(0x1B+c,1)` audit adds. Its only callee is the deferred `0x2DAE4` (record §48-V). |

The cost if one of these is wrong: the function stays unported until its one
classification row is reverted.

Explicitly **not** classified host-owned, and why:
- `0x51F72`: two of its three uses fill `mem[]` buffers.
- `0x50D23`: it stores the `E87A0` front buffer in `mem[]` through EDI.
