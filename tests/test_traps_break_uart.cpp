#include "emu/mem.hpp"
#include "emu/cpu.hpp"
#include <cassert>
#include <sstream>
#include <iostream>

int main(){
    // breakpoint patch/restore
    {
        Memory r(64*1024);
        r.store32(0, 0x00000013u); // NOP-ish ADDI x0,x0,0
        bool on = r.toggle_break(0);
        assert(on);
        uint32_t e = r.load32(0);
        assert(e == 0x00100073u);  // EBREAK
        bool off = r.toggle_break(0);
        assert(!off);
        assert(r.load32(0) == 0x00000013u);
    }

    // misaligned trap (store32 at addr 2)
    {
        Memory r(64*1024); CPU c; c.pc=0;
        // sw x0, 2(x0) -> misaligned store
        // Minimal encode: use store32 directly and expect the emulator to throw/flag.
        try {
            r.store32(2, 0xDEADBEEFu);
            assert(false && "store32 should have thrown");
        } catch(...) { /* ok */ }
    }

    // UART MMIO (redirect cout and check a char went through)
    {
        Memory r(64*1024);
        std::ostringstream capture;
        auto *old = std::cout.rdbuf(capture.rdbuf());
        r.store8(0x40000000u, 'Z');
        std::cout.rdbuf(old);
        assert(capture.str() == "Z");
    }

    std::cout << "[traps-break-uart] ok\n";
    return 0;
}

