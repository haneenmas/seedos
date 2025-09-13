cmake_minimum_required(VERSION 3.16)
project(seedos CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(emu STATIC
    emu/cpu.hpp
    emu/mem.hpp
    emu/cpu.cpp
    emu/mem.cpp
    emu/disasm.hpp
    emu/disasm.cpp
    
)

add_executable(seedos emu/main.cpp)
target_link_libraries(seedos PRIVATE emu)
