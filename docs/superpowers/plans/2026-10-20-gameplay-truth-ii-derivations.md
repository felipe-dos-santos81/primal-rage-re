# Gameplay Truth II — derivation record

Plan: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii.md` (Task 2).
Spec: `docs/superpowers/specs/2026-10-20-gameplay-truth-ii-design.md` §W1.
All addresses are linear (Ghidra) addresses. §W1 is a report pass: it measures every
unexplained run of the U10 capture from 4113 to the capture's last frame and decides no
class and no pin (Task 3 owns the first run's class).

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

### Run 1 — capture 4113..4134: candidate A/B/C, unresolved

The 22 frames are the poked final's death animation. Capture 4113 (raw 7858) is at
`f = 0x1502` (its S record `ms=112273 f=1502 mode=000D ... rng=39F4CD40 cred=00000004`,
`poll.log:5416`), the same f as port 3444 (`frames.txt`: `03444 f=1502 tick=000015B2
mode=000D`); the port's T record at `f = 0x1502` has the same mode, state, rng and
credit (`trace.txt`; the trace claim reports `0 differing through 9953`).

Measured on the frame bytes (not a pin; `gp_compare`'s loaders over the whole dump):

- capture 4113's rows 0..165 are byte-equal to port 3444 and to **no other** frame of
  the 4664-frame dump;
- capture 4113's rows 166..199 are byte-equal to **no** frame of the dump (likewise
  4114 rows 168..199 and 4134 rows 138..199), so the run is not a splice of any two
  dumped port frames;
- the box moves from rows 166..199 (9732 px) at 4113 to rows 138..186 (1209 px) at
  4134; 4123 is the run's largest frame (rows 27..199, 47747 px) with its bottom rows
  185..199 equal to port 3454, a scene flash inside the run.

The port frames 3444..3463 are `f = 0x1502..0x1515`, modes `0x0D/0x0C`; the alternating
pokes `0x104B0C = 1` / `0x10789E = 0x78` fire at `f = 0x14CB..0x1543` (the same
`0x10789E` also at `f = 0x14BF`; below), so the run is inside the poked path.
**Candidate: A/B/C unresolved.** The no-holder measurement is evidence for A (the state
and the top of the screen agree, the death-animation band does not, and the trace cannot
see the death streams' per-hit effects). B must be argued from the pokes: Task 3 has to
quote the W records and the raw path the un-poked final takes, and show the capture's
band is a poked-only state (the replay applies the same pokes at `W.f + 1`, the U9/U10
record's §W.8, so the port is in the capture's poked state). C is open only through the
one-frame-per-f named gap (the U11 record's §K.10 item 2): no dumped port frame holds
the band, so the existing `SCAN3_ALLOWED`/`OVERLAY_ALLOWED` recipe (which binds the
band to a port frame) cannot express it. Task 3's evidence list: the 22 capture frames'
band bytes against the port's presents around 3444..3463; the S/T records of
`f = 0x1502..0x1515`; the W records below; `0x37EA0` (the death-animation end), the
`0x37DD4`/`0x29C78` streams (first `f = 0x14FB` mode `0xC` and `f = 0x14FE` mode `0xD`,
the U9/U10 record's §W.16) and the mode-`0xD`/`0xF` difference (that section's
`0x3DA50`/`0x475EC` first reach mode `0xF`).

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
before. **Candidate: none of A/B/C — how far the port got, not a divergence** (review
focus 3). Task 3 needs nothing here; a later task that wants these frames compared must
extend the replay's `end` past `0x26E1` (the capture's tail is 60 iterations) and treat
the movie pacing as its own claim (`smk-oracle` already compares twi5/twg in isolation).

### Run 3 — capture 5648..5688: none of A/B/C, the second boot logo

Same post-script tail (the X and `tail=60` above). The 41 frames are `twg.smk` (the TWG
logo; 5648's first box rows 163..199 x 112..192, 766 px, is the logo assembling;
5660..5688's 57513 px is the settled logo against the port's black boot frame).
**Candidate: none of A/B/C — the same script end.** Task 3 needs nothing; the same
`end` extension covers it.

### Run 4 — capture 5690..5703: none of A/B/C, the attract title

Same post-script tail. 5690 is the 166 px `CREDITS: 5` band (rows 7..12, x 130..196) on
black; 5691..5703 is the waterfall attract title with `CREDITS: 5` (32400..55514 px
against the port's boot frame). **Candidate: none of A/B/C — the same script end.** Task 3
needs nothing; these screens belong to the front-end/smk oracles, not to a U10 tail pin.

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

No class, no pin, no allowance and no named gap is decided here; the A/B/C decision, a
pin move or a named gap belongs to Task 3. Runs 2..4 are the capture's post-script tail
(60 iterations after the X), not port behaviour past the replay's end. The 287
unexplained frames are the exact count for this dump and this capture at 60c99a51: a
replay that changes the presents moves it, and the list must be re-measured then.
