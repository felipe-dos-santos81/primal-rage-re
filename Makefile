# Makefile for Primal Rage (DOS, 1995) — reverse engineering + SDL3 port
# Two pipelines:
#   RE   info → gra → symbols → cluster → decompile → analyse → oracle → original
#   PORT build → test → check → run → verify
SERVICE = Primal Rage (DOS)

# Variables
PORT_DIR = port
BUILD_DIR = build
GAME_DIR = data/game/C
RUNNER = data/game/run-window.sh
SMK_CAPTURES = data/smk-captures
SMK_DUMP = /tmp/pr_smk_dump
TITLE_CAPTURES = data/title-captures
TITLE_DUMP = /tmp/pr_title_dump
ATTRACT_DUMP = /tmp/pr_attract_dump
FRONTEND_DUMP = /tmp/pr_frontend_dump
K11_CAPTURES = data/k11-captures
K11_DUMP = /tmp/pr_k11_dump
GP_DUMP = /tmp/pr_gp_dump
DIFF_IMAGE ?= /tmp/pr_diff_image.bin
DIFF_TABLE ?= /tmp/pr_diff_table.md
E2_IMAGE ?= /tmp/pr_e2_image.bin
E2_TABLE = docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
E2_LIVE = docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt
scenario ?= walk
K11_ARGS ?=
TITLE_PIN_DIR = /tmp/pr_title_pin
DECOMP_DIR = port/decomp
SCRIPTS_DIR = _tools/ghidra_scripts
PROJ_DIR = _tools/ghidra_proj
PROJECT_NAME = prage
PYTHON = python3

# Ghidra needs JDK 25 (12.2 refuses older runtimes).
JAVA_HOME_DIR ?= /Library/Java/JavaVirtualMachines/temurin-25.jdk/Contents/Home
GHIDRA_HOME ?= $(HOME)/ghidra_12.2_DEV
HEADLESS = $(GHIDRA_HOME)/support/analyzeHeadless
GHIDRA_ENV = JAVA_HOME=$(JAVA_HOME_DIR)

# Optional args: make check frames=120 / make re-render gra=S16CAGE.GRA chunk=1
frames ?= 60
# Boot enters state 0 (attract, ~690 frames) before the title, so a verify run
# must cross the attract to exercise the attract/title sample and music
# assertions. The short `frames` default still drives `make check`'s
# attract-only smoke render.
# Coupling: main.c's CHECK_TITLE_REACH_FRAMES (700) makes any --check run at or
# above it FAIL if the title was not reached. So verify_frames must stay above
# the attract length; if a future attract grows past 700, raise both the attract
# bound and verify_frames together or `make verify` fails loudly.
verify_frames ?= 820
gra ?= S16TITLE.GRA
chunk ?= 0

.PHONY: help deps build test verify check smk-oracle run clean \
        re-info re-gra re-render re-symbols re-cluster re-extract re-extract-test \
        re-decompile re-analyze re-oracle re-original title-pin title-capture \
        title-oracle attract-oracle frontend-capture frontend-oracle demo-oracle demo-fight-oracle \
        attract2-oracle attract2-compare k11-capture k11-oracle k11-report gp-capture gp-replay gp-oracle gp-report diff-verify gp-charsel-oracle \
        entry-triage \
        gp-moves-oracle gp-keys-oracle gp-twop-oracle
.PHONY: gp-modes-oracle gp-modes-one

# ── Environment ──────────────────────────────────────────────────────────────

help: ## Print this help message
	@printf '\033[01;32m${SERVICE} — reverse engineering + SDL3 port\033[00;37m\n\n'
	@printf "\033[33mUsage:\033[0m\n  make [target] [arg=\"val\"...]\n\n\033[33mTargets:\033[0m\n"
	@grep -E '^[-a-zA-Z0-9_\.\/]+:.*?## .*$$' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*?## "}; \
		{printf "  \033[36m%-26s\033[0m %s\n", $$1, $$2}'

deps: ## Check the toolchain and the (untracked) game assets
	@echo "$(SERVICE) — toolchain"
	@for t in cmake $(PYTHON) git; do \
		command -v $$t >/dev/null 2>&1 || { echo "missing: $$t"; exit 1; }; \
	done
	@pkg-config --exists sdl3 2>/dev/null || brew list --versions sdl3 >/dev/null 2>&1 || \
		echo "warning: SDL3 not visible to pkg-config/brew — CMake needs its config package"
	@[ -x "$(HEADLESS)" ] || \
		echo "warning: Ghidra analyzeHeadless not at $(HEADLESS) (only the re-* targets need it)"
	@[ -d "$(JAVA_HOME_DIR)" ] || \
		echo "warning: JDK 25 not at $(JAVA_HOME_DIR) — override with JAVA_HOME_DIR=..."
	@command -v dosbox-x >/dev/null 2>&1 || \
		echo "warning: dosbox-x not found (only re-original needs it)"
	@[ -f "$(GAME_DIR)/PRAGE.EXE" ] || \
		echo "warning: $(GAME_DIR)/PRAGE.EXE missing — assets are deliberately untracked"
	@echo "ok"

# ── Port · build and verify ──────────────────────────────────────────────────

# `cmake --build` recompiles edited sources, including port/tests/*.c, and
# `verify`/every oracle target depends on this one — so none can run stale.
build: ## Configure and build the SDL3 port (CMake → build/)
	@echo "Configuring $(PORT_DIR)/ ..."
	cmake -S $(PORT_DIR) -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

# The oracle fixtures are git-ignored, so the suite SKIPS those checks unless
# asked. Pass oracle=1 (or use `verify`); pass nothing otherwise — the test
# treats even an empty PR_ORACLE_REQUIRED as "required".
test: build ## Run the assertion suite (oracle=1 requires the byte-exact oracles)
	@if [ -n "$(oracle)" ]; then \
		PR_ORACLE_REQUIRED=1 PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	fi

check: build ## Run N frames headless, writing frames/frame_*.ppm/.pal/.idx (frames=60)
	@echo "Running $(frames) frames headless ..."
	./$(BUILD_DIR)/prageport --game-dir $(GAME_DIR) --check $(frames)
	@ls -1 frames/frame_*.ppm 2>/dev/null | head -3
	@echo "The reference that is apples-to-apples is the Python decoder on the same GRA frames:"
	@echo "  python3 tools/gra_render.py data/game/C/S16TITLE.GRA 2 out.ppm --frame N --indices out.idx"
	@echo "An emulator cannot drive the port's chosen full-screen title frames (Task 15 report)."

# Smacker frame oracle: the test dumps every decoded frame as 320x200 RGB24
# (PR_SMK_DUMP), then smk_compare.py checks them pixel-exact against the capture.
# Captures are git-ignored; absent capture skips (or fails under
# PR_ORACLE_REQUIRED=1, which smk_compare.py enforces itself).
smk-oracle: build ## Pixel-exact Smacker frame oracle (skips without data/smk-captures)
	@echo "== smacker frame oracle (pixel-exact) =="
	@if [ -d $(SMK_CAPTURES)/twi5 ] || [ -d $(SMK_CAPTURES)/twg ]; then \
		rm -rf $(SMK_DUMP); \
		PR_SMK_DUMP=$(SMK_DUMP) PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "smk-oracle: no capture at $(SMK_CAPTURES)/, frames not compared"; \
	fi
	@$(PYTHON) tools/smk_compare.py --capture $(SMK_CAPTURES)/twi5 --port $(SMK_DUMP)/twi5 --frames 120
	@$(PYTHON) tools/smk_compare.py --capture $(SMK_CAPTURES)/twg --port $(SMK_DUMP)/twg --frames 41

# Title frame oracle: the PR_TITLE_DUMP driver runs game_init() and the 96-frame
# pinned title window headless, one RGB24 frame per presented frame;
# title_compare.py aligns the dump into each capture by content and requires a
# zero-byte match. Two captures prove determinism (the second is optional). The
# driver runs alone, since game_init() may run once per process.
title-oracle: build ## Pixel-exact title oracle (skips without data/title-captures)
	@echo "== title oracle (pixel-exact, presented window) =="
	@if [ -d $(TITLE_CAPTURES)/title ]; then \
		rm -rf $(TITLE_DUMP); \
		PR_TITLE_DUMP=$(TITLE_DUMP) PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "title-oracle: no capture at $(TITLE_CAPTURES)/, frames not compared"; \
	fi
	@$(PYTHON) tools/title_compare.py --capture $(TITLE_CAPTURES)/title \
		$(if $(wildcard $(TITLE_CAPTURES)/title2),--capture $(TITLE_CAPTURES)/title2,) \
		--port $(TITLE_DUMP)/title

# Attract-prefix pixel oracle: the PR_ATTRACT_DUMP run dumps every presented
# state-0 frame plus the post-attract title window; attract_compare.py derives
# the boundary from the title frames, checks every capture frame in the matched
# prefix, and (with --expect-first 215) requires the FIRST divergence to be the
# last attract capture frame (raw 2180 on `title`, 2175 on `title2`) — so a
# regression anywhere in frames 0..214 fails. Captures are git-ignored; an
# absent capture skips (or fails under PR_ORACLE_REQUIRED=1).
attract-oracle: build ## Pixel-exact attract-prefix oracle (skips without data/title-captures)
	@echo "== attract prefix oracle (pixel-exact) =="
	@if [ -d $(TITLE_CAPTURES)/title ]; then \
		rm -rf $(ATTRACT_DUMP); \
		PR_ATTRACT_DUMP=$(ATTRACT_DUMP) PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "attract-oracle: no capture at $(TITLE_CAPTURES)/, frames not compared"; \
	fi
	@$(PYTHON) tools/attract_compare.py --capture $(TITLE_CAPTURES)/title \
		$(if $(wildcard $(TITLE_CAPTURES)/title2),--capture $(TITLE_CAPTURES)/title2,) \
		--expect-first 215 --port $(ATTRACT_DUMP)

# Front-end oracle (the enforced one): the PR_FRONTEND_DUMP driver drives state
# 2 into states 3/4, one RGB24 frame per presented frame plus a per-frame hash
# log. Two gates stand: the PR_FRONTEND_DET determinism check (run_tests
# re-invokes itself twice and requires the two hash logs byte-identical — the
# whole first recipe line, so a failure fails the ladder), and the pixel
# comparison, which exits non-zero on any unexplained frame except the two named
# in tools/title_compare.py's FRONTEND_ALLOWED_UNEXPLAINED. All-black capture
# frames are dropped as documented artifacts (port/spec/game_flow.md); a
# content-bearing frame that disagrees still fails. The window is derived from
# the port's own dump, so the gate cannot detect an under-rendering port.
# Captures are git-ignored; an absent capture skips both.
frontend-oracle: build ## Front-end oracle (states 3/4; skips without data/title-captures/frontend)
	@echo "== front-end oracle (pixel-exact, states 3/4) =="
	@if [ -d $(TITLE_CAPTURES)/frontend ]; then \
		rm -rf $(FRONTEND_DUMP); \
		PR_FRONTEND_DET=$(FRONTEND_DUMP) PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "frontend-oracle: no capture at $(TITLE_CAPTURES)/frontend, frames not compared"; \
	fi
	@$(PYTHON) tools/title_compare.py --frontend --capture $(TITLE_CAPTURES)/frontend \
		--port $(FRONTEND_DUMP)/run1

# Demo window report: the same PR_FRONTEND_DUMP run as frontend-oracle (its frame
# count covers the state-6 entry and the 900-frame state-7 demo). title_compare.py
# --demo locates the front-end window with the same content alignment, then
# classifies the capture region after it against the port frames after the last
# frame that window exhibits — the same clean/splice/transition/unexplained model
# — and reports the window, the counts and the first unexplained frame. It is
# report-only (always exits 0) and is NOT in verify's sequence. Captures are
# git-ignored; an absent capture skips both lines.
demo-oracle: build ## Demo window report, states 9/6/7 (skips without data/title-captures/frontend)
	@echo "== demo window (report-only, states 9/6/7) =="
	@if [ -d $(TITLE_CAPTURES)/frontend ]; then \
		rm -rf $(FRONTEND_DUMP); \
		PR_FRONTEND_DET=$(FRONTEND_DUMP) PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "demo-oracle: no capture at $(TITLE_CAPTURES)/frontend, demo window not compared"; \
	fi
	@$(PYTHON) tools/title_compare.py --demo --capture $(TITLE_CAPTURES)/frontend \
		--port $(FRONTEND_DUMP)/run1

# Demo-fight oracle (a RATCHET, enforced in verify): the same dump and classification
# as demo-oracle, restricted to the fight window [fe_b+1 .. first all-black capture
# frame). Claim: no captured frame below DEMO_FIGHT_MIN_FIRST is unexplained, and the
# first unexplained frame is >= it. N = 1886 was measured on 8d538d5 (the commit that
# raised it, on 9abbdd0): 0x34E2C's reaction callback 0x3E3A8 (the T-rex's reaction
# 0x2A at f = 962: the 0xC8950 stream at hold 2.0, state 9/7/0 and the +0x0C callback
# 0x3E328), and the front-end driver's dump of loop frame 1970, the frame presented in
# the iteration that starts in state 7 and exits it (0x11BCC's timer exit drops the
# state to 0 inside it; the driver now credits a presented frame to the state its
# iteration started in, as the title/attract hooks do; 1382 frames, 0..1381). Together
# they explain captures 1881..1884: the front-end window reaches [560..1884], the
# frame before 1885, the capture's first all-black frame after the demo. That is the
# end of the demo-fight capture, so the fight window is empty and title_compare prints
# "fight window empty ... 0 unexplained in the fight window", "fully explained". N =
# 1886 = end + 1 is the exact pin: the claim is now "every content-bearing capture
# frame up to 1885 is explained". It can still fail: a shrink of the front-end window
# (which the front-end oracle, whose window is derived from the port's own dump,
# cannot see) reopens the fight window, and a frame there that the port frames after
# the window do not explain is below N. The capture holds no later fight, so N cannot
# rise further with this capture. Re-measured unchanged on ec8e132 (the slot +0x18
# hook 0x19020 with 0x3E484/0x3E1D0 and 0x18C14, record §35: the dump is
# byte-identical).
# (Before it, N = 1881, measured on bcce10b; N = 1763 on c7320b2; N = 1750 on 2287114;
# N = 1715 on 1268371; N = 1659 on cc38a38; N = 1563 on 27c95c0; N = 1546 on 219691e;
# N = 1481 on 3d64c61; N = 1478 on 4065c1d; N = 1411 on c78dc97; N = 1358 on b5a48a0;
# N = 998 on ff38dcc; N = 992 on 3fee8d0; N = 950 on a51685d; N = 892 on 6a48972; N =
# 891 on cfff063; N = 880 on 0a8346b; N = 870 on a76414d; N = 867 on 2137bce; N = 866
# on 7147288; N = 864 on b915712; N = 860 on afa47b3; N = 859 on b2cb490; N = 858 on
# 594e4b9; N = 851 on dad2712.)
# `make demo-oracle` now reports the region after the front-end window, [1885..3616],
# with no port frame left (the dump ends with the demo): first unexplained 1886, the
# capture's next cycle (the top-level dump ends with the demo; attract2-oracle below
# classifies that region against the cycle2/ dump). The tool also fails if N > window
# end + 1, or if the window collapses with an unexplained front-end frame outside the
# two allowed by name.
DEMO_FIGHT_MIN_FIRST = 1886
demo-fight-oracle: build ## Demo-fight ratchet, states 6/7 (skips without data/title-captures/frontend)
	@echo "== demo-fight oracle (ratchet on the first unexplained frame, N=$(DEMO_FIGHT_MIN_FIRST)) =="
	@if [ -d $(TITLE_CAPTURES)/frontend ]; then \
		rm -rf $(FRONTEND_DUMP); \
		PR_FRONTEND_DET=$(FRONTEND_DUMP) PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "demo-fight-oracle: no capture at $(TITLE_CAPTURES)/frontend, fight window not compared"; \
	fi
	@$(PYTHON) tools/title_compare.py --demo-fight --demo-fight-min-first $(DEMO_FIGHT_MIN_FIRST) \
		--capture $(TITLE_CAPTURES)/frontend --port $(FRONTEND_DUMP)/run1

# Attract cycle-2 oracle (a RATCHET, enforced in verify; roar-timing Task 26, record
# §36): the same PR_FRONTEND_DUMP run. After the demo's exit frame (loop 1970) the
# driver writes every presented frame to run1/cycle2 (loops 1971..4099, and inside
# loop 1971 the 166 screens the logo player 0x1C740 writes: its 0x52106 blanks and
# every TWI5/TWG frame), a separate dump so the front-end and demo-fight windows do
# not change. title_compare --attract2 classifies the capture from the first all-black
# frame after the front-end window (1885) to its end against those frames only. Claim:
# no captured frame below ATTRACT2_MIN_FIRST is unexplained, and the first unexplained
# frame is >= it. The driver also writes each `- LOADING -` screen the loader draws
# over the held frame inside an iteration (res.c's seam, record §45-A). Capture 2384,
# the `- LOADING -` frame before the second demo, was allowed by name (record §37)
# until those screens explained it; ATTRACT2_ALLOWED_UNEXPLAINED is now empty.
# N = 3617 (§48-A, branch frame-3593): the capture's end (3616) + 1, the exact pin,
# since no unexplained frame is left in the region (3545 stays allowed by name).
# At f = 4914 (loop 4027) the third demo's left fighter, character 4 (s16spi),
# takes reaction 0x25, whose callback 0x45AD0 (*(u32*)0xA4C0C) the port did not
# have: it curls into the spiked ball (stream 0xEB64E, state 9/7/1), its +0x18
# hook 0x459F4 finds the hit at f = 4917 and its +0x1C callback 0x45A34 knocks
# the tyrannosaur back (0x3B714) and uncurls it (0xEB692). Porting 0x45AD0,
# 0x45A70, 0x459F4, 0x45A34 and the stream target 0x459D0 explained 3593..3616.
# (Before it, N = 3593 (§47-A, branch frame-3545): FE_LOOPS 3900 -> 4100 brings the high-score
# screen's end (state 9 -> 6 after loop 3984) and the third demo (s16dia against
# s16spi in s16caves; its state-6 entry reads six files in loop 3985) into the dump.
# 3544 is now the port's own loader screen (3543 is all-black, excluded), 3546..3592
# its third demo fight, and 3545
# (raw 8338) is allowed by name as a three-frame splice (ATTRACT2_SPLICE3_ALLOWED,
# cycle-2 2192/2193/2194: the last loader screen, the load frame's present and the
# next, which the read's tick re-sync 0x1B45F/0x1B464 lets fall inside one capture
# scan). The driver lists the loader screens in cycle2/loader.txt for that check.
# 3593 (raw 8386) was the third demo's first fight frame the port did not match.)
# (Before it, N = 3545 was measured on 1251af7 (§46-A): the high-score tables 0x2DB58/0x2DBC4/
# 0x2DCA0, their boot fill 0x1E824 (0x1E918's ten factory records and the champion
# 0xA7D74) and the rest of 0x1EA08 (the rows through 0x2F4D0/0x2F4BC and the
# champion's figure 0x2AE14/0x2A17C) draw the attract's high-score screen and explain
# 3408..3542. 3543/3544 (black, `- LOADING -` on black) match earlier port screens,
# not the port's hand-off: the dump ends at loop 3899 inside the high-score screen
# (loops 3685..3899), about 85 ticks before the original leaves it. 3545 (raw 8338)
# is the third demo fight, past the driver's window. Before that, N = 3408, measured on b05adcc (on 54394e9; §45-A): the voice
# dispatcher 0x2C3FC with the sound module's sample path, 0x1543C's voice 0x4D
# (its first read of s16spisd.gra draws the loader's text over the game frame at
# loop 3557), the fighter spawns' sound-bank reads (0x33E51), the DIG driver
# handle DS_001028C8 (0x1CF8E) and the loader-screen seam explain capture 3257.
# 3408 is the attract's third cycle: the high-score table after the second demo,
# which the port did not draw.) The driver's loop (FE_LOOPS, a measurement window)
# runs to 4100 so that the capture's last frame lies inside the dump. It fails if
# an unexplained frame appears below N, N exceeds the capture's end + 1, or N is
# at or below the region's start.
# Like the front-end oracle, its window comes from the port's own dump, so it cannot
# detect an under-rendering port. (Before it, N = 3257, measured on b947895:
# character 3's reaction callbacks 0x15350 and 0x151C0 with their callbacks and
# stream targets, the process-table entry 0x2910C and the opcode-0x0C spawn's a5
# explained 3099..3256; before that N = 3099, measured on fcce893: the
# raptor's reaction stream's 0xD500 target 0x3C32C returned it to its stance at
# loop 3293 and explained 2950..3098; before that N = 2950, measured on ce5f295:
# character 3's reaction-0x23 callback 0x14E44 (the raptor's grab at loop 3116)
# and its +0x18 hook 0x14CC4 (the miss at loop 3133) explained 2763..2949;
# before that N = 2763, measured on c9875d1:
# 0x3B298's call to 0x1A734 (0x3B443), the ape's block restart at loop 3040,
# explained 2674..2762; before that N = 2674, measured on 9469a30: the
# raptor's block, 0x1AB5C's arm 0x1A7CC with 0x1A6AC/0x1A8F4 and the wired 0x1A640
# in the +0x52 == 6 handler 0x1A978, explained 2461..2673; before that N = 2461,
# measured on 5448e09: state 6's 0x34978 reset of the live-fighter count
# DS_001078FA explained 2386..2460; before that N = 2386, measured on c0edb4c and
# re-measured unchanged on a7ccc86 and 65f4084; before that N = 2384, measured on
# fc8e775.)
ATTRACT2_MIN_FIRST = 3617
attract2-oracle: build ## Attract cycle-2 ratchet after the demo (skips without data/title-captures/frontend)
	@echo "== attract cycle-2 oracle (ratchet on the first unexplained frame, N=$(ATTRACT2_MIN_FIRST)) =="
	@if [ -d $(TITLE_CAPTURES)/frontend ]; then \
		rm -rf $(FRONTEND_DUMP); \
		PR_FRONTEND_DET=$(FRONTEND_DUMP) PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "attract2-oracle: no capture at $(TITLE_CAPTURES)/frontend, cycle 2 not compared"; \
	fi
	@$(MAKE) --no-print-directory attract2-compare

# The --attract2 comparison alone, on the dump the last front-end run left (verify
# runs it right after demo-fight-oracle, which made that dump).
attract2-compare:
	@$(PYTHON) tools/title_compare.py --attract2 --attract2-min-first $(ATTRACT2_MIN_FIRST) \
		--capture $(TITLE_CAPTURES)/frontend --port $(FRONTEND_DUMP)/run1

# K11 service-menu capture (named-gaps A): the pinned original under DOSBox-X,
# driven into mode 0x27, with a live-RAM poll log. Writes only
# data/k11-captures/<scenario>/ (tools/k11_capture.py guards the path).
k11-capture: title-pin ## Capture the pinned original's service menu (scenario=walk|idle|menuesc|diags|de; writes data/k11-captures/)
	$(PYTHON) tools/k11_capture.py --scenario $(scenario) --out $(K11_CAPTURES)/$(scenario) --exe $(TITLE_PIN_DIR)/PRAGE.EXE $(K11_ARGS)

# K11 oracle (enforced in verify like the front-end oracle: it skips without
# the capture and fails on any mismatch with it; k11_compare.py ignores an
# inherited PR_ORACLE_REQUIRED and fails on an absent capture only with
# --required, which this target does not pass). The port script comes from the capture's poll log; the
# PR_K11_DUMP driver runs alone (game_init once per process). The second
# scenario, menuesc, is the soft restart (record named-gaps-b §B.12): the MAIN
# MENU Esc, the 0x2520B longjmp, the black frame and the reboot, compared
# byte-exact with the same narrow claims except that the capture may run on
# past the port's final screen (k11_compare K11_OPEN_END). idle stays in
# k11-report: its stock script ends 180 ticks after the Enter, before the
# 0x4B1-tick timeout.
# The menuesc window END must reach K11_OPEN_END['menuesc'] = 388 (tools/k11_compare.py): a
# measured value like the demo-fight N (measured at 14134c9); raise it if the window grows.
k11-oracle: build ## K11 oracles: the walk and the menuesc restart (each skips without its data/k11-captures/<scenario>)
	@echo "== K11 service-menu oracle (pixel-exact, the walk) =="
	@if [ -d $(K11_CAPTURES)/walk ]; then \
		rm -rf $(K11_DUMP)/walk; mkdir -p $(K11_DUMP); \
		$(PYTHON) tools/k11_session.py port-script --scenario walk --capture $(K11_CAPTURES)/walk --out $(K11_DUMP)/walk.script && \
		PR_K11_DUMP=$(K11_DUMP)/walk PR_K11_SCRIPT=$(K11_DUMP)/walk.script PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "k11-oracle: no capture at $(K11_CAPTURES)/walk, frames not compared"; \
	fi
	@$(PYTHON) tools/k11_compare.py --capture $(K11_CAPTURES)/walk --port $(K11_DUMP)/walk
	@echo "== K11 soft-restart oracle (pixel-exact, menuesc: the MAIN MENU Esc 0x2520B restart) =="
	@if [ -d $(K11_CAPTURES)/menuesc ]; then \
		rm -rf $(K11_DUMP)/menuesc; mkdir -p $(K11_DUMP); \
		$(PYTHON) tools/k11_session.py port-script --scenario menuesc --capture $(K11_CAPTURES)/menuesc --out $(K11_DUMP)/menuesc.script && \
		PR_K11_DUMP=$(K11_DUMP)/menuesc PR_K11_SCRIPT=$(K11_DUMP)/menuesc.script PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "k11-oracle: no capture at $(K11_CAPTURES)/menuesc, frames not compared"; \
	fi
	@$(PYTHON) tools/k11_compare.py --scenario menuesc --capture $(K11_CAPTURES)/menuesc --port $(K11_DUMP)/menuesc

# The evidence scenarios (G1/G2/G3): the same driver and comparison, report-only
# (menuesc is also enforced by k11-oracle).
k11-report: build ## Report-only K11 comparison of the evidence captures (idle, menuesc, diags, de)
	@for s in idle menuesc diags de; do \
		if [ -d $(K11_CAPTURES)/$$s ]; then \
			rm -rf $(K11_DUMP)/$$s; mkdir -p $(K11_DUMP); \
			$(PYTHON) tools/k11_session.py port-script --scenario $$s --capture $(K11_CAPTURES)/$$s --out $(K11_DUMP)/$$s.script && \
			PR_K11_DUMP=$(K11_DUMP)/$$s PR_K11_SCRIPT=$(K11_DUMP)/$$s.script PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
			$(PYTHON) tools/k11_compare.py --report --scenario $$s --capture $(K11_CAPTURES)/$$s --port $(K11_DUMP)/$$s; \
		else \
			echo "k11-report: no capture at $(K11_CAPTURES)/$$s"; \
		fi; \
	done

# Gameplay capture (spec 2026-09-30-gameplay-ground-truth-design.md §4.1): the
# pinned original under DOSBox-X with frame-keyed injection and a per-frame
# snapshot log. Writes only data/k11-captures/gp-<scenario>/ (gp_capture.guard_gp).
GP_ARGS ?=
gp-capture: title-pin ## Capture a gameplay scenario (scenario=gp-pads|gp-idle-loss; writes data/k11-captures/)
	$(PYTHON) tools/gp_capture.py --scenario $(scenario) --out $(K11_CAPTURES)/$(scenario) --exe $(TITLE_PIN_DIR)/PRAGE.EXE $(GP_ARGS)

# Gameplay replay (spec 2026-09-30-gameplay-ground-truth-design.md §4.2): the
# v2 port script from the capture's poll.log, then the PR_GP_DUMP driver alone
# (game_init once per process). An absent capture skips, or fails under
# PR_ORACLE_REQUIRED unless GP_OPTIONAL=1. make verify passes GP_OPTIONAL=1:
# a gp capture skips there like the K11 oracles (spec §4.3, record §G.11), so
# a checkout without data/k11-captures/gp-pads still passes verify.
GP_OPTIONAL ?=
GP_SCRIPT_ARGS ?=
gp-replay: build ## Replay a gameplay capture in the port (scenario=gp-…; dump in $(GP_DUMP)/<scenario>)
	@case "$(scenario)" in gp-?*) ;; *) \
		echo "usage: make gp-replay scenario=gp-<name> (got scenario=$(scenario))"; exit 2;; esac
	@if [ -d $(K11_CAPTURES)/$(scenario) ]; then \
		rm -rf $(GP_DUMP)/$(scenario); mkdir -p $(GP_DUMP); \
		$(PYTHON) tools/gp_session.py port-script --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) --out $(GP_DUMP)/$(scenario).script $(GP_SCRIPT_ARGS) && \
		PR_GP_DUMP=$(GP_DUMP)/$(scenario) PR_GP_SCRIPT=$(GP_DUMP)/$(scenario).script PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	elif [ -z "$(GP_OPTIONAL)" ] && [ -n "$${PR_ORACLE_REQUIRED+x}" ]; then \
		echo "gp-replay: no capture at $(K11_CAPTURES)/$(scenario) (PR_ORACLE_REQUIRED)"; exit 1; \
	else \
		echo "gp-replay: no capture at $(K11_CAPTURES)/$(scenario)"; \
	fi

# Gameplay oracle (spec 2026-09-30-gameplay-ground-truth-design.md §4.3): the
# capture against the port's replay, a frame ratchet (title_compare's explain
# model) and a trace ratchet (S against T by the frame counter). Enforced like
# the K11 oracle: it skips without the capture (gp-replay gets GP_OPTIONAL=1,
# so PR_ORACLE_REQUIRED does not fail it either, record §H.2 item 3) and fails
# on a broken claim or an unpinned N once the capture exists. Both claims are
# narrow (record §G.16): neither says the frames or state after the first
# unexplained/differing one are right. U4 pinned the values below from the
# measured first unexplained capture frame / first differing f / window start
# (MAX_START: the capture frame where the window begins; a later start fails, so a
# port regression cannot slide the window past the frame that set MIN_FIRST).
# All re-measured on main c6cac22 (U0's 40 functions) + U3 + U4 (record §G.24): unchanged
# from the first measurement on the pre-U0 base (MIN_FIRST re-pinned by U5 and U6a,
# TRACE_MIN_FIRST by U6a, below).
# MIN_FIRST: first unexplained capture frame 2064 (raw 5190), measured after U6a ported 0x3640C,
# 0x23208, 0x37DCC and 0x3A588 (record gameplay-u6 §U6.21): gp_compare --report prints "nearest
# port 1670, rows 0..97, x 0..319 (13832 px)". Port 1670 is f=0xC75, mode 8 (frames.txt); the
# capture enters mode 8 (game_mode_08_step 0x28468) at f=0xC71 (poll.log P record) from the
# round-1 fight (mode 6), and capture 2063 is port 1666 (f=0xC71) clean, so 2064 lies in mode 8
# between f=0xC71 and f=0xC75, on the fight screen. Every row of it equals the same row of a port
# frame: rows 0..2 equal in port 1665/1666 (f=0xC70/0xC71), rows 3..97 port 1669 (f=0xC74),
# rows 98..199 port 1670 (f=0xC75). A three-frame scan-out is outside the explain model (clean,
# a splice of two adjacent frames, one transition row), and port 1667/1668 (f=0xC72/0xC73)
# appear in no capture frame; the capture has no S snapshot for f=0xC71..0xC73 (as at several
# other mode changes, not all: record §U6.21). Why the original's scan-out shows three frames
# is not isolated: a named gap with that evidence.
# Provenance: U4 pinned 203; U5 raised it to 787 (record 2026-10-01-gameplay-u5 §C5.12/§C5.13:
# the 0x37A58 top wrap compares the frame byte rec+0x52, raw 0x37B03/0x37B08); 787 was the miss of
# the then-unregistered move callback 0x23208 (divergence 2, record §G.24), ported by U6a.
# Raise it when the frame claim improves (gp_compare prints "improved: raise N").
GP_IDLE_LOSS_MIN_FIRST = 2064
# TRACE_MIN_FIRST: no traced difference over the whole replay, port against the capture:
# gp_compare prints "0 differing through 8319 ... N = 8320 is the exact pin" (7973 frames compared,
# f = 0x141..0x207F, i.e. decimal 321..8319; 26 f without a capture snapshot are not compared).
# N = end + 1 = 8320 is the exact pin (gp_compare's ratchet: an N above the end fails as
# unreachable), so a traced difference at any compared f fails it. Raised from 2088 by U6a's
# four ports (record gameplay-u6 §U6.21); 2088 = 0x828 was the rng difference four frames after
# the miss of 0x23208 (record §G.24). No run-to-run bound (record §G.19).
GP_IDLE_LOSS_TRACE_MIN_FIRST = 8320
# MAX_START: the window begins at capture frame 90 (raw 1744), the first capture frame that
# shows the port's first frame (gp_compare prints "window from capture 90"); it must be < MIN_FIRST.
GP_IDLE_LOSS_MAX_START = 90
# The capture the three values belong to: the poll.log sha256 and the frame count of
# data/k11-captures/gp-idle-loss (record §G.18; run 1 of the two captures, the oracle reads this
# one). gp-oracle FAILS (never skips) on a present capture with another poll.log: a re-capture
# invalidates the pins above, because the pick time-out steps in 64-frame units of the absolute
# frame counter (f & 0x3F == 0, record §G.24), so a re-capture whose character select starts at
# f <= 0x280 moves everything after it by 64 frames. Re-measure all three, then re-pin these.
GP_IDLE_LOSS_CAPTURE_SHA256 = 773e264731ea83a23623c6ec6cc5547165628d8adcf96f5a7c99c1c2b88c8447
GP_IDLE_LOSS_CAPTURE_FRAMES = 8173
gp-oracle: build ## Gameplay oracle: gp-idle-loss frame and trace ratchets (skips without data/k11-captures/gp-idle-loss)
	@echo "== gameplay oracle: gp-idle-loss (frame and trace ratchets) =="
	@$(MAKE) --no-print-directory gp-replay scenario=gp-idle-loss GP_OPTIONAL=1
	@$(PYTHON) tools/gp_compare.py --scenario gp-idle-loss --capture $(K11_CAPTURES)/gp-idle-loss \
		--port $(GP_DUMP)/gp-idle-loss --min-first "$(GP_IDLE_LOSS_MIN_FIRST)" \
		--trace-min-first "$(GP_IDLE_LOSS_TRACE_MIN_FIRST)" --max-start "$(GP_IDLE_LOSS_MAX_START)" \
		--capture-sha256 "$(GP_IDLE_LOSS_CAPTURE_SHA256)" --capture-frames "$(GP_IDLE_LOSS_CAPTURE_FRAMES)"

# U5 character-select walk (record 2026-10-01-gameplay-u5 §C5.16-§C5.18): the gp-u5-charsel capture
# against its port replay, both ratchets, enforced like gp-oracle (skips without the capture,
# fails on a present capture with another poll.log). Values measured in U5 Task 7 (report lines in
# §C5.17), pinned by U5 Task 8 Step 1 (pins.sh); raise N/F when the claims improve.
# Provenance (U5 Task 7, record §C5.17): the report's FIRST UNEXPLAINED capture line names the frame after
# the port's last one. The port's script ends at the capture's mode-6 frame (f = 0x5E8 = 1512), while
# the capture's poll.log runs on to about f = 0x925 (round 1 until the 60 s limit); the port's last
# frame is byte-identical to the capture frame before N, so N is how far the port got, not a defect.
# The trace line reports no differing frame through the script's last f, so TRACE_MIN_FIRST is that
# f plus one, the exact pin (one more fails as unreachable).
GP_CHARSEL_MIN_FIRST = 516
GP_CHARSEL_TRACE_MIN_FIRST = 1513
GP_CHARSEL_MAX_START = 100
GP_CHARSEL_CAPTURE_SHA256 = 138fb537cd8c364bab0ccc68fc8fdd9079de19f3b0fc789d427f6d9652a18d7e
GP_CHARSEL_CAPTURE_FRAMES = 1464
gp-charsel-oracle: build ## Gameplay oracle: gp-u5-charsel frame and trace ratchets (skips without data/k11-captures/gp-u5-charsel)
	@echo "== gameplay oracle: gp-u5-charsel (frame and trace ratchets) =="
	@$(MAKE) --no-print-directory gp-replay scenario=gp-u5-charsel GP_OPTIONAL=1
	@$(PYTHON) tools/gp_compare.py --scenario gp-u5-charsel --capture $(K11_CAPTURES)/gp-u5-charsel \
		--port $(GP_DUMP)/gp-u5-charsel --min-first "$(GP_CHARSEL_MIN_FIRST)" \
		--trace-min-first "$(GP_CHARSEL_TRACE_MIN_FIRST)" --max-start "$(GP_CHARSEL_MAX_START)" \
		--capture-sha256 "$(GP_CHARSEL_CAPTURE_SHA256)" --capture-frames "$(GP_CHARSEL_CAPTURE_FRAMES)"

# Plan gameplay-u6b (record gameplay-u6 §U6.22): the gp-u6-moves-b capture (P1 Sauron's twelve
# scripted attempts in round 1, record §U6.12; 8 performed, all six distinct moves) against its
# port replay. gp-u6-moves-b is the clean re-capture: the first capture, gp-u6-moves, recorded
# unscripted keyboard input from f=0x9CB and is not pinned (record §U6.22). Three ratchets, each
# the measured first unexplained/differing item (raise it when it improves): the frames, the trace
# (gp_session.TRACE_FIELDS) and the moves claim (gp_session.MOVE_FIELDS: c0 c1 r0 r1 s0_43,
# record §U6.11); the window start; the capture identity (a re-capture fails: re-measure, then
# re-pin). Skips without data/k11-captures/gp-u6-moves-b, like gp-oracle.
# Provenance (U6b Task 15, `make gp-report scenario=gp-u6-moves-b` on 9fa9ce1 plus the test
# comment fold-ins; record §U6.22):
#   MIN_FIRST: "frames: FIRST UNEXPLAINED capture 1005 (raw 4113): nearest port 759, rows 6..26,
#     x 119..199 (27 px)" (the health-bar rows; consistent with the trace difference below).
#   TRACE_MIN_FIRST: "trace: first difference f=8D6 (2262) in s1_5a: capture 16, port 2D", six
#     frames after P1's 0x24 move at f=0x8D0; the replay has no unregistered callback left
#     (distinct=4), so the cause is not a miss and is not isolated (a named gap, record §U6.22).
#   MOVES_MIN_FIRST: "moves: first difference f=B85 (2949) in r1: capture 0, port 15".
#   MAX_START: "frames: window from capture 88 (raw 1741)".
#   CAPTURE_SHA256/FRAMES: data/k11-captures/gp-u6-moves-b/poll.log and its frame_*.raw.gz count
#     (U6b Task 6 re-run, re-checked on disk at Task 15).
GP_MOVES_MIN_FIRST = 1005
GP_MOVES_TRACE_MIN_FIRST = 2262
GP_MOVES_MOVES_MIN_FIRST = 2949
GP_MOVES_MAX_START = 88
GP_MOVES_CAPTURE_SHA256 = dcf242da915c41f4b4e381babdabdc30cd04ce89ed83e25f5178601e4878b2e3
GP_MOVES_CAPTURE_FRAMES = 2205
gp-moves-oracle: build ## Gameplay oracle: gp-u6-moves-b frame, trace and moves ratchets (skips without data/k11-captures/gp-u6-moves-b)
	@echo "== gameplay oracle: gp-u6-moves-b (frame, trace and moves ratchets) =="
	@$(MAKE) --no-print-directory gp-replay scenario=gp-u6-moves-b GP_OPTIONAL=1
	@$(PYTHON) tools/gp_compare.py --scenario gp-u6-moves-b --capture $(K11_CAPTURES)/gp-u6-moves-b \
		--port $(GP_DUMP)/gp-u6-moves-b --min-first "$(GP_MOVES_MIN_FIRST)" \
		--trace-min-first "$(GP_MOVES_TRACE_MIN_FIRST)" --max-start "$(GP_MOVES_MAX_START)" \
		--moves-min-first "$(GP_MOVES_MOVES_MIN_FIRST)" \
		--capture-sha256 "$(GP_MOVES_CAPTURE_SHA256)" --capture-frames "$(GP_MOVES_CAPTURE_FRAMES)"

# U11 in-match keys (plan 2026-10-01-gameplay-u11-in-match-keys.md, record
# 2026-10-01-gameplay-u11-derivations.md §K.6/§K.12): tools/gp_keys.py judges
# each key event of data/k11-captures/gp-keys-fight twice: `evidence` (the
# capture shows the raw-derived effect) and `effects` (the port's PR_GP_DUMP
# replay shows it at the same frames; a ratchet: the events before the first
# the port does not reproduce must number >= GP_KEYS_MIN_EFFECTS). Skips without
# the capture; with it, an unpinned value or another poll.log fails. Narrow: an
# event is judged on the latch, the pause bytes, the pad words, mode, b1f, cred
# and rng only; the pause/prompt frames are not compared (record §K.10).
# MIN_EFFECTS: the port reproduces the first 11 of the 11 events of record §K.6 on
# data/k11-captures/gp-keys-fight (Task 6, record §K.12: "effects: first not reproduced 11,
# ratchet N 0 ok (improved: raise N)"); raise it when gp_keys prints "improved: raise N".
# CAPTURE_SHA256: that capture's poll.log; another capture fails until it is re-measured
# (Task 5 Step 1) and both values re-pinned.
GP_KEYS_MIN_EFFECTS = 11
GP_KEYS_CAPTURE_SHA256 = 8425afbc46d51173532f6f4c27a8f16c594e2bc572b4273e056bdbf51a9678ae
gp-keys-oracle: build ## In-match keys: evidence + effects ratchet on gp-keys-fight (skips without data/k11-captures/gp-keys-fight)
	@echo "== in-match keys oracle: gp-keys-fight (evidence, effects ratchet) =="
	@$(MAKE) --no-print-directory gp-replay scenario=gp-keys-fight GP_OPTIONAL=1
	@$(PYTHON) tools/gp_keys.py evidence --capture $(K11_CAPTURES)/gp-keys-fight \
		--capture-sha256 "$(GP_KEYS_CAPTURE_SHA256)"
	@$(PYTHON) tools/gp_keys.py effects --capture $(K11_CAPTURES)/gp-keys-fight --port $(GP_DUMP)/gp-keys-fight \
		--min-effects "$(GP_KEYS_MIN_EFFECTS)" --capture-sha256 "$(GP_KEYS_CAPTURE_SHA256)"

# Gameplay U7 oracle (plan docs/superpowers/plans/2026-10-01-gameplay-u7-two-players.md, record
# docs/superpowers/plans/2026-10-01-gameplay-u7-derivations.md): data/k11-captures/gp-twop, LEFT
# PLAYER ARCADE with P2 joining in the character select and both sides pressing keys in a short
# fight. The tool tests always run; with the capture present it must first be a two-human match
# (tools/gp_twop.py check, record §T.1.6), then the frame, trace and moves ratchets as gp-moves-oracle's. Skips
# without the capture, even under PR_ORACLE_REQUIRED (spec §4.3). An empty pin with the capture
# present FAILS (gp_compare: "not pinned"). Pinned by U7 Task 6 (record §T.10) and the final review
# (the moves claim, below); all three ratchets are exact pins at the end of the port's script.
# MIN_FIRST: measured at 597c78c (record §T.10): first unexplained capture frame 612 (raw 3223),
# nearest port 463 = f=0x5E1, the port's last frame. 612 is how far the port got, not a divergence:
# the port's script ends at the capture's X record (f=0x5E1) and capture 612..679 are the capture's
# STOP_AT_END tail (the 60 game frames after X; the report lists the first 5) that the port never
# ran, like gp-u5-charsel's 516. Capture
# 83..611 are all explained (the START MENU, the P2 join, both cursors and confirms, the wipes, the
# versus screen, the fight to X). Raise it only if the port's script is lengthened.
GP_TWOP_MIN_FIRST = 612
# TRACE_MIN_FIRST: no traced difference over the whole replay (gp_compare: "0 differing through
# 1505", f = 0x134..0x5E1; 8 f without a capture snapshot are not compared). N = end + 1 = 1506 is
# the exact pin (an N above the end fails as unreachable), so a traced difference at any compared
# f fails it. No run-to-run bound (one capture only, Decision 3; record §G.19).
GP_TWOP_TRACE_MIN_FIRST = 1506
# MOVES_MIN_FIRST: the moves claim (gp_session.MOVE_FIELDS: c0 c1 r0 r1 s0_43, record gameplay-u6
# §U6.11) has no differing frame either: gp_compare prints "moves: 0 differing through 1505"
# (f = 0x134..0x5E1; 8 f without a snapshot not compared). N = end + 1 = 1506, the exact pin as
# TRACE_MIN_FIRST (1507 fails as unreachable), like gp-moves-oracle's. Pinned by the final review
# (record §T.10): the plan's "reported, not pinned" predates U6b's oracle.
GP_TWOP_MOVES_MIN_FIRST = 1506
# MAX_START: the window begins at capture frame 83 (raw 1742), the first capture frame that shows
# the port's first frame (gp_compare: "window from capture 83"); it must be < MIN_FIRST.
GP_TWOP_MAX_START = 83
# The capture the values belong to (record §T.8): the poll.log sha256 and the frame count of
# data/k11-captures/gp-twop; another capture FAILS until the four values are re-measured.
GP_TWOP_CAPTURE_SHA256 = 9c01a73bfb04be19f794316e80b784a86b782b65f06592f2091d1544edf2e22c
GP_TWOP_CAPTURE_FRAMES = 680
# END: empty = the replay runs the whole port script (to X, f=0x5E1; record §T.9); a value cuts it
# (gp_session.py port-script --end), for a replay that stalls.
GP_TWOP_END =
gp-twop-oracle: build ## Gameplay U7 oracle: two-human check, then frame, trace and moves ratchets on data/k11-captures/gp-twop (skips without it)
	@echo "== gameplay oracle: gp-twop (two humans; frame, trace and moves ratchets; record U7) =="
	$(PYTHON) -m unittest tools.tests.test_gp_twop
	@if [ -d $(K11_CAPTURES)/gp-twop ]; then $(PYTHON) tools/gp_twop.py check --capture $(K11_CAPTURES)/gp-twop; \
		else echo "gp-twop-oracle: no capture at $(K11_CAPTURES)/gp-twop (skipped)"; fi
	@$(MAKE) --no-print-directory gp-replay scenario=gp-twop GP_OPTIONAL=1 GP_SCRIPT_ARGS="$(if $(GP_TWOP_END),--end $(GP_TWOP_END))"
	@$(PYTHON) tools/gp_compare.py --scenario gp-twop --capture $(K11_CAPTURES)/gp-twop \
		--port $(GP_DUMP)/gp-twop --min-first "$(GP_TWOP_MIN_FIRST)" \
		--trace-min-first "$(GP_TWOP_TRACE_MIN_FIRST)" --max-start "$(GP_TWOP_MAX_START)" \
		--moves-min-first "$(GP_TWOP_MOVES_MIN_FIRST)" \
		--capture-sha256 "$(GP_TWOP_CAPTURE_SHA256)" --capture-frames "$(GP_TWOP_CAPTURE_FRAMES)"

# Gameplay U8 (plan 2026-10-01-gameplay-u8-other-modes.md, record 2026-10-01-gameplay-u8-
# derivations.md): the other START MENU rows and the attract start, one capture each. For every
# <ID>:<scenario> in GP_MODES_SCENARIOS whose data/k11-captures/<scenario> exists: the evidence
# check that the capture reached its row (tools/gp_modes.py), the port replay (cut at
# GP_MODES_<ID>_END when set) and both ratchets with the capture identity pin; an absent capture
# skips (exit 0), even under PR_ORACLE_REQUIRED (spec §4.3). A scenario is listed only once its
# values are pinned (record §U8.16-§U8.22), each value's provenance in its comment. The port dump
# is removed after its comparison unless GP_MODES_KEEP=1 (record §U8.6: 52-89 MB of /tmp each).
# gp-u8-right-arcade (record §U8.16): measured at bc51fd0 (+ its miss set) on the capture below.
# MIN_FIRST: first unexplained capture frame 726 (raw 3767), nearest port 526 (f=0x7B6): side 0
# takes character 3's reaction 0x20 at f=0x7B7 (r0=20) and the original runs its move callback
# 0x14EF8 (s0_52 = 0x0B, 0x14F16), which the port does not have (fn-miss 0x14EF8, unported, owner
# track P batch P2); TRACE_MIN_FIRST: first differing f=0x7BA (1978) in e0 (capture 0000, port
# 1010), the same cause; MAX_START: the window starts at capture frame 88 (raw 1745). The port's
# script runs to X (f=0x8E1). Raise N/F when they improve.
GP_MODES_RA_MIN_FIRST = 726
GP_MODES_RA_TRACE_MIN_FIRST = 1978
GP_MODES_RA_MAX_START = 88
GP_MODES_RA_CAPTURE_SHA256 = b72dbaa7486b651bd1acfddffe886aeb515eed3f916220d44375f0f67227906f
GP_MODES_RA_CAPTURE_FRAMES = 1140
# gp-u8-left-training (record §U8.17): measured at b7f13c6 (+ its miss set) on the capture below.
# MIN_FIRST: first unexplained capture frame 1076 (raw 4207), nearest port 847 = f=0x921, the
# port's last frame (the script ends at the capture's X record): how far the port got, not a
# divergence; capture 1076.. is the STOP_AT_END tail. TRACE_MIN_FIRST: no traced difference
# ("0 differing through 2337"), so end + 1 = 2338 is the exact pin (2339 fails as unreachable);
# MAX_START: the window starts at capture frame 83 (raw 1735). Raise N/F when they improve.
GP_MODES_LT_MIN_FIRST = 1076
GP_MODES_LT_TRACE_MIN_FIRST = 2338
GP_MODES_LT_MAX_START = 83
GP_MODES_LT_CAPTURE_SHA256 = 90eeef83f77dab25d9ed7be30fcadf278bfd0413c068c12bd700ff183ab09917
GP_MODES_LT_CAPTURE_FRAMES = 1141
# gp-u8-right-training (record §U8.18): measured at 9170f5c (+ its miss set) on the capture below.
# MIN_FIRST: first unexplained capture frame 1098 (raw 4272): capture 1097 equals port 845 (f=0x961,
# the port's last frame, 0 px), so 1098 is the next game frame (f=0x962), which the port never ran
# (its script ends at the capture's X record): how far the port got, not a divergence (gp_compare's
# row-hash "nearest port 844"). TRACE_MIN_FIRST: "0 differing through 2401", end + 1 = 2402, the
# exact pin; MAX_START: the window starts at capture frame 90 (raw 1745). Raise N/F when they improve.
GP_MODES_RT_MIN_FIRST = 1098
GP_MODES_RT_TRACE_MIN_FIRST = 2402
GP_MODES_RT_MAX_START = 90
GP_MODES_RT_CAPTURE_SHA256 = 496964928c537f3b428414e21d02f254b399af4b2f39b393d10d9855c74f1ea5
GP_MODES_RT_CAPTURE_FRAMES = 1167
# gp-u8-tug-of-war (record §U8.19): measured at c212a83 (+ its miss set) on the capture below.
# MIN_FIRST: first unexplained capture frame 1107 (raw 4343): capture 1106 equals port 826 (f=0x9A1,
# the port's last frame, 0 px), so 1107 is the next game frame, which the port never ran (its script
# ends at the capture's X record): how far the port got, not a divergence (gp_compare's row-hash
# "nearest port 825"). TRACE_MIN_FIRST: "0 differing through 2465", end + 1 = 2466, the exact pin;
# MAX_START: the window starts at capture frame 104 (raw 1741). Raise N/F when they improve.
GP_MODES_TW_MIN_FIRST = 1107
GP_MODES_TW_TRACE_MIN_FIRST = 2466
GP_MODES_TW_MAX_START = 104
GP_MODES_TW_CAPTURE_SHA256 = 30cd09b8d24a51a41cc37c0ffebd433a08e5a741b248f28e8112c28ff04d1439
GP_MODES_TW_CAPTURE_FRAMES = 1176
# gp-u8-handicap (record §U8.20): measured at 1380841 (+ its miss set) on the capture below.
# MIN_FIRST: first unexplained capture frame 1022 (raw 4155): capture 1021 equals port 789 (f=0x8E1,
# the port's last frame, 0 px), so 1022 is the next game frame, which the port never ran (its script
# ends at the capture's X record): how far the port got, not a divergence (gp_compare's row-hash
# "nearest port 652"). TRACE_MIN_FIRST: "0 differing through 2273", end + 1 = 2274, the exact pin;
# MAX_START: the window starts at capture frame 80 (raw 1739). Raise N/F when they improve.
GP_MODES_HC_MIN_FIRST = 1022
GP_MODES_HC_TRACE_MIN_FIRST = 2274
GP_MODES_HC_MAX_START = 80
GP_MODES_HC_CAPTURE_SHA256 = 8f35fd3abd3a5c527e0f72984ed8ba419cadbcefa4e53b349d5a1ed7b69cdfa3
GP_MODES_HC_CAPTURE_FRAMES = 1081
# gp-u8-endurance (record §U8.21): measured at 20c379e (+ its miss set) on the capture below. The
# scenario ends 300 frames into the team select 0x44798 (decision D3: idle, ENDURANCE never leaves
# mode 0x10; its fight is a named gap, record §U8.9), so N is where that team-select tail ends.
# MIN_FIRST: first unexplained capture frame 278 (raw 2759): capture 277 equals port 163 (f=0x494,
# the port's last presented frame; the team select presents every third f), so 278 shows the next
# present, which the port never ran (its script ends at the capture's X record, f=0x495): how far the
# port got, not a divergence. TRACE_MIN_FIRST: "0 differing through 1173", end + 1 = 1174, the exact
# pin; MAX_START: the window starts at capture frame 90 (raw 1744). Raise N/F when they improve.
GP_MODES_EN_MIN_FIRST = 278
GP_MODES_EN_TRACE_MIN_FIRST = 1174
GP_MODES_EN_MAX_START = 90
GP_MODES_EN_CAPTURE_SHA256 = 01c9069076ce231d652dbe1d97f0a80155d5e73bef932b0254816a79ec746204
GP_MODES_EN_CAPTURE_FRAMES = 308
# gp-u8-attract-start (record §U8.22): measured at 2f0eeed (+ its miss set) on the capture below
# (the pad arm: P1's F1 in mode 3, arm frame f=0x125, mode 0x1A at f=0x126). MIN_FIRST: first
# unexplained capture frame 1087 (raw 3854): capture 1086 equals port 823 (f=0x7E1, the port's last
# frame, 0 px), so 1087 is the next game frame, which the port never ran (its script ends at the
# capture's X record): how far the port got, not a divergence (gp_compare's row-hash "nearest port
# 822"). TRACE_MIN_FIRST: "0 differing through 2017", end + 1 = 2018, the exact pin; MAX_START: the
# window starts at capture frame 80 (raw 1742). Raise N/F when they improve.
GP_MODES_AS_MIN_FIRST = 1087
GP_MODES_AS_TRACE_MIN_FIRST = 2018
GP_MODES_AS_MAX_START = 80
GP_MODES_AS_CAPTURE_SHA256 = d6b0cf6b210992de4e7cda211f02cbe5653e21a22d3fbec3dd02e7e30e0351fd
GP_MODES_AS_CAPTURE_FRAMES = 1156
GP_MODES_SCENARIOS =
GP_MODES_SCENARIOS += RA:gp-u8-right-arcade
GP_MODES_SCENARIOS += LT:gp-u8-left-training
GP_MODES_SCENARIOS += RT:gp-u8-right-training
GP_MODES_SCENARIOS += TW:gp-u8-tug-of-war
GP_MODES_SCENARIOS += HC:gp-u8-handicap
GP_MODES_SCENARIOS += EN:gp-u8-endurance
GP_MODES_SCENARIOS += AS:gp-u8-attract-start
GP_MODES_KEEP ?=
gp-modes-oracle: build ## Gameplay U8 oracle: the other START MENU rows and the attract start (each skips without its capture)
	@echo "== gameplay U8: other modes (frame and trace ratchets; each skips without its capture) =="
	@$(PYTHON) -m unittest tools.tests.test_gp_modes
	@for p in $(GP_MODES_SCENARIOS); do \
		$(MAKE) --no-print-directory gp-modes-one GP_MODES_ID=$${p%%:*} scenario=$${p#*:} || exit 1; \
	done

gp-modes-one: build
	@if [ -d $(K11_CAPTURES)/$(scenario) ]; then \
		$(PYTHON) tools/gp_modes.py check --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) && \
		$(MAKE) --no-print-directory gp-replay scenario=$(scenario) GP_OPTIONAL=1 \
			GP_SCRIPT_ARGS="$(if $(GP_MODES_$(GP_MODES_ID)_END),--end $(GP_MODES_$(GP_MODES_ID)_END))" && \
		$(PYTHON) tools/gp_compare.py --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) \
			--port $(GP_DUMP)/$(scenario) --min-first "$(GP_MODES_$(GP_MODES_ID)_MIN_FIRST)" \
			--trace-min-first "$(GP_MODES_$(GP_MODES_ID)_TRACE_MIN_FIRST)" \
			--max-start "$(GP_MODES_$(GP_MODES_ID)_MAX_START)" \
			--capture-sha256 "$(GP_MODES_$(GP_MODES_ID)_CAPTURE_SHA256)" \
			--capture-frames "$(GP_MODES_$(GP_MODES_ID)_CAPTURE_FRAMES)"; \
		rc=$$?; [ -n "$(GP_MODES_KEEP)" ] || rm -rf $(GP_DUMP)/$(scenario); exit $$rc; \
	else \
		echo "gp-modes-oracle: no capture at $(K11_CAPTURES)/$(scenario), skipped"; \
	fi

# Gameplay U9/U10 oracles (plan docs/superpowers/plans/2026-10-02-gameplay-u9-u10-win-and-endings.md,
# record docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md): the win path
# (data/k11-captures/gp-u9-win) and the ending (data/k11-captures/gp-u10-ending), each reached
# under memory pokes (gp_session's ('poke', ...) steps; the port replays the capture's W records
# as `poke` lines at the same frames). The tool tests always run; with a capture present it must
# first show the raw-derived path (tools/gp_win.py check, record §W.9), then the frame and trace
# ratchets (gp_compare), the milestone ratchet (the leading milestones the port reaches at the
# capture's frame) and the win-fields trace ratchet (gp_session.WIN_FIELDS). Skips without the
# capture, even under PR_ORACLE_REQUIRED (spec §4.3); with it, an empty pin FAILS. The claims are
# narrow like gp-oracle's: what a poke replaced (the hits that would have KO'd P2, the death
# animation that would have set DS_00104B0C) is not claimed (record §W.9).
# The values below are pinned by the plan's Tasks 9 and 11 from the measured report lines
# (record §W.12/§W.14); raise each when it improves.
GP_WIN_MIN_FIRST =
GP_WIN_TRACE_MIN_FIRST =
GP_WIN_MAX_START =
GP_WIN_MILESTONES =
GP_WIN_WIN_MIN_FIRST =
GP_WIN_CAPTURE_SHA256 =
GP_WIN_CAPTURE_FRAMES =
GP_ENDING_MIN_FIRST =
GP_ENDING_TRACE_MIN_FIRST =
GP_ENDING_MAX_START =
GP_ENDING_MILESTONES =
GP_ENDING_WIN_MIN_FIRST =
GP_ENDING_CAPTURE_SHA256 =
GP_ENDING_CAPTURE_FRAMES =
.PHONY: gp-win-oracle gp-ending-oracle gp-win-one
gp-win-oracle: build ## Gameplay U9 oracle: win-path evidence, frame/trace/milestone/win ratchets on data/k11-captures/gp-u9-win (skips without it)
	@echo "== gameplay oracle: gp-u9-win (the win path under pokes; plan U9/U10) =="
	$(PYTHON) -m unittest tools.tests.test_gp_win
	@$(MAKE) --no-print-directory gp-win-one scenario=gp-u9-win GP_WIN_ID=WIN
gp-ending-oracle: build ## Gameplay U10 oracle: ending evidence, frame/trace/milestone/win ratchets on data/k11-captures/gp-u10-ending (skips without it)
	@echo "== gameplay oracle: gp-u10-ending (the ending under pokes; plan U9/U10) =="
	$(PYTHON) -m unittest tools.tests.test_gp_win
	@$(MAKE) --no-print-directory gp-win-one scenario=gp-u10-ending GP_WIN_ID=ENDING
gp-win-one: build
	@if [ ! -d $(K11_CAPTURES)/$(scenario) ]; then echo "gp-win-one: no capture at $(K11_CAPTURES)/$(scenario) (skipped)"; else \
		$(PYTHON) tools/gp_win.py check --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) \
			--capture-sha256 "$(GP_$(GP_WIN_ID)_CAPTURE_SHA256)" && \
		$(MAKE) --no-print-directory gp-replay scenario=$(scenario) GP_OPTIONAL=1 && \
		$(PYTHON) tools/gp_compare.py --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) \
			--port $(GP_DUMP)/$(scenario) --min-first "$(GP_$(GP_WIN_ID)_MIN_FIRST)" \
			--trace-min-first "$(GP_$(GP_WIN_ID)_TRACE_MIN_FIRST)" --max-start "$(GP_$(GP_WIN_ID)_MAX_START)" \
			--capture-sha256 "$(GP_$(GP_WIN_ID)_CAPTURE_SHA256)" --capture-frames "$(GP_$(GP_WIN_ID)_CAPTURE_FRAMES)" && \
		$(PYTHON) tools/gp_win.py path --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) \
			--port $(GP_DUMP)/$(scenario) --min-milestones "$(GP_$(GP_WIN_ID)_MILESTONES)" \
			--win-min-first "$(GP_$(GP_WIN_ID)_WIN_MIN_FIRST)" --capture-sha256 "$(GP_$(GP_WIN_ID)_CAPTURE_SHA256)"; fi

gp-report: build ## Report-only gameplay comparison (scenario=gp-…): counts and first differences, no ratchet, exit 0
	@$(MAKE) --no-print-directory gp-replay scenario=$(scenario) GP_OPTIONAL=1
	@$(PYTHON) tools/gp_compare.py --report --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) --port $(GP_DUMP)/$(scenario)

# Differential verification (spec 2026-09-30-reverse-completion-design §5): the original's own
# bytes run in an emulator against the port's C functions from the same image; compares every
# changed byte, the return register, the ordered list of calls into the call set with the memory changed
# so far at each (record E3: callees stubbed or run on both sides, the port through its PR_SEAM lines; a
# stub poisons the registers its callee does not preserve) and the block coverage. Skips
# cleanly without unicorn or capstone (spec §5.5); tools/diff_verify.py skips without PRAGE.EXE. The
# claim is narrow: equivalence on the exercised blocks and inputs only.
diff-verify: build ## Differential verification: original x86 bytes vs the port's C functions (skips without unicorn or capstone)
	@echo "== differential verification (original bytes vs the port's C; records E1, E3) =="
	@if $(PYTHON) -c "import unicorn, capstone" 2>/dev/null; then \
		$(PYTHON) -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify && \
		$(PYTHON) tools/diff_verify.py --diffrun $(BUILD_DIR)/diffrun --exe $(GAME_DIR)/PRAGE.EXE \
			--image $(DIFF_IMAGE) --table $(DIFF_TABLE) --self-check; \
	else echo "diff-verify: skipped: unicorn or capstone is not installed (pip install -r tools/requirements-diff.txt)"; fi

# E2 (record docs/superpowers/plans/2026-10-01-reverse-e2-derivations.md): triage of the entry
# candidates Ghidra never listed. The committed table must equal a fresh run on the image the port's
# loader dumps, so a port change that ports a target regenerates it in the same commit. Skips without
# capstone or PRAGE.EXE, and fails instead under PR_ORACLE_REQUIRED=1 (make verify sets it).
entry-triage: build ## E2 triage of the non-Ghidra entry candidates: unit tests + the committed table must equal a fresh run (skips without capstone or PRAGE.EXE; fails under PR_ORACLE_REQUIRED=1)
	@echo "== entry triage (the non-Ghidra entry candidates; record E2) =="
	@if ! $(PYTHON) -c "import capstone" 2>/dev/null; then \
		echo "entry-triage: skipped: capstone is not installed (pip install -r tools/requirements-diff.txt)"; \
		[ "$${PR_ORACLE_REQUIRED}" != 1 ] || exit 1; \
	elif [ ! -f $(GAME_DIR)/PRAGE.EXE ]; then echo "entry-triage: skipped: $(GAME_DIR)/PRAGE.EXE is absent"; \
		[ "$${PR_ORACLE_REQUIRED}" != 1 ] || exit 1; \
	else $(PYTHON) -m unittest tools.tests.test_entry_triage && \
		./$(BUILD_DIR)/diffrun --exe $(GAME_DIR)/PRAGE.EXE --image-out $(E2_IMAGE) && \
		$(PYTHON) tools/entry_triage.py --image $(E2_IMAGE) --live $(E2_LIVE) --check $(E2_TABLE) \
			--expect 579 --expect-u0 575; fi

# Headless FM render: on hosts where SDL audio cannot open, the windowed run is
# silent, so this plays the title bank through the sequencer + OPL core + mixer
# and writes a 16-bit stereo WAV at the OPL rate for listening in any player.
AUDIO_WAV ?= /tmp/pr_title_fm.wav
AUDIO_SECONDS ?= 12
audio-render: build ## Render the title FM music headlessly to a WAV (AUDIO_WAV, AUDIO_SECONDS)
	PR_AUDIO_WAV=$(AUDIO_WAV) PR_AUDIO_WAV_SECONDS=$(AUDIO_SECONDS) ./$(BUILD_DIR)/run_tests

# The --check run must come first: test_gfx.c reads frame_0001/0009/0017/0025.idx
# from the CWD, so the ladder has to produce them (frames >= 25) before the suite
# consumes them — otherwise that four-frame comparison never runs.
verify: build ## Full ladder: --check frames, oracle-required tests, front-end + demo-fight + attract cycle-2 ratchet oracles, K11 oracle, symbols.h idempotence
	@echo "== headless frames (must precede the tests that read frames/frame_*.idx) =="
	./$(BUILD_DIR)/prageport --game-dir $(GAME_DIR) --check $(verify_frames)
	@echo "== tests (oracles required; consume the captured frames) =="
	PR_ORACLE_REQUIRED=1 PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests
	@echo "== restart driver (the 0x65431 soft restart, record named-gaps-b §B.3) =="
	PR_RESTART=1 PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests
	@PR_ORACLE_REQUIRED=1 $(MAKE) --no-print-directory smk-oracle
	@echo "== title oracle (pixel-exact) =="
	@PR_ORACLE_REQUIRED=1 $(MAKE) --no-print-directory title-oracle
	@echo "== front-end oracle (pixel-exact, states 3/4; enforced) =="
	@$(MAKE) --no-print-directory frontend-oracle
	@echo "== demo-fight oracle (ratchet on the first unexplained frame) =="
	@$(MAKE) --no-print-directory demo-fight-oracle
	@echo "== attract cycle-2 oracle (ratchet; the demo-fight run's dump) =="
	@$(MAKE) --no-print-directory attract2-compare
	@echo "== attract prefix oracle (pixel-exact) =="
	@PR_ORACLE_REQUIRED=1 $(MAKE) --no-print-directory attract-oracle
	@echo "== K11 service-menu oracles (the walk and the menuesc restart; each skips without its capture) =="
	@$(MAKE) --no-print-directory k11-oracle
	@echo "== gameplay replay driver (the gp-pads capture; skips without it; record §G.11) =="
	@$(MAKE) --no-print-directory gp-replay scenario=gp-pads GP_OPTIONAL=1
	@echo "== gameplay oracle (frame and trace ratchets; skips without its capture; record §G.16) =="
	@$(MAKE) --no-print-directory gp-oracle
	@$(MAKE) --no-print-directory gp-charsel-oracle
	@echo "== gameplay oracle: gp-u6-moves-b (skips without its capture; record gameplay-u6 §U6.22) =="
	@$(MAKE) --no-print-directory gp-moves-oracle
	@$(MAKE) --no-print-directory gp-keys-oracle
	@$(MAKE) --no-print-directory gp-twop-oracle
	@$(MAKE) --no-print-directory gp-modes-oracle
	@$(MAKE) --no-print-directory gp-win-oracle
	@$(MAKE) --no-print-directory gp-ending-oracle
	@$(MAKE) --no-print-directory diff-verify
	@$(MAKE) --no-print-directory entry-triage
	@echo "== k11 and gp tool unit tests =="
	PR_ORACLE_REQUIRED=1 $(PYTHON) -m unittest tools.tests.test_k11_fields tools.tests.test_k11_session tools.tests.test_k11_capture tools.tests.test_k11_compare tools.tests.test_gp_session tools.tests.test_gp_capture tools.tests.test_gp_compare tools.tests.test_gp_moves tools.tests.test_gp_keys
	@echo "== title_compare unit tests (splice3, record §47-A) =="
	$(PYTHON) -m unittest tools.tests.test_title_compare
	@echo "== gra_extract oracle tests (real assets required) =="
	@PR_ORACLE_REQUIRED=1 $(MAKE) --no-print-directory re-extract-test
	@echo "== symbols.h must regenerate byte-identically =="
	$(PYTHON) tools/gen_symbols.py $(DECOMP_DIR) $(PORT_DIR)/src/symbols.h
	@git diff --quiet -- $(PORT_DIR)/src/symbols.h || { \
		echo "symbols.h is stale after regeneration — commit the regenerated header"; exit 1; }
	@echo "all checks passed"

run: build ## Run the port windowed, reading the original assets
	./$(BUILD_DIR)/prageport --game-dir $(GAME_DIR)

clean: ## Remove build outputs and locally generated oracles (keeps the SDD ledger)
	rm -rf $(BUILD_DIR)
	rm -f $(PORT_DIR)/tests/ghidra_data.bin $(PORT_DIR)/tests/title_screen_ref.ppm
	rm -rf frames/
	@echo "Cleanup complete. (.superpowers/ deliberately kept — it holds the plan ledger.)"

# ── RE · static inspection ───────────────────────────────────────────────────

re-info: ## Dump the LE layout and decode the INDEX resource table
	$(PYTHON) tools/le_info.py $(GAME_DIR)/PRAGE.EXE
	$(PYTHON) tools/le_info.py --index $(GAME_DIR)/INDEX

re-gra: ## Dump GRA header fields and chunk chains for the installed S16 set
	$(PYTHON) tools/gra_headers.py $(GAME_DIR)
	$(PYTHON) tools/gra_chunks.py \
		$(GAME_DIR)/S16FONTS.GRA $(GAME_DIR)/S16CAGE.GRA \
		$(GAME_DIR)/S16TITLE.GRA $(GAME_DIR)/S16COBSD.GRA

re-render: ## Render one GRA chunk to PPM (gra=S16TITLE.GRA chunk=0 [frame=N] [palette=N])
	$(PYTHON) tools/gra_render.py $(GAME_DIR)/$(gra) $(chunk) /tmp/$$(basename $(gra) .GRA).ppm $(if $(palette),--palette $(palette)) $(if $(frame),--frame $(frame))

re-extract: ## Extract every S16 sprite to extracted/ (RGBA PNG per descriptor + manifest.json)
	$(PYTHON) tools/gra_extract.py $(GAME_DIR) extracted

re-extract-test: ## Unit + oracle tests for the extractor (oracle needs data/game/C)
	PR_GAME_DIR=$(GAME_DIR) $(PYTHON) -m unittest tools.tests.test_gra_extract -v

re-symbols: ## Regenerate port/src/symbols.h from the decompilation
	$(PYTHON) tools/gen_symbols.py $(DECOMP_DIR) $(PORT_DIR)/src/symbols.h

re-cluster: ## Recluster the call graph and rewrite prage.clusters.txt
	$(PYTHON) tools/callgraph.py $(DECOMP_DIR)

# ── RE · Ghidra headless (JDK 25 + the lx-loader extension) ──────────────────

re-decompile: ## Ghidra headless: import PRAGE.EXE and analyse (slow, one-shot)
	$(GHIDRA_ENV) $(HEADLESS) $(PROJ_DIR) $(PROJECT_NAME) \
		-import $(GAME_DIR)/PRAGE.EXE -overwrite \
		-scriptPath $(SCRIPTS_DIR)

re-analyze: ## Ghidra headless: fix up, then re-export prage.c and the index CSVs
	$(GHIDRA_ENV) $(HEADLESS) $(PROJ_DIR) $(PROJECT_NAME) \
		-process PRAGE.EXE \
		-scriptPath $(SCRIPTS_DIR) \
		-postScript FixupProgram.java \
		-postScript ExportDecomp.java $(DECOMP_DIR)/prage.c 90 \
		-postScript ExportMeta.java $(DECOMP_DIR)/

re-oracle: ## Ghidra headless: dump the fixup-applied data object to port/tests/ghidra_data.bin
	@$(GHIDRA_ENV) $(HEADLESS) $(PROJ_DIR) $(PROJECT_NAME) \
		-process PRAGE.EXE -noanalysis \
		-scriptPath $(SCRIPTS_DIR) \
		-postScript DumpBytes.java 80000 8B0D0 > /tmp/prage_dataobj.txt
	@$(PYTHON) -c 'import re; t = open("/tmp/prage_dataobj.txt").read(); \
		m = re.search(r"80000 ([0-9a-f]+)", t); \
		assert m, "DumpBytes produced no data — is the project imported?"; \
		open("$(PORT_DIR)/tests/ghidra_data.bin", "wb").write(bytes.fromhex(m.group(1))); \
		print("wrote", len(m.group(1)) // 2, "bytes to $(PORT_DIR)/tests/ghidra_data.bin")'
	@echo "Untracked on purpose: it is a copy of the game's own bytes."

re-original: ## Run the original game in DOSBox-X (interactive; ESC then y, twice, to quit)
	sh $(RUNNER)

title-pin: ## Build the pinned copy of PRAGE.EXE for the title oracle (writes /tmp only)
	$(PYTHON) tools/title_pin.py --src $(GAME_DIR)/PRAGE.EXE --out $(TITLE_PIN_DIR)/PRAGE.EXE

title-capture: title-pin ## Capture the pinned original run for the title oracle (writes data/title-captures/)
	$(PYTHON) tools/title_capture.py --out $(TITLE_CAPTURES)/title --time-limit 45

frontend-capture: title-pin ## Capture the pinned original's front-end region (Task 1 front-end chain)
	$(PYTHON) tools/title_capture.py --out $(TITLE_CAPTURES)/frontend --time-limit 120
