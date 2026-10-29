cmake_minimum_required(VERSION 3.20)
project(prageport C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
add_compile_options(-Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing)

# PORT: the coverage census instrument (record state-coverage §D1.9). When ON,
# prageport and run_tests link a PR_COVERAGE-compiled core whose seam macros and
# per-function PR_HIT lines record hits; diffrun always links the plain core,
# whose bytes must not change.
option(PR_COVERAGE "Instrument the port's per-function coverage hit counter" OFF)

find_package(SDL3 REQUIRED CONFIG)

set(PRAGE_CORE_SOURCES
  src/mem.c src/platform/res.c src/platform/gra.c src/platform/sprite.c
  src/platform/render.c src/platform/gfx.c src/platform/input.c
  src/platform/audio/opl/opal.c src/platform/audio/opl/opl.c
  src/platform/audio/samples.c src/platform/audio/patches.c
  src/platform/audio/pitch.c src/platform/audio/sequencer.c
  src/platform/audio/mixer.c src/platform/audio/ail.c
  src/platform/smacker.c src/host.c src/game/flow.c src/game/movie.c
  src/game/rng.c src/game/actors.c src/game/effects.c src/game/camera.c
  src/game/fighter.c src/game/fight.c src/game/config.c src/game/nameentry.c
  src/game/menu.c src/game/attract.c src/game/svcmenu.c)

add_library(prage_core STATIC ${PRAGE_CORE_SOURCES})
target_include_directories(prage_core PUBLIC src)
# Task 12 reversal: Task 6 made prage_core drop SDL3::SDL3 PUBLIC so run_tests
# (which had no SDL code) need not link SDL. host.c now lives in the library and
# is the real implementation the suite must exercise, so SDL must be linked.
# PRIVATE still confines the dependency to the link line; SDL headers stay in
# host.c/main.c only, which is the binding constraint, not the link.
target_link_libraries(prage_core PRIVATE SDL3::SDL3)

# The coverage-instrumented core: the same sources compiled with PR_COVERAGE,
# so the seam macros and per-function PR_HIT lines record hits. prageport and
# run_tests link it when the option is ON; diffrun always links prage_core.
if(PR_COVERAGE)
  add_library(prage_core_cov STATIC ${PRAGE_CORE_SOURCES})
  target_compile_definitions(prage_core_cov PRIVATE PR_COVERAGE)
  target_include_directories(prage_core_cov PUBLIC src)
  target_link_libraries(prage_core_cov PRIVATE SDL3::SDL3)
  set(PRAGE_APP_CORE prage_core_cov)
else()
  set(PRAGE_APP_CORE prage_core)
endif()

add_executable(prageport src/main.c)
if(PR_COVERAGE)
  target_compile_definitions(prageport PRIVATE PR_COVERAGE)
endif()
target_link_libraries(prageport PRIVATE ${PRAGE_APP_CORE})

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/symbols.h
  COMMAND ${CMAKE_COMMAND} -E env python3
          ${CMAKE_CURRENT_SOURCE_DIR}/../tools/gen_symbols.py
          ${CMAKE_CURRENT_SOURCE_DIR}/../port/decomp
          ${CMAKE_CURRENT_SOURCE_DIR}/src/symbols.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../tools/gen_symbols.py
          ${CMAKE_CURRENT_SOURCE_DIR}/../port/decomp/prage.functions.csv
          ${CMAKE_CURRENT_SOURCE_DIR}/../port/decomp/prage.c
  COMMENT "generating symbols.h")
add_custom_target(symbols DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/symbols.h)
add_dependencies(prage_core symbols)
if(PR_COVERAGE)
  add_dependencies(prage_core_cov symbols)
endif()

enable_testing()
# The registry lives in tests/test.h (TEST_CASES/TEST_DRIVERS); CONFIGURE_DEPENDS
# re-globs at build time, so a new tests/test_*.c needs no edit here.
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/test_*.c)
add_executable(run_tests tests/run_tests.c ${TEST_SOURCES})
target_include_directories(run_tests PRIVATE src tests)
if(PR_COVERAGE)
  target_compile_definitions(run_tests PRIVATE PR_COVERAGE)
endif()
target_link_libraries(run_tests PRIVATE ${PRAGE_APP_CORE})
add_test(NAME unit COMMAND run_tests WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")

# The port side of the differential harness (tools/diff_verify.py, spec
# 2026-09-30-reverse-completion-design §5). A tool, not a test: its file name does not match the
# tests/test_*.c glob above, so run_tests does not link it.
add_executable(diffrun tests/diff_runner.c)
target_link_libraries(diffrun PRIVATE prage_core)
