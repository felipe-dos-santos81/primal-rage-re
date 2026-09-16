cmake_minimum_required(VERSION 3.20)
project(prageport C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
add_compile_options(-Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing)

find_package(SDL3 REQUIRED CONFIG)

add_executable(prageport src/main.c src/mem.c)
target_include_directories(prageport PRIVATE src)
target_link_libraries(prageport PRIVATE SDL3::SDL3)

enable_testing()
add_executable(run_tests tests/run_tests.c tests/test_scaffold.c tests/test_mem.c tests/test_le.c src/mem.c)
target_include_directories(run_tests PRIVATE src tests)
add_test(NAME unit COMMAND run_tests)
