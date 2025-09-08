#pragma once
#include <array>
#include <cstdint>

struct Memory; // forward declaration

// CPU holds architectural state: 32 regs + program counter.
struct CPU {
    std::array<uint32_t, 32> x{}; // x0..x31, x0 will be forced to 0
    uint32_t pc{0};

    CPU(){ x.fill(0); }
    bool step(Memory& mem); // execute one instruction at pc
};
