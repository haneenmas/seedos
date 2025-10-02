#include <iostream>
#include "cpu.hpp"
#include "mem.hpp"
#include "syscall.hpp"

// Minimal calling convention (like a tiny OS ABI):
//   a7 = syscall id
//   a0, a1 = arguments / return in a0
//
// IDs we implement:
//   0: exit            (a0 = exit_code)
//   1: puti            (a0 = integer to print)
//   2: sbrk            (a0 = delta; returns old break in a0)
//   3: cycles          (returns cpu.cycles in a0)
//   4: putch           (a0 = byte/char)
//
// Note: register numbers: a0=x10, a1=x11, a7=x17.

bool handle_ecall(CPU& cpu, Memory& mem){
    uint32_t a0 = cpu.x[10];
    uint32_t a1 = cpu.x[11];
    uint32_t id = cpu.x[17];

    switch(id){
    case 0: { // exit
        cpu.exit_code = static_cast<int32_t>(a0);
        cpu.halted    = true;
        std::cout << "[sys] exit(" << cpu.exit_code << ")\n";
        return true;
    }
    case 1: { // puti
        std::cout << "[sys] puti(" << static_cast<int32_t>(a0) << ")\n";
        return true;
    }
    case 2: { // sbrk
        int32_t delta = static_cast<int32_t>(a0);
        uint32_t old  = mem.sbrk(delta);
        cpu.x[10]     = old;     // return in a0
        std::cout << "[sys] sbrk(" << delta << ") -> " << old << "\n";
        return true;
    }
    case 3: { // cycles
        cpu.x[10] = static_cast<uint32_t>(cpu.cycles);
        std::cout << "[sys] cycles -> " << cpu.x[10] << "\n";
        return true;
    }
    case 4: { // putch
        char c = static_cast<char>(a0 & 0xFF);
        std::cout << c << std::flush;
        return true;
    }
        case 5: { // write buffer from emulated memory: a0=ptr, a1=len
            uint32_t ptr = cpu.x[10];
            uint32_t len = cpu.x[11];
            for(uint32_t i=0; i<len; ++i){
                char c = static_cast<char>(mem.load8(ptr + i));
                std::cout << c;
            }
            std::cout.flush();
            cpu.x[10] = len;    // return bytes written in a0 (like POSIX write)
            return true;
        }

    default:
        std::cout << "[sys] unknown ecall id=" << id << " a0="<<a0<<" a1="<<a1<<"\n";
        return true;
    }
    
    
    
    


}
