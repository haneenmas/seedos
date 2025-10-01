#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cstdint>
#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"
#include "trace.hpp"
#include "elf.hpp"
#include <sys/stat.h>

static bool file_exists(const char* p){ struct stat st; return ::stat(p,&st)==0; }



// ---------- Encoders ----------
static inline uint32_t enc_I(uint8_t op,uint8_t rd,uint8_t f3,uint8_t rs1,int32_t imm12){
    uint32_t u=(uint32_t)(imm12 & 0xFFF); return (u<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op;
}
static inline uint32_t enc_LUI(uint8_t rd, uint32_t imm20){ return (imm20<<12)|(rd<<7)|0x37; }
static inline uint32_t enc_R(uint8_t f7,uint8_t rs2,uint8_t rs1,uint8_t f3,uint8_t rd,uint8_t op=0x33){
    return (f7<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op;
}
static inline uint32_t enc_S(uint8_t rs2,uint8_t rs1,uint8_t f3,int32_t imm12){
    uint32_t u=(uint32_t)imm12; uint32_t i11_5=(u>>5)&0x7F, i4_0=u&0x1F;
    return (i11_5<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(i4_0<<7)|0x23;
}
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






// ---------- Part A: basic syscalls + timer ----------
static void emit_syscall_demo(Memory& ram, uint32_t PC){
    const char* msg = "Hello, heap & timer!\\n";
    uint32_t MSG = 0x0800;
    for(uint32_t i=0; msg[i]; ++i) put8(ram, MSG+i, (uint8_t)msg[i]);

    emit_li32(ram, PC+0x00, 10, MSG);                // a0=ptr
    put32(ram, PC+0x08, enc_I(0x13,11,0b000,0,20));  // a1=len
    put32(ram, PC+0x0C, enc_I(0x13,17,0b000,0,4));   // a7=write_str
    put32(ram, PC+0x10, ECALL());

    put32(ram, PC+0x14, enc_I(0x13,10,0b000,0,32));  // a0=+32
    put32(ram, PC+0x18, enc_I(0x13,17,0b000,0,3));   // sbrk
    put32(ram, PC+0x1C, ECALL());
    put32(ram, PC+0x20, enc_I(0x13,17,0b000,0,1));   // print_u32(old_brk)
    put32(ram, PC+0x24, ECALL());

    put32(ram, PC+0x28, enc_I(0x13,17,0b000,0,8));   // get_time
    put32(ram, PC+0x2C, ECALL());
    put32(ram, PC+0x30, enc_I(0x13,17,0b000,0,1));   // print_u32(time)
    put32(ram, PC+0x34, ECALL());
}

// ---------- Part B: malloc reuse proof ----------
static void emit_alloc_demo(Memory& m, uint32_t PC){
    put32(m, PC+0x00, enc_I(0x13,10,0b000,0,16));  // a0=16
    put32(m, PC+0x04, enc_I(0x13,17,0b000,0,5));   // malloc
    put32(m, PC+0x08, ECALL());                    // a0=p1
    put32(m, PC+0x0C, enc_I(0x13,9,0b000,10,0));   // x9=p1
    put32(m, PC+0x10, enc_I(0x13,10,0b000,9,0));   // a0=x9
    put32(m, PC+0x14, enc_I(0x13,17,0b000,0,6));   // free(a0)
    put32(m, PC+0x18, ECALL());
    put32(m, PC+0x1C, enc_I(0x13,10,0b000,0,24));  // a0=24
    put32(m, PC+0x20, enc_I(0x13,17,0b000,0,5));   // malloc
    put32(m, PC+0x24, ECALL());                    // a0=p3
    put32(m, PC+0x28, enc_I(0x13,17,0b000,0,1));   // print_u32(p3)
    put32(m, PC+0x2C, ECALL());
}

// ---------- Part C: threads that race on a shared counter ----------
static void emit_inc_worker(Memory& m, uint32_t base, uint32_t counter_addr,
                            uint32_t iters, bool use_lock, uint32_t lock_addr)
{
    // a5 = counter base, a6 = iterations remaining
    emit_li32(m, base+0x00, 15, counter_addr);       // a5
    emit_li32(m, base+0x08, 16, iters);              // a6
    if(use_lock) emit_li32(m, base+0x10, 10, lock_addr); // a0=lock addr (for ECALLs)

    uint32_t L = base+0x18;

    if(use_lock){                                    // lock(a0)
        put32(m, L+0x00, enc_I(0x13,17,0b000,0,9));  // a7=9
        put32(m, L+0x04, ECALL());
    } else {
        // no-op padding, keeps layout similar
        put32(m, L+0x00, enc_I(0x13, 0,0b000,0,0));  // addi x0,x0,0
        put32(m, L+0x04, enc_I(0x13, 0,0b000,0,0));
    }

    // t0 = *(a5)
    put32(m, L+0x08, enc_I(0x03, 5,0b010,15,0));     // lw t0(=x5), 0(a5)
    // t0 = t0 + 1
    put32(m, L+0x0C, enc_R(0, 0, 5, 0b000, 5));      // add x5,x5,x0  (just to show R type)
    put32(m, L+0x10, enc_I(0x13, 5,0b000,5,1));      // addi x5,x5,1
    // *(a5) = t0
    put32(m, L+0x14, enc_S(5,15,0b010,0));           // sw x5,0(a5)

    if(use_lock){                                    // unlock(a0)
        put32(m, L+0x18, enc_I(0x13,17,0b000,0,10)); // a7=10
        put32(m, L+0x1C, ECALL());
    } else {
        put32(m, L+0x18, enc_I(0x13, 0,0b000,0,0));
        put32(m, L+0x1C, enc_I(0x13, 0,0b000,0,0));
    }

    // a6-- ; if (a6!=0) goto L
    put32(m, L+0x20, enc_I(0x13,16,0b000,16,-1));    // addi a6,a6,-1
    put32(m, L+0x24, enc_B(0b001,16,0,(int32_t)L-(int32_t)(L+0x24))); // bne a6,x0,L

    // exit(0)
    put32(m, L+0x28, enc_I(0x13,10,0b000,0,0));
    put32(m, L+0x2C, enc_I(0x13,17,0b000,0,0));
    put32(m, L+0x30, ECALL());
}

int main(){
    Memory ram(64*1024);
    // Try to load and run an ELF if one is present; otherwise run built-in demos.
    const char* ELF_PATH = "program.elf"; // place program.elf next to the binary or run dir
    if (file_exists(ELF_PATH)) {
        uint32_t entry = load_elf32_into_memory(ELF_PATH, ram);
        CPU elf_cpu; elf_cpu.pc = entry; elf_cpu.quantum = 200; elf_cpu.tid = 0;
        std::cout << "[elf] loaded '"<<ELF_PATH<<"' entry=0x" << std::hex << entry << std::dec << "\n";
        // Run until it exits (ECALL 0) or halts
        for (int steps=0; steps<10'000'000 && !elf_cpu.halted; ++steps) {
            elf_cpu.step(ram);
        }
        std::cout << "[elf] finished exit_code="<<elf_cpu.exit_code
                  << " instret="<<elf_cpu.instret<<" cycles="<<elf_cpu.cycles<<"\n\n";
    } else {
        std::cout << "[elf] '"<<ELF_PATH<<"' not found; running built-in demos.\n";
    }


    // --- Part A & B quick checks ---
    emit_syscall_demo(ram, 0x0000);
    emit_alloc_demo  (ram, 0x0040);

    {
        CPU tmp; tmp.pc = 0x0000;
        for(int i=0;i<200 && !tmp.halted; ++i) tmp.step(ram);
    }
    {
        CPU tmp; tmp.pc = 0x0040;
        for(int i=0;i<200 && !tmp.halted; ++i) tmp.step(ram);
    }

    // --- Part C: race demo settings ---
    const uint32_t COUNTER = 0x0600;         // shared counter in RAM
    ram.store32(COUNTER, 0);
    const uint32_t LOCK    = 0x0608;         // lock “address” (just an id)

    // Make 6 threads: 3 without lock, then reset & 3 with lock
    auto run_group = [&](bool use_lock){
        // reset counter
        ram.store32(COUNTER, 0);

        // Program images
        uint32_t base = use_lock ? 0x1200 : 0x0800;
        for (int i=0;i<3;i++){
            emit_inc_worker(ram, base + i*0x100, COUNTER, 500, use_lock, LOCK);
        }

        // Create CPUs
        std::vector<CPU> threads(3);
        for (int i=0;i<3;i++){
            threads[i].pc      = base + i*0x100;
            threads[i].quantum = 50;
            threads[i].prio    = 1;
            threads[i].tid     = i;
        }

        // Ready queue: indices sorted by priority then FIFO
        std::vector<int> ready = {0,1,2};
        auto pick = [&](){
            // all same prio → FIFO; if different, pick smallest prio
            return ready.front();
        };

        while(true){
            // stop condition: all halted
            bool all_halted=true;
            for(auto& t:threads) all_halted &= t.halted;
            if(all_halted) break;

            // skip halted at front
            while(!ready.empty() && threads[ready.front()].halted) {
                ready.erase(ready.begin());
                if(ready.empty()){
                    // re-seed with any non-halted
                    for(int i=0;i<3;i++) if(!threads[i].halted) ready.push_back(i);
                }
            }
            int idx = pick();
            CPU& cur = threads[idx];

            // run until yield/halt
            for(int k=0;k<1000 && !cur.halted && !cur.yielded; ++k) cur.step(ram);

            // requeue if not halted
            if(!cur.halted) {
                ready.erase(std::find(ready.begin(),ready.end(),idx));
                ready.push_back(idx);
            } else {
                ready.erase(std::find(ready.begin(),ready.end(),idx));
                // if queue empties, repopulate with remaining non-halted
                if(ready.empty()){
                    for(int i=0;i<3;i++) if(!threads[i].halted) ready.push_back(i);
                }
            }
        }

        uint32_t final = ram.load32(COUNTER);
        std::cout << (use_lock ? "[with lock] " : "[no lock] ")
                  << "counter=" << final << " (expected " << 3*10000 << " with lock)\n";
        for (int i=0;i<3;i++){
            const auto& c = threads[i];
            double cpi = (c.instret==0)?0.0:(double)c.cycles/(double)c.instret;
            std::cout << "  T" << i << ": instret="<<c.instret<<" cycles="<<c.cycles
                      << " CPI="<<std::fixed<<std::setprecision(2)<<cpi << "\n";
        }
    };

    std::cout << "\n--- Race without lock ---\n";
    run_group(false);
    std::cout << "\n--- Race with lock ---\n";
    run_group(true);

    std::cout << "[timer] ticks="<<ram.time()<<"\n";
    
    
    // Dump trace next to the binary (Xcode runs from your build dir)
    if (!global_trace().write_ndjson("seedos_trace.ndjson")){
        std::cerr << "[trace] failed to write trace file\n";
    } else {
        std::cout << "[trace] wrote seedos_trace.ndjson ("
                  << "last instructions retained)\n";
    }

    return 0;
}
