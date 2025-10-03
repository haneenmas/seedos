// emu/main.cpp
#include <iostream>
#include <iomanip>
#include <thread>
#include <array>
#include <unordered_set>
#include <sstream>
#include <cstring>
#include <cctype>
#include <cstdint>
#include <vector>
#include <sys/stat.h>

#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"
#include "trace.hpp"
#include "elf.hpp"
#include "sync.hpp"
#include "syscall.hpp"

// -------------------------------
// Small utilities used everywhere
// -------------------------------
static inline bool file_exists(const char* p){
    struct stat st; return ::stat(p, &st) == 0;
}

static std::string hex32(uint32_t x){
    std::ostringstream os; os << std::hex << std::showbase << x; return os.str();
}

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
        uint32_t w = ram.load32(a);
        std::cout << "  " << hex32(a) << ": " << disasm(w) << "\n";
    }
}

// ---------- encoders ----------
static inline void put32(Memory& m, uint32_t addr, uint32_t word){ m.store32(addr, word); }
static inline uint32_t enc_I(uint8_t rd, uint8_t rs1, int32_t imm12, uint8_t f3){
    uint32_t u = (uint32_t)(imm12 & 0xFFF);
    return (u<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|0x13;
}
static inline uint32_t enc_R(uint8_t rd, uint8_t rs1, uint8_t rs2, uint8_t f3, uint8_t f7){
    return (f7<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|0x33;
}
static inline uint32_t enc_LUI(uint8_t rd, uint32_t imm20){
    return ((imm20 & 0xFFFFF)<<12)|(rd<<7)|0x37;
}
static inline uint32_t enc_ECALL(){ return 0x00000073u; }

// ---------- tiny program images for two "processes" ----------
static void load_task_program(Memory& ram, uint32_t base, int which){
    uint32_t a = base;
    if (which == 1){
        put32(ram, a+0x00, enc_I(1, 0, 5, 0));                 // addi x1,x0,5
        put32(ram, a+0x04, enc_I(2, 1,10, 0));                 // addi x2,x1,10
        put32(ram, a+0x08, enc_R(4, 1, 2, 0, 0));              // add  x4,x1,x2
        put32(ram, a+0x0C, enc_R(5, 4, 1, 0, 0x20));           // sub  x5,x4,x1
        put32(ram, a+0x10, enc_ECALL());                       // exit
        return;
    }
    put32(ram, a+0x00, enc_LUI(3, 0x12));                      // x3 = 0x12000
    put32(ram, a+0x04, enc_I(3, 3, 34, 0));                    // x3 += 34
    put32(ram, a+0x08, enc_I(6, 0,  0, 0));                    // x6 = 0
    put32(ram, a+0x0C, enc_I(6, 6,  1, 0));                    // x6++
    put32(ram, a+0x10, enc_I(6, 6,  1, 0));                    // x6++
    put32(ram, a+0x14, enc_I(6, 6,  1, 0));                    // x6++
    put32(ram, a+0x18, enc_I(7, 6,  2, 0));                    // x7 = x6 + 2
    put32(ram, a+0x1C, enc_ECALL());                           // exit
}

// -------------------------- debugger helpers --------------------------
static void run_with_breakpoints(CPU& cpu, Memory& ram,
                                 std::initializer_list<uint32_t> bp_list,
                                 int max_steps = 200)
{
    std::unordered_set<uint32_t> bps(bp_list);
    for (int i = 0; i < max_steps && !cpu.halted; ++i) {
        if (bps.count(cpu.pc)) {
            uint32_t inst = ram.load32(cpu.pc);
            std::cout << "[brk] pc=0x" << std::hex << cpu.pc << std::dec
                      << "  " << disasm(inst) << "\n";
            dump_regs(cpu);
            for (int s = 0; s < 3 && !cpu.halted; ++s) {
                cpu.step(ram);
                uint32_t ninst = ram.load32(cpu.pc);
                std::cout << "  -> next pc=0x" << std::hex << cpu.pc << std::dec
                          << "  " << disasm(ninst) << "\n";
            }
        } else {
            cpu.step(ram);
        }
    }
}

static void run_repl(CPU& cpu, Memory& ram, std::unordered_set<uint32_t>& bps){
    auto help = []{
        std::cout <<
        "commands:\n"
        "  c                 continue until breakpoint/exit\n"
        "  s [n]             single-step n (default 1)\n"
        "  b <hex>           toggle breakpoint (e.g. b 0xC)\n"
        "  r                 show registers\n"
        "  m <addr> <n>      dump n words from addr (hex)\n"
        "  d [k]             disasm k ahead (default 4)\n"
        "  q                 quit\n"
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
                if(bps.count(cpu.pc)) { std::cout << "[hit] " << hex32(cpu.pc) << "\n"; break; }
                cpu.step(ram);
            }
        }else if(cmd=="s"){
            int n=1; (void)(iss>>n);
            while(n-- > 0 && !cpu.halted) cpu.step(ram);
            uint32_t w = ram.load32(cpu.pc);
            std::cout << "next: " << hex32(cpu.pc) << "  " << disasm(w) << "\n";
        }else if(cmd=="b"){
            std::string hx; iss>>hx; uint32_t a = std::stoul(hx,nullptr,0);
            if(bps.erase(a)) std::cout<<"- bp "<<hex32(a)<<"\n";
            else { bps.insert(a); std::cout<<"+ bp "<<hex32(a)<<"\n"; }
        }else if(cmd=="r"){
            dump_regs(cpu);
        }else if(cmd=="m"){
            std::string hx; int n=1; iss>>hx>>n; dump_words(ram, std::stoul(hx,nullptr,0), n);
        }else if(cmd=="d"){
            int k=4; (void)(iss>>k); disasm_ahead(ram, cpu.pc, k);
        }else if(cmd=="q"){
            break;
        }else if(cmd=="h" || cmd=="?"){
            help();
        }else if(cmd.empty()){
            continue;
        }else{
            std::cout << "unknown. type 'h' for help.\n";
        }
        if(cpu.halted) std::cout << "[halted]\n";
    }
}

// -------------------------- schedulers --------------------------
struct Task { const char* name; CPU cpu; bool done=false; uint64_t steps=0; };

static void run_round_robin_demo(){
    std::cout << "\n[sched] round-robin demo\n";
    Memory ram(64*1024);
    Task A{"A"}, B{"B"};
    const uint32_t A_BASE=0x0000, B_BASE=0x1000;
    load_task_program(ram, A_BASE, 1); A.cpu.pc = A_BASE;
    load_task_program(ram, B_BASE, 2); B.cpu.pc = B_BASE;

    std::array<Task*,2> q = { &A, &B };
    const int QUANTUM = 20;
    const uint64_t MAX_STEPS = 2000;

    int cur = 0; uint64_t total = 0;
    while(total < MAX_STEPS){
        Task* t = q[cur];
        if(!t->done){
            int slice = 0;
            while(slice < QUANTUM && !t->done){
                if(t->cpu.halted){ t->done = true; break; }
                t->cpu.step(ram);
                ++t->steps; ++slice; ++total;
            }
            std::cout << "[sched] ran task " << t->name
                      << " for " << slice << " steps  pc=0x"
                      << std::hex << t->cpu.pc << std::dec << "\n";
        }
        if(A.done && B.done) break;
        cur = (cur+1) % 2;
        if(q[cur]->done && !(A.done && B.done)) cur = (cur+1) % 2;
    }
    std::cout << "[sched] finished: total=" << total
              << "  A.steps=" << A.steps << "  B.steps=" << B.steps << "\n";
}

static void run_round_robin_preemptive_demo(){
    std::cout << "[sched] preemptive RR demo\n";
    Task A{"A"}, B{"B"};
    const uint32_t A_BASE=0x0000, B_BASE=0x1000;
    Memory ram(64*1024);
    load_task_program(ram, A_BASE, 1); A.cpu.pc = A_BASE;
    load_task_program(ram, B_BASE, 2); B.cpu.pc = B_BASE;

    Task* q[2] = { &A, &B };
    const int SLICE = 20; const int MAX_TOTAL = 2000;
    int cur = 0, total = 0;
    while(total < MAX_TOTAL && !(A.cpu.halted && B.cpu.halted)){
        Task& t = *q[cur];
        t.cpu.quantum = SLICE;          // “timer”
        int ran = 0;
        while(t.cpu.quantum > 0 && !t.cpu.halted){ t.cpu.step(ram); ran++; total++; }
        std::cout << "[sched] preempted task " << t.name
                  << " after " << ran << " steps  pc=0x"
                  << std::hex << t.cpu.pc << std::dec << "\n";
        cur ^= 1;
    }
    std::cout << "[sched] DONE total=" << total
              << "  A.steps=" << A.cpu.cycles
              << "  B.steps=" << B.cpu.cycles << "\n";
}

// -------------------------- CLI options --------------------------
struct Options {
    std::string elf = "program.elf";
    bool heap = false, race=false, sys=false, user=false, dbg=false, rr=false, rrp=false, all=true;
};

static void print_help(){
    std::cout <<
    "seedos usage:\n"
    "  --elf <path>     try to load ELF (default program.elf)\n"
    "  --heap           run heap/timer demo\n"
    "  --race           run race w/ and w/o lock\n"
    "  --sys            run syscall demo\n"
    "  --user-ecall     run user-mode write-string demo\n"
    "  --dbg            run interactive debugger demo\n"
    "  --rr             run cooperative round-robin\n"
    "  --rrp            run preemptive round-robin\n"
    "  --all            run everything (default if no flags)\n"
    "  --help           show this help\n";
}

static Options parse_cli(int argc, char** argv){
    Options o;
    for(int i=1;i<argc;i++){
        std::string a = argv[i];
        auto need = [&](bool &flag){ o.all=false; flag=true; };
        if(a=="--help"){ print_help(); std::exit(0); }
        else if(a=="--all"){ o.all=true; }
        else if(a=="--elf" && i+1<argc){ o.elf = argv[++i]; }
        else if(a=="--heap"){ need(o.heap); }
        else if(a=="--race"){ need(o.race); }
        else if(a=="--sys"){ need(o.sys); }
        else if(a=="--user-ecall"){ need(o.user); }
        else if(a=="--dbg"){ need(o.dbg); }
        else if(a=="--rr"){ need(o.rr); }
        else if(a=="--rrp"){ need(o.rrp); }
        else { std::cerr << "unknown arg: " << a << "\n"; print_help(); std::exit(1); }
    }
    return o;
}

// =========================== main ===========================
int main(int argc, char** argv){
    Options opt = parse_cli(argc, argv);

    // reusable RAM/CPU for ELF & heap demo
    Memory ram(64*1024);
    CPU cpu; cpu.pc = 0;

    // 1) ELF (always attempted first; if it fails, we fall through)
    if (file_exists(opt.elf.c_str())) {
        uint32_t entry = load_elf32_into_memory(opt.elf.c_str(), ram);
        CPU elf_cpu; elf_cpu.pc = entry; elf_cpu.quantum = 200; elf_cpu.tid = 0;
        std::cout << "[elf] loaded '" << opt.elf << "' entry=0x"
                  << std::hex << entry << std::dec << "\n";
        for (int steps=0; steps<10'000'000 && !elf_cpu.halted; ++steps) elf_cpu.step(ram);
        std::cout << "[elf] finished exit_code=" << elf_cpu.exit_code
                  << " instret=" << elf_cpu.instret
                  << " cycles="  << elf_cpu.cycles << "\n\n";
    } else {
        std::cout << "[elf] '" << opt.elf << "' not found; running selected demos.\n";
    }

    // If no specific flags => run all sections
    bool ALL = opt.all;

    // 2) Heap “timer”
    if (ALL || opt.heap){
        std::cout << "Hello, heap & timer!";
        uint32_t start_brk = ram.sbrk(0);
        uint32_t ticks     = 11;
        uint32_t end_brk   = ram.sbrk(32);
        std::cout << start_brk << "\n" << ticks << "\n" << end_brk << "\n\n";
    }

    // 3) Race without/with lock
    if (ALL || opt.race){
        std::cout << "--- Race without lock ---\n";
        {
            int counter = 0;
            auto work = [&]{ for(int i=0;i<1'000'000;i++) counter++; };
            std::thread t1(work), t2(work); t1.join(); t2.join();
            std::cout << "[race] racy result=" << counter << "\n\n";
        }
        std::cout << "[race] --- with lock ---\n";
        {
            LockedCounter c2;
            auto work = [&]{ for(int i=0;i<1'000'000;i++) c2.add(1); };
            std::thread t1(work), t2(work); t1.join(); t2.join();
            std::cout << "[race] locked result=" << c2.value.load() << "\n\n";
        }
    }

    // 4) ECALL syscalls demo
    if (ALL || opt.sys){
        std::cout << "[sys] demo calls\n";
        CPU kcpu; Memory kram(64*1024);
        kcpu.x[17]=1; kcpu.x[10]=123; handle_ecall(kcpu,kram);
        kcpu.x[17]=2; kcpu.x[10]=64;  handle_ecall(kcpu,kram);
        kcpu.x[17]=3;                 handle_ecall(kcpu,kram);
        kcpu.x[17]=4; kcpu.x[10]='A'; handle_ecall(kcpu,kram);
        kcpu.x[17]=0; kcpu.x[10]=0;   handle_ecall(kcpu,kram);
        std::cout << "\n";
    }

    // 5) User-mode string via ECALL write
    if (ALL || opt.user){
        std::cout << "[user] demo: write string via ECALL\n";
        Memory um(64*1024); CPU u; u.pc=0;
        const char* msg = "Hello from user space!\n";
        constexpr uint32_t BASE=0x200;
        for(uint32_t i=0; msg[i]; ++i) um.store8(BASE+i, (uint8_t)msg[i]);
        uint32_t len=(uint32_t)std::strlen(msg);
        auto encSYSTEM=[](uint32_t imm12){ return (imm12<<20)|0x73; };
        um.store32(0x00, enc_LUI(10, BASE>>12));
        um.store32(0x04, enc_I(10,10,(int32_t)(BASE & 0xFFF),0));
        um.store32(0x08, enc_I(11, 0, (int32_t)len, 0));
        um.store32(0x0C, enc_I(17, 0, 5, 0));
        um.store32(0x10, encSYSTEM(0));
        um.store32(0x14, enc_I(10, 0, 0, 0));
        um.store32(0x18, enc_I(17, 0, 0, 0));
        um.store32(0x1C, encSYSTEM(0));
        for (int i=0;i<50 && !u.halted;i++) u.step(um);
    }

    // 6) Debugger & schedulers
    if (ALL || opt.dbg){
        std::cout << "\n[dbg] breakpoints + single-step demo\n";
        Memory m(64*1024); CPU c; c.pc=0;
        m.store32(0x00, enc_I(1,0,5,0));
        m.store32(0x04, enc_I(2,1,10,0));
        m.store32(0x08, enc_R(4,1,2,0,0));
        m.store32(0x0C, enc_R(5,4,1,0,0x20));
        m.store32(0x10, enc_I(6,5,1,0));
        m.store32(0x14, enc_I(7,6,2,0));
        auto encSYSTEM=[](uint32_t imm12){ return (imm12<<20)|0x73; };
        m.store32(0x18, enc_I(10,0,0,0)); m.store32(0x1C, enc_I(17,0,0,0)); m.store32(0x20, encSYSTEM(0));
        std::unordered_set<uint32_t> bps={0x0C};
        run_repl(c, m, bps);
    }

    if (ALL || opt.rr)  run_round_robin_demo();
    if (ALL || opt.rrp) run_round_robin_preemptive_demo();

    return 0;
}
