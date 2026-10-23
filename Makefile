# Makefile for Primal Rage (DOS, 1995) — reverse engineering + SDL3 port
# Two pipelines:
#   RE   info → gra → symbols → cluster → decompile → analyse → oracle → original
#   PORT build → test → check → run → verify
#
# The pin constants below (DEMO_FIGHT_MIN_FIRST, ATTRACT2_MIN_FIRST, GP_*) hold
# measured ratchets: each N is the first unexplained/differing frame with its
# provenance in the comment above it, and is raised (never lowered) when the
# port improves. Their evidence is load-bearing (AGENTS.md) — do not "tidy" it.
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
# K11_DUMP and GP_DUMP are fixed shared /tmp paths: oracle-replay or capture-compare runs across
# worktrees must be serialized (one user at a time), or given a per-worktree command-line override
# (`make ... GP_DUMP=/tmp/pr_gp_dump_<lane>`; a command-line variable wins over this file).
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

.PHONY: help deps build test verify check run clean audio-render \
        smk-oracle title-pin title-capture title-oracle attract-oracle \
        frontend-capture frontend-oracle demo-oracle demo-fight-oracle \
        attract2-oracle attract2-compare k11-capture k11-oracle k11-report \
        save-reload-oracle \
        gp-capture gp-replay gp-oracle gp-report gp-charsel-oracle \
        gp-moves-oracle gp-keys-oracle gp-twop-oracle gp-modes-oracle gp-modes-one \
        gp-win-oracle gp-ending-oracle gp-win-one gp-u12-oracle gp-u13-oracle gp-u14-oracle gp-u15-oracle gp-u16-oracle \
        gp-u17-oracle \
        diff-verify entry-triage \
        re-info re-gra re-render re-symbols re-cluster re-extract re-extract-test \
        re-decompile re-analyze re-oracle re-original

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
	@if [ -f "$(PORT_DIR)/tests/ghidra_data.bin" ]; then \
		echo "ok: $(PORT_DIR)/tests/ghidra_data.bin present"; \
	else \
		echo "warning: $(PORT_DIR)/tests/ghidra_data.bin missing — PR_ORACLE_REQUIRED runs need it; make re-oracle regenerates it"; \
	fi
	@for module in unicorn capstone; do \
		if $(PYTHON) -c "import $$module" >/dev/null 2>&1; then \
			echo "ok: python $$module importable"; \
		else \
			echo "warning: python $$module not importable — make diff-verify needs tools/requirements-diff.txt"; \
		fi; \
	done
	@cmos="$(GAME_DIR)/CMOS"; \
	if [ ! -e "$$cmos" ] || { [ "$$(wc -c < "$$cmos" | tr -d ' ')" = 2040 ] && [ "$$(tr -d '\000' < "$$cmos" | wc -c | tr -d ' ')" = 0 ]; }; then \
		echo "ok: $$cmos absent or 2040 zero bytes"; \
	else \
		echo "warning: $$cmos must be absent or 2040 all-zero bytes — make verify requires it"; \
	fi
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
# The port-exhibited pin (sub-project A A1c): ATTRACT_MIN_PORT_EXH is a lower
# bound on the number of port attract frames the capture exhibits (a statistic
# the oracle only printed). A port that renders fewer attract frames exhibits
# fewer, which would otherwise pass; a measured count below the pin FAILs. The
# pin is MEASURED from both captures: `attract_compare.py --min-port-exh 99999`
# on data/title-captures/title and .../title2 prints
# "port attract frames exhibited 173/690" for each, so the pin is 173. Raise it
# when the number grows (the tool prints no "improved" line; a measured count
# below the pin fails).
ATTRACT_MIN_PORT_EXH ?= 173
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
		--expect-first 215 --min-port-exh $(ATTRACT_MIN_PORT_EXH) --port $(ATTRACT_DUMP)

# Front-end oracle (the enforced one): the PR_FRONTEND_DUMP driver drives state
# 2 into states 3/4, one RGB24 frame per presented frame plus a per-frame hash
# log. Two gates stand: the PR_FRONTEND_DET determinism check (run_tests
# re-invokes itself twice and requires the two hash logs byte-identical — the
# whole first recipe line, so a failure fails the ladder), and the pixel
# comparison, which exits non-zero on any unexplained frame except the two named
# in tools/title_compare.py's FRONTEND_ALLOWED_UNEXPLAINED. All-black capture
# frames are dropped as documented artifacts (port/spec/game_flow.md); a
# content-bearing frame that disagrees still fails. The window is derived from
# the port's own dump; A1b (below) pins its END so a port that renders fewer
# frames — which would shrink the window and leave the unexplained set empty —
# fails the gate.
# Captures are git-ignored; an absent capture skips both.
# The window END pin (sub-project A A1b): the front-end window is the capture
# frames a port frame is exhibited by, and its end b is where the port's last
# exhibited frame lands. A port that renders fewer frames stops the window
# earlier, but the frames it drops fall OUTSIDE the shrunken window, so the
# unexplained set stays empty and only the end pin catches them. b is MEASURED
# from the enforced capture: `title_compare.py --frontend` on
# data/title-captures/frontend against the PR_FRONTEND_DET dump prints
# "window distinct [560..1884]" (raw 3108..4791), so the pin is 1884. Raise it
# when the window grows (the tool warns "improved: raise the pin"); a measured
# b below it FAILS the gate.
FRONTEND_END ?= 1884
frontend-oracle: build ## Front-end oracle (states 3/4; skips without data/title-captures/frontend)
	@echo "== front-end oracle (pixel-exact, states 3/4) =="
	@if [ -d $(TITLE_CAPTURES)/frontend ]; then \
		rm -rf $(FRONTEND_DUMP); \
		PR_FRONTEND_DET=$(FRONTEND_DUMP) PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "frontend-oracle: no capture at $(TITLE_CAPTURES)/frontend, frames not compared"; \
	fi
	@$(PYTHON) tools/title_compare.py --frontend --frontend-end $(FRONTEND_END) \
		--capture $(TITLE_CAPTURES)/frontend \
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
# cycle-2 2191/2192/2193: the last loader screen, the load frame's present and the
# next, which the read's tick re-sync 0x1B45F/0x1B464 lets fall inside one capture
# scan; they were 2192/2193/2194, and the dump 2308 frames, not 2307, until track I's
# master init read s16title at boot as the original does, 0x2F9E9/0x2F9FC, which took
# the driver's s16title loader screen out of the dump: record 2026-10-13-reverse-i §I.4).
# The driver lists the loader screens in cycle2/loader.txt for that check.
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
# The port-frame coverage pins (sub-project A A1c): K11_<SCENARIO>_MIN_EXH is a lower
# bound on the distinct port frames the capture exhibits (the `--min-exh` pin); a port
# that renders fewer frames — which only shrinks its port-derived window and settled
# list — exhibits fewer and FAILs. The same comparison also now fails a non-black port
# frame up to the final settled screen that no capture frame exhibits. Both are
# MEASURED from the captures: `k11_compare.py --min-exh 99999` on the walk dump prints
# "port frames exhibited 36 < pinned" (so 36) and on the menuesc dump "… exhibited 184"
# (so 184). Raise a pin when its count grows; a measured count below the pin fails.
K11_WALK_MIN_EXH ?= 36
K11_MENUESC_MIN_EXH ?= 184
k11-oracle: build ## K11 oracles: the walk and the menuesc restart (each skips without its data/k11-captures/<scenario>)
	@echo "== K11 service-menu oracle (pixel-exact, the walk) =="
	@if [ -d $(K11_CAPTURES)/walk ]; then \
		rm -rf $(K11_DUMP)/walk; mkdir -p $(K11_DUMP); \
		$(PYTHON) tools/k11_session.py port-script --scenario walk --capture $(K11_CAPTURES)/walk --out $(K11_DUMP)/walk.script && \
		PR_K11_DUMP=$(K11_DUMP)/walk PR_K11_SCRIPT=$(K11_DUMP)/walk.script PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "k11-oracle: no capture at $(K11_CAPTURES)/walk, frames not compared"; \
	fi
	@$(PYTHON) tools/k11_compare.py --capture $(K11_CAPTURES)/walk --port $(K11_DUMP)/walk \
		--min-exh $(K11_WALK_MIN_EXH)
	@echo "== K11 soft-restart oracle (pixel-exact, menuesc: the MAIN MENU Esc 0x2520B restart) =="
	@if [ -d $(K11_CAPTURES)/menuesc ]; then \
		rm -rf $(K11_DUMP)/menuesc; mkdir -p $(K11_DUMP); \
		$(PYTHON) tools/k11_session.py port-script --scenario menuesc --capture $(K11_CAPTURES)/menuesc --out $(K11_DUMP)/menuesc.script && \
		PR_K11_DUMP=$(K11_DUMP)/menuesc PR_K11_SCRIPT=$(K11_DUMP)/menuesc.script PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "k11-oracle: no capture at $(K11_CAPTURES)/menuesc, frames not compared"; \
	fi
	@$(PYTHON) tools/k11_compare.py --scenario menuesc --capture $(K11_CAPTURES)/menuesc --port $(K11_DUMP)/menuesc \
		--min-exh $(K11_MENUESC_MIN_EXH)

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

# Save-reload oracle (plan 2026-10-15-reverse-j-closeout.md task 7; record
# 2026-10-15-reverse-j-derivations.md §J3). data/k11-captures/save-reload is two
# boots of the pinned original: boot 1 (the one writable mount, the 11 keys ending
# in CONFIG OPTIONS' `right` tap and two Escs) leaves the OPTIONS MENU, whose
# menu_run return calls config_file_save (0x2CBAF); boot 2 (read-only, one Enter)
# starts from the file boot 1 wrote. The two claims (tools/save_reload.py):
# write — the port's boot-1 replay with PR_K11_SAVE=<scratch> writes a file
# byte-identical to the capture's CMOS; load — the port booted from a copy of that
# CMOS holds, at the pinned frame and in every later dumped frame, the five
# storage ranges of §J3.3 that boot 2's poll.log holds at the pin. Both narrow:
# no display claim, no coverage of the other save sites, the kinds outside the
# ranges, or anything past the pin. Enforced like k11-oracle: skips without the
# capture (PR_ORACLE_REQUIRED is not read) and fails on a broken claim once it
# exists; a difference is a finding, never a re-pin towards the port.
# Pins, all measured on the one capture (2026-10-15, DOSBox-X 2026.08.31 at
# /usr/local/bin/dosbox-x, session.txt):
#   CMOS_SHA256: data/k11-captures/save-reload/CMOS (2040 bytes, the XOR last
#     byte); POLL_SHA256: .../poll.log, boot 2's (boot 1's is .../boot1/poll.log,
#     sha cfc05e51af655c0ab59f7e15b0253eef75e660cc92f83d9844c03a7eb3875bfc);
#   ENTER_FRAME 491: boot 2's first P record with mode 0x27 after its K
#     (f=0x1EB; boot 1's is 304) — the port dump frame the load claim judges from;
#   FRAMES 162: boot 2's frame_*.raw count.
SAVE_RELOAD_CMOS_SHA256 = dad3b1f6f84bd851eee675281a7a3310dcf967d42a375b1b9919fb1a41f104f9
SAVE_RELOAD_POLL_SHA256 = fb9ab5aa5f833e4decd680c55358e3df25b4dda887e1837d1033e25cb19a05f5
SAVE_RELOAD_ENTER_FRAME = 491
SAVE_RELOAD_FRAMES = 162
save-reload-oracle: build ## Save-reload oracle: boot 1's replay writes the captured CMOS, boot 2 loads it (skips without data/k11-captures/save-reload)
	@echo "== save-reload oracle (the write claim, then the load claim) =="
	@if [ -d $(K11_CAPTURES)/save-reload ]; then \
		rm -rf $(K11_DUMP)/save-reload; mkdir -p $(K11_DUMP)/save-reload/boot1-scratch; \
		rm -f $(K11_DUMP)/save-reload/boot1-scratch/cmos; \
		$(PYTHON) tools/k11_session.py port-script --scenario save-reload --boot 1 --capture $(K11_CAPTURES)/save-reload/boot1 --out $(K11_DUMP)/save-reload/boot1.script && \
		PR_K11_DUMP=$(K11_DUMP)/save-reload/boot1 PR_K11_SCRIPT=$(K11_DUMP)/save-reload/boot1.script \
			PR_K11_SAVE=$(K11_DUMP)/save-reload/boot1-scratch/cmos PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests && \
		$(PYTHON) tools/save_reload.py write --capture $(K11_CAPTURES)/save-reload \
			--scratch $(K11_DUMP)/save-reload/boot1-scratch/cmos --game-dir $(GAME_DIR) \
			--cmos-sha256 "$(SAVE_RELOAD_CMOS_SHA256)" && \
		cp -f $(K11_CAPTURES)/save-reload/CMOS $(K11_DUMP)/save-reload/boot2-cmos && \
		cmp $(K11_CAPTURES)/save-reload/CMOS $(K11_DUMP)/save-reload/boot2-cmos && \
		$(PYTHON) tools/k11_session.py port-script --scenario save-reload --boot 2 --capture $(K11_CAPTURES)/save-reload --out $(K11_DUMP)/save-reload/boot2.script && \
		PR_K11_DUMP=$(K11_DUMP)/save-reload/boot2 PR_K11_SCRIPT=$(K11_DUMP)/save-reload/boot2.script \
			PR_K11_SAVE=$(K11_DUMP)/save-reload/boot2-cmos PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests && \
		$(PYTHON) tools/save_reload.py load --capture $(K11_CAPTURES)/save-reload \
			--port $(K11_DUMP)/save-reload/boot2 --enter-frame "$(SAVE_RELOAD_ENTER_FRAME)" \
			--poll-sha256 "$(SAVE_RELOAD_POLL_SHA256)" --capture-frames "$(SAVE_RELOAD_FRAMES)"; \
	else echo "save-reload-oracle: no capture at $(K11_CAPTURES)/save-reload (skipped)"; fi

# Gameplay capture (spec 2026-09-30-gameplay-ground-truth-design.md §4.1): the
# pinned original under DOSBox-X with frame-keyed injection and a per-frame
# snapshot log. Writes only data/k11-captures/gp-<scenario>/ (gp_capture.guard_gp).
GP_ARGS ?=
GP_REPORT_MAX ?= 5
gp-capture: title-pin ## Capture a gameplay scenario (scenario=gp-…; writes data/k11-captures/)
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
# Track G g6 RE-MEASURED and RAISED this pin (record 2026-10-14-reverse-g §G6): the three-frame
# scan-out is now isolated (the master loop's own gate 0x25643 / catch-up 0x256C5: mode-8 entry
# iterations whose compute overruns an ISR tick skip their presents, and the re-opened presents
# land sub-tick apart inside one 70.09 Hz scan; the loader re-sync hypothesis 0x1B45F/0x1B464
# was tested and rejected on the tick 1:1 cadence and the byte-identical held raws) and capture
# 2064 is allowed by name in SCAN3_ALLOWED (top rows 0..2 of port 1666/1665 held across raws
# 5187..5189, pair (1669, 1670) rows 3..97/98..199, the 6301-frame dump's unique adjacent bind,
# top sha 7bdf2c56.., §G6's table; per-frame per-bytes, fails on any mutation of a bound frame).
# With it active every capture frame 90..7558 is explained and the first unexplained is 7559
# (raw 12207): the first frame of content past the replayed script — the port's replay ends with
# "0 restart(s) landed" and its last presented frame is f=0x207D, while the capture from 7557
# (all-black, skipped) / 7558 (six non-black rows, still matched) goes on to the soft restart's
# boot screens; the X record is ms=174412 f=0x207F step=4 end. How far the port got, not a
# divergence (the gp-keys 1136 class, record §G5). Raise only when the script is lengthened
# (gp_compare prints "improved: raise N").
# Provenance: U4 pinned 203; U5 raised it to 787 (record 2026-10-01-gameplay-u5 §C5.12/§C5.13:
# the 0x37A58 top wrap compares the frame byte rec+0x52, raw 0x37B03/0x37B08); 787 was the miss of
# the then-unregistered move callback 0x23208 (divergence 2, record §G.24), ported by U6a; U6a
# pinned 2064 (record gameplay-u6 §U6.21); track G g6 raised it to 7559 with §G6's allowance.
# Raise it when the frame claim improves (gp_compare prints "improved: raise N").
GP_IDLE_LOSS_MIN_FIRST = 7559
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
# MIN_REACH (sub-project A §A1): the highest port frame any classified capture frame
# exhibits ("coverage ... up to port 6300"), the replay's last presented frame (f=0x207D);
# below it both the window and the coverage shrink, so a port that renders fewer screens
# fails. Measured 2026-10-16 on data/k11-captures/gp-idle-loss via gp_compare --report.
# Raise it when gp_compare prints "improved: raise the pin".
GP_IDLE_LOSS_MIN_REACH = 6300
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
		--min-reach "$(GP_IDLE_LOSS_MIN_REACH)" \
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
# MIN_REACH (sub-project A §A1): the highest port frame any classified capture frame
# exhibits ("coverage ... up to port 358"), the replay's last presented frame (the script
# ends at the capture's mode-6 frame). Measured 2026-10-16 on data/k11-captures/gp-u5-charsel
# via gp_compare --report. Raise it when gp_compare prints "improved: raise the pin".
GP_CHARSEL_MIN_REACH = 358
gp-charsel-oracle: build ## Gameplay oracle: gp-u5-charsel frame and trace ratchets (skips without data/k11-captures/gp-u5-charsel)
	@echo "== gameplay oracle: gp-u5-charsel (frame and trace ratchets) =="
	@$(MAKE) --no-print-directory gp-replay scenario=gp-u5-charsel GP_OPTIONAL=1
	@$(PYTHON) tools/gp_compare.py --scenario gp-u5-charsel --capture $(K11_CAPTURES)/gp-u5-charsel \
		--port $(GP_DUMP)/gp-u5-charsel --min-first "$(GP_CHARSEL_MIN_FIRST)" \
		--trace-min-first "$(GP_CHARSEL_TRACE_MIN_FIRST)" --max-start "$(GP_CHARSEL_MAX_START)" \
		--min-reach "$(GP_CHARSEL_MIN_REACH)" \
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
# comment fold-ins; record §U6.22; re-measured at the C3e closure, log /tmp/pr_c3e_final.log):
#   MIN_FIRST: "frames: FIRST UNEXPLAINED capture 2139 (raw 5260): nearest port 1737, rows
#     64..197, x 1..319 (4279 px)"; U6b Task 15 measured 1005 (the health-bar rows) and the pin
#     had stayed there while the port's replay advanced the first unexplained frame.
#     Track G g7 (record 2026-10-14-reverse-g §G7) names that class: 2139 is the first frame of
#     the capture's post-script tail. The port's script ends at the capture's X record (f=0xCAF,
#     ms=75222 step=38 end) and the dump's last presented frame is 1737 (f=0xCAF): capture 2139
#     holds port 1737's rows 0..63 and 198..199 (the still-scanned last screen) and rows 64..197
#     (the 4279 px) the original's next screen (f>=0xCB0), which no replay of this script can
#     present. How far the port got, not a divergence (the gp-keys 1136 / gp-idle-loss 7559
#     class); N=2139 is exact for this capture (a longer script is a re-capture). The window
#     starts at 88 (GP_MOVES_MAX_START), unchanged.
#   TRACE_MIN_FIRST: "trace: 0 differing through 3247; ratchet N 2262 ok (every item is
#     explained: N = 3248 is the exact pin)"; U6b Task 15's first difference was f=8D6 (2262) in
#     s1_5a: capture 16, port 2D, six frames after P1's 0x24 move at f=0x8D0, with no
#     unregistered callback left (distinct=4), so the cause was not a miss (a named gap, record
#     §U6.22); the replay now matches the field through the compared range's end.
#   MOVES_MIN_FIRST: "moves: 0 differing through 3247; ratchet N 2949 ok (every item is
#     explained: N = 3248 is the exact pin)"; U6b Task 15's first difference was f=B85 (2949) in
#     r1: capture 0, port 15; the replay now matches those bytes through the compared range's end.
#   MAX_START: "frames: window from capture 88 (raw 1741)".
#   CAPTURE_SHA256/FRAMES: data/k11-captures/gp-u6-moves-b/poll.log and its frame_*.raw.gz count
#     (U6b Task 6 re-run, re-checked on disk at Task 15).
GP_MOVES_MIN_FIRST = 2139
GP_MOVES_TRACE_MIN_FIRST = 3248
GP_MOVES_MOVES_MIN_FIRST = 3248
GP_MOVES_MAX_START = 88
GP_MOVES_CAPTURE_SHA256 = dcf242da915c41f4b4e381babdabdc30cd04ce89ed83e25f5178601e4878b2e3
GP_MOVES_CAPTURE_FRAMES = 2205
# MIN_REACH (sub-project A §A1): the highest port frame any classified capture frame
# exhibits ("coverage ... up to port 1737"), the replay's last presented frame (the script
# ends at the capture's X record, f=0xCAF). Measured 2026-10-16 on
# data/k11-captures/gp-u6-moves-b via gp_compare --report. Raise it when gp_compare prints
# "improved: raise the pin".
GP_MOVES_MIN_REACH = 1737
gp-moves-oracle: build ## Gameplay oracle: gp-u6-moves-b frame, trace and moves ratchets (skips without data/k11-captures/gp-u6-moves-b)
	@echo "== gameplay oracle: gp-u6-moves-b (frame, trace and moves ratchets) =="
	@$(MAKE) --no-print-directory gp-replay scenario=gp-u6-moves-b GP_OPTIONAL=1
	@$(PYTHON) tools/gp_compare.py --scenario gp-u6-moves-b --capture $(K11_CAPTURES)/gp-u6-moves-b \
		--port $(GP_DUMP)/gp-u6-moves-b --min-first "$(GP_MOVES_MIN_FIRST)" \
		--trace-min-first "$(GP_MOVES_TRACE_MIN_FIRST)" --max-start "$(GP_MOVES_MAX_START)" \
		--moves-min-first "$(GP_MOVES_MOVES_MIN_FIRST)" \
		--min-reach "$(GP_MOVES_MIN_REACH)" \
		--capture-sha256 "$(GP_MOVES_CAPTURE_SHA256)" --capture-frames "$(GP_MOVES_CAPTURE_FRAMES)"

# U11 in-match keys (plan 2026-10-01-gameplay-u11-in-match-keys.md, record
# 2026-10-01-gameplay-u11-derivations.md §K.6/§K.12): tools/gp_keys.py judges
# each key event of data/k11-captures/gp-keys-fight twice: `evidence` (the
# capture shows the raw-derived effect) and `effects` (the port's PR_GP_DUMP
# replay shows it at the same frames; a ratchet: the events before the first
# the port does not reproduce must number >= GP_KEYS_MIN_EFFECTS). Skips without
# the capture; with it, an unpinned value or another poll.log fails. Narrow: an
# event is judged on the latch, the pause bytes, the pad words, mode, b1f, cred
# and rng only. Track G task 5 (record 2026-10-14-reverse-g §G5) added the frame
# ratchet below: the pause and prompt frames are now compared, with capture 835
# the one frame allowed by name (its band is the never-dumped present state, the
# §K.10 gap).
# MIN_EFFECTS: the port reproduces the first 11 of the 11 events of record §K.6 on
# data/k11-captures/gp-keys-fight (Task 6, record §K.12: "effects: first not reproduced 11,
# ratchet N 0 ok (improved: raise N)"); raise it when gp_keys prints "improved: raise N".
# CAPTURE_SHA256: that capture's poll.log; another capture fails until it is re-measured
# (Task 5 Step 1) and both values re-pinned.
GP_KEYS_MIN_EFFECTS = 11
GP_KEYS_CAPTURE_SHA256 = 8425afbc46d51173532f6f4c27a8f16c594e2bc572b4273e056bdbf51a9678ae
# The frame + trace ratchet (track G task 5, plan 2026-10-14-reverse-g-widening, record
# 2026-10-14-reverse-g-derivations.md §G5): tools/gp_compare.py over the same replay.
# Provenance, measured on 5f874678 (the replay's 789-frame dump at $(GP_DUMP)/gp-keys-fight):
#   MIN_FIRST: the allowance (gp_compare.BANDS_ALLOWED['gp-keys-fight']: capture 835 = raw
#     3934, rows 0..80 byte-bound to port 592 (f=0x832), rows 92..199 to port 593 (f=0x833),
#     band rows 81..91 pinned by sha256 d9c0ac62f7e0c0d4082fba8d195eae4f767c56a7887bdf7346450511f998cad8,
#     the dump's unique matching adjacent pair; §G5: 2442 band bytes equal neither frame, no
#     splice point, no port frame holds the band) explains 835, leaving ZERO unexplained
#     frames inside the replay's range: "first unexplained 1136, ratchet N 1136 ok".
#     1136 (raw 5027)
#     is the end of the port's script (the capture's X record f=0873, step=19 end; captures
#     1136..1165 are the restart's boot movies the port never ran) — like gp-twop's 612, how
#     far the port got, not a divergence. Raise only if the script is lengthened.
#   TRACE_MIN_FIRST: "trace: 0 differing through 2163; ratchet N 2164 ok (every item is
#     explained: N = 2164 is the exact pin)" — 0x873 + 1, the replay's last traced f.
#   MAX_START: "frames: window from capture 100 (raw 1747)".
#   CAPTURE_FRAMES: 1166 frame_*.raw.gz (§K.14); CAPTURE_SHA256 above is unchanged (the
#     same capture — track G pinned no new hash). No moves pin: the capture has the move
#     fields and the report pass shows 0 differing, but the moves claim is out of this
#     task's scope (§G5).
GP_KEYS_MIN_FIRST = 1136
GP_KEYS_TRACE_MIN_FIRST = 2164
GP_KEYS_MAX_START = 100
GP_KEYS_CAPTURE_FRAMES = 1166
# MIN_REACH (sub-project A §A1): the highest port frame any classified capture frame
# exhibits ("coverage ... up to port 788"), the replay's last presented frame (the script
# ends at the capture's X record, f=0x873). Measured 2026-10-16 on
# data/k11-captures/gp-keys-fight via gp_compare --report. Raise it when gp_compare prints
# "improved: raise the pin".
GP_KEYS_MIN_REACH = 788
gp-keys-oracle: build ## In-match keys: evidence + effects ratchet and the frame ratchet on gp-keys-fight (skips without data/k11-captures/gp-keys-fight)
	@echo "== in-match keys oracle: gp-keys-fight (evidence, effects ratchet, frames ratchet) =="
	@$(MAKE) --no-print-directory gp-replay scenario=gp-keys-fight GP_OPTIONAL=1
	@$(PYTHON) tools/gp_keys.py evidence --capture $(K11_CAPTURES)/gp-keys-fight \
		--capture-sha256 "$(GP_KEYS_CAPTURE_SHA256)"
	@$(PYTHON) tools/gp_keys.py effects --capture $(K11_CAPTURES)/gp-keys-fight --port $(GP_DUMP)/gp-keys-fight \
		--min-effects "$(GP_KEYS_MIN_EFFECTS)" --capture-sha256 "$(GP_KEYS_CAPTURE_SHA256)"
	@$(PYTHON) tools/gp_compare.py --scenario gp-keys-fight --capture $(K11_CAPTURES)/gp-keys-fight \
		--port $(GP_DUMP)/gp-keys-fight --min-first "$(GP_KEYS_MIN_FIRST)" \
		--trace-min-first "$(GP_KEYS_TRACE_MIN_FIRST)" --max-start "$(GP_KEYS_MAX_START)" \
		--min-reach "$(GP_KEYS_MIN_REACH)" \
		--capture-sha256 "$(GP_KEYS_CAPTURE_SHA256)" --capture-frames "$(GP_KEYS_CAPTURE_FRAMES)"

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
# MIN_REACH (sub-project A §A1): the highest port frame any classified capture frame
# exhibits ("coverage ... up to port 463"), the replay's last presented frame (the script
# ends at the capture's X record, f=0x5E1). Measured 2026-10-16 on data/k11-captures/gp-twop
# via gp_compare --report. Raise it when gp_compare prints "improved: raise the pin".
GP_TWOP_MIN_REACH = 463
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
		--min-reach "$(GP_TWOP_MIN_REACH)" \
		--capture-sha256 "$(GP_TWOP_CAPTURE_SHA256)" --capture-frames "$(GP_TWOP_CAPTURE_FRAMES)"

# Gameplay U8 (plan 2026-10-01-gameplay-u8-other-modes.md, record 2026-10-01-gameplay-u8-
# derivations.md): the other START MENU rows and the attract start, one capture each. For every
# <ID>:<scenario> in GP_MODES_SCENARIOS whose data/k11-captures/<scenario> exists: the evidence
# check that the capture reached its row (tools/gp_modes.py), the port replay (cut at
# GP_MODES_<ID>_END when set) and both ratchets with the capture identity pin; an absent capture
# skips (exit 0), even under PR_ORACLE_REQUIRED (spec §4.3). A scenario is listed only once its
# values are pinned (record §U8.16-§U8.22), each value's provenance in its comment. The port dump
# is removed after its comparison unless GP_MODES_KEEP=1 (record §U8.6: 52-89 MB of /tmp each).
# gp-u8-right-arcade (record §U8.16; re-measured by track P batch 2, record 2026-10-02-reverse-p2
# §P2.4, once 0x14EF8/0x14F50 and their streams' 0xD000 targets 0x14FA8/0x14FF8/0x150AC are ported):
# measured at 835db45 (rebased onto b16922d as dfd8367) (+ its miss set) on the capture below.
# MIN_FIRST: first unexplained capture frame 1072 (raw 4116): capture 1071 equals port 825 (f=0x8E1,
# the port's last frame, 0 px), so 1072 is the next game frame, which the port never ran (its script
# ends at the capture's X record): how far the port got, not a divergence (gp_compare's row-hash
# "nearest port 824"). TRACE_MIN_FIRST: "0 differing through 2273", end + 1 = 2274, the exact pin.
# Before P2 (U8, at bc51fd0) both stopped at the reaction 0x20 of f=0x7B7 (N 726, F 1978).
# MAX_START: the window starts at capture frame 88 (raw 1745). Raise N/F when they improve.
GP_MODES_RA_MIN_FIRST = 1072
GP_MODES_RA_TRACE_MIN_FIRST = 2274
GP_MODES_RA_MAX_START = 88
GP_MODES_RA_CAPTURE_SHA256 = b72dbaa7486b651bd1acfddffe886aeb515eed3f916220d44375f0f67227906f
GP_MODES_RA_CAPTURE_FRAMES = 1140
# MIN_REACH (sub-project A §A1): the replay's last exhibited port frame ("coverage ... up
# to port 825", script ends at the capture's X record). Measured 2026-10-16 on
# data/k11-captures/gp-u8-right-arcade via gp_compare --report. Raise when it improves.
GP_MODES_RA_MIN_REACH = 825
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
# MIN_REACH (sub-project A §A1): "coverage ... up to port 847", the replay's last exhibited
# frame (script ends at the capture's X record). Measured 2026-10-16 on
# data/k11-captures/gp-u8-left-training via gp_compare --report. Raise when it improves.
GP_MODES_LT_MIN_REACH = 847
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
# MIN_REACH (sub-project A §A1): "coverage ... up to port 845", the replay's last exhibited
# frame (script ends at the capture's X record). Measured 2026-10-16 on
# data/k11-captures/gp-u8-right-training via gp_compare --report. Raise when it improves.
GP_MODES_RT_MIN_REACH = 845
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
# MIN_REACH (sub-project A §A1): "coverage ... up to port 826", the replay's last exhibited
# frame (script ends at the capture's X record). Measured 2026-10-16 on
# data/k11-captures/gp-u8-tug-of-war via gp_compare --report. Raise when it improves.
GP_MODES_TW_MIN_REACH = 826
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
# MIN_REACH (sub-project A §A1): "coverage ... up to port 789", the replay's last exhibited
# frame (script ends at the capture's X record). Measured 2026-10-16 on
# data/k11-captures/gp-u8-handicap via gp_compare --report. Raise when it improves.
GP_MODES_HC_MIN_REACH = 789
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
# MIN_REACH (sub-project A §A1): "coverage ... up to port 163", the replay's last exhibited
# frame (script ends at the capture's X record, f=0x495; the team select presents every
# third f). Measured 2026-10-16 on data/k11-captures/gp-u8-endurance via gp_compare
# --report. Raise when it improves.
GP_MODES_EN_MIN_REACH = 163
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
# MIN_REACH (sub-project A §A1): "coverage ... up to port 823", the replay's last exhibited
# frame (script ends at the capture's X record). Measured 2026-10-16 on
# data/k11-captures/gp-u8-attract-start via gp_compare --report. Raise when it improves.
GP_MODES_AS_MIN_REACH = 823
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
			--min-reach "$(GP_MODES_$(GP_MODES_ID)_MIN_REACH)" \
			--capture-sha256 "$(GP_MODES_$(GP_MODES_ID)_CAPTURE_SHA256)" \
			--capture-frames "$(GP_MODES_$(GP_MODES_ID)_CAPTURE_FRAMES)"; \
		rc=$$?; [ -n "$(GP_MODES_KEEP)" ] || rm -rf $(GP_DUMP)/$(scenario); exit $$rc; \
	else \
		echo "gp-modes-oracle: no capture at $(K11_CAPTURES)/$(scenario), skipped"; \
	fi

# Gameplay truth II U12 (plan docs/superpowers/plans/2026-10-20-gameplay-truth-ii.md Task 4;
# record §J4): gp-u12-endurance, the ENDURANCE team pass 0x44798's eight picks and the fight
# they open (modes 0x30/0x31), the named gap record 2026-10-01-gameplay-u8-derivations.md
# §U8.9 leaves past gp-u8-endurance's 300-frame team-select cut (D3; that capture and its
# GP_MODES_EN_* pins stay as they are). Measured on data/k11-captures/gp-u12-endurance (evidence
# closeout Task 6; an absent capture skips the target, gp-replay's GP_OPTIONAL=1 and gp_compare's
# own skip, while a present capture with an empty pin FAILS: "not pinned").
# MIN_FIRST = 732 is the first unexplained capture frame: the replay's script ends at its X record
# (f=0x78A, port frame 490, the last of 491 dumped) and the capture runs 60 game frames on, so
# capture 732..771 (40 frames) are the post-script tail, how far the port got, not a divergence;
# every capture frame of the window below it is explained (400 clean, 222 splice, 10 all-black),
# through the whole mode-0x30/0x31 span (0x31 from f=0x6D9, port frame 418). TRACE_MIN_FIRST = 1931
# is the exact pin (`0 differing through 1930`, the X record's f), MAX_START the window's first
# capture frame, MIN_REACH the replay's last exhibited port frame (the report's "port 490"), the
# capture identity the poll.log sha256 and the frame_*.raw.gz count. Raise MIN_FIRST/MIN_REACH
# when the script is extended.
GP_U12_MIN_FIRST = 732
GP_U12_TRACE_MIN_FIRST = 1931
GP_U12_MAX_START = 100
GP_U12_MIN_REACH = 490
GP_U12_CAPTURE_SHA256 = 2edcc088d26162d0fd288c8a5caf17500ac4b7f4a974f57b6f780be0236a9014
GP_U12_CAPTURE_FRAMES = 772
gp-u12-oracle: build ## Gameplay U12 oracle: gp-u12-endurance's team pass and fight (skips without data/k11-captures/gp-u12-endurance)
	@echo "== gameplay oracle: gp-u12-endurance (team pass and modes 0x30/0x31) =="
	@$(PYTHON) -m unittest tools.tests.test_gp_modes
	@if [ ! -d $(K11_CAPTURES)/gp-u12-endurance ]; then echo "gp-u12-oracle: no capture at $(K11_CAPTURES)/gp-u12-endurance (skipped)"; else \
		$(PYTHON) tools/gp_modes.py check --scenario gp-u12-endurance --capture $(K11_CAPTURES)/gp-u12-endurance && \
		$(MAKE) --no-print-directory gp-replay scenario=gp-u12-endurance GP_OPTIONAL=1 && \
		$(PYTHON) tools/gp_compare.py --scenario gp-u12-endurance --capture $(K11_CAPTURES)/gp-u12-endurance \
		--port $(GP_DUMP)/gp-u12-endurance --min-first "$(GP_U12_MIN_FIRST)" \
		--trace-min-first "$(GP_U12_TRACE_MIN_FIRST)" --max-start "$(GP_U12_MAX_START)" \
		--min-reach "$(GP_U12_MIN_REACH)" \
		--capture-sha256 "$(GP_U12_CAPTURE_SHA256)" --capture-frames "$(GP_U12_CAPTURE_FRAMES)"; \
	fi

# Gameplay U9/U10 oracles (plan docs/superpowers/plans/2026-10-02-gameplay-u9-u10-win-and-endings.md,
# record docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md): the win path
# (data/k11-captures/gp-u9-win) and the ending (data/k11-captures/gp-u10-ending), each reached
# under memory pokes (gp_session's ('poke', ...) steps; the port replays the capture's W records
# as `poke` lines at W.f + 1, the iteration that reads them, record §W.8). The tool tests always run; with a capture present it must
# first show the raw-derived path (tools/gp_win.py check, record §W.9), then the frame and trace
# ratchets (gp_compare), the milestone ratchet (the leading milestones the port reaches at the
# capture's frame) and the win-fields trace ratchet (gp_session.WIN_FIELDS). Skips without the
# capture, even under PR_ORACLE_REQUIRED (spec §4.3); with it, an empty pin FAILS. The claims are
# narrow like gp-oracle's: what a poke replaced (the hits that would have KO'd P2, the death
# animation that would have set DS_00104B0C) is not claimed (record §W.9).
# The values below are pinned by the plan's Tasks 9 and 11 from the measured report lines
# (record §W.12/§W.14); raise each when it improves.
# gp-u9-win (record §W.12): measured at de44e7f (+ its miss set) on the capture below
# (poll.log sha256 and its 2521 stored frames pinned together). Report lines, verbatim:
#   "frames: FIRST UNEXPLAINED capture 346 (raw 2830): nearest port 221, rows 0..4, x 0..319 (933 px)"
#   "frames: window from capture 100 (raw 1744)"
#   "trace: first difference f=866 (2150) in rng: capture 9BE3F91A, port 8D1ACCD4"
#   "path: 0 not reproduced through 7; ratchet N 0 ok (every item is explained: N = 8 is the exact pin)"
#   "win: first difference f=C5A (3162) in afc: capture 6, port 3"
# MIN_FIRST 346: a long frame at the mode-8 entry, then the catch-up (window.txt: capture 345 is
# raw 2827, 346 is raw 2830; raws 2828-2829 repeat 2827, so the screen held port 218, f=0x48A,
# the first mode-8 frame, for three capture frames, ~43 ms). Capture 346 is rows 0..4 of port 218
# and rows 5..199 of port 221 (f=0x48D), 0 px each: a splice of ports 218 and 221, which the
# two-adjacent-frame model cannot express; ports 219/220 (f=0x48B/0x48C) appear in no capture
# frame. Not a port divergence, and not the poke's: the same scan-out is at gp-idle-loss's
# unpoked mode-8 entry (capture 2064, record §U6.21), and S records are missed at many unpoked
# mode entries here too. The next unexplained, 356, is a partial-draw composite of ports
# 229/230/231 (no pixel without a port source); the first that can be real content is 1374, at
# the 0x21044 miss (named in §W.12). TRACE_MIN_FIRST 2150: the original
# draws rng at f=0x866 in mode 9, 6 frames after the port's miss of the unported animation
# target 0x21044 (f=0x860; P track). WIN_MIN_FIRST 3162: match 2's stage, 0x25848's rng pick,
# follows that rng. MILESTONES 8: all reproduced. Raise each when it improves.
# Re-measured by track P batches 4+5 (record 2026-10-03-reverse-p4-p5 §P4.7/§P5.7) once 0x400E0,
# 0x21044 and 0x21084 are ported: the set loses those three rows and gains the targets the replay
# then reaches (0x213F0 P7 at f=0x8ED, 0x213F4 P7 at f=0x91F, 0x2BDA0 P6 at f=0xD0C, all skipped
# by anim_indirect), and TRACE_MIN_FIRST rises 2150 -> 2364 (the first trace difference; the WIN
# ratchet stays 3162, MIN_FIRST 346 and MAX_START 100).
# Re-measured by track P batch 6 (record 2026-10-03-reverse-p6 §P6.12) once 0x2BDA0 is ported:
# the set loses that row alone (the replay still reaches f=0xDAE and now runs the target, so
# fn-miss PR_GP_DUMP distinct=6 dropped=0) and every pin above is unchanged and exact, because the
# trace's first difference at f=0x93C precedes 0x2BDA0's first frame 0xD0C: every pin is exact
# (frames +1 347, trace +1 2365, win +1 3163, path +1 9 unreachable, max-start -1 99 fail).
# Re-measured by track P batch 7 (record 2026-10-04-reverse-p7 §P7.8) once 0x213F0 and 0x213F4 are
# ported, on the batch's task-2 tree: the set loses both rows and gains 0x36114 (Task 3's D000
# target, which the replay now reaches; fn-miss PR_GP_DUMP distinct=5 dropped=0: 0x5D812
# actor_spawn and set_dead, 0x29D60 and 0x5D812 from frontend_mode_1b_step, 0x36114
# anim_indirect hits=1). TRACE_MIN_FIRST rises 2364 -> 2377 (the first trace difference, f=0x949;
# N 2378 is unreachable); WIN_MIN_FIRST stays 3162 (f=0xC5A; 3163 unreachable). MIN_FIRST stays
# 346 (the mode-8 long frame; 347 fails), MAX_START 100 (99 fails), MILESTONES 8 (9 unreachable).
# Re-measured by track P batch 7 (record 2026-10-04-reverse-p7 §P7.8) once task 3 ports 0x36114: the
# set loses that row (fn-miss PR_GP_DUMP distinct=4 dropped=0: 0x5D812 actor_spawn and set_dead,
# 0x29D60 and 0x5D812 from frontend_mode_1b_step) and TRACE_MIN_FIRST and WIN_MIN_FIRST rise
# 2377/3162 -> 3503 (0 differing through 3502; N 3504 is unreachable, the replay's end).
# MIN_FIRST stays 346 (the mode-8 long frame; 347 fails), MAX_START 100 (99 fails) and
# MILESTONES 8 (9 unreachable).
# Re-measured by track G g6 (record 2026-10-14-reverse-g §G6): the mode-8 three-frame scan-out at
# capture 346 is ISOLATED (the 0x25643 gate's catch-up on the mode-8 entry's over-tick iterations;
# the loader re-sync hypothesis tested and rejected on the tick 1:1 cadence and the byte-identical
# held raws 2828/2829) and ALLOWED BY NAME in SCAN3_ALLOWED (top rows 0..4 = port 218's f=0x48A,
# the screen held across raws 2827..2829; rows 5..199 = port 221's f=0x48D; the 1977-frame dump's
# unique adjacent pair (220, 221); top sha 703828af..; ports 219/220 never blitted). With it active
# every capture frame 100..2208 is explained (356 and 1374, §W.12's next two, are explained on the
# current dump's replay — the P batches ported animation targets on that path since §W.12 measured;
# per-frame re-attribution is not done here) and the first unexplained is 2209
# (raw 4953): nearest port 1777 (f=0xBA1), rows 192..197, x 0..85 (166 px) — a 6-row strip whose
# bytes (sha256 a886c0f3..) are IDENTICAL to gp-u10-ending's 2062 strip, and NO frame of the
# 1977-frame dump holds them — including the loader/pump-hook samples of the mode-0x17→0x12 entry
# (dumps 1775..1777 all at f=0xBA1 with its tick advancing 0xBFA→0xC00 inside the iteration — the
# res.c stall shape — then one static image through f=0xBBF). The strip lands in the (0,192)
# text-row region, but the port did dump that entry's sampled states and none matches: the cause
# is NOT attributed here — a named gap, not §G6's scan-out class and not allowed. Pin 2210 fails, MAX_START 100 unchanged and
# exact (99 fails), TRACE/WIN/MILESTONES unchanged (3503/3503/8).
# SUPERSEDED (track G g7 below attributes the strip to the loader's `- LOADING -` overlay and allows it by name; MIN_FIRST 2209 -> 2452).
# Re-measured by track G g7 (record 2026-10-14-reverse-g §G7): the strip is now ATTRIBUTED and
# ALLOWED BY NAME in OVERLAY_ALLOWED — the loader's `- LOADING -` presentation (string 489, drawn
# by 0x1B3EA/0x1B3EF at (0,0xE6) = rows 192..197 x 0..85; its 166 glyph px are port 1776's and
# the mask's source port 1775 is the same indices under the stale black palette) over the
# mode-0x12 screen (port 1777, the f=0xBA1 entry frame): a composition of two frames the port
# itself dumped, which the two-frame model cannot express. The port's own presentation of that
# text is over the black screen (capture 2208 = port 1776) and its next one is dump frame 1805
# (f=0xC0E, over the later screen): the resolve presenting it is s16snd2's first, and the original's
# is the same resource one call earlier — the entry's voice 0x32 (case 1) is a music request whose
# song handle 0x03021BFA is index 6, and the original's sequencer reads that song through the
# resource layer in the entry's window (the strip's background is the entry's screen, not the
# later one; the capture's S records show no stall at f=0xC0E and one extra tick in the entry
# window), while the port's music path is host-owned and never resolves (platform/audio/ has no
# res_resolve), so the replay's first s16snd2 resolve is the *sample* queue of voice 0x3A at
# f=0xC0E. The frame of that presentation is thus the host-owned audio model's, not the ported
# game code's — a model artifact (§G7); the sequencer's exact read frame inside the entry's window
# is not isolated. With the allowance active every content-bearing
# capture frame 100..2451 is explained and the first unexplained is 2452 (raw 5603): nearest port
# 1976, rows 62..169, x 94..218 (1905 px) — the replay's post-script tail (port 1976 is f=0xDAE,
# the X record; rows 0..61/170..199 are its still-scanned screen). MIN_FIRST 2209 -> 2452; 2453
# fails (the exact pin). MAX_START 100 unchanged and exact (99 fails), TRACE/WIN/MILESTONES
# unchanged (3503/3503/8). The pixel claim now reaches the replay's end (f=0xDAE) instead of
# stopping at the strip.
GP_WIN_MIN_FIRST = 2452
GP_WIN_TRACE_MIN_FIRST = 3503
GP_WIN_MAX_START = 100
GP_WIN_MILESTONES = 8
GP_WIN_WIN_MIN_FIRST = 3503
GP_WIN_CAPTURE_SHA256 = 7dcea0f16403c0fa52b6ef690edbcccb5340d9b9d1ec96c66ca4d11770708ba8
GP_WIN_CAPTURE_FRAMES = 2521
# MIN_REACH (sub-project A §A1): the highest port frame any classified capture frame
# exhibits ("coverage ... up to port 1976"), the replay's last presented frame (f=0xDAE,
# the X record). Measured 2026-10-16 on data/k11-captures/gp-u9-win via gp_compare --report.
# Raise it when gp_compare prints "improved: raise the pin".
GP_WIN_MIN_REACH = 1976
# gp-u10-ending (record §W.14; re-measured by track P batch 2, plan reverse-p2 Task 9, record §W.16,
# once 0x2381C is ported): measured at 20e8f4a (P2 rebased onto b16922d, + its new miss set) on the
# capture below (poll.log sha256 and its 5704 stored frames pinned together). Report lines, verbatim:
#   "frames: FIRST UNEXPLAINED capture 331 (raw 2831): nearest port 219, rows 0..63, x 0..319 (10336 px)"
#   "frames: window from capture 83 (raw 1745)"
#   "trace: 0 differing through 9953; ratchet N 9954 ok"
#   "path: 0 not reproduced through 29; ratchet N 30 ok"
#   "win: 0 differing through 9953; ratchet N 9954 ok"
# memsize: gp-u10-ending runs DOSBox-X with memsize=64 (a harness value, record §W.13); the original's
# memory growth over the poked final is not root-caused, so memsize=64 claims nothing about the game's
# memory need.
# MIN_FIRST 331 (unchanged by P2): a long frame at the mode-8 entry, then the catch-up, as gp-u9-win's 346
# (window.txt: capture 330 is raw 2828, 331 is raw 2831; raws 2829-2830 repeat 2828, so the
# screen held port 216, f=0x483, the first mode-8 frame, for three capture frames, ~43 ms).
# Capture 331 is rows 0..1 of port 216, rows 2..63 of port 218 (f=0x485) and rows 64..199 of
# port 219 (f=0x486), 0 px each: a three-frame composite the two-adjacent-frame model cannot
# express; not a port divergence, and not the poke's (the same at gp-idle-loss's unpoked mode-8
# entry, record §U6.21). The next unexplained, 341/342, are partial-draw composites of ports
# 227/228/229 and 228/229 (no pixel without a port source). Since P2 the former 1274/1275 (the
# 0x2381C miss) are explained; --report's next two, 2062 (port 1686, f=0xB9A, mode 0x12, 166 px)
# and 3686 (port 3071, f=0x12EA, mode 0x24, 379 px), are not examined (record §W.16).
# TRACE_MIN_FIRST 9954: 0 differing through f=0x26E1 (9953), the X record: the end of the port's
# replay, not a divergence. Before P2 (at 0ea42d0) it stopped at f=0x849 (2121), the frame after
# the port's 0x2381C miss (CHAOS's reaction 0x25 at f=0x848); with 0x2381C ported the rng is shared
# to the end and the final's opponent order (1,4,3,0,6,5,2) equals the capture's.
# WIN_MIN_FIRST 9954: 0 differing through 9953, the replay's end (before P2: 5634, f=0x1602, b0c set
# by 0x37EA0 29 frames late in the port). The port then still missed 0x37DD4/0x29C78 (batch 6 drops
# 0x37DD4, below) and b0c now agrees at every frame, so that delay followed the rng and the opponent
# order (record §W.16).
# MILESTONES 30: all reproduced. SUPERSEDED (track P batch 7 ported 0x29C78 and track G g7 raised
# the frame ratchet to 4113; see both re-measures below): at this measurement the replay still
# missed 0x29C78 (33 hits; batch 6 dropped 0x37DD4),
# whose effects the traced fields cannot see: the clean trace to f=0x26E1 does
# not claim the death streams or the mode-0xF content are reproduced, and is no evidence of
# correctness past the frame ratchet's 331.
# Every pin + 1 fails (record §W.16). Raise each when it improves.
# Re-measured by track P batch 3 (record 2026-10-03-reverse-p3 §P3.9) once 0x475EC is ported: the
# set loses that row alone and every pin above is unchanged (each + 1 fails).
# Re-measured by track P batches 4+5 (record 2026-10-03-reverse-p4-p5 §P5.7) once 0x3DA50 is
# ported: the set loses that row alone, the replay reaches no new target and every pin above is
# unchanged (each + 1 fails).
# Re-measured by track P batch 6 (record 2026-10-03-reverse-p6 §P6.12) once 0x37DD4 is
# ported: the set loses that row alone and every pin above is unchanged (each + 1 fails).
# Re-measured by track P batch 7 (record 2026-10-04-reverse-p7 §P7.8) once 0x29C78 is ported: the set
# loses that row (fn-miss PR_GP_DUMP distinct=4 dropped=0: 0x5D812 actor_spawn and set_dead, 0x29D60
# and 0x5D812 from frontend_mode_1b_step) and every pin above is exact and unchanged: frames 331
# (332 fails), trace/win 9954 (9955 is unreachable, the replay's end), path 30 (31 unreachable),
# MAX_START 83. TRACE/WIN are at the replay's end, so they cannot rise.
# Re-measured by track G g6 (record 2026-10-14-reverse-g §G6): the mode-8 three-frame scan-out at
# capture 331 is ISOLATED (the 0x25643 gate's catch-up, like idle-loss's 2064) and ALLOWED BY NAME
# in SCAN3_ALLOWED (top rows 0..1 = port 216's f=0x483, the screen held across raws 2828..2830, the
# dump's earlier holders of those rows being {216, 243, 1127}; rows 2..63 = port 218's f=0x485 and
# rows 64..199 = port 219's f=0x486, the 4664-frame dump's unique adjacent pair (218, 219);
# top sha 2d6fb6ad..; port 217 never blitted). With it active every capture frame 83..2061 is
# explained and the first unexplained is 2062 (raw 4954) — §W.16's already-named next-one (port 1686,
# f=0xB9A, mode 0x12, 166 px, rows 192..197 x 0..85): the SAME strip bytes as gp-u9-win's 2209
# (sha256 a886c0f3..), at the same mode-0x17→0x12 entry, and no frame of the 4664-frame dump holds
# them — including the loader/pump-hook samples of that entry (dumps 1684..1686 all at f=0xB9A, its
# tick 0xBF3→0xBF9 inside the iteration, then one static image through f=0xBB8). Cause not
# attributed here — not §G6's class and not allowed. Pin 2063
# fails; 3686 (§W.16's other next-one) is explained on the current dump since the P4-P7 batches
# (the report's next unexplained after 2062 are 4113..4116).
# SUPERSEDED (track G g7 below attributes the same strip to the loader's `- LOADING -` overlay and allows it by name; MIN_FIRST 2062 -> 4113).
# MAX_START 83 unchanged and exact (82 prints the improved line), TRACE/WIN/MILESTONES unchanged
# (9954/9954/30; TRACE/WIN are at the replay's end, so they cannot rise).
# Re-measured by track G g7 (record 2026-10-14-reverse-g §G7): the strip is the SAME presentation
# as gp-u9-win's 2209 — the same glyph fingerprint (de1c1bb2..) — at U10's entry: port 1685's 166
# px over the mode-0x12 screen (port 1686, the f=0xB9A entry frame), the mask from port 1684 (the
# palette pair is swapped against U9's) — and is ALLOWED BY NAME in OVERLAY_ALLOWED. With it active
# every content-bearing capture frame 83..4112 is explained and the first unexplained is 4113
# (raw 7858): nearest port 3444 (f=0x1502, mode 0x0D), rows 166..199, x 0..319 (9732 px) — the
# poked final's death animation (the DEATH_DONE pokes' mode-0x0D region, §W.9's unclaimed death
# streams), NOT allowed: a genuine unexplained frame the ratchet stops at. MIN_FIRST 2062 -> 4113;
# 4114 fails (the exact pin). MAX_START 83 unchanged and exact (82 prints the improved line),
# TRACE/WIN/MILESTONES unchanged (9954/9954/30; TRACE/WIN are at the replay's end, so they cannot
# rise). The pixel claim now reaches f=0x1502 (the final's 4th death) instead of stopping at the
# strip (f=0xB9A).
# BEFORE Task 3 (superseded by the next block): 4113 was the first unexplained until the span-writer
# fix explained it; 4114 no longer fails, and the pixel claim now reaches f=0x26E1.
# Re-measured by gameplay truth II Task 3 (record 2026-10-20-gameplay-truth-ii §W1): 4113..4134 were
# a REAL port divergence in the RLE span writers — the bank is added as a dword (0x81110/0x81010),
# so a byte sum above 0xFF carries into the next pixel of the four-pixel group, and a run colour's
# 2/3-byte tail is a carried word for runs of 14+ pixels (0x585F0 n=14 is the first word-tail
# writer, 0x58600 n=15; n=13 is one dl) but dl repeated below that (0x5855C..0x585C0), with the
# same threshold in the mirror fill table (0x52284/0x52294). The port added the bank byte-wise
# (sprite.c rle_row / copy_run) and the mirrored span wrote one byte for every pixel; the fix also
# reproduces the hflip tables 0x80D0C/0x80E0C (high-end dwords, low-end dl tail, bswapped literal).
# With the fix the two dead fighters' flash-palette frames (port 3444..3463) match and
# every capture frame 83..5436 is explained; the first unexplained is 5437 (raw 13279) — the
# capture's post-script tail (§W1 run 2, the port's replay ends at f=0x26E1), NOT allowed:
# MIN_FIRST 4113 -> 5437 and 5438 fails (the exact pin). The window REACH rises 3444 -> 4662
# (the highest port frame the capture exhibits; the dump's last frame index is 4663, f=0x26E1,
# and is not exhibited) because the enforced run no longer stops at 4113; 4663 fails.
# MAX_START 83 unchanged and exact (82 prints the improved line), TRACE/WIN/MILESTONES unchanged.
GP_ENDING_MIN_FIRST = 5437
GP_ENDING_TRACE_MIN_FIRST = 9954
GP_ENDING_MAX_START = 83
GP_ENDING_MILESTONES = 30
GP_ENDING_WIN_MIN_FIRST = 9954
GP_ENDING_CAPTURE_SHA256 = a88de48ad39e90df1e3d3329fbd5aa876c69a08484fa22c66db8deebd3feff38
GP_ENDING_CAPTURE_FRAMES = 5704
# MIN_REACH (sub-project A §A1): the highest port frame any classified capture frame
# exhibits. Measured 2026-10-21 by gameplay truth II Task 3 after the §W1 span-writer fix:
# 4662 = the highest port frame the capture exhibits (the dump's last frame is index 4663,
# f=0x26E1, and is not exhibited) — the enforced run no longer stops at capture 4113.
# Before it: 3444 (4113's nearest port, the first unexplained).
# Raise it when gp_compare prints "improved: raise the pin".
GP_ENDING_MIN_REACH = 4662
# The port dump is removed after its comparison unless GP_WIN_KEEP=1 (as gp-modes-one's
# GP_MODES_KEEP; record §W.10: 129 MB and 302 MB of /tmp).
GP_WIN_KEEP ?=
gp-win-oracle: build ## Gameplay U9 oracle: win-path evidence, frame/trace/milestone/win ratchets on data/k11-captures/gp-u9-win (skips without it)
	@echo "== gameplay oracle: gp-u9-win (the win path under pokes; plan U9/U10) =="
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
			--min-reach "$(GP_$(GP_WIN_ID)_MIN_REACH)" \
			--capture-sha256 "$(GP_$(GP_WIN_ID)_CAPTURE_SHA256)" --capture-frames "$(GP_$(GP_WIN_ID)_CAPTURE_FRAMES)" && \
		$(PYTHON) tools/gp_win.py path --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) \
			--port $(GP_DUMP)/$(scenario) --min-milestones "$(GP_$(GP_WIN_ID)_MILESTONES)" \
			--win-min-first "$(GP_$(GP_WIN_ID)_WIN_MIN_FIRST)" --capture-sha256 "$(GP_$(GP_WIN_ID)_CAPTURE_SHA256)"; \
		rc=$$?; [ -n "$(GP_WIN_KEEP)" ] || rm -rf $(GP_DUMP)/$(scenario); exit $$rc; fi

# Gameplay truth II U13 (plan docs/superpowers/plans/2026-10-20-gameplay-truth-ii.md Task 5;
# gp_session.WIN_MENU1 and gp_win.MILESTONES['gp-u13-ending2']): gp-u13-ending2, the ending of
# a character other than 0. P1 confirms character-select cell 3 (class 1, BLIZZARD): the plan's
# one p1.right is corrected by the raw (0xC8882 = 00 02 03 01 06 04 05, indexed by the cursor at
# 0x43D6E/0x43D74), so cell 1 is class 2 TALON and BLIZZARD needs three (record gameplay-u5
# §C5.4/§C5.7). Its first poke writes BLIZZARD's land marks (0x81, not u10's SAURON 0x80:
# 0x41C28 state 1 tallies (mark & 0x7F) == DS_0010782A[side] | side << 6, 0x41D39/0x41D5C), and
# the rest are gp-u10-ending's from its second poke on; the ending row is therefore
# _all(ad4=0, c0=1) rather than SAURON's c0 = 0. Measured on the clean Task 7 capture after
# its own loader-overlay recipe (tools/gp_compare.py): the window starts at capture 96
# (raw 1739), and every content-bearing capture frame through 5890 is explained by name —
# 2188 by the loader's glyph overlay, 4248 (raw 7887) and 4320 (raw 8019) as three-present
# scan-outs, 5120 (raw 8839) as a four-row band (record 2026-10-22-pixel-and-proof-closure
# §P1, §P2, §P3). The first unexplained is 5891 (raw 14361); 5891..5903 are the post-script
# tail (the replay's dump ends at f=0x271B and its trace at f=0x271D, X f=10013; record §P1.6)
# and are NOT claimed: MIN_FIRST = 5891, and 5892 fails. The enforced comparison's window
# reach is port 4939 (the highest port frame any classified capture frame exhibits through
# capture 5890; report mode's reach is the same because the unexplained tail is the capture's
# last frames), so MIN_REACH = 4939 and 4940 fails. Trace and win fields agree through
# f=10013, so their end pins are 10014. gp_win.py finds 30/30 milestones including BLIZZARD
# c0=1, and the capture has 5904 frames with this poll.log sha256 (unchanged since the
# capture: e581916f..2b69c6).
# An absent capture skips (gp-replay's GP_OPTIONAL=1, gp_win.py's no-poll.log skip); a present
# capture with empty/changed pins fails. Raise MIN_FIRST/TRACE_MIN_FIRST/MIN_REACH/MILESTONES/
# WIN_MIN_FIRST when they improve.
GP_U13_MIN_FIRST = 5891
GP_U13_TRACE_MIN_FIRST = 10014
GP_U13_MAX_START = 96
GP_U13_MIN_REACH = 4939
GP_U13_CAPTURE_SHA256 = e581916fa0a8e23a2ea52519aa0b358e4bc63084ce9defa0e1f733f3d12b69c6
GP_U13_CAPTURE_FRAMES = 5904
GP_U13_MILESTONES = 30
GP_U13_WIN_MIN_FIRST = 10014
gp-u13-oracle: build ## Gameplay U13 oracle: gp-u13-ending2's non-character-0 ending (skips without data/k11-captures/gp-u13-ending2)
	@echo "== gameplay oracle: gp-u13-ending2 (BLIZZARD's ending under pokes) =="
	@$(PYTHON) -m unittest tools.tests.test_gp_win
	@if [ ! -d $(K11_CAPTURES)/gp-u13-ending2 ]; then echo "gp-u13-oracle: no capture at $(K11_CAPTURES)/gp-u13-ending2 (skipped)"; else \
		$(PYTHON) tools/gp_win.py check --scenario gp-u13-ending2 --capture $(K11_CAPTURES)/gp-u13-ending2 \
			--capture-sha256 "$(GP_U13_CAPTURE_SHA256)" && \
		$(MAKE) --no-print-directory gp-replay scenario=gp-u13-ending2 GP_OPTIONAL=1 && \
		$(PYTHON) tools/gp_compare.py --scenario gp-u13-ending2 --capture $(K11_CAPTURES)/gp-u13-ending2 \
			--port $(GP_DUMP)/gp-u13-ending2 --min-first "$(GP_U13_MIN_FIRST)" \
			--trace-min-first "$(GP_U13_TRACE_MIN_FIRST)" --max-start "$(GP_U13_MAX_START)" \
			--min-reach "$(GP_U13_MIN_REACH)" \
			--capture-sha256 "$(GP_U13_CAPTURE_SHA256)" --capture-frames "$(GP_U13_CAPTURE_FRAMES)" && \
		$(PYTHON) tools/gp_win.py path --scenario gp-u13-ending2 --capture $(K11_CAPTURES)/gp-u13-ending2 \
			--port $(GP_DUMP)/gp-u13-ending2 --min-milestones "$(GP_U13_MILESTONES)" \
			--win-min-first "$(GP_U13_WIN_MIN_FIRST)" --capture-sha256 "$(GP_U13_CAPTURE_SHA256)"; \
		rc=$$?; [ -n "$(GP_WIN_KEEP)" ] || rm -rf $(GP_DUMP)/gp-u13-ending2; exit $$rc; fi

# Gameplay truth II U14 (plan docs/superpowers/plans/2026-10-20-gameplay-truth-ii.md Task 6;
# gp_session.SCENARIOS['gp-u14-win-live'] and gp_win.MILESTONES['gp-u14-win-live']): a match
# won by fought hits, without the WIN_MENU pokes (spec W5). The menu path is gp-idle-loss's
# with the character-select confirm of character 0; two rounds of alternating 1A/1B presses
# (tools/gp_moves.py's rows, UNMEASURED) aim at the CPU's KO byte. Its claim is the poked wins'
# narrow one plus what a poke could not claim: the game itself raises P2's +0x5A to 0x78,
# reaches the same mode-8/mode-9 entries and the land mark, and gp_win.py check requires an S
# record with the death-done byte b0c = 1 at or after the round-2 KO (mode 9) with no
# DEATH_DONE poke — the game's own 0x37EA0 -> 0x37FF2 write that U9/U10 poked (spec W5). The
# byte is not cleared between rounds (only mode 0x17's setup 0x271E0 and the final's consumers
# clear it), so the rule proves a game death animation ended by the match-end screen, not that
# round 2's specifically did; the frames and the WIN trace carry which animation ran. Which
# presses land and the round lengths are Task 11's measurement; spec W5's shared-code limit
# still applies — the same path's gp-u9-win (first mode-6 S f=0x480) and gp-u10-ending (f=0x479)
# captures have c1 = 6 (CHAOS), afc = 5, so the KO'd fighter's death stream is not U10's final
# SAURON's; Task 11's own capture records both (SNAP fields) and records a raw surprise if
# they differ. The pins are EMPTY until data/k11-captures/gp-u14-win-live is captured and
# measured (Task 11): the target skips on the absent capture directory (`[ ! -d ]`), and a
# present capture with an empty pin fails first in gp_win.py check's sha_ok ("poll.log sha256
# ... != the pinned (unpinned)").
# Provenance goes here with the measured values: MIN_FIRST the first unexplained capture frame,
# TRACE_MIN_FIRST the first differing traced f, MAX_START the window's first capture frame,
# MIN_REACH the replay's last exhibited port frame, MILESTONES/WIN_MIN_FIRST gp_win.py path's
# two ratchets, and the capture identity (poll.log sha256 and the frame_*.raw.gz count). Raise
# MIN_FIRST/TRACE_MIN_FIRST/MIN_REACH/MILESTONES/WIN_MIN_FIRST when they improve.
GP_U14_MIN_FIRST =
GP_U14_TRACE_MIN_FIRST =
GP_U14_MAX_START =
GP_U14_MIN_REACH =
GP_U14_CAPTURE_SHA256 =
GP_U14_CAPTURE_FRAMES =
GP_U14_MILESTONES =
GP_U14_WIN_MIN_FIRST =
gp-u14-oracle: build ## Gameplay U14 oracle: gp-u14-win-live's fought win (skips without data/k11-captures/gp-u14-win-live)
	@echo "== gameplay oracle: gp-u14-win-live (a match won by fought hits) =="
	@$(PYTHON) -m unittest tools.tests.test_gp_win
	@if [ ! -d $(K11_CAPTURES)/gp-u14-win-live ]; then echo "gp-u14-oracle: no capture at $(K11_CAPTURES)/gp-u14-win-live (skipped)"; else \
		$(PYTHON) tools/gp_win.py check --scenario gp-u14-win-live --capture $(K11_CAPTURES)/gp-u14-win-live \
			--capture-sha256 "$(GP_U14_CAPTURE_SHA256)" && \
		$(MAKE) --no-print-directory gp-replay scenario=gp-u14-win-live GP_OPTIONAL=1 && \
		$(PYTHON) tools/gp_compare.py --scenario gp-u14-win-live --capture $(K11_CAPTURES)/gp-u14-win-live \
			--port $(GP_DUMP)/gp-u14-win-live --min-first "$(GP_U14_MIN_FIRST)" \
			--trace-min-first "$(GP_U14_TRACE_MIN_FIRST)" --max-start "$(GP_U14_MAX_START)" \
			--min-reach "$(GP_U14_MIN_REACH)" \
			--capture-sha256 "$(GP_U14_CAPTURE_SHA256)" --capture-frames "$(GP_U14_CAPTURE_FRAMES)" && \
		$(PYTHON) tools/gp_win.py path --scenario gp-u14-win-live --capture $(K11_CAPTURES)/gp-u14-win-live \
			--port $(GP_DUMP)/gp-u14-win-live --min-milestones "$(GP_U14_MILESTONES)" \
			--win-min-first "$(GP_U14_WIN_MIN_FIRST)" --capture-sha256 "$(GP_U14_CAPTURE_SHA256)"; \
		rc=$$?; [ -n "$(GP_WIN_KEEP)" ] || rm -rf $(GP_DUMP)/gp-u14-win-live; exit $$rc; fi

# Gameplay truth II U15 (plan docs/superpowers/plans/2026-10-20-gameplay-truth-ii.md Task 7;
# gp_session.SCENARIOS['gp-u15-join']): P2 joins the gp-idle-loss fight mid-round with F2.
# Mode 6's caller 0x2525F runs the join poll 0x28CC8: side 1's start mask DS_0009ACBC[1]
# is newly pressed in DS_001088E4, 0x28D13 sets b1f and 0x28D19 calls the credit spend
# 0x2CA7C, which 0x2CA93 skips while b1f != 0 (the free-credit join), and 0x28DA4 stores
# the join's own mode 0x17 for 0x78 frames (0x28E75). The scenario ends 60 frames into
# that window, so gp_twop.py check --join judges the join's evidence (the record before
# it mode 6, the join record mode 0x17, the joining side's start mask in the newly-
# pressed word, b1f 1 -> 3, no debit) instead of the two-human fight claim: no fight
# frame follows the join, and the CPU command block still writes the joining side's
# word in 0x17 (gp-keys-fight f=0x85A..0x85C: e2 = 0 while the pad holds 0x100). The
# pins below are measured on data/k11-captures/gp-u15-join (record gameplay-truth-ii §J7):
# the target skips on the absent capture directory (`[ ! -d ]`), and a capture whose
# poll.log sha256 differs fails in gp_compare's sha check. The capture missed game frames
# 0x832/0x833 (harness, spec §3.7), where F2's start bit rose, so the join check reads the
# press as the held edge across them (record §J7: the raw `new` bit was not sampled).
# Measured report (gp_compare --report --max-unexplained 0, replay of the capture's script
# to its X record f=0x86D): window from capture 90 (raw 1745), 707 classified (492 clean,
# 214 splice, 1 transition, 0 unexplained, 8 all-black), "0 unexplained through 804";
# trace/moves "0 differing through 2157". The replay has no first unexplained and no first
# trace difference, so MIN_FIRST and TRACE_MIN_FIRST are the window bounds (gp_compare's
# "exact pin": the end of the compared range, N unreachable above it), not a divergence:
# MIN_FIRST = 805 (frames through 804 compared) and TRACE_MIN_FIRST = 2158 (traced f
# through 2157 compared; the controller's 2157 passes too, and 2158 is gp_compare's
# own "N = 2158 is the exact pin"). Their +1 fails as "N is unreachable"; the claim is
# that the scripted end was reached with nothing unexplained inside the window.
# MAX_START = 90 is
# the window's first capture frame. MIN_REACH = 590 is the replay's last exhibited port
# frame (the report's "up to port 590"; it is not the capture's 805: the port's frame
# numbering differs and the window's interior and order are not claimed). The capture
# identity is the poll.log sha256 and the 805 frame_*.raw.gz.
GP_U15_MIN_FIRST = 805
GP_U15_TRACE_MIN_FIRST = 2158
GP_U15_MAX_START = 90
GP_U15_MIN_REACH = 590
GP_U15_CAPTURE_SHA256 = 5f7ab1d72c7893e6fd66e8db143b615e6d3b385122ae0a93b9037ad1ffe3664c
GP_U15_CAPTURE_FRAMES = 805
gp-u15-oracle: build ## Gameplay U15 oracle: gp-u15-join's mid-round join (skips without data/k11-captures/gp-u15-join)
	@echo "== gameplay oracle: gp-u15-join (a mid-round join) =="
	@$(PYTHON) -m unittest tools.tests.test_gp_twop
	@if [ ! -d $(K11_CAPTURES)/gp-u15-join ]; then echo "gp-u15-oracle: no capture at $(K11_CAPTURES)/gp-u15-join (skipped)"; else \
		$(PYTHON) tools/gp_twop.py check --join --capture $(K11_CAPTURES)/gp-u15-join && \
		$(MAKE) --no-print-directory gp-replay scenario=gp-u15-join GP_OPTIONAL=1 && \
		$(PYTHON) tools/gp_compare.py --scenario gp-u15-join --capture $(K11_CAPTURES)/gp-u15-join \
			--port $(GP_DUMP)/gp-u15-join --min-first "$(GP_U15_MIN_FIRST)" \
			--trace-min-first "$(GP_U15_TRACE_MIN_FIRST)" --max-start "$(GP_U15_MAX_START)" \
			--min-reach "$(GP_U15_MIN_REACH)" \
			--capture-sha256 "$(GP_U15_CAPTURE_SHA256)" --capture-frames "$(GP_U15_CAPTURE_FRAMES)"; \
		rc=$$?; [ -n "$(GP_WIN_KEEP)" ] || rm -rf $(GP_DUMP)/gp-u15-join; exit $$rc; fi

# Gameplay truth II U16 (plan docs/superpowers/plans/2026-10-20-gameplay-truth-ii.md Task 8;
# gp_session.SCENARIOS['gp-u16-twop2']): gp-twop's two-human start played past its script
# end to the match result (mode 0x09; mode 8 is the undecided round's result) and on to the
# conquered-lands screen (mode 0x12), with TALON's three finisher entries (keyboard
# 0x15/0x16/0x17 -> reactions 0x32/0x33/0x34, the starters 0x37640/0x37774/0x37898; 0x17..0x19
# are characters 0/1/3/4/5/6's entries) attempted on the defeated P2 in mode 9. Each starter's
# gate is the other slot's +0x42 bit 5 and +0x43 bit 3 (0x3766E/0x37678); 0x36F10's arm A
# sets both (`word [slot+0x42] |= 0x820`, 0x36FD4) for the side the match result
# DS_00104AD4 beat (0x36F71..0x36F9A), reached from the health sync 0x34BE8 and its
# position gate 0x36E2C, and DS_00105B3A (the config word's bit 8, 0x20C9F) is 0 on the
# all-zero CMOS. The capture reaches modes 0x09 and 0x12, but `gp_twop.py check --finisher`
# found no 0x32..0x34 reaction after the three attempted TALON entries. R2: the finisher
# reaction remains a named gap; `--finisher --allow-missing-reaction` enforces the two-human
# match and both end modes without claiming a reaction. The S records carry c0/c1/r0/r1/s0_43
# and all five fields agree through the replay end, so pin the moves claim too.
# Provenance (Task 8, record gameplay-truth-ii §J8):
#   MIN_FIRST 8069: after the byte-pinned scan-out at capture 4089 and loader overlay at
#     8066, the first unexplained frame is 8069 (raw 11594), nearest port 6816. The capture
#     tail from this point is not claimed; this is the first frame outside the reproduced
#     window, not a claim that its screen is correct.
#   TRACE_MIN_FIRST / MOVES_MIN_FIRST 8637: 0 differences through f=8636; 8637 is the
#     compared end + 1 and an exact reach pin.
#   MAX_START 100: the report's window starts at capture 100 (raw 1743).
#   MIN_REACH 6817: highest port frame exhibited before the first unexplained capture frame.
#   CAPTURE_SHA256 / CAPTURE_FRAMES: this capture's poll.log and 8091 raw.gz frame files.
# Raising either N pin, the reach or frame count by one, lowering MAX_START by one, or
# changing the sha must fail; remeasure all values for a replacement capture.
GP_U16_MIN_FIRST = 8069
GP_U16_TRACE_MIN_FIRST = 8637
GP_U16_MOVES_MIN_FIRST = 8637
GP_U16_MAX_START = 100
GP_U16_MIN_REACH = 6817
GP_U16_CAPTURE_SHA256 = 5d38ca1fc3a52bcd02d91df3d7f0ae94a0275f9531f8bd81ffacee5343d31513
GP_U16_CAPTURE_FRAMES = 8091
gp-u16-oracle: build ## Gameplay U16 oracle: gp-u16-twop2's extended two-human match and frame evidence (finisher reaction gap; skips without data/k11-captures/gp-u16-twop2)
	@echo "== gameplay oracle: gp-u16-twop2 (extended two-human match and frame evidence; finisher reaction gap) =="
	@$(PYTHON) -m unittest tools.tests.test_gp_twop
	@if [ ! -d $(K11_CAPTURES)/gp-u16-twop2 ]; then echo "gp-u16-oracle: no capture at $(K11_CAPTURES)/gp-u16-twop2 (skipped)"; else \
		$(PYTHON) tools/gp_twop.py check --finisher --allow-missing-reaction --capture $(K11_CAPTURES)/gp-u16-twop2 && \
		$(MAKE) --no-print-directory gp-replay scenario=gp-u16-twop2 GP_OPTIONAL=1 && \
		$(PYTHON) tools/gp_compare.py --scenario gp-u16-twop2 --capture $(K11_CAPTURES)/gp-u16-twop2 \
			--port $(GP_DUMP)/gp-u16-twop2 --min-first "$(GP_U16_MIN_FIRST)" \
			--trace-min-first "$(GP_U16_TRACE_MIN_FIRST)" --max-start "$(GP_U16_MAX_START)" \
			--moves-min-first "$(GP_U16_MOVES_MIN_FIRST)" \
			--min-reach "$(GP_U16_MIN_REACH)" \
			--capture-sha256 "$(GP_U16_CAPTURE_SHA256)" --capture-frames "$(GP_U16_CAPTURE_FRAMES)"; \
		rc=$$?; [ -n "$(GP_WIN_KEEP)" ] || rm -rf $(GP_DUMP)/gp-u16-twop2; exit $$rc; fi

# Host input truth U17 (plan docs/superpowers/plans/2026-10-21-host-input-truth.md Task 6;
# gp_session.SCENARIOS['gp-u17-device2'], record §H6.6/§H6.9): the K11 service-menu walk to MODIFY
# CONTROLS (0x31F24) with the K11 walk's own keys, one 5-frame Down, Esc (leave MODIFY CONTROLS) and
# Esc (leave the OPTIONS MENU, which resumes the master loop so the cleared word is in an S record).
# The Down arm (0x320CF -> 0x320ED `dev + 2`) is gated by `until = [0x101500] + 0x0C`, so the 83 ms
# hold is one step, 0 -> 2. The device write is MODIFY CONTROLS' own return path (0x32345
# config_keys_apply writes the record's device into BIOS+0x2D4, 0x1AE3B) -- i.e. on the FIRST Esc,
# not the OPTIONS MENU leave; that second Esc runs 0x2CBAF config_file_save (a no-op on the
# capture's read-only mount) and resumes the master loop. No pad answered the probe, so the ISR
# sampler's presence block (0x1BC4F..0x1BCE6) probes 0x4FBBB(3), gets 0 and clears the word on its
# next tick: P1 is the only selected side, so the P1-only arm's 0x1BCBF clears it (dev2 == 0;
# 0x1BC6F is the both-player arm's clear).
#
# The steps are WALL-paced ('after_wall'): a service-menu sub-loop presents frames through
# 0x2EA78 inside one game_frame, so the master frame counter and the spin predicate freeze and a
# frame-keyed step never comes due (the 2026-10-22 failed run, whose diagnosis is record §H6.9;
# the successful publish removed its staging directory). Only the wall clock reaches those
# screens. The frame-keyed replay cannot follow wall steps yet, so this oracle pins the measured
# capture-side evidence, the selection's screen evidence and the capture identity, and the
# frame/trace ratchets stay empty and unused until the driver's tick-based path lands (the K11
# driver's pump-hook model) -- a named gap, not a passing claim.
#
# The claim is narrow: no game-port axis or button is injected (the harness cannot), so the arms'
# evidence is the unit tests and the differential rows; this capture pins the clear, not the pad's
# answer.
#
# The measured pins (capture 2026-10-22, 155 frames, raw 1373..3669, wall 55.5 s; the target skips
# on the absent capture directory and every pin below fails when moved):
#   CLEAR_FRAME 761 (f=0x2F9): the first S record after the last injected key (the OPTIONS MENU
#                 leave at f=0x2F7) whose dev1 is 0 -- the master loop resumes on that leave and
#                 the sampler's no-pad clear has already run, so dev1/dev2 read 0 in every S record
#   DEVICE_BEFORE/DEVICE_FRAME 149/150 + their band sha256 + the band rows 40:46: the MODIFY
#                 CONTROLS device-label band (text row 6, `config_option_row`'s
#                 `text_cursor_set(base-8, 6, ...)`) before and after the step -- KEYBOARD's band
#                 (9dfc4df1..) then the 4 BUTTON JOYSTICK one (87141468..), with more ink after
#                 (1028 -> 1096 px). This is how `--device` is pinned on a wall-paced capture: the
#                 poll has no S record while a menu is open, so the VALUE comes from the screen,
#                 and an explicit `--device` without --unseen-selection-ok fails
#   SHA256       5c0a3b56.. of poll.log; FRAMES 155 (frame_*.raw.gz)
# GP_U17_DEVICE (2) is the scenario's DERIVED expectation (the one +2 step), not a measured pin:
# it is passed with --unseen-selection-ok so the tool records that the poll cannot confirm it, and
# the screen bands above are its evidence. A re-capture invalidates every pin (record §G.24).
GP_U17_DEVICE = 2
GP_U17_CLEAR_FRAME = 761
GP_U17_DEVICE_BEFORE = 149
GP_U17_DEVICE_FRAME = 150
GP_U17_DEVICE_ROWS = 40:46
GP_U17_DEVICE_BEFORE_SHA256 = 9dfc4df15b4b797073449209cb1c1d3fb721319ba8fa2f135c21ab1b5f335528
GP_U17_DEVICE_FRAME_SHA256 = 871414689bd920a87b68bec299889bc2c20667b62a9c4980cea1daf3cb930fff
GP_U17_MIN_FIRST =
GP_U17_TRACE_MIN_FIRST =
GP_U17_MAX_START =
GP_U17_MIN_REACH =
GP_U17_CAPTURE_SHA256 = 5c0a3b5610535f9b28739ea7a436104aabc3e2c740c97d40400080b5c41d6d5d
GP_U17_CAPTURE_FRAMES = 155
gp-u17-oracle: build ## Host input truth U17 oracle: the device-2 selection's no-pad clear, its screen evidence and the capture identity on gp-u17-device2 (skips without it)
	@echo "== gameplay oracle: gp-u17-device2 (the device-2 selection and the no-pad clear) =="
	@$(PYTHON) -m unittest tools.tests.test_gp_u17
	@if [ ! -d $(K11_CAPTURES)/gp-u17-device2 ]; then echo "gp-u17-oracle: no capture at $(K11_CAPTURES)/gp-u17-device2 (skipped)"; else \
		$(PYTHON) tools/gp_u17.py check --capture $(K11_CAPTURES)/gp-u17-device2 \
			--device "$(GP_U17_DEVICE)" --unseen-selection-ok \
			--clear-frame "$(GP_U17_CLEAR_FRAME)" \
			--device-before "$(GP_U17_DEVICE_BEFORE)" --device-frame "$(GP_U17_DEVICE_FRAME)" \
			--device-rows "$(GP_U17_DEVICE_ROWS)" \
			--device-before-sha "$(GP_U17_DEVICE_BEFORE_SHA256)" \
			--device-frame-sha "$(GP_U17_DEVICE_FRAME_SHA256)" \
			--capture-sha256 "$(GP_U17_CAPTURE_SHA256)" \
			--capture-frames "$(GP_U17_CAPTURE_FRAMES)"; \
		rc=$$?; \
		echo "gp-u17-oracle: the frame/trace ratchets wait for the tick-based replay path (wall-paced menu; named gap, record §H6.9)"; \
		exit $$rc; fi

gp-report: build ## Report-only gameplay comparison (scenario=gp-…): counts and first differences, no ratchet, exit 0
	@$(MAKE) --no-print-directory gp-replay scenario=$(scenario) GP_OPTIONAL=1
	@$(PYTHON) tools/gp_compare.py --report --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) --port $(GP_DUMP)/$(scenario) --max-unexplained "$(GP_REPORT_MAX)"

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
verify: build ## Full ladder: --check frames, oracle-required tests, every pixel and ratchet oracle, differential verification, E2 triage, symbols.h idempotence
	@echo "== the game dir's cmos (every capture ran on 2040 zero bytes; the port's drivers read it since track S) =="
	@$(PYTHON) -c "import sys; sys.path.insert(0, 'tools'); import k11_capture as k; print('cmos=%s' % k.check_cmos('$(GAME_DIR)'))"
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
	@echo "== save-reload oracle (boot 1's write and boot 2's load; skips without its capture) =="
	@$(MAKE) --no-print-directory save-reload-oracle
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
	@echo "== gameplay oracle: gp-u12-endurance (the team pass and fight; skips without its capture; record §J4) =="
	@$(MAKE) --no-print-directory gp-u12-oracle
	@echo "== gameplay oracle: gp-u13-ending2 (a non-character-0 ending; skips without its capture) =="
	@$(MAKE) --no-print-directory gp-u13-oracle
	@echo "== gameplay oracle: gp-u14-win-live (a fought win; skips without its capture) =="
	@$(MAKE) --no-print-directory gp-u14-oracle
	@echo "== gameplay oracle: gp-u15-join (a mid-round join; skips without its capture) =="
	@$(MAKE) --no-print-directory gp-u15-oracle
	@echo "== gameplay oracle: gp-u16-twop2 (extended two-human match and frame evidence; finisher gap; skips without its capture) =="
	@$(MAKE) --no-print-directory gp-u16-oracle
	@echo "== host input truth U17: the device-2 selection and the no-pad clear; skips without its capture =="
	@$(MAKE) --no-print-directory gp-u17-oracle
	@$(MAKE) --no-print-directory gp-win-oracle
	@$(MAKE) --no-print-directory gp-ending-oracle
	@$(MAKE) --no-print-directory diff-verify
	@$(MAKE) --no-print-directory entry-triage
	@echo "== k11 and gp tool unit tests =="
	PR_ORACLE_REQUIRED=1 $(PYTHON) -m unittest tools.tests.test_k11_fields tools.tests.test_k11_session tools.tests.test_k11_capture tools.tests.test_k11_compare tools.tests.test_save_reload tools.tests.test_gp_session tools.tests.test_gp_capture tools.tests.test_gp_compare tools.tests.test_gp_moves tools.tests.test_gp_keys tools.tests.test_gp_u17 tools.tests.test_title_pin
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
