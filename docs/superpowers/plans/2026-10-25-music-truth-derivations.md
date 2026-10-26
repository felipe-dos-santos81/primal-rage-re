# Music Truth — derivations

Status: complete, 2026-10-26. Branch `music-truth`, base `a7876ffc` (`docs: close the
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
  streams at their first key-on, maps capture ms → port tick with the inverse equation
  in §M1.6.3 (`test_audio.c:1274`), drops the
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
| `DS_001028C0` | MDI **sequence** handle | `0x1CFE8` (allocate), `0x1CA2C`/`0x1CA40`/`0x1CA86`/`0x1CAD2`/`0x1D1E1` (gates), cleared on a failed buffer (`0x1D11E`) |
| `DS_001028C4` | MDI driver handle | `0x1CFD6` (allocate), read `0x1D0CC`, cleared `0x1D113` |
| `DS_001028C8` | DIG driver handle | `0x1CF8E` (port stores 1), every sample path |
| `DS_001028CC` | pending song (handle) | queued `0x1CA35`; requeued `0x1D1E7` (unpause, §M1.3); cleared `0x1CA08`/`0x1CAA1`, and `0x1D124` on the allocation failure |
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

**The song-id sources.** `DS_001028CC` has exactly two non-zero writers in the code
object and three clears. The queue writer is `0x1CA35` in `0x1CA14` (the raw addresses
are in §M1.2; `snd_music_request`): `EAX` = the request's handle, stored after the pause
(`DS_001028DA != 1`) and sequence (`DS_001028C0 != 0`) gates. Its two call sites are both
in the dispatcher: `0x2C463` (case 1, with `EAX = [0x105D5C]`, the record's `+4` handle)
and `0x2C8B6` (case 4's restart, `EAX = 0x2803E64`).

The second is the **unpause requeue at `0x1D1E7`** in `0x1D1B0`
(`sound_music_pause_toggle`, `flow.c:6470`). The resume arm (taken when
`DSB(0x1028DA) == 1`) clears `DA` at `0x1D1C2`, then requires three guards before the
store: the current song byte `D9 != 0` (`0x1D1C8`), the current song handle
`D4 != 0` (`0x1D1D1`/`0x1D1D7`), and the sequence `C0 != 0` (`0x1D1DB`/`0x1D1E1`/
`0x1D1E3`/`0x1D1E5`). When all three hold, `0x1D1E7` stores `D4` (the current song) into
`CC` and returns; otherwise it returns without a store. The other arm tests `C0`, calls
`0x1CA40`, and when the sequence plays stops it (`0x1D20B`) and sets `DA = 1`
(`0x1D213`); it never touches `D4`/`D9`/`DC`. Consequently a normal pause/resume
requeues the song that is already the last-started `DC`, and `0x1C930` takes its
**same-song `je` arm** (`0x1C93B`): no resolve, no copy; it re-inits the copied buffer,
sets the volume and starts. If a song was requested after the last start and before the
pause (`D4 != DC`), the resume's requeue takes the new-song arm instead (resolve + copy
of the same handle). The port's `sound_music_pause_toggle` already stores `D4` into
`CC` under the same guards (`flow.c:6478-6479`), so this path is faithful; the record
must not treat it as a clear.

The three clears are `0x1CA08` (after either arm of `0x1C930`, EAX zeroed at `0x1CA06`),
`0x1CAA1` (`snd_music_stop`, EDX zeroed at `0x1CA76`), and `0x1D124` (the
`sound_buffers_alloc` failure arm, which stores the gate's ECX at `0x1D113`/`0x1D11E`/
`0x1D124`; the port stores 0 at `flow.c:6426-6428`). A code-object scan for the
`0x001028CC` immediate lists only these five writers plus two readers (`0x1C936` and
`0x1CF30`); the scan below is the covering check.

The handle is a resource handle: `res_resolve` (`0x1B544`,
`res.c:300`) returns `mem + entry[+0x10] + (handle & 0x7FFFFF)`. The resolved pointer's
first dword is the payload size (the same `[esi-4]` the copy reads). `0x1C930` therefore
needs: the pending handle, the resource layer, the 0x5100 buffer, and the host sequence.

**Covering check (the writer set).** Scan the fixup-applied image's code object for the
little-endian `0x001028CC` immediate and disassemble the instruction that covers each
hit (`python3 - <<'EOF'`, capstone, `code = img[0:0x73B15-0x10000]`,
`needle = struct.pack('<I', 0x1028CC)`). It prints exactly seven instructions:

```
01C936 a1cc281000   mov eax, [0x1028CC]   ; read (0x1C930's pending gate)
01CA08 a3cc281000   mov [0x1028CC], eax   ; clear (0x1C930, eax=0 at 0x1CA06)
01CA35 a3cc281000   mov [0x1028CC], eax   ; queue (0x1CA14)
01CAA1 8915cc281000 mov [0x1028CC], edx   ; clear (0x1CA6C, edx=0 at 0x1CA76)
01CF30 833dcc28100000 cmp dword [0x1028CC], 0 ; read (0x1CF20's pending gate)
01D124 890dcc281000 mov [0x1028CC], ecx   ; clear (0x1D0BC failure arm)
01D1E7 890dcc281000 mov [0x1028CC], ecx   ; requeue D4 (0x1D1B0 resume arm)
```

So `0x1CA35` queues a *request*, `0x1D1E7` requeues the *current* song, and the three
clears are distinct writes; no other instruction *names* the dword as an immediate
operand (a computed-register write is not excluded by this scan; the decomp's
`DAT_001028cc =` assignments agree with the five writers).

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

The plan and spec originally said "the 37 gameplay `.dro` traces". The measured tree
(and the fix commit corrects the plan/spec to these counts):

- `data/k11-captures/**`: **37** `.dro` files. **29** are under `gp-*` capture directories;
  the other **8** are the service-menu/save/diagnostic captures
  (`de`, `diags`, `idle` ×2, `menuesc`, `save-reload`, `walk` ×2).
- `data/audio-captures/`: **2** more (the title capture the report-only test reads:
  `prage_000.dro` 7,201 writes / 28,117 ms of writes, `prage_001.dro` 1,052 / 9,433).
- Total `.dro` under `data/`: **39**.

So "37 gameplay traces" is a miscount: 37 is the `k11-captures` total, of which 29 are
gameplay. The oracle's inventory is the 29 `gp-*` files; the other 10 (8 + 2) are **named
apart, not gameplay** — the 2 `audio-captures` are the title captures (music, not a gameplay
scenario) and the 8 other k11 files are the service-menu/save/diagnostic captures — so a song
absent from the 29 is a named gap, not a silent skip. Three of the 29 `gp-*` files
(`gp-pads/prage_000` and `gp-u17-device2/prage_000`, which are boot fragments, and
`gp-pads/prage_001`, a restart fragment) carry no music; 26 of the 29 have more than 1,000
writes.

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
- **Two more restart fragments after the boot's first write block**: `gp-pads/prage_001`
  (189 writes; its first block begins at `0x121`, where
  the boot stream has `0x21`) and `walk/prage_001` (361 writes; `0xB4=0x2E` at tick 0).
  Their first key-on is on `0xB4` and the boot init block is absent. `idle/prage_001`
  (7,007 writes, 26.5 s) is the **third** restart fragment and *does* carry music; its first
  key-on is `0xB8=0x36` (the boot init block is absent: the capture began after the
  init). `gp-pads`'s and `walk`'s first files hold the boot fragment while the `_001`
  files hold the rest; DX-CAPTURE wrote a second file when the OPL capture restarted.
- **`save-reload/prage_000` is truncated after the boot, not restarted.** Its first
  **147** complete `(tick, reg, val)` events equal `de/prage_000`'s first 147 (a
  read-only parser comparison): it opens with the boot stream, then stops after 210
  writes / 4.1 s (6 key-ons). Its `0xB4=0x36` at 1,624 ms is a blip in that short file,
  not a music section, and its first key-on is the boot stream's `0xB0=0x2B`.
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
stream has the same four notes at its ticks 60/68/75/83. The existing title-anchor
pairing maps a port tick to capture ms with
`ms = (tick - 60) * 1000 // 120` (`tools/opl_seq.py:705`): integer floor division, the
`60` the bank's first-note tick and the `120` the sequencer ticks per second. It accepts
a capture key-on within ±3 ms (`abs(capture_ms - ms) <= 3`, `cmd_capture_anchors`), a
*matching* tolerance that is **not** the phase's new tolerance-free claim. For the first
four notes the equation gives 0/66/125/191 ms and the capture has 0/64/124/192 (all
within the ±3 ms). The report-only capture comparison uses the inverse
`ct = (capture_ms * 120 + 500) / 1000 + 60` (integer division after the +500, i.e.
round-half-up; `test_audio.c:1274`), which maps 0/64/124/192 back to 60/68/75/83 exactly.
Neither equation is §M4's alignment: the new register oracle anchors both streams at the
first key-on and compares the write stream tolerance-free; the ±3 ms above belongs only
to the existing title-bank anchor check. The capture's tick-63 block carries the same
operator/EG/wave values as the oracle's second note (the first note's AM/VIB and TL
writes are already the driver-init values in the capture's tick-0 block, one of the
documented model differences). The notes are cut at 248 ms — the capture's first music
section is a 4-note fragment, not the whole title song. The 9866-write title stream is
proved by the unit oracle, not by these captures.

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
shifted +4, its operator registers landing at +9 because channel 4's slots are 9 and 12).
That is an **11-register correspondence with ten equal values and one disclosed
difference**: the carrier TL `0x4C=0x95` in the capture against `0x43=0x98` in the
decoded bank, the MIDI-channel-4 voice. It is exactly the existing ch1/ch4
carrier-TL/volume residual `port/spec/audio.md` item 1 excludes by name (the engine's
per-channel volume on ch1/ch4 is unpinnable; 2026-09-20-midi-controllers-tl-derivation
§R3), not an exact-value match and not a fitted value. That ten-value correspondence is
the identification; the same capture values start the first music section of
`gp-u8-right-arcade` (6,700), `gp-idle-loss` (7,256) and `gp-u12-endurance` (10,684),
and `gp-u22-diablo-ending`'s 6,049 group is the same values one channel over
(`0x22=0x31, 0x25=0x72, 0x42=0x5B, 0x45=0x95, 0x62=0xF4, …`). The oracle's own comparison
(Task 4) establishes the rest of each section. This is the record's one worked bank
identification; Task 4 owns the per-section attribution for the other sections.

#### §M1.6.5 The clock offsets (not an alignment)

The last column, `poll E ms − .dro last write`, is **not** a constant: 24.0–26.6 s on the
normal runs, 39.6–42.6 s on the ending captures, 29.2 s on the U16 finisher, and 32–90 s
on the boot-only and short files. The
DOSBox OPL capture's ms and the harness's poll `ms` share no origin (the capture's clock is
the accumulated delay between FM writes; the poll's is the host monotonic clock). The
register-stream oracle must therefore anchor on the **first key-on inside the `.dro`**, as
the plan says, and never on a poll timestamp. The offsets are recorded so a later task
does not mistake the apparent 24 s difference for a constant.

#### §M1.6.6 Grouping: the measured common prefixes

The music register stream is a function of the song and the sequence position, not of the
gameplay script. Two traces that play the same songs therefore share a long
`(register,value)` prefix (ticks ignored) until their scripts diverge. A shared prefix
proves the two traces emitted the same register writes in the same order; it does **not**
by itself identify which bank produced them. The lengths below are measured; the
per-section bank attribution is Task 4's oracle output (the controller's ruling on this
task's review: no generic matcher here). Measured pairwise common-prefix lengths (capped
at 20,000 writes):

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

The 3,841–3,899 groups share the boot stream, the title fragment and the start of the
selmo section (the two worked identifications above); the longer pairs continue matching
far past it. The six byte-identical boot-fragment files are the degenerate group. These
are the measured "traces repeat" groups: a shared prefix is register-level evidence that
the same music was played, not a bank identification.

#### §M1.6.7 The code-reachable 26-bank set (not captured coverage)

The code's music requests *can* reach these case-1 banks in the captured states. The
table is a **code-reachability** list: it says what the game can ask for, not that each
bank appears in a captured section. Only the title fragment (`0x54`/`0x56`) and the ~7 s
section (`0x2E`/`0x30`) have worked identifications (§M1.6.3/§M1.6.4); the per-trace,
per-section attribution is Task 4's measured oracle output (the controller's ruling: do
not fit a generic matcher in Task 1). The `basis` column separates the two worked
identifications from code reachability:

| state | id(s) | bank | basis |
|---|---|---|---|
| title/attract | `0x54`/`0x56` | `s16title.gra` +0x36102 | worked: 9866-write oracle + the boot fragment (§M1.6.3) |
| character screen / select | `0x2E`/`0x30` | `s16selmo.gra` +0x25C | worked: the ~7 s section (§M1.6.4) |
| fight hook `0x430E8` | `0x31` | `s16selmo.gra` +0x08 | code reachability |
| fight hook `0x4142C` | `0x32` | `s16snd2.gra` +0x21BFA | code reachability |
| match entry (the round) | stage table 0..7 | `0x20`/`0x21`/`0x1B`/`0x1C`/`0x1E`/`0x1D`/`0x1F`/`0x1F` | code reachability (`DS_000C9888`, `0x4F714`, caller `0x25C09`) |
| match end | `0xDF` or `0x23` | `s16sound+0x6EC8` / `+0x5B88` | code reachability (`sound_voice_match_end`) |
| mode 0x13 | `0x2C` | `s16conti.gra` +0x08 | code reachability |
| hook `0x25AE8` | `0x3D` | `s16sound.gra` +0x39F4 | code reachability |
| mode 0x1F | `0x3B` | `s16sound.gra` +0x335E | code reachability |
| name entry/high score | `0xE1`/`0xE3` | `s16hghsc.gra` +0x08 | code reachability |
| fighter states | `0xEC`/`0xEB`/`0xE4` | `s16sound.gra` +0x72D4/+0x7714, `s16konsd.gra` +0x1CCB0 | code reachability |

The 29 `gp-*` traces' captured windows are the hooks/modes above (menu, select, fight,
win/ending), but **no claim is made here that a specific captured section is the bank on
its row**. The `0x2E`/`0x30` identification is the one worked case; Task 4's oracle
supplies the rest by aligning the port's trace to each `.dro` (controller ruling).
**Named gap:** a bank that no captured section plays cannot be claimed covered; the
oracle's first difference / skip-by-name rule turns an unplayed bank into a named gap
rather than a false pass. The 26-bank set is complete; its captured coverage is not.

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
  section is the selmo bank, and the fight stage song is `DS_000C9888[stage]` (the code
  reachability; Task 4's oracle is what checks the captured sections against it).
- **Task 4** consumes §M1.6: 29 gp `.dro` files (not 37 gameplay), each anchored at its
  first key-on; the six byte-identical boot-fragment files, the boot-truncated
  `save-reload` and the three restart fragments are inventoried; a bank absent from the
  traces is a named gap. **Task 4 owns the per-trace/per-section section-to-bank
  attribution** (the controller's ruling); §M1.6.7's table is code-reachable only.
- **Task 5** consumes §M1.7: the WAV format/rate and the `0x54`-inherited baseline.
- **Task 7** consumes §M1.5: the `0x1C930`/`0x1CF40` boundary stays `UNROWED`; the
  counter is `1033/1033 ...` and must not move in Task 1.

### §M1.9 Named gaps and what §M1 does not claim

1. **Per-section bank attribution** for every trace beyond §M1.6.4: Task 4's oracle
   output (the controller's ruling). The 26-bank set is complete and §M1.6.7 is a
   code-reachability list; the mapping of each captured section to its bank is not
   claimed here.
2. **The `.dro`'s ms timeline is the capture's own** (the accumulated delay between FM
   writes, with the trailing silence in the header and no alignment to the poll log); no
   claim is made that a `.dro` tick equals a host or port frame.
3. **The title music section in the captures is a 4-note fragment**, not the title
   song's proof; the 9866-write unit oracle is the proof. The title-anchor ±3 ms in
   §M1.6.3 is the existing matching tolerance, not the new oracle's alignment.
4. **Rendered audibility** is not claimed by any measurement here.
5. **A song/bank absent from the 29 gameplay traces** is a named gap, not a new capture
   (the phase's binding constraint).
6. The six identical boot-fragment files, `gp-pads/prage_001`, `walk/prage_001` and the
   boot-truncated `save-reload/prage_000` hold no in-match music; `idle/prage_001` is a
   restart fragment that does. An oracle that skips a no-music trace must skip it **by
   name**, not silently.

### §M1.10 Gate for Task 1

Research only: no port code, no test, no Makefile and no pin changed. `make diff-verify`
exit 0 (the counter above), `make audio-render` exit 0 (the 9866 line and the WAV), and
`python3 tools/port_progress.py` unchanged. The Task-1 artefacts are this record and the
plan/spec corrections.

## §M2 The game-side song request/resolve (Task 2)

### §M2.1 The hole and the fix, measured

§M1.2 measured the hole: `snd_music_request` (`0x1CA14`) is byte-faithful but its
queue gate `DS_001028C0 != 0` (`0x1CA2C`) never passed, because `game_audio_init`
(`0x1CF40`) stored no handle at `0x1028C0` (the sequence, `0x1CFE8`) or
`0x1028C4` (the MDI driver, `0x1CFD6`). The attract's `0x54`/`0x56` request
(`attract.c:295`, `0x111FF`) and the match entry's stage voice (`0x4F714`,
caller `0x25C09`, `flow.c:1404`) reached it and their pending store was dropped.

The fix is the two registrations plus the game-side resolve:

- `game_audio_init` now stores the non-zero **stand-in 1** at `0x1028C4` when
  the MDI driver installed and at `0x1028C0` when the sequence allocated, the
  same form as the DIG driver's stand-in at `0x1028C8` (`0x1CF8E`). Every reader
  of those dwords tests them against zero, so the stand-in is the port's
  expression of "the install succeeded" and is not an AIL handle. The readers
  are `0x1CA2C` (the request's queue gate), `0x1CA40`/`0x1CA86`/`0x1CAD2`/
  `0x1D1E1`/`0x1D1F2` (playing, stop, volume, pause) and `0x1D0CC` (the
  MIDI-buffer gate). The last one matters: with `0x1028C4` and `0x1028C0` now
  set, `game_init`'s `0x1C0B1 -> 0x1D0BC` allocates the raw's `0x5100` song
  buffer at `0x1028D0` (`0x1D0E8..0x1D107`), which is the copy target below.
- `game_audio_service` (`0x1CF20`) now runs the raw's pending arm in the raw's
  place: after the four `0x1CB18` slot starts, `if (DSD(0x1028CC) != 0)
  snd_music_start()` (`0x1CF30/0x1CF37` compare, `0x1CF39` call). The title
  substitute's `s_music_request` block stays after it (see §M2.6).

Measured on the changed tree: a stage-0 match request (`sound_voice_stage(0)`,
id `0x20`, record handle `0x0A800008`) returns to `0x1CA14` with AL = 1 and
`DS_001028CC = 0x0A800008`.

### §M2.2 The port of `0x1C930` (`snd_music_start`, `flow.c`)

The raw body and its addresses are §M1.3's disassembly. The port keeps its two
arms and every raw store, in the raw's order:

| raw | port | notes |
|---|---|---|
| `0x1C936` read `0x1028CC` | `song = DSD(DS_001028CC)` | the pending song |
| `0x1C93B` compare `0x1028DC` | `song == DSD(DS_001028DC)` | same-song arm |
| `0x1C943` store `0x1028DC` | `DSD(DS_001028DC) = song` | new-song arm only |
| `0x1C948` `0x1B544` | `res_resolve(song)` | the resource layer |
| `0x1C956`/`0x1C9BD` `0x5DEAF` | `AIL_stop_sequence(s_sequence)` | |
| `0x1C961..0x1C977` copy | `memcpy(mem + DSD(0x1028D0), p + 4, [p])` | size = the resolved block's first dword; payload from +4 |
| `0x1C98A`/`0x1C9D5` `0x5DE48` | `AIL_init_sequence(s_sequence, bank, 0)` | `bank` is the copied buffer's FORM XMID (§M2.2.1) |
| `0x1C9A5`/`0x1C9F0` `0x5DECA` | `AIL_set_sequence_volume(s_sequence, DSD(0x0A2CB8), 500)` | the music volume, 500 ms |
| `0x1C9B4`/`0x1C9FE` `0x5DE79` | `AIL_start_sequence(s_sequence)` | |
| `0x1CA08` clear `0x1028CC` | `DSD(DS_001028CC) = 0` | both arms |

`PORT:` note in the function: the sequence is the host object `s_sequence`, the
three AIL calls are the host hand-off to `port/src/platform/audio/`, and the
copy target is the raw's own `0x5100` block. Two guards are declared deviations
the raw does not have: a handle that does not resolve (a unit fixture, not a
shipped song) and a size above `0x5100` drop the request with the pending word
cleared instead of dereferencing NULL or overflowing the block. Neither can fire
on the shipped data: all 26 distinct banks are 540..20,400 bytes (§M1.4.1), and
every handle in `DS_000BBDC8` resolves.

#### §M2.2.1 The AIL hand-off container (measured)

The copied bytes are the resolved block's payload after its size dword:
`FORM <0x0E> XDIR` (22 bytes), then `CAT <size> XMID`, then the `FORM XMID`
container (measured for both banks the unit test uses):

```
S16BEACH.GRA +8:  size 20400, payload "FORM", first FORM/XMID at payload+0x22 (FORM size 20358)
S16TITLE.GRA +0x36102: size 4920, payload "FORM", first FORM/XMID at payload+0x22 (FORM size 4878)
```

The raw hands AIL the buffer **base** (the XDIR head); the shipped AIL parses
the chunk list. The port's `AIL_init_sequence` derives its length from the
first FORM's own size (`seq_bank_size` = 8 + 14 = 22 bytes here) and cannot
reach the XMID inside the following CAT, so `snd_music_start` hands it the
copied buffer's first FORM/XMID (`game_music_bank_find(mem + 0x1028D0,
0x5100)`), the same mapping `title_music_start` stands on and the 9866-write
oracle proves. **The copied bytes are the raw's copy; only the pointer handed
to the port's AIL differs**, and the buffer's FORM/XMID is the only one in it.
The scan bound is the buffer's own `0x5100`, so it cannot read past the block.

### §M2.3 The unit test and its RED/GREEN

`port/tests/test_game.c`, `check_music_request()` (called from `test_flow` after
the title-music assertions, on the live sequence `game_audio_init` allocated).
It asserts: the shipped record is case 1 with handle `0x0A800008`; the request
returns 1 and stores exactly that pending handle and current song; the service
resolves, copies the 20,400-byte payload (byte-identical to
`res_resolve(0x0A800008) + 4`), stores `0x1028DC`, clears `0x1028CC` and the
sequencer plays; a second request through `sound_voice_stage(0)` takes the
same-song arm (a zeroed buffer stays zeroed, proving no re-resolve/copy); and a
title request (`0x03836102`, byte `0x54`) resolves, copies its 4,920-byte
payload and plays.

RED (before the implementation; line numbers are that revision's):

```
FAIL port/tests/test_game.c:1793: 0 != 1                                  <- 0x1028C4 stand-in
FAIL port/tests/test_game.c:1794: 0 != 1                                  <- 0x1028C0 stand-in
FAIL port/tests/test_game.c:1617: 0 != 1                                  <- snd_music_request's AL
FAIL port/tests/test_game.c:1618: 0 != 176160776                          <- DS_001028CC (0x0A800008)
FAIL port/tests/test_game.c:1625: -559038242 != 176160776                 <- DS_001028DC
FAIL port/tests/test_game.c:1626: 0 != 1                                  <- snd_music_playing
FAIL port/tests/test_game.c:1630: the resolve copied into the 0x5100 buffer
FAILURES: 7
```

The 9866-write title oracle and the capture comparison printed their unchanged
lines in the same RED run. GREEN: `PR_ORACLE_REQUIRED=1 ./build/run_tests` →
`all checks passed`, with `oracle C-vs-Python: 9866 writes byte-exact` and the
same capture first difference (write 430).

### §M2.4 The `UNROWED` boundary (measured)

`0x1C930` stays in `tools/diff_verify.py`'s `UNROWED`. A row faithful enough
to claim the hand-off would need all of:

- the sequence dword `0x1028C0` to hold the handle the original's four AIL calls
  receive. The raw's body uses it as an opaque AIL handle (`0x1C94D`, `0x1C983`,
  `0x1C99E`, `0x1C9AD`, `0x1C9BB`, `0x1C9CE`, `0x1C9E9`, `0x1C9F8`); seeding a
  value there is fabricating an AIL handle, and in the port the dword is the
  stand-in 1 while the real object is `s_sequence`, a C pointer with no mem[]
  representation. Ruling R3 and the plan's "do not fabricate AIL handles" rule
  this out.
- the original's `0x5DEAF`/`0x5DE48`/`0x5DECA`/`0x5DE79` (runtime, `>= 0x5D000`,
  no ported body) to be stubs. The mem-only comparison would then check the
  stores/copy and **not** the hand-off, which is the phase's functional claim
  (spec §7.5). The row would not be a closure for it.
- the resource layer **inside** the case: diffrun never calls `res_load_index`
  (`port/tests/diff_runner.c` has no such call), so the INDEX table at
  `DS_001014E0` and the payload block would have to be seeded by the case. The
  sample-start rows do seed a table (line 33183), so that alone is not
  disqualifying, but it makes the compared bytes a synthetic resource and still
  leaves the hand-off out.

The record does not add a synthetic row; the counter is unchanged (measured): `1033/1033 functions VERIFIED;
14993/14993 mutants detected; 0 named gaps; 825/845 rows with callees closed
(188 have none); 807/845 closed on full rows; 8 ported functions with no row
(45 FN_ ported-unrowed: 8 portable [5 registry-visible + 3 header-shape
exceptions] + 37 runtime; the scan-visible subset is 42 = 5 + 37)`, and the
harness unittest `Ran 234 tests ... OK`.

`game_audio_service`'s row is unaffected: its cases keep `0x1028CC = 0`, so the
original never reaches `0x1CF39` and its `unhit_named` address is still unhit.
**Deferred doc fix:** that entry's reason text still says the port "replaces
`0x1C930` by host code (flow.c title_music_start)"; the counterpart now exists
(`snd_music_start`) and the border is the host AIL hand-off, not the missing
port. Task 7's docs pass should restate it (the plan's Task 2 file list allows
`tools/diff_verify.py` only for a row).

### §M2.5 The pins (measured on the changed tree)

The title, attract, front-end, audio and gameplay first-unexplained/trace pins are
unchanged. One class of pin moved: the nine ending captures' loader-overlay
allowances became clean matches, so their `MAX_MISSING`/`MIN_REACH` pins were
re-measured and retired (§M2.7). The table below is the final tree.

| check | result |
|---|---|
| `PR_ORACLE_REQUIRED=1` suite | `all checks passed`; 9866 writes byte-exact; capture first difference 430, same registers/values |
| `make title-oracle` | window `[216..326]`, 111 frames, **0 unexplained**, 95/96 port frames each capture, 0 determinism disagreements |
| `make attract-oracle` | 173/690 exhibited, 74 missing classes (pin 74), first divergence capture 215 (expected) |
| `make frontend-oracle` | window `[560..1884]`, 30 missing classes (pin 30), the 2 named unexplained frames only, 1145/1382 exhibited |
| `make demo-fight-oracle` | N 1886; fight window empty; fully explained |
| `make attract2-oracle` | N 3617; 126 missing classes (pin 126); 0 unexplained |
| `make gp-charsel-oracle` | first unexplained 516 (pin 516); trace 1513 (pin 1513); poll sha `138fb537..a18d7e` |
| `make gp-oracle` (gp-idle-loss) | first unexplained 7559 (pin 7559); trace 8320; poll sha `773e2647..8c8447` |
| `make k11-oracle` | walk 38/38 settled, 47/47 non-black; menuesc END 388, 184 clean, poll sha `aacd3242..0f161e` |
| `make save-reload-oracle` | write sha `dad3b1f6..f104f9`; load holds the five ranges at `f=01EB` |
| `make diff-verify` | the §M2.4 counter, unchanged |
| `make entry-triage` | exit 0 (233 unported / 262 ported targets; 131 supplement) |
| `python3 tools/port_progress.py` | `799 1203 66` / `759 759 100` |
| `make check frames=60` | exit 0; `PR_RESTART=1` driver exit 0 |
| the nine ending oracles | re-measured in §M2.7 (the overlay names retired; the N/start/trace/win pins unchanged) |

The gameplay and menu oracles exercise the new resolve: the stage bank (`0x20` =
s16beach) is the same GRA the fight entry already loads, so no new loader
presentation appears there; the ending captures' mode-0x12 entry did gain one
(the music bank s16snd2), and that is §M2.7's measured change.

### §M2.6 Named differences and what §M2 does not claim

1. **The title bank starts twice in a port run — FIXED by §M2.8.** Before the
   review fix, the attract's `0x54`/`0x56` request reached the raw path and
   `game_state_title`'s `s_music_request` substitute restarted S16TITLE at the
   title state (measured: one `snd_music_start` hit with `DS_001028CC =
   0x03836102`, then one `title_music_start` hit). The substitute now asks only
   when the raw request is neither pending nor the last started song
   (`game_state_title_music_request`, §M2.8), so the attract→title run starts the
   bank once (measured: 1 `snd_music_start`, 0 `title_music_start`), while the
   standalone title path still starts it. No named difference remains here.
2. **The hand-off is host-side.** The OPL register stream is the claim (§M1.6);
   rendered audibility stays the model claim (§M1.7). The unit test checks the
   sequencer's playing state and the copied bytes, not a rendered sample.
3. **No differential row** for the reasons in §M2.4; `0x1C930` and `0x1CF40`
   stay `UNROWED`, and the counter is unmoved.
4. **The menu/front-end banks' captured coverage** is Task 4's attribution
   (§M1.6.7); §M2 claims only that a request selects and starts its table bank.

### §M2.7 The ending loader allowances retire (a measured consequence)

The full gate found one class of pin movement, and it is the fix working. The
mode-0x12 (conquered lands) entry's first read of `s16snd2` (index 6) is a
**music** request: voice `0x32` case 1, song handle `0x03021BFA`
(`0x03021BFA = 6 << 23 | 0x21BFA`). The original's sequencer reads that song
through the resource layer (`0x1B544`, draw arm) in the entry's window, so the
`- LOADING -` text (string 489, `0x1B3EA/0x1B3EF`, rows 192..197 x 0..85) lands
over the entry's screen. In the old port the music path never resolved the bank,
so the replay's first `s16snd2` read was the later *sample* queue of voice `0x3A`
(`0x03022554`, also index 6) and the same text landed one screen later. Nine
captures had a named `OVERLAY_ALLOWED` entry for exactly that composition
(`tools/gp_compare.py`'s header said so). With `0x1C930` ported, the entry's own
music resolve presents the loader in the replay's own frame; every one of the
nine capture frames is a **clean** match, so the allowance is stale by the tool's
own rule ("a named frame the two-frame model explains now is reported as a stale
name") and the pins are re-measured.

| scenario | retired overlay (capture) | MAX_MISSING old -> new | MIN_REACH old -> new |
|---|---:|---:|---:|
| `gp-u9-win` | 2209 | 12 -> 12 | 1976 -> 1976 |
| `gp-u10-ending` | 2062 | 65 -> 63 | 4662 -> 4661 |
| `gp-u13-ending2` | 2188 | 59 -> 57 | 4952 -> 4951 |
| `gp-u16-twop2` | 8066 | 23 -> 23 | 6817 -> 6818 |
| `gp-u18-talon-ending` | 2227 | 57 -> 55 | 4864 -> 4863 |
| `gp-u19-vertigo-ending` | 2312 | 51 -> 49 | 4989 -> 4988 |
| `gp-u20-chaos-ending` | 2180 | 72 -> 70 | 4890 -> 4889 |
| `gp-u21-armadon-ending` | 2217 | 55 -> 53 | 4973 -> 4972 |
| `gp-u22-diablo-ending` | 2191 | 54 -> 52 | 4929 -> 4928 |

Every other pin of those oracles is exact and unchanged: `MIN_FIRST` (U10 5437,
U13 5904, U16 8069, U18 5735, U19 5991, U20 5776, U21 5932, U22 5798, U9 2452),
`MAX_START`, `TRACE_MIN_FIRST` (U16's moves too), `WIN_MIN_FIRST`,
`MILESTONES`, the capture shas/frame counts, and the scan3/band allowances. The
`OVERLAY_ALLOWED` table is now empty (the nine entries are listed in its header
and in `tools/tests/test_gp_compare.py`'s retirement test); the mechanism and its
mutation tests stay for a future scenario.

Why the reach moves (mostly -1, U16 +1): the reach is the highest port frame any
classified capture frame exhibits. The entry frame that used to need a
three-frame composition now matches one port frame directly, which changes the
matching alignment by one port frame at the entry; it is a comparison-model
consequence of the same improvement, not a lost port frame (the replay's dump
length is unchanged and every other ratchet is exact). The missing-class counts
drop because the loader-over-black frame the old port dumped at `f=0xC0E` is no
longer produced (the bank is already resident) while the loader-over-entry frame
the capture holds is now the replay's own.

The nine affected targets were re-run with `PR_PINS_EXACT=1` after the re-pin and
pass; the full gate result is in the Task 2 report.

### §M2.8 The review fixes (Task 2 fix round)

The Task 2 review found one Critical, one Important and one Minor issue. All
three are fixed on `efd34a94`'s tree in a follow-up commit.

**Critical — the second title start is gone.** `game_state_title` no longer
sets the substitute flag unconditionally. Its music request is now
`game_state_title_music_request()` (`flow.c`, exported in `flow.h`):

```
if (DSD(DS_001028CC) == TITLE_SONG) return;   /* the raw request is pending */
if (DSD(DS_001028DC) == TITLE_SONG) return;   /* the raw request already started it */
s_music_request = 1;
```

`TITLE_SONG` is `0x03836102` (resource 7 + `0x36102`, ids `0x54`/`0x56`);
`title_music_start`'s body is unchanged. The attract request's `snd_music_start`
stores `DS_001028DC = 0x03836102` before it resolves (and leaves `CC` pending
until `0x1CA08`), so either state suppresses the substitute; a title entry the
attract did not precede (the standalone title path, the title dump driver, the
unit fixture) still requests it. Measured under gdb on `PR_ATTRACT_DUMP`:
before the fix 1 `snd_music_start` + 1 `title_music_start`; after the fix 1
`snd_music_start` + 0 `title_music_start`, and the driver passes.

The regression is in `check_music_request` (`test_game.c`): the match start is
the first counted init, the attract's title request takes the count to 2, the
title entry's `game_state_title_music_request()` + service leaves it at 2 (one
start across the attract request + title entry); then `CC = DC = 0` and a
second title entry raises it to 3 (the standalone path). A mutation that
removes the two guards makes the check fail `3 != 2` (the extra title-state
start) and `4 != 3` (its carry-on), so the regression is not vacuous.

**Important — the exact hand-off is observed.** Two `PORT:` seams are added:

- `seq_loaded_bank_hash()` (`sequencer.c/h`): the FNV-1a of the bytes the last
  successful `seq_load` accepted, i.e. the bank handed to `AIL_init_sequence`.
  `AIL_init_sequence` itself is unchanged except for the counter below.
- `AIL_init_sequence_count_reset()` / `AIL_init_sequence_count()` (`ail.c/h`):
  the successful `AIL_init_sequence` calls since the reset (a failed init is not
  a start).

The test hashes the copied payload's inner FORM/XMID (`payload + 0x22`, the
length `size - 0x22`, the same range `AIL_init_sequence` is handed) and asserts
the sequencer's hash equals it after the match start and after the title start,
with the two banks' hashes different. `snd_music_playing()`/`seq_playing()` and
the copied bytes cannot distinguish the hand-off; this does. The proof: mutate
the new-song arm's `AIL_init_sequence` argument to `title_music_bank()` and the
suite fails its one new assertion (`seq_loaded_bank_hash() == match_hash`,
measurement `-40586013 != -2014854434`); the playing/copy assertions still pass,
which is exactly the review's scenario.

**Minor — the Makefile's current-state comments.** The five U18–U22 reach
comments now distinguish the highest *exhibited* port frame (the pin: 4863,
4988, 4889, 4972, 4928) from the dump's last frame (4864, 4989, 4890, 4973,
4929, unchanged and not exhibited), instead of calling the new reach "the
dump's last frame"; a higher measured reach is an improvement to re-pin. The
U9/U10/U13/U16/U18–U22 blocks' overlay prose is past tense with the §M2.7
retirement named, and their clean/allowance counts are the re-measured ones
(3,682/1,935/9 + two allowances for U18; 3,385 + three for U19; 3,400 + three
for U20; 3,599 + fifteen bands for U21; 3,562 + eight bands and six scan-outs
for U22). `OVERLAY_ALLOWED` is empty and every remaining mention says retired.

The fix commit's re-run evidence (focused suite, title/attract/front-end/
demo-fight/attract2, the nine exact-pinned ending oracles, the full
`make verify`) is in the Task 2 report's fix section.

---

## §M3 The played song and the register comparison (Task 3)

### §M3.1 The driver: `PR_AUDIO_DUMP`

Task 3's observable is the port's own OPL register stream for a replayed
scenario. It is the gp replay driver (`test_game.c`, `PR_GP_DUMP` +
`PR_GP_SCRIPT`) plus one new env var:

```
PR_GP_DUMP=/tmp/pr_gp_dump/gp-u5-charsel \
PR_GP_SCRIPT=/tmp/pr_gp_dump/gp-u5-charsel.script \
PR_GAME_DIR=data/game/C PR_AUDIO_DUMP=/tmp/pr_audio_u5.txt ./build/run_tests
```

The script is `tools/gp_session.py port-script --scenario gp-u5-charsel
--capture data/k11-captures/gp-u5-charsel --out …`; no new capture. The dump
is written by an OPL-core write hook (`opl_set_write_hook`, `opl.h/opl.c`), so
it is not bounded by the `OPL_TRACE_MAX` 16,384-write test window: the longer
gameplay replays overflow it (the captures alone run to 38,514 writes,
§M1.6.2), and the same driver serves the long traces Task 4 needs. The
gp-u5-charsel replay itself emits 3,344 writes. The hook is a test seam: NULL
on the game path, one branch per write; it records the register, the value and
the current `opl_set_write_attr` attribution byte. Two new test seams feed it:
`seq_tick_count()` (the sequencer's per-song service-call counter, `S.svc`, 0
at `seq_start`) and the existing `AIL_init_sequence_count()` /
`seq_loaded_bank_hash()`. Format:

```
W <frame> <tick> <write> <reg> <val> <attr>     one per register write, in order
S <frame> cc=<pending> dc=<last started> vol=<0x000A2CB8>
  bank=<seq_loaded_bank_hash> inits=<AIL_init_sequence_count> ticks=<game_audio_ticks>
```

`frame` is the master-loop frame counter `DS_000EF6DC` of the iteration that
makes the write (the iteration raising it from f to f+1); `tick` is
`seq_tick_count()`, i.e. the write's tick within the song the sequencer is
playing (0 for `seq_start`'s own writes). An `S` line is emitted only when one
of its fields changes; `cc` is read at the end of the iteration, and
`game_audio_service` consumes a pending request in the same iteration, so it is
normally 0. A same-song restart (`0x1C93B`) changes no `S` field and appears
only in its writes.

Measured on `gp-u5-charsel` (3,344 writes, frames 216..1510 of the replay's 1,513):

| frame | `dc` (last started song) | bank | `inits` |
|---:|---|---|---:|
| 216 | `0x03836102` S16TITLE (attract's `0x54`/`0x56`) | `FD94B4E3` | 1 |
| 634 | `0x0980025C` s16selmo +0x25C (ids `0x2E`/`0x30`, character screen) | `286ED662` | 2 |
| 1092 | `0x09800008` s16selmo +8 (id `0x31`, fight hook `0x430E8`) | `F1774E70` | 3 |
| 1370 | `0x0A800008` s16beach +8 (id `0x20`, the match entry `0x4F714`) | `87E7C2DE` | 4 |

`vol` is 0x50 throughout: the port's config-derived music volume
(`attract_config_volumes`, `0x2C8F0`, from `DS_000A2CB8`). It is the value the
game path passes to `AIL_set_sequence_volume`, and the captured TL rows match
under it (§M3.4). The existing title capture's 0x54 (`test_audio.c:1018`) is a
different session's setting and is unchanged; the title oracle drives the
sequencer explicitly.

### §M3.2 The anchor

The selmo (character screen) song is the first music section both streams
contain, and §M1.6.4 already identifies it. Anchored at its **first key-on**:

| | port | capture |
|---|---|---|
| position | frame 658, seq tick 50 | 7,156 ms |
| write | `0xB4 = 0x2B`, attr 4 (MIDI ch4) | `0xB4 = 0x2B` |

The anchor note's captured register set is 11 writes; the port emits the same
11 values, plus two writes the capture does not record because its driver
init already left them 0 (the write-on-change artifact):

```
port   0x29=31 0x2C=72 0x49=5B 0x4C=95 0x69=F4 0x6C=8A 0x89=15 0x8C=05
       0xE9=00 0xEC=00 0xC4=30 0xA4=99 0xB4=2B
capture 0x29=31 0x2C=72 0x49=5B 0x4C=95 0x69=F4 0x6C=8A 0x89=15 0x8C=05
       0xC4=30 0xA4=99 0xB4=2B
```

### §M3.3 The real bug the anchor found: the driver's voice cursor persisted

Before any value comparison, the anchor itself was wrong: the port keyed the
selmo bank's first note on OPL channel 0 (`0x20/0x23/0x40/0x43/…`), while the
capture keys it on **channel 4** (`0x29/0x2C/0x49/0x4C/…`). The port's
`seq_start` reset `S.next` — the FM driver's voice-rotation cursor (SBPRO2.MDI
`0x1408` over the slot-owner bytes `0x1a49`) — to its install value on every
song start.

The raw does not: `0x5DE79` (AIL_start_sequence) is `FUN_0006a780`, which calls
`FUN_0006a7c0` (release the sequence's notes) and `FUN_00069250`, then sets the
status to 4 and `+0x14 = +0x10 + 8`; `0x69250` resets the **sequence record**
(its `+0x90` MIDI-channel map, the CC logs, `+0x30` the service counter, the
note/log tables) and writes no driver state. The cursor is the driver object's,
initialised once at driver install (`0x306e` stores 0xffff) and advanced by its
allocator (`0x3095`-`0x30d8`); a freed voice does not move it back (`0x3162`).
The capture is the direct evidence: the title bank's four notes took OPL
channels 0..3, and 7.2 s later the selmo bank's first note is on channel 4 —
the cursor continued across
`AIL_stop_sequence`/`AIL_init_sequence`/`AIL_start_sequence`.

Fix: `seq_start` no longer resets `S.next`; the module's static initialiser is
the driver-install state, and a new test seam `seq_driver_reset()` restores it
for unit oracles that compare a pristine-process stream. The 9866-write
C-vs-Python title oracle calls it before its capture (its byte-exact line and
the capture first difference 430 are unchanged, see §M3.6). The regression is
`test_audio.c`'s new case 0g: a note on channel 0, stop, start → the next note
is on channel 1 (the cursor persisted), and after `seq_driver_reset()` it is
back on channel 0. Mutation proof: restoring
`S.next = SEQ_OPL_CHANNELS - 1;` in `seq_start` fails exactly that assertion
(`test_audio.c:903: 0 != 1`), and nothing else.

After the fix the anchor's 11 registers match, and the whole section compares
(§M3.4).

### §M3.4 The comparison

Both streams are reduced the way the title oracle reduces them (`port/spec/audio.md`
"Known capture divergences"): anchor at the first key-on; drop a write whose
value equals the last kept value for that register (write-on-change collapse,
shadow seeded from the capture's pre-anchor state on both sides); drop `0xBD`
and the carrier-TL registers `0x43..0x45/0x4B..0x4D/0x53..0x55` on MIDI
channels 1 and 4 only. The capture records no MIDI channel, so its side's
attribution is inferred from the port's write at the same mapped tick and
register (the title oracle's rule). The capture's `ms` maps to a relative tick
with round-half-up: `((ms - anchor_ms) * 120 + 500) / 1000`.

The comparison is **music-tick-indexed**, not frame-indexed: both streams are
the same sequence from the same start, and the two clocks do not share a rate
per game frame. Measured: the port's selmo section spans frames 658..1092 (434
frames) and 866 seq ticks (50..916); the capture's corresponding window is
7,156..14,449 ms = 875 ticks. Over the same 434 frames the capture's clock is
7,293 ms (16.80 ms/frame; the port's replay is 16.67 ms/frame), i.e. 0.8 %
longer. Whether that is the capture environment's clock or a few frames'
difference in when the original issues the next request is **not isolated**
here — the next request is a sound-only fight hook (`0x430E8`, no pinned visual
change) — so no claim is made about the boundary's exact frame. It is not a
dropped port tick: every port tick in the window is matched below.

**Result.** The port's selmo section: 2,610 raw writes in `[634, 1092)`, 1,738
reduced writes (1,794 with the carrier-TL exclusion off; 41 carrier-TL ch1/4
writes excluded by name), over relative ticks 0..866.

- **Per-tick multiset: equal at every relative tick 0..866** (both with and
  without the carrier-TL exclusion). Every register write the capture makes in
  the port's 866-tick window is present with the same value at the same tick
  index, and nothing extra.
- **Run-to-run stability (measured, three runs):** the selmo section is
  identical in all three — 2,610 raw writes, anchor frame 658/tick 50, last
  tick 916, 866 relative ticks. The replay is wall-paced, so only the later
  sections vary: the `0x31` section is identical in the three runs (528 writes,
  last tick 540), while the last (match) section's tail varied by one replay
  frame and 5 sequencer ticks (tick 283/283/288, 148/150/148 writes) — the
  music advances with the host tick while the section end is a game frame.
  Task 4's oracle must therefore pin a minimum compared length (or drive the
  sequencer deterministically); this window's claim is the per-tick equality
  over the port's section, not a fixed tick count.
- **Ordered: the first difference is reduced write 175** (181 without the
  carrier-TL exclusion), relative tick 74, port frame 695, capture 7,772 ms:

  ```
  port    0xB2 = 0x0A   then  0xB4 = 0x0B
  capture 0xB4 = 0x0B   then  0xB2 = 0x0A
  ```

  Same registers, same values, same tick, reversed order. This is the declared
  **intra-tick write-order** model difference (spec item 11): the port's
  `seq_tick` keys off expired voices in ascending OPL-channel order
  (`for v = 0..17`), while the driver's per-tick voice visit order is not
  pinned by its bytes. It is named, not fitted, and no tolerance is applied.

The section then continues matching: the capture's own stop burst for the next
song is at 14,449 ms (`0x1B7=0x07 0x1B8=0x03 0xB0=0x16 0xB1=0x0E 0xB2=0x0A
0xB3=0x0B 0xB4=0x12 0xB5=0x12 0xB6=0x0B`), the same burst the port emits at
frame 1092 for the `0x31` request; the port's 866-tick prefix ends 9 ticks
before it (the clock measurement above).

### §M3.5 The in-match song (identified, window left to Task 4)

At frame 1370 the match entry requests `0x0A800008` (s16beach, stage 0, id
`0x20`, §M1.4.2) — the game path selects and starts the in-match song. Its
first note (frame 1396, seq tick 56) keys OPL channel 15 (bank 1) with
`0x14A=0x0B, 0x1A5=0xB2, 0x1B5=0x2E`. The capture holds those three writes at
26,402 ms, and `0x1A5=0xB2`/`0x1B5=0x2E` again at 34,478 ms (a repeat of the
figure; the TL write is suppressed there by the capture's write-on-change),
so the song is the capture's. The port's replay ends at frame 1512 (the
script's end; its last write is tick 283), so Task 3 does not claim a wrapped
window for it; the `0x31` and `0x20` window alignment (and the capture's
per-section boundary attribution) is Task 4's, as the plan rules.

### §M3.6 Pins and what §M3 does not claim

Measured on the changed tree:

| check | result |
|---|---|
| `PR_ORACLE_REQUIRED=1 ./build/run_tests` | `all checks passed`; `oracle C-vs-Python: 9866 writes byte-exact`; `capture oracle first difference at C write 430` (unchanged) |
| the new regression | case 0g; mutation restores the reset → `test_audio.c:903: 0 != 1` |
| `PR_AUDIO_DUMP` off | the gp replay driver's frames/trace/log are unchanged (the hook is NULL; §M3.1) |

Not claimed: rendered audibility (§M1.7); the sections after the selmo
(`0x31`, `0x20`) as wrapped windows (Task 4); the capture's frame↔music clock
relationship outside the compared window; the driver's intra-tick voice visit
order (named); and the `seq_start` controller-state reset (`S.program`,
`S.volume`, …), which this window's evidence neither supports nor opposes —
Task 4's other songs are where it would show.

## §M4 The register-stream oracle (Task 4)

### §M4.1 The tool and the model

`tools/audio_compare.py` compares the port's `PR_AUDIO_DUMP` register stream
(port/tests/test_game.c, §M3.1) against a capture's DBRAWOPL `.dro`
(tools/opl_trace.py's parser, §M1.6) for the 29 gameplay traces. It is
stdlib-only and has three commands: `compare` (one trace), `summary` (the
covered/missing inventory) and `capture-end` (the X frame capped for the
replay cut). The unit tests are `tools/tests/test_audio_compare.py` (13 cases,
the RED/GREEN and mutation evidence in §M4.5).

**Sections.** The port dump's writes are grouped into songs where the song
tick drops back to 0 (a new `AIL_init_sequence` or the same-song restart
`0x1C93B`). A section's `dc` and bank hash come from the first `S` record at or
after its first write's frame: the `S` line is emitted at the end of the
iteration, after that iteration's writes, so the song's tick-0 writes precede
it. The `dc` is the raw resource handle `(INDEX index << 23) | offset`; the
tool prints `resource+0xoffset` from `INDEX` and the FNV-1a bank hash.

**Anchor.** The section's first key-on is its first `0xB0..0xB8`/`0x1B0..0x1B8`
write with bit `0x20` (both OPL banks: the match song's first note is
`0x1B5=0x2E`, §M3.5). The candidate capture key-ons are the same
`(register, value)`; each candidate is verified against the port's first 4
relative tick groups before it is accepted, so a repeated note elsewhere in
the capture (the selmo's `0xB4=0x2B` occurs at 7,156, 8,284 and 13,173 ms in
`gp-u5-charsel`) cannot be mistaken for the section start.

**Reduction.** Both streams start at the anchor's write (the same-tick
preamble before the key-on is dropped symmetrically, as the title oracle
does), and drop:

  * a write whose value equals the last hardware value for that register
    (write-on-change collapse; the capture records write-on-change only, the
    port writes unconditionally). The shadow is seeded from the capture's
    pre-anchor state on both sides. **The shadow tracks excluded writes too**:
    an excluded carrier-TL/`0xBD` write still leaves its value in the chip and
    the capture's own write-on-change sees it. Task 3's reduction skipped
    excluded writes in the shadow; the measured consequence of the fix is
    §M4.4 item 2. It changes the selmo's claimed reduced count from 1,687 to
    1,703 and leaves the per-tick multisets equal.
  * `0xBD` and the carrier-TL registers `0x43..0x45/0x4B..0x4D/0x53..0x55` on
    MIDI channels 1 and 4 only (the engine's per-channel volume residual,
    `port/spec/audio.md`). The capture records no MIDI channel: a captured
    write inherits the attribution of the port's raw write at the same
    relative tick/register (0xFF when none).

**Comparison.** Per music tick, as a **sorted multiset** of `(register,
value)`. The sorted group is the declared **intra-tick write-order model**:
the port keys expired voices in ascending OPL-channel order, while the
driver's per-tick voice visit order is not pinned (§M3.4). The capture's `ms`
is the emulator's accumulated inter-write delay, not a shared clock; the same
selmo note-off is at 479, 484 and 484 ms in `gp-u8-attract-start`,
`gp-u5-charsel` and `gp-u8-handicap`, so a write within ~0.6 tick of a 1/120 s
boundary can round to the adjacent label. The declared **whole-tick model**:
a tick group (or one write of it) may match the capture's group one tick
earlier or later (the tool's group alignment). The write content and order across ticks
are exact; a dropped, extra or wrong write has no matching group and fails at
its tick. The one-tick slack may not swallow a write: a whole-group shift is
allowed only when the skipped group is empty, and the alignment ends with
`matched == port_writes`, so every port write is accounted for exactly once. A
dropped, extra or wrong write therefore fails even when the capture's groups
are one tick off.

The **per-tick statistic** is `per_tick_equal`: the number of leading claimed
relative ticks whose reduced `(register, value)` multisets are equal (ticks
`0..per_tick_equal-1`; the tool prints "per-tick multiset equal through tick
`per_tick_equal-1`"). It is a reported strength, not the pass/fail rule (the
group alignment is). Scoped exactly: the **selmo section** is per-tick exact
for all its claimed ticks in **24 of the 26 music traces**; the exceptions are
`gp-u8-attract-start` (the tick-57 note-off jitter, §M4.4 item 6) and
`gp-u23-u16-finisher` (the capture's tick split, §M4.4 item 2).
`gp-u22-diablo-ending`'s **title** has a separate label-slack move; its selmo
is exact. The count is not a whole-trace exactness figure.

**Boundary.** Only a section's final tick `last_rel` is the request boundary
and is not claimed (the claims are `0..last_rel-1`; tick `last_rel` is not in
the compared groups and its equality is reported for information). A
difference at `last_rel-1`, the last *claimed* tick, is a real first difference:
it fails unless the section's bank is an explicit `allow-diff` named gap. The
capture's writes after the claim are its own tail and are not compared.

**Named gaps.** A section that cannot be anchored is a named gap when its bank
hash is in the measured `TRACE_PINS` `allow-unanchored` list; a section that
anchors but differs on a measured capture-side artifact is a named gap when it
is in the `allow-diff` list (§M4.4 item 2). Anything else fails. A section with
one tick (the replay's cut at the next song's start) has no claim and is
reported by name; `--min-ticks` is what stops a trace shrinking to nothing.
An absent capture exits 3 and the caller prints the skip by name.

### §M4.2 The measured comparison on `gp-u5-charsel`

The title section: anchor `0xB0=0x2B` at capture 0 ms (port frame 245, song
tick 60), 30 claimed ticks, 38 reduced writes, per-tick multiset equal through
tick 29; its final tick 30 is equal but not claimed. The selmo section:
anchor `0xB4=0x2B` at 7,156 ms (port frame 658, song tick 50), **866 claimed
ticks** (0..865), **1,703 reduced writes**, per-tick multiset equal through
tick 865, boundary tick 866 differs (the port's stop burst is 9 music ticks
before the capture's, §M3.4's clock measurement). The ordered streams differ
first at reduced write **175** (relative tick 74): port `0xB2=0x0A` then
`0xB4=0x0B`, capture `0xB4=0x0B` then `0xB2=0x0A` — the same registers, same
values, same tick, reversed order: the declared intra-tick order model,
unchanged from §M3.4. Task 3's 1,738 reduced writes counted the boundary tick
and skipped excluded writes in the shadow; the tool's 1,703 is the claimed
window under the corrected shadow, and the per-tick equality and the first
ordered difference are the same measurement.

The selmo section is the second section in the port's stream for every
standard trace; its length is the character-select/menu window the capture
holds (146..1,581 ticks). Later sections (`0x31`, the match songs) are only
comparable when the driver's voice cursor still matches at their first
key-on; most captures' cursors advanced by the boundary's extra note, so they
are named gaps (§M4.4 item 1).

### §M4.3 The 29 gameplay traces

The inventory is the measured §M1.6.2 list: 28 `gp-*` directories, 29 `.dro`
files (gp-pads holds `prage_000` and `prage_001`). Each trace's replayed cut is
`min(1400, the capture's X frame)`; the cut is enough for the sections below
and keeps the oracle inside the phase's budget. The table is the oracle run of
§M4.5 on the phase tree; the pinned minimum is in §M4.5. "Claimed" is the
section's bank/offset, capture anchor ms and claimed ticks; "named gap" is a
section the capture cannot carry, with its bank hash.

| trace | .dro | claimed | named gap | pin |
|---|---|---|---:|---:|
| `gp-idle-loss` | prage_000 | s16title+0x36102 (FD94B4E3) @0: 30; s16selmo+0x25C (286ED662) @7256: 1468 | — | 1498 |
| `gp-idle-loss-run2` | prage_000 | title @0: 30; selmo+0x25C (286ED662) @7157: 1478 | — | 1508 |
| `gp-keys-fight` | prage_000 | title @0: 30; selmo+0x25C @7157: 1478 | — | 1508 |
| `gp-pads` | prage_000, prage_001 | title @0: 30 | — (no in-match music; the prage_001 restart fragment anchors nothing) | 30 |
| `gp-twop` | prage_000 | title @0: 30; selmo+0x25C @7032: 446; selmo+0x8 (F1774E70) @11364: 480 | s16citys+0x8 (20DA829D) | 956 |
| `gp-u10-ending` | prage_000 | title @0: 30; selmo+0x25C @7033: 146 | selmo+0x8 (F1774E70); s16citys+0x8 (20DA829D); s16sound+0x5B88 (3BA4C376) | 176 |
| `gp-u12-endurance` | prage_000 | title @0: 30; selmo+0x25C @10684: 926; selmo+0x8 @19009: 72 | — | 1028 |
| `gp-u13-ending2` | prage_000 | title @0: 30; selmo+0x25C @7000: 326 | selmo+0x8 (F1774E70); s16jungl+0x8 (BC44EF50); s16sound+0x5B88 (3BA4C376) | 356 |
| `gp-u15-join` | prage_000 | title @0: 30; selmo+0x25C @7257: 1468 | — | 1498 |
| `gp-u16-twop2` | prage_000 | title @0: 30; selmo+0x25C @7157: 446; selmo+0x8 @11493: 480 | s16citys+0x8 (20DA829D) | 956 |
| `gp-u17-device2` | prage_000 | skip: no frame-keyed port script (wall-paced, §H6.9); the `.dro` is the 248 ms boot fragment | — | 0 |
| `gp-u18-talon-ending` | prage_000 | title @0: 30; selmo+0x25C @7100: 206; selmo+0x8 @9424: 480; s16beach+0x8 (87E7C2DE) @14825: 248 | s16sound+0x5B88 (3BA4C376) | 964 |
| `gp-u19-vertigo-ending` | prage_000 | title @0: 30; selmo+0x25C @7101: 266 | selmo+0x8 (F1774E70); s16sound+0x3E64 (3D536DDE); s16sound+0x5B88 (3BA4C376) | 296 |
| `gp-u20-chaos-ending` | prage_000 | title @0: 30; selmo+0x25C @7032: 386 | selmo+0x8 (F1774E70); s16caves+0x8 (86946EB7); s16sound+0x5B88 (3BA4C376) | 416 |
| `gp-u21-armadon-ending` | prage_000 | title @0: 30; selmo+0x25C @6716: 446; selmo+0x8 @11048: 480; s16jungl+0x8 (BC44EF50) @16425: 246 | s16sound+0x5B88 (3BA4C376) | 1200 |
| `gp-u22-diablo-ending` | prage_000 | selmo+0x25C @6049: 506; selmo+0x8 @10885: 480 | title (FD94B4E3; the capture stop on the last claimed tick, §M4.4 item 5); s16himal+0x8 (BF1C78E2); s16sound+0x5B88 (3BA4C376) | 986 |
| `gp-u23-u16-finisher` | prage_000 | title @0: 30 | selmo+0x25C (286ED662, diff); selmo+0x8 (F1774E70, diff); s16citys+0x8 (20DA829D) | 30 |
| `gp-u5-charsel` | prage_000 | title @0: 30; selmo+0x25C @7156: 866 | selmo+0x8 (F1774E70); s16beach+0x8 (87E7C2DE, one tick at the cut) | 896 |
| `gp-u6-moves` | prage_000 | title @0: 30; selmo+0x25C @7184: 1478 | — | 1508 |
| `gp-u6-moves-b` | prage_000 | title @0: 30; selmo+0x25C @7206: 1474 | — | 1504 |
| `gp-u8-attract-start` | prage_000 | title @0: 30; selmo+0x25C @1684: 1633 | selmo+0x8 (F1774E70) | 1663 |
| `gp-u8-endurance` | prage_000 | title @0: 30; selmo+0x25C @10825: 583 | — | 613 |
| `gp-u8-handicap` | prage_000 | title @0: 30; selmo+0x25C @6316: 1581 | — | 1611 |
| `gp-u8-left-training` | prage_000 | title @0: 30; selmo+0x25C @7548: 1433 | — | 1463 |
| `gp-u8-right-arcade` | prage_000 | title @0: 30; selmo+0x25C @6700: 1533 | — | 1563 |
| `gp-u8-right-training` | prage_000 | title @0: 30; selmo+0x25C @8817: 1281 | — | 1311 |
| `gp-u8-tug-of-war` | prage_000 | title @0: 30; selmo+0x25C @9793: 1166 | — | 1196 |
| `gp-u9-win` | prage_000 | title @0: 30; selmo+0x25C @7157: 146 | selmo+0x8 (F1774E70); s16citys+0x8 (20DA829D); s16sound+0x5B88 (3BA4C376) | 176 |

All 29 gameplay files are accounted for: 26 have a music section (§M1.6.2's
measured count), `gp-pads`' and `gp-u17-device2`'s boot fragments and
`gp-pads/prage_001` hold none. The 8 other k11 `.dro` files
(`de/prage_000`, `diags/prage_000`, `idle/prage_000`, `idle/prage_001`,
`menuesc/prage_000`, `save-reload/prage_000`, `walk/prage_000`,
`walk/prage_001`) and the 2 title captures (`data/audio-captures/prage_000.dro`,
`prage_001.dro`) are not gameplay and the summary names them apart. The
section-to-bank attribution is the measured output above: each claimed
section's bank handle is the port's requested song, and the capture window at
its anchor is attributed to it because every claimed tick's reduced multiset
matches.

### §M4.4 The measured model differences (named gaps)

1. **The section-boundary voice cursor (the unanchored later sections).**
   The driver's voice-rotation cursor (`S.next`, the SBPRO2.MDI `0x1408` over
   `0x1a49`, §M3.3) advances on every allocated note and is not reset by a
   song start. The game's next-song request is a game-frame event, so the port
   can stop a song 1..9 music ticks before the capture does; a note in the
   capture's tail advances the capture's cursor, and the port's later section's
   first key-on is then on different OPL channels. Measure: `gp-u5-charsel`'s
   selmo plays one more capture key-on at 14,409 ms (`0xB6=0x2B`, capture
   channel 6) after the port's 866-tick window, so the port's `0x31` section's
   first key-on is `0xB5=0x36` where the capture's is `0xB7=0x36` (two OPL
   channels higher). No anchor candidate matches the port's first 4 tick
   groups, and the section is a named gap by its bank hash. The same cause
   names the `0x31`/match sections in the traces above; the sections that do
   still anchor (`gp-twop`, `gp-u16-twop2`, `gp-u12-endurance`,
   `gp-u18-talon-ending`, `gp-u21-armadon-ending`, `gp-u22-diablo-ending`) are
   compared and match (446..480 ticks for `0x31`), so this is not a blanket
   skip.
2. **The `gp-u23-u16-finisher` capture's tick split (the two diff-allowed
   sections).** The port's reduced selmo stream is **identical** to
   `gp-u16-twop2`'s (901 writes each; the `0x31` stream likewise 281 each), and
   the u23 port matches the **u16 capture** at both sections (446 and 480
   ticks, §M4.5's cross-check). The u23 capture's own playback differs:
   at its selmo tick 25 it splits the note's writes between ticks 25 and 26 and
   records a carrier-TL `0x145=0x20` the port does not write (the port's
   hardware value for that register was already `0x20`, so its write-on-change
   suppression differs from the capture's). The first diff is at relative tick
   25 (port group 23 writes vs capture 14) and at tick 21 in the `0x31`
   section. These two sections are `diff-allowed` by hash; the trace's claim is
   its 30-tick title, and the same music is claimed tolerance-free by
   `gp-u16-twop2`.
3. **`gp-u17-device2`.** The scenario is the wall-paced service-menu walk
   (§H6.9); `gp_session.port-script` refuses it by design, and its `.dro` is
   the 248 ms boot fragment (185 writes, no music). It is skipped by name; no
   claim is made.
4. **`gp-pads`.** The MAIN MENU path never reaches the character select, so
   the port's stream has only the title section; both `.dro` files are boot
   fragments/restart fragments with no in-match music. The title's 30 ticks are
   claimed; the capture's missing selmo is not a port silence.
5. **The section-end tick (only `last_rel`).** The claims end at
   `last_rel-1`; tick `last_rel` alone is unclaimed. `gp-u22-diablo-ending`'s
   title is the one section whose capture stop burst lands on the last
   *claimed* tick (capture rel 5, the port's boundary rel 6): that is a
   one-tick stop difference inside the claimed window, so the title is an
   explicit `allow-diff` named gap (FD94B4E3), not a partial claim. The earlier
   behaviour excused `last_rel-1` as a boundary; that was the review's
   Important-1 defect and is fixed (the penultimate-tick mutation test fails).
6. **The capture label jitter.** The one-tick model of §M4.1 is a measured,
   not fitted, allowance: `gp-u8-attract-start`'s selmo has 2 label-slack
   moves (the note-off at 57 vs 58), and its per-tick equality stops at tick
   56; the claim continues under the group model (1,633 ticks).
   `gp-u22-diablo-ending`'s title has one label-slack move (the key-off at rel
   4 vs 5) and then the explicit stop difference at the last claimed tick
   (§M4.4 item 5): it is the `allow-diff` named gap, not a continuing claim.
   `gp-u23-u16-finisher`'s selmo is the separate capture tick split (§M4.4
   item 2).
7. **The banks: requested vs capture-matched, and what the cut windows do
   not cover.** The replayed port sections **requested 10 distinct banks**:
   `s16title+0x36102` (FD94B4E3, 0x54/0x56), `s16selmo+0x25C` (286ED662,
   0x2E/0x30), `s16selmo+0x08` (F1774E70, 0x31), `s16beach+0x08`
   (87E7C2DE, 0x20), `s16citys+0x08` (20DA829D, 0x1D), `s16himal+0x08`
   (BF1C78E2, 0x1E), `s16caves+0x08` (86946EB7, 0x1F), `s16jungl+0x08`
   (BC44EF50, 0x1B), `s16sound+0x5B88` (3BA4C376, 0x23) and
   `s16sound+0x3E64` (3D536DDE, 0x21). Only **5 of the 10 are
   capture-matched** in the measured cut windows: title, selmo+0x25C,
   selmo+0x08, beach+0x08 and jungl+0x08. `s16citys+0x08`, `s16himal+0x08`,
   `s16caves+0x08`, `s16sound+0x5B88` and `s16sound+0x3E64` are requested but
   appear only as named gaps (unanchored/diff), so they are **not claimed
   capture sections**. The other **16 distinct banks** of §M1.6.7's 26-bank
   code-reachable set are **not requested or attributed in the measured cut
   windows**: `s16stone+0x08` (0x1C), `s16snd2+0x21BFA` (0x32),
   `s16sound+0x6EC8` (0xDF), `s16conti+0x08` (0x2C), `s16sound+0x39F4`
   (0x3D), `s16sound+0x335E` (0x3B), `s16hghsc+0x08` (0xE1/0xE3),
   `s16grave+0x1A10` (0x25), `s16grave+0x0A8C` (0x26),
   `s16grave+0x05E4` (0x27), `s16sound+0x08` (0x28), `s16grave+0x08` (0x2A),
   `s16sound+0x6CA8` (0x24), `s16sound+0x7714` (0xE4),
   `s16sound+0x72D4` (0xEC) and `s16konsd+0x1CCB0` (0xEB). This is **not** a
   claim that the complete traces never play them: the oracle compares the
   cut replay windows (AUDIO_END 1400 frames or the capture's X), and the
   unmatched capture tails were not measured. They are named gaps: a bank not
   covered/attributed in the measured cut windows is not claimed.

### §M4.5 Pins, tests and the gate

**The pins.** `tools/audio_compare.py`'s `TRACE_PINS` holds, per trace, the
minimum total claimed ticks and the two named-gap lists. The minima are the
total of two full oracle runs on the phase tree; **both runs produced every
value byte-identically**. The review fix round re-measured on its own oracle
run: every total equaled its pin except `gp-u18-talon-ending`, which was
longer (964 pin -> 966); `gp-u21-armadon-ending` measured 1,200 in the fix
run (as at the pin) and 1,202 in the earlier third run. A minimum pin accepts
a longer claim. `gp-u22-diablo-ending`
moved 991 -> **986** under Important 1: its title's stop burst is on the last
*claimed* tick (capture rel 5, port boundary rel 6), so the title is an
explicit `allow-diff` named gap (FD94B4E3) instead of a 5-tick claim.

**The +1 failure table.** Each pin is proved one step above against the
artifact that measured it (`/tmp/pr_audio_dump2/<scenario>.txt`, the second
run; the `--min-ticks` argument overrides the table's pin for the proof only):

```
for s in <the 27 pinned traces>; do
  min=$(python3 -c "... TRACE_PINS[s][0]")
  python3 tools/audio_compare.py compare --name "$s" --port /tmp/pr_audio_dump2/$s.txt \
    $(for f in $(python3 tools/audio_compare.py expected-files --scenario $s); do \
        echo --capture data/k11-captures/$s/$f; done) \
    --game-dir data/game/C --min-ticks $((min+1))
done
```

| scenario | pin | +1 | claimed | rc |
|---|---:|---:|---:|---:|
| gp-idle-loss | 1498 | 1499 | 1498 | 1 |
| gp-idle-loss-run2 | 1508 | 1509 | 1508 | 1 |
| gp-keys-fight | 1508 | 1509 | 1508 | 1 |
| gp-pads | 30 | 31 | 30 | 1 |
| gp-twop | 956 | 957 | 956 | 1 |
| gp-u10-ending | 176 | 177 | 176 | 1 |
| gp-u12-endurance | 1028 | 1029 | 1028 | 1 |
| gp-u13-ending2 | 356 | 357 | 356 | 1 |
| gp-u15-join | 1498 | 1499 | 1498 | 1 |
| gp-u16-twop2 | 956 | 957 | 956 | 1 |
| gp-u18-talon-ending | 964 | 965 | 964 | 1 |
| gp-u19-vertigo-ending | 296 | 297 | 296 | 1 |
| gp-u20-chaos-ending | 416 | 417 | 416 | 1 |
| gp-u21-armadon-ending | 1200 | 1201 | 1200 | 1 |
| gp-u22-diablo-ending | 986 | 987 | 986 | 1 |
| gp-u23-u16-finisher | 30 | 31 | 30 | 1 |
| gp-u5-charsel | 896 | 897 | 896 | 1 |
| gp-u6-moves | 1508 | 1509 | 1508 | 1 |
| gp-u6-moves-b | 1504 | 1505 | 1504 | 1 |
| gp-u8-attract-start | 1663 | 1664 | 1663 | 1 |
| gp-u8-endurance | 613 | 614 | 613 | 1 |
| gp-u8-handicap | 1611 | 1612 | 1611 | 1 |
| gp-u8-left-training | 1463 | 1464 | 1463 | 1 |
| gp-u8-right-arcade | 1563 | 1564 | 1563 | 1 |
| gp-u8-right-training | 1311 | 1312 | 1311 | 1 |
| gp-u8-tug-of-war | 1196 | 1197 | 1196 | 1 |
| gp-u9-win | 176 | 177 | 176 | 1 |

Each failed line is exactly `claimed <pin> ticks, pinned minimum <pin+1>`; the
longer gp-u21 run (1,202) and gp-u18 run (966) pass the minimum pins. No pin
needed lowering: every shipped pin is the measured minimum of the artifact
used for its +1 proof.

**RED/GREEN.** The first RED was taken with the tool absent (the task starts
with no comparator): 13 tests, all failed (`can't open file ...`). The fix
round's REDs were taken against the pre-fix `9511c77e` tool:

  * **The reviewer's exact case is port-side.**
    `test_align_groups_port_side_dropped_write` is the group-level case:
    port `[A, B, C, D, E, F, G]`, capture `[A, ∅, B, C, D, E, F, G]` gives
    `ok=True, claimed=7`; deleting the **port's** B and retaining its empty
    tick gave `ok=True, claimed=7` on the pre-fix `9511c77e` tool
    (`.../old_audio_compare.py` import, output captured in the fix report)
    because branch1 (`pg == nxt_c`) skipped the capture's non-empty B; the
    fixed tool returns `ok=False, claimed=2, first_tick=2`. This is the
    branch1 `and not cg` guard's regression.
  * `test_dropped_write_port_side_one_tick_later_fails` is the same mutation
    through the public comparator: the old tool returned `synthetic: ok`
    (8 claimed ticks, 7 reduced writes); the fixed tool fails it.
  * The capture-side counterparts are not the reviewer's exact case:
    `test_dropped_write_capture_side_one_tick_later_fails` and
    `test_dropped_write_capture_side_one_tick_earlier_fails` both delete the
    capture's B, so the mutated capture no longer matches the anchor's first
    four tick groups; both the pre-fix and the fixed tool report the section
    **unanchored**. They are anchor-prefix cases, not tick-group cases, and they
    do not isolate branch2's `and not pg` guard (a skipped non-empty port group
    also fails the `matched == port_writes` accounting, so that guard is
    redundant; no shipped case isolates it — a test minor, not changed here).
    `test_dropped_write_capture_groups_one_tick_earlier_fails` is the
    public-comparator shape the pre-fix tool accepted at its tick groups and the
    fixed tool refuses.
  * `test_penultimate_tick_difference_fails` — the old tool excused the
    `last_rel-1` difference as `the first group difference is at the request
    boundary tick 6` and returned `ok`; the fixed tool fails it at tick 6.
  * `test_partial_capture_loss_fails`, the summary inventory/NOT RUN/FAIL
    cases, and `test_expected_files_inventory` cover Important 3.

The module is now **25 tests**; the fixed tool passes all 25.

**The whole-group fix.** Both branches now require the skipped group to be
empty (`pg == nxt_c and not cg`; `cg == nxt_p and not pg`). The loop branches
are what enforce the write accounting: a matched group adds its size, a single
moved write adds one, and a skipped group must be empty (it adds nothing), so
a dropped/extra write cannot ride the one-tick slack. The final
`matched == port_writes` check is an **assertion/safety net**, not the
enforcement. The `first_diff` reports the tick's groups.

**The boundary fix.** Only `last_rel` is unclaimed. The `boundary_note` branch
that excused `last_rel-1` is removed; a difference there is `diff` or an
explicit `allow-diff` (`gp-u22-diablo-ending`'s title, FD94B4E3). The summary
line still reports the boundary tick `last_rel`'s equality for information.

**The inventory/gating fixes.** `audio-one`:
  * an absent capture directory still writes a `no capture` skip report and
    exits 0 (the declared skip);
  * a present directory without an X record fails (`no X record`);
  * `gp_session.py port-script` failures fail, except `gp-u17-device2` when
    the error is the specific `wall-paced steps` refusal (§H6.9);
  * the capture arguments come from `audio_compare.py expected-files`
    (28 directories, 29 files; gp-pads has two), so a missing file reaches
    the tool and fails as `MISSING capture file(s)`;
  * `summary` fails on a present directory whose file set differs from the
    expected inventory (naming the missing/extra file), a present scenario
    with no report (`NOT RUN`), a report with `ok:false` (`FAIL`), a
    malformed `.dro`, and a skip other than gp-u17's wall-paced one; the
    absent-directory skip is the one non-failing absence. The summary's
    inventory line prints `29/29` when complete.

**Mutation proof on a real trace.** Dropping one write from the
`gp-u5-charsel` selmo's claimed window (`audio.txt` write 1200 removed) fails:
`FIRST DIFFERENCE (tick groups differ) at port relative tick 374: port [...
0xf3=0x00] vs capture [... 0xf3=0x00 0x1b8=0x07]`, then `claimed 30 ticks,
pinned minimum 896`; exit 1. Changing the title's first key-on to
`0xB0=0x2C` fails as unanchored (`no capture key-on 0xb0=0x2c with matching
first 4 ticks (0 candidates)`; exit 1).

**Cross-check (the `gp-u23` cause).** `gp-u23-u16-finisher`'s port stream
against `gp-u16-twop2`'s capture: title 30, selmo+0x25C 446, selmo+0x8 480 —
all matched, no allowances. `gp-u16-twop2`'s port against the u23 capture
fails at the same two sections. The two port streams are identical; the u23
capture is the differing side.

**The Makefile.** `make audio-oracle` (`AUDIO_DUMP=/tmp/pr_audio_dump`,
`AUDIO_END=1400`, `AUDIO_JOBS=4`) loops the 28 scenario names through
`audio-one`, which cuts each capture's script at `min(AUDIO_END, X)`, runs
`PR_GP_DUMP`/`PR_AUDIO_DUMP`, and compares; the summary prints the 29-file
inventory, each trace's total/pin/named gaps, the 8+2 non-gameplay files and
the minimum measured claim. The 28 parallel cut replays take about **3m** on
the phase host. `make verify` runs it after `gp-ending-oracle` and before
`diff-verify`, and the unit module is in the verify python list. A cut script
is a prefix of the full replay, so the driver's `PR_GP_DUMP` fn-miss check
(`count <= want`, `dropped == 0`) passes; the run's own `(cut at N)` header
selects `cut`.

### §M4.6 What §M4 does not claim

Rendered audibility (the OPL core/mixer model, §M1.7); a bank not
covered/attributed in the measured cut windows (a named gap, not a new
capture); the capture tails the cut does not reach; the sections named in
§M4.4;
the capture's ms timeline as a port/frame clock (§M1.6.5); the inside-tick
write order (declared model); and the exact tick label of a write within the
declared one-tick capture-label model. The claim is the register content,
order across ticks, song choice and the per-section bank attribution in the
captured windows.

## §M5 PCM determinism (Task 5)

### §M5.1 The selected song, the volume and the render path

The measured song is the Task-3 selmo: the bank the character screen requests
as handle `0x0980025C` (INDEX 19 = `s16selmo.gra`, offset `0x25C`; §M3.1 and
§M1.6.4). The handle offset names the block's size dword, `0x780` at file
offset `0x25C`; the game copies those 1,920 bytes from the dword's end into its
`0x5100` song buffer (`0x1C961..0x1C977`), and the `game_music_bank_find` scan
`snd_music_bank` runs finds the block's `FORM XMID` at file offset `0x282`
(fsz `0x756`, so `seq_bank_size` = `0x75E` = 1,886 bytes). The test reads the
same block from the shipped file (`selmo_song_bank`, `test_audio.c`), and
`seq_loaded_bank_hash()` is asserted to be **`0x286ED662`** — §M3.1's measured
bank hash for `dc=0x0980025C` — so the render is pinned to the Task-3 song, not
to any bank the file happens to contain first.

**The volume.** The game path's sequence volume is `DS_000A2CB8`, passed to
`AIL_set_sequence_volume` at `0x1C9A5`/`0x1C9F0`; the replayed captures hold
`vol=0x50` throughout (§M3.1). `0x2C8F0`'s unscaled arm derives it from config
field `0x35`'s default `0xA0` as `0xA0 >> 1 = 0x50`, and the scaled arm gives
the same `0x50` at the default scale 3 (`(0xA0 * 3) / 3 >> 1`). The render sets
**`0x50`** explicitly (`GAME_SEQUENCE_VOLUME`, `test_audio.c`) instead of
inheriting the title oracle's `0x54` test setting (§M1.7's omission).
**Declared difference:** the game's `AIL_set_sequence_volume(..., 500)` fades
from `AIL_init_sequence`'s `0x7F` seed over 500 ms; the render applies the
target `0x50` at once. The pinned artifact is the port's own deterministic
render, not the AIL fade model.

**The render.** `PR_AUDIO_SONG_WAV=<path>` renders 12 s (fixed;
`SELMO_WAV_SECONDS`, independent of the title's `AUDIO_SECONDS` knob) through
the same sequencer + OPL core + mixer path as the title render, paced by the
rendered audio at 120 sequencer ticks/s. Format is the title's: PCM 16-bit
stereo, 49,716 Hz, 596,592 frames, 2,386,368 data bytes, 2,386,412 file bytes.
`make audio-render` sets both `PR_AUDIO_WAV` and `PR_AUDIO_SONG_WAV`; the title
render block is unchanged.

**The pristine driver.** The render calls `seq_driver_reset()` before
`seq_start`, restoring the FM driver's install-time voice cursor. The cursor is
driver state that persists across songs (§M3.3), so without the reset the song
WAV depends on how long the title render before it ran: with `AUDIO_SECONDS=12`
the song sha was `784fb66f…`, with `AUDIO_SECONDS=2` it was `a4cf3512…` (same
12 s song, different channel assignment). With the reset the song WAV is the
same for `AUDIO_SECONDS=12` and `2` (`530a9430…`), so the pin is independent of
the title's listen-length knob. The pinned artifact is this pristine-driver
stream; the game path itself does not call the seam.

### §M5.2 The measured WAV and the R4 decision

| check | measured |
|---|---|
| peak absolute sample | **2,790** |
| non-zero samples | 1,149,339 of 1,193,184 |
| sha256 (run 1, `AUDIO_SECONDS=12`) | `530a94301f4be46f9a5c666c94516f8a1f7fca6f136d3be0562faed802c47cef` |
| sha256 (run 2, `AUDIO_SECONDS=12`) | `530a94301f4be46f9a5c666c94516f8a1f7fca6f136d3be0562faed802c47cef` |
| sha256 (run 3, `AUDIO_SECONDS=2`) | `530a94301f4be46f9a5c666c94516f8a1f7fca6f136d3be0562faed802c47cef` |
| `cmp` run 1 vs run 2 | byte-identical |

**R4:** the two default runs on this host are byte-identical (the third run
varies `AUDIO_SECONDS`, which exercises the pristine-driver reset above), so
the sha256 is pinned (`SELMO_WAV_SHA256`) together with the peak
(`SELMO_WAV_PEAK`). The claim is the port's own deterministic non-silence, not
audibility and not a match to the original's PCM (§M1.7).

**The title render is unchanged by Task 5.** The pre-Task-5 tree and the
Task-5 tree both write `515b7c94a9d0cfb36afa183f9e6642c04ff8ac0150a1f324182cee6bb8d82f3a`
(peak 4,090, 1,122,775 non-zero of 1,193,184; `cmp` identical). §M1.7's
`df74acfb…`/3,402 is the Task-1 baseline and is not the current branch's value;
the difference predates Task 5 and is consistent with §M3.3's voice-cursor
persistence fix (`seq_start` no longer resets `S.next`; the render follows the
oracle's 4,096-tick replay). It is recorded here, not claimed by Task 5.

**The empty song.** The same block with no song bank (the state before the
selection was implemented, and the mutation re-run below) renders exact
silence: peak **0**, sha256
`71193adec3906987ba36200dc465f71b677dd5978dd90136eae3f4a5d1d9f634` (the
all-zero 12 s body — deterministic, and the first RED's evidence). The
non-silence check and the pinned peak/hash reject it.
### §M5.3 RED, GREEN and the mutation proofs

The test block was written before the song selection existed:

```
$ make audio-render AUDIO_WAV=/tmp/pr_title_red.wav AUDIO_SONG_WAV=/tmp/pr_selmo_red.wav
PR_AUDIO_WAV: wrote /tmp/pr_title_red.wav (12 s, 49716 Hz)
FAIL .../port/tests/test_audio.c:1476: the selected song's render is not silent
PR_AUDIO_SONG_WAV: wrote /tmp/pr_selmo_red.wav (12 s, 49716 Hz) peak 0 sha256 71193adec3906987ba36200dc465f71b677dd5978dd90136eae3f4a5d1d9f634
FAILURES: 1
```

The failure is the expected one: the empty (not yet selected) song renders
exact silence, which the old check (only `fopen`) cannot see. The first GREEN
with the selmo bank selected (before the driver reset below) was peak 2,702,
sha `784fb66f…`; then the `AUDIO_SECONDS` experiment showed the artifact
depended on the preceding title render's length (12 s title: `784fb66f…`; 2 s
title: `a4cf3512…`), which is why the render now calls `seq_driver_reset`
(§M5.1). The final artifact:

```
$ make audio-render AUDIO_WAV=/tmp/pr_title_g1.wav AUDIO_SONG_WAV=/tmp/pr_selmo_g1.wav
PR_AUDIO_WAV: wrote /tmp/pr_title_g1.wav (12 s, 49716 Hz)
PR_AUDIO_SONG_WAV: wrote /tmp/pr_selmo_g1.wav (12 s, 49716 Hz) peak 2790 sha256 530a94301f4be46f9a5c666c94516f8a1f7fca6f136d3be0562faed802c47cef
all checks passed
```

The pin is not vacuous:

- **Silent-song mutation** (the bank lookup forced to NULL with the pins in
  place): four failures — `the s16selmo song bank resolves`,
  `the selected song's render is not silent`, `0 != 2790`, and the sha
  mismatch — at `test_audio.c:1492/1515/1516/1518`; peak 0, sha
  `71193ade…`.
- **Volume mutation** (`GAME_SEQUENCE_VOLUME` `0x50` -> `0x4F`): the peak
  stays 2,790 but the sha becomes
  `44222b27278fa5e992b0e54a087d28f95033494d83fbaded6a70436df6fb2266` and the
  sha check fails (`FAILURES: 1`). The provenance assertion
  (`seq_loaded_bank_hash == 0x286ED662`) is the guard against rendering a
  different bank.

The initial RED's line number is as captured; the final checks are at
`1492/1515/1516/1518` after the song-selection and driver-reset code was
added.

`make verify` does not run `audio-render` (the design's G3 gate is the
`make audio-render` target); the unit suite without `PR_AUDIO_SONG_WAV` is
unchanged.

### §M5.4 What §M5 does not claim

Rendered audibility or any PCM comparison to the original (the OPL core and
mixer are the invented model, §M1.7); the AIL sequence-volume fade (declared
above); the title render's hash as a Task-5 artifact (recorded, unchanged); and
any song other than the Task-3 selmo. The claim is one deterministic,
non-silent render of the measured menu song at the game path's sequence volume.

## §M6 The docs and the full gate (Task 7)

### §M6.1 The claim set the docs carry

The functional claim is that the game's own music requests select and start the
original's song. `snd_music_request` (`0x1CA14`) now queues one: `game_audio_init`
stores the non-zero stand-in `1` at `DS_001028C0`/`DS_001028C4` (the
install-succeeded form the DIG path already used at `0x1028C8`), so its
`DS_001028C0 != 0` gate (`0x1CA2C`) opens; `game_audio_service` runs the raw's
pending arm (`0x1CF30`/`0x1CF39`); and `snd_music_start` ports `0x1C930`'s
new-song arm (resolve `0x1B544`, copy to the raw's `0x5100` buffer at `0x1028D0`,
`AIL_init_sequence`/`AIL_set_sequence_volume(DS_000A2CB8, 500)`/
`AIL_start_sequence`) and same-song arm (re-init of the copied buffer). The title
path starts S16TITLE once (`game_state_title_music_request` suppresses the
`title_music_start` substitute when the attract's `0x54`/`0x56` request is
pending or last started; §M2.8). A real sequencer bug is fixed: `seq_start` no
longer resets the FM driver's voice-rotation cursor (§M3.3). The nine ending
captures' loader `OVERLAY_ALLOWED` entries retired as clean matches and their
`MAX_MISSING`/`MIN_REACH` pins were re-measured (§M2.7).

The evidence claim is the register stream: `tools/audio_compare.py` compares the
port's `PR_AUDIO_DUMP` writes against the 29 gameplay `gp-*` `.dro` traces,
tolerance-free across ticks (the declared intra-tick order model and the measured
one-tick capture-label model; a skipped group must be empty and every port write
matched once), and the selmo section is per-tick exact in 24 of the 26 music
traces (§M4). The PCM pin is `make audio-render`'s selected selmo (peak 2,790,
sha256 `530a9430…`; §M5). **No audibility claim is made**, and no PCM comparison
to the original is claimed.

The docs: `README.md` (the in-match/menu sentence; the title stays 66% and the
"799 of Ghidra's 1203" / "100% (759 of 759)" lines are the measured
`port_progress.py` figures), `AGENTS.md` (the phase bullet), `docs/PROGRESS.md`
(the phase paragraph and the comparator-count correction), `port/spec/audio.md`
(the status note and the "Music truth" section), the plan (Task 7 and the Global
Constraints now name both gate commands) and this record (§M6). The spec's status
is `Implemented and gated, 2026-10-26` and its §2 is labelled the pre-phase
baseline. `port/RE_GUIDE.md` needed no change (it carries no music claim). The
Makefile's U9 current-state comment was the one stale sentence: "the port's music
path is host-owned and never resolves" became "was then host-owned and never
resolved", followed by the existing §M2.7 retirement note.

### §M6.2 The Task 6 declaration

The plan's Task 6 is optional ("take only if Tasks 1–5 leave budget; otherwise
declare them"). It is **declared, not taken**:

- **The service-menu language reload (`§K11.3`)** — not implemented. The
  language field (`0x29` bits 24–26), the `<LANG>.TXT` reseat and the
  idempotence removal remain a future small gap.
- **The coin-countdown capture** — deferred to a future phase. It needs a new
  capture, and the phase's binding no-new-capture constraint forbids one
  (ledger ruling R1); the coin evidence stays open.
- **The proof-completion housekeeping** — not taken: the `waitpid` retry/assert,
  the allowance-before-window-start guard and duplicate-key rejection, `PC_DEAD`,
  the `closed_rows` assertion, the 32 `ResourceWarning` contexts,
  `SpanCensusTests`' docstring and the proof-completion design §7 sentence. No
  pin or assertion meaning changes, and none of these is a Music Truth claim.

### §M6.3 The Task 5 pin restated

`make audio-render` (`AUDIO_SONG_WAV=/tmp/pr_selmo_fm.wav`) renders the selected
selmo bank (`0x0980025C`, bank hash `0x286ED662`) at the game path's sequence
volume `0x50`, fails on a silent render, and pins peak **2,790** and whole-file
sha256 `530a94301f4be46f9a5c666c94516f8a1f7fca6f136d3be0562faed802c47cef`.
The two default runs are byte-identical (R4); `seq_driver_reset()` makes the WAV
independent of the title render's `AUDIO_SECONDS` (§M5.1). The claim is
deterministic non-silence, not audibility and not a PCM match to the original
(§M1.7). The title render is unchanged (`515b7c94…`, peak 4,090). The pin is
exercised by `make audio-render`, not by `make verify` (the design's G3), so the
phase's gate runs **both** commands.

### §M6.4 The docs-only corrections from the task reviews

1. `test_audio.c:901` -> `:903` (the §M3.6 mutation location; the Task 3 report
   and §M3.3 already had `:903`).
2. The record's two `test_audio.c:1117` prose citations -> `:1274`, the current
   capture-tick mapping (`u32 ct = (cap_ev[i].tick * 120u + 500u) / 1000u + 60u`).
   The Task 1 review had already flagged that `:1117` was the wrong line at its
   revision (`:1114`).
3. "the other 10 (8 + 2) are not gameplay music" -> "**named apart, not
   gameplay**": the 2 `audio-captures` are the title captures (music, not a
   gameplay scenario) and the 8 other k11 files are service-menu/save/diagnostic.
4. "**Two captures are restarted** after the boot's first write block" -> "Two
   *more* restart fragments" (the third is `idle/prage_001`, which does carry
   music).
5. "`port/spec/audio.md` items 1/9 excludes" -> **item 1** only (item 9 is the
   `0x122` init-shadow artefact, not the carrier-TL exclusion).
6. "Task 3/4 repeat it for the other sections" -> **Task 4 owns the per-section
   attribution** for the other sections.
7. `title_music_start`'s header comment (comment only; the body is unchanged)
   now states that `game_state_title_music_request` suppresses the substitute
   when the raw request is pending or last started, so the attract->title run
   starts the bank once.
8. The branch2 attribution: §M4.5 and the test comments now say the two
   capture-side dropped-write cases fail at the **anchor's four-tick prefix**
   on both the pre-fix and the fixed tool (they are anchor-prefix counterparts,
   not tick-group cases); the branch2 `and not pg` guard is redundant with the
   `matched == port_writes` accounting and no shipped case isolates it (a test
   minor, not changed).
9. The test module's docstring: every case uses the CLI except the direct
   `align_groups` case; the inventory/`summary`/partial-file gating cases and
   the anchor-prefix failure mode are named.
10. The stale comparator count: `docs/PROGRESS.md`'s 23 -> **25** cases.
11. The plan's "the covered songs" -> "the songs whose register streams match in
    the measured cut windows".
12. The Makefile U9 comment tense (above).
13. The spec status -> `Implemented and gated, 2026-10-26`, and §2 labelled the
    pre-phase baseline; the plan's Task 7 and Global Constraints name both gate
    commands.

Not changed (deferred code/test minors, as instructed): the silent trace-write
failures in `test_game.c`'s `PR_AUDIO_DUMP` block, the first-prefix anchor
candidate (a later candidate is not tried after a prefix match), the missing `S`
metadata reuse of the previous song's bank, and the one-start regression that
calls `game_state_title_music_request()` directly instead of through
`game_state_title` (so a revert of the caller is not unit-pinned). The
differential harness's `game_audio_service` `unhit_named` reason text also still
says the port "replaces `0x1C930` by host code (flow.c title_music_start)"; it is
a harness annotation outside this task's named file set (§M2.4 records the
measured boundary), not a claim in the phase docs.

### §M6.5 The final gate (measured)

Command 1, `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify`
(log `/tmp/opencode/task7-verify.log`), **exit 0**, last line
`VERIFY_EXIT=0`, preceded by:

```
gate: captures 34/35 present, 1 missing (strict=1, pins_exact=1)
...
oracle C-vs-Python: 9866 writes byte-exact
capture oracle first difference at C write 430: C tick=1010 reg=0xa0 val=0xd3 vs capture tick=1010 reg=0x1a8 val=0x68 (C 9866 writes, capture 6903 normalised)
...
== register-stream oracle (the 29 gp-* traces; each skips without its capture) ==
audio-compare: gameplay inventory: 28 traces, 29/29 .dro files (26 with a music section), 0 directories absent
  gp-idle-loss             1 file(s): ok 1498 ticks (pinned 1498)
  gp-idle-loss-run2        1 file(s): ok 1508 ticks (pinned 1508)
  gp-keys-fight            1 file(s): ok 1508 ticks (pinned 1508)
  gp-pads                  2 file(s): ok 30 ticks (pinned 30)
  gp-twop                  1 file(s): ok 956 ticks (pinned 956) named-gap 20DA829D
  gp-u10-ending            1 file(s): ok 176 ticks (pinned 176) named-gap F1774E70 20DA829D 3BA4C376
  gp-u12-endurance         1 file(s): ok 1028 ticks (pinned 1028)
  gp-u13-ending2           1 file(s): ok 356 ticks (pinned 356) named-gap F1774E70 BC44EF50 3BA4C376
  gp-u15-join              1 file(s): ok 1498 ticks (pinned 1498)
  gp-u16-twop2             1 file(s): ok 956 ticks (pinned 956) named-gap 20DA829D
  gp-u17-device2           1 file(s): SKIP (no frame-keyed port script (wall-paced; record §H6.9))
  gp-u18-talon-ending      1 file(s): ok 966 ticks (pinned 964) named-gap 3BA4C376
  gp-u19-vertigo-ending    1 file(s): ok 296 ticks (pinned 296) named-gap F1774E70 3D536DDE 3BA4C376
  gp-u20-chaos-ending      1 file(s): ok 416 ticks (pinned 416) named-gap F1774E70 86946EB7 3BA4C376
  gp-u21-armadon-ending    1 file(s): ok 1200 ticks (pinned 1200) named-gap 3BA4C376
  gp-u22-diablo-ending     1 file(s): ok 986 ticks (pinned 986) named-gap FD94B4E3 BF1C78E2 3BA4C376
  gp-u23-u16-finisher      1 file(s): ok 30 ticks (pinned 30) named-gap 286ED662 F1774E70 20DA829D
  gp-u5-charsel            1 file(s): ok 896 ticks (pinned 896) named-gap F1774E70 87E7C2DE
  gp-u6-moves              1 file(s): ok 1508 ticks (pinned 1508)
  gp-u6-moves-b            1 file(s): ok 1504 ticks (pinned 1504)
  gp-u8-attract-start      1 file(s): ok 1663 ticks (pinned 1663) named-gap F1774E70
  gp-u8-endurance          1 file(s): ok 613 ticks (pinned 613)
  gp-u8-handicap           1 file(s): ok 1611 ticks (pinned 1611)
  gp-u8-left-training      1 file(s): ok 1463 ticks (pinned 1463)
  gp-u8-right-arcade       1 file(s): ok 1563 ticks (pinned 1563)
  gp-u8-right-training     1 file(s): ok 1311 ticks (pinned 1311)
  gp-u8-tug-of-war         1 file(s): ok 1196 ticks (pinned 1196)
  gp-u9-win                1 file(s): ok 176 ticks (pinned 176) named-gap F1774E70 20DA829D 3BA4C376
audio-compare: minimum measured claim: 30 ticks
audio-compare: not gameplay: 8 k11 .dro file(s) (de/prage_000.dro, diags/prage_000.dro, idle/prage_000.dro, idle/prage_001.dro, menuesc/prage_000.dro, save-reload/prage_000.dro, walk/prage_000.dro, walk/prage_001.dro)
audio-compare: not gameplay: 2 audio-capture .dro file(s) (prage_000.dro, prage_001.dro)
audio-compare: summary ok
...
diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps; 825/845 rows with callees closed (188 have none); 807/845 closed on full rows; 8 ported functions with no row (45 FN_ ported-unrowed: 8 portable [5 registry-visible + 3 header-shape exceptions] + 37 runtime; the scan-visible subset is 42 = 5 + 37). ...
...
== symbols.h must regenerate byte-identically ==
python3 tools/gen_symbols.py port/decomp port/src/symbols.h
1304 globals, 1206 functions -> port/src/symbols.h
  dropped 11 globals, 1 functions outside the LE objects
all checks passed
```

- **The audio oracle ran, it did not skip:** 27 replayed traces `ok` (the 26
  music traces + title-only `gp-pads`), against the 29/29 file inventory, with
  `gp-u17-device2` the one declared wall-paced skip and `audio-compare: summary
  ok`; there is no `FAIL` line in the log.
- **The differential counter is the Task-1 baseline exactly:** `1033/1033
  functions VERIFIED; 14993/14993 mutants detected; 0 named gaps; 825/845 rows
  with callees closed (188 have none); 807/845 closed on full rows; 8 ported
  functions with no row`. No row was added and `0x1C930`/`0x1CF40` stay
  `UNROWED`.
- **The title/attract claims are unchanged:** the 9866-write byte-exact line and
  the report-only capture first difference 430 are the same registers/values; the
  gate's title/attract/front-end/demo-fight/attract2/gp/K11/save-reload numbers
  are the §M2.5 values.
- `python3 tools/port_progress.py` (re-run after the gate) prints `799 1203 66` /
  `759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)`,
  so the README title stays 66% and its "799 of Ghidra's 1203" / "100% (759 of
  759)" lines are the measured figures.

Command 2, `make audio-render` (log
`/tmp/opencode/task7-audio-render-final.log`), **exit 0**:

```
oracle C-vs-Python: 9866 writes byte-exact
capture oracle first difference at C write 430: C tick=1010 reg=0xa0 val=0xd3 vs capture tick=1010 reg=0x1a8 val=0x68 (C 9866 writes, capture 6903 normalised)
PR_AUDIO_WAV: wrote /tmp/pr_title_fm.wav (12 s, 49716 Hz)
PR_AUDIO_SONG_WAV: wrote /tmp/pr_selmo_fm.wav (12 s, 49716 Hz) peak 2790 sha256 530a94301f4be46f9a5c666c94516f8a1f7fca6f136d3be0562faed802c47cef
all checks passed
```

The song WAV's whole-file sha256 is
`530a94301f4be46f9a5c666c94516f8a1f7fca6f136d3be0562faed802c47cef` (peak
2,790); the title WAV is `515b7c94a9d0cfb36afa183f9e6642c04ff8ac0150a1f324182cee6bb8d82f3a`
(peak 4,090), unchanged. The claim is deterministic non-silence, not audibility.
`make verify` does not run `audio-render`; the phase's final gate is both
commands (design G3, Task 5 review).

### §M6.6 The phase's named gaps and what it does not claim

- **Rendered audibility / PCM vs the original** — not claimed; the OPL core and
  mixer are the port's invented model (§M1.7). The PCM pin is determinism only.
- **Bank coverage in the measured cut windows** — 16 of the 26 code-reachable
  banks are not requested or attributed there; 5 more are requested-only named
  gaps (citys, himal, caves, sound+0x5B88, sound+0x3E64); only title,
  selmo+0x25C, selmo+0x08, beach+0x08 and jungl+0x08 are capture-matched. The
  unmatched capture tails were not measured, so full-trace absence is not
  claimed (§M4.4 item 7).
- **The later `0x31`/match sections** whose section-boundary voice cursor the
  capture's tail advanced; the sections that still anchor (446..480 ticks) match
  (§M4.4 item 1).
- **The `gp-u23-u16-finisher` capture artifact** (its selmo/`0x31` sections are
  diff-allowed; the same port stream matches the u16 capture, §M4.4 item 2) and
  **`gp-u22-diablo-ending`'s title** (label slack plus a stop on the last claimed
  tick; explicit `allow-diff`, §M4.4 item 5/6).
- **The differential boundary**: `0x1C930`/`0x1CF40` stay `UNROWED`; a faithful
  row would fabricate the AIL handle and could not compare the hand-off (§M2.4).
- **Task 6's three items** (§M6.2), and the deferred code/test minors and harness
  annotation noted in §M6.4.
- **A song/bank absent from the 29 gameplay traces** is a named gap, never a new
  capture (the phase's binding constraint).
