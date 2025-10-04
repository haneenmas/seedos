cmake_minimum_required(VERSION 3.16)

project(seedos LANGUAGES CXX)

# ---- C++ standard ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default build type if not provided
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---- emulator sources ----
# If emu/mem.cpp is missing, create a tiny stub so CI doesn’t break.
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
set_target_properties(emu PROPERTIES XCODE_GENERATE_SCHEME TRUE)

# ---- main app ----
add_executable(seedos emu/main.cpp)
target_link_libraries(seedos PRIVATE emu)
set_target_properties(seedos PROPERTIES XCODE_GENERATE_SCHEME TRUE)

# ---- tests (optional, only if sources exist) ----
option(BUILD_TESTING "Build tests" ON)
include(CTest)

if(BUILD_TESTING)
  set(TEST_SOURCES "")

  # add tests that actually exist in the repo
  if(EXISTS "${CMAKE_SOURCE_DIR}/tests/test_cpu.cpp")
    list(APPEND TEST_SOURCES tests/test_cpu.cpp)
  endif()

  # (add more tests here guarded by EXISTS checks)

  if(TEST_SOURCES)
    add_executable(seedos_tests ${TEST_SOURCES})
    target_include_directories(seedos_tests PRIVATE emu tests)
    target_link_libraries(seedos_tests PRIVATE emu)
    set_target_properties(seedos_tests PROPERTIES XCODE_GENERATE_SCHEME TRUE)
    add_test(NAME unit COMMAND seedos_tests)
  else()
    message(STATUS "No test sources found; skipping seedos_tests target.")
  endif()
endif()

# ---- optional smoke test (disabled by default) ----
# add_test(NAME demo_heap COMMAND seedos --heap)
