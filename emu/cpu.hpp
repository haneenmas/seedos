#pragma once
#include <cstdint>

// forward declare to avoid include cycles
class Memory;

struct CPU {
    uint32_t x[32]{};     // integer regs (x0..x31); x0 is hard-wired to 0
    uint32_t pc{0};       // program counter

    // syscall/stop state
    bool     halted{false};
    uint32_t exit_code{0};

    bool step(Memory& mem);  // run one instruction; updates pc/x[]
};
