#include <iostream>
#include <iomanip>
#include <cstdint>
#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"

// --- tiny encoders so we don't hand-type hex -------------------------------
static inline uint32_t enc_I(uint8_t op, uint8_t rd, uint8_t f3, uint8_t rs1, int32_t imm12){
    uint32_t u = (uint32_t)(imm12 & 0xFFF);
    return (u<<20) | (rs1<<15) | (f3<<12) | (rd<<7) | op;
}
static inline uint32_t enc_R(uint8_t f7, uint8_t rs2, uint8_t rs1, uint8_t f3, uint8_t rd, uint8_t op=0x33){
    return (f7<<25) | (rs2<<20) | (rs1<<15) | (f3<<12) | (rd<<7) | op;
}
static inline uint32_t enc_S(uint8_t op, uint8_t f3, uint8_t rs1, uint8_t rs2, int32_t imm12){
    uint32_t u = (uint32_t)(imm12 & 0xFFF);
    uint32_t i11_5 = (u>>5)&0x7F, i4_0 = u & 0x1F;
    return (i11_5<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(i4_0<<7)|op;
}
static inline uint32_t enc_B(uint8_t f3, uint8_t rs1, uint8_t rs2, int32_t off){
    uint32_t u=(uint32_t)off; // byte offset (even)
    uint32_t i12=(u>>12)&1, i10_5=(u>>5)&0x3F, i4_1=(u>>1)&0xF, i11=(u>>11)&1;
    return (i12<<31)|(i10_5<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(i4_1<<8)|(i11<<7)|0x63;
}
static inline uint32_t enc_U(uint8_t op, uint8_t rd, uint32_t imm20){ return (imm20<<12)|(rd<<7)|op; }
static inline uint32_t enc_J(uint8_t op, uint8_t rd, int32_t off){
    uint32_t u=(uint32_t)off; // byte offset, LSB=0
    uint32_t i20=(u>>20)&1, i10_1=(u>>1)&0x3FF, i11=(u>>11)&1, i19_12=(u>>12)&0xFF;
    return (i20<<31)|(i19_12<<12)|(i11<<20)|(i10_1<<21)|(rd<<7)|op;
}

static inline void put(Memory& m, uint32_t a, uint32_t w){ m.store32(a, w); }
static inline void show(const CPU& cpu, const Memory& ram){
    uint32_t inst = ram.load32(cpu.pc);
    std::cout << "PC=0x" << std::hex << std::setw(8) << std::setfill('0') << cpu.pc
              << "  " << disasm(inst) << std::dec << "\n";
}

int main(){
    Memory ram(64*1024);
    CPU cpu; cpu.pc = 0;

    // --- Block A: arithmetic & LUI/ADDI/ADD/SUB ----------------------------
    put(ram, 0x0000, enc_I(0x13, 1, 0b000, 0, 5));          // addi x1,x0,5
    put(ram, 0x0004, enc_I(0x13, 2, 0b000, 1, 10));         // addi x2,x1,10  -> x2=15
    put(ram, 0x0008, enc_U(0x37, 3, 0x12));                 // lui  x3,0x12   -> x3=0x12000
    put(ram, 0x000C, enc_I(0x13, 3, 0b000, 3, 34));         // addi x3,x3,34  -> x3=0x12022
    put(ram, 0x0010, enc_R(0x00, 2, 1, 0b000, 4));          // add  x4,x1,x2  -> x4=20
    put(ram, 0x0014, enc_R(0x20, 1, 4, 0b000, 5));          // sub  x5,x4,x1  -> x5=15

    // --- Block B: memory LW/SW ---------------------------------------------
    put(ram, 0x0018, enc_U(0x37, 10, 0x1));                 // lui  x10,0x1   -> x10=0x1000
    put(ram, 0x001C, enc_S(0x23, 0b010, 10, 4, 0));         // sw   x4,0(x10) -> [0x1000]=20
    put(ram, 0x0020, enc_I(0x03, 7, 0b010, 10, 0));         // lw   x7,0(x10) -> x7=20

    // --- Block C: loop with BNE (sum 3..1) ---------------------------------
    put(ram, 0x0024, enc_I(0x13, 11, 0b000, 0, 0));         // addi x11,x0,0  acc=0
    put(ram, 0x0028, enc_I(0x13, 12, 0b000, 0, 3));         // addi x12,x0,3  i=3
    // loop:
    put(ram, 0x002C, enc_R(0x00, 12, 11, 0b000, 11));       // add  x11,x11,x12
    put(ram, 0x0030, enc_I(0x13, 12, 0b000, 12, -1));       // addi x12,x12,-1
    put(ram, 0x0034, enc_B(0b001, 12, 0, (int32_t)0x002C - (int32_t)0x0034)); // bne x12,x0, loop
    put(ram, 0x0038, enc_I(0x13, 6, 0b000, 11, 0));         // addi x6,x11,0  -> x6=3+2+1=6

    // --- Block D: call/return with JAL/JALR (a0=5, a1=12, returns a2=17) ---
    put(ram, 0x003C, enc_I(0x13, 10, 0b000, 0, 5));         // addi x10,x0,5  a0
    put(ram, 0x0040, enc_I(0x13, 11, 0b000, 0, 12));        // addi x11,x0,12 a1
    put(ram, 0x0044, enc_J(0x6F, 1, (int32_t)0x0080 - (int32_t)0x0048)); // jal  x1, func
    put(ram, 0x0048, enc_I(0x13, 7, 0b000, 12, 0));         // addi x7,x12,0  copy result here

    // func at 0x80: a2 = a0 + a1; return
    put(ram, 0x0080, enc_R(0x00, 11, 10, 0b000, 12));       // add x12,x10,x11
    put(ram, 0x0084, enc_I(0x67, 0, 0b000, 1, 0));          // jalr x0,x1,0

    // --- run & trace --------------------------------------------------------
    for(int i=0;i<40;++i){
        show(cpu, ram);
        bool ok = cpu.step(ram);
        if(!ok){ std::cerr<<"Unsupported at PC=0x"<<std::hex<<cpu.pc<<std::dec<<"\n"; break; }
        std::cout<<" -> x1="<<cpu.x[1]<<" x2="<<cpu.x[2]<<" x3=0x"<<std::hex<<cpu.x[3]<<std::dec
                 <<" x4="<<cpu.x[4]<<" x5="<<cpu.x[5]<<" x6="<<cpu.x[6]<<" x7="<<cpu.x[7]
                 <<"  pc=0x"<<std::hex<<cpu.pc<<std::dec<<"\n";
    }
    return 0;
}
