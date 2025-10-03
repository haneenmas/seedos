#pragma once
#include <cstdint>
#include <iostream>
#include "emu/cpu.hpp"
#include "emu/mem.hpp"
#include "emu/disasm.hpp"

inline uint32_t encI(uint8_t rd,uint8_t rs1,int32_t imm12){
    uint32_t u=(uint32_t)(imm12 & 0xFFF);
    return (u<<20)|(rs1<<15)|(0<<12)|(rd<<7)|0x13;
}
inline uint32_t encR(uint8_t rd,uint8_t rs1,uint8_t rs2,uint8_t f3,uint8_t f7){
    return (f7<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|0x33;
}
inline void put32(Memory& m, uint32_t a, uint32_t w){ m.store32(a, w); }

struct TestCtx{
    int passed=0, failed=0;
    void ok(bool cond, const char* msg){
        if(cond){ ++passed; } else { ++failed; std::cout << "[FAIL] " << msg << "\n"; }
    }
    void summary(){
        std::cout << "[tests] passed=" << passed << " failed=" << failed << "\n";
    }
};

