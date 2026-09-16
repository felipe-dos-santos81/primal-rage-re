# Decompiling `PRAGE.EXE` — steps and dependencies

Reproducible record of how `port/decomp/` was produced. Target: a 32-bit
DOS/4GW **bound Linear Executable** (see `../FORMATS.md`).

## Dependencies

| Dep | Version / path | Needed for |
|---|---|---|
| JDK 25 (Temurin) | `/Library/Java/JavaVirtualMachines/temurin-25.jdk/Contents/Home` | Ghidra 12.2 (`application.java.min=25`; JDK 21 is too old) |
| Ghidra | `~/ghidra_12.2_DEV` (`support/analyzeHeadless`) | loader + analyzer + decompiler |
| `ghidra-lx-loader` | `<ghidra>/Extensions/Ghidra/ghidra-lx-loader/lib/ghidra-lx-loader.jar` (also in `~/Library/ghidra/ghidra_12.2_DEV/Extensions/`); source: `~/code/mine/ghidra-bundle/lx-loader` | loads LE, maps objects, applies fixups |
| Python 3.12 + `capstone` | `pip` | `tools/*.py`, ad-hoc disasm checks |
| `dosbox-x` 2026.08.31 | Homebrew | ground-truth memory/behaviour |
| git | system | lx-loader source / pinning |

Scripts: `_tools/ghidra_scripts/*.java` (passed via `-scriptPath`/`-postScript`).

```bash
export JAVA_HOME=/Library/Java/JavaVirtualMachines/temurin-25.jdk/Contents/Home
G=/Users/felipe.dos.santos/ghidra_12.2_DEV/support/analyzeHeadless
mkdir -p _tools/ghidra_proj          # analyzeHeadless needs the parent dir to exist
```

## Steps

1. **Identify the binary.** `file` → *LE for MS-DOS, DOS4GW DOS extender
   (embedded)*; strings show WATCOM C/C++32. `tools/le_info.py` finds the LE
   header at `0x290A4` and dumps the object table (code `0x10000`/100 pages,
   data `0x80000`/113 pages, entry `0x5245C`).
2. **Abandon manual image extraction.** The DOS/4GW page data is not at
   `LE + data_pages_offset`; the object page map + fixups are needed. (Several
   probe scripts were written and discarded.)
3. **Ground truth via dosbox-x** (optional but decisive): run the game with the
   guest RAM memory-mapped to a file, then align file↔memory with known strings.
   ```
   dosbox-x -conf /tmp/prage.conf -fastlaunch       # [dosbox] memory file = /tmp/prage.mem
   ```
   Gave the constant per-object deltas (code `file+0x19E1AC`, data
   `file+0x19F1AC`) and confirmed the code/data regions.
4. **Install the LE loader** (`ghidra-lx-loader`) so Ghidra loads the EXE
   directly. Import + full analysis:
   ```bash
   $G _tools/ghidra_proj prage -import data/game/C/PRAGE.EXE -overwrite -scriptPath _tools/ghidra_scripts
   ```
   → `x86:LE:32:default`, `.object1 @ 0x10000`, `.object2 @ 0x80000`,
   entry `0x6245C`, **1338 functions**.
5. **Fix up the loaded program** (`FixupProgram.java`): the loader leaves
   `.object1` non-executable → `setExecute(true)`; disassemble + `createFunction`
   at the entry (`0x6245C` = `jmp 0x624D4`).
   ```bash
   $G _tools/ghidra_proj prage -process PRAGE.EXE -scriptPath _tools/ghidra_scripts \
      -postScript FixupProgram.java
   ```
6. **Re-analyze** (now that the code object is executable) and **export**
   everything in one pass:
   ```bash
   $G _tools/ghidra_proj prage -process PRAGE.EXE -scriptPath _tools/ghidra_scripts \
      -postScript FixupProgram.java \
      -postScript ExportDecomp.java port/decomp/prage.c 90 \
      -postScript ExportMeta.java port/decomp/
   ```
   → **1352 functions, 0 decompile failures**; `prage.c` (1.5 MB),
   `prage.functions.csv`, `prage.strings.csv` (225), `prage.symbols.csv`,
   `prage.imports.csv` (0 — DOS4GW uses DPMI/DOS interrupts, no imports).
7. **Call graph + clustering** (for the subsystem map):
   ```bash
   $G _tools/ghidra_proj prage -process PRAGE.EXE -noanalysis -scriptPath _tools/ghidra_scripts \
      -postScript ExportCallGraph.java port/decomp/prage.calls.csv
   python3 tools/callgraph.py port/decomp        # -> port/decomp/prage.clusters.txt
   ```

Other tools: `tools/le_info.py`, `tools/gra_headers.py`, `tools/gra_chunks.py`.

## Gotchas

* Ghidra 12.2 requires **JDK ≥ 25**; set `JAVA_HOME` explicitly.
* `analyzeHeadless` aborts if the project's parent directory does not exist.
* `.object1` is imported **without execute permission** → no code analysis
  until `setExecute(true)` (hence the two-pass import/analyse flow).
* The LE entry is a 2-byte `jmp 0x624D4` immediately followed by the
  `WATCOM C/C++32 Run-Time system` **string**; Ghidra disassembles that string
  as code. Ignore it — the real startup is `0x624D4`.
* The loader also maps the raw file as an `.image` overlay; its
  `FUN_.image__*` symbols are **analysis noise** (145 of 1352) — filter them.
* DOS4GW **relocates and fixes up** the image, so file bytes ≠ memory bytes
  wherever fixups live; align with the loader or dosbox-x, never by raw diff.
* Re-running scripts: use `-process PRAGE.EXE -noanalysis` so analysis is not
  repeated.
