# Music Truth — derivations

Status: in progress, 2026-10-25. Branch `music-truth`, base `a7876ffc` (`docs: close the
proof completion phase`). Plan `docs/superpowers/plans/2026-10-25-music-truth.md`; spec
`docs/superpowers/specs/2026-10-25-music-truth-design.md`.

The record is the authoritative raw-byte source for the tasks that implement from it. §M1
is Task 1 (the baseline and the music-path derivation) and is research only: no port code
changed. Later tasks append §M2 (the game-side resolve), §M3 (the played song), §M4 (the
register-stream oracle), §M5 (PCM determinism) and §M6 (the docs and the full gate).

Conventions: an address is a Ghidra linear address (code `0x10000..0x73B14`, data
`0x80000..0x10B0CF`, `DSB/DSW/DSD(addr)` in the port's flat `mem[]`); a resource handle is
`(INDEX index << 23) | offset` (`res_handle`, `res.c`); a capture `.dro` path is relative to
the repo root. **On any plan-vs-raw conflict the raw wins**; the corrections are recorded
in the section that measures them.

## §M1 Baseline and the music path (Task 1)

### §M1.1 The measured baseline

#### §M1.1.1 `python3 tools/port_progress.py`

```
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)
```

The phase's Task 1 changes no port code, so the raw/portable counts are exactly the base
commit's. `799` real functions have a `/* 0xADDR` header or `fn_register`; `759` of the
`759` portable ones are done. The 404 unported are the classified 53 and the 351 runtime
functions.

#### §M1.1.2 `make diff-verify`

Run on this tree (`make diff-verify`, exit 0). The unittest gate first:

```
Ran 234 tests in 149.047s

OK
```

and the differential summary's last line:

```
diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps;
825/845 rows with callees closed (188 have none); 807/845 closed on full rows;
8 ported functions with no row (45 FN_ ported-unrowed: 8 portable [5 registry-visible + 3
header-shape exceptions] + 37 runtime; the scan-visible subset is 42 = 5 + 37). ...
```

That is the base commit's counter exactly; the record cites it so Task 2 can detect a
movement. The 8 `UNROWED` functions include `0x1C930` and `0x1CF40` (the two the phase's
boundary records cover); §M1.5 records their boundary.

#### §M1.1.3 `make audio-render`

`make audio-render` (`AUDIO_WAV=/tmp/pr_title_fm.wav`, `AUDIO_SECONDS=12`) ran the whole
unit suite and printed:

```
oracle C-vs-Python: 9866 writes byte-exact
capture oracle first difference at C write 430: C tick=1010 reg=0xa0 val=0xd3 vs capture tick=1010 reg=0x1a8 val=0x68 (C 9866 writes, capture 6903 normalised)
PR_AUDIO_WAV: wrote /tmp/pr_title_fm.wav (12 s, 49716 Hz)
...
all checks passed
```

- The **9866-write** line is the governing title oracle: the port's C register stream
  equals `tools/opl_seq.py`'s byte for byte (tick, register, value and order) for the whole
  S16TITLE bank over the test's 4096-tick cap (`test_audio.c:940-982`; the line is printed
  by `test_audio.c:982`).
- The **capture comparison is report-only** (`test_audio.c:1037-1199`): it anchors both
  streams at their first key-on, maps capture ms → port tick at 120 Hz, drops the
  documented-excluded registers (`0xBD`; carrier TL only on the residual MIDI channels 1
  and 4) and write-on-change repeats, then prints the first difference and does not fail.
  The capture is `data/audio-captures/prage_000.dro` (`CAPTURE_DRO`,
  `test_audio.c:496`): 7,201 register writes over 28,117 ms of writes (62,935 ms header),
  6,903 after the normalisation. The first difference is write 430, `0xA0` vs `0x1A8`, a
  channel/slot attribution difference in the same tick.
- The **render** writes `/tmp/pr_title_fm.wav`: 2,386,412 bytes, a 44-byte RIFF/WAVE header,
  PCM 16-bit stereo at 49,716 Hz, 596,592 frames. Measured: peak sample 3,402 (of 32,767),
  1,122,762 of 1,193,184 samples non-zero, sha256
  `df74acfb65d345fb72cb214102089f2a0ab8d4b271ddc17e2a5f5c4f1a380844`. This is the PCM
  baseline §M5 must not move for the title and must beat for the new song.

`PR_AUDIO_WAV=/nonexistent-dir/pr_title_fm.wav ./build/run_tests` fails with exactly one
assertion, `test_audio.c:1218: PR_AUDIO_WAV: cannot open output file` (`FAILURES: 1`):
the render block's only `CHECK` is the `fopen`. A silent WAV therefore passes (the block
never reads the samples it wrote; see §M1.7).

### §M1.2 The port's current audio path and the zero return

The raw music chain has three layers, all ported or host-owned:

1. `0x2C3FC` (`sound_voice`, `flow.c:6631`) — the voice dispatcher. Case 1 stores the
   record's handle in `DS_00105D5C` and calls `0x1CA14` (`0x2C463`); case 4 stores `0x21`
   and calls `0x1CA14` with `0x2803E64` (`0x2C8B6`).
2. `0x1CA14` (`snd_music_request`, `flow.c:6300-6308`) — stores the song byte at
   `DS_001028D9` (`0x1CA14`), the handle at `DS_001028D4` (`0x1CA22`), and, unless the
   music is paused (`DS_001028DA == 1`, `0x1CA27`) **or `DS_001028C0 == 0` (`0x1CA2C`)**,
   stores the handle as the pending song `DS_001028CC` (`0x1CA35`) and returns AL = 1.
3. `0x1CF20` (`game_audio_service`, `flow.c:6201`) — per frame: four `0x1CB18` slot starts,
   then, if `DS_001028CC != 0` (`0x1CF30`), calls `0x1C930` (`0x1CF39`), which resolves and
   starts the song.

The port's `snd_music_request` is byte-faithful: it stores `D9` and `D4` and tests `DA`.
It then tests `DSD(DS_001028C0) == 0` and returns 0 (`flow.c:6306`). **In the original
`DS_001028C0` is the sequence handle `0x1CF40` allocates at `0x1CFE8`**, so it is non-zero
whenever the MDI install succeeds and the gate never fires. The port's `game_audio_init`
(`flow.c:6065`) allocates `s_sequence` (`flow.c:6103`) but deliberately stores neither it
nor the MDI handle (`flow.c:6086-6093`, record 2026-10-14-reverse-n §N.2.2: "the handles
the raw stores in `mem[]` are host objects in the port"), so `DS_001028C0` stays 0 and
**every in-match/menu request returns 0 and is dropped**. That is the whole functional
hole: the request is precise and the caller ignores the return, so the song is never
pending; the pending dword is never set.

Only the title path bypasses `0x1CA14`: `game_state_title` sets the C flag `s_music_request`
(`flow.c:4532`) and `title_music_start` (`flow.c:6181`) loads the S16TITLE bank in place via
`game_music_bank_find` and calls `AIL_init_sequence`/`AIL_set_sequence_volume(DS_000A2CB8)`/
`AIL_start_sequence` (the raw's `0x5DE48`/`0x5DECA`/`0x5DE79` sequence). It is the working
reference (9866 writes, §M1.1.3), not the general path.

The relevant original state (all in `mem[]`, none shadowed):

| addr | meaning | writers/readers |
|---|---|---|
| `DS_001028C0` | MDI **sequence** handle | `0x1CFE8` (allocate), `0x1CA2C`/`0x1CA40`/`0x1CA86`/`0x1CAD2`/`0x1D1E1` (gates), `0x1D0BC` (clears on a failed buffer) |
| `DS_001028C4` | MDI driver handle | `0x1CFD6` (allocate), read `0x1D0CC`, cleared `0x1D113` |
| `DS_001028C8` | DIG driver handle | `0x1CF8E` (port stores 1), every sample path |
| `DS_001028CC` | pending song (handle) | `0x1CA35`, cleared `0x1CA08`/`0x1CAA1`/`0x1D0BC`/`0x1D1E7` |
| `DS_001028D0` | music copy buffer (0x5100 bytes) | allocated `0x1D0E8`, written `0x1C970`/`0x1C977`, read `0x1C97C`/`0x1C9C7` |
| `DS_001028D4` | current song handle | `0x1CA22`, cleared `0x1CA7A` |
| `DS_001028D9` | current song byte | `0x1CA14`, cleared `0x1CA80` |
| `DS_001028DC` | last started song | `0x1C943`, compared `0x1C93B` |
| `DS_000A2CB8` | music volume | `0x1CACB`, read `0x1C997`/`0x1C9E2`/`0x1CAF6`; image default `0x7F` |
| `DS_001028DA` | music paused | `0x1D1C2`/`0x1D213` |

### §M1.3 The raw `0x1C930` (re-derived from the fixup-applied image)

`0x1C930`, size 228 (`0xE4`), 1 caller, 5 callees (`port/decomp/prage.c:8311`). The caller
is `0x1CF39` inside `0x1CF20` (`port/decomp/prage.c:8674`), whose guard is
`cmp dword ptr [0x1028cc], 0; je 0x1cf3e` (`0x1CF30/0x1CF37`).

The disassembly (capstone over `mem_load_le` + `mem_load_le_fixups`' image; bytes in the
second column):

```
01C930  53                     push ebx / ecx / edx / esi / edi / ebp
01C936  a1cc281000             mov eax, [0x1028CC]          ; the pending song
01C93B  3b05dc281000           cmp eax, [0x1028DC]          ; same as the last started?
01C941  7473                   je 0x1C9B6
01C943  a3dc281000             mov [0x1028DC], eax          ; remember it
01C948  e8f7ebffff             call 0x1B544                 ; res_resolve(handle)
01C94D  8b15c0281000           mov edx, [0x1028C0]          ; sequence handle
01C953  52                     push edx
01C954  89c6                   mov esi, eax
01C956  e854150400             call 0x5DEAF                 ; AIL_stop_sequence(seq)
01C95B  83c604                 add esi, 4                   ; skip the size dword
01C95E  83c404                 add esp, 4
01C961  8b3dd0281000           mov edi, [0x1028D0]          ; the 0x5100 buffer
01C967  8b4efc                 mov ecx, [esi-4]             ; the resource's size dword
01C96A  57                     push edi
01C96B  89c8                   mov eax, ecx
01C96D  c1e902                 shr ecx, 2
01C970  f2a5                   movsd                        ; dword copy
01C972  8ac8                   mov cl, al
01C974  80e103                 and cl, 3
01C977  f2a4                   repne movsb                  ; byte tail
01C979  5f                     pop edi
01C97A  6a00                   push 0
01C97C  8b1dd0281000           mov ebx, [0x1028D0]          ; AIL_init_sequence(seq, buf, 0)
01C982  53                     push ebx
01C983  8b0dc0281000           mov ecx, [0x1028C0]
01C989  51                     push ecx
01C98A  e8b9140400             call 0x5DE48
01C98F  83c40c                 add esp, 0xC
01C992  68f4010000             push 0x1F4                   ; 500 ms fade
01C997  8b35b82c0a00           mov esi, [0xA2CB8]           ; the music volume
01C99D  56                     push esi
01C99E  8b3dc0281000           mov edi, [0x1028C0]
01C9A4  57                     push edi
01C9A5  e820150400             call 0x5DECA                 ; AIL_set_sequence_volume
01C9AA  83c40c                 add esp, 0xC
01C9AD  8b2dc0281000           mov ebp, [0x1028C0]
01C9B3  55                     push ebp
01C9B4  eb48                   jmp 0x1C9FE
01C9B6  8b1dc0281000           mov ebx, [0x1028C0]          ; the same-song arm
01C9BC  53                     push ebx
01C9BD  e8ed140400             call 0x5DEAF                 ; stop
01C9C2  83c404                 add esp, 4
01C9C5  6a00                   push 0
01C9C7  8b0dd0281000           mov ecx, [0x1028D0]          ; re-init the copied buffer
01C9CD  51                     push ecx
01C9CE  8b35c0281000           mov esi, [0x1028C0]
01C9D4  56                     push esi
01C9D5  e86e140400             call 0x5DE48
01C9DA  83c40c                 add esp, 0xC
01C9DD  68f4010000             push 0x1F4
01C9E2  8b3db82c0a00           mov edi, [0xA2CB8]
01C9E8  57                     push edi
01C9E9  8b2dc0281000           mov ebp, [0x1028C0]
01C9EF  55                     push ebp
01C9F0  e8d5140400             call 0x5DECA                 ; volume
01C9F5  83c40c                 add esp, 0xC
01C9F8  a1c0281000             mov eax, [0x1028C0]          ; fall into the start
01C9FD  50                     push eax
01C9FE  e876140400             call 0x5DE79                 ; AIL_start_sequence(seq)
01CA03  83c404                 add esp, 4
01CA06  31c0                   xor eax, eax
01CA08  a3cc281000             mov [0x1028CC], eax          ; the pending song is consumed
01CA0D  5d                     pop ebp / edi / esi / edx / ecx / ebx / ret
```

The two arms:
- **new song** (`[0x1028CC] != [0x1028DC]`): store the last song, resolve the handle with
  `0x1B544`, stop the sequence, copy the resource's size-prefixed bytes (`[esi-4]` is the
  size dword; the payload starts at `+4`) into `[0x1028D0]`, `AIL_init_sequence(seq, buf, 0)`,
  `AIL_set_sequence_volume(seq, [0xA2CB8], 500)`, start.
- **same song** (`je`): stop, `AIL_init_sequence` the *already copied* buffer, volume, start.
  No store past `0x1028DC`, no resolve, no copy.

Both arms clear `DS_001028CC` at `0x1CA08`. `0x1C930` returns nothing (EAX is zeroed).

**The song-id source.** `DS_001028CC` is written by exactly one other instruction:
`0x1CA35` in `0x1CA14` (the raw addresses are in §M1.2; `snd_music_request`). That
function's two call sites are both in the dispatcher: `0x2C463` (case 1, with
`EAX = [0x105D5C]`, the record's `+4` handle) and `0x2C8B6` (case 4's restart,
`EAX = 0x2803E64`). The handle is a resource handle: `res_resolve` (`0x1B544`,
`res.c:300`) returns `mem + entry[+0x10] + (handle & 0x7FFFFF)`. The resolved pointer's
first dword is the payload size (the same `[esi-4]` the copy reads). `0x1C930` therefore
needs: the pending handle, the resource layer, the 0x5100 buffer, and the host sequence.

**The buffer bound.** `DS_001028D0` is allocated by `0x1D0BC` (`sound_buffers_alloc`,
`flow.c:6416`) as `res_block_alloc(0x5100)` and zeroed (`0x1D0E8..0x1D107`). All 26
distinct case-1 banks are smaller: min 540, max 20,400, total 97,660 bytes (§M1.4.1), so
the copy cannot overflow the buffer on the shipped assets. (The raw has no bound check;
the port's implementation must keep the resource size as the copy length and can rely on
the same maximum, or guard it, but must not invent a different buffer size.)

### §M1.4 The song/voice tables

#### §M1.4.1 `DS_000BBDC8`, the 12-byte voice records

`0x2C3FC` indexes `DS_000BBDC8 + id * 12` (`0x2C412..0x2C41B`): `+0` case, `+4` handle,
`+8` byte (the request's `DL`). The shipped image's 0x100 records (`0..0xFF`; an id of
`0x100` folds to 0 at `0x2C409`) hold: 5 case 0, **29 case 1 (music requests)**, 154 case 2
(samples), 3 case 3 (paired samples), 1 case 4 (restart), 18 case 5 (stops), 34 case 6
(nothing), and 12 records whose case byte is 27, 30, 33, 37, 40, 46, 59, 189, 211, 216,
223 or 235 — all above 6, so the dispatcher's `ja` default returns 0 (`0x2C8E8`). The 29 case-1
records, decoded from the fixup-applied image and the INDEX/`.GRA` bytes (id, record
handle, resource, offset, size dword, payload FORM type):

| id | handle | resource | offset | size | payload |
|---:|---|---|---:|---:|---|
| `0x1B` | `0x0D000008` | `s16jungl.gra` (26) | `0x08` | 4,012 | FORM XDIR |
| `0x1C` | `0x0D800008` | `s16stone.gra` (27) | `0x08` | 1,674 | FORM XDIR |
| `0x1D` | `0x0B800008` | `s16citys.gra` (23) | `0x08` | 7,640 | FORM XDIR |
| `0x1E` | `0x0C800008` | `s16himal.gra` (25) | `0x08` | 4,876 | FORM XDIR |
| `0x1F` | `0x0B000008` | `s16caves.gra` (22) | `0x08` | 12,902 | FORM XDIR |
| `0x20` | `0x0A800008` | `s16beach.gra` (21) | `0x08` | 20,400 | FORM XDIR |
| `0x21` | `0x02803E64` | `s16sound.gra` (5) | `0x3E64` | 7,456 | FORM XDIR |
| `0x23` | `0x02805B88` | `s16sound.gra` (5) | `0x5B88` | 4,378 | FORM XDIR |
| `0x24` | `0x02806CA8` | `s16sound.gra` (5) | `0x6CA8` | 540 | FORM XDIR |
| `0x25` | `0x0C001A10` | `s16grave.gra` (24) | `0x1A10` | 4,068 | FORM XDIR |
| `0x26` | `0x0C000A8C` | `s16grave.gra` (24) | `0x0A8C` | 3,966 | FORM XDIR |
| `0x27` | `0x0C0005E4` | `s16grave.gra` (24) | `0x05E4` | 1,188 | FORM XDIR |
| `0x28` | `0x02800008` | `s16sound.gra` (5) | `0x08` | 3,618 | FORM XDIR |
| `0x2A` | `0x0C000008` | `s16grave.gra` (24) | `0x08` | 1,496 | FORM XDIR |
| `0x2C` | `0x09000008` | `s16conti.gra` (18) | `0x08` | 570 | FORM XDIR |
| `0x2E` | `0x0980025C` | `s16selmo.gra` (19) | `0x025C` | 1,920 | FORM XDIR |
| `0x30` | `0x0980025C` | `s16selmo.gra` (19) | `0x025C` | 1,920 | FORM XDIR |
| `0x31` | `0x09800008` | `s16selmo.gra` (19) | `0x08` | 590 | FORM XDIR |
| `0x32` | `0x03021BFA` | `s16snd2.gra` (6) | `0x21BFA` | 2,388 | FORM XDIR |
| `0x3B` | `0x0280335E` | `s16sound.gra` (5) | `0x335E` | 1,682 | FORM XDIR |
| `0x3D` | `0x028039F4` | `s16sound.gra` (5) | `0x39F4` | 1,132 | FORM XDIR |
| `0x54` | `0x03836102` | `s16title.gra` (7) | `0x36102` | 4,920 | FORM XDIR |
| `0x56` | `0x03836102` | `s16title.gra` (7) | `0x36102` | 4,920 | FORM XDIR |
| `0xDF` | `0x02806EC8` | `s16sound.gra` (5) | `0x6EC8` | 1,032 | FORM XDIR |
| `0xE1` | `0x0A000008` | `s16hghsc.gra` (20) | `0x08` | 2,606 | FORM XDIR |
| `0xE3` | `0x0A000008` | `s16hghsc.gra` (20) | `0x08` | 2,606 | FORM XDIR |
| `0xE4` | `0x02807714` | `s16sound.gra` (5) | `0x7714` | 946 | FORM XDIR |
| `0xEB` | `0x1B01CCB0` | `s16konsd.gra` (54) | `0x1CCB0` | 578 | FORM XDIR |
| `0xEC` | `0x028072D4` | `s16sound.gra` (5) | `0x72D4` | 1,082 | FORM XDIR |

**26 distinct banks**: `0x54`/`0x56` share one (the title/attract bank), `0xE1`/`0xE3` one
(`s16hghsc`), and `0x2E`/`0x30` one (`s16selmo` +0x25C). The payloads are the
size-prefixed XMIDI blobs `0x1C930` copies: a `u32` size then a `FORM ... XMID` container
(the first bytes are `FORM`, and the XDIR/XMID structure is the same one
`game_music_bank_find` and `tools/opl_seq.py` parse).

The **title bank** (`0x54`/`0x56`): index 7 = `s16title.gra` (1,505,988 bytes), offset
`0x36102`; the size dword at `0x36102` is `0x1338` (4,920); the XMID payload starts at
`0x36106` (`FORM XDIR`), and its `FORM XMID` chunk is at `0x36128`. The port's
`title_music_bank` scans for the first FORM/XMID in the resource; the only one is at
`0x36128`, inside the XDIR's CAT, so the port sequences the same song the raw copies
(the 9866-write oracle proves it).

#### §M1.4.2 The stage table `DS_000C9888`

`sound_voice_stage` (`0x4F714`, `flow.c:6731`) is a tail jump into `0x2C3FC` with the
stage's word: `mov ax, [eax*2 + 0xC9888]; and eax, 0xFFFF; jmp 0x2C3FC` (`0x4F71C`). Its
one caller is `0x25C09` in `game_hook_25bbc` (`0x25BBC`, `flow.c:1395`), the match entry
that also spawns both fighters. The shipped table's words 0..7 are:

```
stage 0 -> 0x20   stage 1 -> 0x21   stage 2 -> 0x1B   stage 3 -> 0x1C
stage 4 -> 0x1E   stage 5 -> 0x1D   stage 6 -> 0x1F   stage 7 -> 0x1F
```

i.e. the six arena banks (`s16beach`, `s16sound+3E64`, `s16jungl`, `s16stone`, `s16himal`,
`s16citys`) plus `s16caves` for stages 6/7. **The port already makes this request**:
`flow.c:1404` calls `sound_voice_stage(DSW(DS_00104AFC))` at the same address. The request
reaches `0x1CA14` and returns 0 only because of `DS_001028C0` (§M1.2). So Task 2 is not
missing the selection: the game-side request exists and is faithful; the resolve/start is
what is absent.

The other music call sites the captures can reach (from the port's wired calls; each is a
case-1 record):
- `0x2E`/`0x30` — the character screen / select (`fight_char_screen_open`,
  `fight_char_screen_open_both`, `fight_hook_4367c`; `fight.c:342`, `:375`, `:1210`,
  `:1250`, `flow.c:989`).
- `0x31` — `fight_hook_430e8` (`fight.c:1135`).
- `0x32` — `fight_hook_4142c` (`fight.c:1310`).
- `0x2C` — `game_mode_13_step` case 0 (`flow.c:3754`).
- `0x3D` — `game_hook_25ae8` (`flow.c:4105`).
- `0x3B` — `game_mode_1f_step` case 0 (`flow.c:5504`).
- `0xE1`/`0xE3` — the name-entry/high-score modes (`flow.c`'s mode 0x1E arms).
- `0x24`, `0x25`, `0x27`, `0x28`, `0x2A` — the front-end/attract modes
  (`flow.c:1436-2582`).
- `0xDF`/`0x23` — the match end (`sound_voice_match_end`, `0x4F728`, `flow.c:6742`).
- `0x54`/`0x56` — the attract's `0x111FF` (`attract.c:295`), the title reference.
- `0xEC`/`0xEB`/`0xE4` — fighter-state sounds (`fighter.c`).

#### §M1.4.3 The `.GRA` extraction (the method the later tasks reuse)

For a record `handle = (index << 23) | offset`, the payload is
`GRA_file[index][offset+4 .. offset+4+size)`, where `size` is the little-endian u32 at
`offset`, and `GRA_file` is `INDEX[index].name` (the first 12 bytes of the 20-byte record,
`res.c:295-300`). All 29 records pass the `FORM`/`XDIR` check and the 26 distinct payloads
are 540..20,400 bytes. This is the same mapping `res_resolve` performs at runtime; it is
recorded here because the oracle (§M4) will need to name a song by its bank bytes without
running the port.

### §M1.5 `0x1CF40` and the AIL boundary (why the row is not possible)

`0x1CF40`, size 215, 1 caller (`0x1BEC4`, which passes AL = DL = 1), 10 distinct callees
(`port/decomp/prage.calls.csv`): `0x5D851`, `0x5D87E` (called 4 times), `0x5DB7C`,
`0x5DBCB`, `0x5DC0F`, `0x5DDD0`, `0x5DE1F`, `0x5DA12`, `0x5DA87`, `0x5DAA6`. The
disassembly (full in the 2026-10-14-reverse-n record §N.2.2) is:

- `0x1CF4D call 0x5D851` (AIL startup), `0x1CF58 [0xA2CB1] = 1`;
- with DL != 0: three `0x5D87E` preference calls, `0x5DB7C` (DIG install) into
  `[0x1028C8]`, then four `0x5DBCB`/`0x5DC0F` pairs into `[0x102860 + 0x18*i]`;
- with AL != 0: `0x5D87E(0xB,1)`, `0x5DDD0` (MDI install) into `[0x1028C4]`, then
  `0x5DE1F(mdi)` into **`[0x1028C0]` at `0x1CFE8`** (the sequence handle);
- `0x5DA12(0x1BDF4)` (register timer), `0x5DA87(timer, 0x3C)`, `0x5DAA6(timer)` (start).

The port's `game_audio_init` (`flow.c:6065`) makes the same host calls and stores the DIG
stand-in `1` at `DS_001028C8` (`0x1CF8E`) but **stores nothing at `0x1028C0`/`0x1028C4`**
(they are host objects `s_sequence`/samples). It also adds `mixer_reset`, the `0xA2CB4`
store and the FAT.OPL load. That is §N.2.2's outcome 1 and the reason the function stays
`UNROWED`: a row would pass only by masking the differing stores, or by a DIG stub
answering a *fitted* non-zero handle. The same is true of `0x1C930` (§N.2.3): every block
of its two arms contains a store or call the port does not make (the pending/last gate,
the resolve, the stop, the copy, the clear), so there is no reached subset that
corresponds to the port, and the raw is not reached by the port's own run either (it
never sets `0x1028CC`).

**The phase keeps the boundary:** `0x1C930` and `0x1CF40` stay in `UNROWED`; the new
game-side resolve is a C function with the `/* 0x1C930 — §M2 */` header and a `PORT:` note
naming the host hand-off. A differential row is not fabricated. The measured boundary is
the one above; if Task 2 finds a faithful subset (e.g. a code-poked row that stops at the
first AIL call and compares the pending/song stores), it must record the pokes and the
counter movement, not call the AIL handle verified.

### §M1.6 The `.dro` inventory

#### §M1.6.1 The measured count and the plan's correction

The plan and spec say "the 37 gameplay `.dro` traces". The measured tree:

- `data/k11-captures/**`: **37** `.dro` files. **29** are under `gp-*` capture directories;
  the other **8** are the service-menu/save/diagnostic captures
  (`de`, `diags`, `idle` ×2, `menuesc`, `save-reload`, `walk` ×2).
- `data/audio-captures/`: **2** more (the title capture the report-only test reads:
  `prage_000.dro` 7,201 writes / 28,117 ms of writes, `prage_001.dro` 1,052 / 9,433).
- Total `.dro` under `data/`: **39**.

So "37 gameplay traces" is a miscount: 37 is the `k11-captures` total, of which 29 are
gameplay. The oracle's inventory is the 29 `gp-*` files; the other 10 (8 + 2) are not
gameplay music and are named separately so a song absent from the 29 is a named gap, not
a silent skip. Three of the 29 `gp-*` files (`gp-pads/prage_000`, `gp-pads/prage_001`,
`gp-u17-device2/prage_000`) hold only the boot fragment (below) and carry no in-match
music; 26 of the 29 have more than 1,000 writes.

#### §M1.6.2 Every `.dro`

Columns: header `milliseconds` (the capture's accumulated delay, including trailing
silence), the register-write count, the last write's tick, the first key-on (tick, register,
value), the first key-on **after a write gap > 1000 ms** (the first music after the boot),
the melodic key-on count (`0xB0..0xB8` with bit `0x20`), and the poll log's last `ms` minus
the `.dro`'s last write tick (a clock offset, **not** an alignment; see §M1.6.5).

| `.dro` | header ms | writes | last write (ms) | first key-on | first music key-on | key-ons | poll E − last write (ms) |
|---|---:|---:|---:|---|---|---:|---:|
| `data/k11-captures/de/prage_000.dro` | 248 | 185 | 248 | `0/0xb0=0x2b` | — | 4 | 32071 |
| `data/k11-captures/diags/prage_000.dro` | 248 | 185 | 248 | `0/0xb0=0x2b` | — | 4 | 55023 |
| `data/k11-captures/gp-idle-loss-run2/prage_000.dro` | 176156 | 38514 | 176156 | `0/0xb0=0x2b` | `7157/0xb4=0x2b` | 1941 | 24271 |
| `data/k11-captures/gp-idle-loss/prage_000.dro` | 176302 | 38499 | 176302 | `0/0xb0=0x2b` | `7256/0xb4=0x2b` | 1913 | 24180 |
| `data/k11-captures/gp-keys-fight/prage_000.dro` | 66144 | 7170 | 66144 | `0/0xb0=0x2b` | `7157/0xb4=0x2b` | 349 | 24221 |
| `data/k11-captures/gp-pads/prage_000.dro` | 50951 | 185 | 248 | `0/0xb0=0x2b` | — | 4 | 75134 |
| `data/k11-captures/gp-pads/prage_001.dro` | 248 | 189 | 248 | `0/0xb4=0x2b` | — | 4 | 75134 |
| `data/k11-captures/gp-twop/prage_000.dro` | 23075 | 3274 | 23075 | `0/0xb0=0x2b` | `7032/0xb4=0x2b` | 179 | 24224 |
| `data/k11-captures/gp-u10-ending/prage_000.dro` | 165454 | 31208 | 165454 | `0/0xb0=0x2b` | `7033/0xb4=0x2b` | 1557 | 39828 |
| `data/k11-captures/gp-u12-endurance/prage_000.dro` | 30311 | 4521 | 30311 | `0/0xb0=0x2b` | `10684/0xb4=0x2b` | 220 | 24169 |
| `data/k11-captures/gp-u13-ending2/prage_000.dro` | 166388 | 31132 | 166388 | `0/0xb0=0x2b` | `7000/0xb4=0x2b` | 1502 | 39992 |
| `data/k11-captures/gp-u15-join/prage_000.dro` | 32234 | 5135 | 32234 | `0/0xb0=0x2b` | `7257/0xb4=0x2b` | 263 | 26013 |
| `data/k11-captures/gp-u16-twop2/prage_000.dro` | 141993 | 33216 | 141993 | `0/0xb0=0x2b` | `7157/0xb4=0x2b` | 1570 | 24222 |
| `data/k11-captures/gp-u17-device2/prage_000.dro` | 248 | 185 | 248 | `0/0xb0=0x2b` | — | 4 | 54932 |
| `data/k11-captures/gp-u18-talon-ending/prage_000.dro` | 165423 | 30256 | 165423 | `0/0xb0=0x2b` | `7100/0xb4=0x2b` | 1492 | 39604 |
| `data/k11-captures/gp-u19-vertigo-ending/prage_000.dro` | 166379 | 31346 | 166379 | `0/0xb0=0x2b` | `7101/0xb4=0x2b` | 1555 | 39647 |
| `data/k11-captures/gp-u20-chaos-ending/prage_000.dro` | 167306 | 31252 | 167306 | `0/0xb0=0x2b` | `7032/0xb4=0x2b` | 1541 | 39720 |
| `data/k11-captures/gp-u21-armadon-ending/prage_000.dro` | 167359 | 31320 | 167359 | `0/0xb0=0x2b` | `6716/0xb4=0x2b` | 1557 | 40456 |
| `data/k11-captures/gp-u22-diablo-ending/prage_000.dro` | 167751 | 31830 | 167751 | `0/0xb0=0x2b` | `6049/0xb1=0x2b` | 1600 | 42570 |
| `data/k11-captures/gp-u23-u16-finisher/prage_000.dro` | 93533 | 19403 | 93533 | `0/0xb0=0x2b` | `7372/0xb4=0x2b` | 939 | 29171 |
| `data/k11-captures/gp-u5-charsel/prage_000.dro` | 36190 | 6645 | 36190 | `0/0xb0=0x2b` | `7156/0xb4=0x2b` | 333 | 24130 |
| `data/k11-captures/gp-u6-moves-b/prage_000.dro` | 51578 | 11942 | 51578 | `0/0xb0=0x2b` | `7206/0xb4=0x2b` | 593 | 24873 |
| `data/k11-captures/gp-u6-moves/prage_000.dro` | 106210 | 29128 | 106210 | `0/0xb0=0x2b` | `7184/0xb4=0x2b` | 1437 | 26556 |
| `data/k11-captures/gp-u8-attract-start/prage_000.dro` | 31765 | 6820 | 31765 | `0/0xb0=0x2b` | `1684/0xb4=0x2b` | 356 | 24534 |
| `data/k11-captures/gp-u8-endurance/prage_000.dro` | 16673 | 1628 | 16673 | `0/0xb0=0x2b` | `10825/0xb4=0x2b` | 85 | 24002 |
| `data/k11-captures/gp-u8-handicap/prage_000.dro` | 36110 | 5839 | 36110 | `0/0xb0=0x2b` | `6316/0xb4=0x2b` | 292 | 24547 |
| `data/k11-captures/gp-u8-left-training/prage_000.dro` | 37190 | 5839 | 37190 | `0/0xb0=0x2b` | `7548/0xb4=0x2b` | 292 | 24285 |
| `data/k11-captures/gp-u8-right-arcade/prage_000.dro` | 35934 | 5906 | 35934 | `0/0xb0=0x2b` | `6700/0xb4=0x2b` | 307 | 24110 |
| `data/k11-captures/gp-u8-right-training/prage_000.dro` | 38234 | 5764 | 38234 | `0/0xb0=0x2b` | `8817/0xb4=0x2b` | 294 | 24029 |
| `data/k11-captures/gp-u8-tug-of-war/prage_000.dro` | 39298 | 5775 | 39298 | `0/0xb0=0x2b` | `9793/0xb4=0x2b` | 292 | 23977 |
| `data/k11-captures/gp-u9-win/prage_000.dro` | 57124 | 8331 | 57124 | `0/0xb0=0x2b` | `7157/0xb4=0x2b` | 357 | 24198 |
| `data/k11-captures/idle/prage_000.dro` | 39898 | 185 | 247 | `0/0xb0=0x2b` | — | 4 | 90088 |
| `data/k11-captures/idle/prage_001.dro` | 26453 | 7007 | 26453 | `0/0xb4=0x2b` | `1628/0xb8=0x36` | 348 | 63882 |
| `data/k11-captures/menuesc/prage_000.dro` | 36335 | 2051 | 36335 | `0/0xb0=0x2b` | `22965/0xb4=0x2b` | 102 | 24001 |
| `data/k11-captures/save-reload/prage_000.dro` | 4100 | 210 | 4100 | `0/0xb0=0x2b` | `1624/0xb4=0x36` | 6 | 41251 |
| `data/k11-captures/walk/prage_000.dro` | 33673 | 185 | 247 | `0/0xb0=0x2b` | — | 4 | 75008 |
| `data/k11-captures/walk/prage_001.dro` | 625 | 361 | 625 | `0/0xb4=0x2e` | — | 8 | 74630 |

The 37 rows are measured by `tools/opl_trace.py`'s parser. Notes:

- **Six files are byte-identical register streams** (a normalized `(reg,val)` sha256
  match): `de/prage_000`, `diags/prage_000`, `gp-pads/prage_000`,
  `gp-u17-device2/prage_000`, `idle/prage_000`, `walk/prage_000`. All six hold only the
  boot fragment (185 writes, 4 key-ons, no music after the boot); their capture recorded
  only that fragment (for `gp-pads` and `walk` the `_001` file holds the rest — DX-CAPTURE
  restarted the OPL recording), and their `.dro` was written at the first FM note-on and
  closed with the trailing delay.
- **Three more are truncated:** `gp-pads/prage_001` (189 writes), `walk/prage_001`
  (361 writes) and `save-reload/prage_000` (210 writes, 4.1 s, 6 key-ons). Their first
  key-on is on `0xB4` (`gp-pads/001`, value `0x2B`), `0xB4` (`walk/001`, `0x2E`) and
  `0xB0` (`save-reload`, `0x2B`): the capture began after the boot's first write block.
  `idle/prage_001` (7,007 writes, 26.5 s) *is* a music run, but its first key-on is
  `0xB4` (the boot init block is absent: the capture began after the init). `gp-pads`'s
  and `walk`'s first files hold the boot fragment while the `_001` files hold the rest;
  DX-CAPTURE wrote a second file when the OPL capture restarted.
- **28 of the 37** have more than 1,000 writes and carry music in a captured window;
  **26 of the 29 `gp-*`** do. The nine without music are the six byte-identical
  boot-fragment files plus `gp-pads/prage_001`, `save-reload/prage_000` and
  `walk/prage_001`.
- The `ms` header can exceed the last write (e.g. `gp-pads/000`: 50,951 vs 248): the
  header is the capture's accumulated delay; the trailing silence after the last write is
  included. The last-write tick is the stream's span and the only time the oracle needs.

#### §M1.6.3 The boot fragment (the title reference in every trace)

Every file that begins at boot (34 of 37) opens with the same 146-write AIL/driver init
block at tick 0, then the **S16TITLE bank's first four notes**: key-ons at ticks
0, 64, 124 and 192 on `0xB0`/`0xB1`/`0xB2`/`0xB3`, with `(block,fnum)`
`(2,0x3CF)`, `(2,0x334)`, `(2,0x2DB)`, `(1,0x28B)`, keyed off at 64, 124, 192, 248. That
is the title/attract bank `0x54`/`0x56` (`0x03836102`): `tools/opl_seq.py`'s decoded
stream has the same four notes at its ticks 60/68/75/83 (the 120 Hz oracle tick =
capture ms + 500), and the capture's tick-63 block carries the same operator/EG/wave
values as the oracle's second note (the first note's AM/VIB and TL writes are already
the driver-init values in the capture's tick-0 block, one of the documented model
differences). The notes are cut at 248 ms — the capture's first music section is a 4-note
fragment, not the whole title song. The 9866-write title stream is proved by the unit
oracle, not by these captures.

#### §M1.6.4 The first music after the boot

After the 4-note fragment there is a 5.8–10.6 s write gap (the boot/logo path) and then
the first music section. Its first key-on is the "first music key-on" column: mostly
`0xB4=0x2B` at 6,049–10,825 ms on the long `gp-*` runs; `gp-u22-diablo-ending` is
`0xB1=0x2B` (6,049), `gp-u8-attract-start` is the outlier at 1,684 ms (its scenario
starts from the attract), `idle/prage_001` is `0xB8=0x36`, and `save-reload/prage_000`'s
`0xB4=0x36` at 1,624 ms is a blip in a 4.1 s file, not a music section. **The section at
~7 s is the `s16selmo` bank**
(`0x2E`/`0x30`, handle `0x0980025C`, `s16selmo.gra` +0x25C): `gp-u5-charsel`'s first note
at 7,156 writes `0x29=0x31, 0x2C=0x72, 0x49=0x5B, 0x4C=0x95, 0x69=0xF4, 0x6C=0x8A,
0x89=0x15, 0x8C=0x05, 0xC4=0x30, 0xA4=0x99, 0xB4=0x2B`, and the decoded bank's first note
is `0x20=0x31, 0x23=0x72, 0x40=0x5B, 0x43=0x98, 0x60=0xF4, 0x63=0x8A, 0x80=0x15, 0x83=0x05,
0xC0=0x30, 0xA0=0x99, 0xB0=0x2B` (the same note on channel 4: the oracle's channel 0
shifted +4, its operator registers landing at +9 because channel 4's slots are 9 and 12;
the one value difference is the carrier TL `0x95` vs `0x98`, the declared sequence-volume
difference). That 11-value
first-note match is the identification; the same 11 values start the first music section
of `gp-u8-right-arcade` (6,700), `gp-idle-loss` (7,256) and `gp-u12-endurance` (10,684),
and `gp-u22-diablo-ending`'s 6,049 group is the same values one channel over
(`0x22=0x31, 0x25=0x72, 0x42=0x5B, 0x45=0x95, 0x62=0xF4, …`). The oracle's own comparison
(Task 4) establishes the rest of each section. This is the record's one worked bank
identification; Task 3/4 repeat it for the other sections.

#### §M1.6.5 The clock offsets (not an alignment)

The last column, `poll E ms − .dro last write`, is **not** a constant: 24.0–26.6 s on the
normal runs, 39.6–42.6 s on the ending captures, 29.2 s on the U16 finisher, and 32–90 s
on the truncated files. The
DOSBox OPL capture's ms and the harness's poll `ms` share no origin (the capture's clock is
the accumulated delay between FM writes; the poll's is the host monotonic clock). The
register-stream oracle must therefore anchor on the **first key-on inside the `.dro`**, as
the plan says, and never on a poll timestamp. The offsets are recorded so a later task
does not mistake the apparent 24 s difference for a constant.

#### §M1.6.6 Grouping: the traces repeat by song

The music register stream is a function of the song and the sequence position, not of the
gameplay script. Two traces that play the same songs therefore share a long
`(register,value)` prefix (ticks ignored) until their scripts diverge. Measured pairwise
common-prefix lengths (capped at 20,000 writes):

| common writes | trace A | trace B |
|---:|---|---|
| 16,194 | `gp-u16-twop2` | `gp-u23-u16-finisher` |
| 11,936 | `gp-u6-moves-b` | `gp-u6-moves` |
| 7,193 | `gp-u10-ending` | `gp-u9-win` |
| 5,128 | `gp-idle-loss` | `gp-u15-join` |
| 4,976 | `gp-idle-loss-run2` | `gp-keys-fight` |
| 3,899 | `gp-keys-fight` / `gp-idle-loss-run2` | `gp-u8-attract-start` |
| 3,866 | `gp-keys-fight` / `gp-idle-loss-run2` / `gp-u6-moves` / `gp-u6-moves-b` | each other |
| 3,854 | `gp-u15-join` / `gp-idle-loss` / `gp-u8-attract-start` / `gp-u6-moves` / `gp-keys-fight` | each other |
| 3,841 | `gp-u8-handicap` | `gp-u8-attract-start` / `gp-u6-moves` |

The 3,841–3,899 groups are the shared boot+title+menu songs; the longer pairs are the
same match/ending song played for longer. The six byte-identical boot-fragment files are
the degenerate group. **These are the "songs repeat" groups**: the shared prefix is the
register-level proof of the repeated song, and it is longer than any coincidence (the
first 200 writes are the driver init; the rest are music).

#### §M1.6.7 The songs covered (code-derived) and the named gap

The code's music requests reach these case-1 banks in the captured states:

| state | id(s) | bank | proof |
|---|---|---|---|
| title/attract | `0x54`/`0x56` | `s16title.gra` +0x36102 | 9866-write oracle + the boot fragment |
| character screen / select (mode 0x10) | `0x2E`/`0x30` | `s16selmo.gra` +0x25C | the 7 s section (§M1.6.4) |
| fight hook `0x430E8` | `0x31` | `s16selmo.gra` +0x08 | code |
| fight hook `0x4142C` | `0x32` | `s16snd2.gra` +0x21BFA | code |
| match entry (the round) | stage table 0..7 | `0x20`/`0x21`/`0x1B`/`0x1C`/`0x1E`/`0x1D`/`0x1F`/`0x1F` | `DS_000C9888`, `0x4F714`, caller `0x25C09` |
| match end | `0xDF` or `0x23` | `s16sound+0x6EC8` / `+0x5B88` | `sound_voice_match_end` |
| mode 0x13 | `0x2C` | `s16conti.gra` +0x08 | code |
| hook `0x25AE8` | `0x3D` | `s16sound.gra` +0x39F4 | code |
| mode 0x1F | `0x3B` | `s16sound.gra` +0x335E | code |
| name entry/high score | `0xE1`/`0xE3` | `s16hghsc.gra` +0x08 | code |
| fighter states | `0xEC`/`0xEB`/`0xE4` | `s16sound.gra` +0x72D4/+0x7714, `s16konsd.gra` +0x1CCB0 | code |

The 29 `gp-*` traces' captured windows are the hooks/modes above (menu, select, fight,
win/ending); the *per-trace* section-to-bank attribution beyond §M1.6.4 is the Task 3/4
oracle's output, not this task's claim. **Named gap:** a bank that no captured section
plays cannot be claimed covered. The 26-bank table is complete, and the oracle's first
difference / skip-by-name rule will turn an unplayed bank into a named gap rather than a
false pass.

### §M1.7 The PCM state

`make audio-render` writes a 44-byte RIFF/WAVE header then PCM frames:

- **Format**: `fmt ` = 1 (PCM), 2 channels, 49,716 Hz (`MIXER_OPL_RATE`,
  `mixer.h:50`), byte rate 198,864, block align 4, 16 bits per sample.
- **Length**: 12 s default (`AUDIO_SECONDS`, Makefile:2019), 596,592 frames,
  2,386,368 data bytes, 2,386,412 file bytes.
- **Measured content**: peak 3,402, 1,122,762 non-zero samples of 1,193,184, sha256
  `df74acfb65d345fb72cb214102089f2a0ab8d4b271ddc17e2a5f5c4f1a380844`.
- **Fails only on `fopen`**: the block's only assertion is `test_audio.c:1218`;
  `/nonexistent-dir/pr_title_fm.wav` gives exactly that one failure (`FAILURES: 1`).
  There is no read of `chunk` or the file after `fwrite`, so a silent WAV passes by
  construction. The claim is determinism, not audibility.
- **The sequence-volume omission**: the render block starts at `seq_start()` and never
  calls `seq_set_sequence_volume`. It inherits the value the preceding oracle block set,
  `0x54` (`test_audio.c:954`, the capture-derived steady-CC7 value). The image's
  `DS_000A2CB8` default is `0x7F` (§M1.2) and `title_music_start` passes whatever
  `DS_000A2CB8` holds; the render's `0x54` is therefore not the game's configured
  sequence volume. Task 5 must set the game's sequence volume explicitly and pin the
  resulting peak/sha; the current hash is the *test's* `0x54` baseline, not the game's.

The OPL core and mixer are an invented model (Q8 gain, nearest-neighbour channel
mapping, four voices; `port/spec/audio.md`). A PCM comparison to the original would pin
the model, not the game; the register stream is the claim (§M1.6), the PCM hash is only
the port's own determinism.

### §M1.8 What the later tasks consume

- **Task 2** consumes §M1.2/§M1.3: the raw gate is `DS_001028C0`; the port has a live
  sequence (`s_sequence`) but stores 0. The game-side resolve must (a) let a request with a
  live sequence become pending, (b) resolve the handle through `res_resolve`, (c) copy the
  size-prefixed bank bytes into the 0x5100 buffer (or an equivalent bound-checked copy),
  (d) `AIL_init_sequence`/`set_volume(DS_000A2CB8)`/`start`, (e) clear the pending dword,
  and (f) not move `title_music_start`. The AIL handle/mixer stay host-owned; no
  differential row is fabricated.
- **Task 3** consumes §M1.4/§M1.6: the stage table and the 29 banks; the first music
  section is the selmo bank, and the fight stage song is `DS_000C9888[stage]`.
- **Task 4** consumes §M1.6: 29 gp `.dro` files (not 37 gameplay), each anchored at its
  first key-on; the six byte-identical boot-fragment files and the three truncated files
  are inventoried; a bank absent from the traces is a named gap.
- **Task 5** consumes §M1.7: the WAV format/rate and the `0x54`-inherited baseline.
- **Task 7** consumes §M1.5: the `0x1C930`/`0x1CF40` boundary stays `UNROWED`; the
  counter is `1033/1033 ...` and must not move in Task 1.

### §M1.9 Named gaps and what §M1 does not claim

1. **Per-section bank attribution** for every trace beyond §M1.6.4: Task 3/4's oracle
   output. The 26-bank set is complete; the mapping of each captured section to its bank
   is not claimed here.
2. **The `.dro`'s ms timeline is the capture's own** (the accumulated delay between FM
   writes, with the trailing silence in the header and no alignment to the poll log); no
   claim is made that a `.dro` tick equals a host or port frame.
3. **The title music section in the captures is a 4-note fragment**, not the title
   song's proof; the 9866-write unit oracle is the proof.
4. **Rendered audibility** is not claimed by any measurement here.
5. **A song/bank absent from the 29 gameplay traces** is a named gap, not a new capture
   (the phase's binding constraint).
6. The six identical boot-fragment files and the three truncated files hold no in-match
   music; an oracle that skips them must skip them **by name**, not silently.

### §M1.10 Gate for Task 1

Research only: no port code, no test, no Makefile and no pin changed. `make diff-verify`
exit 0 (the counter above), `make audio-render` exit 0 (the 9866 line and the WAV), and
`python3 tools/port_progress.py` unchanged. The only written artefact is this record (and
the spec/plan files the controller stages separately).
