#include "test_util.hpp"
#include <cassert>

int main(){
    Memory ram(64*1024);
    CPU cpu; cpu.pc=0;

    // addi x1,x0,5 ; addi x2,x1,10 ; sub x5,x2,x1 ; ecall
    auto encI=[](uint8_t rd,uint8_t rs1,int imm){ return ((imm&0xFFF)<<20)|(rs1<<15)|(0<<12)|(rd<<7)|0x13; };
    auto encR=[](uint8_t rd,uint8_t rs1,uint8_t rs2,uint8_t f7){ return (f7<<25)|(rs2<<20)|(rs1<<15)|(0<<12)|(rd<<7)|0x33; };
    auto ecall=[](){ return 0x73u; };

    put32(ram,0x00,encI(1,0,5));
    put32(ram,0x04,encI(2,1,10));
    put32(ram,0x08,encR(5,2,1,0x20));
    put32(ram,0x0C,ecall());

    for(int i=0;i<20 && !cpu.halted;i++) cpu.step(ram);

    assert(cpu.x[1]==5);
    assert(cpu.x[2]==15);
    assert(cpu.x[5]==10);
    return 0;
}
