#include <iostream>
#include <iomanip>
#include <cstdint>
#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"

// ---------- encoders (same style as before) --------------------------------
static inline uint32_t enc_I(uint8_t op, uint8_t rd, uint8_t f3, uint8_t rs1, int32_t imm12){
    uint32_t u=(uint32_t)(imm12 & 0xFFF);
    return (u<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op;
}
static inline uint32_t enc_R(uint8_t f7,uint8_t rs2,uint8_t rs1,uint8_t f3,uint8_t rd,uint8_t op=0x33){
    return (f7<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op;
}
static inline uint32_t enc_S(uint8_t op, uint8_t f3, uint8_t rs1, uint8_t rs2, int32_t imm12){
    uint32_t u=(uint32_t)(imm12 & 0xFFF);
    uint32_t i11_5=(u>>5)&0x7F, i4_0=u&0x1F;
    return (i11_5<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(i4_0<<7)|op;
}
static inline uint32_t enc_B(uint8_t f3,uint8_t rs1,uint8_t rs2,int32_t off){
    uint32_t u=(uint32_t)off; uint32_t i12=(u>>12)&1, i10_5=(u>>5)&0x3F, i4_1=(u>>1)&0xF, i11=(u>>11)&1;
    return (i12<<31)|(i10_5<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(i4_1<<8)|(i11<<7)|0x63;
}
static inline uint32_t enc_U(uint8_t op,uint8_t rd,uint32_t imm20){ return (imm20<<12)|(rd<<7)|op; }
static inline uint32_t enc_J(uint8_t op,uint8_t rd,int32_t off){
    uint32_t u=(uint32_t)off;
    uint32_t i20=(u>>20)&1, i10_1=(u>>1)&0x3FF, i11=(u>>11)&1, i19_12=(u>>12)&0xFF;
    return (i20<<31)|(i19_12<<12)|(i11<<20)|(i10_1<<21)|(rd<<7)|op;
}
// helpers to avoid PC-relative mistakes
static inline uint32_t enc_J_to(uint8_t rd, uint32_t pc, uint32_t target){ int32_t off=(int32_t)target-(int32_t)pc; return enc_J(0x6F, rd, off); }

static inline void put(Memory& m, uint32_t a, uint32_t w){ m.store32(a, w); }
static inline void show(const CPU& c, const Memory& ram){
    uint32_t inst=ram.load32(c.pc);
    std::cout<<"PC=0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<c.pc<<"  "<<disasm(inst)<<std::dec<<"\n";
}

int main(){
    Memory ram(64*1024);
    CPU cpu; cpu.pc=0;

    // ===== Caller (like main) =====
    // 1) Set up stack pointer to top of RAM (0x10000) so we can make frames
    put(ram, 0x0000, enc_U(0x37, /*rd=*/2, /*imm20=*/0x10));      // lui  x2(sp),0x10  -> sp=0x10000

    // 2) Arguments: a0=7, a1=4
    put(ram, 0x0004, enc_I(0x13, /*rd=*/10, 0b000, /*rs1=*/0, 7)); // addi x10(a0),x0,7
    put(ram, 0x0008, enc_I(0x13, /*rd=*/11, 0b000, /*rs1=*/0, 4)); // addi x11(a1),x0,4

    // 3) Call func at 0x0040 (returns in a2=x12)
    put(ram, 0x000C, enc_J_to(/*rd=*/1, /*pc=*/0x000C, /*target=*/0x0040)); // jal x1(ra), func

    // 4) After return, copy result to x7 for visibility; then halt
    put(ram, 0x0010, enc_I(0x13, /*rd=*/7, 0b000, /*rs1=*/12, 0)); // addi x7,x12,0
    put(ram, 0x0014, enc_J_to(/*rd=*/0, /*pc=*/0x0014, /*target=*/0x0014)); // jal x0,0 (halt)

    // ===== Callee at 0x0040 =====
    // Prologue: make frame, save ra and s0, set s0=sp
    put(ram, 0x0040, enc_I(0x13, /*rd=*/2, 0b000, /*rs1=*/2, -16)); // addi sp,sp,-16
    put(ram, 0x0044, enc_S(0x23, 0b010, /*rs1=*/2, /*rs2=*/1, 12)); // sw   ra,12(sp)
    put(ram, 0x0048, enc_S(0x23, 0b010, /*rs1=*/2, /*rs2=*/8, 8));  // sw   s0,8(sp)
    put(ram, 0x004C, enc_I(0x13, /*rd=*/8, 0b000, /*rs1=*/2, 0));   // addi s0,sp,0   (establish fp)

    // Body:
    // a2 = a0 + a1; store to local tmp at 4(sp), load it back; addi +3
    put(ram, 0x0050, enc_R(0x00, /*rs2=*/11, /*rs1=*/10, 0b000, /*rd=*/12));   // add  a2,x10(a0),x11(a1)
    put(ram, 0x0054, enc_S(0x23, 0b010, /*rs1=*/2, /*rs2=*/12, 4));            // sw   a2,4(sp)   (tmp= a0+a1)
    put(ram, 0x0058, enc_I(0x03, /*rd=*/12, 0b010, /*rs1=*/2, 4));             // lw   a2,4(sp)   (reload tmp)
    put(ram, 0x005C, enc_I(0x13, /*rd=*/12, 0b000, /*rs1=*/12, 3));            // addi a2,a2,3    (result = tmp + 3)

    // Epilogue: restore s0, ra; pop; return
    put(ram, 0x0060, enc_I(0x03, /*rd=*/8,  0b010, /*rs1=*/2, 8));  // lw s0,8(sp)
    put(ram, 0x0064, enc_I(0x03, /*rd=*/1,  0b010, /*rs1=*/2, 12)); // lw ra,12(sp)
    put(ram, 0x0068, enc_I(0x13, /*rd=*/2,  0b000, /*rs1=*/2, 16)); // addi sp,sp,16
    put(ram, 0x006C, enc_I(0x67, /*rd=*/0,  0b000, /*rs1=*/1, 0));  // jalr x0,ra,0

    // Run & trace
    for(int i=0;i<30;++i){
        show(cpu, ram);
        bool ok = cpu.step(ram);
        if(!ok){ std::cerr<<"Unsupported at PC=0x"<<std::hex<<cpu.pc<<std::dec<<"\n"; break; }
        std::cout<<" -> sp=0x"<<std::hex<<cpu.x[2]<<std::dec
                 <<"  ra=0x"<<std::hex<<cpu.x[1]<<std::dec
                 <<"  fp(s0)="<<cpu.x[8]
                 <<"  a0="<<cpu.x[10]<<" a1="<<cpu.x[11]<<" a2="<<cpu.x[12]
                 <<"  x7="<<cpu.x[7]
                 <<"  pc=0x"<<std::hex<<cpu.pc<<std::dec<<"\n";
    }
    return 0;
}
