#include <iostream>
#include <iomanip>
#include <cstdint>
#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"

// --- encoders ---
static inline uint32_t enc_I(uint8_t op,uint8_t rd,uint8_t f3,uint8_t rs1,int32_t imm12){ uint32_t u=(uint32_t)(imm12&0xFFF); return (u<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op; }
static inline uint32_t enc_R(uint8_t f7,uint8_t rs2,uint8_t rs1,uint8_t f3,uint8_t rd,uint8_t op=0x33){ return (f7<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op; }
static inline uint32_t enc_B(uint8_t f3,uint8_t rs1,uint8_t rs2,int32_t off){ uint32_t u=(uint32_t)off; uint32_t i12=(u>>12)&1,i10_5=(u>>5)&0x3F,i4_1=(u>>1)&0xF,i11=(u>>11)&1; return (i12<<31)|(i10_5<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(i4_1<<8)|(i11<<7)|0x63; }
static inline uint32_t enc_J(uint8_t op,uint8_t rd,int32_t off){ uint32_t u=(uint32_t)off; uint32_t i20=(u>>20)&1,i10_1=(u>>1)&0x3FF,i11=(u>>11)&1,i19_12=(u>>12)&0xFF; return (i20<<31)|(i19_12<<12)|(i11<<20)|(i10_1<<21)|(rd<<7)|op; }
static inline uint32_t B_to(uint8_t f3, uint8_t rs1, uint8_t rs2, uint32_t pc, uint32_t target){ int32_t off=(int32_t)target-(int32_t)pc; return enc_B(f3, rs1, rs2, off); }

static inline uint32_t ECALL(){ return 0x00000073; }

static inline void put(Memory& m,uint32_t a,uint32_t w){ m.store32(a,w); }
static inline void show(const CPU& c,const Memory& ram){
    uint32_t inst=ram.load32(c.pc);
    std::cout<<"PC=0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<c.pc<<"  "<<disasm(inst)<<std::dec<<"\n";
}

int main(){
    Memory ram(64*1024);
    CPU cpu; cpu.pc=0;

    // Program: sum 1..100 in x5, then print_u32(sum), exit(0)
    // regs:
    //   x5 = sum
    //   x6 = i
    //   x7 = N_plus_1 (101)
    // layout & labels
    uint32_t L0   = 0x0000; // start
    uint32_t LOOP = 0x0010; // loop body
    uint32_t DONE = 0x0030; // after loop

    // prologue: sum=0; i=1; N_plus_1=101
    put(ram, L0+0x00, enc_I(0x13/*addi*/, 5, 0b000, 0,   0));   // addi x5,x0,0
    put(ram, L0+0x04, enc_I(0x13/*addi*/, 6, 0b000, 0,   1));   // addi x6,x0,1
    put(ram, L0+0x08, enc_I(0x13/*addi*/, 7, 0b000, 0, 101));   // addi x7,x0,101
    // jump into loop
    put(ram, L0+0x0C, enc_J(0x6F/*jal*/, 0, (int32_t)LOOP - (int32_t)(L0+0x0C)));

    // LOOP:
    put(ram, LOOP+0x00, enc_R(0, 6, 5, 0b000, 5));               // add  x5,x5,x6   ; sum += i
    put(ram, LOOP+0x04, enc_I(0x13, 6, 0b000, 6, 1));            // addi x6,x6,1    ; i++
    put(ram, LOOP+0x08, B_to(0b100/*blt*/, 6, 7, LOOP+0x08, LOOP)); // blt  x6,x7, LOOP
    // fallthrough to DONE
    put(ram, LOOP+0x0C, enc_J(0x6F/*jal*/, 0, (int32_t)DONE - (int32_t)(LOOP+0x0C)));

    // DONE: a0=sum ; print_u32(a0); exit(0)
    put(ram, DONE+0x00, enc_I(0x13, 10, 0b000, 5, 0));   // addi x10,x5,0  (move)
    put(ram, DONE+0x04, enc_I(0x13, 17, 0b000, 0, 1));   // addi x17,x0,1  (syscall id=1)
    put(ram, DONE+0x08, ECALL());                        // print_u32(a0)
    put(ram, DONE+0x0C, enc_I(0x13, 10, 0b000, 0, 0));   // a0=0 (exit code)
    put(ram, DONE+0x10, enc_I(0x13, 17, 0b000, 0, 0));   // a7=0 (exit)
    put(ram, DONE+0x14, ECALL());                        // exit(0)

    // run
    for(int i=0;i<2000 && !cpu.halted; ++i){
        // comment out next two lines if you want silent run
        // show(cpu, ram);
        // std::cout<<" -> a0="<<cpu.x[10]<<" pc=0x"<<std::hex<<cpu.pc<<std::dec<<"\n";
        if(!cpu.step(ram)){ std::cerr<<"Unsupported at PC=0x"<<std::hex<<cpu.pc<<std::dec<<"\n"; break; }
    }

    double cpi = (cpu.instret==0) ? 0.0 : (double)cpu.cycles / (double)cpu.instret;
    std::cout << "[halted] exit="<<cpu.exit_code
              << "  instret="<<cpu.instret
              << "  cycles="<<cpu.cycles
              << "  CPI="<<std::fixed<<std::setprecision(2)<<cpi << "\n";
    return (int)cpu.exit_code;
}
