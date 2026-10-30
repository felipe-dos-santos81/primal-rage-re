#!/usr/bin/env python3
"""Tolerance-free register-stream oracle: the port's OPL writes vs a capture's.

The reference is the original's own DBRAWOPL (`.dro`) trace; the port's stream
is the `PR_AUDIO_DUMP` file (port/tests/test_game.c). The claim is song choice,
notes, patches and tempo in the captured window; rendered audibility is not
claimed.

The only alignment is each song's **first key-on**: the port section's first
`0xB0..0xB8`/`0x1B0..0x1B8` write with the key bit, and the capture key-on with
the same register and value. A candidate anchor is verified against the port's
first few relative ticks before it is accepted, so a repeated note elsewhere in
the capture cannot be mistaken for the section start.

Both streams are reduced the way the title oracle reduces them
(`port/spec/audio.md`, "Known capture divergences"):

  * anchor at the section's first key-on;
  * drop a write whose value equals the last kept value for that register
    (write-on-change collapse; the capture records write-on-change only, the
    port writes every family unconditionally; the shadow is seeded from the
    capture's pre-anchor state on both sides);
  * drop `0xBD` and the carrier-TL registers `0x43..0x45/0x4B..0x4D/0x53..0x55`
    on MIDI channels 1 and 4 only (the engine's per-channel volume residual).
    The capture records no MIDI channel, so a captured write inherits the
    attribution of the port's raw write at the same relative tick/register.

The comparison is **per music tick, as a sorted multiset** (the declared
intra-tick write-order model difference: the port keys expired voices in
ascending OPL channel order where the driver's per-tick visit order is not
pinned). The capture's ms is the emulator's accumulated inter-write delay, so a
write's label can round to the adjacent tick (the same selmo note-off is at
479..484 ms in three captures): a group, or one write of it, may sit one tick
earlier or later in the capture. That is the declared whole-tick model, not a
fitted window: the write content and order across ticks are exact, and a
dropped, extra or wrong write has no matching group and fails at its tick. The
per-tick multiset equality is reported as the stronger measured form (it is
exact for most sections).

Each section's **final tick** is the request boundary and is not claimed: the
section end is a game-frame event while the capture's music clock is its own
(the measured 0.8 % span difference, record 2026-10-25-music-truth-derivations
§M3.4). Its writes are reported, not compared.

A section that cannot be anchored is a **named gap**, not a silent skip: the
measured `TRACE_PINS` table (or `--allow-unanchored HASH[,HASH]`) names the
sections a trace's capture cannot carry (the driver's voice cursor advanced by
the boundary's extra capture note). A section that anchors but differs on a
measured capture-side artifact (a tick split plus a carrier-TL write-on-change
state, §M4.4) is a named gap too, through `--allow-diff HASH[,HASH]`; its
leading matched tick groups are still claimed, bounded by the per-section
`TRACE_PREFIX_PINS` minimum, and only the unmatched remainder is the gap. Any
other unanchored or differing section fails. A capture with no `.dro` at all is
skipped by name (exit 3); the caller prints it.

Exit codes: 0 = every required section compared and matched, every diff-allowed
prefix meets its pin (and the whole-section claimed tick total is at least its
pinned minimum); 1 = a first difference, an unallowed unanchored/differing
section, or a short/lost claim; 2 = usage/parse error; 3 = the capture is absent
(the caller prints the skip by name).

Stdlib only.
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import opl_trace

# The measured gameplay inventory (record 2026-10-25-music-truth-derivations
# §M1.6.2): 28 `gp-*` capture directories holding 29 `.dro` files (gp-pads holds
# two). The 8 other k11 captures and the 2 audio captures are not gameplay and
# are named apart by `summary`.
GP_TRACES = [
    'gp-idle-loss', 'gp-idle-loss-run2', 'gp-keys-fight', 'gp-pads', 'gp-twop',
    'gp-u10-ending', 'gp-u12-endurance', 'gp-u13-ending2', 'gp-u15-join',
    'gp-u16-twop2', 'gp-u17-device2', 'gp-u18-talon-ending',
    'gp-u19-vertigo-ending', 'gp-u20-chaos-ending', 'gp-u21-armadon-ending',
    'gp-u22-diablo-ending', 'gp-u23-u16-finisher', 'gp-u5-charsel',
    'gp-u6-moves', 'gp-u6-moves-b', 'gp-u8-attract-start', 'gp-u8-endurance',
    'gp-u8-handicap', 'gp-u8-left-training', 'gp-u8-right-arcade',
    'gp-u8-right-training', 'gp-u8-tug-of-war', 'gp-u9-win',
]

# Not gameplay: the service-menu/save/diagnostic k11 captures (8 files across 6
# directories) and the two title captures the report-only test reads.
OTHER_K11_TRACES = ['de', 'diags', 'idle', 'menuesc', 'save-reload', 'walk']

# The expected per-directory `.dro` inventory (record §M1.6.2): 28 directories,
# 29 files; gp-pads is the one directory whose capture restarted and holds two.
# A present directory whose file set differs is a partial loss and fails the
# summary; an absent directory is the declared skip.
EXPECTED_FILES = {name: ('prage_000.dro',) for name in GP_TRACES}
EXPECTED_FILES['gp-pads'] = ('prage_000.dro', 'prage_001.dro')
EXPECTED_TOTAL = sum(len(v) for v in EXPECTED_FILES.values())

# The measured per-trace pins (record 2026-10-25-music-truth-derivations §M4.5):
#   name: (minimum total claimed ticks, allow-unanchored bank hashes,
#          allow-diff bank hashes)
# The minimum is the total of two runs on the phase tree; both runs produced
# every value below byte-identically. The allowances are the measured sections a
# trace cannot carry: the driver voice cursor advanced by the section boundary's
# extra capture note (unanchored) or the capture's tick-split/carrier-TL
# write-on-change artifact (diff). A section not in either list must anchor and
# match. `gp-u17-device2` has no frame-keyed port script (wall-paced; record
# §H6.9) and its `.dro` is the 248 ms boot fragment, so it is skipped by name.
# >>> PIN-BASELINE-BEGIN trace-pins
TRACE_PINS = {
    'gp-idle-loss': (2144, (0x3BA4C376, 0xBF1C78E2, 0xE3533961,), (0x19AB8B2B, 0x8FE546D4, 0xBF1C78E2,)),
    'gp-idle-loss-run2': (2144, (0x3BA4C376, 0xE3533961,), (0x19AB8B2B, 0x8FE546D4, 0xBF1C78E2, 0xE3533961,)),
    'gp-keys-fight': (2144, (0xBF1C78E2,), (0xBF1C78E2,)),
    'gp-pads': (30, (), ()),
    'gp-twop': (956, (0x20DA829D,), ()),
    'gp-u10-ending': (5679, (0x20DA829D, 0x3BA4C376,), (0x15B3F30E, 0x19AB8B2B, 0x42BECD5B, 0xB40FF4A6, 0xE3533961,)),
    'gp-u12-endurance': (1436, (), (0x3D536DDE,)),
    'gp-u13-ending2': (6307, (0x3BA4C376,), (0x15B3F30E, 0x19AB8B2B, 0x42BECD5B, 0xB40FF4A6, 0xE3533961,)),
    'gp-u15-join': (2144, (0xBF1C78E2,), ()),
    'gp-u16-twop2': (956, (0x20DA829D, 0x3BA4C376,), (0xE3533961,)),
    'gp-u17-device2': (0, (), ()),
    'gp-u18-talon-ending': (3991, (0x3BA4C376,), (0x15B3F30E, 0x19AB8B2B, 0x42BECD5B, 0x87E7C2DE, 0x94C63685, 0xB40FF4A6, 0xE3533961,)),
    'gp-u19-vertigo-ending': (3805, (0x3BA4C376,), (0x15B3F30E, 0x19AB8B2B, 0x3D536DDE, 0x42BECD5B, 0x94C63685, 0xB40FF4A6, 0xE3533961,)),
    'gp-u20-chaos-ending': (4142, (0x3BA4C376,), (0x15B3F30E, 0x19AB8B2B, 0x42BECD5B, 0x86946EB7, 0x94C63685, 0xB40FF4A6, 0xE3533961,)),
    'gp-u21-armadon-ending': (5161, (0x3BA4C376,), (0x15B3F30E, 0x19AB8B2B, 0x94C63685, 0xB40FF4A6, 0xBC44EF50, 0xE3533961,)),
    'gp-u22-diablo-ending': (6013, (0x3BA4C376,), (0x15B3F30E, 0x19AB8B2B, 0x42BECD5B, 0xB40FF4A6, 0xBF1C78E2, 0xE3533961,)),
    'gp-u23-u16-finisher': (30, (0x20DA829D, 0x3BA4C376,), (0x1895A942, 0x286ED662, 0x3EA76262, 0xE3533961, 0xF1774E70,)),
    'gp-u5-charsel': (1603, (), ()),
    'gp-u6-moves': (2144, (), (0xBF1C78E2,)),
    'gp-u6-moves-b': (2144, (0xBF1C78E2,), ()),
    'gp-u8-attract-start': (2144, (), (0x86946EB7,)),
    'gp-u8-endurance': (613, (), ()),
    'gp-u8-handicap': (2966, (), ()),
    'gp-u8-left-training': (2966, (), ()),
    'gp-u8-right-arcade': (2968, (), ()),
    'gp-u8-right-training': (2486, (), (0xF1774E70,)),
    'gp-u8-tug-of-war': (2966, (), ()),
    'gp-u9-win': (2831, (0x20DA829D, 0x3BA4C376,), (0x86946EB7, 0xE3533961,)),
}



# <<< PIN-BASELINE-END trace-pins

# The measured matched-prefix minimum of each diff-allowed section (record
# 2026-10-29-live-truth-derivations §L11), keyed by the port dump's section
# index. `al.claimed` counts the leading tick groups before the first difference;
# the unmatched remainder stays a named gap. These pins are separate from
# TRACE_PINS' whole-section tick totals, which do not move. A later full match is
# a stronger claim and is accepted; a short prefix, a lost prefix, or a new
# diff-allowed section without a pin fails.
TRACE_PREFIX_PINS = {
    'gp-idle-loss': {3: 30, 9: 960, 10: 301},
    'gp-idle-loss-run2': {3: 30, 5: 5, 6: 20, 9: 960, 10: 303},
    'gp-keys-fight': {3: 30},
    'gp-u10-ending': {5: 5, 8: 932, 12: 120, 13: 1437, 14: 826, 15: 118},
    'gp-u12-endurance': {3: 180},
    'gp-u13-ending2': {5: 5, 8: 932, 12: 120, 13: 1435, 14: 827, 15: 301},
    'gp-u16-twop2': {5: 5, 8: 5},
    'gp-u18-talon-ending': {5: 5, 6: 28, 8: 933, 9: 581, 12: 120, 13: 1435, 14: 827, 15: 302},
    'gp-u19-vertigo-ending': {3: 180, 5: 5, 6: 180, 8: 930, 9: 14, 12: 120, 13: 1436, 14: 828, 15: 303},
    'gp-u20-chaos-ending': {5: 5, 6: 180, 8: 932, 9: 581, 12: 120, 13: 1437, 14: 57, 15: 302},
    'gp-u21-armadon-ending': {5: 5, 6: 204, 9: 581, 12: 120, 13: 1437, 14: 828, 15: 302},
    'gp-u22-diablo-ending': {3: 30, 5: 5, 6: 30, 8: 931, 12: 120, 13: 1435, 14: 828, 15: 118},
    'gp-u23-u16-finisher': {1: 25, 2: 21, 5: 5, 7: 425, 8: 177},
    'gp-u6-moves': {3: 20},
    'gp-u8-attract-start': {3: 85},
    'gp-u8-right-training': {2: 21},
    'gp-u9-win': {5: 5, 9: 170},
}

# The expected per-scenario section identity/status inventory (the whole-song
# guard, record §M4.5). Each entry is (bank hash, kind) in the port dump's
# order, after the boot writes:
#   'matched' — the section must anchor and match (the tick pins alone do not
#               catch its disappearance; e.g. gp-u23's 30-tick title-only pin);
#   'named'   — the section is present and is the measured named gap
#               (unanchored/diff-allowed/no-claim); if it anchors and matches,
#               that is an improvement and is accepted. A section missing from
#               the port dump fails in both kinds.
TRACE_SECTIONS = {
    'gp-idle-loss': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0xBF1C78E2, 'named'), (0x3BA4C376, 'named'), (0xE3533961, 'named'), (0xBF1C78E2, 'named'), (0x3BA4C376, 'named'), (0xE3533961, 'named'), (0x8FE546D4, 'named'), (0x19AB8B2B, 'named')),
    'gp-idle-loss-run2': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0xBF1C78E2, 'named'), (0x3BA4C376, 'named'), (0xE3533961, 'named'), (0xBF1C78E2, 'named'), (0x3BA4C376, 'named'), (0xE3533961, 'named'), (0x8FE546D4, 'named'), (0x19AB8B2B, 'named')),
    'gp-keys-fight': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0xBF1C78E2, 'named'), (0xBF1C78E2, 'named'), (0xBF1C78E2, 'named')),
    'gp-pads': ((0xFD94B4E3, 'matched'),),
    'gp-twop': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0x20DA829D, 'named')),
    'gp-u10-ending': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0x20DA829D, 'named'), (0x3BA4C376, 'named'), (0xE3533961, 'named'), (0x20DA829D, 'named'), (0x3EA76262, 'matched'), (0x42BECD5B, 'named'), (0x94C63685, 'matched'), (0x26B142E7, 'matched'), (0x13211DEB, 'matched'), (0xB40FF4A6, 'named'), (0x15B3F30E, 'named'), (0x15B3F30E, 'named'), (0x19AB8B2B, 'named')),
    'gp-u12-endurance': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0x3D536DDE, 'named')),
    'gp-u13-ending2': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0xBC44EF50, 'matched'), (0x3BA4C376, 'named'), (0xE3533961, 'named'), (0xBC44EF50, 'matched'), (0x3EA76262, 'matched'), (0x42BECD5B, 'named'), (0x94C63685, 'matched'), (0x26B142E7, 'matched'), (0x13211DEB, 'matched'), (0xB40FF4A6, 'named'), (0x15B3F30E, 'named'), (0x15B3F30E, 'named'), (0x19AB8B2B, 'named')),
    'gp-u15-join': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0xBF1C78E2, 'named')),
    'gp-u16-twop2': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0x20DA829D, 'named'), (0x3BA4C376, 'named'), (0xE3533961, 'named'), (0x20DA829D, 'named'), (0x3BA4C376, 'named'), (0xE3533961, 'named'), (0x42BECD5B, 'named')),
    'gp-u18-talon-ending': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0x87E7C2DE, 'matched'), (0x3BA4C376, 'named'), (0xE3533961, 'named'), (0x87E7C2DE, 'named'), (0x3EA76262, 'matched'), (0x42BECD5B, 'named'), (0x94C63685, 'named'), (0x26B142E7, 'matched'), (0x13211DEB, 'matched'), (0xB40FF4A6, 'named'), (0x15B3F30E, 'named'), (0x15B3F30E, 'named'), (0x19AB8B2B, 'named')),
    'gp-u19-vertigo-ending': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0x3D536DDE, 'named'), (0x3BA4C376, 'named'), (0xE3533961, 'named'), (0x3D536DDE, 'named'), (0x3EA76262, 'matched'), (0x42BECD5B, 'named'), (0x94C63685, 'named'), (0x26B142E7, 'matched'), (0x13211DEB, 'matched'), (0xB40FF4A6, 'named'), (0x15B3F30E, 'named'), (0x15B3F30E, 'named'), (0x19AB8B2B, 'named')),
    'gp-u20-chaos-ending': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0x86946EB7, 'matched'), (0x3BA4C376, 'named'), (0xE3533961, 'named'), (0x86946EB7, 'named'), (0x3EA76262, 'matched'), (0x42BECD5B, 'named'), (0x94C63685, 'named'), (0x26B142E7, 'matched'), (0x13211DEB, 'matched'), (0xB40FF4A6, 'named'), (0x15B3F30E, 'named'), (0x15B3F30E, 'named'), (0x19AB8B2B, 'named')),
    'gp-u21-armadon-ending': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0xBC44EF50, 'matched'), (0x3BA4C376, 'named'), (0xE3533961, 'named'), (0xBC44EF50, 'named'), (0x3EA76262, 'matched'), (0x42BECD5B, 'matched'), (0x94C63685, 'named'), (0x26B142E7, 'matched'), (0x13211DEB, 'matched'), (0xB40FF4A6, 'named'), (0x15B3F30E, 'named'), (0x15B3F30E, 'named'), (0x19AB8B2B, 'named')),
    'gp-u22-diablo-ending': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0xBF1C78E2, 'named'), (0x3BA4C376, 'named'), (0xE3533961, 'named'), (0xBF1C78E2, 'named'), (0x3EA76262, 'matched'), (0x42BECD5B, 'named'), (0x94C63685, 'matched'), (0x26B142E7, 'matched'), (0x13211DEB, 'matched'), (0xB40FF4A6, 'named'), (0x15B3F30E, 'named'), (0x15B3F30E, 'named'), (0x19AB8B2B, 'named')),
    'gp-u23-u16-finisher': ((0xFD94B4E3, 'matched'), (0x286ED662, 'named'), (0xF1774E70, 'named'), (0x20DA829D, 'named'), (0x3BA4C376, 'named'), (0xE3533961, 'named'), (0x20DA829D, 'named'), (0x3EA76262, 'named'), (0x1895A942, 'named'), (0x42BECD5B, 'named')),
    'gp-u5-charsel': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0x87E7C2DE, 'matched')),
    'gp-u6-moves': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0xBF1C78E2, 'named')),
    'gp-u6-moves-b': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0xBF1C78E2, 'named')),
    'gp-u8-attract-start': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0x86946EB7, 'named')),
    'gp-u8-endurance': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched')),
    'gp-u8-handicap': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0xBC44EF50, 'matched')),
    'gp-u8-left-training': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0xBC44EF50, 'matched')),
    'gp-u8-right-arcade': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0xF270182D, 'matched')),
    'gp-u8-right-training': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'named'), (0xBC44EF50, 'matched')),
    'gp-u8-tug-of-war': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0xBC44EF50, 'matched')),
    'gp-u9-win': ((0xFD94B4E3, 'matched'), (0x286ED662, 'matched'), (0xF1774E70, 'matched'), (0x20DA829D, 'named'), (0x3BA4C376, 'named'), (0xE3533961, 'named'), (0x20DA829D, 'named'), (0x3EA76262, 'matched'), (0x42BECD5B, 'matched'), (0x86946EB7, 'named')),
}

# The register shadow is indexed by the full 9-bit register so the second OPL2
# bank (0x1E0..0x1F5) does not alias the first (0xE0..0xF5). Same size as the
# title oracle's OPL_SHADOW_REGS (port/tests/test_audio.c).
SHADOW_REGS = 0x200

# The first relative ticks a candidate anchor must match before the section is
# compared. A constant verification prefix, not a tolerance: the full section
# is still compared, and a wrong anchor has to fail this before it can be
# mistaken for the section start.
ANCHOR_TICKS = 4

# The capture's music tick rate: 120 sequencer ticks/s (the title anchor's
# `ms = (tick - 60) * 1000 // 120`, tools/opl_seq.py:705). The capture's ms is
# its own accumulated delay, never a poll timestamp.
TICKS_PER_SECOND = 120


def is_key_on(reg, val):
    return (0xB0 <= reg <= 0xB8 or 0x1B0 <= reg <= 0x1B8) and (val & 0x20)


# The carrier-TL register low bytes (both OPL banks): the declared carrier
# channels 0..8, op2 +3 (sequencer.c OPL_SLOT/op2 is +3).
CARRIER_TL = (0x43, 0x44, 0x45, 0x4B, 0x4C, 0x4D, 0x53, 0x54, 0x55)

# The residual carrier-TL attrs the capture proves, per bank (record
# claim-coverage §C3): the engine's per-channel volume residual is on MIDI
# channels 1 and 4 in the first OPL bank (`port/spec/audio.md`), and on 9 and 3
# in the second bank. The second-bank rows are measured in gp-u23-u16-finisher's
# s16selmo+0x25C section: `0x145=0x20` attr 0x01 (rel 25), `0x145=0x98` attr
# 0x04 (rel 98), `0x145=0x16` attr 0x09 (rel 326) and `0x145=0x16` attr 0x03
# (rel 387). A first-bank carrier-TL write under any other attr is compared.
FIRST_BANK_RESIDUAL_ATTRS = (1, 4)
SECOND_BANK_RESIDUAL_ATTRS = (1, 4, 9, 3)


def documented_excluded(reg, attr):
    """The port deliberately does not reproduce these against a capture.

    `0xBD` unconditionally, and the carrier-TL registers on the residual MIDI
    channels only: 1 and 4 in the first OPL bank (the engine's per-channel
    volume residual, `port/spec/audio.md`), and 1/4/9/3 in the second bank (the
    measured second-bank carrier-TL rows, record claim-coverage §C3). Modulator
    TL and carrier TL under any other attribution are compared.
    """
    lo = reg & 0xFF
    if lo == 0xBD:
        return True
    if lo in CARRIER_TL:
        if reg >= 0x100:
            return attr in SECOND_BANK_RESIDUAL_ATTRS
        return attr in FIRST_BANK_RESIDUAL_ATTRS
    return False


# The OPL3 operator slot offsets within a register bank (sequencer.c OPL_SLOT:
# channels 0-8 are 0x00/0x01/0x02/0x08/0x09/0x0a/0x10/0x11/0x12; op2 is +3).
_OPL_SLOT = (0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12)


def opl_channel(reg):
    """The OPL channel (0-17) a register addresses, or None (0xBD / unknown)."""
    lo = reg & 0xFF
    bank = 1 if reg >= 0x100 else 0
    if lo == 0xBD:
        return None
    for fam in (0xA0, 0xB0, 0xC0):
        if fam <= lo <= fam + 8:
            return bank * 9 + (lo - fam)
    for fam in (0x20, 0x40, 0x60, 0x80, 0xE0):
        off = lo - fam
        if off in _OPL_SLOT:
            return bank * 9 + _OPL_SLOT.index(off)
        if off >= 3 and (off - 3) in _OPL_SLOT:
            return bank * 9 + _OPL_SLOT.index(off - 3)
    return None


def rotate_reg(reg, k):
    """The register with its OPL channel rotated by k (mod 18), the constant
    channel rotation of the voice-rotation cursor (record claim-coverage §C3.2).
    """
    ch = opl_channel(reg)
    if ch is None:
        return reg
    nc = (ch + k) % 18
    nb = 1 if nc >= 9 else 0
    lo = reg & 0xFF
    for fam in (0xA0, 0xB0, 0xC0):
        if fam <= lo <= fam + 8:
            return fam + (nc % 9) + (0x100 if nb else 0)
    for fam in (0x20, 0x40, 0x60, 0x80, 0xE0):
        off = lo - fam
        if off in _OPL_SLOT:
            return fam + _OPL_SLOT[nc % 9] + (0x100 if nb else 0)
        if off >= 3 and (off - 3) in _OPL_SLOT:
            return fam + 3 + _OPL_SLOT[nc % 9] + (0x100 if nb else 0)
    return reg


def resource_name(handle, index_names):
    """The raw handle is `(index << 23) | offset` (record §M1.4.1)."""
    index = handle >> 23
    off = handle & 0x7FFFFF
    if 0 <= index < len(index_names):
        return '%s+0x%X' % (index_names[index], off)
    return 'handle:0x%08X' % handle


def read_index_names(game_dir):
    if not game_dir:
        return []
    path = os.path.join(game_dir, 'INDEX')
    try:
        with open(path, 'rb') as f:
            data = f.read()
    except OSError:
        return []
    names = []
    for i in range(len(data) // 20):
        names.append(data[i * 20:i * 20 + 12].split(b'\0')[0].decode('latin1'))
    return names


class Section:
    """A run of register writes with non-decreasing song ticks. A new song (or
    a same-song restart) drops the tick counter to 0, which starts a section."""

    def __init__(self, writes):
        self.writes = writes
        self.first_frame = writes[0][0]
        self.dc = 0
        self.bank = 0
        self.attributed = False
        self.anchor_idx = None
        self.anchor_tick = None
        for i, w in enumerate(writes):
            if is_key_on(w[3], w[4]):
                self.anchor_idx = i
                self.anchor_tick = w[1]
                break

    @property
    def last_tick(self):
        return max(w[1] for w in self.writes)


def parse_port(path):
    """Returns (sections, s_lines). The `dc`/bank come from the first S record
    at or after the section's first write frame (the S line is emitted after
    the iteration's writes). A section with no such S record is unattributed:
    its `dc`/bank stay 0 and `attributed` is False (it must not borrow the
    preceding song's bank or allowances)."""
    s_lines = []
    writes = []
    with open(path) as f:
        for line in f:
            p = line.split()
            if not p:
                continue
            if p[0] == 'S':
                fields = dict(kv.split('=', 1) for kv in p[2:] if '=' in kv)
                s_lines.append({
                    'frame': int(p[1]),
                    'cc': int(fields.get('cc', '0'), 16),
                    'dc': int(fields.get('dc', '0'), 16),
                    'vol': int(fields.get('vol', '0'), 16),
                    'bank': int(fields.get('bank', '0'), 16),
                    'inits': int(fields.get('inits', '0')),
                    'ticks': int(fields.get('ticks', '0')),
                })
            elif p[0] == 'W':
                writes.append((int(p[1]), int(p[2]), int(p[3]),
                               int(p[4], 16), int(p[5], 16), int(p[6], 16)))
    if not writes:
        return [], s_lines
    groups = []
    cur = []
    prev_tick = None
    for w in writes:
        if prev_tick is not None and w[1] < prev_tick:
            groups.append(cur)
            cur = []
        cur.append(w)
        prev_tick = w[1]
    groups.append(cur)
    sections = [Section(g) for g in groups]
    for s in sections:
        rec = next((r for r in s_lines if r['frame'] >= s.first_frame), None)
        if rec is not None:
            s.dc = rec['dc']
            s.bank = rec['bank']
            s.attributed = True
    return sections, s_lines


def rotate_section(section, k):
    """A copy of the section with every register's OPL channel rotated by k.

    The voice-rotation cursor is the driver's monotonic state, so a section whose
    capture is a constant channel rotation of the port's (the §M4.4 item-1
    section-boundary gap) is aligned by relabeling the port's registers before
    the reduction. Rotation preserves the key-on register family, so anchor_idx
    and anchor_tick are unchanged; dc/bank/attributed are copied verbatim.
    """
    if k == 0:
        return section
    rotated = Section([(f, t, w, rotate_reg(reg, k), val, attr)
                       for (f, t, w, reg, val, attr) in section.writes])
    rotated.dc = section.dc
    rotated.bank = section.bank
    rotated.attributed = section.attributed
    return rotated


def parse_capture(path):
    with open(path, 'rb') as f:
        header, events = opl_trace.parse(f.read())
    return header, events


def capture_files(path):
    """The `.dro` files in a capture directory, sorted (gp-pads holds two)."""
    if os.path.isfile(path):
        return [path]
    if not os.path.isdir(path):
        return []
    return [os.path.join(path, f) for f in sorted(os.listdir(path))
            if f.lower().endswith('.dro')]


def poll_x_frame(capture_dir):
    path = os.path.join(capture_dir, 'poll.log')
    if not os.path.isfile(path):
        return None
    last = None
    with open(path) as f:
        for line in f:
            p = line.split()
            if p and p[0] == 'X':
                for kv in p[1:]:
                    if kv.startswith('f='):
                        last = int(kv[2:], 16)
    return last


def first_music_key_on(events):
    """The first key-on after a write gap > 1000 ms (the music after the boot,
    record §M1.6.2). The title fragment's own first key-on is not music: the
    boot's notes are contiguous, and the next section begins after the gap."""
    prev_ms = None
    gap_start = None
    for ms, reg, val in events:
        if prev_ms is not None and ms - prev_ms > 1000:
            gap_start = ms
        if gap_start is not None and is_key_on(reg, val):
            return ms
        prev_ms = ms
    return None


def _shadow_from(events, upto):
    shadow = bytearray(SHADOW_REGS)
    for k in range(upto):
        _, reg, val = events[k]
        shadow[reg & (SHADOW_REGS - 1)] = val
    return shadow


def _port_attr_map(section):
    """(relative tick, register) -> the MIDI channel of the port's raw write.
    The capture records no channel; the port knows the voice's. The last write
    at a (tick, register) wins, as the title oracle's linear scan does."""
    at = section.anchor_tick
    attrs = {}
    for _f, t, _w, reg, _val, attr in section.writes[section.anchor_idx:]:
        rel = t - at
        if rel >= 0:
            attrs[(rel, reg)] = attr
    return attrs


def _reduce_port(section, shadow):
    at = section.anchor_tick
    out = []
    for _f, t, _w, reg, val, attr in section.writes[section.anchor_idx:]:
        rel = t - at
        if rel < 0:
            continue
        sh = reg & (SHADOW_REGS - 1)
        excluded = documented_excluded(reg, attr)
        if val == shadow[sh]:
            continue
        # The shadow tracks the hardware state even when the register is not
        # compared: an excluded write still leaves its value in the chip, and
        # the capture's own write-on-change suppression sees that value. (A
        # shadow that skipped excluded writes desynchronised the two sides and
        # made a later compared write appear or vanish; record §M4.)
        shadow[sh] = val
        if excluded:
            continue
        out.append((rel, reg, val, attr))
    return out


def _reduce_capture(events, anchor_idx, attrs, shadow):
    anchor_ms = events[anchor_idx][0]
    out = []
    for ms, reg, val in events[anchor_idx:]:
        rel = ((ms - anchor_ms) * TICKS_PER_SECOND + 500) // 1000
        attr = attrs.get((rel, reg), 0xFF)
        sh = reg & (SHADOW_REGS - 1)
        excluded = documented_excluded(reg, attr)
        if val == shadow[sh]:
            continue
        shadow[sh] = val
        if excluded:
            continue
        out.append((rel, reg, val, attr))
    return out


def _by_tick(stream):
    d = {}
    for rel, reg, val, _ in stream:
        d.setdefault(rel, []).append((reg, val))
    for k in d:
        d[k].sort()
    return d


def _tick_diff(port_ticks, capture_ticks, lo, hi):
    """First relative tick in [lo, hi) whose multiset differs, with both sides."""
    for rel in range(lo, hi):
        if port_ticks.get(rel, []) != capture_ticks.get(rel, []):
            return rel, port_ticks.get(rel, []), capture_ticks.get(rel, [])
    return None


def _fmt_tick(ws):
    if not ws:
        return '<none>'
    return ' '.join('%#04x=%#04x' % (r, v) for r, v in ws)


# The capture's DRO ms is the emulator's accumulated inter-write delay; the
# same selmo note-off is at 479..484 ms in three captures (record §M4), so a
# write within ~0.6 tick of a 1/120 s boundary rounds to an adjacent label. The
# declared whole-tick model, implemented by align_groups: a tick group (or one
# write of it) may match one tick earlier or later in the capture. The write
# content, per-tick multiset and order across ticks are exact; the order
# *within* a tick is the declared intra-tick model.


class AlignResult:
    def __init__(self):
        self.ok = True
        self.jittered = 0
        self.claimed = 0
        self.matched = 0
        self.port_writes = 0
        self.first_tick = None
        self.first_port = None
        self.first_cap = None
        self.reason = None


def _counter(ws):
    import collections
    return collections.Counter(ws)


def align_groups(port_groups, cap_groups):
    """Compare the port's claimed tick groups against the capture's groups.

    Groups are sorted `(register, value)` multisets, so the declared intra-tick
    write-order model needs no per-write transposition rule. A group may match
    the capture's group at the same tick, at the next tick, or the previous
    one (the declared one-tick capture-label slack); a single write may move
    between adjacent groups by the same slack. Every port write is matched
    exactly once (`matched == port_writes` at the end, or the alignment fails);
    a skipped group must be empty, so the slack can never swallow a dropped or
    extra write. A dropped, extra or wrong write has no matching group and
    fails at its tick.

    `port_groups` are the claimed ticks 0..N-1; `cap_groups` go to N so a
    claimed write whose capture label is one tick late can still match. The
    capture's groups after the last matched port group are its own tail and are
    not claimed."""
    r = AlignResult()
    P = [sorted(g) for g in port_groups]
    C = [sorted(g) for g in cap_groups]
    r.port_writes = sum(len(g) for g in P)
    i = j = 0
    n = len(P)
    while i < n:
        pg = sorted(P[i])
        cg = sorted(C[j]) if j < len(C) else []
        if pg == cg:
            i += 1
            j += 1
            r.matched += len(pg)
            continue
        nxt_p = sorted(P[i + 1]) if i + 1 < len(P) else []
        nxt_c = sorted(C[j + 1]) if j + 1 < len(C) else []
        # A whole group one tick later in the capture. C[j] is skipped, so it
        # must be empty: the one-tick slack may not swallow a capture write.
        if pg and pg == nxt_c and not cg:
            i += 1
            j += 2
            r.matched += len(pg)
            r.jittered += 1
            continue
        # A whole later port group against the capture's current group. P[i] is
        # skipped, so it must be empty (a dropped port write may not hide here).
        if cg and cg == nxt_p and not pg:
            i += 2
            j += 1
            r.matched += len(cg)
            r.jittered += 1
            continue
        # One write whose capture label is one tick later: move it back.
        dp = _counter(pg) - _counter(cg)
        dc = _counter(cg) - _counter(pg)
        if sum(dp.values()) == 1 and not dc:
            w = next(iter(dp))
            if w in nxt_c:
                nxt_c.remove(w)
                C[j + 1] = nxt_c
                C[j] = sorted(cg + [w])
                r.jittered += 1
                continue
        # One write whose capture label is one tick earlier: move it forward.
        if sum(dc.values()) == 1 and not dp:
            w = next(iter(dc))
            # Only a write the port actually has one tick later may move: a
            # dropped write must not walk into the capture's tail.
            if j + 1 < len(C) and w in nxt_p:
                C[j] = sorted(x for x in cg if x != w)
                C[j + 1] = sorted(nxt_c + [w])
                r.jittered += 1
                continue
        r.ok = False
        r.claimed = i
        r.first_tick = i
        r.first_port = pg
        r.first_cap = cg
        r.reason = 'tick groups differ'
        return r
    # Every unmatched non-empty capture group inside the claimed range is an
    # extra capture write the port did not play. Only the group at index
    # len(P) (the request boundary/tail) may remain unclaimed.
    if r.ok:
        for k in range(j, len(P)):
            cg_k = C[k] if k < len(C) else []
            if cg_k:
                r.ok = False
                r.first_tick = k
                r.first_port = []
                r.first_cap = cg_k
                r.reason = 'unmatched capture group in the claimed window'
                break
    if r.matched != r.port_writes:
        r.ok = False
        r.first_tick = n - 1 if n else 0
        r.first_port = []
        r.first_cap = []
        r.reason = ('write accounting: %d matched, %d port writes'
                    % (r.matched, r.port_writes))
    r.claimed = i
    return r


class SectionResult:
    def __init__(self, section, index):
        self.section = section
        self.index = index
        self.status = 'unanchored'
        self.capture_path = None
        self.anchor_ms = None
        self.anchor_event = None
        self.capture_end = None      # (capture file index, next event index)
        self.claimed = 0
        self.prefix_min = None
        self.boundary_rel = None
        self.boundary_equal = None
        self.first_diff = None
        self.first_diff_kind = None
        self.first_diff_port = None
        self.first_diff_cap = None
        self.first_transposition = None
        self.jittered = 0
        self.per_tick_equal = 0
        self.reduced = 0
        self.reason = None


def _evaluate_candidate(section, index, capture_index, anchor_event,
                        captures, attrs, allow_diff):
    """The full comparison at one capture anchor; also records the capture
    interval the next section may not reuse (the events through the claimed
    window, or through the first difference for an allowed diff)."""
    path, _header, events = captures[capture_index]
    ms = events[anchor_event][0]
    last_rel = section.last_tick - section.anchor_tick
    shadow = _shadow_from(events, anchor_event)
    pw = _reduce_port(section, bytearray(shadow))
    cw = _reduce_capture(events, anchor_event, attrs, bytearray(shadow))
    pd = _by_tick(pw)
    cd = _by_tick(cw)
    res = SectionResult(section, index)
    res.status = 'anchored'
    res.capture_path = path
    res.anchor_ms = ms
    res.anchor_event = anchor_event
    res.boundary_rel = last_rel
    res.boundary_equal = pd.get(last_rel, []) == cd.get(last_rel, [])
    groups_p = [pd.get(t, []) for t in range(last_rel)]
    groups_c = [cd.get(t, []) for t in range(last_rel + 1)]
    al = align_groups(groups_p, groups_c)
    res.claimed = al.claimed
    res.jittered = al.jittered
    res.reduced = sum(len(groups_p[t]) for t in range(min(al.claimed, last_rel)))
    eq = _tick_diff(pd, cd, 0, last_rel)
    res.per_tick_equal = eq[0] if eq is not None else last_rel
    if not al.ok:
        if section.bank in allow_diff:
            res.status = 'diff-allowed'
            res.reason = ('the measured capture-side register/write-on-change '
                          'artifact at this section (allowed by name)')
        else:
            res.status = 'diff'
        res.first_diff_kind = al.reason
        res.first_diff = al.first_tick
        res.first_diff_port = al.first_port
        res.first_diff_cap = al.first_cap
    limit = res.claimed
    if res.status == 'diff-allowed' and res.first_diff is not None:
        limit = max(limit, res.first_diff)
    end = anchor_event
    for k in range(anchor_event, len(events)):
        rel = ((events[k][0] - ms) * TICKS_PER_SECOND + 500) // 1000
        if rel <= limit:
            end = k
        else:
            break
    res.capture_end = (capture_index, end + 1)
    return res


def compare_section(section, index, captures, allow_unanchored, allow_diff,
                    search_from=(0, 0)):
    res = SectionResult(section, index)
    if section.anchor_idx is None:
        res.status = 'no-key-on'
        res.reason = 'the section has no key-on to anchor'
        return res
    at = section.anchor_tick
    last_rel = section.last_tick - at
    if last_rel < 1:
        res.status = 'no-claim'
        res.reason = 'the section is one tick; nothing before its boundary tick'
        return res
    key_w = section.writes[section.anchor_idx]
    key_ch = opl_channel(key_w[3])
    prefix = min(ANCHOR_TICKS, last_rel)

    # Step 1: the zero-rotation anchor — the section's first key-on at the same
    # register. The common case (and every synthetic k=0 section).
    cands = []
    for ci, (path, _header, events) in enumerate(captures):
        for j, (ms, reg, val) in enumerate(events):
            if (ci, j) < search_from:
                continue
            if reg == key_w[3] and val == key_w[4] and is_key_on(reg, val):
                cands.append((ci, j, ms))
    for ci, j, _ms in cands:
        path, _header, events = captures[ci]
        shadow = _shadow_from(events, j)
        pw = _reduce_port(section, bytearray(shadow))
        attrs = _port_attr_map(section)
        cw = _reduce_capture(events, j, attrs, bytearray(shadow))
        pd = _by_tick(pw)
        cd = _by_tick(cw)
        # The anchor prefix: the port's first `prefix` ticks against the
        # capture's groups at the same ticks (a candidate that cannot match
        # them is a repeated note elsewhere, not this section). The first
        # prefix-passing candidate is the section's chronological occurrence.
        pre = align_groups([pd.get(t, []) for t in range(prefix)],
                           [cd.get(t, []) for t in range(prefix + 1)])
        if not pre.ok:
            continue
        return _evaluate_candidate(section, index, ci, j, captures, attrs,
                                   allow_diff)

    # Step 2: the rotation anchor (the §M4.4 item-1 section-boundary voice-cursor
    # gap). The constant channel rotation k is read from the two cursors, not
    # searched: the port's cursor is the first key-on's channel - 1 (seq_start
    # releases every voice, so the first note lands one past the cursor), and the
    # capture's cursor is the channel of the last key-on before the candidate
    # (the boundary tail note). A candidate whose register is not the port's
    # first key-on rotated by that k is rejected, so a wrongly-rotated copy of
    # the section fails here rather than by prefix coincidence.
    port_cursor = (key_ch - 1) % 18 if key_ch is not None else 17
    rotated_cands = []                 # (ci, j, ms, k)
    for ci, (path, _header, events) in enumerate(captures):
        for j, (ms, reg, val) in enumerate(events):
            if (ci, j) < search_from:
                continue
            if val != key_w[4] or not is_key_on(reg, val):
                continue
            cap_cursor = 17
            for jj in range(j - 1, -1, -1):
                if is_key_on(events[jj][1], events[jj][2]):
                    cap_cursor = opl_channel(events[jj][1])
                    if cap_cursor is None:
                        cap_cursor = 17
                    break
            k = (cap_cursor - port_cursor) % 18
            if reg == rotate_reg(key_w[3], k):
                rotated_cands.append((ci, j, ms, k))
    for ci, j, _ms, k in rotated_cands:
        rotated = rotate_section(section, k)
        path, _header, events = captures[ci]
        shadow = _shadow_from(events, j)
        pw = _reduce_port(rotated, bytearray(shadow))
        attrs = _port_attr_map(rotated)
        cw = _reduce_capture(events, j, attrs, bytearray(shadow))
        pd = _by_tick(pw)
        cd = _by_tick(cw)
        pre = align_groups([pd.get(t, []) for t in range(prefix)],
                           [cd.get(t, []) for t in range(prefix + 1)])
        if not pre.ok:
            continue
        return _evaluate_candidate(rotated, index, ci, j, captures, attrs,
                                   allow_diff)
    if section.bank in allow_unanchored:
        res.status = 'unanchored-allowed'
        res.reason = ('no anchor in the capture for this bank (allowed by name: '
                      'the measured section-boundary voice-cursor gap)')
    else:
        res.status = 'unanchored'
        res.reason = ('no capture key-on %#04x=%#04x with matching first %d ticks '
                      '(%d candidate%s)' % (key_w[3], key_w[4], prefix,
                                            len(cands) + len(rotated_cands),
                                            '' if len(cands) + len(rotated_cands) == 1 else 's'))
    return res


def summarize_section(res, index_names):
    s = res.section
    name = resource_name(s.dc, index_names) if s.dc else 'boot'
    head = 'section %d %s bank=%08X' % (res.index, name, s.bank)
    if res.status == 'anchored':
        extra = ('anchor capture %d ms (frame %d tick %d); claimed ticks 0..%d '
                 '(%d ticks, %d reduced writes)'
                 % (res.anchor_ms, s.writes[s.anchor_idx][0], s.anchor_tick,
                    res.claimed - 1, res.claimed, res.reduced))
        extra += '; per-tick multiset equal through tick %d' % (res.per_tick_equal - 1)
        if res.per_tick_equal == 0:
            extra = extra.replace('equal through tick -1', 'not equal at tick 0')
        if res.jittered:
            extra += '; %d declared whole-tick label slack move(s)' % res.jittered
        extra += ('; boundary tick %d %s (not claimed)'
                  % (res.boundary_rel, 'equal' if res.boundary_equal else 'differs'))
        return head + ': ' + extra
    if res.status in ('diff', 'diff-allowed'):
        prefix = ''
        if res.status == 'diff-allowed':
            prefix = '; claimed matched prefix %d ticks' % res.claimed
            if res.prefix_min is not None:
                prefix += ' (pinned minimum %d)' % res.prefix_min
        return (head + ': FIRST DIFFERENCE (%s) at port relative tick %d: '
                'port [%s] vs capture [%s]%s%s'
                % (res.first_diff_kind, res.first_diff,
                   _fmt_tick(res.first_diff_port), _fmt_tick(res.first_diff_cap),
                   ' (DIFF ALLOWED named gap: ' + res.reason + ')'
                   if res.status == 'diff-allowed' else '', prefix))
    if res.status == 'unanchored-allowed':
        return head + ': UNANCHORED named gap (' + res.reason + ')'
    return head + ': ' + res.status.replace('-', ' ') + ' (' + str(res.reason) + ')'


def compare_trace(name, port_path, capture_paths, min_ticks=0,
                  allow_unanchored=(), allow_diff=(), game_dir=None,
                  verbose=False, json_out=None, expect_sections=None,
                  prefix_pins=None):
    sections, _s_lines = parse_port(port_path)
    if prefix_pins is None and name in TRACE_PINS:
        prefix_pins = TRACE_PREFIX_PINS.get(name, {})
    missing = [p for p in capture_paths if not os.path.exists(p)]
    if missing:
        dirs = {os.path.dirname(p) for p in capture_paths}
        if not any(os.path.isdir(d) for d in dirs):
            print('audio-compare: SKIP %s: no capture at %s'
                  % (name, ', '.join(capture_paths)))
            return 3
        # A directory that exists but is short a file is a partial loss: name
        # it and fail. Only the absent-directory case above is the skip.
        print('audio-compare: %s: MISSING capture file(s): %s'
              % (name, ', '.join(missing)))
        return 1
    captures = []
    for path in capture_paths:
        try:
            header, events = parse_capture(path)
        except (OSError, ValueError) as exc:
            print('audio-compare: %s: cannot decode %s: %s' % (name, path, exc))
            return 2
        captures.append((path, header, events))
    index_names = read_index_names(game_dir)
    allow = set(allow_unanchored)
    allow_d = set(allow_diff)
    expected = expect_sections
    if expected is None:
        expected = TRACE_SECTIONS.get(name)
    results = []
    total = 0
    prefix_total = 0
    failed = []
    named_gaps = []
    search_from = (0, 0)
    print('audio-compare: %s (capture %s)'
          % (name, ', '.join(os.path.basename(p) for p, _, _ in captures)))
    for i, s in enumerate(sections):
        if s.anchor_idx is None and s.dc == 0 and not s.attributed:
            continue                      # the boot's two OPL reset writes
        if not s.attributed:
            r = SectionResult(s, i)
            r.status = 'unattributed'
            r.reason = ('no S metadata at or after the section start; its '
                        'bank is unknown, not the preceding song\'s')
        else:
            r = compare_section(s, i, captures, allow, allow_d, search_from)
            if r.capture_end is not None:
                search_from = r.capture_end
        if r.status == 'diff-allowed' and prefix_pins is not None:
            r.prefix_min = prefix_pins.get(r.index)
        results.append(r)
        print('  ' + summarize_section(r, index_names))
        if r.status == 'anchored':
            total += r.claimed
        elif r.status == 'diff':
            failed.append('section %s first difference (%s) at relative tick %d'
                          % (resource_name(s.dc, index_names), r.first_diff_kind,
                             r.first_diff))
        elif r.status in ('unanchored-allowed', 'diff-allowed'):
            named_gaps.append('%08X' % s.bank)
            if r.status == 'diff-allowed':
                prefix_total += r.claimed
                if prefix_pins is not None:
                    if r.prefix_min is None:
                        failed.append('section %d bank %08X: diff-allowed matched '
                                      'prefix has no per-section pin'
                                      % (r.index, s.bank))
                    elif r.claimed < r.prefix_min:
                        failed.append('section %d bank %08X: claimed matched prefix '
                                      '%d ticks, pinned minimum %d'
                                      % (r.index, s.bank, r.claimed, r.prefix_min))
        elif r.status in ('no-claim', 'no-key-on'):
            # A one-tick section (the replay ends at the next song's start), or
            # a section with no key-on to anchor (the sample-queue hand-off
            # s16snd2+0x21BFA, or a song cut before its first note): nothing to
            # claim. Reported by name; --min-ticks is what stops a trace
            # shrinking to nothing.
            named_gaps.append('%08X' % s.bank)
        elif r.status in ('unanchored', 'unattributed'):
            failed.append('section %s %s (%s)'
                          % (resource_name(s.dc, index_names), r.status, r.reason))
        if verbose:
            print('    writes=%d anchor_tick=%s last_tick=%s attributed=%s'
                  % (len(s.writes), s.anchor_tick, s.last_tick, s.attributed))
    if expected is not None:
        if len(results) != len(expected):
            failed.append('expected %d section(s) from the scenario inventory, '
                          'got %d' % (len(expected), len(results)))
        else:
            for (bank, kind), r in zip(expected, results):
                if r.section.bank != bank:
                    failed.append('section %d: expected bank %08X, got %08X'
                                  % (r.index, bank, r.section.bank))
                elif kind == 'matched' and r.status != 'anchored':
                    failed.append('section %d bank %08X: expected matched, got %s'
                                  % (r.index, bank, r.status))
                elif kind == 'named' and r.status in (
                        'unanchored', 'diff', 'unattributed'):
                    failed.append('section %d bank %08X: expected a named gap, '
                                  'got %s' % (r.index, bank, r.status))
    if prefix_pins:
        by_index = {r.index: r for r in results}
        for index, minimum in sorted(prefix_pins.items()):
            r = by_index.get(index)
            if r is None:
                failed.append('section %d: pinned matched prefix %d ticks is '
                              'absent from the port dump' % (index, minimum))
            elif r.status not in ('anchored', 'diff-allowed'):
                failed.append('section %d bank %08X: matched-prefix claim lost '
                              '(%s), pinned minimum %d'
                              % (index, r.section.bank, r.status, minimum))
    print('audio-compare: %s: %d compared ticks over %d section(s); %d named gap(s) %s'
          % (name, total, sum(1 for r in results if r.status == 'anchored'),
             len(named_gaps), ('[' + ' '.join(named_gaps) + ']') if named_gaps else ''))
    if prefix_total:
        print('audio-compare: %s: %d claimed matched-prefix ticks over %d '
              'diff-allowed section(s); the unmatched remainders stay named gaps'
              % (name, prefix_total,
                 sum(1 for r in results if r.status == 'diff-allowed')))
    if total < min_ticks:
        failed.append('claimed %d ticks, pinned minimum %d' % (total, min_ticks))
    ok = not failed
    print('audio-compare: %s: %s' % (name, 'ok' if ok else 'FAIL'))
    for f in failed:
        print('audio-compare: %s: FAIL: %s' % (name, f))
    if json_out:
        rec = {
            'name': name,
            'ok': ok,
            'total_ticks': total,
            'min_ticks': min_ticks,
            'prefix_ticks': prefix_total,
            'prefix_sections': sum(1 for r in results if r.status == 'diff-allowed'),
            'sections': len(results),
            'compared': sum(1 for r in results if r.status == 'anchored'),
            'named_gaps': named_gaps,
            'captures': [os.path.basename(p) for p, _, _ in captures],
            'section_detail': [{'index': r.index,
                                'bank': '%08X' % r.section.bank,
                                'status': r.status,
                                'claimed_ticks': r.claimed,
                                'prefix_min': r.prefix_min} for r in results],
            'failed': failed,
        }
        parent = os.path.dirname(json_out)
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(json_out, 'w') as f:
            f.write(json.dumps(rec, sort_keys=True) + '\n')
    return 0 if ok else 1


def cmd_compare(a):
    name = a.name or os.path.basename(a.port)
    min_ticks = a.min_ticks
    allow = [int(x, 16) for x in a.allow_unanchored.split(',') if x]
    allow_diff = [int(x, 16) for x in a.allow_diff.split(',') if x]
    pin = TRACE_PINS.get(name)
    if pin is not None:
        if min_ticks == 0:
            min_ticks = pin[0]
        allow = list(pin[1]) + allow
        allow_diff = list(pin[2]) + allow_diff
    prefix_pins = None
    if a.prefix_pin:
        prefix_pins = dict(TRACE_PREFIX_PINS.get(name, {}))
        for part in a.prefix_pin:
            index, minimum = part.split(':')
            prefix_pins[int(index, 0)] = int(minimum, 0)
    expect = None
    if a.expect_sections:
        expect = []
        for part in a.expect_sections.split(','):
            if not part:
                continue
            bank, kind = part.split(':')
            expect.append((int(bank, 16), kind))
    return compare_trace(name, a.port, a.capture, min_ticks, allow, allow_diff,
                         a.game_dir, a.verbose, a.json_out, expect, prefix_pins)


def cmd_capture_end(a):
    x = poll_x_frame(a.capture)
    if x is None:
        print(0)
        return 0
    print(min(a.max, x) if a.max is not None else x)
    return 0


def cmd_expected_files(a):
    files = EXPECTED_FILES.get(a.scenario)
    if files is None:
        print('audio-compare: unknown scenario %s' % a.scenario)
        return 2
    for f in files:
        print(f)
    return 0


def cmd_summary(a):
    """The covered/missing inventory, read from the per-trace reports the
    oracle writes (`compare --json-out`) and the capture tree.

    Fails (exit 1) on a present directory whose file set differs from the
    expected inventory, a present scenario with no report (`NOT RUN`), a report
    that says `ok:false` (`FAIL`), a malformed `.dro`, or a skip other than the
    declared gp-u17 wall-paced refusal. The absent-directory skip is the one
    non-failing absence."""
    rows = {}
    if a.jsonl and os.path.exists(a.jsonl):
        with open(a.jsonl) as f:
            for line in f:
                line = line.strip()
                if line:
                    rec = json.loads(line)
                    rows[rec['name']] = rec
    if a.reports and os.path.isdir(a.reports):
        import glob
        for path in sorted(glob.glob(os.path.join(a.reports, '*', 'report.json'))):
            try:
                with open(path) as f:
                    rec = json.load(f)
            except (OSError, ValueError) as exc:
                print('audio-compare: cannot read report %s: %s' % (path, exc))
                return 1
            rows.setdefault(rec.get('name', ''), rec)
    bad = []
    missing, present = [], []
    total_files = 0
    music_files = 0
    for name in GP_TRACES:
        d = os.path.join(a.captures, name)
        expected = EXPECTED_FILES[name]
        if not os.path.isdir(d):
            missing.append(name)
            continue
        actual = sorted(os.path.basename(p) for p in capture_files(d))
        missing_files = [f for f in expected if f not in actual]
        extra = [f for f in actual if f not in expected]
        present.append((name, actual, missing_files, extra))
        total_files += len(actual)
        for f in missing_files:
            bad.append('%s: missing %s' % (name, f))
        for f in extra:
            bad.append('%s: unexpected %s' % (name, f))
        for path in capture_files(d):
            try:
                _h, events = parse_capture(path)
            except (OSError, ValueError) as exc:
                bad.append('%s/%s: cannot decode (%s)'
                           % (name, os.path.basename(path), exc))
                continue
            if first_music_key_on(events) is not None:
                music_files += 1
    others = []
    for name in OTHER_K11_TRACES:
        others += capture_files(os.path.join(a.captures, name))
    audio = capture_files(a.audio_captures) if a.audio_captures else []
    print('audio-compare: gameplay inventory: %d traces, %d/%d .dro files '
          '(%d with a music section), %d director%s absent'
          % (len(GP_TRACES), total_files, EXPECTED_TOTAL, music_files,
             len(missing), 'y' if len(missing) == 1 else 'ies'))
    for name, files, missing_files, extra in present:
        note = ''
        if missing_files:
            note += ' MISSING ' + ' '.join(missing_files)
        if extra:
            note += ' EXTRA ' + ' '.join(extra)
        rec = rows.get(name)
        if rec is None:
            print('  %-24s %d file(s): NOT RUN%s' % (name, len(files), note))
            bad.append('%s: NOT RUN' % name)
            continue
        if rec.get('skipped'):
            allowed = (name == 'gp-u17-device2'
                       and 'wall-paced' in str(rec.get('skipped', '')))
            print('  %-24s %d file(s): SKIP (%s)%s'
                  % (name, len(files), rec['skipped'], note))
            if not allowed:
                bad.append('%s: unexpected skip (%s)'
                           % (name, rec.get('skipped')))
            continue
        if not rec.get('ok'):
            print('  %-24s %d file(s): FAIL %s ticks (pinned %s)%s'
                  % (name, len(files), rec.get('total_ticks'), rec.get('min_ticks'), note))
            bad.append('%s: FAIL' % name)
            continue
        gaps = (' named-gap ' + ' '.join(rec['named_gaps'])) if rec['named_gaps'] else ''
        print('  %-24s %d file(s): ok %d ticks (pinned %d)%s%s'
              % (name, len(files), rec['total_ticks'], rec['min_ticks'], gaps, note))
    for name in missing:
        print('  %-24s SKIP (no capture directory)' % name)
    measured = [r['total_ticks'] for r in rows.values()
                if not r.get('skipped') and r.get('ok')]
    if measured:
        print('audio-compare: minimum measured claim: %d ticks' % min(measured))
    print('audio-compare: not gameplay: %d k11 .dro file(s) (%s)'
          % (len(others),
             ', '.join(os.path.relpath(p, a.captures) for p in others) or 'none'))
    print('audio-compare: not gameplay: %d audio-capture .dro file(s) (%s)'
          % (len(audio), ', '.join(os.path.basename(p) for p in audio) or 'none'))
    if bad:
        for b in bad:
            print('audio-compare: summary FAIL: %s' % b)
        return 1
    print('audio-compare: summary ok')
    return 0


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)

    c = sub.add_parser('compare', help='compare one port dump against its capture file(s)')
    c.add_argument('--name')
    c.add_argument('--port', required=True)
    c.add_argument('--capture', action='append', required=True)
    c.add_argument('--min-ticks', type=int, default=0)
    c.add_argument('--allow-unanchored', default='')
    c.add_argument('--allow-diff', default='')
    c.add_argument('--prefix-pin', action='append', default=[],
                   help='override one section\'s matched-prefix minimum: INDEX:TICKS')
    c.add_argument('--game-dir')
    c.add_argument('--expect-sections', default='',
                   help='override the scenario inventory: hash:matched,hash:named')
    c.add_argument('--json-out')
    c.add_argument('--verbose', action='store_true')
    c.set_defaults(func=cmd_compare)

    e = sub.add_parser('capture-end', help='the capture X frame, capped (for --end)')
    e.add_argument('--capture', required=True)
    e.add_argument('--max', type=int)
    e.set_defaults(func=cmd_capture_end)

    x = sub.add_parser('expected-files',
                       help='the expected .dro basenames of one scenario')
    x.add_argument('--scenario', required=True)
    x.set_defaults(func=cmd_expected_files)

    s = sub.add_parser('summary', help='the covered/missing trace inventory')
    s.add_argument('--captures', required=True)
    s.add_argument('--audio-captures')
    s.add_argument('--jsonl')
    s.add_argument('--reports')
    s.set_defaults(func=cmd_summary)

    t = sub.add_parser('self-test', help='point at the unit test module')
    t.set_defaults(func=lambda _a: (print('self-test: python3 -m unittest '
                                        'tools.tests.test_audio_compare'), 0)[1])

    a = ap.parse_args(argv)
    return a.func(a)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
