cmake_minimum_required(VERSION 3.16)

project(seedos LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----- library: only .cpp files that truly exist in emu/ -----
add_library(emu
  emu/cpu.cpp
  emu/disasm.cpp
  emu/mem.cpp
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

# ----- tiny test via CTest (optional) -----
include(CTest)
if (BUILD_TESTING)
  add_test(NAME demo_heap COMMAND seedos --heap)
  set_tests_properties(demo_heap PROPERTIES
    PASS_REGULAR_EXPRESSION "Hello, heap & timer!")
endif()
