#include "cpu.hpp"
#include "mem.hpp"
#include <cstdint>

// get_bits(value, pos, len): extract 'len' bits starting at bit 'pos'
static inline uint32_t get_bits(uint32_t v, int pos, int len) {
    return (v >> pos) & ((1u << len) - 1u);
}
// sign-extend a small signed number (e.g., 12-bit) to 32-bit
static inline int32_t sign_extend(uint32_t v, int bits) {
    uint32_t m = 1u << (bits - 1);
    return (int32_t)((v ^ m) - m);
}

bool CPU::step(Memory& mem) {
    // 1) FETCH: read 4 bytes (one 32-bit instruction) at pc
    uint32_t inst = mem.load32(pc);

    // 2) DECODE common fields (RISC-V I-type layout)
    uint32_t opcode = get_bits(inst, 0, 7);   // bits 6..0
    uint32_t rd     = get_bits(inst, 7, 5);   // bits 11..7
    uint32_t funct3 = get_bits(inst, 12, 3);  // bits 14..12
    uint32_t rs1    = get_bits(inst, 15, 5);  // bits 19..15

    // 3) EXECUTE: implement OP-IMM/ADDI only
    if (opcode == 0x13) {             // OP-IMM group
        if (funct3 == 0b000) {        // ADDI: x[rd] = x[rs1] + imm12
            int32_t imm = sign_extend(get_bits(inst, 20, 12), 12);
            uint32_t res = (uint32_t)((int32_t)x[rs1] + imm);
            if (rd != 0) x[rd] = res; // x0 must stay 0
            pc += 4;                  // next instruction
        } else {
            return false;             // other OP-IMM not handled yet
        }
    } else {
        return false;                 // other opcodes not handled yet
    }

    x[0] = 0; // enforce x0=0
    return true;
}

