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
        attract2-oracle attract2-compare k11-capture k11-oracle k11-report gp-capture gp-replay gp-oracle gp-report diff-verify gp-charsel-oracle

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
# from the first measurement on the pre-U0 base.
# MIN_FIRST: first unexplained capture frame 787 (raw 3899), round 1 (mode 6): its nearest port
# frame 575 is f=0x823, one frame before the unregistered move callback 0x23208 at f=0x824
# (divergence 2, record §G.24, U6's). Raised from 203 by U5 (record 2026-10-01-gameplay-u5
# §C5.12/§C5.13): divergence 1, the character-select idle animation turning at f=0x340, was the
# 0x37A58 top wrap comparing rec+0x4F where the raw's 0x37B03/0x37B08 compares the frame byte
# rec+0x52; with it fixed the claim covers the character select, the time-out, 0x11, 0x17 and the
# round start. Raise it when the frame claim improves (gp_compare prints "improved: raise N").
GP_IDLE_LOSS_MIN_FIRST = 787
# TRACE_MIN_FIRST: first differing f=0x828 (decimal 2088) in rng, port against the capture
# (capture 73A05D37, port CE92DD04). Four frames earlier (f=0x824) the port's P2 attack
# misses an UNREGISTERED move-table callback, 0x23208 (character 1, reaction 0x26; U0's
# §U0.12 list; fn_misslog, record §G.24), whose effects (animation, slot +0x52/+0x53/+0x54/+0xC,
# voice 0x79) are not traced fields; whether it causes the rng/hit difference is not excluded.
# No run-to-run bound (record §G.19: the two captures agree at every f from 0x625); raise it
# when the trace claim improves.
GP_IDLE_LOSS_TRACE_MIN_FIRST = 2088
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
# the port's last one. The port's script ends where the capture's poll.log ends, its last frame is
# byte-identical to the capture frame before N, and the capture runs on past it; so N is how far the
# port got, not a defect. The trace line reports no differing frame through the script's last f, so
# TRACE_MIN_FIRST is that f plus one, the exact pin (one more fails as unreachable).
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

gp-report: build ## Report-only gameplay comparison (scenario=gp-…): counts and first differences, no ratchet, exit 0
	@$(MAKE) --no-print-directory gp-replay scenario=$(scenario) GP_OPTIONAL=1
	@$(PYTHON) tools/gp_compare.py --report --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) --port $(GP_DUMP)/$(scenario)

# Differential verification (spec 2026-09-30-reverse-completion-design §5): the original's own
# bytes run in an emulator against the port's C functions from the same image; compares every
# changed byte, the return register and the block coverage. Skips cleanly without unicorn or
# capstone (spec §5.5); tools/diff_verify.py skips without PRAGE.EXE. The claim is narrow: equivalence on
# the exercised blocks and inputs only.
diff-verify: build ## Differential verification: original x86 bytes vs the port's C functions (skips without unicorn or capstone)
	@echo "== differential verification (original bytes vs the port's C; record E1) =="
	@if $(PYTHON) -c "import unicorn, capstone" 2>/dev/null; then \
		$(PYTHON) -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify && \
		$(PYTHON) tools/diff_verify.py --diffrun $(BUILD_DIR)/diffrun --exe $(GAME_DIR)/PRAGE.EXE \
			--image $(DIFF_IMAGE) --table $(DIFF_TABLE) --self-check; \
	else echo "diff-verify: skipped: unicorn or capstone is not installed (pip install -r tools/requirements-diff.txt)"; fi

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
	@$(MAKE) --no-print-directory diff-verify
	@echo "== k11 and gp tool unit tests =="
	PR_ORACLE_REQUIRED=1 $(PYTHON) -m unittest tools.tests.test_k11_fields tools.tests.test_k11_session tools.tests.test_k11_capture tools.tests.test_k11_compare tools.tests.test_gp_session tools.tests.test_gp_capture tools.tests.test_gp_compare
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
