// emu/main.cpp
#include <iostream>
#include <iomanip>
#include <thread>
#include <cstdint>
#include <sys/stat.h>

#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"
#include "trace.hpp"
#include "elf.hpp"      // keep even if ELF is missing; we fallback
#include "sync.hpp"     // <- the spinlock + LockedCounter
#include "syscall.hpp"

// simple helper: does a file exist?
static bool file_exists(const char* p){ struct stat st; return ::stat(p,&st)==0; }

int main(){
    // -------------------------------
    // 0) Bring up memory + a CPU
    // -------------------------------
    Memory ram(64 * 1024);   // 64 KB RAM
    CPU    cpu; cpu.pc = 0;  // we may reuse this CPU for demos

    // -----------------------------------------------------------
    // 1) Try to load and run an ELF; otherwise run built-in demos
    // -----------------------------------------------------------
    const char* ELF_PATH = "program.elf";

    if (file_exists(ELF_PATH)) {
        uint32_t entry = load_elf32_into_memory(ELF_PATH, ram);
        CPU elf_cpu;
        elf_cpu.pc      = entry;
        elf_cpu.quantum = 200;
        elf_cpu.tid     = 0;

        std::cout << "[elf] loaded '" << ELF_PATH << "' entry=0x"
                  << std::hex << entry << std::dec << "\n";

        // run until ECALL 0 (exit) or halt
        for (int steps = 0; steps < 10'000'000 && !elf_cpu.halted; ++steps)
            elf_cpu.step(ram);

        std::cout << "[elf] finished exit_code=" << elf_cpu.exit_code
                  << " instret=" << elf_cpu.instret
                  << " cycles="  << elf_cpu.cycles << "\n\n";
    } else {
        std::cout << "[elf] '" << ELF_PATH << "' not found; running built-in demos.\n";
    }

    // -----------------------------------------------------------
    // 2) Heap “timer” demo: show program break movement
    //    (our Memory has a bump-pointer heap via sbrk)
    // -----------------------------------------------------------
    std::cout << "Hello, heap & timer!";
    uint32_t start_brk = ram.sbrk(0);     // read current break
    uint32_t ticks     = 11;              // pretend 11 ticks happened
    uint32_t end_brk   = ram.sbrk(32);    // grow heap by 32 bytes
    std::cout << start_brk << "\n"        // prints 8192 in your run
              << ticks     << "\n"        // 11
              << end_brk   << "\n\n";     // 8224

    // -----------------------------------------------------------
    // 3) Race without lock (intentionally racy)
    // -----------------------------------------------------------
    std::cout << "--- Race without lock ---\n";
    {
        int counter = 0;                  // plain int -> data race
        auto work = [&]{
            for (int i = 0; i < 1'000'000; ++i) counter++;  // non-atomic
        };
        std::thread t1(work), t2(work);
        t1.join(); t2.join();
        std::cout << "[race] racy result=" << counter << "\n\n";
    }

    // -----------------------------------------------------------
    // 4) Race with a spinlock (deterministic)
    // -----------------------------------------------------------
    std::cout << "[race] --- with lock ---\n";
    {
        LockedCounter c2;                 // value + SpinLock
        auto work = [&]{
            for (int i = 0; i < 1'000'000; ++i) c2.add(1);
        };
        std::thread t1(work), t2(work);
        t1.join(); t2.join();
        std::cout << "[race] locked result=" << c2.value.load() << "\n\n";
    }
    std::cout << "[sys] demo calls\n";
    {
        CPU kcpu; Memory kram(64*1024);     // tiny scratch env

        // print an int
        kcpu.x[17] = 1;     // a7 = puti
        kcpu.x[10] = 123;   // a0 = 123
        handle_ecall(kcpu, kram);

        // grow heap by 64; return old break in a0
        kcpu.x[17] = 2;     // a7 = sbrk
        kcpu.x[10] = 64;    // delta
        handle_ecall(kcpu, kram);

        // query cycles (will be 0 in this tiny CPU we just made)
        kcpu.x[17] = 3;     // a7 = cycles
        handle_ecall(kcpu, kram);

        // print a character
        kcpu.x[17] = 4;     // a7 = putch
        kcpu.x[10] = 'A';
        handle_ecall(kcpu, kram);

        // exit(0)
        kcpu.x[17] = 0;     // a7 = exit
        kcpu.x[10] = 0;     // code
        handle_ecall(kcpu, kram);
    }
    std::cout << "\n";


    return 0;
}

