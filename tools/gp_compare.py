#!/usr/bin/env python3
"""The gameplay oracle (spec 2026-09-30-gameplay-ground-truth-design.md §4.3,
record §G.13..). Two claims, each a ratchet on its first unexplained item:

  frames  the capture's distinct frames (data/k11-captures/<scenario>/
          frame_%05d.raw.gz, gp_capture) against the port's displayed frames
          (<port>/frame_%05d.ipx, test_gp_replay) with title_compare.explain's
          model: clean, a byte splice of adjacent port frames (the 70.09 Hz
          capture against the 60.05 Hz game), or one transition row. All-black
          capture frames are the documented capture artefact and are skipped.
          Frames named in BANDS_ALLOWED for the scenario are explained ONLY by
          their per-frame byte recipe (a top/bottom byte-bind to one unique
          adjacent port pair plus a pinned band hash; record §G5) — never a
          class, and a named frame whose bytes are not the recipe fails.
          Frames named in SCAN3_ALLOWED are explained ONLY by their per-frame
          three-segment recipe: a pinned top-row hash that at least one EARLIER
          dump frame must hold, plus one unique adjacent port pair (B, B+1)
          byte-binding the rows below the top (record §G6). Frames named in
          OVERLAY_ALLOWED are explained ONLY by their per-frame glyph-overlay
          recipe: one port frame's pixels on the mask where a second port frame
          differs from a third (a pinned glyph fingerprint), and the third's
          (the screen's) off it (record §G7). None of the three tables is
          ever an extension of the two-frame model.
          The first unexplained capture frame must be >= --min-first N, and the
          window start (the first capture frame that shows the port's first frame)
          must be <= --max-start and < N, so a regressed port cannot slide the window
          forward past the frame that set N. Not claimed: the order of the port's
          frames, and that every port frame appears (a coverage count is reported).
  trace   the capture's S records (poll.log) against the port's T records
          (trace.txt) by the frame counter f, over gp_session.TRACE_FIELDS;
          the first differing f must be >= --trace-min-first F. tick is
          reported apart (host-timed, spec §7 Q6). --dev-words adds
          gp_session.DEV_FIELDS (dev1/dev2, the joystick device words behind
          DS_00101514, record §H6) to the same claim -- gp-u17-device2's only:
          every older capture's poll.log lacks the fields, so the flag is what
          makes them part of a claim, and a capture without them fails it.

The search for a frame's explanation tries the port frames [p - BACK, p +
AHEAD) around the last explained index p first, then the whole dump, so the
result is the unwindowed classification (the window is a search order, a
harness value). Enforced runs stop at the first unexplained frame; --report
goes on (up to REPORT_MAX; --max-unexplained overrides, 0 lifts the cap) and
always exits 0. An absent capture skips (exit 0); a present capture with an
unpinned N or start fails. Stdlib only; title_compare and gp_session are
read-only here."""
import argparse
import collections
import gzip
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs
import title_compare as tc

BACK, AHEAD = 2, 64          # harness: the search order (spec §4.3), not a game value
REPORT_MAX = 5
IPX_BYTES = 64000 + 768

# Per-scenario named per-frame allowances for the frames claim, patterned on
# title_compare.ATTRACT2_SPLICE3_ALLOWED (record §G5): only the named capture
# frames, each only with its own byte recipe, never a class and never an
# extension of the two-frame model. An entry is (capture frame j, top_row, band_hi,
# band_sha256): the frame is explained ONLY if
#   1. sha256 of its bytes [top_row*ROW, band_hi*ROW) equals band_sha256, and
#   2. EXACTLY ONE adjacent pair (P, P+1) in the CURRENT port dump holds: the
#      capture's rows [0, top_row) equal port[P]'s rows and the capture's rows
#      [band_hi, FRAME_H) equal port[P+1]'s.
# The bound rows are byte-verified against the port's dump, so a port that renders
# either bound frame differently un-explains the capture frame (Review Focus #1;
# BandAllowanceTests). The band itself has no port binding and is capture-pinned
# bytes: that is the allowance's named limit — the band is a present state the
# driver never dumped (one frame per f; record §K.10 item 2), and its cause stays
# the named gap §K.12 records. A named frame the two-frame model explains now is
# reported as a stale name; a named frame that is not the named bytes FAILS.
BANDS_ALLOWED = {
    'gp-keys-fight': (
        # Capture 835 (raw 3934): the altq-n (QUIT TO DOS?, c = 0x833) scan-out
        # catch-up frame. Rows 0..80 are byte-equal to the port's f = 0x832 frame,
        # rows 92..199 to f = 0x833, and rows 81..91 hold an 11-row band no port
        # frame holds (2442 of its bytes equal neither bound frame; §G5 re-measured
        # at 5f874678, which is also where N/MAX_START/TRACE were pinned). The pair
        # (592, 593) is the dump's unique match (measured: 1 hit over all 789
        # frames).
        (835, 81, 92, 'd9c0ac62f7e0c0d4082fba8d195eae4f767c56a7887bdf7346450511f998cad8'),
    ),
    'gp-u21-armadon-ending': (
        # Endgame truth Task 6 (record §E6): fifteen scan-out catches of a present the
        # driver never dumped — the same class as the U11 entry and U13's 5120 below,
        # but of the copy itself. The game's present copies the composed 320x200 frame
        # into the aperture top to bottom; a 70.09 Hz scan-out that lands inside that
        # copy reads the previous present's rows above the copy's front, the new
        # present's rows below it, and one mixed row at the front. Measured on this
        # 4,974-frame dump: each frame below has two byte-equal segments from one
        # unique adjacent dumped pair and a band (as a row range, its front row mixed;
        # plus, where the front row is inside the band, that row) that no dumped
        # present holds — no single present carries the band's rows. Fourteen of the
        # fifteen have every one of their 64,000 pixels equal to a pixel of some port
        # frame within ±4 of the pair, so their content is a pure tear of two dumped
        # presents; the fifteenth (5119, the mode-0x15 fade) holds 425 band pixels that
        # no frame within ±4 has, a fade step between the two presents (the U13 5120
        # class).
        # Every neighbour is clean, a splice or a transition, so the port's own
        # sequence is intact. The entries' order is ascending; `top_row` 0 and
        # `band_hi` 200 are the entries whose band reaches an edge (the pair then binds
        # through the other segment alone).
        #
        # Capture 426 (raw 3002): round 1's mode-8 entry. Rows 0..57 are port 291's
        # (f=0505 tick=00000557 mode=0008), rows 191..199 are port 292's (f=0506
        # tick=00000558 mode=0008) and rows 58..190 are the mixed band; (291, 292) is
        # the dump's unique match. Neighbours 425 clean at 291 and 427 clean at 295.
        (426, 58, 191, '5d3d8e35e3f51cf6b311f324e6464e3867ff0d1b14245a6bc171596019018b4b'),
        # Capture 756 (raw 3333): the same mode-8 screen two presents on. Rows 0..150
        # are port 577's (f=0623 tick=00000675 mode=0008), rows 154..199 port 578's
        # (f=0624 tick=00000676) and rows 151..153 the band; (577, 578) unique.
        # Neighbours 755 clean at 577 and 757 a splice.
        (756, 151, 154, 'e59ab79c2ef799dc9c62fd288ecb3beab3e0872789ef00233e3ebbe87c96a3de'),
        # Capture 4317 (raw 8037): the final's mode-0x0D -> 0x0C transition. Rows 0..3
        # are port 3610's (f=1577 tick=00001636 mode=000C), rows 32..199 port 3611's
        # (f=1578 tick=00001637 mode=000C) and rows 4..31 the band; (3610, 3611)
        # unique. Neighbours 4316 clean at 3609 and 4318 a splice. (The same entry
        # U19's 4362 and U20's 4272 name, caught inside the copy.)
        (4317, 4, 32, '3144a709b3ec68ebd634a739d50dc4d828745de7cabf039dd1bf22b13687fafb'),
        # Capture 4470 (raw 8216): the mode-0xF master screen. Rows 47..199 are port
        # 3741's (f=15F8 tick=000016D1 mode=000F) and rows 0..46 the band (the capped
        # front row 46 included: 216 of its 320 px are 3741's); (3740, 3741) is the
        # unique match, bound by the bottom rows. Neighbour 4469 clean at 3739, 4471 a
        # transition.
        (4470, 0, 47, 'ddafe873449ea92c58b677ed0eb03621b32e20e027b26bb642507bd91f6f0e6e'),
        # Capture 4477 (raw 8223): rows 29..199 port 3747's (f=15FE tick=000016D7
        # mode=000F), rows 0..28 the band; (3746, 3747) unique. Neighbours 4476 clean
        # at 3745 and 4478 a transition.
        (4477, 0, 29, '2d86bf24b3159f478651834c631e1f5444edde3a2546871b40e1d53e0db2dc19'),
        # Capture 4486 (raw 8232): rows 0..194 port 3753's (f=1604 tick=000016DD
        # mode=000F), rows 195..199 the band; (3753, 3754) unique. Neighbours 4485 a
        # splice and 4487 clean at 3755.
        (4486, 195, 200, '1a44ec8841928d96d6f4fbd54f642751f01484928ba811f212f51b847506bf08'),
        # Capture 4491 (raw 8237): rows 11..199 port 3759's (f=160A tick=000016E3
        # mode=000F), rows 0..10 the band; (3758, 3759) unique. Neighbours 4490 clean
        # at 3757 and 4492 a transition.
        (4491, 0, 11, '6831e908ba5d1ebcae85df1864be3b05c7d8606fcdb8c1bf80c3a96495e0082e'),
        # Capture 4497 (raw 8243): rows 82..199 port 3764's (f=160F tick=000016E8
        # mode=000F), rows 0..81 the band; (3763, 3764) unique. Neighbours 4496 clean
        # at 3762 and 4498 clean at 3764.
        (4497, 0, 82, 'df42f0b55e5a6d540833176d9931e422279d4034524bdb733ae0c0a5691bf3ff'),
        # Capture 4502 (raw 8248): rows 13..199 port 3768's (f=1613 tick=000016EC
        # mode=000F), rows 0..12 the band; (3767, 3768) unique. Neighbours 4501 clean
        # at 3766 and 4503 a splice.
        (4502, 0, 13, '3cf9379061f7441b7c4471b55310e473981af591de9127889290b38c964c3563'),
        # Capture 4505 (raw 8251): rows 0..4 port 3770's (f=1615 tick=000016EE
        # mode=000F), rows 32..199 port 3771's (f=1616) and rows 5..31 the band;
        # (3770, 3771) unique. Neighbours 4504 clean at 3769 and 4506 a splice.
        (4505, 5, 32, 'c72d73dac4804087ccd98a88e53a5106f9fa8624eab027d6e6bc223bb7ce3555'),
        # Capture 4511 (raw 8257): rows 65..199 port 3776's (f=161B tick=000016F4
        # mode=000F), rows 0..64 the band; (3775, 3776) unique. Neighbours 4510 clean
        # at 3774 and 4512 clean at 3776.
        (4511, 0, 65, '30e62f335803a407aa4620248f09698f7c20f7f7c939d71b3969aa804d55469e'),
        # Capture 4527 (raw 8273): rows 0..134 port 3788's (f=1627 tick=00001700
        # mode=000F), rows 135..199 the band; (3788, 3789) unique. Neighbours 4526
        # clean at 3788 and 4528 clean at 3790.
        (4527, 135, 200, '929bc3ad9b85d5c8d642b4d5c10dff9133c466a13a5f972d823330ffe1e5efc9'),
        # Capture 4554 (raw 8300): rows 0..102 port 3811's (f=163E tick=00001717
        # mode=000F), rows 103..199 the band; (3811, 3812) unique. Neighbours 4553
        # clean at 3811 and 4555 clean at 3813.
        (4554, 103, 200, 'e10b51fb7b5bd6efa86e215f5bbbe4a745e845bc1bd25bccbfd5bc57d435f89f'),
        # Capture 4560 (raw 8306): rows 0..109 port 3816's (f=1643 tick=0000171C
        # mode=000F), rows 110..199 the band; (3816, 3817) unique. Neighbours 4559
        # clean at 3816 and 4561 clean at 3818.
        (4560, 110, 200, 'b0ca25c494237c479259872d18dd2967d93bd49a1850b7f6de03ff15c4b66950'),
        # Capture 5119 (raw 8872) at the ending's mode-0x15 fade, the U13 5120 class:
        # rows 2..199 are port 4301's (f=182A tick=00001903 mode=0015) and rows 0..1
        # the band; (4300, 4301) unique. The band's 640 pixels: 215 equal port 4301's
        # (31 of those also equal port 4300's, where the two frames coincide) and 425
        # equal no frame within ±4 — the fade's own step between the two presents (no
        # constant per-channel offset reproduces them), the "present state the driver
        # never dumped" U13's 5120 names.
        # Neighbours 5118 clean at 4300 and 5120 clean at 4302.
        (5119, 0, 2, '287a61f4cc91a282bfdd583a0ebf510b90d87de913545aeaa8ed76d60b0e950a'),
    ),
    'gp-u22-diablo-ending': (
        # Endgame truth Task 7 (record §E7): eight present-copy scan-outs of the
        # DIABLO capture, the two-segment shape (rows 0..top_row-1 of one present and
        # band_hi..199 of the next, the band a state no dumped present holds; the
        # U13/U21 class). Six more frames of this capture are unique three-frame
        # scan-outs (SCAN3_ALLOWED) and one is the loader overlay (OVERLAY_ALLOWED);
        # where a three-frame recipe binds all 200 rows it is used instead of a band.
        # The entries' order is ascending; `top_row` 0 and `band_hi` 200 are the
        # entries whose band reaches an edge (the pair then binds through the other
        # segment alone).
        #
        # Capture 1988 (raw 4829), mode 9: rows 0..116 port 1638's (f=0B02
        # tick=00000B57 mode=0009), rows 122..199 port 1639's (f=0B03 tick=B58,
        # mode 0009) and rows 117..121 the band; (1638, 1639) unique. Neighbours
        # 1987 clean at 1638 and 1989 a (1639, 1640) splice.
        (1988, 117, 122, '8910a572dd6de6db88203905a608459ef595d210c64ffb39498a76e4270365b9'),
        # Capture 2014 (raw 4855), mode 9: rows 66..199 are port 1661's (f=0B19
        # tick=00000B6E mode=0009) and rows 0..65 the band; (1660, 1661) unique.
        # Neighbours 2013 clean at 1659 and 2015 a (1661, 1662) splice.
        (2014, 0, 66, '62708bc21c8c9429e72c92b7786eeb331ec4429ec7d909fe2100a4456475e71a'),
        # Capture 2020 (raw 4861), mode 9: rows 13..199 are port 1666's (f=0B1E
        # tick=00000B73 mode=0009) and rows 0..12 the band; (1665, 1666) unique.
        # Neighbours 2019 clean at 1664 and 2021 a (1666, 1667) splice.
        (2020, 0, 13, '4e2d273ec17c1c645cde62e95f74cc75bb427a67ce58fb11fb5edffff58fdf54'),
        # Capture 5008 (raw 8959), the ending's mode-0x15 fade: rows 0..176 port
        # 4270's (f=184E tick=00001929 mode=0015), rows 191..199 port 4271's
        # (f=184F tick=0000192A) and rows 177..190 the band (the fade's own step
        # between the two presents, U13's 5120 class); (4270, 4271) unique.
        # Neighbours 5007 clean at 4270 and 5009 clean at 4272.
        (5008, 177, 191, '3e0e4a3ce88a017d190e8f06e26b2f60e0a243210d3853dde3b3a304603021ef'),
        # Capture 5013 (raw 8966), the same fade: rows 2..199 port 4277's (f=1855
        # tick=00001930 mode=0015) and rows 0..1 the band (the fade's step at the
        # top edge); (4276, 4277) unique. Neighbours 5012 clean at 4275 and 5014
        # clean at 4277.
        (5013, 0, 2, '0ba833920723fcc0d3cd9e8e408c84af8a78ac80139d448173a3f935ba7d9144'),
        # Capture 5015 (raw 8968): rows 2..199 port 4279's (f=1857 tick=00001932
        # mode=0015) and rows 0..1 the band; (4278, 4279) unique. Neighbour 5014
        # clean at 4277; 5016 is 5015's successor state (its own named band below).
        (5015, 0, 2, 'd6a7f2bc5434edc9cb86ec6fe81e32f44b09891dfadbf99206cee5c1b4fb69b6'),
        # Capture 5016 (raw 8969): rows 0..93 port 4280's (f=1858 tick=00001933
        # mode=0015), rows 134..199 port 4281's (f=1859 tick=00001934) and rows
        # 94..133 the band; (4280, 4281) unique. Neighbour 5017 clean at 4281.
        (5016, 94, 134, 'b0f6fe0c5e29acab29da07a98ad2ffd274f39bbfc4ad9b2fc9809481ac4c7f06'),
        # Capture 5024 (raw 8983): rows 0..148 port 4291's (f=1863 tick=0000193E
        # mode=0015), rows 189..199 port 4292's (f=1864 tick=0000193F) and rows
        # 149..188 the band; (4291, 4292) unique. Neighbours 5023 clean at 4291
        # and 5025 clean at 4293.
        (5024, 149, 189, 'f5d6102f0b72f59341a505962e2541e1fd83203c3f4fe1560b4d9c62cec2b673'),
    ),
    'gp-u13-ending2': (
        # Capture 5120 (raw 8839) at the ending's mode-0x15 fade (record §P3).
        # Its neighbour 5119 is clean at port 4315 (f=1824) and 5121 is a
        # (4317, 4318) splice, so the frame sits at the f=1825/f=1826 fade step.
        # Rows 1..4 are the dump's ONLY unheld rows (measured over all 4940
        # default frames, 4954 with --end-to-last-s: no other row of 0..199
        # has zero holders); row 0 is port 4316/4317/4318's (identical there)
        # and the whole range rows 5..199 is port 4317's alone (no other frame
        # holds all 195 rows), so the pair (4316, 4317) is the dump's unique
        # match. The band is 4 rows x 320 =
        # 1280 pixels (3840 bytes), of which exactly 13 pixels differ from port
        # 4317's rows: 8 are 00001c where 4317's index is 0x32 (black in the
        # frame's DAC), 1 is black where it is 0x2E (100000) and 4 are black
        # where it is 0x4D (00000c). The band is not a stale copy of any present
        # (no frame holds those rows). The neighbours 5119/5122 corroborate the
        # port's rendered colours at its own indices; they do not reproduce the
        # capture's 13 pixels: capture 5119 (clean at 4315) shows exactly
        # DAC_4315[0x32]/[0x2E]/[0x4D] = 000000/180000/000014 there and capture
        # 5122 (clean at 4318) shows 000000/100000/00000c.
        # Named limit (as the U11 entry): the band has no port binding, and its
        # cause is not isolated. The 1/5 bounds are capture-pinned, not
        # machine-forced: the shape also accepts wider bands (band_pair returns
        # 4316 for (0, 6), (0, 7), ... too), so the bounds follow the unheld
        # run's ends as the U11 entry's 81/92 do. No dumped frame's DAC renders
        # index 0x32 as 00001c (0 of 4940 default, 4954 with
        # --end-to-last-s) and 93 other pixels with index 0x32 are black in this
        # very capture frame, so no single palette applied to port 4317's
        # indices reproduces the band; the frame-wide constant-offset test picks
        # k = 0 (13 mismatching pixels) over every
        # other k (over the +/-64 domain the next best is 12651; over all 256
        # offsets it is 11282, k = +109), and the three offsets that do cover
        # the 13 ({-29, +14, +29}) are DAC aliases of the same two colours
        # (00001c occurs at five entries, black at 207), not a palette shift.
        (5120, 1, 5, '56b900b1562b3abdf600bb678e31abed485327c292ea26eb022fe7daf0e32250'),
    ),
}

# Per-scenario named THREE-frame scan-out allowances for the frames claim
# (record §G6): the mode-8 entry's catch-up burst (the gate at 0x25643 fails for
# iterations whose compute overruns an ISR tick, their presents are skipped, and
# the re-opened presents land inside ONE 70.09 Hz capture scan — see §G6 for the
# isolation and the rejection of the loader re-sync hypothesis there). Like
# BANDS_ALLOWED this is never a class and never an extension of the two-frame
# model: only the named capture frames, each only with its own byte recipe. An
# entry is (capture frame j, r1, r2, top_sha256): the frame is explained ONLY if
#   1. sha256 of its bytes [0, r1*ROW) equals top_sha256 (the still-held top rows
#      of the old screen, the segment no *unique* port frame answers for),
#   2. EXACTLY ONE adjacent pair (B, B+1) in the CURRENT port dump holds: the
#      capture's rows [r1, r2) equal port[B]'s rows and its rows [r2, FRAME_H)
#      equal port[B+1]'s (the middle range is vacuous when r1 == r2 — the U9
#      shape, where only the bottom rows are pair-bound), and
#   3. at least one port frame A with A < B holds the capture's rows [0, r1):
#      the still-scanned old screen IS one of the port's own earlier frames.
# The pair and the top are byte-verified against the port's dump, so a port that
# renders any bound segment differently un-explains the capture frame (Review
# Focus #1; Scan3AllowanceTests). The top segment is not uniquely bound: several
# frames can share those rows (§G6's table names the candidates) — the named
# hash plus the existence of an earlier holder is the allowance's named limit,
# and the old screen's whole frames are claimed by the capture frames below the
# ratchet anyway. A named frame the two-frame model explains now is reported as
# a stale name; a named frame that is not the named bytes FAILS.
SCAN3_ALLOWED = {
    'gp-idle-loss': (
        # Capture 2064 (raw 5190): the round-1 mode-8 entry catch-up (§G6).
        # Rows 0..2 are port 1665/1666's (f=0xC70/0xC71, identical there, the
        # screen held across raws 5187..5189), rows 3..97 are port 1669's
        # (f=0xC74) exclusively and rows 98..199 are port 1670's (f=0xC75)
        # exclusively; the pair (1669, 1670) is the 6301-frame dump's unique
        # match; f=0xC72/0xC73 were never blitted (their presents the gate
        # skipped — §G6).
        (2064, 3, 98, '7bdf2c561c38a264337bc74abd093172ac99ceebb8c818635e189530cf1c58dc'),
    ),
    'gp-u9-win': (
        # Capture 346 (raw 2830): the same mode-8 entry under U9's pokes, but
        # the shape is the unpoked one (§G6/§W.12). Rows 0..4 are port 218's
        # (f=0x48A, the first mode-8 frame, held across raws 2827..2829) and
        # rows 5..199 are port 221's (f=0x48D) exclusively — no middle band;
        # the vacuous-middle pair (220, 221) is the 1977-frame dump's unique
        # match; ports 219/220 (f=0x48B/0x48C) were never blitted.
        (346, 5, 5, '703828af123edf1a8c3bade8d42bdd56a68576cd87e13de7339bdd203dc8f0b6'),
    ),
    'gp-u10-ending': (
        # Capture 331 (raw 2831): the same mode-8 entry (§G6/§W.14). Rows 0..1
        # are port 216's (f=0x483, the first mode-8 frame, held across raws
        # 2828..2830; the dump's holders of those two rows before port 218 are
        # {216, 243, 1127}), rows 2..63 are port 218's (f=0x485) exclusively
        # and rows 64..199 are port 219's (f=0x486) exclusively; the pair
        # (218, 219) is the 4664-frame dump's unique match; port 217 (f=0x484)
        # was never blitted.
        (331, 2, 64, '2d6fb6ad829baae61c65a844fb44f05cb51393322afdce4ab8117960e1306ddd'),
    ),
    'gp-u13-ending2': (
        # Capture 4248 (raw 7887) at the ending's mode-0x0D -> 0x0C transition
        # (record §P2; the poll's S records f=152A mode=000D at ms=112474,
        # f=152B mode=000C at ms=112803 and f=152C at ms=112806). Rows 0..45
        # are port 3549's (f=152B tick=000015C7 mode=000D, the still-scanned old
        # screen), rows 46..96 are port 3550's (f=152B tick=000015CA
        # mode=000C) and rows 97..199 are port 3551's (f=152C tick=000015CB
        # mode=000C); the pair (3550, 3551) is the 4940-frame dump's unique
        # match for the two lower ranges, and port 3549 is the only frame
        # before it holding rows 0..45. The capture's neighbours show the same
        # scan: 4247 is clean at 3549 and 4249 is a two-frame splice of
        # (3551, 3552) at row 187 — the capture's scan shows three consecutive
        # presents.
        # Named limit (as the other entries and §G6): the top rows are bound by
        # hash and by the EXISTENCE of an earlier holder, not by its uniqueness
        # (measured: 3549 alone), and the row splits 46/97 are capture-pinned
        # bytes — the recipe claims nothing about the presents' timing.
        (4248, 46, 97, 'fc1044a6a9e5bb6cee142ce02811c4a4823585747351089bd1514a76685fc6e6'),
        # Capture 4320 (raw 8019) — the same three-present scan, 72 capture
        # frames later at the next mode-0x0D -> 0x0C transition (record §P3): its
        # S bracket is f=1566 mode=000D (ms=114172) -> f=1567 mode=000C. Rows
        # 0..17 are port 3613's (f=1567 tick=0000163C mode=000D; rows 14..17 are
        # 3613's among the three presents, and row 14 is 3613's alone, record
        # §P3.2), rows 18..107 are port 3614's (f=1567
        # tick=00001640 mode=000C; rows 88..107 exist nowhere else) and rows
        # 108..199 are port 3615's (f=1568 tick=00001641 mode=000C, the nearest
        # of the three; rows 108..189 exist nowhere else). The split is forced
        # by the bytes, not chosen: 3614 does not hold rows 14..17 (so r1 >= 18)
        # and 3613 does not hold row 18 (so no frame holds rows 0..18, r1 <= 18);
        # 3615 does not hold rows 88..107 (so r2 >= 108) and 3614 does not hold
        # rows 108..189 (so r2 <= 108). The pair (3614, 3615) is the 4940-frame
        # dump's unique match. Named limits (as the 4248 entry and §P2.3): the
        # top rows are bound by hash and by the EXISTENCE of an earlier holder
        # (measured: 3613 alone, but the recipe does not enforce uniqueness),
        # the row splits 18/108 are capture-pinned bytes (the PRESENTS' timing,
        # the scan-out's row rate and why three presents landed in one capture
        # frame are not claimed), and the middle and bottom are byte-bound to
        # the current dump.
        (4320, 18, 108, 'c8fa5b9305b132f09d1853594b40429750636f08da70c5e154dc5945ce4b8cb4'),
    ),
    'gp-u16-twop2': (
        # Capture 4089 (raw 6792): row 0 is byte-pinned by the hash and is held
        # by 314 earlier port frames; rows 1..65 are port 3424's, and rows
        # 66..199 are port 3425's. (3424, 3425) is the unique adjacent-pair
        # binding. The one-row top's non-unique holder is the named limit (§J8).
        (4089, 1, 66, '8eeaad13ee80ed904df9a8d600d3972f34959d6d5a0692cdbd319003b80376f6'),
    ),
    'gp-u18-talon-ending': (
        # Capture 4099 (raw 7759): the final's mode-0x0D -> 0x0C transition (record
        # §E3), the same three-present scan as U13's 4248/4320: rows 0..63 are port
        # 3430's (f=14CC tick=0000155B mode=000D; also 3428's f=14CB and 3429's
        # f=14CC tick=00001549, identical there), rows 64..167 are port 3431's
        # (f=14CC tick=0000155F mode=000C) and rows 168..199 are port 3432's
        # (f=14CD tick=00001560 mode=000C); (3431, 3432) is the 4865-frame dump's
        # unique adjacent-pair bind, rows 64..134 are 3431's alone (so r1 <= 64)
        # and 3430 does not hold row 64 (so r1 = 64). The neighbours are clean at
        # 3429 (capture 4098) and 3432 (4100).
        (4099, 64, 168, '2fb5ec8f9b74cc393b7619ad560c9771ff75d987dc1bcdd46c2d751c87c15772'),
        # Capture 4147 (raw 7856): the next poked KO's three-present scan (the
        # report's nearest port is 3474). Rows 0..45 are port 3474's (f=14F4
        # tick=000015B3 mode=000D), rows 46..168 are port 3475's (f=14F4
        # tick=000015B6 mode=000C) and rows 169..199 are port 3476's (f=14F5
        # tick=000015B7 mode=000C); 3474 holds rows 0..45 only and 3476 rows
        # 169..199 only, so the split 46/169 is forced by the bytes and the pair
        # (3475, 3476) is the dump's unique bind. The neighbours are clean at
        # 3474 (capture 4146) and a (3476, ...) splice (4148).
        (4147, 46, 169, 'bc171866bbf274469cf629a494a4f948527c292bb9cb87d9b9906d0f8d963704'),
    ),
    'gp-u19-vertigo-ending': (
        # Capture 2478 (raw 5662): the health-bonus screen's three-present scan 166
        # capture frames after the loader overlay (record §E4; mode 0x23). Rows 0..32
        # are held by port 177 and 2020..2022 (not uniquely bound — the named limit),
        # rows 33..147 are port 2023's (f=0DFC tick=00000E56 mode=0023) and rows
        # 148..199 are port 2024's (f=0DFD tick=00000E57 mode=0023); (2023, 2024) is
        # the 4990-frame dump's unique adjacent-pair bind, and the r2 split is forced
        # by the bytes (row 148 exists only in 2024; r2 147 fails to bind). The
        # neighbours are clean at 2022 (capture 2477) and 2024 (2479).
        (2478, 33, 148, '70d994ab367ffc4c077db8b677ae23029f85bb358b43a67efe3badd180e8bc0e'),
        # Capture 4362 (raw 7863): the final's mode-0x0D -> 0x0C transition, the same
        # three-present scan as U13's 4248/4320 and U18's 4099/4147. Rows 0..52 are
        # port 3577's (f=1516 tick=000015B3 mode=000D, its only holder), rows 53..117
        # are port 3578's (f=1516 tick=000015B7 mode=000C) and rows 118..199 are port
        # 3579's (f=1517 tick=000015B8 mode=000C); (3578, 3579) is the dump's unique
        # bind and the r2 split is forced (row 118 exists only in 3579). The
        # neighbours are clean at 3577 (capture 4361) and a (3579, ...) splice (4363).
        (4362, 53, 118, '7b1d3e43b68a309a87c71f991a28f9aaf0597d789a50fe82c20deb8eaf0d66b0'),
        # Capture 4430 (raw 7961): the final's mode-0x0C replacement screen's next
        # three-present scan (no mode change here). Rows 0..11 are port 3640's
        # (f=1552 tick=0000160B mode=000C, its only holder), rows 12..145 are port
        # 3641's (f=1552 tick=0000160D mode=000C) and rows 146..199 are port 3642's
        # (f=1553 tick=0000160E mode=000C); (3641, 3642) is the dump's unique bind
        # and the r2 split is forced (row 146 exists only in 3642). The neighbours
        # are clean at 3640 (capture 4429) and a (3642, ...) splice (4431).
        (4430, 12, 146, '83ca9cdcfbc340ebe24f6331d6ebdb6871ec07704108fc1e3bf139704adefdf9'),
    ),
    'gp-u20-chaos-ending': (
        # Capture 381 (raw 2971): the round-1 mode-8 screen's three-present scan, the
        # first present bunching after the opening (record §E5). Row 0 is held by port
        # 260 (f=04FB tick=00000551 mode=0008) alone among the frames before the bind
        # (the still-scanned old screen's top row), rows 1..157 are port 263's (f=04FE
        # tick=00000552 mode=0008, its only holder over rows 5..12 and 35..157) and rows
        # 158..199 are port 264's (f=04FF tick=00000553 mode=0008); (263, 264) is the
        # 4891-frame dump's unique adjacent-pair bind and the r2 split is forced (row 158
        # exists only in 264). The neighbours are clean at port 260 (capture 380) and a
        # (264, ...) splice at 382.
        (381, 1, 158, '6ac5160d40dd5666f49650e2f9643c27dedceb5565424bb188405f306c1eb936'),
        # Capture 2350 (raw 5736): the health-bonus screen's three-present scan (mode
        # 0x23), the same class as U19's 2478. Rows 0..24 are held by the earlier ports
        # 193..196, 1039 and 1926..1931 (not uniquely bound — the named limit), rows
        # 25..139 are port 1932's (f=0E34 tick=00000E90 mode=0023) and rows 140..199 are
        # port 1933's (f=0E35 tick=00000E91 mode=0023); (1932, 1933) is the dump's unique
        # adjacent-pair bind and the r2 split is forced (row 140 exists only in 1933).
        (2350, 25, 140, '706572f06c0cdb32aa3e8efd4a52fb42da0bd30f2b4730b9b70ac28987103c68'),
        # Capture 4272 (raw 8091): the final's mode-0x0D -> 0x0C transition, the same
        # three-present scan as U13's 4248/4320, U18's 4099/4147 and U19's 4362. Rows
        # 0..18 are port 3566's (f=159F tick=00001674 mode=000D, its only holder over rows
        # 0..4 and 11..18), rows 19..90 are port 3567's (f=159F tick=00001678 mode=000C)
        # and rows 91..199 are port 3568's (f=15A0 tick=00001679 mode=000C); (3567, 3568)
        # is the dump's unique adjacent-pair bind and the r2 split is forced (row 91
        # exists only in 3568).
        (4272, 19, 91, '9744b9869a3b9e4b749ddbdacba395ad527c0f3fbef5e40ff88eae19013e30be'),
    ),
    'gp-u22-diablo-ending': (
        # Capture 380 (raw 3028): round 1's mode-8 screen, three presents inside one
        # scan (§G6's class). Rows 0..10 are port 284's (f=04FB tick=00000551
        # mode=0008) alone before the bind (the still-scanned old screen's top rows),
        # rows 11..60 are port 286's (f=04FD tick=00000552 mode=0008) and rows 61..199
        # are port 287's (f=04FE tick=00000553 mode=0008); (286, 287) is the dump's
        # unique adjacent-pair bind and the r2 split is forced (row 61 exists only in
        # 287). Neighbours 379 clean at 284 and 381 a (287, 288) splice.
        (380, 11, 61, 'e9a10eb395f6b88c1d97654ecd76d736f8f6a08c9c5617867912719dc4be4ccc'),
        # Capture 1267 (raw 4009), mode 5: rows 0..24 are held by the earlier ports
        # 218..221 (mode 3, the still-black early screens) and 1026 (f=0844
        # tick=00000899 mode=0005) — not uniquely bound, the named limit — rows 25..178
        # are port 1027's (f=0845 tick=0000089A mode=0005) and rows 179..199 port
        # 1028's (f=0846 tick=0000089B); (1027, 1028) unique. Neighbours 1266 clean
        # at port 1 (the still-black early screen) and 1268 clean at 1028.
        (1267, 25, 179, '9f20c2ce81c05a414a7b6fa5d0b2766944eef64f9ba8fe06b8b74ca70e8bb2a8'),
        # Capture 4136 (raw 7998), the final's mode 0xC: rows 0..23 are port 3496's
        # (f=154D tick=000015F1 mode=000C) alone before the bind, rows 24..149 are
        # port 3497's (f=154E tick=000015F2 mode=000C) and rows 150..199 port 3498's
        # (f=154F tick=000015F3); (3497, 3498) unique. Neighbours 4135 clean at 3496
        # and 4137 clean at 3498.
        (4136, 24, 150, '221c7921fd2463f65fb8a482b6c928207aaac3bf322b42e6e825abec17483240'),
        # Capture 4228 (raw 8129), the same final: rows 0..41 are port 3579's
        # (f=159E tick=00001662 mode=000C) alone before the bind, rows 42..144 are
        # port 3580's (f=159F tick=00001663) and rows 145..199 port 3581's (f=15A0
        # tick=00001664); (3580, 3581) unique. Neighbours 4227 clean at 3579 and
        # 4229 clean at 3581.
        (4228, 42, 145, '13f698a7b1bebc862720e22041862f02b4c5f315540e3e86911a52f7f7370000'),
        # Capture 5027 (raw 8987), the ending's mode-0x15 fade: rows 0..118 are port
        # 4294's (f=1866 tick=00001941 mode=0015) alone before the bind, rows 119..121
        # are port 4295's (f=1867 tick=00001942) and rows 122..199 port 4296's
        # (f=1868 tick=00001943); (4295, 4296) unique. Neighbours 5026 clean at 4294
        # and 5028 clean at 4296.
        (5027, 119, 122, '4fdc50e09875fafc1716dd46e40518e75cea280a5770b81daca0e8103c542927'),
        # Capture 5033 (raw 8994), the same fade: rows 0..129 are port 4300's
        # (f=186C tick=00001947 mode=0015) alone before the bind, rows 130..197 are
        # port 4302's (f=186E tick=00001949) and rows 198..199 port 4303's (f=186F
        # tick=0000194A); (4302, 4303) unique. Port 4301 is nowhere on the frame (its
        # whole present fell between two scans). Neighbours 5032 clean at 4300 and
        # 5034 clean at 4302.
        (5033, 130, 198, '9a5cd9bd7b6dc2c14642eab62211a5d64a5928dec2abb29b2002725ee333c17e'),
    ),
}


# Per-scenario named per-frame glyph-overlay allowances (record §G7): the one
# capture frame whose loader presentation landed on a screen the replay only
# presents later. The loader blits its `- LOADING -` text (string 489,
# 0x1B3EA/0x1B3EF) straight into the aperture, over whatever the screen holds,
# and the read that follows stalls the loop. At the mode-0x12 entry the first
# read of s16snd2 (index 6) is a *music* request (voice 0x32 case 1, song handle
# 0x03021BFA): the original's sequencer reads the song through the resource
# layer (0x1B544, draw arm) in the entry's window, so the text lands over the
# entry's screen; the port's music path is host-owned (platform/audio/ never
# resolves), so the replay's first s16snd2 resolve is the *sample* queue of
# voice 0x3A (0x03022554, also index 6) at f=0xC0E and the same text lands one
# screen later. The capture frame is therefore `- LOADING -` over the
# mode-0x12 screen — a composition of two frames the port itself dumped (the
# presentation frame and the screen), which the two-frame model cannot express.
# Like BANDS_ALLOWED/SCAN3_ALLOWED this is never a class and never an extension
# of the two-frame model: only the named capture frames, each only with its own
# byte recipe. An entry is (capture j, T1, T2, S, glyph_sha): the frame is
# explained ONLY if
#   1. sha256 of T1's bytes on the mask {i: T1[i] != T2[i]} equals glyph_sha
#      (T1 = the port's `- LOADING` frame with the glyphs visible; T2 = the same
#      indices under the stale black palette, so the mask is exactly the
#      glyphs' pixels and the mask's shape is part of the fingerprint), and
#   2. every pixel of the capture equals T1's on the mask and S's (the screen
#      frame) off it — the whole frame, not a box.
# So a mutation of T1's overlay bytes, of the mask's shape (via T2) or of any
# S pixel un-explains the capture frame (OverlayAllowanceTests). Named limits
# (as SCAN3_ALLOWED's top segment): T2 is only the mask's indicator — its bytes
# inside the mask bind nothing beyond the `!=` relation, and T1's bytes off the
# mask are not claimed; the glyph sha binds T1's byte sequence at the sorted
# mask indices, not the index set itself, so two different index sets carrying
# identical T1 bytes could coincide. A named frame the two-frame model explains
# now is reported as a stale name; a named frame that is not its named bytes
# FAILS.
OVERLAY_ALLOWED = {
    'gp-u9-win': (
        # Capture 2209 (raw 4953) at the mode-0x17->0x12 entry: the loader's
        # text (port 1776's 166 glyph px, rows 192..197 x 0..85) over the
        # mode-0x12 screen (port 1777, the frame at f=0xBA1 = the entry).
        # Port 1775 is the same index image under the stale black palette (the
        # mask's source). The capture shows the presentation one screen later
        # than the replay's own (the replay's is over the black screen,
        # capture 2208 = port 1776); the port's own later presentation of the
        # same text is dump frame 1805 (f=0xC0E). §G6's "held by no frame"
        # stands for the whole strip; this names the composition.
        (2209, 1776, 1775, 1777,
         'de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced'),
    ),
    'gp-u10-ending': (
        # Capture 2062 (raw 4954): the same presentation at U10's entry
        # (port 1685 over 1686, mask source 1684 — the palette pair is swapped
        # against U9's). The glyph fingerprint is the same bytes (the same
        # string at the same place).
        (2062, 1685, 1684, 1686,
         'de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced'),
    ),
    'gp-u13-ending2': (
        # Capture 2188 (raw 5047): U13's own mode-0x17 -> 0x12 loader
        # presentation, with the 166 glyph pixels at rows 192..197, x 0..85
        # from port 1799 over port 1800's mode-0x12 screen; port 1798 supplies
        # the same indices under the stale black palette. The glyph bytes match
        # the same loader string, but the capture and all three port frames are
        # independently measured for U13.
        (2188, 1799, 1798, 1800,
         'de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced'),
    ),
    'gp-u16-twop2': (
        # Capture 8066 (raw 11502): port 6815's 166 loader glyph pixels over
        # port 6816's screen; port 6814 is the mask source. Independently
        # measured for U16, with the same glyph-byte fingerprint (§J8).
        (8066, 6815, 6814, 6816,
         'de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced'),
    ),
    'gp-u18-talon-ending': (
        # Capture 2227 (raw 4988): U18's own loader presentation at the
        # mode-0x17 -> 0x12 entry, the same §G7 composition as U9/U10/U13: port
        # 1810's (f=0BBC tick=00000C12 mode=0017) 166 glyph pixels at rows
        # 192..197, x 0..85 over port 1811's (f=0BBC tick=00000C18 mode=0012)
        # screen; port 1809 (f=0BBC tick=00000C12 mode=0017) is the same indices
        # under the stale black palette (the mask's source). The capture shows
        # the composition over the new screen, where the replay's own
        # presentation 1810 is over the stale one: no clean match or two-frame
        # splice expresses it (2228 is clean at port 1811). The glyph
        # fingerprint is the loader string's (de1c1bb2..), measured on this dump.
        (2227, 1810, 1809, 1811,
         'de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced'),
    ),
    'gp-u19-vertigo-ending': (
        # Capture 2312 (raw 5021): VERTIGO's own loader presentation at the
        # mode-0x17 -> 0x12 entry, the same §G7 composition as U9/U10/U13/U18: port
        # 1872's (f=0BDA tick=00000C2B mode=0017) 166 glyph pixels at rows 192..197,
        # x 0..85 over port 1873's (f=0BDA tick=00000C31 mode=0012) screen; port 1871
        # (f=0BDA tick=00000C2B mode=0017) is the same indices under the stale black
        # palette (the mask's source). The capture shows the composition over the new
        # screen, where the replay's own presentation 1872 is over the stale one: no
        # clean match or two-frame splice expresses it (2311 is clean at 1872, 2313
        # clean at 1873). The glyph fingerprint is the loader string's (de1c1bb2..),
        # measured on this dump; T1 1872/T2 1871 are the only pair at mask 166 that
        # reproduces the frame's pixels.
        (2312, 1872, 1871, 1873,
         'de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced'),
    ),
    'gp-u20-chaos-ending': (
        # Capture 2180 (raw 5094): CHAOS's own loader presentation at the mode-0x17 ->
        # 0x12 entry, the same §G7 composition as U9/U10/U13/U18/U19: port 1781's
        # (f=0C12 tick=00000C65 mode=0017) 166 glyph pixels at rows 192..197, x 0..85
        # over port 1782's (f=0C12 tick=00000C6B mode=0012) screen; port 1780 (f=0C12
        # tick=00000C65 mode=0017, the same tick's second present) is the same indices
        # under the stale black palette (the mask's source). The capture shows the
        # composition over the new screen, where the replay's own presentation 1781 is
        # over the stale one: no clean match or two-frame splice expresses it (2179 is
        # clean at 1781 and 2181 clean at 1782). The glyph fingerprint is the loader
        # string's (de1c1bb2..), measured on this dump; T1 1781/T2 1780 are the only
        # pair at mask 166 that reproduces the frame's pixels.
        (2180, 1781, 1780, 1782,
         'de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced'),
    ),
    'gp-u21-armadon-ending': (
        # Capture 2217 (raw 5125): ARMADON's own loader presentation at the mode-0x17 ->
        # 0x12 entry, the same §G7 composition as U9/U10/U13/U18/U19/U20: port 1799's
        # (f=0C1C tick=00000C6D mode=0017) 166 glyph pixels at rows 192..197, x 0..85
        # over port 1800's (f=0C1C tick=00000C73 mode=0012) screen; port 1798 (f=0C1C
        # tick=00000C6D mode=0017) is the same indices under the stale black palette
        # (the mask's source). The capture shows the composition over the new screen,
        # where the replay's own presentation 1799 is over the stale one: no clean
        # match or two-frame splice expresses it (2216 is clean at port 29, the
        # still-black early screen, and 2218 clean at 1800). The glyph fingerprint is
        # the loader string's (de1c1bb2..), measured on this dump. Named limit: the
        # later mode-0x12 pair (1802, 1801) carries the same glyph bytes at the same
        # 166 indices, so T1's identity is not unique (T2 is only the mask's
        # indicator); the capture's pixels are the same under either.
        (2217, 1799, 1798, 1800,
         'de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced'),
    ),
    'gp-u22-diablo-ending': (
        # Capture 2191 (raw 5152): DIABLO's own loader presentation at the mode-0x17 ->
        # 0x12 entry, the same §G7 composition as U9/U10/U13/U18/U19/U20/U21: port
        # 1822's (f=0C12 tick=00000C66 mode=0017) 166 glyph pixels at rows 192..197,
        # x 0..85 over port 1823's (f=0C12 tick=00000C6C mode=0012) screen; port 1821
        # (f=0C12 tick=00000C66 mode=0017) is the same indices under the stale black
        # palette (the mask's source). The capture shows the composition over the new
        # screen, where the replay's own presentation 1822 is over the stale one: no
        # clean match or two-frame splice expresses it (2190 is clean at port 29, the
        # still-black early screen, and 2192 clean at 1823). The glyph fingerprint is
        # the loader string's (de1c1bb2..), measured on this dump. Named limit: the
        # later mode-0x12 pair (1825, 1824) carries the same glyph bytes at the same
        # 166 indices, so T1's identity is not unique (T2 is only the mask's
        # indicator); the capture's pixels are the same under either.
        (2191, 1822, 1821, 1823,
         'de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced'),
    ),
}


def band_pair(c, port, top_row, band_hi):
    """The unique adjacent port pair P with c's rows [0, top_row) == port[P]'s and
    c's rows [band_hi, FRAME_H) == port[P+1]'s; None if there is not exactly one."""
    R = tc.ROW
    if len(port) < 2:
        return None
    lo, hi = c[:top_row * R], c[band_hi * R:]
    hits = []
    prev = port[0]
    for m in range(1, len(port)):
        cur = port[m]
        if prev[:top_row * R] == lo and cur[band_hi * R:] == hi:
            hits.append(m - 1)
        prev = cur
    return hits[0] if len(hits) == 1 else None


def band_explain(c, port, entry):
    """(P, None) when capture frame c is byte-identical to the named entry's recipe
    against this port dump; (None, why) otherwise."""
    _j, top_row, band_hi, band_sha = entry
    got = hashlib.sha256(c[top_row * tc.ROW:band_hi * tc.ROW]).hexdigest()
    if got != band_sha:
        return None, 'its band rows %d..%d are %s.., not the named %s..' % (
            top_row, band_hi - 1, got[:12], band_sha[:12])
    P = band_pair(c, port, top_row, band_hi)
    if P is None:
        return None, 'no unique adjacent port pair binds its rows [0,%d)/[%d,200)' % (
            top_row, band_hi)
    return P, None


def scan3_bind(c, port, r1, r2):
    """The unique adjacent port pair (B, B+1) with c's rows [r1, r2) == port[B]'s
    and c's rows [r2, FRAME_H) == port[B+1]'s; None if there is not exactly one.
    The [r1, r2) test is vacuous when r1 == r2 (the U9 two-segment shape), so the
    bind then asks for exactly one pair whose B+1 holds the whole [r2, FRAME_H)."""
    R = tc.ROW
    if len(port) < 2:
        return None
    mid, bot = c[r1 * R:r2 * R], c[r2 * R:]
    hits = []
    for m in range(1, len(port)):
        if port[m - 1][r1 * R:r2 * R] == mid and port[m][r2 * R:] == bot:
            hits.append(m - 1)
    return hits[0] if len(hits) == 1 else None


def scan3_top(c, port, B, r1):
    """The port frames A < B that hold c's rows [0, r1) (the still-scanned old
    screen): the earlier holders, empty when none — the top segment is not
    uniquely bound (§G6's named limit), only required to exist below B."""
    R = tc.ROW
    lo = c[:r1 * R]
    return [a for a in range(B) if port[a][:r1 * R] == lo]


def scan3_explain(c, port, entry):
    """(exhibits, None) when capture frame c is byte-identical to the named
    three-frame recipe against this port dump; (None, why) otherwise. exhibits is
    the earlier top holders plus the bound pair (B, B+1); B is dropped when the
    middle range is vacuous (r1 == r2) so a frame with no bound pixel is never
    exhibited."""
    _j, r1, r2, top_sha = entry
    got = hashlib.sha256(c[:r1 * tc.ROW]).hexdigest()
    if got != top_sha:
        return None, 'its top rows 0..%d are %s.., not the named %s..' % (
            r1 - 1, got[:12], top_sha[:12])
    B = scan3_bind(c, port, r1, r2)
    if B is None:
        return None, 'no unique adjacent port pair binds its rows [%d,%d) and [%d,200)' % (
            r1, r2, r2)
    tops = scan3_top(c, port, B, r1)
    if not tops:
        return None, 'no port frame before %d holds its named top rows' % B
    ex = sorted(set(tops) | ({B, B + 1} if r1 < r2 else {B + 1}))
    return ex, None


def overlay_mask(t1, t2):
    """The pixel indices i where T1 differs from T2 (§G7: the loader's glyph
    pixels, the presentation frame against the same indices under the stale
    black palette)."""
    return [i for i in range(tc.FRAME_W * tc.FRAME_H)
            if t1[3 * i:3 * i + 3] != t2[3 * i:3 * i + 3]]


def overlay_explain(c, port, entry):
    """(info, None) when capture frame c is byte-identical to the named entry's
    recipe against this port dump; (None, why) otherwise. info is the mask's
    size; the caller reports the frames. Every pixel is checked: T1's off the
    mask and T2's values bind nothing (T2 is the mask's indicator only)."""
    _j, t1_i, t2_i, s_i, glyph_sha = entry
    t1, t2, s = port[t1_i], port[t2_i], port[s_i]
    mask = overlay_mask(t1, t2)
    got = hashlib.sha256(b''.join(t1[3 * i:3 * i + 3] for i in mask)).hexdigest()
    if got != glyph_sha:
        return None, 'its glyph pixels are %s.., not the named %s..' % (got[:12], glyph_sha[:12])
    mset = set(mask)
    for i in range(tc.FRAME_W * tc.FRAME_H):
        if i in mset:
            want, src = t1, t1_i
        else:
            want, src = s, s_i
        if c[3 * i:3 * i + 3] != want[3 * i:3 * i + 3]:
            return None, ('its pixel row %d x %d is %s, not port %d\'s %s'
                          % (i // tc.FRAME_W, i % tc.FRAME_W, c[3 * i:3 * i + 3].hex(),
                             src, want[3 * i:3 * i + 3].hex()))
    return len(mask), None


def expand_ipx(data):
    """64 000 indices + a 768-byte DAC -> RGB24, as fe_write_frame (rgb = dac[idx])."""
    if len(data) != IPX_BYTES:
        raise ValueError('ipx frame is %d bytes, expected %d' % (len(data), IPX_BYTES))
    idx, dac = data[:64000], data[64000:]
    rgb = bytearray(tc.FRAME_BYTES)
    for c in range(3):
        rgb[c::3] = idx.translate(bytes(dac[3 * i + c] for i in range(256)))
    return bytes(rgb)


def _paths(d, pattern):
    out, i = [], 0
    while os.path.exists(os.path.join(d, pattern % i)):
        out.append(os.path.join(d, pattern % i))
        i += 1
    return out


def load_capture_frame(path):
    with gzip.open(path) as f:
        data = f.read()
    if len(data) != tc.FRAME_BYTES:
        raise ValueError('%s is %d bytes, expected %d' % (path, len(data), tc.FRAME_BYTES))
    return data


def load_port_frame(path):
    with open(path, 'rb') as f:
        return expand_ipx(f.read())


class Lazy:
    """A read-only frame sequence loaded on demand with a small LRU cache;
    title_compare.explain indexes it like a list."""

    def __init__(self, paths, loader, cache=160):
        self.paths, self.loader, self.cache = paths, loader, cache
        self.lru = collections.OrderedDict()

    def __len__(self):
        return len(self.paths)

    def __getitem__(self, i):
        if i < 0 or i >= len(self.paths):
            raise IndexError(i)
        if i in self.lru:
            self.lru.move_to_end(i)
            return self.lru[i]
        data = self.loader(self.paths[i])
        self.lru[i] = data
        if len(self.lru) > self.cache:
            self.lru.popitem(last=False)
        return data


class View:
    def __init__(self, seq, lo, hi):
        self.seq, self.lo, self.hi = seq, lo, hi

    def __len__(self):
        return self.hi - self.lo

    def __getitem__(self, i):
        return self.seq[self.lo + i]


def shift(kind, data, lo):
    if kind == 'clean':
        return kind, data + lo
    if kind == 'splice':
        return kind, [(N + lo, a, b) for N, a, b in data]
    if kind == 'transition':
        return kind, [(N + lo, r, a, b) for N, r, a, b in data]
    return kind, data


def exhibited(kind, data):
    s = set()
    if kind == 'clean':
        s.add(data)
    elif kind == 'splice':
        for N, lo, hi in data:
            if hi > 0:
                s.add(N)
            if lo < tc.FRAME_BYTES:
                s.add(N + 1)
    elif kind == 'transition':
        for N, _r, _a, _b in data:
            s.update((N, N + 1))
    elif kind == 'band':
        s.update((data, data + 1))     # the named band splice binds port P above and P+1 below
    elif kind == 'scan3':
        s.update(data)                 # the named scan-out binds the earlier top holder(s) and the pair (B, B+1)
    elif kind == 'overlay':
        s.update(data)                 # the named overlay binds T1's mask bytes and S's off-mask bytes
    return s


def classify(c, port, rows, p):
    """(kind, data) for capture frame c, the window around p first."""
    ch = tc.row_hashes(c)
    n = len(port)
    lo, hi = max(0, p - BACK), min(n, p + AHEAD)
    if hi - lo >= 1:
        kind, data = tc.explain(c, ch, View(port, lo, hi), rows[lo:hi], hi - lo)
        if kind != 'unexplained':
            return shift(kind, data, lo)
    return tc.explain(c, ch, port, rows, n)


BLACK_ROW = hashlib.md5(bytes(tc.ROW)).digest()


def start_check(name, start, raws, max_start, min_first, out=print):
    """The window START is ratcheted too (record §I item 8): the start is where the
    capture first shows the port's first frame, so a port that regressed to an earlier
    screen that recurs later in the capture would slide it forward, past the frame that
    set N, and go green. It must not be later than the pinned start, and it must lie
    below N (a window at or beyond N claims nothing)."""
    rc = 0
    if max_start is None:
        out('gp_compare: %s: frames: FAIL: the window-start pin is not set (GP_IDLE_LOSS_MAX_START)' % name)
        rc = 1
    elif start > max_start:
        out('gp_compare: %s: frames: FAIL: window starts at capture %d (raw %d) > pinned start %d'
            % (name, start, raws[start], max_start))
        rc = 1
    elif start < max_start:
        out('gp_compare: %s: frames: window start %d < pinned start %d (improved: lower the pin)'
            % (name, start, max_start))
    if min_first is not None and start >= min_first:
        out('gp_compare: %s: frames: FAIL: window start %d >= ratchet N %d: the window claims nothing below N'
            % (name, start, min_first))
        rc = 1
    return rc


def reach_check(name, reach, min_reach, out=print):
    """The window REACH is ratcheted (sub-project A §A1): `reach` is the highest port frame
    any classified capture frame exhibits. Both the window and the coverage count are
    computed up to it, so a port that renders fewer screens stops at a lower reach and
    passes; it must be at least the pinned reach. A larger reach is an improvement (raise
    the pin). `min_reach is None` fails (a gp scenario without the pin is unguarded)."""
    rc = 0
    if min_reach is None:
        out('gp_compare: %s: frames: FAIL: the window-reach pin is not set (GP_*_MIN_REACH)' % name)
        rc = 1
    elif reach < min_reach:
        out('gp_compare: %s: frames: FAIL: window reach port %d < pinned reach %d '
            '(the port exhibited fewer port frames)' % (name, reach, min_reach))
        rc = 1
    elif reach > min_reach:
        out('gp_compare: %s: frames: window reach port %d > pinned reach %d '
            '(improved: raise the pin)' % (name, reach, min_reach))
    return rc


def frame_claim(name, cap, raws, port, rows, min_first, report, out=print, max_start=None,
                bands=None, scan3=None, overlay=None, max_unexplained=REPORT_MAX):
    """Returns (rc, first_unexplained or None, next_after_last_classified, reach). In report
    mode no ratchet verdict is printed and no pin is checked (rc stays 1 only for
    "window empty"; main() turns every report-mode rc into 0). `max_unexplained`
    caps the report-mode unexplained list (0 lifts the cap; the enforced run
    always stops at the first). `bands`, `scan3` and `overlay` are the scenario's
    named per-frame allowances (BANDS_ALLOWED, SCAN3_ALLOWED, OVERLAY_ALLOWED); a
    named frame that is not its named bytes prints its FAIL in both modes and
    fails the enforced run. `reach` is `p`, the highest exhibited port frame, the
    window's end (sub-project A §A1)."""
    bands = {} if bands is None else {b[0]: b for b in bands}
    scan3 = {} if scan3 is None else {s[0]: s for s in scan3}
    overlay = {} if overlay is None else {o[0]: o for o in overlay}
    start = None
    head = min(2, len(port))
    for j in range(len(cap)):
        c = cap[j]
        if not any(c):
            continue
        kind, data = tc.explain(c, tc.row_hashes(c), View(port, 0, head), rows[:head], head)
        if 0 in exhibited(kind, data):          # exact: exhibiting port 0 needs only frames 0 and 1
            start = j
            break
    if start is None:
        out("gp_compare: %s: frames: window empty: no capture frame exhibits the port's first frame" % name)
        return 1, None, 0, 0
    p, counts, black, unexpl, seen = 0, collections.Counter(), 0, [], set()
    allow_bad = 0
    j = start
    while j < len(cap):
        c = cap[j]
        if not any(c):
            black += 1
            j += 1
            continue
        kind, data = classify(c, port, rows, p)
        if j in bands:
            if kind != 'unexplained':
                out('gp_compare: %s: frames: note: capture %d (raw %d) is named in the '
                    'allowance but %s now; retire the name' % (name, j, raws[j], kind))
            else:
                P, why = band_explain(c, port, bands[j])
                if P is None:
                    out('gp_compare: %s: frames: FAIL: allowed capture %d (raw %d) is not '
                        'its named band splice: %s' % (name, j, raws[j], why))
                    allow_bad = 1
                else:
                    out('gp_compare: %s: frames: captured frame %d (raw %d) allowed by name '
                        'as a band splice: port %d/%d rows 0..%d/%d..199, band rows %d..%d '
                        'the named bytes'
                        % (name, j, raws[j], P, P + 1, bands[j][1] - 1, bands[j][2],
                           bands[j][1], bands[j][2] - 1))
                    kind, data = 'band', P
        if kind == 'unexplained' and j in scan3:
            ex, why = scan3_explain(c, port, scan3[j])
            if ex is None:
                out('gp_compare: %s: frames: FAIL: allowed capture %d (raw %d) is not '
                    'its named three-frame scan-out: %s' % (name, j, raws[j], why))
                allow_bad = 1
            else:
                _j, r1, r2, _sha = scan3[j]
                B = scan3_bind(c, port, r1, r2)
                tops = ' '.join(str(a) for a in ex if a < B)
                mid = '' if r1 == r2 else 'port %d rows %d..%d, ' % (B, r1, r2 - 1)
                out('gp_compare: %s: frames: captured frame %d (raw %d) allowed by name '
                    'as a three-frame scan-out: top rows 0..%d held by port %s, %s'
                    'port %d rows %d..199 the named bytes'
                    % (name, j, raws[j], r1 - 1, tops, mid, B + 1, r2))
                kind, data = 'scan3', ex
        elif j in scan3 and kind != 'unexplained':
            out('gp_compare: %s: frames: note: capture %d (raw %d) is named in the '
                'scan-out allowance but %s now; retire the name' % (name, j, raws[j], kind))
        if kind == 'unexplained' and j in overlay:
            info, why = overlay_explain(c, port, overlay[j])
            if info is None:
                out('gp_compare: %s: frames: FAIL: allowed capture %d (raw %d) is not '
                    'its named glyph overlay: %s' % (name, j, raws[j], why))
                allow_bad = 1
            else:
                _j, t1_i, t2_i, s_i, _sha = overlay[j]
                out('gp_compare: %s: frames: captured frame %d (raw %d) allowed by name '
                    'as a glyph overlay: %d px of port %d over port %d (the mask from '
                    'port %d, the named glyph bytes)'
                    % (name, j, raws[j], info, t1_i, s_i, t2_i))
                kind, data = 'overlay', (t1_i, s_i)
        elif j in overlay and kind != 'unexplained':
            out('gp_compare: %s: frames: note: capture %d (raw %d) is named in the '
                'overlay allowance but %s now; retire the name' % (name, j, raws[j], kind))
        counts[kind] += 1
        if kind == 'unexplained':
            unexpl.append(j)
            if not report or (max_unexplained and len(unexpl) >= max_unexplained):
                break
        else:
            ex = exhibited(kind, data)
            seen |= ex
            if ex:
                p = max(p, max(ex))              # the port only moves forward in time
        j += 1
    out('gp_compare: %s: frames: window from capture %d (raw %d); %d classified: %d clean, %d splice, '
        '%d transition, %d band-allowed, %d scan3-allowed, %d overlay-allowed, %d unexplained, %d all-black'
        % (name, start, raws[start], sum(counts.values()), counts['clean'], counts['splice'],
           counts['transition'], counts['band'], counts['scan3'], counts['overlay'],
           counts['unexplained'], black))
    for k, u in enumerate(unexpl):
        cu = cap[u]
        m = nearest(cu, rows)
        box = diff_box(cu, port[m])
        out('gp_compare: %s: frames: %s capture %d (raw %d): nearest port %d, rows %d..%d, x %d..%d (%d px)'
            % ((name, 'FIRST UNEXPLAINED' if k == 0 else 'UNEXPLAINED', u, raws[u], m) + box))
    first = unexpl[0] if unexpl else None
    # Coverage, reported and NOT ratcheted (record §I item 9, a named gap): the port's
    # non-black frames up to the last one the capture exhibits that no classified capture
    # frame exhibits. Order is not claimed either.
    missing = [m for m in range(min(p + 1, len(rows))) if m not in seen and any(h != BLACK_ROW for h in rows[m])]
    out('gp_compare: %s: frames: coverage (reported, not ratcheted): %d non-black port frame(s) up to port %d '
        'not exhibited by any classified capture frame%s'
        % (name, len(missing), p, '' if not missing else ': %s%s' % (missing[:20], ' ...' if len(missing) > 20 else '')))
    if report:
        if first is None:
            out('gp_compare: %s: frames: 0 unexplained through %d' % (name, len(cap) - 1))
        if allow_bad:
            out('gp_compare: %s: frames: FAIL: an allowance named its frame but not its bytes' % name)
        return 0, first, j, p
    rc = max(ratchet(name, 'frames', first, len(cap), min_first, out),
             start_check(name, start, raws, max_start, min_first, out), allow_bad)
    return rc, first, j, p


def nearest(c, rows):
    ch = tc.row_hashes(c)
    best, best_m = -1, 0
    for m, ph in enumerate(rows):
        a, b = tc.row_common(ch, ph)
        if min(a + b, tc.FRAME_H) > best:
            best, best_m = min(a + b, tc.FRAME_H), m
    return best_m


def diff_box(c, q):
    R = tc.ROW
    rs = [r for r in range(tc.FRAME_H) if c[r * R:(r + 1) * R] != q[r * R:(r + 1) * R]]
    if not rs:
        return 0, 0, 0, 0, 0
    xs = [x for r in rs for x in range(tc.FRAME_W) if c[r * R + 3 * x:r * R + 3 * x + 3] != q[r * R + 3 * x:r * R + 3 * x + 3]]
    return rs[0], rs[-1], min(xs), max(xs), len(xs)


def ratchet(name, what, first, end, n, out=print, noun='unexplained'):
    """first unexplained (None: none up to `end`) against the pinned N."""
    if n is None:
        out('gp_compare: %s: %s: FAIL: the ratchet N is not pinned (U4 Task 5)' % (name, what))
        return 1
    if first is None:
        if n > end:
            out('gp_compare: %s: %s: FAIL: N %d > end %d: N is unreachable' % (name, what, n, end))
            return 1
        out('gp_compare: %s: %s: 0 %s through %d; ratchet N %d ok%s'
            % (name, what, noun, end - 1, n, '' if n == end else ' (every item is explained: N = %d is the exact pin)' % end))
        return 0
    if first < n:
        out('gp_compare: %s: %s: FAIL: first %s %d < ratchet N %d' % (name, what, noun, first, n))
        return 1
    out('gp_compare: %s: %s: first %s %d, ratchet N %d ok%s'
        % (name, what, noun, first, n, '' if first == n else ' (improved: raise N)'))
    return 0


DATA_BASE_PORT = 0x80000    # the port's data object base (mem.h DATA_BASE), record §H item 2


def normalised(cap, port, fs, base):
    """Fields that differ between the capture and the port by construction (record
    §H items 1-2), compared after the conversion: t508 (the capture reads it in the
    spin, the port after the releasing tick: port = capture + 1) and ent (a pointer:
    capture linear address - the capture's data base + the port's). Returns
    {field: (compared, differing, first differing f)}; reported, never ratcheted."""
    out = {'t508': [0, 0, None], 'ent': [0, 0, None]}
    for f in fs:
        if f not in cap:
            continue
        c, p = cap[f], port[f]
        want = {'t508': (c['t508'] + 1) & 0xFFFFFFFF}
        if base is not None:
            want['ent'] = ((c['ent'] - base + DATA_BASE_PORT) & 0xFFFFFFFF) if c['ent'] else 0
        for n, w in want.items():
            out[n][0] += 1
            if p[n] != w:
                out[n][1] += 1
                if out[n][2] is None:
                    out[n][2] = f
    return {n: tuple(v) for n, v in out.items()}


def trace_claim(name, cap_lines, port_lines, min_first, out=print, report=False, fields=None, what='trace'):
    """The trace claim over `fields` (gp_session.TRACE_FIELDS by default). With
    what='moves' (gp_session.MOVE_FIELDS, record gameplay-u6 §U6.11) the same
    comparison is labelled 'moves' and the tick and normalised lines are left to
    the trace claim."""
    fields = gs.TRACE_FIELDS if fields is None else fields
    cap = gs.snapshots(cap_lines)
    port = {}
    for l in port_lines:
        r = gs.parse(l)
        if r and r['kind'] == 'T':
            port.setdefault(r['f'], r)
    fs = sorted(port)
    compared = skipped = 0
    first = field = tick_first = None
    for f in fs:
        if f not in cap:
            skipped += 1
            continue
        compared += 1
        if tick_first is None and cap[f]['tick'] != port[f]['tick']:
            tick_first = f
        for n in fields:
            if cap[f][n] != port[f][n]:
                first, field = f, n
                break
        if first is not None:
            break
    if what != 'trace':
        out('gp_compare: %s: %s: %d frames compared%s (f %s..) over %s, %d without a capture snapshot'
            % (name, what, compared, '' if first is None else ' up to the first difference',
               ('%X' % fs[0]) if fs else '-', ' '.join(fields), skipped))
    else:
        out('gp_compare: %s: trace: %d frames compared%s (f %s..), %d without a capture snapshot; first tick '
            'difference %s (reported, not ratcheted)'
            % (name, compared, '' if first is None else ' up to the first difference', ('%X' % fs[0]) if fs else '-', skipped,
               'none' if tick_first is None else 'f=%X' % tick_first))
    if first is not None:
        out('gp_compare: %s: %s: first difference f=%X (%d) in %s: capture %X, port %X'
            % (name, what, first, first, field, cap[first][field], port[first][field]))
    if what != 'trace':
        if compared == 0:
            out('gp_compare: %s: %s: FAIL: no port T record has a capture snapshot (nothing compared)' % (name, what))
            return 1, None
        end = (fs[-1] + 1) if fs else 0
        if report:
            if first is None:
                out('gp_compare: %s: %s: 0 differing through %d' % (name, what, end - 1))
            return 0, first
        return ratchet(name, what, first, end, min_first, out, 'differing'), first
    base = None
    for l in cap_lines:
        r = gs.parse(l)
        if r and r['kind'] == 'B':
            base = r['base']
            break
    norm = normalised(cap, port, fs, base)
    out('gp_compare: %s: trace: normalised (reported, not ratcheted): %s%s'
        % (name, '; '.join('%s %d of %d differ%s' % (n, d, c, '' if d == 0 else ' (first f=%X)' % f0)
                           for n, (c, d, f0) in sorted(norm.items()) if n != 'ent' or base is not None),
           '' if base is not None else '; ent not compared (the capture has no B record)'))
    if compared == 0:
        out('gp_compare: %s: trace: FAIL: no port T record has a capture snapshot (nothing compared)' % name)
        return 1, None
    end = (fs[-1] + 1) if fs else 0
    if report:
        if first is None:
            out('gp_compare: %s: trace: 0 differing through %d' % (name, end - 1))
        return 0, first
    return ratchet(name, 'trace', first, end, min_first, out, 'differing'), first


def capture_identity(name, cap_dir, n_frames, sha, frames, out):
    """The pins (N, F, the window start) are measured on one capture: the pick time-out
    steps in 64-frame units of the absolute frame counter (record §G.24), so a re-capture
    can move the whole timeline. Given --capture-sha256, a capture whose poll.log hashes
    differently (or whose frame count differs) fails, never skips. Returns True when it matches
    or no pin was asked for."""
    if sha is None:
        return True
    if sha == '':
        out('gp_compare: %s: capture: FAIL: the capture identity (poll.log sha256) is not pinned' % name)
        return False
    with open(os.path.join(cap_dir, 'poll.log'), 'rb') as f:
        got = hashlib.sha256(f.read()).hexdigest()
    want_frames = _int_or_none(frames)
    if got != sha or (want_frames is not None and n_frames != want_frames):
        out('gp_compare: %s: capture: FAIL: poll.log sha256 %s (%d frames) != the pinned %s (%s frames): '
            'a re-capture invalidates the pinned N, F and window start; re-measure them from the new '
            'capture (record §G.24) before pinning' % (name, got, n_frames, sha,
                                                       '?' if want_frames is None else want_frames))
        return False
    out('gp_compare: %s: capture: poll.log sha256 %s..%s, %d frames: matches the pin'
        % (name, got[:8], got[-6:], n_frames))
    return True


def has_dev_fields(cap_lines):
    """The capture records gp_session.DEV_FIELDS (record §H6): --dev-words'
    precondition. An older poll.log's S lines lack dev1/dev2, so a claim over them
    would compare nothing; the flag's run FAILs instead."""
    snaps = gs.snapshots(cap_lines)
    return bool(snaps) and all(n in next(iter(snaps.values())) for n in gs.DEV_FIELDS)


def _int_or_none(s):
    return None if s in (None, '') else int(s, 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--scenario', required=True)
    ap.add_argument('--capture', required=True)
    ap.add_argument('--port', required=True)
    ap.add_argument('--min-first', default=None)
    ap.add_argument('--trace-min-first', default=None)
    ap.add_argument('--max-start', default=None)
    ap.add_argument('--min-reach', default=None,
                    help='the pinned highest port frame any classified capture frame exhibits '
                         '(sub-project A §A1): a port that renders fewer screens fails')
    ap.add_argument('--capture-sha256', default=None,
                    help="the pinned sha256 of the capture's poll.log; with it a different capture fails")
    ap.add_argument('--capture-frames', default=None,
                    help="the pinned number of frame_*.raw.gz in the capture (given with --capture-sha256)")
    ap.add_argument('--moves-min-first', default=None,
                    help='the moves claim over gp_session.MOVE_FIELDS (record gameplay-u6 §U6.11); '
                         'report mode runs it whenever the capture records those fields')
    ap.add_argument('--dev-words', action='store_true',
                    help='add gp_session.DEV_FIELDS (dev1/dev2, the joystick device words; record §H6) '
                         'to the trace claim. Only gp-u17-device2 passes this: every older capture\'s '
                         'poll.log lacks the fields, and their pins stay (the parser treats an absent '
                         'field as absent)')
    ap.add_argument('--report', action='store_true')
    ap.add_argument('--max-unexplained', type=int, default=REPORT_MAX,
                    help='report mode: stop after this many unexplained frames (0 = no cap; default 5)')
    a = ap.parse_args()
    name = a.scenario
    if a.report:
        print('gp_compare: %s: report only: no ratchet applied, exit 0' % name)
    if not os.path.isdir(a.capture):
        print('gp_compare: no capture at %s (skipped)' % a.capture)
        return 0
    if not os.path.isfile(os.path.join(a.port, 'trace.txt')):
        print('gp_compare: no port dump at %s (trace.txt missing)' % a.port)
        return 0 if a.report else 1
    if not os.path.isfile(os.path.join(a.capture, 'poll.log')):
        print('gp_compare: %s: capture %s has no poll.log' % (name, a.capture))
        return 0 if a.report else 1
    cpaths = _paths(a.capture, 'frame_%05d.raw.gz')
    ppaths = _paths(a.port, 'frame_%05d.ipx')
    if not a.report and not capture_identity(name, a.capture, len(cpaths), a.capture_sha256,
                                             a.capture_frames, print):
        return 1
    if not cpaths or not ppaths:
        print('gp_compare: %s: no frames (capture %d, port %d)' % (name, len(cpaths), len(ppaths)))
        return 0 if a.report else 1
    cap = Lazy(cpaths, load_capture_frame)
    port = Lazy(ppaths, load_port_frame)
    rows = [tc.row_hashes(port[m]) for m in range(len(port))]
    raws = tc.raw_map(a.capture) or list(range(len(cap)))
    n_frames = None if a.report else _int_or_none(a.min_first)
    n_trace = None if a.report else _int_or_none(a.trace_min_first)
    rc1, _, _, reach = frame_claim(name, cap, raws, port, rows, n_frames, a.report, print,
                                   None if a.report else _int_or_none(a.max_start),
                                   BANDS_ALLOWED.get(name, ()), SCAN3_ALLOWED.get(name, ()),
                                   OVERLAY_ALLOWED.get(name, ()),
                                   max_unexplained=a.max_unexplained)
    rc4 = 0 if a.report else reach_check(name, reach, _int_or_none(a.min_reach), print)
    with open(os.path.join(a.capture, 'poll.log')) as f:
        cl = f.read().splitlines()
    with open(os.path.join(a.port, 'trace.txt')) as f:
        pl = f.read().splitlines()
    # The device-word claim (record §H6): dev1/dev2 join the trace fields, and a
    # capture without them is a FAIL rather than a silent narrower claim. The FAIL
    # returns before the claim: comparing a field the capture does not record
    # would raise KeyError, not report (§H6.4; tools/tests/test_gp_compare.py's CLI
    # case). Report mode goes on with the plain trace fields, its exit is 0 anyway.
    trace_fields = gs.TRACE_FIELDS
    if a.dev_words:
        if not has_dev_fields(cl):
            print('gp_compare: %s: trace: FAIL: the capture records no device-word fields (dev1/dev2)' % name)
            if not a.report:
                return 1
        else:
            trace_fields = gs.TRACE_FIELDS + gs.DEV_FIELDS
    rc2, _ = trace_claim(name, cl, pl, n_trace, print, a.report, trace_fields)
    rc3 = 0
    snaps = gs.snapshots(cl)
    has_moves = bool(snaps) and all(n in next(iter(snaps.values())) for n in gs.MOVE_FIELDS)
    if a.moves_min_first is not None and not has_moves:
        print('gp_compare: %s: moves: FAIL: the capture records no move fields (nothing compared)' % name)
        rc3 = 1
    elif a.moves_min_first is not None or (a.report and has_moves):
        rc3, _ = trace_claim(name, cl, pl, None if a.report else _int_or_none(a.moves_min_first),
                             print, a.report, gs.MOVE_FIELDS, 'moves')
    return 0 if a.report else (1 if rc1 or rc2 or rc3 or rc4 else 0)


if __name__ == '__main__':
    sys.exit(main())
