#pragma once
#include <cstdint>

class Memory;

class CPU {
public:
    // simple architectural state
    uint32_t pc      = 0;
    uint32_t x[32]   = {0};   // x0..x31
    // accounting
    uint64_t instret = 0;
    uint64_t cycles  = 0;
    // cooperative/preemptive slice
    int      quantum = 0;
    // exit & halt
    bool     halted  = false;
    int      exit_code = 0;
    int      tid       = 0;   // optional “thread id” used by demos

    // traps (wow #2)
    enum class Trap { None, Illegal, MisalignedLoad, MisalignedStore, AccessFault, Breakpoint };
    Trap last_trap = Trap::None;

public:
    // Execute one instruction; returns false if halted
    bool step(Memory& mem);
};
