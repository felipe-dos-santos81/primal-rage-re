cmake_minimum_required(VERSION 3.20)
project(prageport C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
add_compile_options(-Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing)

find_package(SDL3 REQUIRED CONFIG)

add_library(prage_core STATIC src/mem.c src/platform/res.c src/platform/gra.c src/platform/gfx.c src/platform/input.c)
target_include_directories(prage_core PUBLIC src)

add_executable(prageport src/main.c)
target_link_libraries(prageport PRIVATE prage_core SDL3::SDL3)

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
add_executable(run_tests tests/run_tests.c tests/test_scaffold.c tests/test_mem.c tests/test_le.c tests/test_res.c tests/test_gra.c tests/test_gfx.c tests/test_input.c)
target_include_directories(run_tests PRIVATE src tests)
target_link_libraries(run_tests PRIVATE prage_core)
add_test(NAME unit COMMAND run_tests WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/..")
