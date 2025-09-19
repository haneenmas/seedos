#include <iostream>
#include <iomanip>
#include <cstdint>
#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"

// encoders
static inline uint32_t enc_I(uint8_t op,uint8_t rd,uint8_t f3,uint8_t rs1,int32_t imm12){ uint32_t u=(uint32_t)(imm12&0xFFF); return (u<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op; }
static inline uint32_t enc_J(uint8_t op,uint8_t rd,int32_t off){ uint32_t u=(uint32_t)off; uint32_t i20=(u>>20)&1,i10_1=(u>>1)&0x3FF,i11=(u>>11)&1,i19_12=(u>>12)&0xFF; return (i20<<31)|(i19_12<<12)|(i11<<20)|(i10_1<<21)|(rd<<7)|op; }
static inline void put32(Memory& m,uint32_t a,uint32_t w){ m.store32(a,w); }
static inline void put8 (Memory& m,uint32_t a,uint8_t  b){ m.store8(a,b); }

static inline uint32_t ECALL(){ return 0x00000073; }
static inline uint32_t enc_LUI(uint8_t rd, uint32_t imm20) {
    return (imm20 << 12) | (rd << 7) | 0x37; // opcode for LUI is 0x37
}


int main(){
    Memory ram(64*1024);
    CPU cpu; cpu.pc = 0x0000;

    // Write "Hello, heap!\n" into data at 0x0800
    const char* msg = "Hello, heap!\n";
    uint32_t MSG = 0x0800;
    for(uint32_t i=0; msg[i]; ++i) put8(ram, MSG+i, (uint8_t)msg[i]);

    // Program:
    // 1) write_str(MSG, len)
    // 2) a0 = sbrk(+32)  -> returns old break
    // 3) print_u32(a0)   -> show heap base before growth
    // 4) exit(0)

    uint32_t PC = 0x0000;

    // a0 = MSG
    put32(ram, PC+0x00, enc_LUI(10, 0x1));                          // lui  x10,0x1   -> x10=0x1000
    put32(ram, PC+0x04, enc_I(0x13, 10, 0b000, 10, -0x800));        // addi x10,x10,-2048 -> 0x0800

    put32(ram, PC+0x08, enc_I(0x13, 11, 0b000, 0, 14));             // addi x11,x0,14

    // shift the following instructions forward by +0x04 (we inserted one extra at 0x00..0x04 above)
    put32(ram, PC+0x0C, enc_I(0x13, 17, 0b000, 0, 4));              // a7 = 4 (write_str)
    put32(ram, PC+0x10, ECALL());
    put32(ram, PC+0x14, enc_I(0x13, 10, 0b000, 0, 32));             // a0 = +32 (sbrk delta)
    put32(ram, PC+0x18, enc_I(0x13, 17, 0b000, 0, 3));              // a7 = 3 (sbrk)
    put32(ram, PC+0x1C, ECALL());                                   // returns old brk in a0
    put32(ram, PC+0x20, enc_I(0x13, 17, 0b000, 0, 1));              // a7 = 1 (print_u32)
    put32(ram, PC+0x24, ECALL());
    put32(ram, PC+0x28, enc_I(0x13, 10, 0b000, 0, 0));              // exit(0)
    put32(ram, PC+0x2C, enc_I(0x13, 17, 0b000, 0, 0));
    put32(ram, PC+0x30, ECALL());
    // run silently (we’ve printed via syscalls already)
    for(int i=0;i<200 && !cpu.halted; ++i){
        if(!cpu.step(ram)){ std::cerr<<"Unsupported at PC=0x"<<std::hex<<cpu.pc<<std::dec<<"\n"; break; }
    }

    double cpi = (cpu.instret==0) ? 0.0 : (double)cpu.cycles / (double)cpu.instret;
    std::cout << "[halted] exit="<<cpu.exit_code
              << "  instret="<<cpu.instret
              << "  cycles="<<cpu.cycles
              << "  CPI="<<std::fixed<<std::setprecision(2)<<cpi << "\n";
    return (int)cpu.exit_code;
}
