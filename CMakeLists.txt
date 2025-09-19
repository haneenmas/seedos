cmake_minimum_required(VERSION 3.16)
project(seedos CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(emu STATIC
  emu/cpu.hpp  emu/cpu.cpp
  emu/mem.hpp
  emu/disasm.hpp  emu/disasm.cpp
    emu/main.hpp  emu/main.cpp
  
)

target_include_directories(emu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/emu)

add_executable(seedos emu/main.cpp)
target_link_libraries(seedos PRIVATE emu)

