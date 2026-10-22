# Gameplay Truth II — derivation record

Plan: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii.md` (Task 2).
Spec: `docs/superpowers/specs/2026-10-20-gameplay-truth-ii-design.md` §W1.
All addresses are linear (Ghidra) addresses. §W1 is a report pass: it measures every
unexplained run of the U10 capture from 4113 to the capture's last frame, and Task 3's
decision subsection under Run 1 records the class, the fix and the pin it moved.

## §W1 The whole U10 tail (Task 2, measured at 60c99a51)

### Commands and results

```bash
make gp-replay scenario=gp-u10-ending GP_OPTIONAL=1
python3 tools/gp_compare.py --report --scenario gp-u10-ending \
  --capture data/k11-captures/gp-u10-ending --port /tmp/pr_gp_dump/gp-u10-ending \
  --max-unexplained 0
```

The replay wrote `/tmp/pr_gp_dump/gp-u10-ending` (`frames.txt`, `trace.txt`,
`frame_00000.ipx`..`frame_04663.ipx`; the last frames line is `04663 f=26E1 tick=000027CC
mode=0003`, the script's `end 9953`). The driver printed its pinned miss set (`PR_GP_DUMP
0x5D812 actor_spawn hits=65951`, `0x5D812 set_dead hits=65079`, `0x29D60
frontend_mode_1b_step hits=1`, `0x5D812 frontend_mode_1b_step hits=3`, `distinct=4
dropped=0`) and `all checks passed`. The report exited 0 (report mode applies no ratchet);
it is the uncapped run Task 1's `--max-unexplained 0` enables.

### The classification (verbatim)

The counts line, exactly as printed:

```text
gp_compare: gp-u10-ending: frames: window from capture 83 (raw 1745); 5598 classified: 3134 clean, 2169 splice, 6 transition, 0 band-allowed, 1 scan3-allowed, 1 overlay-allowed, 287 unexplained, 23 all-black
```

The whole 297-line report follows verbatim (every `UNEXPLAINED` line is in it; the two
named allowances are capture 331's scan-out and capture 2062's glyph overlay, and the
closing lines are the coverage, trace and moves reports):

```text
gp_compare: gp-u10-ending: report only: no ratchet applied, exit 0
gp_compare: gp-u10-ending: frames: captured frame 331 (raw 2831) allowed by name as a three-frame scan-out: top rows 0..1 held by port 216, port 218 rows 2..63, port 219 rows 64..199 the named bytes
gp_compare: gp-u10-ending: frames: captured frame 2062 (raw 4954) allowed by name as a glyph overlay: 166 px of port 1685 over port 1686 (the mask from port 1684, the named glyph bytes)
gp_compare: gp-u10-ending: frames: window from capture 83 (raw 1745); 5598 classified: 3134 clean, 2169 splice, 6 transition, 0 band-allowed, 1 scan3-allowed, 1 overlay-allowed, 287 unexplained, 23 all-black
gp_compare: gp-u10-ending: frames: FIRST UNEXPLAINED capture 4113 (raw 7858): nearest port 3444, rows 166..199, x 0..319 (9732 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4114 (raw 7859): nearest port 3445, rows 168..199, x 0..319 (2481 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4115 (raw 7860): nearest port 3446, rows 165..199, x 225..271 (815 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4116 (raw 7861): nearest port 3447, rows 161..199, x 222..271 (937 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4117 (raw 7863): nearest port 3448, rows 113..199, x 0..319 (24673 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4118 (raw 7864): nearest port 3449, rows 150..199, x 222..276 (1335 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4119 (raw 7865): nearest port 3450, rows 139..199, x 0..319 (8903 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4120 (raw 7866): nearest port 3451, rows 136..199, x 0..319 (2023 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4121 (raw 7867): nearest port 3452, rows 131..190, x 220..290 (1429 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4122 (raw 7868): nearest port 3453, rows 127..188, x 220..290 (1407 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4123 (raw 7869): nearest port 3453, rows 27..199, x 0..319 (47747 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4124 (raw 7870): nearest port 3454, rows 118..197, x 0..316 (5831 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4125 (raw 7871): nearest port 3455, rows 119..184, x 226..316 (1630 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4126 (raw 7872): nearest port 3456, rows 106..199, x 0..319 (2395 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4127 (raw 7873): nearest port 3457, rows 106..186, x 222..319 (1725 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4128 (raw 7874): nearest port 3458, rows 101..186, x 215..319 (1833 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4129 (raw 7875): nearest port 3459, rows 101..186, x 215..319 (1900 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4130 (raw 7876): nearest port 3459, rows 69..192, x 0..319 (9724 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4131 (raw 7877): nearest port 3460, rows 101..186, x 0..319 (6870 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4132 (raw 7878): nearest port 3461, rows 138..186, x 103..271 (3077 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4133 (raw 7879): nearest port 3462, rows 138..192, x 0..271 (1679 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 4134 (raw 7880): nearest port 3463, rows 138..186, x 221..271 (1209 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5437 (raw 13279): nearest port 0, rows 0..199, x 0..319 (63947 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5438 (raw 13284): nearest port 0, rows 0..199, x 0..319 (63952 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5439 (raw 13285): nearest port 0, rows 0..199, x 0..319 (63952 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5440 (raw 13289): nearest port 0, rows 0..199, x 0..319 (63955 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5441 (raw 13290): nearest port 0, rows 0..199, x 0..319 (63955 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5442 (raw 13294): nearest port 0, rows 0..199, x 0..319 (63963 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5443 (raw 13295): nearest port 0, rows 0..199, x 0..319 (63963 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5444 (raw 13299): nearest port 0, rows 0..199, x 0..319 (63966 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5445 (raw 13300): nearest port 0, rows 0..199, x 0..319 (63967 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5446 (raw 13304): nearest port 0, rows 0..199, x 0..319 (63969 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5447 (raw 13305): nearest port 0, rows 0..199, x 0..319 (63982 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5448 (raw 13309): nearest port 0, rows 0..199, x 0..319 (63982 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5449 (raw 13310): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5450 (raw 13314): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5451 (raw 13315): nearest port 0, rows 0..199, x 0..319 (63999 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5452 (raw 13319): nearest port 0, rows 0..199, x 0..319 (63999 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5453 (raw 13320): nearest port 0, rows 0..199, x 0..319 (64000 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5454 (raw 13324): nearest port 0, rows 0..199, x 0..319 (64000 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5455 (raw 13325): nearest port 0, rows 0..199, x 0..319 (64000 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5456 (raw 13329): nearest port 0, rows 0..199, x 0..319 (64000 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5457 (raw 13330): nearest port 0, rows 0..199, x 0..319 (64000 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5458 (raw 13334): nearest port 0, rows 0..199, x 0..319 (64000 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5459 (raw 13335): nearest port 0, rows 0..199, x 0..319 (64000 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5460 (raw 13339): nearest port 0, rows 0..199, x 0..319 (64000 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5461 (raw 13340): nearest port 0, rows 0..199, x 0..319 (63999 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5462 (raw 13344): nearest port 0, rows 0..199, x 0..319 (63999 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5463 (raw 13345): nearest port 0, rows 0..199, x 0..319 (64000 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5464 (raw 13349): nearest port 0, rows 0..199, x 0..319 (64000 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5465 (raw 13350): nearest port 0, rows 0..199, x 0..319 (63999 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5466 (raw 13354): nearest port 0, rows 0..199, x 0..319 (63999 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5467 (raw 13355): nearest port 0, rows 0..199, x 0..319 (63999 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5468 (raw 13359): nearest port 0, rows 0..199, x 0..319 (63999 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5469 (raw 13360): nearest port 0, rows 0..199, x 0..319 (63997 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5470 (raw 13364): nearest port 0, rows 0..199, x 0..319 (63974 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5471 (raw 13365): nearest port 0, rows 0..199, x 0..319 (63967 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5472 (raw 13370): nearest port 0, rows 0..199, x 0..319 (63879 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5473 (raw 13375): nearest port 0, rows 0..199, x 0..319 (63802 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5474 (raw 13380): nearest port 0, rows 0..199, x 0..319 (63846 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5475 (raw 13385): nearest port 0, rows 0..199, x 0..319 (63919 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5476 (raw 13390): nearest port 0, rows 0..199, x 0..319 (63931 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5477 (raw 13395): nearest port 0, rows 0..199, x 0..319 (63859 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5478 (raw 13396): nearest port 0, rows 0..199, x 0..319 (63859 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5479 (raw 13400): nearest port 0, rows 0..199, x 0..319 (63661 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5480 (raw 13401): nearest port 0, rows 0..199, x 0..319 (63661 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5481 (raw 13405): nearest port 0, rows 0..199, x 0..319 (63525 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5482 (raw 13406): nearest port 0, rows 0..199, x 0..319 (63524 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5483 (raw 13410): nearest port 0, rows 0..199, x 0..319 (63430 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5484 (raw 13411): nearest port 0, rows 0..199, x 0..319 (63431 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5485 (raw 13415): nearest port 0, rows 0..199, x 0..319 (63356 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5486 (raw 13416): nearest port 0, rows 0..199, x 0..319 (63356 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5487 (raw 13420): nearest port 0, rows 0..199, x 0..319 (63300 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5488 (raw 13421): nearest port 0, rows 0..199, x 0..319 (63300 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5489 (raw 13425): nearest port 0, rows 0..199, x 0..319 (63257 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5490 (raw 13426): nearest port 0, rows 0..199, x 0..319 (63245 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5491 (raw 13430): nearest port 0, rows 0..199, x 0..319 (63224 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5492 (raw 13431): nearest port 0, rows 0..199, x 0..319 (63210 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5493 (raw 13435): nearest port 0, rows 0..199, x 0..319 (63214 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5494 (raw 13436): nearest port 0, rows 0..199, x 0..319 (63201 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5495 (raw 13440): nearest port 0, rows 0..199, x 0..319 (63214 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5496 (raw 13441): nearest port 0, rows 0..199, x 0..319 (63214 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5497 (raw 13445): nearest port 0, rows 0..199, x 0..319 (63224 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5498 (raw 13446): nearest port 0, rows 0..199, x 0..319 (63235 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5499 (raw 13450): nearest port 0, rows 0..199, x 0..319 (63289 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5500 (raw 13451): nearest port 0, rows 0..199, x 0..319 (63310 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5501 (raw 13455): nearest port 0, rows 0..199, x 0..319 (63333 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5502 (raw 13456): nearest port 0, rows 0..199, x 0..319 (63368 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5503 (raw 13460): nearest port 0, rows 0..199, x 0..319 (63368 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5504 (raw 13461): nearest port 0, rows 0..199, x 0..319 (63435 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5505 (raw 13465): nearest port 0, rows 0..199, x 0..319 (63434 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5506 (raw 13466): nearest port 0, rows 0..199, x 0..319 (63491 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5507 (raw 13471): nearest port 0, rows 0..199, x 0..319 (63535 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5508 (raw 13476): nearest port 0, rows 0..199, x 0..319 (63570 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5509 (raw 13481): nearest port 0, rows 0..199, x 0..319 (63591 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5510 (raw 13486): nearest port 0, rows 0..199, x 0..319 (63609 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5511 (raw 13487): nearest port 0, rows 0..199, x 0..319 (63609 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5512 (raw 13491): nearest port 0, rows 0..199, x 0..319 (63613 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5513 (raw 13492): nearest port 0, rows 0..199, x 0..319 (63613 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5514 (raw 13496): nearest port 0, rows 0..199, x 0..319 (63608 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5515 (raw 13497): nearest port 0, rows 0..199, x 0..319 (63608 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5516 (raw 13501): nearest port 0, rows 0..199, x 0..319 (63603 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5517 (raw 13502): nearest port 0, rows 0..199, x 0..319 (63603 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5518 (raw 13506): nearest port 0, rows 0..199, x 0..319 (63604 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5519 (raw 13507): nearest port 0, rows 0..199, x 0..319 (63604 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5520 (raw 13511): nearest port 0, rows 0..199, x 0..319 (63620 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5521 (raw 13512): nearest port 0, rows 0..199, x 0..319 (63620 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5522 (raw 13516): nearest port 0, rows 0..199, x 0..319 (63615 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5523 (raw 13517): nearest port 0, rows 0..199, x 0..319 (63615 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5524 (raw 13521): nearest port 0, rows 0..199, x 0..319 (63606 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5525 (raw 13522): nearest port 0, rows 0..199, x 0..319 (63606 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5526 (raw 13526): nearest port 0, rows 0..199, x 0..319 (63622 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5527 (raw 13527): nearest port 0, rows 0..199, x 0..319 (63622 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5528 (raw 13531): nearest port 0, rows 0..199, x 0..319 (63621 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5529 (raw 13532): nearest port 0, rows 0..199, x 0..319 (63621 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5530 (raw 13536): nearest port 0, rows 0..199, x 0..319 (63608 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5531 (raw 13537): nearest port 0, rows 0..199, x 0..319 (63609 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5532 (raw 13541): nearest port 0, rows 0..199, x 0..319 (63602 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5533 (raw 13542): nearest port 0, rows 0..199, x 0..319 (63601 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5534 (raw 13546): nearest port 0, rows 0..199, x 0..319 (63604 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5535 (raw 13547): nearest port 0, rows 0..199, x 0..319 (63607 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5536 (raw 13551): nearest port 0, rows 0..199, x 0..319 (63651 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5537 (raw 13552): nearest port 0, rows 0..199, x 0..319 (63588 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5538 (raw 13556): nearest port 0, rows 0..199, x 0..319 (63587 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5539 (raw 13557): nearest port 0, rows 0..199, x 0..319 (63575 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5540 (raw 13561): nearest port 0, rows 0..199, x 0..319 (63580 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5541 (raw 13562): nearest port 0, rows 0..199, x 0..319 (63597 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5542 (raw 13566): nearest port 0, rows 0..199, x 0..319 (63590 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5543 (raw 13567): nearest port 0, rows 0..199, x 0..319 (63608 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5544 (raw 13571): nearest port 0, rows 0..199, x 0..319 (63609 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5545 (raw 13572): nearest port 0, rows 0..199, x 0..319 (63620 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5546 (raw 13576): nearest port 0, rows 0..199, x 0..319 (63620 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5547 (raw 13577): nearest port 0, rows 0..199, x 0..319 (63673 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5548 (raw 13582): nearest port 0, rows 0..199, x 0..319 (63670 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5549 (raw 13587): nearest port 0, rows 0..199, x 0..319 (63657 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5550 (raw 13592): nearest port 0, rows 0..199, x 0..319 (63691 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5551 (raw 13597): nearest port 0, rows 0..199, x 0..319 (63718 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5552 (raw 13598): nearest port 0, rows 0..199, x 0..319 (63718 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5553 (raw 13602): nearest port 0, rows 0..199, x 0..319 (63742 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5554 (raw 13603): nearest port 0, rows 0..199, x 0..319 (63742 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5555 (raw 13607): nearest port 0, rows 0..199, x 0..319 (63763 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5556 (raw 13608): nearest port 0, rows 0..199, x 0..319 (63763 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5557 (raw 13612): nearest port 0, rows 0..199, x 0..319 (63811 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5558 (raw 13613): nearest port 0, rows 0..199, x 0..319 (63811 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5559 (raw 13617): nearest port 0, rows 0..199, x 0..319 (63870 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5560 (raw 13618): nearest port 0, rows 0..199, x 0..319 (63870 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5561 (raw 13622): nearest port 0, rows 0..199, x 0..319 (63928 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5562 (raw 13623): nearest port 0, rows 0..199, x 0..319 (63928 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5563 (raw 13627): nearest port 0, rows 0..199, x 0..319 (63963 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5564 (raw 13628): nearest port 0, rows 0..199, x 0..319 (63963 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5565 (raw 13632): nearest port 0, rows 0..199, x 0..319 (63983 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5566 (raw 13633): nearest port 0, rows 0..199, x 0..319 (63983 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5567 (raw 13637): nearest port 0, rows 0..199, x 0..319 (63977 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5568 (raw 13638): nearest port 0, rows 0..199, x 0..319 (63977 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5569 (raw 13642): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5570 (raw 13643): nearest port 0, rows 0..199, x 0..319 (63987 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5571 (raw 13647): nearest port 0, rows 0..199, x 0..319 (63996 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5572 (raw 13648): nearest port 0, rows 0..199, x 0..319 (63996 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5573 (raw 13652): nearest port 0, rows 0..199, x 0..319 (63994 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5574 (raw 13653): nearest port 0, rows 0..199, x 0..319 (63992 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5575 (raw 13657): nearest port 0, rows 0..199, x 0..319 (63990 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5576 (raw 13658): nearest port 0, rows 0..199, x 0..319 (63987 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5577 (raw 13662): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5578 (raw 13663): nearest port 0, rows 0..199, x 0..319 (63987 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5579 (raw 13667): nearest port 0, rows 0..199, x 0..319 (63990 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5580 (raw 13668): nearest port 0, rows 0..199, x 0..319 (63985 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5581 (raw 13672): nearest port 0, rows 0..199, x 0..319 (63982 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5582 (raw 13673): nearest port 0, rows 0..199, x 0..319 (63984 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5583 (raw 13677): nearest port 0, rows 0..199, x 0..319 (63987 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5584 (raw 13678): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5585 (raw 13682): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5586 (raw 13683): nearest port 0, rows 0..199, x 0..319 (63974 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5587 (raw 13688): nearest port 0, rows 0..199, x 0..319 (63974 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5588 (raw 13693): nearest port 0, rows 0..199, x 0..319 (63974 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5589 (raw 13698): nearest port 0, rows 0..199, x 0..319 (63970 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5590 (raw 13703): nearest port 0, rows 0..199, x 0..319 (63978 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5591 (raw 13708): nearest port 0, rows 0..199, x 0..319 (63974 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5592 (raw 13713): nearest port 0, rows 0..199, x 0..319 (63983 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5593 (raw 13718): nearest port 0, rows 0..199, x 0..319 (63979 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5594 (raw 13723): nearest port 0, rows 0..199, x 0..319 (63985 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5595 (raw 13728): nearest port 0, rows 0..199, x 0..319 (63980 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5596 (raw 13729): nearest port 0, rows 0..199, x 0..319 (63980 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5597 (raw 13733): nearest port 0, rows 0..199, x 0..319 (63985 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5598 (raw 13734): nearest port 0, rows 0..199, x 0..319 (63985 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5599 (raw 13738): nearest port 0, rows 0..199, x 0..319 (63990 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5600 (raw 13739): nearest port 0, rows 0..199, x 0..319 (63992 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5601 (raw 13743): nearest port 0, rows 0..199, x 0..319 (63990 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5602 (raw 13744): nearest port 0, rows 0..199, x 0..319 (63988 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5603 (raw 13748): nearest port 0, rows 0..199, x 0..319 (63989 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5604 (raw 13749): nearest port 0, rows 0..199, x 0..319 (63991 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5605 (raw 13753): nearest port 0, rows 0..199, x 0..319 (63985 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5606 (raw 13754): nearest port 0, rows 0..199, x 0..319 (63985 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5607 (raw 13758): nearest port 0, rows 0..199, x 0..319 (63987 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5608 (raw 13759): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5609 (raw 13763): nearest port 0, rows 0..199, x 0..319 (63988 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5610 (raw 13764): nearest port 0, rows 0..199, x 0..319 (63989 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5611 (raw 13768): nearest port 0, rows 0..199, x 0..319 (63988 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5612 (raw 13769): nearest port 0, rows 0..199, x 0..319 (63988 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5613 (raw 13773): nearest port 0, rows 0..199, x 0..319 (63990 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5614 (raw 13774): nearest port 0, rows 0..199, x 0..319 (63990 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5615 (raw 13778): nearest port 0, rows 0..199, x 0..319 (63987 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5616 (raw 13779): nearest port 0, rows 0..199, x 0..319 (63984 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5617 (raw 13783): nearest port 0, rows 0..199, x 0..319 (63981 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5618 (raw 13784): nearest port 0, rows 0..199, x 0..319 (63985 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5619 (raw 13789): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5620 (raw 13794): nearest port 0, rows 0..199, x 0..319 (63985 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5621 (raw 13799): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5622 (raw 13804): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5623 (raw 13809): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5624 (raw 13814): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5625 (raw 13819): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5626 (raw 13824): nearest port 0, rows 0..199, x 0..319 (63987 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5627 (raw 13829): nearest port 0, rows 0..199, x 0..319 (63988 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5628 (raw 13834): nearest port 0, rows 0..199, x 0..319 (63987 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5629 (raw 13839): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5630 (raw 13844): nearest port 0, rows 0..199, x 0..319 (63984 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5631 (raw 13845): nearest port 0, rows 0..199, x 0..319 (63984 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5632 (raw 13849): nearest port 0, rows 0..199, x 0..319 (63984 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5633 (raw 13850): nearest port 0, rows 0..199, x 0..319 (63984 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5634 (raw 13854): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5635 (raw 13855): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5636 (raw 13859): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5637 (raw 13860): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5638 (raw 13864): nearest port 0, rows 0..199, x 0..319 (63988 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5639 (raw 13865): nearest port 0, rows 0..199, x 0..319 (63988 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5640 (raw 13869): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5641 (raw 13870): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5642 (raw 13874): nearest port 0, rows 0..199, x 0..319 (63988 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5643 (raw 13875): nearest port 0, rows 0..199, x 0..319 (63988 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5644 (raw 13879): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5645 (raw 13880): nearest port 0, rows 0..199, x 0..319 (63986 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5646 (raw 13884): nearest port 0, rows 0..199, x 0..319 (63987 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5648 (raw 13887): nearest port 1, rows 163..199, x 112..192 (766 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5649 (raw 13896): nearest port 1, rows 163..199, x 112..192 (766 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5650 (raw 13897): nearest port 1, rows 164..199, x 112..195 (1906 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5651 (raw 13907): nearest port 1, rows 139..199, x 112..192 (3357 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5652 (raw 13917): nearest port 1, rows 125..199, x 111..194 (5345 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5653 (raw 13927): nearest port 1, rows 81..199, x 114..192 (7559 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5654 (raw 13937): nearest port 1, rows 73..199, x 113..192 (9342 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5655 (raw 13947): nearest port 1, rows 73..199, x 114..191 (9417 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5656 (raw 13948): nearest port 1, rows 54..199, x 110..192 (10975 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5657 (raw 13957): nearest port 1, rows 54..199, x 110..192 (10975 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5658 (raw 13958): nearest port 1, rows 24..199, x 112..194 (12853 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5659 (raw 13967): nearest port 1, rows 24..199, x 112..194 (12853 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5660 (raw 13968): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5661 (raw 13978): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5662 (raw 13988): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5663 (raw 13998): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5664 (raw 14008): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5665 (raw 14048): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5666 (raw 14049): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5667 (raw 14059): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5668 (raw 14069): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5669 (raw 14079): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5670 (raw 14089): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5671 (raw 14099): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5672 (raw 14109): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5673 (raw 14119): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5674 (raw 14129): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5675 (raw 14139): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5676 (raw 14149): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5677 (raw 14150): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5678 (raw 14159): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5679 (raw 14170): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5680 (raw 14180): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5681 (raw 14190): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5682 (raw 14200): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5683 (raw 14209): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5684 (raw 14210): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5685 (raw 14219): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5686 (raw 14220): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5687 (raw 14230): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5688 (raw 14240): nearest port 0, rows 0..199, x 0..319 (57513 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5690 (raw 14295): nearest port 1, rows 7..12, x 130..196 (166 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5691 (raw 14298): nearest port 0, rows 0..199, x 0..319 (55514 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5692 (raw 14303): nearest port 0, rows 0..199, x 0..319 (49248 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5693 (raw 14307): nearest port 0, rows 0..199, x 0..319 (49249 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5694 (raw 14312): nearest port 0, rows 0..199, x 0..319 (45009 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5695 (raw 14317): nearest port 0, rows 0..199, x 0..319 (45009 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5696 (raw 14321): nearest port 0, rows 0..199, x 0..319 (41007 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5697 (raw 14326): nearest port 0, rows 0..199, x 0..319 (38647 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5698 (raw 14331): nearest port 0, rows 0..199, x 0..319 (38288 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5699 (raw 14335): nearest port 0, rows 0..199, x 0..319 (36512 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5700 (raw 14340): nearest port 0, rows 0..199, x 0..319 (35894 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5701 (raw 14345): nearest port 0, rows 0..199, x 0..319 (33824 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5702 (raw 14349): nearest port 0, rows 0..199, x 0..319 (33288 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5703 (raw 14354): nearest port 0, rows 0..199, x 0..319 (32400 px)
gp_compare: gp-u10-ending: frames: coverage (reported, not ratcheted): 121 non-black port frame(s) up to port 4662 not exhibited by any classified capture frame: [9, 11, 29, 31, 32, 90, 91, 127, 131, 132, 133, 135, 217, 972, 973, 1036, 1037, 1518, 1619, 1620] ...
gp_compare: gp-u10-ending: trace: 9612 frames compared (f 134..), 34 without a capture snapshot; first tick difference f=135 (reported, not ratcheted)
gp_compare: gp-u10-ending: trace: normalised (reported, not ratcheted): ent 0 of 9612 differ; t508 6936 of 9612 differ (first f=264)
gp_compare: gp-u10-ending: trace: 0 differing through 9953
gp_compare: gp-u10-ending: moves: 9612 frames compared (f 134..) over c0 c1 r0 r1 s0_43, 34 without a capture snapshot
gp_compare: gp-u10-ending: moves: 0 differing through 9953
```

The first and last unexplained lines are:

```text
gp_compare: gp-u10-ending: frames: FIRST UNEXPLAINED capture 4113 (raw 7858): nearest port 3444, rows 166..199, x 0..319 (9732 px)
gp_compare: gp-u10-ending: frames: UNEXPLAINED capture 5703 (raw 14354): nearest port 0, rows 0..199, x 0..319 (32400 px)
```

### The four runs

The 287 unexplained capture frames form four runs. The 23 all-black capture frames are
the documented capture artefact and are skipped; the two black frames inside the tail,
5647 (raw 13885) and 5689 (raw 14291), separate runs 2/3 and 3/4.

| run | capture frames | raws | n | nearest port | rows | x | px |
|---|---|---|---|---|---|---|---|
| 1 | 4113..4134 | 7858..7880 | 22 | 3444..3463 | 27..199 | 0..319 | 815..47747 |
| 2 | 5437..5646 | 13279..13884 | 210 | 0 | 0..199 | 0..319 | 63201..64000 |
| 3 | 5648..5688 | 13887..14240 | 41 | 0..1 | 0..199 | 0..319 | 766..57513 |
| 4 | 5690..5703 | 14295..14354 | 14 | 0..1 | 0..199 | 0..319 | 166..55514 |

`nearest` is `gp_compare.nearest()`'s row-hash maximum, not a temporal match; in runs 2..4
the dump has no frame after the replay's end, so the nearest is the port's boot frame
(0 or 1) and the boxes are against it (the same artefact the U5 record's §C5.18 records
for its tail).

### Run 1 — capture 4113..4134: A, the RLE span writers' bank carry (Task 3)

The 22 frames are the poked final's death animation. Capture 4113 (raw 7858) is at
`f = 0x1502` (its S record `ms=112273 f=1502 mode=000D ... rng=39F4CD40 cred=00000004`,
`poll.log:5416`), the same f as port 3444 (`frames.txt`: `03444 f=1502 tick=000015B2
mode=000D`); the port's T record at `f = 0x1502` has the same mode, state, rng and
credit (`trace.txt`; the trace claim reports `0 differing through 9953`).

Measured on the frame bytes (not a pin; `gp_compare`'s loaders over the whole dump).
**These two bullets are the pre-fix measurement; the Task 3 decision below replaces
them** ("no other frame" held only because every candidate held the offset palette):

- capture 4113's rows 0..165 are byte-equal to port 3444 and to **no other** frame of
  the 4664-frame dump;
- capture 4113's rows 166..199 are byte-equal to **no** frame of the dump (likewise
  4114 rows 168..199 and 4134 rows 138..199), so the run is not a splice of any two
  dumped port frames;
- the box moves from rows 166..199 (9732 px) at 4113 to rows 138..186 (1209 px) at
  4134 (the tested band of 4134's 138..199 box with the matching rows below 186);
  4123 is the run's largest frame (rows 27..199, 47747 px) with its bottom rows
  185..199 equal to port 3454, a scene flash inside the run.

The port frames 3444..3463 are `f = 0x1502..0x1515`, modes `0x0D/0x0C`; the alternating
pokes `0x104B0C = 1` / `0x10789E = 0x78` fire at `f = 0x14CB..0x1543` (the same
`0x10789E` also at `f = 0x14BF`; below), so the run is inside the poked path.

**Task 3's decision (measured 2026-10-21 at `969b6410` + the fix): A — a real port
divergence in the RLE span writers.** The state, sprite ids, positions and shapes agree;
only the *palette index* of the drawn pixels differs. Measured over the whole run:

- capture 4113's rows 0..165 are byte-equal to port 3444 and rows 166..199 to port
  3445 — a normal 70.09/60.05 splice, i.e. no missing content; the pre-fix
  comparer saw no splice because of the index offset below;
- every differing pixel of the run maps to `DAC[port_index + 1]` (701 of 701 in
  frame 4113's band; the per-frame rate is 58-100% one step, most frames 100%, the
  remainders being the splice's animation-frame edges), and the rest of the screen
  is byte-equal;
- the drawn sprites are the dying CPU fighters' death-animation sprites: id `0x0F9D`
  (130x160, palette entry 0x1BB9FCD8, start 275 → wrapped DAC 19..49, bank 18) and
  id `0x1825` (52x49, 0x10A50F70, start 244, bank 243), both at layer 224, plus
  their neighbouring animation frames (`0x0F9C`, `0x0F9B`, ...), so the animation
  frame index and the handles are the capture's; the player (0x1BB9FD58, start 142,
  bank 141) and the arena (0x0C039CA0, start 82, bank 81) are byte-exact.

The cause is the **dword bank add** of the original's span writers:

- `0x51E91` reads `ebx = DAT_00081310[bank_byte]` = (bank_byte-1) replicated over
  four bytes; `0x58D14..0x58D40` (literal) and `0x58554..0x58750` (run) add it with
  `add edx, ebx`, so a byte sum above 0xFF **carries into the next pixel of the same
  four-pixel group** (`0x81010`/`0x81110` are the unrolled writers);
- a *run colour* is `DAT_00081314[colour] (= colour replicated) + ebx`; its full
  four-pixel groups write the carried dword, and the 2/3-pixel tail is a **carried
  word** when the run has 14+ pixels (`0x585F0` n=14 is the first word-tail writer:
  3 dwords + `mov word [edi+0xc], dx`; `0x58600` n=15 word + `dl`; n=13 is `0x585E0`,
  one `dl`) but `dl` repeated for shorter runs (`0x5855C`/`0x58564`); the mirror fill
  table `0x80D0C` has the same threshold (`0x52284` n=14 word, `0x52294` n=15 word +
  `dl`);
- the hflip tables carry the same dword: `0x80E0C`'s literals add the bank dword and
  `bswap` it into the backward destination (`0x529B4` n=4: `[edi-3] = bswap(edx)`; the
  tails are `dl`/`dh` at `edi`/`edi-1`, `0x52990` n=2), and `0x80D0C`'s fills anchor
  the natural dwords at the run's high end (`0x521FC` n=4 `[edi-3]`, `0x5222C` n=8 two
  dwords) with the low-end 1..3 bytes as `dl` and the 14+ word tail at the low byte
  (`0x52284` n=14, `0x52294` n=15). The mirrored span is therefore not the plain
  pattern reversed: its fill groups keep the dword's byte order, and its literal
  group phase starts at the visible source start. The write maps of all four tables
  (0x81010, 0x81110, 0x80D0C, 0x80E0C) were extracted for n=1..63 from the unrolled
  routines and compared with the implemented byte rules: 0 mismatches;
- the port added the bank byte-wise (`dst[i] = src[i] + bank`) and, for a mirrored
  span, wrote `(u8)sum` / `src + bank` for every pixel. That is exact while
  `colour + bank <= 0xFF`, which is why the arena, HUD and the title/attract oracles
  never saw it; the death flash palettes (start 244/275, run colours 0xFD..0xFF)
  overflow, so their pixels rendered one entry low. The mirrored low-end group was
  low on its rightmost bytes too.

The failing-first evidence: the new assertions in `test_platform.c`'s
`check_bank_and_colour`/`check_raw_copy` (4-pixel fill `02 03 03 03`, 3-pixel
`02 02 02`, 5-pixel `02 03 03 03 02`, 14-pixel tail `02 03`, literal
`FE FF 01 02`+4 → `02 04 06 06`) fail on the pre-fix tree (the old byte-wise add
prints `2 != 3`, `3 != 4`, `5 != 6`) and pass after; the new differential case
`render_list_blit` `b8` (bank byte 0xFE, offset 253, every drawn colour overflows)
reports `original 0x68, port 0x67` on the old code and VERIFIED after; the mirrored
case `b9` (b8's bank over `D5B_R | 0x8000`, type 9) reports `original 0x11, port 0x10`
on the pre-mirror-fix tree and VERIFIED after; the harness suite pins the catch sets
(`@bank`, `@rowtab`, `@stride` gain `b8`/`b9`, `@nomirror` gains `b9`). The unit
`check_bank_and_colour` cases pin the mirrored fill 5 (`02 02 03 03 03`), 15
(`02 02 03 02 03 03 03 ...`), the mirrored literal 6 (`08 07 06 06 04 02`) and a
clipped mirrored literal whose group phase starts at the visible source (`07 06 06 04
02`). The differential fixture's longest run is 8, so the 14+ word tail is
unit-tested only (raw-derived, and the n=1..63 table check above).

**The measured consequence (2026-10-21, `make gp-ending-oracle`):**

```text
gp_compare: gp-u10-ending: frames: window from capture 83 (raw 1745); 5334 classified: 3144 clean, 2181 splice, 6 transition, 0 band-allowed, 1 scan3-allowed, 1 overlay-allowed, 1 unexplained, 21 all-black
gp_compare: gp-u10-ending: frames: FIRST UNEXPLAINED capture 5437 (raw 13279): nearest port 0, rows 0..199, x 0..319 (63947 px)
gp_compare: gp-u10-ending: frames: first unexplained 5437, ratchet N 5437 ok
gp_compare: gp-u10-ending: frames: window reach port 4662 > pinned reach 3444 (improved: raise the pin)
gp_compare: gp-u10-ending: trace: 0 differing through 9953; ratchet N 9954 ok
gp_compare: gp-u10-ending: win: 0 differing through 9953; ratchet N 9954 ok
```

Runs 2..4 (below) are the capture's post-script tail and are not in the enforced
window: the first unexplained is 5437, run 2's start, exactly the next pin the
reviewer named. Pins: `GP_ENDING_MIN_FIRST` 4113 → **5437** (5438 fails);
`GP_ENDING_MIN_REACH` 3444 → **4662** (the highest port frame the capture exhibits;
the dump's last frame is index 4663, f=0x26E1, and is not exhibited; 4663 fails); `GP_ENDING_MAX_START` 83, `GP_ENDING_TRACE_MIN_FIRST` 9954,
`GP_ENDING_WIN_MIN_FIRST` 9954 and `GP_ENDING_MILESTONES` 30 unchanged (the trace,
win and path lines still pass at their exact pins). The run is fully explained; the
B and C candidates are closed — B cannot hold because the port applies the same
pokes and the capture's state is the port's (the W records' `was=00 now=01` only
show the poke was not pre-empted), and C is not needed because the frames are a
clean splice of two dumped port frames (the allowance tables bind exact bytes; a
palette transform is not one of their classes).


### Run 2 — capture 5437..5646: none of A/B/C, the capture's post-script tail

The capture's own `session.txt` records `stop_at_end=1 tail=60 signal_f=271D` and
`check=ok stopped at the end (SIGTERM at f=271D, rc=0)`: after the scenario's end step
(its `X ms=189553 f=26E1 step=21 end`, `poll.log:10008`) the harness keeps capturing for
a fixed 60 iterations (`signal_f=0x271D` = `0x26E1` + 60), and the port's replay script
ends at the same `end 9953` (`f = 0x26E1`, its last trace line and frame 4663). Capture
5437 (raw 13279) is 189.46 s at the documented 70.09 Hz (the same mapping puts capture
frame 0's raw 1391 at 19.85 s against the first S record's `ms=19999`), i.e. within one
capture interval of the X. The content is the attract's first boot logo, `twi5.smk` (the
Time Warner Interactive logo; `attract_step` case 0, the original `0x11000` case 0's
`0x1C740` calls, `port/src/game/attract.c:227-233`), which the replay's script ends
before. **None of A/B/C — how far the port got, not a divergence** (review focus 3).
Task 3's fix leaves 5437, this run's start, as the pin: these frames are not in the
enforced window. A later task that wants them compared must extend the replay's `end`
past `0x26E1` (the capture's tail is 60 iterations) and treat the movie pacing as its
own claim (`smk-oracle` already compares twi5/twg in isolation).

### Run 3 — capture 5648..5688: none of A/B/C, the second boot logo

Same post-script tail (the X and `tail=60` above). The 41 frames are `twg.smk` (the TWG
logo; 5648's first box rows 163..199 x 112..192, 766 px, is the logo assembling;
5660..5688's 57513 px is the settled logo against the port's black boot frame).
**None of A/B/C — the same script end.** The same `end` extension covers it.

### Run 4 — capture 5690..5703: none of A/B/C, the attract title

Same post-script tail. 5690 is the 166 px `CREDITS: 5` band (rows 7..12, x 130..196) on
black; 5691..5703 is the waterfall attract title with `CREDITS: 5` (32400..55514 px
against the port's boot frame). **None of A/B/C — the same script end.** These screens
belong to the front-end/smk oracles, not to a U10 tail pin.

### The pokes (verbatim from `poll.log`)

```text
W ms=40477 f=0482 step=4 addr=00108106 len=7 was=00000000000000 now=80808080808080 late=0 race=0
W ms=40477 f=0482 step=4 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=56708 f=0850 step=5 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=110443 f=14BF step=6 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=110636 f=14CB step=7 addr=00104B0C len=1 was=00 now=01 late=0 race=0
W ms=111203 f=14D5 step=8 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=111372 f=14DF step=9 addr=00104B0C len=1 was=00 now=01 late=0 race=0
W ms=111856 f=14E9 step=10 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=112019 f=14F3 step=11 addr=00104B0C len=1 was=00 now=01 late=0 race=0
W ms=112188 f=14FD step=12 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=112355 f=1507 step=13 addr=00104B0C len=1 was=00 now=01 late=0 race=0
W ms=112521 f=1511 step=14 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=112687 f=151B step=15 addr=00104B0C len=1 was=00 now=01 late=0 race=0
W ms=113188 f=1525 step=16 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=113355 f=152F step=17 addr=00104B0C len=1 was=00 now=01 late=0 race=0
W ms=113820 f=1539 step=18 addr=0010789E len=1 was=00 now=78 late=0 race=0
W ms=113997 f=1543 step=19 addr=00104B0C len=1 was=00 now=01 late=0 race=0
```

The replay applies each at `W.f + 1` (§W.8 of the U9/U10 record); steps 7..19 are the
forced death-done path whose frames 4113..4134 measure.

### What this section does not claim

This report pass decided no class, no pin, no allowance and no named gap; Task 3's
decision subsection under Run 1 now does (A, the span writers' bank carry, pins 5437 and
4662). Runs 2..4 are the capture's post-script tail (60 iterations after the X), not
port behaviour past the replay's end. The 287 unexplained frames were the exact count
for this dump and this capture at 60c99a51: the fix makes Run 1 explained, so the
enforced run stops at 5437 and the report lists 265 unexplained (runs 2..4) — re-measure
it when the presents change.

## §J9 The joystick verdict (Task 13, raw evidence; measured at f1f665c2)

### The verdict

The game logic has no joystick path. `0x2D2F0` (`input_joystick_device`) is `xor eax,eax; ret` —
the raw bytes at code-object file offset `0x80144` (`0x2D2F0 + 0x52E54`) are `31 c0 c3 00 ...` —
and it has its own differential row (`tools/diff_verify.py`:
`Spec("input_joystick_device", 0x2D2F0, [Case("j0", {"eax": 0xFFFFFF00}),
Case("j1", {"eax": 0x00000001})], eax_mask=0xFFFFFFFF, mutants=("@keep", "@al"))`). `0x4F644`
(`input_state_update`) stores the two calls' results in `0x1088DC` (`0x4F66D`, selector `0xFF`)
and `0x1088D4` (`0x4F67C`, selector `0xFFFFFF00`, then `and eax,0xFF` at `0x4F681`); both are 0,
so the gate at `0x4F68B` (`f6 05 d4 88 08 00 02` = `test byte [0x1088d4],2`, `74 0b` =
`je 0x4F69F`) never opens and the merge at `0x4F694`/`0x4F699` (`a1 d8 88 08 00` =
`mov eax,[0x1088d8]`, `09 05 e4 88 08 00` = `or [0x1088e4],eax`) never runs. The row records the
same in ``unhit_named={0x4F694: "dead with 0x2D2F0 real (`xor eax,eax; ret`): the byte 0x1088D4 it
has just stored is 0, so `test byte [0x1088d4],2; je` 0x4F68B is always taken"}``. `0x1088D4`'s only
other read, `0x3575E` (`f6 05 d4 88 08 00 04` = `test byte [0x1088d4],4`) and its `0x38526` twin,
feeds the `0x35770`/`0x38538` store of `0x1078FB` (`((0x1088D4 & 4) == 0) + 2`), a constant 3 in
the shipped bytes; `0x1088DC` has no reader.

**Correction to W8's wording (the raw wins).** The dword the dead gate would merge is `0x1088D8`,
the held mask from the second `0x50161` selector (`port/src/platform/input.c:118`), not a
joystick word; the joystick result words are `0x1088DC`/`0x1088D4`, and they are 0. W8's "the only
other game-port reader is `0x5004A`" is also too narrow: the live game-port sampler is the
host-owned ISR `0x1BBAC`, and the boot checks `0x20DAC`/`0x20DD1` also wait on the game port
(`0x4FBBB`) and clear the device words `+0x2D4`/`+0x2D6` when the check fails.

### The original's joystick is host-owned input

The original does have a joystick path, but it is entirely in the host-owned input layer, not in
the game logic:

| address | what | evidence |
|---|---|---|
| `0x1BBAC` | the timer ISR's input sampler; called at `0x1BE1C` from `0x1BDF4` | `e8 8b fd ff ff` = `call 0x1bbac` at `0x1BE1C`; host-owned, `tools/port_classification.txt:44` |
| `+0x2D4`/`+0x2D6` | the key-config device words it switches on (0 keyboard, 2/4/6 joystick) | `0x1BBB6`/`0x1BBDC`; the `+0x2D4` switch `0x1BD06` `jmp [eax*4+0x1BB74]`, table `0x1BD0E/0x1BD15/0x1BD2E/0x1BD3C` for 0/2/4/6; the `+0x2D6` switch `0x1BD61`/`0x1BD7C` `jmp [edx*4+0x1BB90]`, table `0x1BD84/0x1BD8B/0x1BDA4/0x1BDC9` for 0/2/4/6 |
| `0x4FF8F`/`0x4FFD8` | calibrated A/B axis bits from `0xE1C1E..0xE1C2C`: the bit sets at `abs(diff) >= 0x1E` (a ±0x1D dead band) | host-owned, rows 70-71; `0x4FFA2`/`0x4FFAC` (`66 83 f9 1e`/`66 83 f9 e2`) and `0x4FFC4`/`0x4FFCE` for A, `0x4FFEB`/`0x4FFF5` and `0x5000D`/`0x50017` for B; called from the sampler's 2/4/6 arms (`0x1BD15`, `0x1BD2E`, `0x1BD3C`, `0x1BD8B`, `0x1BDA4`, `0x1BDC9`) |
| `0x1B890`/`0x1B8DC`/`0x1B908` | `in 0x201` button bits | host-owned, rows 41-43 |
| `+0x2D8`/`+0x2D9` | the key bitmap the arms write (`0x1BD5B`, `0x1BDC0`, `0x1BDE8`) | `0x500C4` reads them (`input.c:92-93`) |
| `0x1AE28` | `config_keys_apply` writes the device words (`DSW` at `0x1AE3B`/`0x1AE4F`) | `port/src/game/config.c:1285,1287` |
| `0x31F24` | OPTIONS MENU MODIFY CONTROLS edits device 0/2/4/6 and calls `0x1AE28` (`0x32347`, argument at `0x32345`) | `port/src/game/svcmenu.c:848` |
| `0xA2C62` | the default key record: device words 0/0 (keyboard) | raw `00 00 ... 00 00 ...` at data-object file offset `0xE94B6` |
| `0x20DAC`/`0x20DD1` | boot controller checks: they call the host-owned `0x4FBBB` (the game-port wait) and clear the device word `+0x2D4` (`0x20DBA`) / `+0x2D6` (`0x20DDF`) when the check fails; the port leaves `0x20D0F..0x20DE3` out (`port/src/game/flow.c:6980`) | `tools/port_classification.txt:63` (the callee `0x4FBBB`); record named-gaps-b §B.6a |
| `0x5004A` | the calibration (PIT `0x50021`, game port `0x201` via `0x4FC05`/`0x4FCFD`); callers `0x1C0AC`, `0x24DD3` (Alt-J), `0x2EB4D` | host-owned, `tools/port_classification.txt:66` |

The chain is: `0x1BBAC` → `+0x2D8`/`+0x2D9` → `0x500C4`'s level word `0xE1C34` → `0x4F644`'s pad
words `0x1088E0`/`0x1088E2` → `fight.c`'s stick bits (`0x43BB5` etc.). The keyboard case 0 of the
same switch is what the port's host models (`host.c` `host_kb_bit`, the device-0 arms); the
joystick cases 2/4/6 are host-owned, not modelled by the port's host. The default record is
keyboard (0/0), so a default boot has no joystick input; a service-menu selection of 2/4/6 plus a
saved image reaches the sampler's joystick arms. All the live game-port sampling code is
host-owned; the config writers `0x1AE28`/`0x31F24` are ported. The unreferenced sampler routines
`0x1B934..0x1BB73` (callers of `0x4FF8F`/`0x4FFD8` at `0x1B976`/`0x1B9B6`/`0x1BA16`/`0x1BA96`/
`0x1BAD6`/`0x1BB36`, named as callers by `tools/port_classification.txt:70-71`) have no row of
their own (`tools/port_classification.txt` rows 34-44, 62-67, 70-71 cover the live sampler, the
calibration and the axis readers).

### What follows for the host phase

An SDL-gamepad map is **additive**: it belongs in the host input layer (`host.c`), the layer that
already stands in for `0x1BBAC`, and it feeds the same key bitmap. It is not a reimplementation of
a ported path and needs no `0x4F644` change; the `0x2D2F0`/`0x4F644` gate stays dead. If the host
wants the original's joystick behaviour, the thing to model is the sampler's device-2/4/6 arms
(the `|diff| >= 0x1E` axis thresholds — a ±0x1D dead band — and the `in 0x201` button bits), not
`0x4F644`. A report-only Alt-J/boot probe (`0x5004A`, `0x24DD3`; `0x2EB4D`) is optional, a
K11-style session, and is not a `gp-*` fixture and not a pin.

### What this section does not claim

It does not claim the port reproduces the original's game-port sampling, the service menu's device
selection, the `|diff| >= 0x1E` axis thresholds (a ±0x1D dead band) or the button bits. It adds no
capture, no `gp-*` scenario, no pin and no `port/src` change; the `0x2D2F0` and `0x4F644` rows are
unchanged and VERIFIED. The verdict is raw-only, measured at `f1f665c2`.

## §J4 The `gp-u12-endurance` fight capture (evidence closeout Task 6; measured on the closeout branch)

`data/k11-captures/gp-u12-endurance` (git-ignored) captures the ENDURANCE team pass and its fight. The already-existing artifact was verified, not recaptured. `session.txt` reports all checks ok: base; 22/22 injected steps; end frame reached; mode 0x27 after Enter; snapshots' keyboard bytes equal raw (0 differ); 772/772 frames written; port script v2; no unscripted input; 0/0 pokes, 0 raced; stopped at the end (SIGTERM at f=07C6, rc=0). `python3 tools/gp_capture.py check-input data/k11-captures/gp-u12-endurance` prints `check=ok no unscripted input`. `poll.log` sha256 is `2edcc088d26162d0fd288c8a5caf17500ac4b7f4a974f57b6f780be0236a9014`; the directory has 772 `frame_*.raw.gz` files.

### The replay's miss set

`make gp-replay scenario=gp-u12-endurance` reaches the X record (f=0x78A). Its exact misses are the known pair `0x29D60` and `0x5D812` from `frontend_mode_1b_step`, one hit each; these are the §G.24 wipe hooks, not unported replay targets. The other known rows are `0x5D812` from `actor_spawn` (3899 hits) and `set_dead` (3602 hits). `distinct=4 dropped=0`. `port/tests/test_platform.c` pins the two scenario-specific pairs in `k_miss_gp_u12_endurance`, and the exact-name `k_gp_sets` row selects them. No new porting was needed.

### The mode path

`python3 tools/gp_modes.py check --scenario gp-u12-endurance --capture data/k11-captures/gp-u12-endurance` passes. Its measured path is:

```
row mode 0x2B: f=33B (P); no other game-start mode
START MENU open before select; divert b1d=3 b1f=3 at f=33C; credits 5 -> 5
character select mode 0x10 at f=360
mode 0x30 reached at f=65E
mode 0x31 reached (report only) at f=6D9 (port frame 418)
scenario X record at f=78A
```

Thus the non-failing 0x31 report row did appear; no named limit for that half is needed.

### The measured report

`python3 tools/gp_compare.py --report --scenario gp-u12-endurance --capture data/k11-captures/gp-u12-endurance --port /tmp/pr_gp_dump/gp-u12-endurance --max-unexplained 0` is report-only (no ratchet; exit 0):

```
frames: window from capture 100 (raw 1745); 662 classified: 400 clean, 222 splice, 0 transition, 0 band-allowed, 0 scan3-allowed, 0 overlay-allowed, 40 unexplained, 10 all-black
frames: FIRST UNEXPLAINED capture 732 (raw 3728): nearest port 490, rows 57..158, x 154..230 (2221 px)
frames: coverage (reported, not ratcheted): 12 non-black port frame(s) up to port 490 not exhibited by any classified capture frame: [14, 16, 74, 302, 339, 342, 343, 344, 345, 347, 348, 349]
trace: 1610 frames compared (f 13A..), 7 without a capture snapshot; first tick difference f=13B (reported, not ratcheted)
trace: normalised (reported, not ratcheted): ent 0 of 1610 differ; t508 673 of 1610 differ (first f=34F)
trace: 0 differing through 1930
moves: 1610 frames compared (f 13A..) over c0 c1 r0 r1 s0_43, 7 without a capture snapshot
moves: 0 differing through 1930
```

The report-only command lists capture 732..771 as the 40 unexplained frames after the replay's X record, not as a divergence: the scripted port replay ends at port frame 490, and those capture frames are the post-script tail. The oracle's ratchet stops at the pinned first unexplained frame 732. In its window below 732 it reports 400 clean, 222 splice and 10 all-black frames, with no unexplained content-bearing frame. The 12 unexhibited port frames are reported coverage, not a coverage/order ratchet.

### The pins and their proofs (`make gp-u12-oracle`, PASS at the pinned values)

| pin | value | provenance | failing-direction proof |
|---|---:|---|---|
| `GP_U12_MIN_FIRST` | 732 | First unexplained capture frame 732; capture frames below it in the exhibited window are explained | 733 → FAIL (`first unexplained 732 < ratchet N 733`) |
| `GP_U12_TRACE_MIN_FIRST` | 1931 | `0 differing through 1930`; gp_compare's exact pin at the X record f=0x78A | 1932 → FAIL (`N 1932 > end 1931: N is unreachable`) |
| `GP_U12_MAX_START` | 100 | Measured window starts at capture 100 (raw 1745) | 99 → FAIL (`window starts at capture 100 > pinned start 99`) |
| `GP_U12_MIN_REACH` | 490 | Last port frame reached by the replay | 491 → FAIL (`window reach port 490 < pinned reach 491`) |
| `GP_U12_CAPTURE_FRAMES` | 772 | Capture contains 772 `frame_*.raw.gz` files | 773 → FAIL (capture identity frame-count mismatch) |
| `GP_U12_CAPTURE_SHA256` | `2edcc088d26162d0fd288c8a5caf17500ac4b7f4a974f57b6f780be0236a9014` | `poll.log` sha256 | changed first hex digit (`3edcc088…`) → FAIL (capture identity mismatch) |

Each proof was run against the real capture and replay dump with only that gp_compare pin changed; all six returned nonzero with the stated failure, and the defaults were left unchanged. The final `make gp-u12-oracle` passes, including `tools.tests.test_gp_modes`, the evidence check and the replay/compare. The narrow claim is no unexplained content-bearing capture frame in the exhibited window below 732, the ENDURANCE team pass and modes 0x30/0x31 reached, and the scripted replay reaching port frame 490. It does not claim the 732..771 post-script tail is reproduced, nor port-frame order or complete interior frame coverage.

## §J5 The `gp-u13-ending2` BLIZZARD ending capture (evidence closeout Task 7; measured on the closeout branch)

`data/k11-captures/gp-u13-ending2` (git-ignored) is the U10-clone path to the second ending. P1 takes three right edges to character-select cell 3 (class 1, BLIZZARD), then the scenario's `memsize=64` and seven `0x81` land-mark poke lead to the final. Nobody typed in the DOSBox-X window. `DISPLAY=:1 make gp-capture scenario=gp-u13-ending2` ran with the scenario's 240 s time limit and emitted `dosbox memsize=64` in its DOSBox-X command.

### The capture checks and identity

```
snapshots 10021, f 5..2754, 43 frames missed (spec §3.7)
CHECK base: ok
CHECK steps fired 23/23: ok
CHECK end frame reached: ok
CHECK mode 0x27 after the Enter: ok
CHECK snapshots kb == raw (0 differ): ok
CHECK frames written 5904/5904: ok
CHECK port script v2: ok
CHECK no unscripted input: ok
CHECK pokes written 17/17, 0 raced: ok
CHECK stopped at the end (SIGTERM at f=2754, rc=0): ok
```

`python3 tools/gp_capture.py check-input data/k11-captures/gp-u13-ending2` prints `check=ok no unscripted input`. The capture spans raw video frames 1388..14417 and contains 5904 `frame_*.raw.gz` files. `poll.log` sha256 is `e581916fa0a8e23a2ea52519aa0b358e4bc63084ce9defa0e1f733f3d12b69c6`. The recorder printed ffmpeg broken-pipe muxer messages while DOSBox-X shut down; the capture command returned success and every capture check above passed.

### The replay's miss set

The first `make gp-replay scenario=gp-u13-ending2` correctly failed the existing exact-set guard (`4 != 2`):

```
fn-miss PR_GP_DUMP 0x5D812 actor_spawn hits=66037
fn-miss PR_GP_DUMP 0x5D812 set_dead hits=65158
fn-miss PR_GP_DUMP 0x29D60 frontend_mode_1b_step hits=1
fn-miss PR_GP_DUMP 0x5D812 frontend_mode_1b_step hits=3
fn-miss PR_GP_DUMP distinct=4 dropped=0
```

The two scenario-specific pairs are the §G.24 wipe hooks: `0x29D60` is the bare `ret`, and `0x5D812` is the runtime `xor eax,eax; ret`; they are not unported replay targets. The base `actor_spawn`/`set_dead` pairs are the known runtime stub. `port/tests/test_platform.c` now pins only the two measured `frontend_mode_1b_step` pairs in `k_miss_gp_u13_ending2`, selected by its exact-name `k_gp_sets` row. The replay then passes with `distinct=4 dropped=0`, 0 restarts, and the 36-line port script ending at X f=10013. No new porting was needed.

### The BLIZZARD ending evidence

`python3 tools/gp_win.py check --scenario gp-u13-ending2 --capture data/k11-captures/gp-u13-ending2` passes all 30 milestones. The ending predicate requires `ad4=0, c0=1`; the matched ending row at f=184D proves both, and specifically proves this is BLIZZARD (`c0=1`), not U10's character-0 ending.

```
character select (mode 0x10) at f=283 ok
round 1: P1 is character 1 (cursor 3 confirmed, 0x43CAD) at f=4D0 ok
round 1 KO with the seven lands poked P1's at f=4DC ok
round 2 (mode 6, round index 2) at f=89E ok
round 2 KO: P1 wins the match (0x27BA4 result 0) -> mode 9 at f=8AB ok
the conquered-lands screen (mode 0x12, 0x4142C), the won land marked by 0x286BC (0x80 | side 0 | character) at f=BF3 ok
0x41C28 state 3 counts the seventh land at f=C94 ok
WORLD DOMINATION: 0x4160C clears the marks, +0x82 = 1 at f=CE0 ok
YOU MUST REPLENISH YOUR HEALTH: mode 0x23 (0x417C4, 0x26978) at f=E12 ok
the health bonus (mode 0x22) at f=E8D ok
mode 0x24 (0x26F58) at f=1321 ok
the final: mode 0xC, stage 7, flag DS_00104B14 (0x26F58, 0x25C88) at f=14ED ok
final opponent 1 KO'd -> mode 0xD (0x272DC) at f=14F9 ok
final opponent 1 replaced (0x274FC, count 1) at f=1503 ok
final opponent 2 KO'd -> mode 0xD (0x272DC) at f=150D ok
final opponent 2 replaced (0x274FC, count 2) at f=1517 ok
final opponent 3 KO'd -> mode 0xD (0x272DC) at f=1521 ok
final opponent 3 replaced (0x274FC, count 3) at f=152B ok
final opponent 4 KO'd -> mode 0xD (0x272DC) at f=1535 ok
final opponent 4 replaced (0x274FC, count 4) at f=153F ok
final opponent 5 KO'd -> mode 0xD (0x272DC) at f=1549 ok
final opponent 5 replaced (0x274FC, count 5) at f=1553 ok
final opponent 6 KO'd -> mode 0xD (0x272DC) at f=155D ok
final opponent 6 replaced (0x274FC, count 6) at f=1567 ok
final opponent 7 KO'd -> mode 0xD (0x272DC) at f=1571 ok
YOU ARE MASTER OF THE NEW URTH: mode 0xF, count 7 (0x274FC) at f=157B ok
BLIZZARD's ending (mode 0x1F, 0x208F8): the winner's character DS_0010782A[DS_00104AD4 * 0x94] (0x20900..0x20914) is character c0=1 at f=184D ok
the ending, second part (mode 0x1F, state 3) at f=1DEF ok
the high-score entry (mode 0x1E) at f=21B5 ok
back in mode 3 at f=271D ok
30/30 milestones ok
```

### The measured overlay and frame report

The first report-only unexplained frame is the loader's `- LOADING -` strip, so `tools/gp_compare.py` has a U13-specific `OVERLAY_ALLOWED` recipe. Capture frame 2188 (raw 5047) is exactly 166 glyph pixels (rows 192..197, x 0..85) from port frame 1799 over the mode-0x12 screen at port 1800; port 1798 supplies the same indices under the stale black palette. The measured glyph-byte sha256 is `de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced`. These are U13's own capture/frame indices, independently measured rather than copied from U10. `test_u13_overlay_recipe_is_pinned_and_mutations_fail` pins that exact tuple and shows that changing its sha or screen-pair index un-explains the fixture; the full `tools.tests.test_gp_compare` suite passes (63 tests). No mode-8 scan-out allowance is needed (`scan3-allowed=0`).

`python3 tools/gp_compare.py --report --scenario gp-u13-ending2 --capture data/k11-captures/gp-u13-ending2 --port /tmp/pr_gp_dump/gp-u13-ending2 --max-unexplained 0` (report-only, no ratchet) reports:

```
frames: captured frame 2188 (raw 5047) allowed by name as a glyph overlay: 166 px of port 1799 over port 1800 (the mask from port 1798, the named glyph bytes)
frames: window from capture 96 (raw 1739); 5786 classified: 3492 clean, 2270 splice, 7 transition, 0 band-allowed, 0 scan3-allowed, 1 overlay-allowed, 16 unexplained, 22 all-black
frames: FIRST UNEXPLAINED capture 4248 (raw 7887): nearest port 3551, rows 0..96, x 0..319 (22923 px)
frames: other unexplained capture frames 4320, 5120, and 5891..5903
frames: coverage (reported, not ratcheted): 72 non-black port frame(s) up to port 4939 not exhibited by any classified capture frame
trace: 9667 frames compared (f 130..), 43 without a capture snapshot; first tick difference f=131 (reported, not ratcheted)
trace: normalised: ent 0 of 9667 differ; t508 7857 of 9667 differ (first f=273)
trace: 0 differing through 10013
moves: 9667 frames compared (f 130..) over c0 c1 r0 r1 s0_43, 43 without a capture snapshot
moves: 0 differing through 10013
```

The enforced compare stops at the first unexplained frame: it classifies 4139 frames from window start 96, reaches port 3549, and finds the same first unexplained capture frame 4248. The `--report` scan continues after that frame through port 4939, which is why its reported reach is not the enforced `GP_U13_MIN_REACH`. Capture 4248 is a large unexplained region (nearest port 3551, rows 0..96, x 0..319); it is not pinned as a scan-out or overlay and its cause is not claimed. The later unexplained frames, including 5891..5903 after the ending/script tail, are likewise outside the frame claim. No allowance was invented for them.

### Pins and their failing-direction proofs (`make gp-u13-oracle`, PASS at the final pins)

| pin | value | provenance | failing-direction proof |
|---|---:|---|---|
| `GP_U13_MIN_FIRST` | 4248 | First unexplained capture frame after the named loader overlay | 4249 → FAIL (`first unexplained 4248 < ratchet N 4249`) |
| `GP_U13_TRACE_MIN_FIRST` | 10014 | 0 trace differences through f=10013, the replay's X record | 10015 → FAIL (`N 10015 > end 10014: N is unreachable`) |
| `GP_U13_MAX_START` | 96 | Measured window starts at capture 96 (raw 1739) | 95 → FAIL (`window starts at capture 96 > pinned start 95`) |
| `GP_U13_MIN_REACH` | 3549 | Enforced compare reaches port 3549 before stopping at capture 4248 | 3550 → FAIL (`window reach port 3549 < pinned reach 3550`) |
| `GP_U13_CAPTURE_SHA256` | `e581916fa0a8e23a2ea52519aa0b358e4bc63084ce9defa0e1f733f3d12b69c6` | `poll.log` sha256 | first hex digit changed (`f581…`) → FAIL (capture identity mismatch) |
| `GP_U13_CAPTURE_FRAMES` | 5904 | Count of `frame_*.raw.gz` files | 5905 → FAIL (capture identity frame-count mismatch) |
| `GP_U13_MILESTONES` | 30 | `gp_win.py path`: all 30 rows reproduced | 31 → FAIL (`N 31 > end 30: N is unreachable`) |
| `GP_U13_WIN_MIN_FIRST` | 10014 | Win fields have 0 differences through f=10013 | 10015 → FAIL (`N 10015 > end 10014: N is unreachable`) |

Each failing-direction proof was run against the capture and replay dump with only that pin changed; the exact output is summarized in the table. The first target attempt exposed a measurement distinction: report-only continued to port 4939, while enforced comparison stopped at port 3549 at the first unexplained frame. The initial 4939 reach pin therefore failed; it was corrected to the enforced output and the oracle then passed. The final `make gp-u13-oracle` runs `tools.tests.test_gp_win` (24 tests, including Task 4's BLIZZARD label guard), `gp_win.py check`, the replay, the frame/trace compare, and the win-path/milestone ratchets.

**The narrow claim:** capture frames in the exhibited window below 4248 are explained (with the one byte-pinned loader overlay); the ending row proves `ad4=0,c0=1`, all 30 milestones and the script end at f=10013 are reached, and trace/win fields match through that end. The unexplained frame at 4248 is the pixel-claim limit; nothing at or after it is claimed correct. Port-frame order and complete interior coverage are not claimed.

## §J6 The `gp-u14-win-live` live-win capture — named gap (evidence closeout Task 9, R1)

The approved session had a two-attempt limit; nobody typed in the DOSBox-X window. The first `DISPLAY=:1 make gp-capture scenario=gp-u14-win-live` command was terminated by the shell tool's 200-second timeout before capture checks completed. The follow-up found no capture directory or files, and `python3 tools/gp_capture.py check-input data/k11-captures/gp-u14-win-live` failed with `no poll.log` (exit 2). This was a command timeout, not a completed scenario result.

The authorized retry used a 700-second command timeout for the scenario's 200-second `time_limit`. It ran 9,761 snapshots (`f 4..265D`, 57 missed frames), but the capture's own checks failed:

```
CHECK base: ok
CHECK steps fired 68/68: ok
CHECK end frame reached: FAIL
CHECK mode 0x27 after the Enter: ok
CHECK snapshots kb == raw (0 differ): ok
CHECK frames written 7287/7287: ok
CHECK port script v2 (the scenario end (X record) was not reached): FAIL
CHECK no unscripted input: ok
CHECK pokes written 0/0, 0 raced: ok
CHECK stopped at the end (SIGTERM at f=-, rc=0): FAIL
```

`gp_capture.py` reported 7,287 frames (raw 1386..14016), wall 200.9 s, and wrote the failed run to `data/k11-captures/gp-u14-win-live.failed` in the real checkout; it left the requested `gp-u14-win-live` directory as it was. The `check-input` command on that requested path again exited 2 with `no poll.log`. The failed artifact is preserved at `/tmp/pr_gp_failed/gp-u14-win-live-run2/`; the first timed-out attempt left no artifact. `data/k11-captures/gp-u14-win-live` is absent.

With no capture, `make gp-u14-oracle` passes the 24 `tools.tests.test_gp_win` tests and reports `no capture ... (skipped)` (exit 0). All eight `GP_U14_*` assignments remain empty; the target already guards its capture-dependent check/replay/compare/path commands on the capture directory, so no Makefile change was needed. There is no replay, live-KO/`b0c` evidence, milestone evidence, or comparison from which to measure a pin. No U14 pin or capture claim is possible.

**Named gap / narrow statement:** the U14 live win is not covered. Neither attempt completed the scenario's end checks; the retry's missing X record means it cannot establish a fought KO or the game's `b0c`. The U9/U10 poked-win oracles are not a substitute: their `DEATH_DONE` pokes do not prove a live U14 win, and no such claim is made for U14. No third attempt, weakened claim, or fitted pin is permitted under R1.

## §J7 The `gp-u15-join` capture (evidence closeout Task 5; measured on the closeout branch)

`data/k11-captures/gp-u15-join` (git-ignored): P2 joins the gp-idle-loss fight mid-round with F2 (`gp_session.SCENARIOS['gp-u15-join']`, 75 s limit, 4 injected steps, end 60 frames into the join's mode 0x17). Captured with `DISPLAY=:1 make gp-capture scenario=gp-u15-join`, nobody typing in the window.

### The capture's checks

```
snapshots 2208, f 4..8A9, 6 frames missed (spec §3.7)
CHECK base: ok                    CHECK steps fired 4/4: ok
CHECK end frame reached: ok       CHECK mode 0x27 after the Enter: ok
CHECK snapshots kb == raw (0 differ): ok     CHECK frames written 805/805: ok
CHECK port script v2: ok          CHECK no unscripted input: ok
CHECK pokes written 0/0, 0 raced: ok         CHECK stopped at the end (SIGTERM at f=08A9, rc=0): ok
```
`tools/gp_capture.py check-input data/k11-captures/gp-u15-join`: `check=ok no unscripted input`. `poll.log` sha256 `5f7ab1d72c7893e6fd66e8db143b615e6d3b385122ae0a93b9037ad1ffe3664c`, 805 frames (raw 1372..3920). The harness missed six game frames: `0x26E`, `0x270`, `0x652`, `0x653`, **`0x832`, `0x833`**.

### The replay's miss set

`make gp-replay scenario=gp-u15-join` first failed `4 != 2`; the unexpected rows were the two §G.24 wipe hooks, `0x29D60` (a bare `ret`) and `0x5D812` (the runtime stub), both from `frontend_mode_1b_step`, hits=1 each. They are the same pair as gp-u9-win and gp-u10-ending, not a target the replay must execute: `k_miss_gp_u15_join` in `port/tests/test_platform.c` pins them (with the base pair `0x5D812` actor_spawn 4396 hits and set_dead 4072: `distinct=4 dropped=0`), and its `k_gp_sets` row selects it by exact name. No new porting.

### The join evidence, and the R6 held-edge fallback

`python3 tools/gp_twop.py check --join --capture data/k11-captures/gp-u15-join`:
```
join f=834 (cred 4 -> 4); 58 S records from the join to X f=86D; side 0 [('pad', 58)], side 1 [('pad', 55), ('zero', 3)]; fight presses 0/0
mid-round join: mode 6 -> 17, b1f 1 -> 3, start press seen as the held-edge across missed frames
mid-round join: ok
```
The press rose in `0x832`/`0x833`, both unsampled, so the S records around it are:

| f | mode | raw | new | held | b1f |
|---|---|---|---|---|---|
| 0x831 | 6 | 0100 | 0 | 0 | 1 |
| 0x832, 0x833 | missed (the P record sees mode 0x17 at 0x833) | | | | |
| 0x834 | 0x17 | 0100 | 0 | 0100 | 3 |

`two_human(join_only=True)` first required `new & START_MASK[1]` at the join record (the first `b1f == 3`), which the missed frames hid (`new` is set for one game frame only). **Ruling R6** (the controller's amendment): when `new` has no bit, accept the joining side's start mask held in the join record **and** not held in the last S record before it (`start_edge == 'held'`); a mask already held before the join still fails ("not newly pressed at f=… (new …, held …; the record before it held …)"). The mode 6 → 0x17, `b1f` 1 → 3 and no-debit checks are unchanged. `tools/tests/test_gp_twop.py` covers the `new` pass (now also asserting `start_edge == 'new'`), the held-edge pass, the held-before failure and the no-bit failure; mutating away either clause of the held test fails a test.

**The narrow claim:** the join's start press is inferred from the held edge across two unsampled frames; the raw `new` bit was not sampled. The credit/`b1f`/mode evidence is sampled directly (`0x831` before, `0x834` after). Nothing past the capture's end (60 frames into the 0x78-frame window) is claimed.

### The measured report

`python3 tools/gp_compare.py --report --scenario gp-u15-join --capture data/k11-captures/gp-u15-join --port /tmp/pr_gp_dump/gp-u15-join --max-unexplained 0`:
```
frames: window from capture 90 (raw 1745); 707 classified: 492 clean, 214 splice, 1 transition, 0 band-allowed, 0 scan3-allowed, 0 overlay-allowed, 0 unexplained, 8 all-black
frames: coverage (reported, not ratcheted): 9 non-black port frame(s) up to port 590 not exhibited by any classified capture frame: [9, 11, 413, 414, 453, 454, 455, 456, 458]
frames: 0 unexplained through 804
trace: 1831 frames compared (f 141..), 6 without a capture snapshot; first tick difference f=141 (reported, not ratcheted)
trace: normalised (reported, not ratcheted): ent 0 of 1831 differ; t508 1495 of 1831 differ (first f=281)
trace: 0 differing through 2157
moves: 1831 frames compared (f 141..) over c0 c1 r0 r1 s0_43, 6 without a capture snapshot
moves: 0 differing through 2157
```
The replay has no first unexplained frame and no first trace/move difference: 805 is the end of the compared capture frames and the script's end was reached. This is the window bound, **not** a divergence ratchet, and not "how far the port got" short of the end.

### The pins and their proofs (`make gp-u15-oracle`, PASS at the pinned values)

| pin | value | provenance | proof |
|---|---|---|---|
| `GP_U15_MIN_FIRST` | 805 | `0 unexplained through 804`; gp_compare's exact pin | 806 → FAIL (`N 806 > end 805: N is unreachable`); 804 → passes (a lower bound is weaker, not wrong) |
| `GP_U15_TRACE_MIN_FIRST` | 2158 | `0 differing through 2157`; gp_compare's `N = 2158 is the exact pin` | 2159 → FAIL (`N 2159 > end 2158: N is unreachable`); 2157 and 2156 pass. **Deviation from R7:** the ruling named 2157 (the last clean frame), whose +1 does not fail; 2158 is the tool's exact pin and its +1 does fail |
| `GP_U15_MAX_START` | 90 | window from capture 90 | 89 → FAIL (`window starts at capture 90 > pinned start 89`) |
| `GP_U15_MIN_REACH` | 590 | `up to port 590` | 591 → FAIL (`window reach port 590 < pinned reach 591`); 589 → passes with "improved: raise the pin". **Deviation from R7:** the ruling named 805 (a capture frame count); the report's reach is a port frame, 590, and 805 fails (`590 < 805`) |
| `GP_U15_CAPTURE_FRAMES` | 805 | the harness's 805/805 frames written | 806 → FAIL (the identity line) |
| `GP_U15_CAPTURE_SHA256` | `5f7ab1d7…e3664c` | `poll.log` | a different sha (`6f7a…`) → FAIL (the identity line) |

Every FAIL was shown by `make gp-u15-oracle GP_U15_X=value` (the Makefile restored: the final default run exits 0). The full `make verify` was not run (the closeout runs the gate).

### What this section does not claim

It does not claim port-frame order or interior coverage inside the window (the nine unexhibited port frames above are reported, not ratcheted), the raw `new` start bit at the join (see the held-edge fallback), anything past the capture's end, or the join's mode-0x17 window beyond 60 frames; the capture does not move any existing pin.

## §J8 The `gp-u16-twop2` extended two-human capture (evidence closeout Task 8; measured on the closeout branch)

`data/k11-captures/gp-u16-twop2` (git-ignored) runs the U7 two-human start through the fight and the mode-0x09 result, attempts TALON's three finisher entries on the defeated P2, and ends at mode 0x12. The session was approved; nobody typed in the DOSBox-X window. Captured with `DISPLAY=:1 make gp-capture scenario=gp-u16-twop2` (240 s limit). The recorder printed ffmpeg broken-pipe shutdown messages, but DOSBox-X returned 0 and every capture check passed:

```
snapshots 8677, f 5..21F8, 15 frames missed (spec §3.7)
CHECK base: ok                    CHECK steps fired 96/96: ok
CHECK end frame reached: ok       CHECK mode 0x27 after the Enter: ok
CHECK snapshots kb == raw (0 differ): ok     CHECK frames written 8091/8091: ok
CHECK port script v2: ok          CHECK no unscripted input: ok
CHECK pokes written 0/0, 0 raced: ok         CHECK stopped at the end (SIGTERM at f=21F8, rc=0): ok
```

`python3 tools/gp_capture.py check-input data/k11-captures/gp-u16-twop2` prints `check=ok no unscripted input`. The capture spans raw video frames 1379..11618, has 8091 `frame_*.raw.gz` files and pins `poll.log` sha256 `5d38ca1fc3a52bcd02d91df3d7f0ae94a0275f9531f8bd81ffacee5343d31513`.

### The replay's miss set

The first replay attempt exceeded the command's 120 s tool limit; it was rerun with a 600 s limit and completed. `make gp-replay scenario=gp-u16-twop2` first rejected the new scenario with `4 != 2`. The full replay's exact log is:

```
fn-miss PR_GP_DUMP 0x5D812 actor_spawn hits=3796
fn-miss PR_GP_DUMP 0x5D812 set_dead hits=3422
fn-miss PR_GP_DUMP 0x29D60 frontend_mode_1b_step hits=1
fn-miss PR_GP_DUMP 0x5D812 frontend_mode_1b_step hits=1
fn-miss PR_GP_DUMP distinct=4 dropped=0
```

The scenario adds only `0x29D60` (`frontend_mode_1b_step`, the bare `ret`) and `0x5D812` (`frontend_mode_1b_step`, the runtime stub), the already-classified §G.24 wipe hooks; these are not unported targets the replay must execute. `k_miss_gp_u16_twop2` and the exact-name `k_gp_sets` row in `port/tests/test_platform.c` pin exactly those two pairs. The base `actor_spawn`/`set_dead` rows remain the known runtime stub pair. The rebuild and full replay then pass (`0 restarts`, `distinct=4 dropped=0`). No new porting was needed.

### Two-human evidence and the finisher gap (R2)

`python3 tools/gp_twop.py check --finisher --capture data/k11-captures/gp-u16-twop2`:

```
gp_twop: gp-u16-twop2: join f=2CA (cred 4 -> 4); 7911 S records from the join to X f=21BC; side 0 [('pad', 7911)], side 1 [('pad', 7911)]; fight presses 368/40
gp_twop: gp-u16-twop2: two-human match: ok
gp_twop: gp-u16-twop2: finisher: FAIL; mode 0x09 reached, mode 0x12 reached, reactions []
gp_twop: gp-u16-twop2: FAIL: no finisher reaction 0x32..0x34 in any S record
```

The capture proves two pad-controlled sides through the replay end, including the no-debit join (`cred 4 -> 4`); both required modes are reached. Although the three finisher entry sequences were attempted, no sampled `r0`/`r1` value is in `0x32..0x34`. No narrower retry was made and no finisher pin was fabricated. **R2: finisher application remains a named gap.** Consequently `gp-u16-oracle` calls the ordinary `gp_twop.py check` (two-human claim only), not `--finisher`; its enforced claims are the two-human evidence and the narrow frame/trace/moves ratchets below.

### The measured pixel allowances and report

The first `gp_compare --report` exposed capture 4089 (raw 6792), 24 unexplained frames, and the loader strip at 8066. These were not silently moved past: each was checked against the byte recipes already implemented by the judge, and each now has a scenario-specific, independently measured entry in `tools/gp_compare.py`.

* **`SCAN3_ALLOWED['gp-u16-twop2']`:** capture 4089 (raw 6792), recipe `(4089, 1, 66, 8eeaad13ee80ed904df9a8d600d3972f34959d6d5a0692cdbd319003b80376f6)`. The capture's row 0 hash is pinned; the unique adjacent pair is port frames 3424/3425, binding rows 1..65 and 66..199 respectively. Row 0 has 314 earlier port holders, so the exact named limitation is that its holder is not unique; no more of that old-screen state is asserted. The scan-out recipe's port bindings and capture hash are mutation-tested by `Scan3Allowance.test_a_mutated_bound_pair_frame_unexplains_it_again` and `test_different_top_bytes_are_not_the_named_frame`; `test_the_table_names_the_measured_scanouts` pins the U16 table entry.
* **`OVERLAY_ALLOWED['gp-u16-twop2']`:** capture 8066 (raw 11502), recipe `(8066, 6815, 6814, 6816, de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced)`. The 166 glyph pixels (rows 192..197, x 0..85) come from port 6815; port 6814 supplies the mask, and every off-mask pixel is bound to port 6816. The whole capture frame passes `overlay_explain`. Its screen, overlay pixels, and mask-source mutations are covered by `OverlayAllowance.test_a_mutated_screen_frame_unexplains_it_again`, `test_a_mutated_overlay_frame_unexplains_it_again` and `test_a_mutated_mask_source_unexplains_it_again`; `test_the_table_names_the_measured_overlays` pins the U16 entry. No general class or other allowance was added.

The final report command is `python3 tools/gp_compare.py --report --scenario gp-u16-twop2 --capture data/k11-captures/gp-u16-twop2 --port /tmp/pr_gp_dump/gp-u16-twop2 --max-unexplained 0` (report-only, exit 0). Its measured lines:

```
frames: window from capture 100 (raw 1743); 7958 classified: 5128 clean, 2823 splice, 4 transition, 0 band-allowed, 1 scan3-allowed, 1 overlay-allowed, 1 unexplained, 12 all-black
frames: FIRST UNEXPLAINED capture 8069 (raw 11594): nearest port 6816, rows 73..173, x 56..255 (6391 px)
frames: coverage (reported, not ratcheted): 27 non-black port frame(s) up to port 6817 not exhibited by any classified capture frame: [9, 11, 176, 217, 218, 219, 221, 222, 3334, 3423, 3713, 5420, 5447, 5454, 5458, 5845, 5858, 5928, 5953, 5962] ...
trace: 8308 frames compared (f 13A..), 15 without a capture snapshot; first tick difference f=13B (reported, not ratcheted)
trace: normalised (reported, not ratcheted): ent 0 of 8308 differ; t508 4267 of 8308 differ (first f=27B)
trace: 0 differing through 8636
moves: 8308 frames compared (f 13A..) over c0 c1 r0 r1 s0_43, 15 without a capture snapshot
moves: 0 differing through 8636
```

The capture's S records carry all five move fields `c0 c1 r0 r1 s0_43`; the report compares them and finds no differences through f=8636, so `GP_U16_MOVES_MIN_FIRST` is pinned to the compared end + 1 (8637), as is `GP_U16_TRACE_MIN_FIRST`. The tick difference at f=13B is reported separately by `gp_compare`, not part of the trace ratchet. The first unexplained pixel frame is 8069, after both measured allowances; its 6391-pixel mismatch and the subsequent 8070..8090 tail are not byte-pinned and receive no allowance. The claim stops before 8069. The replay reaches its X record at f=21BC; its dump's highest frame exhibited before that first unexplained capture frame is port 6817.

### Pins and failing-direction proofs (`make gp-u16-oracle`, PASS)

`make gp-u16-oracle GP_WIN_KEEP=1` passes the 25 `gp_twop` tests, two-human check, replay, capture identity, frame/trace/moves ratchets. `GP_WIN_KEEP=1` only preserves the `/tmp` dump for pin proofs; the default target removes it. The values and actual failing-direction checks (each run used the retained port dump with the other pins at their defaults):

| Pin | Value | Provenance | Failing direction, observed output |
|---|---:|---|---|
| `GP_U16_MIN_FIRST` | 8069 | First unexplained capture frame after allowances | 8070 → FAIL: `first unexplained 8069 < ratchet N 8070` |
| `GP_U16_TRACE_MIN_FIRST` | 8637 | `0 differing through 8636`; compared end + 1 | 8638 → FAIL: `N 8638 > end 8637: N is unreachable` |
| `GP_U16_MOVES_MIN_FIRST` | 8637 | `moves: 0 differing through 8636`; compared end + 1 | 8638 → FAIL: `N 8638 > end 8637: N is unreachable` |
| `GP_U16_MAX_START` | 100 | Report window begins at capture 100 | 99 → FAIL: `window starts at capture 100 (raw 1743) > pinned start 99` |
| `GP_U16_MIN_REACH` | 6817 | Highest port frame exhibited before capture 8069 | 6818 → FAIL: `window reach port 6817 < pinned reach 6818` |
| `GP_U16_CAPTURE_FRAMES` | 8091 | Count of `frame_*.raw.gz` files | 8092 → FAIL: capture identity reports 8091 frames != pinned 8092 |
| `GP_U16_CAPTURE_SHA256` | `5d38ca1f…d31513` | `poll.log` sha256 | all-zero sha → FAIL: actual hash != pinned hash |

The pin proofs were direct `gp_compare.py` invocations against the retained, measured replay, not assumed failures; the exit status was 1 for each modified pin. Makefile values were not changed for those checks. The final claim is narrow: the two-human evidence is sampled, no content-bearing capture frame in the exhibited window below 8069 is unexplained (including only the two individually byte-pinned allowances), and the replay reaches the scripted end. Frame order and complete interior coverage are not claimed; the 27 reported unexhibited port frames are not ratcheted. The mode-0x09/0x12 states are evidenced, but the finisher reactions and all pixel content from capture 8069 onward are not claimed. Full `make verify` was not run per the task brief.

## §J10 Closeout index and remeasurement (Task 10, 2026-10-22)

Capture index: §J4 is U12 ENDURANCE (team pass and modes 0x30/0x31); §J5 is U13 BLIZZARD's ending (30/30 milestones, pixel claim stops at capture 4248); §J6 is the U14 live-win R1 named gap (no completed capture, no pin); §J7 is U15's mid-round join; §J8 is U16's extended two-human capture (R2 finisher named gap). §J9 records the raw joystick verdict.

Task 10 re-ran each available oracle and a report-only replay using `make gp-report scenario=<scenario> GP_REPORT_MAX=0`. All four reports exited 0 and reproduced their recorded comparison results (U15 has no unexplained frame within its compared end); the oracle pins and capture identities also matched. No pin or oracle line was changed:

| Scenario | Oracle | Report-only measurement |
|---|---|---|
| `gp-u15-join` | PASS; `MIN_FIRST=805`, `TRACE_MIN_FIRST=2158`, `MAX_START=90`, `MIN_REACH=590`, 805 frames and pinned poll hash | `0 unexplained through 804`; trace and moves `0 differing through 2157`; window starts at 90, reaches port 590 |
| `gp-u12-endurance` | PASS; `MIN_FIRST=732`, `TRACE_MIN_FIRST=1931`, `MAX_START=100`, `MIN_REACH=490`, 772 frames and pinned poll hash | first unexplained capture 732; trace and moves `0 differing through 1930`; report enumerates the post-script tail 732–771, outside the claim |
| `gp-u13-ending2` | PASS; `MIN_FIRST=4248`, `TRACE_MIN_FIRST=10014`, `WIN_MIN_FIRST=10014`, `MAX_START=96`, `MIN_REACH=3549`, 30 milestones and 5904 frames | first unexplained capture 4248; trace/moves/win fields agree through f=10013; later pixels are not claimed |
| `gp-u16-twop2` | PASS; `MIN_FIRST=8069`, trace/moves `8637`, `MAX_START=100`, `MIN_REACH=6817`, 8091 frames and pinned poll hash | first unexplained capture 8069; trace/moves agree through f=8636; finisher effect remains unproved |

`make gp-u14-oracle` ran its 24 tests and skipped because `data/k11-captures/gp-u14-win-live` is absent; all eight `GP_U14_*` pins remain empty, consistent with §J6. The report passes are remeasurements, not broader claims: U12's post-script tail, U13 from frame 4248 onward, U16's finisher and tail, and U14's live win remain uncovered as stated above.
