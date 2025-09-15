#include <iostream>
#include <iomanip>
#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"

static inline void put(Memory& m, uint32_t addr, uint32_t word){ m.store32(addr, word); }
<<<<<<< HEAD

=======
>>>>>>> d6aed02 (feat(emu): jal/jalr + or latest changes)
static inline void show_next(const CPU& cpu, const Memory& ram){
    uint32_t inst = ram.load32(cpu.pc);
    std::cout << "PC=0x" << std::hex << std::setw(8) << std::setfill('0') << cpu.pc
              << "  " << disasm(inst) << std::dec << "\n";
}

int main(){
    Memory ram(64*1024);
    CPU cpu; cpu.pc = 0;

<<<<<<< HEAD
    // Program:
    // 0x0000: addi x1,x0,5
    // 0x0004: addi x2,x1,10
    // 0x0008: lui  x3,0x12          ; x3 = 0x00012000
    // 0x000C: addi x3,x3,34         ; x3 = 0x00012022
    // 0x0010: add  x4,x1,x2         ; x4 = 5 + 15 = 20
    put(ram, 0x0000, 0x00500093u);   // addi x1,x0,5
    put(ram, 0x0004, 0x00a08113u);   // addi x2,x1,10
    put(ram, 0x0008, 0x000121B7u);   // lui  x3,0x12
    put(ram, 0x000C, 0x02218193u);   // addi x3,x3,34
    put(ram, 0x0010, 0x00208233u);   // add  x4,x1,x2

    for(int i=0;i<5;++i){
        show_next(cpu, ram);
        bool ok = cpu.step(ram);
        if(!ok){ std::cerr<<"Unsupported at PC="<<cpu.pc<<"\n"; return 1; }
        // show a few key regs after each step
        std::cout<<" -> x1="<<cpu.x[1]<<" x2="<<cpu.x[2]<<" x3="<<cpu.x[3]<<" x4="<<cpu.x[4]
                 <<"  pc="<<cpu.pc<<"\n";
=======
    // --- Caller at 0x0000 ---
    // a0=x10=5, a1=x11=12
    put(ram, 0x0000, 0x00500513u);   // addi x10,x0,5
    put(ram, 0x0004, 0x00C00593u);   // addi x11,x0,12

    // Call function at 0x0040: jal x1, +0x38 (from PC=0x0008 to 0x0040)
    put(ram, 0x0008, 0x038000EFu);   // jal x1, +56   (x1=return addr 0x000C)

    // After return, copy result (x12 -> x7) so we can see it
    put(ram, 0x000C, 0x00060393u);   // addi x7,x12,0

    // --- Callee at 0x0040 ---
    // x12 = x10 + x11 ; return to x1
    put(ram, 0x0040, 0x00B50633u);   // add  x12,x10,x11
    put(ram, 0x0044, 0x00008067u);   // jalr x0,x1,0   (return)

    // Run enough steps to see call/return and the copy
    for (int i = 0; i < 6; ++i) {
        show_next(cpu, ram);
        bool ok = cpu.step(ram);
        if (!ok) { std::cerr<<"Unsupported at PC=0x"<<std::hex<<cpu.pc<<std::dec<<"\n"; break; }
        std::cout<<" -> a0(x10)="<<cpu.x[10]<<" a1(x11)="<<cpu.x[11]
                 <<" a2(x12)="<<cpu.x[12]<<" x7="<<cpu.x[7]
                 <<"  pc=0x"<<std::hex<<cpu.pc<<std::dec<<"\n";
>>>>>>> d6aed02 (feat(emu): jal/jalr + or latest changes)
    }
    return 0;
}
