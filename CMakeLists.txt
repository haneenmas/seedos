cmake_minimum_required(VERSION 3.16)
project(seedos CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(emu STATIC
  emu/cpu.hpp  emu/cpu.cpp
  emu/mem.hpp
  emu/disasm.hpp  emu/disasm.cpp
    emu/main.hpp  emu/main.cpp
    emu/trace.cpp  emu/trace.hpp
    emu/elf.cpp      emu/elf.hpp
    emu/syscall.cpp
  
)

target_include_directories(emu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/emu)
add_executable(seedos emu/main.cpp)
target_link_libraries(seedos PRIVATE emu)

# tests target (no gtest needed)
add_executable(seedos_tests
    tests/test_cpu.cpp
)
target_include_directories(seedos_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(seedos_tests PRIVATE emu)




