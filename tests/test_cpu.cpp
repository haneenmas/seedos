// tests/test_cpu.cpp
#include "test_util.hpp"
#include "emu/cpu.hpp"
#include "emu/mem.hpp"
#include "emu/disasm.hpp"

// helper: write a 32-bit word to memory at addr
static inline void put32(Memory& m, uint32_t addr, uint32_t w){ m.store32(addr, w); }

// encoders (same style as in your main)
static inline uint32_t enc_I(uint8_t op,uint8_t rd,uint8_t rs1,int32_t imm12){
    uint32_t u=(uint32_t)imm12 & 0xFFF;
    return (u<<20)|(rs1<<15)|(0b000<<12)|(rd<<7)|op;
}
static inline uint32_t enc_R(uint8_t op,uint8_t rd,uint8_t rs1,uint8_t rs2,uint8_t funct3,uint8_t funct7){
    return (funct7<<25)|(rs2<<20)|(rs1<<15)|(funct3<<12)|(rd<<7)|op;
}
static inline uint32_t enc_B(uint8_t op,uint8_t rs1,uint8_t rs2,uint8_t funct3,int imm){
    // imm is byte offset; encode to B-form
    uint32_t u=(uint32_t)imm;
    uint32_t b12=(u>>12)&1, b11=(u>>11)&1, b10_5=(u>>5)&0x3F, b4_1=(u>>1)&0xF;
    return (b12<<31)|(b10_5<<25)|(rs2<<20)|(rs1<<15)|(funct3<<12)|(b4_1<<8)|(b11<<7)|op;
}

int main(){
    TestState T;

    // ---------- test 1: ADDI ----------
    {
        Memory ram(64*1024); CPU cpu; cpu.pc=0;
        // addi x1, x0, 5
        put32(ram,0x0, enc_I(0x13, 1, 0, 5));
        cpu.step(ram);
        EXPECT_EQ(T, cpu.x[1], (uint32_t)5);
    }

    // ---------- test 2: SUB (R-type) ----------
    {
        Memory ram(64*1024); CPU cpu; cpu.pc=0;
        // addi x4,x0,20 ; addi x5,x0,5 ; sub x6,x4,x5
        put32(ram,0x00, enc_I(0x13, 4, 0, 20));
        put32(ram,0x04, enc_I(0x13, 5, 0, 5));
        put32(ram,0x08, enc_R(0x33, 6, 4, 5, 0b000, 0b0100000)); // SUB
        cpu.step(ram); cpu.step(ram); cpu.step(ram);
        EXPECT_EQ(T, cpu.x[6], (uint32_t)15);
    }

    // ---------- test 3: BEQ taken & not taken ----------
    {
        Memory ram(64*1024); CPU cpu; cpu.pc=0;
        // addi x10,x0,1 ; addi x11,x0,1 ; beq x10,x11,+8 ; addi x7,x0,99 ; addi x7,x0,42
        put32(ram,0x00, enc_I(0x13,10,0,1));
        put32(ram,0x04, enc_I(0x13,11,0,1));
        put32(ram,0x08, enc_B(0x63,10,11,0b000, +8));
        put32(ram,0x0C, enc_I(0x13, 7,0,99));   // skipped
        put32(ram,0x10, enc_I(0x13, 7,0,42));   // executed
        for(int i=0;i<5;i++) cpu.step(ram);
        EXPECT_EQ(T, cpu.x[7], (uint32_t)42);
    }

    // ---------- test 4: Memory sbrk grows & returns old brk ----------
    {
        Memory ram(64*1024);
        uint32_t old = ram.sbrk(0);
        uint32_t old2= ram.sbrk(64);
        EXPECT_EQ(T, old,  old2);         // sbrk(0) equals previous break
        EXPECT_EQ(T, ram.sbrk(0), old+64);
    }

    return T.summary();
}
