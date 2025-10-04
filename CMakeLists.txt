cmake_minimum_required(VERSION 3.16)

project(seedos LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----- library: only .cpp files that truly exist in emu/ -----
add_library(emu
  emu/cpu.cpp
  emu/disasm.cpp
  emu/trace.cpp
  emu/elf.cpp
  emu/syscall.cpp
)
target_include_directories(emu PUBLIC emu)

# ----- app -----
add_executable(seedos
  emu/main.cpp
)
target_link_libraries(seedos PRIVATE emu)

# ---- tests ----
option(BUILD_TESTING "Build tests" ON)
include(CTest)
if (BUILD_TESTING)
  add_executable(seedos_tests
    tests/test_cpu.cpp
    tests/test_util.cpp
  )
  target_include_directories(seedos_tests PRIVATE emu tests)
  target_link_libraries(seedos_tests PRIVATE emu)

  add_test(NAME unit COMMAND seedos_tests)
endif()

