cmake_minimum_required(VERSION 3.16)
project(seedos LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- library with emulator sources ----
add_library(emu
    emu/cpu.cpp
    emu/disasm.cpp
    emu/trace.cpp
    emu/elf.cpp
    emu/syscall.cpp
    # headers listed so they show in Xcode (optional)
    emu/cpu.hpp
    emu/disasm.hpp
    emu/trace.hpp
    emu/elf.hpp
    emu/syscall.hpp
    emu/mem.hpp
    emu/sync.hpp
    emu/spsc.hpp
    emu/asm.hpp
)
target_include_directories(emu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/emu)

# ---- main executable ----
add_executable(seedos
    emu/main.cpp
)
target_link_libraries(seedos PRIVATE emu)

# ---- tests executable (shows up as a separate scheme) ----
add_executable(seedos_tests
      tests/test_util.hpp
    tests/test_cpu.cpp
     tests/test_spsc.cpp
    tests/test_assembler.cpp
    tests/test_traps_break_uart.cpp
)
target_include_directories(seedos_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/emu
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
target_link_libraries(seedos_tests PRIVATE emu)

# Enable ctest and add one smoke test (optional)
enable_testing()
add_test(NAME unit_cpu COMMAND seedos_tests)
