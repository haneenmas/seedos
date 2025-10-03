#pragma once
#include <cstdint>
class Memory;

struct CPU {
    // architectural state
    uint32_t x[32]{}; uint32_t pc{0};

    // runtime flags/counters
    bool halted{false}; uint32_t exit_code{0};
    uint64_t cycles{0}, instret{0};
    uint32_t quantum{0}, slice_count{0}; bool yielded{false};

    // scheduling metadata (not architectural)
    uint32_t tid{0};   // thread id (for prints/ownership if you want later)
    uint32_t prio{1};  // smaller number = higher priority

    bool step(Memory& mem);
    
};
