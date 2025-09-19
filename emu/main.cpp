#include <iostream>
#include <iomanip>
#include <cstdint>
#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"

// encoders (minimal)
static inline uint32_t enc_I(uint8_t op,uint8_t rd,uint8_t f3,uint8_t rs1,int32_t imm12){ uint32_t u=(uint32_t)(imm12&0xFFF); return (u<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op; }
static inline uint32_t enc_R(uint8_t f7,uint8_t rs2,uint8_t rs1,uint8_t f3,uint8_t rd,uint8_t op=0x33){ return (f7<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op; }
static inline uint32_t enc_B(uint8_t f3,uint8_t rs1,uint8_t rs2,int32_t off){ uint32_t u=(uint32_t)off; uint32_t i12=(u>>12)&1,i10_5=(u>>5)&0x3F,i4_1=(u>>1)&0xF,i11=(u>>11)&1; return (i12<<31)|(i10_5<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(i4_1<<8)|(i11<<7)|0x63; }
static inline uint32_t enc_J(uint8_t op,uint8_t rd,int32_t off){ uint32_t u=(uint32_t)off; uint32_t i20=(u>>20)&1,i10_1=(u>>1)&0x3FF,i11=(u>>11)&1,i19_12=(u>>12)&0xFF; return (i20<<31)|(i19_12<<12)|(i11<<20)|(i10_1<<21)|(rd<<7)|op; }
static inline uint32_t enc_J_to(uint8_t rd,uint32_t pc,uint32_t target){ int32_t off=(int32_t)target-(int32_t)pc; return enc_J(0x6F,rd,off); }

// fixed encodings
static inline uint32_t ECALL()  { return 0x00000073; }
static inline uint32_t EBREAK() { return 0x00100073; }

static inline void put(Memory& m,uint32_t a,uint32_t w){ m.store32(a,w); }
static inline void show(const CPU& c,const Memory& ram){
    uint32_t inst=ram.load32(c.pc);
    std::cout<<"PC=0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<c.pc<<"  "<<disasm(inst)<<std::dec<<"\n";
}

int main(){
    Memory ram(64*1024);
    CPU cpu; cpu.pc=0;

    // --- Small program: print 42, print 'H', then exit(0) ---
    // a0=42 ; a7=1 (print_u32) ; ecall
    put(ram, 0x0000, enc_I(0x13, 10, 0b000, 0, 42));   // addi x10,x0,42
    put(ram, 0x0004, enc_I(0x13, 17, 0b000, 0, 1));    // addi x17,x0,1
    put(ram, 0x0008, ECALL());

    // a0='H' ; a7=2 (putchar) ; ecall
    put(ram, 0x000C, enc_I(0x13, 10, 0b000, 0, 'H'));
    put(ram, 0x0010, enc_I(0x13, 17, 0b000, 0, 2));
    put(ram, 0x0014, ECALL());

    // exit(0)
    put(ram, 0x0018, enc_I(0x13, 10, 0b000, 0, 0));    // a0=0
    put(ram, 0x001C, enc_I(0x13, 17, 0b000, 0, 0));    // a7=0
    put(ram, 0x0020, ECALL());

    // self-loop (should not reach)
    put(ram, 0x0024, enc_J_to(0, 0x0024, 0x0024));

    for(int i=0;i<20 && !cpu.halted; ++i){
        show(cpu, ram);
        if(!cpu.step(ram)){ std::cerr<<"Unsupported at PC=0x"<<std::hex<<cpu.pc<<std::dec<<"\n"; break; }
        std::cout<<" -> a0(x10)="<<cpu.x[10]<<" a7(x17)="<<cpu.x[17]<<"  pc=0x"<<std::hex<<cpu.pc<<std::dec<<"\n";
    }
    std::cout<<"[halted] exit_code="<<cpu.exit_code<<"\n";
    return (int)cpu.exit_code;
}
