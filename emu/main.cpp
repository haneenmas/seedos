#include <iostream>
#include <iomanip>
#include <cstdint>
#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"

// --- encoders ---
static inline uint32_t enc_I(uint8_t op,uint8_t rd,uint8_t f3,uint8_t rs1,int32_t imm12){
    uint32_t u=(uint32_t)(imm12 & 0xFFF); return (u<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op;
}
static inline uint32_t enc_LUI(uint8_t rd, uint32_t imm20){ return (imm20<<12)|(rd<<7)|0x37; }
static inline uint32_t enc_B(uint8_t f3,uint8_t rs1,uint8_t rs2,int32_t off){
    uint32_t u=(uint32_t)off;
    uint32_t i12=(u>>12)&1,i10_5=(u>>5)&0x3F,i4_1=(u>>1)&0xF,i11=(u>>11)&1;
    return (i12<<31)|(i10_5<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(i4_1<<8)|(i11<<7)|0x63;
}
static inline uint32_t ECALL(){ return 0x00000073; }
static inline void put32(Memory& m,uint32_t a,uint32_t w){ m.store32(a,w); }
static inline void put8 (Memory& m,uint32_t a,uint8_t  b){ m.store8(a,b); }

static inline void emit_li32(Memory& m, uint32_t at, uint8_t rd, uint32_t imm){
    uint32_t upper = (imm + 0x800) >> 12;
    int32_t  lower = (int32_t)imm - (int32_t)(upper<<12);
    put32(m, at+0, enc_LUI(rd, upper));
    put32(m, at+4, enc_I(0x13, rd, 0b000, rd, lower));
}

// ---- Part A: syscalls write_str + sbrk demo ----
static void emit_syscall_demo(Memory& ram, uint32_t PC){
    const char* msg = "Hello, heap & timer!\\n";
    uint32_t MSG = 0x0800;
    for(uint32_t i=0; msg[i]; ++i) put8(ram, MSG+i, (uint8_t)msg[i]);

    // a0=MSG (via li), a1=len(20), a7=4 -> write_str
    emit_li32(ram, PC+0x00, 10, MSG);
    put32(ram, PC+0x08, enc_I(0x13,11,0b000,0,20));
    put32(ram, PC+0x0C, enc_I(0x13,17,0b000,0,4));
    put32(ram, PC+0x10, ECALL());

    // a0=+32; a7=3 -> sbrk ; print a0 (old break) with a7=1
    put32(ram, PC+0x14, enc_I(0x13,10,0b000,0,32));
    put32(ram, PC+0x18, enc_I(0x13,17,0b000,0,3)); put32(ram, PC+0x1C, ECALL());
    put32(ram, PC+0x20, enc_I(0x13,17,0b000,0,1)); put32(ram, PC+0x24, ECALL());

    // timer: read time via a7=8; print_u32
    put32(ram, PC+0x28, enc_I(0x13,17,0b000,0,8)); put32(ram, PC+0x2C, ECALL());
    put32(ram, PC+0x30, enc_I(0x13,17,0b000,0,1)); put32(ram, PC+0x34, ECALL());

    // exit(0) just for the demo block (we won’t run to here in combined test)
    put32(ram, PC+0x38, enc_I(0x13,10,0b000,0,0));
    put32(ram, PC+0x3C, enc_I(0x13,17,0b000,0,0));
    put32(ram, PC+0x40, ECALL());
}

// ---- Part B: allocator reuse proof (p1'=malloc16; free; malloc24 -> same ptr) ----
static void emit_alloc_demo(Memory& m, uint32_t PC){
    // a0=16; a7=5 -> malloc ; save to s1 (x9); free; malloc(24); print ptr
    put32(m, PC+0x00, enc_I(0x13,10,0b000,0,16)); // a0=16
    put32(m, PC+0x04, enc_I(0x13,17,0b000,0,5)); put32(m, PC+0x08, ECALL()); // a0=p1
    put32(m, PC+0x0C, enc_I(0x13,9,0b000,10,0));  // x9 = a0
    put32(m, PC+0x10, enc_I(0x13,10,0b000,9,0));  // a0 = x9
    put32(m, PC+0x14, enc_I(0x13,17,0b000,0,6)); put32(m, PC+0x18, ECALL()); // free
    put32(m, PC+0x1C, enc_I(0x13,10,0b000,0,24)); // a0=24
    put32(m, PC+0x20, enc_I(0x13,17,0b000,0,5)); put32(m, PC+0x24, ECALL()); // a0=p3
    // print p3
    put32(m, PC+0x28, enc_I(0x13,17,0b000,0,1)); put32(m, PC+0x2C, ECALL());
    // (exit not used here)
}

// ---- Part C: two threaded print using scheduler + timer reads ----
static inline uint32_t enc_Rtype_ADD(uint8_t rd,uint8_t rs1,uint8_t rs2){ return (0<<25)|(rs2<<20)|(rs1<<15)|(0<<12)|(rd<<7)|0x33; }

static void emit_thread_A(Memory& m, uint32_t base){
    // prints 'A'..'J' with timestamp before each char
    emit_li32(m, base+0x00, 7, 8);              // a7=8 get_time
    uint32_t L = base+0x08;
    put32(m, L+0x00, ECALL());                   // a0=time()
    put32(m, L+0x04, enc_I(0x13,17,0b000,0,1));  // print_u32(time)
    put32(m, L+0x08, ECALL());
    put32(m, L+0x0C, enc_I(0x13,10,0b000,0,'A')); // a0='A'
    put32(m, L+0x10, enc_I(0x13,17,0b000,0,2));  // putchar
    put32(m, L+0x14, ECALL());
    put32(m, L+0x18, enc_I(0x13,10,0b000,10,1)); // a0++ (next char)
    // if a0<='J' loop
    emit_li32(m, base+0x20, 8, 'J'+1);
    put32(m, L+0x1C, enc_B(0b100,10,8,(int32_t)L - (int32_t)(L+0x1C))); // blt a0,x8,L
    put32(m, base+0x24, enc_I(0x13,10,0b000,0,0)); // exit(0)
    put32(m, base+0x28, enc_I(0x13,17,0b000,0,0));
    put32(m, base+0x2C, ECALL());
}

static void emit_thread_B(Memory& m, uint32_t base){
    // prints 'a'..'j', yields each iteration, also prints time occasionally
    emit_li32(m, base+0x00, 10, 'a');           // a0='a'
    emit_li32(m, base+0x08, 7, 8);              // a7=8 get_time
    uint32_t L = base+0x10;
    put32(m, L+0x00, ECALL());                  // a0=time()
    put32(m, L+0x04, enc_I(0x13,17,0b000,0,1)); // print_u32
    put32(m, L+0x08, ECALL());
    put32(m, L+0x0C, enc_I(0x13,17,0b000,0,2)); // putchar current a0
    put32(m, L+0x10, ECALL());
    put32(m, L+0x14, enc_I(0x13,10,0b000,10,1)); // a0++
    // yield()
    put32(m, L+0x18, enc_I(0x13,17,0b000,0,7)); put32(m, L+0x1C, ECALL());
    // if a0<='j' loop
    emit_li32(m, base+0x24, 8, 'j'+1);
    put32(m, L+0x20, enc_B(0b100,10,8,(int32_t)L - (int32_t)(L+0x20)));
    // exit
    put32(m, base+0x28, enc_I(0x13,10,0b000,0,0));
    put32(m, base+0x2C, enc_I(0x13,17,0b000,0,0));
    put32(m, base+0x30, ECALL());
}

int main(){
    Memory ram(64*1024);

    // Part A: write_str + sbrk + timer value
    emit_syscall_demo(ram, 0x0000);

    // Part B: allocator reuse (prints p3)
    emit_alloc_demo(ram, 0x0040);

    // Part C: two threads at separate PCs
    uint32_t A_START = 0x0200;
    uint32_t B_START = 0x0400;
    emit_thread_A(ram, A_START);
    emit_thread_B(ram, B_START);

    // Run Part A & B quickly by stepping a small loop; then switch to scheduler
    {
        CPU tmp; tmp.pc = 0x0000; tmp.quantum = 0;
        for(int i=0;i<200 && !tmp.halted; ++i){
            if(!tmp.step(ram)) { std::cerr<<"Unsupported at PC=0x"<<std::hex<<tmp.pc<<std::dec<<"\n"; break; }
        }
    }
    {
        CPU tmp; tmp.pc = 0x0040; tmp.quantum = 0;
        for(int i=0;i<200 && !tmp.halted; ++i){
            if(!tmp.step(ram)) { std::cerr<<"Unsupported at PC=0x"<<std::hex<<tmp.pc<<std::dec<<"\n"; break; }
        }
    }

    // Scheduler: run A and B with time quanta, proving preemption & timer ticks
    CPU A; A.pc = A_START; A.quantum = 20;
    CPU B; B.pc = B_START; B.quantum = 20;
    CPU* cur = &A; CPU* nxt = &B;

    while(!(A.halted && B.halted)){
        for(int k=0;k<500 && !cur->halted && !cur->yielded; ++k){
            if(!cur->step(ram)){ std::cerr<<"Unsupported at PC=0x"<<std::hex<<cur->pc<<std::dec<<"\n"; cur->halted=true; break; }
        }
        if (cur->yielded || cur->halted) std::swap(cur, nxt);
    }

    auto rpt=[&](const char* name,const CPU& c){
        double cpi=(c.instret==0)?0.0:(double)c.cycles/(double)c.instret;
        std::cout<<"\n["<<name<<"] exit="<<c.exit_code<<" instret="<<c.instret<<" cycles="<<c.cycles<<" CPI="<<std::fixed<<std::setprecision(2)<<cpi<<"\n";
    };
    rpt("A",A); rpt("B",B);
    std::cout << "[timer] final ticks=" << ram.time() << "\n";
    return 0;
}
