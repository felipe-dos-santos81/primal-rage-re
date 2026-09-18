cmake_minimum_required(VERSION 3.20)
project(prageport C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
add_compile_options(-Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing)

find_package(SDL3 REQUIRED CONFIG)

add_library(prage_core STATIC src/mem.c src/platform/res.c src/platform/gra.c src/platform/sprite.c src/platform/render.c src/platform/gfx.c src/platform/input.c src/platform/audio/opl/opal.c src/platform/audio/opl/opl.c src/platform/audio/samples.c src/platform/audio/patches.c src/platform/audio/sequencer.c src/platform/audio/mixer.c src/platform/audio/ail.c src/platform/smacker.c src/host.c src/game/flow.c src/game/movie.c src/game/rng.c)
target_include_directories(prage_core PUBLIC src)
# Task 12 reversal: Task 6 made prage_core drop SDL3::SDL3 PUBLIC so run_tests
# (which had no SDL code) need not link SDL. host.c now lives in the library and
# is the real implementation the suite must exercise, so SDL must be linked.
# PRIVATE still confines the dependency to the link line; SDL headers stay in
# host.c/main.c only, which is the binding constraint, not the link.
target_link_libraries(prage_core PRIVATE SDL3::SDL3)

add_executable(prageport src/main.c)
target_link_libraries(prageport PRIVATE prage_core)

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

enable_testing()
add_executable(run_tests tests/run_tests.c tests/test_scaffold.c tests/test_mem.c tests/test_le.c tests/test_res.c tests/test_gra.c tests/test_gfx.c tests/test_input.c tests/test_host.c tests/test_flow.c tests/test_opl.c tests/test_samples.c tests/test_mixer.c tests/test_sequencer.c tests/test_ail.c tests/test_smacker.c tests/test_movie.c tests/test_sprite.c tests/test_render.c tests/test_rng.c)
target_include_directories(run_tests PRIVATE src tests)
target_link_libraries(run_tests PRIVATE prage_core)
add_test(NAME unit COMMAND run_tests WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")
