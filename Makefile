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
gra ?= S16TITLE.GRA
chunk ?= 0

.PHONY: help deps build test verify check smk-oracle run clean \
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
	@echo "ok"

# ── Port · build and verify ──────────────────────────────────────────────────

build: ## Configure and build the SDL3 port (CMake → build/)
	@echo "Configuring $(PORT_DIR)/ ..."
	cmake -S $(PORT_DIR) -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

# Note: the Ghidra/title-screen oracles are deliberately untracked, so the suite
# SKIPS them unless requested. Pass oracle=1 (or use `verify`) to require them —
# and pass nothing at all otherwise, because the test treats even an empty
# PR_ORACLE_REQUIRED as "required".
test: build ## Run the assertion suite (oracle=1 requires the byte-exact oracles)
	@if [ -n "$(oracle)" ]; then \
		PR_ORACLE_REQUIRED=1 PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	fi

check: build ## Run N frames headless, writing frame_*.ppm/.pal/.idx (frames=60)
	@echo "Running $(frames) frames headless ..."
	./$(BUILD_DIR)/prageport --game-dir $(GAME_DIR) --check $(frames)
	@ls -1 frame_*.ppm 2>/dev/null | head -3
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

# The --check run must come first: test_gfx.c reads frame_0001/0009/0017/0025.idx
# from the CWD, so the ladder has to produce them (frames >= 25) before the suite
# consumes them — otherwise that four-frame comparison never runs.
verify: build ## Full ladder: --check frames, oracle-required tests, symbols.h idempotence
	@echo "== headless frames (must precede the tests that read frame_*.idx) =="
	./$(BUILD_DIR)/prageport --game-dir $(GAME_DIR) --check $(frames)
	@echo "== tests (oracles required; consume the captured frames) =="
	PR_ORACLE_REQUIRED=1 PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests
	@PR_ORACLE_REQUIRED=1 $(MAKE) --no-print-directory smk-oracle
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
	rm -f $(PORT_DIR)/tests/ghidra_data.bin $(PORT_DIR)/tests/title_screen_ref.ppm \
	      $(PORT_DIR)/tests/s16title_frame10.idx
	rm -f frame_*.ppm frame_*.pal frame_*.idx
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
