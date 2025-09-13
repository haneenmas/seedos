#include "disasm.hpp"
#include <sstream>
#include <iomanip>
#include <string>
#include <cstdint>

// ---- helpers (match cpu.cpp) ----------------------------------------------
static inline uint32_t get_bits(uint32_t v, int pos, int len) {
    return (v >> pos) & ((1u << len) - 1u);
}
static inline int32_t sign_extend(uint32_t v, int bits) {
    uint32_t m = 1u << (bits - 1);
    return (int32_t)((v ^ m) - m);
}

// ---- pretty-printer --------------------------------------------------------
std::string disasm(uint32_t inst) {
    uint32_t opcode = get_bits(inst, 0, 7);
    std::ostringstream ss;

    if (opcode == 0x13) {
        // I-type: OP-IMM  (ADDI)
        uint32_t rd     = get_bits(inst, 7, 5);
        uint32_t funct3 = get_bits(inst, 12, 3);
        uint32_t rs1    = get_bits(inst, 15, 5);
        if (funct3 == 0b000) {
            int32_t imm = sign_extend(get_bits(inst, 20, 12), 12);
            ss << "addi x" << rd << ", x" << rs1 << ", " << imm;
        } else {
            ss << "op-imm(?)";
        }

    } else if (opcode == 0x33) {
        // R-type: ADD/SUB
        uint32_t rd     = get_bits(inst, 7, 5);
        uint32_t funct3 = get_bits(inst, 12, 3);
        uint32_t rs1    = get_bits(inst, 15, 5);
        uint32_t rs2    = get_bits(inst, 20, 5);
        uint32_t funct7 = get_bits(inst, 25, 7);
        if (funct3 == 0b000 && funct7 == 0b0000000) {
            ss << "add x" << rd << ", x" << rs1 << ", x" << rs2;
        } else if (funct3 == 0b000 && funct7 == 0b0100000) {
            ss << "sub x" << rd << ", x" << rs1 << ", x" << rs2;
        } else {
            ss << "r-type(?)";
        }

    } else if (opcode == 0x37) {
        // U-type: LUI
        uint32_t rd    = get_bits(inst, 7, 5);
        uint32_t imm20 = get_bits(inst, 12, 20);
        ss << "lui x" << rd << ", 0x" << std::hex << imm20 << std::dec;

    } else if (opcode == 0x63) {
        // B-type: BEQ (branch if equal)
        uint32_t rs1   = get_bits(inst, 15, 5);
        uint32_t rs2   = get_bits(inst, 20, 5);
        uint32_t funct3= get_bits(inst, 12, 3);

        // imm[12|10:5|4:1|11] << 1, then sign-extend (13 bits)
        uint32_t i12   = get_bits(inst, 31, 1);
        uint32_t i10_5 = get_bits(inst, 25, 6);
        uint32_t i4_1  = get_bits(inst, 8, 4);
        uint32_t i11   = get_bits(inst, 7, 1);
        uint32_t imm13 = (i12<<12) | (i11<<11) | (i10_5<<5) | (i4_1<<1);
        int32_t  off   = sign_extend(imm13, 13);

        if (funct3 == 0b000) {
            std::ostringstream offss; offss << std::showpos << off << std::noshowpos;
            ss << "beq x" << rs1 << ", x" << rs2 << ", " << offss.str();
        } else {
            ss << "branch(?)";
        }

    } else if (opcode == 0x03) {
        // I-type loads: LW
        uint32_t rd     = get_bits(inst, 7, 5);
        uint32_t funct3 = get_bits(inst, 12, 3);
        uint32_t rs1    = get_bits(inst, 15, 5);
        if (funct3 == 0b010) {
            int32_t imm = sign_extend(get_bits(inst, 20, 12), 12);
            ss << "lw x" << rd << ", " << imm << "(x" << rs1 << ")";
        } else {
            ss << "load(?)";
        }

    } else if (opcode == 0x23) {
        // S-type stores: SW
        uint32_t funct3 = get_bits(inst, 12, 3);
        uint32_t rs1    = get_bits(inst, 15, 5);
        uint32_t rs2    = get_bits(inst, 20, 5);
        uint32_t imm11_5= get_bits(inst, 25, 7);
        uint32_t imm4_0 = get_bits(inst, 7, 5);
        int32_t  imm    = sign_extend((imm11_5<<5) | imm4_0, 12);
        if (funct3 == 0b010) {
            ss << "sw x" << rs2 << ", " << imm << "(x" << rs1 << ")";
        } else {
            ss << "store(?)";
        }

    } else {
        ss << "unknown(0x" << std::hex << inst << std::dec << ")";
    }

    return ss.str();
}
