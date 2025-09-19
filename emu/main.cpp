#include <iostream>
#include <iomanip>
#include <cstdint>
#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"

// Minimal encoders
static inline uint32_t enc_I(uint8_t op, uint8_t rd, uint8_t f3, uint8_t rs1, int32_t imm12){
    uint32_t u=(uint32_t)(imm12 & 0xFFF);
    return (u<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op;
}
static inline uint32_t enc_R(uint8_t f7,uint8_t rs2,uint8_t rs1,uint8_t f3,uint8_t rd,uint8_t op=0x33){
    return (f7<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op;
}
static inline uint32_t enc_B(uint8_t f3,uint8_t rs1,uint8_t rs2,int32_t off){
    uint32_t u=(uint32_t)off; // byte offset (even)
    uint32_t i12=(u>>12)&1, i10_5=(u>>5)&0x3F, i4_1=(u>>1)&0xF, i11=(u>>11)&1;
    return (i12<<31)|(i10_5<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(i4_1<<8)|(i11<<7)|0x63;
}
static inline uint32_t enc_U(uint8_t op,uint8_t rd,uint32_t imm20){ return (imm20<<12)|(rd<<7)|op; }
static inline uint32_t enc_J(uint8_t op,uint8_t rd,int32_t off){
    uint32_t u=(uint32_t)off; uint32_t i20=(u>>20)&1,i10_1=(u>>1)&0x3FF,i11=(u>>11)&1,i19_12=(u>>12)&0xFF;
    return (i20<<31)|(i19_12<<12)|(i11<<20)|(i10_1<<21)|(rd<<7)|op;
}
static inline uint32_t enc_J_to(uint8_t rd, uint32_t pc, uint32_t target){ int32_t off=(int32_t)target-(int32_t)pc; return enc_J(0x6F,rd,off); }

static inline void put(Memory& m, uint32_t a, uint32_t w){ m.store32(a,w); }
static inline void show(const CPU& c, const Memory& ram){
    uint32_t inst=ram.load32(c.pc);
    std::cout<<"PC=0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<c.pc<<"  "<<disasm(inst)<<std::dec<<"\n";
}

int main(){
    Memory ram(64*1024);
    CPU cpu; cpu.pc=0;

    // Setup: x5 = -3, x4 = 2  (test signed compares)
    put(ram, 0x0000, enc_I(0x13, 5, 0b000, 0, -3));  // addi x5,x0,-3
    put(ram, 0x0004, enc_I(0x13, 4, 0b000, 0,  2));  // addi x4,x0,2

    // If (x5 < x4) then x7=111, else x7=999 using BLT/BGE
    // blt x5,x4, +8  -> skip over the 'else' assignment
    // OLD (wrong): jumps to 0x0010, then immediately skips the THEN block
    // put(ram, 0x0008, enc_B(0b100, /*rs1=*/5, /*rs2=*/4, /*off=*/8));

    // NEW (correct): jump straight to THEN at 0x0014 when blt is true
    put(ram, 0x0008, enc_B(0b100, /*rs1=*/5, /*rs2=*/4, /*off=*/(int32_t)0x0014 - (int32_t)0x0008));

    put(ram, 0x000C, enc_I(0x13, 7, 0b000, 0, 999 & 0xFFF)); // will be sign-truncated; just for visibility
    // jump over then-part
    put(ram, 0x0010, enc_J_to(/*rd=*/0, /*pc=*/0x0010, /*target=*/0x0018));
    // then: x7 = 111
    put(ram, 0x0014, enc_I(0x13, 7, 0b000, 0, 111));
    // fallthrough:
    // Now check BGE: if (x5 >= x4) set x6=1 else x6=0
    // bge x5,x4, +8  -> take if true to set x6=1
    put(ram, 0x0018, enc_B(0b101, /*rs1=*/5, /*rs2=*/4, /*off=*/8));
    // false path: x6=0
    put(ram, 0x001C, enc_I(0x13, 6, 0b000, 0, 0));
    // jump over true path
    put(ram, 0x0020, enc_J_to(/*rd=*/0, /*pc=*/0x0020, /*target=*/0x0028));
    // true path: x6=1
    put(ram, 0x0024, enc_I(0x13, 6, 0b000, 0, 1));

    // Halt: jal x0, 0
    put(ram, 0x0028, enc_J_to(/*rd=*/0, /*pc=*/0x0028, /*target=*/0x0028));

    for(int i=0;i<20;++i){
        show(cpu,ram);
        bool ok=cpu.step(ram);
        if(!ok){ std::cerr<<"Unsupported at PC=0x"<<std::hex<<cpu.pc<<std::dec<<"\n"; break; }
        std::cout<<" -> x4="<<cpu.x[4]<<" x5="<<(int32_t)cpu.x[5]
                 <<"  x6="<<cpu.x[6]<<" x7="<<cpu.x[7]
                 <<"  pc=0x"<<std::hex<<cpu.pc<<std::dec<<"\n";
    }
    return 0;
}
