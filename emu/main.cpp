#include <iostream>
#include "cpu.hpp"
#include "mem.hpp"

static inline void put(Memory& m, uint32_t addr, uint32_t word) {
    m.store32(addr, word); // helper: write one 32-bit little-endian word
}

int main(){
    Memory ram(64 * 1024); // 64 KiB RAM
    CPU cpu; cpu.pc = 0;

    // Program:
    //   addi x1, x0, 5    ; x1 = 5        encoding 0x00500093
    //   addi x2, x1, 10   ; x2 = 15       encoding 0x00a08113
    put(ram, 0x0000, 0x00500093u);
    put(ram, 0x0004, 0x00a08113u);

    bool ok = cpu.step(ram);
    if(!ok){ std::cerr << "Unsupported at PC=0\n"; return 1; }
    std::cout << "After 1st step: x1=" << cpu.x[1] << " pc=" << cpu.pc << "\n";

    ok = cpu.step(ram);
    if(!ok){ std::cerr << "Unsupported at PC=4\n"; return 1; }
    std::cout << "After 2nd step: x2=" << cpu.x[2] << " pc=" << cpu.pc << "\n";
}
