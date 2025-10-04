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

# --- Memory TU: use emu/mem.cpp if it exists; otherwise compile a tiny shim ---
set(MEM_CPP "${CMAKE_SOURCE_DIR}/emu/mem.cpp")
if (EXISTS "${MEM_CPP}")
  list(APPEND EMU_SOURCES "${MEM_CPP}")
else()
  set(GENERATED_MEM_CPP "${CMAKE_BINARY_DIR}/generated_mem.cpp")
  file(WRITE "${GENERATED_MEM_CPP}" 
"#if __has_include(\"emu/mem.hpp\")
  #include \"emu/mem.hpp\"
#elif __has_include(\"mem.hpp\")
  #include \"mem.hpp\"
#else
  #error \"mem.hpp not found (expected in repo under emu/) \"
#endif
")
  list(APPEND EMU_SOURCES "${GENERATED_MEM_CPP}")
endif()

# --- library ---
add_library(emu ${EMU_SOURCES})

# make both include styles work: "mem.hpp" and "emu/mem.hpp"
target_include_directories(emu PUBLIC
  ${CMAKE_SOURCE_DIR}/emu
  ${CMAKE_SOURCE_DIR}
)

# --- main app ---
add_executable(seedos emu/main.cpp)
target_link_libraries(seedos PRIVATE emu)

# --- optional tests (only if files exist) ---
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

    add_test(NAME demo_heap COMMAND seedos --heap)
    set_tests_properties(demo_heap PROPERTIES PASS_REGULAR_EXPRESSION "Hello, heap & timer!")
  endif()
endif()
