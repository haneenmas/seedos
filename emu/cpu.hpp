#pragma once
#include <cstdint>

class Memory;

// A CPU *is* the thread context.
struct CPU {
    uint32_t x[32]{};        // x0..x31 (x0 stays 0)
    uint32_t pc{0};          // program counter

    // run state
    bool     halted{false};
    uint32_t exit_code{0};

    // perf counters
    uint64_t cycles{0};
    uint64_t instret{0};

    // scheduling knobs
    uint32_t quantum{0};     // 0 = no preemption, otherwise N instructions per timeslice
    uint32_t slice_count{0}; // retired in current slice
    bool     yielded{false}; // set by step() when quantum expires, or by ECALL 7

    bool step(Memory& mem);  // execute one instruction; updates counters/pc/x[]
};
