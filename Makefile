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
# must cross the attract to exercise the title/announcer/music assertions. The
# short `frames` default still drives `make check`'s attract-only smoke render.
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
        title-oracle attract-oracle frontend-capture frontend-oracle demo-oracle demo-fight-oracle

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
# first unexplained frame is >= it. It is NOT "the fight is reproduced": the window is
# not yet explained, so N is the measured first unexplained frame, raised as the port
# improves (the tool prints "ratchet improved ... raise N"). N = 866 was measured on
# 60ad24b plus the commit that adds this line: state 6's 0x20DF4 now calls 0x12750
# (0x20E33), which builds the 0xF0A78 node list the 0xBB254 flier's cb1 0x127C0
# pops, and the front-end driver seeds the frame counter DS_000EF6DC to the
# port's own boot-run count before state 2 (886), so 0x1282C's (& 0x3F) gate
# spawns the grey flier at f = 91 as captures 864/865 show. `make demo-oracle`
# printed
# "title_compare: demo: first unexplained captured frame 866 (raw 3773); 2745 in the window",
# and the demo-fight oracle printed "ratchet improved: first unexplained 866 > 864".
# (Before it, N = 864, measured on b915712; N = 860 on afa47b3; N = 859 on
# b2cb490; N = 858 on 594e4b9; N = 851 on dad2712.)
# The two counts differ by scope: 2745 is --demo's window, [866..3616] to the capture's
# end; 1019 is this oracle's fight window, [866..1884], cut at the first all-black frame.
# Both share the first unexplained frame, 866. The tool also fails if N > window end + 1
# or if the window collapses; a shrunk front-end window lowers fe_b+1, so the
# ratchet itself fails (first unexplained < N).
# At 866 == fe_b+1 the claim is currently only that the front-end window does not
# shrink and the window start does not move. Skips without the capture.
DEMO_FIGHT_MIN_FIRST = 866
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
verify: build ## Full ladder: --check frames, oracle-required tests, front-end + demo-fight ratchet oracles, symbols.h idempotence
	@echo "== headless frames (must precede the tests that read frames/frame_*.idx) =="
	./$(BUILD_DIR)/prageport --game-dir $(GAME_DIR) --check $(verify_frames)
	@echo "== tests (oracles required; consume the captured frames) =="
	PR_ORACLE_REQUIRED=1 PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests
	@PR_ORACLE_REQUIRED=1 $(MAKE) --no-print-directory smk-oracle
	@echo "== title oracle (pixel-exact) =="
	@PR_ORACLE_REQUIRED=1 $(MAKE) --no-print-directory title-oracle
	@echo "== front-end oracle (pixel-exact, states 3/4; enforced) =="
	@$(MAKE) --no-print-directory frontend-oracle
	@echo "== demo-fight oracle (ratchet on the first unexplained frame) =="
	@$(MAKE) --no-print-directory demo-fight-oracle
	@echo "== attract prefix oracle (pixel-exact) =="
	@PR_ORACLE_REQUIRED=1 $(MAKE) --no-print-directory attract-oracle
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
