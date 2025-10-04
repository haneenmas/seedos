cmake_minimum_required(VERSION 3.16)

project(seedos LANGUAGES CXX)

# --- C++ standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- emulator sources (only .cpp here) ---
set(EMU_SOURCES
    emu/cpu.cpp
    emu/disasm.cpp
    emu/elf.cpp
    emu/syscall.cpp
    emu/trace.cpp
)

# --- Memory translation unit: use emu/mem.cpp if it exists; otherwise make a stub ---
set(MEM_CPP "${CMAKE_SOURCE_DIR}/emu/mem.cpp")
if (EXISTS "${MEM_CPP}")
  list(APPEND EMU_SOURCES "${MEM_CPP}")
else()
  # header-only fallback: compile a tiny TU that just includes the header
  set(GENERATED_MEM_CPP "${CMAKE_BINARY_DIR}/generated_mem.cpp")
  file(WRITE "${GENERATED_MEM_CPP}" "#include \"mem.hpp\" // header-only fallback\n")
  list(APPEND EMU_SOURCES "${GENERATED_MEM_CPP}")

  # Fail-fast if the header itself is missing from the repo
  if (NOT EXISTS "${CMAKE_SOURCE_DIR}/emu/mem.hpp")
    message(FATAL_ERROR "Missing required header: emu/mem.hpp")
  endif()
endif()

# --- build the library ---
add_library(emu ${EMU_SOURCES})
target_include_directories(emu PUBLIC emu)

# --- main app ---
add_executable(seedos emu/main.cpp)
target_link_libraries(seedos PRIVATE emu)

# --- optional tests: only add files that actually exist ---
include(CTest)
if (BUILD_TESTING)
  set(TEST_SOURCES)
  foreach(f IN LISTS
      "tests/test_cpu.cpp"
      "tests/test_util.cpp"
  )
    if (EXISTS "${CMAKE_SOURCE_DIR}/${f}")
      list(APPEND TEST_SOURCES "${f}")
    endif()
  endforeach()

  if (TEST_SOURCES)
    add_executable(seedos_tests ${TEST_SOURCES})
    target_link_libraries(seedos_tests PRIVATE emu)

    # Simple smoke test if you want one even without gtest:
    add_test(NAME demo_heap COMMAND seedos --heap)
    set_tests_properties(demo_heap PROPERTIES PASS_REGULAR_EXPRESSION "Hello, heap & timer!")
  endif()
endif()
