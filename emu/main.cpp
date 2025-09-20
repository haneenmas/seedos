#include <iostream>
#include <iomanip>
#include <cstdint>
#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"

// ---------- Tiny encoders ----------
static inline uint32_t enc_I(uint8_t op,uint8_t rd,uint8_t f3,uint8_t rs1,int32_t imm12){
    uint32_t u=(uint32_t)(imm12 & 0xFFF);
    return (u<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op;
}
static inline uint32_t enc_R(uint8_t f7,uint8_t rs2,uint8_t rs1,uint8_t f3,uint8_t rd,uint8_t op=0x33){
    return (f7<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op;
}
static inline uint32_t enc_B(uint8_t f3,uint8_t rs1,uint8_t rs2,int32_t off){
    uint32_t u=(uint32_t)off;
    uint32_t i12=(u>>12)&1,i10_5=(u>>5)&0x3F,i4_1=(u>>1)&0xF,i11=(u>>11)&1;
    return (i12<<31)|(i10_5<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(i4_1<<8)|(i11<<7)|0x63;
}
static inline uint32_t enc_LUI(uint8_t rd, uint32_t imm20){ return (imm20<<12)|(rd<<7)|0x37; }
static inline uint32_t enc_J(uint8_t op,uint8_t rd,int32_t off){
    uint32_t u=(uint32_t)off;
    uint32_t i20=(u>>20)&1,i10_1=(u>>1)&0x3FF,i11=(u>>11)&1,i19_12=(u>>12)&0xFF;
    return (i20<<31)|(i19_12<<12)|(i11<<20)|(i10_1<<21)|(rd<<7)|op;
}
static inline uint32_t ECALL(){ return 0x00000073; }
static inline void put32(Memory& m,uint32_t a,uint32_t w){ m.store32(a,w); }

// Load 32-bit constant into 'rd' using LUI+ADDI (safe for any value)
static inline void emit_li32(Memory& m, uint32_t at, uint8_t rd, uint32_t imm){
    // canonical RISC-V "li"
    uint32_t upper = (imm + 0x800) >> 12;              // rounded upper
    int32_t  lower = (int32_t)imm - (int32_t)(upper<<12);
    put32(m, at+0, enc_LUI(rd, upper));
    put32(m, at+4, enc_I(0x13, rd, 0b000, rd, lower)); // addi rd,rd,lower
}

// ---------- Two tiny programs in RAM ----------
// T1: prints 'A','B',...,'J' then exit(0)
static void emit_thread_A(Memory& m, uint32_t base){
    // i in x5, ch in x6
    put32(m, base+0x00, enc_I(0x13, 5, 0b000, 0, 0));      // i=0
    put32(m, base+0x04, enc_I(0x13, 6, 0b000, 0, 'A'));    // ch='A'
    // LOOP:
    uint32_t L = base+0x08;
    put32(m, L+0x00, enc_I(0x13,10,0b000,6,0));            // a0 = ch
    put32(m, L+0x04, enc_I(0x13,17,0b000,0,2));            // a7 = putchar
    put32(m, L+0x08, ECALL());
    put32(m, L+0x0C, enc_I(0x13, 6,0b000,6,1));            // ch++
    put32(m, L+0x10, enc_I(0x13, 5,0b000,5,1));            // i++
    // if (i < 10) goto LOOP
    put32(m, L+0x14, enc_B(0b100, 5, 0, (int32_t)L - (int32_t)(L+0x14))); // blt x5,x0? nope; need const 10 in x7
    // Fix: load 10 into x7 once, and branch on i<x7
    emit_li32(m, base+0x24, 7, 10);                        // x7 = 10 (placed after)
    put32(m, L+0x14, enc_B(0b100, 5, 7, (int32_t)L - (int32_t)(L+0x14))); // blt x5,x7,L
    // exit(0)
    put32(m, base+0x28, enc_I(0x13,10,0b000,0,0));         // a0=0
    put32(m, base+0x2C, enc_I(0x13,17,0b000,0,0));         // a7=exit
    put32(m, base+0x30, ECALL());
}

// T2: prints 'a','b',...,'j' and occasionally ECALL yield()
static void emit_thread_B(Memory& m, uint32_t base){
    put32(m, base+0x00, enc_I(0x13, 5,0b000,0,0));         // i=0
    put32(m, base+0x04, enc_I(0x13, 6,0b000,0,'a'));       // ch='a'
    uint32_t L = base+0x08;
    put32(m, L+0x00, enc_I(0x13,10,0b000,6,0));            // a0=ch
    put32(m, L+0x04, enc_I(0x13,17,0b000,0,2));            // a7=putchar
    put32(m, L+0x08, ECALL());
    put32(m, L+0x0C, enc_I(0x13, 6,0b000,6,1));            // ch++
    put32(m, L+0x10, enc_I(0x13, 5,0b000,5,1));            // i++
    // Optional cooperative yield every 3 chars:
    emit_li32(m, L+0x14, 7, 7);                            // a7=7
    put32(m, L+0x18, ECALL());                             // yield()
    // if (i < 10) goto L
    emit_li32(m, base+0x30, 8, 10);                        // x8=10
    put32(m, L+0x1C, enc_B(0b100, 5, 8, (int32_t)L - (int32_t)(L+0x1C))); // blt x5,x8,L
    // exit(0)
    put32(m, base+0x34, enc_I(0x13,10,0b000,0,0));
    put32(m, base+0x38, enc_I(0x13,17,0b000,0,0));
    put32(m, base+0x3C, ECALL());
}

int main(){
    Memory ram(64*1024);

    // Place two programs at different addresses
    uint32_t A_START = 0x0000;
    uint32_t B_START = 0x0100;
    emit_thread_A(ram, A_START);
    emit_thread_B(ram, B_START);

    // Create two "threads" (two CPU contexts) sharing the same RAM
    CPU A; A.pc = A_START; A.quantum = 20;  // preempt every 20 instructions
    CPU B; B.pc = B_START; B.quantum = 20;

    // Round-robin scheduler
    CPU* cur = &A;
    CPU* nxt = &B;

    while(!(A.halted && B.halted)){
        // run until the thread yields, halts, or we hit a safety cap
        for(int k=0;k<500 && !cur->halted && !cur->yielded; ++k){
            if(!cur->step(ram)){
                std::cerr << "Unsupported at PC=0x" << std::hex << cur->pc << std::dec << "\n";
                cur->halted = true;
                break;
            }
        }
        // swap on yield or halt
        if (cur->yielded || cur->halted){
            std::swap(cur, nxt);
        }
    }

    auto report = [](const char* name, const CPU& c){
        double cpi = (c.instret==0)?0.0:(double)c.cycles/(double)c.instret;
        std::cout << "\n["<<name<<"] exit="<<c.exit_code
                  << " instret="<<c.instret
                  << " cycles="<<c.cycles
                  << " CPI="<<std::fixed<<std::setprecision(2)<<cpi << "\n";
    };
    report("A", A);
    report("B", B);
    return 0;
}
