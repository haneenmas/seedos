#include <iostream>
#include <iomanip>
#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"

static inline void put(Memory& m, uint32_t addr, uint32_t word){ m.store32(addr, word); }
static inline void show_next(const CPU& cpu, const Memory& ram){
    uint32_t inst = ram.load32(cpu.pc);
    std::cout << "PC=0x" << std::hex << std::setw(8) << std::setfill('0') << cpu.pc
              << "  " << disasm(inst) << std::dec << "\n";
}

int main(){
    Memory ram(64*1024);
    CPU cpu; cpu.pc = 0;

    // ---- caller ----
    put(ram, 0x0000, 0x00500513u);   // addi x10,x0,5     ; a0=5
    put(ram, 0x0004, 0x00C00593u);   // addi x11,x0,12    ; a1=12
    put(ram, 0x0008, 0x038000EFu);   // jal  x1, +56      ; call 0x0040 (ra=x1=0x000C)
    put(ram, 0x000C, 0x00060393u);   // addi x7,x12,0     ; copy result to x7

    // ---- callee at 0x0040: a2 = a0 + a1 ; return ----
    put(ram, 0x0040, 0x00B50633u);   // add  x12,x10,x11  ; a2 = a0+a1
    put(ram, 0x0044, 0x00008067u);   // jalr x0,x1,0      ; return

    for (int i = 0; i < 6; ++i) {
        show_next(cpu, ram);
        bool ok = cpu.step(ram);
        if (!ok) { std::cerr<<"Unsupported at PC=0x"<<std::hex<<cpu.pc<<std::dec<<"\n"; break; }
        std::cout<<" -> a0(x10)="<<cpu.x[10]<<" a1(x11)="<<cpu.x[11]
                 <<" a2(x12)="<<cpu.x[12]<<" x7="<<cpu.x[7]
                 <<"  pc=0x"<<std::hex<<cpu.pc<<std::dec<<"\n";
    }
    return 0;
}
