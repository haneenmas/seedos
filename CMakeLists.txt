cmake_minimum_required(VERSION 3.16)

project(seedos LANGUAGES CXX)

# ---- C++ standard ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default build type if not provided
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---- Sources for the emulator library ----
# If you actually have emu/mem.cpp, we'll use it; otherwise generate a stub
set(MEM_CPP emu/mem.cpp)
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${MEM_CPP}")
  set(MEM_CPP "${CMAKE_BINARY_DIR}/generated_mem.cpp")
  file(WRITE "${MEM_CPP}" "#include \"emu/mem.hpp\"\n")
endif()

add_library(emu
  emu/cpu.cpp
  emu/disasm.cpp
  emu/trace.cpp
  emu/elf.cpp
  emu/syscall.cpp
  ${MEM_CPP}
)
target_include_directories(emu PUBLIC emu)

# Helpful on macOS Xcode projects (harmless elsewhere)
set_target_properties(emu PROPERTIES XCODE_GENERATE_SCHEME TRUE)

# ---- Main executable ----
add_executable(seedos
  emu/main.cpp
)
target_link_libraries(seedos PRIVATE emu)
set_target_properties(seedos PROPERTIES XCODE_GENERATE_SCHEME TRUE)

# ---- Tests ----
option(BUILD_TESTING "Build tests" ON)
include(CTest)

if(BUILD_TESTING)
  # Only use files that actually exist
  set(TEST_SOURCES tests/test_cpu.cpp)

  foreach(_f IN LISTS TEST_SOURCES)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${_f}")
      message(FATAL_ERROR "Required test source not found: ${_f}")
    endif()
  endforeach()

  add_executable(seedos_tests ${TEST_SOURCES})
  target_include_directories(seedos_tests PRIVATE emu tests)
  target_link_libraries(seedos_tests PRIVATE emu)
  set_target_properties(seedos_tests PROPERTIES XCODE_GENERATE_SCHEME TRUE)

  add_test(NAME unit COMMAND seedos_tests)
endif()

# ---- (Optional) very quick smoke test for CI without unit tests ----
# add_test(NAME demo_heap COMMAND seedos --heap)
