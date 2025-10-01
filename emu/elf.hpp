#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>
#include "mem.hpp"

// Returns entry point address after loading PT_LOAD segments into Memory.
uint32_t load_elf32_into_memory(const std::string& path, Memory& mem);

// Utility: slurp a whole file into a vector
std::vector<uint8_t> read_file(const std::string& path);
