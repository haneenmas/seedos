cmake_minimum_required(VERSION 3.16)
project(seedos LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- generate a tiny translation unit that depends on mem.hpp ---
# (Use "mem.hpp" — not "emu/mem.hpp" — because we add emu/ to the include path)
file(WRITE ${CMAKE_BINARY_DIR}/generated_mem.cpp
"#include \"mem.hpp\"\n
// tiny TU to force mem.hpp to be part of the emu library build
int __mem_header_anchor = 0;\n")

# --- emulator library ---
add_library(emu
    emu/cpu.cpp        emu/cpu.hpp
    emu/disasm.cpp     emu/disasm.hpp
    emu/elf.cpp        emu/elf.hpp
    emu/syscall.cpp    emu/syscall.hpp
    emu/trace.cpp      emu/trace.hpp
    emu/main.cpp       emu/main.hpp
   
    emu/sync.hpp       # header-only
    ${CMAKE_BINARY_DIR}/generated_mem.cpp
)
target_include_directories(emu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/emu)

# --- main executable (for your demos/REPL) ---
add_executable(seedos emu/main.cpp)
target_link_libraries(seedos PRIVATE emu)

# --- tests (optional) ---
include(CTest)
if (BUILD_TESTING)
  # A tiny sanity test: runs the --heap demo and looks for the banner line
  add_test(NAME demo_heap
           COMMAND $<TARGET_FILE:seedos> --heap)
  set_tests_properties(demo_heap PROPERTIES
    PASS_REGULAR_EXPRESSION "Hello, heap & timer!")
endif()
