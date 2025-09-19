#include <iostream>
#include <iomanip>
#include <cstdint>
#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"

// ---------- Tiny encoders so we don't hand-write hex -----------------------
static inline uint32_t enc_I(uint8_t opcode, uint8_t rd, uint8_t funct3,
                             uint8_t rs1, int32_t imm12) {
    uint32_t imm = (uint32_t)(imm12 & 0xFFF);
    return (imm << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}
static inline uint32_t enc_S(uint8_t opcode, uint8_t funct3,
                             uint8_t rs1, uint8_t rs2, int32_t imm12) {
    uint32_t imm = (uint32_t)(imm12 & 0xFFF);
    uint32_t imm11_5 = (imm >> 5) & 0x7F;
    uint32_t imm4_0  = imm & 0x1F;
    return (imm11_5 << 25) | (rs2 << 20) | (rs1 << 15) |
           (funct3 << 12) | (imm4_0 << 7) | opcode;
}
static inline uint32_t enc_U(uint8_t opcode, uint8_t rd, uint32_t imm20) {
    // imm20 goes to bits 31..12
    return (imm20 << 12) | (rd << 7) | opcode;
}
static inline uint32_t enc_J(uint8_t opcode, uint8_t rd, int32_t off) {
    // off is signed byte offset; J encodes bits [20|10:1|11|19:12], LSB must be 0
    uint32_t u = (uint32_t)off;
    uint32_t i20    = (u >> 20) & 0x1;
    uint32_t i10_1  = (u >> 1)  & 0x3FF;
    uint32_t i11    = (u >> 11) & 0x1;
    uint32_t i19_12 = (u >> 12) & 0xFF;
    return (i20 << 31) | (i19_12 << 12) | (i11 << 20) |
           (i10_1 << 21) | (rd << 7) | opcode;
}

static inline void put(Memory& m, uint32_t addr, uint32_t word){ m.store32(addr, word); }
static inline void show_next(const CPU& cpu, const Memory& ram){
    uint32_t inst = ram.load32(cpu.pc);
    std::cout << "PC=0x" << std::hex << std::setw(8) << std::setfill('0') << cpu.pc
              << "  " << disasm(inst) << std::dec << "\n";
}

int main(){
    Memory ram(64*1024);
    CPU cpu; cpu.pc = 0;

    // === Caller (at 0x0000) ================================================
    // sp = 0x10000 (top of 64KB)
    put(ram, 0x0000, enc_U(0x37, /*rd=*/2, /*imm20=*/0x10));        // lui  x2,0x10  -> sp=0x10000

    // a0 = 5; a1 = 12
    put(ram, 0x0004, enc_I(0x13, /*rd=*/10, 0b000, /*rs1=*/0, 5));   // addi x10,x0,5
    put(ram, 0x0008, enc_I(0x13, /*rd=*/11, 0b000, /*rs1=*/0, 12));  // addi x11,x0,12

    // call func at 0x0040: jal ra, (0x40 - 0x000C)
    put(ram, 0x000C, enc_J(0x6F, /*rd=*/1, /*off=*/(int32_t)0x0040 - (int32_t)0x000C));

    // after return: copy a2 (x12) → x7 so we can see the result in caller
    put(ram, 0x0010, enc_I(0x13, /*rd=*/7, 0b000, /*rs1=*/12, 0));   // addi x7,x12,0

    // === Callee (at 0x0040) ================================================
    // Prologue: make 16-byte frame; save ra at 12(sp)
    put(ram, 0x0040, enc_I(0x13, /*rd=*/2, 0b000, /*rs1=*/2, -16));  // addi sp,sp,-16
    put(ram, 0x0044, enc_S(0x23, 0b010, /*rs1=*/2, /*rs2=*/1, 12));  // sw   x1,12(sp)

    // Body: a2 = a0 + a1
    put(ram, 0x0048, 0x00B50633u);                                   // add  x12,x10,x11  (R-type)

    // Epilogue: restore ra; pop frame; return
    put(ram, 0x004C, enc_I(0x03, /*rd=*/1, 0b010, /*rs1=*/2, 12));   // lw   x1,12(sp)
    put(ram, 0x0050, enc_I(0x13, /*rd=*/2, 0b000, /*rs1=*/2, 16));   // addi sp,sp,16
    put(ram, 0x0054, enc_I(0x67, /*rd=*/0, 0b000, /*rs1=*/1, 0));    // jalr x0,x1,0

    // Run a few steps and print state
    for (int i = 0; i < 12; ++i) {
        show_next(cpu, ram);
        bool ok = cpu.step(ram);
        if (!ok) { std::cerr<<"Unsupported at PC=0x"<<std::hex<<cpu.pc<<std::dec<<"\n"; break; }
        std::cout<<" -> sp(x2)=0x"<<std::hex<<cpu.x[2]<<std::dec
                 <<" a0(x10)="<<cpu.x[10]<<" a1(x11)="<<cpu.x[11]
                 <<" a2(x12)="<<cpu.x[12]<<" x7="<<cpu.x[7]
                 <<"  pc=0x"<<std::hex<<cpu.pc<<std::dec<<"\n";
    }
    return 0;
}
