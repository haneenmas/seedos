#include <iostream>
#include <iomanip>
#include <cstdint>
#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"

static inline uint32_t enc_I(uint8_t op,uint8_t rd,uint8_t f3,uint8_t rs1,int32_t imm12){ uint32_t u=(uint32_t)(imm12&0xFFF); return (u<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op; }
static inline uint32_t ECALL(){ return 0x00000073; }
static inline void put32(Memory& m,uint32_t a,uint32_t w){ m.store32(a,w); }

int main(){
    Memory ram(64*1024);
    CPU cpu; cpu.pc = 0x0000;
    uint32_t PC = 0x0000;

    // Helper: set a7
    auto set_a7 = [&](uint32_t v, uint32_t at){
        put32(ram, at,   enc_I(0x13, 17, 0b000, 0, (int32_t)v)); // addi a7,x0,v
        put32(ram, at+4, ECALL());
    };

    // 1) p1 = malloc(16); print_u32(p1)
    put32(ram, PC+0x00, enc_I(0x13, 10, 0b000, 0, 16));   // a0=16
    set_a7(5, PC+0x04);                                   // a7=5 malloc -> a0=p1
    set_a7(1, PC+0x0C);                                   // print a0

    // 2) p2 = malloc(16); print_u32(p2)
    put32(ram, PC+0x14, enc_I(0x13, 10, 0b000, 0, 16));   // a0=16
    set_a7(5, PC+0x18);                                   // a0=p2
    set_a7(1, PC+0x20);                                   // print p2

    // 3) free(p1)
    // move a0 <- p1 by re-calling malloc(0) trick? Simpler: we kept p1 in a0 after print_u32? No.
    // We'll call malloc(0) as NOP, so instead: call free on p2, then re-malloc to show reuse?
    // Better: capture p1 in s1 (x9) and p2 in s2 (x18) using addi
    // Since we don't yet move regs easily, we’ll re-run the first malloc pattern and stash results.

    // (Re-do allocation but stash pointers:)
    // p1 = malloc(16) again, then free it, then malloc(24) → should return same ptr as that p1
    put32(ram, PC+0x28, enc_I(0x13, 10, 0b000, 0, 16));   // a0=16
    set_a7(5, PC+0x2C);                                   // a0=p1'
    // save p1' into s1 (x9): addi x9,a0,0
    put32(ram, PC+0x34, enc_I(0x13, 9, 0b000, 10, 0));    // x9 = a0
    // free(p1'): a0=x9
    put32(ram, PC+0x38, enc_I(0x13, 10, 0b000, 9, 0));    // a0 = x9
    set_a7(6, PC+0x3C);                                   // free(a0)

    // 4) p3 = malloc(24); print_u32(p3)  --> should equal p1'
    put32(ram, PC+0x44, enc_I(0x13, 10, 0b000, 0, 24));   // a0=24
    set_a7(5, PC+0x48);                                   // a0=p3
    set_a7(1, PC+0x50);                                   // print p3

    // exit(0)
    put32(ram, PC+0x58, enc_I(0x13, 10, 0b000, 0, 0));    // a0=0
    set_a7(0, PC+0x5C);                                   // exit

    // run quietly
    for(int i=0;i<500 && !cpu.halted; ++i){
        if(!cpu.step(ram)){ std::cerr<<"Unsupported at PC=0x"<<std::hex<<cpu.pc<<std::dec<<"\n"; break; }
    }

    std::cout << "[heap] base="<<ram.hbase()<<" brk="<<ram.brk()
              << "  instret="<<cpu.instret<<" cycles="<<cpu.cycles<<"\n";
    return (int)cpu.exit_code;
}
