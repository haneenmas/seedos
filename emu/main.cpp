// emu/main.cpp
#include <iostream>
#include <iomanip>
#include <thread>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <cctype>
#include <array>
#include <unordered_set>
#include <sys/stat.h>

#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"
#include "trace.hpp"
#include "elf.hpp"       // ELF loader (we fallback to demos if file missing)
#include "sync.hpp"      // spinlock + LockedCounter
#include "syscall.hpp"
#include "spsc.hpp"// handle_ecall
#include "asm.hpp"

// ------------------ small helpers ------------------
static inline std::string hex32(uint32_t x){
    std::ostringstream os;
    os << std::hex << std::showbase << x;
    return os.str();
}

static inline bool file_exists(const char* p){
    struct stat st; return ::stat(p, &st) == 0;
}

// encoders we use in demos
static inline uint32_t enc_I(uint8_t rd, uint8_t rs1, int32_t imm12, uint8_t funct3){
    uint32_t u = (uint32_t)(imm12 & 0xFFF);
    return (u<<20)|(rs1<<15)|(funct3<<12)|(rd<<7)|0x13; // ADDI op
}
static inline uint32_t enc_R(uint8_t rd, uint8_t rs1, uint8_t rs2,
                             uint8_t funct3, uint8_t funct7){
    return (funct7<<25)|(rs2<<20)|(rs1<<15)|(funct3<<12)|(rd<<7)|0x33; // R-type
}
static inline uint32_t enc_LUI(uint8_t rd, uint32_t imm20){
    return ((imm20 & 0xFFFFF) << 12) | (rd<<7) | 0x37; // LUI
}
static inline uint32_t enc_SYSTEM(uint32_t imm12){ return (imm12<<20) | 0x73; } // ECALL/EBREAK

static inline void put32(Memory& m, uint32_t addr, uint32_t w){ m.store32(addr, w); }

// ------------------ tiny programs for scheduler demos ------------------
static void load_task_program(Memory& ram, uint32_t base, int which){
    uint32_t a = base;
    if (which == 1){
        put32(ram, a+0x00, enc_I(1, 0, 5, 0));                     // addi x1,x0,5
        put32(ram, a+0x04, enc_I(2, 1,10, 0));                     // addi x2,x1,10
        put32(ram, a+0x08, enc_R(4, 1, 2, 0, 0));                  // add  x4,x1,x2
        put32(ram, a+0x0C, enc_R(5, 4, 1, 0, 0b0100000));          // sub  x5,x4,x1
        put32(ram, a+0x10, enc_SYSTEM(0));                         // ecall (exit)
        return;
    }
    // which == 2
    put32(ram, a+0x00, enc_LUI(3, 0x12));
    put32(ram, a+0x04, enc_I(3, 3, 34, 0));
    put32(ram, a+0x08, enc_I(6, 0,  0, 0));
    put32(ram, a+0x0C, enc_I(6, 6,  1, 0));
    put32(ram, a+0x10, enc_I(6, 6,  1, 0));
    put32(ram, a+0x14, enc_I(6, 6,  1, 0));
    put32(ram, a+0x18, enc_I(7, 6,  2, 0));
    put32(ram, a+0x1C, enc_SYSTEM(0));
}

struct Task{
    const char* name;
    CPU cpu;
    bool done = false;
    uint64_t steps = 0;
};

static void run_round_robin_demo(){
    std::cout << "\n[sched] round-robin demo\n";

    Memory ram(64*1024);
    Task A{"A"}, B{"B"};
    const uint32_t A_BASE = 0x0000;
    const uint32_t B_BASE = 0x1000;

    load_task_program(ram, A_BASE, 1); A.cpu.pc = A_BASE;
    load_task_program(ram, B_BASE, 2); B.cpu.pc = B_BASE;

    std::array<Task*,2> q = { &A, &B };
    const int QUANTUM = 20;            // instructions per slice
    const uint64_t MAX_STEPS = 2000;

    int cur = 0;
    uint64_t total = 0;

    while (total < MAX_STEPS){
        Task* t = q[cur];
        if (!t->done){
            int slice = 0;
            while (slice < QUANTUM && !t->done){
                if (t->cpu.halted){ t->done = true; break; }
                t->cpu.step(ram);
                ++t->steps; ++slice; ++total;
            }
            std::cout << "[sched] ran task " << t->name
                      << " for " << slice << " steps  pc=0x"
                      << std::hex << t->cpu.pc << std::dec << "\n";
        }
        if (A.done && B.done) break;
        cur = (cur + 1) % 2;
        if (q[cur]->done && !(A.done && B.done)) cur = (cur + 1) % 2;
    }

    std::cout << "[sched] finished: total=" << total
              << "  A.steps=" << A.steps << "  B.steps=" << B.steps << "\n";
}

// ------------------ tiny REPL debugger (with 'b' toggle) ------------------
static void dump_regs(const CPU& c){
    using std::hex; using std::dec;
    std::cout << "   pc=0x" << std::hex << c.pc << std::dec
              << " | x1="  << c.x[1] << " x2=" << c.x[2]
              << " x3=0x" << std::hex << c.x[3] << std::dec
              << " x4="  << c.x[4] << " x5=" << c.x[5]
              << " x6="  << c.x[6] << " x7=" << c.x[7] << "\n";
}
static void dump_words(const Memory& ram, uint32_t addr, int n){
    for(int i=0;i<n;i++){
        uint32_t a = addr + 4*i;
        uint32_t w = ram.load32(a);
        std::cout << "  " << hex32(a) << ": " << hex32(w)
                  << "  " << disasm(w) << "\n";
    }
}
static void disasm_ahead(const Memory& ram, uint32_t pc, int k){
    for(int i=0;i<k;i++){
        uint32_t a = pc + 4*i;
        std::cout << "  " << hex32(a) << ": " << disasm(ram.load32(a)) << "\n";
    }
}

static void run_repl(CPU& cpu, Memory& ram, std::unordered_set<uint32_t>& bps){
    auto help = []{
        std::cout <<
        "commands:\n"
        "  c                 continue until breakpoint/exit\n"
        "  s [n]             single-step n (default 1)\n"
        "  b <hex>           toggle software breakpoint at address\n"
        "  r                 show registers\n"
        "  m <addr> <n>      dump n words from addr (hex)\n"
        "  d [k]             disasm k ahead from current pc (default 4)\n"
        "  q                 quit debugger\n"
        "  h                 help\n";
    };
    help();

    std::string line;
    while(true){
        std::cout << "(dbg) pc=" << hex32(cpu.pc) << " > " << std::flush;
        if(!std::getline(std::cin, line)) break;
        std::istringstream iss(line);
        std::string cmd; iss >> cmd;

        if(cmd=="c"){
            while(!cpu.halted){
                if(bps.count(cpu.pc)){
                    std::cout << "[hit] breakpoint at " << hex32(cpu.pc) << "\n";
                    break;
                }
                cpu.step(ram);
            }
        } else if(cmd=="s"){
            int n=1; (void)(iss>>n);
            while(n-- > 0 && !cpu.halted){ cpu.step(ram); }
            std::cout << "next: " << hex32(cpu.pc) << "  "
                      << disasm(ram.load32(cpu.pc)) << "\n";

        } else if(cmd=="b"){
            std::string hx; iss >> hx;
            if(hx.empty()){
                std::cout << "usage: b <hexaddr>\n";
            }else{
                uint32_t a = std::stoul(hx, nullptr, 0);
                try{
                    bool enabled = ram.toggle_break(a);   // EBREAK patch/restore
                    if(enabled){
                        bps.insert(a);
                        std::cout << "+ bp " << hex32(a) << " (EBREAK patched)\n";
                    }else{
                        bps.erase(a);
                        std::cout << "- bp " << hex32(a) << " (restored)\n";
                    }
                }catch(const std::exception& e){
                    std::cout << "bp error: " << e.what() << "\n";
                }
            }

        } else if(cmd=="r"){
            dump_regs(cpu);

        } else if(cmd=="m"){
            std::string hx; int n=1; iss >> hx >> n;
            if(hx.empty()){ std::cout << "usage: m <hexaddr> <n>\n"; continue; }
            uint32_t a = std::stoul(hx, nullptr, 0);
            dump_words(ram, a, n);

        } else if(cmd=="d"){
            int k=4; (void)(iss>>k);
            disasm_ahead(ram, cpu.pc, k);

        } else if(cmd=="q"){
            break;

        } else if(cmd=="h" || cmd=="?"){
            help();

        } else if(cmd.empty()){
            continue;

        } else{
            std::cout << "unknown. type 'h' for help.\n";
        }
        if(cpu.halted) std::cout << "[halted]\n";
    }
}

// ------------------ main ------------------
int main(){
    // 0) bring up memory + a CPU
    Memory ram(64*1024);
    CPU    cpu; cpu.pc = 0;

    // 1) Try ELF; else run built-in demos
    const char* ELF_PATH = "program.elf";
    if (file_exists(ELF_PATH)) {
        uint32_t entry = load_elf32_into_memory(ELF_PATH, ram);
        CPU elf_cpu; elf_cpu.pc = entry; elf_cpu.quantum = 200; elf_cpu.tid = 0;
        std::cout << "[elf] loaded '" << ELF_PATH << "' entry=0x"
                  << std::hex << entry << std::dec << "\n";
        for (int i=0;i<10'000'000 && !elf_cpu.halted;i++) elf_cpu.step(ram);
        std::cout << "[elf] finished exit_code=" << elf_cpu.exit_code
                  << " instret=" << elf_cpu.instret
                  << " cycles="  << elf_cpu.cycles << "\n\n";
    } else {
        std::cout << "[elf] '" << ELF_PATH << "' not found; running built-in demos.\n";
    }

    // 2) Heap & “timer”
    std::cout << "Hello, heap & timer!";
    uint32_t start_brk = ram.sbrk(0);
    uint32_t ticks     = 11;
    uint32_t end_brk   = ram.sbrk(32);
    std::cout << start_brk << "\n" << ticks << "\n" << end_brk << "\n\n";

    // 3) race without lock
    std::cout << "--- Race without lock ---\n";
    {
        int counter = 0;
        auto work = [&]{ for(int i=0;i<1'000'000;i++) counter++; };
        std::thread t1(work), t2(work); t1.join(); t2.join();
        std::cout << "[race] racy result=" << counter << "\n\n";
    }

    // 4) race with lock
    std::cout << "[race] --- with lock ---\n";
    {
        LockedCounter c2;
        auto work = [&]{ for(int i=0;i<1'000'000;i++) c2.add(1); };
        std::thread t1(work), t2(work); t1.join(); t2.join();
        std::cout << "[race] locked result=" << c2.value.load() << "\n\n";
    }

    // 5) syscall demo
    std::cout << "[sys] demo calls\n";
    {
        CPU kcpu; Memory kram(64*1024);
        kcpu.x[17]=1; kcpu.x[10]=123; handle_ecall(kcpu, kram);  // puti(123)
        kcpu.x[17]=2; kcpu.x[10]=64;  handle_ecall(kcpu, kram);  // sbrk(64)
        kcpu.x[17]=3;                 handle_ecall(kcpu, kram);  // cycles
        kcpu.x[17]=4; kcpu.x[10]='A'; handle_ecall(kcpu, kram);  // putch('A')
        kcpu.x[17]=0; kcpu.x[10]=0;   handle_ecall(kcpu, kram);  // exit(0)
    }
    std::cout << "\n";

    // 6) user program that writes a string via ECALL(write)
    std::cout << "[user] demo: write string via ECALL\n";
    {
        Memory urm(64*1024);
        CPU ucpu; ucpu.pc = 0;
        const char* msg = "Hello from user space!\n";
        constexpr uint32_t BASE = 0x200;
        for(uint32_t i=0; msg[i]; ++i) urm.store8(BASE+i,(uint8_t)msg[i]);
        uint32_t len = (uint32_t)std::strlen(msg);
        urm.store32(0x00, enc_LUI(10, BASE >> 12));                     // a0 = upper BASE
        urm.store32(0x04, enc_I(10, 10, (int32_t)(BASE & 0xFFF), 0));   // a0 += low12
        urm.store32(0x08, enc_I(11, 0, (int32_t)len, 0));               // a1 = len
        urm.store32(0x0C, enc_I(17, 0, 5, 0));                          // a7 = write
        urm.store32(0x10, enc_SYSTEM(0));                               // ecall
        urm.store32(0x14, enc_I(10, 0, 0, 0));                          // a0=0
        urm.store32(0x18, enc_I(17, 0, 0, 0));                          // a7=0
        urm.store32(0x1C, enc_SYSTEM(0));                               // ecall(exit)
        for (int i=0;i<50 && !ucpu.halted;i++) ucpu.step(urm);
    }

    // 7) interactive REPL with software breakpoints
    std::cout << "\n[dbg] breakpoints + single-step demo\n";
    {
        Memory drm(64*1024);
        CPU d; d.pc = 0;

        drm.store32(0x00, enc_I(1, 0, 5, 0));                      // addi x1, x0, 5
        drm.store32(0x04, enc_I(2, 1,10, 0));                      // addi x2, x1, 10
        drm.store32(0x08, enc_R(4, 1, 2, 0, 0));                   // add  x4, x1, x2
        drm.store32(0x0C, enc_R(5, 4, 1, 0, 0b0100000));           // sub  x5, x4, x1
        drm.store32(0x10, enc_I(6, 5, 1, 0));                      // addi x6, x5, 1
        drm.store32(0x14, enc_I(7, 6, 2, 0));                      // addi x7, x6, 2
        drm.store32(0x18, enc_I(10,0,0,0));                        // a0=0
        drm.store32(0x1C, enc_I(17,0,0,0));                        // a7=0
        drm.store32(0x20, enc_SYSTEM(0));                          // ecall(exit)

        std::unordered_set<uint32_t> bps = { 0x0C };
        // example bp
        std::cout << "[asm] mini-assembler demo\n";
            Memory m(64*1024); CPU c; c.pc=0;
            std::string src = R"(
                addi x1, x0, 5
                addi x2, x1, 10
                loop:
                add  x4, x1, x2
                sub  x5, x4, x1
                beq  x5, x2, done
                jal  loop
                done:
                ecall
            )";
            assemble_to_memory(src, m, 0);
            for(int i=0;i<50 && !c.halted;i++) c.step(m);
        std::cout << "[spsc] demo\n";
           SPSCQueue<int, 1024> q;
           std::atomic<bool> done{false};
           std::thread prod([&]{ for(int i=0;i<100000;i++){ while(!q.push(i)){} } done=true; });
           int got=0,x=0;
           std::thread cons([&]{ while(!done.load() || !q.empty()){ if(q.pop(x)) got++; } });
           prod.join(); cons.join();
           std::cout << "[spsc] received=" << got << "\n";
        run_repl(d, drm, bps);
    }
    

    // 8) round-robin scheduler
    run_round_robin_demo();

    return 0;
}
