cmake_minimum_required(VERSION 3.16)
project(seedos LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library (implementation files only)
add_library(emu
    emu/cpu.cpp
    emu/disasm.cpp
    emu/mem.cpp
    emu/trace.cpp
    emu/elf.cpp
    emu/syscall.cpp
    # header-only helpers are included via target_include_directories
    # emu/spsc.hpp
    # emu/asm.hpp
)
target_include_directories(emu PUBLIC emu)

# App
add_executable(seedos
    emu/main.cpp
)
target_link_libraries(seedos PRIVATE emu)

# Tests (single runner that compiles all test .cpp files)
add_executable(seedos_tests
    tests/test_util.cpp
    tests/test_cpu.cpp
    tests/test_spsc.cpp
    tests/test_assembler.cpp
    tests/test_traps_break_uart.cpp
)
target_link_libraries(seedos_tests PRIVATE emu)

include(CTest)
if (BUILD_TESTING)
  add_test(NAME demo_heap COMMAND seedos --heap)
endif()
