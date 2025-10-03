// tests/test_util.hpp
#pragma once
#include <cstdint>
#include "emu/mem.hpp"
#include "emu/cpu.hpp"
#include "emu/disasm.hpp"

inline void put32(Memory& m, uint32_t a, uint32_t w) { m.store32(a, w); }
