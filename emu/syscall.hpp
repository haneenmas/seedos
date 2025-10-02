#pragma once
#include <cstdint>
struct CPU;     // forward-declare (defined in cpu.hpp)
struct Memory;  // forward-declare (defined in mem.hpp)

// Handle an ECALL using simple IDs in a7 and args in a0/a1.
// Returns true if handled.
bool handle_ecall(CPU& cpu, Memory& mem);
